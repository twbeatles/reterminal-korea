#include "../src/storage/CalendarStore.h"

void test_per_provider_cap_defers_without_failure() {
  // KIS cap is 6: token + 4 quotes + reissue token = 6, so the retried 4th
  // quote is deferred (never sent) and must NOT be recorded as a failure.
  app::AppState st;
  st.settings = settings::DefaultSettings();
  st.settings.providers.kis_enabled = true;
  strcpy(st.settings.providers.kis_app_key, "FAKEAPPKEY0000000");
  strcpy(st.settings.providers.kis_app_secret, "FAKEAPPSECRET000000");
  for (size_t i = 0; i < kMaxWatchlist; i++) {
    strcpy(st.settings.watchlist[i].symbol, "005930");
    st.settings.watch_count++;
  }
  app::Snapshot snap;
  app::KisSession kis;
  FakeHttp http;
  http.On("oauth2/tokenP", 200, "kis_token_ok.json");
  http.On("inquire-price", 200, "kis_ok.json");
  http.On("inquire-price", 200, "kis_ok.json");
  http.On("inquire-price", 200, "kis_ok.json");
  http.On("inquire-price", 200, "kis_token_expired.json");
  http.On("inquire-price", 200, "kis_ok.json");
  app::PassLog log;
  // Monday 10:00 KST: market open, only KisPrice due.
  app::RunFetchPass(st, snap, kis, http, net::kCaBundlePem,
                    Ctx("2026-11-02 10:00"), true, 0, &log);
  TEST_ASSERT_EQUAL(1, log.count);
  TEST_ASSERT_EQUAL(6, log.requests);
  TEST_ASSERT_EQUAL(4, http.Count("inquire-price"));  // retry never sent
  TEST_ASSERT_EQUAL(2, http.Count("oauth2/tokenP"));
  TEST_ASSERT_TRUE(snap.has_kis[0] && snap.has_kis[1] && snap.has_kis[2]);
  TEST_ASSERT_FALSE(snap.has_kis[3]);
  // Deferred, not failed: provider state untouched, still due next pass.
  TEST_ASSERT_EQUAL_INT64(
      0, st.providers[(int)Provider::KisPrice].last_attempt_utc);
  TEST_ASSERT_EQUAL(0, st.providers[(int)Provider::KisPrice].attempts);
  TEST_ASSERT_EQUAL((int)Outcome::Ok,
                    (int)st.providers[(int)Provider::KisPrice].last_outcome);
  TEST_ASSERT_EQUAL_INT64(
      0, st.providers[(int)Provider::KisPrice].retry_after_utc);
}

void test_demo_values_carry_demo_marker() {
  // F-01: every demo value source carries DEMO so demo data can never be
  // mistaken for live provider data, including after a codec round-trip.
  app::Snapshot snap;
  demo::FillDemo(snap, 1700000000LL);
  const char* sources[] = {
      snap.now.temp_c.source, snap.now.humidity.source, snap.now.rain_mm.source,
      snap.fc.tmp.source, snap.fc.pop.source, snap.air.pm10.source,
      snap.air.pm25.source, snap.fx.usd.source, snap.fx.jpy100.source};
  for (const char* s : sources) {
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_NOT_NULL(strstr(s, "DEMO"));
  }
  uint8_t buf[3072];
  size_t n = snapshot_codec::EncodeSnapshot(snap, buf, sizeof(buf));
  TEST_ASSERT_TRUE(n > 0);
  app::Snapshot back;
  TEST_ASSERT_TRUE(snapshot_codec::DecodeSnapshot(buf, n, back));
  TEST_ASSERT_EQUAL_STRING("기상청(DEMO)", back.now.temp_c.source);
  TEST_ASSERT_EQUAL_STRING("에어코리아(DEMO)", back.air.pm10.source);
  TEST_ASSERT_EQUAL_STRING("한국수출입은행(DEMO)", back.fx.usd.source);
}

void test_fx_scale_is_bounded() {
  char out[64];
  fmt::FxRate(12345,2,out,sizeof(out));
  TEST_ASSERT_EQUAL_STRING("123.45",out);
  fmt::FxRate(12345,INT32_MAX,out,sizeof(out));
  TEST_ASSERT_EQUAL_STRING("--",out);
  fmt::FxRate(12345,-1,out,sizeof(out));
  TEST_ASSERT_EQUAL_STRING("--",out);
}

void test_low_battery_precedes_user_actions() {
  app::AppState st;
  st.settings=FullSettings();
  power::CycleInput in;
  in.st=&st; in.battery_mv=3200; in.clock_verified=true;
  in.next_due_utc=0;
  for (uint8_t key : {uint8_t(1),uint8_t(2),uint8_t(4)}) {
    in.reason=power::CycleWake::Button; in.button_mask=key;
    in.long_press=key==1;
    const auto plan=power::PlanCycle(in);
    TEST_ASSERT_TRUE(plan.low_battery_mode);
    TEST_ASSERT_FALSE(plan.connect_wifi || plan.sync_clock || plan.enter_portal);
    TEST_ASSERT_EQUAL_INT64(power::kMaxSleepLowSec,plan.sleep_seconds);
  }
  st.settings.wifi_ssid[0]='\0';
  in.reason=power::CycleWake::FirstBoot;
  TEST_ASSERT_FALSE(power::PlanCycle(in).enter_portal);
  in.usb=true;
  TEST_ASSERT_TRUE(power::PlanCycle(in).enter_portal);
}

void test_local_due_does_not_connect_radio() {
  app::AppState st;
  st.sensor_present=true;
  const int64_t now=Kst("2026-11-02 12:00");
  TEST_ASSERT_TRUE(app::NextWakeUtc(st,now,true)<=now);
  TEST_ASSERT_EQUAL_INT64(INT64_MAX,app::NextNetworkWakeUtc(st,now,true));
  power::CycleInput in;
  in.st=&st; in.clock_verified=true; in.now_utc=now;
  in.next_due_utc=app::NextNetworkWakeUtc(st,now,true);
  TEST_ASSERT_FALSE(power::PlanCycle(in).connect_wifi);
  st.settings=ConfiguredSettings();
  app::MarkResult(st,Provider::AirKorea,Outcome::AuthError,now-60,true,-1,0);  // 하루 뒤 재시도 대기
  st.providers[(int)Provider::AirKorea].last_attempt_utc=0;
  app::MarkNetworkUnavailable(st,now,true);
  TEST_ASSERT_EQUAL((int)Outcome::ProviderError,(int)st.providers[(int)Provider::KmaNow].last_outcome);
  TEST_ASSERT_TRUE(st.providers[(int)Provider::KmaNow].retry_after_utc>now);
  TEST_ASSERT_EQUAL_INT64(0,st.providers[(int)Provider::Sht40].last_attempt_utc);
  TEST_ASSERT_EQUAL_INT64(0,st.providers[(int)Provider::AirKorea].last_attempt_utc);
}

void test_calendar_rollover_and_exchange_provenance() {
  cal::YearTable tables[2];
  cal::Clear(tables[0],2026); cal::Clear(tables[1],2027);
  tables[1].loaded=true; tables[1].fetched_utc=123;
  cal::SetHoliday(tables[1],1,1);
  cal::AlignYears(tables,2027);
  TEST_ASSERT_TRUE(tables[0].loaded);
  TEST_ASSERT_EQUAL_INT64(123,tables[0].fetched_utc);
  TEST_ASSERT_TRUE(cal::IsHoliday(tables[0],1,1));
  TEST_ASSERT_EQUAL(2028,tables[1].year);
  TEST_ASSERT_FALSE(tables[1].loaded);
  const int64_t weekday=Kst("2027-01-04 10:00");
  TEST_ASSERT_EQUAL_STRING("정규장 시간",cal::MarketStateText(weekday,true,tables,2));
  TEST_ASSERT_EQUAL_STRING("정규장 외",cal::MarketStateText(Kst("2027-01-04 08:59"),true,tables,2));
  TEST_ASSERT_EQUAL_STRING("정규장 외",cal::MarketStateText(Kst("2027-01-04 15:30"),true,tables,2));
  TEST_ASSERT_EQUAL_STRING("시간 미확인",cal::MarketStateText(weekday,false,tables,2));
  TEST_ASSERT_EQUAL_STRING("일정 미확인",cal::MarketStateText(weekday,true,nullptr,0));
  TEST_ASSERT_EQUAL_STRING("휴장",cal::MarketStateText(Kst("2027-01-02 10:00"),true,nullptr,0));
  cal::SetMarketClosed(tables[0],1,5);
  TEST_ASSERT_EQUAL_STRING("휴장",cal::MarketStateText(Kst("2027-01-05 10:00"),true,tables,2));
  TEST_ASSERT_EQUAL_STRING("휴장",cal::MarketStateText(Kst("2027-01-01 10:00"),true,tables,2));
  tables[0].market_verified=true;
  TEST_ASSERT_EQUAL_STRING("장시작 전",cal::MarketStateText(Kst("2027-01-04 08:59"),true,tables,2));
  TEST_ASSERT_EQUAL_STRING("장중",cal::MarketStateText(Kst("2027-01-04 09:00"),true,tables,2));
  TEST_ASSERT_EQUAL_STRING("장마감",cal::MarketStateText(Kst("2027-01-04 15:30"),true,tables,2));
  TEST_ASSERT_EQUAL_STRING("장중",cal::MarketStateText(weekday,true,tables,2));
  FakeStorage storage;
  TEST_ASSERT_TRUE(cal::SaveTables(storage,tables,weekday));
  cal::YearTable restored[2];
  TEST_ASSERT_TRUE(cal::LoadTables(storage,restored));
  TEST_ASSERT_TRUE(restored[0].market_verified);
  TEST_ASSERT_TRUE(cal::IsHoliday(restored[0],1,1));
  // Reported weekday regression: no exchange table must not imply a closure.
  app::AppState st;
  cal::Clear(st.calendars[0],2026);
  st.calendars[0].loaded=true;
  const int64_t now=Kst("2026-11-17 10:00");
  const auto page=vm::BuildMarket(app::Snapshot{},st,now,true,true,
      cal::MarketStateText(now,true,st.calendars,2),false);
  TEST_ASSERT_EQUAL_STRING("정규장 시간",page.state);  // no UTF-8 truncation
  const auto calendar=vm::BuildCalendar(st,now,true,true,false);
  TEST_ASSERT_FALSE(calendar.flags[17] & vm::kCalClosed);
  TEST_ASSERT_FALSE(calendar.flags[17] & vm::kCalHoliday);
}

void test_fetch_deadline_and_forced_refresh_gates() {
  app::AppState st;
  st.settings=ConfiguredSettings();
  app::Snapshot snap; app::KisSession kis; FakeHttp http;
  http.On("getUltraSrtNcst",200,"kma_ultra_ok.json");
  struct Gate : app::IFetchControl {
    const FakeHttp& http;
    explicit Gate(const FakeHttp& h):http(h){}
    bool CanRequest() const override { return http.urls.size()<1; }
  } gate(http);
  app::PassLog log;
  const auto ctx=Ctx("2026-11-01 14:50");  // matches the fixture's source issue
  app::RunFetchPass(st,snap,kis,http,net::kCaBundlePem,ctx,true,0,&log,&gate);
  TEST_ASSERT_EQUAL(1,log.requests);
  TEST_ASSERT_TRUE(snap.has_now);
  TEST_ASSERT_EQUAL_INT64(0,st.providers[(int)Provider::AirKorea].last_attempt_utc);
  app::MarkResult(st,Provider::AirKorea,Outcome::AuthError,ctx.now_utc,true,-1,0);
  app::MarkResult(st,Provider::Exim,Outcome::QuotaExceeded,ctx.now_utc,true,-1,0);
  Provider due[(int)Provider::Count];
  const int n=app::DueProviders(st,ctx.now_utc,true,due,(int)Provider::Count,true);
  for (int i=0;i<n;i++) {
    TEST_ASSERT_NOT_EQUAL((int)Provider::AirKorea,(int)due[i]);
    TEST_ASSERT_NOT_EQUAL((int)Provider::Exim,(int)due[i]);
  }
}

void test_72h_reboot_keeps_release_budget() {
  app::AppState st;
  st.settings=ConfiguredSettings();
  st.settings.providers.air_key[0]=st.settings.providers.exim_key[0]='\0';
  st.providers[(int)Provider::KmaForecast].auth_blocked=true;
  st.time_verified=true;
  app::Snapshot snap; app::KisSession kis;
  struct ReleasedHttp : FakeHttp {
    std::string body;
    HttpResponse get(const char* url, const char* const*, size_t, const char*) override {
      urls.push_back(url);
      int date = 0, time = 0;
      const char* d = strstr(url, "base_date=");
      const char* t = strstr(url, "base_time=");
      if (d) sscanf(d, "base_date=%d", &date);
      if (t) sscanf(t, "base_time=%d", &time);
      JsonDocument doc;
      deserializeJson(doc, ReadFile("test/fixtures/kma_ultra_ok.json"));
      char ds[16], ts[8];
      snprintf(ds, sizeof(ds), "%08d", date); snprintf(ts, sizeof(ts), "%04d", time);
      for (JsonObject row : doc["response"]["body"]["items"]["item"].as<JsonArray>()) {
        row["baseDate"] = ds; row["baseTime"] = ts;
      }
      body.clear(); serializeJson(doc, body);
      HttpResponse r; r.status=200; r.body=body.c_str(); r.body_len=body.size(); return r;
    }
  } http;
  FakeStorage storage; RuntimeStore persisted;
  const int64_t start=Kst("2026-11-02 00:40");
  for (int tick=0;tick<72*12;tick++) {
    const int64_t now=start+tick*300;
    FetchContext ctx; ctx.now_utc=ctx.fetched_utc=now; ctx.clock_verified=true;
    app::PassLog log;
    app::RunFetchPass(st,snap,kis,http,net::kCaBundlePem,ctx,true,0,&log);
    persisted.Checkpoint(storage,st,snap,now);
    if (tick && tick%(12*12)==0) {
      app::AppState reboot; reboot.settings=st.settings;
      app::Snapshot recovered; RuntimeStore reader;
      TEST_ASSERT_TRUE(reader.Restore(storage,reboot.settings,reboot,recovered));
      reboot.time_verified=true;
      st=reboot; snap=recovered; persisted=reader;
    }
  }
  TEST_ASSERT_EQUAL(72,http.Count("getUltraSrtNcst"));
  TEST_ASSERT_TRUE(snap.has_now);
  TEST_ASSERT_TRUE(storage.write_count<=2*48*4);
}
