# 04. Wi-Fi·HTTPS·인증정보·설정포털

## 기기 직접 통신
- ESP32-S3 2.4GHz Wi-Fi를 기기 단독 네트워크 수단으로 사용한다. 외부 Backend URL은 설정 항목 자체에서 제거(사용자 소유 로컬 프록시 설정도 MVP에 넣지 않음).
- SSID 등록 실패 시 무한 재연결 대신 재시도 상한 → 오류 표시 → 절전/재시도 시점 예약.
- DNS/HTTP/TLS 연결은 고정 request timeout, redirect 호스트 허용목록, body max size 적용.
- 루트 CA 번들 또는 필수 서버 인증서의 안전한 관리방법 선정(검증 가능한 경로); 날짜 미동기화로 TLS 실패하면 RTC 및 시간 재동기화 경로를 문서화. `setInsecure()` 금지.

## Wi-Fi 초기 설정
`FACTORY/UNCONFIGURED -> START_SETUP_AP -> LOCAL_PORTAL -> VALIDATE_FIELDS -> CONNECT_TEST -> SAVE_ATOMIC -> REBOOT/RUN`
- 자체 AP 이름 예: `KDASH-E1001-XXXX`(고정 PII/시리얼 번호 사용 피함), 무작위 AP 비밀번호 기기 화면에 표시. AP에서만 HTTP 설정 페이지 허용, 외부 WAN 노출 금지.
- **기본 AP는 비공개**: 최초 등록/버튼 롱프레스에서만 10분간. 사용자가 포털 종료하면 AP OFF.
- CSRF 방어·설정 세션 만료·폼 길이 제한·URL allowlist·특수문자 escape 적용.
- 기존 Wi-Fi 키/시크릿은 설정 페이지 재조회 응답에 평문 출력하지 않고, '등록됨/재설정'만 표시.
- 설정 변경 시 접속 성공을 확인한 뒤 저장. 실패 시 마지막 정상 설정으로 복구. 공장초기화는 별도의 **명시적 2단계 확인**.

## 같은 Wi-Fi(LAN) 설정 포털 (2026-11-12, ADR 0024)
위 "AP에서만 HTTP 설정 페이지 허용" 규칙을 다음처럼 확장했다. AP 포털은 첫 설정·복구 경로로 그대로 둔다.
- 연결돼 있으면 KEY0 길게(또는 설정 "상시", USB 전원) 기기 IP의 포트 80에서 연다. 외부 WAN 노출 금지(포트포워딩·UPnP 안 함).
- 접근 제어: 기기 화면에만 보이는 1회용 6자리 페어링 코드(10분, 5회 실패 시 5분 잠금) → 세션 쿠키 1개(HttpOnly, SameSite=Strict, 유휴 10분/최대 30분) + CSRF + Host/Origin 허용목록(DNS 리바인딩 방어). 코드는 UART·HTTP 응답에 남기지 않는다.
- 평문 HTTP다: 같은 Wi-Fi 비밀번호를 아는 사람이 무선을 캡처하면 입력한 키를 볼 수 있다. 화면에 경고하고, 민감하면 AP 포털을 쓰게 한다.
- 설정 백업은 비밀 제외, 가져오기는 비밀을 받지 않는다.

## Settings 구조
`schemaVersion`, `location{gridX,gridY,displayName,airStation}`, `wifi{ssid,secret}`, `providers{kmaKey,airKey,eximKey,kisAppKey,kisAppSecret,enabled...}`, `watchlist[]`, `display{page,language,brightnessNotApplicable}`, `schedule{usbPoll,batteryPoll,sleepWindow}`.
- 스키마 v3: `device{lanMode, autoRotate, rotateMinutes, marketHoursOnly, marketFromHour, marketToHour, refreshScalePct}` 추가(v2는 읽을 때 기본값으로 이전). 저장 설정이 우선이고 `dev_credentials.h`는 개발 환경에서 NVS가 비었을 때의 초기값뿐이다.
- ePaper에 백라이트/밝기 조절 옵션을 넣지 말 것. 실제 지원하는 회색조 모드만.
- Watchlist는 종목코드 6자리 유효성 및 시장구분 whitelist 적용. KIS가 비활성화되면 종목에 DEMO/전일가만 보여준다.
- API secret 길이·문자 인코딩·저장 실패에 대한 Unit Test.

## 저장과 키 위협 모델
- NVS/Flash는 기본적으로 **하드웨어 분실 시 비밀 완전 보호를 보증하지 않음**. 가능한 ESP32 flash encryption/secure boot 및 지원·키관리·복구 비용 조사하고 문서화; 실기 공장 fuse 영구 설정은 별도 사용자 승인 필요.
- 공개 소스/직접 배포용 설정 파일은 플레이스홀더만. `secrets.*`, `.env`, `*.key`, `*.pem` 등의 커밋 방지(단, 배포용 public root CA는 키가 아닌 공개 인증서로 구분).
- 증권사 키는 개인 실기기 입력만. 실물 기기 판매·시세 재배포 지원으로 기능 확대하지 않음.

## 네트워크 장애 모드
- 인증오류: 즉시 재시도 1회 이하 + UI에 키 점검. 그 뒤에는 하루 1회만 자동 재시도하고, 설정을 다시 저장하면 바로 풀린다([ADR 0026](adr/0026-audit-2026-11-12-fixes.md)).
- TLS 오류: 인증 우회 없이 중단 + 시간·인증서 검증 경로 안내.
- HTTP 429: Retry-After 존중, 모르면 지수 백오프 + jitter; 제한 상한.
- HTTP 5xx/timeouts: 지수 백오프 + 오래된 값 캐시 표시.
- 폼 입력 오류: 설정 유지, 구체적 입력 오류(비밀값 제외) 안내.

## 완료 증거
- `evidence/security/`에 단위테스트, 로그 redaction 검증, 부팅/설정/재연결 프로토콜.
- 스캔 대상: 모든 저장소 파일 및 `firmware.bin`에서 테스트용 시크릿 marker 문자열이 등장하지 않는지 검사.
- captive portal이 아닌 **일반 로컬 AP 페이지**부터 안전하게 구현. OS captive detection까지 자동 작동한다고 주장하지 않는다.
