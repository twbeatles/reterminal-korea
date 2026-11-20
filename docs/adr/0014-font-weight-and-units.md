# ADR 0014 — 글꼴 굵기·단위 기호·큰 숫자 간격

- 상태: 채택, 실기 육안 확인 대기. 날짜 2026-11-11.
- 배경: 4계조(2bpp) 안티앨리어싱 글꼴은 IBM Plex Sans KR **Regular** 한 종이었다. 14/18px 한글 획이 가늘어 대부분 회색 단계로만 찍혔고, 원본 글꼴에 U+00B5가 없어 `µ`는 손으로 그린 5×8 비트맵을 확대해 썼다. 64px 숫자 바로 아래 18px 줄은 2px 간격이었다.
- 비교: 같은 계열 Regular/Medium/SemiBold × 선형/감마 0.6 양자화를 4계조로 렌더해 비교했다. 감마 보정은 Medium과 겹치면 획이 뭉개져 채택하지 않았다.
- 선택:
  - 14/18px → **Medium**, 64px 숫자 → **SemiBold**. 선형 2bpp 양자화는 유지한다.
  - 미세먼지 단위 `µg/m³` → `㎍/㎥`(U+338D/U+33A5, Plex 원본 글리프, 에어코리아 표기와 같음). 기존 µ 대체 비트맵은 생성기에 남겨 둔다.
  - 64px 값 아래 18px 줄 y+66 → y+72(반환 y+96), 단위 없는 값 반환 y+66 → y+70, 현재날씨 설명 y+70/96 → y+74/100.
- 출처/라이선스: google/fonts `ofl/ibmplexsanskr/IBMPlexSansKR-{Regular,Medium,SemiBold}.ttf`(OFL-1.1). Regular SHA256이 기존 생성 입력(53750379…)과 같다. Medium 4a7130f5…, SemiBold 823c1956…. TTF는 커밋하지 않고(`build/`, gitignore) 생성된 헤더의 `Input SHA256`으로 추적한다.
- 비용: Flash +2.4KB(1,812,822B, 54.2%), RAM 변화 없음.
- 검증: host 미리보기(evidence/live-api-integration/font-preview), native 79/0, pytest 162/0. 실제 패널의 획 선명도와 회색 단계 01/10 밝기 순서는 사용자 육안 확인 전까지 미검증이다.
- 대안: SemiBold 전면 적용(14px에서 한글 획이 붙음), 감마 보정(획 번짐), 1bpp 흑백(계단 현상 재발).
