# ADR 0006 — ViewModel 신설, Pages.h 유지 (T04a)

- 상태: 채택
- 날짜: 2026-11-02 (KST)
- 질문: T04a가 요구한 "Pages.h를 서식 적용 텍스트 중심으로 교체"를 그대로 하는가.
- 선택: 교체하지 않고 `src/ui/ViewModel.{h,cpp}` + `src/ui/Format.{h,cpp}`를
  신설한다. `Pages.h`는 `Coordinator`/`Fetcher`가 쓰는 스냅샷 모델로 유지하고,
  렌더러는 ViewModel 셀만 그린다.
- 이유: 기존 스냅샷 파이프라인·테스트를 깨지 않고 배지 결정표(G-11)를 순수
  함수로 고정하기 위해서. 렌더러 기하 oracle(여백·칸·배지 반전)은 통과.
- 되돌리는 방법: 전 파이프라인이 ViewModel로 이전하면 Pages.h를 제거하고
  이 ADR을 폐기한다.
