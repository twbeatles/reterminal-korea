#include "Renderer.h"

#include <stdio.h>
#include <string.h>
#include "Icons.h"
#include "fonts/font_kr_14.h"
#include "fonts/font_kr_18.h"
#include "fonts/font_kr_64.h"

namespace kdash {
namespace ui {
namespace {
constexpr uint8_t kCalHolidayMark = vm::kCalHoliday, kCalClosedMark = vm::kCalClosed, kCalTermMark = vm::kCalTerm;

// Card geometry (ADR 0008). Every card stays inside its column, so the
// 392..408 gap and the x<20 margin never receive pixels.
constexpr int kHeadH = 38;    // 24px icon + title + rule + gap
constexpr int kHeroX = 76;    // big value / text column next to a 64px icon
constexpr int kLowerCard = 318;
constexpr int kRowH = 36;

const font::BitmapFont& Def(uint8_t lh) {
  if (lh <= 14) return fonts::kfont_kr_14;
  if (lh <= 18) return fonts::kfont_kr_18;
  return fonts::kfont_kr_64;
}

const font::BitmapFont& Pick(const Fonts* fs, int want) {
  if (fs) {
    if (want <= 14 && fs->small.line_height) return fs->small;
    if (want <= 18 && fs->medium.line_height) return fs->medium;
    if (fs->big.line_height) return fs->big;
  }
  return Def((uint8_t)want);
}

struct Pen {
  Canvas& c;
  const font::BitmapFont& s;  // 14px
  const font::BitmapFont& m;  // 18px
  const font::BitmapFont& b;  // 64px digits
};

void Text(Canvas& c, const font::BitmapFont& f, int x, int y, int w,
          const char* text, const char* suffix = "") {
  char fitted[256];
  FitText(f, text, suffix, w, fitted, sizeof(fitted));
  c.draw_text(f, x, y, fitted);
}

void HLine(Canvas& c, int x, int y, int w) {
  for (int i = 0; i < w; i++) {
    if (x + i >= 392 && x + i < 408) continue;  // keep the column gap clean
    c.pixel(x + i, y, true);
  }
}

// Inverted label at the right edge of [x, x+w); returns its left x.
int DrawBadge(const Pen& p, int x, int y, int w, vm::Badge b) {
  if (b == vm::Badge::None) return x + w;
  const char* t = vm::BadgeText(b);
  int bw = p.c.measure(p.s, t) + 10;
  if (bw > w) bw = w;
  p.c.fill_rect(x + w - bw, y, bw, 18, false);
  p.c.draw_text(p.s, x + w - bw + 5, y + 2, t);
  p.c.invert_rect(x + w - bw, y, bw, 18);
  return x + w - bw;
}

// 24px icon + 18px title + rule; the badge sits at the right of the head.
void DrawCardHead(const Pen& p, int x, int y, int w, IconId icon,
                  const char* title, vm::Badge b = vm::Badge::None) {
  DrawIcon(p.c, icon, IconSize::S24, x, y);
  const int bx = DrawBadge(p, x, y + 3, w, b);
  Text(p.c, p.m, x + 30, y + 3, bx - x - 36, title);
  HLine(p.c, x, y + 28, w);
}

// Leading run that the 64px digit font can draw ("21.5°C", "1,384.50", "--",
// "18" of "18 ㎍/㎥ 좋음"); the rest goes to the 18px line below. Falls back
// to 18px entirely when the big run does not fit. Returns the bottom y.
int DrawBigValue(const Pen& p, int x, int y, int w, const char* text) {
  char big[48] = {0};
  const char* q = text ? text : "";
  size_t n = 0;
  while (*q) {
    const char* next = q;
    const uint32_t cp = font::Utf8Next(next);
    const font::GlyphInfo* g = font::FindGlyph(p.b, cp);
    if (cp == ' ' || !g || g->cp != cp) break;  // exact glyph, not □
    const size_t len = (size_t)(next - q);
    if (n + len >= sizeof(big)) break;
    memcpy(big + n, q, len);
    n += len;
    q = next;
  }
  while (*q == ' ') q++;
  if (n == 0 || p.c.measure(p.b, big) > w) {
    Text(p.c, p.m, x, y, w, text);
    return y + 24;
  }
  p.c.draw_text(p.b, x, y, big);
  if (!*q) return y + 70;
  Text(p.c, p.m, x, y + 72, w, q);  // 8px air under 64px digits
  return y + 96;
}

// Icon row: 24px icon, label, value (value keeps its unit), badge at right.
void DrawIconRow(const Pen& p, int x, int y, int w, const vm::Cell& cell) {
  DrawIcon(p.c, cell.icon, IconSize::S24, x, y);
  const int bx = DrawBadge(p, x, y + 3, w, cell.badge);
  Text(p.c, p.m, x + 30, y + 3, 100, cell.label);
  Text(p.c, p.m, x + 136, y + 3, bx - x - 142, cell.value);
}

// Card body for one cell: value (18px) + sub (14px).
void DrawCellBody(const Pen& p, int x, int y, int w, const vm::Cell& cell) {
  Text(p.c, p.m, x, y, w, cell.value);
  if (cell.sub[0]) Text(p.c, p.s, x, y + 26, w, cell.sub);
}

void DrawHeader(const Pen& p, const vm::Header& h) {
  Rect hr = HeaderRect();
  Text(p.c, p.m, hr.x, hr.y + 4, 360, h.place);
  const int dx = kRightX;
  const int dw = p.c.draw_text(p.m, dx, hr.y + 4, h.datetime);
  if (!h.time_ok) {
    // "확인 필요" alone is ambiguous in the header: name the clock.
    const char* t = "시간 확인 필요";
    int bw = p.c.measure(p.s, t) + 10;
    const int bx = (748 - bw) > dx + dw + 8 ? 748 - bw : dx + dw + 8;
    p.c.fill_rect(bx, hr.y + 4, bw, 18, false);
    p.c.draw_text(p.s, bx + 5, hr.y + 6, t);
    p.c.invert_rect(bx, hr.y + 4, bw, 18);
  }
  DrawIcon(p.c, h.wifi_ok ? IconId::Wifi : IconId::WifiOff, IconSize::S24,
           756, 14);
  HLine(p.c, kMarginX, kHeaderBottom + 4, 760);
}

void DrawFooter(const Pen& p, uint8_t page, bool demo, const char* source) {
  static const char* kDots[] = {"●○○○", "○●○○", "○○●○", "○○○●"};
  static const char* kNames[] = {"생활", "시장", "달력", "기기"};
  Rect fr = FooterRect();
  HLine(p.c, kMarginX, kFooterTop - 4, 760);
  int x = fr.x;
  x += p.c.draw_text(p.m, x, fr.y + 4, kDots[page % 4]) + 8;
  x += p.c.draw_text(p.m, x, fr.y + 4, kNames[page % 4]) + 10;
  if (demo) {
    const int bw = p.c.measure(p.s, kLDemo) + 10;
    DrawBadge(p, x, fr.y + 4, bw, vm::Badge::Demo);
  }
  if (source && source[0]) Text(p.c, p.s, kRightX, fr.y + 6, 280, source);
}

}  // namespace

const font::BitmapFont& DefaultSmall() { return Def(14); }
const font::BitmapFont& DefaultMedium() { return Def(18); }
const font::BitmapFont& DefaultBig() { return Def(64); }

void RenderLife(const vm::LifePage& p, Canvas& c, const Fonts* fonts) {
  c.clear(true);
  const Pen pen{c, Pick(fonts, 14), Pick(fonts, 18), Pick(fonts, 64)};
  DrawHeader(pen, p.header);
  const Rect L = BodyLeftRect(), R = BodyRightRect();

  // Left: current weather (hero icon + big temperature) and detail rows.
  DrawCardHead(pen, L.x, L.y, L.w, IconId::Thermometer, "현재 날씨",
               p.temp.badge);
  int y = L.y + kHeadH;
  DrawIcon(c, p.now_icon, IconSize::L64, L.x, y);
  DrawBigValue(pen, L.x + kHeroX, y, L.w - kHeroX, p.temp.value);
  Text(c, pen.m, L.x + kHeroX, y + 74, L.w - kHeroX, p.now_text);
  if (p.temp.sub[0]) Text(c, pen.s, L.x + kHeroX, y + 100, L.w - kHeroX, p.temp.sub);
  y = L.y + 172;
  DrawIconRow(pen, L.x, y, L.w, p.humidity);
  DrawIconRow(pen, L.x, y + kRowH, L.w, p.rain);

  // Left lower: next forecast slot.
  DrawCardHead(pen, L.x, kLowerCard, L.w, p.forecast.icon, "3시간 예보",
               p.forecast.badge);
  DrawCellBody(pen, L.x, kLowerCard + kHeadH, L.w, p.forecast);

  // Right: air quality, PM10 / PM2.5 side by side with big numbers.
  DrawCardHead(pen, R.x, R.y, R.w, IconId::Air, "대기질");
  const int half = R.w / 2;
  const vm::Cell* pm[] = {&p.pm10, &p.pm25};
  int air_bottom = R.y + kHeadH + 26;
  for (int i = 0; i < 2; i++) {
    const int x = R.x + i * half, w = half - 8;
    Text(c, pen.m, x, R.y + kHeadH, w, pm[i]->label);
    const int bottom = DrawBigValue(pen, x, R.y + kHeadH + 26, w, pm[i]->value);
    if (pm[i]->sub[0]) Text(c, pen.s, x, bottom + 4, w, pm[i]->sub);
    const int observed_bottom = bottom + (pm[i]->sub[0] ? 20 : 0);
    if (pm[i]->badge != vm::Badge::None)
      DrawBadge(pen, x, observed_bottom + 4, w, pm[i]->badge);
    const int labelled_bottom = observed_bottom + (pm[i]->badge != vm::Badge::None ? 20 : 0);
    if (labelled_bottom > air_bottom) air_bottom = labelled_bottom;
  }
  // Station + measurement time (dataTime) under both numbers.
  if (p.air_sub[0]) {
    DrawIcon(c, IconId::Clock, IconSize::S24, R.x, air_bottom + 10);
    Text(c, pen.s, R.x + 30, air_bottom + 15, R.w - 30, p.air_sub);
  }

  // Right lower: indoor sensor.
  DrawCardHead(pen, R.x, kLowerCard, R.w, p.indoor.icon, "실내", p.indoor.badge);
  DrawCellBody(pen, R.x, kLowerCard + kHeadH, R.w, p.indoor);

  DrawFooter(pen, 0, p.demo, "출처 기상청 · 에어코리아");
}

void RenderMarket(const vm::MarketPage& p, Canvas& c, const Fonts* fonts) {
  c.clear(true);
  const Pen pen{c, Pick(fonts, 14), Pick(fonts, 18), Pick(fonts, 64)};
  DrawHeader(pen, p.header);
  const Rect L = BodyLeftRect(), R = BodyRightRect();

  // Left: official FX fixing (고시), one 150px block per currency.
  DrawCardHead(pen, L.x, L.y, L.w, IconId::FxExchange, kLFx);
  if (p.fx_count == 0) Text(c, pen.s, L.x, L.y + kHeadH, L.w, "환율 수신 전");
  for (size_t i = 0; i < p.fx_count && i < 2; i++) {
    const vm::MarketRow& r = p.rows[i];
    const int y = L.y + kHeadH + (int)i * 150;
    const int x = L.x + kHeroX, w = L.w - kHeroX;
    DrawIcon(c, r.icon, IconSize::L64, L.x, y);
    const int bx = DrawBadge(pen, x, y, w, r.badge);
    char head[48];
    snprintf(head, sizeof(head), "%s %s", r.pair, r.kind);
    Text(c, pen.m, x, y, bx - x - 4, head);
    const int bottom = DrawBigValue(pen, x, y + 22, w, r.value);
    if (r.sub[0]) Text(c, pen.s, x, bottom + 2, w, r.sub);
  }

  // Left bottom: KOSPI index (KIS REST snapshot).
  if (p.has_index) {
    const int iy = L.y + kHeadH + 276;
    HLine(c, L.x, iy - 6, L.w);
    const int bx = DrawBadge(pen, L.x, iy, L.w, p.index_badge);
    Text(c, pen.m, L.x, iy, 90, p.index_label);
    const int cw = p.index_chg[0] ? c.measure(pen.m, p.index_chg) : 0;
    const int vw = c.measure(pen.m, p.index_value);
    int vx = bx - 8 - cw - (cw ? 10 : 0) - vw;
    if (vx < L.x + 96) vx = L.x + 96;
    Text(c, pen.m, vx, iy, vw + 4, p.index_value);
    if (cw) Text(c, pen.m, vx + vw + 10, iy, cw + 4, p.index_chg);
    Text(c, pen.s, L.x, iy + 26, L.w, p.index_sub);
  }

  // Right: watchlist rows (code, 현재가/전일 종가, value, basis).
  DrawCardHead(pen, R.x, R.y, R.w, IconId::Chart, "관심종목");
  if (p.n <= p.fx_count) Text(c, pen.s, R.x, R.y + kHeadH, R.w, "등록된 종목 없음");
  for (size_t i = p.fx_count, row = 0; i < p.n && row < kMaxWatchlist; i++, row++) {
    const vm::MarketRow& r = p.rows[i];
    const int y = R.y + kHeadH + (int)row * 64;
    const int bx = DrawBadge(pen, R.x, y, R.w, r.badge);
    // Line 1: 종목명 (코드 when the name is unknown) ... 값. Line 2: 코드 · 구분 · 기준.
    const int vw = c.measure(pen.m, r.value);
    const int vx = (bx - 8 - vw) > R.x + 150 ? bx - 8 - vw : R.x + 150;
    Text(c, pen.m, R.x, y, vx - R.x - 8, r.name[0] ? r.name : r.label);
    Text(c, pen.m, vx, y, bx - vx - 4, r.value);
    char detail[128];
    if (r.name[0]) snprintf(detail, sizeof(detail), "%s · %s%s%s", r.label, r.kind,
                            r.sub[0] ? " · " : "", r.sub);
    else snprintf(detail, sizeof(detail), "%s%s%s", r.kind, r.sub[0] ? " · " : "", r.sub);
    // 전일 대비율은 2줄 오른쪽 끝(있을 때만). 상승/하락 기호는 부호(+/-)로만 구분한다.
    const int cw = r.chg[0] ? c.measure(pen.m, r.chg) : 0;
    if (cw) Text(c, pen.m, R.x + R.w - cw, y + 24, cw, r.chg);
    Text(c, pen.s, R.x, y + 26, cw ? R.w - cw - 8 : R.w, detail);
    HLine(c, R.x, y + 52, R.w);
  }
  DrawIcon(c, IconId::Store, IconSize::S24, R.x, 400);
  char state[48];
  snprintf(state, sizeof(state), "시장 %s", p.state[0] ? p.state : "--");
  Text(c, pen.m, R.x + 30, 403, R.w - 30, state);

  DrawFooter(pen, 1, p.demo, "출처 수출입은행 · 금융위원회 · 한국투자증권");
}

void RenderCalendar(const vm::CalendarPage& p, Canvas& c, const Fonts* fonts) {
  c.clear(true);
  const Pen pen{c, Pick(fonts, 14), Pick(fonts, 18), Pick(fonts, 64)};
  DrawHeader(pen, p.header);
  const Rect L = BodyLeftRect(), R = BodyRightRect();
  DrawCardHead(pen, L.x, L.y, L.w, IconId::Clock, p.valid ? p.title : "달력");
  DrawCardHead(pen, R.x, R.y, R.w, IconId::Clock, "오늘");
  if (!p.valid) {
    // 날짜는 신뢰할 수 있는 시계가 있을 때만 그린다(오늘을 추측하지 않는다).
    Text(c, pen.m, L.x, L.y + kHeadH + 8, L.w, "시간 확인 필요");
    Text(c, pen.s, L.x, L.y + kHeadH + 38, L.w, "Wi-Fi 연결 후 시계가 맞으면 표시됩니다");
    DrawFooter(pen, 2, p.demo, "");
    return;
  }
  // Month grid: 7 columns x up to 6 rows. No color on this panel, so the marks are
  // shape-based: holiday = outlined cell, today = inverted cell, 휴 = market closed, 절 = solar term.
  static const char* const kDow[] = {"일", "월", "화", "수", "목", "금", "토"};
  const int colw = L.w / 7, rowh = 48;
  const int gy = L.y + kHeadH;
  for (int i = 0; i < 7; i++) {
    const int w = c.measure(pen.s, kDow[i]);
    Text(c, pen.s, L.x + i * colw + (colw - w) / 2, gy + 2, colw, kDow[i]);
  }
  const int ry0 = gy + 24;
  for (int d = 1; d <= p.days; d++) {
    const int idx = p.first_wday + d - 1;
    const int cx = L.x + (idx % 7) * colw, cy = ry0 + (idx / 7) * rowh;
    char num[8];
    snprintf(num, sizeof(num), "%d", d);
    const int w = c.measure(pen.m, num);
    Text(c, pen.m, cx + (colw - w) / 2, cy + 4, colw, num);
    const uint8_t f = p.flags[d];
    char mark[8] = {0};
    if (f & kCalClosedMark) strncat(mark, "휴", sizeof(mark) - 1);
    if (f & kCalTermMark) strncat(mark, "절", sizeof(mark) - 1);
    if (mark[0]) {
      const int mw = c.measure(pen.s, mark);
      Text(c, pen.s, cx + (colw - mw) / 2, cy + 26, colw, mark);
    }
    if (f & kCalHolidayMark) c.rect(cx + 3, cy + 1, colw - 6, rowh - 6);
    if (d == p.today) c.invert_rect(cx + 3, cy + 1, colw - 6, rowh - 6);
  }
  // Right: today's big date and the next holiday / solar term.
  const int ty = R.y + kHeadH;
  const int bottom = DrawBigValue(pen, R.x, ty, R.w, p.day_big);
  Text(c, pen.m, R.x, bottom, R.w, p.weekday);
  int y = ty + 130;
  Text(c, pen.s, R.x, y, R.w, "다음 공휴일");
  Text(c, pen.m, R.x, y + 18, R.w, p.next_holiday);
  y += 70;
  Text(c, pen.s, R.x, y, R.w, "다음 절기");
  Text(c, pen.m, R.x, y + 18, R.w, p.next_term);
  y += 70;
  HLine(c, R.x, y, R.w);
  Text(c, pen.s, R.x, y + 8, R.w, "테두리 공휴일 · 검정 오늘");
  Text(c, pen.s, R.x, y + 28, R.w, "기호: 휴=휴장일 · 절=24절기");
  if (!p.tables_ok) Text(c, pen.s, R.x, y + 48, R.w, "공휴일 표를 아직 받지 못했습니다");
  DrawFooter(pen, 2, p.demo, p.tables_ok ? "출처 한국천문연구원" : "");
}

void RenderDevice(const vm::DevicePage& p, Canvas& c, const Fonts* fonts) {
  c.clear(true);
  const Pen pen{c, Pick(fonts, 14), Pick(fonts, 18), Pick(fonts, 64)};
  DrawHeader(pen, p.header);
  const Rect L = BodyLeftRect(), R = BodyRightRect();

  DrawCardHead(pen, L.x, L.y, L.w, IconId::Device, "기기 상태");
  struct Line { IconId icon; const char* text; };
  const Line lines[] = {{p.wifi_icon, p.wifi},
                        {p.battery_icon, p.battery},
                        {IconId::Clock, p.clock}};
  int y = L.y + kHeadH;
  for (const Line& ln : lines) {
    DrawIcon(c, ln.icon, IconSize::S24, L.x, y);
    Text(c, pen.m, L.x + 30, y + 3, L.w - 30, ln.text);
    y += kRowH;
  }
  Text(c, pen.s, L.x + 30, y + 3, L.w - 30, p.fw);
  Text(c, pen.s, L.x, 360, L.w, p.charging);
  if (p.portal_url[0]) {
    // 같은 Wi-Fi 설정 포털: 주소(보조 .local)와 1회용 페어링 코드(반전 배너).
    Text(c, pen.s, L.x + 30, y + 26, L.w - 30, p.portal_url);
    Text(c, pen.s, L.x + 30, y + 46, L.w - 30, p.portal_host);
    if (p.portal_code[0]) {
      const int py = y + 72;
      DrawIcon(c, IconId::Device, IconSize::S24, L.x + 4, py + 2);
      Text(c, pen.m, L.x + 34, py + 5, L.w - 40, p.portal_code);
      c.invert_rect(L.x, py, L.w, 28);
    }
  }
  if (p.notice[0]) {
    // Inverted banner: the reason this page was forced (low battery/safe mode).
    const int ny = y + 34;
    DrawIcon(c, IconId::Warning, IconSize::S24, L.x + 4, ny + 2);
    Text(c, pen.m, L.x + 34, ny + 5, L.w - 40, p.notice);
    c.invert_rect(L.x, ny, L.w, 28);
  }
  Text(c, pen.s, L.x, 382, L.w, p.hint);
  Text(c, pen.s, L.x, 404, L.w, p.hint2);

  DrawCardHead(pen, R.x, R.y, R.w, IconId::Sync, "데이터 수신");
  for (int i = 0; i < (int)sched::Provider::Count; i++) {
    const int ry = R.y + kHeadH + i * 30;
    Text(c, pen.s, R.x, ry, R.w, p.providers[i]);
    if (i + 1 < (int)sched::Provider::Count) HLine(c, R.x, ry + 22, R.w);
  }

  DrawFooter(pen, 3, false, "");
}

void RenderPortal(Canvas& c, const char* ap, const char* pass, const char* url) {
  c.clear(true);
  const auto& f=DefaultMedium();
  c.draw_text(f,20,20,"기기 설정 / 10분 후 종료");
  c.draw_text(f,20,80,"Wi-Fi AP");
  c.draw_text(f,20,110,ap);
  c.draw_text(f,20,170,"임시 AP 비밀번호");
  c.draw_text(f,20,200,pass);
  c.draw_text(f,20,270,"접속 주소");
  c.draw_text(f,20,300,url);
  c.draw_text(DefaultSmall(),20,370,"저장된 비밀번호와 API 키는 표시하지 않습니다");
}

void RenderLanPortal(Canvas& c, const char* url, const char* host, const char* code) {
  c.clear(true);
  const auto& f = DefaultMedium();
  c.draw_text(f, 20, 20, "기기 설정 (같은 Wi-Fi) / 10분 후 종료");
  c.draw_text(f, 20, 80, "휴대폰 브라우저 주소");
  c.draw_text(f, 20, 110, url);
  c.draw_text(DefaultSmall(), 20, 146, host);
  c.draw_text(f, 20, 200, "페어링 코드");
  c.draw_text(f, 20, 230, code);
  c.draw_text(DefaultSmall(), 20, 330, "휴대폰을 이 기기와 같은 Wi-Fi에 연결하세요");
  c.draw_text(DefaultSmall(), 20, 352, "저장된 비밀번호와 API 키는 표시하지 않습니다");
}

bool CheckMarginsClean(const Canvas& c) {
  // Forbidden zones: left margin 0..19, column gap 392..408.
  for (int y = 0; y < Canvas::H; y++) {
    for (int x = 0; x < 20; x++)
      if (c.pixel(x, y)) return false;
    for (int x = 392; x < 408; x++)
      if (c.pixel(x, y)) return false;
  }
  return true;
}

bool CheckBadgePresent(const Canvas& c, int x, int y, int w, int h) {
  return c.black_ratio(x, y, w, h) > 0.05;
}

}  // namespace ui
}  // namespace kdash
