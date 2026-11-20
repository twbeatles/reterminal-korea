# Phase Progress Report — KIS 인증정보 추가 및 재빌드

- 날짜·시간(KST): 2026-11-11.
- 작업 단계/티켓: 사용자 요청 KIS App Key/App Secret 문서 저장·개발 헤더 입력·재빌드.
- 커밋 SHA / 공식 기반 commit: 작업 기준 main e9a8561 / Seeed 8d97d8912987c07529287629f7e73039bb0e6384.
- 작업 환경(OS/PlatformIO/보드 패키지 버전): Windows PowerShell / Core 6.2.0 / espressif32@8.0.0. 보드 설정 및 패키지 변경 없음.
- 기기 연결 여부 및 모델 확인: 이번 세션 기기 연결/업로드/초기화 없음. 대상 E1001.
- API 실키 접근: 사용자가 제공한 KIS 자격증명을 로컬 파일에 입력. 실값은 공개 문서에 기록하지 않음.

## 구현 내용

- 변경 파일: Git 제외 `secrets.api-credentials.md`, `include/dev_credentials.h`; 공개 기록은 이 보고서와 validation.txt.
- 변경 목적: App Key/App Secret 순서를 구분하여 문서와 개발 매크로에 입력하고 펌웨어 재빌드.
- 기존 기능 영향: 코드 변경 없음. Wi-Fi/기존 공급자/지역 유지, KIS OFF·관심종목 없음 유지. 실제 현재가 조회 활성화는 별도 설정 필요. 유효 NVS는 개발 헤더보다 우선한다.

## 재현 가능한 증거

| 구분 | 명령/방법 | 결과 (PASS/FAIL/NOT_RUN) | 검증 단계 (Host/Build/Hardware/LiveAPI) | 증거 경로 |
|---|---|---|---|---|
| 로컬 설정 검증 | Python: 매크로와 문서 값 일치·192바이트 한도·KIS OFF 확인, 값 미출력 | PASS | Host | validation.txt |
| Git 제외·공개 유출 검사 | git check-ignore, 공개 파일/스테이징에서 로컬 실값 검색(값 미출력) | validation.txt 참조 | Host | validation.txt |
| Firmware build | pio run -e reterminal_e1001, 로그는 비공개 경로로 캡처 | PASS: RAM 70,788 B / Flash 1,683,482 B; 18.33초 | Build | validation.txt; Git 제외 build/kis-credential-build.log |
| Clean build | clean 명령 미실행 | NOT_RUN | Build | 증분 빌드 |
| Unit tests | 0개 실행, 실패 수 해당 없음 | NOT_RUN | Host | 설정 입력만 변경 |
| Fault injection / Host preview | 미실행 | NOT_RUN | Host | 기존 결과 재사용 |
| E1001 smoke | 미실행 | NOT_RUN | Hardware | BLOCKED_HW |
| Live API | 토큰 발급/시세 호출 미실행 | NOT_RUN | LiveAPI | 키 저장과 유효성 검증 구분 |
| Sleep/wake | 미실행 | NOT_RUN | Hardware | BLOCKED_HW |

## 독립 검증 및 보안

- 단위/포맷/환율 기준일 Oracle: 로직 변경 없음.
- Mutation 테스트 결과: NOT_RUN.
- 자격증명 유출 검사 범위/결과: 로컬 파일 제외 확인 및 Git 추적 파일/신규 보고서·검증 기록/스테이징에서 실값 미포함 검사. 값/토큰 미출력. 세부 결과는 validation.txt.
- 사용한 API 출처와 기준일 검증: 사용자 제공 값을 KIS로 기록했다. 실전·모의 구분/만료/승인/호출 한도는 미확인. 최신 계약이나 실응답을 새로 검증했다고 주장하지 않는다. 공식 LVGL 설정 및 기존 docs/10·이전 증거 경계 유지.
- 바이너리에는 자격증명이 포함될 수 있으므로 빌드 산출물과 원본 로그는 Git 제외 로컬 경로에 보관한다.

## 미완료/블로커

- BLOCKED_HW: 기기 접근/업로드/실제 Wi-Fi·화면·절전 확인 미실행.
- BLOCKED_CREDENTIALS: KIS 키 미보유 블로커는 해소. 실제 인증/현재가 수신은 미검증. 계좌·주문 기능 추가 없음.
- BLOCKED_NETWORK: 실 API 호출 미요청/미실행. 네트워크 장애로 판정하지 않음.
- 기타 이슈: 관심종목 미설정·KIS OFF이므로 현재가 조회는 아직 비활성. 기존 양천구 대기질 미표출 공지는 이전 보고서 참조.
- Git remote 없음: 비밀 없는 진행 기록만 main에 커밋, push 대상 없음.

## 게이트 판정

- 이번 단계 목표 PASS/FAIL/PARTIAL: 로컬 입력·펌웨어 빌드 PASS. 실기·실 API 완료 아님. 이후 사용자가 실제 플래싱을 요청했으므로 해당 실행 결과는 별도 보고서로 기록한다.
- 다음 단계에서 해야 할 일: 관심종목/실전 서비스 지원 확인 후 필요 시 KIS 활성화. 기기 업로드는 별도 사용자 지시에 따라 진행.
- 실기 없이 성공이라고 표시하지 않은 항목: 실제 키 유효성/토큰/현재가/기기 플래시/NVS 반영/TLS/화면/저전력.
