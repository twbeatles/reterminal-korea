#pragma once
// CacheStore: value + source time + last fetch + schema + checksum framing.
// (02 캐시 정책, CACHE-01) Portable. Atomicity (dual-slot/tmp+rename) is the
// SlotStore's responsibility; CacheStore only validates/framed bytes.
#include <stddef.h>
#include <stdint.h>

namespace kdash {

inline constexpr uint32_t kCacheMagic = 0x4B444341;  // 'KDCA'

// CRC32 (IEEE) over header+payload — fault injection (bit flip) must be
// detected and routed to last-known-good or safe reset, never trusted.
uint32_t Crc32(const uint8_t* data, size_t len);

struct CacheHeader {
  uint32_t magic = kCacheMagic;
  // Schema namespace is CacheStore-local (encode writes 1, decode accepts
  // 1..2); it is unrelated to SlotStore's slot schema. Decode recomputes the
  // CRC inline because the stored crc field must read as zero mid-stream.
  uint16_t schema_version = 1;
  uint16_t payload_kind = 0;  // caller-defined (e.g. provider id)
  int64_t source_observed_utc = 0;
  int64_t last_fetch_utc = 0;
  uint32_t payload_len = 0;
  uint32_t crc32 = 0;  // over header(with crc=0) + payload
};

// Encodes header+payload into out (out_cap must fit). Returns total bytes or 0.
size_t CacheEncode(const uint8_t* payload, size_t payload_len,
                   uint16_t payload_kind, int64_t observed_utc,
                   int64_t fetch_utc, uint8_t* out, size_t out_cap);

// Decodes; verifies magic/schema/crc. false => caller uses last-known-good
// or safe re-init (CACHE-01). Never returns partial garbage as valid.
bool CacheDecode(const uint8_t* blob, size_t blob_len, CacheHeader& header_out,
                 const uint8_t*& payload_out, size_t& payload_len_out);

}  // namespace kdash
