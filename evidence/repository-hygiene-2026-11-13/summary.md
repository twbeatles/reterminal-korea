# Phase Progress Report — GitHub 공개 전 산출물 추적 정리

- 날짜·시간(KST): 2026-11-13.
- 작업 단계/티켓: 사용자 추가 요청 — 불필요 이미지/증거 산출물 ignore 및 commit.
- 커밋 SHA / 공식 기반 commit: main `26bfd6f` 기반. 보드/공식 소스 변경 없음.
- 작업 환경(OS/PlatformIO/보드 패키지 버전): Windows, git. 펌웨어 환경 변경 없음.
- 기기 연결 여부 및 모델 확인: 기존 E1001 COM5의 USB 24h 수집 PID12536 유지, 포트 열기/리셋/플래시 없음.
- API 실키 접근: 없음.

## 구현 내용

- 변경 파일: `.gitignore`, `README.md`, `evidence/README.md`, 이 보고서. 기존 evidence 산출물 132개를 `git rm --cached`로 인덱스에서만 제외.
- 변경 목적: 생성 PNG·UART/빌드/테스트 로그·측정/진단 JSON·캡처 메타데이터·검증 txt는 로컬 전용. 검토한 Markdown 보고, README 대표 이미지 4개, 기기 빌드 필수 글꼴/아이콘 헤더, fixture/golden, 공식 참조 소스/라이선스는 유지.
- 기존 기능 영향: 상세 증거 132개/824,576 B가 현재 공개 파일 트리에서 빠짐. 로컬 파일 전부 존재 확인, 삭제하지 않음. 각 보고서의 상세 로그 경로는 로컬 경로임을 evidence README에 명시. 문서의 evidence 직접 링크는 Markdown 보고로 연결되어 있어 유지된다.

## 재현 가능한 증거

| 구분 | 명령/방법 | 결과 (PASS/FAIL/NOT_RUN) | 검증 단계 (Host/Build/Hardware/LiveAPI) | 증거 경로 |
|---|---|---|---|---|
| Index inventory | `git ls-files -z`, 경로별 크기 집계 | PASS: evidence 추적 151개 중 산출물132개 제외, 기존 MD19개 보존 | Host | Git index/이번 commit |
| Local preservation | 제외 전후 `Path(path).is_file()` 전수 확인 | PASS: 132개 모두 로컬 보존 | Host | 세션 명령 출력 |
| Ignore policy | `git check-ignore --no-index <path>` | PASS: active 캡처 ignore, docs/images·필수 폰트 헤더·soak fixture·보고서 KEEP | Host | `.gitignore` |
| Markdown links | 추적 MD의 evidence 링크 전수 조회 | PASS: 비 Markdown 산출물에 대한 직접 링크 0건 | Host | 세션 명령 출력 |
| Clean build / unit tests | ignore/인덱스 변경만 검사. 코드 개선 검증은 별도 보고 참조 | NOT_RUN(이 작업 자체) | Host/Build | `../long-term-followup-2026-11-13/summary.md` |
| Fault injection / Host preview / E1001 smoke / Live API / Sleep/wake | 기기·기능 변경 없음 | NOT_RUN | — | — |

## 독립 검증 및 보안

- 단위/포맷/환율 기준일 Oracle: 데이터/코드 변경 없음.
- Mutation 테스트 결과: NOT_RUN.
- 자격증명 유출 검사 범위/결과: `git log --all --name-only -- include/dev_credentials.h secrets.api-credentials.md .env` 결과 없음. 현재 저장소 보안 회귀는 같은 세션 코드 개선의 pytest에서 별도 실행. 과거 모든 Git blob의 비밀 부재를 증명한 것은 아님.
- 사용한 API 출처와 기준일 검증: 외부 API 사용 없음.

## 미완료/블로커

- BLOCKED_HW / BLOCKED_CREDENTIALS: 해당 없음.
- BLOCKED_NETWORK: 원격 저장소 미등록, push 불가.
- 기타 이슈: 과거 commit에는 이전 이미지/로그가 남아 있다. 이번 ignore/untrack은 이력을 지우거나 clone 전체 용량을 줄이는 작업이 아니다. 이력 재작성/강제 push 없음. 실제 비밀이 과거에 커밋된 사실이 발견되면 별도 조치가 필요하다.

## 게이트 판정

- 이번 단계 목표 PASS/FAIL/PARTIAL: PASS(현재 Git 추적 정책 및 로컬 보존).
- 다음 단계에서 해야 할 일: 새 로그/프리뷰는 로컬 evidence로 두고, 검토한 최소 결과만 Markdown 보고에 기록. GitHub remote를 등록한 뒤 main push.
- 실기 없이 성공이라고 표시하지 않은 항목: 펌웨어/24h 시험의 전체 합격. 이번 정리로 검증 수준을 올리지 않았다.
