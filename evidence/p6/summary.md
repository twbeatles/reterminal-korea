# Phase Progress Report — stage-4 (공식 전문 확보·추가 교정)

- 날짜·시간(KST): 2026-11-02
- 작업: 공식 원문 22건 전문 확보(`hardware/reference/`, MIT) 후 추가 대조·수정·테스트 보강
- 1차 출처: 공식 저장소 @8d97d891 (LICENSE MIT 확인 후 보관), 위키 스펙
- 기기 연결 여부: 실기 없음 (BLOCKED_HW 유지)

## 구현 내용
- 수정: 배터리 샘플링을 공식 게이지와 일치시킴 (10회·25ms·단순평균).
  기존 `Average8`(min/max 제외)은 공식과 달라 `MeanOf`로 교체 + 계약 상수
  (`kAdcSamples/kAdcIntervalMs/kAdcSettleMs`)를 테스트로 고정.
- 수정: `BatteryPolicy.h`에 누락된 `<stddef.h>` include 추가 (Arduino 빌드 FAIL 해결).
- 확인 후 변경 없음: `PercentMv` 보간식·클램프가 공식 `voltageToPercent`과 동등
  (정수 반올림 vs float 차이는 화면 표시용으로 문서화), PMIC REG08 매핑 전부 일치,
  순정 수면 모델(60s owner-guard)은 우리 deep-sleep 구조와 달라 참조만 기록.
- 문서: `hardware/reference/README.md` (출처·라이선스·파일별 용도),
  `official_product_facts.md` 보관 위치 반영, `driver_comparison.md` 순정 Gray4 기록,
  `THIRD_PARTY_NOTICES.md` 보관 고지.
- 도구: `tools/fetch_reference.py` (22건 재생성 가능, GxEPD2 제외).

## 재현 가능한 증거
| 구분 | 명령/방법 | 결과 | 단계 | 증거 경로 |
|---|---|---|---|---|
| Unit tests | `python tools/run_native_tests.py` | PASS (37/37) | Host | 터미널 로그 |
| Unit tests | `python -m pytest host/tests -q` | PASS (98) | Host | 터미널 로그 |
| Clean build | `pio run -e reterminal_e1001` | SUCCESS | Build | RAM 21.4% / Flash 38.0% |
| 참조 무결성 | `python tools/fetch_reference.py` | 22 files | — | `hardware/reference/` |
| E1001 smoke | 실기 없음 | NOT_RUN | Hardware | BLOCKED_HW |

## 독립 검증 및 보안
- 곡선 전구간(2500~4200mV) 단조·0~100·매듭점 일치 sweep (C++/pytest 양쪽).
- SY6974 전체 bus 매핑(0x00~0x07) 대조 — 우리 `!=0` 판정과 일치.
- Gray4 프레임 흐름(0x10/0x13→0x12→0x02→0x07, partial 없음) 확인 — T04d 참조용.
- 비밀 스캔 이상 없음 (`test_sec01_*` 통과).

## 게이트 판정
- PARTIAL (HOST+BUILD PASS, HW·Live 미검증).
- 다음 단계: 실기 입수 시 T11 + PMIC/곡선/버튼 대조.
