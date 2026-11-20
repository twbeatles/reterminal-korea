#include "SlotStore.h"

#include <memory>
#include <new>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "CacheStore.h"

namespace kdash {
namespace slot {
namespace {
constexpr size_t kBlobCap = 4608;
constexpr size_t kPayloadCap = 4092;

bool Keys(const char* base, char* a, char* b, char* meta) {
  // ESP-IDF NVS keys have at most 15 characters, including our .meta suffix.
  if (!base || !*base || strlen(base) > 10) return false;
  snprintf(a, 16, "%s.a", base);
  snprintf(b, 16, "%s.b", base);
  snprintf(meta, 16, "%s.meta", base);
  return true;
}

struct SlotInfo {
  bool valid = false;
  uint32_t gen = 0;
  const uint8_t* payload = nullptr;
  size_t len = 0;
};

SlotInfo ReadSlot(IStorage& st, const char* key, uint16_t kind, uint8_t* buf) {
  SlotInfo info;
  size_t actual = 0;
  if (!st.read(key, buf, kBlobCap, actual) || actual > kBlobCap) return info;
  CacheHeader h;
  const uint8_t* pl = nullptr;
  size_t len = 0;
  if (!CacheDecode(buf, actual, h, pl, len) || h.schema_version != 2 ||
      h.payload_kind != kind || len < 4 || len - 4 > kPayloadCap) return info;
  info.gen = (uint32_t)pl[0] | ((uint32_t)pl[1] << 8) |
             ((uint32_t)pl[2] << 16) | ((uint32_t)pl[3] << 24);
  if (!info.gen) return info;
  info.payload = pl + 4;
  info.len = len - 4;
  info.valid = true;
  return info;
}

// Serial-number arithmetic keeps recovery correct across uint32 wrap.
bool Newer(uint32_t candidate, uint32_t current) {
  return candidate != current && (uint32_t)(candidate - current) < 0x80000000u;
}

bool PreferB(const SlotInfo& a, const SlotInfo& b) {
  return b.valid && (!a.valid || b.gen == a.gen || Newer(b.gen, a.gen));
}
}  // namespace

bool Write(IStorage& st, const char* base, uint16_t kind,
           const uint8_t* payload, size_t len, int64_t observed,
           int64_t now, uint32_t& out_gen) {
  char ka[16], kb[16], mk[16];
  if (!Keys(base, ka, kb, mk) || (!payload && len) || len > kPayloadCap)
    return false;
  // Bounded, failure-checked heap scratch: never a multi-KB task frame.
  std::unique_ptr<uint8_t[]> scratch(new (std::nothrow) uint8_t[kBlobCap]);
  if (!scratch) return false;
  SlotInfo a = ReadSlot(st, ka, kind, scratch.get());
  SlotInfo b = ReadSlot(st, kb, kind, scratch.get());
  const bool active_b = PreferB(a, b);
  const uint32_t gen = active_b ? b.gen : (a.valid ? a.gen : 0);
  uint32_t next = gen + 1;
  if (!next) next = 1;
  // Never trust the meta hint to choose the overwrite target: a previous
  // slot commit may have succeeded while the meta write failed.
  const char* target = (a.valid || b.valid) ? (active_b ? ka : kb) : kb;
  const uint8_t target_id = (target == kb) ? 1 : 0;
  CacheHeader h;
  h.schema_version = 2;
  h.payload_kind = kind;
  h.source_observed_utc = observed;
  h.last_fetch_utc = now;
  h.payload_len = (uint32_t)(len + 4);
  const size_t bytes = sizeof(h) + len + 4;
  memcpy(scratch.get(), &h, sizeof(h));
  uint8_t* inner = scratch.get() + sizeof(h);
  for (int i = 0; i < 4; i++) inner[i] = (uint8_t)(next >> (8 * i));
  if (len) memcpy(inner + 4, payload, len);
  uint32_t crc = Crc32(scratch.get(), bytes);
  memcpy(scratch.get() + offsetof(CacheHeader, crc32), &crc, sizeof(crc));
  if (!st.write_atomic(target, scratch.get(), bytes)) return false;
  // The slot is already recoverable by generation even if this hint fails.
  uint8_t hint[5] = {target_id, (uint8_t)next, (uint8_t)(next >> 8),
                     (uint8_t)(next >> 16), (uint8_t)(next >> 24)};
  if (!st.write_atomic(mk, hint, sizeof(hint))) return false;
  out_gen = next;
  return true;
}

bool Read(IStorage& st, const char* base, uint16_t kind, uint8_t* out,
          size_t cap, size_t& out_len) {
  out_len = 0;
  char ka[16], kb[16], mk[16];
  if (!out || !Keys(base, ka, kb, mk)) return false;
  std::unique_ptr<uint8_t[]> scratch(new (std::nothrow) uint8_t[kBlobCap]);
  if (!scratch) return false;
  SlotInfo a = ReadSlot(st, ka, kind, scratch.get());
  SlotInfo b = ReadSlot(st, kb, kind, scratch.get());
  SlotInfo best = b;
  if (!PreferB(a, b)) {
    if (!a.valid) return false;
    // B reused the same scratch; reload A only when it is the selected slot.
    best = ReadSlot(st, ka, kind, scratch.get());
    if (!best.valid || best.gen != a.gen || best.len != a.len) return false;
  }
  if (!best.valid || best.len > cap) return false;
  if (best.len) memcpy(out, best.payload, best.len);
  out_len = best.len;
  return true;
}

bool SnapshotWriteAllowed(bool changed, int64_t now, int64_t last,
                          int writes_today, int max_writes_per_day) {
  if (!changed || writes_today >= max_writes_per_day) return false;
  return last <= 0 || now - last >= 30 * 60;
}
}  // namespace slot
}  // namespace kdash
