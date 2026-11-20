# 백로그 과제 구현 지시 (docs/13 + docs/tasks 기반)

이 저장소의 남은 과제를 이어서 구현한다. 아래 순서를 지킨다.

1. 읽기: `AGENTS.md` → `docs/13_ROADMAP.md` → 수행할 `docs/tasks/Txx_*.md` → 그 문서가 가리키는 docs/ADR/소스.
2. 과제 선택: 사용자가 지정하지 않았다면 `docs/13_ROADMAP.md` 3절 권장 순서에서 **선행 과제가 끝났고 상태가 NOT_STARTED/PARTIAL인 첫 과제**를 고른다. 키·실기가 필요한 단계만 남았다면 BLOCKED로 표시하고 다음 과제로 넘어간다.
3. 기준선 확인: 작업 전에 다음을 실행해 녹색인지 확인한다. 이미 실패 중이면 그 원인부터 보고한다.
   - `python -m pytest host/tests -q`
   - `python tools/run_native_tests.py`
   - `pio run -e reterminal_e1001`
4. 구현: 과제 문서의 "구현 단계"를 순서대로, 단계마다 테스트를 먼저 또는 함께 작성한다.
   - 판단 로직은 이식 코드로 두고, 하드웨어 호출은 기기 전용 어댑터에만 둔다(host_native 빌드에서 제외).
   - 제공자 의미를 바꾸면 `test/golden/provider_cases.json`부터 고친다(C++·Python 공통).
   - 공식 예제·데이터시트·API 문서의 실제 원문을 확인하기 전에는 핀·명령·코드값·경로를 추측해 확정하지 않는다. 확인하지 못한 값은 "출발값"으로 표시한다.
5. 검증: 단계마다 위 3개 명령을 다시 실행한다. 새 로그·응답·화면 문자열은 `test_sec01_*`(비밀 스캔)를 통과해야 한다.
6. 기록:
   - 과제 문서 상단의 상태 갱신
   - `docs/12_COVERAGE_AND_GAPS.md` 상태표 갱신
   - 스펙과 다른 결정은 ADR 작성
   - `evidence/p<N>/summary.md`를 `templates/PROGRESS_REPORT.md` 형식으로 작성(실행한 명령·결과 수치·미검증 항목)
7. 금지: 사용자 기기 무단 플래시, NVS 전체 삭제, eFuse/secure boot 변경, 키·토큰을 채팅·커밋·로그에 남기기, `setInsecure()`, 빌드 성공을 실기 동작으로 보고하기, 커밋·푸시(사용자 요청 시에만).
