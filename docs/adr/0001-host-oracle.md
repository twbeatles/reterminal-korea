# ADR 0001 — Python Host 미러를 독립 Oracle로 사용

- 상태: **개정됨 (2026-11-02, 감사 ISSUE-003 반영)**
- 최초 채택: 2026-11-02 (KST)
- 질문: C++ 툴체인이 없는 세션에서 portable C++ 코어의 정확성을 어떻게 입증하는가.

## 최초 결정 (요약)

`host/kdash` Python 패키지에 같은 계약을 독립 구현하고 pytest로 검증했습니다.

## 문제 (감사에서 확인)

- C++ 코드는 한 번도 실행되지 않았습니다. `pio test -e host_native`는 `test_build_src` 기본값(False) 때문에 링크 단계에서 실패하는 구성이었습니다.
- 두 구현은 최소 5곳에서 서로 다른 결과를 냈습니다(KMA 슬롯 선택, AirKorea 측정소·PM2.5 신선도, watchlist 빈 토큰, 길이 단위). 그런데도 fixture가 단순해서 테스트는 녹색이었습니다.

## 개정 결정

1. **C++이 정본**이고, 실행 검증 게이트는 다음 두 가지입니다.
   - CI: `pio test -e host_native` (`test_build_src = yes`)
   - 로컬(컴파일러 없는 Windows 포함): `python tools/run_native_tests.py` (g++/clang/`pip install ziglang`)
2. **공통 골든 케이스** `test/golden/provider_cases.json`(40건)을 C++(`test_native/test_core.cpp`)과 Python(`host/tests/test_golden.py`)이 **같은 원시 응답 fixture**로 함께 실행합니다. 한쪽만 바꾸면 테스트가 실패합니다.
3. Python 미러는 원시 응답 본문을 입력으로 받는 형태로 맞췄습니다. 앞으로도 "독립 구현 + 공통 기대값" 구조를 유지합니다.

## 근거

07 독립 Oracle 요구는 유지하면서, "같은 fixture에서 같은 기대값"이라는 원래 의도를 실제로 강제합니다.
