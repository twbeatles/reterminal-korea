// Fast (differential) refresh — ADR 0034.
// Register values follow GxEPD2 GDEY075T7 (_InitDisplay/_Init_Part with
// useFastPartialUpdateFromOTP); the oracle below is written from those values,
// not from the implementation.
#include <utility>

namespace {
class FastPort : public kdash::epd::PanelPort {
 public:
  uint32_t tick = 0;
  bool ready = true;
  uint32_t row_ms = 0;
  int resets = 0;
  bool streaming = false;
  int sessions = 0, ends = 0;
  uint8_t cmd = 0;
  std::vector<uint8_t> commands;
  std::vector<std::pair<uint8_t, std::vector<uint8_t>>> writes;  // non-plane payloads
  std::vector<uint8_t> plane[2];
  uint32_t now_ms() override { return tick; }
  void pause_ms(uint32_t ms) override { tick += ms; }
  void reset(bool) override { resets++; }
  bool idle() override { return ready; }
  void command(uint8_t c) override { cmd = c; commands.push_back(c); writes.push_back({c, {}}); }
  void begin_data() override { streaming = true; ++sessions; }
  void end_data() override { streaming = false; ++ends; }
  bool use_otp_lut() const override { return true; }
  void data(const uint8_t* p, size_t n) override {
    if (cmd == 0x10 || cmd == 0x13) {
      plane[cmd == 0x10 ? 0 : 1].insert(plane[cmd == 0x10 ? 0 : 1].end(), p, p + n);
      tick += row_ms;
      return;
    }
    writes.back().second.insert(writes.back().second.end(), p, p + n);
  }
  std::vector<uint8_t> payload(uint8_t c) const {
    for (const auto& w : writes) if (w.first == c) return w.second;
    return {};
  }
  long index(uint8_t c) const {
    for (size_t i = 0; i < commands.size(); ++i) if (commands[i] == c) return (long)i;
    return -1;
  }
};

void SetInk(std::vector<uint8_t>& fb, int x, int y, uint8_t ink) {
  uint8_t& b = fb[(size_t)y * 200 + x / 4];
  const int shift = 6 - 2 * (x % 4);
  b = (uint8_t)((b & ~(3u << shift)) | ((ink & 3u) << shift));
}
}  // namespace

void test_fast_frame_register_sequence_and_planes() {
  std::vector<uint8_t> oldfb(96000, 0), newfb(96000, 0);
  // new: x0 black(3), x1 light(1), x2 dark(2), x3 white
  SetInk(newfb, 0, 0, 3); SetInk(newfb, 1, 0, 1); SetInk(newfb, 2, 0, 2);
  // old: x1 black, x3 light gray antialias pixel
  SetInk(oldfb, 1, 0, 3); SetInk(oldfb, 3, 0, 1);
  FastPort p;
  TEST_ASSERT_TRUE(kdash::epd::PushFastFrame(p, oldfb.data(), false, newfb.data()));
  TEST_ASSERT_EQUAL(2, p.resets);
  TEST_ASSERT_EQUAL(48000, (int)p.plane[0].size());
  TEST_ASSERT_EQUAL(48000, (int)p.plane[1].size());
  // GxEPD2 polarity with CDI 0x29 (DDX=01): bit 1 = white, 0 = black.
  TEST_ASSERT_EQUAL_HEX8(0xBF, p.plane[0][0]);  // old: only x1 black (light x3 -> white)
  TEST_ASSERT_EQUAL_HEX8(0x5F, p.plane[1][0]);  // new: x0,x2 black; x1 light -> white
  TEST_ASSERT_EQUAL_HEX8(0xFF, p.plane[1][1]);  // untouched area stays white
  const std::vector<uint8_t> panel{0x1F}, cdi{0x29, 0x07}, mode{0x02}, temp{0x6E};
  const std::vector<uint8_t> res{0x03, 0x20, 0x01, 0xE0};
  TEST_ASSERT_TRUE(p.payload(0x00) == panel);
  TEST_ASSERT_TRUE(p.payload(0x50) == cdi);
  TEST_ASSERT_TRUE(p.payload(0xE0) == mode);
  TEST_ASSERT_TRUE(p.payload(0xE5) == temp);  // OTP fast-partial temperature override
  TEST_ASSERT_TRUE(p.payload(0x61) == res);
  TEST_ASSERT_EQUAL(-1, p.index(0x20));       // no register LUT upload
  TEST_ASSERT_TRUE(p.index(0xE5) < p.index(0x04));
  TEST_ASSERT_TRUE(p.index(0x04) < p.index(0x10));
  TEST_ASSERT_TRUE(p.index(0x10) < p.index(0x13));
  TEST_ASSERT_TRUE(p.index(0x13) < p.index(0x12));
  TEST_ASSERT_TRUE(p.index(0x12) < p.index(0x02));
  TEST_ASSERT_EQUAL_HEX8(0x07, p.commands.back());  // power off + deep sleep, as full frames
  TEST_ASSERT_EQUAL(2, p.sessions);
  TEST_ASSERT_EQUAL(p.sessions, p.ends);
  TEST_ASSERT_FALSE(p.streaming);

  // After a 4-gray full refresh, any ink in the old frame counts as black so the
  // differential waveform drives faint antialias pixels back to white.
  FastPort g;
  TEST_ASSERT_TRUE(kdash::epd::PushFastFrame(g, oldfb.data(), true, newfb.data()));
  TEST_ASSERT_EQUAL_HEX8(0xAF, g.plane[0][0]);
  TEST_ASSERT_EQUAL_HEX8(0x5F, g.plane[1][0]);
}

void test_fast_frame_rejects_bad_input_and_timeout() {
  std::vector<uint8_t> fb(96000, 0);
  FastPort p;
  TEST_ASSERT_FALSE(kdash::epd::PushFastFrame(p, nullptr, false, fb.data()));
  TEST_ASSERT_FALSE(kdash::epd::PushFastFrame(p, fb.data(), false, nullptr));
  TEST_ASSERT_FALSE(kdash::epd::PushFastFrame(p, fb.data(), false, fb.data(), 0));
  TEST_ASSERT_TRUE(p.commands.empty());
  FastPort busy;
  busy.ready = false;
  TEST_ASSERT_FALSE(kdash::epd::PushFastFrame(busy, fb.data(), false, fb.data(), 100));
  TEST_ASSERT_TRUE(busy.tick <= 110);
  TEST_ASSERT_TRUE(busy.plane[0].empty());
  FastPort slow;
  slow.row_ms = 10;
  TEST_ASSERT_FALSE(kdash::epd::PushFastFrame(slow, fb.data(), false, fb.data(), 250));
  TEST_ASSERT_EQUAL(slow.sessions, slow.ends);
  TEST_ASSERT_FALSE(slow.streaming);  // deadline path must release CS
  TEST_ASSERT_EQUAL(-1, slow.index(0x12));  // never refresh a half-written frame
}

void test_fast_refresh_caps_and_cycle() {
  // Every gate must hold: user opt-in, OTP waveform panel, previous frame in RAM.
  for (int m = 0; m < 7; ++m) {
    const ui::RefreshCaps c = ui::FastRefreshCaps(m & 1, m & 2, m & 4);
    TEST_ASSERT_FALSE(c.partial_supported);
    TEST_ASSERT_FALSE(c.gray_mode);
  }
  TEST_ASSERT_TRUE(ui::FastRefreshCaps(true, true, true).partial_supported);

  // Simulated USB-awake session: first frame has no previous frame -> Full,
  // then five fast frames, then a 4-gray Full that clears accumulated ghosting.
  int n = 0;
  bool has_prev = false;
  std::vector<ui::RefreshDecision> seq;
  for (int i = 0; i < 13; ++i) {
    const ui::RefreshDecision d =
        ui::DecideRefresh(true, true, n, ui::FastRefreshCaps(true, true, has_prev));
    seq.push_back(d);
    n = ui::NoteRefreshDone(d, n);
    has_prev = true;
  }
  const ui::RefreshDecision F = ui::RefreshDecision::Full, P = ui::RefreshDecision::Partial;
  const ui::RefreshDecision want[] = {F, P, P, P, P, P, F, P, P, P, P, P, F};
  for (int i = 0; i < 13; ++i) TEST_ASSERT_EQUAL_MESSAGE((int)want[i], (int)seq[i], "cycle");

  // Battery deep sleep loses PSRAM: never a fast frame without the previous image.
  TEST_ASSERT_EQUAL((int)F, (int)ui::DecideRefresh(true, true, 0, ui::FastRefreshCaps(true, true, false)));
  // Setting off keeps the current always-full behaviour.
  TEST_ASSERT_EQUAL((int)F, (int)ui::DecideRefresh(true, true, 0, ui::FastRefreshCaps(false, true, true)));
}

void test_settings_v5_fast_refresh_roundtrip_and_migration() {
  Settings s = ConfiguredSettings(), out;
  TEST_ASSERT_FALSE(s.device.fast_refresh);  // default off until hardware-verified
  TEST_ASSERT_EQUAL(5, kSettingsSchemaVersion);
  s.device.fast_refresh = true;
  s.device.charge_protect = true;
  uint8_t data[2048];
  const size_t n = settings_codec::Encode(s, data, sizeof(data));
  TEST_ASSERT_TRUE(n > 8);
  TEST_ASSERT_TRUE(settings_codec::Decode(data, n, out));
  TEST_ASSERT_TRUE(out.device.fast_refresh);
  TEST_ASSERT_TRUE(out.device.charge_protect);
  // Device-only flag: cache identity (DATA fingerprint) must not change.
  const uint32_t fp = settings_codec::DataFingerprint(s);
  s.device.fast_refresh = false;
  TEST_ASSERT_EQUAL_UINT32(fp, settings_codec::DataFingerprint(s));
  // v4 bytes (no trailing flag) migrate with fast refresh OFF, keeping protection.
  data[0] = 4;
  TEST_ASSERT_TRUE(settings_codec::Migrate(4, data, n - 1, out));
  TEST_ASSERT_FALSE(out.device.fast_refresh);
  TEST_ASSERT_TRUE(out.device.charge_protect);
  TEST_ASSERT_EQUAL_UINT32(fp, settings_codec::DataFingerprint(out));
  // Unknown value is rejected, never guessed.
  data[0] = 5; data[n - 1] = 2;
  TEST_ASSERT_FALSE(settings_codec::Decode(data, n, out));
}

void test_fast_refresh_backup_roundtrip() {
  Settings s = ConfiguredSettings(), back;
  s.device.fast_refresh = true;
  char json[1500], bad[32];
  TEST_ASSERT_TRUE(portal::ExportBackup(s, json, sizeof(json)) > 0);
  TEST_ASSERT_NOT_NULL(strstr(json, "\"fast_refresh\":true"));
  TEST_ASSERT_EQUAL((int)portal::ImportResult::Ok, (int)portal::ImportBackup(json, back, bad, sizeof(bad)));
  TEST_ASSERT_TRUE(back.device.fast_refresh);
  TEST_ASSERT_EQUAL((int)portal::ImportResult::BadValue,
      (int)portal::ImportBackup("{\"format\":\"kdash-backup\",\"version\":1,\"device\":{\"fast_refresh\":3}}",
                                back, bad, sizeof(bad)));
}
