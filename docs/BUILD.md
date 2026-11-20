# BUILD — 재현 빌드·테스트 절차

2026-11-16 기기 문서 재조사 후속: Python 209개·C++ 155개 PASS, 타깃 빌드 RAM 78,468 B / Flash 1,977,234 B([증거](../evidence/device-docs-audit-2026-11-16/summary.md), [ADR 0032](adr/0032-device-document-audit.md)). 공식 저장소 변경 점검은 `python tools/check_upstream.py`로 한다(E1001 관련 파일이 바뀌면 exit 1, 네트워크 오류는 exit 2).

2026-11-16 감사 후속: [실행 보고](../evidence/audit-2026-11-16/summary.md), [ADR 0031](adr/0031-audit-2026-11-16-runtime-fixes.md). 주요 4건과 추가 공백을 조치했으며 최신 테스트·업로드 결과와 실기 미검증 경계는 이 보고를 따른다. Linux CI에 `pio run -e reterminal_e1001`을 추가했다.

2026-11-13 장기 운영 후속: [실기 보고](../evidence/long-term-device-2026-11-13/summary.md), [운용 절차](18_DEVICE_OPERATIONS_RUNBOOK.md). 기본 배포 환경과 로컬 충전 보호 개발 환경을 구분하며, `reterminal_e1001_validation_dev`는 장애 주입 전용이라 운영에 남기지 않는다. 최신 테스트 수·이미지 해시·실기 검증 경계는 이 보고가 우선한다.

처음 기기를 연결한다면 [API 키·첫 플래싱 준비 가이드](14_FIRST_FLASH_GUIDE.md)를 먼저 확인한다. 포털로 키를 입력하면 개발 헤더 없이 빌드할 수 있다.

2026-11-12 최신: Python 164개·C++ 127개 PASS, 타깃 빌드 RAM 78,380 B / Flash 1,967,550 B([감사 수정 증거](../evidence/audit-2026-11-12/summary.md), ADR 0026). 2026-11-11 v1 기준: Python 162개·C++ 80개 PASS, 타깃 빌드 RAM 70,804 B / Flash 1,814,058 B([V1 기준점](V1_BASELINE.md)). Windows에서 기본 `python`에 pyserial·컴파일러가 없을 수 있으니 필요하면 해당 패키지가 있는 인터프리터(예: `C:\Python315\python.exe`)로 `run_native_tests.py`·pytest를 실행한다. 직전 기록: chunked 수신/FSC V2 회귀 추가 후 Python 159개·C++ 77개 PASS. 첫 USB 업로드·화면 확인 및 API별 결과는 [실기 보고](../evidence/first-device-flash/summary.md), [실 API 통합 보고](../evidence/live-api-integration/summary.md)를 따른다. 아래 2026-11-05 수치는 과거 실행 이력이다.

기준일: 2026-11-05 KST. 현재 구현/한계는 docs/12_COVERAGE_AND_GAPS.md,
최신 실행 근거는 evidence/audit-followup/summary.md에 있다.

## Host 및 빌드

```powershell
python -m pytest host/tests -q
python tools/mutation_check.py
python tools/soak_report.py test/fixtures/soak_ok.log
python tools/run_native_tests.py
pio run -e reterminal_e1001
python tools/check_stack_usage.py --json evidence/audit-remediation/stack-current.json
python tools/render_previews.py
```

현재 실행: Python 159개, C++ 72개(내부 공통 골든 40건), 실패 0.
타깃 증분 빌드 SUCCESS, RAM 70,788 B / Flash 1,682,406 B.
Clean rebuild·실기 동작·실 API 통신 결과는 아니다.

- 네이티브 러너는 CXX → g++/gcc → clang++/clang → python -m ziglang 순서로 찾는다.
  누락 시 exit 2(BLOCKED_TOOLCHAIN)이며 CI는 성공 처리하지 않는다.
- Unity/ArduinoJson은 .pio/libdeps의 기존 패키지를 쓴다. 처음 준비할 때
  pio pkg install -e host_native를 실행한다. firmware env만 준비하면 Unity가 없을 수 있다.
- pio test -e host_native는 host GCC가 있는 별도 경로다. Windows의 현재 환경은
  gcc/g++ 부재로 실패하므로 공통 run_native_tests.py를 검증 게이트로 쓴다.
- 정적 스택 도구는 알려진 경로만 검사한다. 실제 SDK 간접 호출과 high-water-mark는 실기가 필요하다.
- 재현 가능한 생성 배열은 .gitattributes에서 LF를 강제한다.
- 실제 C++ 화면은 .pio/preview/*.pbm과 evidence/p1/host-preview/*.png에 생성된다.
  tools/generate_preview.py의 예전 ASCII 프리뷰는 실제 한글 렌더 검증의 대체가 아니다.
  이번 감사 후속 프리뷰 14종은 아래 명령으로 별도 증거 폴더에 생성한다.
  python tools/render_previews.py --output-dir evidence/audit-followup/previews
  GitHub Actions 워크플로는 제거했다(원격 CI 불필요).
- 로컬 검증은 scripts/ci_local.ps1, scripts/ci_local.sh로 수행한다. bitbucket-pipelines.yml도 남아 있다.
  플랫폼/컴파일러 다운로드가 실패하면 환경 장애로 기록하며, 실기 검증으로 승격하지 않는다.

## 본인 키의 개발 빌드

[개인 설정 분리 안내](20_PUBLIC_REPOSITORY_GUIDE.md)에 따라 샘플을 저장소 밖 `kdash_private_credentials.h`로 복사해 설정하고, **개발 환경 `pio run -e reterminal_e1001_dev`** 로 빌드한다. 기본 환경(`reterminal_e1001`)과 릴리스는 개인 헤더를 컴파일하지 않는다(ADR 0024/0030).
실제 키를 채팅·커밋·UART·이슈에 넣지 않는다. 이 파일은 ignore된다.
data.go.kr Encoding/Decoding 키를 모두 수용한다.

업로드는 실제 기기 쓰기 권한을 받은 경우에만 수행한다.
실기 검증 전용 옵션(개발 환경에서 환경변수로만 지정, 저장소 설정 아님):
`PLATFORMIO_BUILD_FLAGS="-D KDASH_DEV_LAN_ALWAYS=1 -D KDASH_DEV_ROTATE_MIN=1 -D KDASH_DEV_SCAN_SELFTEST=1"` — 포털 상시 열기, 순환 1분, 부팅 후 Wi-Fi 검색 개수 기록.
검증 뒤에는 이 옵션 없이 다시 올려 기본 설정으로 되돌린다.
로그는 E1001 캐리어 USB-UART GPIO44/43의 Serial1 115200으로 나간다.
현재 main은 설정/복구 → 센서 → 시각/Wi-Fi/조회 → 영속화 → 화면 → idle/sleep까지 연결한다.
실제 E1001에서 한 번도 검증되지 않았으므로 먼저 부팅/스택/패널/전원 smoke를 수행한다.
포털의 임시 AP 비밀번호만 현장 화면에 표시하며 저장된 비밀은 표시하지 않는다.

## PC에서 읽기 전용 API probe

```powershell
python tools/api_probe.py --dry-run
```

이 명령은 계획만 출력하며 검증을 주장하지 않는다. 실제 호출은 --live와
--provider 및 환경 키, 각 provider의 실제 일일 할당량 --daily-quota가 모두 필요하다.
키는 argv에 넣지 않는다. 필요한 환경 변수 이름은 KDASH_KMA_KEY, KDASH_AIR_KEY,
KDASH_EXIM_KEY, KDASH_KIS_APP_KEY, KDASH_KIS_APP_SECRET, KDASH_HOLIDAY_KEY다.

- --provider: kma/air/exim/fsc/kis/holiday 중 선택. 쿼터는 PROVIDER=N 형식이다.
- 실제 할당량의 1% 이내를 일일 ledger와 배타 lock으로 예약한다.
  실패/중단 예약은 안전하게 사용된 것으로 처리한다.
- raw는 ignore된 evidence/probe/raw 아래, 정제된 응답은 test/fixtures/live_* 아래 생성한다.
  자동 정제 후에도 공개 전 내용을 검토한다. raw를 커밋하지 않는다.
- 키 미제공/--live 누락은 exit 2, 실패/미완료 capture는 exit 1이다.
- 자정·발표 지연·연휴·KIS 제한 검증은 해당 시간대의 별도 실응답이 필요하다.
  일반 capture 성공으로 전체 공급자 계약이 검증됐다고 보고하지 않는다.

## 릴리스 패키징

```powershell
python scripts/package_release.py --version 1.0.0
# 호출 주기 배율은 포털 "기기 설정"에서 바꾼다(저장됨). build_flags의 -D KDASH_REFRESH_SCALE_PCT=200(100~400)은 저장값이 없을 때의 초기값이다.
```

clean committed Git + 개발 비밀 파일 없음이 선행 조건이다.
도구는 항상 빌드하고 실제 firmware marker의 board/version, 설정, 필수 firmware/bootloader/partitions,
해시와 알려진 비밀/marker를 검사한 후 dist/e1001-<version>에 원자적으로 구성한다.
검사 실패 시 fail closed한다. 임의 바이너리의 비밀 부재를 수학적으로 증명하는 도구는 아니다.

Flash offset/추가 bootstrap 이미지/32MB 배치는 실기와 해당 버전의 PIO upload recipe 대조가 필요하다.
패키지는 산출물 묶음이며 검증된 단독 복구 명령이 아니다. 자동 OTA/플래시/erase는 수행하지 않는다.

## CA·Flash

tools/gen_ca_bundle.py --verify는 CA 파일을 재생성하고 서버 체인을 조회한다.
이번 수정 검증에서는 실행하지 않았다. TLS 검증은 계속 필수다.
32MB는 실물 확인 전까지 확정하지 않고 기본 파티션을 유지한다(ADR 0002/0005).
