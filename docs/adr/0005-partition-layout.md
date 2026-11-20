# ADR 0005 — 파티션 배치 (T10, 실기 미확인)

- 상태: 제안 (32MB Flash 실기 확인 후 채택)
- 날짜: 2026-11-02 (KST)
- 질문: 커스텀 파티션 표를 지금 만드는가.
- 선택: 만들지 않는다. `esptool flash_id` + 부팅 로그 `ESP.getFlashChipSize()`
  로 32MB가 확인되기 전까지 보드 기본값을 유지한다.
- 확인 후 안(예정): nvs 64KB 이상(설정·스냅샷·달력), app0, coredump 64KB(현장
  크래시 분석용, 비밀 미포함 확인 필요), OTA 연기 중이므로 app1은 예약만.
- 릴리스 스크립트 `scripts/package_release.py`는 준비됨(깨끗한 트리 강제,
  board 일치 검사, sha256 산출). `dist/`는 `.gitignore` 처리됨.
- 되돌리는 방법: 실기 확인値を 기록하고 `partitions_e1001.csv` + ADR 갱신.
