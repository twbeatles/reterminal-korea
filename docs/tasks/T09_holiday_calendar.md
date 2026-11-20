# T09 — 공휴일·휴장 달력

- 상태: DONE_VERIFIED_HOST + BUILD (특일 실응답 BLOCKED_CREDENTIALS, KRX표 출처 기재 대기)
- 선행: T02(달력 저장)
- 필요 자원: data.go.kr 키(특일 정보 API 활용 신청)
- 관련: docs/06 "공휴일 파일 없이 월~금만 영업일이라고 결론내리지 않는다", 감사 G-12, `fin::IsKrxOpen`, `fin::FscDue`, `app::NextAttemptUtc`, EXIM walk-back

## 1. 목표

지금은 주말만 제외하고, 공휴일은 화면에 "휴장 가능"으로 보수적으로 표시합니다. 공식 공휴일 데이터로 다음 동작을 정확하게 만듭니다.
- KIS 장중 판정 — 공휴일에는 호출하지 않음
- FSC 13시 게이트 — 다음 영업일 기준
- EXIM walk-back — 이미 휴일로 알고 있는 날짜는 건너뜀
- 시장 상태 라벨 — "휴장"으로 확정

## 2. 데이터 출처 (구현 전 원문 확인 필수)

- 한국천문연구원 특일 정보 (data.go.kr, `SpcdeInfoService`, 공휴일 조회 `getRestDeInfo?solYear=YYYY&solMonth=MM`)
  - 임시공휴일·대체공휴일이 지정되면 반영됩니다.
  - 응답 필드(예상): `locdate`(YYYYMMDD), `dateName`, `isHoliday`(Y/N). **실응답으로 확정**(T01 방식).
- KRX 추가 휴장일은 공휴일 API에 없을 수 있습니다. 예: 연말 마지막 거래일 다음 날(12/31), 근로자의 날 등.
  - 거래소 공지로 확인한 뒤 **연도별 상수 표**로 넣습니다.
  - 표에는 출처 URL과 확인일을 남깁니다. 확인하지 못한 연도는 "휴장 가능"으로 남겨 둡니다.
- 새 호스트·경로를 추가할 때는 `NetPolicy` allowlist(같은 `apis.data.go.kr`인지)와 `Endpoints`에 빌더·테스트를 추가합니다.

## 3. 설계

- `src/domain/Calendar.{h,cpp}` (이식)

```cpp
struct HolidayTable { int year; uint32_t bitmap[12]; bool loaded; int64_t fetched_utc; };
// bit d-1 of bitmap[m-1] = 휴일
enum class DayKind : uint8_t { BusinessDay, Weekend, PublicHoliday, MarketClosed, Unknown };
DayKind ClassifyKstDay(int64_t utc, const HolidayTable* tables, size_t n);
bool IsKrxOpen(int64_t utc, const HolidayTable*, size_t n);   // 기존 함수 확장
int64_t NextBusinessDayUtc(int64_t utc, ...);                 // FSC 게이트용
```

- 저장: NVS `cal.YYYY`(올해와 내년, 각 48B 비트맵 + 메타). T02 `SlotStore`를 재사용합니다.
- 갱신: 매월 1일에 이번 달과 다음 달 2건을 조회합니다. 12월에는 다음 해 전체를 조회합니다(12회 이하). 실패하면 기존 표를 유지합니다.
- 표가 없는 연도: `Unknown` → 기존 동작(주말만 제외) + "휴장 가능" 라벨.

## 4. 연결 지점

| 사용처 | 변경 |
|---|---|
| `fin::IsKrxOpen` | 달력이 휴일이면 false |
| `fin::FscDue` | "평일 13시" → "영업일 13시" |
| `app::NextAttemptUtc` (KIS/FSC) | 다음 개장 계산에 달력 반영 |
| `FetchExim` walk-back | 휴일로 확정된 날짜는 요청하지 않음 |
| 뷰모델 시장 상태 | 장중/장마감/휴장(확정)/휴장 가능(미확정) |

## 5. 테스트

- 설 연휴 3일 + 대체공휴일, 연말 12/31, 표 없는 연도(Unknown), 월 경계, 윤년 2/29.
- 3일 연휴 동안 KIS 호출 0, FSC는 연휴 다음 영업일 13시에 1회 (가짜 시계 시뮬레이션).
- 특일 API 응답 fixture(합성 → T01 이후 실응답)와 파서 골든 케이스.

## 6. 완료 기준

- [ ] HOST 테스트 통과, 빌드 성공, 골든 케이스 추가
- [ ] 실응답 확인 (BLOCKED_CREDENTIALS)
- [ ] KRX 추가 휴장 표에 출처·확인일 기재
