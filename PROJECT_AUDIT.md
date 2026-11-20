# Project Audit

> **2026-11-16 조치 현황:** 아래 본문은 수정 전 감사 기록으로 보존한다. ISSUE-015~018 및 G-1~3/G-5/G-6의 코드·문서 조치와 G-4의 공유 복원 경로 테스트·펌웨어 CI를 추가했다. 모든 SDK/실기 조합 테스트 완료를 뜻하지 않는다. 최신 실행 결과·플래싱·남은 물리 검증은 [후속 보고](evidence/audit-2026-11-16/summary.md), 설계 결정은 [ADR 0031](docs/adr/0031-audit-2026-11-16-runtime-fixes.md)을 따른다.

| 항목 | 조치 및 검증 범위 |
|---|---|
| ISSUE-015 | 신뢰하는 시스템 시계 재적용 제거, RTC 최초 시드 1회 제한, 새 NTP만 RTC 기록. 밀리초 시계 120회 회귀 및 실기 카운터 로그. 24시간 정확도는 별도 검증. |
| ISSUE-016 | 수면 직전 칩/USB/OTG 재확인 후 충전 허용·read-back. 실패 시 30초 wake. 모의 레지스터 오류 검사, USB 탈착·충전 전류 실측은 미완료. |
| ISSUE-017 | 실제 공유 복원 함수에서 설정 저장이면 시작 화면 적용. 캐시/공급자 상태·일반 wake 페이지 보존 및 CRC 실패 원자성 회귀. 실기 페어링 저장 UI는 미완료. |
| ISSUE-018 | USB 자동 시계 전용 연결의 블로킹·첫 사이클 중복 연결 제거. 자동 10회·수동/배터리 경로 회귀. |
| G-1 | 내용 해시·갱신 UTC를 RTC CRC 블롭 v2에 보존. 일반 수면 최대15분 문서 정정. 배터리 수명/전류 미측정. |
| G-2 | 시간 예산으로 미전송된 요청을 deferred로 처리. 다종목 중간 소진/성공값 보존 회귀. |
| G-3 | 다음 해 NoData 재요청 하루 제한. API 실제 공개 일정은 미확인. |
| G-4 | 시계·충전 포트 및 실제 복원 조합 회귀, Linux 타깃 빌드 CI 추가. Wi-Fi/WebServer/패널 SDK 전체 자동 실기 검증은 잔여 한계. |
| G-5 | 최대 길이 설정/4종목 캐시 용량 회귀, 실제 파티션 대조, 부팅 NVS 통계. 파편화/다른 namespace 무제한 증가 보장은 없음. |
| G-6 / §6 | AP 연결 시험 저장 조건·수면 전 충전 허용·실제 수면 주기·공휴일 주석 수정. |

- 감사일: 2026-11-16
- 대상: `main` @ `aaca78f` (K-Dash E1001 펌웨어 `1.0.0`)
- 범위: 기능 구현·런타임 안정성. 코드는 수정하지 않았다.
- 이전 감사(2026-11-12, ISSUE-001~014)는 모두 조치되어 [ADR 0026](docs/adr/0026-audit-2026-11-12-fixes.md)에 기록돼 있다. 그 본문은 Git 이력(`432fdf6`의 `PROJECT_AUDIT.md`)에 남아 있다. ADR이 예전 번호를 참조하므로 이번 이슈는 **ISSUE-015부터** 번호를 잇는다.

## 1. Executive Summary

**전체 상태.** 코드 품질과 방어 수준이 높다. 순수 C++ 코어와 ESP32 어댑터가 분리돼 있고, 설정·캐시는 이중 슬롯 + CRC + read-back으로 저장하며, 요청 예산·백오프·인증 차단·워치독·크래시 가드가 모두 구현돼 있다. 호스트 테스트는 전부 통과했다(Python 204개, C++ 144개).

**전체 위험도: 낮음~중간.** Critical·High 이슈는 찾지 못했다. 남은 문제 4건은 모두 Medium이며, 호스트 테스트가 닿지 않는 **ESP32 어댑터 계층과 그 조합 경로**에 몰려 있다.

**가장 중요한 문제**

| ID | 요약 | 우선순위 | 신뢰도 |
|---|---|---|---|
| ISSUE-015 | USB 상시 구동 중 사이클마다 시스템 시계의 소수 초를 버려 시계가 시간당 약 1분 늦어지고, 그 값으로 하드웨어 RTC까지 덮어쓴다 | Medium | Likely |
| ISSUE-016 | 충전 보호가 "충전 중단" 상태로 배터리 구동·수면에 들어가면, USB를 다시 꽂아도 기기가 깰 때까지(저전압 모드 최대 24시간) 충전이 재개되지 않는다 | Medium | Likely |
| ISSUE-017 | 기기 설정만 저장하면 재부팅 뒤 기기 탭에 머문다. `시작 화면` 설정이 적용되지 않고 자동 순환도 버튼을 누를 때까지 시작되지 않는다 | Medium | Confirmed |
| ISSUE-018 | 시계 미검증 + 저장된 Wi-Fi 접속 불가(USB) 상태에서 30초마다 20초씩 루프가 막혀 버튼(포털 열기 포함) 입력이 유실된다 | Medium | Likely |

**데이터 손상/유실 가능성.** 설정·캐시·달력 저장 경로에서 손상이나 유실로 이어지는 결함은 찾지 못했다. 다만 ISSUE-015는 정상인 PCF8563 RTC 시각을 늦어진 값으로 덮어쓴다. NTP가 닿으면 약 1시간 안에 교정되지만, NTP가 장기간 막히면 누적된다.

**가장 먼저 수정할 영역.** `src/drivers/EspClockSync.cpp`의 시각 적용 조건(ISSUE-015). 다음은 수면 진입 전 충전 상태 복구(ISSUE-016).

## 2. Project Understanding

### 2.1 목적

Seeed reTerminal E1001(ESP32-S3, 800×480 흑백 전자잉크) 한 대가 서버 없이 공공 API를 HTTPS로 직접 조회해 생활·시장·달력·기기 4개 탭을 그리고, USB에서는 상시 구동, 배터리에서는 Deep Sleep으로 동작하는 펌웨어다. 금융은 읽기 전용 시세만 다룬다.

### 2.2 주요 entrypoint

| Entrypoint | 위치 | 역할 |
|---|---|---|
| `setup()` / `loop()` | [src/main.cpp](src/main.cpp) | 부팅 조립, 버튼·포털·자동 순환·30초 주기 사이클 |
| `app::RunCycle` | [src/app/CycleRunner.cpp](src/app/CycleRunner.cpp) | 한 사이클 오케스트레이션(호스트·실기 공용) |
| `portal::OpenPortal` / `TickLanPortal` | [src/provision/PortalRunner.cpp](src/provision/PortalRunner.cpp) | AP·LAN 설정 포털 |
| `pio test -e host_native`, `tools/run_native_tests.py` | [test_native/test_core.cpp](test_native/test_core.cpp) | C++ 코어 테스트 |
| `python -m pytest host/tests` | [host/tests/](host/tests/) | Python 오라클·도구 테스트 |

### 2.3 핵심 모듈

| 계층 | 모듈 | 책임 |
|---|---|---|
| 계획 | `power/CyclePlanner`, `app/Coordinator`, `power/Schedule` | wake 사유 → Wi-Fi/렌더/수면 계획, 공급자별 due·백오프·인증 차단 |
| 수집 | `app/Fetcher`, `app/RequestBudget`, `app/SymbolBatch`, `providers/*`, `net/HttpTransport`, `net/BodyReader` | 공급자 호출, 요청 예산, 종목 부분 실패 집계, 파싱 |
| 달력 | `app/CalendarRefresh`, `providers/HolidayService`, `domain/Calendar` | 공휴일 연도표, 장 상태 |
| 저장 | `storage/SlotStore`, `SettingsStore`, `RuntimeStore`, `CalendarStore`, `power/RtcPersistence` | NVS 이중 슬롯, RTC 메모리 보존 |
| 설정 | `provision/PortalHandler`, `PortalServer*`, `PortalAuth`, `SettingsBackup` | 폼 검증, 페어링·세션·CSRF, 저장 |
| 표시 | `ui/ViewModel`, `Renderer`, `FramePresenter`, `drivers/EpdPanel` | 화면 구성, 변경 시에만 갱신 |
| 하드웨어 | `drivers/EspClockSync`, `Pmic`, `Battery`, `Watchdog`, `net/WifiConnector`, `power/SleepController` | 시계, 충전, 워치독, Wi-Fi, 수면 |

### 2.4 데이터 저장 방식

- **NVS(`kdash` 네임스페이스):** `set.a/b`(설정), `run.a/b`(스냅샷 + 공급자 상태), `cal.a/b`(공휴일 표). 세대 번호 + CRC 이중 슬롯에 쓰고 read-back으로 확인한다.
- **RTC NOINIT 메모리:** 스냅샷, 공급자 상태, KIS 토큰, 현재 페이지, 마지막 렌더 CRC, 부팅·크래시 카운터. 전원 인가 리셋에서는 버린다.
- **바인딩:** NVS 체크포인트와 RTC 블롭 모두 `settings_codec::DataFingerprint`(기기 설정 제외)에 묶여 있어, 데이터와 관련된 설정이 바뀌면 캐시를 버린다.

### 2.5 외부 의존성

기상청, 에어코리아, 한국수출입은행, 금융위원회, 한국투자증권, 한국천문연구원 API(HTTPS, 내장 CA 번들, 허용 호스트 목록). NTP 3곳. 라이브러리는 ArduinoJson 7.4.3 하나다.

### 2.6 핵심 실행 흐름

```
setup() → LoadSettings(NVS→dev→기본) → RestoreRuntime(NVS→RTC) → RunCycle(FirstBoot|Timer|Button)

RunCycle
  → SyncClock(RTC) → PlanCycle(wake, 버튼, 시계, 배터리, due)
  → [포털] OpenPortal → PortalHandler 검증 → SaveSettingsVerified(SlotStore) → 재부팅
  → [네트워크] ConnectWifi → SyncClock(NTP) → RunFetchPass
        → DueProviders → Fetch*(Get/Post, 예산) → pj::* 파서 → Snapshot → MarkResult(백오프)
        → RefreshCalendar → SaveTables
  → RefreshFreshness → Render(변경 시) → Checkpoint(NVS) → Retain(RTC)
  → [배터리] PlanCycle(다음 wake) → SleepFor(Deep Sleep)

loop() (USB)
  → MaintainCharge → MaintainWifi → 버튼 디코드 → TickLanPortal → 자동 순환 → 30초마다 RunCycle(Timer)
```

## 3. Audit Coverage & Limitations

### 3.1 직접 읽고 확인한 모듈

- 전체 열람: `main.cpp`, `app/CycleRunner`, `Coordinator`, `Fetcher`, `RequestBudget`, `SymbolBatch`, `CalendarRefresh`, `PageRotation`, `EspCyclePlatform`, `power/CyclePlanner`, `Schedule`, `RtcPersistence`, `RtcState`, `SleepController`, `ChargeProtection`, `BatteryPolicy`, `storage/SlotStore`, `NvsStorage`, `RuntimeStore`, `CalendarStore`, `boot/SettingsLoader`, `provision/PortalRunner`, `PortalServer`, `PortalServer_esp32`, `PortalHandler`, `PortalAuth`, `LanPolicy`, `net/WifiConnector`, `WifiSupervisor`, `drivers/EspClockSync`, `ClockBootstrap`, `ButtonDecoder`, `Pmic`, `Battery`, `Watchdog`, `domain/ClockPolicy`, `providers/HolidayService`, `ui/FramePresenter`.
- 부분 열람: `net/HttpTransport`(요청·타임아웃 구간).
- 열람하지 않음: 개별 공급자 파서(`ProviderJson`, `KmaParser`, `EximParser`, `FinanceParsers`, `AirKoreaParser`), `ui/Renderer`·`ViewModel`·`Canvas`, `SettingsCodec`·`SnapshotCodec`, `SettingsBackup`, `BodyReader`, `tools/`·`scripts/`. 이전 감사와 호스트 테스트가 집중적으로 다룬 영역이라 이번에는 테스트 통과만 확인했다.

### 3.2 CodeGraph로 확인한 호출 관계

- `setup`/`loop` → `RunCycle` → `ICyclePlatform`(가상 호출) → `EspCyclePlatform` → `clock::SyncClock` → `BootstrapClock` (ISSUE-015 경로)
- `RunFetchPass` → `DueProviders` → `Fetch*` → `Finish` → `MarkResult`
- `MaintainCharge` 호출자가 `main.cpp`의 `setup`·`loop` 두 곳뿐이고 수면 경로에는 없음(ISSUE-016)
- `SyncClock`, `BootstrapClock`(ESP 어댑터 경유), `MaintainCharge`, `RunAp`에 대해 "3 hop 안에 테스트 없음"으로 보고됨(§5 G-4)

### 3.3 실행한 테스트

| 명령 | 결과 |
|---|---|
| `python -m pytest host/tests -q` | 204 passed (Windows 11, Python 3.15) |
| `python tools/run_native_tests.py` | 144 Tests, 0 Failures, 0 Ignored |

실행하지 않은 것: `pio run`(펌웨어 빌드), `tools/mutation_check.py`, `tools/render_previews.py`, `tools/check_public_repo.py`.

### 3.4 확인하지 못한 환경

- **실기 미연결.** 이번 이슈는 모두 코드 흐름으로 판단했다. 실기 재현은 하지 않았다.
- 실제 API 응답, NTP 동작, SY6974B 레지스터 유지 특성, Deep Sleep 전류는 확인하지 못했다.
- Linux·macOS 실행은 하지 않았다(CI 매트릭스에는 3개 OS가 들어 있다).

### 3.5 분석상의 한계

- CodeGraph는 `#if defined(ARDUINO)` 안의 ESP 전용 코드도 색인하지만, Arduino·ESP-IDF SDK 내부 동작(`settimeofday`, lwIP SNTP 주기, `WebServer`)은 알 수 없다. 이 부분은 SDK의 일반 동작을 근거로 추론했고, 해당 이슈의 신뢰도를 Likely로 둔 이유다.
- CodeGraph 탐색 결과는 일부 구간이 생략돼 돌아와, 핵심 파일은 직접 열람으로 보완했다.

## 4. High-Risk Issues

Critical·High 등급은 없다. 아래 4건은 모두 Medium이다.

### [ISSUE-015] USB 상시 구동 중 시스템 시계가 사이클마다 초 단위로 절삭되어 늦어지고 RTC까지 오염됨

* **위치:** [src/drivers/EspClockSync.cpp:34-38, 53-57](src/drivers/EspClockSync.cpp#L34-L57) `TrustedNtpUtc`·`ApplyUtc`, [src/drivers/ClockBootstrap.cpp:8-10](src/drivers/ClockBootstrap.cpp#L8-L10), [src/domain/ClockPolicy.cpp:9-20](src/domain/ClockPolicy.cpp#L9-L20), 호출부 [src/app/CycleRunner.cpp:28](src/app/CycleRunner.cpp#L28)
* **우선순위:** Medium
* **신뢰도:** Likely (코드 흐름은 명확하나 실기에서 지연량을 측정하지 않음)
* **문제:** NTP 동기화가 한 번이라도 성공하면 `TrustedNtpUtc`는 "신뢰할 NTP 시각"으로 `time(nullptr)`, 즉 **현재 시스템 시각의 초 단위 내림값**을 돌려준다. `BootstrapClock`은 그 값을 매번 `ApplyUtc`로 다시 적용하고, `ApplyUtc`는 `tv_usec = 0`으로 `settimeofday`를 호출한다. 결과적으로 `SyncClock`이 불릴 때마다 시스템 시계에서 소수 초(평균 0.5초)가 사라진다.
* **발생 조건:** USB 전원으로 계속 깨어 있고 부팅 후 NTP가 한 번 성공한 상태. `RunCycle`은 30초마다 실행되며 매번 `SyncClock(false)`를 부르고, 네트워크가 필요한 사이클에서는 한 번 더 부른다.
* **영향:**
  - 시계가 시간당 약 60초(120회 × 평균 0.5초) 늦어진다. lwIP SNTP의 주기 재동기화(기본 1시간)가 동작하면 그때 바로잡히므로 평소에는 0~약 1분 지연의 톱니 모양이 된다. 헤더 시계, 기준 시각 표시, 장 상태 전환, 기상청 발표 회차 판단이 그만큼 늦는다.
  - `Decide`는 RTC와 2초 넘게 차이 나면 `WriteRtc`를 지시한다. 따라서 정확하던 PCF8563이 약 2~4분마다 늦어진 시스템 시각으로 덮어써진다.
  - NTP가 장기간 닿지 않으면(UDP 123 차단, Wi-Fi 장기 단절 등) 교정이 없어 하루 약 24분씩 누적된다. RTC도 같은 값이므로 재부팅으로는 복구되지 않는다. 이 경우 신선도 판정, 발표 회차 선택, 장중 판정이 실제로 어긋난다.
* **근거:**
  ```cpp
  bool TrustedNtpUtc(int64_t& epoch) override {
    epoch = (int64_t)time(nullptr);                       // 현재 시스템 시각(초 내림)
    return g_ntp_notifications.load(...) != 0 && epoch >= KDASH_BUILD_EPOCH;
  }
  bool ApplyUtc(int64_t epoch) override {
    struct timeval tv = {};  tv.tv_sec = (time_t)epoch;   // tv_usec = 0
    return settimeofday(&tv, nullptr) == 0;
  }
  // ClockBootstrap.cpp
  if (d.verified && !p.ApplyUtc(d.epoch)) return Decision{};   // 호출마다 실행
  ```
* **반증 확인:**
  - NTP 재시도 주기 제한(`cadence` 6시간)은 `SyncNtp` 호출만 막는다. `ApplyUtc`는 `attempt`와 무관하게 매번 실행된다.
  - 배터리 경로는 해당하지 않는다. Deep Sleep 뒤에는 `g_ntp_notifications`가 0이라 RTC 값을 wake당 한 번만 적용한다.
  - lwIP SNTP 재동기화가 지연을 약 1시간분으로 제한한다는 점은 확인했다(그래서 High가 아니다). 다만 이는 SDK 기본 설정에 기댄 것이며 코드에서 보장하지 않는다.
  - 호스트 테스트의 가짜 플랫폼(`test_audit_runtime.inc`)은 `TrustedNtpUtc`가 고정값 `ntp`를 돌려주고 `ApplyUtc`가 정수 초만 다루므로 이 절삭을 볼 수 없다.
* **호출/영향 범위:** `loop`/`setup` → `RunCycle` → `ICyclePlatform::SyncClock` → `clock::SyncClock` → `BootstrapClock` → `Decide`/`ApplyUtc`/`WriteRtc`. 시각을 쓰는 모든 모듈(`Coordinator`, `Fetcher`, `Freshness`, `Calendar`, `ViewModel`, `RuntimeStore`)이 영향권이다.
* **권장 수정 방향:** 시스템 시계가 이미 신뢰 상태(NTP 동기화됨)면 `ApplyUtc`를 호출하지 않는다. 시스템 시각을 바꾸는 것은 (a) RTC에서 처음 시드할 때, (b) `SyncNtp`가 새 동기화를 받았을 때로 한정한다. RTC 쓰기도 새 NTP 동기화 직후에만 한다.
* **필요한 회귀 테스트:** 가짜 플랫폼이 밀리초 단위 시스템 시각을 갖게 하고, `trusted_ntp=true`에서 `BootstrapClock(attempt=false)`를 120회 호출한다. 기대: 시스템 시각 변화 0ms, `ApplyUtc` 호출 0회, `WriteRtc` 호출 0회.

### [ISSUE-016] 충전 보호가 "충전 중단" 상태로 수면에 들어가면 USB 재연결 후에도 기기가 깰 때까지 충전이 재개되지 않음

* **위치:** [src/power/ChargeProtection.cpp:32, 42-43](src/power/ChargeProtection.cpp#L32-L43) `Protection::Tick`, [src/power/SleepController.cpp:25-52](src/power/SleepController.cpp#L25-L52) `SleepFor`, [src/power/CyclePlanner.cpp:23-28](src/power/CyclePlanner.cpp#L23-L28)
* **우선순위:** Medium
* **신뢰도:** Likely (SY6974B가 VBUS 제거 뒤에도 REG01을 유지한다는 전제. 실기 미확인)
* **문제:** 보호가 켜진 기기는 USB 연결 상태에서 추정 80%에 도달하면 `CHG_CONFIG=0`(충전 중단)을 쓰고, 같은 틱에서 PMIC I2C 워치독도 끈다. 이 상태로 USB를 뽑으면 이후 틱은 `!(bus & 0x80)` 분기에서 아무것도 바꾸지 않고 `Unknown`을 반환한다. `SleepFor`도 충전 설정을 복구하지 않는다. 즉 배터리로 쓰는 내내 PMIC에는 "충전 중단"이 남는다.
* **발생 조건:** `배터리 보호` 사용 + 80% 이상에서 USB 분리(보호 사용자의 일상적인 상태) + 기기가 Deep Sleep인 동안 USB 재연결.
* **영향:** USB를 꽂아도 기기는 깨어나지 않으므로(wake 원인은 타이머·버튼뿐) 다음 wake까지 충전이 시작되지 않는다. 깬 뒤에도 30초 간격 3회 연속 ≤75% 판정이 필요해 약 90초가 더 걸린다.
  - 일반 배터리 모드: 최대 약 15분 지연
  - 안전 모드: 최대 6시간
  - **저전압 모드: 최대 24시간.** 배터리가 바닥나 충전기를 꽂았는데 하루 동안 충전되지 않는 것으로 보인다. 버튼을 누르면 즉시 깬다.
* **근거:**
  ```cpp
  if (!(bus & 0x80)) { high_ = low_ = 0; return state_ = State::Unknown; }   // USB 없음: 레지스터 그대로
  ...
  if (!Set(port,5,timer,0x30,0) ||                       // I2C 워치독 해제 → 기본값 복귀 없음
      !Set(port,1,cfg,0x10,limited_ ? 0 : 0x10)) return fail();
  ```
  `SleepFor`에는 PMIC 접근이 없고 `Wire1.end()`만 한다.
* **반증 확인:**
  - [ADR 0028](docs/adr/0028-charge-protection-and-runtime-reliability.md)은 "잠든 동안 USB를 연결하면 80% 보호를 보장하지 않는다"는 한계만 적고 있다. 실제 코드에서는 반대 방향(충전이 막힌 채 유지)이 일어나며 문서에 없다.
  - 워치독 만료에 의한 기본값 복귀는 코드가 직접 꺼서 일어나지 않는다. ADR도 "default 충전으로 복귀하지 않게" 한다고 적는다.
  - USB가 붙으면 기기는 USB 모드로 깨어 있으므로, 문제는 "수면 중 연결"에 한정된다.
  - VBUS가 있으면 시스템은 VBUS로 구동되므로 과방전 위험은 아니다(그래서 High가 아니다).
* **호출/영향 범위:** `main.cpp` `setup`/`loop` → `pmic::MaintainCharge` → `charge::Protection::Tick`. 수면 경로 `RunCycle` → `ICyclePlatform::Sleep` → `power::SleepFor`는 `pmic`를 호출하지 않는다.
* **권장 수정 방향:** 배터리 구동으로 수면에 들어가기 직전(USB 없음 확인 시) `CHG_CONFIG=1`로 복구한다. 잠든 기기는 어차피 80% 제한을 집행할 수 없으므로 ADR 0028의 기존 한계와도 맞는다. 그리고 이 동작을 ADR·포털 안내문에 적는다.
* **필요한 회귀 테스트:** `charge::Protection`에 가짜 레지스터를 물려, Limited 상태(REG01 bit4=0)에서 "USB 없음 + 수면 준비" 경로를 실행한다. 기대: REG01 bit4=1로 read-back 확인, 다른 비트 불변. 실기: 80% 이상에서 분리 → 수면 → 재연결 후 깨우지 않고 충전 전류 유무 확인.

### [ISSUE-017] 기기 설정만 저장하면 재부팅 뒤 기기 탭에 머물러 `시작 화면`이 무시되고 자동 순환이 시작되지 않음

* **위치:** [src/provision/PortalRunner.cpp:38-51](src/provision/PortalRunner.cpp#L38-L51) `RestartAfterConfig`, [src/power/RtcPersistence.cpp:64-78](src/power/RtcPersistence.cpp#L64-L78) `RestoreRuntime`, [src/main.cpp:138-139](src/main.cpp#L138-L139), [src/app/PageRotation.cpp:15](src/app/PageRotation.cpp#L15)
* **우선순위:** Medium (기능이 조용히 적용되지 않음. 버튼 한 번으로 회피 가능)
* **신뢰도:** Confirmed (코드 흐름)
* **문제:** 부팅 시 `g_state.page = settings.display_page`로 시작 화면을 정한 직후, `RestoreRuntime`이 RTC 블롭의 페이지로 덮어쓴다. 데이터 지문이 바뀌지 않는 저장(기기 설정 탭)은 캐시를 유지하려고 RTC 블롭을 지우지 않는다(ADR 0026). 그런데 USB의 버튼 모드 LAN 포털은 열릴 때 `state.page = Device`로 바꾸고 그 사이클 끝에서 RTC에 기록하며, 저장 시에는 `CloseLan`(이전 화면 복귀)을 거치지 않고 바로 재시작한다. 따라서 재부팅 뒤 페이지는 항상 `Device`다.
* **발생 조건:** USB 전원, LAN 포털(버튼 모드)에서 `기기 설정` 탭만 저장. `시작 화면`과 `자동 순환`이 바로 이 탭에 있으므로, 이 설정을 바꾸는 정상 사용 경로 전체가 해당한다.
* **영향:**
  - `시작 화면`을 바꿔도 재부팅 뒤 기기 탭이 뜬다. 전원을 완전히 뺐다 꽂을 때만 적용된다.
  - `DecideRotation`은 생활·시장 탭에서만 동작한다. 자동 순환을 켜고 저장하면 기기 탭에 머문 채 순환이 시작되지 않는다. 사용자가 버튼으로 탭을 넘겨야 한다.
* **근거:**
  ```cpp
  // main.cpp
  g_state.page = (ui::Page)g_state.settings.display_page;
  power::RestoreRuntime(g_nvs, g_state, g_snap, g_kis);      // 아래에서 page를 덮어씀
  // RtcPersistence.cpp
  if (rtc::Decode(g_rtc_blob, ...)) { state.page = (ui::Page)page; ... }
  // PortalRunner.cpp — 기기 설정만 바뀐 저장
  power::RequestAuthClearOnBoot();   // RTC 블롭 유지
  g_lan.Close();  ESP.restart();     // CloseLan(state) 미호출 → return_page 복귀 없음
  // PageRotation.cpp
  if (in.page != ui::Page::Life && in.page != ui::Page::Market) return out;
  ```
* **반증 확인:**
  - 데이터가 바뀐 저장과 공장 초기화는 `InvalidateForPortal`이 `g_rtc_length=0`으로 만들어 정상 적용된다. 문제는 기기 설정만 바뀐 저장에 한정된다.
  - 전원 인가 리셋은 RTC를 버리므로 정상이다.
  - 상시(Always) 모드는 포털이 페이지를 바꾸지 않아 "저장 전 보던 탭"으로 돌아온다. `시작 화면`이 무시되는 점은 같다.
  - `display_page` 관련 테스트는 코덱·폼 값만 검증한다(`test_lan_portal.inc:483`, `test_lan_portal2.inc:141`). 재부팅 후 적용을 확인하는 테스트는 없다.
* **호출/영향 범위:** `TickLanPortal` → `HandleEvent(Saved)` → `RestartAfterConfig` → 재부팅 → `setup` → `RestoreRuntime` → `rtc::Decode`. 영향: `main.cpp` 자동 순환 블록, `FramePresenter`.
* **권장 수정 방향:** 설정 저장으로 인한 재시작이면 RTC의 페이지를 쓰지 않고 `settings.display_page`로 시작한다. 예를 들어 `RequestAuthClearOnBoot`와 같은 방식의 RTC 플래그를 두고 `RestoreRuntime`에서 페이지 복원을 건너뛴다.
* **필요한 회귀 테스트:** RTC 블롭 `page=Device`, 설정 `display_page=Market`, "설정 저장 재시작" 플래그가 선 상태에서 복원을 실행한다. 기대: `state.page == Market`. 플래그가 없으면(Deep Sleep wake) `state.page == Device`로 기존 동작 유지.

### [ISSUE-018] 시계 미검증 + 저장된 Wi-Fi 접속 불가(USB) 상태에서 30초마다 20초씩 루프가 막혀 버튼 입력이 유실됨

* **위치:** [src/power/CyclePlanner.cpp:70-76](src/power/CyclePlanner.cpp#L70-L76), [src/app/CycleRunner.cpp:62-70](src/app/CycleRunner.cpp#L62-L70), [src/net/WifiConnector.cpp:55-74](src/net/WifiConnector.cpp#L55-L74) `ConnectWifi`, [src/main.cpp:194-207, 251-256](src/main.cpp#L194-L256)
* **우선순위:** Medium (드문 조건이지만 복구 수단인 포털 열기를 방해함)
* **신뢰도:** Likely
* **문제:** 시계가 검증되지 않으면 `PlanCycle`은 타이머 사이클마다 `sync_clock=true`를 반환한다. `RunCycle`은 이때 `ConnectWifi`를 호출하고, 이 함수는 연결될 때까지 최대 20초(`kWifiAttemptTimeoutMs`) 블로킹한다. 공급자 실패와 달리 이 경로에는 백오프가 없어 USB에서는 30초 주기 중 20초가 매번 막힌다. 버튼 디코더는 `loop()`에서만 샘플링하므로, 막힌 구간 안에서 누르고 뗀 입력은 감지되지 않는다.
* **발생 조건:** USB 전원 + Wi-Fi SSID가 저장돼 있으나 접속 불가(공유기 교체·이사·비밀번호 변경) + RTC 무효(배터리 완전 방전 뒤 등). 배터리 방전 후 다른 장소에서 다시 켜는 경우가 여기에 해당한다.
* **영향:** 탭 넘기기와 **녹색 2초 길게 누르기(포털 열기)** 가 약 2/3 확률로 무시된다. 이 상태에서 벗어나려면 포털로 Wi-Fi를 고쳐야 하는데 그 입력이 불안정해진다. 부팅 직후 첫 화면도 `setup`의 선행 연결 20초 + 사이클 내 재시도 20초로 40초 이상 늦는다.
* **근거:**
  ```cpp
  // CyclePlanner.cpp
  if (!in.clock_verified) { p.sync_clock = true; p.sleep_seconds = 30 * 60; p.render = true; return p; }
  // CycleRunner.cpp
  const bool need_network = ... (plan.connect_wifi || plan.sync_clock);
  if (need_network && budget.CanRequest()) { wifi = platform.ConnectWifi(state.settings); ... }
  // WifiConnector.cpp
  while (WiFi.status() != WL_CONNECTED && millis() - start < kWifiAttemptTimeoutMs) delay(200);
  ```
  `sleep_seconds = 30분`은 배터리 수면에만 쓰인다. USB에서는 `main.cpp`의 `kPassEveryMs`(30초)가 주기를 정한다.
* **반증 확인:**
  - 시계가 검증된 상태에서 Wi-Fi가 끊기면 `MarkNetworkUnavailable`이 공급자에 백오프를 걸어 블로킹이 5분에 한두 번으로 줄어든다. 문제는 시계 미검증 경로에만 있다.
  - `WifiSupervisor`가 시도 중이어도(`AttemptInFlight`) `ConnectWifi`는 재시작만 생략할 뿐 같은 20초를 기다린다.
  - SSID가 비어 있으면 `ConnectWifi`가 즉시 반환하므로 미설정 기기는 해당하지 않는다.
  - 워치독(90초)은 넘지 않는다. 버튼을 22초 이상 누르고 있으면 길게 누르기는 감지된다.
* **호출/영향 범위:** `loop` → `RunCycle(Timer)` → `PlanCycle` → `ICyclePlatform::ConnectWifi` → `net::ConnectWifi`. 영향: `ButtonDecoder`, `TickLanPortal`(포털 요청 처리도 같은 구간 동안 멈춤).
* **권장 수정 방향:** USB 상시 구동에서는 `MaintainWifi`(논블로킹 감시자)가 이미 재연결을 맡고 있다. 시계 동기화만 필요한 사이클에서는 블로킹 연결을 하지 않고 "이미 연결돼 있으면 NTP 시도"로 바꾸거나, 시계 미검증 재시도에도 백오프를 둔다.
* **필요한 회귀 테스트:** 호스트 `RunCycle`에 가짜 플랫폼(`usb=true`, `time_verified=false`, `ConnectWifi`가 항상 실패)을 물려 타이머 사이클 10회를 돌린다. 기대: `ConnectWifi` 호출 횟수가 백오프 상한 이하(현재는 10회).

## 5. Potential Functional Gaps

| ID | 분류 | 내용 |
|---|---|---|
| G-1 | Confirmed Gap | **배터리 모드의 실제 수면은 최대 15분이며 wake마다 전체 갱신이 일어날 가능성이 높다.** `RunCycle`이 다음 wake를 `Sht40.last_attempt_utc + 900`으로 제한한다([CycleRunner.cpp:105-106](src/app/CycleRunner.cpp#L105-L106)). 또 Deep Sleep 뒤에는 내용 해시(`g_last_content_hash`)가 없어 전체 CRC로 비교하므로, 헤더 시계만 달라도 약 4.7초 전체 갱신을 한다([FramePresenter.cpp:96-97](src/ui/FramePresenter.cpp#L96-L97)). 하루 최대 96회 wake + 갱신이 배터리 수명에 주는 영향은 측정하지 못했다(추정). 내용 해시를 RTC에 함께 보존하면 줄일 수 있다. |
| G-2 | Confirmed Gap | **사이클 시간 예산(60초)이 공급자 처리 도중 소진되면 보내지 않은 요청이 실패로 기록된다.** `Get`/`Post`는 `!Budget(p)`일 때 `deferred`를 켜지 않고 기본값(`ProviderError`)을 반환하므로 `MarkResult`가 백오프를 건다([Fetcher.cpp:64-72](src/app/Fetcher.cpp#L64-L72)). 느린 네트워크에서 다종목(KIS·금융위) 조회 중에만 생기며 영향은 "다음 조회 지연 + 오류 표시"다. 요청 수 상한 초과는 이미 지연(deferred)으로 올바르게 처리된다. |
| G-3 | 추정 | **다음 해 공휴일 표가 비어 있으면 1시간마다 재요청한다.** `ParseHolidayYear`는 `totalCount=0`을 `NoData`로 반환하고 `RefreshCalendar`는 인증·쿼터 외 실패를 3600초 뒤 재시도한다. 천문연구원이 다음 해 자료를 늦게 공개하는 기간이 있다면 하루 최대 24건이 추가된다. 실제 API의 공개 시점은 확인하지 못했다. |
| G-4 | Confirmed Gap | **ESP32 어댑터 계층에 자동 테스트가 없다.** `EspClockSync`, `Pmic`, `WifiConnector`, `PortalServer_esp32`, `SleepController`, `RtcPersistence`(복원 순서), `main.cpp` 조립부. 이번 이슈 4건이 모두 이 계층이나 이 계층과 코어의 조합에서 나왔다. |
| G-5 | 추정 | **NVS 여유 공간이 확인되지 않았다.** `run.a/b`는 슬롯당 최대 약 4KB이고 갱신 시 새 블롭을 먼저 쓴다. 기본 NVS 파티션이 작다면 관심종목 4개 + 전체 공급자 상태에서 체크포인트가 실패할 수 있다. 실패는 감지·로그·재시도되며 데이터 손상은 아니다. 실제 파티션 크기와 블롭 크기는 재지 않았다. |
| G-6 | Likely Gap | **AP 포털은 Wi-Fi 접속 시험에 성공해야만 저장된다.** Wi-Fi가 일시적으로 안 되는 상황에서 API 키나 지역만 고치는 것은 불가능하다. 의도된 안전장치로 보이지만 안내 문구는 없다. |

## 6. Documentation Mismatches

| 문서 | 기술 | 실제 구현 |
|---|---|---|
| [README.md](README.md) §5 "배터리 전원 모드 … 깊은 잠(최대 6시간)" | 최대 6시간 수면 | 일반 모드에서는 실내 센서 재측정 때문에 최대 15분이다(G-1). 6시간에 도달하는 것은 안전 모드뿐이고, 저전압 모드는 24시간이다. |
| [README.md](README.md) §5 "시계 숫자만 바뀐 경우는 30분 동안 다시 그리지 않습니다" | 조건 없이 서술 | USB 상시 구동에서만 성립한다. Deep Sleep 뒤 첫 렌더는 시계만 달라도 전체 갱신한다(G-1). |
| [README.md](README.md) §4 "시작 화면" | 포털에서 설정 가능 | 저장 직후 재부팅에는 적용되지 않는다(ISSUE-017). |
| [ADR 0028](docs/adr/0028-charge-protection-and-runtime-reliability.md) "한계" | 수면 중 USB 연결 시 80% 보호를 보장하지 않음 | 충전 중단 상태로 잠들었다면 반대로 충전이 막힌 채 유지된다(ISSUE-016). |
| [src/app/Coordinator.cpp:42-43](src/app/Coordinator.cpp#L42-L43) 주석 "Holidays are not modelled" | 공휴일 미반영 | `cal::IsKrxOpen`에 달력을 넘겨 반영하고 있다(낡은 주석, 동작 영향 없음). |

그 밖에 README의 버튼 조작, 포털 잠금(5회·5분), 수동 새로고침 쿨다운(120초), 워치독(90초), 종목명 76개, 인증 오류 하루 1회 재시도, 테스트 명령은 구현과 일치했다. `CLAUDE.md`는 저장소에 없다.

## 7. Recommended Fix Plan

실제 코드는 수정하지 않았다.

### Phase 1 — Immediate

1. **ISSUE-015:** 시스템 시계가 이미 NTP로 신뢰 상태면 `ApplyUtc`·`WriteRtc`를 건너뛴다. 새 NTP 동기화와 RTC 최초 시드에서만 시각을 쓴다.
2. **ISSUE-016:** 배터리 구동으로 수면에 들어가기 전 `CHG_CONFIG`를 충전 허용으로 복구한다. ADR 0028과 포털 안내문을 고친다.

### Phase 2 — Stability

3. **ISSUE-017:** 설정 저장 재시작에서는 RTC 페이지 복원을 생략하고 `display_page`를 쓴다.
4. **ISSUE-018:** USB에서 시계 동기화만을 위한 블로킹 Wi-Fi 연결을 없애거나 백오프를 둔다.
5. **G-2:** 시간 예산 소진으로 보내지 못한 요청은 실패가 아니라 지연으로 처리한다.
6. **G-1:** 내용 해시를 RTC 블롭에 보존해 배터리 wake의 불필요한 전체 갱신을 줄인다(전류 측정과 함께).

### Phase 3 — Structural

7. **G-4:** 시계 적용, 충전 수면 전이, 부팅 복원 순서를 `ICyclePlatform`처럼 포트로 분리해 호스트에서 검증한다. 특히 `EspClockPlatform`의 "현재 시각을 신뢰 NTP로 되돌려 주는" 계약을 `IClockPlatform`에 명시한다.
8. **G-5:** 최악 구성의 `run`·`set` 블롭 크기를 호스트 테스트로 고정하고 NVS 파티션 여유를 문서화한다.
9. §6의 README·ADR 문구를 실제 동작에 맞춘다.

## 8. Test Recommendations

### Unit

- **시계 무손실(ISSUE-015):** 밀리초 해상도 가짜 시계, `trusted_ntp=true`, `attempt=false`로 `BootstrapClock` 120회 → 시스템 시각 변화 0ms, `ApplyUtc` 0회, `WriteRtc` 0회.
- **시계 최초 시드:** `trusted_ntp=false`, RTC 유효 → `ApplyUtc` 정확히 1회, 값은 RTC 시각.
- **충전 수면 전이(ISSUE-016):** 가짜 레지스터 REG01=`0x0A`(충전 중단), USB 없음 → 수면 준비 호출 후 REG01 bit4=1, bit6(WD_RST)는 쓰지 않음, 다른 비트 불변.
- **충전 히스테리시스 유지:** USB 연결 + 78% → 수면 준비를 호출하지 않는 한 상태 불변.
- **시간 예산 소진(G-2):** `IFetchControl::CanRequest`가 KIS 2번째 종목부터 false → `providers[KisPrice].attempts == 0`, `retry_after_utc` 불변, 성공한 1번째 종목 값 유지.

### Integration

- **시작 화면 적용(ISSUE-017):** RTC `page=Device` + 설정 `display_page=Market` + 설정 저장 재시작 표시 → 복원 뒤 `page == Market`, 스냅샷·공급자 상태는 RTC 값 유지.
- **Deep Sleep 페이지 유지:** 같은 입력에서 재시작 표시 없음 → `page == Device`.
- **시계 미검증 블로킹 상한(ISSUE-018):** `usb=true`, `time_verified=false`, `ConnectWifi` 항상 실패, 타이머 사이클 10회 → `ConnectWifi` 호출 수가 정한 상한 이하, 각 사이클의 `Millis` 진행이 상한 이하.
- **배터리 wake 갱신(G-1):** RTC에 렌더 CRC와 내용 해시 보존, 15분 뒤 같은 스냅샷 → `push_gray` 0회.

### End-to-End (실기)

- USB 24시간 구동 중 UART `time:` 로그와 호스트 시각을 10분마다 비교 → 편차 2초 이내, PCF8563 쓰기는 NTP 동기화 횟수 이하.
- 보호 켠 상태로 80% 이상에서 USB 분리 → 수면 확인 → 깨우지 않고 USB 재연결 → 충전 전류가 흐르는지 확인.
- LAN 포털에서 자동 순환을 켜고 저장 → 재부팅 뒤 설정한 시작 화면이 뜨고 설정 간격 뒤 순환이 시작되는지 확인.

### Concurrency

- Wi-Fi 이벤트 태스크와 루프 태스크 사이 공유 상태는 `std::atomic`(`g_pending_reason`, `g_ntp_notifications`)으로만 주고받아 추가 지적은 없다. SNTP 콜백이 `SyncNtp` 대기 중 들어오는 경우와 `ApplyUtc`가 겹치는 경우를 ISSUE-015 수정 뒤 실기에서 한 번 확인한다.

### Regression

- ISSUE-017 수정 뒤에도 "기기 설정만 바뀐 저장은 캐시 유지 + 인증 차단 해제"(`test_audit_fixes.inc`)가 통과해야 한다.
- ISSUE-018 수정 뒤에도 "수동 새로고침 + 시계 미검증 → NTP 시도"(`CyclePlanner` 버튼 경로)가 유지돼야 한다.

### Platform-specific

- 호스트 테스트는 CI에서 Windows·Linux·macOS 모두 돌린다. 추가 권장: `pio run -e reterminal_e1001` 빌드를 CI 한 곳(Linux)에 넣어 ESP 전용 코드의 컴파일 회귀를 잡는다. 현재 워크플로에는 펌웨어 빌드가 없다.

## 9. Final Assessment

| 항목 | 평가 | 근거 |
|---|---|---|
| Functional Correctness | **Acceptable** | 코어 로직은 탄탄하다. 기기 설정 저장 후 화면 복원(ISSUE-017)과 시계 절삭(ISSUE-015)이 남아 있다. |
| Runtime Stability | **Acceptable** | 워치독·크래시 가드·백오프가 갖춰져 있다. 시계 미검증 블로킹(ISSUE-018)과 충전 재개 지연(ISSUE-016)이 남아 있다. |
| Data Integrity | **Good** | 이중 슬롯 + CRC + read-back, 지문 바인딩, 쓰기 예산. 손상 경로를 찾지 못했다. 예외는 RTC 시각 덮어쓰기(ISSUE-015)다. |
| Error Resilience | **Good** | 공급자별 오류 분류, 부분 실패 집계, 인증·쿼터·속도 제한 구분, 저장 실패 재시도가 일관된다. |
| Cross-platform Robustness | **Good** | 호스트 도구·테스트가 3개 OS CI에 올라 있고 Windows에서 통과를 확인했다. 펌웨어 자체는 E1001 전용이다. |
| Test Confidence | **Acceptable** | 코어 348개 테스트는 충실하다. ESP 어댑터와 부팅 복원 조합은 자동 테스트가 없고, 장기·배터리 실기 검증은 `BLOCKED_HW`다. |

### 실제로 먼저 수정할 문제 3개

1. **ISSUE-015** — 사이클마다 시스템 시계를 초 단위로 절삭해 다시 쓰는 문제. USB 상시 구동 기기 전부에 해당하고 RTC까지 오염시킨다.
2. **ISSUE-016** — 충전 중단 상태가 수면 중 유지되는 문제. 저전압 기기에 충전기를 꽂아도 최대 24시간 충전되지 않는다.
3. **ISSUE-017** — 기기 설정 저장 후 기기 탭에 머무는 문제. `시작 화면`과 `자동 순환` 설정이 조용히 적용되지 않는다.
