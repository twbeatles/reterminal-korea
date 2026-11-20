# 아이콘 + 카드형 E-ink UI 구현 계획

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Material Symbols 1bpp 아이콘과 카드 머리띠·64px 핵심값으로 생활/시장/기기 화면을 재구성한다.

**Architecture:** Python 생성기가 아이콘 TTF → `src/ui/icons/IconData.h`(enum + 24/64px 비트맵 표)를 만든다.
포터블 `Icons.cpp`가 Canvas에 그리고, ViewModel은 데이터→`IconId`를 결정하며(순수 함수), Renderer는 배치만 한다.

**Tech Stack:** C++17 (Arduino/ESP32-S3 + host zig c++), Unity, Python 3 + Pillow, pytest.

## Global Constraints
- 대상: reTerminal E1001 800×480 1bpp. 좌 여백 x<20, 열 간격 392..407에는 픽셀 금지(`CheckMarginsClean`).
- docs/05 필수 문구 유지: `환율(고시)`, `전일 종가`, `현재가`, `미세먼지`, `초미세먼지`, `실내`, `관측`, 배지 문구, `DEMO`.
- 아이콘은 텍스트 라벨을 대체하지 않는다(병기).
- 아이콘 원본 TTF 커밋 금지(`build/` gitignore). 입력 sha256 `f7837ba8573d9a7a74931618ced8307bc81f96d49af296bf6c8052c772ed5282`.
- 아이콘 Flash 총합 < 8KB. 신규 런타임 라이브러리 없음.
- 실기 가독성은 BLOCKED(E1001 미연결) — 컴파일/프리뷰 통과를 실기 정상으로 격상하지 않음.
- 검증 명령: `python tools/run_native_tests.py` (기준 51 Tests 0 Failures), `python -m pytest -q host/tests` (기준 139 passed).
- 저장소에 커밋이 없음 → 사용자 요청 전까지 커밋 단계는 생략(작업 트리 유지).

---

### Task 1: 아이콘 생성기 + 생성 헤더 + 라이선스

**Files:**
- Create: `tools/icon_subset/make_icons.py`
- Create (generated): `src/ui/icons/IconData.h`
- Create: `src/ui/icons/LICENSE-Apache-2.0.txt` (원문 그대로)
- Test: `host/tests/test_icons_gen.py`

**Interfaces — Produces (`IconData.h`):**
```cpp
namespace kdash { namespace ui {
enum class IconId : uint8_t { None, Sun, PartlyCloudy, Cloud, Rain, Snow, RainSnow,
  Unknown, Usd, Jpy, Humidity, Umbrella, Air, Home, FxExchange, Chart, Store,
  Wifi, WifiOff, BatteryFull, BatteryMid, BatteryLow, BatteryCharging, Clock, Sync, Count };
namespace icondata {
// size×size, stride (size+7)/8, MSB-first, 1=black. nullptr = 해당 크기 없음.
static const uint8_t* const k24[(int)IconId::Count];
static const uint8_t* const k64[(int)IconId::Count];
} } }
```

- [ ] Step 1: 테스트 작성 — `host/tests/test_icons_gen.py`
  - TTF(`build/icons/MaterialSymbolsOutlined.ttf`) 없으면 `pytest.skip("BLOCKED: icon TTF not downloaded")`.
  - 같은 입력으로 2회 생성 → 바이트 동일.
  - 잘못된 sha256 TTF(임시 복사 후 1바이트 변경) → 종료코드 ≠ 0.
  - 커밋된 `IconData.h`가 재생성 결과와 동일(드리프트 방지), 헤더에 Apache-2.0 고지와 입력 sha256 포함.
  - `IconId` 순서가 스크립트 `ICONS` 표와 동일, 64px 보유 집합 = {Sun..Unknown, Usd, Jpy}.
- [ ] Step 2: `python -m pytest -q host/tests/test_icons_gen.py` → FAIL(스크립트 없음).
- [ ] Step 3: 생성기 구현 — Pillow `truetype` + `set_variation_by_axes([0, 0, opsz, 600])`
  (opsz = clamp(size, 20, 48)), `textbbox`로 중앙 정렬, threshold 128, 결정적 출력, 용량 보고.
- [ ] Step 4: `python tools/icon_subset/make_icons.py --ttf build/icons/MaterialSymbolsOutlined.ttf --out src/ui/icons/IconData.h` 후 테스트 PASS.

### Task 2: 런타임 `Icons.h/.cpp`

**Files:** Create `src/ui/Icons.h`, `src/ui/Icons.cpp`; Test `test_native/test_ui_icons.inc` (+ `test_core.cpp`에 include/RUN_TEST)

**Interfaces — Produces:**
```cpp
namespace kdash { namespace ui {
enum class IconSize : uint8_t { S24, L64 };
int  IconPx(IconSize s);                                  // 24 / 64
bool HasIcon(IconId id, IconSize s);
void DrawIcon(Canvas& c, IconId id, IconSize s, int x, int y);  // 없는 아이콘 = no-op
} }
```
- [ ] Step 1: 테스트 — 모든 IconId(None 제외) 24px 보유; 64px 보유 집합 정확;
  `DrawIcon` 후 bbox `black_ratio>0` 및 bbox 밖 무픽셀; 화면 밖 좌표(-30,-30)/(790,470) 크래시 없음;
  `None`/`Count`/64px 미보유 → 무픽셀.
- [ ] Step 2: native 실행 → 컴파일 실패 확인.
- [ ] Step 3: 구현(Canvas::pixel 사용 — 클리핑은 pixel이 담당).
- [ ] Step 4: native 실행 PASS.

### Task 3: ViewModel 아이콘·헤더 날짜

**Files:** Modify `src/ui/Format.h/.cpp`, `src/ui/ViewModel.h/.cpp`; Test `test_native/test_ui_icons.inc`

**Interfaces — Produces:**
```cpp
// Format.h
ui::IconId WeatherIconFor(int sky, int pty);   // 결측 = -1
size_t HeaderDateTime(int64_t now_utc, char* out, size_t cap);  // "11-02(월) 14:30"
ui::IconId BatteryIconFor(int battery_mv, bool usb);
// ViewModel.h
struct Cell { ...; ui::IconId icon = ui::IconId::None; };
struct LifePage { ...; ui::IconId now_icon = ui::IconId::Unknown; char now_text[32] = {0}; };
struct MarketRow { ...; ui::IconId icon = ui::IconId::None; char pair[24] = {0}; };  // "KRW / USD"
struct DevicePage { ...; ui::IconId wifi_icon, battery_icon; };
```
- [ ] Step 1: 테스트 — WeatherIconFor 전 조합(SKY{1,3,4,-1,9}×PTY{0..7,-1});
  HeaderDateTime 자정 경계(KST 2026-11-01 23:59 → "11-01(일) 23:59", 00:00 → "11-02(월) 00:00");
  BatteryIconFor(usb→Charging, -1→None, 4100→Full, 3700→Mid, 3300→Low: PercentMv 기준 ≥60/≥20);
  BuildLife: temp.icon==Sun 등 셀 아이콘, now_icon이 데모(SKY 4, PTY 0)에서 Cloud;
  BuildMarket: USD 행 icon Usd·pair "KRW / USD", JPY 행 Jpy·"KRW / 100JPY", 종목 행 Chart;
  BuildDevice: wifi off → WifiOff.
- [ ] Step 2: FAIL 확인 → Step 3: 구현 → Step 4: PASS (기존 51 테스트 포함).

### Task 4: 카드형 Renderer

**Files:** Modify `src/ui/Renderer.cpp` (+`Renderer.h` 주석); Test `test_native/test_ui_icons.inc`
- 내부 함수: `DrawCardHead(c,f,x,y,w,icon,title)` (24px 아이콘 + 18px 제목 + y+28 위치 1px 선),
  `DrawBadge(c,fs,x,y,w,badge)` (기존 반전 로직 추출), `DrawBigValue(c,big,fm,x,y,w,text)` (64px, 폭 초과 시 18px 폴백).
- 배치는 spec §4.4 그대로. 헤더: place(≤300px) · datetime(x=420) · Wifi 아이콘(x=756). 푸터: `●○○` + 페이지명.
- [ ] Step 1: 테스트 — 3페이지×(정상/NoData/Long place/DEMO) `CheckMarginsClean`;
  생활 좌측 날씨 아이콘 영역(L.x, L.y+36, 64, 64) black_ratio>0.02;
  배지 존재(stale 시 셀 영역); 렌더 문자열 글리프 누락 0(헤더·카드 제목·셀 값 각각 `MissingGlyphs`);
  Wi-Fi off 헤더 아이콘 영역 픽셀 존재.
- [ ] Step 2: FAIL 확인 → Step 3: 구현 → Step 4: native + pytest 전체 PASS.

### Task 5: 프리뷰·문서·보고

**Files:** `evidence/p1/host-preview/*` 재생성, Create `docs/adr/0008-icon-card-ui.md`, Modify `docs/05_KOREAN_EINK_UI.md`, `THIRD_PARTY_NOTICES.md`, Create `evidence/ui-icons/summary.md`
- [ ] `python tools/render_previews.py` → 10 PNG, 모두 `margin=1`. PNG 눈검사 기록.
- [ ] ESP32 빌드 가능 시 `pio run -e reterminal_e1001` Flash 증가량 기록(불가하면 BLOCKED 기록).
- [ ] ADR/문서/고지 갱신, PROGRESS_REPORT 형식 최종 보고.
