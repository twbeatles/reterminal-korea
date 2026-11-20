#include "RtcState.h"

#include <string.h>
#include <memory>
#include <new>

#include "../storage/CacheStore.h"
#include "../storage/Codec.h"
#include "../storage/SnapshotCodec.h"

namespace kdash {
namespace rtc {

uint32_t PayloadCrc(const uint8_t* payload, size_t len) {
  return Crc32(payload, len);
}

size_t Encode(const app::Snapshot& snap, const app::ProviderStatus* status,
              size_t n_status, const app::KisSession& kis, uint8_t page,
              uint32_t last_rendered_crc, uint8_t partials_since_full,
              uint32_t boot_count, uint32_t crashes, uint8_t* out,
              size_t out_cap, const RenderState& render) {
  if (!out || out_cap < sizeof(Header)) return 0;
  uint8_t* pl = out + sizeof(Header);
  size_t cap = out_cap - sizeof(Header);
  codec::Writer w(pl, cap);
  std::unique_ptr<uint8_t[]> sbuf(new (std::nothrow) uint8_t[3072]);
  if (!sbuf) return 0;
  size_t sn = snapshot_codec::EncodeSnapshot(snap, sbuf.get(), 3072);
  if (!sn) return 0;
  w.u16((uint16_t)sn);
  w.bytes(sbuf.get(), sn);
  uint8_t tbuf[512];
  size_t tn = snapshot_codec::EncodeStatus(status, n_status, tbuf, sizeof(tbuf));
  if (!tn) return 0;
  w.u16((uint16_t)tn);
  w.bytes(tbuf, tn);
  w.str(kis.token, 1024);
  w.i64(kis.expiry_utc);
  w.i64(kis.last_issue_attempt_utc);
  w.u8(page);
  w.u32(last_rendered_crc);
  w.u8(partials_since_full);
  w.u32(boot_count);
  w.u32(crashes);
  w.u32(render.content_hash);
  w.i64(render.refreshed_utc);
  if (!w.ok()) return 0;
  Header h;
  h.crc32 = Crc32(pl, w.pos());
  memcpy(out, &h, sizeof(h));
  return sizeof(h) + w.pos();
}

static bool DecodeUnsafe(const uint8_t* blob, size_t len, app::Snapshot& snap,
            app::ProviderStatus* status, size_t n_status,
            app::KisSession& kis, uint8_t& page, uint32_t& last_rendered_crc,
            uint8_t& partials_since_full, uint32_t& boot_count,
            uint32_t& crashes, RenderState& render) {
  if (!blob || len < sizeof(Header) || !status) return false;
  Header h;
  memcpy(&h, blob, sizeof(h));
  if (h.magic != kMagic || h.version != kVersion) return false;
  const uint8_t* pl = blob + sizeof(Header);
  size_t pl_len = len - sizeof(Header);
  uint32_t want = h.crc32;
  // CRC is over payload only.
  uint8_t zero[4] = {0};
  (void)zero;
  if (Crc32(pl, pl_len) != want) return false;
  codec::Reader r(pl, pl_len);
  uint16_t sn = 0, tn = 0;
  if (!r.u16(sn)) return false;
  const uint8_t* sbytes = nullptr;
  if (!r.bytes(sbytes, sn)) return false;
  if (!snapshot_codec::DecodeSnapshot(sbytes, sn, snap)) return false;
  if (!r.u16(tn)) return false;
  const uint8_t* tbytes = nullptr;
  if (!r.bytes(tbytes, tn)) return false;
  if (!snapshot_codec::DecodeStatus(tbytes, tn, status, n_status))
    return false;
  char tok[1024 + 1] = {0};
  if (!r.str(tok, sizeof(tok), 1024)) return false;
  strncpy(kis.token, tok, sizeof(kis.token) - 1);
  if (!r.i64(kis.expiry_utc)) return false;
  if (!r.i64(kis.last_issue_attempt_utc)) return false;
  uint8_t pg = 0, ps = 0;
  uint32_t lrc = 0, bc = 0, cr = 0;
  if (!r.u8(pg) || !r.u32(lrc) || !r.u8(ps)) return false;
  if (pg >= (uint8_t)ui::Page::Count) return false;
  if (!r.u32(bc) || !r.u32(cr)) return false;
  if (!r.u32(render.content_hash) || !r.i64(render.refreshed_utc)) return false;
  if (!r.done()) return false;
  page = pg;
  last_rendered_crc = lrc;
  partials_since_full = ps;
  boot_count = bc;
  crashes = cr;
  return true;
}

bool Decode(const uint8_t* blob, size_t len, app::Snapshot& snap,
            app::ProviderStatus* status, size_t n_status,
            app::KisSession& kis, uint8_t& page, uint32_t& last_rendered_crc,
            uint8_t& partials_since_full, uint32_t& boot_count,
            uint32_t& crashes, RenderState* render) {
  if (!status || n_status != (size_t)sched::Provider::Count) return false;
  struct Restored {
    app::Snapshot snap;
    app::ProviderStatus status[(int)sched::Provider::Count];
    app::KisSession kis;
    uint8_t page = 0, partials = 0;
    uint32_t crc = 0, boots = 0, crashes = 0;
    RenderState render;
  };
  std::unique_ptr<Restored> temp(new (std::nothrow) Restored);
  if (!temp || !DecodeUnsafe(blob, len, temp->snap, temp->status, n_status,
                            temp->kis, temp->page, temp->crc, temp->partials,
                            temp->boots, temp->crashes, temp->render)) return false;
  snap = temp->snap;
  memcpy(status, temp->status, sizeof(temp->status));
  kis = temp->kis;
  page = temp->page;
  last_rendered_crc = temp->crc;
  partials_since_full = temp->partials;
  boot_count = temp->boots;
  crashes = temp->crashes;
  if (render) *render = temp->render;
  return true;
}

}  // namespace rtc
}  // namespace kdash
