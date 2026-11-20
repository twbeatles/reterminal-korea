#include "ProviderJson.h"

#include <ArduinoJson.h>
#include <stdio.h>

#include <initializer_list>
#include <string.h>

#include "ResultCodes.h"
#include "../domain/Freshness.h"

namespace kdash {
namespace pj {
namespace {

constexpr size_t kMaxItems = 96;  // >= kma::kVillageNumRows, AirKorea 24 rows
constexpr size_t kNumBuf = 24;

// Per-call scratch for numbers rendered to text (single-threaded firmware;
// the adapter is not reentrant by design).
struct Scratch {
  char num[kMaxItems][4][kNumBuf];
};
Scratch g_scratch;

// String or number -> text. Returns nullptr for null/absent/other types.
const char* Text(JsonVariantConst v, char* buf, size_t cap) {
  if (v.isNull()) return nullptr;
  if (v.is<const char*>()) return v.as<const char*>();
  if (v.is<long>()) {
    snprintf(buf, cap, "%ld", v.as<long>());
    return buf;
  }
  if (v.is<double>()) {
    snprintf(buf, cap, "%.6g", v.as<double>());
    return buf;
  }
  return nullptr;
}

// Two spellings (lowercase/UPPERCASE) of the same key.
JsonVariantConst Either(JsonObjectConst o, const char* a, const char* b) {
  JsonVariantConst v = o[a];
  return v.isNull() ? o[b] : v;
}

// Envelope status for data.go.kr style responses.
struct Envelope {
  char code[16] = {0};
  bool xml = false;
  bool parsed = false;
};

// XML error body (HTTP 200): pull returnReasonCode / resultCode.
bool XmlEnvelope(const char* body, Envelope& env) {
  if (!codes::LooksLikeXml(body)) return false;
  env.xml = true;
  if (!codes::XmlTagText(body, "returnReasonCode", env.code, sizeof(env.code)))
    codes::XmlTagText(body, "resultCode", env.code, sizeof(env.code));
  return true;
}

// Bodies from IHttpClient are NUL-terminated; guard anyway.
bool Usable(const char* body, size_t len) { return body && len > 0; }

// items.item may be an array or a single object (XML->JSON quirk).
template <typename F>
void ForEachItem(JsonVariantConst items, F&& f) {
  if (items.is<JsonArrayConst>()) {
    size_t i = 0;
    for (JsonVariantConst it : items.as<JsonArrayConst>()) {
      if (i >= kMaxItems) break;
      if (it.is<JsonObjectConst>()) f(i++, it.as<JsonObjectConst>());
    }
  } else if (items.is<JsonObjectConst>()) {
    f(0, items.as<JsonObjectConst>());
  }
}

// Parses a data.go.kr JSON envelope. Fills env.code; returns the document
// root "response" (or the root itself for envelope-less variants).
JsonVariantConst DataGoKrRoot(JsonDocument& doc, Envelope& env) {
  JsonVariantConst root = doc["response"];
  if (root.isNull()) root = doc.as<JsonVariantConst>();
  char b[kNumBuf];
  const char* code = Text(root["header"]["resultCode"], b, sizeof(b));
  if (code) strncpy(env.code, code, sizeof(env.code) - 1);
  env.parsed = true;
  return root;
}

DeserializationError ParseFiltered(JsonDocument& doc, const char* body,
                                   size_t len, const JsonDocument& filter) {
  return deserializeJson(doc, body, len, DeserializationOption::Filter(filter),
                         DeserializationOption::NestingLimit(12));
}

kma::RawItem g_items[kMaxItems];

size_t KmaItems(JsonVariantConst root, int date, int time, int nx, int ny,
                bool& valid) {
  size_t n = 0;
  valid = true;
  int first_x = -1, first_y = -1;
  const auto items = root["body"]["items"]["item"];
  if (items.is<JsonArrayConst>() && items.size() > kMaxItems) valid = false;
  ForEachItem(root["body"]["items"]["item"], [&](size_t i, JsonObjectConst o) {
    // Source identity is checked before any value reaches the portable parser.
    // Numeric grid fields are documented; numeric strings are accepted too.
    char bd[kNumBuf], bt[kNumBuf], gx[kNumBuf], gy[kNumBuf];
    const char* ds = Text(o["baseDate"], bd, sizeof(bd));
    const char* ts = Text(o["baseTime"], bt, sizeof(bt));
    auto grid = [](const char* s) {
      int v = 0;
      if (!s || !*s) return -1;
      for (; *s; ++s) {
        if (*s < '0' || *s > '9' || v > 200) return -1;
        v = v * 10 + *s - '0';
      }
      return v >= 1 && v <= 200 ? v : -1;
    };
    const int x = grid(Text(o["nx"], gx, sizeof(gx)));
    const int y = grid(Text(o["ny"], gy, sizeof(gy)));
    if (first_x < 0) { first_x = x; first_y = y; }
    if (KstStrToUtc(ds, ts) != KstYmdHmToUtc(date, time) ||
        KstYmdHmToUtc(date, time) <= 0 || x < 0 || y < 0 ||
        x != first_x || y != first_y ||
        (nx >= 0 && x != nx) || (ny >= 0 && y != ny)) valid = false;
    kma::RawItem& r = g_items[i];
    r.category = o["category"].as<const char*>();
    JsonVariantConst v = o["obsrValue"];
    if (v.isNull()) v = o["fcstValue"];
    r.value = Text(v, g_scratch.num[i][0], kNumBuf);
    r.fcst_date = Text(o["fcstDate"], g_scratch.num[i][1], kNumBuf);
    r.fcst_time = Text(o["fcstTime"], g_scratch.num[i][2], kNumBuf);
    n = i + 1;
  });
  return n;
}

const char* EnvCode(const Envelope& env) {
  return env.code[0] ? env.code : nullptr;
}

}  // namespace

kma::NowOut KmaUltraNow(const char* body, size_t len, int base_yyyymmdd,
                        int base_hhmm, const FetchContext& ctx, int nx, int ny) {
  Envelope env;
  if (!Usable(body, len) || XmlEnvelope(body, env)) {
    return kma::ParseUltraNow(nullptr, 0, EnvCode(env), true, base_yyyymmdd,
                              base_hhmm, ctx);
  }
  JsonDocument filter;
  filter["response"]["header"] = true;
  // `item` may be an array or a single object: an array-shaped filter would
  // drop the object form, so keep the whole (size-capped) item subtree.
  filter["response"]["body"]["items"]["item"] = true;
  JsonDocument doc;
  if (ParseFiltered(doc, body, len, filter)) {
    return kma::ParseUltraNow(nullptr, 0, nullptr, false, base_yyyymmdd,
                              base_hhmm, ctx);  // schema failure
  }
  JsonVariantConst root = DataGoKrRoot(doc, env);
  bool valid = false;
  size_t n = KmaItems(root, base_yyyymmdd, base_hhmm, nx, ny, valid);
  if (!valid && codes::DataGoKr(EnvCode(env)) == Outcome::Ok)
    return kma::ParseUltraNow(nullptr, 0, nullptr, false, base_yyyymmdd, base_hhmm, ctx);
  return kma::ParseUltraNow(g_items, n, EnvCode(env), false, base_yyyymmdd,
                            base_hhmm, ctx);
}

kma::ForecastOut KmaVillage(const char* body, size_t len, int base_yyyymmdd,
                            int base_hhmm, const FetchContext& ctx, int nx, int ny) {
  Envelope env;
  if (!Usable(body, len) || XmlEnvelope(body, env)) {
    return kma::ParseVillage(nullptr, 0, EnvCode(env), true, base_yyyymmdd,
                             base_hhmm, ctx);
  }
  JsonDocument filter;
  filter["response"]["header"] = true;
  filter["response"]["body"]["items"]["item"] = true;  // array or object
  JsonDocument doc;
  if (ParseFiltered(doc, body, len, filter)) {
    return kma::ParseVillage(nullptr, 0, nullptr, false, base_yyyymmdd,
                             base_hhmm, ctx);
  }
  JsonVariantConst root = DataGoKrRoot(doc, env);
  bool valid = false;
  size_t n = KmaItems(root, base_yyyymmdd, base_hhmm, nx, ny, valid);
  if (!valid && codes::DataGoKr(EnvCode(env)) == Outcome::Ok)
    return kma::ParseVillage(nullptr, 0, nullptr, false, base_yyyymmdd, base_hhmm, ctx);
  return kma::ParseVillage(g_items, n, EnvCode(env), false, base_yyyymmdd,
                           base_hhmm, ctx);
}

air::AirOut AirKorea(const char* body, size_t len, const char* station,
                     const FetchContext& ctx) {
  Envelope env;
  if (!Usable(body, len) || XmlEnvelope(body, env)) {
    return air::Parse(nullptr, 0, EnvCode(env), true, station, ctx);
  }
  JsonDocument filter;
  filter["response"]["header"] = true;
  filter["response"]["body"]["items"] = true;  // array (or nested item)
  JsonDocument doc;
  if (ParseFiltered(doc, body, len, filter)) {
    return air::Parse(nullptr, 0, nullptr, false, station, ctx);
  }
  JsonVariantConst root = DataGoKrRoot(doc, env);
  static air::Row rows[kMaxItems];
  size_t n = 0;
  JsonVariantConst items = root["body"]["items"];
  if (items.is<JsonObjectConst>() && !items["item"].isNull())
    items = items["item"];  // tolerate the KMA-style nesting
  ForEachItem(items, [&](size_t i, JsonObjectConst o) {
    air::Row& r = rows[i];
    r.station = o["stationName"].as<const char*>();
    r.data_time = o["dataTime"].as<const char*>();
    r.pm10_value = Text(o["pm10Value"], g_scratch.num[i][0], kNumBuf);
    r.pm25_value = Text(o["pm25Value"], g_scratch.num[i][1], kNumBuf);
    n = i + 1;
  });
  return air::Parse(rows, n, EnvCode(env), false, station, ctx);
}

exim::EximOut Exim(const char* body, size_t len, int searchdate_yyyymmdd,
                   const FetchContext& ctx) {
  static exim::Entry entries[kMaxItems];
  if (!Usable(body, len) || codes::LooksLikeXml(body)) {
    // Not the documented JSON array: never treat as holiday.
    exim::Entry e;
    e.result = -1;
    return exim::Parse(&e, 1, searchdate_yyyymmdd, ctx);
  }
  JsonDocument filter;
  JsonObject f = filter.add<JsonObject>();
  for (const char* k : {"result", "RESULT", "cur_unit", "CUR_UNIT",
                        "deal_bas_r", "DEAL_BAS_R"})
    f[k] = true;
  JsonDocument doc;
  DeserializationError err = ParseFiltered(doc, body, len, filter);
  if (err || doc.isNull()) {
    // "null" body: the API has been observed to answer null outside
    // fixing hours — same meaning as an empty array (no fixing).
    if (!err && doc.isNull()) return exim::Parse(nullptr, 0, searchdate_yyyymmdd, ctx);
    exim::Entry e;
    e.result = -1;
    return exim::Parse(&e, 1, searchdate_yyyymmdd, ctx);
  }
  if (!doc.is<JsonArrayConst>()) {
    exim::Entry e;
    e.result = -1;
    return exim::Parse(&e, 1, searchdate_yyyymmdd, ctx);
  }
  size_t n = 0;
  for (JsonVariantConst v : doc.as<JsonArrayConst>()) {
    if (n >= kMaxItems) break;
    if (!v.is<JsonObjectConst>()) continue;
    JsonObjectConst o = v.as<JsonObjectConst>();
    exim::Entry& e = entries[n];
    char rb[kNumBuf];
    e.result = exim::ParseResultText(Text(Either(o, "result", "RESULT"), rb,
                                          sizeof(rb)));
    e.cur_unit = Either(o, "cur_unit", "CUR_UNIT").as<const char*>();
    e.deal_bas_r = Text(Either(o, "deal_bas_r", "DEAL_BAS_R"),
                        g_scratch.num[n][0], kNumBuf);
    n++;
  }
  return exim::Parse(entries, n, searchdate_yyyymmdd, ctx);
}

fin::FscOut Fsc(const char* body, size_t len, const char* symbol,
                const char* market, const FetchContext& ctx) {
  Envelope env;
  if (!Usable(body, len) || XmlEnvelope(body, env)) {
    return fin::ParseFsc(nullptr, 0, EnvCode(env), true, symbol, market, ctx);
  }
  JsonDocument filter;
  filter["response"]["header"] = true;
  filter["response"]["body"]["items"]["item"] = true;  // array or object
  JsonDocument doc;
  if (ParseFiltered(doc, body, len, filter)) {
    return fin::ParseFsc(nullptr, 0, nullptr, false, symbol, market, ctx);
  }
  JsonVariantConst root = DataGoKrRoot(doc, env);
  static fin::FscRow rows[kMaxItems];
  size_t n = 0;
  ForEachItem(root["body"]["items"]["item"], [&](size_t i, JsonObjectConst o) {
    fin::FscRow& r = rows[i];
    r.bas_dt = Text(o["basDt"], g_scratch.num[i][0], kNumBuf);
    r.srtn_cd = Text(o["srtnCd"], g_scratch.num[i][1], kNumBuf);
    r.clpr = Text(o["clpr"], g_scratch.num[i][2], kNumBuf);
    n = i + 1;
  });
  return fin::ParseFsc(rows, n, EnvCode(env), false, symbol, market, ctx);
}

fin::KisOut Kis(int http_status, const char* body, size_t len,
                const char* symbol, const char* market,
                const FetchContext& ctx) {
  fin::KisQuoteBody b;
  b.http_status = http_status;
  JsonDocument filter;
  filter["rt_cd"] = true;
  filter["msg_cd"] = true;
  filter["error_code"] = true;
  filter["output"]["stck_prpr"] = true;
  filter["output"]["prdy_ctrt"] = true;
  filter["output"]["prdy_vrss_sign"] = true;
  JsonDocument doc;
  char pb[kNumBuf];
  char cb[kNumBuf];
  char sb[kNumBuf];
  if (Usable(body, len) && !ParseFiltered(doc, body, len, filter)) {
    b.rt_cd = doc["rt_cd"].as<const char*>();
    b.msg_cd = doc["msg_cd"].as<const char*>();
    if (!b.msg_cd) b.msg_cd = doc["error_code"].as<const char*>();
    b.stck_prpr = Text(doc["output"]["stck_prpr"], pb, sizeof(pb));
    b.prdy_ctrt = Text(doc["output"]["prdy_ctrt"], cb, sizeof(cb));
    b.prdy_vrss_sign = Text(doc["output"]["prdy_vrss_sign"], sb, sizeof(sb));
  }
  return fin::ParseKis(b, symbol, market, ctx);
}

fin::KisIndexOut KisIndex(int http_status, const char* body, size_t len,
                          const char* name, const FetchContext& ctx) {
  fin::KisIndexBody b;
  b.http_status = http_status;
  JsonDocument filter;
  filter["rt_cd"] = true;
  filter["msg_cd"] = true;
  filter["error_code"] = true;
  filter["output"]["bstp_nmix_prpr"] = true;
  filter["output"]["bstp_nmix_prdy_ctrt"] = true;
  filter["output"]["prdy_vrss_sign"] = true;
  JsonDocument doc;
  char pb[kNumBuf], cb[kNumBuf], sb[kNumBuf];
  if (Usable(body, len) && !ParseFiltered(doc, body, len, filter)) {
    b.rt_cd = doc["rt_cd"].as<const char*>();
    b.msg_cd = doc["msg_cd"].as<const char*>();
    if (!b.msg_cd) b.msg_cd = doc["error_code"].as<const char*>();
    b.bstp_nmix_prpr = Text(doc["output"]["bstp_nmix_prpr"], pb, sizeof(pb));
    b.bstp_nmix_prdy_ctrt = Text(doc["output"]["bstp_nmix_prdy_ctrt"], cb, sizeof(cb));
    b.prdy_vrss_sign = Text(doc["output"]["prdy_vrss_sign"], sb, sizeof(sb));
  }
  return fin::ParseKisIndex(b, name, ctx);
}

int64_t KisToken(int http_status, const char* body, size_t len,
                 const FetchContext& ctx, char* token_out, size_t token_cap,
                 Outcome& outcome) {
  if (token_out && token_cap) token_out[0] = '\0';
  JsonDocument filter;
  filter["access_token"] = true;
  filter["expires_in"] = true;
  filter["error_code"] = true;
  filter["msg_cd"] = true;
  JsonDocument doc;
  if (!Usable(body, len) || ParseFiltered(doc, body, len, filter)) {
    outcome = codes::Kis(http_status, nullptr, nullptr);
    if (outcome == Outcome::Ok) outcome = Outcome::ProviderError;
    return -1;
  }
  const char* err = doc["error_code"].as<const char*>();
  if (!err) err = doc["msg_cd"].as<const char*>();
  const char* tok = doc["access_token"].as<const char*>();
  long expires = doc["expires_in"].as<long>();
  int64_t expiry = fin::KisTokenExpiry(http_status, tok, expires, ctx);
  if (expiry < 0 || !token_out || strlen(tok) + 1 > token_cap) {
    outcome = codes::Kis(http_status, nullptr, err);
    if (outcome == Outcome::Ok) outcome = Outcome::ProviderError;
    // 403 with an unknown code on tokenP == rejected app key/secret.
    if (http_status == 403 && outcome != Outcome::RateLimited)
      outcome = Outcome::AuthError;
    return -1;
  }
  memcpy(token_out, tok, strlen(tok) + 1);
  outcome = Outcome::Ok;
  return expiry;
}

}  // namespace pj
}  // namespace kdash
