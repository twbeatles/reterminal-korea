# T08 — 실내 센서 SHT40 + 배터리 전압

- 상태: 코덱·정책 DONE_VERIFIED_HOST + BUILD (I2C·기준계·방전곡선 BLOCKED_HW, U-17 미확보)
- 선행: 없음. I2C 버스를 T03(RTC)과 공유합니다.
- 필요 자원: 실기, 배터리 방전 곡선 측정(U-17)
- 관련: P3-04, docs/06 "배터리", `src/drivers/SensorCheck.*`, `include/IStorage.h`의 `ISensor`, 공식 예제 `SHT4x_Sensor`, `Battery_Monitor`

## 1. SHT40

### 하드웨어 사실 (공식 예제)
- I2C SDA=19, SCL=20, 주소 0x44.
- 예제는 `SensirionI2cSht4x` 라이브러리의 `measureHighPrecision(t, rh)`를 씁니다.

### 결정: 라이브러리 대신 직접 구현 (권장)
- SHT4x 프로토콜은 단순합니다: 명령 `0xFD`(고정밀 측정) 전송 → 약 10ms 대기 → 6바이트 읽기 [T_msb, T_lsb, CRC, RH_msb, RH_lsb, CRC].
- CRC-8 다항식 0x31, 초기값 0xFF.
- 변환: `T = -45 + 175 * raw_t / 65535`, `RH = -6 + 125 * raw_rh / 65535`. RH는 0~100으로 자릅니다.
- **구현 전 Sensirion SHT4x 데이터시트 원문과 명령·대기시간·공식을 대조합니다.** SHT4x 직접 구현 확정으로 U-16 불필요 (라이브러리 미사용).

### 파일
| 파일 | 역할 |
|---|---|
| `src/drivers/Sht4xCodec.{h,cpp}` | CRC-8, 6바이트 → (T, RH) 디코드 (이식, 테스트 대상) |
| `src/drivers/Sht4x_esp32.cpp` | `ISensor` 구현 (I2C) |
| `app` 연결 | `AppState.sensor_present=true`, Sht40 제공자를 Fetcher와 별도의 "로컬 측정 단계"로 처리 |

- 측정 결과는 `DataPoint<double>`(source `"실내 센서"`, observed=측정 시각)로 `Snapshot`에 추가합니다(`indoor_c`, `indoor_rh`).
- 판정: `CheckSensorRange` 실패 → Missing, `IsIndoorPlausible` 실패 → Missing + 확인 필요 배지. 값을 임의로 자르지 않습니다.
- 자체 발열 보정: ePaper 갱신·Wi-Fi 직후에는 측정하지 않습니다. 깨어난 직후 가장 먼저 측정합니다.

### 테스트
- CRC 정상/변조, 경계값(raw 0 → -45℃ → 범위 밖 Missing, raw 65535), RH 클램프, NaN 경로.

## 2. 배터리

### 하드웨어 사실 (공식 예제 `Battery_Monitor`)
- 측정 enable GPIO21: HIGH일 때 측정, LOW일 때 차단(누설 방지).
- ADC GPIO1, `analogReadResolution(12)`, `analogReadMilliVolts`, 분압 ×2.

### 설계
- `src/drivers/Battery.{h,cpp}`: enable HIGH → 안정화 대기(출발값 5ms, 현재 10ms — ADR 0032) → 8회 평균(최솟값·최댓값 제외) → enable LOW → mV 반환.
- `src/power/BatteryPolicy.{h,cpp}` (이식)
  - mV → 상태(정상 / 낮음 / 위험) 판정 임계값은 상수 출발값입니다.
  - **% 표시는 공식 E1001 101점 곡선으로 합니다** (U-17 해결, 2026-11-02): `BatteryPolicy::PercentMv` 구간 선형보간. Low=20%, Critical=5%.
  - 곡선이 확보되면 구간 선형 보간 표(전압→%)를 상수로 넣고, 측정 근거 파일을 링크합니다.
- USB 연결 시 전압이 충전 전압으로 보일 수 있습니다. 이를 USB 감지로 쓰지 않습니다(U-12와 별개).

### 테스트
- 평균·이상치 제거, 임계 경계, 곡선 보간(곡선 확보 후).

## 3. 완료 기준

- [ ] HOST 테스트 통과, 빌드 성공
- [ ] 실기: SHT40 값을 기준 온습도계와 비교(±0.5℃ 범위 기록), 배터리 전압을 멀티미터와 비교 (BLOCKED_HW)
- [ ] U-17: 완충→차단까지 전압 로그(T11) → 곡선 상수 반영
