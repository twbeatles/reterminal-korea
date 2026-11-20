namespace {
class FailedFlash : public FakeStorage {
 public:
  bool failing=true;
  bool write_atomic(const char* key,const uint8_t* bytes,size_t n) override {
    if (failing) { ++write_count; return false; }
    return FakeStorage::write_atomic(key,bytes,n);
  }
};
}

void test_storage_fault_cooldown_and_recovery() {
  FailedFlash flash; RuntimeStore store; app::AppState state; app::Snapshot snap;
  state.time_verified=true;
  const int64_t now=Kst("2026-11-13 00:10");
  TEST_ASSERT_FALSE(store.Checkpoint(flash,state,snap,now));
  TEST_ASSERT_TRUE(store.CheckpointFailed());
  for (int sec=1;sec<300;sec+=30) TEST_ASSERT_FALSE(store.Checkpoint(flash,state,snap,now+sec));
  TEST_ASSERT_EQUAL(1,flash.write_count);
  flash.failing=false;
  TEST_ASSERT_TRUE(store.Checkpoint(flash,state,snap,now+300));
  RuntimeStore restored; app::AppState back; app::Snapshot cached;
  TEST_ASSERT_TRUE(restored.Restore(flash,state.settings,back,cached));
}

void test_failed_flash_attempts_exhaust_daily_budget() {
  FailedFlash flash; RuntimeStore store; app::AppState state; app::Snapshot snap;
  state.time_verified=true;
  const int64_t now=Kst("2026-11-13 00:10");
  for (int i=0;i<60;i++) TEST_ASSERT_FALSE(store.Checkpoint(flash,state,snap,now+i*300));
  TEST_ASSERT_EQUAL(48,flash.write_count);
  TEST_ASSERT_FALSE(store.Checkpoint(flash,state,snap,now+86400));
  TEST_ASSERT_EQUAL(49,flash.write_count);  // later KST day reopens allowance
}

void test_failed_flash_clock_reversal_does_not_open_budget_or_stall_forever() {
  FailedFlash flash; RuntimeStore store; app::AppState state; app::Snapshot snap;
  state.time_verified=true;
  const int64_t now=Kst("2026-11-13 12:00");
  TEST_ASSERT_FALSE(store.Checkpoint(flash,state,snap,now));
  flash.failing=false;
  TEST_ASSERT_FALSE(store.Checkpoint(flash,state,snap,now-86400));
  TEST_ASSERT_FALSE(store.Checkpoint(flash,state,snap,now-86400+299));
  TEST_ASSERT_EQUAL(1,flash.write_count);
  TEST_ASSERT_TRUE(store.Checkpoint(flash,state,snap,now-86400+300));
  // Failure reservations survive in the next successfully verified header.
  RuntimeStore back; app::AppState restored; app::Snapshot restored_snap;
  TEST_ASSERT_TRUE(back.Restore(flash,state.settings,restored,restored_snap));
}

void test_charge_confirmation_is_atomic_with_status_read() {
  ChargerMock m; charge::Protection policy;
  ChargeSamples(policy,m,4100);
  TEST_ASSERT_TRUE(policy.confirmed().valid);
  TEST_ASSERT_EQUAL_HEX8(0x0A,policy.confirmed().cfg);
  TEST_ASSERT_EQUAL_HEX8(0x8F,policy.confirmed().timer);
  m.fail_read=8;  // final status fails even though control read-back succeeded
  TEST_ASSERT_EQUAL((int)charge::State::Fault,(int)policy.Tick(m,true,4100));
  TEST_ASSERT_FALSE(policy.confirmed().valid);
  TEST_ASSERT_EQUAL((int)charge::State::Fault,(int)policy.state());
  m.fail_read=-1;
  TEST_ASSERT_EQUAL((int)charge::State::Limited,(int)policy.Tick(m,true,4100));
  TEST_ASSERT_TRUE(policy.confirmed().valid);
}

void test_failed_write_reservations_survive_next_verified_checkpoint() {
  FailedFlash flash; RuntimeStore store; app::AppState state; app::Snapshot snap;
  state.time_verified=true;
  const int64_t now=Kst("2026-11-13 00:10");
  for (int i=0;i<47;i++) TEST_ASSERT_FALSE(store.Checkpoint(flash,state,snap,now+i*300));
  flash.failing=false;
  TEST_ASSERT_TRUE(store.Checkpoint(flash,state,snap,now+47*300));
  RuntimeStore restored; app::AppState back; app::Snapshot cached;
  TEST_ASSERT_TRUE(restored.Restore(flash,state.settings,back,cached));
  const int before=flash.write_count;
  back.time_verified=true;
  back.providers[1].auth_blocked=true;  // critical change also obeys the daily cap
  TEST_ASSERT_FALSE(restored.Checkpoint(flash,back,cached,now+48*300));
  TEST_ASSERT_EQUAL(before,flash.write_count);
}
