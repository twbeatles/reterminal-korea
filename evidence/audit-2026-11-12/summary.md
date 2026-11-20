# Phase Progress Report — 2026-11-12 감사 지적 수정

- 날짜·시간(KST): 2026-11-12 11:00~14:30
- 작업 단계/티켓: PROJECT_AUDIT.md(2026-11-12) ISSUE-007~014, G-1~G-9, §6 문서 불일치. ADR 0026.
- 커밋 SHA / 공식 기반 commit: main bc08fc5 + 감사 지적 전건 수정 및 실기 플래시 검증 완료.
- 작업 환경: Windows 11, PlatformIO `espressif32@8.0.0`, 보드 `seeed_xiao_esp32s3`(qio_opi), 호스트 테스트는 `tools/run_native_tests.py`.
- 기기 연결 여부 및 모델 확인: 사용자 E1001, COM5(CH340) 실연결. 업로드 이미지 해시 검증 및 부팅/화면 갱신 완료.
- API 실키 접근: 기기의 로컬 개발 설정(`config: source=dev`)으로 실 API 호출. 키 값은 어디에도 기록하지 않았다.

## 구현 내용

- 변경 파일(신규): `src/provision/LanPolicy.*`, `src/app/SymbolBatch.*`, `test_native/test_audit_fixes.inc`, `test/fixtures/kis_zero_price_synthetic.json`(합성), `docs/adr/0026-audit-2026-11-12-fixes.md`, 이 폴더.
- 변경 파일(수정): `PortalHandler.*`, `PortalServer_esp32.cpp`, `PortalRunner.cpp`, `PortalServer.*`, `SettingsCodec.*`, `RuntimeStore.cpp`, `RtcPersistence.*`, `Coordinator.*`, `Fetcher.cpp`, `CalendarRefresh.*`, `ViewModel.*`, `WifiConnector.cpp`, `WifiSupervisor.h`, `PageRotation.*`, `main.cpp`, `test_core.cpp`, README·docs/04·docs/12·ADR 0023·PROJECT_AUDIT.
- 변경 목적: ADR 0026 표 및 PROJECT_AUDIT.md 이슈 참조.
- 기존 기능 영향: 장외 KIS 호출이 마감당 재시도 예산 1회분까지 늘 수 있다(이력 없음 포함). 잘못된 키는 하루 1회 호출된다. 런타임 지문 정의 변경(기기 설정이 기본값이면 영향 없음, 실기 확인).

## 재현 가능한 증거

| 구분 | 명령/방법 | 결과 | 검증 단계 | 증거 경로 |
|---|---|---|---|---|
| Clean build | `pio run -e reterminal_e1001`, `pio run -e reterminal_e1001_dev` | PASS (경고 없음). RAM 78,380 B, Flash 1,968,458 / 1,969,462 B | Build | `build-summary.log` |
| Unit tests | `python tools/run_native_tests.py` | PASS 131/131 (신규 15) | Host | `native-tests.log` |
| Unit tests | `python -m pytest host/tests -q` | PASS 164/164 | Host | `pytest.log` |
| Fault injection | KIS 0원 종목, FSC 자료 없음, 토큰 만료 재발급 후 캡 경계, Wi-Fi 스캔 선택 및 개방형 비밀번호 초기화, 공휴일 달력 인증 오류 24h 재시도/재저장 해제 | PASS(호스트) | Host | `test_audit_fixes.inc` |
| Host preview | — | NOT_RUN | — | — |
| E1001 smoke | `pio run -e reterminal_e1001_dev -t upload --upload-port COM5` → UART 150초 | PASS: 부팅, `restore: kis_quotes=1 kospi=1 fx=1`, Wi-Fi 연결, NTP, 표시 ok=1 (`display: page=0 crc=514bbc47 elapsed_ms=4737 ok=1`) | Hardware | `upload-redacted.log`, `boot-redacted.log` |
| Live API | 같은 부팅 | KMA·에어코리아·EXIM·FSC VALID. KIS 첫 시도 토큰 1분 제한(403) → 65초 뒤 `OK requests=6`(토큰+4종목+코스피) | LiveAPI | `boot-redacted.log` |
| 자동 순환 | UART 400초(장중 목요일) | PASS: 5분 뒤 `rotate: page 0 -> 1`, 패널 갱신 ok | Hardware | `rotation-redacted.log` |
| Sleep/wake | — | NOT_RUN (USB 전원 상태) | — | — |

## 독립 검증 및 보안

- Oracle: Python 오라클의 `retry_allowed`(즉시 재시도 예산)는 바뀐 정책(하루 1회 재시도)과 충돌하지 않아 고치지 않았다. 골든 40건 통과.
- Mutation: `tools/mutation_check.py` 게이트 통과(기존 2종 KILLED). 신규 테스트에 대한 별도 mutation은 실행하지 않았다.
- 자격증명 유출 검사: `test_sec01_no_secrets_or_tls_bypass_in_repo` 통과. 이 폴더의 로그 전부 `find_secrets` 0건. 사용자 홈 경로는 `<home>`으로 가렸다.
- `tools/check_sources.py`: 외부 링크 1건 실패(wiki.seeedstudio.com HTTPError) — 외부 사이트 문제로 이번 변경과 무관.

## 미완료/블로커

- BLOCKED_HW(사람 조작 필요): 배터리 전원 LAN 포털 + 공유기 끄기 → 10분/30분 뒤 종료·절전(ISSUE-007), 배터리 갱신 전류 비교(ISSUE-010), 포털에서 기기 설정만 저장 → 재부팅 뒤 `config: device-only change; runtime cache kept` 로그와 시세 유지(ISSUE-008), 기기 탭 "종목 N 실패" 육안 확인.
- BLOCKED_CREDENTIALS: 없음. 단 KIS가 존재하지 않는 종목에 실제로 어떻게 답하는지는 확인하지 않았다(합성 픽스처로 대체).
- 관찰(이번 수정 대상 아님): 기기를 연달아 리셋한 직후 KIS 종목 조회가 초당 제한(HTTP 500, `EGW00201`)으로 몇 번 실패하다 성공했다(`rotation-redacted.log`). 변경 전과 요청 순서·간격은 같다. 필요하면 KIS 요청 사이 간격을 별도 ADR로 정한다(ADR 0019에 같은 제한 기록).
- 기기 설정 출처가 `dev`(NVS 비어 있음)라서 dev 환경으로 플래싱했다. 기본 환경으로 올리면 포털 설정이 필요하다.

## 게이트 판정

- 이번 단계 목표: SUCCESS — 코드·호스트 131개/164개 테스트·타깃 빌드·실기 플래시·부팅 및 화면 갱신 검증 완료.
- 다음 단계: 배터리 모드 장기 계측 및 브라우저 포털 수동 E2E 확인.
