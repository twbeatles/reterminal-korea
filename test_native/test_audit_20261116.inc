// Regression cases for PROJECT_AUDIT ISSUE-015..018 and G-1..G-5.
void test_trusted_clock_never_reapplies_or_rewrites_rtc() {
  struct PreciseClock : AuditClockPlatform {
    int applies = 0;
    int64_t system_ms = 1790000060789LL;
    bool TrustedNtpUtc(int64_t& e) override { e=system_ms/1000; return true; }
    bool ApplyUtc(int64_t e) override { ++applies; system_ms=e*1000; return true; }
  } p;
  const int64_t before=p.system_ms;
  for (int i=0;i<120;i++) {
    TEST_ASSERT_TRUE(clock::BootstrapClock(p,1767225600LL,false).verified);
    p.system_ms+=30000;
  }
  TEST_ASSERT_EQUAL_INT64(before+120*30000,p.system_ms);
  TEST_ASSERT_EQUAL(0,p.applies);
  TEST_ASSERT_EQUAL(0,p.writes);
}

void test_sleep_restores_charge_and_preserves_other_bits() {
  ChargerMock m; charge::Protection p;
  ChargeSamples(p,m,4100);
  TEST_ASSERT_EQUAL_HEX8(0x0A,m.bytes[1]);
  const uint8_t timer=m.bytes[5];
  // USB still attached: do not override hysteresis.
  TEST_ASSERT_TRUE(charge::PrepareForSleep(m));
  TEST_ASSERT_EQUAL_HEX8(0x0A,m.bytes[1]);
  m.bytes[10]=0;
  TEST_ASSERT_TRUE(charge::PrepareForSleep(m));
  TEST_ASSERT_EQUAL_HEX8(0x1A,m.bytes[1]);
  TEST_ASSERT_EQUAL_HEX8(timer,m.bytes[5]);
  m.bytes[1]=0x4A;  // WD_RST command must not be replayed
  TEST_ASSERT_TRUE(charge::PrepareForSleep(m));
  TEST_ASSERT_EQUAL_HEX8(0x1A,m.bytes[1]);
}

void test_sleep_charge_restore_failures_are_detected() {
  for (int reg : {1,10,11}) {
    ChargerMock m; m.bytes[10]=0; m.fail_read=reg;
    TEST_ASSERT_FALSE(charge::PrepareForSleep(m));
    TEST_ASSERT_EQUAL(0,m.writes);
  }
  for (bool ignored : {false,true}) {
    ChargerMock m; m.bytes[1]=0x0A; m.bytes[10]=0;
    m.ignore_write=ignored; m.fail_write=!ignored;
    TEST_ASSERT_FALSE(charge::PrepareForSleep(m));
  }
  ChargerMock m; m.bytes[11]=0x41;
  TEST_ASSERT_TRUE(charge::PrepareForSleep(m));
  TEST_ASSERT_EQUAL(0,m.writes);
  m.bytes[11]=0x40; m.bytes[10]=0; m.bytes[1]|=0x20;
  TEST_ASSERT_FALSE(charge::PrepareForSleep(m));
  TEST_ASSERT_EQUAL(0,m.writes);
}

void test_config_boot_page_keeps_restored_data_and_sleep_page() {
  app::AppState st; app::Snapshot sn, back; app::KisSession kis, restored;
  st.settings.display_page=(int)ui::Page::Market;
  demo::FillDemo(sn,1790000000LL);
  st.providers[(int)Provider::AirKorea].attempts=2;
  uint8_t blob[sizeof(rtc::Header)+rtc::kPayloadMax];
  const size_t n=rtc::Encode(sn,st.providers,(size_t)Provider::Count,kis,
      (uint8_t)ui::Page::Device,123,0,1,0,blob,sizeof(blob),{456,1790000000LL});
  TEST_ASSERT_TRUE(n>0);
  uint32_t crc=0;
  rtc::RenderState render;
  TEST_ASSERT_TRUE(power::RestoreRetainedRuntime(blob,n,true,st,back,restored,crc,render));
  TEST_ASSERT_EQUAL((int)ui::Page::Market,(int)st.page);
  TEST_ASSERT_TRUE(back.has_now);
  TEST_ASSERT_EQUAL(2,st.providers[(int)Provider::AirKorea].attempts);
  TEST_ASSERT_TRUE(power::RestoreRetainedRuntime(blob,n,false,st,back,restored,crc,render));
  TEST_ASSERT_EQUAL((int)ui::Page::Device,(int)st.page);
  TEST_ASSERT_EQUAL_UINT32(456,render.content_hash);
  TEST_ASSERT_EQUAL_INT64(1790000000LL,render.refreshed_utc);
  // Retained 15-minute wake: same content, changed clock -> skip.
  TEST_ASSERT_FALSE(ui::ShouldPresent(false,crc!=0,false,true,900000));
  TEST_ASSERT_TRUE(ui::ShouldPresent(false,crc!=0,false,true,1800000));
  blob[n-1]^=1;
  TEST_ASSERT_FALSE(power::RestoreRetainedRuntime(blob,n,true,st,back,restored,crc,render));
  TEST_ASSERT_EQUAL((int)ui::Page::Device,(int)st.page);
  TEST_ASSERT_EQUAL_INT64(1790000000LL,render.refreshed_utc);
}

void test_untrusted_usb_clock_never_blocks_automatic_cycles() {
  app::AppState st; strcpy(st.settings.wifi_ssid,"AUDIT-SSID");
  app::Snapshot sn; app::KisSession kis; FakeStorage storage; CycleFake p;
  p.rtc_ok=p.ntp_ok=p.connect_ok=false; p.connect_ms=20000;
  for (int i=0;i<10;i++) {
    const auto before=p.ms;
    app::RunCycle(i==0 ? power::CycleWake::FirstBoot : power::CycleWake::Timer,
        ButtonEvent::None,st,sn,kis,nullptr,storage,0,"test",p);
    TEST_ASSERT_EQUAL_UINT32(before,p.ms);
  }
  TEST_ASSERT_EQUAL(0,p.connects);
  app::RunCycle(power::CycleWake::Button,ButtonEvent::ShortKey0,
      st,sn,kis,nullptr,storage,0,"test",p);
  TEST_ASSERT_EQUAL(1,p.connects);
  p.usb=false;
  app::RunCycle(power::CycleWake::Timer,ButtonEvent::None,
      st,sn,kis,nullptr,storage,0,"test",p);
  TEST_ASSERT_EQUAL(2,p.connects);
}

void test_mid_provider_time_budget_defers_unsent_kis_request() {
  app::AppState st; SetupKisOnly(st,2);
  app::Snapshot sn; app::KisSession kis; FakeHttp http;
  strcpy(kis.token,"FAKE-TOKEN"); kis.expiry_utc=Kst("2026-11-03 12:00");
  http.On("inquire-price",200,"kis_ok.json");
  struct Gate : app::IFetchControl {
    const FakeHttp& http;
    explicit Gate(const FakeHttp& h):http(h){}
    bool CanRequest() const override { return http.urls.empty(); }
  } gate(http);
  app::RunFetchPass(st,sn,kis,http,net::kCaBundlePem,Ctx("2026-11-02 10:00"),true,0,nullptr,&gate);
  TEST_ASSERT_EQUAL(1,http.urls.size());
  TEST_ASSERT_TRUE(sn.has_kis[0]); TEST_ASSERT_FALSE(sn.has_kis[1]);
  const auto& status=st.providers[(int)Provider::KisPrice];
  TEST_ASSERT_EQUAL(0,status.attempts);
  TEST_ASSERT_EQUAL_INT64(0,status.last_attempt_utc);
  TEST_ASSERT_EQUAL_INT64(0,status.retry_after_utc);
}

void test_next_year_empty_calendar_retries_daily() {
  app::ResetCalendarRetry();
  app::AppState st; strcpy(st.settings.providers.kma_key,"AUDIT-FAKE-KEY");
  auto ctx=Ctx("2026-11-02 10:00");
  cal::Clear(st.calendars[0],2026); cal::Clear(st.calendars[1],2027);
  st.calendars[0].loaded=true; st.calendars[0].fetched_utc=ctx.now_utc;
  audit_holiday::Http http; http.data="{\"response\":{\"header\":{\"resultCode\":\"00\"},\"body\":{\"totalCount\":0,\"pageNo\":1,\"numOfRows\":128}}}";
  struct Gate : app::IFetchControl { bool CanRequest() const override { return true; } } gate;
  FakeStorage storage;
  app::RefreshCalendar(st,http,storage,ctx,gate);
  TEST_ASSERT_EQUAL(1,http.calls);
  ctx.now_utc+=3600;
  app::RefreshCalendar(st,http,storage,ctx,gate);
  TEST_ASSERT_EQUAL(1,http.calls);
  ctx.now_utc+=86400;
  app::RefreshCalendar(st,http,storage,ctx,gate);
  TEST_ASSERT_EQUAL(2,http.calls);
  app::ResetCalendarRetry();
}

void test_deferred_token_reissue_does_not_consume_cooldown() {
  app::AppState st; SetupKisOnly(st,1);
  app::Snapshot sn; app::KisSession kis; FakeHttp http;
  strcpy(kis.token,"FAKE-TOKEN"); kis.expiry_utc=Kst("2026-11-03 12:00");
  http.On("inquire-price",200,"kis_token_expired.json");
  struct Gate : app::IFetchControl {
    const FakeHttp& http;
    explicit Gate(const FakeHttp& h):http(h){}
    bool CanRequest() const override { return http.urls.empty(); }
  } gate(http);
  app::RunFetchPass(st,sn,kis,http,net::kCaBundlePem,Ctx("2026-11-02 10:00"),true,0,nullptr,&gate);
  TEST_ASSERT_EQUAL(1,http.urls.size());
  TEST_ASSERT_EQUAL(0,http.posts.size());
  TEST_ASSERT_EQUAL_INT64(0,kis.last_issue_attempt_utc);
  TEST_ASSERT_EQUAL_INT64(0,st.providers[(int)Provider::KisPrice].last_attempt_utc);
}

void test_maximum_persistent_blobs_fit_nvs_with_rewrite_reserve() {
  Settings s=ConfiguredSettings();
  auto fill=[](auto& text) { memset(text,'A',sizeof(text)-1); text[sizeof(text)-1]=0; };
  fill(s.wifi_ssid); fill(s.wifi_secret);
  fill(s.location.display_name); fill(s.location.air_station);
  fill(s.providers.kma_key); fill(s.providers.air_key); fill(s.providers.exim_key);
  fill(s.providers.kis_app_key); fill(s.providers.kis_app_secret);
  s.watch_count=kMaxWatchlist;
  for (auto& w:s.watchlist) { strcpy(w.symbol,"005930"); w.market=Market::Unified; }
  app::AppState st; st.settings=s; st.time_verified=true;
  app::Snapshot sn; demo::FillDemo(sn,1790000000LL);
  fill(sn.fx.usd.value.currency_unit); fill(sn.fx.jpy100.value.currency_unit);
  fill(sn.air.station);
  for (size_t i=0;i<kMaxWatchlist;i++) {
    sn.has_kis[i]=sn.has_fsc[i]=true;
    fill(sn.kis[i].quote.value.symbol); fill(sn.fsc[i].quote.value.symbol);
    fill(sn.kis[i].quote.value.market); fill(sn.fsc[i].quote.value.market);
  }
  sn.has_kospi=true; fill(sn.kospi.quote.value.name);
  uint8_t bytes[4092];
  const size_t set_size=settings_codec::Encode(s,bytes,sizeof(bytes));
  const size_t snap_size=snapshot_codec::EncodeSnapshot(sn,bytes,sizeof(bytes));
  const size_t status_size=snapshot_codec::EncodeStatus(st.providers,(size_t)Provider::Count,bytes,sizeof(bytes));
  TEST_ASSERT_TRUE(set_size>0 && set_size<=2048);
  TEST_ASSERT_TRUE(snap_size>0 && snap_size<=3072);
  const size_t run_size=28+snap_size+status_size;
  TEST_ASSERT_TRUE(run_size<=4092);
  // Conservative: 40-byte slot envelope; index + up to two chunk headers.
  auto entries=[](size_t n) { return (n+31)/32+3; };
  const size_t set_entries=entries(set_size+40), run_entries=entries(run_size+40);
  const size_t cal_entries=entries(256+40);
  const size_t max_entries=set_entries>run_entries ? set_entries:run_entries;
  // 5 NVS pages, one reserved for GC. Namespace/meta keys + Wi-Fi allowance.
  const size_t peak=2*(set_entries+run_entries+cal_entries)+max_entries+40;
  printf("nvs sizing: settings=%u snapshot=%u status=%u runtime=%u peak_entries=%u usable=504\n",
      (unsigned)set_size,(unsigned)snap_size,(unsigned)status_size,(unsigned)run_size,(unsigned)peak);
  TEST_ASSERT_TRUE(peak<=4*126);
  FakeStorage storage; RuntimeStore runtime;
  TEST_ASSERT_TRUE(runtime.Checkpoint(storage,st,sn,1790000000LL));
}
