#include "SettingsCodec.h"

#include <memory>
#include <new>
#include <initializer_list>
#include <string.h>

#include "CacheStore.h"
#include "Codec.h"
#include "SettingsStore.h"

namespace kdash {
namespace settings_codec {
namespace {

bool DecodeBody(codec::Reader& r, Settings& s, uint16_t ver) {
  Settings& tmp = s;
  int32_t gx = 0, gy = 0, page = 0;
  uint8_t fsc = 0, kis = 0, usb = 1;
  uint16_t watch_count = 0;
  if (!r.i32(gx) || !r.i32(gy)) return false;
  if (!r.str(tmp.location.display_name, sizeof(tmp.location.display_name),
             kMaxDisplayNameLen))
    return false;
  if (!r.str(tmp.location.air_station, sizeof(tmp.location.air_station),
             kMaxStationLen))
    return false;
  if (!r.str(tmp.wifi_ssid, sizeof(tmp.wifi_ssid), kMaxSsidLen)) return false;
  if (!r.str(tmp.wifi_secret, sizeof(tmp.wifi_secret), kMaxSecretLen))
    return false;
  if (!r.str(tmp.providers.kma_key, sizeof(tmp.providers.kma_key), kMaxKeyLen))
    return false;
  if (!r.str(tmp.providers.air_key, sizeof(tmp.providers.air_key), kMaxKeyLen))
    return false;
  if (!r.str(tmp.providers.exim_key, sizeof(tmp.providers.exim_key),
             kMaxKeyLen))
    return false;
  if (!r.str(tmp.providers.kis_app_key, sizeof(tmp.providers.kis_app_key),
             kMaxKeyLen))
    return false;
  if (!r.str(tmp.providers.kis_app_secret, sizeof(tmp.providers.kis_app_secret),
             kMaxKeyLen))
    return false;
  if (!r.u8(fsc) || !r.u8(kis)) return false;
  if (!r.u16(watch_count)) return false;
  if (watch_count > kMaxWatchlist) return false;
  for (uint16_t i = 0; i < watch_count; i++) {
    char sym[kMaxSymbolLen + 1] = {0};
    uint8_t m = 0;
    if (!r.str(sym, sizeof(sym), kMaxSymbolLen)) return false;
    if (!r.u8(m)) return false;
    if (m == (uint8_t)Market::Unknown || m > (uint8_t)Market::Unified ||
        settings::ValidateWatchItem(sym, MarketName((Market)m)) !=
            settings::Validation::Ok) return false;
    settings::CopyBounded(tmp.watchlist[i].symbol, sizeof(tmp.watchlist[i].symbol), sym);
    tmp.watchlist[i].market = (Market)m;
  }
  if (!r.i32(page)) return false;
  if (!r.u8(usb)) return false;
  if (page < 0 || page > 3) return false;  // Life/Market/Calendar/Device
  if (fsc > 1 || kis > 1 || usb > 1) return false;
  // Semantic validation (same rules as the portal).
  using settings::Validation;
  if (settings::ValidateLocation(gx, gy, tmp.location.display_name,
                                 tmp.location.air_station) != Validation::Ok)
    return false;
  if (tmp.wifi_ssid[0] &&
      settings::ValidateWifi(tmp.wifi_ssid, tmp.wifi_secret) != Validation::Ok)
    return false;
  for (const char* key : {tmp.providers.kma_key, tmp.providers.air_key,
                          tmp.providers.exim_key, tmp.providers.kis_app_key,
                          tmp.providers.kis_app_secret}) {
    if (settings::ValidateKey(key) != Validation::Ok) return false;
  }
  if (ver >= 3) {
    uint8_t lan = 0, flags = 0, rot = 0, from = 0, to = 0;
    uint16_t scale = 0;
    if (!r.u8(lan) || !r.u8(flags) || !r.u8(rot) || !r.u8(from) || !r.u8(to) ||
        !r.u16(scale))
      return false;
    if (flags & ~0x03) return false;  // unknown bits: reject, never guess
    DeviceConfig d;
    d.lan_mode = (LanPortalMode)lan;
    d.auto_rotate = (flags & 0x01) != 0;
    d.market_hours_only = (flags & 0x02) != 0;
    d.rotate_minutes = rot;
    d.market_from_hour = from;
    d.market_to_hour = to;
    d.refresh_scale_pct = scale;
    if (settings::ValidateDevice(d) != Validation::Ok) return false;
    tmp.device = d;
  }  // v2: device keeps its in-class defaults (refresh scale = build default, ADR 0017)
  if (ver >= 4) {
    uint8_t protect = 0;
    if (!r.u8(protect) || protect > 1) return false;
    tmp.device.charge_protect = protect != 0;
  }
  if (ver >= 5) {
    uint8_t fast = 0;
    if (!r.u8(fast) || fast > 1) return false;
    tmp.device.fast_refresh = fast != 0;
  }
  tmp.location.grid_x = gx;
  tmp.location.grid_y = gy;
  tmp.providers.fsc_enabled = fsc != 0;
  tmp.providers.kis_enabled = kis != 0;
  tmp.watch_count = watch_count;
  tmp.display_page = page;
  tmp.schedule.usb_power = usb != 0;
  return true;
}

}  // namespace

size_t Encode(const Settings& s, uint8_t* out, size_t out_cap) {
  codec::Writer w(out, out_cap);
  w.u16((uint16_t)kSettingsSchemaVersion);
  w.i32(s.location.grid_x);
  w.i32(s.location.grid_y);
  w.str(s.location.display_name, kMaxDisplayNameLen);
  w.str(s.location.air_station, kMaxStationLen);
  w.str(s.wifi_ssid, kMaxSsidLen);
  w.str(s.wifi_secret, kMaxSecretLen);
  w.str(s.providers.kma_key, kMaxKeyLen);
  w.str(s.providers.air_key, kMaxKeyLen);
  w.str(s.providers.exim_key, kMaxKeyLen);
  w.str(s.providers.kis_app_key, kMaxKeyLen);
  w.str(s.providers.kis_app_secret, kMaxKeyLen);
  w.u8(s.providers.fsc_enabled ? 1 : 0);
  w.u8(s.providers.kis_enabled ? 1 : 0);
  size_t wc = s.watch_count <= kMaxWatchlist ? s.watch_count : kMaxWatchlist;
  w.u16((uint16_t)wc);
  for (size_t i = 0; i < wc; i++) {
    w.str(s.watchlist[i].symbol, kMaxSymbolLen);
    w.u8((uint8_t)s.watchlist[i].market);
  }
  w.i32(s.display_page);
  w.u8(s.schedule.usb_power ? 1 : 0);
  w.u8((uint8_t)s.device.lan_mode);
  w.u8((uint8_t)((s.device.auto_rotate ? 0x01 : 0) | (s.device.market_hours_only ? 0x02 : 0)));
  w.u8((uint8_t)s.device.rotate_minutes);
  w.u8((uint8_t)s.device.market_from_hour);
  w.u8((uint8_t)s.device.market_to_hour);
  w.u16((uint16_t)s.device.refresh_scale_pct);
  w.u8(s.device.charge_protect ? 1 : 0);
  w.u8(s.device.fast_refresh ? 1 : 0);
  if (!w.ok()) return 0;
  return w.pos();
}

bool Decode(const uint8_t* data, size_t len, Settings& out) {
  if (!data) return false;
  codec::Reader r(data, len);
  uint16_t ver = 0;
  if (!r.u16(ver)) return false;
  // v5 is current; v2..v4 preserve credentials and migrate with new device flags OFF.
  if (ver != (uint16_t)kSettingsSchemaVersion && ver != (uint16_t)kSettingsSchemaVersionV4 &&
      ver != (uint16_t)kSettingsSchemaVersionV3 && ver != (uint16_t)kSettingsSchemaVersionV2)
    return false;
  // Keep failure atomic without stacking two Settings copies on loopTask.
  std::unique_ptr<Settings> tmp(new (std::nothrow) Settings);
  if (!tmp || !DecodeBody(r, *tmp, ver)) return false;
  if (!r.done()) return false;
  out = *tmp;
  return true;
}

uint32_t DataFingerprint(const Settings& s) {
  // Heap staging: never stack a ~1.6KB Settings plus a 2KB buffer on loopTask.
  std::unique_ptr<Settings> norm(new (std::nothrow) Settings(s));
  std::unique_ptr<uint8_t[]> bytes(new (std::nothrow) uint8_t[2048]);
  if (!norm || !bytes) return 0;
  norm->device = DeviceConfig();
  norm->display_page = 0;
  norm->schedule.refresh_scale_pct = 100;  // runtime mirror of device.refresh_scale_pct
  const size_t n = Encode(*norm, bytes.get(), 2048);
  // Keep the existing v3 DATA identity across device-only v4/v5 migration.
  // The final two bytes (charge_protect, fast_refresh) belong to DeviceConfig.
  if (n) bytes[0] = kSettingsSchemaVersionV3;
  const uint32_t crc = n >= 2 ? Crc32(bytes.get(), n - 2) : 0;
  memset(bytes.get(), 0, 2048);
  // Settings copies hold secrets: scrub before release.
  memset(norm->wifi_secret, 0, sizeof(norm->wifi_secret));
  memset(norm->providers.kis_app_secret, 0, sizeof(norm->providers.kis_app_secret));
  if (!n) return 0;
  return crc ? crc : 1;  // 0 is reserved for "failed"
}

bool Migrate(int from_version, const uint8_t* data, size_t len, Settings& out) {
  // No published v1 wire contract exists in Git. Never invent one or replace
  // caller state on failure. v2/v3 add only device defaults, never invent keys.
  if ((from_version != kSettingsSchemaVersion && from_version != kSettingsSchemaVersionV4 &&
       from_version != kSettingsSchemaVersionV3 && from_version != kSettingsSchemaVersionV2) ||
      !data || len < 2 ||
      data[0] != (uint8_t)from_version || data[1] != (uint8_t)(from_version >> 8))
    return false;
  return Decode(data, len, out);
}

}  // namespace settings_codec
}  // namespace kdash
