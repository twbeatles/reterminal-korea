#pragma once
// Shared firmware configuration contract (E1001 only).
// Mirrored by host/kdash/policy.py (validation) — keep both in sync.
// All kMax*Len values are UTF-8 BYTE limits excluding the NUL terminator;
// every char buffer below is kMax*Len + 1 (ISSUE-007).

#include <stddef.h>
#include <stdint.h>

namespace kdash {

inline constexpr int kSettingsSchemaVersion = 4;  // v4: +charge_protect
inline constexpr int kSettingsSchemaVersionV3 = 3;
inline constexpr int kSettingsSchemaVersionV2 = 2;  // 읽기 전용 이전 경로(Migrate)
inline constexpr int kCacheSchemaVersion = 1;

inline constexpr size_t kMaxSsidLen = 32;      // 802.11 SSID octets
inline constexpr size_t kMinWpaSecretLen = 8;  // WPA2 passphrase 8..63
inline constexpr size_t kMaxSecretLen = 64;    // 64 == raw hex PSK
inline constexpr size_t kMaxKeyLen = 256;      // KIS App Secret (~180) 수용
inline constexpr size_t kMaxStationLen = 48;   // 한글 16자
inline constexpr size_t kMaxDisplayNameLen = 64;  // 한글 21자
inline constexpr size_t kMaxWatchlist = 4;
inline constexpr size_t kMaxSymbolLen = 6;  // 6-digit code

inline constexpr int kBodyMaxBytes = 65536;  // NET-01 budget: oversize => abort
inline constexpr int kHttpTimeoutMs = 15000;
inline constexpr int kMaxRetriesPerProvider = 2;  // initial + 1 retry, auth errors: 1 total
inline constexpr int kEximFallbackMaxDays = 7;

enum class Market : uint8_t { Unknown = 0, Krx, Nxt, Unified };

struct LocationConfig {
  int grid_x = 60;  // fixture default (Seoul example grid), NOT a claim about Seoul
  int grid_y = 127;
  char display_name[kMaxDisplayNameLen + 1] = "서울 종로구";
  char air_station[kMaxStationLen + 1] = "종로구";
};

struct ProviderKeys {
  char kma_key[kMaxKeyLen + 1] = {0};  // data.go.kr (KMA/AirKorea/FSC 공용 가능)
  char air_key[kMaxKeyLen + 1] = {0};
  char exim_key[kMaxKeyLen + 1] = {0};
  char kis_app_key[kMaxKeyLen + 1] = {0};
  char kis_app_secret[kMaxKeyLen + 1] = {0};
  bool fsc_enabled = false;  // P5 optional, default OFF
  bool kis_enabled = false;  // P5 optional, default OFF
};

struct WatchItem {
  char symbol[kMaxSymbolLen + 1] = {0};  // 6-digit code, validated
  Market market = Market::Krx;
};

// 빌드 설정 -D KDASH_REFRESH_SCALE_PCT는 이제 "초기값"이다(v2에서 이전하거나 저장값이 없을 때).
// 포털에서 바꾼 값은 NVS에 저장된다(ADR 0024). 100~400만 유효.
#ifndef KDASH_REFRESH_SCALE_PCT
#define KDASH_REFRESH_SCALE_PCT 100
#endif
inline constexpr int kRefreshScaleMin = 100;
inline constexpr int kRefreshScaleMax = 400;
inline constexpr int kDefaultRefreshScalePct =
    KDASH_REFRESH_SCALE_PCT < kRefreshScaleMin ? kRefreshScaleMin
    : KDASH_REFRESH_SCALE_PCT > kRefreshScaleMax ? kRefreshScaleMax
                                                 : KDASH_REFRESH_SCALE_PCT;

inline constexpr int kRotateMinMinutes = 1;
inline constexpr int kRotateMaxMinutes = 60;
inline constexpr int kDefaultRotateMinutes = 5;

// LAN(같은 Wi-Fi) 설정 포털 모드. Off: SoftAP 포털만. Button: 녹색 2초로 10분 열기.
// Always: USB 전원일 때 상시 열기(로그인은 항상 페어링 코드 필요).
enum class LanPortalMode : uint8_t { Off = 0, Button = 1, Always = 2 };

struct DeviceConfig {
  bool charge_protect = false;  // estimated 80% target; only supported PMIC
  LanPortalMode lan_mode = LanPortalMode::Button;
  // 생활 <-> 시장 자동 순환(USB 전원일 때만 동작; 배터리는 갱신 때만 켜지므로 순환하지 않는다).
  bool auto_rotate = true;
  int rotate_minutes = kDefaultRotateMinutes;  // 1..60
  // 시장 탭은 장 시간(KST, from <= 시 < to)에만 순환에 넣는다. 밖에서는 생활 화면에 머문다.
  bool market_hours_only = true;
  int market_from_hour = 9;   // 0..23
  int market_to_hour = 16;    // 1..24, from보다 커야 함
  int refresh_scale_pct = kDefaultRefreshScalePct;  // 에어코리아·환율·KIS 주기 배율(%)
};

struct ScheduleConfig {
  bool usb_power = true;  // detected at runtime; this is the fallback default
  // 간격 구동 공급자(에어코리아·환율·KIS)의 주기 배율(%, 100~400). 저장되지 않는 런타임 값으로,
  // 빌드 설정 KDASH_REFRESH_SCALE_PCT에서 온다(ADR 0017). 100 = 기본 주기.
  int refresh_scale_pct = 100;
};

struct Settings {
  int schema_version = kSettingsSchemaVersion;
  LocationConfig location;
  char wifi_ssid[kMaxSsidLen + 1] = {0};
  char wifi_secret[kMaxSecretLen + 1] = {0};  // never printed / never served back
  ProviderKeys providers;
  WatchItem watchlist[kMaxWatchlist];
  size_t watch_count = 0;
  int display_page = 0;  // 0:life 1:market 2:calendar 3:device (부팅 시 시작 화면)
  ScheduleConfig schedule;
  DeviceConfig device;
};

const char* MarketName(Market m);  // "KRX"/"NXT"/"통합"/""

}  // namespace kdash
