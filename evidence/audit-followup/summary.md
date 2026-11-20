# Phase Progress Report — 2026-11-05 감사 후속 개선

- 날짜·시간(KST): 2026-11-05, 이번 후속 구현 세션.
- 작업 단계/티켓: ISSUE-004~006 및 감사 Phase 1~3의 소프트웨어 개선.
- 커밋 SHA / 공식 기반 commit: 작업 기준 main 106cb58ec68c9bb400181312a060c7f8b5b7b21b / Seeed 8d97d8912987c07529287629f7e73039bb0e6384. 최종 변경 commit은 Git 이력 참조.
- 작업 환경: Windows PowerShell, Python 3.15.2, PlatformIO Core 6.2.0, espressif32 8.0.0, Arduino-ESP32 3.3.12, ArduinoJson 7.4.3. 기존 Zig/패키지 캐시 사용. 새 firmware 라이브러리 설치 없음.
- 기기 연결 여부 및 모델 확인: 없음. flash/erase/reset/eFuse 조작 없음.
- API 실키 접근: 없음. fixture/fake transport만 사용.

## 구현 내용

- 변경 파일:
  - src/providers/ProviderJson.*, host/kdash/kma.py, src/app/Fetcher.cpp — 원본 회차/격자 검증과 실제 요청 격자 전달.
  - src/providers/AirKoreaParser.*, host/kdash/airkorea.py — 항목별 최신 유효값, 응답 행 순서 독립화.
  - src/ui/ViewModel.cpp, Renderer.cpp — PM별 관측시각/과거 보완 배지, 최근 조회 실패 표시, 긴 배지로 항목명이 사라지지 않는 배치, 최신 종가 fallback.
  - src/app/CycleRunner.*, CyclePlatform.h, EspCyclePlatform.cpp, Coordinator.h, include/IHttpClient.h, src/net/HttpTransport.h — 동일 Host/Device orchestration, 예산·시각·저전압·실패 렌더 재시도.
  - src/drivers/Watchdog.*, src/main.cpp, provision/PortalServer_esp32.cpp — SDK 초기화/재설정/현재 task 등록 및 진행 Feed. SDK idle mask 유지.
  - src/storage/RuntimeStore.*, SettingsCodec.*, power/RtcPersistence.cpp — 정상 쓰기 생략과 실패 구분, 오류 로그, 실패 렌더의 RTC 성공 CRC 무효화, migration 실패 원자성.
  - test_native/test_audit_followup.inc, test_core.cpp, test_audit_flow.inc, host/tests/test_audit_followup.py, test_kma.py — 신규 회귀 및 합성 응답의 실제 원본 메타데이터 정합성.
  - tools/render_previews.*, check_stack_usage.py, .github/workflows/host-tests.yml — 공용 compiler 선택, preview 오류 전파, 새 시나리오, stack 포트 경로, 세 OS CI.
  - PROJECT_AUDIT.md, docs/02·03·05·06·10·12·BUILD, ADR 0004/0010, 이 보고서/stack.json/previews — 계약·증거·미검증 구분.
- 변경 목적: 잘못된 원본 시각 승인과 UI 혼합/실패 은폐 수정, 실제 실행 경계의 회귀 검증.
- 기존 기능 영향:
  - KMA 성공 응답에서 baseDate/baseTime/nx/ny 누락을 더 이상 허용하지 않는다. 예전 수치 전용 합성 테스트는 올바른 메타데이터를 갖도록 수정했다.
  - 72시간 fake API도 매 요청 회차를 반영한다. 오래된 고정 회차를 최신 관측으로 받아들이게 하지 않는다.
  - NVS/RTC wire schema는 변경하지 않았다. 현재 버전 migration은 decode 검증, 미지원 버전은 원자적 거부다.
  - 기존 last-good·retry·인증 차단·quota·금융 종류/JPY(100) 계약을 유지한다.

## 재현 가능한 증거

| 구분 | 명령/방법 | 결과 | 검증 단계 | 증거 경로 |
|---|---|---|---|---|
| Unit/도구/보안 | python -m pytest host/tests -q | PASS: **159 passed**, 실패 0, warnings 13 | Host | host/tests/test_audit_followup.py 및 기존 suite / 세션 출력 |
| C++ Unit/Integration | python tools/run_native_tests.py | PASS: **72 Tests, 0 Failures, 0 Ignored**; 내부 golden 40건 | Host | test_native/test_core.cpp, test_audit_followup.inc / 세션 출력 |
| Firmware | pio run -e reterminal_e1001 | PASS: RAM **70,788 B**, Flash **1,682,406 B** | Build | .pio/build/reterminal_e1001/firmware.elf, firmware.bin |
| Clean build | 별도 clean 명령 미실행 | NOT_RUN | Build | 증분 빌드와 구분 |
| Fault injection | 잘못된/누락 회차·격자, 혼합 PM 시각, 503 캐시, 시계 불신, 저전압, 예산 소진, 렌더 실패/복구, NVS 오류/생략 구분 | PASS: 위 72개에 포함 | Host | test_audit_followup.inc |
| 72h/reboot | 요청 회차를 반영하는 fake API, 기존 재부팅·쓰기 budget 회귀 | PASS: 72회 실황 요청 | Host simulation | test_audit_flow.inc |
| Mutation | python tools/mutation_check.py | PASS: Missing→0·freshness 부호 반전 모두 KILLED | Host | 도구 출력 / 기존 mutation script |
| Soak parser | python tools/soak_report.py test/fixtures/soak_ok.log | PASS: fixture fetch 8 / requests 11 / panic 0 | Host fixture | test/fixtures/soak_ok.log; 실제 soak 아님 |
| Static stack | python tools/check_stack_usage.py --json evidence/audit-followup/stack.json | PASS: max app frame **2,432 B**, 알려진 경로+reserve 한계 이내 | Build artifact | stack.json; 실기 high-water-mark 아님 |
| Host preview | python tools/render_previews.py --output-dir evidence/audit-followup/previews | PASS: **14종**, margin=1; 혼합 시각/503 배지 화면 직접 검수 | Host renderer | previews/*.png |
| OS CI | GitHub Actions Windows/Linux/macOS matrix 추가 | NOT_RUN: 외부 runner 실행 증거 없음 | CI | .github/workflows/host-tests.yml |
| E1001 smoke / sleep / 전류 / 복구 | 미실행 | NOT_RUN | Hardware | BLOCKED_HW |
| Live API / 실제 TLS·NTP | 미실행 | NOT_RUN | LiveAPI | BLOCKED_CREDENTIALS |

신규 Python 회귀 8개는 수정 전 모두 실패함을 확인했다. 이후 메타데이터 없는 기존
합성 테스트와 고정 회차 72h fake가 새 검증에 실패해 테스트 입력을 실제 계약에 맞췄다.
테스트를 통과시키려고 원본 검증을 완화하지 않았다. 초기 C++ 헤더 누락/Unity double
설정 문제는 테스트·구현에서 정정했고 최종 결과는 위와 같다.

## 독립 검증 및 보안

- Oracle: 고정 기대 epoch/관측 문자열/배지/성공·실패 상태로 새 회귀를 작성했다.
  실제 C++ 래스터의 생활/혼합 PM 시각/서비스 실패 이미지를 보고 배지 때문에
  PM 항목명이 숨는 문제도 수정했다. Python/C++ 전체 픽셀 parity는 폰트 스택 차이
  때문에 적용하지 않는 기존 결정이며 ADR 0010에 대안을 명시했다.
- Mutation: 기존 두 mutant가 모두 죽었다. 전체 결함 부재의 증명으로 해석하지 않는다.
- 보안 검사: 전체 host suite의 UI/security/secret-scan 통과. 실키·로컬 비밀 파일을
  열거나 출력하지 않았으며 실제 API 호출/기기 쓰기 없음. HTTPS/CA 검증 유지.
- 출처: 공식 고정 checkout의 E1001 board/qio_opi/PSRAM 원문과 설치 SDK의
  esp_task_wdt.h/sdkconfig를 읽었다. 이전 감사에서 본문 확인이 막힌 data.go.kr
  최신 계약을 이번 수정만으로 재검증했다고 주장하지 않는다.

## 제안별 이행과 남은 경계

| 감사 제안 | 현재 판정 | 완료/보류 근거 |
|---|---|---|
| ISSUE-004/005/006 | DONE_VERIFIED_HOST / DONE_BUILD_ONLY | 원본 검증, PM별 시각, 실패 배지 및 회귀 |
| task watchdog 배선 | DONE_BUILD_ONLY + BLOCKED_HW | init/reconfigure/add/feed 연결; 실제 hang reset/전류 영향 미검증 |
| 실제 orchestration Host 실행 | DONE_VERIFIED_HOST | 동일 RunCycle을 포트 주입으로 실행; 시계/전원/예산/저장/화면 오류 회귀 |
| 렌더/체크포인트 실패 처리 | DONE_VERIFIED_HOST / DONE_BUILD_ONLY | 재시도 플래그·RTC 성공 CRC 무효화·정상 생략과 실제 오류 구분 |
| 더 최근 FSC fallback | DONE_VERIFIED_HOST | Stale KIS와 유효하고 더 최근인 FSC일 때만 종류를 유지한 fallback |
| 설정 migration | 현재 버전/실패 원자성 DONE_VERIFIED_HOST; 실제 차기 버전 NOT_APPLICABLE | 게시된 이전 wire contract가 없어 임의 migration을 만들지 않음. 새 schema 변경 때 codec/fixture 필요 |
| 세 OS CI | CONFIGURED / BLOCKED_CI | workflow 추가, 실제 Linux/macOS/Windows runner 증거는 없음 |
| 독립 UI 회귀 | DONE_VERIFIED_HOST (범위 제한) | 고정 ViewModel 기대값+실 C++ 이미지 검수; 전체 픽셀 parity 아님 |
| 동시성·정전·시각/금융 경계 | 기존 회귀 유지 및 PASS | probe exclusive lock, 슬롯 fault injection, KST/윤년/공휴일/JPY(100) 골든; 실제 전원차단 아님 |
| KRX 전체 휴장/NXT·통합 시간·약관 | BLOCKED_PROVIDER_CONTRACT | 공식 계약/실응답 확보 필요. 기존 제한된 시간 창을 전면 지원으로 승격하지 않음 |
| TLS/DNS/header active time·배터리 | BLOCKED_HW / BLOCKED_CREDENTIALS | 기존 deadline+watchdog 보강, 엄격한 전체 60초 상한·전류는 실측하지 않음 |
| 패널/partial/100회 wake/USB 복구/장기 soak | BLOCKED_HW | 기존 실기 게이트 유지. 자동 OTA/32MB 파티션/eFuse 활성화 없음 |
| 문서 불일치 | 수정 | ADR 0004 실제 드라이버, 포트 구조, 시각/실패 정책, 현재 검증 수치·과거 감사 구분 |

## 미완료/블로커

- BLOCKED_HW: 실제 watchdog 강제 복구, 패널/잔상/전류/flash 용량/held-key wake/USB 복구/장기 soak.
- BLOCKED_CREDENTIALS: 공급자 승인키와 시각별 실응답, 실제 TLS/NTP.
- BLOCKED_PROVIDER_CONTRACT: KRX 추가 휴장/NXT·통합 시간·약관 확정. 임의 값 추가 없음.
- BLOCKED_CI: 외부 세 OS runner 실행 증거 없음. 로컬 Windows 결과만 확인했다.
- Git 원격: 이 checkout에 remote가 설정돼 있지 않아 push 대상이 없다. 임의 저장소를 연결하지 않는다.
- 기타: 13 warnings는 기존 langsmith/Python deprecation이다. 제품 테스트 실패와 구별한다.

## 게이트 판정

- 소프트웨어 수정/Host·Build 게이트: PASS. 제품 출시·전체 감사 제안 완료: **PARTIAL**.
- 다음 단계: 실제 E1001에서 watchdog/패널/전류/wake/복구 계측, 승인된 실 API fixture,
  거래소 계약 확정, 외부 CI 실행. 차기 schema 변경 전 migration/rollback 검증.
- 실기 없이 성공이라고 표시하지 않은 항목: 패널·부분 갱신·sleep/wake·전류·실 API/TLS·USB 복구.
