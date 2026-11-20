# T03 — 시계: PCF8563 RTC + NTP + 신뢰 판정

- 상태: DONE_VERIFIED_HOST + BUILD (RTC 24h 드리프트·Wi-Fi 차단 재부팅은 BLOCKED_HW)
- 선행: 없음
- 필요 자원: 실기(최종 확인)
- 관련: docs/06 "시간 모델", `include/IClock.h`, 공식 예제 `examples/base/RTC_PCF8563`

## 1. 목표

- 부팅 직후 Wi-Fi 없이도 **믿을 수 있는 시각**을 확보합니다(PCF8563).
- NTP에 성공하면 RTC를 보정합니다.
- 시계를 믿을 수 없으면 HTTPS를 호출하지 않고 화면에 "시간 확인 필요"를 표시합니다.

## 2. 하드웨어 사실 (공식 예제 원문)

- I2C SDA=GPIO19, SCL=GPIO20, 주소 0x51. SHT40(0x44)과 버스를 공유합니다.
- 레지스터: 0x02 seconds(bit7 = VL, 전압 저하로 시각 불신), 0x03~0x08 분/시/일/요일/월(bit7 세기)/년 — 모두 BCD.
- VL=1이면 저장된 시각을 믿을 수 없습니다(백업 전지 CR1220 방전·교체).
- 예제는 외부 라이브러리 없이 `Wire`로 레지스터를 직접 읽고 씁니다. 이 방식을 따르면 라이선스 부담이 없습니다.

## 3. 설계

### 3-1. 신뢰 규칙 (이식 로직, `src/domain/ClockPolicy.{h,cpp}`)

```text
입력: rtc_ok(I2C 성공), rtc_vl, rtc_epoch, ntp_ok, ntp_epoch, build_epoch(펌웨어 빌드 시각)
출력: epoch, verified, source ∈ {NTP, RTC, NONE}, action ∈ {WRITE_RTC, NONE}

1) ntp_ok && ntp_epoch >= build_epoch           → (ntp, verified, NTP),
     rtc_ok이고 |rtc - ntp| > 2s이면 action=WRITE_RTC
2) rtc_ok && !rtc_vl && rtc_epoch >= build_epoch → (rtc, verified, RTC)
3) 그 외                                           → (0, unverified, NONE)
```

- `build_epoch`는 `-D KDASH_BUILD_EPOCH=$(date +%s)`처럼 빌드 시 주입합니다. 빌드 스크립트(extra_script)로 넣거나, `scripts/version.py` 결과를 활용합니다. 빌드 시각보다 이른 시각은 불신합니다(2026년 이전 RTC 초기값 방지).
- RTC만 있고 NTP가 없는 상태가 72시간 이상 이어지면 표시만 "RTC 기준"으로 바꿉니다. TLS는 RTC 시각으로 계속 시도하고, 실패하면 T05 스케줄의 백오프를 따릅니다.

### 3-2. 파일

| 파일 | 역할 |
|---|---|
| `src/domain/ClockPolicy.{h,cpp}` | 위 규칙 (이식, 테스트 대상) |
| `src/drivers/Pcf8563.{h,cpp}` | BCD 변환·레지스터 읽기/쓰기. BCD 함수는 이식, I2C 부분은 기기 전용 |
| `src/drivers/SystemClock.{h,cpp}` | `IClock` 구현: 부팅 시 RTC → `settimeofday`, NTP 결과 반영 |

- `IClock::now_utc(out)`은 verified가 아니면 false를 반환합니다. `main.cpp`의 `FetchContext.clock_verified`를 이 값으로 채웁니다(현재는 NTP 성공 여부만 사용).
- RTC는 **UTC로 저장**합니다(KST 변환은 UI에서만). 예제는 지역 시각을 쓰므로 다르다는 점을 주석으로 남깁니다.

## 4. 구현 단계

1. `ClockPolicy` + 테스트 (아래 표)
2. BCD 변환 + PCF8563 레지스터 디코드/인코드 함수(이식) + 테스트: 바이트 배열 ↔ 시각, 세기 비트, VL 비트
3. 기기 I2C 어댑터 + `SystemClock`
4. `main.cpp`
   - 부팅: RTC 읽기 → 정책 적용 → verified면 바로 fetch 가능
   - NTP 성공 시 `WRITE_RTC`
5. 빌드, UART에 `time: source=RTC|NTP|NONE verified=…` 출력 (시각 값 출력은 허용)

## 5. 테스트

| 케이스 | 기대 |
|---|---|
| NTP OK, RTC 3초 차이 | NTP 채택, WRITE_RTC |
| NTP 실패, RTC OK, VL=0 | RTC 채택, verified |
| NTP 실패, VL=1 | unverified, 화면 "시간 확인 필요" |
| RTC 시각 < 빌드 시각 | 불신 |
| BCD `0x59` | 59, 잘못된 BCD `0x5A` → 디코드 실패 |
| 세기 비트 C=1 | 1900년대로 디코드 (공식 RTC 예제, 예전 구현은 반전 버그였음 — 2026-11-02 수정) |
| UTC 15:00 = KST 다음날 00:00 | 기존 경계 테스트와 일관 |

## 6. 완료 기준

- [ ] 이식 로직 테스트 통과, 펌웨어 빌드 성공
- [ ] 실기: Wi-Fi 차단 상태로 재부팅 → RTC 기준 verified, 정상 fetch 대기 (BLOCKED_HW)
- [ ] 실기: 24시간 RTC 드리프트 측정 기록 (T11)
