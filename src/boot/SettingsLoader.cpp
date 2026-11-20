#include "SettingsLoader.h"

#include <memory>
#include <new>

#include "../storage/SettingsCodec.h"
#include "../storage/SettingsStore.h"
#include "../storage/SlotStore.h"

#if defined(ARDUINO) && defined(KDASH_BOARD_E1001)
#include "SerialLog.h"
#endif

#if defined(KDASH_ALLOW_DEV_CREDENTIALS) && KDASH_ALLOW_DEV_CREDENTIALS && __has_include(<kdash_private_credentials.h>)
#include <kdash_private_credentials.h>
#define KDASH_HAS_DEV_CREDENTIALS 1
#else
#define KDASH_HAS_DEV_CREDENTIALS 0
#endif


namespace kdash {
namespace boot {
namespace {
const char* g_config_source = "default";
}  // namespace

const char* ConfigSource() { return g_config_source; }

bool LoadSettingsFromNvs(IStorage& storage, Settings& s) {
  uint8_t payload[2048];
  size_t payload_len = 0;
  if (!slot::Read(storage, "set", slot::kKindSettings, payload,
                  sizeof(payload), payload_len))
    return false;
  return settings_codec::Decode(payload, payload_len, s);
}

void LoadDevSettings(Settings& s) {
#if KDASH_HAS_DEV_CREDENTIALS
#if defined(ARDUINO) && defined(KDASH_BOARD_E1001)
  KDASH_LOG.println("KDASH_PRIVATE_BUILD|END");  // release packager rejects this marker
#endif
  using settings::CopyBounded;
  bool ok = CopyBounded(s.wifi_ssid, sizeof(s.wifi_ssid), KDASH_DEV_WIFI_SSID) &&
            CopyBounded(s.wifi_secret, sizeof(s.wifi_secret), KDASH_DEV_WIFI_SECRET) &&
            CopyBounded(s.providers.kma_key, sizeof(s.providers.kma_key), KDASH_DEV_KMA_KEY) &&
            CopyBounded(s.providers.air_key, sizeof(s.providers.air_key), KDASH_DEV_AIR_KEY) &&
            CopyBounded(s.providers.exim_key, sizeof(s.providers.exim_key), KDASH_DEV_EXIM_KEY) &&
            CopyBounded(s.providers.kis_app_key, sizeof(s.providers.kis_app_key), KDASH_DEV_KIS_APP_KEY) &&
            CopyBounded(s.providers.kis_app_secret, sizeof(s.providers.kis_app_secret), KDASH_DEV_KIS_APP_SECRET) &&
            CopyBounded(s.location.display_name, sizeof(s.location.display_name), KDASH_DEV_DISPLAY_NAME) &&
            CopyBounded(s.location.air_station, sizeof(s.location.air_station), KDASH_DEV_AIR_STATION);
  s.location.grid_x = KDASH_DEV_GRID_X;
  s.location.grid_y = KDASH_DEV_GRID_Y;
  s.providers.fsc_enabled = KDASH_DEV_FSC_ENABLED != 0;
  s.providers.kis_enabled = KDASH_DEV_KIS_ENABLED != 0;
  // 실기 검증 전용(개발 환경에서 -D로만 켠다): 포털 상시 열기, 순환 간격 단축.
#ifdef KDASH_DEV_LAN_ALWAYS
  s.device.lan_mode = LanPortalMode::Always;
#endif
#ifdef KDASH_DEV_ROTATE_MIN
  s.device.rotate_minutes = KDASH_DEV_ROTATE_MIN;
#endif
  using settings::Validation;
  if (!ok ||
      settings::ValidateWifi(s.wifi_ssid, s.wifi_secret) != Validation::Ok ||
      settings::ValidateLocation(s.location.grid_x, s.location.grid_y,
                                 s.location.display_name,
                                 s.location.air_station) != Validation::Ok ||
      settings::ValidateKey(s.providers.kma_key) != Validation::Ok ||
      settings::ValidateKey(s.providers.air_key) != Validation::Ok ||
      settings::ValidateKey(s.providers.exim_key) != Validation::Ok ||
      settings::ValidateKey(s.providers.kis_app_key) != Validation::Ok ||
      settings::ValidateKey(s.providers.kis_app_secret) != Validation::Ok ||
      settings::ParseWatchlist(KDASH_DEV_WATCHLIST, s.watchlist, kMaxWatchlist,
                               s.watch_count) != Validation::Ok) {
#if defined(ARDUINO) && defined(KDASH_BOARD_E1001)
    KDASH_LOG.println("config: private credentials invalid -> defaults (설정 필요)");
#endif
    s = settings::DefaultSettings();
  }
#else
  (void)s;
#if defined(ARDUINO) && defined(KDASH_BOARD_E1001)
  KDASH_LOG.println("config: no private credentials -> 설정 필요");
#endif
#endif
}

namespace {
// 호출 주기 배율은 이제 저장 설정(device.refresh_scale_pct)이 출처다. 런타임 값에 복사만 한다.
// 빌드 설정 KDASH_REFRESH_SCALE_PCT는 저장값이 없을 때의 초기값이다(Config.h).
void ApplyRefreshScale(Settings& s) {
  int pct = s.device.refresh_scale_pct;
  s.schedule.refresh_scale_pct = pct < kRefreshScaleMin ? kRefreshScaleMin
                                 : pct > kRefreshScaleMax ? kRefreshScaleMax : pct;
}
}  // namespace

static void LoadSettingsImpl(IStorage& storage, Settings& s);
void LoadSettings(IStorage& storage, Settings& s) {
  LoadSettingsImpl(storage, s);
  ApplyRefreshScale(s);  // runtime mirror of the stored device setting
}

static void LoadSettingsImpl(IStorage& storage, Settings& s) {
  // Boot order (T02 step 5): NVS -> external private header -> defaults.
  if (LoadSettingsFromNvs(storage, s)) {
    g_config_source = "nvs";
    return;
  }
  // Bounded heap staging (never stack a ~1.6KB Settings on the boot task).
  std::unique_ptr<Settings> dev(new (std::nothrow) Settings);
  if (dev) LoadDevSettings(*dev);
  if (dev && dev->wifi_ssid[0]) {
    s = *dev;
    g_config_source = "dev";
    return;
  }
  s = settings::DefaultSettings();
  g_config_source = "default";
}

void LogConfigRedacted(const Settings& s) {
#if defined(ARDUINO) && defined(KDASH_BOARD_E1001)
  // Labels only ("등록됨/미등록"), never values (SEC-01).
  KDASH_LOG.printf("config: wifi=%s kma=%s air=%s exim=%s kis=%s/%s fsc=%d kis_on=%d watch=%u grid=%d,%d refresh_scale=%d\n",
                settings::RegisteredLabel(s.wifi_ssid),
                settings::RegisteredLabel(s.providers.kma_key),
                settings::RegisteredLabel(s.providers.air_key),
                settings::RegisteredLabel(s.providers.exim_key),
                settings::RegisteredLabel(s.providers.kis_app_key),
                settings::RegisteredLabel(s.providers.kis_app_secret),
                (int)s.providers.fsc_enabled, (int)s.providers.kis_enabled,
                (unsigned)s.watch_count, s.location.grid_x, s.location.grid_y,
                s.schedule.refresh_scale_pct);
#else
  (void)s;
#endif
}

}  // namespace boot
}  // namespace kdash
