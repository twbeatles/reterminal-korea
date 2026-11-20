#include "Format.h"

#include <stdio.h>
#include <string.h>

#include "../domain/Freshness.h"
#include "../power/BatteryPolicy.h"

namespace kdash {
namespace fmt {
namespace {

void CommaInt64(int64_t v, char* out, size_t cap) {
  char tmp[32];
  snprintf(tmp, sizeof(tmp), "%lld", (long long)v);
  const char* neg = "";
  const char* p = tmp;
  if (*p == '-') {
    neg = "-";
    p++;
  }
  size_t n = strlen(p);
  char buf[40];
  size_t w = 0;
  for (size_t i = 0; i < n; i++) {
    buf[w++] = p[i];
    size_t left = n - i - 1;
    if (left && left % 3 == 0) buf[w++] = ',';
  }
  buf[w] = '\0';
  snprintf(out, cap, "%s%s", neg, buf);
}

}  // namespace

size_t TempC(double v, bool missing, char* out, size_t cap) {
  if (!out || !cap) return 0;
  if (missing) {
    snprintf(out, cap, "--");
    return 2;
  }
  snprintf(out, cap, "%.1f°C", v);
  return strlen(out);
}

size_t Percent(int v, bool missing, char* out, size_t cap) {
  if (!out || !cap) return 0;
  if (missing) {
    snprintf(out, cap, "--");
    return 2;
  }
  snprintf(out, cap, "%d%%", v);
  return strlen(out);
}

size_t RainMm(double v, bool missing, char* out, size_t cap) {
  if (!out || !cap) return 0;
  if (missing) {
    snprintf(out, cap, "--");
    return 2;
  }
  if (v < 0.05)
    snprintf(out, cap, "0mm");
  else
    snprintf(out, cap, "%.1fmm", v);
  return strlen(out);
}

size_t Pm(int v, bool missing, const char* grade, char* out, size_t cap) {
  if (!out || !cap) return 0;
  if (missing) {
    snprintf(out, cap, "--");
    return 2;
  }
  snprintf(out, cap, "%d ㎍/㎥ %s", v, grade ? grade : "");
  return strlen(out);
}

const char* Pm10Grade(int v) {
  if (v <= 30) return "좋음";
  if (v <= 80) return "보통";
  if (v <= 150) return "나쁨";
  return "매우나쁨";
}

const char* Pm25Grade(int v) {
  if (v <= 15) return "좋음";
  if (v <= 35) return "보통";
  if (v <= 75) return "나쁨";
  return "매우나쁨";
}

const char* SkyText(int code) {
  switch (code) {
    case 1: return "맑음";
    case 3: return "구름많음";
    case 4: return "흐림";
    default: return "--";
  }
}

const char* PtyText(int code) {
  switch (code) {
    case 0: return "없음";
    case 1: return "비";
    case 2: return "비/눈";
    case 3: return "눈";
    case 4: return "소나기";
    default: return "--";
  }
}

size_t FxRate(int64_t won_scaled, int32_t scale, char* out, size_t cap) {
  if (!out || !cap) return 0;
  if (scale < 0 || scale > 6) {
    snprintf(out, cap, "--");
    return strlen(out);
  }
  int64_t ip = won_scaled;
  int64_t frac = 0;
  int64_t div = 1;
  for (int i = 0; i < scale; i++) div *= 10;
  if (div > 1) {
    frac = ip % div;
    if (frac < 0) frac = -frac;
    ip /= div;
  }
  char ibuf[40];
  CommaInt64(ip, ibuf, sizeof(ibuf));
  if (scale <= 0) {
    snprintf(out, cap, "%s", ibuf);
  } else if (scale == 2) {
    snprintf(out, cap, "%s.%02d", ibuf, (int)frac);
  } else {
    char fbuf[32];
    snprintf(fbuf, sizeof(fbuf), "%lld", (long long)frac);
    // zero-pad to scale
    char pad[16] = {0};
    size_t fl = strlen(fbuf);
    size_t zi = 0;
    while (fl + zi < (size_t)scale && zi < sizeof(pad) - 1) pad[zi++] = '0';
    snprintf(out, cap, "%s.%s%s", ibuf, pad, fbuf);
  }
  return strlen(out);
}

size_t Krw(int64_t price, char* out, size_t cap) {
  if (!out || !cap) return 0;
  CommaInt64(price, out, cap);
  return strlen(out);
}

size_t Index2(int64_t value_x100, char* out, size_t cap) {
  if (!out || !cap) return 0;
  if (value_x100 < 0) value_x100 = 0;
  char whole[24];
  CommaInt64(value_x100 / 100, whole, sizeof(whole));
  snprintf(out, cap, "%s.%02d", whole, (int)(value_x100 % 100));
  return strlen(out);
}

size_t ChangePct(int32_t bp, char* out, size_t cap) {
  if (!out || !cap) return 0;
  const int a = bp < 0 ? -bp : bp;
  snprintf(out, cap, "%s%d.%02d%%", bp > 0 ? "+" : bp < 0 ? "-" : "", a / 100, a % 100);
  return strlen(out);
}

size_t KstTime(int64_t epoch_utc, int64_t now_utc, char* out, size_t cap) {
  if (!out || !cap) return 0;
  KstBroken a = BreakKst(epoch_utc), n = BreakKst(now_utc);
  if (a.year == n.year && a.month == n.month && a.day == n.day)
    snprintf(out, cap, "%02d:%02d", a.hour, a.minute);
  else
    snprintf(out, cap, "%02d-%02d %02d:%02d", a.month, a.day, a.hour, a.minute);
  return strlen(out);
}

size_t FixingDay(int64_t fixing_utc, char* out, size_t cap) {
  if (!out || !cap) return 0;
  KstBroken k = BreakKst(fixing_utc);
  static const char* kWd[] = {"일", "월", "화", "수", "목", "금", "토"};
  const char* wd = (k.weekday >= 0 && k.weekday < 7) ? kWd[k.weekday] : "";
  snprintf(out, cap, "고시 기준일 %02d-%02d(%s)", k.month, k.day, wd);
  return strlen(out);
}

ui::IconId WeatherIconFor(int sky, int pty) {
  switch (pty) {
    case 1: case 4: case 5: return ui::IconId::Rain;
    case 2: case 6: return ui::IconId::RainSnow;
    case 3: case 7: return ui::IconId::Snow;
    default: break;
  }
  switch (sky) {
    case 1: return ui::IconId::Sun;
    case 3: return ui::IconId::PartlyCloudy;
    case 4: return ui::IconId::Cloud;
    default: return ui::IconId::Unknown;
  }
}

size_t HeaderDateTime(int64_t now_utc, char* out, size_t cap) {
  if (!out || !cap) return 0;
  KstBroken k = BreakKst(now_utc);
  static const char* kWd[] = {"일", "월", "화", "수", "목", "금", "토"};
  const char* wd = (k.weekday >= 0 && k.weekday < 7) ? kWd[k.weekday] : "";
  snprintf(out, cap, "%02d-%02d(%s) %02d:%02d", k.month, k.day, wd, k.hour,
           k.minute);
  return strlen(out);
}

ui::IconId BatteryIconFor(int battery_mv, bool usb) {
  if (usb) return ui::IconId::BatteryCharging;
  if (battery_mv < 0) return ui::IconId::None;
  // Low/Critical follow battery::Classify; only the icon tier uses the curve,
  // no percentage is displayed (U-17).
  if (battery::IsLowPower(battery_mv)) return ui::IconId::BatteryLow;
  return battery::PercentMv(battery_mv) >= 60 ? ui::IconId::BatteryFull
                                               : ui::IconId::BatteryMid;
}

size_t Age(int64_t observed_utc, int64_t now_utc, char* out, size_t cap) {
  if (!out || !cap) return 0;
  int64_t d = now_utc - observed_utc;
  if (d < 0) d = 0;
  if (d < 3600)
    snprintf(out, cap, "%d분 전", (int)(d / 60));
  else if (d < 86400)
    snprintf(out, cap, "%d시간 전", (int)(d / 3600));
  else
    snprintf(out, cap, "%d일 전", (int)(d / 86400));
  return strlen(out);
}

}  // namespace fmt
}  // namespace kdash
