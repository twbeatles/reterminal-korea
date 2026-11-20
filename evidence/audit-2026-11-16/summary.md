# Phase Progress Report — PROJECT_AUDIT 후속 수정·E1001 플래싱

- 날짜·시간(KST): 2026-11-16. UART 세션의 정확한 UTC 시작/종료는 각 `.events.jsonl` 참조.
- 작업 단계/티켓: ISSUE-015~018 코드 수정, G-1~3/G-5/G-6 개선, G-4 핵심 공유 경로 회귀·펌웨어 CI 추가. 전체 SDK/물리 검증 완료는 아님.
- 커밋 SHA / 공식 기반 commit: main `aaca78f` 기반. 최종 커밋은 이 보고를 포함한 Git 이력 / Seeed `8d97d8912987c07529287629f7e73039bb0e6384`.
- 작업 환경(OS/PlatformIO/보드 패키지 버전): Windows PowerShell, Python 3.15, PlatformIO espressif32@8.0.0, Arduino-ESP32 3.3.12, seeed_xiao_esp32s3/qio_opi.
- 기기 연결 여부 및 모델 확인: 사용자 E1001 실연결, COM5 CH340. esptool ESP32-S3 rev0.2 / embedded PSRAM8MB / Flash32MB, PMIC ID=0x40 재확인. 제품명은 사용자 확인이며 PCB 실크 리비전은 확인하지 못함.
- API 실키 접근: 기존 저장소 밖 개인 개발 설정을 컴파일에 사용. 값 출력·커밋 없음. 기본 배포 환경에는 포함하지 않음.

## 구현 내용

- 변경 파일: `src/drivers/{ClockBootstrap.*,EspClockSync.cpp,Pmic.*}`, `src/power/{ChargeProtection.*,SleepController.cpp,RtcPersistence.*,RtcState.*}`, `src/app/{CycleRunner.cpp,Fetcher.cpp,CalendarRefresh.cpp,Coordinator.cpp}`, `src/ui/FramePresenter.*`, `src/storage/NvsStorage.cpp`, `src/provision/PortalHandler.cpp`, `test_native/{test_audit_runtime.inc,test_audit_20261116.inc,test_core.cpp}`, `.github/workflows/host-tests.yml`, `README.md`, `PROJECT_AUDIT.md`, `docs/{BUILD.md,10_SOURCES_AND_OPEN_QUESTIONS.md,adr/0028-*,adr/0031-*}`, 이 증거 폴더.
- 변경 목적: 실행 중 시계의 소수 초 손실 제거, 수면 전 충전 허용 복구, 설정 저장 후 시작 화면 적용, USB 시계 복구 블로킹 제거, 시간 예산 초과 deferred 처리(미전송 토큰 재발급 cooldown도 소비하지 않음), RTC 내용 해시 보존, 다음 해 공휴일 NoData 하루 제한, 실제 NVS 용량 근거, 포털·README 안내 수정.
- 기존 기능 영향: 캐시·공급자 상태 보존, 수동 새로고침·배터리 연결 유지. RTC 스키마 v2로 변경하여 구 v1은 NVS fallback. 일반 수면 최대15분은 유지. 새 의존성·핀·NVS 파티션·eFuse 변경 없음. 기기 설정 초기화·전체 flash 삭제 없음.
- 감사 문서: 사용자가 작성한 수정 전 감사 본문을 보존하고 상단에 조치 매핑을 추가했다. 설계 변경 이유·대안·복구는 ADR 0031에 기록했다.

## 재현 가능한 증거

| 구분 | 명령/방법 | 결과 (PASS/FAIL/NOT_RUN) | 검증 단계 (Host/Build/Hardware/LiveAPI) | 증거 경로 |
|---|---|---|---|---|
| Clean build | `pio run -e reterminal_e1001 -t clean`; `pio run -e reterminal_e1001` | PASS 최종 소스 clean 재빌드, RAM 78,468B / Flash 1,976,954B | Build | `clean-final.log`, `build-clean-final.log` |
| Operating build | `pio run -e reterminal_e1001_protected_dev` | PASS, Flash 1,978,094B. 최종 이미지 메타데이터는 `operating-image.json` | Build | `operating-build-final.log`, `operating-image.json` |
| Unit tests | `python tools/run_native_tests.py` | PASS 153 tests, 0 failures, 0 ignored | Host | `native.log` |
| Unit tests | `python -m pytest host/tests -q` | PASS 204 tests, 0 failures, 외부 의존성 deprecation 경고 13개 | Host | `pytest-final.log` |
| Fault injection | fake clock 120회·충전 I2C/read-back 실패/OTG/USB·CRC 파손·중간 요청 예산 소진·10회 Wi-Fi 연결 불가 | PASS, 실제 생산용 bootstrap/충전/복원/사이클 함수 회귀. SDK 장애 주입 아님 | Host | `native.log`, `test_native/test_audit_20261116.inc` |
| Mutation gate | `python tools/mutation_check.py` | PASS 기존 2종 KILLED. 신규 변경 전체 mutation coverage는 아님 | Host | `mutation.log` |
| Host preview | `python tools/render_previews.py --output-dir evidence/audit-2026-11-16/previews` | PASS 19종, device-charge-portal 육안 확인. 합성 화면 DEMO | Host | `preview.log`, `previews/` |
| E1001 identification | PlatformIO 패키지 `esptool.exe --chip esp32s3 --port COM5 --baud 115200 flash-id` | PASS ESP32-S3 rev0.2/PSRAM8MB/Flash32MB | Hardware | 이 보고의 식별 기록, upload 로그 |
| E1001 smoke / intermediate | 기본 환경 업로드→설정 없음 확인→기존 protected_dev 환경으로 복귀 | PASS, 기본 firmware는 개인 설정을 포함하지 않아 AP 설정 화면. NVS source=default/dev 대조 | Hardware | `upload.log`, `release-boot.log`, `operating-upload.log`, `operating-boot.log` |
| E1001 final upload | `pio run -e reterminal_e1001_protected_dev -t upload --upload-port COM5`; UART 160초 | PASS 쓰기 해시 검증·부팅. 반복 관측은 아래 최종 실기 관측 참조 | Hardware | `final-upload.log`, `final-boot.log`, `operating-image.json` |
| Live API | 최종 기기 로그의 내부 outcome/시각 검증 | 관측: KMA 실황·KIS(토큰+4종목+지수) OK. 기존 환율/종가 캐시 복원. 모든 공급자 새 응답 성공을 뜻하지 않음 | LiveAPI | `final-boot.log`, `final-proof.json` |
| NVS capacity | 최대 길이 코덱 + 실제 부팅 nvs_get_stats | PASS: settings1569B/snapshot1133B/status270B/runtime1431B, 계산 peak326/504 entries. 실제 사용285/총630·여유345 | Host/Hardware | `native.log`, `release-boot.log`, `final-boot.log` |
| Stack audit | `python tools/check_stack_usage.py --json evidence/audit-2026-11-16/stack.json` | PASS 알려진 정적 경로 실패0. SDK 간접 호출/high-water 측정 아님 | Build | `stack.json`, `stack.log` |
| Sleep/wake·충전 전류 | USB 물리 탈착·전류 계측·15분 wake | NOT_RUN | Hardware | — |
| Linux/macOS CI | workflow에 타깃 빌드 추가, 원격 실행 | NOT_RUN, 이 세션은 Windows 실행만 | Build | `.github/workflows/host-tests.yml` |

## 독립 검증 및 보안

- 단위/포맷/환율 기준일 Oracle: 기존 공통 골든과 KST 자정·공휴일·영업일 offset·JPY(100)·측정소 `-` 결측 회귀 포함. 시세의 시간속성·DEMO 구분 유지.
- Mutation 테스트 결과: 기존 2종 KILLED. 초기 C++ 2개 실패는 변경된 SNTP 적용 계약의 mock 기대값과 pageNo/numOfRows가 빠진 합성 공휴일 응답을 바로잡아 해결했다. 최종 신규 9개 포함 153개 통과.
- 자격증명 유출 검사 범위/결과: 공개 소스·증거 로그·Git index에 저장소 밖 known private 값 및 패턴 검사. 상세 결과 `security-final.log`와 `evidence-scan.json`. UART는 기존 sanitizer를 통해 저장. 개발 바이너리·개인 헤더는 공개 증거에 포함하지 않음.
- 저장소 정책: `.gitignore`에 따라 증거 Markdown 보고만 커밋한다. 로그·JSON·미리보기는 이 로컬 증거 폴더에 보존하고 강제로 공개 추적하지 않는다.
- 사용한 API 출처와 기준일 검증: Seeed 공식 고정 예제 원문, 설치 SDK 헤더, Espressif SNTP/NVS 공식 문서 2026-11-16 대조. 외부 제공자 약관/모든 발표 회차 재검증 아님.

## 최종 실기 관측

- 사용자 육안 확인: 이 세션에서 “대시보드가 정상 표시됨” 응답을 받았다. 패널 표시 smoke는 사용자 확인과 UART 드라이버 성공 로그를 함께 근거로 삼는다.
- 최종 UART 수집: 2026-11-16 15:02:22~15:05:02 KST, 약160초. NTP 샘플5개 모두 시스템 적용1회·RTC 쓰기1회 유지, 소수 초 비영 유지. health3개 모두 USB/Wi-Fi 연결, heap233,776B 유지. 패널 갱신 성공1회, 충전 제한 state4/readback1/cfg0a 확인. panic/watchdog/refresh 실패/checkpoint 실패 로그0건. `final-proof.json` 8개 단기 판정 모두 PASS.
- 보안: 정제 로그23개에서 패턴/known private 값 검출0건. 공개 Git index 검사도 findings0건. private 운영 바이너리의 해시만 보고하고 바이너리는 ignored `.pio`에 유지한다.

최종 운영 이미지 `firmware.bin` 2,024,080B, SHA256 `ff678a9d34ba02b2813883001a9fca2c4a459e786ac7f7cdde3f414d224b1459`. 기본 배포 이미지와 별개이며 개인 설정을 포함해 공개하지 않는다. `final-proof.json`에 카운터·상태·오류 수를 기록했다. 160초 단기 관측은 24시간 정확도·누수 부재·배터리 수명 보증이 아니다.

## 미완료/블로커

- BLOCKED_HW: 물리 USB 탈착/재연결, 잠든 상태 충전 전류, SOC 정확도, 버튼 wake, 배터리 수면 갱신/전류 실측, 24시간 시계 정확도. 연결은 되어 있으나 원격으로 전원 탈착·계측기를 조작할 수 없음. 충전 레지스터 mock 통과를 전류 실측으로 격상하지 않음.
- BLOCKED_CREDENTIALS: 로컬 개발 설정 사용 가능. 모든 공급자·발표일·공휴일 공개 시점 실응답 검증은 이번 단기 실행 범위 밖.
- BLOCKED_NETWORK: 원격 Git push 대상 없음(`git remote -v` 비어 있음). 임의 URL을 만들거나 추측하지 않음. 원격 CI 실행도 미확인.
- 기타: 실제 LAN 페어링→기기 설정 저장→시작 화면/순환 UI 전체 조작은 미완료. 생산용 공유 복원 경로 및 자동 순환 기존 회귀로 검증. 모든 ESP32 SDK adapter를 호스트에서 모의 실행하는 체계는 잔여 한계.

## 게이트 판정

- 이번 단계 목표: 코드/호스트(총357개, 실패0)/clean 타깃 빌드/플래싱/단기 실기 smoke PASS. 전체 게이트는 장기·전류·물리 조작과 외부 CI가 남아 PARTIAL.
- 다음 단계에서 해야 할 일: USB 탈착 상태에서 충전 재개 전류, 일반15분 wake·내용 동일 시 화면 skip, 실제 LAN 시작 화면 저장/자동 순환, 24시간 clock-vs-host 비교. 원격을 지정한 뒤 main push 및 CI 확인.
- 실기 없이 성공이라고 표시하지 않은 항목: partial refresh, deep sleep 전류/배터리 수명, 충전 전류·80% SOC, 버튼 wake, 24시간 안정성. 기기 연결·업로드·부팅 로그와 구분했다.
