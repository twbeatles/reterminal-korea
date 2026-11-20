# Phase Progress Report — stage-2 (T02~T10 HOST + T12)

- 날짜·시간(KST): 2026-11-02
- 작업 단계/티켓: T02(저장) T03(시계) T04a~c(화면 HOST) T04d(스텁) T05(전원계획)
  T06(버튼) T07(포털 핸들러) T08(센서·배터리 코덱) T09(달력) T10(패키징)
  T12(F-01/02/03/04/05/08/09/10) + T01/T11 도구 스켈레톤
- 커밋 SHA / 공식 기반 commit: 미커밋 작업트리 (이 세션에서 커밋하지 않음)
- 작업 환경(OS/PlatformIO/보드 패키지 버전): Windows, espressif32@8.0.0,
  seeed_xiao_esp32s3, Arduino
- 기기 연결 여부 및 모델 확인: 실기 없음 (전부 BLOCKED_HW로 표기)
- API 실키 접근(키 자체 절대 쓰지 말 것): 없음 (BLOCKED_CREDENTIALS)

## 구현 내용
- 변경 파일:
  - 신규: `src/storage/{Codec,SettingsCodec,SnapshotCodec,SlotStore,NvsStorage}.*`
  - 신규: `src/domain/{ClockPolicy,Calendar}.*`, `src/drivers/{Pcf8563,SystemClock,ButtonDecoder,Sht4xCodec,EpdPanel,Watchdog,Pcf8563_esp32}.*`
  - 신규: `src/ui/{Format,ViewModel,BitmapFont,Canvas,Renderer}.*`, `src/power/{BatteryPolicy,CyclePlanner,RtcState}.*`
  - 신규: `src/provision/{GeoGrid,PortalHandler}.*`, `src/app/{RequestBudget,DemoProvider,ProviderName}.*`, `src/providers/MarketSession.*`, `src/util/Log.*`
  - 수정: `src/main.cpp`(NVS→dev→기본값 부팅, RTC+NTP 정책), `src/drivers/SensorCheck.cpp`(범위밖→OutOfRange), `src/storage/CacheStore.cpp`(스키마 1·2 허용)
  - 테스트: `test_native/test_stage2.cpp`(17건) + `host/kdash/extended.py` + `host/tests/test_stage2.py`(7건)
  - 도구: `font_subset/make_bitmap_font.py`, `check_sources.py`, `api_probe.py`, `render_previews.*`, `uart_logger.py`, `soak_report.py`, `scripts/package_release.py`
  - 문서: ADR 0004/0005/0006, `docs/12`, `docs/tasks/T02~T12` 상태, `THIRD_PARTY_NOTICES.md`, `.gitignore`(probe raw/)
- 변경 목적: 로드맵(13) 권장 순서 T02→T03→T04→T06→T08→T05→T07→T09→T10→T12의
  Host 가능 범위를 한 세션에서 끝까지 구현. 실기·키 필요분은 스텁·BLOCKED로 분리.
- 기존 기능 영향: 기존 19 네이티브·90 pytest 전부 통과 유지. `SensorCheck` 범위밖
  반환값이 `NoData`→`OutOfRange`로 변경(헤더 enum 계약 fulfil, 기존 테스트 무영향).

## 재현 가능한 증거
| 구분 | 명령/방법 | 결과 (PASS/FAIL/NOT_RUN) | 검증 단계 (Host/Build/Hardware/LiveAPI) | 증거 경로 |
|---|---|---|---|---|
| Clean build | `pio run -e reterminal_e1001` | PASS | Build | RAM 21.3% / Flash 37.7% |
| Unit tests | `python tools/run_native_tests.py` | PASS (36/36) | Host | 터미널 로그 |
| Unit tests | `python -m pytest host/tests -q` | PASS (97) | Host | 터미널 로그 |
| Fault injection | 정전 주입·비트반전·CRC 변조 (test_slotstore_powercut 등) | PASS | Host | `test_native/test_stage2.cpp` |
| Host preview | `python tools/render_previews.py` (10종) | PASS (여백 oracle 전부 clean) | Host | `evidence/p1/host-preview/*.png` |
| E1001 smoke | 실기 없음 | NOT_RUN | Hardware | BLOCKED_HW (T11 체크리스트 대기) |
| Live API | 키 없음 | NOT_RUN | LiveAPI | BLOCKED_CREDENTIALS (T01 계획만) |
| Sleep/wake | 실기 없음 | NOT_RUN | Hardware | BLOCKED_HW |

## 독립 검증 및 보안
- 단위/포맷/환율 기준일 Oracle: 기하·픽셀 성질 검사(여백 금지대·칸 bbox·배지 반전율),
  `1,384.50`·`고시 기준일` 무실시간 문구, 자정경계 `10-31 23:50`.
- Mutation 테스트 결과: Missing→0 변조 시 실패(`--` 강제), 257B·CSRF·XSS·1단계 리셋
  거부 케이스 포함. 정전 100회 무작위 중단 시뮬레이션은 미수행(후속).
- 자격증명 유출 검사 범위/결과: 전수 `find_secrets` 스캔 + `test_sec01_*` 통과.
  (`PortalHandler.cpp` 변수명 오탐 1건 수정함.)
- 사용한 API 출처와 기준일 검증: 가이드 원문 미대조 항목은 코드에 명시
  (격자 상수·SHT4x 공식·PM 등급·NXT 시간은 "출발값/재확인 필요").

## 미완료/블로커
- BLOCKED_HW: RTC 드리프트·sleep/wake 100회·버튼 오인식률·SHT40 기준계·배터리
  멀티미터·포털 실시나리오·화면 사진·잔상·전류·Flash 32MB·USB복구·soak 전부.
- BLOCKED_CREDENTIALS: T01 probe 전체, 특일 실응답, EXIM/KIS/FSC Live.
- BLOCKED_NETWORK: 없음.
- 기타 이슈 (재현/회피책):
  - T04b 폰트 비트맵 미생성(TTF 다운로드 후 `make_bitmap_font.py` 실행 필요).
    프리뷰 한글은 박스 표시, 지오메트리만 검증됨.
  - T04d 전송 스텁(`push_full` false 반환, 갱신 주장 안 함).
  - F-06 근접측정소·F-07 프리뷰도구 정리 미완.
  - `main.cpp` USB idle 루프 유지(deep sleep 진입은 T05 실기 단계).

## 게이트 판정
- 이번 단계 목표 PASS/FAIL/PARTIAL: PARTIAL (HOST+BUILD 전부 PASS, HW·Live 미검증).
- 다음 단계에서 해야 할 일: 실기 입수 시 T11 체크리스트 순서대로 검증하고
  `docs/12`를 `DONE_VERIFIED_HW`로 갱신. 키 발급 시 T01 probe 후 골든 반영.
- 실기 없이 성공이라고 표시하지 않은 항목: 위 미완료 전부. 빌드 성공을
  실기 동작으로 격상하지 않음.
