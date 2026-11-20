# 13. 향후 과제 로드맵 — 에이전트 구현용 (2026-11-02 기준)

2026-11-13 추가: 실제 기기 장기 운영 개선은 [감사 계획 LT-01~10](17_LONG_TERM_OPERATIONS_AUDIT.md) 순서로 진행한다. 80% 충전 제한의 리비전/칩 확인 및 USB·배터리 soak를 우선한다. 이 항목의 최신 검증 경계는 [단계 보고](../evidence/long-term-2026-11-13/summary.md)를 따른다.

> 대상 독자: 이 저장소에서 다음 단계를 구현하는 코딩 에이전트(와 검토하는 사람).
> 이 문서는 **무엇이 남았고, 어떤 순서로, 어떤 완료 기준으로** 구현하는지 정리한 색인입니다.
> 과제별 상세 명세(설계·파일·단계·테스트·증거)는 `docs/tasks/Txx_*.md`에 있습니다.
> 작업 시작 전 `AGENTS.md`, `docs/12_COVERAGE_AND_GAPS.md`, `docs/adr/0003-live-api-integration.md`를 먼저 읽습니다.

> **감사 후 현재 상태:** T02~T09의 Host 로직과 기기 어댑터 연결을 진행했으며 현재 판정은 docs/12와 evidence/audit-remediation/summary.md가 우선합니다. 아래 단계 표는 원래 작업 분해를 보존한 것입니다. 미연결로 적힌 옛 항목을 다시 구현하지 말고 실제 소스를 먼저 확인합니다. 다음 주요 게이트는 T01 실 API, T11 실기/전류/복구, KRX 추가 휴장 출처 확정입니다. 패널은 ADR 0004의 자체 UC8179를 사용합니다.

## 1. 현재 위치 (이미 끝난 것 — 반복 구현 금지)

| 영역 | 상태 | 위치 |
|---|---|---|
| 도메인 모델·KST 시간·신선도 | DONE_VERIFIED_HOST | `src/domain/` |
| 제공자 파서 + JSON 어댑터 (KMA/AirKorea/EXIM/FSC/KIS) | DONE_VERIFIED_HOST | `src/providers/` |
| 결과코드 분류 (data.go.kr/EXIM/KIS) | DONE_VERIFIED_HOST | `src/providers/ResultCodes.*` |
| URL 빌더·키 정규화·허용 호스트 | DONE_VERIFIED_HOST | `src/net/Endpoints.*`, `NetPolicy.*` |
| ESP32 HTTPS 전송 (루트 CA 고정) | DONE_BUILD_ONLY | `src/net/HttpTransport.*`, `CaBundle.h` |
| 스케줄 (백오프·발표 회차·장중·FSC 13시·한도) | DONE_VERIFIED_HOST | `src/app/Coordinator.*` |
| fetch 오케스트레이션 + last-known-good 스냅샷(RAM) | DONE_VERIFIED_HOST | `src/app/Fetcher.*` |
| 설정 검증·watchlist·포털 검증 로직·RNG 포트 | DONE_VERIFIED_HOST | `src/storage/SettingsStore.*`, `src/provision/SetupPortal.*` |
| 펌웨어 진입점: 컴포지션 루트 + 모듈 (Wi-Fi→NTP→fetch→UART `Serial1`) | DONE_BUILD_ONLY | `src/main.cpp` + `src/{boot,net,drivers,app,power,provision,ui}/` 모듈 (ADR 0009) |
| 공통 골든(C++·Python 40건), 네이티브 테스트 러너, CI 3단계 | DONE | `test/golden/`, `tools/run_native_tests.py` |

## 2. 남은 과제 목록

| ID | 과제 | 선행 | 필요 자원 | 최대 검증 수준 (실기·키 없이) | 상세 |
|---|---|---|---|---|---|
| T01 | 실 API probe로 미확정값 확정 (U-05/06/08/09, EXIM 코드) | — | **API 키 + 실기**(또는 PC probe) | — (전부 BLOCKED_CREDENTIALS) | `docs/tasks/T01_live_api_probe.md` |
| T02 | 영속 저장: NVS 설정, 원자 쓰기, 스냅샷·상태 보존 | — | — | DONE_VERIFIED_HOST + BUILD | `docs/tasks/T02_storage_persistence.md` |
| T03 | 시계: PCF8563 RTC + NTP + 신뢰 판정 | — | 실기(최종) | HOST(로직) + BUILD | `docs/tasks/T03_clock_rtc.md` |
| T04 | 화면: 뷰모델 → 한글 폰트 → 캔버스 → UC8179 출력 | T03(시각 표시) | 실기(최종) | HOST(뷰모델·캔버스·프리뷰) + BUILD | `docs/tasks/T04_display_renderer.md` |
| T05 | 전원: deep sleep, 타이머·버튼 wake, RTC 메모리 보존 | T02, T03 | 실기 | HOST(계획 로직) + BUILD | `docs/tasks/T05_power_sleep_wake.md` |
| T06 | 버튼: 디바운스, 짧게/길게, wake 버튼 판별 | — | 실기(최종) | HOST(상태기계) + BUILD | `docs/tasks/T06_buttons_input.md` |
| T07 | 설정 포털: SoftAP + 웹서버 + 저장 + 공장초기화 | T02, T04(비밀번호 표시), T06(롱프레스) | 실기 | HOST(요청 핸들러) + BUILD | `docs/tasks/T07_setup_portal.md` |
| T08 | 실내 센서 SHT40 + 배터리 전압 | — | 실기 | HOST(판정) + BUILD | `docs/tasks/T08_sensor_battery.md` |
| T09 | 공휴일·휴장 달력 | T02 | API 키(특일정보) | HOST | `docs/tasks/T09_holiday_calendar.md` |
| T10 | 릴리스·복구·파티션 (OTA는 연기) | T02~T07 | 실기 | BUILD | `docs/tasks/T10_release_recovery.md` |
| T11 | 실기 검증·장기 soak·전류 측정 | 전부 | 실기 + 계측기 | — (BLOCKED_HW) | `docs/tasks/T11_hw_validation_soak.md` |
| T12 | 작은 후속 과제 모음 | 개별 | 개별 | 개별 | `docs/tasks/T12_followups.md` |

## 3. 의존 그래프와 권장 순서

```text
            ┌────────── T01 (키 생기면 언제든, 결과는 골든에 반영)
            │
T02 저장 ──┬──> T05 전원 ──┐
T03 시계 ──┤               ├──> T10 릴리스 ──> T11 실기 검증/soak
T06 버튼 ──┼──> T07 포털 ──┤
T04 화면 ──┘               │
T08 센서·배터리 ────────────┘
T09 달력 (T02 이후 아무 때나)
```

키·실기가 없는 에이전트의 권장 순서:
**T02 → T03 → T04(a→b→c→d) → T06 → T08 → T05 → T07 → T09 → T10 → T12**.
T01과 T11은 자원이 생기는 즉시 끼워 넣습니다.

## 4. 공통 완료 기준 (모든 과제에 적용)

1. **작게 나눠 진행합니다.** 과제 문서의 "구현 단계"마다 (a) 변경 → (b) `python tools/run_native_tests.py` + `python -m pytest host/tests -q` → (c) `pio run -e reterminal_e1001` 순서로 확인합니다.
2. **이식 가능한 로직과 하드웨어 어댑터를 분리합니다.** 판단 로직은 `src/` 안의 Arduino 비의존 코드로 두고 네이티브 테스트로 검증합니다. Arduino/ESP-IDF 호출은 얇은 어댑터(`#if defined(ARDUINO) && defined(KDASH_BOARD_E1001)`)에만 둡니다. 어댑터 파일은 `platformio.ini`의 `host_native` `build_src_filter`에서 제외합니다.
3. **골든·회귀를 함께 늘립니다.** 제공자 의미가 바뀌면 `test/golden/provider_cases.json`을 먼저 고치고, C++과 Python을 모두 통과시킵니다.
4. **상태를 정직하게 표기합니다.** `DONE_VERIFIED_HOST` / `DONE_BUILD_ONLY` / `DONE_VERIFIED_HW` / `DONE_VERIFIED_LIVE_API` / `BLOCKED_*`. 빌드 성공을 실기 동작으로 격상하지 않습니다.
5. **비밀을 다루는 규칙을 지킵니다.** 키·토큰·SSID 비밀번호는 로그, 화면, 포털 응답, fixture, 증거에 남기지 않습니다. 새 로그 줄을 추가하면 `test_sec01_*`가 통과해야 합니다.
6. **증거를 남깁니다.** `evidence/p<N>/summary.md`를 `templates/PROGRESS_REPORT.md` 형식으로 작성하고, `docs/12`의 상태표를 갱신합니다.
7. **스펙과 다르게 결정하면 ADR을 남깁니다.** `docs/adr/00NN-*.md`에 이유·영향·대안을 기록합니다. 입력 패키지 `docs/00~10`은 수정하지 않습니다.
8. **새 라이브러리는 먼저 조사합니다.** 라이선스·RAM/Flash 비용·버전 고정을 `THIRD_PARTY_NOTICES.md`와 `platformio.ini`에 함께 반영합니다.

## 5. 새로 생긴 미결 질문 (docs/10의 U-01~U-11에 이어서)

| ID | 질문 | 확인 방법 | 영향 과제 |
|---|---|---|---|
| U-12 | 해결: VBUS GPIO 없음. PMIC SY6974(REG0A bit7, 0x6B/Wire1)으로 판정 — `src/drivers/Pmic.*` 구현됨. T11 실기 대조만 남음 | 공식 SenseCraft PMIC 코드 @8d97d891 | T05 |
| U-13 | 해결: GPL-3.0 확인됨. 자체 UC8179 우선으로 채택 (ADR 0004) | ZinggJM/GxEPD2 LICENSE | T04 |
| U-14 | 해결(소스상): 공식 RTC 드라이버는 알람 미사용, E1001 설정에 INT 핀 없음 → 타이머 wake만. T11 실기 재확인 1회 | SenseCraft rtc_pcf8563 + E1001 config.h | T05 |
| U-15 | UC8179 부분 갱신의 영역 정렬 단위·잔상 | 실기 100회 | T04d |
| U-16 | `SensirionI2cSht4x` 라이선스·버전 | 라이브러리 원문 | T08 |
| U-17 | 해결: 공식 101점 곡선 확보 → `BatteryPolicy::PercentMv` 구현됨. T11 실측 대조만 남음 | SenseCraft E1001 config.h | T08, T11 |

## 6. 에이전트에게 주는 운영 지침

- 시작할 때는 `prompts/02_BACKLOG_PROMPT.md`를 사용합니다.
- 한 세션에서 **과제 하나(또는 하위 단계 몇 개)** 만 끝까지 진행합니다. 여러 과제를 반쯤 열어두지 않습니다.
- 실기가 필요한 단계에 도달하면 해당 단계를 `BLOCKED_HW`로 표시하고, 같은 과제의 Host 가능한 단계를 계속합니다.
- 사용자 E1001에 무단 플래시, NVS 전체 삭제, eFuse 변경을 하지 않습니다(AGENTS.md).
