# hardware/reference — 공식 원문 보관 (읽기 전용)

향후 에이전트가 "원문이 뭐라고 했지?"를 물을 때 여기를 먼저 읽는다.
`vendor-upstream/` sparse checkout이 지워져도 이 폴더는 남는다.

## 출처·버전

- 저장소: https://github.com/Seeed-Projects/OSHW-reTerminal-Series-E-D
- 커밋: `8d97d8912987c07529287629f7e73039bb0e6384` (2026-08-31, 2026-11-02에
  GitHub API로 HEAD 일치 확인)
- 파일명 규칙: 원문 경로의 `/`를 `__`로 바꾼 것. 예:
  `examples/base/RTC_PCF8563/RTC_PCF8563.ino`
  → `examples__base__RTC_PCF8563__RTC_PCF8563.ino`
- 내려받기: `python tools/fetch_reference.py` (URL·커밋 고정)

## 라이선스

원문 저장소 LICENSE는 **MIT** (Copyright (c) 2026 Seeed-Projects, 전문은
`LICENSE` 파일에 보관). MIT 조건에 따라 원저작자·허가 고지를 유지한다.
이 폴더의 파일은 참조용이며 펌웨어에 `#include`하거나 복사-편입하지 않는다.
`GxEPD2`(GPL-3.0)는 의도적으로 제외했다.

## 파일별 용도

| 파일 | 확인한 사실 |
|---|---|
| `RTC_PCF8563.ino` | 레지스터맵·세기비트(C=1→1900s)·init 순서·400kHz·VL 규칙 |
| `Battery_Monitor.ino` | ADC1·enable21·12bit·11dB·5ms·×2 분압 |
| `UserButtons.ino` | KEY0/1/2 = 3/4/5, HW 풀업, 50ms debounce |
| `LowPower_DeepSleep.ino` | EXT1 GPIO3 wake, RTC wake 미사용, 8MB 보드 주석 |
| `SHT4x_Sensor.ino` | SHT40 0x44, 고정밀 측정 |
| `GxEPD2_reTerminal_E1001.ino` | EPD 핀·GDEY075T7·HSPI 2MHz·paged 16KB |
| `GxEPD2_..._Gray4.ino` | UC8179 init·LUT·BUSY 극성·0x10/0x13→0x12→0x02→0x07 |
| `Buzzer_Control.ino` | 부저 GPIO45 |
| `board_registry.h` | E1001 = combo 520, 7_5_gray4_800_480 |
| `reterminal_e1001/config.h` | 전 핀맵·PMIC 0x6B·Wire1 39/40·배터리 101점 곡선 |
| `reterminal_e1001.cpp` | Wire 400kHz·PMIC/RTC/env init 순서 |
| `pmic_sy6974.h/.cpp` | REG0A bit7 USB 판정 (읽기 전용 사용) |
| `rtc_pcf8563.h/.cpp` | 알람 미사용 (U-14 근거) |
| `battery_gauge.h/.cpp` | 10회·25ms·단순평균·보간식 (우리 샘플링 계약의 원본) |
| `app_power_manager.h/.cpp` | 순정 60s owner-guard 수면 모델 (참고용, 우리와 구조 다름) |
| `SY6974.h/.cpp` | REG08 매핑·isUsbAttached (동작 참조만, 라이브러리 미편입) |

## 위키 스펙

https://wiki.seeedstudio.com/getting_started_with_reterminal_e1001/ (2026-11-02):
ESP32-S3+PSRAM 8MB, Flash 32MB, 800x480 mono, SHT계열 센서, 부저, 2000mAh,
USB-C 5V/1A, 0–40°C. "3개월" 문구는 근거로 사용 금지.
