# Phase Progress Report — 휴장 문구 수정 펌웨어 실기 업로드

- 날짜·시간(KST): 2026-11-17
- 작업 단계/티켓: MARKET-LABEL-01 실기 업로드 후속 완료
- 커밋 SHA / 공식 기반 commit: 펌웨어 소스 `9012d72` / Seeed `8d97d8912987c07529287629f7e73039bb0e6384`
- 작업 환경(OS/PlatformIO/보드 패키지 버전): Windows / PlatformIO / espressif32@8.0.0, 기존 `reterminal_e1001_protected_dev` 운영 환경 유지
- 기기 연결 여부 및 모델 확인: 사용자 연결 E1001, COM5 CH340. 업로더에서 ESP32-S3 rev0.2 / PSRAM8MB 확인. 부팅 `board=e1001` 확인.
- API 실키 접근(키 자체 절대 쓰지 말 것): 기존 로컬 개발 설정 사용. 값 출력·공개·초기화 없음. 개발 바이너리는 비공개 빌드 산출물로 유지.

## 구현 내용
- 변경 파일: 본 보고서 및 기존 `summary.md`의 후속 보고 링크. 펌웨어 소스 추가 변경 없음.
- 변경 목적: 사용자의 명시적 요청에 따라 휴장 문구 수정 펌웨어를 실제 연결 기기에 업로드.
- 기존 기능 영향: 기존 충전 보호 개발 환경으로 업로드, 설정 초기화·전체 플래시 삭제 없음. NVS used285/free345 및 Wi-Fi 재연결 확인.

## 재현 가능한 증거
| 구분 | 명령/방법 | 결과 (PASS/FAIL/NOT_RUN) | 검증 단계 (Host/Build/Hardware/LiveAPI) | 증거 경로 |
|---|---|---|---|---|
| Clean build | `pio run -e reterminal_e1001_protected_dev -t upload --upload-port COM5` | PASS 증분 운영 빌드·업로드. RAM78,468B / Flash1,978,574B. Clean은 NOT_RUN | Build/Hardware | `device-upload.log` |
| Unit tests | 소스 변경 없음, 직전 C++155개·Python209개 결과 재사용 | PASS 실패0, 이번 턴 재실행하지 않음 | Host | `native.log`, `python.log` |
| Fault injection | 이번에는 장애 주입 빌드 사용 안 함 | NOT_RUN | Hardware | — |
| Host preview | 직전 21종 프리뷰 재사용 | PASS, 소스 변경 없음 | Host | `preview.log`, `previews/` |
| E1001 smoke | 업로드 쓰기 해시 검증, `python tools/uart_logger.py COM5 evidence/market-labels-2026-11-17/device-boot.log --duration-sec 100 --reset-once` | PASS 업로드 후 재부팅, Wi-Fi 연결, NTP verified=1, 패널 refresh_done→power_off 확인 | Hardware | `device-upload.log`, `device-boot.log` |
| Live API | 기기 fetch 로그 | EXIM(provider2) OK/HTTP200/1 request, KIS(provider6) OK/HTTP200/6 requests. 모든 공급자 신규 수신 보장은 아님 | LiveAPI | `device-boot.log` |
| Sleep/wake | USB 연결 상태에서 단기 부팅 관찰만 시행 | NOT_RUN | Hardware | — |

## 독립 검증 및 보안
- 단위/포맷/환율 기준일 Oracle: 이번 업로드에서는 추가 가격 Oracle 검증 없음. 직전 회귀 검증 경계 유지.
- Mutation 테스트 결과: 소스 변경 없으므로 미실행.
- 자격증명 유출 검사 범위/결과: UART는 기존 sanitizer를 통해 기록. 업로드·UART 로그 및 본 보고서를 알려진 로컬 비밀/패턴과 대조한 결과는 `device-security.log`에 기록. 개발 바이너리와 개인 설정은 Git에 추가하지 않음.
- 사용한 API 출처와 기준일 검증: API 계약 변경 없음. 새 가격/기준일의 외부 독립 대조는 미실행.
- 운영 firmware.bin SHA256: `3ac6ea2c0a2d112e024fb097cf15920918bafb183a783ad016bdbd489a5f5cf4`

## 미완료/블로커
- BLOCKED_HW: 패널 드라이버 완료는 UART로 확인. 실제 화면 문구 육안 확인·버튼·Deep Sleep·전류 계측은 미실행.
- BLOCKED_CREDENTIALS: 없음(기존 운영 설정 사용).
- BLOCKED_NETWORK: Git remote 미설정으로 push 불가.
- 기타 이슈 (재현/회피책): 특별 개장·휴장일 검증 경계는 ADR0033과 같음.

## 게이트 판정
- 이번 단계 목표 PASS/FAIL/PARTIAL: PASS 업로드 및 부팅·단기 통신·패널 드라이버 smoke. 실제 화면 육안 확인은 별도.
- 다음 단계에서 해야 할 일: 사용자 기기의 시장/달력 화면에서 수정 문구 확인. 원격 설정 후 main push.
- 실기 없이 성공이라고 표시하지 않은 항목: 육안 화면 품질, 버튼 wake, Sleep/wake, 전류 소비, 장기 안정성.
