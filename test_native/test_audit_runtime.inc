// Tests exercise the production portable body loop and clock bootstrap orchestration.
#include "../src/net/BodyReader.h"
#include "../src/drivers/ClockBootstrap.h"

namespace {
class AuditBodyStream : public net::IBodyStream {
 public:
  std::string bytes;
  size_t position = 0;
  bool keep_open = false;
  bool stall_read = false;
  bool fail_read = false;
  uint32_t tick = 0;
  explicit AuditBodyStream(const std::string& s) : bytes(s) {}
  int available() override { return (int)(bytes.size() - position); }
  bool connected() override { return keep_open || position < bytes.size(); }
  int read(uint8_t* out, size_t count) override {
    if (fail_read) return -1;
    if (stall_read) return 0;
    const size_t n = count < bytes.size() - position ? count : bytes.size() - position;
    memcpy(out, bytes.data() + position, n);
    position += n;
    return (int)n;
  }
  uint32_t milliseconds() override { return tick; }
  void idle() override { tick += 2; }
};

class AuditClockPlatform : public clock::IClockPlatform {
 public:
  int64_t rtc = 1790000000LL, ntp = 1790000060LL, system = 0;
  bool rtc_ok = true, vl = false, trusted_ntp = false, sync_ok = false;
  bool apply_ok = true;
  int sync_calls = 0, writes = 0;
  bool had_clock_at_sync = false;
  bool ReadRtc(int64_t& e, bool& low) override { e = rtc; low = vl; return rtc_ok; }
  bool TrustedNtpUtc(int64_t& e) override { e = ntp; return trusted_ntp; }
  bool SyncNtp(int64_t& e) override {
    ++sync_calls;
    had_clock_at_sync = system == rtc;
    system += 15;  // time continues advancing during a failed network attempt
    e = ntp;
    if (sync_ok) system = ntp;  // platform/SNTP applies the update itself
    return sync_ok;
  }
  bool ApplyUtc(int64_t e) override { if (apply_ok) system = e; return apply_ok; }
  bool WriteRtc(int64_t) override { ++writes; return true; }
};
}  // namespace

void test_http_body_completion(void) {
  char out[65537];
  AuditBodyStream short_json("{\"result\":1}");
  auto r = net::ReadBody(short_json, 1000, out, 65536, 10);
  TEST_ASSERT_EQUAL((int)HttpError::Transport, (int)r.error);
  TEST_ASSERT_EQUAL_STRING("{\"result\":1}", out);
  AuditBodyStream unknown_stalled("{}"); unknown_stalled.keep_open = true;
  r = net::ReadBody(unknown_stalled, -1, out, 65536, 10);
  TEST_ASSERT_EQUAL((int)HttpError::Timeout, (int)r.error);
  AuditBodyStream unknown_ok("{}");
  r = net::ReadBody(unknown_ok, -1, out, 65536, 10);
  TEST_ASSERT_EQUAL((int)HttpError::None, (int)r.error);
  TEST_ASSERT_EQUAL(2, r.size);
  AuditBodyStream empty("");
  r = net::ReadBody(empty, 0, out, 65536, 10);
  TEST_ASSERT_EQUAL((int)HttpError::None, (int)r.error);
  AuditBodyStream exact(std::string(65536, 'x'));
  r = net::ReadBody(exact, 65536, out, 65536, 10);
  TEST_ASSERT_EQUAL((int)HttpError::None, (int)r.error);
  TEST_ASSERT_EQUAL(65536, r.size);
  TEST_ASSERT_EQUAL(0, out[65536]);
  AuditBodyStream exact_unknown(std::string(65536, 'x'));
  r = net::ReadBody(exact_unknown, -1, out, 65536, 10);
  TEST_ASSERT_EQUAL((int)HttpError::None, (int)r.error);
  AuditBodyStream oversized(std::string(65537, 'x'));
  r = net::ReadBody(oversized, -1, out, 65536, 10);
  TEST_ASSERT_EQUAL((int)HttpError::TooLarge, (int)r.error);
  AuditBodyStream declared_big("{}");
  r = net::ReadBody(declared_big, 65537, out, 65536, 10);
  TEST_ASSERT_EQUAL((int)HttpError::TooLarge, (int)r.error);
  TEST_ASSERT_EQUAL(0, declared_big.position);
  AuditBodyStream wrap("{}"); wrap.tick = UINT32_MAX - 4; wrap.keep_open = true;
  r = net::ReadBody(wrap, 3, out, 65536, 10);
  TEST_ASSERT_EQUAL((int)HttpError::Timeout, (int)r.error);
  AuditBodyStream stalled("{}"); stalled.stall_read = true;
  r = net::ReadBody(stalled, 2, out, 65536, 10);
  TEST_ASSERT_EQUAL((int)HttpError::Timeout, (int)r.error);
}

void test_clock_bootstrap_rtc_without_ntp(void) {
  constexpr int64_t minimum = 1767225600LL;
  AuditClockPlatform p;
  auto d = clock::BootstrapClock(p, minimum, true);
  TEST_ASSERT_TRUE(d.verified);
  TEST_ASSERT_EQUAL((int)clock::Source::Rtc, (int)d.source);
  TEST_ASSERT_TRUE(p.had_clock_at_sync);
  TEST_ASSERT_EQUAL(p.rtc + 15, p.system);  // do not rewind after NTP timeout
  TEST_ASSERT_EQUAL(0, p.writes);
  p.vl = true; p.system = 0;
  d = clock::BootstrapClock(p, minimum, true);
  TEST_ASSERT_FALSE(d.verified);
  p.sync_ok = true; p.system = 0;
  d = clock::BootstrapClock(p, minimum, true);
  TEST_ASSERT_TRUE(d.verified);
  TEST_ASSERT_EQUAL((int)clock::Source::Ntp, (int)d.source);
  TEST_ASSERT_EQUAL(p.ntp, p.system);
  TEST_ASSERT_EQUAL(1, p.writes);
  // A numerical RTC match does not clear VL by itself: write after NTP even at zero offset.
  p.rtc = p.ntp; p.writes = 0;
  d = clock::BootstrapClock(p, minimum, true);
  TEST_ASSERT_EQUAL(1, p.writes);
  p.apply_ok = false;
  // NTP applies its own time; failure to seed RTC does not invalidate fresh NTP.
  TEST_ASSERT_TRUE(clock::BootstrapClock(p, minimum, true).verified);
  p.vl=false; p.sync_ok=false;
  TEST_ASSERT_FALSE(clock::BootstrapClock(p, minimum, false).verified);
  AuditClockPlatform not_attempted;
  d = clock::BootstrapClock(not_attempted, minimum, false);
  TEST_ASSERT_TRUE(d.verified);
  TEST_ASSERT_EQUAL(0, not_attempted.sync_calls);
  not_attempted.rtc = minimum - 1;
  TEST_ASSERT_FALSE(clock::BootstrapClock(not_attempted, minimum, false).verified);
}
