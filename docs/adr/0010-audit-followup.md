# ADR 0010 — 원본 시각·실패 표시·실행 포트·watchdog

- 상태: 채택 (Host/Build 검증; Hardware/LiveAPI는 별도)
- 날짜: 2026-11-05 KST
- 질문: 감사 ISSUE-004~006과 런타임/회귀 검증 공백을 어떻게 해결하는가.

## 선택과 영향

1. KMA JSON 경계에서 모든 항목의 baseDate/baseTime과 유효한 nx/ny를 검증한다.
   실제 Fetcher는 요청 격자도 전달한다. 다른/혼합/누락 회차·격자는 성공 캐시에
   적용하지 않는다. NoData/인증·쿼터 응답은 기존 결과코드를 보존한다.
   순수 수치 파서 RawItem은 정규화 경계 뒤의 내부 계약으로 유지한다.
2. AirKorea는 응답 순서와 무관하게 오염물질별 최신 유효값을 선택한다.
   결측을 0으로 만들지 않는다. 각 관측시각을 별도 표시하며 최신 행보다
   오래된 값은 마지막 정상값 배지로 구별한다. 최신 행만 사용해 과거 유효값을
   버리는 대안보다 정보 보존을 선택했다.
3. 배지는 설정 필요 → 인증 확인 → 최근 수집 실패(값이 있으면 마지막 정상값,
   없으면 확인 필요) → TTL 상태 순이다. 실패가 TTL 안에 숨어서는 안 된다.
   수치/원본 시각은 유지하고 정상 수집 시 실패 상태를 해제한다.
   서로 다른 시각의 PM 값에는 공통 측정시각 하나를 붙이지 않는다.
4. 유효한 더 최근 FSC 기준일이 있고 KIS 캐시가 Stale일 때만 종가로 fallback한다.
   정상 KIS 조회가나 종류가 다른 데이터를 임의의 최신가로 재명명하지 않는다.
5. RunCycle은 ICyclePlatform으로 시계·RF·센서·표시·저장·절전을 주입한다.
   E1001과 Host가 같은 orchestration을 사용한다. HTTP ready/deadline은
   IHttpClient 계약에 포함해 concrete cast를 제거한다.
   화면 실패 시 재시도하며 RTC에 성공 CRC가 있는 것처럼 기록하지 않는다.
   NVS 체크포인트의 정상 생략과 실제 실패는 구별해 실제 실패만 로그로 남긴다.
6. 로컬 Arduino-ESP32 3.3.12/ESP-IDF esp_task_wdt.h와 sdkconfig를 확인했다.
   이미 초기화된 SDK watchdog을 90초로 reconfigure하고 현재 loopTask를 등록한다.
   SDK 설정의 idle task mask를 유지한다. 이는 기존 idle timeout 5초를 90초로
   늘리는 전역 변경이다. 네트워크/패널의 정상 blocking 구간을 수용하기 위한 선택이며,
   실기에서 SDK idle 보호에 미치는 영향을 재확인해야 한다.
   Feed는 실제 루프/주기 단계/포털 진행에서만 한다. 별도 타이머 task가 막힌
   앱을 대신 feed하지 않는다. 10분 포털은 정상 진행 시 허용하고 멈추면 감독한다.
   End는 앱 task 구독만 해제하며 SDK 전체 watchdog을 deinit하지 않는다.
7. 60초 네트워크 작업 예산, 요청 timeout, 본문 deadline, 패널 30초 제한과
   task watchdog은 별도 계층이다. 전체 작업이 정확히 60초 안에 끝난다고
   주장하지 않는다. DNS/header/TLS 및 전류의 실측 상한은 BLOCKED_HW다.
8. Git에 게시된 v1 설정 wire contract가 없어 가상의 migration을 만들지 않는다.
   현재 v2는 검증된 원자적 no-op migration을 제공하고, 미지원/손상 버전은
   호출자 상태를 변경하지 않고 거부한다. 실제 새 스키마를 설계할 때 이전
   codec/fixture/rollback을 함께 제공해야 한다.
9. Windows/Linux/macOS Host CI matrix를 추가했다. 실제 다른 OS runner 실행은
   외부 CI 증거가 생길 때만 완료 처리한다. 픽셀 parity 대신 독립 기대값을
   가진 ViewModel 회귀와 실제 C++ 래스터/배지·측정시각 시각 검수를 사용한다.

## 보류·대안

- KRX 전체 휴장일/NXT·통합 거래시간은 공식 계약과 실응답 확인 전 BLOCKED다.
  공휴일 테이블을 거래소 검증 테이블로 승격하지 않는다.
- 실제 패널/절전/전류/TLS/USB 복구, 자동 OTA, 32MB 파티션/eFuse는 이번 작업에서
  실행·활성화하지 않는다. 각각의 기존 ADR/실기 게이트를 따른다.
- KMA 파서에 메타데이터 없는 응답을 허용하는 호환 모드는 두지 않는다.
  따라서 오래된 수치 전용 합성 fixture는 원본 회차·격자를 채워야 한다.

## 근거·검증·되돌리기

- PROJECT_AUDIT.md §4, evidence/audit-followup/summary.md, test_audit_followup.inc.
- 72시간 테스트의 fake API는 요청한 회차를 반영하도록 수정했다. 이전처럼
  고정된 하루의 응답을 모든 날에 최신 값으로 승인하게 만들지 않는다.
- 되돌리기: 이 ADR과 관련 변경을 하나의 수정 단위로 되돌리되 감사 이력은 보존한다.
  원본 시각 검증을 약화해 기존 테스트를 통과시키는 방식은 택하지 않는다.
