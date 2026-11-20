// Stage-2 tests (T02-T12): included from test_core.cpp (no main here).
#include <string.h>

#include <map>
#include <string>
#include <vector>

namespace {
// In-memory IStorage with torn-write injection (power-cut simulation).
class FakeStorage : public kdash::IStorage {
 public:
  std::map<std::string, std::vector<uint8_t>> kv;
  int write_count = 0;
  int fail_on_write = -1;   // 1-based index of the write to truncate
  size_t fail_bytes = 0;    // bytes actually stored on the failing write
  bool fail_return = false;  // failing write returns false (no meta commit)

  bool read(const char* key, uint8_t* out, size_t out_len,
            size_t& out_actual) override {
    auto it = kv.find(key ? key : "");
    if (it == kv.end()) return false;
    if (it->second.size() > out_len) return false;
    memcpy(out, it->second.data(), it->second.size());
    out_actual = it->second.size();
    return true;
  }
  bool write_atomic(const char* key, const uint8_t* data, size_t len) override {
    write_count++;
    if (write_count == fail_on_write) {
      size_t n = fail_bytes < len ? fail_bytes : len;
      kv[key ? key : ""] =
          std::vector<uint8_t>(data, data ? data + n : nullptr);
      return !fail_return;
    }
    kv[key ? key : ""] =
        std::vector<uint8_t>(data, data ? data + len : nullptr);
    return true;
  }
  bool erase(const char* key) override {
    kv.erase(key ? key : "");
    return true;
  }
};

kdash::Settings FullSettings() {
  kdash::Settings s = kdash::settings::DefaultSettings();
  kdash::settings::CopyBounded(s.location.display_name,
                               sizeof(s.location.display_name), "서울 종로구 청운동");
  kdash::settings::CopyBounded(s.location.air_station,
                               sizeof(s.location.air_station), "종로구");
  s.location.grid_x = 60;
  s.location.grid_y = 127;
  kdash::settings::CopyBounded(s.wifi_ssid, sizeof(s.wifi_ssid), "TestAP");
  kdash::settings::CopyBounded(s.wifi_secret, sizeof(s.wifi_secret), "password123");
  char big[257];
  memset(big, 'k', sizeof(big) - 1);
  big[sizeof(big) - 1] = '\0';
  kdash::settings::CopyBounded(s.providers.kma_key,
                               sizeof(s.providers.kma_key), big);
  kdash::settings::CopyBounded(s.providers.air_key,
                               sizeof(s.providers.air_key), "air-key");
  kdash::settings::CopyBounded(s.providers.exim_key,
                               sizeof(s.providers.exim_key), "exim-key");
  kdash::settings::CopyBounded(s.providers.kis_app_key,
                               sizeof(s.providers.kis_app_key), "kis-key");
  kdash::settings::CopyBounded(s.providers.kis_app_secret,
                               sizeof(s.providers.kis_app_secret), "kis-secret");
  s.watch_count = 1;
  kdash::settings::CopyBounded(s.watchlist[0].symbol,
                               sizeof(s.watchlist[0].symbol), "005930");
  s.watchlist[0].market = kdash::Market::Krx;
  return s;
}
}  // namespace

void test_codec_roundtrip(void) {
  uint8_t buf[64];
  kdash::codec::Writer w(buf, sizeof(buf));
  TEST_ASSERT_TRUE(w.u8(0xAB));
  TEST_ASSERT_TRUE(w.u16(0x1234));
  TEST_ASSERT_TRUE(w.u32(0xDEADBEEFu));
  TEST_ASSERT_TRUE(w.i32(-5));
  TEST_ASSERT_TRUE(w.i64(-1234567890123LL));
  TEST_ASSERT_TRUE(w.str("한글", 64));
  TEST_ASSERT_TRUE(w.ok());
  size_t n = w.pos();
  kdash::codec::Reader r(buf, n);
  uint8_t a = 0;
  uint16_t b = 0;
  uint32_t c = 0;
  int32_t d = 0;
  int64_t e = 0;
  char s[65] = {0};
  TEST_ASSERT_TRUE(r.u8(a) && a == 0xAB);
  TEST_ASSERT_TRUE(r.u16(b) && b == 0x1234);
  TEST_ASSERT_TRUE(r.u32(c) && c == 0xDEADBEEFu);
  TEST_ASSERT_TRUE(r.i32(d) && d == -5);
  TEST_ASSERT_TRUE(r.i64(e) && e == -1234567890123LL);
  TEST_ASSERT_TRUE(r.str(s, sizeof(s), 64) && strcmp(s, "한글") == 0);
  TEST_ASSERT_TRUE(r.done());
  // Truncated input rejected.
  kdash::codec::Reader r2(buf, n - 1);
  uint8_t x = 0;
  uint16_t y = 0;
  uint32_t z = 0;
  int32_t q = 0;
  int64_t t = 0;
  char s2[65] = {0};
  r2.u8(x);
  r2.u16(y);
  r2.u32(z);
  r2.i32(q);
  r2.i64(t);
  TEST_ASSERT_FALSE(r2.str(s2, sizeof(s2), 64));
  // Oversize string rejected.
  uint8_t small[4];
  kdash::codec::Writer w2(small, sizeof(small));
  TEST_ASSERT_FALSE(w2.str("toolongstring", 4));
}

void test_settings_codec(void) {
  kdash::Settings s = FullSettings();
  uint8_t payload[2048];
  size_t n = kdash::settings_codec::Encode(s, payload, sizeof(payload));
  TEST_ASSERT_TRUE(n > 0);
  kdash::Settings back;
  TEST_ASSERT_TRUE(kdash::settings_codec::Decode(payload, n, back));
  uint8_t payload2[2048];
  size_t n2 = kdash::settings_codec::Encode(back, payload2, sizeof(payload2));
  TEST_ASSERT_EQUAL(n, n2);
  TEST_ASSERT_EQUAL(0, memcmp(payload, payload2, n));
  // Unknown schema rejected.
  uint8_t bad[64] = {99, 0};
  kdash::Settings dflt;
  TEST_ASSERT_FALSE(kdash::settings_codec::Decode(bad, sizeof(bad), dflt));
  // Truncated blob rejected.
  TEST_ASSERT_FALSE(kdash::settings_codec::Decode(payload, n - 1, dflt));
  // Migration of unknown version => false + defaults.
  kdash::Settings m;
  TEST_ASSERT_FALSE(kdash::settings_codec::Migrate(99, payload, n, m));
}

void test_slotstore_powercut(void) {
  FakeStorage st;
  uint8_t payload[64];
  memset(payload, 0xA5, sizeof(payload));
  uint32_t g1 = 0, g2 = 0;
  TEST_ASSERT_TRUE(kdash::slot::Write(st, "set", kdash::slot::kKindSettings,
                                     payload, sizeof(payload), 100, 200, g1));
  // Torn write of slot B at 50%: previous A must survive.
  st.fail_on_write = st.write_count + 1;
  st.fail_bytes = 10;
  st.fail_return = false;
  uint8_t payload_b[64];
  memset(payload_b, 0x5A, sizeof(payload_b));
  uint32_t gb = 0;
  kdash::slot::Write(st, "set", kdash::slot::kKindSettings, payload_b,
                     sizeof(payload_b), 100, 300, gb);
  st.fail_on_write = -1;
  uint8_t out[128];
  size_t out_len = 0;
  TEST_ASSERT_TRUE(kdash::slot::Read(st, "set", kdash::slot::kKindSettings,
                                    out, sizeof(out), out_len));
  TEST_ASSERT_EQUAL(sizeof(payload), out_len);
  TEST_ASSERT_EQUAL(0, memcmp(out, payload, out_len));
  // Healthy second write, then meta torn before commit: gen compare saves B.
  TEST_ASSERT_TRUE(kdash::slot::Write(st, "set", kdash::slot::kKindSettings,
                                     payload_b, sizeof(payload_b), 100, 400, g2));
  TEST_ASSERT_TRUE(g2 > g1);
  st.fail_on_write = st.write_count + 2;  // meta write torn
  st.fail_bytes = 0;
  st.fail_return = false;
  uint8_t payload_c[64];
  memset(payload_c, 0x33, sizeof(payload_c));
  uint32_t gc = 0;
  kdash::slot::Write(st, "set", kdash::slot::kKindSettings, payload_c,
                     sizeof(payload_c), 100, 500, gc);
  st.fail_on_write = -1;
  TEST_ASSERT_TRUE(kdash::slot::Read(st, "set", kdash::slot::kKindSettings,
                                    out, sizeof(out), out_len));
  TEST_ASSERT_EQUAL(0, memcmp(out, payload_c, out_len));
  // Bit flip in the active slot => other slot used.
  auto it = st.kv.find("set.b");
  TEST_ASSERT_TRUE(it != st.kv.end());
  if (it != st.kv.end() && !it->second.empty()) it->second.back() ^= 0x01;
  TEST_ASSERT_TRUE(kdash::slot::Read(st, "set", kdash::slot::kKindSettings,
                                    out, sizeof(out), out_len));
  // Both corrupt => false (caller uses defaults + portal flag).
  for (auto& kv : st.kv) {
    if (!kv.second.empty()) kv.second[0] ^= 0xFF;
  }
  TEST_ASSERT_FALSE(kdash::slot::Read(st, "set", kdash::slot::kKindSettings,
                                     out, sizeof(out), out_len));
  // Frequency gate: 6 requests in 10-min gaps => <= 1 real write.
  TEST_ASSERT_TRUE(
      kdash::slot::SnapshotWriteAllowed(true, 1000, 0, 0));
  TEST_ASSERT_FALSE(
      kdash::slot::SnapshotWriteAllowed(false, 2000, 1000, 0));
  TEST_ASSERT_FALSE(
      kdash::slot::SnapshotWriteAllowed(true, 1500, 1000, 0));  // <30min
  TEST_ASSERT_FALSE(
      kdash::slot::SnapshotWriteAllowed(true, 100000, 0, 48));  // budget out
}

void test_snapshot_codec(void) {
  kdash::app::Snapshot snap;
  kdash::demo::FillDemo(snap, 1700000000LL);
  uint8_t buf[3072];
  size_t n = kdash::snapshot_codec::EncodeSnapshot(snap, buf, sizeof(buf));
  TEST_ASSERT_TRUE(n > 0);
  kdash::app::Snapshot back;
  TEST_ASSERT_TRUE(kdash::snapshot_codec::DecodeSnapshot(buf, n, back));
  TEST_ASSERT_TRUE(back.has_now && back.has_fc && back.has_air && back.has_fx);
  TEST_ASSERT_EQUAL_STRING("기상청(DEMO)", back.now.temp_c.source);
  TEST_ASSERT_EQUAL_STRING("에어코리아(DEMO)", back.air.pm10.source);
  TEST_ASSERT_EQUAL_STRING("한국수출입은행(DEMO)", back.fx.usd.source);
  TEST_ASSERT_TRUE(strcmp(back.fx.usd.value.currency_unit, "USD") == 0);
  TEST_ASSERT_TRUE(strcmp(back.fx.jpy100.value.currency_unit, "JPY(100)") == 0);
  // Status roundtrip.
  kdash::app::ProviderStatus st[7];
  for (int i = 0; i < 7; i++) {
    st[i].last_success_utc = 1000 + i;
    st[i].attempts = i;
    st[i].auth_blocked = (i == 5);
  }
  uint8_t sb[512];
  size_t sn = kdash::snapshot_codec::EncodeStatus(st, 7, sb, sizeof(sb));
  TEST_ASSERT_TRUE(sn > 0);
  kdash::app::ProviderStatus back2[7];
  TEST_ASSERT_TRUE(
      kdash::snapshot_codec::DecodeStatus(sb, sn, back2, 7));
  TEST_ASSERT_EQUAL(1005, (int)back2[5].last_success_utc);
  TEST_ASSERT_TRUE(back2[5].auth_blocked);
  // Truncated snapshot rejected.
  kdash::app::Snapshot bad;
  TEST_ASSERT_FALSE(kdash::snapshot_codec::DecodeSnapshot(buf, n - 1, bad));
}

void test_clock_policy(void) {
  using kdash::clock::Decide;
  const int64_t kBuild = 1767225600LL;
  auto d1 = Decide(true, false, 1790000000LL, true, 1790000003LL, kBuild);
  TEST_ASSERT_TRUE(d1.verified);
  TEST_ASSERT_EQUAL((int)kdash::clock::Source::Ntp, (int)d1.source);
  TEST_ASSERT_EQUAL((int)kdash::clock::Action::WriteRtc, (int)d1.action);
  auto d2 = Decide(true, false, 1790000000LL, false, 0, kBuild);
  TEST_ASSERT_TRUE(d2.verified);
  TEST_ASSERT_EQUAL((int)kdash::clock::Source::Rtc, (int)d2.source);
  auto d3 = Decide(true, true, 1790000000LL, false, 0, kBuild);
  TEST_ASSERT_FALSE(d3.verified);
  auto d4 = Decide(true, false, 1000000000LL, false, 0, kBuild);
  TEST_ASSERT_FALSE(d4.verified);  // RTC older than build
  // SystemClock glue: unverified => now_utc false.
  kdash::SystemClock sc(kBuild);
  int64_t e = 0;
  TEST_ASSERT_FALSE(sc.now_utc(e));
  sc.NoteRtc(true, false, 1790000000LL);
  TEST_ASSERT_TRUE(sc.is_verified());
  TEST_ASSERT_TRUE(sc.now_utc(e));
  TEST_ASSERT_EQUAL(1790000000LL, (long long)e);
}

void test_pcf8563_bcd(void) {
  int v = 0;
  TEST_ASSERT_TRUE(kdash::pcf::DecBcd(0x59, v) && v == 59);
  TEST_ASSERT_FALSE(kdash::pcf::DecBcd(0x5A, v));
  uint8_t b = 0;
  TEST_ASSERT_TRUE(kdash::pcf::EncBcd(45, b) && b == 0x45);
  kdash::pcf::Regs r;
  TEST_ASSERT_TRUE(
      kdash::pcf::EncodeRegs(2026, 11, 2, 14, 30, 0, 1, r));
  bool vl = false;
  int yy = 0, mo = 0, dd = 0, hh = 0, mi = 0, ss = 0, wd = 0;
  TEST_ASSERT_TRUE(
      kdash::pcf::DecodeRegs(r, vl, yy, mo, dd, hh, mi, ss, wd));
  TEST_ASSERT_FALSE(vl);
  TEST_ASSERT_EQUAL(2026, yy);
  TEST_ASSERT_EQUAL(11, mo);
  TEST_ASSERT_EQUAL(2, dd);
  r.sec |= 0x80;
  TEST_ASSERT_TRUE(
      kdash::pcf::DecodeRegs(r, vl, yy, mo, dd, hh, mi, ss, wd));
  TEST_ASSERT_TRUE(vl);
  // Century bit (official RTC example): C=0 -> 2000s, C=1 -> 1900s.
  kdash::pcf::Regs rc;
  TEST_ASSERT_TRUE(kdash::pcf::EncodeRegs(2026, 11, 2, 14, 30, 0, 1, rc));
  TEST_ASSERT_EQUAL(0, rc.month & 0x80);
  TEST_ASSERT_TRUE(
      kdash::pcf::DecodeRegs(rc, vl, yy, mo, dd, hh, mi, ss, wd));
  TEST_ASSERT_EQUAL(2026, yy);
  rc.month |= 0x80;
  TEST_ASSERT_TRUE(
      kdash::pcf::DecodeRegs(rc, vl, yy, mo, dd, hh, mi, ss, wd));
  TEST_ASSERT_EQUAL(1926, yy);  // C=1 -> 1900s, never 2100
  kdash::pcf::Regs ry;
  TEST_ASSERT_TRUE(kdash::pcf::EncodeRegs(2099, 12, 31, 23, 59, 59, 3, ry));
  ry.month |= 0x80;
  ry.year = 0x99;
  TEST_ASSERT_TRUE(
      kdash::pcf::DecodeRegs(ry, vl, yy, mo, dd, hh, mi, ss, wd));
  TEST_ASSERT_EQUAL(1999, yy);
}

void test_buttons(void) {
  using kdash::ButtonDecoder;
  using kdash::ButtonEvent;
  using kdash::ButtonSample;
  ButtonDecoder d;
  // Chatter below debounce => None.
  TEST_ASSERT_EQUAL((int)ButtonEvent::None,
                    (int)d.Feed({0, 0x01}));
  TEST_ASSERT_EQUAL((int)ButtonEvent::None,
                    (int)d.Feed({30, 0x00}));
  d.Reset();
  // Short press => event on release.
  TEST_ASSERT_EQUAL((int)ButtonEvent::None, (int)d.Feed({0, 0x01}));
  TEST_ASSERT_EQUAL((int)ButtonEvent::None, (int)d.Feed({120, 0x01}));
  TEST_ASSERT_EQUAL((int)ButtonEvent::ShortKey0, (int)d.Feed({200, 0x00}));
  d.Reset();
  // Long press fires once at 2000ms, release gives None.
  d.Feed({0, 0x01});
  ButtonEvent got = ButtonEvent::None;
  for (uint32_t t = 100; t <= 2100; t += 100) {
    ButtonEvent e = d.Feed({t, 0x01});
    if (e != ButtonEvent::None) {
      TEST_ASSERT_EQUAL((int)ButtonEvent::LongKey0, (int)e);
      TEST_ASSERT_EQUAL((int)ButtonEvent::None, (int)got);
      got = e;
    }
  }
  TEST_ASSERT_EQUAL((int)ButtonEvent::LongKey0, (int)got);
  TEST_ASSERT_EQUAL((int)ButtonEvent::None, (int)d.Feed({2200, 0x01}));
  TEST_ASSERT_EQUAL((int)ButtonEvent::None, (int)d.Feed({2300, 0x00}));
  d.Reset();
  // Two buttons: first only.
  d.Feed({0, 0x02});
  d.Feed({100, 0x06});
  TEST_ASSERT_EQUAL((int)ButtonEvent::ShortKey1, (int)d.Feed({300, 0x00}));
  // Wake helpers.
  TEST_ASSERT_EQUAL((int)ButtonEvent::ShortKey2,
                    (int)kdash::WakeButtons(1u << 5, 0x00, false));
  TEST_ASSERT_EQUAL((int)ButtonEvent::LongKey0,
                    (int)kdash::WakeButtons(1u << 3, 0x01, true));
}

void test_sht4x(void) {
  // Build a valid frame then check decode.
  uint8_t raw[6] = {0x80, 0x00, 0, 0x80, 0x00, 0};
  raw[2] = kdash::sht::Crc8(raw, 2);
  raw[5] = kdash::sht::Crc8(raw + 3, 2);
  float t = 0, rh = 0;
  TEST_ASSERT_TRUE(kdash::sht::Decode(raw, t, rh));
  TEST_ASSERT_TRUE(t > 40.0f && t < 45.0f);
  // Corrupt CRC rejected.
  uint8_t bad[6];
  memcpy(bad, raw, 6);
  bad[2] ^= 0x01;
  TEST_ASSERT_FALSE(kdash::sht::Decode(bad, t, rh));
  // raw 0 => -45C => out of sensor range.
  uint8_t z[6] = {0, 0, 0, 0, 0, 0};
  z[2] = kdash::sht::Crc8(z, 2);
  z[5] = kdash::sht::Crc8(z + 3, 2);
  TEST_ASSERT_TRUE(kdash::sht::Decode(z, t, rh));
  TEST_ASSERT_EQUAL((int)kdash::drivers::SensorCheck::OutOfRange,
                    (int)kdash::drivers::CheckSensorRange(t, rh));
  // RH clamps to 0..100 (raw 0 => -6 => 0).
  TEST_ASSERT_TRUE(rh >= 0 && rh <= 100);
}

void test_battery(void) {
  using kdash::battery::Classify;
  using kdash::battery::State;
  TEST_ASSERT_EQUAL((int)State::Normal, (int)Classify(3800));
  TEST_ASSERT_EQUAL((int)State::Low, (int)Classify(3400));
  TEST_ASSERT_EQUAL((int)State::Critical, (int)Classify(3200));
  TEST_ASSERT_EQUAL((int)State::Unknown, (int)Classify(-1));
  // Official sampling contract: 10 samples, 25ms apart, plain mean.
  // Settle 10ms: Seeed E1001 wiki tip (ADR 0032), superset of the sketch's 5ms.
  TEST_ASSERT_EQUAL(10, kdash::battery::kAdcSamples);
  TEST_ASSERT_EQUAL(25, kdash::battery::kAdcIntervalMs);
  TEST_ASSERT_EQUAL(10, kdash::battery::kAdcSettleMs);
  int mv[10] = {3700, 3710, 3690, 3705, 3702, 3698, 3701, 3699, 3703, 3900};
  TEST_ASSERT_EQUAL(3720, kdash::battery::MeanOf(mv, 10));  // 37208/10
  TEST_ASSERT_EQUAL(-1, kdash::battery::MeanOf(nullptr, 10));
  TEST_ASSERT_EQUAL(-1, kdash::battery::MeanOf(mv, 0));
  // Official E1001 discharge curve (SenseCraft config @8d97d891).
  TEST_ASSERT_EQUAL(0, kdash::battery::PercentMv(2700));
  TEST_ASSERT_EQUAL(0, kdash::battery::PercentMv(2795));
  TEST_ASSERT_EQUAL(50, kdash::battery::PercentMv(3642));
  TEST_ASSERT_EQUAL(100, kdash::battery::PercentMv(4111));
  TEST_ASSERT_EQUAL(100, kdash::battery::PercentMv(4200));
  TEST_ASSERT_EQUAL(20, kdash::battery::PercentMv(3464));
  TEST_ASSERT_EQUAL(5, kdash::battery::PercentMv(3252));
  // Full-sweep parity: monotonic, 0..100 over the whole range.
  int prev = -1;
  for (int mv = 2500; mv <= 4200; mv++) {
    int p = kdash::battery::PercentMv(mv);
    TEST_ASSERT_TRUE(p >= 0 && p <= 100);
    TEST_ASSERT_TRUE(p >= prev);
    prev = p;
  }
  // Exact at official knots (spot-check across the curve).
  TEST_ASSERT_EQUAL(1, kdash::battery::PercentMv(2796));  // just above flat knot
  TEST_ASSERT_EQUAL(2, kdash::battery::PercentMv(2990));
  TEST_ASSERT_EQUAL(10, kdash::battery::PercentMv(3378));
  TEST_ASSERT_EQUAL(90, kdash::battery::PercentMv(3927));
  TEST_ASSERT_EQUAL(91, kdash::battery::PercentMv(3984));
}

void test_calendar(void) {
  using kdash::cal::DayKind;
  kdash::cal::YearTable t2026;
  kdash::cal::Clear(t2026, 2026);
  t2026.loaded = true;
  // Seollal 2026-02-17..19 + leap day sanity.
  kdash::cal::SetHoliday(t2026, 2, 17);
  kdash::cal::SetHoliday(t2026, 2, 18);
  kdash::cal::SetHoliday(t2026, 2, 19);
  kdash::cal::SetMarketClosed(t2026, 12, 31);
  int64_t feb17 = kdash::KstToUtc(2026, 2, 17, 12, 0);
  TEST_ASSERT_EQUAL((int)DayKind::PublicHoliday,
                    (int)kdash::cal::ClassifyKstDay(feb17, &t2026, 1));
  TEST_ASSERT_FALSE(kdash::cal::IsKrxOpen(feb17, &t2026, 1));
  int64_t dec31 = kdash::KstToUtc(2026, 12, 31, 12, 0);
  TEST_ASSERT_EQUAL((int)DayKind::MarketClosed,
                    (int)kdash::cal::ClassifyKstDay(dec31, &t2026, 1));
  TEST_ASSERT_FALSE(kdash::cal::IsKrxOpen(dec31, &t2026, 1));
  // No table year => Unknown on weekdays.
  int64_t mon = kdash::KstToUtc(2027, 11, 1, 12, 0);  // Monday
  TEST_ASSERT_EQUAL((int)DayKind::Unknown,
                    (int)kdash::cal::ClassifyKstDay(mon, &t2026, 1));
  // Leap day 2028-02-29 parses.
  TEST_ASSERT_TRUE(kdash::KstToUtc(2028, 2, 29, 12, 0) > 0);
  // Normal business day open during session.
  int64_t wed = kdash::KstToUtc(2026, 11, 4, 10, 0);
  TEST_ASSERT_TRUE(kdash::cal::IsKrxOpen(wed, &t2026, 1));
}

void test_viewmodel(void) {
  kdash::app::AppState st;
  st.settings = FullSettings();
  st.settings.providers.kis_enabled = true;
  st.settings.watch_count = 1;
  kdash::app::Snapshot snap;
  kdash::demo::FillDemo(snap, 1700000000LL);
  int64_t now = 1700003600LL;
  kdash::vm::LifePage life =
      kdash::vm::BuildLife(snap, st, now, true, true, false);
  TEST_ASSERT_EQUAL_STRING("21.5°C", life.temp.value);
  TEST_ASSERT_EQUAL((int)kdash::vm::Badge::None, (int)life.temp.badge);
  // Missing is "--", never "0".
  kdash::app::Snapshot miss = snap;
  miss.now.rain_mm.has_value = false;
  miss.now.rain_mm.validity = kdash::Validity::Missing;
  kdash::vm::LifePage life2 =
      kdash::vm::BuildLife(miss, st, now, true, true, false);
  TEST_ASSERT_EQUAL_STRING("--", life2.rain.value);
  // Stale badge + age in sub.
  kdash::app::Snapshot stale = snap;
  stale.now.temp_c.validity = kdash::Validity::Stale;
  kdash::vm::LifePage life3 =
      kdash::vm::BuildLife(stale, st, now + 7200, true, true, false);
  TEST_ASSERT_EQUAL((int)kdash::vm::Badge::Stale, (int)life3.temp.badge);
  TEST_ASSERT_TRUE(strstr(life3.temp.sub, "시간 전") != nullptr);
  // Unconfigured provider => NeedSetup.
  kdash::app::AppState st2 = st;
  st2.settings.providers.kma_key[0] = '\0';
  kdash::vm::LifePage life4 =
      kdash::vm::BuildLife(snap, st2, now, true, true, false);
  TEST_ASSERT_EQUAL((int)kdash::vm::Badge::NeedSetup, (int)life4.temp.badge);
  // Demo badge everywhere.
  kdash::vm::LifePage life5 =
      kdash::vm::BuildLife(snap, st, now, true, true, true);
  TEST_ASSERT_EQUAL((int)kdash::vm::Badge::Demo, (int)life5.temp.badge);
  // Market: USD comma format, fixing sub has no "실시간".
  kdash::vm::MarketPage mkt =
      kdash::vm::BuildMarket(snap, st, now, true, true, "장중", false);
  TEST_ASSERT_TRUE(mkt.n >= 2);
  TEST_ASSERT_EQUAL_STRING("1,384.50", mkt.rows[0].value);
  TEST_ASSERT_TRUE(strstr(mkt.rows[0].sub, "실시간") == nullptr);
  // Midnight boundary: observed 23:50, now 00:10 next day.
  char tb[16];
  kdash::fmt::KstTime(kdash::KstToUtc(2026, 10, 31, 23, 50),
                      kdash::KstToUtc(2026, 11, 1, 0, 10), tb, sizeof(tb));
  TEST_ASSERT_EQUAL_STRING("10-31 23:50", tb);
}

void test_canvas_renderer(void) {
  kdash::ui::Canvas c;
  TEST_ASSERT_TRUE(kdash::ui::CheckMarginsClean(c));
  kdash::app::AppState st;
  st.settings = FullSettings();
  kdash::app::Snapshot snap;
  kdash::demo::FillDemo(snap, 1700000000LL);
  kdash::vm::LifePage life =
      kdash::vm::BuildLife(snap, st, 1700003600LL, true, true, false);
  kdash::ui::RenderLife(life, c, nullptr);
  TEST_ASSERT_TRUE(kdash::ui::CheckMarginsClean(c));
  // Cell bbox inside its column (measure-based).
  int w = c.measure(kdash::ui::DefaultMedium(), life.temp.value);
  TEST_ASSERT_TRUE(w > 0 && w < 372);
  // Badge inversion present when a badge exists.
  kdash::app::Snapshot stale = snap;
  stale.now.temp_c.validity = kdash::Validity::Stale;
  kdash::vm::LifePage life2 =
      kdash::vm::BuildLife(stale, st, 1700010800LL, true, true, false);
  kdash::ui::RenderLife(life2, c, nullptr);
  kdash::Rect L = kdash::ui::BodyLeftRect();
  TEST_ASSERT_TRUE(
      kdash::ui::CheckBadgePresent(c, L.x, L.y, L.w / 2, 60));
  // All three pages render; PBM writes.
  kdash::vm::MarketPage mkt =
      kdash::vm::BuildMarket(snap, st, 1700003600LL, true, true, "장중", false);
  kdash::ui::RenderMarket(mkt, c, nullptr);
  TEST_ASSERT_TRUE(kdash::ui::CheckMarginsClean(c));
  kdash::vm::DevicePage dev = kdash::vm::BuildDevice(
      st, 1700003600LL, true, true, 3800, "0.4.0", true);
  kdash::ui::RenderDevice(dev, c, nullptr);
  TEST_ASSERT_TRUE(kdash::ui::CheckMarginsClean(c));
  TEST_ASSERT_TRUE(c.crc32() != 0);
}

void test_cycle_planner(void) {
  kdash::app::AppState st;
  st.settings = FullSettings();
  st.page = kdash::ui::Page::Life;
  kdash::power::CycleInput in;
  in.st = &st;
  in.now_utc = 1700000000LL;
  in.clock_verified = true;
  // Nothing due => no wifi, no render, long sleep.
  in.reason = kdash::power::CycleWake::Timer;
  in.next_due_utc = INT64_MAX;
  auto p0 = kdash::power::PlanCycle(in);
  TEST_ASSERT_FALSE(p0.connect_wifi);
  TEST_ASSERT_FALSE(p0.render);
  // Due now => wifi + render.
  in.next_due_utc = in.now_utc;
  auto p1 = kdash::power::PlanCycle(in);
  TEST_ASSERT_TRUE(p1.connect_wifi);
  // Front view, left to right: KEY2 (gray, previous page), KEY1 (gray, next page),
  // KEY0 (green: short = refresh, long = setup). KEY1 => next page, no wifi.
  in.reason = kdash::power::CycleWake::Button;
  in.button_mask = 0x02;
  in.long_press = false;
  in.next_due_utc = INT64_MAX;
  auto p2 = kdash::power::PlanCycle(in);
  TEST_ASSERT_EQUAL((int)kdash::ui::Page::Market, (int)p2.page);
  TEST_ASSERT_FALSE(p2.connect_wifi);
  // KEY2 => previous page (Life wraps to the last tab).
  in.button_mask = 0x04;
  auto p2b = kdash::power::PlanCycle(in);
  TEST_ASSERT_EQUAL((int)kdash::ui::Page::Device, (int)p2b.page);
  TEST_ASSERT_FALSE(p2b.connect_wifi);
  // KEY0 long => portal.
  in.button_mask = 0x01;
  in.long_press = true;
  auto p3 = kdash::power::PlanCycle(in);
  TEST_ASSERT_TRUE(p3.enter_portal);
  // KEY0 short (refresh) with an untrusted clock => NTP attempt, still no HTTPS by itself.
  in.button_mask = 0x01;
  in.long_press = false;
  in.clock_verified = false;
  auto p3b = kdash::power::PlanCycle(in);
  TEST_ASSERT_TRUE(p3b.force_refresh);
  TEST_ASSERT_TRUE(p3b.sync_clock);
  TEST_ASSERT_FALSE(p3b.connect_wifi);
  in.clock_verified = true;
  auto p3c = kdash::power::PlanCycle(in);
  TEST_ASSERT_TRUE(p3c.connect_wifi);
  TEST_ASSERT_FALSE(p3c.sync_clock);
  in.button_mask = 0x01;
  in.long_press = true;
  // Unverified clock => 30-min retry, no HTTPS.
  in.reason = kdash::power::CycleWake::Timer;
  in.button_mask = 0;
  in.long_press = false;
  in.clock_verified = false;
  in.next_due_utc = in.now_utc;
  auto p4 = kdash::power::PlanCycle(in);
  TEST_ASSERT_FALSE(p4.connect_wifi);
  TEST_ASSERT_EQUAL(1800, (int)p4.sleep_seconds);
  // Low battery => max sleep, no fetch.
  in.clock_verified = true;
  in.battery_mv = 3200;
  auto p5 = kdash::power::PlanCycle(in);
  TEST_ASSERT_TRUE(p5.low_battery_mode);
  TEST_ASSERT_FALSE(p5.connect_wifi);
  // 3 crashes => safe mode.
  in.battery_mv = -1;
  in.crashes = 3;
  auto p6 = kdash::power::PlanCycle(in);
  TEST_ASSERT_TRUE(p6.safe_mode);
}

void test_rtc_state(void) {
  kdash::app::Snapshot snap;
  kdash::demo::FillDemo(snap, 1700000000LL);
  kdash::app::ProviderStatus ps[7];
  for (int i = 0; i < 7; i++) ps[i].last_success_utc = 500 + i;
  kdash::app::KisSession kis;
  strncpy(kis.token, "tok12345678901234567890", sizeof(kis.token) - 1);
  kis.expiry_utc = 9999;
  uint8_t blob[4608];
  size_t n = kdash::rtc::Encode(snap, ps, 7, kis, 1, 0xABCD, 3, 9, 0,
                                blob, sizeof(blob));
  TEST_ASSERT_TRUE(n > 0);
  kdash::app::Snapshot s2;
  kdash::app::ProviderStatus ps2[7];
  kdash::app::KisSession k2;
  uint8_t page = 0, parts = 0;
  uint32_t lrc = 0, boots = 0, crashes = 0;
  TEST_ASSERT_TRUE(kdash::rtc::Decode(blob, n, s2, ps2, 7, k2, page, lrc,
                                      parts, boots, crashes));
  TEST_ASSERT_EQUAL(1, page);
  TEST_ASSERT_EQUAL(0xABCD, (int)lrc);
  TEST_ASSERT_EQUAL(9, (int)boots);
  TEST_ASSERT_EQUAL_STRING("기상청(DEMO)", s2.now.temp_c.source);
  // Corrupt CRC => false.
  blob[sizeof(kdash::rtc::Header) + 2] ^= 0x01;
  TEST_ASSERT_FALSE(kdash::rtc::Decode(blob, n, s2, ps2, 7, k2, page, lrc,
                                       parts, boots, crashes));
}

class TestRng : public kdash::IRandom {
 public:
  explicit TestRng(uint32_t s) : s_(s) {}
  uint32_t next_u32() override {
    s_ ^= s_ << 13;
    s_ ^= s_ >> 17;
    s_ ^= s_ << 5;
    return s_;
  }
 private:
  uint32_t s_;
};

void test_portal_handler(void) {
  TestRng rng(42);
  kdash::Settings cur = FullSettings();
  kdash::portal::PortalHandler h(cur, rng, 1000);
  // GET / contains no secret values.
  kdash::portal::Request rq;
  rq.method = kdash::portal::Method::Get;
  rq.path = "/";
  auto r0 = h.Handle(rq, 1100);
  TEST_ASSERT_EQUAL(200, r0.status);
  TEST_ASSERT_TRUE(strstr(r0.body, "password123") == nullptr);
  TEST_ASSERT_TRUE(strstr(r0.body, "kis-secret") == nullptr);
  // POST /save without CSRF => 403.
  kdash::portal::FormField fs[2] = {{"ssid", "A"}, {"secret", ""}};
  kdash::portal::Request bad;
  bad.method = kdash::portal::Method::Post;
  bad.path = "/save";
  bad.fields = fs;
  bad.n_fields = 2;
  bad.csrf = "wrong";
  auto r1 = h.Handle(bad, 1100);
  TEST_ASSERT_EQUAL(403, r1.status);
  TEST_ASSERT_FALSE(h.HasPendingSave());
  // Oversize field => 413.
  char big[300];
  memset(big, 'x', sizeof(big) - 1);
  big[sizeof(big) - 1] = '\0';
  kdash::portal::FormField fs2[1] = {{"ssid", big}};
  kdash::portal::Request big_r;
  big_r.method = kdash::portal::Method::Post;
  big_r.path = "/save";
  big_r.fields = fs2;
  big_r.n_fields = 1;
  big_r.csrf = h.Csrf();
  auto r2 = h.Handle(big_r, 1100);
  TEST_ASSERT_EQUAL(413, r2.status);
  // XSS reflected escaped.
  TEST_ASSERT_TRUE(kdash::provision::EscapeHtml("<script>", nullptr, 0) == 0);
  char esc[64];
  kdash::provision::EscapeHtml("<script>", esc, sizeof(esc));
  TEST_ASSERT_EQUAL_STRING("&lt;script&gt;", esc);
  // Empty secret keeps existing key.
  kdash::portal::FormField fs3[4] = {
      {"ssid", "NewAP"}, {"secret", ""}, {"kma", ""}, {"watchlist", ""}};
  kdash::portal::Request keep;
  keep.method = kdash::portal::Method::Post;
  keep.path = "/save";
  keep.fields = fs3;
  keep.n_fields = 4;
  keep.csrf = h.Csrf();
  auto r3 = h.Handle(keep, 1100);
  TEST_ASSERT_EQUAL(200, r3.status);
  TEST_ASSERT_TRUE(h.HasPendingSave());
  TEST_ASSERT_EQUAL_STRING("password123", h.Pending().wifi_secret);
  // Reset needs two steps.
  kdash::portal::FormField rs1[2] = {{"step1", "RESET"}, {"step2", ""}};
  kdash::portal::Request rr1;
  rr1.method = kdash::portal::Method::Post;
  rr1.path = "/reset";
  rr1.fields = rs1;
  rr1.n_fields = 2;
  rr1.csrf = h.Csrf();
  h.Handle(rr1, 1100);
  TEST_ASSERT_FALSE(h.ResetConfirmed());
  kdash::portal::FormField rs2[2] = {{"step1", "RESET"}, {"step2", "RESET-YES"}};
  kdash::portal::Request rr2 = rr1;
  rr2.fields = rs2;
  h.Handle(rr2, 1100);
  TEST_ASSERT_TRUE(h.ResetConfirmed());
  // Window expired => 410.
  auto r4 = h.Handle(rq, 1000 + 601);
  TEST_ASSERT_EQUAL(410, r4.status);
  // Bad watchlist rejected.
  TestRng rng2(7);
  kdash::portal::PortalHandler h2(cur, rng2, 1000);
  kdash::portal::FormField fs4[4] = {
      {"ssid", "NewAP"}, {"secret", ""}, {"kma", ""}, {"watchlist", "005930,"}};
  kdash::portal::Request wl;
  wl.method = kdash::portal::Method::Post;
  wl.path = "/save";
  wl.fields = fs4;
  wl.n_fields = 4;
  wl.csrf = h2.Csrf();
  auto r5 = h2.Handle(wl, 1100);
  TEST_ASSERT_TRUE(strstr(r5.body, "관심종목") != nullptr);
}

void test_pmic_decode(void) {
  TEST_ASSERT_TRUE(kdash::pmic::UsbAttachedFromReg0A(0x80));
  TEST_ASSERT_FALSE(kdash::pmic::UsbAttachedFromReg0A(0x00));
  TEST_ASSERT_FALSE(kdash::pmic::BusPresentFromReg08(0x00));  // "No input"
  TEST_ASSERT_TRUE(kdash::pmic::BusPresentFromReg08(0x20));   // USB host SDP
  TEST_ASSERT_TRUE(kdash::pmic::BusPresentFromReg08(0x40));   // adapter
}

void test_geogrid(void) {
  int nx = 0, ny = 0;
  TEST_ASSERT_TRUE(kdash::geo::LatLonToGrid(37.5665, 126.9780, nx, ny));
  TEST_ASSERT_EQUAL(60, nx);
  TEST_ASSERT_EQUAL(127, ny);
  TEST_ASSERT_TRUE(kdash::geo::LatLonToGrid(35.1796, 129.0756, nx, ny));
  TEST_ASSERT_EQUAL(98, nx);
  TEST_ASSERT_EQUAL(76, ny);
  TEST_ASSERT_FALSE(kdash::geo::LatLonToGrid(0.0, 0.0, nx, ny));
}

void test_budget_demo_log(void) {
  using kdash::sched::Provider;
  TEST_ASSERT_EQUAL(7, kdash::budget::CapFor(Provider::Exim));
  TEST_ASSERT_EQUAL(4, kdash::budget::CapFor(Provider::FscClose));
  TEST_ASSERT_EQUAL(6, kdash::budget::CapFor(Provider::KisPrice));
  TEST_ASSERT_TRUE(
      kdash::budget::ShouldDefer(Provider::Exim, 7, 7, 24));
  TEST_ASSERT_FALSE(
      kdash::budget::ShouldDefer(Provider::KmaNow, 0, 20, 24));
  // Life providers first: caps leave room when all due.
  int spent = 0;
  TEST_ASSERT_FALSE(
      kdash::budget::ShouldDefer(Provider::KmaNow, 0, spent, 24));
  spent += 2;
  TEST_ASSERT_FALSE(
      kdash::budget::ShouldDefer(Provider::AirKorea, 0, spent, 24));
  // Demo fills without network and is never stored here.
  kdash::app::Snapshot snap;
  kdash::demo::FillDemo(snap, 1700000000LL);
  TEST_ASSERT_TRUE(snap.has_now && snap.has_fx);
  // Provider names + log funnel.
  TEST_ASSERT_EQUAL_STRING("EXIM", kdash::app::ToString(Provider::Exim));
  kdash::log::SetLevel(kdash::log::Level::Info);
  TEST_ASSERT_EQUAL((int)kdash::log::Level::Info,
                    (int)kdash::log::GetLevel());
  kdash::log::KV("provider", "EXIM");
  // Conservative market sessions (U-08): NXT == KRX window.
  int64_t open = kdash::KstToUtc(2026, 11, 4, 10, 0);
  TEST_ASSERT_TRUE(kdash::fin::IsMarketOpen(kdash::Market::Nxt, open));
  int64_t shut = kdash::KstToUtc(2026, 11, 4, 20, 0);
  TEST_ASSERT_FALSE(kdash::fin::IsMarketOpen(kdash::Market::Nxt, shut));
}
