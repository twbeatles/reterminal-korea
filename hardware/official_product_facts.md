# 공식 제품 팩트 — reTerminal E1001 (향후 에이전트 참조용)

> 조사일: 2026-11-02 (KST). 1차 출처만 기록한다. AI 암기값·추측 금지.
> 원문 전문은 `hardware/reference/`에 보관 (MIT, `tools/fetch_reference.py`로
> 재생성, GxEPD2 제외).
> 입력 패키지(`docs/00~10`)가 아니라 여기서 갱신한다.
> 공식 저장소 커밋: `8d97d8912987c07529287629f7e73039bb0e6384` (2026-08-31,
> 당일 GitHub API로 HEAD 일치 확인 — 이후 커밋 없음).

## 1. 제품 스펙 (위키)

- 출처: https://wiki.seeedstudio.com/getting_started_with_reterminal_e1001/
- MCU ESP32-S3 + PSRAM 8MB, **Flash 32MB**, MicroSD(최대 32GB, FAT32)
- 디스플레이 7.5" 모노크롬 800x480, Wi-Fi 2.4GHz b/g/n + BT 5.0
- 센서: 온습도(SHT40), 부저, 마이크(예약), **배터리 2000mAh**, USB-C 5V/1A
- 동작온도 0–40°C, 후면 전원 스위치, 녹색 상태 LED, 적색 충전 LED
- 8핀 확장 헤더(VDD/GND/UART/I2C/GPIO), 상단 버튼 3개
- 주의: "3개월 배터리 수명"은 마케팅 문구 — 수명 계산 근거로 사용 금지.
  순정 펌웨어는 개방 AP(비밀번호 없음)+캡티브 포털 방식을 쓴다(우리 T07의
  WPA2 방식과 다름 — 설계 선택 차이, 버그 아님).

### 1-1. 패널·칩 문서 추가 확인 (2026-11-16)

- Good Display GDEY075T7(https://www.good-display.com/product/396.html): 동작 0~50°C,
  보관 -25~70°C, 전체 3s / 빠른 1.5s / 부분 0.3s(25°C 측정), 4계조. 빠른·부분 갱신은
  5회 연속 뒤 전체 갱신 권장. 사양서·사용 지침 PDF 원문은 미확인.
- Seeed 개요: 기기 동작 0~40°C, E1001 갱신 2~5s, 부분 갱신 지원 표기.
- 위키 팁: 배터리 측정은 enable 후 10ms 대기가 더 정확(공식 스케치는 5ms).
- ESP32-S3 rev v0.2 errata 중 사용 기능(ADC1, deep sleep, CH340 UART 업로드)에 해당하는 항목 없음.
- 상세: `docs/21_DEVICE_DOCUMENT_AUDIT_2026-11-16.md`.

## 2. 보드 핀 (공식 SenseCraft_HMI `boards/reterminal_e1001/config.h`)

| 신호 | 값 | 비고 |
|---|---|---|
| KEY0/KEY1/KEY2 | GPIO 3/4/5 | HW 풀업, `INPUT`,_poll 10ms, debounce 50ms |
| EPD SCK/MOSI/CS/DC/RST/BUSY | 7/9/10/11/12/13 | HSPI 2MHz MODE0, 패널 GDEY075T7(UC8179) |
| I2C0 SDA/SCL (RTC·SHT40·터치) | 19/20 | 400kHz (`Wire.setClock(400000)`) |
| PMIC I2C1 SDA/SCL | 39/40 (Wire1) | SY6974 @0x6B, 400kHz |
| 배터리 ADC / enable | GPIO 1 / GPIO 21 | 12bit·11dB·mV 판독·분압 ×2, 측정 후 enable LOW |
| 부저 / 녹색 LED | GPIO 45 / GPIO 6 | |
| SD SCK/MISO/MOSI/CS/EN/DET | 7/8/9/14/16/15 | SPI 핀을 EPD와 공유 |
| 터치 INT/RST (GT911) | 47/48 | |
| UART (USB-UART 브리지) | RX 44 / TX 43 | `Serial1` 115200. `Serial`은 내장 USB-CDC라 E1001 USB-C로 안 나감 |

## 3. 해결된 미결 질문

- **U-12 (USB 전원 감지) — 해결**: VBUS GPIO는 없다. PMIC SY6974의
  REG0A(0x0A) bit7(`isUsbAttached`) 또는 REG08(0x08) bus-status bits[7:5]
  (!=0이면 입력 존재, 0은 "No input")로 판정한다. 근거: `lib/SY6974/SY6974.{h,cpp}`,
  `boards/common/pmic_sy6974.cpp`. 우리 구현 `src/drivers/Pmic.*` (읽기 전용 —
  PMIC 설정 레지스터에는 절대 쓰지 않는다).
- **U-13 (GxEPD2 라이선스) — 해결**: GPL-3.0 (ZinggJM/GxEPD2 LICENSE 원문 확인).
  결론: 자체 UC8179 드라이버 우선 (ADR 0004 채택). GxEPD2 사용 시 배포물 전체에
  GPL-3.0 소스 제공 의무가 따른다.
- **U-14 (RTC 알람 wake) — 해결(소스상)**: 공식 RTC 드라이버(`rtc_pcf8563.cpp`)는
  시각 읽기/쓰기만 하고 알람 레지스터를 전혀 건드리지 않는다. E1001 설정에 RTC
  INT 핀도 없다. LowPower 예제는 EXT1 GPIO3 wake만 쓴다. 결론: ESP32 타이머
  wake만 사용. T11에서 실기 재확인 1회.
- **U-17 (방전 곡선) — 해결**: 공식 101점 %→V 테이블을 그대로 사용한다
  (`config.h` BATTERY_CURVE, 0%→2.795V … 100%→4.111V). 우리 구현
  `src/power/BatteryPolicy.{h,cpp}` `PercentMv` (구간 선형보간). Low=20%
  (3464mV), Critical=5% (3252mV)는 이 곡선에서 유도.
- **U-16 (Sensirion 라이브러리) — 불필요**: 직접 구현(`Sht4xCodec`, 명령 0xFD,
  CRC-8 poly 0x31/init 0xFF, 공식 SHT4x_Sensor 예제와 동일 조건)이라 라이브러리를
  쓰지 않는다.

## 4. 여전히 실기가 필요한 것

- **U-15**: 부분 갱신 정렬 단위·잔상 — 실기 100회 (U-03과 동일).
- Flash 32MB: 위키 스펙상 32MB이나 XIAO 예제는 8MB 보드 기본값. 실기에서
  `esptool flash_id` + `ESP.getFlashChipSize()` 로그로 확정 후 파티션표 작성
  (ADR 0005).
- PMIC/곡선/버튼/배터리 판독은 전부 BLOCKED_HW 실측 대조가 남음 (T11).

## 5. 구현 시 주의 (원본 대조에서 발견한 함정)

- PCF8563 세기 비트: C=1 → 1900년대, C=0 → 2000년대 (공식 RTC 예제). 반대로
  구현하면 1999년을 2099년으로 오판한다. 우리 `Pcf8563.cpp` 수정됨.
- EPD BUSY 극성: idle HIGH / busy LOW (공식 Gray4 `checkBusy`). 반대로 대기하면
  30초 타임아웃까지 멈춘다. 우리 `EpdPanel.cpp` 수정됨.
- RTC init 순서: probe → CTRL1=0(STOP 해제) → CTRL2=0(알람/타이머 클리어) →
  CLKOUT=0(절전). 우리 `PcfInit()`에 반영.
- SY6974 주소는 **0x6B** (SY6970 계열 일반값 0x6A가 아님). Wire1 사용.
- PMIC에는 읽기만 한다. watchdog 해제·충전전류 같은 설정 쓰기는 순정 동작을
  바꾸므로 우리 펌웨어에서 하지 않는다.
- SD SPI 핀(7/9)이 EPD와 겹친다. SD를 쓴다면 배타 제어가 필요하다 (현재 미사용).
