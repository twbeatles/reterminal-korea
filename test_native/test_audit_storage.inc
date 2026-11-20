// Included by test_core.cpp after test_stage2.cpp.
#include "../src/storage/RuntimeStore.h"

void test_slotstore_recovers_before_next_write(void) {
  FakeStorage st;
  const uint8_t first[] = {1}, second[] = {2}, third[] = {3};
  uint32_t gen = 0;
  TEST_ASSERT_TRUE(kdash::slot::Write(st, "set", 1, first, 1, 10, 10, gen));
  st.fail_on_write = st.write_count + 2;
  st.fail_bytes = 0;
  st.fail_return = true;
  TEST_ASSERT_FALSE(kdash::slot::Write(st, "set", 1, second, 1, 20, 20, gen));
  uint8_t out[8]; size_t len = 0;
  TEST_ASSERT_TRUE(kdash::slot::Read(st, "set", 1, out, sizeof(out), len));
  TEST_ASSERT_EQUAL(2, out[0]);
  // A second failure must damage the older slot, never recovered newest.
  st.fail_on_write = st.write_count + 1;
  st.fail_bytes = 3;
  TEST_ASSERT_FALSE(kdash::slot::Write(st, "set", 1, third, 1, 30, 30, gen));
  TEST_ASSERT_TRUE(kdash::slot::Read(st, "set", 1, out, sizeof(out), len));
  TEST_ASSERT_EQUAL(2, out[0]);
  st.fail_on_write = -1;
  TEST_ASSERT_TRUE(kdash::slot::Write(st, "set", 1, third, 1, 30, 30, gen));
  TEST_ASSERT_EQUAL(3, gen);
  TEST_ASSERT_TRUE(kdash::slot::Read(st, "set", 1, out, sizeof(out), len));
  TEST_ASSERT_EQUAL(3, out[0]);
}

void test_slotstore_torn_writes_repeated(void) {
  FakeStorage st;
  uint8_t good[64]; memset(good, 1, sizeof(good));
  uint32_t gen = 0;
  TEST_ASSERT_TRUE(kdash::slot::Write(st, "snap", 2, good, sizeof(good), 1, 1, gen));
  for (int i = 0; i < 100; i++) {
    uint8_t candidate[64]; memset(candidate, i + 2, sizeof(candidate));
    st.fail_on_write = st.write_count + (i % 2 ? 2 : 1);
    st.fail_bytes = (size_t)(i % 30);
    st.fail_return = true;
    bool ok = kdash::slot::Write(st, "snap", 2, candidate, sizeof(candidate), i, i, gen);
    uint8_t out[64]; size_t len = 0;
    TEST_ASSERT_TRUE(kdash::slot::Read(st, "snap", 2, out, sizeof(out), len));
    TEST_ASSERT_EQUAL(sizeof(out), len);
    // Atomic slot commit may succeed before the hint fails.
    bool candidate_committed = memcmp(out, candidate, sizeof(out)) == 0;
    TEST_ASSERT_TRUE(candidate_committed || memcmp(out, good, sizeof(out)) == 0);
    if (candidate_committed) memcpy(good, candidate, sizeof(good));
    TEST_ASSERT_FALSE(ok);  // All injected writes return failure.
    st.fail_on_write = -1;
  }
}

void test_settings_semantic_decode_atomic(void) {
  kdash::Settings s = FullSettings();
  uint8_t payload[2048];
  kdash::Settings prior = FullSettings();
  kdash::Settings out = prior;
  s.watchlist[0].market = kdash::Market::Unknown;
  size_t len = kdash::settings_codec::Encode(s, payload, sizeof(payload));
  TEST_ASSERT_TRUE(len > 0);
  TEST_ASSERT_FALSE(kdash::settings_codec::Decode(payload, len, out));
  TEST_ASSERT_EQUAL_STRING(prior.wifi_ssid, out.wifi_ssid);
  TEST_ASSERT_EQUAL((int)prior.watchlist[0].market, (int)out.watchlist[0].market);
  s = prior;
  kdash::settings::CopyBounded(s.providers.kma_key, sizeof(s.providers.kma_key), "bad key");
  len = kdash::settings_codec::Encode(s, payload, sizeof(payload));
  TEST_ASSERT_FALSE(kdash::settings_codec::Decode(payload, len, out));
  TEST_ASSERT_EQUAL_STRING(prior.providers.kma_key, out.providers.kma_key);
}

void test_runtime_checkpoint_restore_and_config_binding(void) {
  FakeStorage disk;
  kdash::app::AppState state;
  state.settings = FullSettings();
  state.time_verified = true;
  kdash::app::Snapshot snap;
  kdash::demo::FillDemo(snap, 1790000000);
  state.providers[0].auth_blocked = true;
  state.providers[1].retry_after_utc = 1790080000;
  kdash::RuntimeStore checkpoint;
  TEST_ASSERT_TRUE(checkpoint.Checkpoint(disk, state, snap, 1790000000));
  int writes = disk.write_count;
  TEST_ASSERT_FALSE(checkpoint.Checkpoint(disk, state, snap, 1790000010));
  TEST_ASSERT_EQUAL(writes, disk.write_count);
  kdash::RuntimeStore boot;
  kdash::app::AppState restored;
  restored.settings = state.settings;
  kdash::app::Snapshot values;
  TEST_ASSERT_TRUE(boot.Restore(disk, restored.settings, restored, values));
  TEST_ASSERT_TRUE(values.has_now);
  TEST_ASSERT_TRUE(restored.providers[0].auth_blocked);
  TEST_ASSERT_EQUAL(1790080000, restored.providers[1].retry_after_utc);
  TEST_ASSERT_FALSE(boot.Checkpoint(disk, restored, values, 1790000010));
  kdash::settings::CopyBounded(restored.settings.providers.kma_key,
      sizeof(restored.settings.providers.kma_key), "changed-key");
  TEST_ASSERT_FALSE(boot.Restore(disk, restored.settings, restored, values));
}


void test_runtime_budget_survives_reboot_and_clock_rollback(void) {
  FakeStorage disk;
  kdash::app::AppState state;
  state.settings = FullSettings(); state.time_verified = true;
  kdash::app::Snapshot snap;
  kdash::RuntimeStore store;
  const int64_t now = 1790000000;
  for (int i = 0; i < 48; i++) {
    state.providers[0].auth_blocked = (i % 2) != 0;
    TEST_ASSERT_TRUE(store.Checkpoint(disk, state, snap, now));
  }
  kdash::RuntimeStore boot;
  TEST_ASSERT_TRUE(boot.Restore(disk, state.settings, state, snap));
  state.providers[0].auth_blocked = !state.providers[0].auth_blocked;
  int before = disk.write_count;
  TEST_ASSERT_FALSE(boot.Checkpoint(disk, state, snap, now - 86400));
  TEST_ASSERT_EQUAL(before, disk.write_count);
  TEST_ASSERT_TRUE(boot.Checkpoint(disk, state, snap, now + 86400));
}
