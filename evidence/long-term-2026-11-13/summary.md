# Phase Progress Report — 장기 운영 감사 1단계

- 날짜·시간(KST): 2026-11-13 (세션 실행, 장기 시험 아님)
- 작업 단계/티켓: LT-01 완료(문서), LT-02 완료(Host/Build 관측 기반). LT-03~10은 감사 계획에 기록, 미완료.
- 커밋 SHA / 공식 기반 commit: main `432fdf6` 기반 / Seeed `8d97d8912987c07529287629f7e73039bb0e6384`. 최종 커밋은 이 보고를 포함한 git 이력 참조.
- 작업 환경(OS/PlatformIO/보드 패키지 버전): Windows PowerShell, Python 3.15, `espressif32@8.0.0`, `seeed_xiao_esp32s3`, Arduino/qio_opi.
- 기기 연결 여부 및 모델 확인: 포트 열기·기기 접근 없음. 열거 결과 COM3/COM4만 있으며 모델 미확인. 과거 COM5 실기 기록을 이번 연결 확인으로 사용하지 않았다.
- API 실키 접근: 없음. 기본 환경 빌드이며 개발 자격증명 파일을 읽지 않았다.

## 구현 내용

- 변경 파일: `docs/17_LONG_TERM_OPERATIONS_AUDIT.md`, `docs/adr/0027-long-term-observability.md`, `docs/10_SOURCES_AND_OPEN_QUESTIONS.md`, `docs/12_COVERAGE_AND_GAPS.md`, `docs/13_ROADMAP.md`, `docs/tasks/T11_hw_validation_soak.md`, `src/main.cpp`, `tools/soak_report.py`, `host/tests/test_soak_report.py`, 이 증거 폴더.
- 변경 목적: 사전 감사 및 충전 보호 선행 조건·단계별 장기 검증 기준 수립. 연속 운영 중 내부 RAM 단편화·PSRAM·uptime 관측, USB 연속 관측 기간/공백/리셋 검사.
- 기존 기능 영향: 60초마다 UART 진단 1줄 및 heap 통계 조회. NVS 쓰기·의존성·충전 레지스터·파티션 변경 없음. 기존 soak 호출은 유지하되 PASS 범위를 JSON `scope`로 명시한다.

## 재현 가능한 증거

| 구분 | 명령/방법 | 결과 (PASS/FAIL/NOT_RUN) | 검증 단계 (Host/Build/Hardware/LiveAPI) | 증거 경로 |
|---|---|---|---|---|
| Clean build | clean 미실행. `pio run -e reterminal_e1001` 증분 빌드 | PASS: RAM 78,388 B / Flash 1,968,950 B | Build | `build.log` |
| Unit tests | `python tools/run_native_tests.py` | PASS: 131개, 실패 0 | Host | `native-tests.log` |
| Unit tests | `python -m pytest host/tests -q` | PASS: 177개, 실패 0. 외부 Python 패키지 deprecation 경고 13개 | Host | `pytest.log` |
| Fault injection | 합성 health 로그의 샘플 부족·기간 부족·공백·uptime 초기화/중복·재부팅·USB 해제·잘못된 형식, 32bit millis 경계 이후 64bit 값 | PASS: 신규 13개 테스트에 포함(기존 164개 → 177개) | Host | `host/tests/test_soak_report.py`, `pytest.log` |
| Fault injection | `python tools/soak_report.py test/fixtures/soak_ok.log --min-duration-sec 86400` | PASS: 짧은 합성 fixture 거부(exit 1, verdict FAIL), 실기 결과 아님 | Host | `strict-short-log.json` |
| Lint | `python -m ruff check tools/soak_report.py host/tests/test_soak_report.py`, `git diff --check` | PASS. 첫 lint의 import 정렬 지적은 수정 후 통과 | Host | `lint.log`, 세션 출력 |
| Host preview | UI 변경 없음 | NOT_RUN | Host | — |
| E1001 smoke | 기기 포트 미접근, 플래시 없음 | NOT_RUN | Hardware | — |
| Live API | 호출 없음 | NOT_RUN | LiveAPI | — |
| Sleep/wake | 실기 반복 미실행 | NOT_RUN | Hardware | — |
| USB 24h/72h/7일·배터리 시험·충전 전류 | 이번 세션에서 수행 불가 | NOT_RUN | Hardware | — |

## 독립 검증 및 보안

- 단위/포맷/환율 기준일 Oracle: 기존 Python 177/C++ 131 회귀에 KST/공휴일·영업일/JPY(100)/결측 관련 기존 검사가 포함됨. 제공자 의미·금융 계산 변경 없음.
- Mutation 테스트 결과: NOT_RUN (이번 변경에 새 mutation은 추가하지 않음).
- 자격증명 유출 검사 범위/결과: 기존 보안 회귀 포함 pytest 통과. 신규 증거 log/JSON은 `redact_text` 적용 후 `find_secrets` 0건. 홈 경로 마스킹 및 PowerShell UTF-16 출력을 UTF-8로 정규화. 원래 경로의 로그를 API 키 검증 증거로 쓰지 않는다.
- 사용한 API 출처와 기준일 검증: 신규 API 호출 없음. Seeed E1001 회로도 v1.2 및 Silergy SY6974B Rev.0.9 원문을 2026-11-13 확인. 사용자 실물 리비전 일치는 미확인. 소스·페이지는 감사 문서 §4.

## 미완료/블로커

- BLOCKED_HW: E1001 실물 리비전/충전 칩 확인, ADC/SOC 오차·충전 전류, USB 탈착·watchdog 복귀, 장기 heap·전류·sleep/wake·저전압 회복. 80% 제한은 아직 구현하지 않았다.
- BLOCKED_CREDENTIALS: 이 단계에는 필요 없음. 후속 실 API 장애/복구 시험은 기기 로컬 설정을 사용하며 채팅으로 키를 요구하지 않는다.
- BLOCKED_NETWORK: Git remote가 등록되어 있지 않아 main push 불가. 외부 문서 열람 및 로컬 빌드는 성공.
- 기타 이슈: `uart_logger.py`는 stub(LT-10). 새 health 검사는 접두어 없는 세척 UART 줄을 받는다. 주기적 진단은 장시간 블로킹 중 지연될 수 있으며 strict gate가 공백을 드러낸다. 단일 USB 세션용 검사이므로 배터리 deep sleep 로그에 적용하지 않는다.

## 게이트 판정

- 이번 단계 목표: PARTIAL — 사전 문서·관측 개선의 Host/Build는 PASS. 실제 장기 운영 안정성 및 충전 제한은 미검증/미구현.
- 다음 단계: LT-03 리비전·칩 ID·배터리 정격 확인 → 충전 정책 및 watchdog ADR → Mock fault/Build → 승인된 실기 적용·전류 측정. 동시에 LT-04/05/06의 I2C·네트워크·정지 복구 감사를 진행한다.
- 실기 없이 성공이라고 표시하지 않은 항목: 실제 health 로그 출력/전류 영향, 80% 유지, 재연결, 패널 장기 갱신, USB/배터리 장기 운영, sleep/wake.
