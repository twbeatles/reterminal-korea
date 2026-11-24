# 10. 출처·불확실성·의사결정 목록

## 2026-11-17 시장 상태 문구 수정

- `python tools/check_upstream.py`: 고정 `8d97d891` 대비 ahead=1, 변경 5개, E1001 관련 변경 0개. 로컬 공식 `vendor-upstream/examples/official/LVGLePaperStatusPanel/platformio.ini` 원문의 board/qio_opi/PSRAM/800×480을 대조했다. 하드웨어 설정은 유지한다.
- 공휴일 API와 거래소 일정의 기존 검증 경계를 유지하며, 미확인 일정을 휴장 가능성으로 표현하던 문구를 수정한다([ADR 0033](adr/0033-market-state-labels.md)). 신규 금융 일정이나 외부 API 계약은 이번 작업에서 검증하지 않았다.

## 2026-11-16 기기 공식 문서 재조사

- 공식 저장소 HEAD가 `51b92ab9`(2026-11-16)로 올라갔다. 고정 `8d97d891` 대비 변경은 README와 `web/` 5개 파일뿐이고 E1001 예제·보드 정의는 같다. 고정 SHA를 유지한다. 재점검은 `python tools/check_upstream.py`로 한다.
- Seeed 위키(시작 안내·개요 2026-09-10 갱신, Arduino/ESPHome 쿡북), Good Display GDEY075T7 제품 페이지, ESP-IDF S3 Sleep/GPIO/ADC 문서, ESP32-S3 errata를 대조했다. 결과는 [21번 감사 문서](21_DEVICE_DOCUMENT_AUDIT_2026-11-16.md)와 [ADR 0032](adr/0032-device-document-audit.md)에 있다.
- 미확인: GDEY075T7 사양서·사용 지침 PDF 원문(최소 갱신 간격, 갱신 후 전원 차단 규칙). Deep Sleep 중 GPIO21/16 레벨과 sleep 전류는 BLOCKED_HW다.

## 2026-11-16 런타임 감사 후속 대조

- 공식 고정 checkout `8d97d891`의 LVGLePaperStatusPanel/platformio.ini 원문 재대조: board/qio_opi/PSRAM/800×480 유지. COM5 esptool에서 ESP32-S3 rev0.2, PSRAM8MB, Flash32MB 재확인. 제품 E1001은 사용자 확인이며 esptool만으로 제품명을 식별하지 않는다.
- 설치 Arduino-ESP32 3.3.12의 SNTP/시각/NVS 헤더와 [Espressif SNTP 적용·알림](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/system_time.html), [NVS 페이지·엔트리 구조](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_flash.html)를 대조했다. SDK 적용 시각을 정수 초로 재설정하지 않는다.
- 기본 보드의 default_8MB 파티션·생성 바이너리에서 NVS20KiB 확인. 최대 길이 코덱 회귀 및 실기 NVS 통계를 추가했다. 파티션·핀·의존성·eFuse 변경 없음.
- 충전 ID=0x40 재확인. 수면 전 CHG_CONFIG 복구는 기존 SY6974B Rev0.9 근거를 사용한다. USB 탈착·충전 전류·SOC 교정·24시간 시계 정확도는 이 조치만으로 완료하지 않는다. [ADR 0031](adr/0031-audit-2026-11-16-runtime-fixes.md), [실행 증거](../evidence/audit-2026-11-16/summary.md).
- 외부 API 계약·다음 해 공휴일 실제 공개일은 재검증하지 않았다. 다음 해 NoData 재요청만 하루로 제한했다.

## 2026-11-13 장기 운영·충전 제어 대조

- 후속 실기: 사용자 USB 연결·기본 배터리 확인. COM5 ESP32-S3/8MB PSRAM/32MB Flash, PMIC `REG0B=0x40`을 읽었다. SY6974B 문서 PN/revision과 일치하며 PCB 실크 리비전·배터리 전류 실측은 여전히 미확인. [ADR 0028](adr/0028-charge-protection-and-runtime-reliability.md), [실기 증거](../evidence/long-term-device-2026-11-13/summary.md)가 아래 초기 BLOCKED 판정을 보완한다.

- [장기 운영 감사](17_LONG_TERM_OPERATIONS_AUDIT.md)에 원문·페이지·확인 범위를 정리했다. Seeed 공식 E1001 v1.2 회로도 변경 이력은 ETA6003 → SY6974B 교체를 명시한다. E1001 제품명이나 I2C 0x6B 응답만으로 충전 칩을 확정하지 않는다.
- Silergy SY6974B Rev.0.9의 CHG_CONFIG·전압 설정·watchdog 만료 시 default mode 복귀를 원문 확인했다. **사용자 실물 리비전·칩 ID·80% SOC 교정·충전 차단 전류는 BLOCKED_HW**. 이 확인으로 충전 제한 구현/실기 검증을 완료 처리하지 않는다.
- 고정 공식 LVGLePaperStatusPanel의 board/qio_opi/PSRAM/해상도 설정을 다시 대조했다. 현재 파티션과 설정을 유지한다. 기존 API 출처/약관은 이번 전원 감사에서 재검증하지 않았다.

## 2026-11-24 빠른 차분 갱신 출처 (확인일 2026-11-24)

- GDEY075T7 드라이버: GxEPD2 master [`src/gdey/GxEPD2_750_GDEY075T7.h`](https://github.com/ZinggJM/GxEPD2/blob/master/src/gdey/GxEPD2_750_GDEY075T7.h) / [`.cpp`](https://github.com/ZinggJM/GxEPD2/blob/master/src/gdey/GxEPD2_750_GDEY075T7.cpp). 공식 E1001 예제(`GxEPD2_reTerminal_E1001.ino`)가 이 클래스를 쓴다. ADR 0018 당시 404였던 경로는 `src/gdey/` 아래에 있었다. 원문 값: `hasFastPartialUpdate=true`, `useFastPartialUpdateFromOTP=true`(초기 배치 패널은 false), `partial_refresh_time=450`, `full_refresh_time=1200`, `_Init_Part` OTP fast = 0xE0 02 / 0xE5 6E. N회 권장과 잔상 언급은 없다. 커밋 해시는 기록하지 못했다. GPL-3.0이라 레지스터 값만 참고했다. 결정: [ADR 0034](adr/0034-fast-differential-refresh.md).

## 2026-11-24 외부 API 재대조 (확인일 2026-11-24, 실키 probe 없이 포털·호스트 대조만)

- 대조 범위: data.go.kr 상세 5종(KMA 15084084, AirKorea 15073861, EXIM 3068846, FSC 15094808, 특일 15012690)의 수정일·operation·호스트와 KIS 개발자센터 공지·공개 샘플을 `src/net/Endpoints.*`·`host/kdash/endpoints.py`·`NetPolicy.h`와 대조했다. 실키 probe를 실행하지 않았으므로 U-05/06/08/09 게이트는 유지한다.
- KMA: 수정일 2026-10-19 유지. `VilageFcstInfoService_2.0` + `getUltraSrtNcst`/`getVilageFcst`, 정시/30분 `base_time` 표기 동일. 변경 없음.
- AirKorea: 수정일 2026-06-30 유지. `ArpltnInforInqireSvc/getMsrstnAcctoRltmMesureDnsty` operation 동일. `ver=1.0`은 공개 예시(`ver=1.3`)와 다르나 2026-11-11 실키 응답(내부 코드 00)으로 검증된 현재값이므로 유지한다.
- EXIM: 수정일 2026-04-30 유지. 구 도메인(`www.koreaexim.go.kr`) 종료 예정일(2026-04-30)이 경과해 구 도메인은 더 이상 유효하지 않다. 코드·테스트에 구 도메인 잔재가 없음(`www.koreaexim` 검색: 문서 언급·거부 테스트만)을 확인했다. `oapi.koreaexim.go.kr` 유지. 변경 없음.
- FSC: 수정일 2026-10-28 유지. `GetStockSecuritiesInfoService_V2/getStockPriceInfo_V2` 유지. 이용허락 제4유형(출처표시·상업적 이용금지·변경금지, 제3자 재배포 금지) — 기기는 소유자 본인 기기 표시만 하므로 현행 준수. 변경 없음.
- KIS: `openapi.koreainvestment.com:9443`, `inquire-price`(FHKST01010100)·`inquire-index-price`(FHPUP02100000)·`oauth2/tokenP` 현행 유지(2026년 공개 문서 다수에서 동일 확인). 개발자센터 공지 2건 확인: 2026-10-27 국내주식 재무분석 7종 종료(우리가 쓰는 시세 조회가 아니므로 영향 없음), 2026-10-29 API 결과값 제공사항 변경(상세는 로그인 필요, 미확인) — 다음 실키 probe(U-08) 때 사용 필드(`stck_prpr`·`bstp_nmix_prpr`·`rt_cd`·`msg_cd`) 재확인을 남긴다.
- 특일: 수정일 2023-03-29 유지. `SpcdeInfoService/getRestDeInfo` 연간 조회 유지. 변경 없음.
- 만료 예정 리마인드(코드 변경 없음): GlobalSign Root R3 만료 2029-03-18(ADR 0003 D8) — 만료 전 `python tools/gen_ca_bundle.py --verify`로 체인 재확인 후 교체. 24절기 상수표는 2028-12 종료 — 달력은 올해+내년까지만 필요하므로 2028년 중 2029 fixture 확보가 필요하다.
- 결론: 이번 재대조에서 코드 변경이 필요한 API 변경점은 없다.

## 2026-11-12 LAN 설정 포털 설계 출처 (확인일 2026-11-12, 검색 요약 기준 — 원문 정독은 구현 시)

- Arduino-ESP32 STA 모드 HTTP 서버: [Espressif Arduino Wi-Fi API](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/wifi.html). `ESPmDNS` API 원문은 이번에 직접 열지 못했다 — 구현 전 설치된 코어 3.3.12 헤더로 확인.
- Android `.local`: [AOSP DNS Resolver](https://source.android.com/docs/core/ota/modular-system/dns-resolver)(2021-11부터 지원, VPN·모바일 데이터 제외). 기기별 실패 보고가 있어 IP를 기본 안내로 한다.
- 프로비저닝 대안: [ESP-IDF Unified Provisioning](https://docs.espressif.com/projects/esp-idf/en/latest/api-reference/provisioning/provisioning.html), [Improv Wi-Fi](https://improv-wifi.netlify.app), [tzapu/WiFiManager](https://github.com/tzapu/WiFiManager)(MIT).
- DNS 리바인딩: [PentesterLab](https://pentesterlab.com/glossary/dns-rebinding), [Acar 외 2018](https://faculty.cc.gatech.edu/~frankli/papers/li-sigcomm-iot2018.pdf). Chrome [Local Network Access](https://developer.chrome.com/blog/local-network-access)(141~142, 데스크톱 우선, 버전 표기 출처마다 다름).
- NVS 암호화: [ESP-IDF NVS Encryption ESP32-S3](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/storage/nvs_encryption.html) — eFuse 영구 사용, docs/11에 따라 손대지 않음.
- 결정: [ADR 0024 (제안)](adr/0024-lan-settings-portal.md), 설계: [LAN 설정 포털](superpowers/specs/2026-11-12-lan-settings-portal-design.md).

## 2026-11-11 패널 반복 갱신·글꼴·종목명 출처 (확인일 2026-11-11)

- microSD 핀과 공유 버스: [Seeed Arduino Onboard Peripherals](https://wiki.seeedstudio.com/reterminal_e10xx_with_arduino_peripherals/)의 코드에서 `SD_EN_PIN 16`(전원, HIGH로 켬), `SD_DET_PIN 15`(LOW=카드 있음), `SD_CS_PIN 14`, SCK/MOSI 7/9가 ePaper와 공유됨을 확인했다. EN LOW=전원 차단은 코드에서 추론했고 회로도로 확인하지 못했다. [ESPHome RTC/SD/Mic](https://wiki.seeedstudio.com/reterminal_e10xx_with_esphome_rtc_sd_microphone/)의 "카드를 꽂아 두면 공유 SPI 때문에 ePaper가 갱신되지 않을 수 있다"는 FAQ는 요약본으로만 확인했다.
- 공유 SPI 초기화 원칙: [ESP-IDF sdspi_share](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/sdspi_share.html) — 다른 장치 CS를 idle(high)로 고정하고 SD를 먼저 SPI 모드로 넣어야 하며, SD 모드의 카드는 CS와 무관하게 버스에 응답할 수 있다. SenseCraft HMI 소스(`reterminal_e1001.cpp`)는 디스플레이 begin 전에 SD 핀을 초기화한다. 결정: [ADR 0013](adr/0013-shared-spi-sd-guard.md).
- 패널 파형 판별/극성: 고정 공식 Seeed_GFX의 `uc8179ProbeOtpSupport`(읽기 전용)를 따랐고, 4계조 극성은 공식 관례(`11`=흰색)와 달리 이 기기에서 `00`=흰색임을 실기로 확인했다([ADR 0012](adr/0012-repeated-panel-refresh.md)). 이유 미확인.
- 글꼴: google/fonts `ofl/ibmplexsanskr`(OFL-1.1) Regular/Medium/SemiBold. Regular의 SHA256이 기존 입력과 일치([ADR 0014](adr/0014-font-weight-and-units.md)).
- 미확정: 애널리스트 컨센서스 공식 무료 출처 없음(KIS/FSC에 해당 API 없음). 종목명은 FSC `itmsNm`이 있으나 오후 호출이라 오프라인 표를 쓴다([ADR 0015](adr/0015-watchlist-stock-names.md)).

## 2026-11-11 첫 플래싱 준비 대조

- 실제 API 후속: 비밀번호 정정 후 Wi-Fi/NTP/EXIM/공휴일 기기 수신 확인. KMA/Air의 HTTP/1.0 응답에도 chunked framing이 존재함을 인증된 PC 요청으로 확인했고, HTTP adapter를 수정했다(ADR 0011). FSC 공식 페이지 Swagger에서 `GetStockSecuritiesInfoService_V2/getStockPriceInfo_V2`를 확인하고 인증된 종가 응답을 수집했다. 개별 PC/Build/Hardware 완료 경계는 [실 API 보고](../evidence/live-api-integration/summary.md)를 따른다.

- 후속 실기 연결: 드라이버 설치 후 COM5 esptool flash-id로 ESP32-S3 rev v0.2/8MB embedded PSRAM/32MB Flash를 확인하고 기본 PlatformIO 업로드·해시 검증·부팅에 성공했다. 사용자 대시보드 표시 확인. Wi-Fi 실패로 실 API/시각은 미검증이다. [최신 실기 보고](../evidence/first-device-flash/summary.md). 아래 준비 시점의 “실기 접근 미완료”는 이 후속 실행 이전 상태이며 모든 실기 게이트 완료를 뜻하지 않는다.

- [첫 플래싱 가이드](14_FIRST_FLASH_GUIDE.md)에 서비스별 신청/키 매핑·Windows 업로드·실기 체크리스트를 정리했다.
- Seeed [E1001 시작 안내](https://wiki.seeedstudio.com/getting_started_with_reterminal_e1001/)에서 제품 메모리·2.4GHz Wi-Fi·전원·USB 드라이버·sleep 중 업로드 주의·Full Flash 삭제 경고를 확인했다. 수동 BOOT/RESET 조합은 이 페이지로 확인하지 못했으므로 추측하지 않는다.
- 고정 공식 checkout 8d97d891의 LVGL platformio.ini에서 board/qio_opi/PSRAM/해상도를 재대조했다. 로컬 설정의 플랫폼 버전 고정 및 직접 UC8179 드라이버 선택은 기존 ADR을 유지한다.
- KMA/AirKorea/EXIM/FSC/특일(15012690) 상세 페이지와 KIS 공식 GitHub를 열어 서비스·인증 종류를 대조했다. EXIM 상세 수정일은 2026-04-30, 특일은 2023-03-29로 표시된다. 최신 상세 페이지 열람은 개인별 승인/쿼터·첨부 계약 전체·실키 호출 검증을 대체하지 않는다.
- U-02/03/04~09/11의 실기·실응답 게이트는 유지한다. 기기 도착 소식만으로 이 세션의 실기 접근/검증을 완료 처리하지 않는다.

## 2026-11-05 후속 대조

- 고정 Seeed checkout 8d97d891의 공식 LVGLePaperStatusPanel 설정을 재확인했다.
  E1001 board/qio_opi/PSRAM 설정을 유지하며 실물 flash 용량 확인을 대체하지 않는다.
- 설치 Arduino-ESP32 3.3.12의 esp_task_wdt.h와 qio_opi sdkconfig를 읽었다.
  SDK 기본 timeout=5초, idle CPU0 구독 설정을 확인했다. 앱의 90초 재설정/현재
  task 등록/진행 Feed와 전역 timeout 변경 영향은 ADR 0010에 명시했다.
- data.go.kr 최신 계약 본문/실응답·KRX/NXT 운영 계약은 이번 소프트웨어 수정으로
  확인 완료 처리하지 않는다. U-04~09 및 실기 U-02/03/11 게이트를 유지한다.

## 우선 출처 (확인일 2026-11-01)
1. Seeed 공식 E-Series Hub와 보드·예제: https://github.com/Seeed-Projects/OSHW-reTerminal-Series-E-D
2. Seeed Wiki 제품 개요: https://wiki.seeedstudio.com/reterminal_e10xx_main_page/
3. Seeed LVGL 개발 가이드: https://wiki.seeedstudio.com/work_with_lvgl/ (사이트 문서명·링크 유효성 에이전트 재확인)
4. KMA 공공데이터포털: https://www.data.go.kr/data/15084084/openapi.do (2026-10-19 수정)
5. AirKorea 공공데이터포털: https://www.data.go.kr/data/15073861/openapi.do (2026-06-30 수정)
6. EXIM 환율 API: https://www.data.go.kr/data/3068846/openapi.do (신규 host `oapi.koreaexim.go.kr` 안내)
7. FSC 종가: https://www.data.go.kr/data/15094808/openapi.do (전일/다음 영업일 13시 제약 확인 필요)
8. 한국투자 Open API: https://apiportal.koreainvestment.com/ , https://github.com/koreainvestment/open-trading-api (READ only quote REST / 샘플 변경 가능)
9. 국내 ESP32 동향 참고: https://github.com/mokhwasomssi/IOT_TableClock (E1001 전용 아님)
10. 국내 날씨 ESP32 제작: https://pashiran.tistory.com/category/Making/ESP32%20%EB%82%A0%EC%94%A8%20%ED%91%9C%EC%8B%9C%EA%B8%B0 (E1001 전용 아님)
11. 국내 ePaper 구현: https://qsurf.tistory.com/66 (별도 HA/backend 기반, 본 설계와 다름)
12. ESP32 ePaper 날씨 독립형 참고: https://github.com/maurokenny/esp32-WeatherPortal-EPD (보드 다름)
13. UI 아이콘: https://github.com/google/material-design-icons (`variablefont/MaterialSymbolsOutlined[FILL,GRAD,opsz,wght].ttf`, Apache-2.0, 10,744,832 B, sha256 `f7837ba8573d9a7a74931618ced8307bc81f96d49af296bf6c8052c772ed5282`, 확인일 2026-11-02)

## 구현 전 검증할 미확인 목록
| ID | 질문 | 검증 방법 | 중요도 |
|---|---|---|---|
| U-01 | E1001 정식 예제 LVGL vs GxEPD2 빌드·Heap 비용 | 원본 소스 + 실빌드 + 계측 | P0 필수 |
| U-02 | 현재 E1001 핀맵/RTC·BTN wake 및 배터리 ADC 스케일 | 공식 base 예제 + 실기 | P0/P4 필수 |
| U-03 | ePaper 4-level gray와 partial refresh 조합의 지원/잔상 (2026-11-24: 흑백 빠른 차분 + 4계조 Full 구현, 설정 기본 꺼짐, ADR 0034. 실기: 빠른 갱신 동작·극성 정상 확인, 잔상 100회 측정은 남음) | 실기 100회 측정 | P4 필수 |
| U-04 | API별 키 발급·실 통신·응답 헤더/최대 크기 | 공식 가이드 + 실키 프로브 | P3 필수(인증 미보유시 BLOCKED) |
| U-05 | 기상청 발표 스케줄의 실 제공 지연 및 KMA 격자 파일 | 2026-10 공식 첨부 가이드 | P3 필수 |
| U-06 | AirKorea 최신 operation parameters, 관측소 선택 | 공식 문서 + 실응답 | P3 필수 |
| U-07 | EXIM JPY(100) 및 공휴일 검색 결과의 보장 범위 | 실제 3개 날짜 프로브 | P3 필수 |
| U-08 | KIS 현재가·시장 통합거래시간·토큰 갱신 제한 및 약관 | 개발자센터·현행 샘플 + 본인 키 | P5 필수 |
| U-09 | FSC 종가 API 정확한 path/트래픽/기준일 | 공식 기술문서 + 실응답 | P5 필수 |
| U-10 | 한글 폰트 subset 크기/누락 문자 | 도구·실기 UI 골든 검증 | P1 필수 |
| U-11 | flash encryption / secure boot / local OTA 설정 위험 | 칩 스펙 + 위협 모델 + 사용자 승인 | P6 선택 |

## ADR 형식 (docs/adr/NNN-*.md)
`상태 / 날짜 / 질문 / 선택 / 다른 선택지 / 근거 / 검증 증거 / 되돌리는 방법`.
- 서비스 URL·API 스키마가 달라진 경우 스펙 출처부터 교체하고, 테스트 fixture를 새로 남긴다.
- 소스 자료에 있는 GitHub 사례가 한국형 완성제품이라는 주장은 하지 않는다.
