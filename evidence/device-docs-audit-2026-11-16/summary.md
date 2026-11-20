# Phase Progress Report — 기기 공식 문서 재조사 감사

- 날짜·시간(KST): 2026-11-16
- 작업 단계/티켓: DD-01~DD-14 ([감사 문서](../../docs/21_DEVICE_DOCUMENT_AUDIT_2026-11-16.md), [ADR 0032](../../docs/adr/0032-device-document-audit.md))
- 커밋 SHA / 공식 기반 commit: 작업 기준 `657338b` / Seeed 고정 `8d97d891`(HEAD `51b92ab9`, E1001 관련 변경 0건)
- 작업 환경: Windows 11, PlatformIO Core 6.2.0, `espressif32@8.0.0`, Python 3.15
- 주의: 이 기기의 운영 이미지는 `reterminal_e1001_protected_dev`다. 기본 환경으로 플래시하면 키와 충전 보호가 빠진다(개인 헤더는 저장소 밖 `~/.config/kdash-e1001/`).
- 기기 연결 여부 및 모델 확인: 2026-11-17 사용자 요청으로 COM5(CH340) 연결 E1001에 플래시함(이전 작업 시점에는 미연결). 칩 재확인은 하지 않았고 부팅 로그의 `board=e1001`만 확인
- API 실키 접근: 사용하지 않음

## 구현 내용
- 변경 파일: `src/ui/RefreshPolicy.h`, `src/power/BatteryPolicy.h`, `src/app/EspCyclePlatform.cpp`, `tools/check_upstream.py`(신규), `host/tests/test_check_upstream.py`(신규), `test_native/test_device_docs_20261116.inc`(신규), `test_native/test_core.cpp`, `test_native/test_stage2.cpp`, 문서(21, ADR 0032, 10)
- 변경 목적: 패널 제조사 권장 전체 갱신 주기(5회), 위키 배터리 측정 대기(10ms), 동작온도 밖 진단 로그, 공식 저장소 드리프트 점검 자동화
- 기존 기능 영향: 부분 갱신은 비활성 상태라 화면 동작은 바뀌지 않는다. 배터리 측정은 1회당 5ms 늘어난다. 온도 로그는 범위 밖일 때만 출력된다

## 재현 가능한 증거
| 구분 | 명령/방법 | 결과 | 검증 단계 | 증거 경로 |
|---|---|---|---|---|
| Unit tests (C++) | `python tools/run_native_tests.py` | PASS 155/155 (기존 153 + 신규 2) | Host | 콘솔 |
| Unit tests (Python) | `python -m pytest host/tests -q` | PASS 209/209 (기존 204 + 신규 5) | Host | 콘솔 |
| TDD 확인 | 구현 전 네이티브 빌드 | 예상대로 실패(`PanelTempInRange` 없음) | Host | 콘솔 |
| Target build (clean) | `pio run -e reterminal_e1001 -t clean` 후 `pio run -e reterminal_e1001` | SUCCESS, RAM 78,468 B / Flash 1,977,234 B (문서 수정 뒤 재실행) | Build | 콘솔 |
| Stack gate | `python tools/check_stack_usage.py` | failures 없음 | Build | 콘솔 |
| Public repo scan | `python tools/check_public_repo.py` | 비밀정보 검출 없음 | Host | 콘솔 |
| Lint | `ruff check` (신규 Python 파일) | PASS | Host | 콘솔 |
| Upstream drift | `python tools/check_upstream.py` | exit 0, relevant=0 | LiveAPI(공개 GitHub) | `upstream-check.txt` |
| Upload | `pio run -e reterminal_e1001 -t upload --upload-port COM5` | PASS: 118.79초, 2,023,216 B 기록, hash verified, NVS 삭제 없음 | Hardware | 콘솔 |
| Boot smoke | `python tools/uart_logger.py COM5 <log> --duration-sec 40 --reset-once` | PASS: `kdash boot fw=1.0.0 board=e1001`, PMIC probe_ok, RTC 시각 유효, EPD 전체 갱신 1회(refresh 약 3.5s) 완료 | Hardware | `boot-after-flash.log`(로컬 보관, gitignore) |
| Upload 정정 | `pio run -e reterminal_e1001_protected_dev -t upload --upload-port COM5` | PASS: 145.61초, 2,024,368 B, hash verified. 위 기본 환경 이미지는 키·충전 보호가 없어 `config: source=default`, 키 미등록이었음. 운영 이미지로 교체 | Hardware | 콘솔 |
| Boot smoke(운영 이미지) | `uart_logger.py COM5 boot-protected-dev.log --duration-sec 75 --reset-once` | PASS: `config: source=dev` 키 전부 등록됨, Wi-Fi 연결, NTP 동기, provider 1~6 모두 `outcome=OK http=200`, 충전 cfg 1a→0a(충전 보호 적용) | Hardware | `boot-protected-dev.log`(로컬, gitignore) |
| Sleep/wake / 전류 / 저온 / 배터리 대조 / 5회 부분 갱신 | — | NOT_RUN | Hardware | — |

## 독립 검증 및 보안
- 자격증명 유출 검사: `check_public_repo.py` 통과. 신규 도구는 인증 정보를 쓰지 않고 TLS 검증을 우회하지 않는다
- 사용한 출처와 기준일: 감사 문서 §1 표

## 미완료/블로커
- BLOCKED_HW: sleep 전류와 GPIO hold 판단(DD-05/06), 저온 화질(DD-04), 배터리 전압 대조(DD-02/13), 부분 갱신 5회 주기 잔상(DD-01)
- BLOCKED_NETWORK: GDEY075T7 사양서·사용 지침 PDF 원문을 읽지 못함(다운로드 셸만 수신)
- 기타: ESP-IDF 문서는 stable(v6.1.1) 기준이다. 펌웨어가 쓰는 Arduino-ESP32 3.3.12 SDK와 세부 동작이 다를 수 있다

## 게이트 판정
- 이번 단계 목표: PARTIAL. 문서 대조, Host, Build, 업로드, 부팅 1회는 PASS. 부팅 로그에 Wi-Fi·키 미등록(`config: source=default`)으로 나와 네트워크 경로는 이 실행에서 확인하지 못함. 수면·전류·저온·배터리 대조는 미실행
- 다음 단계: 감사 문서 §4의 실기 측정 4건
