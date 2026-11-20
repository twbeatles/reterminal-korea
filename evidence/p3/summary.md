# Phase Progress Report — 감사 조치 + 실 API 연동 경로 (Host 검증)

- 날짜·시간(KST): 2026-11-02
- 작업 단계/티켓: PROJECT_AUDIT ISSUE-001~010, G-1(API 경로 부분)·G-4~G-8·G-10·G-13, P2-01(전송), P3 어댑터, P5 KIS/FSC 연동
- 커밋 SHA / 공식 기반 commit: 커밋 없음 (사용자 승인 전 커밋 금지) / upstream `8d97d891`
- 작업 환경: Windows 11, Python 3.15.1, pytest 9.2.0, PlatformIO Core 6.2.0, espressif32@8.0.0 (Arduino-ESP32 3.3.x), xtensa g++ 14.2.0, zig 0.16.0 (clang 21.1, `pip install --user ziglang`, 개발 도구로만 사용)
- 기기 연결 여부 및 모델 확인: 미연결 (실기 없음)
- API 실키 접근: 없음. 공개 페이지 열람 + TLS 체인 확인만 수행. 키는 사용하지 않았고 기록도 하지 않음.

## 구현 내용

- 변경 파일 (주요):
  - 신규: `src/providers/{ResultCodes,ProviderJson}.*`, `src/app/Fetcher.*`, `src/net/{HttpTransport.h,CaBundle.h}`, `include/{IRandom.h,dev_credentials.example.h}`
  - 신규 도구: `host/kdash/{codes,secret_scan}.py`, `tools/{run_native_tests,gen_ca_bundle}.py`
  - 신규 테스트 자료: `test/golden/provider_cases.json`, `host/tests/{test_golden,test_secret_scan}.py`
  - 신규 문서: `docs/adr/0003-live-api-integration.md`
  - 재작성: `src/providers/*Parser*`, `src/app/Coordinator.*`, `src/net/{NetPolicy,Endpoints,HttpTransport}.*`, `src/storage/SettingsStore.*`, `src/provision/SetupPortal.*`, `src/main.cpp`, `include/Config.h`, `src/domain/{Types.h,Freshness.*}`, `host/kdash/{kma,airkorea,exim,finance,endpoints,policy}.py`, `test_native/test_core.cpp`, `test/fixtures/*` (실제 응답 형태의 합성 데이터 33종)
  - 설정·문서: `platformio.ini`, `.gitignore`, CI 3종, README, `docs/BUILD.md`, `docs/12`, ADR 0001, `THIRD_PARTY_NOTICES.md`, `PROJECT_AUDIT.md` §10
- 변경 목적: 감사 이슈를 수정하고, 기기가 실제 API를 HTTPS로 호출해 정규화된 스냅샷을 UART로 출력하는 경로를 구현함
- 기존 기능 영향:
  - 파서·설정 API 시그니처가 바뀜(`FetchContext`, 원시 본문 입력)
  - 설정 스키마 v1 → v2(버퍼·키 길이). 저장 백엔드가 아직 없어 마이그레이션 대상 데이터는 없음

## 재현 가능한 증거

| 구분 | 명령/방법 | 결과 | 검증 단계 | 증거 경로 |
|---|---|---|---|---|
| Clean build | `pio run -e reterminal_e1001` | PASS (경고 0, RAM 20.8% 68,196B, Flash 35.6% 1,189,658B) | Build | 콘솔 (`.pio/fw-build2.log`, 비추적) |
| Unit/integration (C++) | `python tools/run_native_tests.py` | PASS 19/19, 공통 골든 40/40 | Host | 콘솔 |
| Unit (Python) | `python -m pytest host/tests -q` | PASS 90/90 (골든 40 parametrized 포함) | Host | 콘솔 |
| 로컬 CI | `scripts/ci_local.ps1` | PASS (host→preview→sanitizer→native→firmware) | Host/Build | 콘솔 |
| Fault injection | XML 200 오류(30/22), HTML 점검 페이지, NODATA, EXIM RESULT 3/4, KIS EGW00123(HTTP 500)/EGW00201/EGW00133(403), 빈 `items:""`, 단일 객체 item, `24:00`, 미래 시각, 시계 미검증 | PASS | Host | `test/golden/provider_cases.json` |
| Mutation | KMA 슬롯 선택을 "가장 먼 슬롯"으로 되돌림 | KILLED (golden 2건 실패) → 원복 | Host | 본 보고서 |
| TLS 체인 | `python tools/gen_ca_bundle.py --verify` | 3/3 OK (data.go.kr→GlobalSign R3, koreaexim→DigiCert G2, KIS:9443→USERTrust RSA) | Network(공개) | `src/net/CaBundle.h` 헤더 |
| Host preview | `python tools/generate_preview.py` | 10장 재생성 | Host | `evidence/p1/host-preview/` |
| E1001 smoke | — | NOT_RUN | BLOCKED_HW | — |
| Live API | — | NOT_RUN | BLOCKED_CREDENTIALS | — |
| Sleep/wake | — | NOT_RUN (deep sleep 미배선) | BLOCKED_HW | — |

## 독립 검증 및 보안

- 단위/포맷/환율 기준일 Oracle:
  - C++과 Python이 같은 원시 fixture와 같은 기대값 표(`test/golden/provider_cases.json`)로 실행됨
  - EXIM 고정소수점은 Decimal oracle로 대조
- Mutation 테스트 결과: KMA 슬롯 뮤턴트 KILLED
- 자격증명 유출 검사 범위/결과:
  - SEC-01 스캔이 `.log/.txt/.xml/.html/.ini/.csv`까지 검사함. 쿼리·헤더·JSON 키·Bearer/JWT를 탐지하며 결과 0건
  - sanitizer 회귀 6형태(BOM 포함) PASS
  - UART에는 라벨(등록됨/미등록)만 출력되고, URL·키·토큰은 PassLog에 기록되지 않음
- 사용한 API 출처와 기준일 검증:
  - data.go.kr 15084084(2026-10-19 수정본)에서 초단기실황 base_time 정시를 확인
  - koreaexim 안내 페이지는 TLS 오류로 열람하지 못함 → RESULT 코드는 보수적으로 분류(ADR 0003 D4)

## 미완료/블로커

- BLOCKED_HW: UART 부팅, Wi-Fi/NTP, TLS 실핸드셰이크, ePaper, 버튼, SHT40, deep sleep, 배터리
- BLOCKED_CREDENTIALS: U-05(KMA 제공 지연 40분 출발값), U-06(AirKorea `stationName`·`24:00` 실응답), U-08(KIS rt_cd/EGW 코드·App Secret 길이), U-09(FSC 경로·정렬), EXIM RESULT 코드 원문
- BLOCKED_NETWORK: 없음
- 기타 이슈 (재현/회피책):
  - PlatformIO native는 Windows에서 gcc가 없어 실행할 수 없음 → `tools/run_native_tests.py`(zig)로 같은 소스를 검증. CI(python:3.12, gcc 포함)에서는 `pio test -e host_native`로 실행
  - 미구현으로 남은 범위: 렌더러, NVS, 포털 서버, 원자 쓰기 (`docs/12` 남은 갭)

## 게이트 판정

- 이번 단계 목표: PARTIAL. 감사 이슈 10건과 API 연동 경로는 DONE_VERIFIED_HOST/DONE_BUILD_ONLY이고, 실기·실키 항목은 BLOCKED
- 다음 단계에서 해야 할 일:
  1. `include/dev_credentials.h`에 본인 키를 넣고 실기 UART로 provider별 `outcome=OK` 확인(`docs/BUILD.md` 3절), `API_PROBE_RECORD` 작성
  2. 실응답 fixture를 sanitizer로 정리해 골든 케이스에 추가
  3. 렌더러 → NVS/포털 → deep sleep 순으로 배선
- 실기 없이 성공이라고 표시하지 않은 항목: 실제 TLS 접속, 실 API 응답 스키마, UART 출력, 전력·수면, 화면
