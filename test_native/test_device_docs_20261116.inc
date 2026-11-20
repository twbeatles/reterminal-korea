// Regressions for docs/21_DEVICE_DOCUMENT_AUDIT_2026-11-16.md (ADR 0032).

// Good Display GDEY075T7 product page: after five consecutive fast/partial
// refreshes, run one full refresh to limit ghosting.
void test_partial_refresh_forces_full_after_five() {
  ui::RefreshCaps caps;
  caps.partial_supported = true;
  TEST_ASSERT_EQUAL(5, ui::kMaxPartialsBeforeFull);
  TEST_ASSERT_EQUAL((int)ui::RefreshDecision::Partial, (int)ui::DecideRefresh(true, true, 4, caps));
  TEST_ASSERT_EQUAL((int)ui::RefreshDecision::Full, (int)ui::DecideRefresh(true, true, 5, caps));
  int n = 0;
  for (int i = 0; i < 5; ++i) n = ui::NoteRefreshDone(ui::DecideRefresh(true, true, n, caps), n);
  TEST_ASSERT_EQUAL(5, n);
  TEST_ASSERT_EQUAL((int)ui::RefreshDecision::Full, (int)ui::DecideRefresh(true, true, n, caps));
  TEST_ASSERT_EQUAL(0, ui::NoteRefreshDone(ui::RefreshDecision::Full, n));
}

// Seeed E1001 wiki: device working temperature 0-40 C (panel itself 0-50 C).
void test_panel_temperature_operating_range() {
  TEST_ASSERT_TRUE(ui::PanelTempInRange(0.0f));
  TEST_ASSERT_TRUE(ui::PanelTempInRange(22.5f));
  TEST_ASSERT_TRUE(ui::PanelTempInRange(40.0f));
  TEST_ASSERT_FALSE(ui::PanelTempInRange(-0.1f));
  TEST_ASSERT_FALSE(ui::PanelTempInRange(40.1f));
  const float nan = 0.0f / 0.0f;
  TEST_ASSERT_TRUE(ui::PanelTempInRange(nan));  // no valid reading: never warn
}
