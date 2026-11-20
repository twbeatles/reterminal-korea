#pragma once
// UI layout contracts (docs/05): 800x480, safe margins, header/footer bands,
// two-column body, font-size ladder, DEMO/stale badges, page ids.
// Pixel-type agnostic — the ESP32 raster backend and the host Pillow preview
// implement the same geometry; geometry tests assert it on both sides.
#include "../../include/IDisplay.h"

namespace kdash {
namespace ui {

inline constexpr int kMarginX = 20;
inline constexpr int kHeaderTop = 12;
inline constexpr int kHeaderBottom = 48;
inline constexpr int kBodyTop = 58;
inline constexpr int kBodyBottom = 434;
inline constexpr int kFooterTop = 440;
inline constexpr int kFooterBottom = 468;
inline constexpr int kColGap = 16;
inline constexpr int kLeftW = 372;   // x=20..392
inline constexpr int kRightX = 408;  // x=408..780
inline constexpr int kColW = 372;

// 순서 = 버튼으로 넘기는 순서. 저장된 페이지 번호는 이 값이므로 바꿀 때 주의(기기 탭이 3으로 이동).
enum class Page : int { Life = 0, Market = 1, Calendar = 2, Device = 3, Count = 4 };

// Fixed Korean labels (docs/05 필수 문구). Dynamic numbers never replace these.
inline constexpr const char* kLTemp = "기온";
inline constexpr const char* kLRain = "강수확률";
inline constexpr const char* kLPm10 = "미세먼지";
inline constexpr const char* kLPm25 = "초미세먼지";
inline constexpr const char* kLIndoor = "실내";
inline constexpr const char* kLFx = "환율(고시)";
inline constexpr const char* kLClose = "전일 종가";
inline constexpr const char* kLLive = "현재가";
inline constexpr const char* kLReceived = "마지막 수신";
inline constexpr const char* kLObserved = "관측";
inline constexpr const char* kLStale = "오래된 정보";
inline constexpr const char* kLCheck = "확인 필요";
inline constexpr const char* kLNoWifi = "Wi-Fi 미연결";
inline constexpr const char* kLDemo = "DEMO";
inline constexpr const char* kLNeedSetup = "설정 필요";
inline constexpr const char* kLLkg = "마지막 정상값";

// Header/footer rectangles (800x480 geometry regression anchors).
inline Rect HeaderRect() { return {kMarginX, kHeaderTop, 760, 36}; }
inline Rect BodyLeftRect() { return {kMarginX, kBodyTop, kLeftW, 376}; }
inline Rect BodyRightRect() { return {kRightX, kBodyTop, kColW, 376}; }
inline Rect FooterRect() { return {kMarginX, kFooterTop, 760, 28}; }

// Truncation keeps unit/기준일: caller passes the protected suffix separately.
bool NeedsEllipsis(int text_advance_px, int box_w_px);
int FitChars(int text_len, int avg_advance_px, int box_w_px);

}  // namespace ui
}  // namespace kdash
