# 09. 단계별 실행 티켓 — 에이전트 순차 구현용

> 아래 단계는 **의존관계가 있는 게이트**이다. 가능한 한 스몰 PR/커밋으로 나누고, 각 단계가 통과하지 않으면 그 단계 결함을 우선 해결한다. 실기/키 부족은 `BLOCKED` 보고만 하고 Host 가능한 후속 작업은 계속.

## P0 — 공식 베이스라인 (필수)
- `P0-01` 신규 저장소 구조 생성, AGENTS.md 고정, 빌드 문서 작성.
- `P0-02` Seeed 공식 repo/커밋 확인, `pio run -d examples/official/LVGLePaperStatusPanel -e reterminal_e1001` 실행·증거.
- `P0-03` GxEPD2/Seeed_GFX/LVGL 선택 비교, 드라이버 라이선스·pins·partition 확인.
- `P0-04` 자체 프로젝트의 E1001 대상 main, Host fake target, 컴파일 스크립트 구성.
- 게이트: `pio run -e reterminal_e1001` 재현 성공 또는 환경별 구체적인 실패로그, 하드웨어 미보유 시 동작 주장 금지.

## P1 — 데이터 모델·한글 UI·Host preview (필수)
- `P1-01` `IClock`, `IHttpClient`, `IDisplay`, Domain DataPoint/Clock/QuoteKind 설계.
- `P1-02` 800×480 생활·금융·기기 페이지 Mock 렌더러 구현. 한글 subset/fallback.
- `P1-03` 데이터 신규/결측/지연·오류/주말과 장중 구분 표시.
- `P1-04` 골든 스크린샷 최소 10종 + 넘침 검사 + 버튼 논리 테스트.
- 게이트: API/실기 없이 Host 테스트/프리뷰 가능, Mock 화면에는 `DEMO`, 800×480 정확.

## P2 — 통신·기기 설정·보안 (필수)
- `P2-01` Wi-Fi 연결, 시간동기화 및 TLS 인증검증 운용.
- `P2-02` E1001 자체 AP 설정 페이지(네트워크/지역/키), 접근제어·타임아웃·입력 검증.
- `P2-03` 설정 NVS+마이그레이션·캐시 CRC/atomic writes·비밀정보 redaction.
- `P2-04` Retry budget, 429, DNS, TLS, 응답 과대 및 전원중단 오류 주입.
- 게이트: Host 네트워크 오류 복구 테스트, 컴파일, 기기 실연결은 별도 HW 상태.

## P3 — 국내 생활 API (MVP 필수)
- `P3-01` KMA 현재 + 단기예보(발표시각 선택, 자정 회귀).
- `P3-02` AirKorea 측정소별 대기정보(결측·관측시각).
- `P3-03` EXIM 달러·100엔 고시환율(영업일 fallback, 라벨).
- `P3-04` SHT40 센서 통합 및 UI 바인딩.
- `P3-05` API source/기준시각/stale 화면 + 개인정보/키 비노출 시험.
- 게이트: 각 Provider Host fixture 5개 이상, 유효성 검증, 지연/결측 UI; 실 API 미검증은 credentials blocker로 별도 표기. 실기 보유 시 디바이스 HTTPS 실동작.

## P4 — Deep Sleep과 장기 안정화 (MVP 필수)
- `P4-01` power-mode USB / battery, RTC 및 버튼 wake; 스케줄 분리.
- `P4-02` 전체/부분 ePaper refresh 최적화, 고스팅 방지.
- `P4-03` 네트워크 재연결 100회, 장치 자동복구/재부팅 보호.
- `P4-04` 실기 24/72h soak, 배터리 소비 계측. 실기 부재 시 TODO + Host clock faults.
- 게이트: 실기 없는 경우 P4 상태 `BLOCKED_HW`; Host 상태로 릴리스 '실기 검증' 주장 금지.

## P5 — 금융 기능 (선택적, MVP 이후)
- `P5-01` FSC 전일 종가 조회 및 기준일 표시, 실시간 아님을 UI/README에 명시.
- `P5-02` KIS 앱키/시크릿 토큰 발급/갱신 + 한 종목 현재가 `READ ONLY`.
- `P5-03` 관심종목 2~4개와 종목코드 관리, API 제한/시장 시간 따른 스케줄.
- `P5-04` 시세표시 이용약관 확인·로그 스캔·의도치 않은 주문 코드 의존 금지.
- 게이트: API 승인/키 없으면 금융 기능 비활성 기본값과 Host fixture 결과로 완료. 진짜 가격/실시간 보장 주장 금지.

## P6 — 공개 배포 / 유지관리 (MVP 이후)
- `P6-01` Bitbucket 및 로컬 CI, Windows powershell 빌드 래퍼, secrets scan, golden preview.
- `P6-02` versioned firmware artifact와 hash, LICENSE/THIRD_PARTY_NOTICES.
- `P6-03` USB 복구 매뉴얼, 선택적 인증·롤백 지원 OTA 설계(실기 검증 전 default off).
- `P6-04` 운영 매뉴얼: API 키 발급 경로, 결측 대응, 버전업·백업, 최종 QA 리포트.
- 게이트: 사용자 소유 E1001에서 최초 설치→설정→수집→렌더→수면→복원 시나리오 실측. 없는 경우 `P6-RC-UNVERIFIED-HW`만 제공.

## 다음 단계로 넘어갈 때
- `templates/PROGRESS_REPORT.md`에 각 티켓별 DONE/BLOCKED, 코드 변경 경로, 증거 파일, 정확한 실행 명령을 남긴다.
- 단순 커버리지 숫자보다 실제 시뮬레이션, 독립 Oracle, mutation test로 정확성 입증.
- 해당 단계 실패가 해결되지 않았으면 넘어간 이유와 제한 범위를 사용자에게 명시.
