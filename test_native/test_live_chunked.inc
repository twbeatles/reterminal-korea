// Regression for data.go.kr's real HTTP/1.0 -> chunked response (2026-11-11).
namespace {
std::string Chunk(const std::string& body) {
  char size[32]; snprintf(size, sizeof(size), "%zx\r\n", body.size());
  return std::string(size) + body + "\r\n";
}
}

void test_chunked_body_success(void) {
  char out[33];
  AuditBodyStream stream("2;source=api\r\n{\"\r\n5\r\na\":1}\r\n0\r\nX-Trace: done\r\n\r\n");
  stream.keep_open = true;  // zero chunk terminates without waiting for close
  auto r = net::ReadBody(stream, -1, out, 32, 10, true);
  TEST_ASSERT_EQUAL((int)HttpError::None, (int)r.error);
  TEST_ASSERT_EQUAL_STRING("{\"a\":1}", out);
  TEST_ASSERT_EQUAL(7, r.size);
  AuditBodyStream exact("2\r\n{}\r\n0\r\n\r\n");
  r = net::ReadBody(exact, -1, out, 2, 10, true);
  TEST_ASSERT_EQUAL((int)HttpError::None, (int)r.error);
  TEST_ASSERT_EQUAL_STRING("{}", out);
}

void test_chunked_body_rejects_bad_framing(void) {
  char out[33];
  const char* invalid[] = {"", "z\r\nx\r\n0\r\n\r\n", "\r\n", "1\na\r\n0\r\n\r\n",
    "2\r\n{", "2\r\n{}X\n0\r\n\r\n", "2\r\n{}\r\n", "0\r\n",
    "0\r\ninvalid-trailer\r\n\r\n", "0\r\n folded: x\r\n\r\n"};
  for (const char* wire : invalid) {
    AuditBodyStream stream(wire);
    auto r = net::ReadBody(stream, -1, out, 32, 10, true);
    TEST_ASSERT_EQUAL_MESSAGE((int)HttpError::Transport, (int)r.error, wire);
  }
  AuditBodyStream long_header(std::string(256, '1') + "\r\n");
  auto r = net::ReadBody(long_header, -1, out, 32, 10, true);
  TEST_ASSERT_EQUAL((int)HttpError::Transport, (int)r.error);
  AuditBodyStream long_trailer("0\r\nX:" + std::string(254, 'x') + "\r\n\r\n");
  r = net::ReadBody(long_trailer, -1, out, 32, 10, true);
  TEST_ASSERT_EQUAL((int)HttpError::Transport, (int)r.error);
}

void test_chunked_body_limits_and_timeout(void) {
  char out[33];
  AuditBodyStream big("21\r\n");
  auto r = net::ReadBody(big, -1, out, 32, 10, true);
  TEST_ASSERT_EQUAL((int)HttpError::TooLarge, (int)r.error);
  AuditBodyStream accumulated("2\r\n{}\r\n20\r\n");
  r = net::ReadBody(accumulated, -1, out, 32, 10, true);
  TEST_ASSERT_EQUAL((int)HttpError::TooLarge, (int)r.error);
  AuditBodyStream overflow("FFFFFFFFFFFFFFFFF\r\n");
  r = net::ReadBody(overflow, -1, out, 32, 10, true);
  TEST_ASSERT_EQUAL((int)HttpError::TooLarge, (int)r.error);
  AuditBodyStream stalled("2\r\n{"); stalled.keep_open = true;
  stalled.tick = UINT32_MAX - 4;
  r = net::ReadBody(stalled, -1, out, 32, 10, true);
  TEST_ASSERT_EQUAL((int)HttpError::Timeout, (int)r.error);
  AuditBodyStream zero_read("2\r\n{}\r\n0\r\n\r\n"); zero_read.stall_read = true;
  r = net::ReadBody(zero_read, -1, out, 32, 10, true);
  TEST_ASSERT_EQUAL((int)HttpError::Timeout, (int)r.error);
  AuditBodyStream tiny_chunks(std::string(1, '1'));
  tiny_chunks.bytes.clear();
  for (int i = 0; i < 1400; ++i) tiny_chunks.bytes += "1;x=a\r\nx\r\n";
  char bounded[1501];
  r = net::ReadBody(tiny_chunks, -1, bounded, 1500, 10, true);
  TEST_ASSERT_EQUAL((int)HttpError::Transport, (int)r.error); // framing budget
}

void test_captured_live_responses_to_dashboard(void) {
  const char* files[] = {"kma_now", "kma_forecast-72", "air_station"};
  app::Snapshot snap;
  auto state = app::AppState{};
  state.settings = ConfiguredSettings();
  auto ctx = Ctx("2026-11-11 09:54");
  std::vector<char> body(65537);
  for (int i = 0; i < 3; ++i) {
    std::string fixture = ReadFile((std::string("test/fixtures/live_20261111T005356Z-a7479994_") + files[i] + ".json").c_str());
    TEST_ASSERT_FALSE(fixture.empty());
    // Split inside JSON, independent of logical fields or TCP packet boundaries.
    AuditBodyStream wire(Chunk(fixture.substr(0, 17)) + Chunk(fixture.substr(17)) + "0\r\n\r\n");
    auto r = net::ReadBody(wire, -1, body.data(), 65536, 10, true);
    TEST_ASSERT_EQUAL((int)HttpError::None, (int)r.error);
    TEST_ASSERT_EQUAL_STRING(fixture.c_str(), body.data());
    if (i == 0) {
      snap.now = pj::KmaUltraNow(body.data(), r.size, 20261111, 900, ctx, 58, 126);
      TEST_ASSERT_EQUAL((int)Outcome::Ok, (int)snap.now.outcome);
      TEST_ASSERT_EQUAL_FLOAT(8.7f, (float)snap.now.temp_c.value);
      snap.has_now = true;
    } else if (i == 1) {
      snap.fc = pj::KmaVillage(body.data(), r.size, 20261111, 800, ctx, 58, 126);
      TEST_ASSERT_EQUAL((int)Outcome::Ok, (int)snap.fc.outcome);
      TEST_ASSERT_TRUE(snap.fc.tmp.has_value);
      snap.has_fc = true;
    } else {
      snap.air = pj::AirKorea(body.data(), r.size, "양천구", ctx);
      TEST_ASSERT_EQUAL((int)Outcome::Ok, (int)snap.air.outcome);
      TEST_ASSERT_FALSE(snap.air.pm10.has_value);
      TEST_ASSERT_FALSE(snap.air.pm25.has_value); // real station outage, never zero
      snap.has_air = true;
    }
  }
  auto life = vm::BuildLife(snap, state, ctx.now_utc, true, true, false);
  TEST_ASSERT_EQUAL_STRING("8.7°C", life.temp.value);
  TEST_ASSERT_EQUAL_STRING("--", life.pm10.value);
  TEST_ASSERT_EQUAL((int)vm::Badge::None, (int)life.temp.badge);
}

void test_captured_financial_responses(void) {
  auto ctx = Ctx("2026-11-11 10:03");
  const auto fsc = ReadFile("test/fixtures/live_20261111T010054Z-0b65d0c7_fsc_close.json");
  auto close = pj::Fsc(fsc.c_str(), fsc.size(), "005930", "KRX", ctx);
  TEST_ASSERT_EQUAL((int)Outcome::Ok, (int)close.outcome);
  TEST_ASSERT_TRUE(close.quote.has_value);
  TEST_ASSERT_EQUAL_INT64(189900, close.quote.value.price_krw);
  TEST_ASSERT_EQUAL((int)QuoteKind::PriorClose, (int)close.quote.value.kind);
  TEST_ASSERT_EQUAL_INT64(Kst("2026-11-09 00:00"), close.quote.observed_at.epoch_utc);
  const auto body = ReadFile("test/fixtures/live_20261111T010221Z-d22b95e0_exim_date-0.json");
  auto fx = pj::Exim(body.c_str(), body.size(), 20261111, ctx);
  TEST_ASSERT_EQUAL((int)Outcome::Ok, (int)fx.outcome);
  TEST_ASSERT_TRUE(fx.usd.has_value);
  TEST_ASSERT_TRUE(fx.jpy100.has_value);
  TEST_ASSERT_EQUAL_INT64(134330, fx.usd.value.won_scaled);
  TEST_ASSERT_EQUAL_INT64(84644, fx.jpy100.value.won_scaled);
  TEST_ASSERT_EQUAL_STRING("JPY(100)", fx.jpy100.value.currency_unit);
  TEST_ASSERT_EQUAL_INT64(Kst("2026-11-11 00:00"), fx.fixing_date_utc);
}

void test_central_seoul_air_station_label(void) {
  auto ctx = Ctx("2026-11-11 10:15");
  auto raw = ReadFile("test/fixtures/live_20261111T011221Z-a8f4f1e9_air_station.json");
  auto air = pj::AirKorea(raw.c_str(), raw.size(), "중구", ctx);
  TEST_ASSERT_EQUAL((int)Outcome::Ok, (int)air.outcome);
  TEST_ASSERT_TRUE(air.pm10.has_value);
  TEST_ASSERT_TRUE(air.pm25.has_value);
  TEST_ASSERT_EQUAL(34, air.pm10.value);
  TEST_ASSERT_EQUAL(22, air.pm25.value);
  TEST_ASSERT_EQUAL_INT64(Kst("2026-11-11 10:00"), air.data_time_utc);
  app::Snapshot snap; snap.air = air; snap.has_air = true;
  app::AppState state; state.settings = ConfiguredSettings();
  auto life = vm::BuildLife(snap, state, ctx.now_utc, true, true, false);
  TEST_ASSERT_NOT_NULL(strstr(life.pm10.value, "34"));
  TEST_ASSERT_NOT_NULL(strstr(life.pm25.value, "22"));
  TEST_ASSERT_NOT_NULL(strstr(life.air_sub, "중구"));
}
