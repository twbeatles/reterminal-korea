# T11 — 실기 검증·장기 soak·전류 측정

- 상태: PARTIAL — 기존 단기 실기 검증은 2026-11-12 증거 참조. 장기 soak·전류는 BLOCKED_HW. 최신 실행 계획은 [장기 운영 감사](../17_LONG_TERM_OPERATIONS_AUDIT.md), 관측 결정은 [ADR 0027](../adr/0027-long-term-observability.md).
- 선행: 해당 기능 과제 완료 (항목별로 진행 가능)
- 필요 자원: reTerminal E1001, USB 케이블, (권장) USB 파워미터 또는 전류 프로파일러, 기준 온습도계, 멀티미터
- 관련: docs/07 L3/L5, docs/06 "필수 테스트", PWR-01, TLS-01, NET-01

## 0. 공통 규칙

- 로그는 **USB-UART 브리지(`Serial1`, GPIO44/43, 115200)** 로 수집합니다. 원본은 커밋하지 않고, `python tools/fixture_sanitizer.py --text raw.log`로 세척한 사본만 `evidence/hw/`에 남깁니다.
- 사진에 SSID·AP 비밀번호·위치 정보가 보이면 가립니다.
- 측정마다 펌웨어 버전, git SHA, 날짜, 전원(USB/배터리)을 기록합니다.
- 결과는 `DONE_VERIFIED_HW` 판정 근거가 됩니다. 통과하지 못한 항목은 그대로 FAIL로 남깁니다.

## 1. 스모크 체크리스트 (L3)

| ID | 절차 | 합격 기준 | 관련 과제 |
|---|---|---|---|
| HW-01 | 전원 인가 | 부팅 로그 1줄에 fw 버전·reset 원인·heap/PSRAM, **비밀 없음** | 현재 |
| HW-02 | Wi-Fi + NTP | `time: NTP verified` | 현재 |
| HW-03 | 3개 호스트 TLS | KMA/AirKorea/EXIM `outcome=OK` (키 필요), 시계를 2020년으로 강제하면 TLS 실패 + 우회 없음 | 현재, T01 |
| HW-04 | 응답 크기 | provider별 본문 크기 기록, 64KB 미만 | 현재 |
| HW-05 | 화면 3페이지 | 사진 + 한글 깨짐 없음 + 긴 지역명 말줄임 | T04 |
| HW-06 | Full 갱신 시간 | 10회 평균·최대 | T04d |
| HW-07 | 잔상 | Full 10회 누적 사진, partial 시험은 U-15 | T04d |
| HW-08 | 버튼 | 짧게 20회·길게 10회 × 3버튼 오인식 0 | T06 |
| HW-09 | SHT40 | 기준계 대비 편차 기록 | T08 |
| HW-10 | RTC | Wi-Fi 끄고 재부팅 → RTC verified, 24h 드리프트 | T03 |
| HW-11 | 배터리 ADC | 멀티미터 대비 편차 | T08 |
| HW-12 | 포털 | 첫 부팅 → 설정 → 재부팅 → 조회, 틀린 비밀번호 → 이전 설정 유지, 10분 타임아웃 | T07 |
| HW-13 | 정전 내성 | 저장 직후 전원 차단 10회 → 설정 유지 | T02 |
| HW-14 | Flash 크기 | 32MB 확인 기록 | T10 |
| HW-15 | USB 전원 감지 | VBUS 감지 가능 여부(U-12) | T05 |

## 2. 장기 시험 (L5)

| ID | 시나리오 | 수집 지표 | 합격 기준 |
|---|---|---|---|
| SOAK-24 | USB 전원 24h | 매 사이클 `ESP.getFreeHeap()`, `getMinFreeHeap`, provider outcome, 재부팅 수 | heap 하향 추세 없음, 의도치 않은 재부팅 0 |
| SOAK-72 | 배터리 72h (또는 방전까지) | 사이클 수, 깨어 있는 시간, 전압 곡선 | 무응답 0, U-17 곡선 확보 |
| NET-100 | 공유기 끄기/켜기 100회 (스크립트로 스마트 플러그 제어 등) | 재연결 시간, 백오프 동작 | 호출 폭주 없음, 무한 재연결 없음 |
| PWR-100 | sleep/wake 100회 (버튼 50 + 타이머 50) | wake 원인 로그 | 누락 0 |
| CLK | NTP 차단 72h | RTC 기준 시각 오차 | 기록 (주장 아님) |

- 로그 수집 도구: `tools/uart_logger.py`는 2026-11-13 LT-10에서 실제 캡처·저장 전 마스킹·재연결·회전·UTC sidecar로 구현했다. `--duration-sec` 기본값 86400, 기존 출력 파일은 덮어쓰지 않는다. 실행 방법과 24h보다 여유 있는 수집 기간은 [운용 절차](../18_DEVICE_OPERATIONS_RUNBOOK.md)를 따른다.
- 분석 도구: `tools/soak_report.py`는 JSON 요약을 출력한다. 기본 PASS는 로그 형식/패닉 검사이며 기간 합격이 아니다. USB 24h 관측에는 `python tools/soak_report.py <sanitized.log> --min-duration-sec 86400 --max-gap-sec 120`을 사용한다. 메모리 추세는 수동 검토한다. 사이클 시간 히스토그램·배터리 sleep 구간의 외부 시각 분석은 미구현이다.

## 3. 전류·수명

- 측정 항목: deep sleep 바닥 전류, 사이클별 전하량(mAs) — Wi-Fi+TLS 조회 / 화면 갱신 / 버튼 처리 각각.
- 수명 추정 = 배터리 용량 ÷ (바닥 전류 + 하루 사이클 수 × 사이클 전하량). **실측 값으로만 계산합니다.** 공식 데모의 14µA를 근거로 쓰지 않습니다.
- 결과로 T05의 `max_sleep`, 저전력 임계값, 배터리 스케줄 간격을 조정하고 ADR에 남깁니다.

## 4. 완료 기준

- [ ] HW-01~15 결과표 (`evidence/hw/smoke.md`)
- [ ] SOAK-24, SOAK-72 보고서
- [ ] 전류 측정표와 수명 추정 근거
- [ ] `docs/12` 상태표를 `DONE_VERIFIED_HW`로 갱신 (통과 항목만)
