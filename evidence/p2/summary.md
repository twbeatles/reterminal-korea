# Phase Progress Report — 정밀 재분석 + 남은 범위 구현 (P0-02 심화~P6)

- 날짜·시간(KST): 2026-11-02
- 작업 단계/티켓: P0-02(공식원문 확정) · URL/CSRF/센서/갱신정책/FSC게이트 ·
  PGM 전환 · 폰트서브셋 실측 · 복구/위협모델/OTA ADR · 버전스탬프 · 커버리지
- 커밋 SHA / 공식 기반 commit: 로컬 미커밋 / upstream `8d97d891`
- 작업 환경: Windows 11 + Python 3.15.1 + pytest + Pillow + fontTools 4.68.0 + PlatformIO 6.2.0(신규)
- 기기 연결 여부: 미연결 (실기 없음)
- API 실키 접근: 없음

## 구현 내용

- 공식 저장소 sparse 확인 → 핀맵 원문 확정: EPD 7/9/10/11/12/13,
  KEY0-2 = 3/4/5, 배터리 ADC 1·EN 21, I2C 19/20, SHT40 0x44, PCF8563 0x51,
  Serial1 RX44/TX43, LVGL `seeed_xiao_esp32s3`+`qio_opi`, combo 520.
  `platformio.ini`를 공식 값으로 일치시킴 (파티션/OTA는 32MB 검증 전 미확정).
- URL 빌더: KMA 2종·AirKorea 후보·EXIM exact-shape + 단일인코딩/키 원문삽입.
- 포털 강화: CSRF 발급/검증, AP명 `KDASH-E1001-XXXX` 검증, 2단계 리셋 유지.
- 센서 게이트: SHT40 범위 + 실내상식(−10~50°C) — 범위 밖은 Missing.
- 갱신 정책: Skip/Partial/Full + 10회 누적 Full + Gray4 미검증 Full.
- FSC 13시 게이트: 평일 13:00 KST 이후 1회.
- 프리뷰 PGM 전환 (02: PNG/PGM), 기존 PBM 정리.
- 폰트: OFL 확인 + 서브셋 실측 2,797,100B → 37,940B (137자/157글리프).
- 문서: `docs/RECOVERY.md`, `docs/11_SECURITY_THREAT_MODEL.md`,
  `docs/adr/0002-ota-deferred.md`, `docs/12_COVERAGE_AND_GAPS.md`,
  `scripts/version.py`, 릴리스 가드 테스트.

## 재현 가능한 증거

| 구분 | 명령/방법 | 결과 | 단계 | 증거 경로 |
|---|---|---|---|---|
| Host 전체 | `python -m pytest host/tests -q` | 52 passed | HOST | 콘솔 |
| 공식 빌드 | `pio run -d vendor-upstream/... -e reterminal_e1001` | 진행 중/결과 별도 기록 | BUILD | `evidence/p0/build.log` 예정 |
| Preview 10종 | `tools/generate_preview.py` | PNG/PGM 800x480 | HOST | `evidence/p1/host-preview/` |
| Subset 실측 | `subset.py --font ...Regular.ttf` | 37,940B, OFL 확인 | HOST | `evidence/p2/font-subset-report.txt` |
| Version | `version.py --version 0.2.0` | stamp+거부 2건 | HOST | `host/tests/test_release.py` |
| E1001/Live | — | NOT_RUN | BLOCKED_HW/CREDENTIALS | — |

## 자사 펌웨어 빌드 (P0-04, 본 세션 실측)

- 명령: `pio run -e reterminal_e1001` → SUCCESS (28.42s)
- `firmware.bin` 297,120B, sha256 앞 32자리 `5cf6f1d0d44906496ff54e3ebaa88143`
- 컴파일 중 실제 오류 7건 발견·수정: `Freshness.h` Types 미포함,
  `SetupPortal.h` stdint 미포함, `Coordinator.h` Layout 미포함,
  `Coordinator.cpp` NextDue 비한정, providers 상대경로 7건
- 보드 정의 기본값 Flash 8MB 표기 확인 — 32MB 파티션/OTA는 여전히 미확정

## 미완료/블로커

- BLOCKED_HW: 실기 UART·버튼·SHT40·RTC·배터리·sleep/wake·ePaper 실측·soak.
  공식 빌드 결과는 `evidence/p0/build.log`에 확정 기록됨.
- BLOCKED_CREDENTIALS: 전 Provider live probe·응답스키마·호출상한.
- 릴리스 표기: `P6-RC-UNVERIFIED-HW`.
