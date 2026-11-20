# Phase Progress Report — 감사 개선 통합

- 날짜·시간(KST): 2026-11-02, 감사 수정 및 이어서 진행한 통합 검증.
- 작업 단계/완료 티켓: ISSUE-001/002/003 코드 수정·Host/Build 검증, GAP-01~05/07~09 연결·회귀, GAP-06/10의 소프트웨어 부분 구현. Hardware/LiveAPI 티켓 완료 처리 없음.
- 커밋 SHA / 공식 기반 commit: 재개 시 기준 main ffeb539 / Seeed 공식 8d97d8912987c07529287629f7e73039bb0e6384. 최종 커밋은 Git 이력에서 확인.
- 작업 환경: Windows PowerShell, Python 3.15, ziglang 대체 네이티브 러너, espressif32 8.0.0, Arduino-ESP32 3.3.12, ArduinoJson 7.4.3.
- 기기 연결 여부 및 모델 확인: 없음. 기기 플래시·초기화·eFuse 변경 없음.
- API 실키 접근: 없음. 실 API 인증 호출 없음.

## 구현 내용

- 변경 파일(감사 이후 전체 범위):
  - src/storage/{SlotStore,SettingsCodec,SnapshotCodec,RuntimeStore,CalendarStore}.* — checked heap, 실제 슬롯 기준 복구, 상태/달력 영속화·readback.
  - src/drivers/ClockBootstrap.*, src/net/{BodyReader,HttpTransport}.* — 실제 런타임이 호출하는 시계 부트스트랩/본문 완료 판정.
  - src/main.cpp, app/Coordinator.*, Fetcher.*, power/{CyclePlanner,RtcState}.* — 기기 흐름 연결, deadline·강제 갱신 게이트·복원·버튼/절전·저전압.
  - src/drivers/{Uc8179,EpdPanel,Sht4x_esp32}.*, ui/Canvas/Renderer/ViewModel/Format 및 폰트 — 실제 glyph·패널 명령·센서·단위/기준시각/실패 표시.
  - src/provision/PortalHandler.*, PortalServer.* 및 ESP32 어댑터 — 입력/CSRF/헤더 제한, 접속시험, 검증 저장, 종료/명시적 초기화.
  - src/providers/HolidayService.*, host/kdash/holidays.py — 공식 특일 계약 파서·조회. Calendar의 public loaded와 market_verified 분리.
  - scripts/package_release.py, tools/api_probe.py 및 CI — 바이너리 정체성/비밀 검사, 실제 읽기 전용 opt-in probe, 환경 실패를 성공으로 취급하지 않는 게이트.
  - test_native/test_audit_*.inc, test_core.cpp, host/tests/test_audit_*.py — fake I/O 및 오류 주입 회귀.
  - .gitattributes — 생성 glyph/icon 표의 LF 고정. 기존 아이콘 카드형 UI와 사용자가 그 사이 반영한 변경은 보존.
  - README, PROJECT_AUDIT §10, docs/12·13·BUILD·RECOVERY·ADR 0007 및 본 증거.
- 목적: 감사의 기능 결함을 수정하고 흩어진 모듈을 실제 E1001 애플리케이션 흐름에 연결.
- 통합 검토 추가 수정: 저전압을 버튼/AP보다 우선, 로컬 센서 due와 네트워크 due 분리, 연결 실패 백오프, 새 센서 값 렌더 판단, 내년 달력 이월, KRX 신뢰 분리, 환율 scale 범위 제한.
- 기존 기능 영향: provider 골든 40건 유지. Snapshot v2는 v1을 읽고 CalendarStore v2도 v1을 읽는다. 토큰은 NVS에 저장하지 않는다. 부분 갱신은 계속 false다.

## 재현 가능한 증거

| 구분 | 명령/방법 | 결과 | 검증 단계 | 증거 경로 |
|---|---|---|---|---|
| Python 전체 | python -m pytest host/tests -q | PASS: 144 passed / 실패 0 / warnings 13 | Host | 세션 출력 및 아래 테스트 파일 |
| C++ 전체 | python tools/run_native_tests.py | PASS: 60 Tests / 0 Failures / 0 Ignored; 내부 provider 골든 40건 | Host | test_native/test_core.cpp 및 포함된 audit tests |
| Firmware | pio run -e reterminal_e1001 | PASS: 증분 빌드, RAM 70,804 B (21.6%), Flash 1,674,526 B (50.1%) | Build | .pio/build/reterminal_e1001/firmware.elf |
| Clean build | 이번 재개 작업에서 미실행 | NOT_RUN | Build | 증분 빌드와 구별 |
| 정적 스택 | python tools/check_stack_usage.py --json evidence/audit-remediation/stack-current.json | PASS: 최대 app frame 2,432 B, 알려진 경로+2,048 B reserve가 8,192 B 이내 | Build artifact | stack-current.json |
| Fault injection | 네이티브 suite의 슬롯/meta 연속 실패, HTTP EOF/timeout/oversize, 시계 불신 | PASS, 위 60개에 포함 | Host | test_audit_storage.inc, test_audit_runtime.inc |
| 72h/reboot | 5분 간격 가상 시계 72시간, 중간 재부팅/NVS 복원 | PASS: KMA 실황 회차 요청 72회, 쓰기 예산 이내 | Host | test_audit_flow.inc |
| Host preview | python tools/render_previews.py | PASS: 12종 PBM/PNG, 모두 margin=1 | Host | .pio/preview/, evidence/p1/host-preview/ |
| 실제 바이너리 검사 | scripts.package_release.inspect_images를 현재 0.3.0 이미지에 실행 | PASS: board/version, 필수 3개 이미지, 알려진 비밀/marker/패턴 검사 | Build artifact | binary-check.json |
| Git whitespace | git diff --check | PASS | Source | 세션 출력 |
| E1001 smoke | 미실행 | NOT_RUN | Hardware | BLOCKED_HW |
| Live API | 미실행 | NOT_RUN | LiveAPI | BLOCKED_CREDENTIALS |
| Sleep/wake/전류 | 미실행 | NOT_RUN | Hardware | BLOCKED_HW |

## 독립 검증 및 보안

- 단위/포맷/환율 Oracle: 공통 골든 40건, JPY(100), KST/자정, 결측 -, 고시/종가/REST 구분 유지. 추가 테스트는 저전압 버튼, 로컬 RF 분리, 달력 이월/신뢰/저장, deadline 및 72h reboot를 검사한다.
- 스택: 부팅 설정 읽기 2,160 B + reserve 2,048 B = 4,208 B. 이전 감사의 slot::Read 9,440 B 프레임은 제거됐다. 현재 검사는 SDK 간접 호출/전체 동적 스택의 증명이 아니며 실기 high-water-mark가 필요하다.
- UI: 생활·최대 관심종목·안전모드 PNG를 직접 확인. DEMO fixture 워터마크가 있으며 실제 기기 출력 사진으로 취급하지 않는다.
- Mutation: 이번에는 미실행.
- 자격증명: SEC-01 포함 전체 pytest 통과. 실제 키 열람·출력 없음. 현재 3개 이미지의 실제 비밀 패턴 검사 통과. 임의 바이너리에 대한 무기밀성 보장은 하지 않는다.
- 바이너리 검사 최초 실패: mbedTLS의 독립된 NUL-terminated PEM delimiter 상수를 키로 오인했다. 정확한 독립 상수만 구별하며 newline/본문이 있는 PEM 형식은 계속 차단되는 회귀를 추가했다. 검사 범위를 넓게 제외하지 않았다.
- Python 최초 실패: 생성 IconData.h의 CRLF/LF 불일치 1건. 생성 테이블 LF 속성 적용 후 전체 통과. 외부 pytest plugin deprecation warning은 남아 있다.
- API 출처: 기존 docs/10과 공식 E1001 원문. SHT4x는 Sensirion v7.3(2026-06), 특일은 공식 data.go.kr 기술 첨부 및 HTTPS 상세 페이지(ADR 0007). 실제 인증 응답 검증은 하지 않았다.

## 미완료/블로커

- BLOCKED_HW: 부팅/스택 최저 여유, 실제 NVS 정전, 패널 방향·파형·갱신시간·잔상, AP/브라우저 저장, 100회 timer/버튼 wake, 저전압·USB 변화, 전류/센서 기준계, 실제 USB 복구.
- BLOCKED_CREDENTIALS: KMA 발표 지연/최대 응답, AirKorea 필드, EXIM 기준일, FSC/KIS 계약·NXT/통합 거래시간, 특일 실응답. 시각별 probe가 필요하다.
- BLOCKED_SOURCE: 연도별 KRX 추가 휴장일의 공식 출처를 확정하지 않았다. public holiday 표만으로 거래소의 장중/영업일을 확정 표시하지 않는다.
- BLOCKED_GIT_REMOTE: remote가 설정되지 않아 push 대상 없음. main에서 작업하며 별도 branch/worktree 없음.
- 기타: Linux/macOS·원격 CI 미실행. 이 환경의 PIO native GCC 경로는 준비되지 않았으며 대체 네이티브 러너를 사용했다. 부분 갱신/OTA/32MB 파티션은 미활성화.
- 전체 active 시간 상한은 SDK blocking 단계/패널 구동/포털 세션을 포함한 하드 실시간 보장이 아니다. 요청 시작/수신에는 deadline이 있고 전체 전력·응답성은 실기 계측이 필요하다.

## 게이트 판정

- 이번 단계 목표: **소프트웨어 수정·Host/Build PASS**, **Hardware/LiveAPI 릴리스 PARTIAL**.
- 다음 단계: 물리 E1001에서 부팅/스택·화면·AP·wake smoke 후 전류/복구를 확인하고, 본인 키로 opt-in probe를 수행한다. 승인 없는 플래시/설정 초기화는 실행하지 않는다.
- 실기 없이 성공이라고 표시하지 않은 항목: 기기 정상 부팅·실 API 성공·화면 출력·부분 갱신·Deep Sleep·버튼 wake·배터리 수명·USB 복구 전부.

## 2026-11-03 추가분 (PROJECT_AUDIT.md §10 조치)

- Python 전체: `python -m pytest host/tests -q` → PASS: 149 passed / 실패 0 / warnings 13.
- C++ 전체: `python tools/run_native_tests.py` → PASS: 63 Tests / 0 Failures (ziglang + .pio 캐시, gcc 미설치).
- 뮤테이션 게이트: `python tools/mutation_check.py` → PASS (2 mutants killed).
- soak 파서 게이트: `python tools/soak_report.py test/fixtures/soak_ok.log` → PASS (fixture 수준).
- Firmware: `python -m platformio run -e reterminal_e1001` → SUCCESS (31.4s, RAM 70,804 B / Flash 1,675,478 B).
- 실-C++ 프리뷰: 12종 PBM 생성, 모두 margin=1.
- 실기·실 API·OTA/USB E2E는 여전히 BLOCKED_HW로 미실행이며 성공으로 표시하지 않는다.
