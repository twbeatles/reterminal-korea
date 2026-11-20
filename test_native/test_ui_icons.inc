// Icon runtime, icon view-model mapping and card renderer (ADR 0008).

bool AnyBlackOutside(const ui::Canvas& c, int x, int y, int w, int h) {
  for (int py = 0; py < ui::Canvas::H; py++)
    for (int px = 0; px < ui::Canvas::W; px++)
      if (c.pixel(px, py) && (px < x || py < y || px >= x + w || py >= y + h))
        return true;
  return false;
}

void test_icon_tables_and_draw(void) {
  using ui::IconId;
  using ui::IconSize;
  TEST_ASSERT_EQUAL(24, ui::IconPx(IconSize::S24));
  TEST_ASSERT_EQUAL(64, ui::IconPx(IconSize::L64));
  for (int i = 1; i < (int)IconId::Count; i++) {
    const IconId id = (IconId)i;
    TEST_ASSERT_TRUE(ui::HasIcon(id, IconSize::S24));
    const bool large = id <= IconId::Unknown || id == IconId::Usd || id == IconId::Jpy;
    TEST_ASSERT_EQUAL(large, ui::HasIcon(id, IconSize::L64));
    for (int s = 0; s < 2; s++) {
      const IconSize sz = s ? IconSize::L64 : IconSize::S24;
      if (!ui::HasIcon(id, sz)) continue;
      const int n = ui::IconPx(sz);
      ui::Canvas c;
      ui::DrawIcon(c, id, sz, 100, 100);
      TEST_ASSERT_TRUE(c.black_ratio(100, 100, n, n) > 0.02);
      TEST_ASSERT_FALSE(AnyBlackOutside(c, 100, 100, n, n));
    }
  }
  ui::Canvas c;
  TEST_ASSERT_FALSE(ui::HasIcon(IconId::None, IconSize::S24));
  TEST_ASSERT_FALSE(ui::HasIcon(IconId::Count, IconSize::S24));
  ui::DrawIcon(c, IconId::None, IconSize::S24, 10, 10);
  ui::DrawIcon(c, IconId::Count, IconSize::L64, 10, 10);
  ui::DrawIcon(c, IconId::Wifi, IconSize::L64, 10, 10);  // 24px only
  TEST_ASSERT_EQUAL_UINT32(ui::Canvas().crc32(), c.crc32());
  // Clipping at every edge must neither crash nor wrap.
  ui::DrawIcon(c, IconId::Sun, IconSize::L64, -30, -30);
  ui::DrawIcon(c, IconId::Sun, IconSize::L64, 760, 440);
  TEST_ASSERT_TRUE(c.black_ratio(0, 0, 34, 34) > 0);
  TEST_ASSERT_TRUE(c.black_ratio(760, 440, 40, 40) > 0);
  TEST_ASSERT_EQUAL_FLOAT(0.0, (float)c.black_ratio(34, 34, 726, 406));
}

void test_icon_view_model(void) {
  using ui::IconId;
  // PTY wins over SKY; missing/out-of-range -> Unknown.
  const int skies[] = {1, 3, 4, -1, 9};
  const IconId sky_icon[] = {IconId::Sun, IconId::PartlyCloudy, IconId::Cloud,
                             IconId::Unknown, IconId::Unknown};
  const IconId pty_icon[] = {IconId::None, IconId::Rain, IconId::RainSnow,
                             IconId::Snow, IconId::Rain, IconId::Rain,
                             IconId::RainSnow, IconId::Snow};
  for (int s = 0; s < 5; s++) {
    TEST_ASSERT_EQUAL((int)sky_icon[s], (int)fmt::WeatherIconFor(skies[s], 0));
    TEST_ASSERT_EQUAL((int)sky_icon[s], (int)fmt::WeatherIconFor(skies[s], -1));
    TEST_ASSERT_EQUAL((int)sky_icon[s], (int)fmt::WeatherIconFor(skies[s], 8));
    for (int p = 1; p <= 7; p++)
      TEST_ASSERT_EQUAL((int)pty_icon[p], (int)fmt::WeatherIconFor(skies[s], p));
  }
  // Header date crosses KST midnight with the weekday.
  char b[32];
  fmt::HeaderDateTime(Kst("2026-11-01 23:59"), b, sizeof(b));
  TEST_ASSERT_EQUAL_STRING("11-01(일) 23:59", b);
  fmt::HeaderDateTime(Kst("2026-11-02 00:00"), b, sizeof(b));
  TEST_ASSERT_EQUAL_STRING("11-02(월) 00:00", b);
  // Battery icon tiers: Classify low/critical -> low, else 60% curve split.
  TEST_ASSERT_EQUAL((int)IconId::BatteryCharging, (int)fmt::BatteryIconFor(3300, true));
  TEST_ASSERT_EQUAL((int)IconId::None, (int)fmt::BatteryIconFor(-1, false));
  TEST_ASSERT_EQUAL((int)IconId::BatteryFull, (int)fmt::BatteryIconFor(4100, false));
  TEST_ASSERT_TRUE(!battery::IsLowPower(3600) && battery::PercentMv(3600) < 60);
  TEST_ASSERT_EQUAL((int)IconId::BatteryMid, (int)fmt::BatteryIconFor(3600, false));
  TEST_ASSERT_EQUAL((int)IconId::BatteryLow, (int)fmt::BatteryIconFor(3300, false));

  app::AppState st;
  st.settings = FullSettings();
  app::Snapshot snap;
  const int64_t now = Kst("2026-11-02 14:30");
  demo::FillDemo(snap, now - 600);
  const auto life = vm::BuildLife(snap, st, now, true, true, false);
  TEST_ASSERT_EQUAL_STRING("11-02(월) 14:30", life.header.datetime);
  TEST_ASSERT_EQUAL((int)IconId::Cloud, (int)life.now_icon);  // demo SKY 4, PTY 0
  TEST_ASSERT_NOT_NULL(strstr(life.now_text, "흐림"));
  TEST_ASSERT_NOT_NULL(strstr(life.now_text, "예보"));
  TEST_ASSERT_EQUAL((int)IconId::Humidity, (int)life.humidity.icon);
  TEST_ASSERT_EQUAL((int)IconId::Umbrella, (int)life.rain.icon);
  TEST_ASSERT_EQUAL((int)IconId::Cloud, (int)life.forecast.icon);
  TEST_ASSERT_EQUAL((int)IconId::Air, (int)life.pm10.icon);
  TEST_ASSERT_EQUAL((int)IconId::Home, (int)life.indoor.icon);
  // Forecast card: temperature once (value), slot time + sky in the sub.
  TEST_ASSERT_NOT_NULL(strstr(life.forecast.value, "강수확률"));
  TEST_ASSERT_NULL(strstr(life.forecast.sub, "°C"));
  TEST_ASSERT_NOT_NULL(strstr(life.forecast.sub, "예보"));
  // PM numbers carry their station and measurement time.
  TEST_ASSERT_NOT_NULL(strstr(life.air_sub, "측정소 종로구"));
  TEST_ASSERT_NOT_NULL(strstr(life.air_sub, "측정 14:20"));
  snap.fc.pty.value = 3;
  TEST_ASSERT_EQUAL((int)IconId::Snow,
                    (int)vm::BuildLife(snap, st, now, true, true, false).now_icon);
  snap.has_fc = false;
  TEST_ASSERT_EQUAL((int)IconId::Unknown,
                    (int)vm::BuildLife(snap, st, now, true, true, false).now_icon);

  const auto mkt = vm::BuildMarket(snap, st, now, true, true, "장중", false);
  TEST_ASSERT_EQUAL(2, mkt.fx_count);
  TEST_ASSERT_EQUAL((int)IconId::Usd, (int)mkt.rows[0].icon);
  TEST_ASSERT_EQUAL_STRING("KRW / USD", mkt.rows[0].pair);
  TEST_ASSERT_EQUAL((int)IconId::Jpy, (int)mkt.rows[1].icon);
  TEST_ASSERT_EQUAL_STRING("KRW / 100JPY", mkt.rows[1].pair);
  for (size_t i = mkt.fx_count; i < mkt.n; i++)
    TEST_ASSERT_EQUAL((int)IconId::Chart, (int)mkt.rows[i].icon);

  const auto dev = vm::BuildDevice(st, now, true, false, 3600, "0.4.0", false);
  TEST_ASSERT_EQUAL((int)IconId::WifiOff, (int)dev.wifi_icon);
  TEST_ASSERT_EQUAL((int)IconId::BatteryMid, (int)dev.battery_icon);
  TEST_ASSERT_EQUAL((int)IconId::Wifi,
                    (int)vm::BuildDevice(st, now, true, true, 3700, "0.4.0", true).wifi_icon);
  // Provider rows use the footer names and say why a row is empty.
  TEST_ASSERT_EQUAL_STRING("실내 센서 · 센서 없음", dev.providers[0]);
  TEST_ASSERT_EQUAL_STRING("기상청 실황 · 수신 기록 없음", dev.providers[1]);
  TEST_ASSERT_EQUAL_STRING("한국투자증권 · 설정 필요", dev.providers[6]);
  TEST_ASSERT_EQUAL_STRING("", dev.notice);
  app::AppState st2 = st;
  st2.providers[(int)sched::Provider::KmaNow].last_success_utc = now - 3600;
  st2.providers[(int)sched::Provider::KmaNow].last_attempt_utc = now - 60;
  st2.providers[(int)sched::Provider::KmaNow].last_outcome = Outcome::ProviderError;
  st2.providers[(int)sched::Provider::Exim].auth_blocked = true;
  const auto dev2 = vm::BuildDevice(st2, now, false, true, 3300, "0.4.0", false);
  TEST_ASSERT_EQUAL_STRING("기상청 실황 · 13:30 · 최근 실패", dev2.providers[1]);
  TEST_ASSERT_EQUAL_STRING("수출입은행 · 키 확인 필요", dev2.providers[4]);
  TEST_ASSERT_EQUAL_STRING("시계 시간 확인 필요", dev2.clock);
  // Low battery / safe mode explain why the device page was forced.
  TEST_ASSERT_NOT_NULL(strstr(dev2.battery, "부족"));
  TEST_ASSERT_NOT_NULL(strstr(dev2.notice, "배터리 부족"));
  TEST_ASSERT_EQUAL_STRING("",
      vm::BuildDevice(st2, now, true, true, 3300, "0.4.0", true).notice);  // USB
  TEST_ASSERT_NOT_NULL(strstr(
      vm::BuildDevice(st2, now, true, true, 3900, "0.4.0", false, true).notice,
      "안전 모드"));
}

struct RenderCase {
  const char* name;
  bool demo, nodata, longplace, wifi, stale;
};

void test_card_renderer(void) {
  const RenderCase cases[] = {
      {"day", false, false, false, true, false},
      {"nodata", false, true, false, true, false},
      {"longkr", false, false, true, true, false},
      {"demo", true, false, false, true, false},
      {"nonet-stale", false, false, false, false, true},
  };
  const int64_t now = Kst("2026-11-02 14:30");
  const kdash::Rect L = ui::BodyLeftRect(), R = ui::BodyRightRect();
  for (const auto& rc : cases) {
    app::AppState st;
    st.settings = FullSettings();
    st.settings.watch_count = kMaxWatchlist;
    for (size_t i = 0; i < kMaxWatchlist; i++)
      snprintf(st.settings.watchlist[i].symbol, 7, "%06u", (unsigned)(i + 1));
    if (rc.longplace)
      settings::CopyBounded(st.settings.location.display_name,
                            sizeof(st.settings.location.display_name),
                            "세종특별자치시 어진동 한누리대로");
    app::Snapshot snap;
    if (!rc.nodata) {
      demo::FillDemo(snap, now - 600);
      for (size_t i = 0; i < kMaxWatchlist; i++) {
        snap.has_fsc[i] = true;
        snap.fsc[i].quote.has_value = true;
        snap.fsc[i].quote.validity = Validity::Valid;
        snap.fsc[i].quote.value.price_krw = 78500 + (int64_t)i;
        snap.fsc[i].quote.value.price_at.epoch_utc = now - 86400;
      }
    }
    if (rc.stale) snap.now.temp_c.validity = Validity::Stale;
    ui::Canvas c;
    const auto life = vm::BuildLife(snap, st, now, true, rc.wifi, rc.demo);
    ui::RenderLife(life, c);
    TEST_ASSERT_TRUE_MESSAGE(ui::CheckMarginsClean(c), rc.name);
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, c.missing_glyphs(), rc.name);
    // Hero weather icon (64px) left of the big temperature.
    TEST_ASSERT_TRUE_MESSAGE(c.black_ratio(L.x, L.y + 38, 64, 64) > 0.02, rc.name);
    // Header Wi-Fi icon slot is always drawn (on or off).
    TEST_ASSERT_TRUE_MESSAGE(c.black_ratio(756, 14, 24, 24) > 0.02, rc.name);
    if (rc.stale)
      TEST_ASSERT_TRUE(ui::CheckBadgePresent(c, L.x + L.w / 2, L.y, L.w / 2, 26));

    const auto mkt = vm::BuildMarket(snap, st, now, true, rc.wifi, "장중", rc.demo);
    ui::RenderMarket(mkt, c);
    TEST_ASSERT_TRUE_MESSAGE(ui::CheckMarginsClean(c), rc.name);
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, c.missing_glyphs(), rc.name);
    if (!rc.nodata) {
      TEST_ASSERT_TRUE(c.black_ratio(L.x, L.y + 38, 64, 64) > 0.02);  // $ icon
      TEST_ASSERT_TRUE(c.black_ratio(R.x, R.y, 24, 24) > 0.02);       // chart head
    }

    const auto dev = vm::BuildDevice(st, now, true, rc.wifi, 3600, "0.4.0", false);
    ui::RenderDevice(dev, c);
    TEST_ASSERT_TRUE_MESSAGE(ui::CheckMarginsClean(c), rc.name);
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, c.missing_glyphs(), rc.name);
    // Low battery + safe mode banners stay inside the left column, all glyphs.
    for (int mode = 0; mode < 2; mode++) {
      const auto warn = vm::BuildDevice(st, now, false, rc.wifi,
                                        mode ? 3900 : 3300, "0.4.0", false, mode == 1);
      TEST_ASSERT_TRUE(warn.notice[0] != 0);
      ui::RenderDevice(warn, c);
      TEST_ASSERT_TRUE_MESSAGE(ui::CheckMarginsClean(c), rc.name);
      TEST_ASSERT_EQUAL_INT_MESSAGE(0, c.missing_glyphs(), rc.name);
    }
  }
  // The fallback counter itself: an unknown codepoint is counted, clear resets.
  ui::Canvas c;
  c.draw_text(ui::DefaultMedium(), 40, 40, "\xEE\x80\x80");  // U+E000 (PUA)
  TEST_ASSERT_EQUAL_INT(1, c.missing_glyphs());
  c.clear();
  TEST_ASSERT_EQUAL_INT(0, c.missing_glyphs());
}
