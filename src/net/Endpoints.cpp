#include "Endpoints.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "../../include/Config.h"

namespace kdash {
namespace endpoints {
namespace {

bool IsUnreserved(char c) {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
         (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~';
}

bool IsHex(char c) {
  return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') ||
         (c >= 'a' && c <= 'f');
}

bool Fits(int n, size_t cap) { return n > 0 && (size_t)n < cap; }

// Keys travel in JSON bodies/headers: printable ASCII, no quote/backslash.
bool SafeHeaderToken(const char* s) {
  if (!s || !*s) return false;
  for (; *s; s++) {
    unsigned char c = (unsigned char)*s;
    if (c < 0x21 || c > 0x7E || c == '"' || c == '\\') return false;
  }
  return true;
}

}  // namespace

bool EncodeOnce(const char* in, char* out, size_t out_cap) {
  if (!in || !out || out_cap < 2) return false;
  static const char* kHex = "0123456789ABCDEF";
  char* p = out;
  size_t left = out_cap;
  for (const unsigned char* q = (const unsigned char*)in; *q; q++) {
    if (*q == '%') return false;  // 이중 인코딩 의심 — 거부
    if (IsUnreserved((char)*q)) {
      if (left < 2) return false;
      *p++ = (char)*q;
      left--;
    } else {
      if (left < 4) return false;
      *p++ = '%';
      *p++ = kHex[(*q >> 4) & 0xF];
      *p++ = kHex[*q & 0xF];
      left -= 3;
    }
  }
  *p = '\0';
  return true;
}

bool NormalizeServiceKey(const char* key, char* out, size_t out_cap) {
  if (!key || !*key || !out || out_cap < 2) return false;
  bool has_pct = false;
  for (const char* p = key; *p; p++) {
    unsigned char c = (unsigned char)*p;
    if (c <= 0x20 || c >= 0x7F) return false;  // pasted whitespace etc.
    if (c == '%') has_pct = true;
  }
  if (!has_pct) return EncodeOnce(key, out, out_cap);  // "Decoding" key
  // "Encoding" key: verify every '%' is a %XX escape, then copy verbatim.
  size_t n = 0;
  for (const char* p = key; *p; p++) {
    if (*p == '%' && !(IsHex(p[1]) && IsHex(p[2]))) return false;
    if (!IsUnreserved(*p) && *p != '%') return false;  // raw +/= mixed in
    if (n + 1 >= out_cap) return false;
    out[n++] = *p;
  }
  out[n] = '\0';
  return true;
}

bool KmaUltraUrl(const char* key, int base_date, int base_time, int nx, int ny,
                 int num_rows, char* out, size_t out_cap) {
  char k[kMaxKeyLen * 3 + 1];
  if (!out || num_rows <= 0 || !NormalizeServiceKey(key, k, sizeof(k)))
    return false;
  int n = snprintf(out, out_cap,
                   "https://apis.data.go.kr/1360000/VilageFcstInfoService_2.0/"
                   "getUltraSrtNcst?serviceKey=%s&pageNo=1&numOfRows=%d"
                   "&dataType=JSON&base_date=%08d&base_time=%04d&nx=%d&ny=%d",
                   k, num_rows, base_date, base_time, nx, ny);
  return Fits(n, out_cap);
}

bool KmaVillageUrl(const char* key, int base_date, int base_time, int nx,
                   int ny, int num_rows, char* out, size_t out_cap) {
  char k[kMaxKeyLen * 3 + 1];
  if (!out || num_rows <= 0 || !NormalizeServiceKey(key, k, sizeof(k)))
    return false;
  int n = snprintf(out, out_cap,
                   "https://apis.data.go.kr/1360000/VilageFcstInfoService_2.0/"
                   "getVilageFcst?serviceKey=%s&pageNo=1&numOfRows=%d"
                   "&dataType=JSON&base_date=%08d&base_time=%04d&nx=%d&ny=%d",
                   k, num_rows, base_date, base_time, nx, ny);
  return Fits(n, out_cap);
}

bool AirKoreaCandidateUrl(const char* key, const char* station, char* out,
                          size_t out_cap) {
  char k[kMaxKeyLen * 3 + 1];
  char enc[kMaxStationLen * 3 + 1];
  if (!station || !station[0] || !out ||
      !NormalizeServiceKey(key, k, sizeof(k)) ||
      !EncodeOnce(station, enc, sizeof(enc)))
    return false;
  int n = snprintf(out, out_cap,
                   "https://apis.data.go.kr/B552584/ArpltnInforInqireSvc/"
                   "getMsrstnAcctoRltmMesureDnsty?stationName=%s&dataTerm=DAILY"
                   "&pageNo=1&numOfRows=24&returnType=json&serviceKey=%s&ver=1.0",
                   enc, k);
  return Fits(n, out_cap);
}

bool EximUrl(const char* authkey, int searchdate, char* out, size_t out_cap) {
  char k[kMaxKeyLen * 3 + 1];
  if (!authkey || !authkey[0] || !out || !EncodeOnce(authkey, k, sizeof(k)))
    return false;
  int n = snprintf(out, out_cap,
                   "https://oapi.koreaexim.go.kr/site/program/financial/"
                   "exchangeJSON?authkey=%s&searchdate=%08d&data=AP01",
                   k, searchdate);
  return Fits(n, out_cap);
}

bool FscPriceUrl(const char* key, const char* symbol, int begin_yyyymmdd,
                 char* out, size_t out_cap) {
  char k[kMaxKeyLen * 3 + 1];
  if (!symbol || strlen(symbol) != 6 || !out ||
      !NormalizeServiceKey(key, k, sizeof(k)))
    return false;
  for (int i = 0; i < 6; i++) {
    if (!isdigit((unsigned char)symbol[i])) return false;
  }
  int n = snprintf(out, out_cap,
                   "https://apis.data.go.kr/1160100/"
                   "GetStockSecuritiesInfoService_V2/getStockPriceInfo_V2"
                   "?serviceKey=%s&numOfRows=10&pageNo=1&resultType=json"
                   "&likeSrtnCd=%s&beginBasDt=%08d",
                   k, symbol, begin_yyyymmdd);
  return Fits(n, out_cap);
}

const char* KisMarketCode(const char* m) {
  if (!m) return nullptr;
  if (strcmp(m, "KRX") == 0) return "J";
  if (strcmp(m, "NXT") == 0) return "NX";
  if (strcmp(m, "통합") == 0) return "UN";
  return nullptr;
}

bool KisPriceUrl(const char* symbol, const char* market, char* out,
                 size_t out_cap) {
  const char* code = KisMarketCode(market);
  if (!code || !symbol || strlen(symbol) != 6 || !out) return false;
  for (int i = 0; i < 6; i++) {
    if (!isdigit((unsigned char)symbol[i])) return false;
  }
  int n = snprintf(out, out_cap,
                   "%s/uapi/domestic-stock/v1/quotations/inquire-price"
                   "?FID_COND_MRKT_DIV_CODE=%s&FID_INPUT_ISCD=%s",
                   kKisBase, code, symbol);
  return Fits(n, out_cap);
}

bool KisIndexUrl(const char* code, char* out, size_t out_cap) {
  if (!code || strlen(code) != 4 || !out) return false;
  for (int i = 0; i < 4; i++) {
    if (!isdigit((unsigned char)code[i])) return false;
  }
  int n = snprintf(out, out_cap,
                   "%s/uapi/domestic-stock/v1/quotations/inquire-index-price"
                   "?FID_COND_MRKT_DIV_CODE=U&FID_INPUT_ISCD=%s",
                   kKisBase, code);
  return Fits(n, out_cap);
}

bool KisTokenUrl(char* out, size_t out_cap) {
  if (!out) return false;
  return Fits(snprintf(out, out_cap, "%s/oauth2/tokenP", kKisBase), out_cap);
}

bool KisTokenBody(const char* app_key, const char* app_secret, char* out,
                  size_t out_cap) {
  if (!out || !SafeHeaderToken(app_key) || !SafeHeaderToken(app_secret))
    return false;
  int n = snprintf(out, out_cap,
                   "{\"grant_type\":\"client_credentials\",\"appkey\":\"%s\","
                   "\"appsecret\":\"%s\"}",
                   app_key, app_secret);
  return Fits(n, out_cap);
}

}  // namespace endpoints
}  // namespace kdash
