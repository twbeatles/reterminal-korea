# Phase Progress Report — E1001 장기 운영 실기 개선

- 날짜·시간(KST): 2026-11-13. 각 UART 수집의 정확한 UTC 시작/종료는 `.events.jsonl` 참조.
- 작업 단계/티켓: LT-03 충전 보호 구현/제어 read-back, LT-04 PMIC 버스/Unknown 관측, LT-05 Wi-Fi atomic 이벤트, LT-06 실제 Wi-Fi/loop stall fault, LT-07 reset guard 보존 수정, LT-10 UART 캡처 구현. LT-08/09는 기존 회귀·패널 smoke 범위, 장기/물리 fault 미완료.
- 커밋 SHA / 공식 기반 commit: main `a4b0f2d` 기반 / Seeed `8d97d8912987c07529287629f7e73039bb0e6384`. 최종 커밋은 이 보고 포함 git 이력, 운영 이미지 해시는 `operating-image.json` 참조.
- 작업 환경(OS/PlatformIO/보드 패키지 버전): Windows PowerShell, Python 3.15, PlatformIO `espressif32@8.0.0`, Arduino-ESP32 3.3.12, `seeed_xiao_esp32s3` / qio_opi.
- 기기 연결 여부 및 모델 확인: 사용자 E1001, COM5 CH340 실연결. esptool 확인 ESP32-S3 rev0.2 / PSRAM8MB / Flash32MB. PMIC `REG0B=0x40`, SY6974B 지원 PN/revision 일치. 물리 PCB 실크 리비전은 미확인.
- API 실키 접근: 기존 로컬 개발 설정을 컴파일에 사용(파일 값 열람/출력 없음), 실제 기기 API 통신. 사용자 기본 배터리 사용·계측 장비 없음 확인. USB 플래시·검증은 사용자 명시 승인 범위.

## 구현 내용

- 변경 파일: `src/power/ChargeProtection.*`, `src/drivers/Pmic.*`, `src/main.cpp`, `src/net/WifiConnector.cpp`, `src/power/{RtcPersistence.cpp,RtcState.h}`, `src/app/{Coordinator.h,CalendarRefresh.cpp}`, `include/Config.h`, `src/storage/SettingsCodec.*`, `src/provision/{PortalHandler.cpp,SettingsBackup.cpp}`, `src/ui/{ViewModel.*,Renderer.cpp}`, `platformio.ini`, `tools/{uart_logger.py,render_previews.cpp}`, `test_native/{test_long_term.inc,test_core.cpp,test_lan_portal.inc}`, `host/tests/test_uart_logger.py`, `.gitignore`, `docs/{10,12,17,18,BUILD}` 및 T11/ADR 0028, 이 증거 폴더.
- 변경 목적: 전압 기반 추정 80% 목표 보호(3연속 샘플, 30초 간격; 75% 재개), 칩 ID 제한·필요 비트만 쓰기·read-back, 상태/포털 설정, v2/v3→v4 보존 읽기, 캐시 지문 유지. I2C 1회 초기화·50ms timeout·1초 USB cache·실패 후 재초기화·Unknown 로그. Wi-Fi task 공유 atomic. UART 저장 전 마스킹·회전·재접속·자동 기간 검사.
- 추가 실기 발견/수정: `RTC_DATA_ATTR` 표식이 watchdog 때 초기화되어 초기 fault 이미지의 Wi-Fi 시험이 다시 실행됐다. 기존 reset guard/RTC cache/인증 해제 표식도 같은 속성이었다. NOINIT+magic/power-on 초기화+기존 CRC 검증으로 변경했다. 달력 재시도도 magic 및 비정상 장기 deadline 복구를 추가했다. Host용 RTC codec 검증을 실제 reset 보존 검증으로 대체하지 않았다.
- 기존 기능 영향: 일반 배포 충전 보호 기본 OFF, 이번 로컬 개발 초기값만 ON(NVS 우선). 보호 모드에서 I2C watchdog bits만 해제; OFF로 바꿔도 watchdog 해제는 유지한다. 안전 타이머·NTC·충전 전류/전압·파티션·eFuse 변경 없음. v4 저장 후 구버전 펌웨어로 돌아가면 설정 재입력이 필요할 수 있음.

## 재현 가능한 증거

| 구분 | 명령/방법 | 결과 (PASS/FAIL/NOT_RUN) | 검증 단계 (Host/Build/Hardware/LiveAPI) | 증거 경로 |
|---|---|---|---|---|
| Clean build | 기존 출력 정리 없이 `pio run -e reterminal_e1001 -e reterminal_e1001_protected_dev`, 최종 기본 환경 재빌드 | PASS(증분). clean 미실행. 최종 protected RAM 78,436 B / Flash 1,974,730 B | Build | `final-build.log`, `release-build.log`, `operating-upload.log` |
| Unit tests | `python tools/run_native_tests.py` | PASS 139개, 실패 0 | Host | `native-tests.log` |
| Unit tests | `python -m pytest host/tests -q` | PASS 180개, 실패 0. 외부 패키지 deprecation 경고 13개 | Host | `pytest.log` |
| Fault injection (Mock) | 충전 칩 ID/OTG/전압 오류/읽기·쓰기·read-back 실패/카운터·히스테리시스/설정 복귀/WD_RST 보존, v2/v3 읽기·v4 bool 검증·지문 유지·백업 | PASS, C++ 신규 8개에 포함 | Host | `test_long_term.inc`, `native-tests.log` |
| Fault injection (Mock) | UART 비밀 마스킹·포트 재접속·회전·DTR/RTS·긴 줄 꼬리 폐기·미완성 자동 report FAIL | PASS, Python 신규 3개에 포함 | Host | `test_uart_logger.py`, `pytest.log` |
| Host preview | `python tools/render_previews.py --output-dir evidence/long-term-device-2026-11-13/previews` | PASS, 19종. 충전+포털 동시 표시 PNG 육안 확인. 전부 합성 DEMO | Host | `preview.log`, `previews/device-charge-portal.png` |
| E1001 smoke / 초기 진단 | `pio run -e reterminal_e1001_dev -t upload --upload-port COM5` 후 UART 90초 | PASS, ID=40/cfg=1a/vreg=58/timer=8f, Wi-Fi·패널·health | Hardware | `diagnostic-upload.log`, `diagnostic-boot.log` |
| 충전 제어 | 보호 개발 이미지, 추정 100%에서 3회 확인 | PASS(레지스터/상태만): cfg 1a→0a, status bc→a4(충전 상태 bits=00). 실제 전류/80% SOC 정확도는 NOT_RUN | Hardware | `fault-validation.log` |
| Wi-Fi loss | validation 이미지에서 45초 뒤 `WiFi.disconnect(false,false)` | PASS, 초기 11.075초 / 수정 재시험 11.596초 내 복구. 공유기 100회 차단 아님 | Hardware | `fault-validation.log`, `fault-validation-fixed.log` |
| Watchdog / reset guard | 100초 뒤 Feed 중단 → 실제 task watchdog, 수정 후 재시험 | PASS: reset=6, boots=2/crashes=1 보존, 충전 cfg=0a 유지, Wi-Fi/화면 로그 복구. 재부팅 뒤 uptime 128,313ms까지 관측, Wi-Fi 주입/정지 주입 각각 1회로 반복 없음 | Hardware | `fault-validation-fixed.log`, `fault-proof.json` |
| 최종 운영 이미지 | `pio run -e reterminal_e1001_protected_dev -t upload --upload-port COM5` | PASS, 쓰기 해시 검증·부팅·cfg=0a·시각/화면/health 로그. binary 내 fault 주입 문자열 없음 확인. 전체 erase 없음 | Hardware | `operating-upload.log`, `operating-image.json`, 실행 중 캡처 |
| Live API | 실제 기기 부팅 시 내부 코드·자료 시각/값 로그 | 관측: KMA 실황·AirKorea·KIS(토큰+4종목+지수) OK. 기존 캐시도 복원. 이 세션의 모든 공급자 재검증은 아님 | LiveAPI | fault/운영 부팅 로그 |
| Sleep/wake·USB 탈착·전류 | 물리 조작/계측기 없음 | NOT_RUN | Hardware | — |
| Lint·회귀 | ruff, diff check, mutation gate | PASS, 기존 mutation 2종 KILLED. 신규 충전 mutation은 미실행 | Host | `lint.log`, `mutation.log` |
| Stack audit | `python tools/check_stack_usage.py --json evidence/long-term-device-2026-11-13/stack.json` | PASS, 알려진 정적 경로 실패 0. SDK 간접 호출·실기 stack high-water는 미검증 | Build | `stack.json` |
| USB 24h 수집 | `uart_logger.py COM5 .../active/usb-24h.log --duration-sec 86700 --soak-min-duration-sec 86400 --reset-once --stop-file .../active/stop.capture` (숨김 백그라운드 프로세스) | RUNNING, PASS 아님. 2026-11-13 10:04:32 KST 시작, 11-14 10:09:32 KST 종료 예정. 종료 시 report JSON 생성 | Hardware | `capture-session.json`, 로컬 ignored `active/` |
| 운영 이미지 단기 연속 관측 | 실행 중 로그의 고정 사본에 `soak_report.py .../operating-smoke.log --min-duration-sec 120 --max-gap-sec 120` | PASS(120.007초 관측만): health 3개, 공백 최대60.004초, boot1, panic0, heap 233,664→233,664 B, PSRAM 8,206,848→8,206,912 B. 24h 합격/누수 부재 보장은 아님 | Hardware | `operating-smoke.log`, `operating-smoke-report.json` |

## 독립 검증 및 보안

- 단위/포맷/환율 기준일 Oracle: 기존 40개 공통 골든 및 KST 자정·공휴일·영업일/JPY(100)/측정소 결측 회귀 포함. 제공자 의미 변경 없음. UI의 잔량은 기존 공식 전압 곡선의 추정이며 계측 Oracle이 아니다.
- Mutation 테스트 결과: 기존 2종 KILLED. 첫 C++ 실행의 v2 합성 fixture 절단 위치가 v4 tail과 달라 실패한 1건은 fixture를 올바른 v2 길이로 수정하고 최종 139개 통과했다. 첫 Python 보안 검사의 합성 Bearer 문자열 지적은 테스트 입력을 런타임 조립하도록 수정했고 보안 검사 예외를 추가하지 않았다.
- 자격증명 유출 검사 범위/결과: 신규 완료된 log/JSON/JSONL에 `redact_text` 적용 후 `find_secrets` 0건, 홈 경로 마스킹·UTF-8 정규화. 기존 보안 회귀 포함 pytest 180개 통과. 운영/검증 개발 바이너리는 `.pio` 아래 ignored, 배포 패키지로 내보내지 않음. 캡처는 마스킹한 텍스트만 저장하며 실행 중 파일은 ignored `active/` 아래 보존한다.
- 사용한 API 출처와 기준일 검증: Seeed v1.2 회로도·고정 공식 예제, Silergy SY6974B Rev.0.9 및 설치된 Espressif `esp_attr.h` noinit 정의를 2026-11-13 대조. 외부 API 약관·모든 공급자 SLA 재검증은 아님.

## 미완료/블로커

- BLOCKED_HW: 충전 전류·ADC 오차/SOC 교정·실제 75% 재개, USB 20회/재부팅 20회, 배터리 72h·sleep/wake100회·저전압 복귀·공유기100회·BUSY/SD 물리 fault·Flash 전원 차단10회, 실제 패널 내용 육안 확인. USB UART/패널 완료 신호만으로 이 항목을 완료 처리하지 않는다.
- BLOCKED_CREDENTIALS: 없음(기존 로컬 설정). 추가 키를 채팅/커밋으로 요구하지 않음.
- BLOCKED_NETWORK: git remote 없음, main push 불가. 로컬 commit으로 남긴다.
- 기타 이슈: 정확한 PCB 리비전은 USB로 확인할 수 없음. 구현은 확인된 PMIC ID에 한정. 기기 꺼짐·수면·PMIC POR 후 다음 실행 전에는 80% 제한을 보장하지 않음. 배터리가 이미 만충이어서 80%→75% 실제 사이클은 아직 관측하지 못했다.

## 게이트 판정

- 이번 단계 목표: PARTIAL — 코드·Host/Build·단기 실기 충전 제어/Wi-Fi/watchdog 검증. 장기 운영·SOC/전류·배터리 반복 전체 합격은 아님.
- 다음 단계: 현재 실행 중인 USB 24h 수집 종료 후 `active/usb-24h.log.report.json` 확인 → 안정화 2시간 이후 heap/largest·PSRAM 검토 → USB72h/7일 → 계측/수동 배터리·절전·탈착 시험. 수집 PC/USB가 중단되면 관측 공백이나 기간 부족을 FAIL로 남긴다. 캡처 중지 방법은 운용 절차 참조.
- 실기 없이 성공이라고 표시하지 않은 항목: 실제 충전 전류 차단/정확한80%, 전류/수명, 배터리 반복/장기/저전압, 보드 리비전, 물리 fault. 컴파일 성공을 실기 전체 정상으로 표시하지 않았다.
