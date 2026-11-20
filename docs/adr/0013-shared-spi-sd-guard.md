# ADR 0013 — 공유 SPI 버스의 microSD 슬롯 고정 (반복 화면 갱신 근본 원인)

- 상태: 채택, 실기 확인. 날짜 2026-11-11.
- 문제: 첫 부팅 화면만 패널에 표시되고, 이후 버튼/주기 갱신은 `display: ... ok=1`이어도 물리 화면이 바뀌지 않았다(ADR 0012의 전원 차단/슬립, 평면 단위 CS 유지, OTP 판별 모두 해결하지 못함).
- 계측 증거(같은 E1001, COM5):
  - 프레임 단계 trace(`epd: phase=...`)에서 리셋 30ms, 전원 켜기 140~170ms, 데이터 전송 약 1.0s로 첫 프레임과 이후 프레임이 같았다. 하지만 `0x12` 이후 BUSY 구간은 첫 프레임 3.7s, 이후 프레임 9.0s였다.
  - 이후 프레임의 약 10s는 레지스터 LUT 모드(9.95s)와 OTP+온도고정 모드(10.19s)에서 사실상 같았다. 즉 데이터가 딸린 설정 명령(PSR/LUT/E0/E5/DTM1/DTM2)이 반영되지 않고, 패널이 기본 OTP 파형으로 기존 RAM을 다시 그린 것으로 판단했다.
  - SD 핀 고정만 추가한 펌웨어(그 외 변경 없음)에서 이후 프레임 BUSY가 4.69s로 첫 프레임과 같아졌고, 사용자가 페이지 0→1→2 실제 전환을 육안으로 확인했다. 직전 펌웨어(trace만 추가)는 같은 날 실패했으므로 이 변경이 결정 요인이라는 A/B 근거가 된다.
- 원문 근거:
  - Seeed wiki `reterminal_e10xx_with_arduino_peripherals`: `SD_EN_PIN 16 // Power enable`, `SD_DET_PIN 15`(LOW=카드 있음), `SD_CS_PIN 14`, `SD_MOSI_PIN 9 // Shared with ePaper Display`, `SD_SCK_PIN 7 // Shared with ePaper Display`, 전원 켜기 `digitalWrite(SD_EN_PIN, HIGH)`.
  - Seeed SenseCraft HMI `reterminal_e1001.cpp`(hardware/reference): 디스플레이 `begin()` 전에 `sd_.initPins()`를 호출한다.
  - Espressif ESP-IDF `sdspi_share`: SPI 모드로 들어가지 않은 카드는 "CS가 선택되지 않아도 버스의 SPI 통신에 임의로 응답할 수 있다". 공유 시 다른 장치의 CS를 idle(high)로 고정하라고 권고한다.
  - Seeed ESPHome 위키 FAQ(요약본으로만 확인, 원문 미인용): 카드를 꽂아 두면 공유 SPI 때문에 ePaper가 제대로 갱신되지 않을 수 있다.
- 선택: `EpdPanel::begin()`에서 OTP probe와 `spi.begin()` 전에 SD CS(14)=HIGH, SD EN(16)=LOW(슬롯 전원 차단), DET(15)=INPUT_PULLUP으로 고정하고 `display: sd_card_present=..` 로그를 남긴다. 펌웨어는 SD를 쓰지 않는다.
- 미확정: 이번 기기는 `sd_card_present=0`(카드 없음)이었다. CS와 EN 중 어느 쪽이 결정적이었는지, 떠 있던 EN이 슬롯 부하 스위치를 반쯤 켜서 버스 부하/간섭을 만들었는지는 분리 실험하지 않았다. EN LOW=전원 차단은 공식 예제의 HIGH=켜기에서 추론한 것이고 회로도로 확인하지 않았다. 카드가 꽂힌 상태와 장시간 반복 갱신은 미검증이다.
- 대안: (a) SD 전원을 켜고 CMD0으로 SPI 모드에 넣은 뒤 패널 사용(Espressif 권장 순서). 하지만 SD를 쓰지 않는 펌웨어에 불필요한 복잡도와 전류가 늘어난다. (b) 사용자에게 카드 제거만 안내. 이번 기기는 카드가 없는데도 실패했으므로 불충분하다.
- 회귀: `host/tests/test_audit_epd_bus.py`가 핀 번호와 "패널 트래픽 전 SD 고정" 순서를 소스 수준에서 고정한다. 물리 동작은 실기에서만 검증한다.
- 영향/복원: 향후 SD 기능을 추가하면 이 고정을 대체하는 정식 버스 공유 초기화(ADR 필요)가 있어야 한다. 단계별 `epd:` trace 로그는 진단용으로 유지한다(비밀 없음).
