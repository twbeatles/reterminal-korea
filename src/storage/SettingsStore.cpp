#include "SettingsStore.h"

#include <ctype.h>
#include <string.h>

namespace kdash {

const char* MarketName(Market m) {
  switch (m) {
    case Market::Krx: return "KRX";
    case Market::Nxt: return "NXT";
    case Market::Unified: return "통합";
    default: return "";
  }
}

namespace settings {
namespace {

bool IsHex(char c) {
  return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') ||
         (c >= 'a' && c <= 'f');
}

bool BoundedText(const char* s, size_t max_len) {
  if (!s || s[0] == '\0') return false;
  size_t n = strlen(s);
  if (n > max_len || !IsValidUtf8(s)) return false;
  for (size_t i = 0; i < n; i++) {
    if ((unsigned char)s[i] < 0x20 || s[i] == 0x7F) return false;  // control
  }
  return true;
}

Market MarketFrom(const char* m) {
  if (!m || strcmp(m, "KRX") == 0) return Market::Krx;
  if (strcmp(m, "NXT") == 0) return Market::Nxt;
  if (strcmp(m, "통합") == 0) return Market::Unified;
  return Market::Unknown;
}

}  // namespace

const char* Describe(Validation v) {
  switch (v) {
    case Validation::Ok:
      return "OK";
    case Validation::BadGrid:
      return "격자(nx,ny) 범위 오류";
    case Validation::BadStation:
      return "지역명/측정소명 오류";
    case Validation::BadSsid:
      return "SSID 오류";
    case Validation::BadKey:
      return "키 형식 오류";
    case Validation::BadSymbol:
      return "종목코드 오류(6자리 숫자)";
    case Validation::BadMarket:
      return "시장구분 오류";
    case Validation::BadSecret:
      return "Wi-Fi 비밀번호 오류(8~63자)";
    case Validation::BadWatchlist:
      return "관심종목 형식 오류";
    case Validation::BadDevice:
      return "기기 설정 범위 오류";
  }
  return "UNKNOWN";
}

bool IsValidUtf8(const char* s) {
  if (!s) return false;
  const unsigned char* p = (const unsigned char*)s;
  while (*p) {
    unsigned char c = *p;
    int extra;
    uint32_t cp;
    if (c < 0x80) {
      p++;
      continue;
    } else if ((c & 0xE0) == 0xC0) {
      extra = 1;
      cp = c & 0x1F;
    } else if ((c & 0xF0) == 0xE0) {
      extra = 2;
      cp = c & 0x0F;
    } else if ((c & 0xF8) == 0xF0) {
      extra = 3;
      cp = c & 0x07;
    } else {
      return false;
    }
    for (int i = 1; i <= extra; i++) {
      if ((p[i] & 0xC0) != 0x80) return false;  // also catches NUL (truncated)
      cp = (cp << 6) | (p[i] & 0x3F);
    }
    if ((extra == 1 && cp < 0x80) || (extra == 2 && cp < 0x800) ||
        (extra == 3 && (cp < 0x10000 || cp > 0x10FFFF)) ||
        (cp >= 0xD800 && cp <= 0xDFFF))
      return false;  // overlong / out of range / surrogate
    p += extra + 1;
  }
  return true;
}

bool CopyBounded(char* dst, size_t dst_cap, const char* src) {
  if (!dst || dst_cap == 0 || !src) return false;
  size_t n = strlen(src);
  if (n + 1 > dst_cap) return false;
  memcpy(dst, src, n + 1);
  return true;
}

Validation ValidateLocation(int grid_x, int grid_y, const char* display_name,
                            const char* air_station) {
  // KMA 격자 범위 가드 (행정동 격자 파일 기반 선택은 포털에서 수행).
  if (grid_x < 1 || grid_x > 200 || grid_y < 1 || grid_y > 200)
    return Validation::BadGrid;
  if (!BoundedText(display_name, kMaxDisplayNameLen))
    return Validation::BadStation;
  if (!BoundedText(air_station, kMaxStationLen)) return Validation::BadStation;
  return Validation::Ok;
}

Validation ValidateWifi(const char* ssid, const char* secret) {
  if (!ssid || ssid[0] == '\0' || strlen(ssid) > kMaxSsidLen)
    return Validation::BadSsid;
  if (!secret || secret[0] == '\0') return Validation::Ok;  // open network
  size_t n = strlen(secret);
  if (n == kMaxSecretLen) {  // raw 256-bit PSK in hex
    for (size_t i = 0; i < n; i++) {
      if (!IsHex(secret[i])) return Validation::BadSecret;
    }
    return Validation::Ok;
  }
  if (n < kMinWpaSecretLen || n > kMaxSecretLen - 1) return Validation::BadSecret;
  for (size_t i = 0; i < n; i++) {
    unsigned char c = (unsigned char)secret[i];
    if (c < 0x20 || c > 0x7E) return Validation::BadSecret;  // WPA: ASCII
  }
  return Validation::Ok;
}

Validation ValidateKey(const char* key) {
  if (!key || key[0] == '\0') return Validation::Ok;  // feature disabled
  size_t n = strlen(key);
  if (n > kMaxKeyLen) return Validation::BadKey;
  for (size_t i = 0; i < n; i++) {
    unsigned char c = (unsigned char)key[i];
    if (c < 0x21 || c > 0x7E || c == '"' || c == '\\') return Validation::BadKey;
  }
  return Validation::Ok;
}

Validation ValidateDevice(const DeviceConfig& d) {
  if ((int)d.lan_mode > (int)LanPortalMode::Always)
    return Validation::BadDevice;
  if (d.rotate_minutes < kRotateMinMinutes || d.rotate_minutes > kRotateMaxMinutes)
    return Validation::BadDevice;
  if (d.market_from_hour < 0 || d.market_from_hour > 23 || d.market_to_hour < 1 ||
      d.market_to_hour > 24 || d.market_from_hour >= d.market_to_hour)
    return Validation::BadDevice;
  if (d.refresh_scale_pct < kRefreshScaleMin || d.refresh_scale_pct > kRefreshScaleMax)
    return Validation::BadDevice;
  return Validation::Ok;
}

Validation ValidateWatchItem(const char* symbol, const char* market) {
  if (!symbol || strlen(symbol) != kMaxSymbolLen) return Validation::BadSymbol;
  for (size_t i = 0; i < kMaxSymbolLen; i++) {
    if (symbol[i] < '0' || symbol[i] > '9') return Validation::BadSymbol;
  }
  if (!market) return Validation::BadMarket;
  return MarketFrom(market) == Market::Unknown ? Validation::BadMarket
                                               : Validation::Ok;
}

Validation ParseWatchlist(const char* text, WatchItem* out, size_t cap,
                          size_t& count) {
  if (!text) return Validation::BadWatchlist;
  const char* p = text;
  while (isspace((unsigned char)*p)) p++;
  if (*p == '\0') {
    count = 0;
    return Validation::Ok;  // 비어 있으면 관심종목 없음
  }
  WatchItem tmp[kMaxWatchlist];
  size_t n = 0;
  const char* tok = text;
  for (;;) {
    const char* end = tok;
    while (*end && *end != ',') end++;
    const char* a = tok;
    const char* b = end;
    while (a < b && isspace((unsigned char)*a)) a++;
    while (b > a && isspace((unsigned char)*(b - 1))) b--;
    if (a == b) return Validation::BadWatchlist;  // empty token
    if (n >= kMaxWatchlist || n >= cap) return Validation::BadWatchlist;
    char item[32];
    if ((size_t)(b - a) >= sizeof(item)) return Validation::BadSymbol;
    memcpy(item, a, (size_t)(b - a));
    item[b - a] = '\0';
    char* colon = strchr(item, ':');
    const char* market = "KRX";
    if (colon) {
      *colon = '\0';
      market = colon + 1;
    }
    Validation v = ValidateWatchItem(item, market);
    if (v != Validation::Ok) return v;
    memcpy(tmp[n].symbol, item, kMaxSymbolLen + 1);
    tmp[n].market = MarketFrom(market);
    n++;
    if (*end == '\0') break;
    tok = end + 1;
  }
  for (size_t i = 0; i < n; i++) out[i] = tmp[i];
  count = n;
  return Validation::Ok;
}

bool NeedsMigration(int stored_schema) {
  return stored_schema != kSettingsSchemaVersion;
}

Settings DefaultSettings() {
  Settings s;  // in-class initializers carry fixture-safe Seoul defaults
  return s;
}

void MaskSecret(const char* secret, char* out, size_t out_cap) {
  const char* label = RegisteredLabel(secret);
  if (!out || out_cap == 0) return;
  strncpy(out, label, out_cap - 1);
  out[out_cap - 1] = '\0';
}

const char* RegisteredLabel(const char* secret) {
  return (secret && secret[0] != '\0') ? "등록됨" : "미등록";
}

}  // namespace settings
}  // namespace kdash
