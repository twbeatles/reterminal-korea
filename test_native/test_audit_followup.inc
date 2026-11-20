// Regression expectations are independent of the old audit's defect probes.
#include "../src/app/CyclePlatform.h"

struct CycleFake final : app::ICyclePlatform {
  uint32_t ms = 0, connect_ms = 0;
  int64_t now = Kst("2026-11-01 14:50");
  bool usb = true, rtc_ok = true, ntp_ok = true, wifi = false;
  bool connect_ok = true, render_ok = true, has_render = false;
  int mv = 4100, crashes = 0, connects = 0, samples = 0, renders = 0, portals = 0;
  int checkpoints = 0, retains = 0, beats = 0;
  int64_t sleep_seconds = 0;
  RuntimeStore runtime;
  uint32_t Millis() override { return ms; }
  int64_t Utc() override { return now + ms / 1000; }
  bool Usb() override { return usb; }
  int BatteryMv() override { return mv; }
  uint32_t Crashes() override { return crashes; }
  void SyncClock(bool connected, app::AppState& s) override { s.time_verified = rtc_ok || (connected && ntp_ok); }
  void OpenPortal(app::AppState&, IStorage&) override { ++portals; ms += 600000; }
  bool ReadIndoor(float& t, float& rh) override { ++samples; t=21; rh=50; return true; }
  bool Wifi() override { return wifi; }
  bool ConnectWifi(const Settings&) override { ++connects; ms += connect_ms; return wifi=connect_ok; }
  bool HasRender() override { return has_render; }
  bool Render(app::AppState& s, const app::Snapshot&, int, const char*, int, bool) override {
    ++renders; if (render_ok) { s.last_render_utc=Utc(); has_render=true; } return render_ok;
  }
  void Checkpoint(IStorage& st, const app::AppState& s, const app::Snapshot& sn, int64_t t) override {
    ++checkpoints; runtime.Checkpoint(st,s,sn,t);
  }
  void ClearCrashes() override { crashes=0; }
  void SetPending(bool) override {}
  void Retain(const app::Snapshot&, const app::AppState&, const app::KisSession&) override { ++retains; }
  void Sleep(int64_t s, const app::Snapshot&, const app::AppState&, const app::KisSession&) override { sleep_seconds=s; }
  void LogPass(const app::PassLog&, const app::Snapshot&) override {}
  void Heartbeat() override { ++beats; }
};

void test_cycle_real_orchestrator_success_and_recovery() {
  app::AppState st; st.settings=settings::DefaultSettings();
  strcpy(st.settings.providers.kma_key,"AUDIT-FAKE-KEY");
  st.providers[(int)Provider::KmaForecast].auth_blocked=true;
  st.providers[(int)Provider::KmaForecast].retry_after_utc=INT64_MAX/2;  // 인증 차단: 다음 날 재시도 대기(ADR 0026)
  app::Snapshot sn; app::KisSession kis; FakeStorage storage; FakeHttp http; CycleFake p;
  http.On("getUltraSrtNcst",200,"kma_ultra_ok.json");
  app::RunCycle(power::CycleWake::Timer, ButtonEvent::None,st,sn,kis,&http,storage,0,"test",p);
  TEST_ASSERT_TRUE(sn.has_now && st.sensor_present);
  TEST_ASSERT_EQUAL(1,p.connects); TEST_ASSERT_EQUAL(1,p.renders);
  TEST_ASSERT_EQUAL(1,p.checkpoints); TEST_ASSERT_EQUAL(1,p.retains);
  TEST_ASSERT_TRUE(p.beats>=3 && storage.write_count>0);
  app::AppState reboot; reboot.settings=st.settings; app::Snapshot restored; RuntimeStore reader;
  TEST_ASSERT_TRUE(reader.Restore(storage,reboot.settings,reboot,restored));
  TEST_ASSERT_TRUE(restored.has_now);
  TEST_ASSERT_FALSE(reboot.time_verified);
}

void test_cycle_low_power_and_untrusted_clock_gate() {
  for (int mode=0;mode<3;++mode) {
    app::AppState st; app::Snapshot sn; app::KisSession kis;
    FakeStorage storage; FakeHttp http; CycleFake p;
    strcpy(st.settings.wifi_ssid,"AUDIT-SSID");
    if (mode==0) { p.usb=false; p.mv=2900; }
    if (mode==1) { p.crashes=3; }
    if (mode==2) { p.rtc_ok=p.ntp_ok=false; }
    app::RunCycle(power::CycleWake::Timer,ButtonEvent::None,st,sn,kis,&http,storage,0,"test",p);
    TEST_ASSERT_EQUAL(0,http.urls.size());
    if (mode<2) { TEST_ASSERT_EQUAL(0,p.connects); TEST_ASSERT_EQUAL(0,p.samples); }
    else { TEST_ASSERT_FALSE(st.time_verified); TEST_ASSERT_EQUAL(0,p.checkpoints); }
    if (mode==0) TEST_ASSERT_EQUAL_INT64(power::kMaxSleepLowSec,p.sleep_seconds);
  }
}

void test_cycle_budget_and_render_retry() {
  app::AppState st; strcpy(st.settings.providers.kma_key,"AUDIT-FAKE-KEY");
  app::Snapshot sn; app::KisSession kis; FakeStorage storage; FakeHttp http; CycleFake p;
  p.connect_ms=60001; p.render_ok=false;
  app::RunCycle(power::CycleWake::Timer,ButtonEvent::None,st,sn,kis,&http,storage,0,"test",p);
  TEST_ASSERT_EQUAL(0,http.urls.size()); TEST_ASSERT_TRUE(st.display_failed);
  // All regular work now suppressed: retry is driven by display_failed alone.
  st.settings.providers.kma_key[0]='\0'; p.has_render=true; p.render_ok=true; p.ms=60002;
  app::RunCycle(power::CycleWake::Timer,ButtonEvent::None,st,sn,kis,&http,storage,0,"test",p);
  TEST_ASSERT_EQUAL(2,p.renders); TEST_ASSERT_FALSE(st.display_failed);
  app::RunCycle(power::CycleWake::Button,ButtonEvent::LongKey0,st,sn,kis,&http,storage,0,"test",p);
  TEST_ASSERT_EQUAL(1,p.portals);
}

void test_settings_migration_rejects_without_mutation() {
  Settings original=FullSettings(), result=original;
  uint8_t bytes[2048];const size_t n=settings_codec::Encode(original,bytes,sizeof(bytes));
  TEST_ASSERT_TRUE(settings_codec::Migrate(kSettingsSchemaVersion,bytes,n,result));
  TEST_ASSERT_FALSE(settings_codec::Migrate(1,bytes,n,result));
  TEST_ASSERT_EQUAL_STRING(original.wifi_ssid,result.wifi_ssid);
  TEST_ASSERT_FALSE(settings_codec::Migrate(99,bytes,n,result));
  TEST_ASSERT_FALSE(settings_codec::Migrate(kSettingsSchemaVersion,bytes,n-1,result));
  TEST_ASSERT_EQUAL_STRING(original.providers.kis_app_secret,result.providers.kis_app_secret);
}

void test_checkpoint_failure_is_distinct_from_defer() {
  FakeStorage storage; RuntimeStore runtime; app::AppState st; app::Snapshot sn;
  st.time_verified=true;
  const int64_t now=Kst("2026-11-01 14:50");
  storage.fail_on_write=1; storage.fail_bytes=0; storage.fail_return=true;
  TEST_ASSERT_FALSE(runtime.Checkpoint(storage,st,sn,now));
  TEST_ASSERT_TRUE(runtime.CheckpointFailed());
  storage.fail_on_write=-1;
  TEST_ASSERT_FALSE(runtime.Checkpoint(storage,st,sn,now+30));  // failure cooldown
  TEST_ASSERT_FALSE(runtime.CheckpointFailed());  // deferred, no new I/O failure
  TEST_ASSERT_TRUE(runtime.Checkpoint(storage,st,sn,now+300));
  TEST_ASSERT_FALSE(runtime.CheckpointFailed());
  TEST_ASSERT_FALSE(runtime.Checkpoint(storage,st,sn,now+301));
  TEST_ASSERT_FALSE(runtime.CheckpointFailed());  // unchanged snapshot is not I/O failure
}

void test_market_fresher_close_replaces_stale_live() {
  app::AppState st; st.settings=ConfiguredSettings(); st.settings.watch_count=1;
  strcpy(st.settings.watchlist[0].symbol,"005930");
  app::Snapshot sn; const auto ctx=Ctx("2026-11-05 14:50");
  sn.has_kis[0]=sn.has_fsc[0]=true;
  auto& live=sn.kis[0].quote;auto& close=sn.fsc[0].quote;
  live.has_value=close.has_value=true;
  live.validity=Validity::Stale;close.validity=Validity::Valid;
  live.value.price_krw=50000;close.value.price_krw=60000;
  live.observed_at=live.value.price_at=SourceTs(Kst("2026-11-02 14:00"));
  close.observed_at=close.value.price_at=SourceTs(Kst("2026-11-04 00:00"));
  auto page=vm::BuildMarket(sn,st,ctx.now_utc,true,true,"",false);
  TEST_ASSERT_EQUAL_STRING("60,000",page.rows[0].value);
  TEST_ASSERT_EQUAL_STRING("전일 종가",page.rows[0].kind);
  live.validity=Validity::Valid;
  page=vm::BuildMarket(sn,st,ctx.now_utc,true,true,"",false);
  TEST_ASSERT_EQUAL_STRING("50,000",page.rows[0].value);
}

void test_kma_rejects_wrong_or_missing_source_issue() {
  const std::string good = ReadFile("test/fixtures/kma_ultra_ok.json");
  const auto ctx = Ctx("2026-11-01 14:50");
  for (const char* field : {"baseDate", "baseTime", "nx", "ny"}) {
    JsonDocument doc;
    TEST_ASSERT_FALSE(deserializeJson(doc, good));
    auto item = doc["response"]["body"]["items"]["item"][0];
    item.remove(field);
    std::string body; serializeJson(doc, body);
    auto out = pj::KmaUltraNow(body.c_str(), body.size(), 20261101, 1400, ctx);
    TEST_ASSERT_EQUAL((int)Outcome::ProviderError, (int)out.outcome);
  }
  for (const char* wrong : {"20261031", "20261102", "bad"}) {
    JsonDocument doc; deserializeJson(doc, good);
    doc["response"]["body"]["items"]["item"][0]["baseDate"] = wrong;
    std::string body; serializeJson(doc, body);
    auto out = pj::KmaUltraNow(body.c_str(), body.size(), 20261101, 1400, ctx);
    TEST_ASSERT_EQUAL((int)Outcome::ProviderError, (int)out.outcome);
  }
  auto out = pj::KmaUltraNow(good.c_str(), good.size(), 20261101, 1400, ctx);
  TEST_ASSERT_EQUAL((int)Outcome::Ok, (int)out.outcome);
  TEST_ASSERT_EQUAL_INT64(Kst("2026-11-01 14:00"), out.temp_c.observed_at.epoch_utc);
  out = pj::KmaUltraNow(good.c_str(), good.size(), 20261101, 1400, ctx, 61, 127);
  TEST_ASSERT_EQUAL((int)Outcome::ProviderError,(int)out.outcome);
  const std::string fc=ReadFile("test/fixtures/kma_village_single_object.json");
  auto forecast=pj::KmaVillage(fc.c_str(),fc.size(),20261101,1400,ctx,60,128);
  TEST_ASSERT_EQUAL((int)Outcome::ProviderError,(int)forecast.outcome);
}

void test_air_keeps_per_pollutant_time_in_ui() {
  air::Row rows[3];
  rows[0] = {"80", "15", "2026-11-01 13:00", "station"};
  rows[1] = {"-", "20", "2026-11-01 14:00", "station"};
  rows[2] = {"90", "30", "2026-11-01 12:00", "station"};
  app::AppState st; st.settings = ConfiguredSettings();
  app::Snapshot snap;
  const auto ctx = Ctx("2026-11-01 14:50");
  snap.air = air::Parse(rows, 3, "00", false, "station", ctx);
  snap.has_air = true;
  TEST_ASSERT_EQUAL(80, snap.air.pm10.value);
  TEST_ASSERT_EQUAL(20, snap.air.pm25.value);
  TEST_ASSERT_EQUAL_INT64(Kst("2026-11-01 14:00"), snap.air.data_time_utc);
  const auto page = vm::BuildLife(snap, st, ctx.now_utc, true, true, false);
  TEST_ASSERT_NOT_NULL(strstr(page.pm10.sub, "13:00"));
  TEST_ASSERT_NOT_NULL(strstr(page.pm25.sub, "14:00"));
  TEST_ASSERT_EQUAL((int)vm::Badge::LastGood, (int)page.pm10.badge);
}

void test_provider_failure_visible_until_success() {
  app::AppState st; st.settings = ConfiguredSettings();
  app::Snapshot snap;
  const auto good = ReadFile("test/fixtures/kma_ultra_ok.json");
  auto ctx = Ctx("2026-11-01 14:50");
  snap.now = pj::KmaUltraNow(good.c_str(), good.size(), 20261101, 1400, ctx);
  snap.has_now = true;
  app::KisSession kis; FakeHttp http;
  http.On("getUltraSrtNcst", 503, "kma_error_key.json");
  app::RunFetchPass(st, snap, kis, http, net::kCaBundlePem, ctx, true, 0, nullptr);
  auto page = vm::BuildLife(snap, st, ctx.now_utc, true, true, false);
  TEST_ASSERT_EQUAL((int)vm::Badge::LastGood, (int)page.temp.badge);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 21.5f, (float)snap.now.temp_c.value);
  app::MarkResult(st, Provider::KmaNow, Outcome::Ok, ctx.now_utc + 60, true, -1, 0);
  page = vm::BuildLife(snap, st, ctx.now_utc + 60, true, true, false);
  TEST_ASSERT_EQUAL((int)vm::Badge::None, (int)page.temp.badge);
}

// 페이지 전환(왼쪽 KEY2 이전 / 가운데 KEY1 다음)은 저장된 스냅샷만 다시 그린다: Wi-Fi·HTTP 0회.
void test_page_keys_never_call_api() {
  app::AppState st; st.settings=settings::DefaultSettings();
  strcpy(st.settings.providers.kma_key,"AUDIT-FAKE-KEY");
  app::Snapshot sn; app::KisSession kis; FakeStorage storage; FakeHttp http; CycleFake p;
  http.On("getUltraSrtNcst",200,"kma_ultra_ok.json");
  app::RunCycle(power::CycleWake::Timer,ButtonEvent::None,st,sn,kis,&http,storage,0,"test",p);
  const size_t base=http.urls.size(); const int connects=p.connects;
  for (int i=0;i<6;++i) {
    app::RunCycle(power::CycleWake::Button,i%2?ButtonEvent::ShortKey2:ButtonEvent::ShortKey1,
                  st,sn,kis,&http,storage,0,"test",p);
  }
  TEST_ASSERT_EQUAL(base,http.urls.size());
  TEST_ASSERT_EQUAL(connects,p.connects);
}

// 녹색 KEY0 짧게(수동 새로고침)는 공급자별 최소 간격 안에서는 다시 호출하지 않는다.
void test_manual_refresh_cooldown() {
  app::AppState st; st.settings=settings::DefaultSettings();
  strcpy(st.settings.providers.kma_key,"AUDIT-FAKE-KEY");
  st.providers[(int)Provider::KmaForecast].auth_blocked=true;
  st.providers[(int)Provider::KmaForecast].retry_after_utc=INT64_MAX/2;  // 인증 차단: 다음 날 재시도 대기(ADR 0026)
  app::Snapshot sn; app::KisSession kis; FakeStorage storage; FakeHttp http; CycleFake p;
  http.On("getUltraSrtNcst",200,"kma_ultra_ok.json");
  app::RunCycle(power::CycleWake::Timer,ButtonEvent::None,st,sn,kis,&http,storage,0,"test",p);
  const size_t first=http.urls.size();
  p.ms+=10000;  // 10초 뒤 연타
  app::RunCycle(power::CycleWake::Button,ButtonEvent::ShortKey0,st,sn,kis,&http,storage,0,"test",p);
  TEST_ASSERT_EQUAL(first,http.urls.size());
  p.ms+=app::kForceMinGapSec*1000;  // 최소 간격 경과
  app::RunCycle(power::CycleWake::Button,ButtonEvent::ShortKey0,st,sn,kis,&http,storage,0,"test",p);
  TEST_ASSERT_TRUE(http.urls.size()>first);
}

// 전일 대비율: 숫자 형식·부호·범위를 엄격히 파싱하고, 없으면 표시하지 않는다.
void test_change_rate_parse_format_and_row() {
  int32_t bp = 99;
  TEST_ASSERT_TRUE(fin::ParseChangeBp("1.23", "2", bp));  TEST_ASSERT_EQUAL(123, bp);
  TEST_ASSERT_TRUE(fin::ParseChangeBp("-0.51", "5", bp)); TEST_ASSERT_EQUAL(-51, bp);
  TEST_ASSERT_TRUE(fin::ParseChangeBp("0.51", "5", bp));  TEST_ASSERT_EQUAL(-51, bp);  // 하락 코드가 부호를 정한다
  TEST_ASSERT_TRUE(fin::ParseChangeBp("2", nullptr, bp)); TEST_ASSERT_EQUAL(200, bp);
  TEST_ASSERT_TRUE(fin::ParseChangeBp("0.00", "3", bp));  TEST_ASSERT_EQUAL(0, bp);
  TEST_ASSERT_TRUE(fin::ParseChangeBp("1.235", "2", bp)); TEST_ASSERT_EQUAL(124, bp);
  TEST_ASSERT_FALSE(fin::ParseChangeBp(nullptr, "2", bp));
  TEST_ASSERT_FALSE(fin::ParseChangeBp("", "2", bp));
  TEST_ASSERT_FALSE(fin::ParseChangeBp("abc", "2", bp));
  TEST_ASSERT_FALSE(fin::ParseChangeBp("1.2x", "2", bp));
  TEST_ASSERT_FALSE(fin::ParseChangeBp("1.", "2", bp));
  TEST_ASSERT_FALSE(fin::ParseChangeBp("250.00", "2", bp));
  char out[16];
  fmt::ChangePct(123, out, sizeof(out)); TEST_ASSERT_EQUAL_STRING("+1.23%", out);
  fmt::ChangePct(-51, out, sizeof(out)); TEST_ASSERT_EQUAL_STRING("-0.51%", out);
  fmt::ChangePct(0, out, sizeof(out));   TEST_ASSERT_EQUAL_STRING("0.00%", out);

  // 응답에 값이 있으면 뷰모델 행에 chg가 채워지고, 없으면 비어 있다.
  fin::KisQuoteBody b; b.http_status=200; b.rt_cd="0"; b.msg_cd="MCA00000";
  b.stck_prpr="71,000"; b.prdy_ctrt="-1.40"; b.prdy_vrss_sign="5";
  FetchContext ctx; ctx.now_utc=ctx.fetched_utc=Kst("2026-11-02 10:00"); ctx.clock_verified=true;
  fin::KisOut o=fin::ParseKis(b,"005930","KRX",ctx);
  TEST_ASSERT_TRUE(o.quote.value.has_change);
  TEST_ASSERT_EQUAL(-140,o.quote.value.change_bp);
  b.prdy_ctrt=nullptr;
  o=fin::ParseKis(b,"005930","KRX",ctx);
  TEST_ASSERT_EQUAL(Outcome::Ok==o.outcome,true);
  TEST_ASSERT_FALSE(o.quote.value.has_change);  // 현재가는 유지, 등락률만 생략
}

// 갱신 주기 배율(절약 모드): 100~400%로 제한, 로컬 센서·발표 회차 구동은 건드리지 않는다.
void test_refresh_scale_stretches_intervals_only() {
  TEST_ASSERT_EQUAL_INT64(300, sched::ScaleInterval(300, 100));
  TEST_ASSERT_EQUAL_INT64(900, sched::ScaleInterval(300, 300));
  TEST_ASSERT_EQUAL_INT64(300, sched::ScaleInterval(300, 50));    // 빈도를 늘리는 값은 100%로
  TEST_ASSERT_EQUAL_INT64(1200, sched::ScaleInterval(300, 9999)); // 상한 400%
  app::AppState st; st.settings=ConfiguredSettings();
  st.settings.providers.kis_enabled=true;
  strcpy(st.settings.providers.kis_app_key,"FAKEAPPKEY"); strcpy(st.settings.providers.kis_app_secret,"FAKEAPPSECRET");
  strcpy(st.settings.watchlist[0].symbol,"005930"); st.settings.watch_count=1;
  st.sensor_present=true;
  const int64_t t0=Kst("2026-11-02 10:00");
  app::MarkResult(st,Provider::Exim,Outcome::Ok,t0,true,-1,0);
  app::MarkResult(st,Provider::Sht40,Outcome::Ok,t0,true,-1,0);
  app::MarkResult(st,Provider::KisPrice,Outcome::Ok,t0,true,-1,0);
  const int64_t exim1=app::NextAttemptUtc(st,Provider::Exim,t0+1,true);
  const int64_t sht1=app::NextAttemptUtc(st,Provider::Sht40,t0+1,true);
  const int64_t kis1=app::NextAttemptUtc(st,Provider::KisPrice,t0+1,true);
  st.settings.schedule.refresh_scale_pct=200;
  TEST_ASSERT_EQUAL_INT64(t0+2*(exim1-t0),app::NextAttemptUtc(st,Provider::Exim,t0+1,true));
  TEST_ASSERT_EQUAL_INT64(t0+2*(kis1-t0),app::NextAttemptUtc(st,Provider::KisPrice,t0+1,true));
  TEST_ASSERT_EQUAL_INT64(sht1,app::NextAttemptUtc(st,Provider::Sht40,t0+1,true));  // 로컬 센서는 그대로
}

// 시계만 바뀐 화면은 즉시 다시 그리지 않는다(깜빡임 감소). 내용이 바뀌거나 오래되면 갱신.
void test_clock_only_change_does_not_present() {
  ui::Canvas a, b;
  const Rect clock{ui::kRightX, ui::kHeaderTop, 748 - ui::kRightX, 28};
  a.draw_text(ui::DefaultMedium(), 100, 100, "값 71,000");
  b.draw_text(ui::DefaultMedium(), 100, 100, "값 71,000");
  a.draw_text(ui::DefaultMedium(), ui::kRightX + 4, ui::kHeaderTop + 4, "09-21 23:13");
  b.draw_text(ui::DefaultMedium(), ui::kRightX + 4, ui::kHeaderTop + 4, "09-21 23:14");
  TEST_ASSERT_NOT_EQUAL(a.crc32(), b.crc32());
  TEST_ASSERT_EQUAL_UINT32(a.content_hash(clock), b.content_hash(clock));
  b.draw_text(ui::DefaultMedium(), 100, 200, "변경");
  TEST_ASSERT_NOT_EQUAL(a.content_hash(clock), b.content_hash(clock));

  using namespace ui;
  TEST_ASSERT_FALSE(ShouldPresent(false, true, false, true, 60 * 1000));            // 시계만: 대기
  TEST_ASSERT_TRUE(ShouldPresent(false, true, false, true, kClockOnlyRefreshMs));   // 30분 지나면 갱신
  TEST_ASSERT_FALSE(ShouldPresent(false, true, false, false, kClockOnlyRefreshMs)); // 아무 변화 없으면 안 함
  TEST_ASSERT_TRUE(ShouldPresent(false, true, true, true, 0));                      // 내용 변경
  TEST_ASSERT_TRUE(ShouldPresent(true, true, false, false, 0));                     // 버튼 강제
  TEST_ASSERT_TRUE(ShouldPresent(false, false, false, false, 0));                   // 첫 화면
}

// 코스피 지수: KIS inquire-index-price 응답(소수 2자리)을 x100 정수로 파싱한다.
void test_kospi_index_parse() {
  int64_t v=0;
  TEST_ASSERT_TRUE(fin::ParseScaled2("5371.68",v)); TEST_ASSERT_EQUAL_INT64(537168,v);
  TEST_ASSERT_TRUE(fin::ParseScaled2("2,615.4",v)); TEST_ASSERT_EQUAL_INT64(261540,v);
  TEST_ASSERT_TRUE(fin::ParseScaled2("900",v));     TEST_ASSERT_EQUAL_INT64(90000,v);
  TEST_ASSERT_FALSE(fin::ParseScaled2("",v));
  TEST_ASSERT_FALSE(fin::ParseScaled2(nullptr,v));
  TEST_ASSERT_FALSE(fin::ParseScaled2("-1.0",v));
  TEST_ASSERT_FALSE(fin::ParseScaled2("0.00",v));   // 0은 실값이 아님(결측)
  TEST_ASSERT_FALSE(fin::ParseScaled2("12a",v));
  const char* body=
    "{\"rt_cd\":\"0\",\"msg_cd\":\"MCA00000\",\"output\":{\"bstp_nmix_prpr\":\"5371.68\","
    "\"bstp_nmix_prdy_vrss\":\"132.55\",\"prdy_vrss_sign\":\"2\",\"bstp_nmix_prdy_ctrt\":\"2.53\"}}";
  FetchContext ctx; ctx.now_utc=ctx.fetched_utc=Kst("2026-11-02 10:00"); ctx.clock_verified=true;
  fin::KisIndexOut o=pj::KisIndex(200,body,strlen(body),"KOSPI",ctx);
  TEST_ASSERT_EQUAL((int)Outcome::Ok,(int)o.outcome);
  TEST_ASSERT_TRUE(o.quote.has_value);
  TEST_ASSERT_EQUAL_INT64(537168,o.quote.value.value_x100);
  TEST_ASSERT_TRUE(o.quote.value.has_change);
  TEST_ASSERT_EQUAL(253,o.quote.value.change_bp);
  TEST_ASSERT_EQUAL_STRING("KOSPI",o.quote.value.name);
  // 하락 부호 코드, 값 없음, 오류 본문
  const char* down="{\"rt_cd\":\"0\",\"msg_cd\":\"MCA00000\",\"output\":{\"bstp_nmix_prpr\":\"2500.10\","
    "\"prdy_vrss_sign\":\"5\",\"bstp_nmix_prdy_ctrt\":\"-0.40\"}}";
  o=pj::KisIndex(200,down,strlen(down),"KOSPI",ctx);
  TEST_ASSERT_EQUAL(-40,o.quote.value.change_bp);
  const char* nochg="{\"rt_cd\":\"0\",\"msg_cd\":\"MCA00000\",\"output\":{\"bstp_nmix_prpr\":\"2500.10\"}}";
  o=pj::KisIndex(200,nochg,strlen(nochg),"KOSPI",ctx);
  TEST_ASSERT_TRUE(o.quote.has_value); TEST_ASSERT_FALSE(o.quote.value.has_change);
  const char* rate="{\"rt_cd\":\"1\",\"msg_cd\":\"EGW00201\",\"msg1\":\"x\"}";
  o=pj::KisIndex(500,rate,strlen(rate),"KOSPI",ctx);
  TEST_ASSERT_TRUE(o.outcome!=Outcome::Ok); TEST_ASSERT_FALSE(o.quote.has_value);
  const char* noval="{\"rt_cd\":\"0\",\"msg_cd\":\"MCA00000\",\"output\":{}}";
  o=pj::KisIndex(200,noval,strlen(noval),"KOSPI",ctx);
  TEST_ASSERT_FALSE(o.quote.has_value);
  char url[256];
  TEST_ASSERT_TRUE(endpoints::KisIndexUrl("0001",url,sizeof(url)));
  TEST_ASSERT_NOT_NULL(strstr(url,"inquire-index-price"));
  TEST_ASSERT_NOT_NULL(strstr(url,"FID_INPUT_ISCD=0001"));
  TEST_ASSERT_FALSE(endpoints::KisIndexUrl("00a1",url,sizeof(url)));
}

static void SetupKisOnly(app::AppState& st, size_t watch) {
  st.settings=ConfiguredSettings();
  st.settings.providers.kma_key[0]=st.settings.providers.air_key[0]=st.settings.providers.exim_key[0]='\0';
  st.settings.providers.fsc_enabled=false; st.settings.providers.kis_enabled=true;
  strcpy(st.settings.providers.kis_app_key,"FAKEAPPKEY0000000");
  strcpy(st.settings.providers.kis_app_secret,"FAKEAPPSECRET000000");
  for (size_t i=0;i<watch;i++) { strcpy(st.settings.watchlist[i].symbol,"005930"); }
  st.settings.watch_count=watch;
}

// 지수는 종목 시세가 모두 성공한 뒤 1회만 요청하며, 지수 실패는 종목 시세의 실패가 아니다.
void test_fetch_kospi_index_and_isolation() {
  {
    app::AppState st; SetupKisOnly(st,2);
    app::Snapshot snap; app::KisSession kis; FakeHttp http; app::PassLog log;
    http.On("oauth2/tokenP",200,"kis_token_ok.json");
    http.On("inquire-price",200,"kis_ok.json"); http.On("inquire-price",200,"kis_ok.json");
    http.On("inquire-index-price",200,"kis_index_ok.json");
    app::RunFetchPass(st,snap,kis,http,net::kCaBundlePem,Ctx("2026-11-02 10:00"),true,0,&log);
    TEST_ASSERT_EQUAL(4,log.requests);  // 토큰 1 + 종목 2 + 지수 1
    TEST_ASSERT_EQUAL(1,http.Count("inquire-index-price"));
    TEST_ASSERT_TRUE(snap.has_kospi);
    TEST_ASSERT_EQUAL_INT64(261543,snap.kospi.quote.value.value_x100);
    TEST_ASSERT_EQUAL(46,snap.kospi.quote.value.change_bp);
    TEST_ASSERT_EQUAL((int)Outcome::Ok,(int)st.providers[(int)Provider::KisPrice].last_outcome);
  }
  {  // 지수가 초당 제한(500)이어도 종목 시세는 성공으로 기록된다.
    app::AppState st; SetupKisOnly(st,1);
    app::Snapshot snap; app::KisSession kis; FakeHttp http; app::PassLog log;
    http.On("oauth2/tokenP",200,"kis_token_ok.json");
    http.On("inquire-price",200,"kis_ok.json");
    http.On("inquire-index-price",500,"kis_rate_limited.json");
    app::RunFetchPass(st,snap,kis,http,net::kCaBundlePem,Ctx("2026-11-02 10:00"),true,0,&log);
    TEST_ASSERT_TRUE(snap.has_kis[0]);
    TEST_ASSERT_FALSE(snap.has_kospi);
    TEST_ASSERT_EQUAL((int)Outcome::Ok,(int)st.providers[(int)Provider::KisPrice].last_outcome);
  }
  {  // 종목 조회가 실패하면 지수는 요청하지 않는다(요청 낭비 방지).
    app::AppState st; SetupKisOnly(st,1);
    app::Snapshot snap; app::KisSession kis; FakeHttp http; app::PassLog log;
    http.On("oauth2/tokenP",200,"kis_token_ok.json");
    http.On("inquire-price",500,"kis_rate_limited.json");
    app::RunFetchPass(st,snap,kis,http,net::kCaBundlePem,Ctx("2026-11-02 10:00"),true,0,&log);
    TEST_ASSERT_EQUAL(0,http.Count("inquire-index-price"));
  }
}

// ---- 달력 탭 ----
static void LoadTables(app::AppState& st, int year) {
  cal::Clear(st.calendars[0], year); cal::Clear(st.calendars[1], year + 1);
  st.calendars[0].loaded = st.calendars[1].loaded = true;
}

void test_calendar_grid_flags_and_labels() {
  app::AppState st; st.settings = settings::DefaultSettings();
  LoadTables(st, 2026);
  cal::SetHoliday(st.calendars[0], 12, 25);
  cal::SetMarketClosed(st.calendars[0], 12, 31);
  const int64_t now = Kst("2026-11-11 14:20");
  vm::CalendarPage p = vm::BuildCalendar(st, now, true, true, false);
  TEST_ASSERT_TRUE(p.valid);
  TEST_ASSERT_EQUAL_STRING("2026년 11월", p.title);
  TEST_ASSERT_EQUAL(2026, p.year); TEST_ASSERT_EQUAL(11, p.month);
  TEST_ASSERT_EQUAL(11, p.today);
  TEST_ASSERT_EQUAL(0, p.first_wday);   // 2026-11-01 = 일요일
  TEST_ASSERT_EQUAL(30, p.days);
  TEST_ASSERT_EQUAL_STRING("11", p.day_big);
  TEST_ASSERT_EQUAL_STRING("수요일", p.weekday);
  // 소설(11-22) 절기 표시, 다음 공휴일 12-25 성탄절 D-44, 다음 절기는 11-22 소설 D-11
  TEST_ASSERT_TRUE(p.flags[22] & vm::kCalTerm);
  TEST_ASSERT_EQUAL_STRING("12-25 성탄절 · D-44", p.next_holiday);
  TEST_ASSERT_EQUAL_STRING("11-22 소설 · D-11", p.next_term);
  // 12월 달력: 공휴일/휴장/절기 표시와 시작 요일(2026-12-01 = 화요일 = 2)
  p = vm::BuildCalendar(st, Kst("2026-12-02 09:00"), true, true, false);
  TEST_ASSERT_EQUAL(2, p.first_wday);
  TEST_ASSERT_TRUE(p.flags[25] & vm::kCalHoliday);
  TEST_ASSERT_TRUE(p.flags[31] & vm::kCalClosed);
  TEST_ASSERT_TRUE(p.flags[7] & vm::kCalTerm);   // 대설 12-07
  TEST_ASSERT_TRUE(p.flags[22] & vm::kCalTerm);  // 동지 12-22
  TEST_ASSERT_FALSE(p.flags[10] & (vm::kCalHoliday|vm::kCalClosed|vm::kCalTerm));
}

void test_calendar_edge_cases() {
  app::AppState st; st.settings = settings::DefaultSettings();
  // 윤년 2월(2028)과 달력 미수신: 공휴일을 단정하지 않는다.
  vm::CalendarPage p = vm::BuildCalendar(st, Kst("2028-02-10 10:00"), true, true, false);
  TEST_ASSERT_EQUAL(29, p.days);
  TEST_ASSERT_FALSE(p.tables_ok);
  TEST_ASSERT_EQUAL_STRING("공휴일 정보 없음", p.next_holiday);
  // 시계 미확인: 날짜를 신뢰하지 않으므로 그리드를 만들지 않는다.
  p = vm::BuildCalendar(st, Kst("2026-11-11 14:20"), false, true, false);
  TEST_ASSERT_FALSE(p.valid);
  // 절기 표가 끝난 해(2029): 다음 절기 정보 없음(추측하지 않음)
  LoadTables(st, 2029);
  p = vm::BuildCalendar(st, Kst("2029-03-10 10:00"), true, true, false);
  TEST_ASSERT_EQUAL_STRING("절기 정보 없음", p.next_term);
  TEST_ASSERT_FALSE(p.flags[5] & vm::kCalTerm);
  // 연말: 12월 말에서 다음 해 1월 1일 공휴일(신정)을 찾는다(두 해 표 사용).
  LoadTables(st, 2026);
  cal::SetHoliday(st.calendars[1], 1, 1);
  p = vm::BuildCalendar(st, Kst("2026-12-30 10:00"), true, true, false);
  TEST_ASSERT_EQUAL_STRING("01-01 신정 · D-2", p.next_holiday);
  // 이름을 모르는 공휴일(음력 기반)은 "공휴일"로
  cal::SetHoliday(st.calendars[0], 12, 31);
  cal::SetHoliday(st.calendars[1], 1, 1, false);
  p = vm::BuildCalendar(st, Kst("2026-12-30 10:00"), true, true, false);
  TEST_ASSERT_EQUAL_STRING("12-31 공휴일 · D-1", p.next_holiday);
  // 오늘이 공휴일이면 D-day
  p = vm::BuildCalendar(st, Kst("2026-12-31 10:00"), true, true, false);
  TEST_ASSERT_EQUAL_STRING("12-31 공휴일 · 오늘", p.next_holiday);
}

void test_calendar_names_fit_font() {
  // 달력 화면의 모든 고정 문구와 절기·공휴일 이름이 내장 글꼴에 있어야 한다(□ 방지).
  ui::Canvas c; const auto& f = ui::DefaultMedium(); const auto& s = ui::DefaultSmall();
  for (size_t i = 0; i < terms::Count(); ++i) c.draw_text(f, 0, 0, terms::Name(terms::At(i).index));
  for (const char* n : {"신정","삼일절","어린이날","현충일","광복절","개천절","한글날","성탄절","공휴일",
                        "일요일","월요일","화요일","수요일","목요일","금요일","토요일","오늘",
                        "다음 공휴일","다음 절기","달력","휴장","절기","공휴일 정보 없음","절기 정보 없음",
                        "시간 확인 필요","시간 미확인","일정 미확인","정규장 시간","정규장 외","장시작 전"}) c.draw_text(f, 0, 0, n);
  for (const char* n : {"일","월","화","수","목","금","토","휴","절","공휴일 휴장 절기","기호: 휴=휴장일 · 절=24절기","년","월"}) c.draw_text(s, 0, 0, n);
  TEST_ASSERT_EQUAL(0, c.missing_glyphs());
}

// ---- Wi-Fi 감독기: 연결 유지·재연결 정책 ----
void test_wifi_supervisor_policy() {
  using net::WifiAction; using net::WifiSupervisor;
  WifiSupervisor w; uint32_t t = 1000;
  // 배터리(keep_alive=false)나 SSID 없음: 아무 것도 하지 않는다.
  TEST_ASSERT_EQUAL((int)WifiAction::None,(int)w.Tick(t,false,false,true));
  TEST_ASSERT_EQUAL((int)WifiAction::None,(int)w.Tick(t,false,true,false));
  // 처음부터 미연결(갱신 이력 복원으로 호출할 것이 없는 부팅): 즉시 연결을 시작한다.
  TEST_ASSERT_EQUAL((int)WifiAction::Begin,(int)w.Tick(t,false,true,true));
  // 시도 중에는 중복 시작하지 않는다.
  t += 5000;  TEST_ASSERT_EQUAL((int)WifiAction::None,(int)w.Tick(t,false,true,true));
  // 20초 안에 연결되면 실패 횟수 초기화.
  t += 3000;  TEST_ASSERT_EQUAL((int)WifiAction::None,(int)w.Tick(t,true,true,true));
  TEST_ASSERT_EQUAL(0,w.failures());
  // 연결이 끊기면 드라이버 자동 재연결을 위해 10초 유예 후 시도.
  t += 60000; TEST_ASSERT_EQUAL((int)WifiAction::None,(int)w.Tick(t,false,true,true));
  t += 5000;  TEST_ASSERT_EQUAL((int)WifiAction::None,(int)w.Tick(t,false,true,true));
  t += 6000;  TEST_ASSERT_EQUAL((int)WifiAction::Begin,(int)w.Tick(t,false,true,true));
  // 시도가 시간 초과되면 실패로 세고 백오프(5s, 10s, 20s ...).
  t += 20000; TEST_ASSERT_EQUAL((int)WifiAction::None,(int)w.Tick(t,false,true,true));
  TEST_ASSERT_EQUAL(1,w.failures());
  TEST_ASSERT_EQUAL_UINT32(5000,w.last_backoff_ms());
  t += 4000;  TEST_ASSERT_EQUAL((int)WifiAction::None,(int)w.Tick(t,false,true,true));  // 백오프 중
  t += 1500;  TEST_ASSERT_EQUAL((int)WifiAction::Begin,(int)w.Tick(t,false,true,true));
  t += 20000; w.Tick(t,false,true,true);
  TEST_ASSERT_EQUAL(2,w.failures());
  TEST_ASSERT_EQUAL_UINT32(10000,w.last_backoff_ms());
  // 4번째 시도는 라디오 전체 리셋.
  t += 10500; TEST_ASSERT_EQUAL((int)WifiAction::Begin,(int)w.Tick(t,false,true,true));   // 시도 #3
  t += 20000; w.Tick(t,false,true,true);
  t += 21000; TEST_ASSERT_EQUAL((int)WifiAction::ResetAndBegin,(int)w.Tick(t,false,true,true)); // 시도 #4
  // 백오프 상한 5분.
  TEST_ASSERT_EQUAL_UINT32(5000,net::WifiSupervisor::BackoffMs(1));
  TEST_ASSERT_EQUAL_UINT32(300000,net::WifiSupervisor::BackoffMs(30));
  // 복구되면 즉시 정상.
  t += 3000;  w.Tick(t,true,true,true);
  TEST_ASSERT_EQUAL(0,w.failures());
}

void test_wifi_supervisor_redraw_is_debounced() {
  using net::WifiSupervisor;
  WifiSupervisor w; uint32_t t = 0;
  w.Tick(t,true,true,true);                       // 부팅: 연결됨, 표시도 연결됨
  TEST_ASSERT_FALSE(w.TakeRedrawRequest(t));
  t += 100000; w.Tick(t,false,true,true);         // 끊김
  t += 10000;  w.Tick(t,false,true,true);
  TEST_ASSERT_FALSE(w.TakeRedrawRequest(t));      // 30초 안정 전에는 그리지 않는다
  t += 25000;  w.Tick(t,false,true,true);
  TEST_ASSERT_TRUE(w.TakeRedrawRequest(t));       // 안정 후 한 번
  TEST_ASSERT_FALSE(w.TakeRedrawRequest(t));
  // 곧바로 복구돼도 5분 최소 간격 전에는 다시 그리지 않는다(깜빡임 방지).
  t += 1000;   w.Tick(t,true,true,true);
  t += 40000;  w.Tick(t,true,true,true);
  TEST_ASSERT_FALSE(w.TakeRedrawRequest(t));
  t += 300000; w.Tick(t,true,true,true);
  TEST_ASSERT_TRUE(w.TakeRedrawRequest(t));
  // 짧게 끊겼다 되돌아오면(안정 전) 표시와 같아져 그릴 필요가 없다.
  t += 400000; w.Tick(t,false,true,true);
  t += 5000;   w.Tick(t,true,true,true);
  t += 60000;  w.Tick(t,true,true,true);
  TEST_ASSERT_FALSE(w.TakeRedrawRequest(t));
}

// redraw_requested(Wi-Fi 아이콘 변화)는 갱신할 것이 없어도 한 번 화면 판정을 일으키고 곧 해제된다.
void test_redraw_request_triggers_one_render() {
  app::AppState st; st.settings=settings::DefaultSettings();
  app::Snapshot sn; app::KisSession kis; FakeStorage storage; FakeHttp http; CycleFake p;
  p.has_render=true;           // 이미 한 번 그려 둠
  app::RunCycle(power::CycleWake::Timer,ButtonEvent::None,st,sn,kis,&http,storage,0,"test",p);
  const int base=p.renders;
  app::RunCycle(power::CycleWake::Timer,ButtonEvent::None,st,sn,kis,&http,storage,0,"test",p);
  TEST_ASSERT_EQUAL(base,p.renders);          // 요청이 없으면 그대로
  st.redraw_requested=true;
  app::RunCycle(power::CycleWake::Timer,ButtonEvent::None,st,sn,kis,&http,storage,0,"test",p);
  TEST_ASSERT_EQUAL(base+1,p.renders);
  TEST_ASSERT_FALSE(st.redraw_requested);
  app::RunCycle(power::CycleWake::Timer,ButtonEvent::None,st,sn,kis,&http,storage,0,"test",p);
  TEST_ASSERT_EQUAL(base+1,p.renders);
}

// 코스피 지수는 스냅샷에 저장돼 재부팅·절전 복귀·장 마감 후에도 시장 탭에 남아야 한다.
void test_kospi_survives_snapshot_roundtrip() {
  app::Snapshot a;
  a.has_kospi = true;
  a.kospi.quote.has_value = true;
  a.kospi.quote.validity = Validity::Valid;
  a.kospi.quote.source = "한국투자증권";
  strcpy(a.kospi.quote.value.name, "KOSPI");
  a.kospi.quote.value.value_x100 = 537168;
  a.kospi.quote.value.has_change = true;
  a.kospi.quote.value.change_bp = -253;
  a.kospi.quote.value.at.epoch_utc = Kst("2026-11-02 15:20");
  a.kospi.quote.value.at.verified = true;
  a.kospi.quote.observed_at = a.kospi.quote.value.at;
  a.kospi.outcome = Outcome::Ok;
  uint8_t buf[3072];
  const size_t n = snapshot_codec::EncodeSnapshot(a, buf, sizeof(buf));
  TEST_ASSERT_TRUE(n > 0);
  app::Snapshot b;
  TEST_ASSERT_TRUE(snapshot_codec::DecodeSnapshot(buf, n, b));
  TEST_ASSERT_TRUE(b.has_kospi);
  TEST_ASSERT_TRUE(b.kospi.quote.has_value);
  TEST_ASSERT_EQUAL_STRING("KOSPI", b.kospi.quote.value.name);
  TEST_ASSERT_EQUAL_INT64(537168, b.kospi.quote.value.value_x100);
  TEST_ASSERT_TRUE(b.kospi.quote.value.has_change);
  TEST_ASSERT_EQUAL(-253, b.kospi.quote.value.change_bp);
  TEST_ASSERT_EQUAL_INT64(Kst("2026-11-02 15:20"), b.kospi.quote.value.at.epoch_utc);
  TEST_ASSERT_EQUAL_STRING("한국투자증권", b.kospi.quote.source);
  TEST_ASSERT_EQUAL((int)Validity::Valid, (int)b.kospi.quote.validity);

  // 복원된 스냅샷으로 시장 탭 뷰모델을 만들면 코스피 행이 나온다.
  app::AppState st; st.settings = settings::DefaultSettings();
  vm::MarketPage p = vm::BuildMarket(b, st, Kst("2026-11-02 20:00"), true, true, "장마감", false);
  TEST_ASSERT_TRUE(p.has_index);
  TEST_ASSERT_EQUAL_STRING("5,371.68", p.index_value);
  TEST_ASSERT_EQUAL_STRING("-2.53%", p.index_chg);

  // 코스피가 없는 스냅샷도 그대로 복원된다(has_kospi=false).
  app::Snapshot none, back;
  const size_t m = snapshot_codec::EncodeSnapshot(none, buf, sizeof(buf));
  TEST_ASSERT_TRUE(snapshot_codec::DecodeSnapshot(buf, m, back));
  TEST_ASSERT_FALSE(back.has_kospi);

  // 이전 형식(v2: 코스피 필드 없음)으로 저장된 스냅샷도 계속 읽힌다.
  TEST_ASSERT_TRUE(m >= 3);
  buf[0] = 2; buf[1] = 0;                  // 버전 2
  app::Snapshot old;
  TEST_ASSERT_TRUE(snapshot_codec::DecodeSnapshot(buf, m - 1, old));  // 끝의 has_kospi 1바이트 제거
  TEST_ASSERT_FALSE(old.has_kospi);
  // 잘리거나 값이 깨진 코스피는 거부한다.
  const size_t t = snapshot_codec::EncodeSnapshot(a, buf, sizeof(buf));
  app::Snapshot bad;
  TEST_ASSERT_FALSE(snapshot_codec::DecodeSnapshot(buf, t - 3, bad));
}

// 런타임 저장소(NVS) 체크포인트를 거쳐도 코스피가 복원된다.
void test_kospi_survives_runtime_checkpoint() {
  FakeStorage storage; RuntimeStore store;
  app::AppState st; st.settings = FullSettings(); st.time_verified = true;
  app::Snapshot sn;
  sn.has_kospi = true; sn.kospi.quote.has_value = true; sn.kospi.quote.validity = Validity::Valid;
  strcpy(sn.kospi.quote.value.name, "KOSPI"); sn.kospi.quote.value.value_x100 = 261543;
  sn.kospi.quote.value.at.epoch_utc = Kst("2026-11-02 15:20");
  sn.kospi.quote.source = "한국투자증권";
  const int64_t now = Kst("2026-11-02 15:30");
  st.last_fetch_utc = now;
  st.providers[(int)Provider::KisPrice].last_success_utc = now;
  TEST_ASSERT_TRUE(store.Checkpoint(storage, st, sn, now, true));
  app::AppState st2; st2.settings = st.settings; app::Snapshot restored; RuntimeStore reader;
  TEST_ASSERT_TRUE(reader.Restore(storage, st2.settings, st2, restored));
  TEST_ASSERT_TRUE(restored.has_kospi);
  TEST_ASSERT_EQUAL_INT64(261543, restored.kospi.quote.value.value_x100);
}

// 가장 큰 스냅샷(전부 채움 + 코스피)이 RTC 버퍼 3072와 런타임 저장 상한 안에 들어간다.
void test_full_snapshot_with_kospi_fits_buffers() {
  app::Snapshot s; demo::FillDemo(s, 1700000000LL);
  for (size_t i = 0; i < kMaxWatchlist; i++) {
    s.has_fsc[i] = s.has_kis[i] = true;
    s.fsc[i].quote.has_value = s.kis[i].quote.has_value = true;
    strcpy(s.fsc[i].quote.value.symbol, "005930"); strcpy(s.kis[i].quote.value.symbol, "005930");
    strcpy(s.fsc[i].quote.value.market, "통합"); strcpy(s.kis[i].quote.value.market, "통합");
    s.fsc[i].quote.source = "금융위원회"; s.kis[i].quote.source = "한국투자증권";
  }
  s.has_kospi = true; s.kospi.quote.has_value = true; s.kospi.quote.source = "한국투자증권";
  strcpy(s.kospi.quote.value.name, "KOSPI"); s.kospi.quote.value.value_x100 = 537168;
  std::vector<uint8_t> buf(3072);
  const size_t n = snapshot_codec::EncodeSnapshot(s, buf.data(), buf.size());
  TEST_ASSERT_TRUE(n > 0 && n < 2800);  // 여유를 남긴다
}

// 장 마감 뒤 확정값 1회: 마지막 성공이 직전 마감(15:30) 이전이면 마감 직후 한 번 더 받는다.
// (재부팅·재플래시로 복원한 값이 장중 값이거나 코스피가 비어 있을 때 마감값으로 채운다.)
void test_kis_post_close_catchup() {
  app::AppState st; SetupKisOnly(st,1);
  const int64_t mon_close = Kst("2026-11-02 15:30");
  // 월요일 14:00에 마지막 성공 → 16:00에는 즉시 대상.
  app::MarkResult(st,Provider::KisPrice,Outcome::Ok,Kst("2026-11-02 14:00"),true,-1,0);
  const int64_t t = Kst("2026-11-02 16:00");
  TEST_ASSERT_EQUAL_INT64(t, app::NextAttemptUtc(st,Provider::KisPrice,t,true));
  // 마감 시각 이전(15:29)에는 일반 규칙(5분 간격)이라 이 규칙과 무관.
  // 마감 후 성공하면(15:45) 다음 거래일 09:00까지 대기.
  app::MarkResult(st,Provider::KisPrice,Outcome::Ok,Kst("2026-11-02 15:45"),true,-1,0);
  TEST_ASSERT_EQUAL_INT64(Kst("2026-11-03 09:00"), app::NextAttemptUtc(st,Provider::KisPrice,Kst("2026-11-02 16:00"),true));
  TEST_ASSERT_TRUE(Kst("2026-11-02 15:45") >= mon_close);
  // 성공 이력이 없어도(설정 저장·재플래시로 캐시가 빈 경우) 마감 후 1회 받는다(ADR 0026, ISSUE-008).
  app::AppState fresh; SetupKisOnly(fresh,1);
  TEST_ASSERT_EQUAL_INT64(Kst("2026-11-02 16:00"), app::NextAttemptUtc(fresh,Provider::KisPrice,Kst("2026-11-02 16:00"),true));
  // 주말: 직전 거래일(금) 마감 이전에 성공했다면 토요일에도 1회 대상.
  app::AppState wk; SetupKisOnly(wk,1);
  app::MarkResult(wk,Provider::KisPrice,Outcome::Ok,Kst("2026-10-30 14:00"),true,-1,0);
  const int64_t sat = Kst("2026-10-31 10:00");
  TEST_ASSERT_EQUAL_INT64(sat, app::NextAttemptUtc(wk,Provider::KisPrice,sat,true));
  // 실패 백오프 중에는 마감 후 대상이어도 Retry-After를 넘지 않는다.
  app::AppState bo; SetupKisOnly(bo,1);
  app::MarkResult(bo,Provider::KisPrice,Outcome::Ok,Kst("2026-11-02 14:00"),true,-1,0);
  app::MarkResult(bo,Provider::KisPrice,Outcome::ProviderError,Kst("2026-11-02 16:00"),true,-1,0);
  TEST_ASSERT_TRUE(app::NextAttemptUtc(bo,Provider::KisPrice,Kst("2026-11-02 16:00")+1,true) > Kst("2026-11-02 16:00")+1);
  // 마감 후 캐치업 요청이 실제 HTTP로 이어지고 코스피가 채워진다.
  app::AppState cu; SetupKisOnly(cu,1);
  app::MarkResult(cu,Provider::KisPrice,Outcome::Ok,Kst("2026-11-02 14:00"),true,-1,0);
  app::Snapshot snap; app::KisSession kis; FakeHttp http; app::PassLog log;
  http.On("oauth2/tokenP",200,"kis_token_ok.json");
  http.On("inquire-price",200,"kis_ok.json");
  http.On("inquire-index-price",200,"kis_index_ok.json");
  app::RunFetchPass(cu,snap,kis,http,net::kCaBundlePem,Ctx("2026-11-02 16:00"),true,0,&log);
  TEST_ASSERT_TRUE(snap.has_kospi);
  TEST_ASSERT_EQUAL(1,log.count);
}
