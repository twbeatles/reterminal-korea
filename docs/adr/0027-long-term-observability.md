# ADR 0027 — 장기 운영 관측 경계

- 상태: 채택 (Host/Build 검증 결과는 증거 보고를 따른다)
- 날짜: 2026-11-13
- 질문: USB로 재부팅 없이 운영할 때 메모리 상태와 시험 기간을 어떻게 증명하는가?
- 선택: UART에 60초 주기로 `health:`를 출력한다. 64bit `esp_timer_get_time()` 기반 uptime, 내부 8bit heap의 free/minimum/largest, 별도 PSRAM free, USB/Wi-Fi bool만 기록한다. NVS 쓰기·새 라이브러리·운영 서버는 추가하지 않는다. 출력 간격 검사는 loop에서 수행하므로 긴 작업 중에는 로그가 지연될 수 있다. 이 지연도 시험의 관찰 대상이다.
- 기존 계약과의 관계: T11의 "매 사이클 heap" 계획을 일정한 60초 관측으로 보완한다. 이 지표는 작업 중 peak를 모두 보여주지 않으며 minimum과 largest를 함께 검토한다. boot heap은 ESP API의 별도 범위이므로 내부 heap 시계열과 혼합하지 않는다.
- 도구: 기본 soak PASS는 `scope=log_sanity_only`로 명확히 표시한다. 선택 옵션 `--min-duration-sec`은 최소 2개 health 샘플, 단조 증가 uptime, 기간, 최대 공백, USB 전원, 재부팅 유무를 검사한다. JSON `scope=continuous_usb_observation` PASS도 기능·메모리 추세·실기 전체 합격은 아니다. 기존 CI fixture의 의미를 보존한다.
- 대안: 부팅 로그만 사용(연속 누수 관측 불가), 매 loop 출력(UART 부하), 자동 주기 재부팅(원인을 숨기고 충전 제어 보장도 해결하지 못함).
- 비용: 매분 짧은 UART 1줄 및 heap 통계 조회. 실제 실행 시간·전류 영향은 실기 미검증이다.
- 충전: 현재 read-only PMIC 계약 유지. 80% 제어는 보드 리비전·정확한 칩과 전류 계측 조건을 확인한 후 별도 ADR로 결정한다.
- 검증 증거: [감사 계획](../17_LONG_TERM_OPERATIONS_AUDIT.md), [단계 보고](../../evidence/long-term-2026-11-13/summary.md).
- 되돌리는 방법: `LogHealth` 호출/함수와 관련 include를 제거하면 펌웨어 동작을 복원할 수 있다. 도구의 기존 단일 파일 호출 방식은 유지된다.
