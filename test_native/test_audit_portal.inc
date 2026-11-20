// Portal regression tests, included after test_stage2.cpp.
#include "../src/provision/PortalServer.h"

void test_portal_full_settings_and_clear(void) {
  TestRng rng(73);
  kdash::Settings s = FullSettings();
  kdash::portal::PortalHandler handler(s, rng, 0);
  kdash::portal::FormField fields[] = {
      {"ssid", "NewAP"}, {"secret", ""}, {"display", "부산"},
      {"station", "광복동"}, {"nx", "98"}, {"ny", "76"},
      {"clear_air", "1"}, {"kis_key", "new-app-key"}, {"kis_secret", ""},
      {"form", "1"}, {"kis", "1"}, {"watch_present", "1"}, {"watchlist", ""}};
  kdash::portal::Request req;
  req.method = kdash::portal::Method::Post; req.path = "/save";
  req.csrf = handler.Csrf(); req.fields = fields;
  req.n_fields = sizeof(fields) / sizeof(fields[0]);
  handler.Handle(req, 1);
  TEST_ASSERT_TRUE(handler.HasPendingSave());
  TEST_ASSERT_EQUAL_STRING("부산", handler.Pending().location.display_name);
  TEST_ASSERT_EQUAL(98, handler.Pending().location.grid_x);
  TEST_ASSERT_EQUAL(76, handler.Pending().location.grid_y);
  TEST_ASSERT_EQUAL_STRING("", handler.Pending().providers.air_key);
  TEST_ASSERT_EQUAL_STRING("password123", handler.Pending().wifi_secret);
  TEST_ASSERT_EQUAL_STRING("kis-secret", handler.Pending().providers.kis_app_secret);
  TEST_ASSERT_FALSE(handler.Pending().providers.fsc_enabled);
  TEST_ASSERT_TRUE(handler.Pending().providers.kis_enabled);
  TEST_ASSERT_EQUAL(0, handler.Pending().watch_count);
  handler.CompleteSave(false);
  TEST_ASSERT_FALSE(handler.HasPendingSave());
  kdash::portal::Request get;
  get.path = "/status";
  TEST_ASSERT_NOT_NULL(strstr(handler.Handle(get, 2).body, "실패"));
  get.path = "/";
  auto rendered = handler.Handle(get, 2);
  TEST_ASSERT_NOT_NULL(strstr(rendered.body, "name=\"secret\""));
  TEST_ASSERT_NULL(strstr(rendered.body, "password123"));
  TEST_ASSERT_NULL(strstr(rendered.body, "kis-secret"));
  kdash::portal::Request close;
  close.method = kdash::portal::Method::Post; close.path = "/close";
  close.csrf = handler.Csrf();
  TEST_ASSERT_EQUAL(200, handler.Handle(close, 3).status);
  TEST_ASSERT_TRUE(handler.Closed());
  TEST_ASSERT_EQUAL(410, handler.Handle(get, 4).status);
}

void test_portal_bounded_headers(void) {
  using kdash::portal::CheckPortalHeaders;
  const char* valid = "POST /save HTTP/1.1\r\nHost: 192.168.4.1\r\nContent-Length: 10\r\nContent-Type: application/x-www-form-urlencoded\r\n\r\n";
  TEST_ASSERT_EQUAL(200, CheckPortalHeaders(valid, strlen(valid)));
  TEST_ASSERT_EQUAL(0, CheckPortalHeaders(valid, 30));
  const char* too_big = "POST /save HTTP/1.1\r\nContent-Length: 8193\r\nContent-Type: application/x-www-form-urlencoded\r\n\r\n";
  TEST_ASSERT_EQUAL(413, CheckPortalHeaders(too_big, strlen(too_big)));
  const char* duplicate = "POST /save HTTP/1.1\r\nContent-Length: 1\r\nContent-Length: 1\r\n\r\n";
  TEST_ASSERT_EQUAL(400, CheckPortalHeaders(duplicate, strlen(duplicate)));
  const char* chunked = "POST /save HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\n";
  TEST_ASSERT_EQUAL(400, CheckPortalHeaders(chunked, strlen(chunked)));
  const char* multipart = "POST /save HTTP/1.1\r\nContent-Length: 1\r\nContent-Type: multipart/form-data\r\n\r\n";
  TEST_ASSERT_EQUAL(415, CheckPortalHeaders(multipart, strlen(multipart)));
  const char* query = "GET /?secret=x HTTP/1.1\r\n\r\n";
  TEST_ASSERT_EQUAL(400, CheckPortalHeaders(query, strlen(query)));
}

void test_portal_verified_settings_commit(void) {
  FakeStorage disk;
  kdash::Settings current = FullSettings();
  kdash::Settings candidate = current;
  kdash::settings::CopyBounded(candidate.wifi_ssid, sizeof(candidate.wifi_ssid), "UpdatedAP");
  TEST_ASSERT_TRUE(kdash::portal::SaveSettingsVerified(disk, current, candidate));
  TEST_ASSERT_EQUAL_STRING("UpdatedAP", current.wifi_ssid);
  kdash::settings::CopyBounded(candidate.wifi_ssid, sizeof(candidate.wifi_ssid), "FailedAP");
  disk.fail_on_write = disk.write_count + 1;
  disk.fail_bytes = 4; disk.fail_return = true;
  TEST_ASSERT_FALSE(kdash::portal::SaveSettingsVerified(disk, current, candidate));
  TEST_ASSERT_EQUAL_STRING("UpdatedAP", current.wifi_ssid);
  disk.fail_on_write = -1;
  kdash::portal::FormField f[] = {{"ssid", "A"}, {"display", ""}};
  TestRng rng(4); kdash::portal::PortalHandler handler(current, rng, 0);
  kdash::portal::Request r;
  r.method = kdash::portal::Method::Post; r.path = "/save"; r.csrf = handler.Csrf();
  r.fields = f; r.n_fields = 2;
  handler.Handle(r, 1);
  TEST_ASSERT_FALSE(handler.HasPendingSave());
}
