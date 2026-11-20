# 설계: 아이콘 + 카드형 E-ink UI 재구성 (2026-11-02)

상태: 사용자 승인(설계) · 구현 전 · 실기 가독성 **BLOCKED (E1001 미연결)**

## 1. 목적
800×480 1bpp 화면에서 날씨·대기질·환율·종목·기기 상태를 아이콘과 카드 머리띠로
한눈에 구분되게 한다. 핵심값(현재 기온, 환율)은 64px 숫자로 강조한다.

### 범위 밖
- 4-level gray, 부분 갱신 정책 변경, 새 데이터 공급자, 버튼 매핑 변경.
- docs/05 레이아웃 그리드(여백 20, 열 간격 392..408, 헤더/본문/푸터 밴드) 변경.

## 2. 기준선 (2026-11-02 재생성 프리뷰)
- `tools/render_previews.py`가 Windows에서 `src/main.cpp`를 제외하지 못하던 경로
  버그 수정(`relpath` 구분자 정규화) 후 10개 PBM/PNG 재생성.
- 단위 중복(`°C°C`, `%%`)은 현행 `DrawCell`에서 이미 해결됨 → 회귀 테스트로 고정만 한다.
- 64px 폰트가 렌더러에서 쓰이지 않음, 헤더에 날짜·Wi-Fi 상태 표시 없음, 섹션 구분이
  라벨 텍스트뿐이라 정보 종류가 시각적으로 구분되지 않음.

## 3. 아이콘 소스
- **Material Symbols Outlined**, Apache-2.0, google/material-design-icons
  `variablefont/MaterialSymbolsOutlined[FILL,GRAD,opsz,wght].ttf` (10,744,832 B,
  2026-11-02 HEAD 확인). 원본 TTF는 커밋하지 않는다(기존 폰트 정책과 동일).
  생성 스크립트가 sha256을 기록·검증하고, LICENSE 전문은 `src/ui/icons/LICENSE-Apache-2.0.txt`에 보존.
- 대안 기각: Weather Icons(OFL)+Material 혼용(스타일 불일치), Tabler/Lucide SVG
  (cairosvg 신규 개발 의존성 필요).

## 4. 구성 요소

### 4.1 생성기 `tools/icon_subset/make_icons.py`
- 입력: TTF 경로/URL, 크기(24, 64), 아이콘 목록(이름→코드포인트, 스크립트 내 고정 표).
- Pillow `ImageFont.truetype` + 가변축 기본 인스턴스(FILL 0, wght 400; 64px은 opsz 48)
  → 정사각 캔버스 중앙 정렬 → threshold 128 → 1bpp MSB-first.
- 출력: `src/ui/icons/icons_24.h`, `icons_64.h` — `IconId` 순서와 일치하는 고정 배열.
  결정적 출력(같은 입력 → 같은 바이트), 용량 보고 라인 출력.

### 4.2 런타임 `src/ui/Icons.h/.cpp` (포터블)
```cpp
// enum은 생성 헤더 src/ui/icons/IconData.h가 정의(생성기 = 단일 원천)
enum class IconId : uint8_t { None, Sun, PartlyCloudy, Cloud, Rain, Snow, RainSnow,
  Unknown, Usd, Jpy, Humidity, Umbrella, Air, Home, FxExchange, Chart, Store,
  Wifi, WifiOff, BatteryFull, BatteryMid, BatteryLow, BatteryCharging, Clock,
  Sync, Count };
enum class IconSize : uint8_t { S24, L64 };
int  IconPx(IconSize s);                       // 24 / 64
bool HasIcon(IconId id, IconSize s);
void DrawIcon(Canvas& c, IconId id, IconSize s, int x, int y);  // 클리핑 안전
```
- 64px 세트는 날씨 7종(Sun..Unknown) + Usd/Jpy만. 나머지는 24px만.
- 래스터 스파이크(2026-11-02) 결과: wght 600이 24px에서 선 끊김 없음 → 채택.
  `rainy_snow`/`rainy_light`/`rainy_heavy`는 구름 없는 빗줄기라 판별 불가 → 비/눈은 `weather_mix`,
  소나기는 Rain 아이콘 공유(텍스트 "소나기"로 구분). 천둥/안개는 KMA PTY에 없으므로 제외(YAGNI).
- 코드포인트: Sun e81a, PartlyCloudy f172, Cloud f15c, Rain f176, Snow e2cd, RainSnow f60b,
  Unknown e8fd, Usd e227, Jpy eafb, Humidity f87e, Umbrella f1ad, Air efd8, Home e9b2,
  FxExchange eb70, Chart e6e1, Store ea12, Wifi e63e, WifiOff e648, BatteryFull f304,
  BatteryMid f30a, BatteryLow f30c, BatteryCharging f305, Clock efd6, Sync e627.
- 입력 TTF sha256: `f7837ba8573d9a7a74931618ced8307bc81f96d49af296bf6c8052c772ed5282`.
- 미보유 조합은 그리지 않고(빈칸) `HasIcon=false` — 텍스트 라벨이 항상 함께 있으므로 정보 손실 없음.
- 예상 Flash: 24px 24개×72B ≈ 1.7KB, 64px 9개×512B ≈ 4.6KB → **< 8KB**.

### 4.3 ViewModel 확장
- `fmt::WeatherIconFor(int sky, int pty) -> IconId` (순수 함수):
  PTY 우선 — 1 비→Rain, 2 비/눈→RainSnow, 3 눈→Snow, 4 소나기→Rain,
  5 빗방울→Rain, 6 빗방울눈날림→RainSnow, 7 눈날림→Snow.
  PTY 0/결측이면 SKY — 1 맑음→Sun, 3 구름많음→PartlyCloudy, 4 흐림→Cloud.
  둘 다 결측·범위 밖→Unknown.
- `Cell`에 `IconId icon` 추가. `LifePage`에 `IconId now_icon`(현재 날씨 대표 아이콘), `fc_icon`.
- `MarketRow`에 `IconId icon`(Usd/Jpy/Chart), `MarketPage`에 시장 상태 아이콘 불필요(텍스트 유지, Store 아이콘 고정).
- `DevicePage`에 `IconId wifi_icon, battery_icon` — 배터리 단계는 기존 BatteryPolicy % 기준
  (≥60 Full, ≥20 Mid, <20 Low, USB → Charging, 미상 → None).
- `Header`에 날짜 포함(`11-02(월) 14:30` 형식, 기존 KST 포맷터 재사용).

### 4.4 렌더러 (Renderer.cpp)
공통 프리미티브(내부 함수):
- `DrawCardHead(c, x, y, w, IconId, title)`: 24px 아이콘 + 18px 제목 + 아래 1px 선(열 폭 안).
- `DrawBadge(...)`: 기존 반전 배지 로직을 함수로 추출(동작 동일).
- `DrawIconRow(c, x, y, w, IconId, label, value)`: 24px 아이콘 + 라벨 + 값 한 줄.

페이지 배치 (y는 본문 58..434 내부, 좌 x=20..392 / 우 x=408..780):
- **헤더**: 동네명(좌) · 날짜시각(중앙 우측) · Wi-Fi/WifiOff 아이콘(우) · 시간 미확인 배지(기존).
- **생활**: 좌 "현재 날씨" 카드 — 64px 날씨 아이콘 + 64px 기온 + 하늘·강수형태 텍스트 + 관측시각,
  하단 아이콘 행 Humidity 습도 / Umbrella 강수량. 우 "3시간 예보"(24px 날씨 아이콘, 기온, 강수확률, 시각),
  "대기질"(Air; 미세먼지·초미세먼지 수치+등급 2열), "실내"(Home; 온도/습도 또는 센서 상태).
- **시장**: 좌 "환율(고시)" 카드(FxExchange) — 행마다 Usd/Jpy 아이콘, `KRW / USD`, `KRW / 100JPY`
  표기, 64px 값(폭 초과 시 18px로 폴백), 고시 기준일. 우 "관심종목"(Chart) — 종목 행(코드, `현재가`/`전일 종가`,
  값, 기준) 최대 4행, 하단 Store 아이콘 + 시장 상태.
- **기기**: 좌 아이콘 행 Wifi/WifiOff, Battery*, Clock, FW. 우 "데이터 수신"(Sync) — 공급자별 마지막 성공.
  하단 버튼 안내(기존 문구).
- **푸터**: `●○○` 페이지 점 + 페이지명, DEMO 표기는 기존 규칙 유지.

### 4.5 불변 규칙 (회귀 금지)
- 필수 문구(docs/05) 유지: `환율(고시)`, `전일 종가`, `현재가`, `미세먼지`, `초미세먼지`, `실내`, `관측`, 배지 문구.
- DEMO/배지 반전, `CheckMarginsClean`(좌 여백·열 간격 무픽셀) 계속 통과.
- 아이콘은 라벨을 대체하지 않는다(아이콘 + 텍스트 병기).

## 5. 오류·결측 처리
- 값 결측 `--`는 유지, 날씨 아이콘은 Unknown(물음표형)으로 표시.
- 공급자 미설정/오류 시 카드 머리띠와 아이콘은 그대로, 값 영역에 기존 배지.
- 64px 값이 열 폭을 넘으면 18px 폰트 + FitText로 폴백(단위·기준일 보존).

## 6. 테스트 (test_native, Unity)
1. 아이콘 테이블 무결성: 모든 `IconId`×크기에 대해 비트맵 길이 = stride×h, 64px 보유 집합 정확.
2. `WeatherIconFor` 전 조합: SKY{1,3,4,결측,범위밖} × PTY{0..7,결측}.
3. `DrawIcon` 픽셀 존재(bbox black_ratio>0)·클리핑(화면 밖 좌표에서 크래시/오버런 없음).
4. 3개 페이지 + 10 시나리오: `CheckMarginsClean` 통과, 배지 영역 반전 존재.
5. 단위 중복 회귀: 렌더된 셀 문자열에 `°C°C`, `%%` 없음.
6. 글리프 커버리지: 렌더러가 그리는 고정 문구·포맷 출력에 `MissingGlyphs==0`(14/18/64px 해당 폰트).
7. 호스트 Python: 생성기 결정성(동일 입력 2회 → 동일 바이트), sha256 불일치 시 실패.

증거: `evidence/p1/host-preview/*.png` 재생성 + 수동 눈검사 기록, 테스트 수/실패 수 원문 로그.

## 7. 문서
- ADR 0008: 아이콘셋 선택과 카드 레이아웃(대안, Flash 비용, 라이선스).
- docs/05 정보구조·레이아웃 절 갱신, THIRD_PARTY_NOTICES에 Material Symbols(Apache-2.0) 추가.
- 실기 가독성(아이콘 24px 판별, 64px 숫자 잔상)은 **BLOCKED — E1001 실기 확인 필요**로 기록.
