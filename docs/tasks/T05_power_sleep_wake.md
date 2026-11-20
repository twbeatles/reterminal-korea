# T05 — 전원: deep sleep, 타이머·버튼 wake, RTC 메모리 보존

- 상태: HOST DONE + BUILD (CyclePlanner·RtcState, sleep/wake·전류 실기 BLOCKED_HW)
- 선행: T02(영속 저장), T03(시계). T04·T06과 함께 통합합니다.
- 필요 자원: 실기, 가능하면 전류계(USB 파워미터 또는 PPK2급)
- 관련: docs/06(Sleep 스테이트머신·필수 테스트), `src/power/{PowerManager,Schedule}.*`, `app::NextWakeUtc`, 공식 예제 `LowPower_DeepSleep`, U-12, U-14

## 1. 목표

배터리 모드에서 "깨어남 → (필요하면) 조회 → (바뀌었으면) 갱신 → 다음 기상 시각 예약 → deep sleep" 한 사이클을 짧고 예측 가능하게 만듭니다. 버튼으로도 깨울 수 있어야 합니다.

## 2. 하드웨어 사실 (공식 예제 원문 / 미확정)

- 버튼 wake: `esp_sleep_enable_ext1_wakeup(1ULL<<3, ESP_EXT1_WAKEUP_ANY_LOW)` + `rtc_gpio_pullup_en(3)`, `rtc_gpio_pulldown_dis(3)`.
  - KEY1(4), KEY2(5)도 RTC GPIO(0–21) 범위라 같은 마스크에 넣을 수 있습니다. 실기 확인 필요.
- 깨어난 원인: `esp_sleep_get_wakeup_cause()`. 어떤 핀이었는지는 `esp_sleep_get_ext1_wakeup_status()`(비트마스크)로 확인합니다.
- 타이머 wake: `esp_sleep_enable_timer_wakeup(us)`.
- RTC 알람 wake는 **공식 소스상 미사용(U-14 해결, 2026-11-02)** — ESP32 타이머 wake만 사용합니다. T11에서 실기 재확인 1회.
- USB 감지는 **PMIC SY6974** (U-12 해결, 2026-11-02): REG0A bit7을 `Pmic::UsbAttached()`로 읽습니다 (읽기 전용). 실패하면 배터리 스케줄로 폴백합니다. (원문: USB 전원 감지 핀은 미확인이었음) 확인 전까지 docs/06 원칙대로 "감지 불가 = 배터리 스케줄"을 적용합니다. 설정에 수동 토글을 두되 기본값은 배터리입니다.
- 예제의 14µA는 데모 단독 수치이며 대시보드 수명 근거가 아닙니다.

## 3. 설계

### 3-1. 사이클 계획기 (이식, `src/power/CyclePlanner.{h,cpp}`)

```cpp
struct CycleInput {
  WakeReason reason;          // Timer / Button / FirstBoot / Unknown
  uint8_t button_mask;        // EXT1 상태 (KEY0=bit3 ...)
  bool long_press;            // T06 판정 결과
  int64_t now_utc; bool clock_verified;
  bool usb;                   // U-12 전까지 false
  int battery_mv;             // T08, 모르면 -1
  const AppState* st;
};
struct CyclePlan {
  bool connect_wifi;          // 조회할 것이 있을 때만
  bool force_refresh;         // KEY2: 간격 무시, 단 auth 차단·한도·429는 존중
  bool enter_portal;          // KEY0 길게 또는 설정 없음
  ui::Page page;              // KEY0/KEY1로 이동
  bool render;                // 페이지 변경 또는 데이터 변경 가능성
  int64_t sleep_seconds;      // clamp(NextWakeUtc - now, 60, max_sleep)
  bool low_battery_mode;      // 임계 이하 → 조회 중단, 최대 수면
};
CyclePlan PlanCycle(const CycleInput&);
```

- `max_sleep`은 배터리에서 6시간, 저전력 모드에서 24시간(출발값)입니다. 시계 unverified이면 30분 후 재시도합니다(NTP 재동기화).
- 저전력 임계값은 T08 실측 전까지 상수 출발값(예: 3,450mV)을 두고 "미검증"으로 표기합니다.

### 3-2. RTC 메모리 보존 (`src/power/RtcState.{h,cpp}`)
- `RTC_DATA_ATTR`에 다음을 둡니다(합계 약 3.4KB, ESP32-S3 RTC slow memory 8KB 이내).
  - magic + 버전 + CRC32
  - `ProviderStatus[7]`(336B), `Snapshot`(1,880B), `KisSession`(1,040B)
  - `page`, `last_rendered_crc`, `partials_since_full`, `boot_count`, `consecutive_crashes`
- 부팅할 때 CRC가 틀리거나 첫 전원 인가(power-on reset)이면 NVS 스냅샷(T02)에서 복원하거나 비웁니다.
- KIS 토큰은 RTC 메모리에만 둡니다. NVS에는 저장하지 않습니다(분실 위험 최소화).
- 직렬화는 T02 `SnapshotCodec`을 재사용합니다. 구조체를 RTC 메모리에 통째로 두는 방식도 같은 바이너리 안이라 가능하지만, CRC 검증은 반드시 넣습니다.

### 3-3. 기기 흐름 (`main.cpp` 재구성)

```text
setup():
  reason = wake cause; restore RtcState (CRC) / NVS
  buttons = T06 판정 (EXT1 mask + 길게 누름 샘플링)
  plan = PlanCycle(...)
  if plan.enter_portal → T07 (최대 10분) → 재부팅
  if plan.connect_wifi → Wi-Fi(타임아웃 20s) → 시계(T03) → RunFetchPass
  if plan.render → T04 RenderStep
  save RtcState; (조건부) NVS 스냅샷 (T02 빈도 제한)
  prepare sleep: Wi-Fi off, display.hibernate(), 배터리 enable LOW, I2C 해제
  enable timer(plan.sleep_seconds) + EXT1(mask 3|4|5, ANY_LOW) + RTC pull-up
  esp_deep_sleep_start()
loop(): 도달하면 오류 (예제처럼 로그 후 재시도)
USB 모드(U-12 확인 후): deep sleep 대신 현재의 30초 idle 루프
```

- 사이클 상한: 깨어 있는 시간은 최대 60초(출발값)로 둡니다. 넘으면 남은 작업을 버리고 잠듭니다(watchdog과 별개, T12).
- 부팅 루프 보호: `consecutive_crashes ≥ 3` → 안전 모드(조회 없이 기기 페이지 + "오류 반복" 표시 + 6시간 수면). 정상 사이클을 마치면 0으로 초기화합니다.

## 4. 구현 단계

1. `CyclePlanner` + 테스트 (아래 표)
2. 100사이클 시뮬레이션 테스트: 가짜 시계·가짜 HTTP·가짜 버튼으로 7일치를 돌리고, 호출 수·렌더 수·수면 시간 분포가 docs/06 계획값 안에 드는지 검사합니다(PWR-01).
3. `RtcState` 직렬화 + CRC 테스트
4. 기기 어댑터(sleep 진입, wake 원인 판독) + `main.cpp` 재구성
5. 빌드 → 실기 단계는 BLOCKED_HW로 남깁니다.

## 5. 테스트 (최소)

| 케이스 | 기대 |
|---|---|
| Timer wake, 조회할 제공자 없음 | Wi-Fi 연결 안 함, 렌더 안 함, 다음 기상까지 수면 |
| Timer wake, KMA 회차 갱신 시각 | Wi-Fi 연결, KMA만 조회 |
| Button KEY1 | 페이지 +1, Wi-Fi 연결 안 함, Full 렌더 |
| Button KEY2 | force_refresh, auth 차단된 제공자는 제외. 시계 미확인이면 sync_clock(NTP 먼저, HTTPS는 시계 확인 후) |
| KEY0 길게 | enter_portal |
| 설정 없음(첫 부팅) | enter_portal |
| 배터리 < 임계 | low_battery_mode, 조회 없음, 기기 페이지 경고 렌더, 최대 수면 |
| 시계 unverified | 30분 후 재시도, HTTPS 없음 |
| `NextWakeUtc` = INT64_MAX | max_sleep |
| 연속 크래시 3회 | 안전 모드 |
| 7일 시뮬레이션 | KIS 장외 호출 0, KMA 시간당 ≤ 1(+NODATA 재시도 1), 무한 루프 없음 |

## 6. 완료 기준

- [ ] HOST 테스트 통과, 펌웨어 빌드 성공
- [ ] 실기: 100회 sleep/wake (버튼 50 + 타이머 50) 중 무응답 0 (BLOCKED_HW)
- [ ] 실기: 사이클별 깨어 있는 시간·평균 전류 기록 → T11에서 배터리 수명 추정 (측정 전 수명 주장 금지)
- [ ] U-12, U-14 결과를 이 문서와 `hardware/board_e1001.md`에 반영
