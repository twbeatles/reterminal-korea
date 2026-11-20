# hardware/driver_comparison.md — 렌더 엔진 선택 비교 (P0-03)

> 상태: `BLOCKED_HW`/`BLOCKED_TOOLCHAIN`. 아래는 선택 기준과 측정 계획이며,
> Heap/PSRAM/갱신시간 수치는 공식 빌드+실기 계측 후 기입한다. 추정치로
> 선택을 확정하지 않는다.

## 후보 (공식 원문 @ 8d97d891 대조 완료)

1. GxEPD2 (`examples/base/GxEPD2_reTerminal_E1001`, Gray4 예제 별도 존재)
   - 패널 `GDEY075T7`, 핀 SCK7/MOSI9/CS10/DC11/RST12/BUSY13, HSPI 2MHz.
   - 예제는 page buffer 16000B 분할 렌더 (`MAX_DISPLAY_BUFFER_SIZE`).
     800x480 1bpp 전체 프레임(48KB)은 PSRAM 사용 또는 분할 렌더로 처리.
   - 4-level gray + partial 조합의 지원/잔상은 실기 100회 측정 필요 (U-03).
2. Seeed_GFX + LVGL status panel (순정 E1001 화면 타입은 7_5_gray4_800_480 — Gray4 모드. 우리 1bpp 경로는 단순화 버전이며 Gray 확장은 T04d 후속) (`examples/official/LVGLePaperStatusPanel`)
   - `board=seeed_xiao_esp32s3`, `qio_opi`, `RETERMINAL_E1001`, 800x480,
     `Seeed_GFX` + `lvgl@^9.2.0`, draw buffer 20 lines, mono threshold 180,
     `BOARD_SCREEN_COMBO 520`. EPD 전원 제어는 Seeed_GFX 내부 처리.

## 결정 규칙 (P0-03 중간 결론 — 실측 후 ADR 확정)

- GxEPD2는 **GPL-3.0** (ZinggJM/GxEPD2 LICENSE 원문 확인, U-13 해결) — 사용 시
  배포물 전체에 소스 제공 의무가 따른다. 따라서 **자체 UC8179 드라이버 우선**
  (ADR 0004 채택, 공식 Gray4 예제의 init 시퀀스·LUT를 참조). UI가 복잡해질 때만
  LVGL 검토. GxEPD2는 배포 방침이 GPL-3.0을 허용할 때만 대안으로 둔다.
- 최종 선택은 `pio run -e reterminal_e1001` 양쪽 빌드 + 실기 full/partial
  갱신시간·ghosting 기록 후 ADR에 남긴다.

## 측정표 (LVGL 빌드 실측 완료 2026-11-02, 나머지는 실기 후 기입)

| 항목 | GxEPD2 full | GxEPD2 partial | LVGL full | 비고 |
|---|---|---|---|---|
| 빌드 RAM | — | — | 89,244B / 327,680B (27.2%) | `evidence/p0/build.log` |
| 빌드 Flash | — | — | 1,208,523B / 3,342,336B (36.2%) | firmware.bin 1,218,256B |
| 빌드 시간 | — | — | 9분 9초 | PlatformIO 6.2.0, esptool 5.4.0 |
| 갱신 시간 | — | — | — | 800x480 실기 측정 필요 |
| ghosting (10회 누적) | — | — | — | 사진 증거 |
