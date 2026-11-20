# V1 기준점 (2026-11-11, 개선 반영본)

이 문서는 `git tag v1`이 가리키는 상태를 기록한다. 같은 날 앞서 만든 첫 기준점은 `v1-initial` 태그로 남겨 두었다(실기 확인까지 마친 상태). 현재 v1은 그 위에 [ADR 0016](adr/0016-refresh-cadence-and-manual-cooldown.md)·[0017](adr/0017-v1-improvements.md) 개선을 더한 것이며, 개선분은 2026-11-11 기기(COM5)에 올려 부팅·Wi-Fi·금융위·KIS·갱신 로그를 확인했다(`evidence/live-api-integration/v1-boot-monitor-redacted.log`). 화면 육안(등락률 표시, 깜빡임 감소)은 사용자 확인이 남았다. 이후 변경은 이 상태와 비교해서 회귀 여부를 판단한다.
단일 기기(Seeed reTerminal E1001) 한 대에서 확인한 결과이며, 다른 기기·패널 로트에서 같다고 보장하지 않는다.

## 식별

| 항목 | 값 |
|---|---|
| 기준 태그 | `v1` (`git rev-parse v1`로 커밋 확인) |
| 펌웨어 표기 | `KDASH_FW_VERSION="1.0.0"` (`platformio.ini`) |
| 대상 | reTerminal E1001, ESP32-S3 rev v0.2, embedded PSRAM 8MB, Flash 32MB(기본 파티션 유지) |
| 빌드 | `pio run -e reterminal_e1001` SUCCESS, RAM 70,900 B (21.6%), Flash 1,820,438 B (54.5%) |
| 테스트 | Python 162 passed, C++ 85 Tests 0 Failures (공통 골든 40건 포함) |
| 당시 로컬 설정(추적 제외) | `include/dev_credentials.h`는 당시 저장소 안 ignored 파일이었다. 현재는 [ADR 0030](adr/0030-public-source-private-configuration.md)에 따라 저장소 밖으로 이전. 관심종목 `005930,000660,035420,005380` |

## v1에서 확인된 것

| 구분 | 내용 | 검증 단계 |
|---|---|---|
| 부팅/업로드 | USB 업로드, 데이터 해시 검증, 정상 부팅 | Hardware |
| 네트워크 | Wi-Fi 2~3초 내 연결, RTC→NTP 시간 신뢰, HTTPS 인증서 검증 유지 | Hardware / LiveAPI |
| 생활 데이터 | 기상청 실황·단기예보, 에어코리아(양천구 PM은 측정소 장애로 `--`), 수출입은행 고시환율(USD, JPY 100) | LiveAPI |
| 시장 | KIS 현재가 4종목 VALID, 종목명 표시(오프라인 표 17종, 없으면 코드) | LiveAPI + 화면 사용자 확인 |
| 화면 | 4계조(2bpp) 글꼴(14/18px Medium, 64px SemiBold), `㎍/㎥`, 흰 배경/검은 글자 극성 | Hardware (사용자 육안) |
| 반복 갱신 | 부팅 후 주기 갱신과 버튼 페이지 0→1→2→0 전환이 패널에 실제 반영, 갱신 시간 약 4.7초 일정 | Hardware (사용자 육안) |

## 개선 반영분 (호스트 테스트·빌드만, 실기 미확인)

| 항목 | 내용 |
|---|---|
| 페이지 전환 | 호출 0회를 테스트로 고정, KEY2 수동 새로고침은 공급자별 120초 쿨다운 ([ADR 0016](adr/0016-refresh-cadence-and-manual-cooldown.md)) |
| Wi-Fi | 끊김·복구·재연결 로그와 60초 후 재연결 시도, 연결 로그에 RSSI·상태 코드 |
| 금융위 | 성공 이력이 없으면 평일 13시 전에도 최초 1회 조회 |
| KIS | 전일 대비율 표시(`+1.23%`) |
| 종목명 | 76종(금융위 `itmsNm`과 대조) |
| 주기 | `-D KDASH_REFRESH_SCALE_PCT`(100~400%) |
| 깜빡임 | 시계만 바뀐 갱신 생략(30분) ([ADR 0018](adr/0018-flicker-reduction-and-partial-refresh.md)) |

> v1 태그 이후: 달력 탭과 코스피 지수가 추가되었다([ADR 0019](adr/0019-calendar-tab-and-kospi-index.md)). 이 기록과 `v1` 태그는 그 이전 상태이므로 `git diff v1`로 변경 범위를 본다(C++ 90개, pytest 164개, RAM 72,668 B, Flash 1,834,342 B).

## v1에서 확인되지 않은 것

- 개선 반영분의 육안 확인(등락률 화면, 깜빡임), Wi-Fi 끊김 감시·재연결(끊김이 일어난 적이 없어 로그만 확인), 장시간 동작.

- 금융위원회(FSC) 종가: PC 실응답/파서만 검증됐고 기기 호출은 한 번도 확인하지 못했다(최초 1회는 13시 전에도 조회하도록 바뀜).
- 반복 갱신 원인(SD 슬롯 핀 고정) 중 CS와 EN 중 어느 쪽이 결정적인지, microSD 카드를 꽂은 상태, 장시간·수백 회 갱신. EN LOW=전원 차단은 공식 예제에서 추론한 것이다. [ADR 0013](adr/0013-shared-spi-sd-guard.md)
- 글꼴의 회색 중간 단계(01/10) 밝기 순서와 잔상, 부분 갱신, Deep Sleep·버튼 깨우기, 배터리 전류, 포털 E2E, USB 복구, 100회 wake.
- Wi-Fi 끊김: 관측된 끊김의 원인은 알 수 없다(재부팅하면 즉시 연결). 새 감시 로그로 원인을 볼 수 있게 했으나 아직 수집하지 못했다.
- 컨센서스(애널리스트 목표주가·추정치): 공식 무료 API가 없어 구현하지 않았다.
- 시장 지수(코스피·코스닥): 응답 형식을 실제로 확인하지 못해 구현하지 않았다.
- 표에 없는 종목은 코드로만 표시된다. 포털에서 갱신 주기를 바꾸는 기능은 없다(빌드 설정만).

## 재현

```powershell
python -m pytest host/tests -q               # 162 passed
python tools/run_native_tests.py             # 80 Tests 0 Failures (컴파일러 필요)
pio run -e reterminal_e1001                  # 펌웨어 빌드
python tools/render_previews.py --output-dir <dir>   # 14개 화면 미리보기
```

기기 기대 로그(UART `Serial1`, 115200):

- 부팅: `wifi: connected`, `display: sd_card_present=0 sd_power=off sd_cs=high`, `display: waveform=internal-otp`
- 프레임: `epd: phase=refresh_done` 약 4.7s, 이후 프레임도 동일(9~10s로 늘면 회귀)
- KIS: `fetch: provider=6 outcome=OK http=200`. 재부팅을 연속하면 tokenP 분당 1회 제한으로 첫 시도가 `RATE_LIMITED`일 수 있다(정상, 백오프 후 재시도).

## 이후 비교 방법

- `git diff v1 --stat`, `git log v1..HEAD`로 변경 범위를 본다. 기준 대비 테스트 수는 줄이지 않는다(Python 162, C++ 80).
- 패널을 건드리는 변경은 반드시 위 기기 기대 로그와 육안 반복 갱신으로 재확인한다. 호스트 통과나 `ok=1`을 실기 정상으로 격상하지 않는다.
- 근거 기록: [ADR 0011~0015](adr/), [실 API 통합 보고](../evidence/live-api-integration/summary.md), [커버리지와 갭](12_COVERAGE_AND_GAPS.md).

## 다음 후보

1. 개선분을 기기에 올려 화면(등락률·종목명)과 로그(Wi-Fi 감시, 금융위 최초 조회)를 확인하고, 확인되면 이 문서를 갱신.
2. 코스피·코스닥 지수(KIS 지수 API 응답 확인 필요), 포털 갱신 주기 설정(NVS 스키마 변경).
3. 컨센서스는 데이터 출처·약관 조사 후 결정.
4. 장시간 반복·절전·카드 삽입 상태 검증.
