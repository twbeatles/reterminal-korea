# Phase Progress Report — P0/P1/P2-core/P3-host + P5-skeleton (Host 검증)

- 날짜·시간(KST): 2026-11-02
- 작업 단계/티켓: P0-01, P0-04(부분) · P1-01~P1-04 · P2-02~P2-04(Host) · P3-01~P3-05(Host) · P5-01~P5-04(Host 스켈레톤)
- 커밋 SHA / 공식 기반 commit: (git init만 수행, 커밋 없음 — 사용자 승인 전 커밋 금지)
- 작업 환경: Windows 11 + Python 3.15.1 + pytest 9.2.0 + Pillow 13.0.0 / C++ 툴체인 없음
- 기기 연결 여부 및 모델 확인: 미연결 (실기 없음)
- API 실키 접근: 없음 (fixture + Mock만 사용, 키 절대 미포함)

## 구현 내용

- 변경 파일: `platformio.ini`, `include/` 4종, `src/domain|providers|storage|power|net|provision|ui|drivers|app` 22종,
  `host/kdash` 7종 + `host/tests` 8종, `test/fixtures` 13종, `test_native/test_core.cpp`,
  `tools/` 2종, `scripts/ci_local.{ps1,sh}`, `bitbucket-pipelines.yml`,
  `hardware/board_e1001.md`, `hardware/driver_comparison.md`, `docs/BUILD.md`,
  `docs/adr/0001-host-oracle.md`, `THIRD_PARTY_NOTICES.md`, `.gitignore`
- 변경 목적: 문서 패키지의 P0(골격)→P1(도메인·파서·스케줄·UI)→P2(설정·캐시·보안·재시도)→P3(생활 API 정규화)→P5(금융 구분) Host 구현
- 기존 기능 영향: 신규 저장소 구조이므로 회귀 대상 없음

## 재현 가능한 증거

| 구분 | 명령/방법 | 결과 | 검증 단계 | 증거 경로 |
|---|---|---|---|---|
| Host unit/integration | `python -m pytest host/tests -q` | 40 passed | DONE_VERIFIED_HOST | (콘솔 로그) |
| Fault injection | bit-flip CACHE-01, XML-200, 401/429, \*-키오류 | PASS (거부·유지) | HOST | `host/tests/test_*.py` |
| Mutation probe | `parse_quoted("-")->(0,2)` 뮤턴트 | KILLED (1 failed) 후 원복 | HOST | 본 보고서 |
| Host preview | `python tools/generate_preview.py` | 10 PNG+PBM, 800x480, 클리핑 없음 | HOST | `evidence/p1/host-preview/` |
| Sanitizer | `fixture_sanitizer.py` 자가검사 | REDACTED 확인 | HOST | `tools/fixture_sanitizer.py` |
| C++ brace/include 점검 | 괄호 균형 스크립트 | OK | BUILD (정적) | 본 보고서 |
| E1001 smoke | — | NOT_RUN | BLOCKED_HW | — |
| Live API | — | NOT_RUN | BLOCKED_CREDENTIALS | — |
| Sleep/wake 100회 | Host 상태머신 루프 | PASS (시뮬레이션) | HOST | `host/tests/test_schedule.py` |

## 독립 검증 및 보안

- 단위/포맷/환율 기준일 Oracle: Decimal 독립 계산 vs `parse_quoted` 일치 (138450/92130); KST 시각 stdlib datetime 대조; 발표시각 CSV 대신 슬롯표 + 경계 테스트
- Mutation 테스트 결과: `-`→0 뮤턴트 killed (원복 후 40 passed)
- 자격증명 유출 검사: SEC-01 스캔 PASS (금지문구·거부단언 제외, 코드 내 활성 우회 0건)
- 사용한 API 출처와 기준일 검증: fixture 기반만 검증, 실응답 미검증

## 미완료/블로커

- BLOCKED_HW: 실기 없음 — UART 부팅, 버튼 3개, SHT40, RTC/NTP, 배터리 ADC, deep-sleep/wake, ePaper full/partial·잔상, TLS 실기 검증 전부 미검증
- BLOCKED_CREDENTIALS: data.go.kr/KMA·AirKorea·EXIM·KIS 키 없음 — live probe·응답스키마·호출상한 미검증
- BLOCKED_NETWORK: 사용하지 않음 (Host 테스트는 로컬 fixture만 사용)
- BLOCKED_TOOLCHAIN(추가): 이 세션 PC에 C++/PlatformIO 툴체인 없음 — `pio run -e reterminal_e1001`, `pio test -e host_native`, 공식 LVGL 예제 빌드 미실행
- 기타: 한글 CJK 래스터는 실 subset 폰트 필요 (U-10) — Host 프리뷰는 ASCII 플레이스홀더, 한국어 라벨 계약은 소스 단언으로 검증

## 게이트 판정

- 이번 단계 목표: PARTIAL (Host 가능 범위 DONE_VERIFIED_HOST, 장치·키·툴체인 BLOCKED)
- 다음 단계에서 해야 할 일: 툴체인 세션에서 `docs/BUILD.md` 공식 빌드 → `pio test` → 핀맵 확정 → 실키 probe (`templates/API_PROBE_RECORD.md`) → 실기 smoke·soak
- 실기 없이 성공이라고 표시하지 않은 항목: E1001 부팅/표시/절전/배터리, 모든 Live API, OTA·복구 매뉴얼 동작 검증
