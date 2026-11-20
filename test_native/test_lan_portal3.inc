// 웹 설정 보완(2026-11-12 사용자 보고) 회귀 테스트: Wi-Fi 검색 흐름, 종목명 실시간 표시(app.js),
// 저장 요약, 여러 줄 상태 화면, 메뉴 요약, 위도·경도 → 격자. test_lan_portal2.inc 뒤에 포함된다.

namespace lantest3 {
using namespace kdash;
using namespace kdash::portal;
using namespace lantest;

struct Fixture {
  TestRng rng;
  Settings s;
  PortalHandler h;
  std::string cookie;
  explicit Fixture(uint32_t seed, const Settings& base)
      : rng(seed), s(base), h(s, rng, 0) {
    h.UseLan(Hosts(), 600);
    cookie = Login(h, 1);
  }
  Response Get(const char* path) {
    Request g = Req(Method::Get, path);
    g.cookie = cookie.c_str();
    return h.Handle(g, 2);
  }
  Response Post(const char* path, const FormField* f, size_t n) {
    Request r = Req(Method::Post, path, f, n);
    r.cookie = cookie.c_str();
    r.csrf = h.Csrf();
    return h.Handle(r, 3);
  }
};

Settings WithWatch() {
  Settings s = FullSettings();
  s.watch_count = 2;
  strcpy(s.watchlist[0].symbol, "005930"); s.watchlist[0].market = Market::Krx;
  strcpy(s.watchlist[1].symbol, "000660"); s.watchlist[1].market = Market::Nxt;
  return s;
}

}  // namespace lantest3

// 보고: "와이파이 스캔이 동작하지 않아(새로고침을 해도)". 검색 후 안내 화면에 Wi-Fi 화면으로 가는 길이 없었고,
// 그 화면을 새로고침하면 검색만 다시 보냈다. 이제 결과 화면으로 자동 이동하고 상태(검색 중/실패/없음)를 보인다.
void test_wifi_scan_flow_redirects_and_reports_state() {
  using namespace kdash::portal;
  lantest3::Fixture f(91, FullSettings());
  Response r = f.Post("/wifi/scan", nullptr, 0);
  TEST_ASSERT_EQUAL(200, r.status);
  TEST_ASSERT_NOT_NULL(strstr(r.body, "http-equiv=refresh"));
  TEST_ASSERT_NOT_NULL(strstr(r.body, "url=/wifi"));
  TEST_ASSERT_NOT_NULL(strstr(r.body, "href=/wifi"));
  TEST_ASSERT_EQUAL((int)PortalAction::Scan, (int)f.h.TakeAction());
  TEST_ASSERT_EQUAL((int)ScanState::Pending, (int)f.h.scan_state());
  TEST_ASSERT_NOT_NULL(strstr(f.Get("/wifi").body, "검색 중"));
  f.h.SetScanResults(nullptr, 0, true);
  TEST_ASSERT_NOT_NULL(strstr(f.Get("/wifi").body, "검색하지 못했습니다"));
  f.h.SetScanResults(nullptr, 0);
  TEST_ASSERT_NOT_NULL(strstr(f.Get("/wifi").body, "찾지 못했습니다"));
  ScanEntry e[1];
  snprintf(e[0].ssid, sizeof(e[0].ssid), "HomeNet");
  e[0].rssi = -45;
  f.h.SetScanResults(e, 1);
  Response page = f.Get("/wifi");
  TEST_ASSERT_NOT_NULL(strstr(page.body, "HomeNet (-45 dBm)"));
  TEST_ASSERT_NULL(strstr(page.body, "찾지 못했습니다"));
  TEST_ASSERT_NULL(strstr(page.body, "검색 중"));
}

// 보고: "종목코드를 수정하면 종목명도 바뀌어야 하는데 고정되어 있다". 이름은 저장된 값으로 한 번만 그렸다.
// 이제 같은 출처 스크립트(app.js)가 입력에 맞춰 이름을 바꾸고, 이름으로 고를 수 있는 목록을 채운다.
void test_watch_editor_live_names_via_app_js() {
  using namespace kdash::portal;
  lantest3::Fixture f(92, lantest3::WithWatch());
  Response page = f.Get("/watch");
  TEST_ASSERT_EQUAL(200, page.status);
  TEST_ASSERT_NOT_NULL(strstr(page.body, "<script src=/app.js"));
  TEST_ASSERT_NOT_NULL(strstr(page.body, "<datalist id=stocks>"));
  for (const char* want : {"data-nm=nm0", "data-nm=nm3", "id=nm0>삼성전자</small>", "id=nm1>SK하이닉스</small>",
                           "id=nm2></small>", "list=stocks"})
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(page.body, want), want);

  // 스크립트: 로그인 없이도 받는다(공개 종목명 표뿐). Host 검사는 그대로.
  Request js = lantest::Req(Method::Get, "/app.js");
  Response r = f.h.Handle(js, 4);
  TEST_ASSERT_EQUAL(200, r.status);
  TEST_ASSERT_NOT_NULL(strstr(r.content_type, "javascript"));
  TEST_ASSERT_NOT_NULL(strstr(r.body, "\"005930\":\"삼성전자\""));
  TEST_ASSERT_NOT_NULL(strstr(r.body, "\"000660\":\"SK하이닉스\""));
  TEST_ASSERT_NOT_NULL(strstr(r.body, "input[data-nm]"));
  TEST_ASSERT_NOT_NULL(strstr(r.body, "getElementById('stocks')"));
  TEST_ASSERT_NOT_NULL(strstr(r.body, "addEventListener('input'"));
  TEST_ASSERT_NULL(strstr(r.body, "password123"));
  TEST_ASSERT_NULL(strstr(r.body, "TestAP"));
  TEST_ASSERT_NULL(strstr(r.body, "</script"));
  // 모든 종목이 들어간다(이름에 따옴표·역슬래시가 없으므로 빠지는 항목 없음).
  size_t entries = 0;
  for (const char* p = r.body; (p = strstr(p, "\":\"")) != nullptr; p++) entries++;
  TEST_ASSERT_EQUAL((int)kdash::stocks::NameCount(), (int)entries);
  js.host = "evil.example";
  TEST_ASSERT_EQUAL(421, f.h.Handle(js, 4).status);

  // AP 포털에서도 같은 스크립트를 준다.
  TestRng rng(93);
  Settings s = FullSettings();
  PortalHandler ap(s, rng, 0);
  Request apjs;
  apjs.path = "/app.js";
  TEST_ASSERT_EQUAL(200, ap.Handle(apjs, 1).status);
  Request apform;
  apform.path = "/";
  Response legacy = ap.Handle(apform, 1);
  TEST_ASSERT_EQUAL(200, legacy.status);  // 스크립트 태그를 더해도 한 페이지 폼이 버퍼 안에 들어간다
  TEST_ASSERT_NOT_NULL(strstr(legacy.body, "<script src=/app.js"));

  // 스크립트 없이도 저장 화면에서 무엇이 저장되는지 종목명으로 확인할 수 있다.
  FormField edit[] = {{"sec", "watch"}, {"watch_rows", "1"}, {"sym0", "035420"}, {"mkt0", "0"},
                      {"sym1", "123456"}, {"mkt1", "1"}};
  Response saved = f.Post("/save", edit, 6);
  TEST_ASSERT_EQUAL(200, saved.status);
  TEST_ASSERT_NOT_NULL(strstr(saved.body, "NAVER(035420·KRX)"));
  TEST_ASSERT_NOT_NULL(strstr(saved.body, "이름 표에 없음(123456·NXT)"));
  TEST_ASSERT_NOT_NULL(strstr(saved.body, "http-equiv=refresh"));
  TEST_ASSERT_TRUE(f.h.HasPendingSave());
}

void test_status_page_multiline_and_menu_summary() {
  using namespace kdash::portal;
  lantest3::Fixture f(94, lantest3::WithWatch());
  f.h.SetStatusLine("FW 1.0.0 · Wi-Fi -42 dBm · USB 전원\n기상청 실황 · 12:00\n<b>x</b>");
  Response st = f.Get("/status");
  TEST_ASSERT_EQUAL(200, st.status);
  TEST_ASSERT_NOT_NULL(strstr(st.body, "USB 전원<br>기상청 실황 · 12:00<br>&lt;b&gt;"));
  TEST_ASSERT_NULL(strstr(st.body, "<b>x"));
  Response menu = f.Get("/");
  TEST_ASSERT_NOT_NULL(strstr(menu.body, "FW 1.0.0 · Wi-Fi -42 dBm · USB 전원"));
  TEST_ASSERT_NULL(strstr(menu.body, "기상청 실황 · 12:00"));  // 메뉴에는 첫 줄만
  // 현재 설정 요약: 이름이 붙은 관심종목, 키는 등록 여부만, 비밀 값은 없다.
  for (const char* want : {"Wi-Fi: TestAP", "삼성전자(005930·KRX)", "SK하이닉스(000660·NXT)",
                           "키: 기상청 등록됨", "자동 순환: 5분 · 시장은 09~16시"})
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(menu.body, want), want);
  for (const char* secret : {"password123", "kis-secret", "kkkkkkkk"})
    TEST_ASSERT_NULL_MESSAGE(strstr(menu.body, secret), secret);
  // SSID는 이스케이프된다.
  Settings odd = FullSettings();
  strcpy(odd.wifi_ssid, "<i>net");
  lantest3::Fixture g(95, odd);
  Response m2 = g.Get("/");
  TEST_ASSERT_NOT_NULL(strstr(m2.body, "&lt;i&gt;net"));
  TEST_ASSERT_NULL(strstr(m2.body, "<i>net"));
}

void test_location_lat_lon_to_grid() {
  using namespace kdash::portal;
  lantest3::Fixture f(96, FullSettings());
  TEST_ASSERT_NOT_NULL(strstr(f.Get("/location").body, "name=lat"));
  FormField seoul[] = {{"sec", "location"}, {"display", "서울 중구"}, {"station", "중구"},
                       {"nx", "1"}, {"ny", "1"}, {"lat", "37.5665"}, {"lon", "126.9780"}};
  Response r = f.Post("/save", seoul, 7);
  TEST_ASSERT_EQUAL(200, r.status);
  TEST_ASSERT_TRUE(f.h.HasPendingSave());
  TEST_ASSERT_EQUAL(60, f.h.Pending().location.grid_x);  // 위경도가 격자 칸보다 우선
  TEST_ASSERT_EQUAL(127, f.h.Pending().location.grid_y);
  TEST_ASSERT_NOT_NULL(strstr(r.body, "격자: 60, 127"));
  f.h.CompleteSave(true);
  FormField busan[] = {{"sec", "location"}, {"lat", "35.1796"}, {"lon", "129.0756"}};
  TEST_ASSERT_EQUAL(200, f.Post("/save", busan, 3).status);
  TEST_ASSERT_EQUAL(98, f.h.Pending().location.grid_x);
  TEST_ASSERT_EQUAL(76, f.h.Pending().location.grid_y);
  f.h.CompleteSave(true);
  // 위경도 없이 격자만 넣는 기존 방식도 그대로.
  FormField grid[] = {{"sec", "location"}, {"nx", "61"}, {"ny", "126"}, {"lat", ""}, {"lon", ""}};
  TEST_ASSERT_EQUAL(200, f.Post("/save", grid, 5).status);
  TEST_ASSERT_EQUAL(61, f.h.Pending().location.grid_x);
  f.h.CompleteSave(true);
  struct Bad { const char* lat; const char* lon; const char* expect; } bads[] = {
      {"37.5", "", "모두"}, {"", "126.9", "모두"}, {"37.5e1", "126.9", "숫자"},
      {"37,5", "126.9", "숫자"}, {"abc", "126.9", "숫자"}, {"0", "0", "한국 범위"},
      {"37.5.1", "126.9", "숫자"}, {"-", "126.9", "숫자"}};
  for (const Bad& b : bads) {
    FormField bf[] = {{"sec", "location"}, {"lat", b.lat}, {"lon", b.lon}};
    Response e = f.Post("/save", bf, 3);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(e.body, b.expect), b.lat);
    TEST_ASSERT_FALSE_MESSAGE(f.h.HasPendingSave(), b.lat);
  }
}

// 보완: 10분 창은 처음 연 시각이 아니라 마지막 활동 기준이다(설정을 고치는 중에 닫히지 않게).
void test_lan_window_extends_with_activity() {
  using namespace kdash;
  using namespace kdash::portal;
  using namespace lantest;
  TestRng rng(97);
  Settings s = FullSettings();
  PortalHandler h(s, rng, 0);
  h.UseLan(Hosts(), 600);
  TEST_ASSERT_EQUAL(600, h.RemainingSec(0));
  const std::string cookie = Login(h, 500);
  Request g = Req(Method::Get, "/keys");
  g.cookie = cookie.c_str();
  TEST_ASSERT_EQUAL(200, h.Handle(g, 590).status);   // 활동
  TEST_ASSERT_FALSE(h.WindowExpired(900));            // 처음 연 지 900초여도 열림
  TEST_ASSERT_EQUAL(290, h.RemainingSec(900));
  TEST_ASSERT_EQUAL(200, h.Handle(g, 1000).status);   // 다시 활동
  TEST_ASSERT_FALSE(h.WindowExpired(1500));
  TEST_ASSERT_TRUE(h.WindowExpired(1601));            // 마지막 활동 뒤 10분 넘게 조용하면 닫힘
  TEST_ASSERT_EQUAL(410, h.Handle(g, 1601).status);
  // 로그인 안 된 요청(로그인 화면)은 창을 늘리지 않는다.
  TestRng rng2(98);
  PortalHandler q(s, rng2, 0);
  q.UseLan(Hosts(), 600);
  TEST_ASSERT_EQUAL(200, q.Handle(Req(Method::Get, "/"), 500).status);
  TEST_ASSERT_TRUE(q.WindowExpired(601));
  // 상시 모드는 만료 없음.
  TestRng rng3(99);
  PortalHandler a(s, rng3, 0);
  a.UseLan(Hosts(), 0);
  TEST_ASSERT_EQUAL(-1, a.RemainingSec(100000));
  TEST_ASSERT_FALSE(a.WindowExpired(100000));
}
