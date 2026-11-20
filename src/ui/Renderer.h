#pragma once
// Renderer: PageView -> Canvas using Layout geometry (T04c). Portable.
// Fonts are injected (T04b tables); default built-in metrics apply when
// count==0. DEMO/badge rendering included.
#include "Canvas.h"
#include "Layout.h"
#include "ViewModel.h"

namespace kdash {
namespace ui {

struct Fonts {
  font::BitmapFont small;   // 14px aux
  font::BitmapFont medium;  // 18px labels
  font::BitmapFont big;     // 40/64px numbers
};

// Built-in fallback metrics (used when a table is empty).
const font::BitmapFont& DefaultSmall();
const font::BitmapFont& DefaultMedium();
const font::BitmapFont& DefaultBig();

void RenderLife(const vm::LifePage& p, Canvas& c, const Fonts* fonts = nullptr);
void RenderMarket(const vm::MarketPage& p, Canvas& c,
                  const Fonts* fonts = nullptr);
void RenderCalendar(const vm::CalendarPage& p, Canvas& c,
                    const Fonts* fonts = nullptr);
void RenderDevice(const vm::DevicePage& p, Canvas& c,
                  const Fonts* fonts = nullptr);
// Only the temporary local AP password is shown here, never saved secrets.
// 같은 Wi-Fi(LAN) 설정 포털 안내(배터리 전원의 블로킹 포털용). 코드는 패널에만 표시.
void RenderLanPortal(Canvas& c, const char* url, const char* host, const char* code);
void RenderPortal(Canvas& c, const char* ap_name, const char* ap_password,
                  const char* local_url);

// Geometry oracle helpers (independent of Render internals).
bool CheckMarginsClean(const Canvas& c);
bool CheckBadgePresent(const Canvas& c, int x, int y, int w, int h);

}  // namespace ui
}  // namespace kdash
