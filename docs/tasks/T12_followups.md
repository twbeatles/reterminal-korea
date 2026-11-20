# T12 — 작은 후속 과제 모음

2026-11-02 반영: F-01(데모)·F-02(예산)·F-03(보수적 세션)·F-04(로그)·F-05(워치독)·F-08(제공자명)·F-09(check_sources)·F-10(릴리스테스트) DONE_HOST. F-06(근접측정소)·F-07(프리뷰정리) 미완. 항목마다 "왜·무엇을·어떻게 확인"을 짧게 정리했습니다. 큰 과제를 진행하다가 관련 항목을 함께 처리해도 됩니다.

## F-01 데모 모드 (키 없이 첫 실행)
- 왜: docs/03 "추가 선택지" — 키 발급 전에도 화면 동작을 확인할 수 있어야 합니다.
- 무엇: `DemoProvider`는 고정 합성 스냅샷을 만듭니다. 모든 값에 DEMO 표시(T04a 배지)를 붙이고 네트워크를 호출하지 않습니다. 설정에 `demo_mode` 플래그를 두고, 키를 하나라도 넣으면 자동으로 해제합니다.
- 확인: 뷰모델 테스트 "DEMO 배지 누락 시 실패", 실값과 섞이지 않음(데모 스냅샷은 저장하지 않음).

## F-02 사이클당 요청 예산의 공정성
- 왜: `kMaxRequestsPerPass=24`에서 EXIM walk-back(최대 7) + KIS 토큰·종목(최대 6) + FSC(4)가 몰리면 뒤 순서 제공자가 굶을 수 있습니다.
- 무엇: 제공자별 상한(예: EXIM 7, FSC 4, KIS 6, 나머지 2)을 두고, 남은 예산이 부족하면 다음 사이클로 미룹니다. 이것을 `MarkResult`의 실패로 기록하지 않습니다.
- 확인: 모든 제공자가 동시에 due인 상황에서 생활 정보(KMA/AirKorea)가 항상 먼저 처리되는 테스트.

## F-03 NXT·통합 시장 시간
- 왜: 지금 `IsKrxOpen`은 KRX 정규장(09:00–15:30)만 봅니다. NXT·통합 종목의 거래 시간은 다릅니다(U-08).
- 무엇: 시장별 세션 표(출처·확인일 포함)를 두고 `WatchItem.market`에 따라 장중을 판정합니다. 확인 전에는 지금처럼 KRX 정규장만 허용합니다(보수적).

## F-04 로그 모듈 일원화
- 왜: `main.cpp`가 `KDASH_LOG.printf`를 직접 씁니다. 비밀 노출 방지를 한 곳에서 강제해야 합니다.
- 무엇: `src/util/Log.{h,cpp}`를 두고 레벨(ERROR/WARN/INFO/DEBUG)을 나눕니다. 형식 문자열 대신 필드 API(`Log::KV("provider", id)`)를 쓰고 URL·키 타입은 받지 않게 합니다. 릴리스 빌드 기본 레벨은 INFO입니다.
- 확인: SEC-01 + `firmware.bin` strings 스캔(T10).

## F-05 Watchdog
- 왜: TLS 핸드셰이크·ePaper BUSY 같은 블로킹 구간이 무한 대기에 빠질 수 있습니다.
- 무엇: `esp_task_wdt`(타임아웃 예: 90s)를 켜고 단계마다 feed합니다. T05의 사이클 상한·연속 크래시 카운터와 연결합니다.
- 확인: 실기에서 일부러 BUSY 대기를 걸어 재부팅 후 안전 모드 진입 확인(BLOCKED_HW).

## F-06 AirKorea 근접 측정소 자동 선택 (선택)
- 왜: 사용자가 측정소명을 정확히 몰라도 되게 하기 위함입니다.
- 무엇: 측정소정보 API(`MsrstnInfoInqireSvc/getNearbyMsrstnList`, TM 좌표 입력)를 사용합니다. 위경도→TM 변환이 필요하며, **API·좌표계 원문 확인 필수**입니다. 포털(T07)에서 1회 조회해 후보를 보여주는 방식으로 하고, 기기 일상 동작에는 넣지 않습니다.

## F-07 Python 프리뷰 도구 정리
- T04c 완료 후 `host/kdash/ui_preview.py`(ASCII 자리표시자)를 C++ 렌더 결과(PBM) → PNG 변환기로 바꾸거나 폐기합니다. `test_ui_security.py`의 골든 테스트도 함께 조정합니다.

## F-08 AppState 상태 표시용 문자열
- `ToString(Provider)`를 추가합니다(지금 UART에는 숫자 id만 나옴). 기기 페이지(T04)와 로그에 공용으로 씁니다.

## F-09 `docs/10` 출처 재확인 주기
- 왜: AGENTS.md "시작 전 수행 1"에 따라 출처 확인일을 최신으로 유지해야 합니다.
- 무엇: `tools/check_sources.py`로 docs/10·ADR 0003의 URL에 HEAD 요청을 보내 상태 코드와 수정일 변경을 보고합니다. 인증서 검증을 우회하지 않습니다. koreaexim 페이지가 TLS 오류로 열리지 않았던 사례도 그대로 보고합니다.

## F-10 CI 강화
- Bitbucket `native-cpp-tests` 단계를 실제로 한 번 돌려 통과 로그를 확보합니다(아직 원격 실행 이력 없음).
- `test-results.xml`(JUnit) 산출: pytest `--junitxml`, Unity 결과 변환.
