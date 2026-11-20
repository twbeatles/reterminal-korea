# ADR 0009 — `main.cpp` 분할, 컴포지션 루트로 축소

- 상태: 채택
- 날짜: 2026-11-03 (KST)
- 질문: 648줄 `main.cpp`에 있던 설정·Wi-Fi·시계·렌더·RTC·포털·달력·슬립·사이클
  로직을 어디에 두는가.
- 선택: 기존 폴더에 전담 모듈 11개를 신설하고(`boot/SerialLog·SettingsLoader`,
  `net/WifiConnector`, `drivers/EspClockSync`, `util/SnapshotLog`,
  `ui/FramePresenter`, `power/RtcPersistence·SleepController`,
  `provision/PortalRunner`, `app/CalendarRefresh·CycleRunner`),
  `main.cpp`는 객체 배선과 `setup()`/`loop()`만 남긴다.
- 이유: 단일 파일이 SRP를 위반하고, RTC 보존 상태·패널·NTP 알림 등 공유
  가변 상태가 전역에 흩어져 있었다. 모듈은 `IStorage`·`IHttpClient`·
  `IFetchControl`·`IClockPlatform` 추상에만 의존하고, 디바이스 코드는
  `#if defined(ARDUINO)` 가드로 Host 빌드와 함께 컴파일된다.
- 검증: 구 심볼·UART 로그 문구 전수 대조, host 149 passed, native 63 passed,
  `pio run -e reterminal_e1001` SUCCESS. 동작 변경 없음.
- 되돌리는 방법: 모듈별 `.h/.cpp`를 삭제하고 이 ADR을 폐기한 뒤 구
  `main.cpp`로 복원한다.
