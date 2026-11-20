#include "SetupPortal.h"

#include <stdio.h>
#include <string.h>

#include "../../include/Config.h"
#include "../storage/SettingsStore.h"

namespace kdash {
namespace provision {
namespace {

bool IsDigits(const char* s) {
  if (!s || !*s) return false;
  for (; *s; s++) {
    if (*s < '0' || *s > '9') return false;
  }
  return true;
}

}  // namespace

bool ValidateField(Field f, const char* value, char* err_out,
                   size_t err_cap) {
  auto fail = [&](const char* msg) {
    if (err_out && err_cap) {
      strncpy(err_out, msg, err_cap - 1);
      err_out[err_cap - 1] = '\0';
    }
    return false;
  };
  using settings::Validation;
  if (!value) value = "";
  if (strlen(value) > kFormFieldMax) return fail("입력이 너무 깁니다");
  switch (f) {
    case Field::Ssid:
      return settings::ValidateWifi(value, nullptr) == Validation::Ok
                 ? true
                 : fail("SSID가 비어 있거나 32바이트를 초과합니다");
    case Field::WifiSecret:
      return settings::ValidateWifi("x", value) == Validation::Ok
                 ? true
                 : fail("Wi-Fi 비밀번호는 8~63자(또는 64자리 16진수)입니다");
    case Field::KmaKey:
    case Field::AirKey:
    case Field::EximKey:
    case Field::KisAppKey:
    case Field::KisAppSecret:
      // Empty == feature disabled ('설정 필요' path).
      return settings::ValidateKey(value) == Validation::Ok
                 ? true
                 : fail("키에 공백/따옴표가 있거나 256자를 초과합니다");
    case Field::GridX:
    case Field::GridY: {
      if (!IsDigits(value) || strlen(value) > 3) return fail("격자는 숫자입니다");
      int v = 0;
      for (const char* p = value; *p; p++) v = v * 10 + (*p - '0');
      return (v >= 1 && v <= 200) ? true : fail("격자 범위 1–200 오류");
    }
    case Field::DisplayName:
      return settings::ValidateLocation(1, 1, value, "x") == Validation::Ok
                 ? true
                 : fail("지역명을 입력하세요(64바이트, 한글 21자 이내)");
    case Field::AirStation:
      return settings::ValidateLocation(1, 1, "x", value) == Validation::Ok
                 ? true
                 : fail("측정소명을 입력하세요(48바이트, 한글 16자 이내)");
    case Field::Watchlist: {
      WatchItem items[kMaxWatchlist];
      size_t count = 0;
      Validation v =
          settings::ParseWatchlist(value, items, kMaxWatchlist, count);
      if (v == Validation::Ok) return true;
      if (v == Validation::BadMarket)
        return fail("시장은 KRX/NXT/통합 중 하나입니다 (예: 005930:NXT)");
      if (v == Validation::BadSymbol) return fail("종목코드는 6자리 숫자입니다");
      return fail("관심종목은 쉼표로 구분한 최대 4개입니다");
    }
  }
  return fail("알 수 없는 필드");
}

size_t EscapeHtml(const char* in, char* out, size_t out_cap) {
  if (!out || out_cap == 0) return 0;
  size_t n = 0;
  if (!in) in = "";
  for (; *in; in++) {
    const char* rep = nullptr;
    switch (*in) {
      case '&': rep = "&amp;"; break;
      case '<': rep = "&lt;"; break;
      case '>': rep = "&gt;"; break;
      case '"': rep = "&quot;"; break;
      case '\'': rep = "&#39;"; break;
      default: break;
    }
    const size_t len = rep ? strlen(rep) : 1;
    if (n + len + 1 > out_cap) break;  // never emit a partial entity
    if (rep) {
      memcpy(out + n, rep, len);
    } else {
      out[n] = *in;
    }
    n += len;
  }
  out[n] = '\0';
  return n;
}

bool MakeApPassword(IRandom& rng, char* out, size_t out_cap) {
  static const char kAlphabet[] = "ABCDEFGHJKMNPQRSTUVWXYZ23456789";  // no 0/O/1/I/L
  if (!out || out_cap < kApPasswordLen + 1) return false;
  for (size_t i = 0; i < kApPasswordLen; i++) {
    out[i] = kAlphabet[UniformBelow(rng, sizeof(kAlphabet) - 1)];
  }
  out[kApPasswordLen] = '\0';
  return true;
}

bool IsTwoStepResetConfirmed(const char* step1, const char* step2) {
  return step1 && step2 && strcmp(step1, "RESET") == 0 &&
         strcmp(step2, "RESET-YES") == 0;
}

bool IssueCsrfToken(IRandom& rng, char* out, size_t out_cap) {
  static const char kHex[] = "0123456789ABCDEF";
  if (!out || out_cap < kCsrfTokenLen * 2 + 1) return false;
  for (size_t i = 0; i < kCsrfTokenLen * 2; i += 8) {
    uint32_t r = rng.next_u32();
    for (int k = 0; k < 8; k++) {
      out[i + k] = kHex[r & 0xF];
      r >>= 4;
    }
  }
  out[kCsrfTokenLen * 2] = '\0';
  return true;
}

bool CheckCsrfToken(const char* presented, const char* expected) {
  if (!presented || !expected) return false;
  size_t lp = strlen(presented), le = strlen(expected);
  if (lp != le || le == 0) return false;
  unsigned diff = 0;
  for (size_t i = 0; i < le; i++) diff |= (unsigned)(presented[i] ^ expected[i]);
  return diff == 0;
}

bool MakeApName(const char* suffix4, char* out, size_t out_cap) {
  if (!suffix4 || !out || strlen(suffix4) != 4) return false;
  for (int i = 0; i < 4; i++) {
    char c = suffix4[i];
    bool hex = (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F');
    if (!hex) return false;
  }
  int n = snprintf(out, out_cap, "%s%s", kApPrefix, suffix4);
  return n > 0 && (size_t)n < out_cap;
}

}  // namespace provision
}  // namespace kdash
