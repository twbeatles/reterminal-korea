// LAN 설정 포털(ADR 0024), 설정 v3, 자동 순환 회귀 테스트. test_audit_portal.inc 뒤에 포함된다.
#include <string>

#include "../src/app/PageRotation.h"
#include "../src/provision/PortalAuth.h"
#include "../src/provision/RequestGuard.h"

namespace lantest {
using namespace kdash;
using namespace kdash::portal;

constexpr const char* kIp = "192.168.0.23";
constexpr const char* kOrigin = "http://192.168.0.23";

AllowedHosts Hosts() {
  AllowedHosts a;
  snprintf(a.ip, sizeof(a.ip), "%s", kIp);
  snprintf(a.mdns, sizeof(a.mdns), "%s", "kdash-3f2a");
  return a;
}

Request Req(Method m, const char* path, const FormField* f = nullptr, size_t n = 0) {
  Request r;
  r.method = m;
  r.path = path;
  r.fields = f;
  r.n_fields = n;
  r.host = kIp;
  if (m == Method::Post) r.origin = kOrigin;
  return r;
}

// 로그인해서 쿠키를 돌려준다. 호출자는 h.Csrf()를 이후 POST에 쓴다.
std::string Login(PortalHandler& h, int64_t now) {
  std::string code = h.PairingCode();
  FormField f[] = {{"code", code.c_str()}};
  Response r = h.Handle(Req(Method::Post, "/login", f, 1), now);
  TEST_ASSERT_EQUAL(200, r.status);
  TEST_ASSERT_NOT_NULL(r.set_cookie);
  return r.set_cookie;
}

}  // namespace lantest

void test_settings_v3_roundtrip_and_v2_migration() {
  using namespace kdash;
  Settings s = FullSettings();
  s.device.lan_mode = LanPortalMode::Always;
  s.device.auto_rotate = false;
  s.device.rotate_minutes = 7;
  s.device.market_hours_only = false;
  s.device.market_from_hour = 8;
  s.device.market_to_hour = 18;
  s.device.refresh_scale_pct = 250;
  uint8_t bytes[2048];
  const size_t n = settings_codec::Encode(s, bytes, sizeof(bytes));
  TEST_ASSERT_TRUE(n > 7);
  Settings back;
  TEST_ASSERT_TRUE(settings_codec::Decode(bytes, n, back));
  TEST_ASSERT_EQUAL((int)LanPortalMode::Always, (int)back.device.lan_mode);
  TEST_ASSERT_FALSE(back.device.auto_rotate);
  TEST_ASSERT_EQUAL(7, back.device.rotate_minutes);
  TEST_ASSERT_FALSE(back.device.market_hours_only);
  TEST_ASSERT_EQUAL(8, back.device.market_from_hour);
  TEST_ASSERT_EQUAL(18, back.device.market_to_hour);
  TEST_ASSERT_EQUAL(250, back.device.refresh_scale_pct);

  // v2 payload == v5 payload without the 9-byte device tail and with version 2.
  uint8_t v2[2048];
  memcpy(v2, bytes, n - 9);
  v2[0] = 2; v2[1] = 0;
  Settings migrated;
  TEST_ASSERT_TRUE(settings_codec::Decode(v2, n - 9, migrated));
  TEST_ASSERT_EQUAL_STRING(s.wifi_ssid, migrated.wifi_ssid);
  TEST_ASSERT_EQUAL_STRING(s.providers.kis_app_secret, migrated.providers.kis_app_secret);
  TEST_ASSERT_TRUE(migrated.device.auto_rotate);  // v2 has none: defaults
  TEST_ASSERT_EQUAL(kDefaultRotateMinutes, migrated.device.rotate_minutes);
  TEST_ASSERT_EQUAL(kDefaultRefreshScalePct, migrated.device.refresh_scale_pct);
  TEST_ASSERT_TRUE(settings_codec::Migrate(2, v2, n - 9, migrated));
  // A v2 header over a v3-length body, or a v3 header without the tail, is rejected.
  Settings untouched = FullSettings();
  TEST_ASSERT_FALSE(settings_codec::Decode(v2, n, untouched));
  TEST_ASSERT_FALSE(settings_codec::Decode(bytes, n - 8, untouched));
  // Out-of-range device values and unknown flag bits are rejected, never clamped.
  uint8_t bad[2048];
  memcpy(bad, bytes, n);
  bad[n - 7] = 0;  // rotate_minutes = 0
  TEST_ASSERT_FALSE(settings_codec::Decode(bad, n, untouched));
  memcpy(bad, bytes, n);
  bad[n - 8] = 0x80;  // unknown flag bit
  TEST_ASSERT_FALSE(settings_codec::Decode(bad, n, untouched));
  memcpy(bad, bytes, n);
  bad[n - 9] = 9;  // lan_mode
  TEST_ASSERT_FALSE(settings_codec::Decode(bad, n, untouched));
}

void test_device_config_defaults_and_validation() {
  using namespace kdash;
  DeviceConfig d;
  TEST_ASSERT_EQUAL((int)LanPortalMode::Button, (int)d.lan_mode);
  TEST_ASSERT_TRUE(d.auto_rotate);
  TEST_ASSERT_EQUAL(5, d.rotate_minutes);
  TEST_ASSERT_TRUE(d.market_hours_only);
  TEST_ASSERT_EQUAL(9, d.market_from_hour);
  TEST_ASSERT_EQUAL(16, d.market_to_hour);
  TEST_ASSERT_EQUAL((int)settings::Validation::Ok, (int)settings::ValidateDevice(d));
  DeviceConfig x = d;
  x.rotate_minutes = 0;
  TEST_ASSERT_EQUAL((int)settings::Validation::BadDevice, (int)settings::ValidateDevice(x));
  x = d; x.rotate_minutes = 61;
  TEST_ASSERT_EQUAL((int)settings::Validation::BadDevice, (int)settings::ValidateDevice(x));
  x = d; x.market_from_hour = 16;  // from must be < to
  TEST_ASSERT_EQUAL((int)settings::Validation::BadDevice, (int)settings::ValidateDevice(x));
  x = d; x.market_to_hour = 25;
  TEST_ASSERT_EQUAL((int)settings::Validation::BadDevice, (int)settings::ValidateDevice(x));
  x = d; x.refresh_scale_pct = 99;
  TEST_ASSERT_EQUAL((int)settings::Validation::BadDevice, (int)settings::ValidateDevice(x));
  x = d; x.market_from_hour = 0; x.market_to_hour = 24;
  TEST_ASSERT_EQUAL((int)settings::Validation::Ok, (int)settings::ValidateDevice(x));
}

void test_page_rotation_policy() {
  using namespace kdash;
  using namespace kdash::app;
  DeviceConfig d;
  RotationInput in;
  in.device = &d;
  in.usb = true;
  in.kst_hour = 10;
  in.page = ui::Page::Life;
  const uint32_t five_min = 5u * 60u * 1000u;
  in.last_change_ms = 1000;
  in.now_ms = 1000 + five_min - 1;
  TEST_ASSERT_FALSE(DecideRotation(in).due);  // 5분 전에는 아무 일도 없다
  in.now_ms = 1000 + five_min;
  RotationDecision r = DecideRotation(in);
  TEST_ASSERT_TRUE(r.due);
  TEST_ASSERT_TRUE(r.change);
  TEST_ASSERT_EQUAL((int)ui::Page::Market, (int)r.next);  // 장 시간: 생활 -> 시장
  in.page = ui::Page::Market;
  r = DecideRotation(in);
  TEST_ASSERT_TRUE(r.change);
  TEST_ASSERT_EQUAL((int)ui::Page::Life, (int)r.next);  // 시장 -> 생활
  // 장 시간 밖(09:00 이상 16:00 미만이 아님): 생활에 머물고, 시장 화면이면 생활로 복귀.
  for (int hour : {8, 16, 23, 0}) {
    in.kst_hour = hour;
    in.page = ui::Page::Life;
    r = DecideRotation(in);
    TEST_ASSERT_TRUE(r.due);
    TEST_ASSERT_FALSE_MESSAGE(r.change, "Life stays Life outside market hours");
    in.page = ui::Page::Market;
    r = DecideRotation(in);
    TEST_ASSERT_TRUE(r.change);
    TEST_ASSERT_EQUAL((int)ui::Page::Life, (int)r.next);
  }
  // 경계: 09시는 장 시간, 15시도 장 시간, 16시는 아니다.
  TEST_ASSERT_TRUE(InMarketHours(d, 9));
  TEST_ASSERT_TRUE(InMarketHours(d, 15));
  TEST_ASSERT_FALSE(InMarketHours(d, 16));
  TEST_ASSERT_FALSE(InMarketHours(d, 8));
  TEST_ASSERT_FALSE(InMarketHours(d, -1));  // 시각을 믿을 수 없음
  // 시각을 믿을 수 없으면 시장을 순환에 넣지 않는다.
  in.kst_hour = -1; in.page = ui::Page::Life;
  TEST_ASSERT_FALSE(DecideRotation(in).change);
  // 시간대 제한을 끄면 언제든 순환한다.
  d.market_hours_only = false;
  in.kst_hour = 3; in.page = ui::Page::Life;
  TEST_ASSERT_TRUE(DecideRotation(in).change);
  d.market_hours_only = true;
  // 배터리, 꺼짐, 달력/기기 탭, 포털 열림은 순환하지 않는다.
  in.kst_hour = 10; in.page = ui::Page::Life;
  in.usb = false;
  TEST_ASSERT_FALSE(DecideRotation(in).due);
  in.usb = true;
  d.auto_rotate = false;
  TEST_ASSERT_FALSE(DecideRotation(in).due);
  d.auto_rotate = true;
  for (ui::Page p : {ui::Page::Calendar, ui::Page::Device}) {
    in.page = p;
    TEST_ASSERT_FALSE(DecideRotation(in).due);
  }
  in.page = ui::Page::Life;
  in.portal_open = true;
  TEST_ASSERT_FALSE(DecideRotation(in).due);
  in.portal_open = false;
  // 간격 설정(1분)과 millis 되돌림(wrap) 안전성.
  d.rotate_minutes = 1;
  in.last_change_ms = 0xFFFFFFFFu - 30000u;
  in.now_ms = 40000u;  // 경과 70초(wrap)
  TEST_ASSERT_TRUE(DecideRotation(in).change);
  in.now_ms = 20000u;  // 경과 50초
  TEST_ASSERT_FALSE(DecideRotation(in).due);
}

void test_request_guard_host_origin_cookie() {
  using namespace kdash::portal;
  using namespace lantest;
  const AllowedHosts a = Hosts();
  TEST_ASSERT_TRUE(HostAllowed("192.168.0.23", a));
  TEST_ASSERT_TRUE(HostAllowed("192.168.0.23:80", a));
  TEST_ASSERT_TRUE(HostAllowed("KDASH-3F2A.local", a));  // 대소문자 무시
  TEST_ASSERT_TRUE(HostAllowed("kdash-3f2a.local:80", a));
  TEST_ASSERT_FALSE(HostAllowed("evil.example", a));            // DNS 리바인딩
  TEST_ASSERT_FALSE(HostAllowed("192.168.0.23.evil.example", a));
  TEST_ASSERT_FALSE(HostAllowed("192.168.0.24", a));
  TEST_ASSERT_FALSE(HostAllowed("192.168.0.23:abc", a));
  TEST_ASSERT_FALSE(HostAllowed("", a));
  TEST_ASSERT_FALSE(HostAllowed(nullptr, a));
  AllowedHosts none;  // 허용 목록이 비면 모두 거부(fail closed)
  TEST_ASSERT_FALSE(HostAllowed("192.168.0.23", none));
  TEST_ASSERT_FALSE(HostAllowed("", none));

  TEST_ASSERT_TRUE(OriginAllowed("http://192.168.0.23", nullptr, a));
  TEST_ASSERT_TRUE(OriginAllowed("http://kdash-3f2a.local", nullptr, a));
  TEST_ASSERT_TRUE(OriginAllowed(nullptr, "http://192.168.0.23/keys", a));
  TEST_ASSERT_TRUE(OriginAllowed("", "http://192.168.0.23:80/wifi", a));
  TEST_ASSERT_FALSE(OriginAllowed("http://evil.example", "http://192.168.0.23/", a));  // Origin 우선
  TEST_ASSERT_FALSE(OriginAllowed("null", nullptr, a));  // 근거 없는 null은 거부
  // 회귀(2026-11-12): Referrer-Policy: no-referrer 때 브라우저는 같은 사이트 POST에 "Origin: null"을 보냈다.
  TEST_ASSERT_TRUE(OriginAllowed("null", "http://192.168.0.23/", a));
  TEST_ASSERT_TRUE(OriginAllowed("null", "http://kdash-3f2a.local/wifi", a));
  TEST_ASSERT_FALSE(OriginAllowed("null", "http://evil.example/", a));  // null이어도 Referer가 외부면 거부
  TEST_ASSERT_FALSE(OriginAllowed("null", "", a));
  TEST_ASSERT_FALSE(OriginAllowed("https://192.168.0.23", nullptr, a));
  TEST_ASSERT_FALSE(OriginAllowed(nullptr, "http://192.168.0.23.evil.example/", a));
  TEST_ASSERT_FALSE(OriginAllowed(nullptr, nullptr, a));  // 둘 다 없으면 거부

  char v[40];
  TEST_ASSERT_TRUE(ExtractCookie("a=1; kdash_sid=abc123; b=2", "kdash_sid", v, sizeof(v)));
  TEST_ASSERT_EQUAL_STRING("abc123", v);
  TEST_ASSERT_TRUE(ExtractCookie("kdash_sid=zz", "kdash_sid", v, sizeof(v)));
  TEST_ASSERT_FALSE(ExtractCookie("xkdash_sid=abc", "kdash_sid", v, sizeof(v)));
  TEST_ASSERT_FALSE(ExtractCookie("a=1", "kdash_sid", v, sizeof(v)));
  char tiny[3];
  TEST_ASSERT_FALSE(ExtractCookie("kdash_sid=abcdef", "kdash_sid", tiny, sizeof(tiny)));
  TEST_ASSERT_FALSE(ExtractCookie(nullptr, "kdash_sid", v, sizeof(v)));
}

void test_portal_auth_pairing_lockout_and_session() {
  using namespace kdash::portal;
  TestRng rng(11);
  PortalAuth a(rng);
  a.Tick(0);
  TEST_ASSERT_EQUAL(6, (int)strlen(a.Code()));
  TEST_ASSERT_TRUE(a.TakeDisplayChanged());
  TEST_ASSERT_FALSE(a.TakeDisplayChanged());
  const std::string first = a.Code();
  char tok[kSessionTokenHex + 1];
  // 틀린 코드는 세션을 만들지 않는다.
  TEST_ASSERT_EQUAL((int)LoginResult::Bad, (int)a.TryLogin("000000x", 1, tok, sizeof(tok)));
  TEST_ASSERT_FALSE(a.HasSession());
  TEST_ASSERT_EQUAL((int)LoginResult::Ok, (int)a.TryLogin(first.c_str(), 2, tok, sizeof(tok)));
  TEST_ASSERT_TRUE(a.HasSession());
  TEST_ASSERT_TRUE(a.SessionValid(tok, 3));
  TEST_ASSERT_FALSE(a.SessionValid("deadbeef", 3));
  // 코드는 1회용: 같은 코드로 다시 로그인할 수 없다(새 코드가 발급됨).
  TEST_ASSERT_TRUE(first != a.Code());
  char tok2[kSessionTokenHex + 1];
  TEST_ASSERT_EQUAL((int)LoginResult::Bad, (int)a.TryLogin(first.c_str(), 4, tok2, sizeof(tok2)));
  // 새 로그인은 이전 세션을 끊는다(세션은 1개).
  const std::string second = a.Code();
  TEST_ASSERT_EQUAL((int)LoginResult::Ok, (int)a.TryLogin(second.c_str(), 5, tok2, sizeof(tok2)));
  TEST_ASSERT_FALSE(a.SessionValid(tok, 6));
  TEST_ASSERT_TRUE(a.SessionValid(tok2, 6));
  // 유휴 10분이면 만료, 활동하면 연장.
  TEST_ASSERT_TRUE(a.SessionValid(tok2, 6 + kSessionIdleSec - 1));
  TEST_ASSERT_FALSE(a.SessionValid(tok2, 6 + 2 * kSessionIdleSec));
  // 최대 30분: 계속 활동해도 끊긴다.
  const std::string third = a.Code();
  char tok3[kSessionTokenHex + 1];
  int64_t t = 1000;
  TEST_ASSERT_EQUAL((int)LoginResult::Ok, (int)a.TryLogin(third.c_str(), t, tok3, sizeof(tok3)));
  for (int64_t k = t; k < t + kSessionMaxSec - 100; k += 300) TEST_ASSERT_TRUE(a.SessionValid(tok3, k));
  TEST_ASSERT_FALSE(a.SessionValid(tok3, t + kSessionMaxSec + 1));
  // 5회 틀리면 5분 잠금. 잠금 중에는 맞는 코드도 거부하고 코드는 보이지 않는다.
  const int64_t base = 5000;
  const std::string live = a.Code();
  for (int i = 0; i < kPairMaxFails - 1; i++)
    TEST_ASSERT_EQUAL((int)LoginResult::Bad, (int)a.TryLogin("123456", base + i, tok, sizeof(tok)));
  TEST_ASSERT_EQUAL((int)LoginResult::Locked, (int)a.TryLogin("123456", base + 10, tok, sizeof(tok)));
  TEST_ASSERT_TRUE(a.Locked());
  TEST_ASSERT_EQUAL_STRING("", a.Code());
  TEST_ASSERT_EQUAL((int)LoginResult::Locked, (int)a.TryLogin(live.c_str(), base + 20, tok, sizeof(tok)));
  a.Tick(base + 10 + kPairLockSec - 1);
  TEST_ASSERT_TRUE(a.Locked());
  a.Tick(base + 10 + kPairLockSec);
  TEST_ASSERT_FALSE(a.Locked());
  TEST_ASSERT_EQUAL(6, (int)strlen(a.Code()));
  TEST_ASSERT_TRUE(live != a.Code());  // 잠금 뒤에는 새 코드
  // 코드는 10분이 지나면 새로 바뀐다.
  const std::string old = a.Code();
  a.Tick(base + 10 + kPairLockSec + kPairCodeTtlSec);
  TEST_ASSERT_TRUE(old != a.Code());
}

void test_lan_handler_requires_login_host_and_origin() {
  using namespace kdash;
  using namespace kdash::portal;
  using namespace lantest;
  TestRng rng(21);
  Settings s = FullSettings();
  PortalHandler h(s, rng, 0);
  h.UseLan(Hosts(), 600);
  TEST_ASSERT_TRUE(h.IsLan());
  const std::string code = h.PairingCode();
  TEST_ASSERT_EQUAL(6, (int)code.size());

  // 로그인 없이는 어떤 설정 화면도 열리지 않는다. 로그인 화면에도 코드는 없다.
  Response r = h.Handle(Req(Method::Get, "/"), 1);
  TEST_ASSERT_EQUAL(200, r.status);
  TEST_ASSERT_NOT_NULL(strstr(r.body, "/login"));
  TEST_ASSERT_NULL(strstr(r.body, code.c_str()));
  for (const char* p : {"/keys", "/wifi", "/export", "/status", "/device"}) {
    r = h.Handle(Req(Method::Get, p), 2);
    TEST_ASSERT_EQUAL_MESSAGE(401, r.status, p);
    TEST_ASSERT_NULL(strstr(r.body, "password123"));
  }
  // POST도 마찬가지(CSRF 값을 알아도 세션이 없으면 거부).
  FormField save[] = {{"sec", "location"}, {"display", "해킹"}, {"csrf", h.Csrf()}};
  Request post = Req(Method::Post, "/save", save, 3);
  post.csrf = h.Csrf();
  TEST_ASSERT_EQUAL(401, h.Handle(post, 3).status);
  TEST_ASSERT_FALSE(h.HasPendingSave());

  // Host 위조(DNS 리바인딩)는 421, Origin 없는/다른 POST는 403.
  Request evil = Req(Method::Get, "/");
  evil.host = "evil.example";
  TEST_ASSERT_EQUAL(421, h.Handle(evil, 4).status);
  FormField login[] = {{"code", code.c_str()}};
  Request noorigin = Req(Method::Post, "/login", login, 1);
  noorigin.origin = nullptr;
  TEST_ASSERT_EQUAL(403, h.Handle(noorigin, 5).status);
  Request foreign = Req(Method::Post, "/login", login, 1);
  foreign.origin = "http://evil.example";
  TEST_ASSERT_EQUAL(403, h.Handle(foreign, 6).status);
  TEST_ASSERT_FALSE(h.SessionActive());

  // 틀린 코드는 세션 없음. 맞는 코드는 쿠키(HttpOnly, SameSite=Strict)를 준다.
  FormField wrong[] = {{"code", "999999"}};
  r = h.Handle(Req(Method::Post, "/login", wrong, 1), 7);
  TEST_ASSERT_NULL(r.set_cookie);
  TEST_ASSERT_FALSE(h.SessionActive());
  const std::string cookie = Login(h, 8);
  TEST_ASSERT_NOT_NULL(strstr(cookie.c_str(), "HttpOnly"));
  TEST_ASSERT_NOT_NULL(strstr(cookie.c_str(), "SameSite=Strict"));
  TEST_ASSERT_TRUE(h.SessionActive());

  // 세션이 있으면 열리고, 어떤 응답에도 저장된 비밀·페어링 코드·세션 토큰이 없다.
  const std::string newcode = h.PairingCode();
  char sid[kSessionTokenHex + 8];
  TEST_ASSERT_TRUE(ExtractCookie(cookie.c_str(), kSessionCookie, sid, sizeof(sid)));
  for (const char* p : {"/", "/wifi", "/keys", "/location", "/watch", "/device", "/status", "/export"}) {
    Request g = Req(Method::Get, p);
    g.cookie = cookie.c_str();
    r = h.Handle(g, 9);
    TEST_ASSERT_EQUAL_MESSAGE(200, r.status, p);
    TEST_ASSERT_NULL_MESSAGE(strstr(r.body, "password123"), p);
    TEST_ASSERT_NULL_MESSAGE(strstr(r.body, "kis-secret"), p);
    TEST_ASSERT_NULL_MESSAGE(strstr(r.body, "kkkkkkkk"), p);
    TEST_ASSERT_NULL_MESSAGE(strstr(r.body, sid), p);
    TEST_ASSERT_NULL_MESSAGE(strstr(r.body, newcode.c_str()), p);
    TEST_ASSERT_NULL_MESSAGE(strstr(r.body, code.c_str()), p);
  }
  // 다른 사람의 쿠키(토큰 불일치)는 거부된다.
  Request stolen = Req(Method::Get, "/keys");
  stolen.cookie = "kdash_sid=00000000000000000000000000000000";
  TEST_ASSERT_EQUAL(401, h.Handle(stolen, 10).status);
}

void test_lan_handler_section_saves_are_scoped() {
  using namespace kdash;
  using namespace kdash::portal;
  using namespace lantest;
  TestRng rng(31);
  Settings s = FullSettings();
  PortalHandler h(s, rng, 0);
  h.UseLan(Hosts(), 600);
  const std::string cookie = Login(h, 1);
  auto post = [&](const char* path, const FormField* f, size_t n, bool csrf = true) {
    Request r = Req(Method::Post, path, f, n);
    r.cookie = cookie.c_str();
    r.csrf = csrf ? h.Csrf() : "";
    return h.Handle(r, 2);
  };

  // LAN에서 구역 없는 전체 덮어쓰기는 금지(400). CSRF가 없거나 틀려도 403.
  FormField nosec[] = {{"ssid", "X"}};
  TEST_ASSERT_EQUAL(400, post("/save", nosec, 1).status);
  FormField loc[] = {{"sec", "location"}, {"display", "부산 해운대"}, {"station", "해운대"},
                     {"nx", "99"}, {"ny", "75"}};
  TEST_ASSERT_EQUAL(403, post("/save", loc, 5, false).status);
  TEST_ASSERT_FALSE(h.HasPendingSave());

  // 지역 저장은 다른 구역(Wi-Fi/키)을 건드리지 않고, Wi-Fi 변경으로 분류되지 않는다.
  // (ssid/secret/kma 필드를 같이 보내도 구역 밖 필드는 무시한다.)
  FormField loc2[] = {{"sec", "location"}, {"display", "부산 해운대"}, {"station", "해운대"},
                      {"nx", "99"}, {"ny", "75"}, {"ssid", "Hijack"}, {"kma", "stolen-key"}};
  TEST_ASSERT_EQUAL(200, post("/save", loc2, 7).status);
  TEST_ASSERT_TRUE(h.HasPendingSave());
  TEST_ASSERT_FALSE(h.PendingWifiChanged());
  TEST_ASSERT_EQUAL_STRING("부산 해운대", h.Pending().location.display_name);
  TEST_ASSERT_EQUAL(99, h.Pending().location.grid_x);
  TEST_ASSERT_EQUAL_STRING("TestAP", h.Pending().wifi_ssid);
  TEST_ASSERT_EQUAL_STRING(s.providers.kma_key, h.Pending().providers.kma_key);
  h.CompleteSave(true);

  // Wi-Fi 구역: SSID가 바뀌면 접속 시험이 필요한 저장(WifiChanged)이다. 빈 비밀번호는 유지.
  FormField wifi[] = {{"sec", "wifi"}, {"ssid", "NewHome"}, {"secret", ""}};
  TEST_ASSERT_EQUAL(200, post("/save", wifi, 3).status);
  TEST_ASSERT_TRUE(h.HasPendingSave());
  TEST_ASSERT_TRUE(h.PendingWifiChanged());
  TEST_ASSERT_EQUAL_STRING("NewHome", h.Pending().wifi_ssid);
  TEST_ASSERT_EQUAL_STRING("password123", h.Pending().wifi_secret);
  h.CompleteSave(false);  // 접속 시험 실패 -> 이전 설정 유지
  TEST_ASSERT_FALSE(h.HasPendingSave());
  Request st = Req(Method::Get, "/status");
  st.cookie = cookie.c_str();
  TEST_ASSERT_NOT_NULL(strstr(h.Handle(st, 3).body, "실패"));
  // 같은 SSID/비밀번호를 그대로 저장하면 접속 시험이 필요 없다.
  FormField same[] = {{"sec", "wifi"}, {"ssid", "TestAP"}, {"secret", ""}};
  TEST_ASSERT_EQUAL(200, post("/save", same, 3).status);
  TEST_ASSERT_FALSE(h.PendingWifiChanged());
  h.CompleteSave(true);

  // 잘못된 구역 이름, 잘못된 값은 거부.
  FormField weird[] = {{"sec", "all"}};
  TEST_ASSERT_EQUAL(400, post("/save", weird, 1).status);
  FormField badgrid[] = {{"sec", "location"}, {"nx", "9999"}};
  TEST_ASSERT_NOT_NULL(strstr(post("/save", badgrid, 2).body, "격자"));
  TEST_ASSERT_FALSE(h.HasPendingSave());

  // 키 구역: 값은 저장하되 응답엔 되돌려 주지 않는다. 삭제 체크박스는 그 키만 지운다.
  FormField keys[] = {{"sec", "keys"}, {"form", "1"}, {"kma", "new-kma-key"}, {"clear_air", "1"},
                      {"fsc", "1"}};
  Response kr = post("/save", keys, 5);
  TEST_ASSERT_EQUAL(200, kr.status);
  TEST_ASSERT_NULL(strstr(kr.body, "new-kma-key"));
  TEST_ASSERT_EQUAL_STRING("new-kma-key", h.Pending().providers.kma_key);
  TEST_ASSERT_EQUAL_STRING("", h.Pending().providers.air_key);
  TEST_ASSERT_TRUE(h.Pending().providers.fsc_enabled);
  TEST_ASSERT_FALSE(h.Pending().providers.kis_enabled);
  TEST_ASSERT_EQUAL_STRING("TestAP", h.Pending().wifi_ssid);
  h.CompleteSave(true);
}

void test_lan_handler_device_section_and_rotation_settings() {
  using namespace kdash;
  using namespace kdash::portal;
  using namespace lantest;
  TestRng rng(41);
  Settings s = FullSettings();
  PortalHandler h(s, rng, 0);
  h.UseLan(Hosts(), 600);
  const std::string cookie = Login(h, 1);
  auto post = [&](const FormField* f, size_t n) {
    Request r = Req(Method::Post, "/save", f, n);
    r.cookie = cookie.c_str();
    r.csrf = h.Csrf();
    return h.Handle(r, 2);
  };
  // 기기 화면은 현재 값을 보여 준다(5분, 09~16시, 포털 모드).
  Request g = Req(Method::Get, "/device");
  g.cookie = cookie.c_str();
  Response page = h.Handle(g, 2);
  TEST_ASSERT_NOT_NULL(strstr(page.body, "name=\"rotate_min\""));
  TEST_ASSERT_NOT_NULL(strstr(page.body, "value=\"5\""));
  TEST_ASSERT_NOT_NULL(strstr(page.body, "name=\"market_from\""));
  TEST_ASSERT_NOT_NULL(strstr(page.body, "value=\"16\""));
  TEST_ASSERT_NOT_NULL(strstr(page.body, "name=fast_refresh"));

  FormField ok[] = {{"sec", "device"}, {"dev_present", "1"}, {"page", "1"}, {"auto_rotate", "1"},
                    {"rotate_min", "10"}, {"market_hours", "1"}, {"market_from", "9"},
                    {"market_to", "15"}, {"refresh_pct", "200"}, {"lan_mode", "2"},
                    {"fast_refresh", "1"}};
  TEST_ASSERT_EQUAL(200, post(ok, 11).status);
  TEST_ASSERT_TRUE(h.HasPendingSave());
  const DeviceConfig& d = h.Pending().device;
  TEST_ASSERT_TRUE(d.auto_rotate);
  TEST_ASSERT_EQUAL(10, d.rotate_minutes);
  TEST_ASSERT_TRUE(d.market_hours_only);
  TEST_ASSERT_EQUAL(9, d.market_from_hour);
  TEST_ASSERT_EQUAL(15, d.market_to_hour);
  TEST_ASSERT_EQUAL(200, d.refresh_scale_pct);
  TEST_ASSERT_EQUAL((int)LanPortalMode::Always, (int)d.lan_mode);
  TEST_ASSERT_EQUAL(1, h.Pending().display_page);
  TEST_ASSERT_TRUE(d.fast_refresh);
  TEST_ASSERT_FALSE(h.PendingWifiChanged());
  h.CompleteSave(true);

  // 체크박스를 빼면 꺼진다(순환 끔, 장 시간 제한 끔).
  FormField off[] = {{"sec", "device"}, {"dev_present", "1"}, {"page", "0"}, {"rotate_min", "5"},
                     {"market_from", "9"}, {"market_to", "16"}, {"refresh_pct", "100"},
                     {"lan_mode", "1"}};
  TEST_ASSERT_EQUAL(200, post(off, 8).status);
  TEST_ASSERT_FALSE(h.Pending().device.auto_rotate);
  TEST_ASSERT_FALSE(h.Pending().device.market_hours_only);
  TEST_ASSERT_FALSE(h.Pending().device.fast_refresh);
  h.CompleteSave(true);

  // 범위 밖/잘못된 값은 거부하고 pending을 만들지 않는다.
  struct Bad { const char* name; const char* value; } bads[] = {
      {"rotate_min", "0"}, {"rotate_min", "61"}, {"rotate_min", "abc"}, {"market_from", "24"},
      {"market_to", "0"}, {"refresh_pct", "50"}, {"refresh_pct", "401"}, {"lan_mode", "3"},
      {"page", "4"}};
  for (const Bad& b : bads) {
    FormField f[] = {{"sec", "device"}, {"dev_present", "1"}, {"page", "0"}, {"rotate_min", "5"},
                     {"market_from", "9"}, {"market_to", "16"}, {"refresh_pct", "100"},
                     {"lan_mode", "1"}, {"x", ""}};
    for (auto& e : f) if (strcmp(e.name, b.name) == 0) e.value = b.value;
    f[8].name = "unused";
    Response r = post(f, 9);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(r.body, "기기 설정 오류"), b.name);
    TEST_ASSERT_FALSE_MESSAGE(h.HasPendingSave(), b.name);
  }
  // 장 시작이 종료와 같거나 늦으면 거부.
  FormField inv[] = {{"sec", "device"}, {"dev_present", "1"}, {"page", "0"}, {"rotate_min", "5"},
                     {"market_from", "16"}, {"market_to", "16"}, {"refresh_pct", "100"},
                     {"lan_mode", "1"}};
  TEST_ASSERT_NOT_NULL(strstr(post(inv, 8).body, "기기 설정 오류"));
  TEST_ASSERT_FALSE(h.HasPendingSave());
}

void test_lan_handler_actions_logout_and_always_mode() {
  using namespace kdash;
  using namespace kdash::portal;
  using namespace lantest;
  TestRng rng(51);
  Settings s = FullSettings();
  PortalHandler h(s, rng, 0);
  h.UseLan(Hosts(), 0);  // 상시 모드: 만료 없음
  const std::string cookie = Login(h, 1);
  (void)cookie;
  // 며칠이 지나도 포털은 410을 내지 않는다(세션만 만료).
  const int64_t later = 3 * 24 * 3600;
  Request g = Req(Method::Get, "/");
  g.cookie = cookie.c_str();
  TEST_ASSERT_EQUAL(200, h.Handle(g, later).status);  // 로그인 화면(세션 만료)
  TEST_ASSERT_NOT_NULL(strstr(h.Handle(g, later).body, "/login"));
  const std::string cookie2 = Login(h, later + 1);
  auto post2 = [&](const char* path) {
    Request r = Req(Method::Post, path);
    r.cookie = cookie2.c_str();
    r.csrf = h.Csrf();
    return h.Handle(r, later + 2);
  };
  TEST_ASSERT_EQUAL((int)PortalAction::None, (int)h.TakeAction());
  TEST_ASSERT_EQUAL(200, post2("/refresh").status);
  TEST_ASSERT_EQUAL((int)PortalAction::Refresh, (int)h.TakeAction());
  TEST_ASSERT_EQUAL((int)PortalAction::None, (int)h.TakeAction());  // 한 번만
  post2("/reboot");
  TEST_ASSERT_EQUAL((int)PortalAction::Reboot, (int)h.TakeAction());
  post2("/ap");
  TEST_ASSERT_EQUAL((int)PortalAction::OpenAp, (int)h.TakeAction());
  // CSRF 없이는 동작 요청이 거부된다.
  Request nocsrf = Req(Method::Post, "/reboot");
  nocsrf.cookie = cookie2.c_str();
  TEST_ASSERT_EQUAL(403, h.Handle(nocsrf, later + 3).status);
  TEST_ASSERT_EQUAL((int)PortalAction::None, (int)h.TakeAction());
  // 상시 모드의 /close는 로그아웃이다: 포털은 닫히지 않고 세션만 끊긴다.
  TEST_ASSERT_EQUAL(200, post2("/close").status);
  TEST_ASSERT_FALSE(h.Closed());
  TEST_ASSERT_FALSE(h.SessionActive());
  Request after = Req(Method::Get, "/keys");
  after.cookie = cookie2.c_str();
  TEST_ASSERT_EQUAL(401, h.Handle(after, later + 4).status);

  // 창이 있는 포털(Button 모드)의 /close는 포털을 닫는다. 창이 끝나면 410.
  TestRng rng2(52);
  PortalHandler w(s, rng2, 0);
  w.UseLan(Hosts(), 600);
  const std::string wc = Login(w, 1);
  Request c = Req(Method::Post, "/close");
  c.cookie = wc.c_str();
  c.csrf = w.Csrf();
  TEST_ASSERT_EQUAL(200, w.Handle(c, 2).status);
  TEST_ASSERT_TRUE(w.Closed());
  TestRng rng3(53);
  PortalHandler e(s, rng3, 0);
  e.UseLan(Hosts(), 600);
  TEST_ASSERT_EQUAL(410, e.Handle(Req(Method::Get, "/"), 601).status);
}

void test_device_tab_shows_portal_address_and_code() {
  using namespace kdash;
  app::AppState st;
  st.settings = FullSettings();
  // 포털이 닫혀 있으면 주소/코드 줄이 없다.
  vm::DevicePage closed = vm::BuildDevice(st, 1700003600LL, true, true, 3800, "1.0.0", true);
  TEST_ASSERT_EQUAL_STRING("", closed.portal_url);
  TEST_ASSERT_EQUAL_STRING("", closed.portal_code);
  st.portal.open = true;
  snprintf(st.portal.ip, sizeof(st.portal.ip), "192.168.0.23");
  snprintf(st.portal.host, sizeof(st.portal.host), "kdash-3f2a");
  snprintf(st.portal.code, sizeof(st.portal.code), "482913");
  st.portal.remaining_min = 8;
  vm::DevicePage open = vm::BuildDevice(st, 1700003600LL, true, true, 3800, "1.0.0", true);
  TEST_ASSERT_NOT_NULL(strstr(open.portal_url, "http://192.168.0.23"));
  TEST_ASSERT_NOT_NULL(strstr(open.portal_host, "kdash-3f2a.local"));
  TEST_ASSERT_NOT_NULL(strstr(open.portal_host, "8분"));
  TEST_ASSERT_NOT_NULL(strstr(open.portal_code, "482913"));
  ui::Canvas c;
  ui::RenderDevice(open, c, nullptr);
  TEST_ASSERT_TRUE(ui::CheckMarginsClean(c));
  TEST_ASSERT_EQUAL_INT(0, c.missing_glyphs());
  // 상시 모드(만료 없음), mDNS 등록 실패(이름 없음), 잠금 중도 화면에 맞는다.
  st.portal.remaining_min = -1;
  st.portal.host[0] = '\0';
  st.portal.code[0] = '\0';
  st.portal.locked = true;
  vm::DevicePage locked = vm::BuildDevice(st, 1700003600LL, true, true, 3800, "1.0.0", true);
  TEST_ASSERT_NOT_NULL(strstr(locked.portal_host, "상시"));
  TEST_ASSERT_NULL(strstr(locked.portal_host, ".local"));
  TEST_ASSERT_NOT_NULL(strstr(locked.portal_code, "잠김"));
  ui::RenderDevice(locked, c, nullptr);
  TEST_ASSERT_TRUE(ui::CheckMarginsClean(c));
  TEST_ASSERT_EQUAL_INT(0, c.missing_glyphs());
}

// 금융위원회 종가가 수신돼도 KIS 현재가에 가려 화면에 안 보이던 문제(2026-11-12) 회귀.
void test_market_row_shows_fsc_close_next_to_live_price() {
  using namespace kdash;
  app::AppState st;
  st.settings = ConfiguredSettings();
  st.settings.watch_count = 1;
  strcpy(st.settings.watchlist[0].symbol, "005930");
  app::Snapshot sn;
  const auto ctx = Ctx("2026-11-12 14:22");
  sn.has_kis[0] = sn.has_fsc[0] = true;
  auto& live = sn.kis[0].quote;
  auto& close = sn.fsc[0].quote;
  live.has_value = close.has_value = true;
  live.validity = close.validity = Validity::Valid;
  live.value.price_krw = 211250;
  close.value.price_krw = 198900;
  live.observed_at = live.value.price_at = SourceTs(ctx.now_utc);
  close.observed_at = close.value.price_at = SourceTs(Kst("2026-11-10 00:00"));
  auto page = vm::BuildMarket(sn, st, ctx.now_utc, true, true, "", false);
  TEST_ASSERT_EQUAL_STRING("211,250", page.rows[0].value);
  TEST_ASSERT_EQUAL_STRING("현재가", page.rows[0].kind);  // 실시간과 종가를 혼동하지 않는다
  TEST_ASSERT_NOT_NULL(strstr(page.rows[0].sub, "종가 198,900"));
  TEST_ASSERT_NOT_NULL(strstr(page.rows[0].sub, "(11-10)"));  // 기준일 명시
  ui::Canvas c;
  ui::RenderMarket(page, c, nullptr);
  TEST_ASSERT_TRUE(ui::CheckMarginsClean(c));
  TEST_ASSERT_EQUAL_INT(0, c.missing_glyphs());
  // 종가가 없으면 기존 표기를 유지한다.
  sn.has_fsc[0] = false;
  page = vm::BuildMarket(sn, st, ctx.now_utc, true, true, "", false);
  TEST_ASSERT_NOT_NULL(strstr(page.rows[0].sub, "조회 · REST"));
  TEST_ASSERT_NULL(strstr(page.rows[0].sub, "종가"));
}

void test_lan_watchlist_row_editor() {
  using namespace kdash;
  using namespace kdash::portal;
  using namespace lantest;
  TestRng rng(61);
  Settings s = FullSettings();
  s.watch_count = 2;
  strcpy(s.watchlist[0].symbol, "005930"); s.watchlist[0].market = Market::Krx;
  strcpy(s.watchlist[1].symbol, "000660"); s.watchlist[1].market = Market::Nxt;
  PortalHandler h(s, rng, 0);
  h.UseLan(Hosts(), 600);
  const std::string cookie = Login(h, 1);
  Request g = Req(Method::Get, "/watch");
  g.cookie = cookie.c_str();
  Response page = h.Handle(g, 2);
  TEST_ASSERT_EQUAL(200, page.status);
  TEST_ASSERT_NOT_NULL(strstr(page.body, "name=\"sym0\""));
  TEST_ASSERT_NOT_NULL(strstr(page.body, "name=\"sym3\""));
  TEST_ASSERT_NOT_NULL(strstr(page.body, "value=\"005930\""));
  TEST_ASSERT_NOT_NULL(strstr(page.body, "삼성전자"));  // 종목명을 같이 보여 준다
  TEST_ASSERT_NOT_NULL(strstr(page.body, "SK하이닉스"));
  auto post = [&](const FormField* f, size_t n) {
    Request r = Req(Method::Post, "/save", f, n);
    r.cookie = cookie.c_str();
    r.csrf = h.Csrf();
    return h.Handle(r, 3);
  };
  // 한 종목 삭제(빈칸) + 한 종목 추가 + 시장 변경.
  FormField edit[] = {{"sec", "watch"}, {"watch_rows", "1"}, {"watch_present", "1"},
                      {"sym0", ""}, {"mkt0", "0"}, {"sym1", "000660"}, {"mkt1", "2"},
                      {"sym2", "035420"}, {"mkt2", "1"}, {"sym3", ""}, {"mkt3", "0"}};
  TEST_ASSERT_EQUAL(200, post(edit, 11).status);
  TEST_ASSERT_TRUE(h.HasPendingSave());
  TEST_ASSERT_EQUAL(2, (int)h.Pending().watch_count);
  TEST_ASSERT_EQUAL_STRING("000660", h.Pending().watchlist[0].symbol);
  TEST_ASSERT_EQUAL((int)Market::Unified, (int)h.Pending().watchlist[0].market);
  TEST_ASSERT_EQUAL_STRING("035420", h.Pending().watchlist[1].symbol);
  TEST_ASSERT_EQUAL((int)Market::Nxt, (int)h.Pending().watchlist[1].market);
  TEST_ASSERT_EQUAL_STRING("TestAP", h.Pending().wifi_ssid);  // 다른 구역은 그대로
  h.CompleteSave(true);
  // 모두 비우면 관심종목 없음. 4개까지 가능.
  FormField none[] = {{"sec", "watch"}, {"watch_rows", "1"}, {"watch_present", "1"}, {"sym0", ""},
                      {"mkt0", "0"}};
  TEST_ASSERT_EQUAL(200, post(none, 5).status);
  TEST_ASSERT_EQUAL(0, (int)h.Pending().watch_count);
  h.CompleteSave(true);
  FormField four[] = {{"sec", "watch"}, {"watch_rows", "1"}, {"sym0", "005930"}, {"mkt0", "0"},
                      {"sym1", "000660"}, {"mkt1", "0"}, {"sym2", "035420"}, {"mkt2", "0"},
                      {"sym3", "005380"}, {"mkt3", "0"}};
  TEST_ASSERT_EQUAL(200, post(four, 10).status);
  TEST_ASSERT_EQUAL(4, (int)h.Pending().watch_count);
  h.CompleteSave(true);
  // 잘못된 코드, 중복, 잘못된 시장은 거부하고 pending을 만들지 않는다.
  struct Bad { const char* sym; const char* mkt; const char* sym1; } bads[] = {
      {"12345", "0", ""}, {"12345a", "0", ""}, {"005930", "0", "005930"}, {"005930", "5", ""},
      {"00593０", "0", ""}};
  for (const Bad& b : bads) {
    FormField f[] = {{"sec", "watch"}, {"watch_rows", "1"}, {"sym0", b.sym}, {"mkt0", b.mkt},
                     {"sym1", b.sym1}, {"mkt1", "0"}};
    Response r = post(f, 6);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(r.body, "관심종목 오류"), b.sym);
    TEST_ASSERT_FALSE_MESSAGE(h.HasPendingSave(), b.sym);
  }
}
