# Phase Progress Report — stage-3 (공식 제품 문서 대조·교정)

- 날짜·시간(KST): 2026-11-02
- 작업 단계/티켓: 공식 문서 대조 (U-12/13/14/16/17 해결, 세기비트·BUSY 버그 수정)
- 커밋 SHA: 미커밋 작업트리
- 1차 출처: Seeed 위키 E1001 페이지, 공식 저장소 @8d97d891 (당일 API로 HEAD 일치 확인),
  ZinggJM/GxEPD2 LICENSE, SenseCraft_HMI 보드 설정·SY6974·PMIC·RTC 원문
- 기기 연결 여부: 실기 없음 (판독 실측은 BLOCKED_HW 유지)

## 구현 내용
- 버그 수정:
  - `src/drivers/Pcf8563.cpp` 세기 비트 반전 (C=1→1900s, 공식 RTC 예제 기준)
  - `src/drivers/EpdPanel.cpp` BUSY 극성 (idle HIGH / busy LOW, Gray4 `checkBusy` 기준)
- 신규: `src/drivers/{Battery,Pmic}.*` (공식 Battery_Monitor·SY6974 조건 그대로),
  `PcfInit()` (STOP 해제·알람 클리어·CLKOUT off·I2C 400kHz), `BatteryPolicy::PercentMv`
  (공식 101점 곡선, Low=20%/Critical=5%), `main.cpp` 연결 (BatteryBegin, PMIC USB 판정)
- 문서: `hardware/official_product_facts.md` (신규, 향후 에이전트 참조용),
  `board_e1001.md` 핀표 확장, `driver_comparison.md`·ADR 0004 (자체 UC8179 채택),
  로드맵 U-12/13/14/17 해결 표기, T03/T05/T08 주의 갱신, `THIRD_PARTY_NOTICES.md`,
  `platformio.ini` 주석
- 기존 기능 영향: `SensorCheck` 변경 없음. 배터리 임계값이 곡선 기반(3464/3252mV)으로
  바뀌었으나 기존 경계 테스트(3400→Low, 3200→Critical)는 그대로 통과.

## 재현 가능한 증거
| 구분 | 명령/방법 | 결과 | 검증 단계 | 증거 경로 |
|---|---|---|---|---|
| Unit tests | `python tools/run_native_tests.py` | PASS (37/37) | Host | 터미널 로그 |
| Unit tests | `python -m pytest host/tests -q` | PASS (98) | Host | 터미널 로그 |
| Clean build | `pio run -e reterminal_e1001` | SUCCESS | Build | RAM 21.4% / Flash 38.0% |
| SEC-01 | `test_sec01_*` 포함 pytest | PASS | Host | 신규 파일 스캔 이상 없음 |
| E1001 smoke | 실기 없음 | NOT_RUN | Hardware | BLOCKED_HW |
| Live API | 키 없음 | NOT_RUN | LiveAPI | BLOCKED_CREDENTIALS |

## 독립 검증 및 보안
- 공식 원문 대조: RTC 레지스터맵·세기비트·init 순서, 배터리 핀/게인/대기시간/분압,
  SY6974 주소·레지스터·비트, EPD 핀·BUSY 극성·init 시퀀스, GxEPD2 GPL-3.0 전문.
- PMIC에는 읽기만 한다 (충전 설정 쓰기 없음 — 순정 동작 보존).
- 남은 실기 대조: PMIC 판독, 곡선 대조, RTC 드리프트, BUSY 타이밍, Flash 32MB 확정.

## 게이트 판정
- PARTIAL (HOST+BUILD PASS, HW·Live 미검증 — stage-2와 동일).
- 다음 단계: 실기 입수 시 T11 체크리스트 + PMIC/곡선 대조 후 `docs/12` 갱신.
