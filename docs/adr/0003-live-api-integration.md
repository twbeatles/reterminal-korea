# ADR 0003 — 실 API 연동 경로 확정값과 스펙 대비 변경

- 상태: 채택
- 날짜: 2026-11-02 (KST)
- 관련: `PROJECT_AUDIT.md` ISSUE-001~010, G-4~G-8 / `docs/03`, `docs/04`, `docs/06`

## 배경

감사 결과, 기기에서 실제 API를 부르는 경로(전송·JSON 어댑터·오케스트레이션)가 없었습니다. 파서도 실제 응답 형태와 맞지 않는 곳이 여러 군데 있었습니다. 이번 작업에서 그 경로를 구현했고, 그 과정에서 문서(입력 패키지 `docs/00~10`)와 다르게 정한 값을 여기에 기록합니다. 입력 패키지 자체는 수정하지 않습니다.

## 결정

| # | 항목 | 이전 | 결정 | 근거 / 확인 방법 |
|---|---|---|---|---|
| D1 | KMA 초단기실황 base_time | `HH30`, 지연 15분 | **`HH00`(정시)**, 지연 출발값 **40분** | data.go.kr 15084084 (2026-10-19 수정본) 요청 예시 "0600 / 06시 발표(정시단위)". 40분은 커뮤니티 구현 관행으로, 공식 페이지에 명시가 없어 **U-05 미해소**. NODATA(03)면 직전 회차로 1회 재요청 |
| D2 | KIS 호스트 | `apiportal.koreainvestment.com`, `oauth2.koreainvestment.com` | **`openapi.koreainvestment.com:9443`** (REST + `/oauth2/tokenP`) | 2026-11-02 `openssl s_client` 접속·체인 검증. apiportal은 개발자 포털 웹사이트 |
| D3 | KIS 성공 판정 | `result_code=="00"`, 401=만료 | **`rt_cd=="0"`**. 토큰 만료 `EGW00123/EGW00121`(HTTP 500 포함), 초당 제한 `EGW00201`, 토큰 발급 1분 제한 `EGW00133`(HTTP 403) | open-trading-api 공개 샘플의 응답 규약. **U-08: 실키로 재확인 필요** |
| D4 | EXIM `RESULT≠1` | 행 무시 → 휴일로 간주(최대 7일 재요청) | **오류**로 분리(3=인증, 4=한도, 그 외=ProviderError). walk-back 중단 | EXIM 공식 페이지는 TLS 오류로 이번에 열람하지 못함. 코드 의미가 다르더라도 "휴일로 오인하지 않는다"는 쪽이 안전 |
| D5 | data.go.kr 키 형식 | Encoding 키만 그대로 삽입 | **Encoding/Decoding 키 모두 허용**(`%`가 있으면 그대로, 없으면 1회 인코딩, 섞이면 거부) | 포털이 두 형식을 함께 발급하며, Decoding 키의 `+`는 쿼리에서 공백으로 해석됨 |
| D6 | 키 최대 길이 | 128 | **256** (버퍼 257) | KIS App Secret은 약 180자로 알려짐(U-08 확인 필요) |
| D7 | 설정 문자열 상한 | 문자 수(Python)와 바이트(C++)가 서로 다름 | **UTF-8 바이트** 기준, 버퍼는 +1 | ISSUE-007 |
| D8 | 루트 CA | 미정 | **GlobalSign Root R3 / DigiCert Global Root G2 / USERTrust RSA** 고정 (`src/net/CaBundle.h`, `tools/gen_ca_bundle.py --verify`) | 2026-11-02 세 호스트 모두 체인 완전, 루트 일치 확인. GlobalSign R3 만료는 2029-03-18 |
| D9 | AirKorea 측정소 일치 | Python: `stationName` 필수 / C++: 없으면 아무 행이나 | **있으면 반드시 일치, 없으면 허용**(측정소별 요청이므로). 최신 시각이 `-`이면 직전 시각 값을 그 시각과 함께 사용 | U-06 미해소 — 실응답으로 확인 필요 |
| D10 | AirKorea `24:00` | 형식 오류 처리 | **익일 00:00**으로 정규화 | 측정소 실시간 자료의 자정 표기 관행. 저장소에 실응답 fixture가 없어 실키 확인 대상 |
| D11 | FSC 키·조회창 | 미정 | data.go.kr 계정 키 공용(`kma_key`). `likeSrtnCd` + `beginBasDt=오늘-14일`, 최신 `basDt` 채택 | U-09 미해소 |
| D12 | 개발용 자격정보 | — | `include/dev_credentials.h`(git 제외)로만 주입. NVS·포털은 다음 단계 | AGENTS.md: 키를 커밋·채팅·로그에 남기지 않음 |

## 영향

- 위 값은 모두 `test/golden/provider_cases.json`(C++·Python 공통)으로 고정했습니다. 실키로 다르게 확인되면 **골든 케이스 → 구현 → 이 ADR** 순서로 함께 고칩니다.
- U-05, U-06, U-08, U-09는 실키 probe(`templates/API_PROBE_RECORD.md`) 전까지 BLOCKED_CREDENTIALS로 남습니다.

## 되돌리는 방법

각 결정은 `src/net/Endpoints.cpp`, `src/net/NetPolicy.h`, `src/providers/*`, `include/Config.h`의 상수 한 곳에 모여 있습니다.
