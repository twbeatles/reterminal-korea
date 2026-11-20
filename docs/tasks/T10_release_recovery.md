# T10 — 릴리스·복구·파티션 (OTA는 연기)

- 상태: 패키징 스크립트 DONE (파티션 ADR 0005 제안, USB 복구 실기 BLOCKED_HW)
- 선행: T02~T07 기능 완료
- 필요 자원: 실기(32MB Flash 확인, USB 복구 시험)
- 관련: docs/08(릴리스 게이트), `docs/RECOVERY.md`, ADR 0002, `scripts/version.py`, `host/tests/test_release.py`

## 1. 파티션 결정 (U-01 잔여)

1. 실기에서 Flash 크기를 확인합니다. `esptool flash_id` 출력을 기록하고 `ESP.getFlashChipSize()`를 부팅 로그에 넣습니다.
2. 32MB가 확인되면 커스텀 파티션 표 `partitions_e1001.csv`를 만듭니다.
   - 포함: nvs(설정·스냅샷·달력, 64KB 이상), app0
   - OTA를 연기하는 동안 app1은 예약만 하거나 생략하고, 그 결정을 ADR에 남깁니다.
   - coredump(64KB) 포함을 권장합니다(현장 크래시 분석용, 비밀 미포함 확인 필요).
3. `platformio.ini`에 `board_build.partitions`와 `board_upload.flash_size`를 반영합니다. 확인 전에는 지금처럼 보드 기본값을 유지합니다.
4. 결정은 ADR `0005-partition-layout.md`로 남깁니다.

## 2. 릴리스 산출물 스크립트 `scripts/package_release.py`

- 입력: `--version X.Y.Z`. git 작업 트리가 깨끗한지 확인합니다(더러우면 거부).
- 동작
  1. `pio run -e reterminal_e1001` 실행
  2. `.pio/build/reterminal_e1001/{firmware.bin, bootloader.bin, partitions.bin}` 수집
  3. `dist/firmware-e1001-<ver>.bin`과 각 파일의 sha256
  4. `version.json`(기존 `scripts/version.py`) + 플래시 오프셋·명령 안내 `FLASHING.md`
- 검사
  - 바이너리 안에 비밀 marker가 없을 것: `strings`로 추출한 뒤 `find_secrets` 검사(docs/04 "firmware.bin 스캔")
  - `version.json.board == reterminal_e1001`
  - 펌웨어 버전 문자열이 `KDASH_FW_VERSION`과 같을 것
- 테스트: `host/tests/test_release.py`에 "더러운 트리 거부", "board 불일치 거부"를 추가합니다(빌드는 모킹).

## 3. 설정 마이그레이션 게이트

- 이전 릴리스 설정 blob(fixture)을 새 펌웨어 코덱으로 읽는 테스트를 매 릴리스마다 추가합니다(T02 `MigrateSettings`).
- `kSettingsSchemaVersion`을 올릴 때는 마이그레이션 함수와 fixture가 반드시 함께 있어야 합니다(테스트로 강제).

## 4. USB 복구 검증 (docs/RECOVERY.md 3단계)

- 실기에서 다음을 확인하고 문서의 "실기 대조 후 기입" 칸을 채웁니다.
  - 부트 모드 진입 버튼 조합
  - COM 포트 표시 이름(USB-UART 브리지)
  - `pio run -t upload` 성공
- 전체 erase는 문서에 "파괴적"으로만 안내합니다. 에이전트는 실행하지 않습니다.

## 5. OTA (연기 — ADR 0002)

이 과제에서는 구현하지 않습니다. USB 복구가 실기에서 검증된 뒤 별도 과제로 엽니다. 그때의 전제 조건은 다음과 같습니다.
- 서명 검증(해시만으로는 불충분)
- 롤백 슬롯
- 서명 키 관리 방안
- secure boot/flash encryption fuse는 사용자 명시 승인 없이 금지

## 6. 완료 기준

- [ ] 패키징 스크립트 + 테스트, `dist/`는 `.gitignore` 처리 확인
- [ ] 파티션 ADR (실기 확인 후)
- [ ] docs/08 릴리스 게이트 체크리스트 전 항목에 상태 기재 (미검증은 미검증으로)
