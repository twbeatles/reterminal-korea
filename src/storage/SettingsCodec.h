#pragma once
// Settings <-> bytes with explicit field serialization + migration.
// Writes v4, reads v2/v3/v4. Unknown versions are rejected
// (caller falls back to defaults + portal flag), never crashed on.
#include <stddef.h>
#include <stdint.h>

#include "../../include/Config.h"

namespace kdash {
namespace settings_codec {

// Encodes s into out. Returns payload bytes or 0 on overflow.
size_t Encode(const Settings& s, uint8_t* out, size_t out_cap);
// Decodes; validates lengths and UTF-8 completeness. False => use defaults.
bool Decode(const uint8_t* data, size_t len, Settings& out);
// Explicit migration dispatch. Current version is validated atomically.
// Unsupported/truncated versions return false without changing out. No v1
// wire contract has shipped; a future version needs a reviewed codec + fixture.
bool Migrate(int from_version, const uint8_t* data, size_t len, Settings& out);

// 데이터 지문: 받아 둔 값의 의미를 바꾸는 설정(Wi-Fi·위치·측정소·키·관심종목·공급자 사용)만의 CRC.
// 화면 동작 설정(DeviceConfig, 시작 화면)은 빠진다 — 순환 간격만 바꿔도 캐시를 버리지 않게(ADR 0026).
// 런타임 캐시(NVS/RTC)를 설정에 묶는 데 쓴다. 비밀 보호 수단이 아니다. 실패하면 0.
uint32_t DataFingerprint(const Settings& s);

}  // namespace settings_codec
}  // namespace kdash
