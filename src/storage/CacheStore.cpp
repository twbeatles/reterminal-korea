#include "CacheStore.h"

#include <string.h>

namespace kdash {

uint32_t Crc32(const uint8_t* data, size_t len) {
  uint32_t crc = 0xFFFFFFFFu;
  for (size_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (int b = 0; b < 8; b++) {
      crc = (crc & 1) ? (crc >> 1) ^ 0xEDB88320u : (crc >> 1);
    }
  }
  return crc ^ 0xFFFFFFFFu;
}

size_t CacheEncode(const uint8_t* payload, size_t payload_len,
                   uint16_t payload_kind, int64_t observed_utc,
                   int64_t fetch_utc, uint8_t* out, size_t out_cap) {
  const size_t need = sizeof(CacheHeader) + payload_len;
  if (!out || out_cap < need) return 0;
  CacheHeader h;
  h.payload_kind = payload_kind;
  h.source_observed_utc = observed_utc;
  h.last_fetch_utc = fetch_utc;
  h.payload_len = (uint32_t)payload_len;
  h.crc32 = 0;
  memcpy(out, &h, sizeof(h));
  if (payload_len) memcpy(out + sizeof(h), payload, payload_len);
  // CRC over header(zero crc field) + payload.
  uint32_t crc = Crc32(out, sizeof(h) + payload_len);
  memcpy(out + offsetof(CacheHeader, crc32), &crc, sizeof(crc));
  return need;
}

bool CacheDecode(const uint8_t* blob, size_t blob_len, CacheHeader& header_out,
                 const uint8_t*& payload_out, size_t& payload_len_out) {
  payload_out = nullptr;
  payload_len_out = 0;
  if (!blob || blob_len < sizeof(CacheHeader)) return false;
  CacheHeader h;
  memcpy(&h, blob, sizeof(h));
  if (h.magic != kCacheMagic) return false;
  if (h.schema_version != 1 && h.schema_version != 2)
    return false;  // 그 외 스키마 => 안전 재설정
  if (blob_len != sizeof(CacheHeader) + h.payload_len) return false;
  uint32_t want = 0;
  memcpy(&want, blob + offsetof(CacheHeader, crc32), sizeof(want));
  // Recompute with the stored crc field zeroed.
  uint8_t tmp[sizeof(CacheHeader)];
  memcpy(tmp, blob, sizeof(tmp));
  memset(tmp + offsetof(CacheHeader, crc32), 0, sizeof(uint32_t));
  uint32_t crc = 0xFFFFFFFFu;
  for (size_t i = 0; i < sizeof(tmp); i++) {
    crc ^= tmp[i];
    for (int b = 0; b < 8; b++) crc = (crc & 1) ? (crc >> 1) ^ 0xEDB88320u : (crc >> 1);
  }
  const uint8_t* pl = blob + sizeof(CacheHeader);
  for (size_t i = 0; i < h.payload_len; i++) {
    crc ^= pl[i];
    for (int b = 0; b < 8; b++) crc = (crc & 1) ? (crc >> 1) ^ 0xEDB88320u : (crc >> 1);
  }
  if ((crc ^ 0xFFFFFFFFu) != want) return false;
  header_out = h;
  payload_out = pl;
  payload_len_out = h.payload_len;
  return true;
}

}  // namespace kdash
