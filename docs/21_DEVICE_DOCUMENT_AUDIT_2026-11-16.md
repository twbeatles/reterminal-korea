# 21. 기기 공식 문서 재조사 감사 (2026-11-16)

- 작성: 2026-11-16 KST. 기준 커밋: main `657338b`.
- 대상: Seeed reTerminal E1001만. E1002/E1003/E1004·XIAO 키트 내용은 비교용으로만 읽었고 적용하지 않았다.
- 목적: 제조사·칩·패널 공식 문서를 새로 읽고, 현재 펌웨어와 어긋나거나 위험한 지점을 찾는다. 결정은 [ADR 0032](adr/0032-device-document-audit.md), 실행 결과는 [증거 보고](../evidence/device-docs-audit-2026-11-16/summary.md)에 둔다.
- 경계: 이번 작업은 문서 대조, Host 테스트, 타깃 빌드까지다. 기기에 플래시하지 않았고 전류·온도·잔상은 측정하지 않았다. 아래의 "조치"는 컴파일과 Host 테스트를 통과했다는 뜻이다. 실기에서 정상 동작한다는 뜻은 아니다.

## 1. 조사한 출처

| 출처 | 확인 내용 | 페이지 표기 갱신일 |
|---|---|---|
| [E1001 시작 안내](https://wiki.seeedstudio.com/getting_started_with_reterminal_e1001/) | 스펙, LED, 2.4GHz 전용, sleep 중 플래시 불가, 배터리 측정 팁 | 2026-09-10 |
| [E 시리즈 개요](https://wiki.seeedstudio.com/reterminal_e10xx_main_page/) | 동작온도 0–40°C, 갱신 2–5초, 부분 갱신 지원 표기 | 2026-09-10 |
| [Arduino 주변장치 쿡북](https://wiki.seeedstudio.com/reterminal_e10xx_with_arduino_peripherals/) | 버튼 active-low/HW 풀업, 배터리 GPIO1/21, SD 공유 SPI | 2026-05-21(요약본 기준) |
| [Arduino RTC·저전력 쿡북](https://wiki.seeedstudio.com/reterminal_e10xx_with_arduino_peripherals_2/) | Deep Sleep 예제(EXT1 GPIO3, `rtc_gpio_pullup_en`, 약 14µA), RTC 알람 미사용 | 미표기 |
| [Arduino ePaper 쿡북](https://wiki.seeedstudio.com/reterminal_e10xx_with_arduino/) | combo 520, Gray4는 1비트보다 약 4배 느림, GxEPD2 예제는 `hibernate()`로 끝남 | 미표기 |
| [ESPHome RTC·SD·마이크](https://wiki.seeedstudio.com/reterminal_e10xx_with_esphome_rtc_sd_microphone/) | SD 카드가 꽂혀 있으면 ePaper가 갱신되지 않을 수 있음(FAQ), CR1220 | 2026-08-05(요약본 기준) |
| [Good Display GDEY075T7](https://www.good-display.com/product/396.html) | 패널 동작 0–50°C, 보관 -25–70°C, 전체 3초/빠른 1.5초/부분 0.3초(25°C), "빠른·부분 갱신 5회 연속 뒤 전체 갱신 권장" | 미표기 |
| [ESP-IDF Sleep Modes (S3)](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/system/sleep_modes.html) | EXT1 핀 상태 잠금, 풀업/풀다운 전류, `rtc_gpio_isolate` | v6.1.1 문서(펌웨어 SDK는 Arduino-ESP32 3.3.12 기반) |
| [ESP-IDF GPIO (S3)](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/peripherals/gpio.html) | Deep Sleep 중 디지털 GPIO 유지에는 `gpio_hold_en` + `gpio_deep_sleep_hold_en` 필요 | 동상 |
| [ESP32-S3 Errata](https://docs.espressif.com/projects/esp-chip-errata/en/latest/esp32s3/index.html) | rev v0.2 해당 항목 목록 | 미표기 |
| [ESP-IDF ADC 보정 (S3)](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/peripherals/adc/adc_calibration.html) | 기준전압 칩별 1000–1200mV 편차, 잡음 민감, curve fitting 보정 | 동상 |
| 공식 저장소 `Seeed-Projects/OSHW-reTerminal-Series-E-D` | 고정 SHA `8d97d891` → HEAD `51b92ab9`(2026-11-16) 비교, SenseCraft `EnterDeepSleep` 원문 | 커밋 시각 기준 |
| 고정 Seeed_GFX `0dfdd713`(로컬 `.pio/libdeps`) | `UC8179_INIT_GRAY_OTP`의 `E0=02`, `E5=5F` | 2026-09-04 커밋 |
| [Home Assistant 커뮤니티 E1002 스레드](https://community.home-assistant.io/t/seeed-studio-reterminal-e1002-color-epaper-display-home-assistant-dashboard/970545?page=2) | 사용자 보고: sleep 진입이 빠르면 화면 갱신이 끊긴다(E1001도 같다고 언급). 공식 문서가 아니며 ESPHome 사례다 | — |

열지 못한 원문: GDEY075T7 사양서 PDF와 Good Display 사용 지침 PDF는 다운로드 셸만 받았다. 최소 갱신 간격, 갱신 후 전원 차단 의무, UV 관련 내용은 **확인하지 못했다**. 추측해서 적지 않는다.

## 2. 공식 저장소 드리프트

고정 SHA `8d97d891`(2026-08-31) 이후 오늘 `51b92ab9` 커밋 1건이 생겼다. 바뀐 파일은 `README.md`와 `web/` 아래 4개뿐이다. E1001 예제, SenseCraft 보드 정의, SY6974 드라이버는 그대로다. **핀·파티션·의존성은 바꾸지 않는다.** 앞으로는 `python tools/check_upstream.py`로 같은 점검을 다시 실행한다(관련 파일이 바뀌면 exit 1).

## 3. 발견 사항과 조치

판정 기준: **조치**=이번에 코드 변경, **문서화**=실측 전이라 코드를 바꾸지 않음, **해당 없음**=문서와 대조한 결과 문제가 아님.

| ID | 우선 | 발견 | 근거 | 판정 |
|---|---|---|---|---|
| DD-01 | P2 | 부분 갱신 누적 상한이 10회였다. 패널 제조사는 5회마다 전체 갱신을 권장한다 | Good Display 제품 페이지 | **조치**: `kMaxPartialsBeforeFull=5`. 부분 갱신은 여전히 꺼져 있으므로(U-15) 지금 화면 동작은 바뀌지 않는다. 나중에 켤 때 잘못된 기본값을 쓰지 않게 하려는 변경이다 |
| DD-02 | P3 | 배터리 측정 enable 후 대기가 5ms였다. 위키는 10ms 대기가 더 정확하다고 안내한다 | E1001 시작 안내 팁, 공식 스케치는 5ms | **조치**: `kAdcSettleMs=10`. 측정 1회당 5ms가 늘어난다. 실제 정확도 향상은 측정하지 않았다 |
| DD-03 | P2 | 동작온도(기기 0–40°C, 패널 0–50°C) 밖에서 갱신해도 아무 기록이 남지 않았다 | 개요 페이지, GDEY075T7 | **조치**: SHT40 값이 0–40°C 밖이면 UART에 `display: indoor_c=… outside_operating_range` 로그를 남긴다. 갱신은 막지 않는다. SHT40은 보드 위 센서라 자체 발열이 섞일 수 있어 패널 온도의 참고값일 뿐이다 |
| DD-04 | P2 | OTP 파형 경로는 `E0=02`, `E5=5F`로 온도값을 고정해서 넣는다. 실제 온도에 따른 파형 보상이 없다. 레지스터 LUT 경로도 고정 LUT 하나만 쓴다 | 고정 Seeed_GFX `UC8179_INIT_GRAY_OTP`와 같은 값 | **문서화**: 공식과 동일하므로 유지한다. 저온(10°C 이하)에서 대비 저하나 잔상이 생길 수 있는지는 실기로 확인해야 한다(BLOCKED_HW) |
| DD-05 | P2 | Deep Sleep 동안 GPIO21(배터리 분압 enable), GPIO16(SD 전원), EPD 제어 핀의 레벨을 유지하지 않는다. ESP-IDF는 디지털 핀 유지에 `gpio_hold_en`과 `gpio_deep_sleep_hold_en`이 필요하다고 설명한다 | ESP-IDF GPIO/Sleep 문서. 공식 LowPower 예제와 SenseCraft `EnterDeepSleep`도 hold를 쓰지 않는다 | **문서화**: 회로도의 외부 풀 저항과 sleep 전류를 확인하기 전에는 바꾸지 않는다. hold를 넣으면 wake 후 해제하지 않을 때 배터리 측정이 멈추는 회귀 위험이 있다. 측정 절차는 §4 |
| DD-06 | P3 | EXT1 wake 핀에 `rtc_gpio_pullup_en`을 쓰지만 RTC_PERIPH 전원 유지(`esp_sleep_pd_config`)는 설정하지 않는다 | ESP-IDF Sleep 문서 | **문서화**: 보드에 하드웨어 풀업이 있어(위키) wake 자체는 내부 풀업 없이도 동작한다. 공식 예제와 같다. 내부·외부 풀업이 병렬이면 전류가 조금 늘 수 있는데, 이는 전류 측정 항목에 포함한다 |
| DD-07 | P3 | 공식 SenseCraft는 충전 중이면 Deep Sleep에 들어가지 않는다 | `app_device_info.cpp` `EnterDeepSleep` | **해당 없음**: 이 프로젝트도 USB 전원이면 수면하지 않는다(`CycleRunner`의 `if (!usb)`). 수면 중 USB를 연결했을 때 80% 제한이 보장되지 않는 점은 ADR 0031에 이미 적혀 있다 |
| DD-08 | P3 | 사용자 보고: sleep 진입이 너무 빠르면 화면 갱신이 중간에 끊긴다 | HA 커뮤니티(ESPHome) | **해당 없음**: 이 펌웨어는 갱신 후 BUSY idle을 기다리고, 전원 끄기(0x02)와 deep sleep(0x07)까지 마친 뒤에 수면한다. 갱신 시간 제한은 30초다 |
| DD-09 | P3 | 위키 쿡북은 Arduino 설정을 "Flash 8MB / Default 8MB"로 안내하지만, 제품 스펙과 실기는 32MB다 | 위키 간 불일치 | **해당 없음**: 현재 빌드는 8MB 기본 파티션을 쓰고 있어 공식 예제와 같다. 32MB 활용은 ADR 0005 결정대로 보류한다 |
| DD-10 | P3 | 위키 안에서 SD 최대 용량이 32GB와 64GB로 다르게 적혀 있다 | 시작 안내 vs 주변장치 쿡북 | **해당 없음**: SD를 쓰지 않는다. 공유 SPI 보호(ADR 0013)는 유지한다 |
| DD-11 | P3 | 기기가 sleep 상태이면 플래시가 되지 않는다 | 시작 안내 | **해당 없음**: [첫 플래싱 가이드](14_FIRST_FLASH_GUIDE.md) 문제 해결 표에 이미 있다 |
| DD-12 | P3 | ESP32-S3 rev v0.2 errata | Errata 요약 | **해당 없음**: ADC-183은 ADC2 DMA 문제인데 이 펌웨어는 ADC1(GPIO1)을 쓴다. RTC-126은 light sleep 문제인데 이 펌웨어는 deep sleep만 쓴다. USBOTG-4289는 USB-OTG 다운로드 문제인데 이 기기는 CH340 UART로 업로드한다. 그 밖의 CACHE-126, ANALOG-160 등은 제목만 확인했다 |
| DD-13 | P3 | ADC 기준전압은 칩마다 1000–1200mV로 다르고, ADC는 잡음에 민감하다 | ESP-IDF ADC 보정 | **문서화**: `analogReadMilliVolts`는 eFuse 보정을 쓰고 10회 평균을 낸다. 표시 %는 추정값이라는 기존 설명(17번 문서)을 유지한다. 개별 기기 오차는 멀티미터 대조가 필요하다 |
| DD-14 | P3 | "3개월 배터리"는 6시간 갱신 기준의 마케팅 수치다 | 시작 안내 | **해당 없음**: 이 펌웨어의 배터리 수면 주기는 15분/6시간/24시간(ADR 0031)이라 그 수치를 쓰지 않는다(기존 문서와 같음) |

## 4. 실기에서 확인할 항목 (BLOCKED_HW)

1. **Sleep 전류(DD-05/06)**: 배터리 단독으로 수면 시 전류를 측정한다. 기준은 공식 예제 약 14µA다(데모 단독 수치). 차이가 크면 회로도 p4에서 GPIO21과 GPIO16의 외부 풀 저항을 확인한다. 그다음 `gpio_hold_en(21)` + `gpio_deep_sleep_hold_en()`을 넣고 wake 직후 `gpio_hold_dis(21)`로 해제하는 방안을 별도 ADR로 비교한다.
2. **저온 화질(DD-04)**: 5–10°C 환경에서 전체 갱신 10회를 하며 대비와 잔상을 사진으로 남긴다. 같은 시점의 `display: indoor_c` 로그도 함께 기록한다.
3. **배터리 측정(DD-02/13)**: 멀티미터로 잰 셀 전압과 `charge: mv=` 로그를 3개 SOC 구간에서 비교한다.
4. **부분 갱신(DD-01, U-15)**: 켜기 전에 5회 누적 후 전체 갱신 주기에서 잔상을 확인한다.

## 5. 이번에 바꾸지 않은 것

핀, 파티션, PMIC 쓰기 범위, eFuse, TLS 검증, 외부 라이브러리는 바꾸지 않았다. 새 라이브러리도 추가하지 않았다. `tools/check_upstream.py`는 표준 라이브러리만 쓰고, 공개 GitHub API를 인증 없이 읽기만 한다.
