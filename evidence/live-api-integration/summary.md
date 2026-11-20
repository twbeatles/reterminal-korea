# Phase Progress Report — 실제 API 수신·대시보드 연동

- 날짜·시간(KST): 2026-11-11.
- 작업 단계/티켓: 첫 플래싱 후 Wi-Fi 비밀번호 정정, HTTP 200/PROVIDER_ERROR 원인 수정, 사용자 요청 공급자 전체 실연동 검증.
- 커밋 SHA / 공식 기반 commit: 작업 기준 main c37da88 + 이번 소스/로컬 설정 변경 / Seeed 8d97d8912987c07529287629f7e73039bb0e6384. 최종 commit은 Git 이력 참조.
- 작업 환경(OS/PlatformIO/보드 패키지 버전): Windows PowerShell / Core 6.2.0 / espressif32 8.0.0 / Arduino-ESP32 3.3.12 / esptool 5.4.0. 새 라이브러리 없음.
- 기기 연결 여부 및 모델 확인: 사용자 E1001, COM5 CH340 실연결, ESP32-S3 rev v0.2/8MB PSRAM/32MB Flash 읽기 확인.
- API 실키 접근: 사용자 제공 키를 Git 제외 로컬 파일에서만 읽음. 원본 UART/빌드/probe는 Git 제외 경로. 키·비밀번호·토큰을 보고서에 기록하지 않음.

## 구현 내용

- 변경 파일:
  - src/net/BodyReader.h/.cpp, HttpTransport.cpp: 상한/전체 deadline을 가진 chunked decoder와 응답 encoding 선택.
  - src/net/Endpoints.cpp, host/kdash/endpoints.py: FSC V2 root 및 `getStockPriceInfo_V2` operation 수정.
  - test_native/test_live_chunked.inc, test_core.cpp, host/tests/test_providers_extra.py: 네트워크 framing/timeout/overflow/실응답→파서→ViewModel 및 FSC 주소 회귀.
  - test/fixtures/live_*: 키 제거된 실제 KMA/Air/FSC/EXIM 응답; evidence/probe/*.json: 성공/실패를 구분한 실행 기록.
  - docs/03·10·12·BUILD, ADR 0011, hardware/board_e1001.md, first-device-flash 및 이 증거 폴더: 실제 완료 범위/남은 항목 기록.
  - 비공개 로컬 파일: Wi-Fi 정정값·FSC/KIS ON·삼성전자 005930 KRX 설정. public defaults는 변경하지 않음.
- 변경 목적: API 정상 응답을 raw chunk 헤더와 함께 JSON으로 읽던 펌웨어 오류와 폐기된 FSC 경로 수정.
- 기존 기능 영향: identity 응답 기존 경로 유지. JSON 회차/격자·결측·고시 환율/종가/REST 현재가 구분·TLS 검증·요청 예산 유지. NVS schema/파티션/eFuse/전체 erase 변경 없음. 정상 재부팅은 업로드와 로그 확인 범위에서만 수행.

## 재현 가능한 증거

| 구분 | 명령/방법 | 결과 | 검증 단계 | 증거 경로 |
|---|---|---|---|---|
| Python suite | python -m pytest host/tests -q | PASS: 159 passed, 실패 0, 기존 warnings 13 | Host | build/live-fix-pytest.log 및 최종 세션 출력 |
| C++ suite | python tools/run_native_tests.py | PASS: 77 Tests, 0 Failures, 공통 골든 40건 포함 | Host | validation.txt; build/live-fix-native-final.log |
| Fault injection | chunk extension/trailer/분할/zero chunk, 잘못된 CRLF/size/overflow/미완결/정지/시계 wrap/총 framing 상한 | PASS: 위 C++ suite의 신규 5개 그룹 포함 | Host | test_native/test_live_chunked.inc |
| 실응답→UI | 정제한 실황/예보/Air를 실제 decoder·C++ JSON parser·ViewModel로 처리 | PASS: 기온 8.7°C, Air `--`, DEMO 없음; FSC/EXIM 기준일/JPY(100) 회귀 | Host with captured fixtures | test_live_chunked.inc |
| Build | pio run -e reterminal_e1001 | PASS: RAM 70,804 B / Flash 1,686,894 B | Build | validation.txt; build/live-fix-build-final.log |
| Clean build | clean 명령 미실행 | NOT_RUN | Build | 증분 빌드와 구분 |
| Stack | python tools/check_stack_usage.py --json evidence/live-api-integration/stack.json | PASS: failures=[]; 알려진 정적 경로만 | Build artifact | stack.json |
| Upload | pio run -e reterminal_e1001 -t upload --upload-port COM5 | PASS: 98.28초, 이미지 해시 검증 후 정상 리셋 | Hardware | upload-redacted.log |
| Boot/Wi-Fi/NTP | 정상 리셋 후 115200 UART 50초 | PASS: fw0.3.0, source=dev, connected, NTP verified=1, FSC/KIS ON/watch=1 | Hardware | boot-redacted.log |
| Live API·표시 | 아래 공급자 표 | 공급자별 판정 참조 | LiveAPI/Hardware | boot-redacted.log 및 후속 passive 로그 |
| Sleep/wake/전류/USB 복구 | 미실행 | NOT_RUN | Hardware | 배터리 장기 검증 아님 |

## 공급자별 검증

| 공급자 | PC/공식 계약 | 실제 E1001 | 판정 및 남은 사항 |
|---|---|---|---|
| 기상청 실황 | 200/00, 기준 2026-11-11 09:00, nx58/ny126, 8개 항목 | provider=1 OK, 기온 8.7/습도68/강수0 VALID | 현재 회차 성공. 자정/발표 지연의 시각별 live 검증은 별도 |
| 기상청 단기예보 | 72행 200/00, base08:00; 1000행 probe는 64KiB 정책으로 미완료 | provider=2 OK, 10:00 예보 기온10/강수확률20 VALID | 기기 72행 경로 성공. 대용량 정책 유지 |
| 에어코리아 | 200/00; 양천구 모든 PM 값 `-`, 통신장애 flag | provider=3 OK, PM10/2.5 MISSING | API 정상·측정소 데이터 결측. 임의 0/다른 측정소/DEMO로 대체 안 함 |
| 수출입은행 | 오늘/전일 정제 응답 캡처 성공 | provider=4 OK, 11-11 고시 USD1343.30/JPY(100)846.44 VALID | 오늘 고시/100엔 단위 확인. 휴일 실응답/긴 연휴는 별도 |
| 공휴일 | 공식 특일 v1.4 기반 기존 회귀 | 첫 업로드 시 2027 OK 및 로컬 테이블 저장; 현재 재조회 주기28일 | 실제 성공 연도 범위를 전체 연도/거래소 휴장 계약으로 확대 해석하지 않음 |
| FSC 종가 | 공식 V2 operation 및 200/00, 005930 최신 기준11-09 종가189900 확인 | 기존 13시 이후 갱신 정책상 오전 미호출 | PC LiveAPI+실응답 C++ PASS, 기기 예약 실행은 미검증 |
| KIS 현재가 | 사용자 실전용/005930 확인. PC probe는 일일 한도 미확인으로 실행 안 함 | 초기 부팅/검증 리셋 직후 tokenP 403 RATE_LIMITED. 이후 추가 리셋 없이 65초 최소 간격 후 provider=6 OK, HTTP200/2 requests, REST 005930 199500 VALID | 실제 E1001 인증·조회·스냅샷 PASS; 토큰 자체 공개 안 함. kis-passive-redacted.log |

## 독립 검증 및 보안

- 단위/포맷/환율 기준일 Oracle: 실제 fixture의 고정 기대값/기준일/격자와 독립 비교. JSON 검증을 느슨하게 하여 통과시키지 않았다.
- Mutation 테스트 결과: 이번 세션 NOT_RUN. 신규 framing fault 주입 회귀 및 기존 시간/JPY/결측 회귀 PASS.
- 자격증명 유출 검사 범위/결과: 최종 추적/신규 파일·스테이징에 모든 로컬 키/SSID/비밀번호 exact-value 검색, fixture 정제·등록 상태 전용 로그. 결과 validation.txt. MAC은 공개 업로드 로그에서 제거. 비밀 포함 가능 바이너리/ELF/map/원본 로그는 제외.
- 사용한 API 출처와 기준일 검증: 2026-11-11 KMA/Air/FSC 공식 포털과 KIS 공식 inquire-price/kis_auth 원문 확인. FSC operation은 상세 페이지 내 Swagger까지 대조. [ADR 0011](../../docs/adr/0011-live-http-chunked-and-fsc-v2.md).
- PC quota: 사용자 제공 KMA10000/Air500/FSC10000/EXIM1000에 각각 1% 일일 ledger 적용. KMA 5회(4회 기본+HTTP/1.0 구조1회), Air1회, FSC2회, EXIM9회 예약(실패도 사용 처리). 기기의 정상 운영 호출은 PC ledger와 별도이므로 총 API 사용량은 포털에서 확인한다.
- PC 실패 이력: 첫 FSC V2-root/구-operation probe는 400/코드12; 전체 operation 수정 후 성공. 일부 EXIM PC 시도는 transport 실패했으며 기본 Windows TLS에서 issuer 확인 오류도 관찰했다. 검증 우회 없이 기기와 같은 공개 루트 CA로 지정한 후 오늘/전일 캡처 성공. 실패를 성공 실행 수에 합산하지 않는다.

## 미완료/블로커

- BLOCKED_HW: Flash/boot/Wi-Fi/일부 live API는 해소. 부분 갱신·배터리 전류·sleep/wake·포털 E2E·USB 복구는 미검증.
- BLOCKED_CREDENTIALS: 키 등록 완료. KIS 일일 할당량은 확인되지 않았고 임의 quota로 PC probe하지 않았다.
- BLOCKED_NETWORK: Wi-Fi 비밀번호 정정 후 해소. 공급자별 제한/측정소 결측과 구분한다.
- 기타: 양천구 측정소 2026-10-29~11-30 미표출 공식 공지. FSC 오전 기기 호출 미검증, 전체 KRX/NXT/통합시간/특수 휴장 계약은 여전히 별도.
- 원격 없음: main에 코드·정제 증거만 기록하며 push 대상 없음.

## 게이트 판정

- 이번 단계 목표: chunked/FSC 주소 수정·회귀·업로드·실제 날씨/환율/KIS 현재가 수신 PASS. 첫 업로드 화면 표시는 사용자 확인했고, 최신 실수치 화면 표시는 후속 사용자 관찰을 구분해 기록한다. 모든 공급자의 모든 경계조건/장기 안정성을 완료 처리하지 않는다.
- 다음 단계: 현재가/실화면 확인, FSC 오후 예약 갱신 확인, 실제 버튼·포털·배터리/절전 계측.
- 실기 없이 성공이라고 표시하지 않은 항목: PC fixture로만 검증된 FSC 기기 호출, 100회 wake/잔상/전류·배터리 수명·공휴일/장외 경계 전체.

## 2026-11-11 패널 반복 갱신 근본 원인 (ADR 0012/0013)

| 단계 | 펌웨어 변경 | 실기 결과 (COM5, 사용자 육안 포함) | 증거 |
|---|---|---|---|
| OTP 판별 + 4계조 | 읽기 전용 OTP probe → `internal-otp`, Seeed 관례 `11=흰색` | 첫 화면 색 반전(사용자 보고), 이후 프레임 미반영 | aa-otp-upload-redacted.log |
| 극성 수정 | OTP도 `00=흰색/11=검정` | 반전 해소, 여전히 첫 화면만 표시. 이후 프레임 elapsed 10.2s | aa-polarity-*-redacted.log |
| 단계 trace | `epd: phase=` 로그만 추가 | 첫 프레임 0x12 BUSY 3.7s, 이후 9.0s(LUT 모드 9.95s와 동일 → 설정 미반영 판단). 화면 미전환 | aa-trace-*-redacted.log |
| SD 슬롯 고정 | begin()에서 SD CS14=HIGH, EN16=LOW, DET15 pull-up | `sd_card_present=0`. 이후 프레임 BUSY 4.69s(첫 프레임과 동일), page 0→1→2. **사용자가 실제 페이지 전환 확인** | sd-guard-*-redacted.log |

- 판정: 반복 전체 갱신 Hardware PASS(단기 3회 버튼 + 주기 갱신). CS/EN 중 결정 요인 분리, SD 카드 삽입 상태, 장시간/저전력 반복은 미검증.
- 테스트: native 79/0, host pytest 162/0(신규 test_audit_epd_bus.py 2건), 빌드 RAM 21.6%/Flash 54.2%, 업로드 해시 검증.
- 보안: 신규 로그 exact-value 검사에서 키/시크릿/SSID/비밀번호 0건(공개 종목코드만 일치).

## 2026-11-11 관심종목 4개와 종목명 (ADR 0015)

| 구분 | 결과 | 증거 |
|---|---|---|
| 설정 | 로컬 `dev_credentials.h`의 관심종목 4개(005930, 000660, 035420, 005380). 부팅 로그 `watch=4` | watch4-upload-redacted.log |
| KIS 현재가 | 4종목 모두 VALID(HTTP 200). 첫 수신 requests=5(토큰1+종목4), 이후 4. 부팅 직후 첫 시도는 tokenP 분당 1회 제한으로 `RATE_LIMITED`(403)였고 백오프 후 재시도로 성공 | 기기 UART 캡처(커밋 제외, 값은 공개 시세) |
| 화면 | 종목명 표시. 사용자가 실제 패널에서 "잘 나오고 있다"고 확인 | 사용자 육안 |
| 테스트 | C++ 80/0. 신규 테스트가 이름 표 정렬·□ 누락 글리프 0·폭 190px 이하·코드 폴백을 고정 | test_audit_display.inc |
| 미검증 | FSC 4종목 오후 호출, 표에 없는 종목, 컨센서스(미구현) | — |
