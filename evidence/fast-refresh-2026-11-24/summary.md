# Phase Progress Report — 빠른 차분 갱신 (ADR 0034)

- 날짜·시간(KST): 2026-11-24
- 작업 단계/티켓: 깜빡임 감소, U-03/U-15 부분 갱신 (ADR 0034)
- 커밋 SHA / 공식 기반 commit: 작업 트리 (기준 4119e8b). 참고 원문은 GxEPD2 master `src/gdey/GxEPD2_750_GDEY075T7.{h,cpp}`이며 커밋 해시는 기록하지 못했다
- 작업 환경: Windows 11, PlatformIO, `reterminal_e1001` env
- 기기 연결 여부 및 모델 확인: **연결함**(USB, COM14, CH340). 부팅 로그 `board=e1001`, `waveform=internal-otp`, PSRAM 8MB
- API 실키 접근: 해당 없음

## 구현 내용
- 변경 파일: `src/drivers/Uc8179.{h,cpp}`, `src/drivers/EpdPanel.{h,cpp}`, `src/ui/RefreshPolicy.{h,cpp}`, `src/ui/FramePresenter.cpp`, `include/Config.h`, `src/storage/SettingsCodec.cpp`, `src/provision/PortalHandler.cpp`, `src/provision/SettingsBackup.cpp`, `test_native/test_fast_refresh.inc`(신규), `test_native/test_core.cpp`, `test_native/test_lan_portal.inc`, `test_native/test_long_term.inc`, 문서
- 변경 목적: USB 전원에서 화면이 바뀔 때 흑백 빠른 차분 갱신(전체 반전 없음)을 쓰고, 5회마다 4계조 전체 갱신을 한다
- 기존 기능 영향: 설정 기본값이 꺼짐이라 기존 동작은 그대로다. 설정 스키마는 v4에서 v5로 바뀐다(자동 이전, DATA 지문 유지). 기존 테스트 2개는 v5에서 끝에 1바이트가 붙어 바이트 오프셋만 고쳤다. 포털 테스트 1개는 전송 필드 개수를 10에서 11로 바꿨다

## 재현 가능한 증거
| 구분 | 명령/방법 | 결과 | 검증 단계 | 증거 경로 |
|---|---|---|---|---|
| Clean build | `pio run -e reterminal_e1001` | PASS (RAM 78,484 B / Flash 1,981,450 B) | Build | 로컬 실행 출력 |
| Unit tests (C++) | `python tools/run_native_tests.py` | PASS 160/160 (기존 155 + 신규 5) | Host | 로컬 실행 출력 |
| Unit tests (Python) | `python -m pytest host/tests -q` | PASS 209/209 | Host | 로컬 실행 출력 |
| Stack check | `python tools/check_stack_usage.py` | PASS (failures 없음) | Host | 로컬 실행 출력 |
| Upload | `pio run -e reterminal_e1001_protected_dev -t upload --upload-port COM14`, 이어서 `-e reterminal_e1001_fast_dev` | PASS (해시 검증, 리셋 후 부팅) | Hardware | 로컬 실행 출력 |
| E1001 boot | UART 100초(protected_dev), 10분(fast_dev) | PASS: 재부팅 없음, `crashes=0`, `config: source=dev` | Hardware | [정제 로그](fast-refresh-redacted.txt) |
| 빠른 갱신 동작 | fast_dev에서 UART `display:` 줄 | **PASS(로그 한정)**: `mode=fast partials=1,2` `ok=1`, 실패 0건 | Hardware | 같은 로그 |
| 빠른 갱신 화면 품질(반전/잔상/글꼴) | 육안 | **NOT_RUN** (사용자 확인 필요) | Hardware | — |
| 5회 뒤 전체 갱신 주기 | 10분 로그 | **NOT_OBSERVED** (빠른 프레임이 2번뿐) | Hardware | — |
| Sleep/wake | — | NOT_RUN (배터리 모드 미시험) | Hardware | — |

## 독립 검증 및 보안
- 레지스터 오라클: 테스트 기대값은 구현이 아니라 GxEPD2 원문 값(0x1F, 0x29 0x07, 0xE0=02, 0xE5=6E, 800×480)과 손으로 계산한 비트 평면(0xBF/0x5F/0xAF)에서 가져왔다
- 자격증명: 새 로그(`mode=`, `partials=`)에는 비밀정보가 없다. 포털 문구와 백업 키는 bool 값 하나뿐이다
- TLS와 네트워크 변경 없음

## 미완료/블로커
- BLOCKED_HW: 육안 확인(반전·깨짐·잔상·글꼴), 5회 주기 전체, 배터리 deep sleep 복귀
- 기타: 배터리 deep sleep 모드에서는 이전 프레임이 없어 항상 전체 갱신한다(설계상 제한, ADR 0034)

## 실기 결과 (2026-11-24, USB 전원)

- 환경: 기존 `reterminal_e1001_protected_dev`로 먼저 올려 정상 부팅을 확인한 뒤, 설정 없이 켤 수 있게 새 환경 `reterminal_e1001_fast_dev`(`KDASH_DEV_FAST_REFRESH`, `config: source=dev`일 때만)를 올렸다. 이 기기는 원래 NVS 설정 없이 개인 헤더로 부팅하므로 설정 스키마 v5는 영향이 없었다(키·격자 유지).
- 전체 갱신 `elapsed_ms=4736~4741`(종전과 같음). 빠른 갱신 `elapsed_ms=1991~1993`(리셋 30ms, 전원 켬 161ms, 데이터 전송 약 0.86s, 갱신 약 0.92s). 원문 값(450ms)보다 길다. 이 시간은 SPI 2MHz로 평면 2개(96KB)를 매번 보내는 전송 시간이 큰 비중이다.
- 빠른 갱신 2회가 `ok=1`로 끝났고 BUSY 대기 초과나 오류는 없었다. 즉 이 패널의 OTP에 빠른 부분 파형이 있고 명령이 받아들여졌다는 것까지는 로그로 확인했다.
- 로그로는 화면 극성이 맞는지, 잔상이 어떤지, 깨지지 않았는지 알 수 없다. 이 항목은 육안 확인 전까지 미검증이다.
- 정제 로그: [fast-refresh-redacted.txt](fast-refresh-redacted.txt). 원본에는 시세 수치와 관심종목이 있어 커밋하지 않았다.

## 실기 확인 절차 (USB 전원)
1. `pio run -e reterminal_e1001_fast_dev -t upload --upload-port <COM>`으로 올린다(설정이 없는 기기). 이미 포털 설정을 저장한 기기는 포털 → 기기 → "빠른 화면 전환"을 켜고 저장한다.
2. 회색 버튼으로 화면을 6번 넘긴다. UART에서 `display: ... mode=full partials=0`이 나온 뒤 `mode=fast partials=1..5`, 다시 `mode=full partials=0`이 나오는지 본다.
3. 육안 확인: fast 프레임에서 전체 반전 깜빡임이 없는지, 글자 반전이나 깨짐이 없는지, `elapsed_ms`(목표 1초 안팎), 5회 뒤 잔상 정도.
4. 반전되거나 아무것도 바뀌지 않으면 설정을 끄고 로그를 첨부한다(ADR 0034 위험 항목).

## 게이트 판정
- 이번 단계 목표: PARTIAL (Host/Build PASS, Hardware는 UART 동작만 확인, 육안·5회 주기 미검증)
- 다음 단계: 위 육안 확인을 하고, 결과에 따라 극성과 기본값을 결정한다. 잔상 100회 측정(U-03)도 남아 있다
