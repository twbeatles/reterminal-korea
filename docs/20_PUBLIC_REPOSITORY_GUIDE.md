# 공개 저장소 준비와 개인 설정

기준: 2026-11-13, [ADR 0030](adr/0030-public-source-private-configuration.md). 2026-11-20 공개 저장소 `reterminal-korea`로 게시했다. 공개 이력은 개인 이메일이 없는 **단일 스냅샷 커밋**에서 시작하며, 개발 중 로컬 커밋 이력은 공개하지 않는다.

## 개인 설정 위치

기본 배포 환경 `reterminal_e1001`은 개인 헤더를 읽지 않는다. Wi-Fi와 API 키는 업로드 후 기기 포털에서 입력한다. 개발 환경만 아래 외부 헤더를 초기값으로 사용하며 유효 NVS가 우선한다.

```powershell
$privateConfigDir = Join-Path $env:USERPROFILE '.config/kdash-e1001'
New-Item -ItemType Directory -Force -Path $privateConfigDir | Out-Null
# 외부 헤더가 이미 있으면 복사를 생략한다. 값은 로컬 편집기에서만 입력한다.
if (-not (Test-Path (Join-Path $privateConfigDir 'kdash_private_credentials.h'))) {
  Copy-Item include/dev_credentials.example.h (Join-Path $privateConfigDir 'kdash_private_credentials.h')
}
pio run -e reterminal_e1001_dev
```

다른 외부 디렉터리는 `KDASH_PRIVATE_CONFIG_DIR`로 지정한다. 저장소 내부 경로는 빌드 hook이 거부한다. 키 자체를 환경 매크로·CLI·채팅에 넣지 않는다. 이번 로컬 작업에서는 기존 ignored 헤더와 메모를 외부 디렉터리로 이전하고 바이트 일치를 확인한 뒤 저장소 안 사본을 제거했다. 메모는 `credentials-notes.md`다. 외부 파일은 Git으로 백업되지 않으므로 개인 백업을 별도로 관리한다.

개발 `.bin/.elf`에는 실제 키가 들어갈 수 있다. 외부 헤더 분리는 소스 추적 방지이며 바이너리나 기기 NVS의 추출 내성을 보장하지 않는다. `.pio/`, `dist/`, 개인 헤더, 로컬 메모, 원시 증거·이미지는 공유하지 않는다. 공개 설명용 `docs/images/`와 검토한 Markdown 증거 보고서는 유지한다. 이전 커밋의 생성 파일은 이력에 남는다.

## 공개 전 검사

```powershell
# 검토한 변경만 stage한 후 검사한다. 도구는 작업 파일이 아니라 Git 인덱스를 검사한다.
python tools/check_public_repo.py
python tools/check_public_repo.py --history
python -m pytest host/tests -q
pio run -e reterminal_e1001
# clean committed checkout에서만 실행한다. 기기에 flash하지 않는다.
python scripts/package_release.py --version 1.0.0
```

인덱스 검사는 비밀 파일 경로, 패턴, 외부 헤더에 있는 실제 로컬 키·SSID의 바이트와 URL 변형을 점검한다. `--history`는 reachable Git blob과 작성자·커미터 이메일도 점검한다. 값은 출력하지 않는다. 합성 검출기 테스트 데이터 예외는 검토한 정확한 blob에만 적용하고 실제 로컬 값은 예외 없이 거부한다. 알려지지 않은 비밀, 이미지 내용·OCR, unreachable object, 외부 첨부물까지 안전하다는 증명은 아니다. 공유할 이미지와 Markdown의 개인 위치·기기 정보는 사람이 검토한다.

릴리스 도구는 개인 빌드 marker, 알려진 로컬 키·SSID, 개발 hook 및 환경 build override를 거부한다. CI는 인덱스 검사를 실행한다. 개발 hook이 없는 기본 빌드는 외부 개인 헤더가 존재해도 가능하다. 소스 검사가 통과해도 개발 바이너리를 공유하면 안 된다.

## Git 작성자와 공개 이력

공개 저장소의 커밋은 GitHub noreply 주소(`twbeatles@users.noreply.github.com`)로만 작성한다. 과거 로컬 이력에는 noreply가 아닌 작성자 이메일이 있었고 `.gitignore`로는 과거 메타데이터를 지울 수 없으므로, 공개할 때는 현재 트리를 새 이력으로 게시했다. 같은 이유로 로컬 개발 이력과 공개 저장소의 이력은 서로 이어지지 않는다. 전역 Git 설정은 바꾸지 않는다.

공개 전 점검은 `python tools/check_public_repo.py`(인덱스)와 `--history`(공개할 이력)로 하며, 이미지 내용과 알려지지 않은 비밀은 사람이 검토한다. 라이선스 파일은 저장소 소유자가 결정해 추가한다.
