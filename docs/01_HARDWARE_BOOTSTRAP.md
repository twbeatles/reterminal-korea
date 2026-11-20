# 01. 하드웨어 베이스라인·개발환경·복구 지시

## 공식 기반 저장소
- https://github.com/Seeed-Projects/OSHW-reTerminal-Series-E-D
- https://wiki.seeedstudio.com/reterminal_e10xx_main_page/
- https://github.com/Seeed-Projects/OSHW-reTerminal-Series-E-D/tree/main/examples/official/LVGLePaperStatusPanel
- `examples/base/` 중 `RTC_PCF8563`, `LowPower_DeepSleep`, `UserButtons`, `Battery_Monitor`, `SHT4x_Sensor`, `GxEPD2_reTerminal_E1001`, `GxEPD2_reTerminal_E1001_Gray4`.

## 제약·확인사항
- E1001 = ESP32-S3, 7.5인치 800×480 4단계 회색조, 3 물리 버튼, PCF8563 RTC, SHT40 계열, 2.4GHz Wi-Fi. 32MB Flash 및 OPI PSRAM은 **제조사 펌웨어 예제의 빌드 설정으로 최종 확인**.
- 공식 LVGL 프로젝트의 환경은 `reterminal_e1001`. 아래 명령은 **공식 저장소 체크아웃 폴더**에서만 실행.

```sh
pio run -d examples/official/LVGLePaperStatusPanel -e reterminal_e1001
```

- 공식 저장소의 아두이노 예제 빌드는 제품에 따라 Seeed_GFX 등 라이브러리 설치가 필요할 수 있음. 공식 README 명령을 확인하고, 목표는 실제 공식 컴파일 성공을 캡처하는 것.
- 하드웨어 핀맵·display power enable·배터리 ADC 스케일·버튼 wake GPIO를 AI가 암기한 값으로 쓰지 않는다. **예제 코드 헤더를 1차 출처**로 삼아 `hardware/board_e1001.md`에 핀 표를 추출한다. 허용되는 작업 전까지 임의 플래시 금지.

## P0 실행 절차
1. 공식 저장소를 **읽기 전용 참고**로 clone하고 커밋 SHA를 `hardware/board_e1001.md`에 기록한다. 새로운 프로젝트와 섞지 않는다.
2. PlatformIO Core 설치 및 버전 확인. Windows PowerShell과 Unix shell 모두 문서화.
3. 공식 LVGL E1001 환경 `pio run` 성공; 실패하면 도구/패키지/소스 오류를 정확히 분류.
4. 공식 GxEPD2 예제에서 구현된 부분/전체/Gray4 갱신 방식과 메모리 모델을 조사하고, 두 엔진의 800×480 렌더 비용을 비교표로 작성. 단순 표시에는 GxEPD2 우선, UI가 복잡하면 LVGL 검토.
5. 자체 프로젝트의 최소 `main.cpp`에서 boot, heap/PSRAM, reset reason, fw version, RTC 시간 상태를 **비밀정보 없이** UART에 출력.
6. 컴파일만으로는 부팅 성공으로 판정하지 않는다. 실기를 확보하면 디스플레이 test pattern, 버튼 3개, 센서, RTC, 배터리 ADC, deep sleep/wake 순서로 실행한다.
7. 부팅·초기화 실패 시 E1001 공식 flash recovery 문서를 찾아 정확한 방법을 기록. 포트/부트 모드/USB 장치를 사전 확인 없이 지우지 않는다.

## 하드웨어 테스트 체크리스트
- [ ] USB 포트, 포트명, USB 드라이버, 보드 부트 모드 기록 (기기 연결 시)
- [ ] Full refresh 한글 테스트 이미지 및 사진
- [ ] 버튼 3개 short/long press 이벤트 30회씩 테스트
- [ ] SHT4x 3회 측정 및 실내 다른 기기와 차이 기록
- [ ] RTC 설정→전원차단→복구, NTP 불가 시 상태 표시
- [ ] 배터리 전압의 유효 범위/플러그 상태 판별 가능한지 계측
- [ ] Deep Sleep→예약기상 / 버튼기상 20회 반복
- [ ] 하드웨어 기능 미확인 시 `BLOCKED_HW`로 남기고 Host 개발 진행

## 산출물
`hardware/board_e1001.md`, `hardware/driver_comparison.md`, `docs/BUILD.md`, `evidence/p0/` (빌드로그·보드사진·리셋 이유·실측 기록), 핀맵·버전 잠금파일, 최소 프로젝트.

## 주의: 플래시 설정
파티션 테이블·FlashSize·OTA 슬롯·파일시스템 크기는 공식 설정과 **실제 32MB 하드웨어**를 검증한 뒤 정한다. 과거 예제의 8MB FQBN을 E1001 32MB 장비에 무비판 적용하지 않는다. OTA 파티션을 넣은 뒤 실제 앱 크기·롤백 슬롯이 맞는지 확인한다.
