# Phase Progress Report — 아이콘 + 카드형 UI (ADR 0008)

- 날짜·시간(KST): 2026-11-02
- 작업 단계/티켓: UI/UX 리팩토링. 설계 `docs/superpowers/specs/2026-11-02-eink-icon-card-ui-design.md`, 계획 `docs/superpowers/plans/2026-11-02-eink-icon-card-ui.md` Task 1~5
- 커밋 SHA / 공식 기반 commit: 저장소에 커밋 없음(전부 untracked) / 공식 예제 @8d97d891 기준은 변경 없음
- 작업 환경: Windows 11, Python 3.15, Pillow 13.0.0, ziglang c++ (host), PlatformIO espressif32@8.0.0
- 기기 연결 여부 및 모델 확인: **미연결**. E1001 실기 확인을 하지 않았다.
- API 실키 접근: 사용하지 않음. 모든 화면은 DemoProvider/fixture 데이터로 렌더링했다.

## 구현 내용
- 변경 파일:
  - 신규: `tools/icon_subset/make_icons.py`, `src/ui/icons/IconData.h`(생성), `src/ui/icons/LICENSE-Apache-2.0.txt`,
    `src/ui/Icons.h`, `src/ui/Icons.cpp`, `test_native/test_ui_icons.inc`, `host/tests/test_icons_gen.py`,
    `docs/adr/0008-icon-card-ui.md`, 설계/계획 문서, 이 파일
  - 수정: `src/ui/Renderer.cpp`(카드형 재작성), `src/ui/ViewModel.{h,cpp}`(icon/now_icon/now_text/pair/wifi·battery icon, 헤더 날짜),
    `src/ui/Format.{h,cpp}`(WeatherIconFor/HeaderDateTime/BatteryIconFor), `src/ui/Canvas.{h,cpp}`(missing_glyphs 카운터),
    `test_native/test_core.cpp`(include + RUN_TEST 3개), `tools/render_previews.py`(Windows 경로 버그),
    `tools/render_previews.cpp`(시나리오 11 market-watchlist), `docs/05`, `docs/10`, `THIRD_PARTY_NOTICES.md`
- 변경 목적: 날씨·대기질·환율·종목·기기 상태를 아이콘과 카드로 구분하고, 핵심값을 64px로 강조한다.
- 기존 기능 영향: 헤더 시각이 `HH:MM`에서 `MM-DD(요) HH:MM`으로 바뀌었다. 페이지명은 헤더에서 푸터(`●○○ 생활`)로 옮겼다.
  DEMO는 푸터에 반전 배지로 표시한다. 렌더러 공개 API(시그니처)는 바뀌지 않았다.

## 재현 가능한 증거
| 구분 | 명령/방법 | 결과 | 검증 단계 | 증거 경로 |
|---|---|---|---|---|
| Clean build | `pio run -e reterminal_e1001` | PASS (RAM 70,804B / Flash 1,671,290B) | Build | 콘솔 로그(이 세션) |
| 아이콘 Flash 실측 | `xtensa-esp-elf-nm -S firmware.elf \| grep icondata` | 35 심볼 6,536B, 중복 없음 | Build | — |
| Stack gate | `python tools/check_stack_usage.py --elf …/firmware.elf` | PASS (`failures: []`) | Build | — |
| Unit tests (native) | `python tools/run_native_tests.py` | PASS 54 Tests 0 Failures (기준 51 + 신규 3) | Host | — |
| Unit tests (python) | `python -m pytest -q host/tests` | PASS 143 passed (기준 139 + 신규 4) | Host | — |
| Host preview | `python tools/render_previews.py` | PASS 11장 모두 `margin=1`, 눈검사 완료 | Host | `evidence/p1/host-preview/*.png` (gitignore 대상, 재생성 가능) |
| E1001 smoke | — | NOT_RUN | Hardware | BLOCKED_HW |
| Live API | — | NOT_RUN | LiveAPI | 범위 밖 |
| Sleep/wake | — | NOT_RUN | Hardware | 범위 밖 |

### 신규 테스트가 고정하는 것
- `test_icon_tables_and_draw`: 아이콘 24개 모두 24px 보유, 64px 보유 집합 = 날씨 7종 + $/¥, bbox 밖 무픽셀, None/Count/미보유는 no-op, 화면 가장자리 클리핑.
- `test_icon_view_model`: SKY{1,3,4,-1,9}×PTY{-1,0..8} 전 조합 매핑, KST 자정 경계 헤더(`11-01(일) 23:59`→`11-02(월) 00:00`),
  배터리 단계, Life/Market/Device의 VM 아이콘과 `KRW / USD`·`KRW / 100JPY` 표기.
- `test_card_renderer`: 5개 시나리오(정상/무데이터/긴 지명/DEMO/Wi-Fi 끊김+stale) × 3페이지에서 여백·열 간격 청정,
  **글리프 누락 0개**(Canvas 폴백 카운터), 히어로 아이콘·헤더 Wi-Fi 아이콘·stale 배지 존재, 카운터 자체 동작.
- `test_icons_gen.py`: 생성 결정성, 커밋 헤더 = 재생성 결과(드리프트 방지), sha256 불일치 입력 거부, enum 순서, 8KB 예산.

### 눈검사 기록 (11장)
- life-day / life-badtime / life-longkr / life-demo / life-nodata: 카드 4개, 히어로 아이콘과 64px 기온 표시. 무데이터 시 `?` 아이콘, `--`, `확인 필요` 배지.
  시간 미확인 시 헤더에 `시간 확인 필요` 반전 배지.
- market-night / market-weekend / market-nokis / market-watchlist: $/¥ 64px 아이콘과 64px 고시환율, 고시 기준일.
  종목 행에 `전일 종가` 레이블, stale 행에 `오래된 정보` 배지, 하단에 시장 상태.
- device-nonet / device-lowbat: Wi-Fi 미연결 아이콘, 배터리 단계 아이콘, 공급자별 수신 목록.
- 프리뷰 PNG는 `render_previews.cpp`가 우하단에 `DEMO` 스탬프를 찍는다(fixture 화면임을 표시).

## 독립 검증 및 보안
- 단위/포맷/환율 기준일 Oracle: 기존 테스트 무변경 통과. 고시환율에는 `고시`와 기준일, 종가에는 `전일 종가`와 기준일이 그대로 표시된다.
- Mutation 테스트: 수행하지 않음. 테스트 작성 중 의도적 RED를 확인했다(생성기 부재, Icons.h 부재, VM 필드 부재, 구 렌더러의 히어로 아이콘 부재).
- 자격증명 유출 검사: `kdash.secret_scan.find_secrets`로 신규/변경 파일 10개를 검사해 0건.
- 출처: Material Symbols 원본 URL·라이선스·sha256을 docs/10 #13과 THIRD_PARTY_NOTICES에 기록(확인일 2026-11-02).
- `tools/check_sources.py`: errors=1. 원인은 `https://wiki.seeedstudio.com/work_with_lvgl/` HTTPError로, 이번 변경과 무관한 기존 외부 링크다.

## 미완료/블로커
- BLOCKED_HW: 24px 아이콘의 실제 판별성, 64px 숫자의 잔상·고스팅, 헤더/푸터 가독성은 E1001 실기에서만 판정할 수 있다.
- BLOCKED_CREDENTIALS: 해당 없음.
- BLOCKED_NETWORK: 해당 없음(아이콘 TTF는 내려받아 `build/icons/`에 둠, gitignore). TTF가 없는 환경에서는 생성기 테스트 2개가 SKIP한다.
- 기타: 저장소에 커밋이 없어 이번 변경도 커밋하지 않았다.

## 게이트 판정
- 이번 단계 목표: PARTIAL. 호스트·빌드 기준으로는 PASS이고, 실기 가독성은 미검증이다.
- 다음 단계: E1001에 플래시해 3페이지를 사진으로 남기고 아이콘·64px 숫자 가독성을 확인한다. 필요하면 wght나 크기를 조정한다.
- 실기 없이 성공이라고 표시하지 않은 항목: 아이콘 판별성, 잔상, 부분 갱신과의 상호작용.
