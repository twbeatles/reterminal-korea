// Native unit + integration tests: `pio test -e host_native` (needs host gcc)
// or `python tools/run_native_tests.py` (zig/gcc/clang, same sources).
// Fixture/golden files are read relative to the project root.
#include <unity.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ArduinoJson.h>
#include <string>
#include <vector>

#include "../include/IRandom.h"
#include "../src/app/Coordinator.h"
#include "../src/app/Fetcher.h"
#include "../src/domain/Freshness.h"
#include "../src/domain/Types.h"
#include "../src/drivers/SensorCheck.h"
#include "../src/net/CaBundle.h"
#include "../src/net/Endpoints.h"
#include "../src/net/NetPolicy.h"
#include "../src/power/Schedule.h"
#include "../src/providers/ProviderJson.h"
#include "../src/providers/ResultCodes.h"
#include "../src/provision/SetupPortal.h"
#include "../src/storage/CacheStore.h"
#include "../src/storage/SettingsStore.h"
#include "../src/ui/RefreshPolicy.h"
#include "../src/app/DemoProvider.h"
#include "../src/app/ProviderName.h"
#include "../src/app/RequestBudget.h"
#include "../src/domain/Calendar.h"
#include "../src/domain/ClockPolicy.h"
#include "../src/drivers/ButtonDecoder.h"
#include "../src/drivers/Pmic.h"
#include "../src/drivers/Pcf8563.h"
#include "../src/drivers/Sht4xCodec.h"
#include "../src/drivers/SystemClock.h"
#include "../src/power/BatteryPolicy.h"
#include "../src/power/CyclePlanner.h"
#include "../src/power/RtcState.h"
#include "../src/providers/MarketSession.h"
#include "../src/provision/GeoGrid.h"
#include "../src/provision/PortalHandler.h"
#include "../src/storage/Codec.h"
#include "../src/storage/SettingsCodec.h"
#include "../src/storage/SlotStore.h"
#include "../src/storage/SnapshotCodec.h"
#include "../src/ui/BitmapFont.h"
#include "../src/ui/Canvas.h"
#include "../src/ui/Format.h"
#include "../src/ui/Icons.h"
#include "../src/ui/Renderer.h"
#include "../src/ui/ViewModel.h"
#include "../src/domain/SolarTerms.h"
#include "../src/net/WifiSupervisor.h"
#include "../src/storage/SnapshotCodec.h"
#include "../src/domain/HolidayNames.h"
#include "../src/util/Log.h"

using namespace kdash;
using sched::Provider;

void setUp(void) {}
void tearDown(void) {}

namespace {

// ---------------------------------------------------------------- helpers --

std::string ReadFile(const char* rel) {
  static const char* kPrefixes[] = {"", "../", "../../", "../../../"};
  const char* root = getenv("KDASH_PROJECT_DIR");
  std::vector<std::string> tries;
  if (root) tries.push_back(std::string(root) + "/" + rel);
  for (const char* p : kPrefixes) tries.push_back(std::string(p) + rel);
  for (const std::string& path : tries) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) continue;
    std::string s;
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) s.append(buf, n);
    fclose(f);
    return s;
  }
  return std::string();
}

int64_t Kst(const char* s) { return air::ParseDataTimeKst(s); }

FetchContext Ctx(const char* now_kst, bool verified = true) {
  FetchContext c;
  c.now_utc = c.fetched_utc = Kst(now_kst);
  c.clock_verified = verified;
  return c;
}

struct Pt {
  bool has = false;
  double value = 0;
  Validity validity = Validity::NotConfigured;
  Timestamp observed, fetched;
};

template <typename T>
Pt P(const DataPoint<T>& d) {
  Pt p;
  p.has = d.has_value;
  p.value = (double)d.value;
  p.validity = d.validity;
  p.observed = d.observed_at;
  p.fetched = d.fetched_at;
  return p;
}
Pt P(const DataPoint<FxRate>& d) {
  Pt p;
  p.has = d.has_value;
  p.value = (double)d.value.won_scaled;
  p.validity = d.validity;
  p.observed = d.observed_at;
  p.fetched = d.fetched_at;
  return p;
}
Pt P(const DataPoint<StockQuote>& d) {
  Pt p;
  p.has = d.has_value;
  p.value = (double)d.value.price_krw;
  p.validity = d.validity;
  p.observed = d.observed_at;
  p.fetched = d.fetched_at;
  return p;
}

class FakeRng : public IRandom {
 public:
  explicit FakeRng(uint32_t s) : s_(s) {}
  uint32_t next_u32() override {
    s_ ^= s_ << 13;
    s_ ^= s_ >> 17;
    s_ ^= s_ << 5;
    return s_;
  }
 private:
  uint32_t s_;
};

// Fake transport: first matching rule (URL substring) wins; each rule can
// serve a queue of responses (last one repeats).
class FakeHttp : public IHttpClient {
 public:
  struct Rule {
    std::string match;
    std::vector<std::pair<int, std::string>> replies;  // status, fixture
    size_t next = 0;
  };
  std::vector<Rule> rules;
  std::vector<std::string> urls;
  std::vector<std::string> posts;
  std::string body_;
  std::string last_ca;

  void On(const char* match, int status, const char* fixture) {
    for (Rule& r : rules) {
      if (r.match == match) {
        r.replies.push_back({status, fixture});
        return;
      }
    }
    rules.push_back(Rule{match, {{status, fixture}}, 0});
  }
  HttpResponse Serve(const char* url) {
    HttpResponse r;
    urls.push_back(url);
    for (Rule& rule : rules) {
      if (strstr(url, rule.match.c_str())) {
        auto& rep = rule.replies[rule.next < rule.replies.size()
                                     ? rule.next
                                     : rule.replies.size() - 1];
        if (rule.next < rule.replies.size()) rule.next++;
        body_ = ReadFile((std::string("test/fixtures/") + rep.second).c_str());
        r.status = rep.first;
        r.body = body_.c_str();
        r.body_len = body_.size();
        return r;
      }
    }
    r.error = HttpError::Dns;
    return r;
  }
  HttpResponse get(const char* url, const char* const*, size_t,
                   const char* ca) override {
    last_ca = ca ? ca : "";
    return Serve(url);
  }
  HttpResponse post(const char* url, const char* const*, size_t,
                    const char* body, const char* ca) override {
    last_ca = ca ? ca : "";
    posts.push_back(body ? body : "");
    return Serve(url);
  }
  int Count(const char* sub) const {
    int n = 0;
    for (const std::string& u : urls) n += strstr(u.c_str(), sub) != nullptr;
    return n;
  }
};

std::string key_like_180() { return std::string(180, 'S'); }

Settings ConfiguredSettings() {
  Settings s = settings::DefaultSettings();
  strcpy(s.providers.kma_key, "FAKEKMAKEY%2BAAAA%3D%3D");
  strcpy(s.providers.air_key, "FAKEAIRKEY+BBBB==");
  strcpy(s.providers.exim_key, "FAKEEXIMKEY0001");
  return s;
}

}  // namespace

// ------------------------------------------------------- golden (shared) --

void test_golden_provider_cases(void) {
  std::string text = ReadFile("test/golden/provider_cases.json");
  TEST_ASSERT_TRUE_MESSAGE(!text.empty(), "golden file not found");
  JsonDocument doc;
  TEST_ASSERT_FALSE(deserializeJson(doc, text));
  int failures = 0, ran = 0;
  auto fail = [&](const char* id, const char* what) {
    printf("  GOLDEN FAIL %s: %s\n", id, what);
    failures++;
  };
  for (JsonObject c : doc["cases"].as<JsonArray>()) {
    const char* id = c["id"];
    const std::string prov = c["provider"].as<const char*>();
    std::string body =
        ReadFile((std::string("test/fixtures/") + c["fixture"].as<const char*>()).c_str());
    if (body.empty()) {
      fail(id, "fixture missing");
      continue;
    }
    FetchContext ctx = Ctx(c["now"], c["verified"] | true);
    ctx.clock_verified = c["verified"].as<bool>();
    JsonObject ex = c["expect"];
    Outcome outcome = Outcome::Ok;
    std::vector<std::pair<std::string, Pt>> pts;
    ran++;
    if (prov == "kma_now") {
      kma::NowOut o = pj::KmaUltraNow(body.c_str(), body.size(), c["base"][0],
                                      c["base"][1], ctx);
      outcome = o.outcome;
      pts = {{"temp_c", P(o.temp_c)}, {"humidity", P(o.humidity)},
             {"rain_mm", P(o.rain_mm)}};
      if (ex["base"].is<const char*>() && o.base_utc != Kst(ex["base"]))
        fail(id, "base_utc");
    } else if (prov == "kma_village") {
      kma::ForecastOut o = pj::KmaVillage(body.c_str(), body.size(),
                                          c["base"][0], c["base"][1], ctx);
      outcome = o.outcome;
      pts = {{"tmp", P(o.tmp)}, {"pop", P(o.pop)}, {"pty", P(o.pty)},
             {"sky", P(o.sky)}};
      if (ex["fcst"].is<const char*>() && o.fcst_utc != Kst(ex["fcst"]))
        fail(id, "fcst_utc");
      if (ex["base"].is<const char*>() && o.base_utc != Kst(ex["base"]))
        fail(id, "base_utc");
    } else if (prov == "air") {
      air::AirOut o = pj::AirKorea(body.c_str(), body.size(), c["station"], ctx);
      outcome = o.outcome;
      pts = {{"pm10", P(o.pm10)}, {"pm25", P(o.pm25)}};
      if (ex["data_time"].is<const char*>() &&
          o.data_time_utc != Kst(ex["data_time"]))
        fail(id, "data_time_utc");
      if (ex["pm25_observed"].is<const char*>() &&
          o.pm25.observed_at.epoch_utc != Kst(ex["pm25_observed"]))
        fail(id, "pm25 observed");
      if (strcmp(o.station, c["station"].as<const char*>()) != 0)
        fail(id, "station echo");
    } else if (prov == "exim") {
      exim::EximOut o = pj::Exim(body.c_str(), body.size(), c["searchdate"], ctx);
      outcome = o.outcome;
      pts = {{"usd", P(o.usd)}, {"jpy100", P(o.jpy100)}};
      if (!ex["empty_response"].isNull() &&
          o.empty_response != ex["empty_response"].as<bool>())
        fail(id, "empty_response");
      if (ex["fixing"].is<const char*>() &&
          o.fixing_date_utc != Kst(ex["fixing"]))
        fail(id, "fixing_date_utc");
      if (o.usd.has_value && strcmp(o.usd.value.currency_unit, "USD") != 0)
        fail(id, "usd unit");
      if (o.jpy100.has_value &&
          strcmp(o.jpy100.value.currency_unit, "JPY(100)") != 0)
        fail(id, "jpy unit");
    } else if (prov == "fsc") {
      fin::FscOut o = pj::Fsc(body.c_str(), body.size(), c["symbol"],
                              c["market"], ctx);
      outcome = o.outcome;
      pts = {{"quote", P(o.quote)}};
      if (o.quote.has_value && o.quote.value.kind != QuoteKind::PriorClose)
        fail(id, "kind must be PriorClose");
      if (ex["observed"].is<const char*>() &&
          o.quote.observed_at.epoch_utc != Kst(ex["observed"]))
        fail(id, "observed");
    } else if (prov == "kis") {
      fin::KisOut o = pj::Kis(c["http_status"], body.c_str(), body.size(),
                              c["symbol"], c["market"], ctx);
      outcome = o.outcome;
      pts = {{"quote", P(o.quote)}};
      if (!ex["token_invalid"].isNull() &&
          o.token_invalid != ex["token_invalid"].as<bool>())
        fail(id, "token_invalid");
      if (o.quote.has_value && o.quote.value.kind != QuoteKind::IntradayQuote)
        fail(id, "kind must be IntradayQuote");
      if (ex["observed"].is<const char*>() &&
          o.quote.observed_at.epoch_utc != Kst(ex["observed"]))
        fail(id, "observed");
    } else if (prov == "kis_token") {
      char tok[256];
      int64_t exp = pj::KisToken(c["http_status"], body.c_str(), body.size(),
                                 ctx, tok, sizeof(tok), outcome);
      if (!ex["expires_in_after_margin"].isNull() &&
          exp - ctx.fetched_utc != ex["expires_in_after_margin"].as<long>())
        fail(id, "token expiry");
    } else {
      fail(id, "unknown provider");
      continue;
    }
    if (strcmp(ToString(outcome), ex["outcome"].as<const char*>()) != 0) {
      char m[96];
      snprintf(m, sizeof(m), "outcome %s != %s", ToString(outcome),
               ex["outcome"].as<const char*>());
      fail(id, m);
    }
    if (!ex["fetched_verified"].isNull() && !pts.empty() &&
        pts[0].second.fetched.verified != ex["fetched_verified"].as<bool>())
      fail(id, "fetched_at.verified");
    for (JsonPair kv : ex["points"].as<JsonObject>()) {
      const Pt* pt = nullptr;
      for (auto& e : pts)
        if (e.first == kv.key().c_str()) pt = &e.second;
      if (!pt) {
        fail(id, "unknown point");
        continue;
      }
      JsonArray want = kv.value().as<JsonArray>();
      if (strcmp(ToString(pt->validity), want[0].as<const char*>()) != 0) {
        char m[96];
        snprintf(m, sizeof(m), "%s validity %s != %s", kv.key().c_str(),
                 ToString(pt->validity), want[0].as<const char*>());
        fail(id, m);
      }
      if (want[1].isNull()) {
        if (pt->has) fail(id, "value present but expected none (never 0)");
      } else if (!pt->has || fabs(pt->value - want[1].as<double>()) > 1e-9) {
        char m[96];
        snprintf(m, sizeof(m), "%s value %g != %g", kv.key().c_str(),
                 pt->value, want[1].as<double>());
        fail(id, m);
      }
    }
  }
  printf("  golden cases run: %d, failures: %d\n", ran, failures);
  TEST_ASSERT_TRUE(ran >= 40);
  TEST_ASSERT_EQUAL_INT(0, failures);
}

// ------------------------------------------------------------- time/KST --

void test_real_date_validation(void) {
  TEST_ASSERT_TRUE(IsRealDate(2024, 2, 29));  // leap day accepted
  TEST_ASSERT_FALSE(IsRealDate(2023, 2, 29));
  TEST_ASSERT_FALSE(IsRealDate(2026, 2, 30));
  TEST_ASSERT_FALSE(IsRealDate(2026, 13, 1));
  TEST_ASSERT_EQUAL_INT64(-1, KstYmdHmToUtc(20260230, 1200));
  TEST_ASSERT_EQUAL_INT64(-1, KstYmdHmToUtc(20230229, 1200));
  TEST_ASSERT_TRUE(KstYmdHmToUtc(20240229, 1200) > 0);
  TEST_ASSERT_EQUAL_INT64(-1, KstStrToUtc("20260230", "1200"));
  TEST_ASSERT_EQUAL_INT64(-1, air::ParseDataTimeKst("2026-02-30 12:00"));
  TEST_ASSERT_EQUAL_INT64(-1, air::ParseDataTimeKst("2023-02-29 12:00"));
  TEST_ASSERT_TRUE(air::ParseDataTimeKst("2024-02-29 12:00") > 0);
}

void test_kst_boundaries(void) {
  int64_t e = MakeUtc(2026, 10, 31, 15, 0);  // UTC 15:00 == KST 11-01 00:00
  KstBroken b = BreakKst(e);
  TEST_ASSERT_EQUAL(0, b.hour);
  TEST_ASSERT_EQUAL(1, b.day);
  TEST_ASSERT_TRUE(IsKstMidnightBoundary(e));
  TEST_ASSERT_EQUAL_INT64(Kst("2026-11-02 00:00"), Kst("2026-11-01 24:00"));
  TEST_ASSERT_EQUAL_INT64(-1, Kst("2026-11-01 24:30"));
  TEST_ASSERT_EQUAL_INT64(-1, Kst("2026-11-01 14:00x"));
  TEST_ASSERT_EQUAL_INT64(Kst("2026-11-01 14:00"), KstStrToUtc("20261101", "1400"));
  TEST_ASSERT_EQUAL_INT64(-1, KstStrToUtc("2026111", "1400"));
}

void test_kma_release_selection(void) {
  int d = 0, t = 0;
  kma::SelectUltraNowBase(Kst("2026-11-01 14:39"), d, t);  // 정시 회차, 40분 지연
  TEST_ASSERT_EQUAL(20261101, d);
  TEST_ASSERT_EQUAL(1300, t);
  kma::SelectUltraNowBase(Kst("2026-11-01 14:40"), d, t);
  TEST_ASSERT_EQUAL(1400, t);
  kma::SelectUltraNowBase(Kst("2026-11-01 00:05"), d, t);  // midnight borrow
  TEST_ASSERT_EQUAL(20261031, d);
  TEST_ASSERT_EQUAL(2300, t);
  kma::SelectUltraNowBase(Kst("2026-01-01 00:05"), d, t);  // year boundary
  TEST_ASSERT_EQUAL(20251231, d);
  kma::SelectVillageBase(Kst("2026-11-01 14:09"), d, t);
  TEST_ASSERT_EQUAL(1100, t);
  kma::SelectVillageBase(Kst("2026-11-01 14:10"), d, t);
  TEST_ASSERT_EQUAL(1400, t);
  kma::SelectVillageBase(Kst("2026-03-01 01:00"), d, t);  // month boundary
  TEST_ASSERT_EQUAL(20260228, d);
  TEST_ASSERT_EQUAL(2300, t);
  bool ok = true;
  kma::ParseDecimal("-", ok);
  TEST_ASSERT_FALSE(ok);
  kma::ParseDecimal("-999", ok);  // KMA missing sentinel
  TEST_ASSERT_FALSE(ok);
  TEST_ASSERT_EQUAL_FLOAT(-3.5f, (float)kma::ParseDecimal("-3.5", ok));
  TEST_ASSERT_TRUE(ok);
}

void test_exim_quoted(void) {
  int64_t ws = 0;
  int32_t sc = 0;
  TEST_ASSERT_TRUE(exim::ParseQuoted("1,384.50", ws, sc));
  TEST_ASSERT_EQUAL_INT64(138450, ws);
  TEST_ASSERT_TRUE(exim::ParseQuoted("921.3", ws, sc));
  TEST_ASSERT_EQUAL_INT64(92130, ws);
  TEST_ASSERT_TRUE(exim::ParseQuoted("9.999", ws, sc));  // truncate, no float
  TEST_ASSERT_EQUAL_INT64(999, ws);
  TEST_ASSERT_FALSE(exim::ParseQuoted("-", ws, sc));
  TEST_ASSERT_FALSE(exim::ParseQuoted("0", ws, sc));  // 0원 never real
  TEST_ASSERT_FALSE(exim::ParseQuoted("12345678901234567890", ws, sc));
  int dates[7];
  TEST_ASSERT_EQUAL(7, (int)exim::PlanSearchDates(20260301, 7, dates, 7));
  TEST_ASSERT_EQUAL(20260223, dates[6]);
}

// ---------------------------------------------------------------- network --

void test_net_policy(void) {
  TEST_ASSERT_TRUE(net::HostAllowed("https://apis.data.go.kr/1360000/x"));
  TEST_ASSERT_TRUE(net::HostAllowed("https://APIS.DATA.GO.KR/x"));
  TEST_ASSERT_TRUE(net::HostAllowed("https://apis.data.go.kr:443/x"));
  TEST_ASSERT_TRUE(net::HostAllowed("https://oapi.koreaexim.go.kr/site/x"));
  TEST_ASSERT_TRUE(net::HostAllowed("https://openapi.koreainvestment.com:9443/oauth2/tokenP"));
  TEST_ASSERT_FALSE(net::HostAllowed("https://openapi.koreainvestment.com/x"));  // 443 != 9443
  TEST_ASSERT_FALSE(net::HostAllowed("http://apis.data.go.kr/x"));
  TEST_ASSERT_FALSE(net::HostAllowed("https://apis.data.go.kr:443@evil.example/x"));
  TEST_ASSERT_FALSE(net::HostAllowed("https://apis.data.go.kr@evil.example/x"));
  TEST_ASSERT_FALSE(net::HostAllowed("https://evil.example\\@apis.data.go.kr/x"));
  TEST_ASSERT_FALSE(net::HostAllowed("https://apis.data.go.kr.evil.example/x"));
  TEST_ASSERT_FALSE(net::HostAllowed("https://www.koreaexim.go.kr/x"));
  TEST_ASSERT_FALSE(net::HostAllowed("https://apiportal.koreainvestment.com/x"));
  TEST_ASSERT_EQUAL(120, net::ParseRetryAfter(" 120"));
  TEST_ASSERT_EQUAL(-1, net::ParseRetryAfter("Wed, 21 Oct 2026 07:28:00 GMT"));
  TEST_ASSERT_EQUAL(3600, net::ParseRetryAfter("999999"));
  TEST_ASSERT_NOT_NULL(strstr(net::kCaBundlePem, "BEGIN CERTIFICATE"));
}

void test_endpoints(void) {
  char out[1400];
  // "Encoding" key verbatim, "Decoding" key encoded once (G-6).
  TEST_ASSERT_TRUE(endpoints::NormalizeServiceKey("ABC%2BDEF%3D%3D", out, sizeof(out)));
  TEST_ASSERT_EQUAL_STRING("ABC%2BDEF%3D%3D", out);
  TEST_ASSERT_TRUE(endpoints::NormalizeServiceKey("ABC+DEF/G==", out, sizeof(out)));
  TEST_ASSERT_EQUAL_STRING("ABC%2BDEF%2FG%3D%3D", out);
  TEST_ASSERT_FALSE(endpoints::NormalizeServiceKey("ABC%2B+DEF", out, sizeof(out)));
  TEST_ASSERT_FALSE(endpoints::NormalizeServiceKey("ABC%ZZ", out, sizeof(out)));
  TEST_ASSERT_FALSE(endpoints::NormalizeServiceKey("ABC DEF", out, sizeof(out)));
  TEST_ASSERT_TRUE(endpoints::KmaUltraUrl("K", 20261101, 1400, 60, 127, 16, out, sizeof(out)));
  TEST_ASSERT_EQUAL_STRING(
      "https://apis.data.go.kr/1360000/VilageFcstInfoService_2.0/getUltraSrtNcst"
      "?serviceKey=K&pageNo=1&numOfRows=16&dataType=JSON&base_date=20261101"
      "&base_time=1400&nx=60&ny=127", out);
  TEST_ASSERT_TRUE(net::HostAllowed(out));
  TEST_ASSERT_TRUE(endpoints::AirKoreaCandidateUrl("K", "종로구", out, sizeof(out)));
  TEST_ASSERT_NOT_NULL(strstr(out, "stationName=%EC%A2%85%EB%A1%9C%EA%B5%AC"));
  TEST_ASSERT_TRUE(endpoints::EximUrl("A", 20261030, out, sizeof(out)));
  TEST_ASSERT_NOT_NULL(strstr(out, "https://oapi.koreaexim.go.kr/"));
  TEST_ASSERT_TRUE(endpoints::FscPriceUrl("K", "005930", 20261019, out, sizeof(out)));
  TEST_ASSERT_NOT_NULL(strstr(out, "/1160100/GetStockSecuritiesInfoService_V2/getStockPriceInfo_V2?"));
  TEST_ASSERT_NOT_NULL(strstr(out, "likeSrtnCd=005930&beginBasDt=20261019"));
  TEST_ASSERT_TRUE(net::HostAllowed(out));
  TEST_ASSERT_TRUE(endpoints::KisPriceUrl("005930", "NXT", out, sizeof(out)));
  TEST_ASSERT_NOT_NULL(strstr(out, "FID_COND_MRKT_DIV_CODE=NX&FID_INPUT_ISCD=005930"));
  TEST_ASSERT_TRUE(net::HostAllowed(out));
  TEST_ASSERT_FALSE(endpoints::KisPriceUrl("005930", "NYSE", out, sizeof(out)));
  TEST_ASSERT_TRUE(endpoints::KisTokenUrl(out, sizeof(out)));
  TEST_ASSERT_TRUE(net::HostAllowed(out));
  TEST_ASSERT_FALSE(endpoints::KisTokenBody("key\"x", "s", out, sizeof(out)));
  char enc[16];
  TEST_ASSERT_FALSE(endpoints::EncodeOnce("100%EC", enc, sizeof(enc)));
}

void test_result_codes(void) {
  TEST_ASSERT_EQUAL((int)Outcome::Ok, (int)codes::DataGoKr("00"));
  TEST_ASSERT_EQUAL((int)Outcome::NoData, (int)codes::DataGoKr("03"));
  TEST_ASSERT_EQUAL((int)Outcome::AuthError, (int)codes::DataGoKr("30"));
  TEST_ASSERT_EQUAL((int)Outcome::QuotaExceeded, (int)codes::DataGoKr("22"));
  TEST_ASSERT_EQUAL((int)Outcome::ProviderError, (int)codes::DataGoKr(nullptr));
  TEST_ASSERT_EQUAL((int)Outcome::ProviderError, (int)codes::DataGoKr("99"));
  TEST_ASSERT_EQUAL((int)Outcome::RateLimited, (int)codes::Kis(403, nullptr, "EGW00133"));
  TEST_ASSERT_EQUAL((int)Outcome::AuthError, (int)codes::Kis(403, nullptr, nullptr));
  TEST_ASSERT_EQUAL((int)Outcome::Ok, (int)codes::Kis(200, "0", "MCA00000"));
  TEST_ASSERT_EQUAL((int)Outcome::ProviderError, (int)codes::Kis(200, "00", nullptr));
  char t[16];
  TEST_ASSERT_TRUE(codes::XmlTagText("<a><b> 30 </b></a>", "b", t, sizeof(t)));
  TEST_ASSERT_EQUAL_STRING("30", t);
}

// --------------------------------------------------------------- settings --

void test_settings_validation(void) {
  using settings::Validation;
  std::string name64(64, 'a'), name65(65, 'a');
  std::string ko21, ko22;
  for (int i = 0; i < 21; i++) ko21 += "가";
  ko22 = ko21 + "가";
  TEST_ASSERT_EQUAL((int)Validation::Ok, (int)settings::ValidateLocation(60, 127, name64.c_str(), "종로구"));
  TEST_ASSERT_EQUAL((int)Validation::BadStation, (int)settings::ValidateLocation(60, 127, name65.c_str(), "종로구"));
  TEST_ASSERT_EQUAL((int)Validation::Ok, (int)settings::ValidateLocation(60, 127, ko21.c_str(), "종로구"));
  TEST_ASSERT_EQUAL((int)Validation::BadStation, (int)settings::ValidateLocation(60, 127, ko22.c_str(), "종로구"));
  TEST_ASSERT_EQUAL((int)Validation::BadStation, (int)settings::ValidateLocation(60, 127, "\xEC\xA2", "종로구"));
  TEST_ASSERT_EQUAL((int)Validation::BadGrid, (int)settings::ValidateLocation(0, 127, "a", "b"));
  // The full-length value fits its buffer (ISSUE-007).
  Settings s;
  TEST_ASSERT_TRUE(settings::CopyBounded(s.location.display_name, sizeof(s.location.display_name), name64.c_str()));
  TEST_ASSERT_EQUAL_STRING(name64.c_str(), s.location.display_name);
  TEST_ASSERT_FALSE(settings::CopyBounded(s.location.display_name, sizeof(s.location.display_name), name65.c_str()));
  std::string key256(256, 'K'), key257(257, 'K');
  TEST_ASSERT_EQUAL((int)Validation::Ok, (int)settings::ValidateKey(key256.c_str()));
  TEST_ASSERT_TRUE(settings::CopyBounded(s.providers.kis_app_secret, sizeof(s.providers.kis_app_secret), key256.c_str()));
  TEST_ASSERT_EQUAL((int)Validation::BadKey, (int)settings::ValidateKey(key257.c_str()));
  TEST_ASSERT_EQUAL((int)Validation::BadKey, (int)settings::ValidateKey("has space"));
  TEST_ASSERT_EQUAL((int)Validation::Ok, (int)settings::ValidateKey(""));
  // WPA2 rules (G-7).
  TEST_ASSERT_EQUAL((int)Validation::Ok, (int)settings::ValidateWifi("home", ""));
  TEST_ASSERT_EQUAL((int)Validation::BadSecret, (int)settings::ValidateWifi("home", "1234567"));
  TEST_ASSERT_EQUAL((int)Validation::Ok, (int)settings::ValidateWifi("home", "12345678"));
  TEST_ASSERT_EQUAL((int)Validation::BadSecret, (int)settings::ValidateWifi("home", std::string(64, 'z').c_str()));
  TEST_ASSERT_EQUAL((int)Validation::Ok, (int)settings::ValidateWifi("home", std::string(64, 'a').c_str()));
  TEST_ASSERT_EQUAL((int)Validation::BadSsid, (int)settings::ValidateWifi(std::string(33, 's').c_str(), ""));
}

void test_watchlist(void) {
  using settings::Validation;
  WatchItem w[kMaxWatchlist];
  size_t n = 99;
  TEST_ASSERT_EQUAL((int)Validation::Ok, (int)settings::ParseWatchlist(" 005930 , 000660:NXT", w, kMaxWatchlist, n));
  TEST_ASSERT_EQUAL(2, (int)n);
  TEST_ASSERT_EQUAL_STRING("000660", w[1].symbol);
  TEST_ASSERT_EQUAL((int)Market::Nxt, (int)w[1].market);
  TEST_ASSERT_EQUAL((int)Validation::Ok, (int)settings::ParseWatchlist("", w, kMaxWatchlist, n));
  TEST_ASSERT_EQUAL(0, (int)n);
  TEST_ASSERT_EQUAL((int)Validation::BadWatchlist, (int)settings::ParseWatchlist("005930,", w, kMaxWatchlist, n));
  TEST_ASSERT_EQUAL((int)Validation::BadWatchlist, (int)settings::ParseWatchlist(",,,", w, kMaxWatchlist, n));
  TEST_ASSERT_EQUAL((int)Validation::BadWatchlist, (int)settings::ParseWatchlist("005930,,000660", w, kMaxWatchlist, n));
  TEST_ASSERT_EQUAL((int)Validation::BadWatchlist, (int)settings::ParseWatchlist("000001,000002,000003,000004,000005", w, kMaxWatchlist, n));
  TEST_ASSERT_EQUAL((int)Validation::BadSymbol, (int)settings::ParseWatchlist("5930", w, kMaxWatchlist, n));
  TEST_ASSERT_EQUAL((int)Validation::BadMarket, (int)settings::ParseWatchlist("005930:NYSE", w, kMaxWatchlist, n));
  char err[96];
  TEST_ASSERT_FALSE(provision::ValidateField(provision::Field::Watchlist, "005930,", err, sizeof(err)));
  TEST_ASSERT_TRUE(provision::ValidateField(provision::Field::KisAppSecret, key_like_180().c_str(), err, sizeof(err)));
}

// ----------------------------------------------------------------- portal --

void test_portal(void) {
  FakeRng a(1), b(1), c(2);
  char pa[16], pb[16], pc[16];
  TEST_ASSERT_TRUE(provision::MakeApPassword(a, pa, sizeof(pa)));
  TEST_ASSERT_TRUE(provision::MakeApPassword(b, pb, sizeof(pb)));
  TEST_ASSERT_TRUE(provision::MakeApPassword(c, pc, sizeof(pc)));
  TEST_ASSERT_EQUAL(12, (int)strlen(pa));
  TEST_ASSERT_EQUAL_STRING(pa, pb);  // deterministic only through the port
  TEST_ASSERT_TRUE(strcmp(pa, pc) != 0);
  for (const char* p = pa; *p; p++) TEST_ASSERT_NULL(strchr("01OIL", *p));
  char small[8];
  TEST_ASSERT_FALSE(provision::MakeApPassword(a, small, sizeof(small)));
  char tok[33];
  TEST_ASSERT_TRUE(provision::IssueCsrfToken(c, tok, sizeof(tok)));
  TEST_ASSERT_EQUAL(32, (int)strlen(tok));
  TEST_ASSERT_TRUE(provision::CheckCsrfToken(tok, tok));
  TEST_ASSERT_FALSE(provision::CheckCsrfToken("", tok));
  char esc[8];
  provision::EscapeHtml("ab<cd", esc, sizeof(esc));  // 7 chars + NUL fit
  TEST_ASSERT_EQUAL_STRING("ab&lt;c", esc);
  provision::EscapeHtml("abcd<", esc, sizeof(esc));  // entity would not fit
  TEST_ASSERT_EQUAL_STRING("abcd", esc);  // no partial "&l"
  char ap[32];
  TEST_ASSERT_TRUE(provision::MakeApName("1A2B", ap, sizeof(ap)));
  TEST_ASSERT_EQUAL_STRING("KDASH-E1001-1A2B", ap);
  TEST_ASSERT_FALSE(provision::MakeApName("SN12", ap, sizeof(ap)));
}

// -------------------------------------------------------------- scheduler --

void test_scheduler_backoff_and_blocks(void) {
  app::AppState st;
  st.settings = ConfiguredSettings();
  const int64_t t0 = Kst("2026-11-02 10:00");  // Monday
  // Never fetched => due now (EXIM interval-driven).
  TEST_ASSERT_TRUE(app::NextAttemptUtc(st, Provider::Exim, t0, true) <= t0);
  // Transport failure => backoff, not due again immediately (ISSUE-006).
  app::MarkResult(st, Provider::Exim, Outcome::ProviderError, t0, true, -1, 0);
  TEST_ASSERT_TRUE(app::NextAttemptUtc(st, Provider::Exim, t0 + 1, true) > t0 + 1);
  TEST_ASSERT_EQUAL_INT64(t0 + 2, app::NextAttemptUtc(st, Provider::Exim, t0 + 1, true));
  // Second failure spends the retry budget => one normal interval.
  app::MarkResult(st, Provider::Exim, Outcome::ProviderError, t0 + 2, true, -1, 0);
  TEST_ASSERT_EQUAL_INT64(t0 + 2 + 12 * 3600, app::NextAttemptUtc(st, Provider::Exim, t0 + 3, true));
  // Auth error => "키 확인 필요" + one automatic retry per day (ADR 0026, was: never).
  app::MarkResult(st, Provider::AirKorea, Outcome::AuthError, t0, true, -1, 0);
  TEST_ASSERT_TRUE(st.providers[(int)Provider::AirKorea].auth_blocked);
  TEST_ASSERT_EQUAL_INT64(t0 + app::kAuthRetrySec,
                          app::NextAttemptUtc(st, Provider::AirKorea, t0 + 99, true));
  app::ClearAuthBlocks(st);
  TEST_ASSERT_TRUE(app::NextAttemptUtc(st, Provider::AirKorea, t0, true) <= t0);
  // Quota => next KST midnight (+10 min).
  app::MarkResult(st, Provider::AirKorea, Outcome::QuotaExceeded, t0, true, -1, 0);
  TEST_ASSERT_EQUAL_INT64(Kst("2026-11-03 00:10"), app::NextAttemptUtc(st, Provider::AirKorea, t0, true));
  // 429 with Retry-After honoured.
  app::MarkResult(st, Provider::KmaNow, Outcome::RateLimited, t0, true, 120, 0);
  TEST_ASSERT_EQUAL_INT64(t0 + 120, app::NextAttemptUtc(st, Provider::KmaNow, t0, true));
  // Unconfigured providers never run.
  st.settings.providers.exim_key[0] = '\0';
  TEST_ASSERT_EQUAL_INT64(INT64_MAX, app::NextAttemptUtc(st, Provider::Exim, t0, true));
  TEST_ASSERT_EQUAL_INT64(INT64_MAX, app::NextAttemptUtc(st, Provider::Sht40, t0, true));
}

void test_scheduler_release_and_market_gates(void) {
  app::AppState st;
  st.settings = ConfiguredSettings();
  // KMA now: after fetching the 14:00 회차, next due when 15:00 is released.
  app::MarkResult(st, Provider::KmaNow, Outcome::Ok, Kst("2026-11-02 14:45"), true, -1, 0,
                  Kst("2026-11-02 14:00"));
  TEST_ASSERT_EQUAL_INT64(Kst("2026-11-02 15:40"),
                          app::NextAttemptUtc(st, Provider::KmaNow, Kst("2026-11-02 14:50"), true));
  app::MarkResult(st, Provider::KmaForecast, Outcome::Ok, Kst("2026-11-02 14:15"), true, -1, 0,
                  Kst("2026-11-02 14:00"));
  TEST_ASSERT_EQUAL_INT64(Kst("2026-11-02 17:10"),
                          app::NextAttemptUtc(st, Provider::KmaForecast, Kst("2026-11-02 14:20"), true));
  // KIS: only during the KRX session (ISSUE-006), once the latest close is in hand
  // (the post-close catch-up of ADR 0023/0026 is covered by test_kis_post_close_catchup).
  st.settings.providers.kis_enabled = true;
  strcpy(st.settings.providers.kis_app_key, "FAKEAPPKEY");
  strcpy(st.settings.providers.kis_app_secret, "FAKEAPPSECRET");
  strcpy(st.settings.watchlist[0].symbol, "005930");
  st.settings.watch_count = 1;
  app::MarkResult(st, Provider::KisPrice, Outcome::Ok, Kst("2026-10-30 15:40"), true, -1, 0);
  TEST_ASSERT_EQUAL_INT64(Kst("2026-11-02 09:00"),
                          app::NextAttemptUtc(st, Provider::KisPrice, Kst("2026-11-02 07:00"), true));
  TEST_ASSERT_EQUAL_INT64(Kst("2026-11-02 09:00"),  // Saturday -> Monday
                          app::NextAttemptUtc(st, Provider::KisPrice, Kst("2026-10-31 10:00"), true));
  // FSC: weekdays after 13:00 KST, once.
  st.settings.providers.fsc_enabled = true;
  // No success yet: due immediately (latest close in the 14-day window).
  TEST_ASSERT_EQUAL_INT64(Kst("2026-11-02 12:59"),
                          app::NextAttemptUtc(st, Provider::FscClose, Kst("2026-11-02 12:59"), true));
  app::MarkResult(st, Provider::FscClose, Outcome::Ok, Kst("2026-11-02 13:01"), true, -1, 0);
  TEST_ASSERT_EQUAL_INT64(Kst("2026-11-03 13:00"),
                          app::NextAttemptUtc(st, Provider::FscClose, Kst("2026-11-02 13:02"), true));
  TEST_ASSERT_TRUE(app::NextWakeUtc(st, Kst("2026-11-02 14:50"), true) <= Kst("2026-11-02 15:40"));
}

// ---------------------------------------------------- fetch integration --

void test_fetch_pass_end_to_end(void) {
  app::AppState st;
  st.settings = ConfiguredSettings();
  app::Snapshot snap;
  app::KisSession kis;
  FakeHttp http;
  http.On("getUltraSrtNcst", 200, "kma_ultra_ok.json");
  http.On("getVilageFcst", 200, "kma_village_multi.json");
  http.On("getMsrstnAcctoRltmMesureDnsty", 200, "air_ok.json");
  http.On("searchdate=20261101", 200, "exim_empty.json");  // Sunday
  http.On("searchdate=20261031", 200, "exim_empty.json");  // Saturday
  http.On("searchdate=20261030", 200, "exim_ok.json");     // Friday fixing
  FetchContext ctx = Ctx("2026-11-01 15:20");
  app::PassLog log;
  app::RunFetchPass(st, snap, kis, http, net::kCaBundlePem, ctx, true, 0, &log);
  TEST_ASSERT_EQUAL(4, log.count);
  TEST_ASSERT_EQUAL(4, log.requests);  // 1+1+1 + Friday fixing, weekends skipped
  TEST_ASSERT_EQUAL(1, http.Count("exchangeJSON"));
  TEST_ASSERT_EQUAL_STRING(net::kCaBundlePem, http.last_ca.c_str());
  for (const std::string& u : http.urls) TEST_ASSERT_TRUE(net::HostAllowed(u.c_str()));
  // Decoding-form air key was encoded once; Encoding-form KMA key verbatim.
  TEST_ASSERT_TRUE(http.Count("serviceKey=FAKEAIRKEY%2BBBBB%3D%3D") == 1);
  TEST_ASSERT_TRUE(http.Count("serviceKey=FAKEKMAKEY%2BAAAA%3D%3D") == 2);
  TEST_ASSERT_EQUAL(2, http.Count("base_time=1400"));  // 15:20 => 14시 회차 둘 다
  TEST_ASSERT_TRUE(snap.has_now && snap.has_fc && snap.has_air && snap.has_fx);
  TEST_ASSERT_EQUAL_FLOAT(21.5f, (float)snap.now.temp_c.value);
  TEST_ASSERT_EQUAL_INT64(Kst("2026-11-01 15:00"), snap.fc.fcst_utc);
  TEST_ASSERT_EQUAL_INT64(138450, snap.fx.usd.value.won_scaled);
  TEST_ASSERT_EQUAL_INT64(Kst("2026-10-30 00:00"), snap.fx.fixing_date_utc);

  // Same minute again: nothing is due => zero requests.
  const size_t before = http.urls.size();
  app::RunFetchPass(st, snap, kis, http, net::kCaBundlePem, ctx, true, 0, &log);
  TEST_ASSERT_EQUAL(0, log.requests);
  TEST_ASSERT_EQUAL((int)before, (int)http.urls.size());

  // 13h later: EXIM due again; walk-back stops at the fixing already held.
  // AirKorea now fails => last-known-good kept and aged to Stale.
  http.rules.clear();
  http.On("getUltraSrtNcst", 200, "kma_ultra_nodata.json");
  http.On("getVilageFcst", 200, "kma_quota.xml");
  http.On("getMsrstnAcctoRltmMesureDnsty", 500, "maintenance.html");
  http.On("exchangeJSON", 200, "exim_empty.json");
  FetchContext later = Ctx("2026-11-02 04:30");
  app::RunFetchPass(st, snap, kis, http, net::kCaBundlePem, later, true, 0, &log);
  TEST_ASSERT_TRUE(snap.has_air);
  TEST_ASSERT_EQUAL(45, snap.air.pm10.value);
  TEST_ASSERT_EQUAL((int)Validity::Stale, (int)snap.air.pm10.validity);
  TEST_ASSERT_EQUAL(1, http.Count("searchdate=20261102"));
  TEST_ASSERT_EQUAL(0, http.Count("searchdate=20261101"));  // known weekend
  TEST_ASSERT_EQUAL(0, http.Count("searchdate=20261031"));  // known weekend
  TEST_ASSERT_EQUAL(1, http.Count("searchdate=20261030"));  // already held
  TEST_ASSERT_EQUAL((int)Outcome::QuotaExceeded,
                    (int)st.providers[(int)Provider::KmaForecast].last_outcome);
  TEST_ASSERT_EQUAL(3, http.Count("getUltraSrtNcst"));  // 1 + NODATA => prev hour once
}

void test_fetch_exim_auth_error_stops_walk_back(void) {
  app::AppState st;
  st.settings = ConfiguredSettings();
  st.settings.providers.kma_key[0] = st.settings.providers.air_key[0] = '\0';
  app::Snapshot snap;
  app::KisSession kis;
  FakeHttp http;
  http.On("exchangeJSON", 200, "exim_auth_error.json");
  app::PassLog log;
  app::RunFetchPass(st, snap, kis, http, net::kCaBundlePem, Ctx("2026-11-01 15:20"), true, 0, &log);
  TEST_ASSERT_EQUAL(1, log.requests);  // ISSUE-005: no 7-day walk-back
  TEST_ASSERT_TRUE(st.providers[(int)Provider::Exim].auth_blocked);
}

void test_fetch_kis_token_reissue_once(void) {
  app::AppState st;
  st.settings = settings::DefaultSettings();
  st.settings.providers.kis_enabled = true;
  strcpy(st.settings.providers.kis_app_key, "FAKEAPPKEY0000000");
  strcpy(st.settings.providers.kis_app_secret, "FAKEAPPSECRET000000");
  strcpy(st.settings.watchlist[0].symbol, "005930");
  st.settings.watch_count = 1;
  app::Snapshot snap;
  app::KisSession kis;
  FakeHttp http;
  http.On("oauth2/tokenP", 200, "kis_token_ok.json");
  http.On("inquire-price", 500, "kis_token_expired.json");
  http.On("inquire-price", 200, "kis_ok.json");
  app::PassLog log;
  app::RunFetchPass(st, snap, kis, http, net::kCaBundlePem, Ctx("2026-11-02 10:00"), true, 0, &log);
  TEST_ASSERT_EQUAL(5, log.requests);  // token, quote(expired), token, quote, 지수 1회(응답 없음=무시)
  TEST_ASSERT_EQUAL(2, http.Count("oauth2/tokenP"));
  TEST_ASSERT_TRUE(snap.has_kis[0]);
  TEST_ASSERT_EQUAL_INT64(78500, snap.kis[0].quote.value.price_krw);
  TEST_ASSERT_NOT_NULL(strstr(http.posts[0].c_str(), "\"grant_type\":\"client_credentials\""));
  // Outside market hours: no KIS request at all, once the post-close value was received
  // (a success before the 15:30 close allows exactly one catch-up, see test_kis_post_close_catchup).
  app::MarkResult(st, Provider::KisPrice, Outcome::Ok, Kst("2026-11-02 16:00"), true, -1, 0);
  FakeHttp idle;
  app::RunFetchPass(st, snap, kis, idle, net::kCaBundlePem, Ctx("2026-11-02 20:00"), true, 0, &log);
  TEST_ASSERT_EQUAL(0, (int)idle.urls.size());
}

// ----------------------------------------------------------- misc legacy --

void test_cache_bitflip_rejected(void) {
  uint8_t blob[128];
  const uint8_t payload[] = "{\"t\":21.5}";
  size_t n = CacheEncode(payload, sizeof(payload) - 1, 1, 1000, 1100, blob, sizeof(blob));
  TEST_ASSERT_TRUE(n > 0);
  CacheHeader h;
  const uint8_t* pl = nullptr;
  size_t pl_len = 0;
  TEST_ASSERT_TRUE(CacheDecode(blob, n, h, pl, pl_len));
  blob[n - 1] ^= 0x01;
  TEST_ASSERT_FALSE(CacheDecode(blob, n, h, pl, pl_len));
}

void test_backoff_budgets(void) {
  TEST_ASSERT_EQUAL(2, (int)sched::BackoffSec(0, 0));
  TEST_ASSERT_EQUAL(300, (int)sched::BackoffSec(20, 0));
  TEST_ASSERT_TRUE(sched::BackoffSec(1, -999) >= 4);  // negative slot safe
  TEST_ASSERT_TRUE(sched::RetryAllowed(Provider::KmaNow, 1, false));
  TEST_ASSERT_FALSE(sched::RetryAllowed(Provider::KmaNow, 2, false));
  TEST_ASSERT_FALSE(sched::RetryAllowed(Provider::KisPrice, 1, true));
}

void test_sensor_and_refresh(void) {
  TEST_ASSERT_EQUAL((int)drivers::SensorCheck::Ok, (int)drivers::CheckSensorRange(22.0f, 48.0f));
  TEST_ASSERT_FALSE(drivers::IsIndoorPlausible(80.0f));
  ui::RefreshCaps caps;
  TEST_ASSERT_EQUAL((int)ui::RefreshDecision::Skip, (int)ui::DecideRefresh(false, true, 0, caps));
  TEST_ASSERT_EQUAL((int)ui::RefreshDecision::Full, (int)ui::DecideRefresh(true, true, 0, caps));
}

void test_fsc_due_gate(void) {
  int64_t mon_after = Kst("2026-11-02 13:30");
  TEST_ASSERT_TRUE(fin::FscDue(mon_after, 0));
  TEST_ASSERT_FALSE(fin::FscDue(mon_after, mon_after + 60));
  TEST_ASSERT_FALSE(fin::FscDue(Kst("2026-10-31 14:00"), 0));  // Saturday
  // 성공 이력이 없으면 13시 전에도 1회, 이력이 있으면 13시까지 대기.
  TEST_ASSERT_TRUE(fin::FscDue(Kst("2026-11-02 09:30"), 0));
  TEST_ASSERT_FALSE(fin::FscDue(Kst("2026-11-02 09:30"), Kst("2026-10-30 14:00")));
  // Public-holiday weekday: the bare primitive still fires (calendar-free by
  // design), but the calendar layer suppresses the actual fetch.
  cal::YearTable tables[2];
  cal::Clear(tables[0], 2026);
  cal::Clear(tables[1], 2027);
  cal::SetHoliday(tables[0], 11, 2);  // Monday 2026-11-02
  tables[0].loaded = true;
  const int64_t holiday = Kst("2026-11-02 14:00");
  TEST_ASSERT_TRUE(fin::FscDue(holiday, 0));
  app::AppState hst;
  hst.settings = ConfiguredSettings();
  hst.settings.providers.fsc_enabled = true;
  hst.settings.watch_count = 1;
  hst.calendars[0] = tables[0];
  hst.calendars[1] = tables[1];
  TEST_ASSERT_TRUE(app::NextAttemptUtc(hst, Provider::FscClose, holiday, true) >
                   holiday);
}

#include "test_stage2.cpp"
#include "test_audit_runtime.inc"
#include "test_audit_storage.inc"
#include "test_audit_display.inc"
#include "test_audit_holiday.inc"
#include "test_audit_portal.inc"
#include "test_lan_portal.inc"
#include "test_lan_portal2.inc"
#include "test_lan_portal3.inc"
#include "test_ui_icons.inc"
#include "test_audit_flow.inc"
#include "test_audit_followup.inc"
#include "test_live_chunked.inc"
#include "test_audit_fixes.inc"
#include "test_long_term.inc"
#include "test_long_term_followup.inc"
#include "test_audit_20261116.inc"
#include "test_device_docs_20261116.inc"

int main(int argc, char** argv) {
  (void)argc;
  (void)argv;
  UNITY_BEGIN();
  RUN_TEST(test_storage_fault_cooldown_and_recovery);
  RUN_TEST(test_failed_flash_attempts_exhaust_daily_budget);
  RUN_TEST(test_failed_flash_clock_reversal_does_not_open_budget_or_stall_forever);
  RUN_TEST(test_charge_confirmation_is_atomic_with_status_read);
  RUN_TEST(test_failed_write_reservations_survive_next_verified_checkpoint);
  RUN_TEST(test_charge_threshold_hysteresis_and_register_preservation);
  RUN_TEST(test_charge_unsupported_id_and_otg_never_write);
  RUN_TEST(test_charge_invalid_samples_reset_confirmation);
  RUN_TEST(test_charge_bus_failure_and_write_readback_fail_closed);
  RUN_TEST(test_charge_restart_off_and_config_drift_recovery);
  RUN_TEST(test_charge_watchdog_command_not_replayed);
  RUN_TEST(test_settings_v4_protection_migration_and_fingerprint);
  RUN_TEST(test_charge_labels_fit_device_font_and_backup_roundtrip);
  RUN_TEST(test_golden_provider_cases);
  RUN_TEST(test_kst_boundaries);
  RUN_TEST(test_real_date_validation);
  RUN_TEST(test_kma_release_selection);
  RUN_TEST(test_exim_quoted);
  RUN_TEST(test_net_policy);
  RUN_TEST(test_endpoints);
  RUN_TEST(test_result_codes);
  RUN_TEST(test_settings_validation);
  RUN_TEST(test_watchlist);
  RUN_TEST(test_portal);
  RUN_TEST(test_scheduler_backoff_and_blocks);
  RUN_TEST(test_scheduler_release_and_market_gates);
  RUN_TEST(test_fetch_pass_end_to_end);
  RUN_TEST(test_fetch_exim_auth_error_stops_walk_back);
  RUN_TEST(test_fetch_kis_token_reissue_once);
  RUN_TEST(test_cache_bitflip_rejected);
  RUN_TEST(test_backoff_budgets);
  RUN_TEST(test_sensor_and_refresh);
  RUN_TEST(test_fsc_due_gate);
  RUN_TEST(test_codec_roundtrip);
  RUN_TEST(test_settings_codec);
  RUN_TEST(test_slotstore_powercut);
  RUN_TEST(test_snapshot_codec);
  RUN_TEST(test_clock_policy);
  RUN_TEST(test_pcf8563_bcd);
  RUN_TEST(test_buttons);
  RUN_TEST(test_sht4x);
  RUN_TEST(test_battery);
  RUN_TEST(test_calendar);
  RUN_TEST(test_viewmodel);
  RUN_TEST(test_canvas_renderer);
  RUN_TEST(test_cycle_planner);
  RUN_TEST(test_rtc_state);
  RUN_TEST(test_portal_handler);
  RUN_TEST(test_pmic_decode);
  RUN_TEST(test_geogrid);
  RUN_TEST(test_budget_demo_log);
  RUN_TEST(test_http_body_completion);
  RUN_TEST(test_chunked_body_success);
  RUN_TEST(test_chunked_body_rejects_bad_framing);
  RUN_TEST(test_chunked_body_limits_and_timeout);
  RUN_TEST(test_captured_live_responses_to_dashboard);
  RUN_TEST(test_captured_financial_responses);
  RUN_TEST(test_central_seoul_air_station_label);
  RUN_TEST(test_clock_bootstrap_rtc_without_ntp);
  RUN_TEST(test_slotstore_recovers_before_next_write);
  RUN_TEST(test_slotstore_torn_writes_repeated);
  RUN_TEST(test_settings_semantic_decode_atomic);
  RUN_TEST(test_runtime_checkpoint_restore_and_config_binding);
  RUN_TEST(test_uc8179_frame_and_timeout);
  RUN_TEST(test_bitmap_raster_real_glyphs);
  RUN_TEST(test_four_gray_raster_and_wire_planes);
  RUN_TEST(test_stock_names_table_and_glyphs);
  RUN_TEST(test_indoor_last_good_and_codec);
  RUN_TEST(test_market_all_watch_items_and_long_place);
  RUN_TEST(test_holiday_service);
  RUN_TEST(test_portal_full_settings_and_clear);
  RUN_TEST(test_portal_bounded_headers);
  RUN_TEST(test_portal_verified_settings_commit);
  RUN_TEST(test_icon_tables_and_draw);
  RUN_TEST(test_icon_view_model);
  RUN_TEST(test_card_renderer);
  RUN_TEST(test_low_battery_precedes_user_actions);
  RUN_TEST(test_local_due_does_not_connect_radio);
  RUN_TEST(test_calendar_rollover_and_exchange_provenance);
  RUN_TEST(test_fetch_deadline_and_forced_refresh_gates);
  RUN_TEST(test_72h_reboot_keeps_release_budget);
  RUN_TEST(test_fx_scale_is_bounded);
  RUN_TEST(test_per_provider_cap_defers_without_failure);
  RUN_TEST(test_demo_values_carry_demo_marker);
  RUN_TEST(test_kma_rejects_wrong_or_missing_source_issue);
  RUN_TEST(test_air_keeps_per_pollutant_time_in_ui);
  RUN_TEST(test_provider_failure_visible_until_success);
  RUN_TEST(test_cycle_real_orchestrator_success_and_recovery);
  RUN_TEST(test_cycle_low_power_and_untrusted_clock_gate);
  RUN_TEST(test_cycle_budget_and_render_retry);
  RUN_TEST(test_page_keys_never_call_api);
  RUN_TEST(test_manual_refresh_cooldown);
  RUN_TEST(test_change_rate_parse_format_and_row);
  RUN_TEST(test_refresh_scale_stretches_intervals_only);
  RUN_TEST(test_clock_only_change_does_not_present);
  RUN_TEST(test_kospi_index_parse);
  RUN_TEST(test_fetch_kospi_index_and_isolation);
  RUN_TEST(test_calendar_grid_flags_and_labels);
  RUN_TEST(test_calendar_edge_cases);
  RUN_TEST(test_calendar_names_fit_font);
  RUN_TEST(test_kospi_survives_snapshot_roundtrip);
  RUN_TEST(test_kospi_survives_runtime_checkpoint);
  RUN_TEST(test_full_snapshot_with_kospi_fits_buffers);
  RUN_TEST(test_kis_post_close_catchup);
  RUN_TEST(test_wifi_supervisor_policy);
  RUN_TEST(test_wifi_supervisor_redraw_is_debounced);
  RUN_TEST(test_redraw_request_triggers_one_render);
  RUN_TEST(test_settings_migration_rejects_without_mutation);
  RUN_TEST(test_market_fresher_close_replaces_stale_live);
  RUN_TEST(test_checkpoint_failure_is_distinct_from_defer);
  RUN_TEST(test_settings_v3_roundtrip_and_v2_migration);
  RUN_TEST(test_device_config_defaults_and_validation);
  RUN_TEST(test_page_rotation_policy);
  RUN_TEST(test_request_guard_host_origin_cookie);
  RUN_TEST(test_portal_auth_pairing_lockout_and_session);
  RUN_TEST(test_lan_handler_requires_login_host_and_origin);
  RUN_TEST(test_lan_handler_section_saves_are_scoped);
  RUN_TEST(test_lan_handler_device_section_and_rotation_settings);
  RUN_TEST(test_lan_handler_actions_logout_and_always_mode);
  RUN_TEST(test_device_tab_shows_portal_address_and_code);
  RUN_TEST(test_market_row_shows_fsc_close_next_to_live_price);
  RUN_TEST(test_lan_watchlist_row_editor);
  RUN_TEST(test_lan_wifi_scan_picker);
  RUN_TEST(test_backup_export_import_roundtrip_and_rejects);
  RUN_TEST(test_wifi_scan_flow_redirects_and_reports_state);
  RUN_TEST(test_watch_editor_live_names_via_app_js);
  RUN_TEST(test_status_page_multiline_and_menu_summary);
  RUN_TEST(test_location_lat_lon_to_grid);
  RUN_TEST(test_lan_window_extends_with_activity);
  RUN_TEST(test_lan_gate_closes_even_when_disconnected);
  RUN_TEST(test_data_fingerprint_ignores_device_settings);
  RUN_TEST(test_runtime_restore_survives_device_only_change);
  RUN_TEST(test_save_reports_whether_data_changed);
  RUN_TEST(test_kis_catchup_bounded_per_close);
  RUN_TEST(test_kis_partial_failure_keeps_other_symbols_and_index);
  RUN_TEST(test_fsc_partial_failure_records_success);
  RUN_TEST(test_symbol_batch_rules);
  RUN_TEST(test_auth_block_retries_daily_and_success_clears);
  RUN_TEST(test_rotation_respects_trading_day_and_portal);
  RUN_TEST(test_wifi_attempt_in_flight);
  RUN_TEST(test_wifi_scan_picker_overrides_prefilled_ssid_and_clears_open_secret);
  RUN_TEST(test_calendar_auth_error_retries_daily_and_clears_on_save);
  RUN_TEST(test_kis_4_symbols_with_token_reissue_records_mark_result);
  RUN_TEST(test_market_partial_symbol_failure_and_unconfigured_badges);
  RUN_TEST(test_trusted_clock_never_reapplies_or_rewrites_rtc);
  RUN_TEST(test_sleep_restores_charge_and_preserves_other_bits);
  RUN_TEST(test_sleep_charge_restore_failures_are_detected);
  RUN_TEST(test_config_boot_page_keeps_restored_data_and_sleep_page);
  RUN_TEST(test_untrusted_usb_clock_never_blocks_automatic_cycles);
  RUN_TEST(test_partial_refresh_forces_full_after_five);
  RUN_TEST(test_panel_temperature_operating_range);
  RUN_TEST(test_mid_provider_time_budget_defers_unsent_kis_request);
  RUN_TEST(test_deferred_token_reissue_does_not_consume_cooldown);
  RUN_TEST(test_next_year_empty_calendar_retries_daily);
  RUN_TEST(test_maximum_persistent_blobs_fit_nvs_with_rewrite_reserve);
  return UNITY_END();
}
