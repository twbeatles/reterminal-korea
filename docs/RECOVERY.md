# RECOVERY — USB 복구 매뉴얼 (08, P6-03)

> 상태: 문서만 작성, 실기 동작 미검증 (`P6-RC-UNVERIFIED-HW`).
> 어떤 `erase`도 에이전트가 자동 실행하지 않는다.

## 1단계: 버튼 롱프레스 → 설정 포털

1. KEY0 길게 누르기. Wi-Fi가 연결돼 있으면 같은 Wi-Fi 포털(기기 탭의 주소와 페어링 코드, 10분), 아니면 `KDASH-E1001-XXXX` AP가 최대 10분간 열린다. 다른 키의 롱프레스로 설정에 들어간다고 가정하지 않는다.
2. AP일 때: 기기 화면에 표시된 AP 비밀번호로 접속 → `http://192.168.4.1/` (고정, 외부 WAN 노출 없음). Wi-Fi가 바뀌어 같은 Wi-Fi 포털이 안 열리면 AP 포털을 쓴다(포털 메뉴의 "AP 모드로 다시 열기"도 같다).
3. 네트워크/지역/키를 수정 → 접속 성공 확인 후 원자 저장 → 재부팅.

## 2단계: 설정 백업/초기화

- 백업/복원: 같은 Wi-Fi 포털의 "설정 백업·복원"(`/backup`)에서 JSON을 내려받고 붙여넣어 가져온다. 지역·관심종목·공급자 사용 여부·기기 설정만 다루며 Wi-Fi와 API 키는 백업에 없고 가져오기로 바뀌지 않는다. 실기 검증은 아직 없다.
- 초기화: 2단계 확인 (`RESET` → 다음 요청의 `RESET-YES`) 후 설정 두 슬롯을 검증된 기본값으로 덮어쓰고 자체 runtime/calendar 키만 제거한다. 이 기본값 표식은 컴파일된 개발 설정의 재적용을 막는다.
- 전체 namespace/Flash erase는 구현하지 않았다. NVS 덮어쓰기를 물리적 비밀 완전 소거나 하드웨어 추출 방지로 주장하지 않는다.

## 3단계: USB-C 부트로더 복원

1. USB-C 연결 → 포트 확인 (Windows 장치관리자/실기 문서 대조).
2. 부트 모드 진입 (공식 flash recovery 문서의 버튼 조합 — 실기 대조 후 기입).
3. 복원 (택 1):
   - `pio run -t upload -e reterminal_e1001`
   - 수동 플래시 도구를 사용할 경우 정확한 버전의 PlatformIO upload recipe와 실기 Flash를 대조한다. offset/추가 bootstrap 이미지 확인 없이 firmware 단독으로 복구된다고 가정하지 않는다.
4. 부팅 로그에 비밀 미노출 확인 후 설정 포털에서 재설정.

## 릴리스 산출물 검사 (P6-02)

- `scripts/version.py`로 `version.json` 생성 (semver + git SHA + board).
- `scripts/package_release.py --version X.Y.Z`는 clean committed 소스에서 빌드해 `dist/e1001-X.Y.Z/`의 firmware/bootloader/partitions, SHA256SUMS, version.json, FLASHING.md를 만든다.
- 실제 바이너리 board/version marker·알려진 비밀/테스트 marker를 확인한다. 이 묶음은 아직 단독 복구 절차가 실기 검증된 배포물이 아니다.
