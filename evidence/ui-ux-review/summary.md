# Phase Progress Report — UI/UX 흐름·아이콘·배치 점검 (ADR 0008 후속)

- 날짜·시간(KST): 2026-11-02
- 작업 단계/티켓: ADR 0008 카드형 UI 후속 점검 (생활/시장/기기 3화면 + 버튼 흐름)
- 커밋 SHA / 공식 기반 commit: 기준 `8a50bbe`, 이번 변경은 그 위의 단일 커밋 / 공식 예제 @8d97d891 기준 변경 없음
- 작업 환경: Windows 11, Python 3.15, Pillow, ziglang c++ (host), PlatformIO espressif32 (`reterminal_e1001`)
- 기기 연결 여부 및 모델 확인: **미연결**. E1001 실기 확인을 하지 않았다.
- API 실키 접근: 사용하지 않음. 모든 화면은 DemoProvider/fixture 데이터로 렌더링했다.

## 구현 내용
- 변경 파일:
  - `tools/icon_subset/make_icons.py`, `src/ui/icons/IconData.h`(재생성): 아이콘 3종 추가(Thermometer/Device/Warning, 24px)
  - `src/ui/ViewModel.{h,cpp}`: 예보 셀 중복 제거, 대기질 `air_sub`, 기기 화면 공급자 이름·사유·`notice`·두 줄 안내, `BuildDevice(..., safe_mode=false)`
  - `src/ui/Renderer.cpp`: 헤더 시각 x=408 정렬, `현재 날씨` 온도계 아이콘, 대기질 측정 정보 줄, 기기 아이콘 머리띠, 경고 반전 배너, 공급자 행 구분선
  - `src/power/CyclePlanner.cpp`: 시계 미확인 상태의 KEY2 → `sync_clock=true`
  - `src/main.cpp`: 안전 모드(`g_crashes >= 3`)를 기기 화면에 전달
  - `test_native/test_stage2.cpp`, `test_native/test_ui_icons.inc`: 회귀 테스트 추가
  - `tools/render_previews.cpp`: 시나리오 12 `device-safemode`
  - 문서: `docs/05_KOREAN_EINK_UI.md`, `docs/adr/0008-icon-card-ui.md`, `docs/tasks/T04_display_renderer.md`(버튼 안내·배너),
    `docs/tasks/T05_power_sleep_wake.md`(KEY2 + 시계 미확인), `THIRD_PARTY_NOTICES.md`(아이콘 개수·크기)
- 변경 목적과 발견한 문제:
  1. (UX 버그) 시계 미확인 상태에서 KEY2를 누르면 `connect_wifi=false`, `sync_clock=false`라 네트워크를 전혀 시도하지 않았다. 이제 NTP 동기화를 먼저 시도하고, HTTPS는 시계가 확인된 뒤에만 수행한다(RunCycle이 기존대로 `ctx.clock_verified`를 확인한다).
  2. (UX) 저전압·안전 모드에서 기기 화면이 강제되지만 화면에 이유가 없었다. 이제 경고 배너로 표시한다.
  3. (정보) 대기질 수치에 측정소와 측정 시각이 없었다(AGENTS 검증 규칙: 원본 기준시각). `측정소 종로구 · 측정 HH:MM`을 추가했다.
  4. (배치) 3시간 예보 카드가 기온을 두 번 표시했다(`22.0°C` + `… 22.0°C · …`). 이제 값 줄은 `22.0°C · 강수확률 60%`, 보조 줄은 `예보 MM-DD HH:MM · 흐림`이다. 보조 줄은 PTY를 SKY보다 우선한다(히어로 아이콘 규칙과 같다).
  5. (아이콘) `현재 날씨` 머리띠와 64px 히어로가 같은 구름 아이콘을 반복했다 → 머리띠를 온도계로 바꿨다. `기기 상태` 머리띠와 시계 행이 같은 시계 아이콘을 반복했다 → 머리띠를 기기 아이콘으로 바꿨다.
  6. (용어) 기기 화면 공급자 이름이 `KMA/Air/EXIM/FSC/KIS`처럼 영문 약어였고, 빈 행은 `--`만 표시했다. 이제 푸터와 같은 한국어 이름을 쓰고 사유를 표시한다. 시계 미확인 시 `시계 NONE 시간 확인 필요` → `시계 시간 확인 필요`.
  7. (배치) 헤더 시각이 x=420에서 시작해 우측 열(x=408)과 어긋났다 → 408로 정렬했다.
  8. (일관성) DEMO 모드에서 데이터가 없을 때 습도/강수량/예보/대기질만 `확인 필요`를 표시했다 → `DEMO`로 통일했다.
- 기존 기능 영향: 렌더러 공개 API는 바뀌지 않았다. `BuildDevice`에 기본값이 있는 인자를 추가해 기존 호출부는 그대로 컴파일된다. `IconId` 끝에 3개를 추가해 기존 값은 바뀌지 않았다.

## 재현 가능한 증거
| 구분 | 명령/방법 | 결과 | 검증 단계 | 증거 경로 |
|---|---|---|---|---|
| Clean build | `pio run -e reterminal_e1001` | PASS (Flash 1,673,206B / 3,342,336B) | Build | 콘솔 로그(이 세션) |
| Stack gate | `python tools/check_stack_usage.py --elf .pio/build/reterminal_e1001/firmware.elf` | PASS (`failures: []`) | Build | — |
| Unit tests (native) | `python tools/run_native_tests.py` | PASS 54 Tests 0 Failures (기존 테스트 함수에 단언 추가) | Host | — |
| Unit tests (python) | `python -m pytest -q host/tests` | PASS 143 passed (아이콘 헤더 재생성 드리프트 테스트 포함) | Host | — |
| 아이콘 Flash 실측 | `xtensa-esp-elf-nm -S -C firmware.elf \| grep icondata` | 38 심볼 6,776B (비트맵 36 + 포인터 표 2) | Build | — |
| Icon regen | `python tools/icon_subset/make_icons.py --ttf build/icons/MaterialSymbolsOutlined.ttf --out src/ui/icons/IconData.h` | icons=27 bitmap=6,552B | Host | — |
| Host preview | `python tools/render_previews.py` | PASS 12장 모두 `margin=1`, 눈검사 완료 | Host | `evidence/p1/host-preview/*.png` (gitignore 대상, 재생성 가능) |
| E1001 smoke | — | NOT_RUN | Hardware | BLOCKED_HW |
| Live API | — | NOT_RUN | LiveAPI | 범위 밖 |
| Sleep/wake | — | NOT_RUN | Hardware | 범위 밖 |

### 추가 단언
- `test_cycle_planner`: 시계 미확인 KEY2 → `force_refresh && sync_clock && !connect_wifi`, 시계 확인 KEY2 → `connect_wifi && !sync_clock`.
- `test_icon_view_model`: 예보 값에 `강수확률`, 보조 줄에 `°C` 없음과 `예보` 포함, `air_sub` 측정소·측정시각,
  공급자 행 문구(센서 없음/수신 기록 없음/설정 필요/키 확인 필요/최근 실패), 시계 미확인 문구, 저전압 `부족`·배너, USB면 배너 없음, 안전 모드 배너.
- `test_card_renderer`: 5개 시나리오 × 저전압/안전 모드 배너 렌더에서 여백·열 간격 청정, 글리프 누락 0개.

## 독립 검증 및 보안
- 단위/포맷/환율 기준일 Oracle: 기존 테스트 무변경 통과. 고시/전일 종가/현재가 표기는 바꾸지 않았다.
- Mutation 테스트: 수행하지 않음.
- 자격증명 유출 검사: `kdash.secret_scan.find_secrets`로 변경 파일 10개를 검사해 0건. 새 화면 문구에 키·SSID·비밀번호를 넣지 않았다.
- 출처: 아이콘은 기존과 같은 sha256 고정 Material Symbols TTF(Apache-2.0)를 쓴다. 새 외부 의존성은 없다.

## 미완료/블로커
- BLOCKED_HW: 새 24px 아이콘(온도계·기기·경고)의 판별성, 반전 배너의 잔상, 14px 공급자 행 가독성은 E1001 실기에서만 판정할 수 있다.
- BLOCKED_CREDENTIALS: 해당 없음.
- BLOCKED_NETWORK: 해당 없음.
- 기타: 시계 미확인 KEY2의 NTP 시도는 호스트 계획 단위로만 검증했다. 실기에서 Wi-Fi 연결 → NTP → 같은 사이클 수신까지는 미검증이다.

## 게이트 판정
- 이번 단계 목표: PARTIAL. 호스트·빌드 기준 PASS, 실기 가독성·버튼 흐름은 미검증.
- 다음 단계: E1001에 플래시해 3화면과 저전압/안전 모드 화면을 사진으로 남기고, 시계 미확인 상태에서 KEY2 흐름을 UART 로그로 확인한다.
- 실기 없이 성공이라고 표시하지 않은 항목: 아이콘 판별성, 배너 잔상, KEY2 NTP 복구 흐름.
