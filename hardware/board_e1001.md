# hardware/board_e1001.md — 핀맵·보드 설정 (P0-02/P0-03)

> 1차 출처: 공식 저장소 예제 원문 (AGENTS.md 규정). AI 암기값 사용 없음.

- 공식 저장소: https://github.com/Seeed-Projects/OSHW-reTerminal-Series-E-D
- 공식 커밋 SHA: `8d97d8912987c07529287629f7e73039bb0e6384`
  (2026-11-02 sparse checkout, `vendor-upstream/`, 읽기 전용 참고 — 커밋 대상 아님)
- Wiki: https://wiki.seeedstudio.com/reterminal_e10xx_main_page/
- LVGL env: `reterminal_e1001`
  (`examples/official/LVGLePaperStatusPanel`, `pio run -d ... -e reterminal_e1001`)
- LVGL 공식 빌드 설정: `board = seeed_xiao_esp32s3`,
  `board_build.arduino.memory_type = qio_opi`,
  `-D RETERMINAL_E1001`, `-D EPAPER_LVGL_HOR_RES=800`,
  `-D EPAPER_LVGL_VER_RES=480`, `Seeed_GFX` + `lvgl@^9.2.0`,
  LVGL draw buffer 20 lines, mono threshold 180,
  `BOARD_SCREEN_COMBO 520` (E1001, Seeed_GFX 내부 처리 — 사용자 GPIO 아님)

## 핀표 (공식 예제 원문 대조 완료, 실기 동작은 BLOCKED_HW)

| 신호 | GPIO | 출처 예제 | 비고 |
|---|---|---|---|
| EPD SCK | 7 | `examples/base/GxEPD2_reTerminal_E1001` | HSPI, 2MHz, MODE0 |
| EPD MOSI | 9 | 동상 | `hspi.begin(SCK,-1,MOSI,-1)` |
| EPD CS | 10 | 동상 | |
| EPD DC | 11 | 동상 | |
| EPD RST | 12 | 동상 | OUTPUT |
| EPD BUSY | 13 | 동상 | 패널 GDEY075T7, page buffer 16000B |
| KEY0 (새로고침 / 길게=portal, wake, 앞에서 볼 때 오른쪽 녹색) | 3 | `UserButtons`, `LowPower_DeepSleep` | HW pull-up, INPUT, 예제 debounce 50ms |
| KEY1 (다음 화면, 가운데 회색) | 4 | `UserButtons` | wake 가능 (GPIO0–21 EXT1) |
| KEY2 (이전 화면, 왼쪽 회색) | 5 | `UserButtons` | wake 가능 (GPIO0–21 EXT1) |
| wake 방식 | 3 (KEY0) | `LowPower_DeepSleep` | `esp_sleep_enable_ext1_wakeup(1ULL<<3, ANY_LOW)` + `rtc_gpio_pullup_en` |
| 배터리 ADC | 1 | `Battery_Monitor` | 12bit, 11dB, `analogReadMilliVolts` |
| 배터리 enable | 21 | `Battery_Monitor` | HIGH 측정, LOW 차단, 분압 ×2 |
| I2C SDA / SCL | 19 / 20 | `SHT4x_Sensor`, `RTC_PCF8563` | 공유 버스, 400kHz (`Wire.setClock`, SenseCraft 보드 init과 동일) |
| PMIC SY6974 (USB 감지) | 0x6B, Wire1 SDA39/SCL40 | SenseCraft `pmic_sy6974`+`SY6974` lib | 읽기 전용. REG0A bit7=USB 연결, REG08 bits[7:5]!=0=입력 존재 (U-12 해결) |
| 부저 / 녹색 LED | 45 / 6 | SenseCraft E1001 `config.h` | |
| SD SCK/MISO/MOSI/CS/EN/DET | 7/8/9/14/16/15 | 동상 | SPI 핀을 EPD와 공유 (미사용). 패널 트래픽 전 CS=HIGH, EN=LOW 고정 필수 — 미고정 시 첫 프레임 이후 갱신 미반영 실기 확인 ([ADR 0013](../docs/adr/0013-shared-spi-sd-guard.md)) |
| 터치 INT/RST (GT911) | 47/48 | 동상 | (미사용) |
| SHT40 주소 | 0x44 | `SHT4x_Sensor` | `SensirionI2cSht4x` 라이브러리 |
| PCF8563 주소 | 0x51 | `RTC_PCF8563` | CR1220, VL 플래그로 초기화 판정 |
| USB-UART 브리지 | RX 44 / TX 43 | 전 예제 공통 | `Serial1` 115200 — USB-CDC `Serial` 아님. 보드 정의가 `ARDUINO_USB_CDC_ON_BOOT=1`이라 `Serial`=내장 USB-CDC (src/main.cpp는 `Serial1` 사용) |

## 미확정·주의 (실기 검증 전)

- 2026-11-16: 공식 HEAD `51b92ab9`와 비교했다. 변경은 README와 `web/`뿐이고 위 핀표의 출처 파일은 그대로라 고정 SHA를 유지한다. 재점검: `python tools/check_upstream.py`. Deep Sleep 중 GPIO21(배터리 enable)과 GPIO16(SD EN)은 레벨을 유지하지 않는다(공식 예제와 같음). 전류 측정 전에는 hold를 추가하지 않는다([21번 감사](../docs/21_DEVICE_DOCUMENT_AUDIT_2026-11-16.md) DD-05).

- 2026-11-11 첫 연결: COM5의 CH340 경유 `esptool v5.4.0 --chip esp32s3 --port COM5 --baud 115200 flash-id`에서 ESP32-S3 rev v0.2, embedded PSRAM 8MB, Flash manufacturer ef/device 4019, detected 32MB를 확인했다. 기본 파티션은 이번 업로드에서 유지했다. RAM/Flash 모든 영역이나 OTA/복구를 검증한 것은 아니다. [실기 보고](../evidence/first-device-flash/summary.md).

- Flash: 위키 스펙상 **32MB** (XIAO 예제 주석의 8MB는 보드 기본값). 실기에서
  `esptool flash_id` + `ESP.getFlashChipSize()` 로그로 확정 후 파티션표 작성
  (ADR 0005). RAM 21.3% / Flash 37.7% (2026-11-02 빌드).
- ePaper 전원 enable: LVGL 경로는 Seeed_GFX combo 520 내부 처리.
  GxEPD2 경로의 별도 전원 핀은 예제에 없음 — 실기 확인 필요.
- deep sleep 14µA는 데모 단독 수치이며 대시보드 기대 수명이 아님.
- 부분갱신/Gray4 지원·잔상은 실기 100회 측정 필요 (U-03).
- 제품 팩트 정리 `hardware/official_product_facts.md` (2026-11-02 조사).
  U-12(PMIC SY6974으로 해결), U-13(GxEPD2=GPL-3.0 확인), U-14(RTC 알람 미사용 확인),
  U-17(공식 101점 곡선 확보), U-16(직접 구현이라 불필요). 남은 것은 U-15(실기 100회).
