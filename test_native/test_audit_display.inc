#include "../src/drivers/Uc8179.h"
#include "../src/domain/StockNames.h"
#include <algorithm>

namespace {
class TestPanelPort : public kdash::epd::PanelPort {
 public:
  uint32_t tick=0;
  bool ready=true;
  bool otp=false;
  bool use_otp_lut() const override { return otp; }
  std::vector<uint8_t> commands;
  std::vector<uint8_t> resolution;
  size_t planes[2]={0,0};
  uint8_t first[2]={0,0};
  uint8_t cmd=0;
  int resets=0;
  int data_sessions=0, data_ends=0;
  bool streaming=false;
  uint32_t row_ms=0;
  uint32_t now_ms() override { return tick; }
  void pause_ms(uint32_t ms) override { tick+=ms; }
  void reset(bool) override { resets++; }
  bool idle() override { return ready; }
  void command(uint8_t c) override { cmd=c; commands.push_back(c); }
  void begin_data() override { streaming=true; ++data_sessions; }
  void end_data() override { streaming=false; ++data_ends; }
  void data(const uint8_t* p,size_t n) override {
    if (cmd==0x61) resolution.insert(resolution.end(),p,p+n);
    if (cmd==0x10 || cmd==0x13) {
      const int i=cmd==0x10?0:1;
      if (!planes[i] && n) first[i]=p[0];
      planes[i]+=n;
      tick+=row_ms;
    }
  }
};
}

void test_uc8179_frame_and_timeout() {
  std::vector<uint8_t> fb(48000,0);
  fb[0]=0xA5;
  TestPanelPort p;
  TEST_ASSERT_TRUE(kdash::epd::PushMonoFrame(p,fb.data()));
  TEST_ASSERT_EQUAL(2,p.resets);
  TEST_ASSERT_EQUAL(48000,p.planes[0]);
  TEST_ASSERT_EQUAL(48000,p.planes[1]);
  TEST_ASSERT_EQUAL_HEX8(0xA5,p.first[0]);
  TEST_ASSERT_EQUAL_HEX8(0xA5,p.first[1]);
  const uint8_t dims[]={0x03,0x20,0x01,0xE0};
  TEST_ASSERT_EQUAL(4,p.resolution.size());
  TEST_ASSERT_EQUAL_MEMORY(dims,p.resolution.data(),4);
  TEST_ASSERT_EQUAL_HEX8(0x07,p.commands.back());
  const auto refresh = std::find(p.commands.begin(), p.commands.end(), 0x12);
  const auto power_off = std::find(p.commands.begin(), p.commands.end(), 0x02);
  TEST_ASSERT_TRUE(refresh != p.commands.end() && power_off != p.commands.end());
  TEST_ASSERT_TRUE(refresh < power_off);  // frame must finish its power cycle
  // Same live port accepts another complete frame after reset from deep sleep.
  TEST_ASSERT_TRUE(kdash::epd::PushMonoFrame(p,fb.data()));
  TEST_ASSERT_EQUAL(4,p.resets);
  TEST_ASSERT_EQUAL(96000,p.planes[0]);
  TEST_ASSERT_EQUAL(96000,p.planes[1]);
  TEST_ASSERT_EQUAL(4,p.data_sessions);
  TEST_ASSERT_EQUAL(4,p.data_ends);
  TEST_ASSERT_FALSE(p.streaming);
  TEST_ASSERT_TRUE(kdash::epd::SleepPanel(p));
  TEST_ASSERT_EQUAL_HEX8(0x07,p.commands.back());
  TestPanelPort busy;
  busy.ready=false;
  TEST_ASSERT_FALSE(kdash::epd::PushMonoFrame(busy,fb.data(),100));
  TEST_ASSERT_TRUE(busy.tick<=110);
  TEST_ASSERT_EQUAL(0,busy.planes[0]);
  TEST_ASSERT_FALSE(kdash::epd::PushMonoFrame(p,nullptr));
  TestPanelPort slow;
  slow.row_ms=10;
  TEST_ASSERT_FALSE(kdash::epd::PushMonoFrame(slow,fb.data(),250));
  TEST_ASSERT_EQUAL(1,slow.data_sessions);
  TEST_ASSERT_EQUAL(slow.data_sessions,slow.data_ends);
  TEST_ASSERT_FALSE(slow.streaming); // failed/deadline path must release CS
}

void test_bitmap_raster_real_glyphs() {
  kdash::ui::Canvas c;
  const kdash::font::GlyphInfo glyphs[]={{'A',0,3,2,1,1,5}};
  const uint8_t bits[]={0xA0,0x40};  // explicit independent 101/010 oracle
  const kdash::font::BitmapFont tiny={glyphs,1,bits,2,4,3,'A'};
  c.draw_text(tiny,20,20,"A");
  TEST_ASSERT_TRUE(c.pixel(21,21));
  TEST_ASSERT_FALSE(c.pixel(22,21));
  TEST_ASSERT_TRUE(c.pixel(23,21));
  TEST_ASSERT_TRUE(c.pixel(22,22));
  TEST_ASSERT_FALSE(c.pixel(21,22));
  TEST_ASSERT_EQUAL(0,kdash::font::MissingGlyphs(kdash::ui::DefaultMedium(),"기온 서울 종로구"));
  c.clear();
  c.draw_text(kdash::ui::DefaultMedium(),20,20,"기");
  const uint32_t a=c.crc32();
  c.clear();
  c.draw_text(kdash::ui::DefaultMedium(),20,20,"온");
  TEST_ASSERT_NOT_EQUAL(a,c.crc32());
  c.clear();
  c.draw_text(kdash::ui::DefaultMedium(),20,20," ");
  TEST_ASSERT_EQUAL_FLOAT(0,c.black_ratio(20,20,40,30));
}

void test_four_gray_raster_and_wire_planes() {
  kdash::ui::Canvas c;
  const kdash::font::GlyphInfo glyphs[]={{'A',0,4,1,0,0,4}};
  const uint8_t bits[]={0x1B}; // 00,01,10,11: independent coverage oracle
  const kdash::font::BitmapFont tiny={glyphs,1,bits,1,1,1,'A',2};
  c.draw_text(tiny,0,0,"A");
  for (int x=0;x<4;++x) TEST_ASSERT_EQUAL(x,c.coverage(x,0));
  c.invert_rect(0,0,4,1);
  for (int x=0;x<4;++x) TEST_ASSERT_EQUAL(3-x,c.coverage(x,0));
  c.clear();
  c.draw_text(tiny,-2,0,"A");
  TEST_ASSERT_EQUAL(2,c.coverage(0,0));
  TEST_ASSERT_EQUAL(3,c.coverage(1,0));
  TEST_ASSERT_EQUAL(0,c.coverage(-1,0));
  std::vector<uint8_t> fb(96000,0x1B);
  TestPanelPort legacy;
  TEST_ASSERT_TRUE(kdash::epd::PushGrayFrame(legacy,fb.data()));
  TEST_ASSERT_EQUAL(48000,legacy.planes[0]);
  TEST_ASSERT_EQUAL(48000,legacy.planes[1]);
  TEST_ASSERT_EQUAL_HEX8(0x55,legacy.first[0]);
  TEST_ASSERT_EQUAL_HEX8(0x33,legacy.first[1]);
  TestPanelPort otp;
  otp.otp=true;
  TEST_ASSERT_TRUE(kdash::epd::PushGrayFrame(otp,fb.data()));
  // E1001 hardware (2026-11-11): OTP waveform keeps 00 white / 11 black;
  // the Seeed_GFX 11-white convention displayed an inverted frame (ADR 0012).
  TEST_ASSERT_EQUAL_HEX8(0x55,otp.first[0]);
  TEST_ASSERT_EQUAL_HEX8(0x33,otp.first[1]);
  std::vector<uint8_t> white(96000,0x00);
  TestPanelPort otp_white;
  otp_white.otp=true;
  TEST_ASSERT_TRUE(kdash::epd::PushGrayFrame(otp_white,white.data()));
  TEST_ASSERT_EQUAL_HEX8(0x00,otp_white.first[0]);
  TEST_ASSERT_EQUAL_HEX8(0x00,otp_white.first[1]);
  TEST_ASSERT_TRUE(std::find(otp.commands.begin(),otp.commands.end(),0x20)==otp.commands.end());
  TEST_ASSERT_TRUE(std::find(otp.commands.begin(),otp.commands.end(),0xE5)!=otp.commands.end());
}

void test_indoor_last_good_and_codec() {
  kdash::app::AppState st;
  kdash::app::Snapshot snap;
  auto ctx=Ctx("2026-11-02 12:00");
  kdash::app::RecordIndoor(st,snap,true,22.5,45,ctx,true);
  TEST_ASSERT_TRUE(snap.indoor_c.has_value);
  TEST_ASSERT_EQUAL_FLOAT(22.5f,(float)snap.indoor_c.value);
  kdash::app::RecordIndoor(st,snap,false,0,0,ctx,true);
  TEST_ASSERT_EQUAL_FLOAT(22.5f,(float)snap.indoor_c.value);
  const auto page=kdash::vm::BuildLife(snap,st,ctx.now_utc,true,false,false);
  TEST_ASSERT_EQUAL((int)kdash::vm::Badge::LastGood,(int)page.indoor.badge);
  uint8_t payload[4096];
  const size_t n=kdash::snapshot_codec::EncodeSnapshot(snap,payload,sizeof(payload));
  TEST_ASSERT_TRUE(n>0);
  kdash::app::Snapshot back;
  TEST_ASSERT_TRUE(kdash::snapshot_codec::DecodeSnapshot(payload,n,back));
  TEST_ASSERT_EQUAL_FLOAT(22.5f,(float)back.indoor_c.value);
  ctx.now_utc+=3600;
  kdash::app::RefreshFreshness(back,ctx);
  TEST_ASSERT_EQUAL((int)kdash::Validity::Stale,(int)back.indoor_c.validity);
}

void test_market_all_watch_items_and_long_place() {
  kdash::app::AppState st;
  st.settings=FullSettings();
  st.settings.watch_count=kdash::kMaxWatchlist;
  kdash::app::Snapshot snap;
  const auto ctx=Ctx("2026-11-02 12:00");
  kdash::demo::FillDemo(snap,ctx.now_utc);
  for (size_t i=0;i<kdash::kMaxWatchlist;i++) {
    snprintf(st.settings.watchlist[i].symbol,7,"%06u",(unsigned)(i+1));
    snap.has_fsc[i]=true;
    snap.fsc[i].quote.has_value=true;
    snap.fsc[i].quote.validity=kdash::Validity::Valid;
    snap.fsc[i].quote.value.price_krw=10000+(int64_t)i;
  }
  const auto page=kdash::vm::BuildMarket(snap,st,ctx.now_utc,true,true,"휴장 가능",true);
  TEST_ASSERT_EQUAL(6,page.n);
  TEST_ASSERT_EQUAL(2,page.fx_count);
  TEST_ASSERT_EQUAL_STRING("000004",page.rows[5].label);
  kdash::ui::Canvas c;
  kdash::ui::RenderMarket(page,c);
  TEST_ASSERT_TRUE(kdash::ui::CheckMarginsClean(c));
  const char* name="세종특별자치시 어진동 한누리대로";
  kdash::settings::CopyBounded(st.settings.location.display_name,
                              sizeof(st.settings.location.display_name),name);
  const auto life=kdash::vm::BuildLife(snap,st,ctx.now_utc,true,true,true);
  TEST_ASSERT_EQUAL_STRING(name,life.header.place);
  kdash::ui::RenderLife(life,c);
  TEST_ASSERT_TRUE(kdash::ui::CheckMarginsClean(c));
}

void test_stock_names_table_and_glyphs() {
  using namespace kdash;
  TEST_ASSERT_EQUAL_STRING("삼성전자", stocks::NameForCode("005930"));
  TEST_ASSERT_EQUAL_STRING("SK하이닉스", stocks::NameForCode("000660"));
  TEST_ASSERT_NULL(stocks::NameForCode("999999"));
  TEST_ASSERT_NULL(stocks::NameForCode(nullptr));
  TEST_ASSERT_NULL(stocks::NameForCode(""));
  const auto& f = ui::DefaultMedium();
  for (size_t i = 0; i < stocks::NameCount(); ++i) {
    const auto& e = stocks::NameAt(i);
    if (i) TEST_ASSERT_TRUE(strcmp(stocks::NameAt(i - 1).code, e.code) < 0);  // sorted: binary search
    TEST_ASSERT_EQUAL_STRING(e.name, stocks::NameForCode(e.code));
    ui::Canvas c;
    c.draw_text(f, 0, 0, e.name);
    TEST_ASSERT_EQUAL_MESSAGE(0, c.missing_glyphs(), e.name);  // no □ fallback
    TEST_ASSERT_TRUE(c.measure(f, e.name) <= 190);  // fits the name column
  }
  // View model carries the name; unknown codes keep the code only.
  app::AppState st; app::Snapshot snap;
  st.settings.watch_count = 2;
  snprintf(st.settings.watchlist[0].symbol, 7, "%s", "005930");
  snprintf(st.settings.watchlist[1].symbol, 7, "%s", "999999");
  vm::MarketPage mp = vm::BuildMarket(snap, st, 1800000000, true, true, "장중", false);
  TEST_ASSERT_EQUAL_STRING("삼성전자", mp.rows[mp.fx_count].name);
  TEST_ASSERT_EQUAL_STRING("", mp.rows[mp.fx_count + 1].name);
  TEST_ASSERT_EQUAL_STRING("999999", mp.rows[mp.fx_count + 1].label);
}
