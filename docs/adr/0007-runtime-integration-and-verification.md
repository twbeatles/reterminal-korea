# ADR 0007 — 감사 후 런타임 연결과 검증 경계

- 상태: 채택 (2026-11-02 KST).
- 배경: PROJECT_AUDIT의 부팅 스택/시각 복구/HTTP framing 결함과 미연결 MVP 경로를 수정한다.
- 선택: Host에서 실행하는 정책·프로토콜과 ESP32 I/O를 분리하고, 실제 어댑터가 같은 정책/프로토콜을 호출하게 한다. NVS scratch/설정 임시는 checked heap을 사용하며, 타깃 ELF의 알려진 호출 경로를 별도 검사한다. 이 검사는 모든 SDK 동적 호출을 증명하지 않으므로 실기 stack high-water-mark 측정도 필요하다.
- 화면: 공식 MIT E1001 Gray4 예제 @8d97d891의 초기화/LUT/두 bit-plane 순서를 사용한다. Canvas의 black=1을 두 plane에 동일하게 전송하면 예제의 반전된 black=11, white=00과 일치한다. SPI 핀·BUSY 극성·명령순서는 원문 기준이다. Uc8179의 fake-port 테스트가 시퀀스/해상도/전송량/타임아웃을 검사한다. 실제 패널 동작은 BLOCKED_HW다. partial은 계속 false이며 실측 전 활성화하지 않는다.
- 폰트: 기존 OFL 원본에서 K-Dash Raster라는 이름으로 14/18px KS X 1001 한글 2,350자 및 UI 문자, 64px 숫자를 생성한다. 전체 Unicode 한글 블록을 2,350자로 잘못 설명하던 생성기를 수정한다. 원본 TTF는 build/에만 두며 생성 배열과 OFL 고지를 배포한다. 실제 비트 raster·glyph 경계·폭 제한을 Host에서 검증한다.
- 센서: 외부 라이브러리 추가 없이 SHT4x 공식 데이터시트 v7.3(2026-06)의 0xFD/10ms/6바이트/CRC/변환식을 따른다. https://sensirion.com/resource/datasheet/sht4x (확인 2026-11-02). 기기 핀/주소는 저장된 공식 E1001 예제를 따른다. 실제 센서 정확도/전류는 BLOCKED_HW다.
- 사용자 기능: 설정/영속화/버튼/화면/저전압/절전 배선을 완료해도 Host/Build와 Hardware/LiveAPI 검증 상태를 분리한다. 설정용 임시 AP 비밀번호만 현장 화면에 표시하며 저장된 Wi-Fi 비밀번호/API 비밀은 표시하지 않는다.
- 대안: GxEPD2 도입은 ADR 0004의 라이선스 결정 때문에 선택하지 않았다. 타임아웃/시각 문제를 TLS 우회로 해결하지 않는다.
- 되돌리기: 모듈 인터페이스는 유지하고 보드 기능을 비활성화할 수 있다. 파티션/eFuse/기기 플래시는 변경하지 않는다.

## 통합 검토 후 보완

- 저전압 게이트를 첫 설정·버튼 분기보다 먼저 적용한다. USB 전원일 때만 배터리 전압 보호 분기를 우회한다.
- 로컬 SHT40의 due와 네트워크 provider의 due를 분리해 센서 측정만으로 RF를 켜지 않는다. 연결 실패는 due provider의 백오프 상태에 반영하고, 새 로컬 값은 다음 렌더 판단에 포함한다.
- 달력은 연도 전환 시 미리 받아 둔 내년 테이블을 보존한다. 공휴일 loaded와 연간 KRX 예외 검증 market_verified를 구별하고, 후자가 없으면 평일 시장 상태는 휴장 가능으로 남긴다. 캐시 v2는 v1을 읽을 수 있다.
- 특일 API 계약은 공식 첨부 기술문서에서 확인했다: https://www.data.go.kr/cmm/cmm/fileDownload.do?atchFileId=FILE_000000002633688&fileDetailSn=1 ; HTTPS base는 공식 상세 페이지 https://www.data.go.kr/tcs/dss/selectApiDataDetailView.do?publicDataPk=15012690 에서 확인. 실제 인증 응답은 별도 BLOCKED_CREDENTIALS다.
- 바이너리 프레임/시계 부트스트랩/정책을 fake 포트로 검증하고, 72시간 시뮬레이션 중 재부팅을 포함한다. 실제 main/SDK/기기는 여전히 빌드까지만 검증한다.
