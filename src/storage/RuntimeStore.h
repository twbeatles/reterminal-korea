#pragma once
// Bounded runtime checkpoint: snapshot and scheduler state share one atomic
// SlotStore generation. No tokens, pointers, credentials or clock trust stored.
#include "../../include/IStorage.h"
#include "../app/Fetcher.h"

namespace kdash {
class RuntimeStore {
 public:
  bool Restore(IStorage& storage, const Settings& settings,
               app::AppState& state, app::Snapshot& snapshot);
  // Call only with a verified UTC and at a permitted NVS write boundary
  // (USB idle or immediately before battery sleep). Returns true only after
  // read-back verification. Normal writes: >=30 min, <=48/day across boots.
  // New auth/quota blocks can checkpoint immediately within the daily cap.
  // I/O attempts consume the cap even on failure; errors cool down for 5 min.
  bool Checkpoint(IStorage& storage, const app::AppState& state,
                  const app::Snapshot& snapshot, int64_t now_utc,
                  bool allow_write = true);
  bool Invalidate(IStorage& storage);
  bool CheckpointFailed() const { return checkpoint_failed_; }
 private:
  bool FailCheckpoint(int64_t now) {
    checkpoint_failed_ = true;
    retry_origin_utc_ = now;
    retry_after_utc_ = now + 300;
    return false;
  }
  bool checkpoint_failed_ = false;
  uint32_t fingerprint_ = 0;
  uint32_t digest_ = 0;
  uint32_t critical_ = 0;
  int64_t last_write_utc_ = 0;
  int64_t day_ = 0;
  int writes_today_ = 0;
  bool loaded_ = false;
  bool budget_known_ = false;
  int64_t retry_origin_utc_ = 0;
  int64_t retry_after_utc_ = 0;
};
}  // namespace kdash
