# Phase Progress Report — 로컬 인증정보 입력

- 날짜·시간(KST): 2026-11-11.
- 작업 단계/티켓: 사용자 제공 인증키 정리 및 로컬 개발 설정 입력. 구현 티켓 변경 없음.
- 커밋 SHA / 공식 기반 commit: 작업 기준 main f10c3ba / Seeed 8d97d8912987c07529287629f7e73039bb0e6384.
- 작업 환경(OS/PlatformIO/보드 패키지 버전): Windows PowerShell / Core 6.2.0 / espressif32@8.0.0. 패키지 변경 없음.
- 기기 연결 여부 및 모델 확인: 사용자가 E1001 보유를 알림. 이번 세션 실제 연결/업로드/초기화 없음.
- API 실키 접근: 사용자 제공 값을 로컬 비공개 파일에 입력. 값 자체는 이 보고서에 기록하지 않음.

## 구현 내용

- 변경 파일: Git 제외 `secrets.api-credentials.md`, `include/dev_credentials.h`; 추적 대상은 이 보고서와 validation.txt뿐.
- 변경 목적: 공공데이터포털 공통키와 별도 EXIM 키, Wi-Fi, 지역을 로컬 문서/개발 헤더에 입력.
- 완료 범위: KMA/Air/EXIM 입력, 특일/FSC의 기상청 키 공유 확인, Wi-Fi 입력, 공식 격자 기반 지역 설정. FSC/KIS는 기존 기본 OFF·관심종목 없음 유지.
- 기존 기능 영향: 소스 코드 변경 없음. 다음 로컬 빌드에는 인증정보가 포함될 수 있어 산출물을 공개하지 않는다. 정상 NVS는 개발 헤더보다 우선한다.

## 재현 가능한 증거

| 구분 | 명령/방법 | 결과 (PASS/FAIL/NOT_RUN) | 검증 단계 (Host/Build/Hardware/LiveAPI) | 증거 경로 |
|---|---|---|---|---|
| 설정 매핑 | Python 로컬 파일 값 일치/필수값 존재/한글 인코딩 검사, 값 미출력 | PASS | Host | validation.txt |
| 비공개 제외 | git check-ignore include/dev_credentials.h secrets.api-credentials.md | PASS | Host | validation.txt |
| 격자 출처 | 공식 2610 ZIP의 XLSX sharedStrings/worksheet XML 읽기 | PASS: 목1~5동 행 241~245 F/G=58/126 | Host source | 아래 공식 링크; 로컬 build/official-kma-2610/ |
| Firmware build | pio run -e reterminal_e1001 (비공개 로그로 캡처) | PASS: RAM 70,788 B / Flash 1,683,250 B; 131.87초 | Build | validation.txt; 원본 로그는 Git 제외 build/credential-build.log |
| Clean build | clean 명령 미실행 | NOT_RUN | Build | 증분 빌드와 구분 |
| Unit tests | 신규 테스트 0개, 기존 suite 재실행 안 함 | NOT_RUN | Host | 설정 입력 작업 |
| Fault injection / Host preview | 미실행 | NOT_RUN | Host | 기존 증거 유지 |
| E1001 smoke / Sleep/wake | 미실행 | NOT_RUN | Hardware | BLOCKED_HW |
| Live API | 미실행 | NOT_RUN | LiveAPI | 키 입력과 유효성 검증 구분 |

## 독립 검증 및 보안

- 단위/포맷/환율 기준일 Oracle: 공급자 종류 및 키 매핑만 확인. 날짜/JPY(100) 로직 변경 없음.
- Mutation 테스트 결과: NOT_RUN.
- 자격증명 유출 검사 범위/결과: 로컬 파일 Git 제외 여부 및 추적 대상 파일에 로컬 실키/Wi-Fi 값이 포함되지 않는지 값 미출력 검사. 결과는 validation.txt.
- 사용한 API 출처와 기준일 검증: 승인/활용기간은 사용자 제공 화면 기준(공공데이터포털 4종 2026-11-11~2028-11-11). 개인별 쿼터/EXIM 만료·활성 상태·실키 유효성은 미검증.
- 기상청 [공식 API 상세](https://www.data.go.kr/data/15084084/openapi.do)에서 [2610 첨부 ZIP](https://www.data.go.kr/cmm/cmm/fileDownload.do?atchFileId=FILE_000000007732424&fileDetailSn=1)을 다운로드하여 목1~5동 모두 동일한 58/126임을 확인했다. 참고 파일은 build 아래 보관하고 편집하지 않았다.
- [에어코리아 2026-11-04 공식 공지](https://m.airkorea.or.kr/info/notice): 양천구 측정소 2026-10-29~11-30 장비 교체/시험가동으로 미표출 예정(일정 변경 가능). 이를 로컬 문서에 명시하고 다른 측정소로 임의 대체하지 않았다.

## 미완료/블로커

- BLOCKED_HW: 실제 USB 업로드, Wi-Fi 연결, 패널·포털·wake·전류·복구.
- BLOCKED_CREDENTIALS: KIS 미제공(선택 기능). KMA/Air/EXIM 및 공유키 수령·입력은 완료했으며 해당 항목의 키 미보유 블로커는 해소. 공급자 실통신 검증은 별도 미완료.
- BLOCKED_NETWORK: 공개 자료 다운로드 성공. 실제 인증 API 호출은 하지 않음.
- 기타 이슈: 양천구 대기질 미표출 공지로 실측값 결측 예상. 실제 응답 검증 전 장애 원인을 키 오류로 단정하지 않는다.
- 원격: Git remote 없음, push 대상 없음. 비밀 파일은 commit 대상에서 제외.

## 게이트 판정

- 이번 단계 목표 PASS/FAIL/PARTIAL: 로컬 문서 및 설정 입력 PASS; 실제 기기 적용/실 API 검증은 미완료.
- 다음 단계에서 해야 할 일: 실제 COM 식별 후 사용자 지시에 따라 업로드, config source 및 Wi-Fi/NTP/HTTPS/공급자별 결과 확인. 기존 NVS이면 포털에서 설정 수정.
- 실기 없이 성공이라고 표시하지 않은 항목: flash/NVS 등록/키 유효성/실제 Wi-Fi·API·TLS/패널/저전력.
