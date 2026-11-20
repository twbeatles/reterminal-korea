# ADR 0004 — ePaper 드라이버 선택 (T04d, 미확정)

- 상태: 채택 (U-13 확인 완료: GxEPD2 = GPL-3.0)
- 날짜: 2026-11-02 (KST)
- 질문: GxEPD2(`GxEPD2_750_GDEY075T7`)를 쓰는가, 자체 UC8179 드라이버를 쓰는가.
- 선택: **자체 UC8179 드라이버 우선**. GxEPD2는 GPL-3.0이라 배포물 전체에 소스
  제공 의무가 따른다. 공식 Gray4 예제에 init 전체(0x01/0x30/0x82/0x06/0x04/0x00/
  0xE3/0x50/0x52/0x61 + LUT 0x20~0x24)와 BUSY 극성(idle HIGH)이 있어 구현 근거가
  있다. 현재 src/drivers/Uc8179.cpp가 초기화/LUT/프레임 전송을 구현하며
  EpdPanel::push_full을 통해 실제 FramePresenter에 연결돼 있다.
  성공한 전송만 last_rendered_at/CRC를 갱신한다. 이는 Host/Build 연결 상태이며
  실제 패널 파형/잔상 검증은 BLOCKED_HW다. 기존 IDisplay::refresh의 false 스텁은
  이 production 경로가 아니다.
  GxEPD2는 배포 방침이 GPL-3.0을 허용할 때만 대안으로 둔다.
- 고정값(공식 원문): SCK 7, MOSI 9, CS 10, DC 11, RST 12, BUSY 13,
  HSPI 2MHz MODE0, BUSY 타임아웃 30s.
- `IDisplay`는 이번 단계에서 바꾸지 않는다(기존 `refresh`/`full_refresh` 유지).
  T04d 제안(`push_full`/`push_partial`/`hibernate`)은 `EpdPanel`에 두었고,
  실기 검증 후 `IDisplay` 개정 여부를 결정한다.
- 부분 갱신: `RefreshCaps.partial_supported=false` 고정 (U-15 실측 전까지 Full만).
- 되돌리는 방법: U-13 결과에 따라 GxEPD2 의존 고정 또는 자체 드라이버 구현.
