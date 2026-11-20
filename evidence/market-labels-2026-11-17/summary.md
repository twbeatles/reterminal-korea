# Phase Progress Report — 시장·달력 휴장 문구 수정

> 후속: 사용자의 업로드 요청에 따라 COM5 실기 업로드·부팅·통신·패널 드라이버 완료를 확인했다. 아래 최초 구현 시점의 미업로드 기록은 이력을 유지하며, 최신 실기 상태는 [실기 업로드 보고](device-summary.md)를 따른다.

- 날짜·시간(KST): 2026-11-17
- 작업 단계/티켓: P1-03/P1-04 후속 MARKET-LABEL-01 완료(Host/Build)
- 커밋 SHA / 공식 기반 commit: 작업 시작 `6a64954f12ac9f93cf72e28acd6909e41a43cb5a` / Seeed `8d97d8912987c07529287629f7e73039bb0e6384`
- 작업 환경(OS/PlatformIO/보드 패키지 버전): Windows / PlatformIO / espressif32@8.0.0 / Arduino-ESP32 3.3.12
- 기기 연결 여부 및 모델 확인: 이번 작업에서는 연결을 조회하거나 기기에 접근하지 않음. 대상은 E1001만. 업로드·초기화 없음.
- API 실키 접근(키 자체 절대 쓰지 말 것): 실제 API 호출 없음. 보안 검사는 로컬 비밀과의 일치 여부만 검사하며 값을 출력하지 않음.

## 구현 내용
- 변경 파일: `src/domain/Calendar.{cpp,h}`, `src/providers/FinanceParsers.h`, `src/ui/{Pages.h,Renderer.cpp,ViewModel.h}`, `test_native/{test_audit_flow.inc,test_audit_followup.inc}`, `tools/render_previews.cpp`, `docs/{05_KOREAN_EINK_UI.md,10_SOURCES_AND_OPEN_QUESTIONS.md,adr/0033-market-state-labels.md}`, 본 보고서.
- 변경 목적: 거래소 연간 일정 미검증을 `휴장 가능`으로 표시하던 오류 수정. 공휴일 표만 수신한 평일은 `정규장 시간/정규장 외`, 표 미수신은 `일정 미확인`, 시계 미검증은 `시간 미확인`. 확인된 휴장은 그대로 유지. 달력 고정 문구를 기호 범례로 명확화.
- 기존 기능 영향: 표시 문자열 및 상태 버퍼(16→32바이트) 변경. 가격·조회·거래일 판정·저장 형식·날짜별 달력 마크는 유지. 보드·드라이버·핀·파티션·라이브러리 변경 없음.

## 재현 가능한 증거
| 구분 | 명령/방법 | 결과 (PASS/FAIL/NOT_RUN) | 검증 단계 (Host/Build/Hardware/LiveAPI) | 증거 경로 |
|---|---|---|---|---|
| Clean build | `pio run -e reterminal_e1001` (증분 빌드) | PASS, RAM 78,468 B / Flash 1,977,446 B. Clean rebuild는 NOT_RUN | Build | `build.log` |
| Unit tests | `python tools/run_native_tests.py` / `python -m pytest host/tests -q` | PASS, C++ 155개·Python 209개, 실패 0. Python 경고 13개 | Host | `native.log`, `python.log` |
| Fault injection | C++ 전체 회귀에 포함된 시계·표 미수신, 공급자·저장 장애 테스트 | PASS (155개에 포함, 별도 합산하지 않음) | Host | `native.log` |
| Host preview | `python tools/render_previews.py --output-dir evidence/market-labels-2026-11-17/previews` | PASS, 21종 margin=1. 새 시장·달력 화면 2종 육안 확인, 한글 누락/잘림 없음, DEMO 표시 | Host | `preview.log`, `previews/market-regular-hours.png`, `previews/calendar-business-day.png` |
| E1001 smoke | 미실행 | NOT_RUN | Hardware | — |
| Live API | 미실행 | NOT_RUN | LiveAPI | — |
| Sleep/wake | 미실행 | NOT_RUN | Hardware | — |

## 독립 검증 및 보안
- 단위/포맷/환율 기준일 Oracle: 09:00/15:30 경계, 주말·공휴일·명시적 휴장, 표/시계 미확인 및 2026-11-17 평일 회귀를 명시 입력과 문자열 기대값으로 검증. 상태가 ViewModel에서 UTF-8 잘림 없이 유지됨. 전체 회귀에 KST·자정·영업일·JPY(100)·측정소 결측 포함.
- Mutation 테스트 결과: 이번 표시 변경에서 별도 실행하지 않음.
- 자격증명 유출 검사 범위/결과: 변경 소스·문서와 이번 로그·프리뷰·펌웨어를 패턴 및 알려진 로컬 비밀 값과 대조. `security.log` 참고. `git diff --check` 통과. 로그/프리뷰는 Git 제외 대상으로 유지하고 보고서만 추적.
- 사용한 API 출처와 기준일 검증: `python tools/check_upstream.py` → `pinned=8d97d891 status=ahead ahead_by=1 changed=5 relevant=0`. 공식 LVGL 예제 원문의 E1001 보드/PSRAM/qio_opi/해상도 재대조. 금융 API·휴장일 실응답은 신규 검증하지 않음(ADR 0033).

## 미완료/블로커
- BLOCKED_HW: 수정 펌웨어의 실제 E1001 화면·버튼·수면 동작 확인 미실행. 실기 연결 상태도 이번에는 확인하지 않음.
- BLOCKED_CREDENTIALS: 신규 금융 일정 API 검증 없음. 표시 수정에는 실제 키 불필요.
- BLOCKED_NETWORK: 없음. Git remote 미설정으로 push 불가.
- 기타 이슈 (재현/회피책): 거래소 특별 개장·휴장 일정은 기존 미검증 상태 유지. `정규장 시간/외`는 시각 기준이며 실제 개장 확정이 아님.

## 게이트 판정
- 이번 단계 목표 PASS/FAIL/PARTIAL: PASS (Host/Build 표시 수정), 실기 확인은 미완료.
- 다음 단계에서 해야 할 일: 사용자 요청에 따라 수정 펌웨어를 업로드한 뒤 실제 시장/달력 화면 확인. 원격 저장소가 설정되면 main push.
- 실기 없이 성공이라고 표시하지 않은 항목: 업로드, 실제 API 통신, 실제 패널 표시, 부분 갱신, Sleep/wake, 전류 소비.
