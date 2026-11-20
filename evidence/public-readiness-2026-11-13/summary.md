# Phase Progress Report — 공개 저장소 준비

- 날짜·시간(KST): 2026-11-13 10:46.
- 작업 단계/티켓: PUB-01 개인 설정 외부 분리, PUB-02 소스/이력 검사, PUB-03 배포 바이너리 보호. 로컬 준비만 수행, 사용자 계정 정지로 push하지 않음.
- 커밋 SHA / 공식 기반 commit: 이 보고서를 포함한 커밋은 `git log -1 --format=%H -- evidence/public-readiness-2026-11-13/summary.md`. 추적 정리 커밋 `52087b5`; 공식 예제 `8d97d891`.
- 작업 환경: Windows, espressif32@8.0.0 / Arduino-ESP32 3.3.12, E1001.
- 기기 연결 여부 및 모델 확인: E1001 COM5 기존 24h 수집 중. 재시작/flash/초기화 없음.
- API 실키 접근: 있음. 실제 헤더/메모를 외부 경로로 이전하고 바이트 일치 확인 후 저장소 안 사본만 제거. 값은 출력하지 않음.

## 구현 내용

- 변경 파일: `scripts/private_config.py`, `tools/check_public_repo.py`, `scripts/package_release.py`, `platformio.ini`, `SettingsLoader.cpp`, 샘플 헤더·gitignore·CI·비밀 검출기·회귀 테스트, README 및 현재 빌드/설정 안내, ADR 0030.
- 변경 목적: 개발 설정은 `~/.config/kdash-e1001/` 또는 외부 `KDASH_PRIVATE_CONFIG_DIR`로 분리. 기본 환경은 개인 헤더 미사용. 릴리스는 개발 marker·알려진 키/SSID·개발 hook/환경 override 거부. Git 인덱스 및 reachable history는 값 없이 검사. 새 커밋은 로컬 noreply 작성자 사용.
- 기존 기능 영향: 개인 개발 환경 사용자는 [새 경로](../../docs/20_PUBLIC_REPOSITORY_GUIDE.md)로 이전 필요. 기존 기기 NVS/포털 우선순위 유지. 개인 개발 바이너리에는 키가 포함될 수 있어 공유 금지.
- Git 정리: 생성 증거 132개는 인덱스에서만 제외하고 로컬 사본 보존. 이미지/원시 증거·로그는 무시하고 검토한 Markdown·문서 이미지·필수 폰트는 유지. [정리 보고](../repository-hygiene-2026-11-13/summary.md). 과거 파일 이력은 유지.

## 재현 가능한 증거

JSON/로그는 로컬 전용이며 아래 수치는 보고서 추가 전 staged 검사 결과다. 최종 커밋 `3c57e45` 뒤 재검사 결과는 인덱스 449 blobs / 전체 993 blobs, 검출 0·금지 경로 0이었다. 과거 작성자 이메일 1종으로 이력 검사 종료 코드 1은 유지된다.

| 구분 | 명령/방법 | 결과 | 검증 단계 | 증거 경로 |
|---|---|---|---|---|
| Unit tests | `python -m pytest host/tests -q` | PASS: 204, 실패 0, 외부 플러그인 경고 13 | Host | `pytest.log` |
| Native regression | `python tools/run_native_tests.py` | PASS: 144, 실패 0 | Host | `../long-term-followup-2026-11-13/native-tests.log` |
| Lint | `python -m ruff check` 변경 Python 8파일 | PASS | Host | 콘솔 결과 |
| Firmware build | 기본 및 protected_dev, 외부 개인 헤더 사용 | PASS: 2환경 | Build | `build.log` |
| Public images | `inspect_images(ROOT,'1.0.0')` | PASS: 3이미지, 알려진 키·marker 미검출 | Build | `image-inspection.json` |
| Release package | clean commit `3c57e45`에서 `python scripts/package_release.py --version 1.0.0` | PASS: 빌드/검사/패키징, flash 없음 | Build | `package.log`, 로컬 `dist/e1001-1.0.0/` |
| Development separation | 실제 dev 이미지의 고정 marker와 알려진 값 존재 여부만 검사 | PASS: 둘 다 존재 확인, 값 미출력 | Build | `image-inspection.json` |
| Source index | `python tools/check_public_repo.py` | PASS: 447 blobs, 검출 0, 금지 경로 0 | Host | `index-audit.json` |
| Source history | `python tools/check_public_repo.py --history` | 파일 내용 PASS: 991 blobs, 검출 0. 과거 작성자 메타데이터 검토 필요로 전체 종료 코드 1 | Host | `history-audit.json` |
| Fault injection | 이력에만 남은 키/UTF-16/오염 바이너리/잘못된 private 경로/환경 override | PASS: 합성 fixture, pytest 포함 | Host | 테스트 소스 |
| Host preview/실기/LiveAPI/Sleep | 기기·UI 동작 변경 범위와 별도, 이번 이미지 미적용 | NOT_RUN | Hardware | 기존 24h 시험 유지 |

기본 `firmware.bin` SHA256: `a82b7f7106d496546fe6b8fa9df45344f16a54980c84372c5fb0d8edf377393f`. 개인 값의 해시가 아닌 공개 이미지 식별자다.

## 독립 검증 및 보안

- 단위/포맷/Oracle: 기존 204 Python/144 C++ 회귀, API 공급자 의미 변경 없음. 변이 2개 KILLED.
- 알려진 실제 로컬 키/SSID의 원문·URL 변형을 staged/과거 blob과 대조, 검출 없음. 검출기 합성 데이터 2개 blob만 정확한 hash로 검토 예외; 실제 로컬 값은 예외 없음.
- 공개용 작성자 `twbeatles` 및 username-only noreply를 저장소 로컬에 설정. 전역 설정 불변. 계정 귀속 확인은 미완료이며 계정 복구 후 GitHub Settings의 정확한 주소를 사용([공식 형식](https://docs.github.com/en/account-and-profile/reference/email-addresses-reference), 확인 2026-11-13).
- 스캔은 알려지지 않은 비밀·이미지 OCR·기기 추출 내성을 보증하지 않음. 과거 작성자 이메일 1종은 여전히 이력에 남음. 이력 재작성 없음.

## 미완료/블로커

- BLOCKED_HW: 추가 펌웨어의 실기 적용은 연속 시험 종료 후. 계측기 없음.
- BLOCKED_CREDENTIALS: 소스 준비에 없음. GitHub 계정 귀속 및 실제 noreply 주소 확인 대기.
- BLOCKED_NETWORK: 사용자 설명상 GitHub 계정 정지. push를 시도하지 않음.
- 공개 게이트: 과거 개인 이메일 공개 수용 또는 별도 승인한 이력 정리 필요. PUBLIC 최종 승인 상태가 아님.

## 게이트 판정

- 이번 단계: 로컬 분리·검증 PASS, 전체 이력의 개인정보 공개 게이트 PARTIAL.
- 다음 단계: 계정 복구 후 정확한 noreply 확인, 과거 작성자 메타데이터 처리 결정, 전체 이력 재검사 후 사용자가 공개할 시점에 push.
- 실기 없이 성공이라고 표시하지 않은 항목: 추가 이미지 실기 동작, NVS 암호화·하드웨어 비밀 추출 저항, 모든 미지 비밀에 대한 완전한 안전성.
