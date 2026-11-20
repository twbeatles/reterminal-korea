#pragma once
// Log: single logging funnel (F-04). Field API only — no URL/key/secret
// types accepted, so secrets cannot reach UART by construction.
// Levels: ERROR/WARN/INFO/DEBUG. Release default INFO.
#include <stdint.h>

namespace kdash {
namespace log {

enum class Level : uint8_t { Error = 0, Warn, Info, Debug };

void SetLevel(Level l);
Level GetLevel();

// Field loggers (all values are labels/numbers, never secrets).
void KV(const char* key, const char* label);
void KV(const char* key, int value);
void KV(const char* key, long long value);
void Line(const char* text);  // fixed literals only

}  // namespace log
}  // namespace kdash
