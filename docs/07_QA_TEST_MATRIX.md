# 07. 품질·테스트·증거 체계

## 테스트 계층
- L0 Static: format, compilation, dependency license, `secrets` scan, 오류 문자, 파일 크기/글꼴 용량 검토.
- L1 Host pure C++: 날짜·환산·JSON/XML 파서·정규화·cache invalidation·backoff·스케줄.
- L2 Host adapters: Fake HTTP 응답/실패/시간 주입, 가상 800×480 framebuffer 골든 이미지.
- L3 Device smoke: UART boot, 버튼3개, 한글 표시, RTC/SHT40/배터리/저전력, TLS 인증 검증.
- L4 Live API: 실제 키 사용 HTTPS, 자료 기준시각, 속도, 응답 몸체 크기 및 rate limit.
- L5 Long-run: 24h→72h soak, 주기 sleep/wake, heap 변화, Wi-Fi 재연결, flash wear/ghosting.

## 필수 케이스 표
| ID | 상황 | 필수 결과 |
|---|---|---|
| KMA-01 | 응답 `resultCode=00`, 온도만 수신 | 온도만 Valid, 나머지 Missing |
| KMA-02 | HTTP200 내부 서비스키 오류 | ProviderError + 캐시 유지 |
| KMA-03 | 자정/발표이전 | 전 회차 요청, 잘못된 미래 시각 요청하지 않음 |
| AIR-01 | `pm25Value="-"` | 빈값 표시, 0으로 변환 금지 |
| AIR-02 | 측정소 다름/관측시간 2시간 전 | stale / station 표시 |
| EXIM-01 | `JPY(100)`와 `USD`, 천단위 구분 | 원/100엔과 원/1달러 정확한 라벨 |
| EXIM-02 | 주말 빈배열 | 전 영업일 제한조회, 기준일 명시 |
| FSC-01 | 요청일과 기준 거래일 다름 | 종가/거래일 표시, 실시간 오인 금지 |
| KIS-01 | 토큰 만료·401·429 | 재발급 횟수 제한, 백오프, 키 비노출 |
| TLS-01 | 만료/불일치 인증서 | 요청 거부, TLS 우회 금지 |
| NET-01 | DNS 오류/응답 10MB 초과 | 예산 내 종료, 기기 리셋 루프 금지 |
| CACHE-01 | 캐시 CRC 틀림·정전중 기록 | 이전 정상본 사용 또는 안전 재설정 |
| UI-01 | 한글 긴 지명·주가 8자리 | 잘림 없거나 합리적 줄임, 단위 보존 |
| UI-02 | 키 미설정/모의데이터 | `DEMO` 또는 `설정 필요` 강제 |
| PWR-01 | 100회 sleep/wake | 버튼/타이머 기상 유지, 무한 재시작 없음 |
| SEC-01 | 로그/fixture/바이너리 비밀 marker | 검색 결과 0 |

## 독립 Oracle
- 환율 단위: 계산기를 거친 예상값을 고정 fixture로 작성, 원본 API 파서 계산 결과와 비교.
- 시각: Python 또는 표준 UTC 라이브러리로 독립 생성한 KST 기대값과 C++ 결과 비교.
- 기상청 발표: 공식 발표시간표를 CSV로 수동 코드화한 이후, 일부 샘플 날짜를 독립 fixture로 검증.
- UI: 800×480 골든 비트맵 비교 + 수동 스크린샷 평가. 동일 production `render()` 함수로 기대값과 실제값을 모두 만들면 독립 Oracle이 아님.
- 설정·캐시: fault injection으로 손상된 blob을 넣고 복구 경로가 실제 동작하는지 확인.

## Mutation / 장애 주입
- parser의 `Missing`을 `0`으로 바꾸거나 stale 검사 부호를 반대로 바꾸는 mutation에서 테스트가 실패해야 함.
- 일부/전부 네트워크가 끊기는 상황과 clock rollover를 100회 반복하고 leak/retry storm 검사.
- 빌드 실패해 테스트를 실행하지 못한 항목은 `NOT_RUN`으로 표시하고, CI pass 처리하지 않는다.

## 증거 저장 규칙
`evidence/p<N>/{build.log,test-results.xml,host-preview/*.png,uart-redacted.log,hardware-photo.jpg,api-probe-sanitized.md,summary.md}`.
- API 키 포함 가능성이 있는 raw UART/HTTP 로그 원본을 커밋 금지.
- 사진은 개인위치·SSID/토큰이 보이면 제거.
- 각 단계 마지막에 `templates/PROGRESS_REPORT.md` 작성하고 검증 유형/기준 커밋 SHA 명시.
