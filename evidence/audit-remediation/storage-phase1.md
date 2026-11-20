# Phase Progress Report — storage remediation

- 날짜·시간(KST): 2026-11-02
- 작업 단계/티켓: 감사 ISSUE-001, GAP-09; RuntimeStore 준비
- 커밋 SHA / 공식 기반 commit: main 커밋 없음 / 8d97d8912987c07529287629f7e73039bb0e6384
- 작업 환경: Windows PowerShell / Python 3.15 / ziglang native runner / espressif32 8.0.0
- 기기 연결 여부 및 모델 확인: 없음
- API 실키 접근: 없음

## 구현 내용

- 변경 파일: src/storage/SlotStore.*, SettingsCodec.cpp, SnapshotCodec.cpp의 heap temporary, CacheStore.h, RuntimeStore.*, test_native/test_audit_storage.inc, tools/check_stack_usage.py
- 목적: 다중 KB 태스크 자동 배열 제거, meta 실패 이후 연속 쓰기 복구, 설정 의미 검증, snapshot/status 일괄 checkpoint와 readback
- 기존 기능 영향: 설정/슬롯 바이너리 형식 유지. 새 runtime payload kind=5, run.a/b 사용. RuntimeStore는 clock trust/token/credentials를 snapshot에 저장하지 않으며 설정 fingerprint는 비공개 equality 목적만 사용.

## 재현 가능한 증거

| 구분 | 명령/방법 | 결과 | 단계 | 증거 경로 |
|---|---|---|---|---|
| Unit/integration | python tools/run_native_tests.py | PASS: 43 tests / 0 failures; golden 40 / 0 failures | Host | 세션 출력, test_audit_storage.inc |
| Fault injection | 위 suite의 연속 meta/slot 실패 + 100회 중단 위치 변화 | PASS, 위 43개에 포함 | Host | test_audit_storage.inc |
| Stack budget | python tools/check_stack_usage.py --json evidence/audit-remediation/stack-phase1.json | PASS | Build artifact | stack-phase1.json |
| Clean build | 미실행 | NOT_RUN | Build | 다른 담당자의 통합 빌드 결과 참조 |
| E1001 boot/power cut | 미실행 | NOT_RUN | Hardware | BLOCKED_HW |
| Live API/sleep | 미실행 | NOT_RUN | Hardware/LiveAPI | BLOCKED_HW/CREDENTIALS |

스택 JSON은 Phase1 ELF 기준 알려진 경로의 정적 프레임만 다룬다. 부팅 설정 읽기 합계 2,128B + 보수적 SDK reserve 2,048B = 4,176B < 8,192B. Read 프레임 176B. 모든 간접 호출/동적 사용량을 증명하는 도구가 아니며 실기 high-water mark는 미검증이다.

## 독립 검증 및 보안

- 기존 골든/설정 roundtrip 유지, invalid 시장/키 decode 실패 시 out 불변 검사.
- NVS 손상 시 defaults 복귀와 heap allocation 실패는 false 반환으로 처리.
- 기밀 출력 추가 없음. firmware secret scan은 별도 릴리스 담당자가 검증.
- Mutation 미실행.

## 미완료/블로커

- BLOCKED_HW: 부팅/실제 NVS 전원차단/high-water mark.
- RuntimeStore의 main 연결 및 통합 상태는 runtime 담당자의 후속 증거에서 판정.
- Heap scratch는 한 번당 bounded allocation이며 OOM 시 실패; 절대 스택 안전이나 기밀 하드웨어 추출 방지를 주장하지 않는다.

## 게이트 판정

- Host 로직/기존 포맷 호환: PASS.
- 실제 기기 내구성: 미검증.
- 다음 단계: main runtime checkpoint/restore 연결, 통합 빌드 후 스택 gate 재실행.
