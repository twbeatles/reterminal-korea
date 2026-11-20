#include "Fetcher.h"

#include <stdio.h>
#include <string.h>

#include "../domain/Freshness.h"
#include "../net/Endpoints.h"
#include "../net/NetPolicy.h"
#include "../drivers/SensorCheck.h"
#include "RequestBudget.h"
#include "SymbolBatch.h"

namespace kdash {
namespace app {
namespace {

using sched::Provider;

constexpr size_t kUrlCap = 1400;  // 3x-encoded 256B key + path

struct Pass {
  AppState& st;
  Snapshot& snap;
  KisSession& kis;
  IHttpClient& http;
  const char* ca;
  const FetchContext& ctx;
  bool usb;
  int jitter;
  PassLog* log;
  IFetchControl* control;
  int requests = 0;
  Provider current = Provider::Count;    // Fetch* currently spending
  int spent[(int)Provider::Count] = {};  // per-provider requests this pass
  bool deferred = false;  // cap hit: retried next pass, never a failure
};

struct Call {
  HttpResponse resp;
  Outcome transport = Outcome::ProviderError;
  int retry_after = -1;
};

bool Budget(Pass& p) {
  return p.requests < kMaxRequestsPerPass &&
         (!p.control || p.control->CanRequest());
}

// Per-provider cap (RequestBudget): at most CapFor(p) requests per pass.
// On a cap hit the request is refused and p.deferred is set; Finish then
// skips MarkResult so the deferral is retried next pass, never a failure.
bool SpendSlot(Pass& p) {
  const int ci = static_cast<int>(p.current);
  if (ci < 0 || ci >= static_cast<int>(Provider::Count)) return true;
  if (budget::ShouldDefer(p.current, p.spent[ci], p.requests,
                           kMaxRequestsPerPass)) {
    p.deferred = true;
    return false;
  }
  p.spent[ci]++;
  return true;
}

Call Get(Pass& p, const char* url, const char* const* headers, size_t nh) {
  Call c;
  if (!net::HostAllowed(url)) return c;
  if (!Budget(p)) { p.deferred = true; return c; }
  if (!SpendSlot(p)) return c;
  p.requests++;
  c.resp = p.http.get(url, headers, nh, p.ca);
  c.transport = ClassifyHttp(c.resp);
  c.retry_after = net::ParseRetryAfter(c.resp.retry_after);
  return c;
}

Call Post(Pass& p, const char* url, const char* const* headers, size_t nh,
          const char* body) {
  Call c;
  if (!net::HostAllowed(url)) return c;
  if (!Budget(p)) { p.deferred = true; return c; }
  if (!SpendSlot(p)) return c;
  p.requests++;
  c.resp = p.http.post(url, headers, nh, body, p.ca);
  c.transport = ClassifyHttp(c.resp);
  c.retry_after = net::ParseRetryAfter(c.resp.retry_after);
  return c;
}

void Log(Pass& p, Provider prov, Outcome o, int status, int reqs) {
  if (!p.log || p.log->count >= (int)Provider::Count) return;
  p.log->entries[p.log->count++] = PassEntry{prov, o, status, reqs};
}

int TodayKst(int64_t now_utc) {
  KstBroken k = BreakKst(now_utc);
  return k.year * 10000 + k.month * 100 + k.day;
}

int ShiftDays(int yyyymmdd, int days) {
  int64_t d = EpochFromCivil(yyyymmdd / 10000, (yyyymmdd / 100) % 100,
                             yyyymmdd % 100) / 86400 + days;
  int y, m, dd;
  CivilFromEpochDays(d, y, m, dd);
  return y * 10000 + m * 100 + dd;
}

void Finish(Pass& p, Provider prov, Outcome o, const Call& last, int reqs,
            int64_t base_utc = 0) {
  if (p.deferred) {  // cap hit: hold last-good, retry next pass, no failure
    p.deferred = false;
    Log(p, prov, o, last.resp.status, reqs);
    return;
  }
  MarkResult(p.st, prov, o, p.ctx.now_utc, p.usb, last.retry_after, p.jitter,
             base_utc);
  Log(p, prov, o, last.resp.status, reqs);
}

// --- KMA ------------------------------------------------------------------

void FetchKmaNow(Pass& p) {
  p.current = Provider::KmaNow;
  p.deferred = false;
  const Settings& s = p.st.settings;
  int d = 0, t = 0;
  kma::SelectUltraNowBase(p.ctx.now_utc, d, t);
  const int start = p.requests;
  Call c;
  kma::NowOut out;
  // NODATA right after the assumed release delay (U-05) => previous hour once.
  for (int attempt = 0; attempt < 2; attempt++) {
    char url[kUrlCap];
    if (!endpoints::KmaUltraUrl(s.providers.kma_key, d, t, s.location.grid_x,
                                s.location.grid_y, kma::kUltraNowNumRows, url,
                                sizeof(url))) {
      Finish(p, Provider::KmaNow, Outcome::AuthError, c, 0);  // malformed key
      return;
    }
    c = Get(p, url, nullptr, 0);
    if (c.transport != Outcome::Ok) break;
    out = pj::KmaUltraNow(c.resp.body, c.resp.body_len, d, t, p.ctx,
                          s.location.grid_x, s.location.grid_y);
    if (out.outcome != Outcome::NoData || attempt == 1) break;
    int64_t prev = KstYmdHmToUtc(d, t) - 3600;
    kma::SelectUltraNowBase(prev + kma::kUltraNowDelayMin * 60, d, t);
  }
  const Outcome o = c.transport != Outcome::Ok ? c.transport : out.outcome;
  if (o == Outcome::Ok) {
    p.snap.now = out;
    p.snap.has_now = true;
  }
  Finish(p, Provider::KmaNow, o, c, p.requests - start,
         o == Outcome::Ok ? out.base_utc : 0);
}

void FetchKmaForecast(Pass& p) {
  p.current = Provider::KmaForecast;
  p.deferred = false;
  const Settings& s = p.st.settings;
  int d = 0, t = 0;
  kma::SelectVillageBase(p.ctx.now_utc, d, t);
  const int start = p.requests;
  Call c;
  kma::ForecastOut out;
  for (int attempt = 0; attempt < 2; attempt++) {
    char url[kUrlCap];
    if (!endpoints::KmaVillageUrl(s.providers.kma_key, d, t, s.location.grid_x,
                                  s.location.grid_y, kma::kVillageNumRows, url,
                                  sizeof(url))) {
      Finish(p, Provider::KmaForecast, Outcome::AuthError, c, 0);
      return;
    }
    c = Get(p, url, nullptr, 0);
    if (c.transport != Outcome::Ok) break;
    out = pj::KmaVillage(c.resp.body, c.resp.body_len, d, t, p.ctx,
                         s.location.grid_x, s.location.grid_y);
    if (out.outcome != Outcome::NoData || attempt == 1) break;
    // Previous 회차 (3h earlier), e.g. release delay longer than assumed.
    int64_t prev = KstYmdHmToUtc(d, t) - 3 * 3600;
    kma::SelectVillageBase(prev + kma::kVillageDelayMin * 60, d, t);
  }
  const Outcome o = c.transport != Outcome::Ok ? c.transport : out.outcome;
  if (o == Outcome::Ok) {
    p.snap.fc = out;
    p.snap.has_fc = true;
  }
  Finish(p, Provider::KmaForecast, o, c, p.requests - start,
         o == Outcome::Ok ? out.base_utc : 0);
}

// --- AirKorea ---------------------------------------------------------------

void FetchAir(Pass& p) {
  p.current = Provider::AirKorea;
  p.deferred = false;
  const Settings& s = p.st.settings;
  char url[kUrlCap];
  Call c;
  if (!endpoints::AirKoreaCandidateUrl(s.providers.air_key,
                                       s.location.air_station, url,
                                       sizeof(url))) {
    Finish(p, Provider::AirKorea, Outcome::AuthError, c, 0);
    return;
  }
  const int start = p.requests;
  c = Get(p, url, nullptr, 0);
  Outcome o = c.transport;
  if (o == Outcome::Ok) {
    air::AirOut out = pj::AirKorea(c.resp.body, c.resp.body_len,
                                   s.location.air_station, p.ctx);
    o = out.outcome;
    if (o == Outcome::Ok) {
      p.snap.air = out;
      p.snap.has_air = true;
    }
  }
  Finish(p, Provider::AirKorea, o, c, p.requests - start);
}

// --- EXIM (walk back over holidays, EXIM-02) ---------------------------------

bool KnownEmpty(const Snapshot& s, int day) {
  for (int d : s.fx_empty_days)
    if (d == day) return true;
  return false;
}

void RememberEmpty(Snapshot& s, int day) {
  if (KnownEmpty(s, day)) return;
  // Keep the newest kEximFallbackMaxDays entries (drop the oldest).
  int oldest = 0;
  for (int i = 1; i < kEximFallbackMaxDays; i++)
    if (s.fx_empty_days[i] < s.fx_empty_days[oldest]) oldest = i;
  if (s.fx_empty_days[oldest] < day) s.fx_empty_days[oldest] = day;
}

void FetchExim(Pass& p) {
  p.current = Provider::Exim;
  p.deferred = false;
  const Settings& s = p.st.settings;
  const int today = TodayKst(p.ctx.now_utc);
  int dates[kEximFallbackMaxDays];
  size_t nd = exim::PlanSearchDates(today,
                                    kEximFallbackMaxDays, dates,
                                    kEximFallbackMaxDays);
  const int start = p.requests;
  Call c;
  Outcome o = Outcome::NoData;
  for (size_t i = 0; i < nd; i++) {
    const cal::DayKind day = cal::ClassifyKstDay(KstYmdHmToUtc(dates[i], 0),
                                               p.st.calendars, 2);
    if (day == cal::DayKind::PublicHoliday || day == cal::DayKind::Weekend)
      continue;
    // Already holding this (or a newer) fixing => nothing new further back.
    const int64_t day_utc = KstYmdHmToUtc(dates[i], 0);
    if (p.snap.has_fx && p.snap.fx.fixing_date_utc >= day_utc) {
      o = Outcome::Ok;
      break;
    }
    if (dates[i] < today && KnownEmpty(p.snap, dates[i])) continue;
    char url[kUrlCap];
    if (!endpoints::EximUrl(s.providers.exim_key, dates[i], url, sizeof(url))) {
      o = Outcome::AuthError;
      break;
    }
    c = Get(p, url, nullptr, 0);
    if (c.transport != Outcome::Ok) {
      o = c.transport;
      break;
    }
    exim::EximOut out = pj::Exim(c.resp.body, c.resp.body_len, dates[i], p.ctx);
    o = out.outcome;
    if (out.empty_response) {  // holiday / not fixed yet
      if (dates[i] < today) RememberEmpty(p.snap, dates[i]);
      continue;
    }
    if (o == Outcome::Ok) {
      p.snap.fx = out;
      p.snap.has_fx = true;
    }
    break;  // Ok or a real error (auth/quota) => stop walking back
  }
  Finish(p, Provider::Exim, o, c, p.requests - start);
}

// --- FSC --------------------------------------------------------------------

void FetchFsc(Pass& p) {
  p.current = Provider::FscClose;
  p.deferred = false;
  const Settings& s = p.st.settings;
  const int start = p.requests;
  const int begin = ShiftDays(TodayKst(p.ctx.now_utc), -14);
  Call c;
  SymbolBatch batch;  // 종목별 실패는 계속, 호출 보호 오류만 멈춤 (ADR 0026)
  for (size_t i = 0; !batch.stopped && i < s.watch_count && i < kMaxWatchlist; i++) {
    const WatchItem& w = s.watchlist[i];
    char url[kUrlCap];
    if (!endpoints::FscPriceUrl(s.providers.kma_key, w.symbol, begin, url,
                                sizeof(url))) {
      batch.Stop(Outcome::AuthError);  // key cannot be encoded: same for every symbol
      break;
    }
    c = Get(p, url, nullptr, 0);
    const bool transport = p.deferred || c.resp.error != HttpError::None || !c.resp.body;
    Outcome o = c.transport;
    if (o == Outcome::Ok && !transport) {
      fin::FscOut out = pj::Fsc(c.resp.body, c.resp.body_len, w.symbol,
                                MarketName(w.market), p.ctx);
      o = out.outcome;
      if (o == Outcome::Ok) {
        p.snap.fsc[i] = out;
        p.snap.has_fsc[i] = true;
      }
    }
    batch.Add(i, o, transport);
  }
  if (!batch.stopped) p.st.symbol_fail_mask[(int)Provider::FscClose] = batch.fail_mask;
  Finish(p, Provider::FscClose, batch.Result(), c, p.requests - start);
}

// --- KIS --------------------------------------------------------------------

Outcome EnsureKisToken(Pass& p, Call& c) {
  KisSession& k = p.kis;
  if (k.token[0] && k.expiry_utc > p.ctx.now_utc) return Outcome::Ok;
  if (k.last_issue_attempt_utc &&
      p.ctx.now_utc - k.last_issue_attempt_utc < 65)
    return Outcome::RateLimited;  // tokenP: 1분당 1회
  const ProviderKeys& keys = p.st.settings.providers;
  char url[128];
  char body[kMaxKeyLen * 2 + 128];
  if (!endpoints::KisTokenUrl(url, sizeof(url)) ||
      !endpoints::KisTokenBody(keys.kis_app_key, keys.kis_app_secret, body,
                               sizeof(body)))
    return Outcome::AuthError;
  static const char* const kHeaders[] = {
      "Content-Type: application/json; charset=utf-8"};
  const int before = p.requests;
  c = Post(p, url, kHeaders, 1, body);
  if (p.requests != before) k.last_issue_attempt_utc = p.ctx.now_utc;
  // Error bodies (403 EGW00133 etc.) carry the reason; parse them too.
  if (c.resp.error != HttpError::None || !c.resp.body)
    return c.transport == Outcome::Ok ? Outcome::ProviderError : c.transport;
  Outcome o = Outcome::ProviderError;
  int64_t exp = pj::KisToken(c.resp.status, c.resp.body, c.resp.body_len,
                             p.ctx, k.token, sizeof(k.token), o);
  if (exp < 0) {
    k.token[0] = '\0';
    return o;
  }
  k.expiry_utc = exp;
  return Outcome::Ok;
}

void FetchKis(Pass& p) {
  p.current = Provider::KisPrice;
  p.deferred = false;
  const Settings& s = p.st.settings;
  const ProviderKeys& keys = s.providers;
  const int start = p.requests;
  Call c;
  SymbolBatch batch;  // 종목별 실패는 계속, 호출 보호 오류만 멈춤 (ADR 0026, ISSUE-009)
  const Outcome issued = EnsureKisToken(p, c);
  if (issued != Outcome::Ok) batch.Stop(issued);
  bool reissued = false;
  for (size_t i = 0; !batch.stopped && i < s.watch_count && i < kMaxWatchlist; i++) {
    const WatchItem& w = s.watchlist[i];
    char url[kUrlCap];
    if (!endpoints::KisPriceUrl(w.symbol, MarketName(w.market), url,
                                sizeof(url))) {
      batch.Add(i, Outcome::ProviderError, false);  // this symbol only
      continue;
    }
    static char auth[1100], appkey[kMaxKeyLen + 16], appsecret[kMaxKeyLen + 16];
    snprintf(auth, sizeof(auth), "authorization: Bearer %s", p.kis.token);
    snprintf(appkey, sizeof(appkey), "appkey: %s", keys.kis_app_key);
    snprintf(appsecret, sizeof(appsecret), "appsecret: %s", keys.kis_app_secret);
    const char* headers[] = {
        "content-type: application/json; charset=utf-8", auth, appkey,
        appsecret, "tr_id: FHKST01010100", "custtype: P"};
    c = Get(p, url, headers, sizeof(headers) / sizeof(headers[0]));
    // KIS reports token/rate errors as HTTP 500 with a JSON body: parse
    // any response that has a body, not only 200s.
    const bool transport = p.deferred || c.resp.error != HttpError::None || !c.resp.body;
    Outcome o = c.transport;
    if (!transport) {
      fin::KisOut out = pj::Kis(c.resp.status, c.resp.body, c.resp.body_len,
                                w.symbol, MarketName(w.market), p.ctx);
      o = out.outcome;
      if (out.token_invalid && !reissued) {
        reissued = true;  // KIS-01: 재발급 1회만
        p.kis.token[0] = '\0';
        p.kis.expiry_utc = 0;
        p.kis.last_issue_attempt_utc = 0;
        Outcome t = EnsureKisToken(p, c);
        if (t != Outcome::Ok) {
          batch.Stop(t);
          break;
        }
        i--;  // retry this symbol once with the new token
        continue;
      }
      if (o == Outcome::Ok) {
        p.snap.kis[i] = out;
        p.snap.has_kis[i] = true;
      }
    }
    batch.Add(i, o, transport);
  }
  if (!batch.stopped) p.st.symbol_fail_mask[(int)Provider::KisPrice] = batch.fail_mask;
  // 코스피 지수: 토큰이 유효하고 호출 보호 오류로 멈추지 않았으며 공급자 캡 여유가 남아 있을 때만
  // 1회 추가 요청(종목 4개가 토큰 재발급을 거쳐 모두 성공한 경우 지수 요청 때문에 p.deferred가 켜져
  // MarkResult가 누락되는 현상 방지, ISSUE-013).
  if (!batch.stopped && !p.deferred && Budget(p) &&
      !budget::ShouldDefer(p.current, p.spent[(int)p.current], p.requests,
                           kMaxRequestsPerPass)) {
    char url[kUrlCap];
    if (endpoints::KisIndexUrl("0001", url, sizeof(url))) {
      static char auth[1100], appkey[kMaxKeyLen + 16], appsecret[kMaxKeyLen + 16];
      snprintf(auth, sizeof(auth), "authorization: Bearer %s", p.kis.token);
      snprintf(appkey, sizeof(appkey), "appkey: %s", keys.kis_app_key);
      snprintf(appsecret, sizeof(appsecret), "appsecret: %s", keys.kis_app_secret);
      const char* headers[] = {
          "content-type: application/json; charset=utf-8", auth, appkey,
          appsecret, "tr_id: FHPUP02100000", "custtype: P"};
      Call ic = Get(p, url, headers, sizeof(headers) / sizeof(headers[0]));
      if (ic.resp.error == HttpError::None && ic.resp.body) {
        fin::KisIndexOut io = pj::KisIndex(ic.resp.status, ic.resp.body,
                                           ic.resp.body_len, "KOSPI", p.ctx);
        if (io.outcome == Outcome::Ok) {
          p.snap.kospi = io;
          p.snap.has_kospi = true;
        }
      }
    }
  }
  Finish(p, Provider::KisPrice, batch.Result(), c, p.requests - start);
}

template <typename T>
void Regrade(DataPoint<T>& d, int64_t ttl, const FetchContext& ctx) {
  if (!d.has_value) return;
  d.validity = ClassifyFreshness(d.observed_at.epoch_utc, ctx.now_utc, ttl,
                                 ctx.clock_verified);
}

}  // namespace

Outcome ClassifyHttp(const HttpResponse& r) {
  switch (r.error) {
    case HttpError::None:
      break;
    default:
      return Outcome::ProviderError;  // DNS/TLS/timeout/oversize/transport
  }
  if (r.status == 429) return Outcome::RateLimited;
  if (r.status == 401 || r.status == 403) return Outcome::AuthError;
  if (r.status != 200) return Outcome::ProviderError;
  if (!net::BodySizeAllowed(r.body_len, kBodyMaxBytes)) {
    return Outcome::ProviderError;
  }
  return Outcome::Ok;
}

void RunFetchPass(AppState& st, Snapshot& snap, KisSession& kis,
                  IHttpClient& http, const char* ca_bundle,
                  const FetchContext& ctx, bool usb, int jitter_slot_ms,
                  PassLog* log, IFetchControl* control, bool force) {
  if (log) *log = PassLog{};
  Pass p{st, snap, kis, http, ca_bundle, ctx, usb, jitter_slot_ms, log, control};
  Provider due[(int)Provider::Count];
  const int n = DueProviders(st, ctx.now_utc, usb, due, (int)Provider::Count, force);
  for (int i = 0; i < n && Budget(p); i++) {
    switch (due[i]) {
      case Provider::KmaNow: FetchKmaNow(p); break;
      case Provider::KmaForecast: FetchKmaForecast(p); break;
      case Provider::AirKorea: FetchAir(p); break;
      case Provider::Exim: FetchExim(p); break;
      case Provider::FscClose: FetchFsc(p); break;
      case Provider::KisPrice: FetchKis(p); break;
      default: break;  // SHT40 is read by the board adapter, not over HTTP
    }
  }
  if (log) log->requests = p.requests;
  RefreshFreshness(snap, ctx);
}

void RefreshFreshness(Snapshot& s, const FetchContext& ctx) {
  Regrade(s.indoor_c, 30 * 60, ctx);
  Regrade(s.indoor_rh, 30 * 60, ctx);
  if (s.has_now) {
    Regrade(s.now.temp_c, kma::kUltraNowTtlSec, ctx);
    Regrade(s.now.humidity, kma::kUltraNowTtlSec, ctx);
    Regrade(s.now.rain_mm, kma::kUltraNowTtlSec, ctx);
  }
  if (s.has_fc) {
    Regrade(s.fc.tmp, kma::kVillageTtlSec, ctx);
    Regrade(s.fc.pop, kma::kVillageTtlSec, ctx);
    Regrade(s.fc.pty, kma::kVillageTtlSec, ctx);
    Regrade(s.fc.sky, kma::kVillageTtlSec, ctx);
  }
  if (s.has_air) {
    Regrade(s.air.pm10, air::kAirTtlSec, ctx);
    Regrade(s.air.pm25, air::kAirTtlSec, ctx);
  }
  if (s.has_fx) {
    Regrade(s.fx.usd, exim::kFixingTtlSec, ctx);
    Regrade(s.fx.jpy100, exim::kFixingTtlSec, ctx);
  }
  if (s.has_kospi) Regrade(s.kospi.quote, fin::kKisTtlSec, ctx);
  for (size_t i = 0; i < kMaxWatchlist; i++) {
    if (s.has_fsc[i]) Regrade(s.fsc[i].quote, fin::kFscTtlSec, ctx);
    if (s.has_kis[i]) Regrade(s.kis[i].quote, fin::kKisTtlSec, ctx);
  }
}

void RecordIndoor(AppState& st, Snapshot& snap, bool read_ok, double temp_c,
                  double rh, const FetchContext& ctx, bool usb) {
  // Accept the sensor's operating range and a conservative indoor range;
  // comparisons also reject NaN without manufacturing a measurement.
  const bool good = read_ok &&
      drivers::CheckSensorRange((float)temp_c, (float)rh) == drivers::SensorCheck::Ok &&
      drivers::IsIndoorPlausible((float)temp_c);
  st.sensor_present = read_ok;
  if (good) {
    DataPoint<double>* points[] = {&snap.indoor_c, &snap.indoor_rh};
    const double values[] = {temp_c, rh};
    for (int i=0;i<2;i++) {
      points[i]->has_value = true;
      points[i]->value = values[i];
      points[i]->source = "실내 센서";
      points[i]->observed_at = SourceTs(ctx.now_utc);
      points[i]->fetched_at = DeviceTs(ctx);
      points[i]->validity = ctx.clock_verified ? Validity::Valid : Validity::Stale;
    }
  }
  MarkResult(st, Provider::Sht40, good ? Outcome::Ok : Outcome::ProviderError,
             ctx.now_utc, usb, -1, 0);
}

}  // namespace app
}  // namespace kdash
