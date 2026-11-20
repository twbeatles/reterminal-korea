# T07 — 설정 포털: SoftAP + 로컬 웹서버 + 저장 + 공장초기화

- 상태: 핸들러 DONE_VERIFIED_HOST + BUILD. 2026-11-12: 같은 Wi-Fi(LAN) 포털 추가([ADR 0024](../adr/0024-lan-settings-portal.md)). 로그인 전 보호·Host/Origin·잠금은 직접 만든 HTTP 요청으로 확인했으나 **실제 브라우저 로그인은 아직 완전히 확인되지 않음**(아래 ADR의 Origin 사례). 로그인 이후 흐름과 AP 시나리오는 BLOCKED_HW
- 선행: T02(원자 저장), T04(AP 비밀번호 화면 표시), T06(KEY0 길게)
- 필요 자원: 실기
- 관련: docs/04 "Wi-Fi 초기 설정"·"Settings 구조", docs/11, `docs/RECOVERY.md`, `src/provision/SetupPortal.*`, `src/storage/SettingsStore.*`

## 1. 목표 상태기계 (docs/04)

```text
FACTORY/UNCONFIGURED 또는 KEY0 길게
 → START_SETUP_AP (KDASH-E1001-XXXX, 무작위 WPA2 비밀번호, 화면 표시)
 → LOCAL_PORTAL (http://192.168.4.1/, 최대 10분)
 → VALIDATE_FIELDS → CONNECT_TEST (새 SSID로 STA 접속 시험, 20s)
 → SAVE_ATOMIC (T02) → REBOOT
 실패 → 이전 설정 유지 + 오류 안내 (비밀값 제외)
 10분 경과/사용자 종료 → AP OFF → 이전 설정으로 재부팅
```

## 2. 설계

### 2-1. 이식 가능한 요청 핸들러 (`src/provision/PortalHandler.{h,cpp}`)
기기 웹서버와 분리해 HOST에서 보안 테스트를 할 수 있게 합니다.

```cpp
struct Request { Method method; const char* path; FormField* fields; size_t n;
                 const char* csrf; size_t body_len; };
struct Response { int status; const char* content_type; char* body; size_t len; };
class PortalHandler {
 public:
  PortalHandler(const Settings& current, IRandom& rng, int64_t opened_at);
  Response Handle(const Request&, int64_t now);   // 라우팅 + 검증 + 렌더
  bool HasPendingSave() const; const Settings& Pending() const;
  bool ResetConfirmed() const;
};
```

### 2-2. 라우트

| 메서드·경로 | 동작 |
|---|---|
| `GET /` | 설정 폼. 비밀 필드는 **값 없이** `등록됨/미등록` 라벨만. 숨은 CSRF 토큰 포함 |
| `POST /save` | CSRF 검사 → 필드별 `ValidateField` → 통과하면 pending에 저장 → "접속 시험 중" 페이지 |
| `GET /status` | 접속 시험 결과(성공/실패 사유 — SSID 없음, 비밀번호 오류 추정, 시간 초과) |
| `POST /reset` | 1단계: `RESET` 입력 확인 화면 / 2단계: `RESET-YES` + CSRF → 공장초기화 예약 (`IsTwoStepResetConfirmed`) |
| `GET /export` | 설정 백업 JSON. 비밀은 `"등록됨"`/`"미등록"`만 (RECOVERY 2단계) |
| `POST /close` | 포털 종료 |
| 그 밖의 경로 | 404 (디렉터리·파일 노출 없음) |

### 2-3. 보안 규칙
- CSRF 토큰: 포털 세션마다 `IssueCsrfToken(rng)`. 상태를 바꾸는 POST는 모두 `CheckCsrfToken` 필수, 없거나 틀리면 403.
- 본문 상한 8KB, 필드 상한 `kFormFieldMax`(256B). 넘으면 413.
- 출력은 전부 `EscapeHtml`을 거칩니다(SSID·지역명·측정소명 반사).
- **비밀값은 어떤 응답에도 넣지 않습니다.** 비밀 필드를 빈칸으로 제출하면 "기존 값 유지"이고, 지우려면 별도 체크박스를 둡니다.
- 응답 헤더: `Cache-Control: no-store`, `X-Content-Type-Options: nosniff`, `Content-Security-Policy: default-src 'self'; style-src 'unsafe-inline'`(인라인 CSS만 허용, 외부 리소스·JS 없음).
- AP 클라이언트는 1대만 허용합니다(`WiFi.softAP(..., max_connection=1)`).
- captive portal 자동 팝업은 주장하지 않습니다(docs/04). DNSServer를 쓰더라도 "편의 기능"으로만 표시합니다.

### 2-4. 폼 필드
- Wi-Fi: SSID(목록 스캔 + 직접 입력), 비밀번호
- 지역
  - 표시 이름, 에어코리아 측정소명
  - 기상청 격자: 직접 입력(nx, ny), 또는 위도·경도 입력 후 기기에서 변환
  - 위도·경도→격자 변환은 기상청 가이드의 Lambert 정각원추 투영식입니다. 상수는 RE=6371.00877km, GRID=5.0km, SLAT1=30.0, SLAT2=60.0, OLON=126.0, OLAT=38.0, XO=43, YO=136이며, **구현 전 가이드 원문과 대조 필수**입니다.
  - 변환 함수는 이식 코드로 두고, 가이드 부록 표의 지점 5곳 이상을 테스트 기대값으로 고정합니다.
- 키: 기상청/데이터포털 공용 키, 에어코리아 키(같은 계정이면 같은 키), 수출입은행 authkey
- P5(선택): FSC 사용, KIS 사용, KIS 앱키·시크릿, 관심종목(`005930, 000660:NXT`)
- 저장 시 지역명·측정소명에 폰트에 없는 글자가 있으면 경고합니다(T04b `MissingGlyphs`).

### 2-5. 기기 어댑터 (`src/provision/PortalServer_esp32.cpp`)
- Arduino-ESP32 내장 `WebServer`(+ 선택 `DNSServer`)를 사용합니다. 외부 라이브러리를 추가하지 않습니다.
- `WiFi.mode(WIFI_AP_STA)`로 AP를 유지한 채 STA 접속을 시험합니다.
- AP 이름 접미사는 MAC 하위 2바이트 16진수로 만듭니다(`MakeApName`, 시리얼·PII 금지).
- AP 비밀번호는 `MakeApPassword(HardwareRng)`로 만듭니다. `HardwareRng`는 `esp_random()` 기반 `IRandom` 구현이며, RF가 켜진 뒤 생성합니다.
- 화면(T04)에 AP 이름·비밀번호·주소·남은 시간을 Full 갱신으로 표시합니다. UART에는 비밀번호를 **출력하지 않습니다**.

## 3. 구현 단계

1. 위도·경도→격자 변환 + 테스트
2. `PortalHandler`(라우트·CSRF·검증·이스케이프·pending) + HOST 보안 테스트
3. HTML 템플릿: 정적 문자열, 서버 측 렌더, JS 없음. 한국어 문구.
4. 기기 어댑터 + T04 안내 화면 + T06 진입 연결
5. 저장 경로: pending → CONNECT_TEST → `SlotStore` 쓰기 → `ClearAuthBlocks` → 재부팅
6. 공장초기화: 2단계 확인 → 설정 슬롯 erase → 재부팅 (NVS 전체 erase는 하지 않음 — 캐시만 별도 삭제)

## 4. 테스트 (HOST, 최소)

| 케이스 | 기대 |
|---|---|
| `GET /` 응답 본문 | 저장된 키·Wi-Fi 비밀번호 문자열이 포함되지 않음 (`find_secrets` + 원문 검색) |
| CSRF 없는 `POST /save` | 403, pending 없음 |
| 필드 257B | 413 또는 필드 오류, 오류 문구에 입력값 미포함 |
| SSID `<script>` | 이스케이프되어 반사 |
| 비밀 필드 빈칸 제출 | 기존 키 유지 |
| `POST /reset` 1단계만 | 초기화 안 됨 |
| 10분 경과 후 요청 | 410 또는 닫힘 |
| watchlist `005930,` | 필드 오류 (기존 검증 재사용) |
| 위경도 변환 | 가이드 표 5지점 일치 |

## 5. 완료 기준

- [ ] HOST 테스트 통과, 펌웨어 빌드 성공
- [ ] 실기: 첫 부팅 → 포털 → 설정 → 재부팅 → 조회 성공 시나리오 (BLOCKED_HW)
- [ ] 실기: 틀린 Wi-Fi 비밀번호 → 이전 설정 유지
- [ ] `docs/RECOVERY.md` 1·2단계 "검증됨" 표기 (실기 후)

## 부록 (2026-11-12): 같은 Wi-Fi(LAN) 포털

AP 포털(위 설계)은 첫 설정·복구용으로 유지하고, 연결된 상태의 일상 설정은 LAN 포털로 한다. 설계 [LAN 설정 포털](../superpowers/specs/2026-11-12-lan-settings-portal-design.md), 결정·구현 현황·실기 검증 표는 [ADR 0024](../adr/0024-lan-settings-portal.md).
추가 경로: 로그인(페어링 코드), 구역별 폼(Wi-Fi·키·지역·관심종목·기기), `/wifi/scan`, `/backup`·`/import`·`/export`, 동작 요청(`/refresh /reboot /ap`). 호스트 테스트는 `test_native/test_lan_portal*.inc`.
