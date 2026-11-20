# Phase Progress Report — 첫 실기 플래싱 준비/드라이버 진단

## Wi-Fi 정정 후 결과 (최신 상태)

- 사용자가 SSID는 동일하고 비밀번호는 다른 정정값을 제공했다. 로컬 문서/헤더를 갱신 후 재빌드 및 COM5 재업로드 성공(116.27초, hash verified).
- 이후 정상 리셋/50초 UART에서 `config: source=dev`, `wifi: connected`, `time: source=NTP verified=1` 확인. 설정한 지역 격자 유지.
- EXIM 고시 환율과 2027 공휴일 조회가 기기에서 OK. KMA 실황/예보/Air는 HTTP 200이나 PROVIDER_ERROR로 남아 후속 구현 진단에 들어갔다. 최종 해결 여부는 [실 API 통합 보고](../live-api-integration/summary.md)를 따른다.
- 증거: wifi-corrected-upload-redacted.log, wifi-corrected-boot-redacted.log. 아래 Wi-Fi 실패는 비밀번호 정정 이전 이력이며 현재 장애로 읽지 않는다.
- 첫 업로드 후 사용자가 대시보드 표시를 확인했다. 부분 갱신/절전/전류/복구는 미검증.

## 2026-11-11 드라이버 설치 후 실제 업로드 — 최신 결과

- 날짜·시간(KST): 2026-11-11.
- 작업 단계/티켓: 승인된 E1001 첫 USB 업로드 및 부팅 smoke. 아래 최초 Code 28 진단은 해결 전 이력이다.
- 커밋 SHA / 공식 기반 commit: main c37da88 + 로컬 개발 설정 / Seeed 8d97d8912987c07529287629f7e73039bb0e6384.
- 작업 환경: Windows, PlatformIO Core 6.2.0, espressif32@8.0.0, esptool 5.4.0. 사용자가 드라이버 설치, 현재 USB-SERIAL CH340(COM5) Status OK.
- 기기 연결/모델: 사용자 E1001 확인, 실제 COM5 연결 및 ESP32-S3 rev v0.2/embedded PSRAM 8MB/Flash 32MB 읽기 확인(manufacturer ef/device 4019).
- API 실키 접근: 로컬 설정에 등록된 키 사용; 값/토큰 출력 없음.

### 구현 내용

- 변경 파일: 이 보고서, upload-redacted.log, boot-redacted.log, validation.txt, hardware/board_e1001.md, docs/10_SOURCES_AND_OPEN_QUESTIONS.md.
- 변경 목적: 실제 플래싱·해시 검증·부팅 확인과 미해결 네트워크 상태 기록.
- 기존 기능 영향: 기본 PlatformIO recipe로 bootloader/partitions/boot_app0/firmware를 기록했다. 전체 Flash erase, NVS 초기화, eFuse 변경, 파티션 용량 확장 없음. 업로드 후 정상 재부팅 및 로그 확인용 RTS 정상 리셋 1회 수행(설정 초기화 아님).

### 재현 가능한 증거

| 구분 | 명령/방법 | 결과 | 검증 단계 | 증거 경로 |
|---|---|---|---|---|
| Driver/COM | pio device list; Get-PnpDevice | PASS: USB CH340 COM5 OK | Hardware/OS | 세션 출력 |
| 칩/Flash | esptool.exe --chip esp32s3 --port COM5 --baud 115200 flash-id | PASS: ESP32-S3 rev v0.2, 8MB PSRAM, 32MB Flash | Hardware | 위 요약; MAC 공개 기록 생략 |
| Build+Upload | pio run -e reterminal_e1001 -t upload --upload-port COM5 | PASS: 159.88초; RAM 70,788 B / Flash 1,683,482 B; 모든 기록 이미지 hash verified | Build/Hardware | upload-redacted.log |
| E1001 boot | 115200 UART 50초 수집, 이후 RTS 리셋 후 50초 재수집 | PASS: fw=0.3.0 board=e1001, 정상 SPI boot, config source=dev | Hardware | boot-redacted.log(재수집) |
| 설정 적용 | 부팅 등록 상태 로그 | PASS: Wi-Fi/KMA/Air/EXIM/KIS 등록, grid=58,126; FSC/KIS OFF, watch=0 | Hardware | boot-redacted.log |
| 화면 | 사용자 화면 확인 응답 | PASS: 대시보드 표시됨(사용자 관찰), 이미지 직접 검수 아님 | Hardware/user observation | 대화 응답 및 이 보고서 |
| Wi-Fi / 시각 | 두 UART 관측 | FAIL: wifi FAILED, source=NONE verified=0 | Hardware | boot-redacted.log |
| 공유기 가시성 | netsh wlan show networks mode=bssid, 정확한 SSID만 로컬 비교·이름 비공개 | PASS: 대상 SSID 발견, 2.4GHz/WPA2-Personal/CCMP, channel 4, PC 신호 93% | Host network observation | validation.txt; 기기 RF 성공 증명 아님 |
| Live API | Wi-Fi 실패로 인증 API 수신 성공 증거 없음 | NOT_VERIFIED | LiveAPI | 성공 주장 없음 |
| Clean/Unit/Fault/Preview | 미실행(테스트 0개), 기존 결과 유지 | NOT_RUN | Host/Build | 실행하지 않은 결과 재인용 없음 |
| Sleep/wake/전류/복구 | 미실행 | NOT_RUN | Hardware | 별도 계측 필요 |

### 독립 검증 및 보안

- 단위/포맷/환율 기준일 Oracle: 데이터 미수신, 수치/시각 검증 완료 처리 안 함.
- Mutation: NOT_RUN, 코드 변경 없음.
- 보안: UART/업로드 원본 로그와 펌웨어는 build 아래 Git 제외. 공개 로그는 모든 로컬 키·SSID·비밀번호를 값 미출력 치환하고 MAC을 제거했다. 추적/스테이징 파일 exact-value 재검사 결과는 validation.txt. 로컬 비밀 문서는 commit하지 않는다.
- 출처/기준일: 기존 공식 LVGL 원문 board/qio_opi/PSRAM/해상도 대조 유지. flash-id는 실제 연결의 새 증거이며 기존 미확인 용량 항목만 보완한다.

### 미완료/블로커

- BLOCKED_HW/DRIVER: **해소**. 업로드 및 부팅/사용자 화면 확인 완료. 부분 갱신/절전/wake/전류/USB 복구는 여전히 미검증.
- BLOCKED_NETWORK: 기기 Wi-Fi 연결 실패, 시각 미검증. PC에 동일 SSID가 2.4GHz/WPA2로 검색되지만 비밀번호·AP 접속 제한·기기 RF/연결 실패 원인은 확정하지 못했다. 비밀번호/공유기 설정 확인 또는 포털 수정이 다음 단계. API/TLS 문제로 단정하지 않는다.
- BLOCKED_CREDENTIALS: 키 등록 확인만 완료; 실 API 유효성 미검증. KIS OFF이므로 주가 조회 비활성 유지.
- 기타: PlatformIO upload 준비 중 패키지 미러 DNS 오류 후 대체 경로로 설치 완료했으며 업로드 최종 exit 0. 로그에 남은 초기 오류를 최종 업로드 실패로 해석하지 않는다.

### 게이트 판정

- 첫 USB 플래싱·부팅·화면 smoke: **PASS**. 온라인 대시보드 전체 동작: **PARTIAL**(Wi-Fi 실패).
- 다음 단계: Wi-Fi 인증/접속 조건 확인 → NTP/RTC 신뢰 시각 → 공급자별 실제 결과코드/데이터 기준시각 확인. 파티션 확대/재초기화 없이 진단.
- 성공 표시하지 않은 항목: 실제 API/TLS·NTP·배터리 수명·부분 갱신·sleep/wake·전원차단 복구.

---

## 최초 연결 이력 — 드라이버 설치 전

- 날짜·시간(KST): 2026-11-11.
- 작업 단계/티켓: 사용자가 기기 ON/USB 연결 후 빌드·실제 플래싱을 요청함.
- 커밋 SHA / 공식 기반 commit: main e9a8561 기준 + KIS 로컬 설정 / Seeed 8d97d8912987c07529287629f7e73039bb0e6384.
- 작업 환경(OS/PlatformIO/보드 패키지 버전): Windows PowerShell / Core 6.2.0 / espressif32@8.0.0. 관리자 권한 없는 Medium 토큰.
- 기기 연결 여부 및 모델 확인: 사용자 E1001 전원/연결 진술. USB VID_1A86/PID_7523 장치는 OS에서 감지되나 driver Code 28, COM 없음. 칩/모델은 아직 펌웨어 도구로 확인하지 못함.
- API 실키 접근: 로컬 개발 헤더에 입력된 기존 키 및 KIS 키 사용. 실값 출력 없음.

## 구현 내용

- 변경 파일: 이 보고서. KIS 설정·빌드는 ../kis-credential-setup/summary.md에 기록.
- 변경 목적: 실제 업로드 대상 포트 식별 및 드라이버 장애 확인.
- 기존 기능 영향: 기기 쓰기/초기화/전체 erase/eFuse 변경 없음. Bluetooth 포트에 업로드하지 않음.

## 재현 가능한 증거

| 구분 | 명령/방법 | 결과 (PASS/FAIL/NOT_RUN) | 검증 단계 (Host/Build/Hardware/LiveAPI) | 증거 경로 |
|---|---|---|---|---|
| Firmware build | pio run -e reterminal_e1001 | PASS: RAM 70,788 B / Flash 1,683,482 B | Build | ../kis-credential-setup/validation.txt |
| 포트 식별 | pio device list; Get-CimInstance Win32_SerialPort | FAIL: COM3/4는 Bluetooth, E1001 USB COM 없음 | Hardware/OS | 본 세션 출력 및 이 보고서 |
| USB 진단 | Get-PnpDevice -PresentOnly; Get-PnpDeviceProperty ProblemCode/DriverInfPath | FAIL: USB Serial Error, Code 28, INF 없음 | Hardware/OS | 본 세션 출력 및 이 보고서 |
| 권한 확인 | whoami /groups | Medium integrity, Administrators deny-only | Host | 본 세션 출력 |
| 드라이버 안내 대조 | Seeed E1001 공식 가이드의 WCH Windows 링크 | PASS: 아래 출처 | Host source | 아래 링크 |
| Upload / E1001 smoke | 대상 COM 미생성으로 업로드 명령 실행하지 않음 | NOT_RUN | Hardware | BLOCKED_HW/DRIVER |
| Clean build / Unit tests | 미실행, 테스트 0개 | NOT_RUN | Build/Host | 기존 빌드/회귀 결과와 구분 |
| Fault injection / Host preview | 미실행 | NOT_RUN | Host | 설정/연결 작업 |
| Live API / Sleep/wake | 미실행 | NOT_RUN | LiveAPI/Hardware | 미검증 |

## 독립 검증 및 보안

- 단위/포맷/환율 기준일 Oracle: 코드 변경 없음.
- Mutation 테스트 결과: NOT_RUN.
- 자격증명 유출 검사 범위/결과: KIS 보고서 validation 및 최종 스테이징의 로컬 실값 검사. 비밀 파일·바이너리·원본 빌드 로그는 Git 제외.
- 사용한 API 출처와 기준일 검증: 인증 API 호출 없음. USB 드라이버 출처는 2026-11-11 [Seeed E1001 시작 안내](https://wiki.seeedstudio.com/getting_started_with_reterminal_e1001/) → [WCH CH341SER Windows 공식 다운로드](https://www.wch-ic.com/downloads/CH341SER_ZIP.html). Mac 링크 대신 Windows 링크 사용.
- 공급자 키를 빌드 산출물과 함께 공개하지 않음. 대상 확인 전 임의 COM/수동 offset/타 모델 펌웨어 사용 없음.

## 미완료/블로커

- BLOCKED_HW/DRIVER: USB 장치 드라이버 미설치(Code 28), COM 미생성. 사용자에게 공식 CH341SER 설치 및 USB 재연결 요청.
- BLOCKED_CREDENTIALS: 키 보유; 실제 인증 유효성 미검증. 이번 장애 원인은 키가 아님.
- BLOCKED_NETWORK: 공식 공개 가이드 접근 성공, 실 API 미실행.
- 기타 이슈: 드라이버 설치에는 관리자 권한이 필요하고 현재 세션은 비승격 상태. 자동 승인 검토에 의한 거절이 아니라 Windows 권한/드라이버 상태로 중단.

## 게이트 판정

- 이번 단계 목표 PASS/FAIL/PARTIAL: PARTIAL — 펌웨어 빌드 PASS, 실기 플래싱 미실행.
- 다음 단계에서 해야 할 일: 드라이버 설치/재연결 완료 후 COM과 USB VID/PID 확인 → PlatformIO upload → 비밀 마스킹한 부팅 로그/설정 source/패널 확인.
- 실기 없이 성공이라고 표시하지 않은 항목: USB flash, 정상 부팅, 실제 칩/Flash 용량, Wi-Fi/API/TLS, 패널, 절전.
