# 설계: 로컬 Wi-Fi(LAN) 설정 관리 포털 (2026-11-12)

상태: **설계 초안 · 사용자 검토 대기 · 구현 전**. 실기 동작 **BLOCKED_HW**(구현 후 E1001에서 검증 필요).
관련 결정: [ADR 0024 (제안)](../../adr/0024-lan-settings-portal.md) — docs/04 "AP에서만 HTTP 설정 페이지 허용" 규칙을 바꾸는 제안.

## 1. 목적

휴대폰 브라우저에서 같은 집 Wi-Fi에 있는 E1001에 `http://<기기 IP>/`(또는 `http://kdash-xxxx.local/`)로 들어가
Wi-Fi·API 키·지역·관심종목·기기 설정을 바꿀 수 있게 한다. 펌웨어를 다시 빌드하지 않아도 된다.

- 펌웨어 이미지에 Wi-Fi 정보나 API 키를 넣지 않는다. 출고(릴리스) 빌드는 `include/dev_credentials.h` 없이 만든다.
- 처음 켤 때(설정 없음)와 Wi-Fi가 바뀌어 접속할 수 없을 때는 지금 있는 **SoftAP 포털**을 쓴다. 이미 연결된 상태에서 하는 일상적인 설정 변경은 **LAN 포털**로 한다.
- '기기' 탭에 접속 주소, 포털 상태, 페어링 코드를 보여 준다.

### 범위 밖 (이번 설계)
- OTA 펌웨어 업로드 (ADR 0002 유지. LAN 포털이 생기면 나중에 붙일 자리만 남긴다).
- 클라우드·중계 서버·앱 설치. 기기 밖 런타임 의존성 금지(AGENTS.md).
- 거래·계좌 관련 항목. KIS는 지금처럼 읽기 전용 시세 키만 받는다.
- HTTPS 로컬 서버(§6-3에서 이유 설명). captive portal 자동 팝업은 주장하지 않는다(docs/04).

## 2. 현재 구조 분석 (CodeGraph 기준, 커밋 3f73627)

### 2-1. 이미 있는 것
| 영역 | 위치 | 지금 하는 일 | LAN 포털에서의 쓰임 |
|---|---|---|---|
| 설정 계약 | [include/Config.h](../../../include/Config.h) `Settings` | 스키마 v2. Wi-Fi·키 5종·지역·관심종목 4개·`display_page`. `refresh_scale_pct`는 저장하지 않음 | v3로 올려 기기 설정 필드 추가 (§5) |
| 로드 순서 | [src/boot/SettingsLoader.cpp](../../../src/boot/SettingsLoader.cpp) `LoadSettingsImpl` | NVS → `dev_credentials.h`(빌드 시 포함) → 기본값 | 릴리스 빌드에서 dev 경로를 빼는 플래그 추가 |
| 원자 저장 | `storage/SlotStore` + `SettingsCodec` | A/B 슬롯, 세대번호, CRC | 그대로 재사용 |
| 검증 | `storage/SettingsStore.cpp` `ValidateWifi/Key/Location/ParseWatchlist` | 규칙 한 벌(ISSUE-003) | 그대로 재사용 |
| 요청 처리 | [src/provision/PortalHandler.cpp](../../../src/provision/PortalHandler.cpp) | 서버와 분리된 순수 핸들러. CSRF, 8KB/256B 상한, 비밀값 비반사, 2단계 초기화, 10분 창. 호스트 테스트 30개 (`test_audit_portal.inc`) | **핵심 재사용 대상.** 인증 계층과 페이지 분할을 덧붙인다 |
| 기기 어댑터 | [src/provision/PortalServer_esp32.cpp](../../../src/provision/PortalServer_esp32.cpp) `RunPortal` | `WIFI_AP_STA` + `BoundedServer`(192.168.4.1:80 고정). **10분 동안 블로킹 루프**. `ConnectTest` 20초 → `SaveSettingsVerified` → 재부팅 | SoftAP 경로로 남긴다. LAN용은 **논블로킹** 어댑터를 새로 만든다 |
| 진입 | `power/CyclePlanner.cpp` `PlanCycle` | 첫 부팅 + SSID 없음, 또는 KEY0(녹색) 2초 → `enter_portal` | 진입 조건은 유지. 플랫폼이 AP/LAN 중 무엇을 열지 고른다 |
| 진입 실행 | `app/CycleRunner.cpp` → `ICyclePlatform::OpenPortal` → `portal::OpenPortal` → `RunPortal` | 저장/초기화 시 `InvalidateForPortal` 후 `ESP.restart()` | 재부팅 경로 그대로 재사용 |
| 메인 루프 | [src/main.cpp](../../../src/main.cpp) `loop()` | USB일 때 `MaintainWifi`, 버튼, 30초마다 `RunCycle` | LAN 서버 `Poll()`을 여기에 넣는다 |
| Wi-Fi 유지 | `net/WifiSupervisor` (ADR 0022) | USB 상시 연결, 재연결 백오프 | LAN 포털 전제 조건 (USB면 늘 연결) |
| 기기 탭 | `ui/ViewModel.h` `DevicePage`, `ui/Renderer.cpp` `RenderDevice` | Wi-Fi·배터리·시계·펌웨어·공급자 상태·버튼 안내 | 주소·포털 상태·페어링 코드 줄 추가 |
| 포털 안내 화면 | `ui/Renderer.cpp` `RenderPortal`, `FramePresenter::ShowPortal` | AP 이름·임시 비밀번호·URL | LAN 변형 추가 |
| 격자 변환 | `provision/GeoGrid` `LatLonToGrid` | 위경도 → 기상청 격자 | 지역 페이지에서 사용 |

### 2-2. 바꿔야 하는 이유 (구조적 제약)
1. **`RunPortal`은 블로킹이다.** 10분 동안 `RunCycle`이 돌지 않아 화면·시세 갱신이 멈춘다. LAN 포털은 대시보드가 계속 동작하는 상태에서 `loop()`가 짧게 돌려 주는 방식이어야 한다.
2. **`BoundedServer`가 `192.168.4.1`에 묶여 있다.** STA 주소(DHCP)에서 요청을 받으려면 바인딩 주소를 받는 생성자가 필요하다.
3. **인증이 "AP 비밀번호를 아는 사람"에 기대고 있다.** SoftAP는 WPA2 무작위 비밀번호와 접속 1대 제한이 곧 접근 제어다. 집 LAN에는 가족·손님·IoT 기기가 함께 있으므로 **별도 인증이 필요하다**(§6).
4. **Wi-Fi 변경과 LAN 세션이 충돌한다.** STA를 새 SSID로 시험하면 휴대폰과 기기가 다른 망으로 갈라진다. 결과를 기기 화면에 보여 주는 흐름이 필요하다(§7-3).
5. **`PortalHandler`가 한 페이지 폼이다.** 휴대폰에서 쓰기 좋게 화면을 나누고, 저장 범위(Wi-Fi 포함 여부)를 구분해야 한다.
6. **`refresh_scale_pct`가 빌드 설정**(ADR 0017)이고 Wi-Fi·키는 `dev_credentials.h` 빌드 의존이 남아 있다.

## 3. 웹 자료 조사 요약 (확인일 2026-11-12)

| 주제 | 내용 | 설계 반영 |
|---|---|---|
| Arduino-ESP32 `WebServer`/STA | [Espressif Arduino Wi-Fi API](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/wifi.html): STA 모드에서도 기기 안 HTTP 서버를 띄울 수 있다 | 외부 라이브러리 없이 코어 `WebServer` 재사용 (현행 T07과 같음) |
| mDNS | Arduino-ESP32 `ESPmDNS`([RNT 예시](https://randomnerdtutorials.com/?p=182870)). Android는 [AOSP DNS Resolver](https://source.android.com/docs/core/ota/modular-system/dns-resolver) 기준 2021-11부터 `.local` 해석, 다만 VPN·모바일 데이터 제외이고 [기기별 실패 보고](https://community.home-assistant.io/t/detail-mdns-android-os-limitations-on-the-web-server-components-documentation/374099)가 있다. Chrome은 OS 해석기에 맡긴다([chromium-discuss](https://groups.google.com/a/chromium.org/g/chromium-discuss/c/6b0vVreNTvQ)) | **IP 주소를 기본으로 화면에 표시**하고 `.local`은 보조 주소로만 안내 |
| 프로비저닝 방식 | [ESP-IDF Unified Provisioning](https://docs.espressif.com/projects/esp-idf/en/latest/api-reference/provisioning/provisioning.html): SoftAP+HTTP 또는 BLE 전송, security1(X25519+AES-CTR+PoP)·security2(SRP6a+AES-GCM). 전용 앱이 필요하다. [Improv Wi-Fi](https://improv-wifi.netlify.app): BLE/시리얼 공개 표준이지만 [BLE 스택 RAM 비용](https://esphome.io/components/esp32_improv/)이 크다 | 앱 설치 없이 브라우저만 쓰는 목표에 맞지 않아 채택하지 않음. "PoP를 기기 화면으로 전달" 발상만 페어링 코드로 차용 |
| 기존 라이브러리 | [tzapu/WiFiManager](https://github.com/tzapu/WiFiManager) MIT, ESP32 지원 | 이미 보안 강화된 자체 포털이 있어 추가하지 않음 (Flash·검토 비용, 기존 테스트 재사용 불가) |
| DNS 리바인딩 | 악성 웹페이지가 도메인을 LAN IP로 다시 해석시켜 기기 HTTP API를 호출하는 공격([PentesterLab](https://pentesterlab.com/glossary/dns-rebinding), [Acar 외 2018](https://faculty.cc.gatech.edu/~frankli/papers/li-sigcomm-iot2018.pdf)). 방어: Host 헤더 허용목록 + 로컬이어도 인증 | Host·Origin 검사 필수(§6-2) |
| 브라우저 LNA | Chrome [Local Network Access](https://developer.chrome.com/blog/local-network-access) 권한 프롬프트(141~142, 데스크톱 우선). 공개 사이트가 LAN 기기에 요청하는 것을 막는 쪽 | 우리 페이지는 기기가 직접 제공하는 최상위 탐색이라 영향이 없다. 외부 사이트발 CSRF 위험이 줄어드는 보조 방어로 본다(의존하지 않음) |
| HTTP 인증 | 코어 `WebServer::authenticate()/requestAuthentication()` Basic 예시([Mischianti](https://mischianti.org/2020/11/09/web-server-with-esp8266-and-esp32-manage-security-and-authentication-4/)). Digest는 ESP8266 코어에서 [Apple 기기 해시 오류 보고](https://github.com/esp8266/Arduino/issues/4717) | 평문 HTTP에서 Basic은 매 요청에 비밀번호가 실리므로 쓰지 않는다. 페어링 코드 → 세션 쿠키 방식 |
| 저장 암호화 | [ESP-IDF NVS Encryption (ESP32-S3)](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/storage/nvs_encryption.html): HMAC eFuse 방식은 eFuse 블록을 영구 사용 | docs/11 방침대로 **이번 범위에서 eFuse·암호화 설정 안 함**. "NVS 저장이 안전하다"고 주장하지 않음 |

## 4. 동작 개요

### 4-1. 포털 두 가지
| | SoftAP 포털 (현행 T07) | LAN 포털 (신규) |
|---|---|---|
| 언제 | 설정 없음(첫 부팅), Wi-Fi 미연결 상태에서 녹색 2초, LAN 포털에서 "AP 모드로 다시 열기" | Wi-Fi 연결 상태에서 녹색 2초(기본), 또는 "상시 허용" 모드(USB) |
| 주소 | `http://192.168.4.1/` | `http://192.168.x.y/` (DHCP IP), 보조 `http://kdash-xxxx.local/` |
| 접근 제어 | WPA2 무작위 AP 비밀번호(화면 표시) + 접속 1대 | **화면 페어링 코드 → 세션 쿠키** + Host/Origin 검사 + CSRF |
| 대시보드 | 멈춤(블로킹 10분) | 계속 갱신 (USB). 배터리는 §4-3 |
| Wi-Fi 변경 | AP 유지한 채 STA 시험 → 결과를 같은 페이지에서 확인 | 시험 중 연결이 끊긴다 → 결과는 **기기 화면** + 새 망에서 `/status` |

### 4-2. LAN 포털 모드 (설정 `portal.lan_mode`)
- `Off`: LAN 포털을 쓰지 않는다. 녹색 2초는 항상 SoftAP.
- `Button` (**기본값 제안**): 녹색 2초 → Wi-Fi 연결이면 LAN 포털 10분, 아니면 SoftAP. 평소에는 포트 80이 닫혀 있다.
- `Always` (선택): USB 전원일 때 포트 80을 늘 연다. 기기 탭에 주소를 늘 표시. 로그인은 여전히 페어링 코드가 필요하다(§6-1). 배터리일 때는 `Button`처럼 동작한다.

> 결정 필요 Q1: 기본값을 `Button`으로 할지 `Always`로 할지. 보안상 `Button`을 권장한다(평소 공격 면 0). 편의가 중요하면 `Always`.

### 4-3. 전원별 동작
- **USB**: `loop()`가 `LanPortal::Poll()`을 매 회 호출(논블로킹). `RunCycle` 중(최대 수십 초)에는 요청이 지연되며, 브라우저 재시도로 처리하고 문서에 적는다.
- **배터리**: `loop()`에 오지 않고 깊은 잠에 든다. 녹색 2초 → 지금 `RunPortal`처럼 **10분 블로킹 LAN 포털**을 열고 닫히면 잠든다. 배터리 저전압·안전 모드에서는 열지 않는다(현행 `PlanCycle` 조건 유지).

### 4-4. 상태 기계 (LAN)
```text
CLOSED ──(녹색 2초 & Wi-Fi 연결 | Always & USB & 연결)──▶ OPEN_UNPAIRED
OPEN_UNPAIRED ──(GET / → 로그인 페이지, 기기 화면에 페어링 코드 표시)──▶ PAIRING
PAIRING ──(코드 일치)──▶ SESSION(쿠키 발급, 유휴 10분/최대 30분)
PAIRING ──(5회 실패)──▶ LOCKED 5분 (코드 폐기·재발급)
SESSION ──POST /save/{section}──▶ VALIDATE
VALIDATE ─(Wi-Fi 안 바뀜)─▶ SAVE_ATOMIC ─▶ REBOOT
VALIDATE ─(Wi-Fi 바뀜)─▶ WIFI_SWITCH_TEST(§7-3) ─성공▶ SAVE_ATOMIC ─▶ REBOOT
                                              └실패▶ 이전 Wi-Fi 복귀 + 화면에 실패 표시
SESSION ──(POST /close | 창 만료 | Button 모드 10분)──▶ CLOSED
```

## 5. 설정 모델 변경 (스키마 v3)

`include/Config.h`와 미러 `host/kdash/policy.py`를 함께 바꾼다.

```cpp
enum class LanPortalMode : uint8_t { Off = 0, Button = 1, Always = 2 };

struct DeviceConfig {                    // 신규
  LanPortalMode lan_mode = LanPortalMode::Button;
  uint8_t default_page = 0;              // 기존 Settings::display_page 이동
  int refresh_scale_pct = 100;           // 기존 ScheduleConfig에서 "저장되는 값"으로 승격 (100~400)
  char hostname_suffix[5] = {0};         // mDNS "kdash-xxxx" (MAC 유도, 비우면 자동)
};
// Settings: schema_version = 3, + DeviceConfig device;
```

- **마이그레이션 v2 → v3**: `SettingsCodec::Migrate`에 경로 추가. v2 필드는 그대로, `device`는 기본값, `display_page` → `device.default_page`, `refresh_scale_pct`는 빌드 값(`KDASH_REFRESH_SCALE_PCT`)을 초기값으로 이전. 고정 픽스처(v2 바이트열)로 회귀 테스트.
- **ADR 0017 변경**: 배율의 출처가 "빌드 설정"에서 "저장 설정(빌드 값은 첫 이전 시 초기값)"으로 바뀐다. ADR 0024에 함께 기록.
- **빌드 의존 제거**: `SettingsLoader`의 `dev_credentials.h` 경로를 `-D KDASH_ALLOW_DEV_CREDENTIALS=1`일 때만 컴파일. 릴리스 환경(`reterminal_e1001`)은 기본 0, 개발용 환경 `reterminal_e1001_dev`에서만 1. 부팅 순서는 `NVS → (dev, 개발 빌드만) → 기본값 → 포털`.
- 공장초기화의 "기본값 톰스톤"(현행 `ResetOwned`)은 그대로 둔다(개발 빌드에서 dev 값이 되살아나지 않게).

### 5-1. 포털에서 다룰 항목
| 화면 | 항목 | 검증 (기존 재사용) | 저장 후 |
|---|---|---|---|
| Wi-Fi | SSID(주변 스캔 목록 + 직접 입력), 비밀번호(쓰기 전용) | `ValidateWifi` | §7-3 전환 시험 |
| API 키 | 기상청/데이터포털 키, 에어코리아 키, 수출입은행 authkey, FSC 사용, KIS 사용·앱키·시크릿 (모두 쓰기 전용, `등록됨/미등록`) | `ValidateKey` | 재부팅 + 인증 차단 기록 초기화(`ClearAuthBlocks`) |
| 지역 | 표시 이름, 측정소, 격자 nx/ny, 또는 위도·경도 → `LatLonToGrid` | `ValidateLocation`, `MissingGlyphs` 경고 | 재부팅 |
| 관심종목 | 최대 4개 `005930,000660:NXT` | `ParseWatchlist` | 재부팅 |
| 기기 | 기본 화면, 갱신 주기 배율(100~400%), LAN 포털 모드, mDNS 이름 | 범위 검사 신규 | 재부팅 |
| 상태(읽기) | 펌웨어 버전·빌드, IP, RSSI, 공급자별 마지막 성공/오류 분류, 배터리/USB, 가동 시간, 설정 출처(nvs/dev/default) | — | — |
| 관리 | 지금 새로고침, 재부팅, 설정 백업 내보내기(비밀 제외 JSON), 2단계 공장초기화, 포털 닫기, "AP 모드로 다시 열기" | `IsTwoStepResetConfirmed` | — |

- 지금처럼 **저장 후 재부팅**으로 반영한다(검증된 경로, 런타임 재구성 버그 위험 회피). "재부팅 없이 즉시 반영"은 후속 과제.
- 화면별 저장은 `pending = current + 해당 화면 필드`로 만든다. 다른 화면 값은 건드리지 않는다.
- 백업 가져오기(import)는 비밀이 없는 항목만 받는다. 1차 범위에서는 내보내기만.

## 6. 보안 설계

### 6-1. 접근 제어: 화면 페어링 코드 + 세션
- 로그인 페이지 요청이 오면 기기가 **6자리 숫자 코드**(또는 `MakeApPassword` 알파벳 8자)를 `IRandom`(하드웨어 RNG)으로 만들어 **e-paper 기기 탭에만** 표시한다. UART·HTTP 응답에는 넣지 않는다. 기기 앞에 있는 사람만 알 수 있다(물리적 근접 = 권한, ESP-IDF PoP와 같은 발상).
- 코드는 1회용, 5분 유효. 5회 틀리면 5분 잠금 후 새 코드. 코드 표시를 위한 화면 갱신은 1분에 1번으로 제한(원격에서 화면 깜빡임을 유발하는 남용 방지).
- 성공하면 128비트 세션 토큰을 쿠키로 준다: `HttpOnly; SameSite=Strict; Path=/`(HTTP라 `Secure` 불가). 세션은 **1개만**. 새 로그인은 이전 세션을 끊는다. 유휴 10분, 최대 30분.
- CSRF 토큰은 세션마다 새로 발급(현행 `IssueCsrfToken/CheckCsrfToken` 재사용). 상태를 바꾸는 요청은 모두 POST + CSRF.
- SoftAP 포털은 지금처럼 AP 비밀번호 자체를 인증으로 보고 페어링을 생략한다(같은 핸들러, `AuthPolicy::ApTrusted`).

> 결정 필요 Q2: 페어링 대신(또는 추가로) 사용자가 정하는 **관리자 비밀번호**를 둘지. 둔다면 NVS에 PBKDF2-SHA256(mbedTLS, 솔트, ≥10k 회) 해시만 저장. 1차 범위는 페어링 코드만 권장(비밀번호 분실 복구 흐름이 필요 없음).

### 6-2. 요청 검사 (모든 요청, 라우팅 전)
1. **Host 허용목록**: 현재 STA IP, `kdash-xxxx.local`, (AP 모드) `192.168.4.1`. 그 외는 421/403 → DNS 리바인딩 차단.
2. **Origin/Referer**: POST에 `Origin`이 있으면 위 Host 중 하나와 정확히 같아야 한다. 없으면 `Referer`로 확인, 둘 다 없으면 거부.
3. 기존 `CheckPortalHeaders` 상한 유지: 헤더 2KB, URI 256B, 본문 8KB, 필드 256B, `Transfer-Encoding` 거부, `application/x-www-form-urlencoded`만.
4. 응답 헤더 유지 + 보강: `Cache-Control: no-store`, `X-Content-Type-Options: nosniff`, `Referrer-Policy: no-referrer`, CSP `default-src 'none'; style-src 'self'; form-action 'self'; frame-ancestors 'none'; base-uri 'none'`. JS는 쓰지 않는다(서버 렌더 HTML 폼).
5. 동시 연결 1개(현행 `BoundedServer`의 accept 방식 유지), 연결당 시간 상한.

### 6-3. 평문 HTTP 판단 (ADR 0024에 기록)
- LAN 포털은 HTTP다. 같은 Wi-Fi 비밀번호를 아는 사람이 무선 구간을 캡처하면 **입력하는 키·비밀번호를 볼 수 있다**(WPA2-PSK는 PSK를 아는 단말끼리 보호하지 않음).
- 기기 내 HTTPS(`esp_https_server`)는 자체서명 인증서 경고가 매번 뜨고, 사용자가 경고를 넘기는 습관을 들이게 하며, TLS 서버 RAM 비용이 크다. 그래서 1차 범위에서 채택하지 않는다.
- 완화: (a) 비밀값은 쓰기 전용, 응답에 절대 다시 싣지 않음 (b) 세션 짧게, 1개만 (c) 포털 화면에 "공용·손님 Wi-Fi에서는 API 키를 입력하지 마세요. 민감하면 AP 모드로 입력하세요" 안내 (d) SoftAP 경로(무작위 WPA2, 1대)는 계속 사용 가능.
- docs/11 원칙 유지: NVS가 분실 기기에서 비밀을 지켜 준다고 주장하지 않는다. eFuse·flash encryption은 손대지 않는다.

### 6-4. 로그·화면
- UART: `portal: lan open mode=button ip=192.168.x.y`, `portal: pair ok|fail n=3`, `portal: save section=keys result=ok` 수준. SSID·비밀번호·키·세션 토큰·페어링 코드는 기록하지 않는다(코드는 화면 전용).
- 기기 탭: IP, `.local` 이름, 남은 시간, 페어링 코드(요청 중일 때만). 키·비밀번호 값은 표시하지 않는다.

## 7. 모듈 설계

### 7-1. 새/변경 모듈
```text
src/provision/
  PortalAuth.{h,cpp}        (신규, 순수) 페어링 코드 발급·검증·잠금, 세션 토큰, 만료. IRandom + 시각 주입
  RequestGuard.{h,cpp}      (신규, 순수) Host 허용목록, Origin/Referer 검사, 쿠키 파싱
  SettingsPatch.{h,cpp}     (신규, 순수) 폼 필드 → 후보 Settings, 화면별 적용, 변경 분류(WifiChanged/KeysChanged/Other)
  PortalPages.{h,cpp}       (신규, 순수) 화면별 HTML 렌더(EscapeHtml 경유), 공통 CSS 문자열
  PortalHandler.{h,cpp}     (변경) 라우팅만 남기고 위 모듈 조합. AuthPolicy{ApTrusted, LanPaired}
  PortalServer_esp32.cpp    (변경) BoundedServer에 바인딩 IP 인자, SoftAP 경로는 동작 유지
  LanPortal_esp32.{h,cpp}   (신규, 기기 어댑터) Open/Poll/Close 논블로킹, ESPmDNS 등록·해제, Wi-Fi 전환 시험
  PortalRunner.cpp          (변경) AP/LAN 선택, 배터리 블로킹 LAN 변형
src/app/
  CyclePlatform.h           (변경) OpenPortal(AppState&, IStorage&, PortalKind) 또는 플랫폼 내부 선택
  AppState (Coordinator.h)  (변경) 비영속 PortalStatus{kind, open, ip[16], host[24], code[9], expires_ms, last_result}
src/ui/
  ViewModel DevicePage      (변경) portal_line, portal_code 필드
  Renderer RenderDevice     (변경) 좌측 카드 펌웨어 줄 아래(약 y=300~360)에 주소·코드 표시. 여백 규칙(CheckMarginsClean) 준수
  RenderPortal              (변경) LAN 안내 변형(주소 + 코드, 비밀번호 없음)
src/boot/SettingsLoader     (변경) KDASH_ALLOW_DEV_CREDENTIALS 가드, refresh 배율 출처 변경
src/storage/SettingsCodec   (변경) v3 인코딩 + v2→v3 Migrate
src/main.cpp                (변경) loop()에서 LanPortal Poll, 결과(Saved/Reset) 처리
platformio.ini              (변경) reterminal_e1001_dev 환경 추가
```

### 7-2. 의존 방향
```text
main.cpp ─▶ LanPortal_esp32 ─▶ PortalHandler ─▶ PortalAuth / RequestGuard / SettingsPatch / PortalPages
                 │                                   └▶ storage/SettingsStore (검증), geo/GeoGrid
                 ├▶ WebServer, ESPmDNS, WiFi (Arduino 코어만)
                 └▶ PortalServer::SaveSettingsVerified (기존 원자 저장)
ui/ViewModel ◀── AppState.portal (상태만 읽음, 포털 모듈을 직접 참조하지 않음)
```
순수 모듈은 `KDASH_HOST_TEST`에서 빌드되고 Arduino 헤더를 포함하지 않는다(현행 원칙).

### 7-3. Wi-Fi 변경 흐름 (LAN 세션에서)
1. `POST /save/wifi` 검증 통과 → 응답: "기기가 새 Wi-Fi로 시험 접속합니다. 결과는 기기 화면에 표시됩니다. 성공하면 휴대폰도 새 Wi-Fi로 옮긴 뒤 화면의 새 주소로 접속하세요." (응답을 보낸 뒤 시작)
2. 어댑터: 서버 닫기 → `WiFi.disconnect(false,false)` → 새 SSID로 `begin` → 20초 대기(watchdog Feed).
3. 성공: `SaveSettingsVerified` → 기기 화면에 "새 Wi-Fi 연결됨, IP x.x.x.x" Full 갱신 → 재부팅.
4. 실패: 이전 SSID로 `begin`(기존 `RunPortal` 실패 경로와 같음) → 이전 망에서 LAN 포털 재개 → 화면과 `/status`에 "연결 실패: 이전 설정 유지"(사유는 SSID 없음/인증 실패 추정/시간 초과만, 값 없음).
5. 전환 중 페어링 세션은 폐기한다(새 망에서 다시 페어링).

## 8. 기기 탭 화면 (800×480, 좌측 카드)

```text
┌ 기기 상태 ───────────────────────┐
│ (wifi) Wi-Fi 연결됨 · -42 dBm     │
│ (batt) USB 전원                   │
│ (clock) 시각 동기화됨              │
│   펌웨어 1.0.0 (abc1234)           │
│ (gear) 설정 http://192.168.0.23   │   ← 포털 열림/Always일 때
│        kdash-3f2a.local · 8분 남음 │
│ ▓ 페어링 코드  482 913 ▓           │   ← 로그인 요청 중일 때만(반전 배너)
│ 왼쪽 ◀ 이전 화면 · 가운데 ▶ 다음 화면 │
│ 녹색 새로고침 · 녹색 2초 누르면 설정  │
└──────────────────────────────────┘
```
- 포털이 닫혀 있고 `Button` 모드면 "녹색 2초: 휴대폰 설정 열기" 안내만.
- 코드 표시/해제와 남은 시간은 화면 갱신을 부르므로 **분 단위**로만 갱신하고, ADR 0018 깜빡임 정책(바뀐 것 없으면 다시 그리지 않음)을 따른다.
- 다른 탭(생활/시장/달력)을 보고 있을 때 로그인 요청이 오면 기기 탭으로 바꾸지 않고 헤더에 작은 표시만 한다 → 결정 필요 Q3 (자동 전환이 편하지만 원격에서 화면을 바꿀 수 있게 됨).

## 9. HTTP 라우트 (LAN/AP 공통, AP는 로그인 생략)

| 메서드·경로 | 인증 | 동작 |
|---|---|---|
| `GET /` | 세션 없으면 로그인 화면 | 메뉴: Wi-Fi / API 키 / 지역 / 관심종목 / 기기 / 상태 / 관리 |
| `POST /login` | 없음(코드) | 페어링 코드 확인 → 쿠키 발급 → `/`로 303 |
| `GET /wifi` · `/keys` · `/location` · `/watch` · `/device` | 세션 | 화면별 폼(비밀은 라벨만) |
| `GET /wifi/scan` | 세션 | 주변 SSID 목록(최대 15개, RSSI 순, 이스케이프) — 스캔 중 STA 잠깐 지연 |
| `POST /save/{section}` | 세션+CSRF | §5-1, §7-3 |
| `GET /status` | 세션 | 상태·마지막 저장 결과 |
| `GET /export` | 세션 | 비밀 제외 JSON (`"등록됨"/"미등록"`) |
| `POST /refresh` · `/reboot` · `/close` · `/ap` | 세션+CSRF | 즉시 새로고침 예약 / 재부팅 / 포털 닫기 / SoftAP로 다시 열기 |
| `POST /reset` | 세션+CSRF | 2단계 공장초기화(현행) |
| `GET /app.css` | 없음 | 정적 CSS(휴대폰 화면용, 외부 리소스 없음) |
| 그 밖 | — | 404 |

## 10. 메모리·성능 예산 (구현 시 실측으로 대체)
- `PortalHandler` 본문 버퍼 8KB + `Settings` 2개(약 1.6KB×2) + 폼 필드 40개 → 포털이 열릴 때 힙에서 할당하고 닫을 때 해제(현행과 같음). PSRAM 8MB 있음.
- `WebServer` + `ESPmDNS` 추가 Flash: 코어 포함이라 소량 예상 — **빌드 후 실측 기록**(현재 Flash 1,839,522 B / 55.0%, ADR 0022).
- `Always` 모드 USB 대기 전류 증가는 무시 가능 수준으로 예상하지만 측정 전까지 주장하지 않는다.

## 11. 테스트 계획

### 11-1. 호스트 (test_native, 신규 `test_lan_portal.inc`)
| 케이스 | 기대 |
|---|---|
| 세션 없이 `GET /keys`, `POST /save/*` | 로그인 화면/401, pending 없음 |
| 페어링 코드 틀림 5회 | 잠금, 6번째 정답도 거부, 5분 뒤 새 코드만 유효 |
| 코드 만료(5분) 후 정답 | 거부 |
| 세션 유휴 10분/최대 30분 | 만료 |
| 두 번째 로그인 | 첫 세션 무효 |
| Host `evil.example` | 거부 (리바인딩) |
| Origin 불일치 POST | 403 |
| 모든 GET 라우트 응답 | 저장된 키·Wi-Fi 비밀번호·세션 토큰·페어링 코드 문자열이 본문에 없음 (`find_secrets` + 원문 검색) |
| `/save/location` | Wi-Fi·키 필드 변경 없음 |
| `/save/wifi` | 분류 `WifiChanged`, 즉시 저장되지 않음 |
| 비밀 필드 빈칸 | 기존 값 유지, "삭제" 체크 시에만 지움 (현행 회귀) |
| v2 픽스처 → v3 Migrate | 기존 값 보존, `device` 기본값, 배율 이전 |
| `KDASH_ALLOW_DEV_CREDENTIALS=0` | dev 경로 컴파일 안 됨, NVS 없음 → 기본값 + 포털 |
| DevicePage 렌더 | 주소/코드 줄 표시, `CheckMarginsClean` 통과, 비밀값 없음 |
| 기존 `test_audit_portal.inc` 30개 | 전부 그대로 통과 (AP 경로 회귀) |

### 11-2. 빌드
- `pio run -e reterminal_e1001`(dev 자격 없음), `pio run -e reterminal_e1001_dev`, 펌웨어 바이너리 시크릿 마커 스캔(docs/04 완료 증거).

### 11-3. 실기 (BLOCKED_HW, 구현 후)
1. USB, `Button`: 녹색 2초 → 기기 탭 주소 확인 → 휴대폰(Android Chrome, iOS Safari)으로 IP 접속 → 페어링 → 키 변경 → 재부팅 → 조회 성공.
2. `.local` 접속 성공/실패를 기기·OS별로 기록(성공을 가정하지 않음).
3. Wi-Fi 변경 성공/실패(틀린 비밀번호) 각 1회 → 이전 설정 유지 확인.
4. 배터리: 녹색 2초 → 10분 블로킹 LAN 포털 → 종료 후 깊은 잠.
5. 다른 단말에서 Host 위조 요청 거부 확인, UART 로그에 비밀·코드 없음 확인.

## 12. 단계별 구현 순서
1. **P1 설정 기반**: 스키마 v3 + Migrate + `KDASH_ALLOW_DEV_CREDENTIALS` + dev 빌드 환경 + 배율 저장화. (호스트 테스트)
2. **P2 순수 포털 계층**: PortalAuth / RequestGuard / SettingsPatch / PortalPages, PortalHandler 라우팅 재구성. 기존 AP 테스트 회귀 0.
3. **P3 LAN 어댑터**: `LanPortal_esp32` 논블로킹 + mDNS + `loop()` 배선 + AppState.portal + 기기 탭 UI. (빌드)
4. **P4 Wi-Fi 전환 흐름 + 배터리 블로킹 변형**.
5. **P5 실기 검증** → T07 완료 기준 갱신, docs/04·14·RECOVERY 사용자 안내 갱신.

## 13. 결정이 필요한 질문
- **Q1** LAN 포털 기본 모드: `Button`(권장) / `Always`.
- **Q2** 관리자 비밀번호 추가 여부(권장: 1차 범위 제외, 페어링 코드만).
- **Q3** 로그인 요청 시 기기 탭으로 자동 전환할지(권장: 전환 안 함, 헤더 표시만).
- **Q4** 평문 HTTP LAN 포털에서 API 키 입력을 허용할지, 키는 AP 모드에서만 받을지(권장: 허용 + 경고 문구, ADR 0024).
- **Q5** 저장 후 재부팅 유지(권장) vs 즉시 반영.

## 14. 결정과 구현 결과 (2026-11-12)

Q1~Q5는 모두 권장안으로 확정해 구현했다. 달라진 점과 미구현 항목은 [ADR 0024](../../adr/0024-lan-settings-portal.md) 끝의 "구현 현황"에 있다. 자동 순환·금융위원회 종가 표시는 [ADR 0025](../../adr/0025-page-auto-rotation-and-market-fsc-close.md).
