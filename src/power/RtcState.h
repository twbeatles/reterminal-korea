#pragma once
// RtcState: deep-sleep-preserved bundle (T05 3-2). Magic + version + CRC32.
// Holds Snapshot + ProviderStatus[7] + KisSession + render counters.
// Serialization reuses SnapshotCodec; CRC is mandatory. Portable logic +
// RTC_NOINIT_ATTR backing on device; magic/length/CRC checked before restore.
#include <stddef.h>
#include <stdint.h>

#include "../app/Coordinator.h"
#include "../app/Fetcher.h"

namespace kdash {
namespace rtc {

inline constexpr uint32_t kMagic = 0x4B445254;  // 'KDRT'
inline constexpr uint16_t kVersion = 2;
struct RenderState {
  uint32_t content_hash = 0;
  int64_t refreshed_utc = 0;
};

struct Header {
  uint32_t magic = kMagic;
  uint16_t version = kVersion;
  uint16_t reserved = 0;
  uint32_t crc32 = 0;  // over payload only
};

// Payload size upper bound (Snapshot ~2KB + status + session + counters).
inline constexpr size_t kPayloadMax = 4096;

uint32_t PayloadCrc(const uint8_t* payload, size_t len);

// Encodes state bundle into out (Header + payload). Returns total or 0.
size_t Encode(const app::Snapshot& snap, const app::ProviderStatus* status,
              size_t n_status, const app::KisSession& kis, uint8_t page,
              uint32_t last_rendered_crc, uint8_t partials_since_full,
              uint32_t boot_count, uint32_t crashes, uint8_t* out,
              size_t out_cap, const RenderState& render = RenderState{});
// Decodes; false on magic/version/crc/decode failure (caller falls back
// to NVS snapshot or clears).
bool Decode(const uint8_t* blob, size_t len, app::Snapshot& snap,
            app::ProviderStatus* status, size_t n_status,
            app::KisSession& kis, uint8_t& page, uint32_t& last_rendered_crc,
            uint8_t& partials_since_full, uint32_t& boot_count,
            uint32_t& crashes, RenderState* render = nullptr);

}  // namespace rtc
}  // namespace kdash
