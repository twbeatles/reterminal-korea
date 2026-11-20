# 08. 배포·OTA·USB 복구·CI 지시

## 개발/배포 원칙
- **Bitbucket-only 모드도 독립적으로 지원**. GitHub Actions를 필수 전제에 두지 않고 `pio run -e reterminal_e1001`, native unit tests 등을 로컬 명령으로 재현 가능하게 한다.
- 권장 CI: 리포지토리 호스팅과 무관한 `scripts/ci_local.sh`, Windows `scripts/ci_local.ps1`, 필요시 Bitbucket Pipelines `bitbucket-pipelines.yml` 래퍼.
- CI에는 진짜 API 키가 없어도 모든 테스트가 통과해야 하고, 네트워크 연결/API live contract 테스트는 별도 옵트인.
- Python은 font subset/Host preview/테스트 스크립트에서만 사용 가능, **기기 런타임에서는 필요 없음**.

## 버전 / 릴리스 산출물
- `0.x.y` semver; 별도 firmware build ID, source git SHA, 보드 모델, flash layout, 빌드 재현 도구 버전.
- `firmware-e1001-<version>.bin` + sha256 + 플래시 파라미터 안내, USB 복구 문서, 설정 마이그레이션 주의.
- 라이선스: Seeed 예제 코드·한글 폰트·외부 라이브러리·API 데이터의 각각 라이선스를 `THIRD_PARTY_NOTICES.md`에 명시. 원본 코드를 무단 일괄 복사하지 않는다.

## OTA 기준 (P6 또는 후속)
- 처음부터 OTA를 필수로 만들지 말고 **USB 복구 경로** 완성 후 선택적 로컬 OTA를 설계.
- 서버 없는 OTA = (a) 본인이 직접 접속한 기기 AP/로컬망에 새 펌웨어 파일 업로드, 또는 (b) 사용자 확인 후 공개 릴리스 자산 다운로드. **지속 운영 백엔드 필수 아님**.
- OTA 이미지 무결성 검증(해시만으로 악의적 변조 방지 불충분) 및 정당한 릴리스 서명/검증 키, 롤백 가능 파티션 검토.
- 새 버전 전환 실패 시 이전 부팅 가능한 슬롯 보존. ESP32 secure boot/flash encryption의 영구 fuse 활성화는 명시적 승인 없이는 금지.

## 복구 설계
- 1단계: 버튼 롱프레스에서 설정 포털 진입.
- 2단계: 설정 백업/초기화(네트워크와 API키 포함 여부 선택, 위험 경고).
- 3단계: USB‑C 부트로더/ESP Web Tools 또는 `pio run -t upload`로 복원(실제 포트 및 모드 공식 문서 대조).
- 매뉴얼에 `전체 erase`는 **비밀·설정 모두 사라지는 파괴적 절차**로 분명히 표시하며 에이전트가 자동 실행하지 않는다.

## 릴리스 게이트
- [ ] Windows + Linux 적어도 한 환경에서 재현 빌드; 다른 환경 미검증은 명시
- [ ] E1001 대상만 패키징, E1002/1003에 교차 플래시되지 않도록 manifest 검사
- [ ] UART 로그인·키 비노출 확인
- [ ] Host regression + E1001 smoke test 상태와 결과 분리
- [ ] 이전 버전 설정 마이그레이션 테스트
- [ ] USB 응급 복구 문서가 실제 동작 검증됐는지 표시
- [ ] 장시간 soak의 목표/실측/장애 기록 포함
- [ ] API 데이터 출처와 금융 이용 제한 명시
