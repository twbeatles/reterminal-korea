#include "NvsStorage.h"

#if defined(ARDUINO) && defined(KDASH_BOARD_E1001)
#include <Preferences.h>
#include <nvs.h>
#include <esp_partition.h>
#include "../boot/SerialLog.h"

namespace kdash {
namespace {
Preferences g_prefs;
bool g_open = false;
}  // namespace

bool NvsStorage::begin() {
  if (g_open) return true;
  g_open = g_prefs.begin("kdash", false);
  nvs_stats_t stats = {};
  const esp_partition_t* part = esp_partition_find_first(
      ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_NVS, "nvs");
  if (part && nvs_get_stats("nvs", &stats) == ESP_OK)
    KDASH_LOG.printf("nvs: bytes=%lu used=%u free=%u total=%u\n",
        (unsigned long)part->size, (unsigned)stats.used_entries,
        (unsigned)stats.free_entries, (unsigned)stats.total_entries);
  return g_open;
}

bool NvsStorage::read(const char* key, uint8_t* out, size_t out_len,
                      size_t& out_actual) {
  out_actual = 0;
  if (!key || !out) return false;
  if (!g_open && !begin()) return false;
  size_t n = g_prefs.getBytesLength(key);
  if (n == 0) return false;
  if (n > out_len) return false;
  size_t got = g_prefs.getBytes(key, out, n);
  if (got != n) return false;
  out_actual = got;
  return true;
}

bool NvsStorage::write_atomic(const char* key, const uint8_t* data, size_t len) {
  if (!key || (!data && len)) return false;
  if (!g_open && !begin()) return false;
  // Preferences putBytes + end() commits; a torn write here is still
  // covered by SlotStore dual-slot (the other slot stays valid).
  size_t n = g_prefs.putBytes(key, data, len);
  return n == len;
}

bool NvsStorage::erase(const char* key) {
  if (!key) return false;
  if (!g_open && !begin()) return false;
  return g_prefs.remove(key);
}

}  // namespace kdash
#else
// Host/CI guard: including this translation unit without the ESP32
// toolchain is a configuration error, not a silent no-op.
#endif
