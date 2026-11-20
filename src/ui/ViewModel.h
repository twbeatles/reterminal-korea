#pragma once
// ViewModel: Snapshot + AppState + clock -> formatted text cells (T04a).
// Pure functions; the renderer only draws strings. Portable.
#include <stddef.h>
#include <stdint.h>

#include "../app/Coordinator.h"
#include "../app/Fetcher.h"
#include "icons/IconData.h"

namespace kdash {
namespace vm {

enum class Badge : uint8_t {
  None = 0,
  Stale,     // 오래된 정보
  LastGood,  // 마지막 정상값
  NeedSetup, // 설정 필요
  Check,     // 확인 필요
  Demo,      // DEMO
};

struct Cell {
  char label[32] = {0};
  char value[48] = {0};
  char unit[24] = {0};
  char sub[64] = {0};
  Badge badge = Badge::None;
  ui::IconId icon = ui::IconId::None;  // 24px card/row icon
};

struct Header {
  char place[kMaxDisplayNameLen + 1] = {0};
  char datetime[32] = {0};  // "11-02(월) 14:30"
  bool wifi_ok = false;
  bool time_ok = false;
};

struct LifePage {
  Header header;
  Cell temp, humidity, rain, forecast, pm10, pm25, sky, indoor;
  // Hero weather icon + "흐림 · 예보 15:00": SKY/PTY exist only in the
  // forecast slot, so the text always names that source time.
  ui::IconId now_icon = ui::IconId::Unknown;
  char now_text[48] = {0};
  char air_sub[64] = {0};  // "측정소 종로구 · 측정 14:00" (PM source time)
  bool demo = false;
};

struct MarketRow {
  char label[16] = {0};   // USD / JPY(100) / 종목코드
  char name[32] = {0};    // 종목명 (오프라인 표, 없으면 비어 있음)
  char kind[16] = {0};    // 고시 / 전일 종가 / 현재가
  char value[32] = {0};
  char sub[64] = {0};     // 기준일 / 조회시각
  Badge badge = Badge::None;
  ui::IconId icon = ui::IconId::None;  // Usd / Jpy / Chart
  char pair[24] = {0};                 // "KRW / USD" (FX rows only)
  char chg[12] = {0};                  // 전일 대비율 "+1.23%" (KIS 현재가 행만, 없으면 비어 있음)
};

struct MarketPage {
  Header header;
  MarketRow rows[kMaxWatchlist + 2];
  size_t n = 0;
  size_t fx_count = 0;
  char state[32] = {0};  // 장중 / 장마감 / 휴장 / 정규장 시간·외 / 미확인
  // 코스피 지수 한 줄(KIS). has_index=false면 그리지 않는다.
  bool has_index = false;
  char index_label[12] = {0};  // "코스피"
  char index_value[24] = {0};  // "5,371.68"
  char index_chg[12] = {0};    // "+2.53%" (없으면 비어 있음)
  char index_sub[40] = {0};    // "14:20 조회 · REST"
  Badge index_badge = Badge::None;
  bool demo = false;
};

// 달력 탭: 한 달 격자 + 오늘 + 다음 공휴일/절기. 공휴일·휴장은 기기가 이미 가진 연도 표,
// 절기는 상수표(2026~2028)에서만 가져오며 모르는 것은 "정보 없음"으로 둔다.
inline constexpr uint8_t kCalHoliday = 1;  // 공휴일(주말과 겹쳐도 표시)
inline constexpr uint8_t kCalClosed = 2;   // 주식시장 추가 휴장일
inline constexpr uint8_t kCalTerm = 4;     // 24절기

struct CalendarPage {
  Header header;
  bool valid = false;        // false: 시계 미확인 등으로 날짜를 신뢰할 수 없음
  bool tables_ok = false;    // 올해 공휴일 표가 로드됨
  int year = 0, month = 0, today = 0;
  uint8_t first_wday = 0;    // 1일의 요일(0=일요일)
  uint8_t days = 0;
  uint8_t flags[32] = {0};   // 1..days
  char title[24] = {0};      // "2026년 11월"
  char day_big[8] = {0};     // "11"
  char weekday[16] = {0};    // "수요일"
  char next_holiday[56] = {0};
  char next_term[40] = {0};
  bool demo = false;
};

CalendarPage BuildCalendar(const app::AppState& st, int64_t now_utc,
                           bool clock_verified, bool wifi_ok, bool demo);

struct DevicePage {
  Header header;
  char wifi[32];
  char clock[32];
  char battery[32];
  char fw[24];
  char charging[80] = {0};
  char providers[(int)sched::Provider::Count][64];  // "이름 · 시각 · 종목 2,4 실패"
  char hint[96];   // page navigation keys
  char hint2[96];  // refresh / setup keys
  char notice[64] = {0};  // why this page was forced (low battery / safe mode)
  // 설정 포털(LAN)이 열려 있을 때만 채운다. 코드는 화면 전용(UART·HTTP 금지).
  char portal_url[40] = {0};   // "설정 http://192.168.0.23"
  char portal_host[96] = {0};  // "또는 kdash-3f2a.local · 8분 남음"
  char portal_code[32] = {0};  // "페어링 코드 482913" / "잠김 · 5분 뒤 새 코드"
  ui::IconId wifi_icon = ui::IconId::WifiOff;
  ui::IconId battery_icon = ui::IconId::None;
};

const char* BadgeText(Badge b);  // "오래된 정보"/"마지막 정상값"/...

LifePage BuildLife(const app::Snapshot& snap, const app::AppState& st,
                   int64_t now_utc, bool clock_verified, bool wifi_ok,
                   bool demo);
MarketPage BuildMarket(const app::Snapshot& snap, const app::AppState& st,
                       int64_t now_utc, bool clock_verified, bool wifi_ok,
                       const char* market_state, bool demo);
DevicePage BuildDevice(const app::AppState& st, int64_t now_utc,
                       bool clock_verified, bool wifi_ok, int battery_mv,
                       const char* fw_version, bool usb,
                       bool safe_mode = false);

}  // namespace vm
}  // namespace kdash
