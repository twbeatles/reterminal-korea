#include "RuntimeStore.h"

#include <memory>
#include <new>
#include <string.h>

#include "CacheStore.h"
#include "Codec.h"
#include "SettingsCodec.h"
#include "SlotStore.h"
#include "SnapshotCodec.h"

namespace kdash {
namespace {
constexpr size_t kCap = 4092;
constexpr size_t kHeader = 28;
constexpr size_t kProviders = (size_t)sched::Provider::Count;
constexpr uint16_t kRuntimeKind = 5;

bool Fingerprint(const Settings& settings, uint32_t& hash) {
  // Data-relevant settings only (ADR 0026): display/device options do not
  // change what the cached values mean. Internal equality check, not a secret.
  hash = settings_codec::DataFingerprint(settings);
  return hash != 0;
}

uint32_t Critical(const app::ProviderStatus* status) {
  uint8_t bytes[kProviders];
  for (size_t i = 0; i < kProviders; i++) {
    bytes[i] = (status[i].auth_blocked ? 1 : 0) |
               (status[i].last_outcome == Outcome::QuotaExceeded ? 2 : 0);
  }
  return Crc32(bytes, sizeof(bytes));
}

bool ValidStatus(const app::ProviderStatus* status) {
  for (size_t i = 0; i < kProviders; i++) {
    const auto& s = status[i];
    if (s.attempts < 0 || s.attempts > 1000 ||
        (uint8_t)s.last_outcome > (uint8_t)Outcome::ProviderError ||
        s.last_success_utc < 0 || s.last_attempt_utc < 0 ||
        s.retry_after_utc < 0 || s.last_base_utc < 0) return false;
  }
  return true;
}
}  // namespace

bool RuntimeStore::Restore(IStorage& storage, const Settings& settings,
                           app::AppState& state, app::Snapshot& snapshot) {
  uint32_t fp = 0;
  if (!Fingerprint(settings, fp)) return false;
  std::unique_ptr<uint8_t[]> bytes(new (std::nothrow) uint8_t[kCap]);
  if (!bytes) return false;
  size_t len = 0;
  if (!slot::Read(storage, "run", kRuntimeKind, bytes.get(), kCap, len))
    return false;
  codec::Reader r(bytes.get(), len);
  uint16_t version = 0, writes = 0, snap_len = 0, status_len = 0;
  uint32_t stored_fp = 0;
  int64_t last = 0, day = 0;
  if (!r.u16(version) || version != 1 || !r.u32(stored_fp) || stored_fp != fp ||
      !r.i64(last) || !r.i64(day) || !r.u16(writes) ||
      !r.u16(snap_len) || !r.u16(status_len) || r.pos() != kHeader ||
      last <= 0 || day < 0 || writes > 48 ||
      (size_t)snap_len + status_len != r.remaining()) return false;
  const uint8_t *snap_bytes = nullptr, *status_bytes = nullptr;
  if (!r.bytes(snap_bytes, snap_len) || !r.bytes(status_bytes, status_len) ||
      !r.done()) return false;
  std::unique_ptr<app::Snapshot> pending(new (std::nothrow) app::Snapshot);
  app::ProviderStatus status[kProviders];
  if (!pending || !snapshot_codec::DecodeSnapshot(snap_bytes, snap_len, *pending) ||
      !snapshot_codec::DecodeStatus(status_bytes, status_len, status, kProviders) ||
      !ValidStatus(status)) return false;
  // Commit all fields only after both codecs and binding pass. Do not restore
  // time_verified: clock trust always comes from this boot's clock adapter.
  snapshot = *pending;
  state.last_fetch_utc = 0;
  for (size_t i = 0; i < kProviders; i++) {
    state.providers[i] = status[i];
    if (state.last_fetch_utc < status[i].last_success_utc)
      state.last_fetch_utc = status[i].last_success_utc;
  }
  fingerprint_ = fp;
  digest_ = Crc32(bytes.get() + kHeader, len - kHeader);
  critical_ = Critical(status);
  last_write_utc_ = last;
  day_ = day;
  writes_today_ = writes;
  loaded_ = true;
  budget_known_ = true;
  retry_origin_utc_ = retry_after_utc_ = 0;
  return true;
}

bool RuntimeStore::Checkpoint(IStorage& storage, const app::AppState& state,
                              const app::Snapshot& snapshot, int64_t now,
                              bool allow_write) {
  checkpoint_failed_ = false;
  if (!allow_write || !state.time_verified || now <= 0) return false;
  if (retry_after_utc_) {
    if (now < retry_origin_utc_) { retry_origin_utc_ = now; retry_after_utc_ = now + 300; }
    if (now < retry_after_utc_) return false;
  }
  if (!ValidStatus(state.providers)) return FailCheckpoint(now);
  uint32_t fp = 0;
  if (!Fingerprint(state.settings, fp)) return FailCheckpoint(now);
  std::unique_ptr<uint8_t[]> bytes(new (std::nothrow) uint8_t[kCap]);
  if (!bytes) return FailCheckpoint(now);
  size_t sn = snapshot_codec::EncodeSnapshot(snapshot, bytes.get() + kHeader,
                                              kCap - kHeader);
  if (!sn) return FailCheckpoint(now);
  size_t pn = snapshot_codec::EncodeStatus(state.providers, kProviders,
      bytes.get() + kHeader + sn, kCap - kHeader - sn);
  if (!pn) return FailCheckpoint(now);
  const uint32_t digest = Crc32(bytes.get() + kHeader, sn + pn);
  const uint32_t critical = Critical(state.providers);
  const int64_t day = (now + 9 * 3600) / 86400;
  // A backwards clock must not reset the flash-wear budget. Only advancing
  // into a genuinely later KST day opens the next day's allowance.
  const int used = (budget_known_ && day <= day_) ? writes_today_ : 0;
  const int64_t budget_day = budget_known_ && day < day_ ? day_ : day;
  if (used >= 48) return false;
  const bool changed_config = !loaded_ || fp != fingerprint_;
  if (!changed_config && digest == digest_) return false;
  if (!changed_config && critical == critical_ &&
      !slot::SnapshotWriteAllowed(true, now, last_write_utc_, used)) return false;
  codec::Writer w(bytes.get(), kHeader);
  w.u16(1); w.u32(fp); w.i64(now); w.i64(budget_day);
  w.u16((uint16_t)(used + 1)); w.u16((uint16_t)sn); w.u16((uint16_t)pn);
  if (!w.ok() || w.pos() != kHeader) return FailCheckpoint(now);
  const size_t len = kHeader + sn + pn;
  uint32_t gen = 0;
  // Reserve before touching flash: a failed/read-back-unconfirmed slot may
  // already have consumed erase/program cycles. Do not count successes only.
  writes_today_ = used + 1; day_ = budget_day; budget_known_ = true;
  bool written = slot::Write(storage, "run", kRuntimeKind, bytes.get(), len,
                             state.last_fetch_utc, now, gen);
  // Always inspect read-back: a durable slot may exist despite hint failure.
  std::unique_ptr<uint8_t[]> check(new (std::nothrow) uint8_t[kCap]);
  size_t actual = 0;
  if (!check || !slot::Read(storage, "run", kRuntimeKind, check.get(), kCap, actual) ||
      actual != len || memcmp(bytes.get(), check.get(), len) != 0) return FailCheckpoint(now);
  (void)written;
  fingerprint_ = fp; digest_ = digest; critical_ = critical;
  last_write_utc_ = now; day_ = budget_day; writes_today_ = used + 1; loaded_ = true;
  retry_origin_utc_ = retry_after_utc_ = 0;
  return true;
}

bool RuntimeStore::Invalidate(IStorage& storage) {
  // No namespace erase: only our owned runtime keys, and no device action
  // occurs unless the application calls this after an authorized reset/save.
  storage.erase("run.a"); storage.erase("run.b"); storage.erase("run.meta");
  size_t len = 0;
  // erase(false) also means a key was absent on Preferences, so verify by read.
  std::unique_ptr<uint8_t[]> check(new (std::nothrow) uint8_t[kCap]);
  if (!check) return false;
  bool present = slot::Read(storage, "run", kRuntimeKind, check.get(), kCap, len);
  if (present) return false;
  loaded_ = false; last_write_utc_ = 0; writes_today_ = 0;
  budget_known_ = false; retry_origin_utc_ = retry_after_utc_ = 0;
  return true;
}
}  // namespace kdash
