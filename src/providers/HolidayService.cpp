#include "HolidayService.h"

#include <ArduinoJson.h>
#include <stdio.h>
#include <string.h>

#include "../../include/Config.h"
#include "../net/Endpoints.h"
#include "../net/NetPolicy.h"
#include "../domain/Freshness.h"
#include "ResultCodes.h"

// Primary contract read 2026-11-02: official 15012690 v1.4 technical attachment
// https://www.data.go.kr/cmm/cmm/fileDownload.do?atchFileId=FILE_000000002633688&fileDetailSn=1
// HTTPS base confirmed by the official detail page:
// https://www.data.go.kr/tcs/dss/selectApiDataDetailView.do?publicDataPk=15012690
// No KRX-specific closure dates are inferred from this public holiday service.
namespace kdash {
namespace holidays {
namespace {
constexpr int kRows = 128;
bool ValidYear(int year) { return year >= 2000 && year <= 2099; }

bool Integer(JsonVariantConst value, int& out) {
  if (value.is<int>()) {
    out = value.as<int>();
    return true;
  }
  const char* text = value.as<const char*>();
  if (!text || !*text || strlen(text) > 8) return false;
  int n = 0;
  for (const char* p = text; *p; ++p) {
    if (*p < '0' || *p > '9') return false;
    n = n * 10 + *p - '0';
  }
  out = n;
  return true;
}

bool Date(int ymd, int year, int& month, int& day) {
  if (ymd / 10000 != year) return false;
  month = ymd / 100 % 100;
  day = ymd % 100;
  if (month < 1 || month > 12 || day < 1 || day > 31) return false;
  int y, m, d;
  CivilFromEpochDays(EpochFromCivil(year, month, day) / 86400, y, m, d);
  return y == year && m == month && d == day;
}
}

bool HolidayYearUrl(const char* key, int year, char* out, size_t cap) {
  if (!out || !cap || !ValidYear(year)) return false;
  char encoded[kMaxKeyLen * 3 + 1];
  if (!endpoints::NormalizeServiceKey(key, encoded, sizeof(encoded))) return false;
  int n = snprintf(out, cap,
      "https://apis.data.go.kr/B090041/openapi/service/SpcdeInfoService/getRestDeInfo"
      "?ServiceKey=%s&solYear=%04d&pageNo=1&numOfRows=%d&_type=json",
      encoded, year, kRows);
  return n > 0 && (size_t)n < cap && net::HostAllowed(out);
}

Outcome ParseHolidayYear(const char* body, size_t len, int year,
                         const FetchContext& ctx, cal::YearTable& out) {
  if (!body || !len || len > kBodyMaxBytes || !ValidYear(year) || !ctx.clock_verified)
    return Outcome::ProviderError;
  if (codes::LooksLikeXml(body)) {
    char code[16] = {};
    if (!codes::XmlTagText(body, "returnReasonCode", code, sizeof(code)))
      codes::XmlTagText(body, "resultCode", code, sizeof(code));
    Outcome o = codes::DataGoKr(code);
    return o == Outcome::Ok ? Outcome::ProviderError : o;
  }
  JsonDocument filter;
  filter["response"]["header"]["resultCode"] = true;
  filter["response"]["body"]["totalCount"] = true;
  filter["response"]["body"]["pageNo"] = true;
  filter["response"]["body"]["numOfRows"] = true;
  filter["response"]["body"]["items"]["item"] = true;
  JsonDocument doc;
  if (deserializeJson(doc, body, len, DeserializationOption::Filter(filter),
                      DeserializationOption::NestingLimit(12))) return Outcome::ProviderError;
  JsonVariantConst root = doc["response"];
  Outcome result = codes::DataGoKr(root["header"]["resultCode"].as<const char*>());
  if (result != Outcome::Ok) return result;
  JsonVariantConst payload = root["body"];
  int total = 0, page = 0, rows = 0;
  if (!Integer(payload["totalCount"], total) || total < 0 || total > kRows ||
      !Integer(payload["pageNo"], page) || page != 1 ||
      !Integer(payload["numOfRows"], rows) || rows < total) return Outcome::ProviderError;
  if (!total) return Outcome::NoData;  // A not-yet-published year is not known empty.
  JsonVariantConst items = payload["items"]["item"];
  size_t count = items.is<JsonArrayConst>() ? items.size() : items.is<JsonObjectConst>() ? 1 : 0;
  if (count != (size_t)total) return Outcome::ProviderError;
  cal::YearTable candidate;
  cal::Clear(candidate, year);
  // Independently verified exchange closures survive a public-calendar refresh.
  if (out.year == year) {
    memcpy(candidate.market_closed, out.market_closed, sizeof(candidate.market_closed));
    candidate.market_verified=out.market_verified;
  }
  bool any_holiday = false;
  for (size_t i = 0; i < count; ++i) {
    JsonVariantConst item = items.is<JsonArrayConst>() ? items[i] : items;
    int date = 0, month = 0, day = 0;
    const char* holiday = item["isHoliday"].as<const char*>();
    if (!Integer(item["locdate"], date) || !Date(date, year, month, day) ||
        !holiday || (strcmp(holiday, "Y") && strcmp(holiday, "N"))) return Outcome::ProviderError;
    if (!strcmp(holiday, "Y")) {
      cal::SetHoliday(candidate, month, day);
      any_holiday = true;
    }
  }
  if (!any_holiday) return Outcome::NoData;
  candidate.loaded = true;
  candidate.fetched_utc = ctx.fetched_utc;
  out = candidate;
  return Outcome::Ok;
}

Outcome FetchHolidayYear(IHttpClient& http, const char* key, const char* ca,
                         int year, const FetchContext& ctx, cal::YearTable& out) {
  if (!ctx.clock_verified || !ca || !*ca) return Outcome::ProviderError;
  char url[1100];
  if (!HolidayYearUrl(key, year, url, sizeof(url))) return Outcome::AuthError;
  HttpResponse response = http.get(url, nullptr, 0, ca);
  if (response.error != HttpError::None) return Outcome::ProviderError;
  if (response.status == 429) return Outcome::RateLimited;
  if (response.status == 401 || response.status == 403) return Outcome::AuthError;
  if (response.status != 200) return Outcome::ProviderError;
  return ParseHolidayYear(response.body, response.body_len, year, ctx, out);
}
}
}
