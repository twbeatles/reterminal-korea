# ADR 0032 — 기기 공식 문서 재조사 후속

- 상태: 채택. 실기 전류·저온·부분 갱신 검증은 별도 게이트.
- 날짜: 2026-11-16 KST
- 질문: Seeed 위키, Good Display 패널, Espressif 문서를 새로 대조한 결과 중 무엇을 지금 코드에 반영하는가?

## 선택

- `ui::kMaxPartialsBeforeFull`을 10에서 5로 바꾼다. Good Display GDEY075T7은 빠른·부분 갱신을 5회 연속한 뒤 전체 갱신을 권장한다. 부분 갱신은 여전히 비활성(U-15)이다.
- `battery::kAdcSettleMs`를 5ms에서 10ms로 바꾼다. Seeed E1001 위키 팁을 따른다. 공식 스케치의 5ms보다 길어 공식 범위를 벗어나지 않는다.
- `ui::PanelTempInRange`(0–40°C)를 추가한다. SHT40 값이 범위 밖이면 UART 로그만 남기고 갱신은 막지 않는다.
- `tools/check_upstream.py`로 고정 공식 SHA와 HEAD를 비교해 E1001 관련 파일 변경만 보고한다.

## 다른 선택지

- Deep Sleep GPIO hold 추가: 공식 예제와 SenseCraft 모두 쓰지 않는다. 회로도 풀 저항과 전류 측정 없이 넣으면 wake 후 hold 해제를 빠뜨렸을 때 배터리 측정이 멈추는 회귀 위험이 있다. 보류.
- 저온에서 갱신 금지: 대시보드가 멈춘다. 실측 근거가 없다. 보류.
- OTP 온도 강제값(E5=5F) 변경: 공식 Seeed_GFX와 같은 값이다. 실측 없이 바꾸지 않는다.

## 검증 증거

[증거 보고](../../evidence/device-docs-audit-2026-11-16/summary.md). Host와 Build까지만 검증했다. 실기 동작은 확인하지 않았다.

## 되돌리는 방법

두 상수를 이전 값(10, 5)으로 되돌린다. `EspCyclePlatform::ReadIndoor`의 로그 분기를 제거한다. 그리고 `test_device_docs_20261116.inc`와 `test_stage2.cpp`의 단언을 같이 되돌린다.
