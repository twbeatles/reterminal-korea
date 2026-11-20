// 주변 Wi-Fi 검색 목록, 설정 백업 내보내기/가져오기 회귀 테스트 (ADR 0024). test_lan_portal.inc 뒤에 포함된다.
#include "../src/provision/SettingsBackup.h"

void test_lan_wifi_scan_picker() {
  using namespace kdash;
  using namespace kdash::portal;
  using namespace lantest;
  TestRng rng(71);
  Settings s = FullSettings();
  PortalHandler h(s, rng, 0);
  h.UseLan(Hosts(), 600);
  const std::string cookie = Login(h, 1);
  auto get = [&](const char* path) {
    Request g = Req(Method::Get, path);
    g.cookie = cookie.c_str();
    return h.Handle(g, 2);
  };
  auto post = [&](const char* path, const FormField* f, size_t n) {
    Request r = Req(Method::Post, path, f, n);
    r.cookie = cookie.c_str();
    r.csrf = h.Csrf();
    return h.Handle(r, 3);
  };
  Response page = get("/wifi");
  TEST_ASSERT_NOT_NULL(strstr(page.body, "action=/wifi/scan"));
  TEST_ASSERT_NULL(strstr(page.body, "ssid_pick"));  // 검색 전에는 목록이 없다

  // 검색 요청은 POST + CSRF + 세션이 필요하고, 어댑터가 가져갈 동작으로 남는다.
  TEST_ASSERT_EQUAL((int)PortalAction::None, (int)h.TakeAction());
  Request nocsrf = Req(Method::Post, "/wifi/scan");
  nocsrf.cookie = cookie.c_str();
  TEST_ASSERT_EQUAL(403, h.Handle(nocsrf, 2).status);
  TEST_ASSERT_EQUAL((int)PortalAction::None, (int)h.TakeAction());
  TEST_ASSERT_EQUAL(200, post("/wifi/scan", nullptr, 0).status);
  TEST_ASSERT_EQUAL((int)PortalAction::Scan, (int)h.TakeAction());

  // 결과: 중복·빈 이름 제거, 15개 제한, 이름은 이스케이프.
  ScanEntry e[20];
  snprintf(e[0].ssid, sizeof(e[0].ssid), "HomeNet"); e[0].rssi = -40;
  snprintf(e[1].ssid, sizeof(e[1].ssid), "<script>x</script>"); e[1].rssi = -50; e[1].open = true;
  snprintf(e[2].ssid, sizeof(e[2].ssid), "HomeNet"); e[2].rssi = -60;  // 중복
  e[3].rssi = -61;                                                       // 숨김(빈 이름)
  for (int i = 4; i < 20; i++) {
    snprintf(e[i].ssid, sizeof(e[i].ssid), "Net%02d", i);
    e[i].rssi = (int8_t)(-70 - i);
  }
  h.SetScanResults(e, 20);
  page = get("/wifi");
  TEST_ASSERT_NOT_NULL(strstr(page.body, "name=ssid_pick"));
  TEST_ASSERT_NOT_NULL(strstr(page.body, "HomeNet (-40 dBm)"));
  TEST_ASSERT_NOT_NULL(strstr(page.body, "&lt;script&gt;"));
  TEST_ASSERT_NULL(strstr(page.body, "<script>"));
  TEST_ASSERT_NOT_NULL(strstr(page.body, ", 개방"));
  int options = 0;
  for (const char* p = page.body; (p = strstr(p, "<option value=")) != nullptr; p++) options++;
  TEST_ASSERT_EQUAL(1 + 15, options);  // 직접 입력 + 최대 15개
  TEST_ASSERT_NULL(strstr(page.body, "password123"));

  // 목록에서 고르면(직접 입력이 비었을 때) 서버가 가진 이름으로 저장된다. 비밀번호 빈칸은 유지.
  FormField pick[] = {{"sec", "wifi"}, {"ssid", ""}, {"ssid_pick", "0"}, {"secret", ""}};
  TEST_ASSERT_EQUAL(200, post("/save", pick, 4).status);
  TEST_ASSERT_TRUE(h.HasPendingSave());
  TEST_ASSERT_EQUAL_STRING("HomeNet", h.Pending().wifi_ssid);
  TEST_ASSERT_EQUAL_STRING("password123", h.Pending().wifi_secret);
  TEST_ASSERT_TRUE(h.PendingWifiChanged());
  h.CompleteSave(false);
  // 직접 입력이 우선.
  FormField both[] = {{"sec", "wifi"}, {"ssid", "Typed"}, {"ssid_pick", "0"}};
  TEST_ASSERT_EQUAL(200, post("/save", both, 3).status);
  TEST_ASSERT_EQUAL_STRING("Typed", h.Pending().wifi_ssid);
  h.CompleteSave(false);
  // 범위 밖/숫자 아님은 거부.
  for (const char* bad : {"15", "99", "abc", "-1"}) {
    FormField f[] = {{"sec", "wifi"}, {"ssid", ""}, {"ssid_pick", bad}};
    Response r = post("/save", f, 3);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(r.body, "SSID 오류"), bad);
    TEST_ASSERT_FALSE_MESSAGE(h.HasPendingSave(), bad);
  }
  FormField none[] = {{"sec", "wifi"}, {"ssid", ""}, {"ssid_pick", ""}};
  TEST_ASSERT_NOT_NULL(strstr(post("/save", none, 3).body, "SSID 오류"));
  TEST_ASSERT_FALSE(h.HasPendingSave());
  // 검색을 다시 해서 빈 결과가 오면 목록이 사라진다(이전 번호로 저장 불가).
  h.SetScanResults(nullptr, 0);
  FormField stale[] = {{"sec", "wifi"}, {"ssid", ""}, {"ssid_pick", "0"}};
  TEST_ASSERT_NOT_NULL(strstr(post("/save", stale, 3).body, "SSID 오류"));
}

void test_backup_export_import_roundtrip_and_rejects() {
  using namespace kdash;
  using namespace kdash::portal;
  using namespace lantest;
  Settings a = FullSettings();
  a.watch_count = 2;
  strcpy(a.watchlist[0].symbol, "005930"); a.watchlist[0].market = Market::Krx;
  strcpy(a.watchlist[1].symbol, "000660"); a.watchlist[1].market = Market::Nxt;
  a.device.rotate_minutes = 12;
  a.device.market_from_hour = 10;
  a.device.market_to_hour = 15;
  a.device.lan_mode = LanPortalMode::Always;
  a.display_page = 1;
  a.providers.kis_enabled = true;

  char json[2048];
  const size_t n = ExportBackup(a, json, sizeof(json));
  TEST_ASSERT_TRUE(n > 0 && n < kBackupMax);
  // 비밀은 어디에도 없다: Wi-Fi, 키, 시크릿 값과 이름 모두.
  for (const char* secret : {"TestAP", "password123", "kis-secret", "kkkkkkkk", "ssid", "secret", "wifi"})
    TEST_ASSERT_NULL_MESSAGE(strstr(json, secret), secret);
  TEST_ASSERT_NOT_NULL(strstr(json, "kdash-backup"));
  TEST_ASSERT_NOT_NULL(strstr(json, "000660:NXT"));
  TEST_ASSERT_EQUAL(0, (int)ExportBackup(a, json, 20));  // 버퍼가 모자라면 0
  ExportBackup(a, json, sizeof(json));

  // 다른 기기 설정에 가져오면 비밀 아닌 항목만 바뀌고 Wi-Fi/키는 그대로다.
  Settings b = FullSettings();
  strcpy(b.wifi_ssid, "OtherAP");
  strcpy(b.providers.kma_key, "other-kma-key");
  b.watch_count = 0;
  b.device = DeviceConfig();
  TestRng rng(81);
  PortalHandler h(b, rng, 0);
  h.UseLan(Hosts(), 600);
  const std::string cookie = Login(h, 1);
  auto imp = [&](const char* text) {
    FormField f[] = {{"backup", text}};
    Request r = Req(Method::Post, "/import", f, 1);
    r.cookie = cookie.c_str();
    r.csrf = h.Csrf();
    return h.Handle(r, 2);
  };
  Response ok = imp(json);
  TEST_ASSERT_EQUAL(200, ok.status);
  TEST_ASSERT_TRUE(h.HasPendingSave());
  const Settings& p = h.Pending();
  TEST_ASSERT_EQUAL(2, (int)p.watch_count);
  TEST_ASSERT_EQUAL_STRING("000660", p.watchlist[1].symbol);
  TEST_ASSERT_EQUAL((int)Market::Nxt, (int)p.watchlist[1].market);
  TEST_ASSERT_EQUAL(12, p.device.rotate_minutes);
  TEST_ASSERT_EQUAL(10, p.device.market_from_hour);
  TEST_ASSERT_EQUAL((int)LanPortalMode::Always, (int)p.device.lan_mode);
  TEST_ASSERT_EQUAL(1, p.display_page);
  TEST_ASSERT_TRUE(p.providers.kis_enabled);
  TEST_ASSERT_EQUAL_STRING("OtherAP", p.wifi_ssid);
  TEST_ASSERT_EQUAL_STRING("other-kma-key", p.providers.kma_key);
  TEST_ASSERT_EQUAL_STRING(b.providers.kis_app_secret, p.providers.kis_app_secret);
  TEST_ASSERT_FALSE(h.PendingWifiChanged());  // 접속 시험 없이 저장
  h.CompleteSave(true);

  // 잘못된 입력은 아무 것도 적용하지 않고, 오류에는 값이 아니라 필드 이름만 나온다.
  struct Case { const char* text; const char* expect; } bad[] = {
      {"not json", "JSON 형식"},
      {"[1,2]", "JSON 형식"},
      {"{\"format\":\"other\",\"version\":1}", "형식 또는 버전"},
      {"{\"format\":\"kdash-backup\",\"version\":2}", "형식 또는 버전"},
      {"{\"format\":\"kdash-backup\",\"version\":1,\"device\":{\"rotate_minutes\":0}}", "device"},
      {"{\"format\":\"kdash-backup\",\"version\":1,\"device\":{\"rotate_minutes\":\"5\"}}", "device"},
      {"{\"format\":\"kdash-backup\",\"version\":1,\"device\":{\"market_from_hour\":16,\"market_to_hour\":16}}", "device"},
      {"{\"format\":\"kdash-backup\",\"version\":1,\"location\":{\"nx\":999}}", "location.nx"},
      {"{\"format\":\"kdash-backup\",\"version\":1,\"location\":{\"nx\":60.5}}", "location.nx"},
      {"{\"format\":\"kdash-backup\",\"version\":1,\"watchlist\":[\"12345:KRX\"]}", "watchlist"},
      {"{\"format\":\"kdash-backup\",\"version\":1,\"watchlist\":[\"005930:KRX\",\"005930:NXT\"]}", "watchlist"},
      {"{\"format\":\"kdash-backup\",\"version\":1,\"watchlist\":[\"005930\",\"000660\",\"035420\",\"005380\",\"000270\"]}", "watchlist"},
      {"{\"format\":\"kdash-backup\",\"version\":1,\"providers\":{\"fsc\":\"yes\"}}", "providers"},
  };
  for (const Case& c : bad) {
    Response r = imp(c.text);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(r.body, c.expect), c.text);
    TEST_ASSERT_FALSE_MESSAGE(h.HasPendingSave(), c.text);
  }
  // 일부 구역만 담긴 백업은 그 구역만 바꾼다. 비밀 이름이 들어 있어도 무시된다(적용 안 함).
  const char* partial =
      "{\"format\":\"kdash-backup\",\"version\":1,\"device\":{\"rotate_minutes\":3},"
      "\"wifi_ssid\":\"Evil\",\"kma_key\":\"stolen\"}";
  TEST_ASSERT_EQUAL(200, imp(partial).status);
  TEST_ASSERT_EQUAL(3, h.Pending().device.rotate_minutes);
  TEST_ASSERT_EQUAL(10, h.Pending().device.market_from_hour);  // 가져온 값 유지
  TEST_ASSERT_EQUAL_STRING("OtherAP", h.Pending().wifi_ssid);
  TEST_ASSERT_EQUAL_STRING("other-kma-key", h.Pending().providers.kma_key);
  h.CompleteSave(true);
  // 크기 상한: 1536바이트를 넘으면 413. 세션 없이는 가져올 수 없다.
  std::string big(kBackupMax + 1, 'x');
  TEST_ASSERT_EQUAL(413, imp(big.c_str()).status);
  FormField f[] = {{"backup", json}};
  Request nosession = Req(Method::Post, "/import", f, 1);
  nosession.csrf = h.Csrf();
  TEST_ASSERT_EQUAL(401, h.Handle(nosession, 3).status);
  // 가져오기 화면은 세션이 있을 때만 열린다.
  Request g = Req(Method::Get, "/backup");
  g.cookie = cookie.c_str();
  TEST_ASSERT_NOT_NULL(strstr(h.Handle(g, 3).body, "name=backup"));
  Request g2 = Req(Method::Get, "/backup");
  TEST_ASSERT_EQUAL(401, h.Handle(g2, 3).status);
}
