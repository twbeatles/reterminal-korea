# Phase Progress Report — 장기 운영 추가 감사

후속 적용: 사용자가 24h 검증 보류 및 추가 이미지 flash를 요청하여 [실기 적용 보고](../followup-flash-2026-11-13/summary.md)로 이어졌다. 아래는 적용 전 Host/Build 단계의 기록이다.

- 날짜·시간(KST): 2026-11-13 10:46.
- 작업 단계/티켓: LT-11 저장 장애 재시도/쓰기 예산, LT-12 PMIC 확정 진단, LT-13 관측 손실 검출. Host/Build 완료, 실기 적용 대기.
- 커밋 SHA / 공식 기반 commit: 이 보고서 포함 커밋은 `git log -1 --format=%H -- evidence/long-term-followup-2026-11-13/summary.md`로 확인. 작업 기준 `52087b5`, 공식 예제 `8d97d891`.
- 작업 환경: Windows, espressif32@8.0.0, Arduino-ESP32 3.3.12, E1001/qio_opi. 보드/핀/파티션 변경 없음.
- 기기 연결 여부 및 모델 확인: 기존 확인 E1001 COM5, 기본 배터리. PID 12536의 24h UART 수집 유지.
- API 실키 접근: 외부 개인 개발 헤더로 개발 빌드 확인. 값·값 해시는 기록하지 않음.

## 구현 내용

- 변경 파일: `src/storage/RuntimeStore.{h,cpp}`, `src/power/ChargeProtection.{h,cpp}`, `src/drivers/Pmic.cpp`, `tools/{soak_report,uart_logger}.py`, C++/Python 회귀 테스트, `docs/17*`, `docs/18*`, ADR 0029.
- 변경 목적: 저장 실패 후 5분 대기, 실패한 I/O도 하루 48회 예산에 포함. 성공 여부와 별개로 RAM 예산을 유지하고 다음 성공 저장에 반영. PMIC 정책에서 확인한 cfg/timer/status만 로그에 사용하며 최종 상태 읽기 실패는 Fault. strict 관측에서 기기 오류와 수집 끊김·폐기·중단도 검출.
- 기존 기능 영향: 저장 포맷/설정 스키마 변경 없음. 실패 재시도는 늦춰지며 정상 저장 빈도는 기존 정책 유지. 분석기는 기존 단일 로그도 지원하되 sidecar 검사 여부를 명시.
- 계획·대안·영향 범위: [ADR 0029](../../docs/adr/0029-storage-failure-and-observation-integrity.md).

## 재현 가능한 증거

원시 파일은 로컬에만 있으며 Git에는 이 보고서를 보관한다.

| 구분 | 명령/방법 | 결과 | 검증 단계 | 증거 경로 |
|---|---|---|---|---|
| Unit tests C++ | `python tools/run_native_tests.py` | PASS: 144, 실패 0 | Host | `native-tests.log` |
| Unit tests Python | `python -m pytest host/tests -q` | PASS: 최종 204, 실패 0, 외부 플러그인 경고 13 | Host | `../public-readiness-2026-11-13/pytest.log` |
| Fault injection | cooldown/48회 실패/시계 역행/회복/PMIC 읽기 실패/수집 손실 합성 입력 | PASS, 위 테스트에 포함 | Host | 테스트 소스 및 위 로그 |
| Mutation | `python tools/mutation_check.py` | PASS: 2개 KILLED | Host | `mutation.log` |
| Firmware build | `python -m platformio run -e reterminal_e1001 -e reterminal_e1001_protected_dev` | PASS: 2환경 | Build | `../public-readiness-2026-11-13/build.log` |
| Host preview | UI 변경 없음 | NOT_RUN | Host | 해당 없음 |
| E1001 smoke | 추가 이미지는 flash하지 않음 | NOT_RUN | Hardware | 기존 시험 유지 |
| Live API | 추가 이미지로 재실행하지 않음 | NOT_RUN | LiveAPI | 기존 실기 보고와 구분 |
| Sleep/wake·전류 | 계측기 없음 | NOT_RUN | Hardware | BLOCKED_HW |

기본/개발 RAM 78,452B, Flash 1,974,266B / 1,975,310B. 이는 컴파일 결과이며 실기 정상 동작이나 여유 heap의 증명이 아니다.

## 독립 검증 및 보안

- 단위/포맷/환율 기준일 Oracle: 기존 자정/KST/영업일·JPY(100)·결측 회귀 포함, 이번 변경으로 API 출처·시세 의미 변경 없음.
- Mutation: 결측을 0으로 바꾸는 오류 및 freshness 부호 오류 모두 검출.
- 자격증명 검사: [공개 준비 보고](../public-readiness-2026-11-13/summary.md). 기본 바이너리에 알려진 로컬 키/SSID·개발 marker 없음. TLS 정책 유지.
- 10:45 진행 로그 분석: fetch 13, panic 0, 검출된 기기 fault 0. 24h strict 판정은 관측 시간 부족으로 FAIL이며 장기 시험 PASS가 아니다. UART 프로세스 재시작/포트 재연결 없음.

## 미완료/블로커

- BLOCKED_HW: 전류계/셀 SOC 계측, 추가 이미지 실기 fault 및 24h/72h/7일, 배터리 sleep/wake. 추정 80%와 실제 SOC·차단 전류를 동일시하지 않는다.
- BLOCKED_CREDENTIALS: 없음(로컬 개발 빌드 가능). 새 이미지의 실 API 재검증은 미실행.
- BLOCKED_NETWORK: 없음. 외부 API 장기 가용성은 보장하지 않음.
- 기타: 실패 예산의 RAM 예약은 완전 전원 차단을 넘어 영속적이지 않다. 시계 역행 중 cooldown을 다시 잡으며 계속되는 역행은 재시도를 더 늦출 수 있다.

## 게이트 판정

- 이번 단계: PARTIAL — 구현/Host/Build PASS, 신규 펌웨어 실기 미적용.
- 다음 단계: 기존 24h 수집 종료 후 sidecar 포함 재분석. 결과를 보존하고 새 이미지 flash/smoke 후 별도 연속 시험. 자동 flash 예약은 만들지 않음.
- 실기 없이 성공이라 표시하지 않은 항목: Flash 마모 수명·충전 전류·정확한 80% SOC·무인 장기 안정성·신규 이미지 sleep/wake.
