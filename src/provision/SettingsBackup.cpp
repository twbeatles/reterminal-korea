#include "SettingsBackup.h"

#include <ArduinoJson.h>
#include <memory>
#include <new>
#include <stdio.h>
#include <string.h>

#include "../storage/SettingsStore.h"

namespace kdash {
namespace portal {
namespace {

void SetBad(char* out, size_t cap, const char* name) {
  if (out && cap) {
    strncpy(out, name, cap - 1);
    out[cap - 1] = '\0';
  }
}

// JSON 정수만 허용(문자열 "5", 실수 5.5, 불리언은 거부).
bool GetInt(JsonVariantConst v, int lo, int hi, int& out) {
  if (!v.is<int>()) return false;  // 정수만: 문자열 "5", 실수, 불리언은 거부
  const long x = v.as<long>();
  if (x < lo || x > hi) return false;
  out = (int)x;
  return true;
}

bool GetBool(JsonVariantConst v, bool& out) {
  if (!v.is<bool>()) return false;
  out = v.as<bool>();
  return true;
}

}  // namespace

size_t ExportBackup(const Settings& s, char* out, size_t cap) {
  if (!out || cap == 0) return 0;
  JsonDocument doc;
  doc["format"] = kBackupFormat;
  doc["version"] = kBackupVersion;
  JsonObject loc = doc["location"].to<JsonObject>();
  loc["display"] = s.location.display_name;
  loc["station"] = s.location.air_station;
  loc["nx"] = s.location.grid_x;
  loc["ny"] = s.location.grid_y;
  JsonArray watch = doc["watchlist"].to<JsonArray>();
  for (size_t i = 0; i < s.watch_count && i < kMaxWatchlist; i++) {
    char item[24];
    snprintf(item, sizeof(item), "%s:%s", s.watchlist[i].symbol, MarketName(s.watchlist[i].market));
    watch.add(item);
  }
  JsonObject prov = doc["providers"].to<JsonObject>();
  prov["fsc"] = s.providers.fsc_enabled;
  prov["kis"] = s.providers.kis_enabled;
  JsonObject dev = doc["device"].to<JsonObject>();
  dev["page"] = s.display_page;
  dev["lan_mode"] = (int)s.device.lan_mode;
  dev["auto_rotate"] = s.device.auto_rotate;
  dev["rotate_minutes"] = s.device.rotate_minutes;
  dev["market_hours_only"] = s.device.market_hours_only;
  dev["market_from_hour"] = s.device.market_from_hour;
  dev["market_to_hour"] = s.device.market_to_hour;
  dev["refresh_scale_pct"] = s.device.refresh_scale_pct;
  dev["charge_protect"] = s.device.charge_protect;
  dev["fast_refresh"] = s.device.fast_refresh;
  if (measureJson(doc) + 1 > cap) return 0;
  return serializeJson(doc, out, cap);
}

ImportResult ImportBackup(const char* json, Settings& inout, char* bad_field, size_t bad_cap) {
  if (bad_field && bad_cap) bad_field[0] = '\0';
  if (!json || !*json || strlen(json) > kBackupMax) return ImportResult::BadJson;
  JsonDocument doc;
  if (deserializeJson(doc, json, DeserializationOption::NestingLimit(6))) return ImportResult::BadJson;
  JsonVariantConst root = doc.as<JsonVariantConst>();
  if (!root.is<JsonObjectConst>()) return ImportResult::BadJson;
  const char* fmt = root["format"] | "";
  int ver = 0;
  if (strcmp(fmt, kBackupFormat) != 0 || !GetInt(root["version"], 1, 1000, ver) ||
      ver != kBackupVersion) {
    SetBad(bad_field, bad_cap, "format/version");
    return ImportResult::BadFormat;
  }
  std::unique_ptr<Settings> cand(new (std::nothrow) Settings(inout));
  if (!cand) return ImportResult::BadValue;
  Settings& c = *cand;
  using settings::Validation;

  JsonVariantConst loc = root["location"];
  if (!loc.isNull()) {
    if (!loc.is<JsonObjectConst>()) { SetBad(bad_field, bad_cap, "location"); return ImportResult::BadValue; }
    if (!loc["display"].isNull()) {
      const char* v = loc["display"] | (const char*)nullptr;
      if (!v || !settings::CopyBounded(c.location.display_name, sizeof(c.location.display_name), v)) {
        SetBad(bad_field, bad_cap, "location.display"); return ImportResult::BadValue;
      }
    }
    if (!loc["station"].isNull()) {
      const char* v = loc["station"] | (const char*)nullptr;
      if (!v || !settings::CopyBounded(c.location.air_station, sizeof(c.location.air_station), v)) {
        SetBad(bad_field, bad_cap, "location.station"); return ImportResult::BadValue;
      }
    }
    if (!loc["nx"].isNull() && !GetInt(loc["nx"], 1, 200, c.location.grid_x)) {
      SetBad(bad_field, bad_cap, "location.nx"); return ImportResult::BadValue;
    }
    if (!loc["ny"].isNull() && !GetInt(loc["ny"], 1, 200, c.location.grid_y)) {
      SetBad(bad_field, bad_cap, "location.ny"); return ImportResult::BadValue;
    }
    if (settings::ValidateLocation(c.location.grid_x, c.location.grid_y, c.location.display_name,
                                   c.location.air_station) != Validation::Ok) {
      SetBad(bad_field, bad_cap, "location"); return ImportResult::BadValue;
    }
  }

  JsonVariantConst watch = root["watchlist"];
  if (!watch.isNull()) {
    JsonArrayConst arr = watch.as<JsonArrayConst>();
    if (!watch.is<JsonArrayConst>() || arr.size() > kMaxWatchlist) {
      SetBad(bad_field, bad_cap, "watchlist"); return ImportResult::BadValue;
    }
    char list[kMaxWatchlist * 24 + 1];
    size_t used = 0;
    for (JsonVariantConst item : arr) {
      const char* t = item.as<const char*>();
      if (!t || strlen(t) > 16) { SetBad(bad_field, bad_cap, "watchlist"); return ImportResult::BadValue; }
      used += (size_t)snprintf(list + used, sizeof(list) - used, "%s%s", used ? "," : "", t);
    }
    list[used] = '\0';
    // 같은 종목 중복 거부.
    for (size_t i = 0; i < arr.size(); i++)
      for (size_t j = i + 1; j < arr.size(); j++) {
        const char* a = arr[i].as<const char*>();
        const char* b = arr[j].as<const char*>();
        if (a && b && strncmp(a, b, kMaxSymbolLen) == 0) {
          SetBad(bad_field, bad_cap, "watchlist"); return ImportResult::BadValue;
        }
      }
    if (settings::ParseWatchlist(list, c.watchlist, kMaxWatchlist, c.watch_count) != Validation::Ok) {
      SetBad(bad_field, bad_cap, "watchlist"); return ImportResult::BadValue;
    }
  }

  JsonVariantConst prov = root["providers"];
  if (!prov.isNull()) {
    if (!prov.is<JsonObjectConst>() ||
        (!prov["fsc"].isNull() && !GetBool(prov["fsc"], c.providers.fsc_enabled)) ||
        (!prov["kis"].isNull() && !GetBool(prov["kis"], c.providers.kis_enabled))) {
      SetBad(bad_field, bad_cap, "providers"); return ImportResult::BadValue;
    }
  }

  JsonVariantConst dev = root["device"];
  if (!dev.isNull()) {
    DeviceConfig& d = c.device;
    int lan = (int)d.lan_mode;
    bool ok = dev.is<JsonObjectConst>();
    ok = ok && (dev["charge_protect"].isNull() || GetBool(dev["charge_protect"], d.charge_protect));
    ok = ok && (dev["fast_refresh"].isNull() || GetBool(dev["fast_refresh"], d.fast_refresh));
    ok = ok && (dev["page"].isNull() || GetInt(dev["page"], 0, 3, c.display_page));
    ok = ok && (dev["lan_mode"].isNull() || GetInt(dev["lan_mode"], 0, 2, lan));
    d.lan_mode = (LanPortalMode)lan;
    ok = ok && (dev["auto_rotate"].isNull() || GetBool(dev["auto_rotate"], d.auto_rotate));
    ok = ok && (dev["rotate_minutes"].isNull() ||
                GetInt(dev["rotate_minutes"], kRotateMinMinutes, kRotateMaxMinutes, d.rotate_minutes));
    ok = ok && (dev["market_hours_only"].isNull() || GetBool(dev["market_hours_only"], d.market_hours_only));
    ok = ok && (dev["market_from_hour"].isNull() || GetInt(dev["market_from_hour"], 0, 23, d.market_from_hour));
    ok = ok && (dev["market_to_hour"].isNull() || GetInt(dev["market_to_hour"], 1, 24, d.market_to_hour));
    ok = ok && (dev["refresh_scale_pct"].isNull() ||
                GetInt(dev["refresh_scale_pct"], kRefreshScaleMin, kRefreshScaleMax, d.refresh_scale_pct));
    if (!ok || settings::ValidateDevice(d) != Validation::Ok) {
      SetBad(bad_field, bad_cap, "device"); return ImportResult::BadValue;
    }
  }
  inout = c;
  return ImportResult::Ok;
}

}  // namespace portal
}  // namespace kdash
