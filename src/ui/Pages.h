#pragma once
// Snapshot model: what the renderer draws (produced by Coordinator from the
// domain DataPoints; UI never touches providers or HTTPS).
#include <stddef.h>

#include "../domain/Types.h"

namespace kdash {
namespace ui {

struct LifeView {
  DataPoint<double> temp_c;
  DataPoint<double> humidity;
  DataPoint<int> pop;
  DataPoint<int> pm10;
  DataPoint<int> pm25;
  DataPoint<double> indoor_c;
  DataPoint<double> indoor_rh;
  const char* district = "";
  bool demo = false;
};

struct MarketRow {
  const char* label = "";  // "USD", "JPY(100)", 종목코드
  const char* kind_label = "";  // "고시", "전일 종가", "현재가"
  DataPoint<FxRate> fx;
  DataPoint<StockQuote> stock;
  bool is_fx = true;
};

struct MarketView {
  MarketRow rows[4];
  size_t n = 0;
  const char* market_state = "";  // "장중"/"장마감"/"휴장"/"정규장 시간"/"일정 미확인"
  bool demo = false;
};

struct DeviceView {
  bool wifi_ok = false;
  const char* last_success[4] = {"", "", "", ""};  // per-provider KST strings
  int battery_pct = -1;                            // -1 => 알 수 없음
  bool usb = false;
  const char* page_hint = "";
  bool time_ok = false;
};

}  // namespace ui
}  // namespace kdash
