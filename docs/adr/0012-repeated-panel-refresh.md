# ADR 0012 — 패널 반복 갱신과 버튼 진단

- 상태: 공식 예제 순서로 보완. 근본 원인은 공유 SPI의 SD 슬롯 핀 미고정이었고 [ADR 0013](0013-shared-spi-sd-guard.md)에서 해결, 실기 화면 전환 확인. 날짜 2026-11-11.
- 질문: 첫 부팅 생활 화면은 실제 수치로 표시되는데 버튼으로 내부 페이지가 바뀌어도 사용자가 패널 변화를 보지 못하는 현상.
- 증거: 사용자 모든 버튼 무반응 보고 뒤 raw mask/decoded event/page before·after를 기록했다. 실제 COM5에서 mask2→0, event2, page0→1 및 mask1→0, event1, page1→0, display_failed=0을 관측했다. 입력 자체와 논리 페이지 전환은 동작하며 물리 화면 성공과 구분한다.
- 원문: Seeed 고정 commit 8d97d891의 `examples/base/GxEPD2_reTerminal_E1001_Gray4/*.ino`는 `initGrayMode → showGrayscaleDemo(refresh 포함) → sleepDisplay`를 수행한다. sleepDisplay는 Power Off 0x02·BUSY 대기·Deep Sleep 0x07/A5다. 기존 Uc8179 PushMonoFrame은 refresh 뒤 완료 반환하여 이 마지막 단계를 생략했다.
- 선택: 모든 full frame을 초기화/업로드/갱신/전원차단/패널 sleep으로 마치고, 다음 frame은 기존 hardware reset으로 시작한다. 단일 30초 deadline에서 남은 시간만 power-off/sleep에 전달한다. EpdPanel의 awake 상태도 성공 시 sleep으로 반영하여 중복 hibernate를 피한다.
- 진단: 기존 핀 GPIO3/4/5는 공식 UserButtons와 동일하게 유지. main은 raw mask가 바뀔 때만, decoded event가 있을 때만 비밀 없는 로그를 남긴다. FramePresenter는 page/CRC/소요시간/드라이버 반환만 기록한다. 이 로그의 ok를 사용자의 물리 화면 확인으로 격상하지 않는다.
- 검증: C++에서 동일 포트의 2회 frame과 power-off 순서를 확인하고 기존 BUSY timeout을 유지한다. 호스트 통과는 패널 물리 동작의 증명이 아니며 결과는 evidence/live-api-integration/summary.md에 기록한다.
- 대안: 버튼 핀 임의 교체/기능 재배정은 입력 로그에 반하므로 하지 않는다. 부분 갱신/새 라이브러리/파티션/설정 초기화는 추가하지 않는다.
- OTP 파형 극성(2026-11-11 추가): 이 E1001은 읽기 전용 OTP 판별에서 `display: waveform=internal-otp`로 나왔다. 처음에는 Seeed_GFX 4계조 관례(TFT_GRAY_3=흰색 → 0x10/0x13 둘 다 1, 즉 와이어 `11`=흰색)를 따라 OTP 경로에서 `ink = 3 - ink`로 반전해 보냈으나, 실기에서 사용자가 "글자 흰색, 배경 검은색" 반전 화면을 직접 보고했다. 따라서 이 패널에서는 OTP 모드도 고정 LUT 경로와 같은 `00=흰색, 11=검정` 극성으로 보낸다(평면 매핑 0x10=하위 비트, 0x13=상위 비트는 공식과 동일). 중간 계조 01/10의 밝기 순서는 실기 육안 확인 전까지 미검증이다. 공식 관례와 다른 이유는 확인하지 못했으며(4bpp 스프라이트 내부 표현 차이 가능성), 다른 패널 로트에서 다를 수 있다는 위험을 남긴다. native 테스트는 OTP 경로가 흰 프레임을 0x00으로 보내는지 고정한다.
- 영향/복원: 전원 종료가 이제 매 frame 포함된다. NVS wire schema/화면 모델은 유지한다. 문제가 지속되면 이전 commit과 정제 로그를 기준으로 드라이버를 USB 복구하며 미검증 상태를 유지한다.
