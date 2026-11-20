# Phase Progress Report — 추가 안정성 펌웨어 실기 적용

- 날짜·시간(KST): 2026-11-13. 사용자 추가 flash 요청에 따라 진행.
- 작업 단계/티켓: LT-11~13 후속 이미지 실기 적용. 실제 24h 검증은 사용자 요청으로 보류.
- 커밋 SHA / 공식 기반 commit: 펌웨어 소스 `3c57e45`(문서 후속 `8c5ff06`), 공식 예제 `8d97d891`. 이 보고서 포함 커밋은 `git log -1 --format=%H -- evidence/followup-flash-2026-11-13/summary.md`.
- 작업 환경: Windows, PlatformIO/espressif32@8.0.0, Arduino-ESP32 3.3.12, E1001/qio_opi. 공식 보드/핀/기본 파티션 변경 없음.
- 기기 연결 여부 및 모델 확인: 기존 E1001 COM5, SY6974B ID `40`, 기본 배터리. 로컬 외부 개발 설정을 사용하는 `reterminal_e1001_protected_dev` 적용. 장애 주입 이미지 아님.
- API 실키 접근: 기존 외부 개인 헤더 사용. 키·비밀번호를 로그나 보고서에 쓰지 않음.

## 구현 내용

- 변경 파일: 이 보고서 및 이전 후속 보고서의 연결 안내. 펌웨어 코드 추가 변경 없음.
- 변경 목적: 이미 Host/Build 검증한 저장 실패 재시도·예산, PMIC 확정 진단, 관측 신뢰성 개선 이미지를 기기에 적용.
- 기존 기능 영향: 이전 24h 수집을 stop-file로 정상 종료 후 flash. 전체 Flash 삭제·NVS 초기화 명령 없음. 부팅 로그의 실제 설정 출처는 `dev`; runtime cache 복원 확인. 초기 NVS가 저장되어 있었다고 주장하지 않음.
- 적용 이미지 SHA256: `5ee9b36f6f3ef309caa6dcad1dbce9d49464106114258eb8c63efe1e28479f79`. 개인 개발 이미지이므로 공유하지 않음.

## 재현 가능한 증거

원시 로그·JSON은 로컬 전용이다. 아래 검사는 장기 수명 시험을 대체하지 않는다.

| 구분 | 명령/방법 | 결과 | 검증 단계 | 증거 경로 |
|---|---|---|---|---|
| Build/upload | `python -m platformio run -e reterminal_e1001_protected_dev -t upload --upload-port COM5` | PASS: 업로드, 쓰기 해시 검증 | Build/Hardware | `upload.log` |
| Unit tests | 이번 turn 코드 변경 없음. 기존 204 Python / 144 C++ / 변이 2개 검출 결과 재사용 | PASS(이전 실행), 재실행 안 함 | Host | [기존 보고](../long-term-followup-2026-11-13/summary.md) |
| Fault injection | 운영 이미지에서 인위적 stall/단선 없음 | NOT_RUN | Hardware | 기존 장애 시험과 구분 |
| Host preview | UI 변경 없음 | NOT_RUN | Host | 해당 없음 |
| E1001 smoke | `python tools/uart_logger.py COM5 evidence/followup-flash-2026-11-13/smoke.log --duration-sec 150 --reset-once --soak-min-duration-sec 60` | PASS: 150초 수집, health span 120.004초, 3샘플 | Hardware | `smoke.log`, sidecar 및 report JSON |
| Live API | provider 1/6 `outcome=OK http=200`, 요청 1/6 | PASS: 기상청 실황·KIS 호출 처리 | LiveAPI | `smoke.log` |
| Display | page 0 `ok=1`, 갱신 4,738ms | PASS: 드라이버 완료 로그 | Hardware | `smoke.log` |
| PMIC | enabled=1, state=4, readback=1, cfg=0a, timer=8f | PASS: 충전 금지 설정 읽기 확인 | Hardware | `smoke.log` |
| Sleep/wake·전류·24h | 이번 단기 smoke 범위 밖 | NOT_RUN | Hardware | BLOCKED_HW/장기 시험 보류 |

## 독립 검증 및 보안

- 단위/포맷/환율 Oracle: 기존 자동 회귀 결과 재사용. 이번 로그의 캐시 복원을 새로운 공급자 API 호출 성공으로 계산하지 않음.
- Mutation: 기존 2개 KILLED, 이번 재실행 안 함.
- 자격증명 검사: 완료 로그를 패턴 및 외부 개인 헤더의 알려진 실제 키/SSID와 대조해 검출 0(`security.json`). 원시 로그/개발 바이너리는 ignored 유지. push 없음.
- 사용한 API 출처·기준시각: 기상청 실황 11-13 10:00 KST, 예보/대기/환율/종가는 복원된 snapshot일 수 있음. KIS REST는 이번 성공 결과 확인. 계약·시장 시간 의미의 전면 재검증은 아님.
- 이전 연속 관측: 10:04:32 시작 → 10:49:22 사용자 요청 종료. health span 2,654.424초(약 44분 14초), 45샘플, 최대 간격 67.359초, panic/device fault 0, 부팅 1회, capture 끊김 0. fetch OK 11/외부 RATE_LIMITED 2. strict 24h 판정은 시간 부족으로 FAIL. `previous-capture-report.json`에 sidecar 포함 재분석 보존.

## 미완료/블로커

- BLOCKED_HW: 실제 차단 전류·80% SOC 교정·sleep/wake·72h/7일. 장시간 시험은 사용자 사정으로 보류.
- BLOCKED_CREDENTIALS: 없음. 실제 키는 저장소 밖 유지.
- BLOCKED_NETWORK: GitHub 계정 정지, push 안 함.
- 기타: 배터리 약 4.16V, 전압 추정치는 100%. 충전 중단은 배터리 강제 방전 기능이 아니므로 즉시 80%로 내려가지 않는다. PMIC 레지스터 확인을 전류계 측정과 동일시하지 않음. 화면 물리 사진/육안 검수는 이번에 하지 않음.

## 게이트 판정

- 이번 목표: upload 및 단기 smoke PASS. 재부팅 추가 없음(최초 의도적 reset 이후 boot 1회), panic/device fault 0, 수집 연결/종료 정상, 손실 0. health 간격 최대 60.004초, 내부 heap 233,488B 유지, PSRAM 8,206,848→8,206,880B. 장기 안정성 PASS로 격상하지 않음.
- 다음 단계: 일반 사용 중 화면/API/충전 상태 관찰. 여건이 될 때 별도 새 로그로 장기 시험 수행. 자동 24h 캡처/예약 없음.
- 실기 없이 성공이라고 표시하지 않은 항목: 패널 육안 품질·Flash 수명·정확한 SOC·차단 전류·장기 무인 운영.
