#include "CalendarStore.h"
#include "Codec.h"
#include "SlotStore.h"
#include <string.h>

namespace kdash { namespace cal {
namespace {
size_t Encode(const YearTable (&tables)[2], uint8_t* out, size_t cap) {
  codec::Writer w(out, cap);
  w.u16(2);
  for (const YearTable& t : tables) {
    w.i32(t.year); w.u8(t.loaded ? 1 : 0); w.i64(t.fetched_utc);
    for (uint32_t b : t.bitmap) w.u32(b);
    for (uint32_t b : t.market_closed) w.u32(b);
    w.u8(t.market_verified ? 1 : 0);
  }
  return w.ok() ? w.pos() : 0;
}
}
bool LoadTables(IStorage& storage, YearTable (&out)[2]) {
  uint8_t bytes[256]; size_t n = 0;
  if (!slot::Read(storage, "cal", slot::kKindCalendar, bytes, sizeof(bytes), n)) return false;
  codec::Reader r(bytes, n);
  uint16_t version = 0;
  if (!r.u16(version) || (version != 1 && version != 2)) return false;
  YearTable tmp[2];
  for (YearTable& t : tmp) {
    int32_t year = 0; uint8_t loaded = 0;
    if (!r.i32(year) || !r.u8(loaded) || !r.i64(t.fetched_utc)) return false;
    if (loaded > 1 || (loaded && (year < 2000 || year > 2199))) return false;
    t.year = year; t.loaded = loaded != 0;
    for (uint32_t& b : t.bitmap) if (!r.u32(b) || (b & 0x80000000u)) return false;
    for (uint32_t& b : t.market_closed) if (!r.u32(b) || (b & 0x80000000u)) return false;
    if (version>=2) {
      uint8_t verified=0;
      if (!r.u8(verified) || verified>1) return false;
      t.market_verified=verified!=0;
    }
  }
  if (!r.done()) return false;
  memcpy(out, tmp, sizeof(tmp));
  return true;
}
bool SaveTables(IStorage& storage, const YearTable (&tables)[2], int64_t now) {
  uint8_t bytes[256], verify[256];
  const size_t n = Encode(tables, bytes, sizeof(bytes));
  uint32_t gen = 0; size_t got = 0;
  if (!n || !slot::Write(storage, "cal", slot::kKindCalendar, bytes, n, now, now, gen)) return false;
  return slot::Read(storage, "cal", slot::kKindCalendar, verify, sizeof(verify), got) &&
         got == n && memcmp(bytes, verify, n) == 0;
}
} }
