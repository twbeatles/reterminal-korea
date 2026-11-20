#include "Log.h"

#if defined(ARDUINO) && defined(KDASH_BOARD_E1001)
#include <Arduino.h>
#define KDASH_LOGDEV Serial1
#else
#include <stdio.h>
#define KDASH_LOGDEV stdout
#endif

namespace kdash {
namespace log {
namespace {
Level g_level = Level::Info;
}

void SetLevel(Level l) { g_level = l; }
Level GetLevel() { return g_level; }

void KV(const char* key, const char* label) {
#if defined(ARDUINO) && defined(KDASH_BOARD_E1001)
  KDASH_LOGDEV.printf("%s=%s\n", key ? key : "?", label ? label : "?");
#else
  fprintf(KDASH_LOGDEV, "%s=%s\n", key ? key : "?", label ? label : "?");
#endif
}

void KV(const char* key, int value) {
#if defined(ARDUINO) && defined(KDASH_BOARD_E1001)
  KDASH_LOGDEV.printf("%s=%d\n", key ? key : "?", value);
#else
  fprintf(KDASH_LOGDEV, "%s=%d\n", key ? key : "?", value);
#endif
}

void KV(const char* key, long long value) {
#if defined(ARDUINO) && defined(KDASH_BOARD_E1001)
  KDASH_LOGDEV.printf("%s=%lld\n", key ? key : "?", value);
#else
  fprintf(KDASH_LOGDEV, "%s=%lld\n", key ? key : "?", value);
#endif
}

void Line(const char* text) {
#if defined(ARDUINO) && defined(KDASH_BOARD_E1001)
  KDASH_LOGDEV.println(text ? text : "");
#else
  fprintf(KDASH_LOGDEV, "%s\n", text ? text : "");
#endif
}

}  // namespace log
}  // namespace kdash
