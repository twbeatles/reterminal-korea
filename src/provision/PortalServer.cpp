#include "PortalServer.h"

#include <memory>
#include <new>
#include <string.h>
#include <ctype.h>

#include "../storage/SettingsCodec.h"
#include "../storage/SettingsStore.h"
#include "../storage/SlotStore.h"

namespace kdash { namespace portal {
namespace {
bool g_last_save_changed_data = true;
bool Same(const char* a, size_t n, const char* b) {
  if (strlen(b) != n) return false;
  for (size_t i = 0; i < n; i++)
    if (tolower((unsigned char)a[i]) != (unsigned char)b[i]) return false;
  return true;
}
}
int CheckPortalHeaders(const char* bytes, size_t len) {
  if (!bytes || len > 2048) return 431;
  for (size_t i = 0; i < len; i++) if (!bytes[i]) return 400;
  const char* finish = nullptr;
  for (size_t i = 3; i < len; i++) {
    if (memcmp(bytes + i - 3, "\r\n\r\n", 4) == 0) { finish = bytes + i + 1; break; }
  }
  if (!finish) return len == 2048 ? 431 : 0;
  const char* end = bytes;
  while (end + 1 < finish && !(end[0] == '\r' && end[1] == '\n')) end++;
  bool post = end - bytes >= 5 && memcmp(bytes, "POST ", 5) == 0;
  bool get = end - bytes >= 4 && memcmp(bytes, "GET ", 4) == 0;
  if (!get && !post) return 405;
  if (end - bytes > 256) return 414;
  for (const char* p = bytes; p < end; p++)
    if (*p == '?' || *p == '#' || (unsigned char)*p < 32) return 400;
  bool length_seen = false, form = false;
  size_t body_len = 0;
  for (const char* line = end + 2; line + 1 < finish && *line != '\r';) {
    end = line;
    while (end + 1 < finish && !(end[0] == '\r' && end[1] == '\n')) end++;
    const char* colon = line;
    while (colon < end && *colon != ':') colon++;
    if (colon == end || colon == line) return 400;
    const char* value = colon + 1;
    while (value < end && *value == ' ') value++;
    if (Same(line, colon - line, "transfer-encoding")) return 400;
    if (Same(line, colon - line, "content-length")) {
      if (length_seen || value == end) return 400;
      length_seen = true;
      for (const char* p = value; p < end; p++) {
        if (!isdigit((unsigned char)*p)) return 400;
        body_len = body_len * 10 + (*p - '0');
        if (body_len > 8192) return 413;
      }
    }
    if (Same(line, colon - line, "content-type")) {
      static const char type[] = "application/x-www-form-urlencoded";
      form = end - value >= (int)(sizeof(type) - 1) &&
          Same(value, sizeof(type) - 1, type) &&
          (end - value == (int)sizeof(type) - 1 || value[sizeof(type) - 1] == ';');
    }
    line = end + 2;
  }
  if (post && !length_seen) return 411;
  if (post && !form) return 415;
  if (get && body_len) return 400;
  return 200;
}

bool SaveSettingsVerified(IStorage& storage, Settings& current, const Settings& pending) {
  std::unique_ptr<uint8_t[]> bytes(new (std::nothrow) uint8_t[2048]);
  std::unique_ptr<uint8_t[]> check(new (std::nothrow) uint8_t[2048]);
  std::unique_ptr<Settings> valid(new (std::nothrow) Settings);
  if (!bytes || !check || !valid) return false;
  size_t len = settings_codec::Encode(pending, bytes.get(), 2048);
  if (!len || !settings_codec::Decode(bytes.get(), len, *valid)) return false;
  uint32_t gen = 0;
  slot::Write(storage, "set", slot::kKindSettings, bytes.get(), len, 0, 0, gen);
  size_t actual = 0;
  bool ok = slot::Read(storage, "set", slot::kKindSettings, check.get(), 2048, actual) &&
      actual == len && memcmp(bytes.get(), check.get(), len) == 0;
  memset(bytes.get(), 0, 2048); memset(check.get(), 0, 2048);
  if (ok) {
    const uint32_t before = settings_codec::DataFingerprint(current);
    const uint32_t after = settings_codec::DataFingerprint(*valid);
    g_last_save_changed_data = !before || !after || before != after;  // unknown => changed
    current = *valid;
  }
  return ok;
}

bool LastSaveChangedData() { return g_last_save_changed_data; }
} }
