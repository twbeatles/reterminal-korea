# 02. 펌웨어 아키텍처·도메인 계약

## 현재 디렉터리 (ADR 0009: `main.cpp`는 컴포지션 루트)

```text
src/main.cpp                      — 컴포지션 루트 (배선 + setup/loop만)
src/boot/{SerialLog,SettingsLoader}.*
src/app/{Coordinator,Fetcher,RequestBudget,DemoProvider,ProviderName,CalendarRefresh,CycleRunner,CyclePlatform,EspCyclePlatform}.*
src/domain/{Types,Freshness,ClockPolicy,Calendar}.*
src/drivers/{Board,ButtonDecoder,Pcf8563(+_esp32),SystemClock,ClockBootstrap,EspClockSync,
             Sht4xCodec(+_esp32),EpdPanel,Uc8179,Watchdog,Battery,Pmic,SensorCheck}.*
src/net/{Endpoints,NetPolicy,BodyReader,CaBundle,HttpTransport,WifiConnector}.*
src/providers/{ResultCodes,KmaParser,AirKoreaParser,EximParser,FinanceParsers,
               ProviderJson,MarketSession,HolidayService}.*
src/storage/{CacheStore,Codec,SettingsStore,SettingsCodec,SnapshotCodec,SlotStore,
             NvsStorage,RuntimeStore,CalendarStore}.*
src/power/{Schedule,PowerManager,BatteryPolicy,CyclePlanner,RtcState,RtcPersistence,SleepController}.*
src/provision/{SetupPortal,GeoGrid,PortalHandler,PortalServer(+_esp32),PortalRunner}.*
src/ui/{Layout,Pages,RefreshPolicy,Format,BitmapFont,Canvas,Renderer,Icons,ViewModel,FramePresenter}.*
src/ui/fonts/ + src/ui/icons/
src/util/{Log,BuildIdentity,SnapshotLog}.*
include/{Config,IClock,IHttpClient,IDisplay,IStorage,IRandom,dev_credentials.example}.h
test/{fixtures,golden}/  test_native/  host/kdash/ + host/tests/
tools/{...}/  evidence/{...}/
```

- `main.cpp`는 객체 배선과 `setup()`/`loop()` 생명주기만 소유한다. 비즈니스 로직은
  위 전담 모듈에 있으며, 모듈은 `include/` 추상(`IStorage`·`IHttpClient` 등)에만
  의존한다. 디바이스 전용 구현은 `.cpp` 안에서 `#if defined(ARDUINO)`로 가드하고,
  Host 빌드에서도 컴파일되도록 둔다.

## 의존 방향
`App Coordinator → Provider Interfaces / Domain → Platform Adapters`, `UI → Domain Snapshot`, `Schedule → Freshness/Clock`, `Board adapter → Vendor libraries`. UI는 직접 HTTPS 호출 불가, provider는 화면 픽셀 알 수 없음. 종속성 역전·Host Mock을 위해 인터페이스를 헤더로 분리.

## 권장 도메인 타입

```cpp
// 의사코드. 네이밍과 타입은 테스트에서 정한다.
enum class Validity { Valid, Missing, ProviderError, Stale, NotConfigured };
enum class QuoteKind { OfficialDailyFx, PriorClose, IntradayQuote };
struct Timestamp { int64_t epoch_utc; bool verified; };
template<class T> struct DataPoint {
  std::optional<T> value;
  Validity validity;
  Timestamp observed_at;  // API의 원본 관측·가격 기준 시각
  Timestamp fetched_at;   // 단말기가 수신한 시각
  const char* source;
};
struct FxRate { string currency_unit; int64_t won_scaled; int scale; QuoteKind quote_kind; };
struct StockQuote { string symbol; string market; int64_t price_krw; QuoteKind kind; Timestamp price_at; };
```

- 실제 구현에서 `std::optional`/`string` 등 메모리 부담이 큰 타입은 임베디드 적합하게 교체 가능. 단 **의미와 단위 계약은 유지**.
- 내부 가격은 단위가 있는 정수/고정소수점 우선, `NaN`, `0`, `-` 혼동 금지.
- `source_observed_at`와 `fetched_at`, `rendered_at`은 분리. KST 날짜 문자열은 UI에서만 생성.
- `service_authorized`, `source_delay`, `ttl` 등 데이터 신뢰/상태 메타를 모델로 보존.

## 요청 파이프라인

```text
BOOT → LOAD_CONFIG → (NO_WIFI_CONFIG? SETUP_PORTAL : WIFI_CONNECT)
→ SYNC_UTC/NTP_OR_RTC → DETERMINE_DUE → FETCH_WITH_BUDGET
→ VALIDATE_BODY_AND_SOURCE_TIME → NORMALIZE → CACHE_ATOMIC
→ COMPARE_DISPLAY_SNAPSHOT → RENDER_CHANGED_REGIONS → PLAN_NEXT_WAKE
→ (USB? IDLE : DEEP_SLEEP)
```

- 개별 API 오류는 전체 화면 실패로 이어지지 않는다.
- 2026-11-05: RunCycle은 ICyclePlatform을 통해 Host/E1001에서 동일하게 실행한다.
  기기 전용 효과는 EspCyclePlatform에 둔다. HTTP ready/deadline은 IHttpClient
  계약이며 concrete cast를 하지 않는다. 원본 시각·실패 표시 정책은 ADR 0010을 따른다.
- `fetch()`는 `Result<Snapshot,ProviderError>`를 반환하고 호출측이 캐시 사용 여부 결정.
- 모든 network request는 1회 timeout, 응답 크기 제한, 재시도 예산을 갖는다.
- 네트워크·렌더링·파일 기록을 무한 루프에서 돌리지 않음. 루프마다 **단일 책임과 종료조건** 필수.
- 컨트롤러가 scheduled refresh와 버튼 강제 갱신을 합치며 긴 버튼 누름은 설정 포털 진입 (단순 클릭과 중복되지 않음).

## 캐시 정책
- 데이터값 + source time + last_fetch_time + schema_version + checksum 보관.
- 설정/캐시는 원자적 쓰기(임시 파일→rename 또는 dual-slot+CRC). NVS 과도한 쓰기 금지.
- 과거 데이터를 보여줄 때 `<날씨: 1시간 전 관측>`처럼 stale 경고. 시간 검증 실패 시 '시간 확인 필요'를 화면에 표시.
- 이전 펌웨어의 설정 마이그레이션: versioned schema, 기본값 안전, 오류 설정은 별도 격리 후 재설정 포털.

## Host Preview 요구사항
- 동일한 UI layout logic을 Host mock display + virtual framebuffer로 렌더하여 PNG/PGM 저장(개발 도구만, 런타임 외부 PC 불필요).
- 시뮬레이션 시계 주입(정시/자정/휴일/서머타임 비해당), 네트워크 응답 fixture와 fault injection 제공.
- UI Goldens는 전자잉크 800×480의 흑백 또는 4-level Gray 모델 기준; PNG는 Preview 도구용일 뿐 E1001이 PNG 렌더링 서비스를 요청하는 설계가 아님.
