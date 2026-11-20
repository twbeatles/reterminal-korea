# ADR 0008 — 아이콘 + 카드형 화면 구성

- 상태: 채택 (호스트 프리뷰·빌드 검증 완료, 실기 가독성 BLOCKED_HW)
- 날짜: 2026-11-02 (KST)
- 질문: 생활/시장/기기 화면에서 날씨·대기질·환율·종목·기기 상태가 라벨 텍스트만으로는
  한눈에 구분되지 않는다. 아이콘을 어떻게 넣고 화면을 어떻게 나눌 것인가.
- 선택:
  1. 아이콘 소스는 **Material Symbols Outlined (Apache-2.0)**. 생성기
     `tools/icon_subset/make_icons.py`가 sha256(`f7837ba8…ed5282`)으로 고정된 TTF를
     1bpp 24px/64px 비트맵으로 굽는다. wght는 600, 생성 헤더 `src/ui/icons/IconData.h`가
     `IconId`를 정의하는 단일 원천이다. 원본 TTF는 커밋하지 않는다.
  2. 화면은 docs/05의 2열 그리드를 유지한 채 **카드**로 나눈다. 카드마다 24px 아이콘,
     제목, 1px 선이 있는 머리띠를 두고, 핵심값(현재 기온, 고시환율, PM 수치)은 64px 숫자로 표시한다.
  3. 날씨 아이콘은 `fmt::WeatherIconFor(sky, pty)`로 정한다. PTY를 SKY보다 우선하고,
     결측이면 Unknown(?)을 쓴다. SKY/PTY는 단기예보 슬롯에만 있으므로 대표 아이콘 옆에
     `흐림 · 예보 HH:MM`처럼 출처 시각을 항상 적는다.
  4. 아이콘은 라벨을 대체하지 않는다(병기). 배지·DEMO·필수 문구(docs/05)는 그대로 유지한다.
- 다른 선택지:
  - 코드로 도형 아이콘 그리기: 에셋과 라이선스가 필요 없지만 표현력이 낮다(사용자 선택에서 제외).
  - Weather Icons(OFL)와 Material 혼용: 날씨 표현은 풍부하지만 스타일이 섞인다.
  - Tabler/Lucide SVG: 래스터화에 cairosvg라는 새 개발 의존성이 필요하다.
  - 아이콘 폰트를 BitmapFont에 합치기: 크기가 폰트 높이에 묶인다.
  - 같은 크기 타일 그리드: 열 간격 규칙(392..408)을 바꿔야 한다.
- 근거:
  - 래스터 스파이크(24/64px, wght 400과 600 비교) 결과 wght 600의 24px에서 선이 끊기지 않았다.
    `rainy_snow`/`rainy_light`/`rainy_heavy`는 구름이 없어 판별하기 어려워서
    비/눈은 `weather_mix`로 바꾸고 소나기는 Rain 아이콘을 함께 쓴다. 천둥과 안개는 KMA PTY에 없어 넣지 않았다.
  - 펌웨어 ELF 실측: icondata 심볼 35개 6,536B(비트맵 33개와 포인터 표 2개), 중복 링크 없음.
  - 배터리 아이콘 단계: USB→충전, `battery::IsLowPower`→Low, 그 외에는 공식 곡선 60%를 기준으로
    Full과 Mid를 나눈다. 퍼센트 숫자는 표시하지 않는다(U-17).
- 검증 증거: `evidence/ui-icons/summary.md`, `evidence/p1/host-preview/*.png`(11장),
  `test_native/test_ui_icons.inc`(3개 테스트), `host/tests/test_icons_gen.py`(4개 테스트).
- 후속 보정 (2026-11-02, UI/UX 점검):
  - 아이콘 3종 추가(`Thermometer`=device_thermostat, `Device`=developer_board, `Warning`=warning). 24px 전용.
    비트맵 36개 6,552B, 펌웨어 ELF 실측 icondata 심볼 38개 6,776B(포인터 표 2개 포함).
  - `현재 날씨` 머리띠가 히어로와 같은 구름 아이콘을 반복해서 온도계로 바꿨다(관측값 카드임을 표시).
  - 대기질 수치에 측정소·측정시각(dataTime)을 붙였다. 3시간 예보의 기온 중복을 없앴다.
  - 기기 화면: 공급자 한국어 이름과 빈 행 사유, 저전압·안전 모드 배너, 두 줄 버튼 안내, 기기 아이콘 머리띠.
  - CyclePlanner: 시계 미확인 상태의 KEY2가 아무 동작도 하지 않던 흐름을 고쳐 NTP 동기화를 시도하게 했다.
  - 증거: `evidence/ui-ux-review/summary.md`.
- 되돌리는 방법: `Renderer.cpp`를 이전 셀 배치로 되돌리고 `Icons.*`/`icons/`를 제거한다.
  ViewModel의 `icon`/`now_icon`/`pair` 필드는 렌더러가 쓰지 않아도 무해하다.
