# 03. 한국형 데이터 API 실연동 계약

## 2026-11-11 실연동 후속

- KMA 실황/단기예보·Air의 인증된 PC 응답과 기본 단위/회차·격자를 확인했다. 양천구 Air 응답은 내부 성공코드 00이나 PM10/PM2.5는 통신장애 `-`이므로 결측 표시를 유지한다.
- 공공 API가 HTTP/1.0 요청에도 chunked 응답할 수 있음을 확인했다. framing을 제거한 완전한 본문만 JSON 어댑터로 전달하며 기존 회차/격자 검증은 유지한다(ADR 0011).
- FSC 현행 조회: `https://apis.data.go.kr/1160100/GetStockSecuritiesInfoService_V2/getStockPriceInfo_V2`. 2026-10-28 공식 Swagger 및 실제 005930 응답으로 확인했다. 종가와 원본 기준 거래일을 유지하며 실시간 현재가와 구분한다.
- EXIM 오늘/전일 인증 PC 응답과 기기 고시환율 수신을 확인했다. JPY(100)는 100엔당 원화다. 공급자별 실행/미검증 경계는 [실 API 통합 보고](../evidence/live-api-integration/summary.md)를 따른다.

## 2026-11-05 정규화 경계 보강

- KMA의 모든 성공 항목은 요청한 baseDate/baseTime과 일치해야 하며 nx/ny가
  유효하고 응답 내에서 일관돼야 한다. 실제 조회는 요청 격자와의 일치도 확인한다.
  원본 메타데이터가 누락되거나 다르면 마지막 정상 캐시/성공 회차를 갱신하지 않는다.
- AirKorea는 순서에 의존하지 않고 PM별 최신 유효값을 선택한다. 각 observed_at을
  보존하고 오래된 값의 결측 보완을 화면에서 구별한다. 결측 '-'는 0이 아니다.
- 최신 공급자 계약/승인키 실응답 검증을 대체하는 변경은 아니다. ADR 0010과
  evidence/audit-followup/summary.md의 Host/Build 및 미검증 경계를 따른다.

> 2026-11-01 기준 공식 포털·샘플로 확인된 URL/구조와 **실제 키·장치로 아직 검증하지 않은 부분을 명확히 구분**한다. 에이전트는 각 provider 구현 전 공식 문서의 수정일·유효 Endpoint·에러코드 확인, **키 없이도 가능한 fixture 테스트**부터 만든다.

## 공통 API 구현 규정
- `IHttpClient`를 주입하고 DNS/TLS/Timeout/HTTP/Body/Schema/BusinessError 분리.
- 모든 URL에 `https://`; 인증키는 로그 출력하지 않고, URL query 인코딩은 **한 번만** 적용.
- 응답 JSON vs XML vs HTML(서비스 장애)을 실제 `Content-Type` 및 body 검사로 분기. HTTP 200이어도 내부 resultCode 실패면 실패.
- Streaming/필터 파싱으로 메모리 사용을 제어. body max length 문서화·실측, 최대 크기 초과는 안전 중단.
- 실제 응답 fixture 수집 시 `scripts/fixture_sanitizer`로 URL/토큰/계정·개인정보 제거. `verified_source_record.json`에 HTTP status, 수집 시각, 기준시각, 결과코드, 응답 필드 해시 저장.

## A. 기상청 단기예보 (P3 필수)
- 공식: https://www.data.go.kr/data/15084084/openapi.do (2026-10-19 수정 기준)
- 개발 URL: `https://apis.data.go.kr/1360000/VilageFcstInfoService_2.0/getUltraSrtNcst`
- 예보 URL: `https://apis.data.go.kr/1360000/VilageFcstInfoService_2.0/getVilageFcst`
- 주요 Query: `serviceKey`, `pageNo=1`, `numOfRows`(모든 필요 항목을 받을 만큼), `dataType=JSON`, `base_date=YYYYMMDD`, `base_time=HHmm`, `nx`, `ny`.
- 내부 header `resultCode=="00"`이며 `items.item[]` 존재 확인. 초단기실황 `T1H`(기온 °C), `REH`(상대습도 %), `RN1`(강수량) 등; 단기 `TMP`,`POP`,`PTY`,`SKY` 등. 항목은 API 상품·시간 타입별로 달라 **실제 가이드**와 맞추어야 함.
- **발표시각 스케줄**은 호출 당시 최신 발표를 단순 현재시각으로 계산하지 않음. 공식 2026-10 API 가이드의 발표·전달 지연 표를 추출해 `ReleaseCalendarKma`로 구현하고, 이미 제공된 회차만 요청. 자정 넘어 base_date 이전 날짜 테스트 필수.
- 위치: 초판은 서울 대표값 예시를 안전한 **fixture 격자(nx,ny)** 로 시드하지만 임의 격자 산출 금지. 공식 행정동 격자 파일 기반 설정/동선택; 정보 없으면 설정 페이지에 지역 설정 필요 안내.
- 필수 fixture: 정상, 발표 전 빈 목록, 일부 category 결측, 자정, XML error with HTTP200, 서비스키 미인식, `-`값.

## B. 에어코리아 대기오염 (P3 필수)
- 공식: https://www.data.go.kr/data/15073861/openapi.do (2026-06-30 수정)
- root: `https://apis.data.go.kr/B552584/ArpltnInforInqireSvc`
- 예상 operation `getMsrstnAcctoRltmMesureDnsty` (측정소별 실시간 조회). 실제 계약의 operation/query(`stationName`, `dataTerm`, `pageNo`, `numOfRows`, `returnType=json`, `serviceKey`, `ver`)는 인증된 호출 전 공식 문서 대조하고 확정.
- 결과 `pm10Value`, `pm25Value`, `dataTime`, 등급 필드가 있는지 실제 응답으로 검증. 결측 `-`/공백/점검은 `Missing`이며 절대 0 취급 금지.
- 지역이 바뀌면 측정소 코드를 재매핑; 지역 대표 측정소 선택의 근거와 동선 변경 테스트 포함.
- 출처명/측정시각/관측소명 표시. 운영계정 심의·출처표시/변경금지 이용 조건 준수.

## C. 한국수출입은행 환율 (P3 필수)
- 공식: https://www.data.go.kr/data/3068846/openapi.do
- 신규 host: `oapi.koreaexim.go.kr`, 이전 `www.koreaexim.go.kr` 경로 사용하지 않음.
- 예시: `GET https://oapi.koreaexim.go.kr/site/program/financial/exchangeJSON?authkey={secret}&searchdate=20261030&data=AP01`
- `{secret}`은 구성 페이지에서 입력하고 로그·픽스처에 넣지 않음.
- 구조(확인 대상): 배열 내 `CUR_UNIT`, `DEAL_BAS_R`, `RESULT` 등. 숫자 `'1,234.56'`의 쉼표 제거, 통화 단위 유효성 검사.
- `USD`는 KRW/1 USD, `JPY(100)`이면 KRW/100 JPY. 원/엔 환율을 정확한 단위로 표시; 환산할 경우 변환 식을 표시.
- 빈 배열은 휴일·비영업일·미발표 가능 → 이전 영업일 검색 상한(7 calendar days, 1일당 최대 한 번 요청), 마지막 결과의 **기준일**을 보존.
- '고시 환율' 레이블. 1분마다 호출하거나 초단위 FX로 소개 금지.

## D. 금융위원회 주식시세정보 (P5 optional)
- 공식: https://www.data.go.kr/data/15094808/openapi.do
- **다음 영업일 13시 이후** 데이터 제공 안내가 있으므로 장중 거래가격으로 표시하지 않는다.
- Provider는 단순 종가·기준일 조회로만 구현. API operation path는 실제 문서/인증 응답 확인 전 문자열 확정 금지.
- 관측시각은 `거래일`, 수신은 `fetch_at`이며 UI에 `전일 종가(기준일: YYYY-MM-DD)` 등을 명시.
- 호출량·조건을 검토하여 지역·기간·종목코드 필터를 적용. 장기 시세 이력을 E1001에 전량 다운로드하지 않음.

## E. 한국투자증권 국내주식 현재가 (P5 optional, Credentials needed)
- 개발자센터 https://apiportal.koreainvestment.com/
- 공식 샘플 https://github.com/koreainvestment/open-trading-api
- **조회** REST 예시 `GET /uapi/domestic-stock/v1/quotations/inquire-price`; `tr_id=FHKST01010100`, `FID_INPUT_ISCD=005930`(삼성전자 예시), 시장분류는 샘플의 `J`(KRX), `NX`(NXT), `UN`(통합) 중 서비스/계좌 지원값 확인.
- 인증: `POST /oauth2/tokenP`로 앱키·시크릿 기반 토큰 발급 사례. **호스트·유효기간·재발급 횟수 제한·응답 스키마는 최신 개발자센터 및 샘플과 실제 호출로 검증**.
- 읽기 전용 단일가 조회만; 주문/체결/계좌·개인정보 처리 파일 import 금지. 샘플의 주문 코드/인증정보 복사 금지.
- 시장 시간표 (KRX/NXT)와 `live` 여부는 API 응답 속성과 실제 timestamp를 바탕으로 표시; 초 단위 체결 스트림이 아닌 **REST 호출 시점 현재가**로만 표현.
- 금융 데이터 약관 및 제3자 재배포 제한 확인. 사용자 본인 소유 기기에만 표시하는 구조.

## 추가 선택지
- 자격증명 미발급이어도 첫 실행 데모가 가능하도록 `DemoProvider`를 제공하되 화면에 크게 `DEMO` 표기.
- 키 없이 동작해야 하는 초기 확인을 원하면 Open-Meteo의 비상업 개인사용 정책을 확인한 뒤 **KMA 대체 옵션**으로 추가할 수 있지만, 기본 한국 공공 API provider와 뒤섞어 출처가 불명확해지지 않게 할 것.

## 완료 기준 (API 1개당)
1. 명세 URL 및 수정일 확정 → 2. 승인/이용 조건 기록 → 3. 실패/정상 fixture 확보 → 4. Host 단위·통합 테스트 → 5. 진짜 키로 한 번 실호출(가능할 때) → 6. E1001 실기 HTTPS 및 메모리 확인(가능할 때) → 7. 로그 마스킹 및 데이터 시각 검증. 5~6이 불가하면 BLOCKED를 남기며 완료 주장 금지.
