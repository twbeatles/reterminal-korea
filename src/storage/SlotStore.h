#pragma once
// SlotStore: dual-slot atomic persistence over IStorage (T02 3-2).
// - Two blob slots (A/B) framed by CacheStore (magic/schema/crc).
// - Payload starts with u32 generation; the newest valid generation wins
//   using serial-number arithmetic (wrap safe). Both read AND write recover
//   from the slots alone; meta is never trusted as the active-slot authority.
// - Payload kinds: 1=settings, 2=snapshot, 3=status, 4=calendar.
// Portable; FakeStorage in tests injects torn writes.
#include <stddef.h>
#include <stdint.h>

#include "../../include/IStorage.h"

namespace kdash {
namespace slot {

inline constexpr uint16_t kKindSettings = 1;
inline constexpr uint16_t kKindSnapshot = 2;
inline constexpr uint16_t kKindStatus = 3;
inline constexpr uint16_t kKindCalendar = 4;

struct Meta {
  uint8_t active = 0;  // 0=A, 1=B (hint)
  uint32_t gen = 0;
};

// Writes payload atomically under base_key ("set"/"snap"/...):
// blobs at "<base>.a"/"<base>.b", hint at "<base>.meta".
bool Write(IStorage& st, const char* base_key, uint16_t kind,
           const uint8_t* payload, size_t payload_len, int64_t observed_utc,
           int64_t now_utc, uint32_t& out_gen);
// Reads newest valid payload into out (out_len = payload bytes). False =>
// caller uses defaults + portal flag. Never returns torn/corrupt data.
bool Read(IStorage& st, const char* base_key, uint16_t kind, uint8_t* out,
          size_t out_cap, size_t& out_len);

// Throttled snapshot gate (T02 3-5): true when a snapshot NVS write is
// allowed (changed && >=30min since last && daily budget left).
bool SnapshotWriteAllowed(bool changed, int64_t now_utc, int64_t last_write_utc,
                          int writes_today, int max_writes_per_day = 48);

}  // namespace slot
}  // namespace kdash
