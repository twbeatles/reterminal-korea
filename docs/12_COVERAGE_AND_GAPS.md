# 12. 요구사항 커버리지와 남은 갭

## 2026-11-16 기기 공식 문서 재조사

- [감사 문서](21_DEVICE_DOCUMENT_AUDIT_2026-11-16.md), [ADR 0032](adr/0032-device-document-audit.md). 부분 갱신 누적 상한 5회(패널 제조사 권장), 배터리 측정 대기 10ms, 동작온도(0–40°C) 밖 UART 경고, 공식 저장소 드리프트 점검 도구를 추가했다. Host·Build만 확인했다([증거](../evidence/device-docs-audit-2026-11-16/summary.md)).
- 새로 BLOCKED_HW로 남긴 항목: Deep Sleep 중 GPIO21/16 레벨과 sleep 전류, OTP 고정 온도값(E5=5F)에서의 저온 화질.

## 2026-11-13 장기 운영 감사 시작

- 같은 날 실기 후속: 충전 보호 정책·v4 설정(v2/v3 읽기 유지)·기기 표시·포털·UART 캡처 및 PMIC/Wi-Fi 안정성 개선을 구현했다. 실기 충전 enable/read-back·Wi-Fi 손실 복구·watchdog 주입 결과 및 최종 이미지 정보는 [실기 보고](../evidence/long-term-device-2026-11-13/summary.md)를 따른다. 아래의 “80% 제한 미구현”은 최초 문서 작성 시점의 이력이다. 정확한 SOC/전류·장기·배터리 반복 검증은 아직 완료가 아니다.

- [사전 감사·단계별 합격 기준](17_LONG_TERM_OPERATIONS_AUDIT.md), [ADR 0027](adr/0027-long-term-observability.md). LT-02는 60초 메모리 관측 및 USB 연속 로그 기간/공백/리셋 검사를 추가한다. 실행 결과는 [단계 보고](../evidence/long-term-2026-11-13/summary.md)를 따른다.
- 80% 충전 제한은 미구현. 공식 E1001 회로도 리비전별 충전 칩 차이가 있어 실물 리비전·칩 ID부터 확인한다. USB 24h/72h/7일, 배터리 72h, 충전 전류, sleep/wake 반복은 여전히 BLOCKED_HW다.

## 2026-11-11 v1 기준점 (패널 반복 갱신·글꼴·관심종목)

- 반복 화면 갱신: 첫 화면만 표시되던 원인은 공유 SPI의 microSD 슬롯 핀(CS14/EN16) 미고정이었다. 패널 통신 전에 고정해 해결했고, 같은 기기에서 갱신 BUSY가 9s→4.7s로 바뀌고 페이지 0→1→2 전환을 사용자가 육안 확인했다 → `DONE_VERIFIED_HW`(단기 버튼 3회·주기 갱신). CS/EN 중 결정 요인 분리, SD 카드 삽입 상태, 장시간·절전 반복은 미검증. [ADR 0012](adr/0012-repeated-panel-refresh.md), [ADR 0013](adr/0013-shared-spi-sd-guard.md).
- 패널 파형: 읽기 전용 OTP 판별로 내부 파형(`internal-otp`) 선택, 극성은 `00=흰색/11=검정`(공식 관례의 반전을 실기에서 정정).
- 글꼴/UI: 14/18px Medium·64px SemiBold 2bpp, `㎍/㎥` 원본 글리프, 큰 숫자 아래 간격 확대. 실기 육안은 사용자 확인("잘 나오고 있다")이며 회색 중간 단계 밝기 순서는 미검증. [ADR 0014](adr/0014-font-weight-and-units.md).
- 관심종목: 4개(삼성전자·SK하이닉스·NAVER·현대차)를 KIS 현재가로 실기 수신(VALID)하고 종목명을 표시(오프라인 17종 표, 없으면 코드). FSC 4종목 오후 호출은 미검증. [ADR 0015](adr/0015-watchlist-stock-names.md).
- 갱신 주기: 페이지 전환(회색 왼쪽=이전, 가운데=다음)은 API를 호출하지 않고 공급자별 주기로만 갱신한다. 녹색 KEY0 짧게(수동 새로고침)는 공급자별 120초 쿨다운. 호스트 테스트만 확인, 실기 미확인. [ADR 0016](adr/0016-refresh-cadence-and-manual-cooldown.md), [펌웨어 점검](15_FIRMWARE_REVIEW_2026-11-11.md).
- v1 개선(ADR 0017): Wi-Fi 끊김 감시·재연결, 금융위 최초 조회(13시 전 1회), KIS 전일 대비율 표시, 종목명 76종(금융위 응답과 대조), 주기 배율(빌드 설정). 호스트 테스트·빌드만 확인, 실기 미확인. [ADR 0017](adr/0017-v1-improvements.md).
- 깜빡임: 헤더 시계만 바뀐 화면은 30분 동안 다시 그리지 않음(실기 8분 관찰로 갱신 횟수 감소 확인, 육안 확인 대기). 부분 갱신은 컨트롤러가 지원하나 4계조 글꼴과 맞지 않아 켜지 않았다. [ADR 0018](adr/0018-flicker-reduction-and-partial-refresh.md).
- 달력 탭(생활→시장→달력→기기): 공휴일·휴장 표와 24절기 상수표(2026~2028) 사용, 새 호출 없음. 코스피 지수는 KIS 업종지수 조회(실기 수신 확인). 달력 화면의 실기 육안·4탭 순환은 미확인. [ADR 0019](adr/0019-calendar-tab-and-kospi-index.md), 코스피 저장·마감 후 1회 갱신 [ADR 0023](adr/0023-kospi-persistence-and-post-close-catchup.md).
- Wi-Fi: USB 전원에서 부팅 시 선연결과 끊김 자동 재연결(백오프·라디오 리셋·진단 로그). 원인은 "갱신할 것이 없으면 연결을 시도하지 않던" 정책이었다. 부팅 직후 연결은 실기 확인, 끊김 복구는 호스트 테스트만. [ADR 0022](adr/0022-wifi-keepalive-and-auto-reconnect.md).
- 미구현: 애널리스트 컨센서스(목표주가·추정치)는 공식 무료 API가 없어 범위 밖이다. 필요하면 데이터 출처·약관 조사를 먼저 한다.
- 검증: Python 162 / C++ 80 PASS, 타깃 빌드 RAM 70,804 B / Flash 1,814,058 B, 업로드 해시 검증. 요약은 [V1 기준점](V1_BASELINE.md).

## 2026-11-11 첫 실기·실 API 통합 후속

- COM5의 실제 E1001에 USB 업로드/hash 검증/정상 부팅 완료. 사용자 대시보드 표시 확인. Flash32MB·embedded PSRAM8MB 확인, 파티션은 유지했다.
- 비밀번호 정정 후 Wi-Fi/NTP 및 EXIM/공휴일 수신 확인. HTTP/1.0에도 chunked가 오는 실제 KMA/Air 응답으로 raw framing 오류를 찾아 bounded decoder를 추가했고, 기기에서 KMA 실황/예보/Air/EXIM 내부 OK·KIS 토큰/현재가 성공을 확인했다.
- 양천구 Air PM 결측은 실제 통신장애 응답과 공식 공지에 부합하므로 `--` 유지. FSC V2 root/operation 수정 및 인증된 종가/기준일 PC 캡처 완료; 기기의 13시 이후 예약 갱신은 별도 미검증.
- Python159/C++77 PASS, 타깃 빌드 RAM70,804 B/Flash1,686,894 B. 새 라이브러리 없음. [상세 보고/남은 경계](../evidence/live-api-integration/summary.md).
- 2026-11-12: 같은 Wi-Fi 포털·자동 순환·설정 v3 추가([ADR 0024](adr/0024-lan-settings-portal.md), [0025](adr/0025-page-auto-rotation-and-market-fsc-close.md)). 로그인 전 보호·잠금·순환은 직접 만든 요청과 로그로 확인, 실제 브라우저 로그인 이후 흐름은 미확인.
- 포털 E2E·부분 갱신·100회 wake·전류·배터리 수명·USB 복구·시장 계약 전체는 완료 처리하지 않는다. 아래 과거 BLOCKED_HW/CREDENTIALS 표는 해당 날짜 이력이다.

## 2026-11-05 감사 후속 상태

최신 검증 근거는 [후속 보고](../evidence/audit-followup/summary.md)와 ADR 0010이다.
아래 과거 결과 표는 날짜별 이력이며 최신 결과와 섞어 완료 처리하지 않는다.

| 항목 | 현재 상태 |
|---|---|
| 2026-11-12 최신 | Python 164 passed / C++ 131 tests 0 failures; 빌드 RAM 78,380 B / Flash 1,968,458 B (`ISSUE-007~014`, `G-1~G-9` 조치 반영, 과거 행은 이력) |
| ISSUE-004~006 | 원본 회차/격자 검증, PM별 관측시각, TTL 내 실패 배지 수정·회귀 완료 |
| Python/C++ | 159 passed / 72 Tests 0 Failures; 공통 골든 40건 유지 |
| 타깃 빌드 | SUCCESS; RAM 70,788 B / Flash 1,682,406 B |
| 실행 경계 | ICyclePlatform/EspCyclePlatform으로 실제 RunCycle을 Host에서도 실행 |
| 감독/복구 | 90초 watchdog task 등록·포털 진행 Feed, 실패 렌더 재시도/RTC 성공 CRC 무효화 |
| 저장 | 정상 체크포인트 생략과 실제 실패 구분, migration 미지원 버전의 상태 보존 |
| UI/도구 | 최신 종가 fallback, 14종 실제 C++ 프리뷰 검수, mutation/soak fixture/stack PASS |
| OS CI | Windows/Linux/macOS matrix 구성; 외부 실행 증거 없음 |
| 미완료 | E1001/L4 실 API·TLS·전류·wake·복구/soak, 거래소 전체 계약, 차기 schema migration |

기준일: 2026-11-02 KST. 과거 단계별 표를 현재 구현 기준으로 통합했다.
검증 결과 표의 카운트·게이트 행은 2026-11-03 실측으로 갱신했다.
2026-11-05 이전 감사의 근거·조치는 `git show 2463c81:PROJECT_AUDIT.md`(§1–10)와
[evidence/audit-remediation/summary.md](../evidence/audit-remediation/summary.md)를 따른다.
2026-11-12 감사와 조치는 현재 [PROJECT_AUDIT.md](../PROJECT_AUDIT.md)와 [ADR 0026](adr/0026-audit-2026-11-12-fixes.md)를 따른다.
설계 입력 docs/00–10은 역사적 계약으로 유지한다.

## 검증 결과

| 게이트 | 명령 | 현재 결과 |
|---|---|---|
| Python 미러·골든·SEC-01·도구 | python -m pytest host/tests -q | 149 passed, 13 warnings, 실패 0 |
| C++ 코어·통합·회귀 | python tools/run_native_tests.py | 63 Tests, 0 Failures; 공통 골든 40건 포함 |
| E1001 펌웨어 | pio run -e reterminal_e1001 | SUCCESS, 증분 빌드; RAM 70,804 B / Flash 1,675,478 B |
| 뮤테이션 게이트 | python tools/mutation_check.py | PASS (Missing→0·신호 반전 둘 다 kill) |
| soak 파서 게이트 | python tools/soak_report.py test/fixtures/soak_ok.log | PASS (fixture 수준; 실기기 로그 대기) |
| 정적 스택 | python tools/check_stack_usage.py --json evidence/audit-remediation/stack-current.json | PASS; 알려진 경로/SDK 여유분 기준, 실기 증명 아님 |
| 실제 C++ 화면 프리뷰 | python tools/render_previews.py | 12종 생성, 모든 margin 검사 통과 |
| 기기/실 API | 미실행 | BLOCKED_HW / BLOCKED_CREDENTIALS |

## 구현과 연결 상태

| 과제 | Host/Build 상태 | 실제 펌웨어 연결 | 남은 검증 |
|---|---|---|---|
| P0 E1001 기반 | 공식 원문/board/qio_opi/핀 확인, 타깃 빌드 성공 | setup/loop | 실물 Flash 32MB·핀/전원 |
| T02 설정/영속 저장 | CRC/세대 비교·연속 meta/slot 정전 모의·설정 검증·readback | NVS 설정, RuntimeStore 스냅샷/재시도/인증 차단 복원 | 실제 NVS 전원차단/마모 |
| T03 RTC/NTP | ClockBootstrap fake-port 회귀 | RTC 유효 시 OS UTC 적용, 실제 SNTP 통지로 신뢰 판정 | RTC 정확도·NTP/TLS 실통신 |
| T04 화면 | 실제 한글 bitmaps·12종 프리뷰·픽셀 oracle | ViewModel→Canvas→UC8179 SPI, 성공한 출력만 CRC 저장 | 패널 표시/방향/파형/잔상/시간 |
| T05 전원 | CyclePlanner·RtcState·요청 deadline·72h 재부팅 시뮬레이션 | USB idle, 배터리 timer/EXT1, 저전압 우선, 크래시 안전모드 | 100회 wake·USB 변화·전류 |
| T06 버튼 | 디바운스/짧게/길게·wake mask | GPIO3/4/5, held-key wake 제외 | 실기 오인식/각 키 wake |
| T07 포털 | CSRF/폼 전체·삭제 체크박스·중복/크기 검증 | AP STA 접속시험→검증 저장→재부팅, 명시적 2단계 초기화 | 브라우저/AP/틀린 키 복원 E2E |
| T08 센서/배터리 | CRC·범위·last-good·codec·배지 | SHT40 0xFD/10ms 측정, ADC/PMIC·전원 계획 | 기준계 대조·전류 |
| T09 달력 | 공식 기술문서 기반 full-year parser·손상 거부·연도 이월 | 올해/내년 조회·NVS·스케줄 휴일 반영 | 특일 실응답·KRX 추가 휴장 출처 |
| T10 릴리스 | 버전/board marker·비밀 marker·clean Git·3개 이미지/해시 검사 | 패키징 도구, 실제 배포 안 함 | 32MB/복구 offset·실기 USB 복구 |
| T01 probe | 명시적 --live·쿼터/timeout·응답 검증·정제 fixture | 개발용 PC에서만 실행하는 검증 도구 | 실제 키·시각별 probe |
| P5 금융 | 기본 OFF, 종가/REST 현재가/고시환율 구별 | 기존 fetch 및 시장 UI, 최대 4종목 | KIS NXT/통합 거래시간·약관 |

## 감사 항목 추적

ISSUE-001~003은 구현 수정 및 Host/Build 회귀 완료다. 실제 기기 재현·복구는 별도 검증한다.
2026-11-03 추가분: RequestBudget 배선(Fetcher SpendSlot·defer≠실패) + Host/Build 회귀(네이티브 63T·호스트 149T), 날짜 역산 검증(C++/호스트 미러 동기화), FillDemo DEMO 표기+코덱 ID 7~9, FscDue 달력계층 명시+공휴일 회귀, 뮤테이션 게이트(tools/mutation_check.py·CI 배선), soak 파서(tools/soak_report.py·fixture 게이트·실기기 로그 대기), Canvas NUL 강건화·CyclePlanner 데드코드 제거.
TLS 런타임(핸드셰이크/만료) 검증은 L4 기기 단계로 연기한다(TLS-01은 정적 스캔 유지).
릴리스 검증은 fixture 수준이며 실기 USB 복구·OTA E2E는 HW 확보 후 수행한다(T10).
GAP-01~05/07~09는 코드와 기기 어댑터 연결까지 구현했다.
GAP-06은 공휴일/기존 시장 창과 연결했지만 KRX 추가 휴장일·NXT 시간은 아직 확정하지 않았다.
GAP-10의 요청 deadline은 구현했으며 실제 전력/최대 active 시간 계측은 남았다.

- 공휴일 API가 정상이어도 전체 KRX 휴장 달력을 검증한 것은 아니다.
  market_verified가 없는 평일은 UI에 휴장 가능으로 표시한다.
- 부분 갱신은 지원 false로 유지한다. 실측 없이 활성화하지 않는다.
  새로고침 정책 단위 테스트는 정책 로직(Partial 경로 판단)을 검증하며 기기 동작(비활성)과 다르다.
- UI 독립 Oracle: 동일 production render()로 기대/실제를 만들지 말라는 07 요구에 대해 현재 테스트는 양쪽 모두 Python 프리뷰로 생성한다. 픽셀 단위 배지 검사와 실-C++ 프리뷰 산출물(render_previews)로 보완하며, C++ 래스터와 PIL 플레이스홀더는 폰트 스택이 달라 픽셀 패리티는 적용하지 않는다.
- MapError의 conn/TLS 구분 불가 주석은 API 레벨 한계를 문서화한 것으로, TLS-01 정적 스캔과 함께 유지한다.
- OTA/커스텀 32MB 파티션/eFuse는 ADR에 따라 미적용이다.
- 설정 폰트는 KS X 1001 기본 한글 2,350자와 UI 문자를 포함한다. 없는 문자는 대체 glyph가 표시될 수 있다.
- 기기 실행·실 API 성공을 Host/Build 결과로 대체하지 않는다.
- NVS 저장/ignore/비밀 스캔은 분실 기기의 비밀 추출 방지를 보장하지 않는다.
