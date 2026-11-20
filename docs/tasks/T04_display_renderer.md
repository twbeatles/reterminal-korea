# T04 — 화면: 뷰모델 → 한글 폰트 → 캔버스 → 패널 출력

- 상태: T04a~c DONE_VERIFIED_HOST, T04d BUILD ONLY (전송 스텁, 실기 BLOCKED_HW). U-13 ADR 0004 제안
- 선행: T03(헤더 시각·"시간 확인 필요" 표시). T04a~c는 T03 없이도 시작할 수 있습니다.
- 필요 자원: 실기(T04d 최종), 한글 OFL 폰트(이미 선정: IBM Plex Sans KR, `tools/font_subset/`)
- 관련: docs/05(정보구조·그리드·문구·갱신 정책), 감사 G-11, `src/ui/{Layout,Pages,RefreshPolicy}.*`, `hardware/driver_comparison.md`, U-13/U-15

## 0. 하위 단계와 순서

| 단계 | 내용 | 대상 | 검증 수준 |
|---|---|---|---|
| T04a | 뷰모델: Snapshot·상태 → 페이지별 문자열·배지 | 이식 | HOST |
| T04b | 한글 비트맵 폰트 생성기 + 폰트 데이터 | 도구 + 이식 | HOST |
| T04c | 1bpp 캔버스 + 텍스트 배치 + 페이지 렌더러 + 프리뷰 | 이식 | HOST |
| T04d | 패널 어댑터(UC8179/GDEY075T7) + 갱신 정책 연결 | 기기 | BUILD → HW |

이렇게 나누는 이유는 화면 로직의 대부분(T04a~c)을 실기 없이 완성하고 검증하기 위해서입니다. 실기 의존 부분은 "프레임버퍼를 패널에 보내는 일"(T04d)로 좁힙니다.

---

## T04a — 뷰모델 (G-11의 핵심)

### 목표
`app::Snapshot` + `app::AppState` + 시계 상태 → `ui::LifeView/MarketView/DeviceView`(문자열·배지 결정 완료)를 만드는 **순수 함수**를 작성합니다. "어떤 값을 어떤 라벨로 보여주는가"는 여기서 전부 결정되고 테스트됩니다. 렌더러는 받은 문자열을 그리기만 합니다.

### 설계
- 기존 `src/ui/Pages.h`의 구조를 "이미 서식이 적용된 텍스트" 중심으로 바꿉니다. `DataPoint`를 그대로 넘기지 않습니다.

```cpp
struct Cell {             // 한 칸 = 라벨 + 값 + 보조줄 + 배지
  char label[32];         // "기온"
  char value[24];         // "21.5", "--"
  char unit[16];          // "°C", "KRW / 100JPY"
  char sub[48];           // "관측 14:00 · 수신 14:10"
  Badge badge;            // None, Stale("오래된 정보"), LastGood("마지막 정상값"),
                          // NeedSetup("설정 필요"), Check("확인 필요"), Demo("DEMO")
};
struct Header { char place[48]; char datetime[32]; bool wifi_ok; bool time_ok; };
struct Footer { char sources[64]; char received[32]; uint8_t page, pages; };
```

- 파일: `src/ui/ViewModel.{h,cpp}`, `src/ui/Format.{h,cpp}`(숫자·시각 서식)

### 서식 규칙 (docs/05 기반, 테스트로 고정)

| 대상 | 규칙 | 예 |
|---|---|---|
| 기온 | 소수 1자리, `°C` | `21.5°C` |
| 습도·강수확률 | 정수 `%` | `55%` |
| 강수량(RN1) | 0이면 `0mm`(실측 0은 값), Missing이면 `--` | `0mm` |
| PM | 정수 + `µg/m³` + 등급 | `18 µg/m³ 보통` |
| PM10 등급 | 0–30 좋음 / 31–80 보통 / 81–150 나쁨 / 151+ 매우나쁨 | 환경부 통합 기준 (구현 전 원문 재확인) |
| PM2.5 등급 | 0–15 / 16–35 / 36–75 / 76+ | 동상 |
| 하늘(SKY) | 1 맑음, 3 구름많음, 4 흐림 | KMA 코드표 (가이드 재확인) |
| 강수형태(PTY) | 0 없음, 1 비, 2 비/눈, 3 눈, 4 소나기 | 단기예보 기준 |
| 환율 | 천단위 콤마 + 소수 2자리, 단위 `KRW / USD`, `KRW / 100JPY` | `1,384.50` |
| 환율 보조줄 | `고시 기준일 10-30(금)` — "실시간" 표현 금지 | |
| 전일 종가 | 콤마 정수, 라벨 `전일 종가`, 보조 `기준일 10-30` | `78,500` |
| 현재가 | 라벨 `현재가`, 보조 `13:30 조회 · REST` (체결 스트림 아님) | |
| 시장 상태 | 장중 / 장마감 / 휴장 가능(주말·공휴일 미확정) | T09 이후 "휴장" 확정 |
| 시각 | 모두 KST, `HH:MM`. 날짜가 다르면 `MM-DD HH:MM` | |
| 오래됨 | `N분 전`/`N시간 전`/`N일 전` | `2시간 전` |

### 배지 결정표 (G-11)

| 조건 | 값 칸 | 배지 |
|---|---|---|
| 제공자 미설정 (`ProviderConfigured=false`) | `--` | 설정 필요 |
| Valid | 값 | 없음 |
| Stale | 값 | 오래된 정보 (+ `sub`에 경과 시간) |
| 최신 시도 실패 + 이전 값 보유 | 이전 값 | 마지막 정상값 |
| Missing | `--` (절대 0 아님) | 없음 (또는 `측정 없음`) |
| ProviderError + 값 없음 | `--` | 확인 필요 |
| AuthError로 차단됨 | `--` 또는 이전 값 | 확인 필요 + 기기 페이지에 "키 확인" |
| 시계 unverified | 값 유지 | 헤더 `시간 확인 필요` |
| Demo 데이터 | 값 | DEMO (화면 전체 워터마크) |

### 테스트
- 표의 모든 행에 1건 이상. `Missing`이 `0`으로 나오면 실패하는 mutation 검사를 포함합니다.
- 긴 지역명(64B 한글 21자), 8자리 주가(`12,345,678`), JPY 단위 보존.
- 자정 경계: 관측 23:50, 지금 00:10 → `11-01 23:50`.

---

## T04b — 한글 비트맵 폰트

### 결정
런타임 TTF 래스터라이저(FreeType 등)는 넣지 않습니다. 빌드 시점에 **크기별 1bpp 비트맵 글꼴**을 C 배열로 생성합니다.
- 이유: RAM·Flash 예측 가능, 라이선스 단순(OFL 폰트 파생물 + 자체 코드), host와 device에서 같은 데이터 사용.
- Adafruit GFX `GFXfont`는 8비트 코드 범위를 전제로 하므로 한글에 쓰지 않습니다.

### 생성기 `tools/font_subset/make_bitmap_font.py`
- 입력: OFL TTF(원본은 커밋 금지 — 기존 `subset.py`와 같은 다운로드/라이선스 검사 재사용), 크기(px), 문자 집합.
- 래스터: Pillow `ImageFont.truetype` + 임계값 128 → 1bpp. 글리프별 bounding box를 자릅니다.
- 출력: `src/ui/fonts/font_<name>_<px>.h`

```cpp
struct GlyphInfo { uint32_t cp; uint32_t offset; uint8_t w, h; int8_t x_off, y_off; uint8_t advance; };
struct BitmapFont { const GlyphInfo* glyphs; uint16_t count; const uint8_t* bitmap;
                    uint8_t line_height, ascent; uint32_t fallback_cp; };
// glyphs는 cp 오름차순 → 이진 탐색. 없으면 fallback(□ U+25A1).
```

- 문자 집합 (크기별로 다르게 해서 용량 절약)

| 크기 | 용도 | 문자 |
|---|---|---|
| 14px | 보조줄·푸터·기기 페이지 | ASCII + 기호(°·µ³%/–·□▶◀●○) + **KS X 1001 한글 2,350자** (사용자 입력 지역명·측정소명 대비) |
| 18px | 라벨·헤더 | ASCII + 기호 + docs/05 고정 문구 + 요일 + 등급·날씨 단어 + 2,350자 |
| 40px | 보조 큰 숫자 | 숫자·`.,-+:%°C` |
| 64px | 메인 기온·환율 | 숫자·`.,-°C` |

- 예상 용량을 보고서로 출력하고 `evidence/p4/font-report.txt`에 남깁니다. 14px 2,350자는 대략 수십~100KB대로 예상되므로 실측합니다.
- 커버리지 검사 함수 `ui::MissingGlyphs(font, utf8)`를 둡니다. 포털 저장 시 지역명·측정소명에 없는 글자가 있으면 경고합니다(T07).

### 테스트
- 생성기: 같은 입력 → 같은 바이트(결정적). 글리프 수·cp 정렬 검사(pytest).
- C++: UTF-8 디코드(2/3/4바이트, 잘린 시퀀스 → U+FFFD 처리), 이진 탐색, fallback.

---

## T04c — 1bpp 캔버스, 텍스트 배치, 페이지 렌더러, 프리뷰

### 캔버스 `src/ui/Canvas.{h,cpp}` (이식)
- 800×480 1bpp, 행 우선, MSB first, 1=검정. 크기 48,000B.
  - 기기: PSRAM에 할당(`ps_malloc`)
  - host: 일반 메모리
- 연산: clear, pixel, hline/vline/rect/fill_rect, invert_rect(배지), draw_text(font, x, y, utf8) → advance, measure(font, utf8).
- 텍스트 배치 `src/ui/TextLayout.{h,cpp}`: 상자 폭에 맞춰 자르고 `…`를 붙이되 **보호 접미사(단위·기준일)는 유지**합니다(docs/05). 기존 `Layout::NeedsEllipsis/FitChars`는 이 구현으로 대체합니다.
- `Canvas::Crc32()` → 화면 변경 감지에 사용합니다(RefreshPolicy의 `content_changed`).

### 페이지 렌더러 `src/ui/Renderer.{h,cpp}` (이식)
- `Render(const PageView&, Canvas&)`: Layout.h 그리드(헤더 12–48, 본문 58–434, 푸터 440–468, 좌열 20–392, 우열 408–780)를 그대로 사용합니다.
- 페이지 구성 (docs/05 기준 초안. 실기 가독성 확인 후 조정)
  - **생활**
    - 좌: 기온(64px) + `관측 HH:MM`, 습도·강수량
    - 우상: 다음 예보 슬롯(`15시 22°C · 강수확률 60% · 흐림`)
    - 우중: PM10/PM2.5 + 등급
    - 우하: 실내 온습도
  - **시장**
    - 좌: 고시환율 2행(USD, 100JPY) + 기준일
    - 우: 종목 최대 4행(코드/라벨, 값, 종가·현재가 구분 라벨, 시각) + 시장 상태
  - **기기**
    - Wi-Fi·시계 출처·배터리·펌웨어 버전
    - 제공자별 마지막 성공 시각과 상태
    - 버튼 안내 두 줄(`KEY0 ◀ 이전 화면 · KEY1 ▶ 다음 화면` / `KEY2 새로고침 · KEY0 2초 누르면 설정`)
    - 저전압·안전 모드로 강제 진입했을 때 경고 배너로 이유 표시
  - **포털 안내 화면**(T07): AP 이름, AP 비밀번호(크게), `http://192.168.4.1/`, 남은 시간
- DEMO: 화면 대각선 워터마크 대신 헤더 오른쪽에 반전 배지 + 본문 상단 `DEMO / 예시 수치`(docs/05).

### 테스트 (독립 oracle — docs/07)
같은 `Render()`로 기대값을 만들지 않습니다. 대신 **기하·픽셀 성질**을 검사합니다.
- 금지 영역(여백 0–19px, 열 사이 392–408px, 영역 경계선 바깥)의 검정 픽셀 수 = 0
- 각 Cell의 텍스트 bounding box가 자기 칸 안에 있음 (`measure` 결과로 검사)
- 배지가 있는 칸에는 반전 사각형이 있음 (해당 영역 검정 비율 > 임계값)
- 10개 시나리오(Day, Night, Weekend, NoNetwork, NoData, LongKorean, WrongTime, LowBattery, KisDisabled, Demo) 각각 렌더 성공 + 위 검사 통과
- 네이티브 테스트가 PBM(P4)을 `.pio/preview/`에 쓰고, `tools/generate_preview.py`가 이를 PNG로 변환해 `evidence/p1/host-preview/`를 **C++ 렌더 결과로** 갱신합니다. 사람 눈 검토 결과를 증거에 기록합니다.
- 기존 Python `ui_preview.py`(ASCII 자리표시자)는 이 단계가 끝나면 "C++ 출력 변환기"로 축소하거나 폐기합니다. 결정 내용을 ADR에 남깁니다.

---

## T04d — 패널 어댑터와 갱신 정책

### 드라이버 선택 (U-13)
1. **GxEPD2** (`GxEPD2_750_GDEY075T7`, 공식 예제 `GxEPD2_reTerminal_E1001`)
   - 장점: 검증된 경로
   - 단점: GPL-3.0으로 알려짐(배포 방침 확인 필요)
   - 프레임버퍼 전송: 저수준 `display.epd2.writeImage(buf, 0, 0, 800, 480, ...)` + `refresh(partial)` 사용 가능 여부를 라이브러리 소스에서 확인합니다.
2. **자체 UC8179 드라이버**
   - 공식 `GxEPD2_reTerminal_E1001_Gray4` 예제는 GxEPD2 없이 SPI 명령을 직접 보냅니다(리셋·초기화 시퀀스·0x10/0x13 데이터 전송·0x12 refresh).
   - 장점: 라이선스 부담 없음, Gray4 확장 쉬움
   - 단점: 초기화 시퀀스 검증 부담
- 선택은 ADR `0004-epaper-driver.md`로 남깁니다. **기본 권장: 라이선스가 허용되면 1, 아니면 2.**

### 어댑터 `src/drivers/EpdPanel.{h,cpp}` (기기 전용)
- 핀(공식 원문): SCK 7, MOSI 9, CS 10, DC 11, RST 12, BUSY 13, HSPI 2MHz MODE0, `hspi.begin(7,-1,9,-1)`.
- `IDisplay`를 다음 형태로 개정합니다.

```cpp
class IDisplay {
  virtual bool begin() = 0;
  virtual bool push_full(const uint8_t* fb) = 0;                // 전체 갱신
  virtual bool push_partial(const uint8_t* fb, const Rect& r) = 0;  // x,w 8정렬
  virtual void hibernate() = 0;                                  // sleep 전 필수
};
```

- BUSY 대기에 타임아웃(예: 30s)을 둡니다. 초과하면 실패를 반환하고 `last_rendered_at`을 갱신하지 않습니다.

### 갱신 흐름 (`app::RenderStep`, 이식)
1. 뷰모델 생성 → 렌더 → `crc = Canvas::Crc32()`
2. `crc == last_rendered_crc` → Skip (docs/05 "값 변경 없으면 미갱신")
3. 페이지 전환·버튼 → Full. 그 외 → `ui::DecideRefresh(changed, aligned, partials_since_full, caps)`
4. 성공 시에만 `last_rendered_crc`, `last_rendered_at`, `partials_since_full` 갱신(RTC 메모리, T05)
- `RefreshCaps.partial_supported`는 실기 검증(U-15) 전까지 **false 고정**입니다. 따라서 모든 갱신은 Full입니다.

### 완료 기준
- [ ] T04a~c: 네이티브/pytest 통과, 10개 프리뷰를 사람이 검토하고 기록
- [ ] T04d: 펌웨어 빌드 성공, Flash/RAM 증가량 기록
- [ ] 실기: 3페이지 표시 사진, Full 갱신 시간 측정, 10회 누적 잔상 사진 (BLOCKED_HW)
- [ ] U-13 결정 ADR 작성, `THIRD_PARTY_NOTICES.md` 갱신
