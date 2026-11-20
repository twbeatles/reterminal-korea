# T06 — 버튼: 디바운스, 짧게/길게, wake 버튼 판별

- 상태: DONE_VERIFIED_HOST + BUILD (오인식률 실기 BLOCKED_HW, 디바운스 50ms 출발값)
- 선행: 없음. T05와 함께 통합합니다.
- 필요 자원: 실기(최종)
- 관련: docs/05 "버튼 매핑", `src/drivers/Board.h`(`button_event/button_longpress`), 공식 예제 `UserButtons`, `LowPower_DeepSleep`

## 1. 하드웨어 사실 (공식 예제 원문)

- KEY0=GPIO3, KEY1=GPIO4, KEY2=GPIO5.
- 하드웨어 풀업이 있어 `pinMode(pin, INPUT)`을 씁니다. 눌림 = LOW.
- 예제 디바운스는 50ms입니다. **출발값일 뿐이며** 실기 측정 후 확정합니다(docs/05).

## 2. 매핑 (docs/05 + RECOVERY.md)

| 버튼 | 짧게 | 길게 (≥ 2.0s, 출발값) |
|---|---|---|
| KEY0 | 이전 페이지 | 설정 포털 진입 |
| KEY1 | 다음 페이지 | (예약: 없음) |
| KEY2 | 강제 새로고침 | (예약: 없음) |

- 버튼의 물리 방향·각인이 UI 안내와 같은지 실기 사진으로 확인합니다. 다르면 매핑을 바꾸고 ADR에 남깁니다.
- 공장초기화는 버튼 조합으로 만들지 않습니다. 포털의 2단계 확인 경로만 사용합니다(실수 방지).

## 3. 설계 — 순수 상태기계 (`src/drivers/ButtonDecoder.{h,cpp}`, 이식)

```cpp
struct ButtonSample { uint32_t t_ms; uint8_t pressed_mask; };  // bit0=KEY0...
enum class ButtonEvent : uint8_t { None, ShortKey0, ShortKey1, ShortKey2, LongKey0 };
class ButtonDecoder {
 public:
  explicit ButtonDecoder(uint16_t debounce_ms = 50, uint16_t long_ms = 2000);
  ButtonEvent Feed(const ButtonSample& s);  // 샘플마다 호출
};
```

- 규칙
  - 안정 상태가 `debounce_ms` 이상 유지돼야 상태 변화로 인정합니다.
  - 짧게는 **떼는 순간**에 발생합니다(길게와 중복되지 않게).
  - 길게는 `long_ms`에 도달하는 순간 한 번만 발생하고, 이후 떼어도 짧게를 내지 않습니다.
  - 두 버튼을 동시에 누르면 먼저 눌린 버튼만 인정합니다(예측 가능성).
- wake 직후 처리 (`WakeButtons(ext1_mask, read_pin_now, sample_for_ms)`)
  - EXT1로 깨어났다면 이미 한 번 눌린 것으로 간주합니다.
  - 버튼이 아직 눌려 있으면 최대 `long_ms`까지 샘플링해 길게 여부를 판정합니다.
  - 이미 떼었다면 짧게로 처리합니다. 디바운스 이전에 떼어진 짧은 탭도 짧게로 인정합니다(wake 자체가 증거).
- USB idle 모드에서는 `loop()`에서 10ms 주기로 샘플링합니다.

## 4. 테스트

| 샘플 시퀀스 | 기대 |
|---|---|
| 0ms 눌림, 30ms 뗌 | None (디바운스 미만 채터링) |
| 0ms 눌림, 120ms 뗌 | ShortKey0 (뗄 때) |
| 0–2,100ms 계속 누름 | 2,000ms 시점 LongKey0 1회, 뗄 때 None |
| 5ms 간격 on/off 채터링 300ms 후 안정적으로 누름, 200ms 뒤 뗌 | ShortKey0 1회 |
| KEY1 누른 상태에서 KEY2 누름 | KEY1 이벤트만 |
| wake mask=KEY2, 이미 떼어짐 | ShortKey2 |
| wake mask=KEY0, 2.1s 유지 | LongKey0 |

## 5. 완료 기준

- [ ] HOST 테스트 통과, `Board`의 `button_event/button_longpress`를 이 디코더 기반으로 구현, 빌드 성공
- [ ] 실기: 각 버튼 짧게 20회·길게 10회 오인식 0, 디바운스 최종값 기록 (BLOCKED_HW)
