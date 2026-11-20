#pragma once
// Settings validation + migration + log redaction (docs/04).
// - Secrets are NEVER echoed (portal re-query shows '등록됨/재설정' only).
// - SSID/keys masked in every log line (mask helper shared by all loggers).
// - Corrupt settings => quarantine to safe defaults + re-enter portal.
// Portable. Mirrored by host/kdash/policy.py. Lengths are UTF-8 bytes.
#include "../../include/Config.h"

namespace kdash {
namespace settings {

enum class Validation : uint8_t { Ok = 0, BadGrid, BadStation, BadSsid,
                                  BadKey, BadSymbol, BadMarket, BadSecret,
                                  BadWatchlist, BadDevice };

const char* Describe(Validation v);

Validation ValidateLocation(int grid_x, int grid_y, const char* display_name,
                            const char* air_station);
// SSID 1..32 bytes. Secret: "" (open network), 8..63 printable ASCII, or
// exactly 64 hex digits (raw PSK) — WPA2 rules (G-7).
Validation ValidateWifi(const char* ssid, const char* secret);
// API key/secret: "" (feature off) or 1..kMaxKeyLen printable ASCII without
// spaces/quotes/backslashes.
Validation ValidateKey(const char* key);
// 기기 설정 범위: lan_mode 0..2, 순환 1..60분, 장 시간 from 0..23 < to 1..24, 배율 100..400.
Validation ValidateDevice(const DeviceConfig& d);
Validation ValidateWatchItem(const char* symbol, const char* market);

// "005930, 000660:NXT" -> WatchItem[]; market default KRX, suffix :KRX|:NXT|
// :통합. Empty tokens ("a,,b", trailing ',') are rejected; at most
// kMaxWatchlist items (G-8). out/count untouched on failure.
Validation ParseWatchlist(const char* text, WatchItem* out, size_t cap,
                          size_t& count);

// Complete, well-formed UTF-8 (no truncated multibyte tail, no overlongs).
bool IsValidUtf8(const char* s);

// Copies src into dst only if it fits completely (never silent truncation).
bool CopyBounded(char* dst, size_t dst_cap, const char* src);

// Migration: unknown/older schema => safe defaults + portal flag, never crash.
bool NeedsMigration(int stored_schema);
Settings DefaultSettings();

// Redaction: "등록됨"/"미등록" only. Any logger printing a raw secret is a
// SEC-01 failure.
void MaskSecret(const char* secret, char* out, size_t out_cap);
const char* RegisteredLabel(const char* secret);  // "등록됨" or "미등록"

}  // namespace settings
}  // namespace kdash
