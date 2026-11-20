// 2026-11-12 감사(PROJECT_AUDIT.md ISSUE-007~010, G-1~G-6) 수정 회귀. ADR 0026.
#include "../src/app/CalendarRefresh.h"
#include "../src/app/PageRotation.h"
#include "../src/app/SymbolBatch.h"
#include "../src/power/RtcPersistence.h"
#include "../src/provision/LanPolicy.h"
#include "../src/provision/PortalServer.h"
#include "../src/storage/RuntimeStore.h"

// ISSUE-007: Wi-Fi가 끊겨도 창 만료/닫힘이 먼저다. 배터리 블로킹은 절대 상한·저전압으로 끝난다.
void test_lan_gate_closes_even_when_disconnected() {
  using portal::LanGate;
  TEST_ASSERT_EQUAL((int)LanGate::Close, (int)portal::DecideLanGate(true, false, false));
  TEST_ASSERT_EQUAL((int)LanGate::Close, (int)portal::DecideLanGate(false, true, false));
  TEST_ASSERT_EQUAL((int)LanGate::WaitLink, (int)portal::DecideLanGate(false, false, false));
  TEST_ASSERT_EQUAL((int)LanGate::Serve, (int)portal::DecideLanGate(false, false, true));
  // 실제 핸들러: 요청이 한 번도 없어도(연결이 끊겨 못 옴) 601초 뒤 만료로 판정된다.
  Settings s = settings::DefaultSettings();
  FakeRng rng(7);
  portal::PortalHandler h(s, rng, 0);
  portal::AllowedHosts hosts;
  strcpy(hosts.ip, "192.168.0.23");
  h.UseLan(hosts, portal::kWindowSec);
  h.Tick(10);
  TEST_ASSERT_FALSE(h.WindowExpired(600));
  TEST_ASSERT_TRUE(h.WindowExpired(601));
  TEST_ASSERT_EQUAL((int)LanGate::Close,
                    (int)portal::DecideLanGate(h.WindowExpired(601), h.Closed(), false));
  // 배터리 블로킹 상한.
  TEST_ASSERT_FALSE(portal::LanBlockingShouldExit(portal::kLanBlockingMaxMs - 1, -1));
  TEST_ASSERT_TRUE(portal::LanBlockingShouldExit(portal::kLanBlockingMaxMs, -1));
  TEST_ASSERT_FALSE(portal::LanBlockingShouldExit(1000, 4000));
  TEST_ASSERT_TRUE(portal::LanBlockingShouldExit(1000, battery::kLowMv - 1));
}

// ISSUE-008(a): 데이터 지문은 기기 설정·시작 화면을 보지 않는다. 데이터 의미를 바꾸는 값은 본다.
void test_data_fingerprint_ignores_device_settings() {
  const Settings a = FullSettings();
  Settings b = a;
  b.device.rotate_minutes = 17; b.device.lan_mode = LanPortalMode::Always;
  b.device.auto_rotate = !a.device.auto_rotate; b.device.refresh_scale_pct = 250;
  b.schedule.refresh_scale_pct = 250; b.display_page = 2;
  const uint32_t fa = settings_codec::DataFingerprint(a);
  TEST_ASSERT_NOT_EQUAL(0u, fa);
  TEST_ASSERT_EQUAL_UINT32(fa, settings_codec::DataFingerprint(b));
  Settings c = a; c.location.grid_x = a.location.grid_x + 1;
  TEST_ASSERT_NOT_EQUAL(fa, settings_codec::DataFingerprint(c));
  Settings d = a; strcpy(d.watchlist[0].symbol, "000660");
  TEST_ASSERT_NOT_EQUAL(fa, settings_codec::DataFingerprint(d));
  Settings e = a; e.providers.kma_key[0] = 'Z';
  TEST_ASSERT_NOT_EQUAL(fa, settings_codec::DataFingerprint(e));
  // RTC 바인딩도 같은 지문을 쓴다.
  TEST_ASSERT_EQUAL_UINT32(fa, power::SettingsFingerprint(b));
}

// ISSUE-008(a): 기기 설정만 바뀐 설정으로도 체크포인트가 복원된다. 위치가 바뀌면 거부된다.
void test_runtime_restore_survives_device_only_change() {
  FakeStorage storage; RuntimeStore store;
  app::AppState st; st.settings = FullSettings(); st.time_verified = true;
  app::Snapshot sn; sn.has_kospi = true; sn.kospi.quote.has_value = true;
  sn.kospi.quote.validity = Validity::Valid; strcpy(sn.kospi.quote.value.name, "KOSPI");
  sn.kospi.quote.value.value_x100 = 261543; sn.kospi.quote.source = "한국투자증권";
  const int64_t now = Kst("2026-11-02 16:00");
  st.last_fetch_utc = now;
  st.providers[(int)Provider::KisPrice].last_success_utc = now;
  TEST_ASSERT_TRUE(store.Checkpoint(storage, st, sn, now, true));
  app::AppState dev; dev.settings = st.settings; dev.settings.device.rotate_minutes = 9;
  app::Snapshot r1; RuntimeStore reader1;
  TEST_ASSERT_TRUE(reader1.Restore(storage, dev.settings, dev, r1));
  TEST_ASSERT_TRUE(r1.has_kospi);
  TEST_ASSERT_EQUAL_INT64(now, dev.providers[(int)Provider::KisPrice].last_success_utc);
  app::AppState moved; moved.settings = st.settings; moved.settings.location.grid_y += 1;
  app::Snapshot r2; RuntimeStore reader2;
  TEST_ASSERT_FALSE(reader2.Restore(storage, moved.settings, moved, r2));
}

// ISSUE-008(a): 저장이 데이터를 바꿨는지 알려 준다(바뀌지 않았으면 캐시를 지우지 않는다).
void test_save_reports_whether_data_changed() {
  FakeStorage storage;
  Settings current = FullSettings();
  Settings pending = current; pending.device.rotate_minutes = 11;
  TEST_ASSERT_TRUE(portal::SaveSettingsVerified(storage, current, pending));
  TEST_ASSERT_FALSE(portal::LastSaveChangedData());
  TEST_ASSERT_EQUAL(11, current.device.rotate_minutes);
  Settings moved = current; moved.location.grid_x += 1;
  TEST_ASSERT_TRUE(portal::SaveSettingsVerified(storage, current, moved));
  TEST_ASSERT_TRUE(portal::LastSaveChangedData());
}

// ISSUE-008(b): 이력 없는 KIS도 마감 후 1회 받되, 마감 이후 재시도 예산을 다 쓰면 다음 장까지 멈춘다.
void test_kis_catchup_bounded_per_close() {
  app::AppState st; SetupKisOnly(st, 1);
  int64_t t = Kst("2026-11-02 16:00");
  TEST_ASSERT_EQUAL_INT64(t, app::NextAttemptUtc(st, Provider::KisPrice, t, true));
  int guard = 0;
  do {
    app::MarkResult(st, Provider::KisPrice, Outcome::ProviderError, t, true, -1, 0);
    t += 60;
  } while (st.providers[(int)Provider::KisPrice].attempts != 0 && ++guard < 20);
  TEST_ASSERT_TRUE(guard < 20);
  // 예산 소진 뒤에는 장외 무호출(다음 거래일 09:00).
  TEST_ASSERT_EQUAL_INT64(Kst("2026-11-03 09:00"),
                          app::NextAttemptUtc(st, Provider::KisPrice, Kst("2026-11-02 23:00"), true));
  // 다음 날 마감 뒤에는 다시 1회 대상이 된다.
  TEST_ASSERT_EQUAL_INT64(Kst("2026-11-03 15:40"),
                          app::NextAttemptUtc(st, Provider::KisPrice, Kst("2026-11-03 15:40"), true));
}

// ISSUE-009: 종목 하나가 실패해도 나머지 종목과 코스피를 받는다. 결과는 성공 + 일부 실패 표시.
void test_kis_partial_failure_keeps_other_symbols_and_index() {
  app::AppState st; SetupKisOnly(st, 3);
  strcpy(st.settings.watchlist[1].symbol, "999999");
  app::Snapshot snap; app::KisSession kis; FakeHttp http; app::PassLog log;
  http.On("oauth2/tokenP", 200, "kis_token_ok.json");
  http.On("inquire-index-price", 200, "kis_index_ok.json");
  http.On("FID_INPUT_ISCD=999999", 200, "kis_zero_price_synthetic.json");
  http.On("inquire-price", 200, "kis_ok.json");
  app::RunFetchPass(st, snap, kis, http, net::kCaBundlePem, Ctx("2026-11-02 10:00"), true, 0, &log);
  TEST_ASSERT_EQUAL(1, http.Count("FID_INPUT_ISCD=999999"));
  TEST_ASSERT_EQUAL(3, http.Count("inquire-price"));
  TEST_ASSERT_TRUE(snap.has_kis[0]);
  TEST_ASSERT_FALSE(snap.has_kis[1]);
  TEST_ASSERT_TRUE(snap.has_kis[2]);
  TEST_ASSERT_TRUE(snap.has_kospi);
  const auto& ps = st.providers[(int)Provider::KisPrice];
  TEST_ASSERT_EQUAL((int)Outcome::Ok, (int)ps.last_outcome);
  TEST_ASSERT_EQUAL_UINT8(0x02, st.symbol_fail_mask[(int)Provider::KisPrice]);
  // 기기 탭: "종목 2 실패"를 1부터 센 번호로 보여 준다.
  const int64_t now = Kst("2026-11-02 10:00");
  vm::DevicePage dev = vm::BuildDevice(st, now, true, true, -1, "test", true);
  TEST_ASSERT_NOT_NULL(strstr(dev.providers[(int)Provider::KisPrice], "종목 2 실패"));
  // 첫 종목이 호출 제한이면 즉시 멈추고 지수도 요청하지 않는다(보호 오류는 기존대로).
  app::AppState st2; SetupKisOnly(st2, 3);
  app::Snapshot snap2; app::KisSession kis2; FakeHttp http2; app::PassLog log2;
  http2.On("oauth2/tokenP", 200, "kis_token_ok.json");
  http2.On("inquire-price", 500, "kis_rate_limited.json");
  app::RunFetchPass(st2, snap2, kis2, http2, net::kCaBundlePem, Ctx("2026-11-02 10:00"), true, 0, &log2);
  TEST_ASSERT_EQUAL(1, http2.Count("inquire-price"));
  TEST_ASSERT_EQUAL(0, http2.Count("inquire-index-price"));
  TEST_ASSERT_EQUAL((int)Outcome::RateLimited, (int)st2.providers[(int)Provider::KisPrice].last_outcome);
}

// G-6: FSC도 같은 규칙. 앞 종목이 자료 없음이어도 뒤 종목을 받고 공급자 성공(마지막 성공 갱신).
void test_fsc_partial_failure_records_success() {
  app::AppState st; st.settings = ConfiguredSettings();
  st.settings.providers.fsc_enabled = true;
  st.settings.providers.kis_enabled = false;
  strcpy(st.settings.watchlist[0].symbol, "000660");
  strcpy(st.settings.watchlist[1].symbol, "005930");
  st.settings.watch_count = 2;
  app::Snapshot snap; app::KisSession kis; FakeHttp http; app::PassLog log;
  http.On("likeSrtnCd=000660", 200, "fsc_nodata.json");
  http.On("likeSrtnCd=005930", 200, "fsc_ok.json");
  const FetchContext ctx = Ctx("2026-11-03 14:00");
  for (int i = 0; i < (int)Provider::Count; i++)  // FSC만 보이도록 나머지는 막는다
    if ((Provider)i != Provider::FscClose) st.providers[i].retry_after_utc = INT64_MAX / 2;
  app::RunFetchPass(st, snap, kis, http, net::kCaBundlePem, ctx, true, 0, &log);
  TEST_ASSERT_EQUAL(2, http.Count("likeSrtnCd="));
  TEST_ASSERT_FALSE(snap.has_fsc[0]);
  TEST_ASSERT_TRUE(snap.has_fsc[1]);
  const auto& ps = st.providers[(int)Provider::FscClose];
  TEST_ASSERT_EQUAL((int)Outcome::Ok, (int)ps.last_outcome);
  TEST_ASSERT_EQUAL_INT64(ctx.now_utc, ps.last_success_utc);
  TEST_ASSERT_EQUAL_UINT8(0x01, st.symbol_fail_mask[(int)Provider::FscClose]);
}

// 집계 규칙 단위.
void test_symbol_batch_rules() {
  app::SymbolBatch b;
  TEST_ASSERT_FALSE(b.Add(0, Outcome::NoData, false));
  TEST_ASSERT_FALSE(b.Add(1, Outcome::Ok, false));
  TEST_ASSERT_EQUAL((int)Outcome::Ok, (int)b.Result());
  TEST_ASSERT_EQUAL_UINT8(0x01, b.fail_mask);
  app::SymbolBatch all;
  all.Add(0, Outcome::ProviderError, false); all.Add(1, Outcome::NoData, false);
  TEST_ASSERT_EQUAL((int)Outcome::ProviderError, (int)all.Result());
  app::SymbolBatch stop;
  stop.Add(0, Outcome::Ok, false);
  TEST_ASSERT_TRUE(stop.Add(1, Outcome::QuotaExceeded, false));
  TEST_ASSERT_TRUE(stop.stopped);
  TEST_ASSERT_EQUAL((int)Outcome::QuotaExceeded, (int)stop.Result());
  TEST_ASSERT_EQUAL_UINT8(0, stop.fail_mask);
  app::SymbolBatch net;
  TEST_ASSERT_TRUE(net.Add(0, Outcome::Ok, true));  // transport failure never counts as Ok
  TEST_ASSERT_EQUAL((int)Outcome::ProviderError, (int)net.Result());
  app::SymbolBatch empty;
  TEST_ASSERT_EQUAL((int)Outcome::Ok, (int)empty.Result());
}

// G-1: 인증 오류는 하루 1회 재시도, 성공하면 차단 해제. 수동 새로고침은 여전히 부르지 않는다.
void test_auth_block_retries_daily_and_success_clears() {
  app::AppState st; st.settings = ConfiguredSettings();
  const int64_t t0 = Kst("2026-11-02 10:00");
  app::MarkResult(st, Provider::Exim, Outcome::AuthError, t0, true, -1, 0);
  TEST_ASSERT_TRUE(st.providers[(int)Provider::Exim].auth_blocked);
  TEST_ASSERT_EQUAL_INT64(t0 + app::kAuthRetrySec,
                          app::NextAttemptUtc(st, Provider::Exim, t0 + 3600, true));
  Provider due[(int)Provider::Count];
  const int n = app::DueProviders(st, t0 + 3600, true, due, (int)Provider::Count, true);
  for (int i = 0; i < n; i++) TEST_ASSERT_NOT_EQUAL((int)Provider::Exim, (int)due[i]);
  app::MarkResult(st, Provider::Exim, Outcome::Ok, t0 + app::kAuthRetrySec, true, -1, 0);
  TEST_ASSERT_FALSE(st.providers[(int)Provider::Exim].auth_blocked);
  // ClearAuthBlocks(같은 키 재저장)는 차단만 풀고 다른 백오프(할당량)는 유지한다.
  app::MarkResult(st, Provider::AirKorea, Outcome::QuotaExceeded, t0, true, -1, 0);
  app::MarkResult(st, Provider::KmaNow, Outcome::AuthError, t0, true, -1, 0);
  app::ClearAuthBlocks(st);
  TEST_ASSERT_FALSE(st.providers[(int)Provider::KmaNow].auth_blocked);
  TEST_ASSERT_EQUAL_INT64(0, st.providers[(int)Provider::KmaNow].retry_after_utc);
  TEST_ASSERT_EQUAL_INT64(Kst("2026-11-03 00:10"), st.providers[(int)Provider::AirKorea].retry_after_utc);
}

// G-2/G-3: 장 시간이라도 거래일이 아니면 시장 화면을 순환에 넣지 않는다. 포털 사용 중엔 바꾸지 않는다.
void test_rotation_respects_trading_day_and_portal() {
  DeviceConfig d;  // 기본: 자동 순환, 장 시간만, 9~16시, 5분
  app::RotationInput in;
  in.device = &d; in.usb = true; in.kst_hour = 10; in.now_ms = 10u * 60u * 1000u;
  in.last_change_ms = 0; in.page = ui::Page::Life;
  in.trading_day = true;
  TEST_ASSERT_TRUE(app::DecideRotation(in).change);
  in.trading_day = false;  // 토요일 10시
  app::RotationDecision r = app::DecideRotation(in);
  TEST_ASSERT_TRUE(r.due); TEST_ASSERT_FALSE(r.change);
  in.page = ui::Page::Market;  // 시장 화면이면 생활로 돌아온다
  r = app::DecideRotation(in);
  TEST_ASSERT_TRUE(r.change); TEST_ASSERT_EQUAL((int)ui::Page::Life, (int)r.next);
  in.trading_day = true; in.portal_open = true;
  TEST_ASSERT_FALSE(app::DecideRotation(in).change);
}

// G-4: 유지 감시가 시작한 연결 시도는 시간 제한 안에서 "진행 중"이다(블로킹 연결이 다시 시작하지 않음).
void test_wifi_attempt_in_flight() {
  net::WifiSupervisor sup;
  TEST_ASSERT_FALSE(sup.AttemptInFlight(0));
  TEST_ASSERT_EQUAL((int)net::WifiAction::Begin, (int)sup.Tick(1000, false, true, true));
  TEST_ASSERT_TRUE(sup.AttemptInFlight(1000 + net::kWifiAttemptTimeoutMs - 1));
  TEST_ASSERT_FALSE(sup.AttemptInFlight(1000 + net::kWifiAttemptTimeoutMs));
  sup.Tick(2000, true, true, true);  // 연결됨
  TEST_ASSERT_FALSE(sup.AttemptInFlight(2001));
  sup.NoteBlockingFailure(3000);
  TEST_ASSERT_FALSE(sup.AttemptInFlight(3001));
}

// ISSUE-011 / G-9: /wifi 폼의 ssid 칸에 기존 SSID가 기본 채워져 있어도 ssid_pick 선택이 적용되고,
// 개방형(open) Wi-Fi를 고르면 이전 비밀번호를 자동으로 비운다.
void test_wifi_scan_picker_overrides_prefilled_ssid_and_clears_open_secret() {
  using namespace kdash::portal;
  using namespace lantest;
  TestRng rng(91);
  Settings s = FullSettings();  // s.wifi_ssid == "TestAP", s.wifi_secret == "password123"
  PortalHandler h(s, rng, 0);
  h.UseLan(Hosts(), 600);
  const std::string cookie = Login(h, 1);
  ScanEntry entries[2];
  snprintf(entries[0].ssid, sizeof(entries[0].ssid), "NewWPA2");
  entries[0].rssi = -42;
  entries[0].open = false;
  snprintf(entries[1].ssid, sizeof(entries[1].ssid), "OpenCafe");
  entries[1].rssi = -55;
  entries[1].open = true;
  h.SetScanResults(entries, 2);

  // 1) 브라우저 폼 기본값처럼 ssid="TestAP"(기존 SSID)가 함께 넘어와도 ssid_pick="0"이 선택된다.
  FormField pick_wpa[] = {
      {"sec", "wifi"}, {"ssid", "TestAP"}, {"ssid_pick", "0"}, {"secret", "FAKE_NEW_PASS"}};
  Request r1 = Req(Method::Post, "/save", pick_wpa, 4);
  r1.cookie = cookie.c_str();
  r1.csrf = h.Csrf();
  TEST_ASSERT_EQUAL(200, h.Handle(r1, 2).status);
  TEST_ASSERT_TRUE(h.HasPendingSave());
  TEST_ASSERT_TRUE(h.PendingWifiChanged());
  TEST_ASSERT_EQUAL_STRING("NewWPA2", h.Pending().wifi_ssid);
  TEST_ASSERT_EQUAL_STRING("FAKE_NEW_PASS", h.Pending().wifi_secret);
  h.CompleteSave(false);

  // 2) 개방형 AP(ssid_pick="1")를 고르고 비밀번호 칸을 비워 두면 이전 WPA 비밀번호가 지워진다.
  FormField pick_open[] = {
      {"sec", "wifi"}, {"ssid", "TestAP"}, {"ssid_pick", "1"}, {"secret", ""}};
  Request r2 = Req(Method::Post, "/save", pick_open, 4);
  r2.cookie = cookie.c_str();
  r2.csrf = h.Csrf();
  TEST_ASSERT_EQUAL(200, h.Handle(r2, 3).status);
  TEST_ASSERT_TRUE(h.HasPendingSave());
  TEST_ASSERT_EQUAL_STRING("OpenCafe", h.Pending().wifi_ssid);
  TEST_ASSERT_EQUAL_STRING("", h.Pending().wifi_secret);
  h.CompleteSave(false);
}

// ISSUE-012: 공휴일 달력(RefreshCalendar)도 인증 오류 시 하루 1회 다시 시도하고,
// ClearAuthBlocks(포털 설정 재저장) 시 즉시 차단이 풀린다.
void test_calendar_auth_error_retries_daily_and_clears_on_save() {
  app::ResetCalendarRetry();
  app::AppState st;
  st.settings = ConfiguredSettings();
  FakeStorage storage;
  FakeHttp http;
  http.On("getRestDeInfo", 403, "kma_error_key.json");
  struct Unlimited : app::IFetchControl {
    bool CanRequest() const override { return true; }
  } budget;

  const FetchContext t0 = Ctx("2026-11-02 10:00");
  app::RefreshCalendar(st, http, storage, t0, budget);
  TEST_ASSERT_EQUAL(2, http.Count("getRestDeInfo"));  // 2026, 2027 각 1회

  // 1시간 뒤에는 재시도하지 않는다(24시간 백오프).
  FetchContext t1h = t0;
  t1h.now_utc += 3601;
  app::RefreshCalendar(st, http, storage, t1h, budget);
  TEST_ASSERT_EQUAL(2, http.Count("getRestDeInfo"));

  // 24시간 뒤에는 자동으로 다시 시도한다.
  FetchContext t24h = t0;
  t24h.now_utc += 86400;
  app::RefreshCalendar(st, http, storage, t24h, budget);
  TEST_ASSERT_EQUAL(4, http.Count("getRestDeInfo"));

  // 포털에서 같은 키를 재저장해 ClearAuthBlocks가 호출되면 24시간 전이라도 즉시 재시도한다.
  app::ClearAuthBlocks(st);
  FetchContext t_save = t24h;
  t_save.now_utc += 10;
  app::RefreshCalendar(st, http, storage, t_save, budget);
  TEST_ASSERT_EQUAL(6, http.Count("getRestDeInfo"));
  app::ResetCalendarRetry();
}

// ISSUE-013: 캐시된 KIS 토큰이 만료 응답을 받아 재발급된 뒤 4종목을 모두 성공적으로 받으면,
// 공급자 캡(6회) 도달로 코스피 지수를 생략하더라도 MarkResult(KisPrice, Ok)가 정상 기록된다.
void test_kis_4_symbols_with_token_reissue_records_mark_result() {
  app::AppState st;
  SetupKisOnly(st, 4);
  app::Snapshot snap;
  app::KisSession kis;
  const FetchContext ctx = Ctx("2026-11-02 10:00");
  strcpy(kis.token, "CACHED_TOKEN_STILL_UNEXPIRED");
  kis.expiry_utc = ctx.now_utc + 3600;

  FakeHttp http;
  http.On("inquire-price", 500, "kis_token_expired.json");  // 1회: 첫 종목 토큰 거부
  http.On("oauth2/tokenP", 200, "kis_token_ok.json");       // 2회: 토큰 재발급
  http.On("inquire-price", 200, "kis_ok.json");             // 3~6회: 4종목 모두 성공
  http.On("inquire-index-price", 200, "kis_index_ok.json");

  app::PassLog log;
  app::RunFetchPass(st, snap, kis, http, net::kCaBundlePem, ctx, true, 0, &log);
  TEST_ASSERT_EQUAL(6, log.requests);
  for (int i = 0; i < 4; ++i) TEST_ASSERT_TRUE(snap.has_kis[i]);
  const auto& ps = st.providers[(int)Provider::KisPrice];
  TEST_ASSERT_EQUAL((int)Outcome::Ok, (int)ps.last_outcome);
  TEST_ASSERT_EQUAL_INT64(ctx.now_utc, ps.last_success_utc);
}

// G-8: 다종목 중 일부 종목만 실패했을 때 이전 값을 가진 해당 종목 행은 LastGood 배지를 표시하고,
// KIS/FSC 모두 미설정이면서 값이 없으면 NeedSetup 배지를 표시한다.
void test_market_partial_symbol_failure_and_unconfigured_badges() {
  const int64_t now = Kst("2026-11-02 10:00");
  app::AppState st;
  SetupKisOnly(st, 2);
  app::Snapshot snap;
  for (int i = 0; i < 2; ++i) {
    snap.has_kis[i] = true;
    snap.kis[i].quote.has_value = true;
    snap.kis[i].quote.validity = Validity::Valid;
    snap.kis[i].quote.value.price_krw = 70000 + i * 1000;
    snap.kis[i].quote.value.price_at = SourceTs(now - 60);
    snap.kis[i].quote.observed_at = SourceTs(now - 60);
  }
  app::MarkResult(st, Provider::KisPrice, Outcome::Ok, now, true, -1, 0);
  st.symbol_fail_mask[(int)Provider::KisPrice] = 0x02;  // 2번째 종목만 이번 패스에 실패

  vm::MarketPage mkt = vm::BuildMarket(snap, st, now, true, true, "장중", false);
  TEST_ASSERT_EQUAL(2, mkt.n);
  TEST_ASSERT_EQUAL((int)vm::Badge::None, (int)mkt.rows[0].badge);
  TEST_ASSERT_EQUAL((int)vm::Badge::LastGood, (int)mkt.rows[1].badge);

  // KIS/FSC 모두 미설정이고 시세도 없을 때는 NeedSetup 배지.
  app::AppState uncfg;
  uncfg.settings = settings::DefaultSettings();
  uncfg.settings.providers.kis_enabled = false;
  uncfg.settings.providers.fsc_enabled = false;
  uncfg.settings.watch_count = 1;
  strcpy(uncfg.settings.watchlist[0].symbol, "005930");
  app::Snapshot empty_snap;
  vm::MarketPage mkt2 = vm::BuildMarket(empty_snap, uncfg, now, true, true, "장중", false);
  TEST_ASSERT_EQUAL(1, mkt2.n);
  TEST_ASSERT_EQUAL((int)vm::Badge::NeedSetup, (int)mkt2.rows[0].badge);
}
