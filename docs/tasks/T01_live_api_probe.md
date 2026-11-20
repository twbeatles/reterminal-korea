# T01 — 실 API probe로 미확정값 확정

- 상태: NOT_STARTED (BLOCKED_CREDENTIALS)
- 선행: 없음. 키가 생기는 즉시 진행할 수 있습니다.
- 필요 자원: 본인 키 — data.go.kr(기상청 단기예보, 에어코리아 대기오염, 선택: 금융위 주식시세), 한국수출입은행 authkey, 선택: KIS 앱키/시크릿. 실기 또는 PC 한 대.
- 관련: `docs/adr/0003-live-api-integration.md` 표의 D1·D3·D4·D9·D10·D11, `templates/API_PROBE_RECORD.md`

## 1. 목표

지금 구현은 공개 페이지·관행·커뮤니티 구현에 근거한 **출발값**으로 동작합니다. 실제 응답으로 아래 항목을 확정하고, 결과를 골든 케이스에 고정합니다.

| 확인 항목 | 현재 가정 | 어디에 반영 |
|---|---|---|
| KMA 초단기실황 제공 지연 | 정시 생성, 40분 후 요청 가능 (`kma::kUltraNowDelayMin`) | `src/providers/KmaParser.h`, `host/kdash/kma.py` |
| KMA 단기예보 제공 지연 | 발표 +10분 (`kVillageDelayMin`) | 동일 |
| KMA 단기예보 정렬 | `fcstDate,fcstTime` 오름차순, 시간당 약 12개 카테고리 → `numOfRows=72`로 다음 슬롯 확보 | `kVillageNumRows` |
| KMA 오류 응답 형태 | 키 오류: HTTP 200 + XML `returnReasonCode` 30 | 골든 `kma_xml_key_error_http200` |
| AirKorea `stationName` | 측정소별 조회 응답에 있을 수도/없을 수도 → 있으면 일치 필수 | `air::Parse`, 골든 `air_station_field_absent_accepted` |
| AirKorea 자정 표기 | `YYYY-MM-DD 24:00` | 골든 `air_midnight_2400` |
| AirKorea `ver` 파라미터 | `ver=1.0` (pm25Value 포함 가정) | `Endpoints.cpp` |
| EXIM RESULT 코드 | 1 성공, 2 데이터코드, 3 인증, 4 일일한도 / 키 소문자 / RESULT 숫자 | `codes::Exim`, 골든 `exim_*` |
| EXIM 11시 이전·휴일 응답 | `[]` 또는 `null` | 골든 `exim_empty_walks_back`, `exim_null_body_is_empty` |
| FSC 경로·필드 | `getStockPriceInfo`, `likeSrtnCd`, `beginBasDt`, `basDt/srtnCd/clpr`, 빈 결과 `items:""` | `Endpoints.cpp`, `ParseFsc` |
| KIS 성공·오류 코드 | `rt_cd="0"`, 만료 `EGW00123`(HTTP 500), 초당 제한 `EGW00201`, 토큰 1분 제한 `EGW00133`(HTTP 403) | `codes::Kis` |
| KIS App Secret 길이 | 약 180자 (상한 256) | `kMaxKeyLen` |
| 응답 크기 | 모두 64KB 이하 (`kBodyMaxBytes`) | 실측 기록 |

## 2. 절차

### 2-1. PC probe (실기 없이 가능)

1. `tools/api_probe.py`를 새로 작성합니다(이 과제의 첫 코드).
   - 입력: 환경변수 `KDASH_KMA_KEY`, `KDASH_AIR_KEY`, `KDASH_EXIM_KEY`, (선택) `KDASH_KIS_APP_KEY`, `KDASH_KIS_APP_SECRET`. **명령행 인자로 키를 받지 않습니다**(셸 기록에 남음).
   - URL 생성은 `host/kdash/endpoints.py`의 함수를 그대로 사용합니다(장치와 같은 모양).
   - 각 호출의 HTTP 상태, Content-Type, 본문 크기, 응답 시간을 기록합니다.
   - 원시 본문은 `evidence/probe/raw/`에만 저장합니다(`.gitignore`에 `evidence/**/raw/` 추가). 저장 직후 `tools/fixture_sanitizer.py` 로직(`kdash.secret_scan.scrub`/`redact_text`)으로 세척한 사본을 `test/fixtures/live_*.json|xml`로 만듭니다.
   - 시나리오:
     - KMA: 실황(현재 시각 기준 회차 + 직전 회차 + 지연 경계 ±5분), 예보(최신 회차, `numOfRows=72`와 `1000` 비교)
     - AirKorea: 설정 측정소, 존재하지 않는 측정소, 자정 직후 1회
     - EXIM: 오늘·어제·지난 토요일, 11시 이전 1회, 일부러 틀린 키 1회
     - FSC: 005930 최근 14일
     - KIS: 토큰 1회 → 현재가 1회 → 61초 안에 토큰 재요청(EGW00133 확인)
   - 호출 수는 각 API의 일일 한도 대비 1% 이하로 제한합니다.
2. 결과마다 `templates/API_PROBE_RECORD.md` 형식으로 `evidence/probe/<provider>-<date>.md`를 작성합니다(키·전체 URL 미기재).

### 2-2. 실기 probe

1. 기본 `pio run -e reterminal_e1001 -t upload`와 기기 포털로 설정하거나 [외부 개인 헤더](../20_PUBLIC_REPOSITORY_GUIDE.md)를 사용하는 개발 환경으로 업로드한 후 UART 브리지 포트로 모니터링합니다(`Serial1`, 115200).
2. `fetch: provider=… outcome=… http=…`와 `snapshot:` 출력을 30분 이상 수집합니다. UART 원본은 커밋하지 않고, 세척본만 `evidence/probe/uart-redacted.log`에 남깁니다.

## 3. 반영 규칙

- 가정과 다르면 **골든 케이스(기대값) → C++ → Python → ADR 0003 표** 순서로 고칩니다. 실응답 fixture로 골든 케이스를 추가하고, 기존 합성 fixture는 회귀용으로 유지합니다.
- KMA 지연이 40분이 아니면 `kUltraNowDelayMin`과 Python `ULTRA_DELAY_MIN`을 함께 바꾸고, `test_kma_release_selection`/`test_kma03_*` 경계값을 갱신합니다.
- 결정이 확정되면 ADR 0003 해당 행을 "확인됨(날짜, 근거 파일)"으로 바꿉니다.

## 4. 완료 기준

- [ ] 표의 모든 항목에 대해 실응답 근거 파일(`evidence/probe/*.md`)이 존재함
- [ ] 실응답 기반 골든 케이스가 제공자마다 2건 이상 추가되고 C++·Python 모두 통과
- [ ] `test_sec01_*` 통과 (raw 디렉터리는 스캔·커밋 제외 확인)
- [ ] `docs/12`에서 P3/P5 Live 칸을 `DONE_VERIFIED_LIVE_API`로 갱신 (확인된 범위만)

## 5. 하지 말 것

- 키를 대화창, 커밋, 이슈, 증거 파일, 명령행에 남기지 않습니다.
- 한도를 시험하려고 반복 호출하지 않습니다(429/쿼터는 자연 발생분만 기록).
- KIS 주문/계좌 API를 호출하지 않습니다(읽기 전용 현재가만).
