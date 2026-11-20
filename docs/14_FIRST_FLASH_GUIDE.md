# E1001 첫 플래싱 준비 — API 키·주의사항·실행 순서

기준일: **2026-11-11 KST**. 대상은 **Seeed reTerminal E1001**만이다.
이 문서는 현재 main의 코드와 공식 자료를 대조한 준비 안내다. 실제 업로드·복구·패널·전류·실 API 검증 완료를 뜻하지 않는다.

## 1. 지금 준비할 것

**API 키 발급을 기다리지 않고 키 없는 펌웨어부터 올려 부팅·화면·설정 포털을 확인할 수 있다.** 이후 Wi-Fi와 키를 포털에서 등록하는 순서를 권장한다. 미등록 공급자의 데이터가 비어 있거나 설정 필요로 표시되는 것은 정상이며, 자동으로 실제 날씨나 DEMO 값이 채워진다고 기대하지 않는다.

- [ ] 제품 라벨이 E1001인지 확인한다(800×480 흑백). E1002/1003용 바이너리를 사용하지 않는다.
- [ ] 데이터 전송 가능한 USB-C 케이블, 안정적인 USB 전원, Windows PC를 준비한다.
- [ ] 2.4GHz Wi-Fi SSID/비밀번호와 포털 접속용 스마트폰 또는 PC를 준비한다.
- [ ] 표시할 지역명, 공식 기상청 행정동 격자 `nx/ny`, 에어코리아 측정소명을 정한다. 코드 기본값 60/127·종로구를 본인 위치로 간주하지 않는다.
- [ ] 기존 출고 화면·펌웨어 버전·설정 중 필요한 정보를 개인적으로 기록한다. 설정/사진 보존이 보장되는 작업은 아니다.
- [ ] 아래 API의 활용 승인 여부·일일 호출 한도·만료일을 확인한다. 키 값은 비밀번호 관리자에 보관하고 문서/채팅/커밋에 쓰지 않는다.

microSD·SenseCraft/TRMNL 계정·상시 실행 PC·백엔드는 K-Dash 운영에 필요하지 않다. PC는 빌드/업로드/선택적 API 점검에 사용한다.

## 2. API 신청 및 키 체크리스트

| 기능 | 신청할 서비스와 공식 링크 | 준비할 값 | 기기 입력 / 개발용 매크로 | 우선순위 |
|---|---|---|---|---|
| 날씨(초단기실황·단기예보) | [기상청_단기예보 조회서비스](https://www.data.go.kr/data/15084084/openapi.do) | 공공데이터포털 일반 인증키 | 기상청 키 / `KDASH_DEV_KMA_KEY` | 생활 화면 사용 시 |
| 미세먼지 PM10/PM2.5 | [한국환경공단_에어코리아_대기오염정보](https://www.data.go.kr/data/15073861/openapi.do) | 해당 서비스 활용 승인된 포털 키 | 에어코리아 키 / `KDASH_DEV_AIR_KEY` | 생활 화면 사용 시 |
| 공휴일 달력 | [한국천문연구원_특일 정보](https://www.data.go.kr/data/15012690/openapi.do) | 특일 서비스 활용 승인 | **기상청 키를 재사용**, 별도 입력 칸 없음 | 일정/휴일 판단용 |
| 고시 환율 USD·JPY(100) | [한국수출입은행 환율 정보](https://www.data.go.kr/data/3068846/openapi.do) → [은행 Open API 신청 안내](https://www.koreaexim.go.kr/ir/HPHKIR020M01?apino=2&viewtype=C&searchselect=&searchword=) | 은행에서 발급한 `authkey` | 수출입은행 키 / `KDASH_DEV_EXIM_KEY` | 환율 표시 시 |
| 주식 종가 | [금융위원회_주식시세정보](https://www.data.go.kr/data/15094808/openapi.do) | 해당 서비스 활용 승인 | **기상청 키 재사용** + FSC 활성화 / `KDASH_DEV_FSC_ENABLED` | 선택, 기본 OFF |
| 국내주식 REST 현재가 | [한국투자 Open API](https://apiportal.koreainvestment.com/), [공식 샘플](https://github.com/koreainvestment/open-trading-api) | 본인 실전 서비스용 App Key + App Secret | KIS 앱키·시크릿 + KIS 활성화 / `KDASH_DEV_KIS_APP_KEY`, `KDASH_DEV_KIS_APP_SECRET`, `KDASH_DEV_KIS_ENABLED` | 선택, 첫 smoke 뒤 |

### 신청할 때 헷갈리기 쉬운 점

1. **포털 키가 같아도 각 서비스 활용 신청은 별개**다. 기상청·에어코리아·특일 정보, 선택 시 FSC까지 각각 승인 상태를 확인한다. 승인 반영 시간과 할당량은 본인 포털 화면을 기준으로 한다.
2. 현재 펌웨어는 기상청 키로 FSC·공휴일까지 호출한다(`Fetcher.cpp`, `CalendarRefresh.cpp`). 세 서비스에 모두 사용할 수 있는 키가 필요하다. 계정/키가 다르면 이 입력 구조로 지원된다고 가정하지 않는다.
3. data.go.kr 키는 Encoding/Decoding 형식을 모두 수용한다. 복사한 값을 추가로 URL 인코딩하거나 `%`를 다시 변환하지 않는다. 처음에는 Decoding 키 원문 사용을 권장한다.
4. EXIM은 공공데이터포털 키와 다른 은행 인증키다. 현재 구현은 신규 호스트 `oapi.koreaexim.go.kr`를 사용한다. 고시 환율이며 초단위 FX가 아니다. JPY는 **100엔당 원화** 단위를 확인한다.
5. FSC 데이터는 기준일 다음 영업일 오후 1시 이후 갱신 안내가 있다. 전일/기준일 종가로 확인하며 장중 현재가로 해석하지 않는다.
6. KIS 접근 토큰은 기기가 발급받는다. 토큰·계좌번호·계좌비밀번호·주문용 정보는 입력하지 않는다. 모의투자 키와 실전 키를 섞지 않는다. KRX/NXT/통합 시장 계약·시간표의 실연동 검증이 남아 있으므로 처음에는 금융 OFF로 진행한다.
7. 관심종목은 최대 4개, 6자리 코드(예: `005930`)다. 금융은 본인 기기의 읽기 전용 조회만 사용한다. FSC/KIS 이용 조건과 재배포 제한을 신청 시 확인한다.

## 3. 비밀을 넣는 방법

### 권장: 비밀 없는 빌드 → 기기 설정 포털

기본 `reterminal_e1001` 환경은 개발 헤더를 읽지 않는다. 공개 패키지는 기본 환경으로 빌드한다.

키는 업로드 후 §5의 기기 포털에서 입력한다. 저장된 키는 다시 평문으로 반환하지 않으며, 빈 비밀 입력 칸은 기존 값 유지, 삭제 체크는 해당 값 삭제다. 포털은 임시 AP 비밀번호로 접근을 제한하며 최대 10분간 열린다. 로컬 HTTP이므로 본인이 연 AP에서만 설정하고 AP 비밀번호가 찍힌 사진은 공유하지 않는다. 외부 API는 HTTPS 인증서 검증을 유지한다.

### 대안: 개발 전용 헤더

> 2026-11-12부터 기본 환경 `reterminal_e1001`은 이 헤더를 컴파일하지 않는다. 헤더를 쓰려면 **개발 환경 `reterminal_e1001_dev`** 로 빌드·업로드한다(ADR 0024).

화면/포털 경로가 실패할 때 검토할 수 있는 개발용 방법이다. 공개 배포용으로 사용하지 않는다.

[개인 설정 분리 안내](20_PUBLIC_REPOSITORY_GUIDE.md)의 명령으로 샘플을 저장소 밖에 복사한다. `include/dev_credentials.h`를 새로 만들지 않는다.

매크로 목록은 표와 [샘플 헤더](../include/dev_credentials.example.h)를 따른다. Wi-Fi는 `KDASH_DEV_WIFI_SSID`, `KDASH_DEV_WIFI_SECRET`, 지역은 `KDASH_DEV_GRID_X/Y`, `KDASH_DEV_DISPLAY_NAME`, `KDASH_DEV_AIR_STATION`이다. 개발 헤더 경로는 ADR 0030을 따른다.

**이 방법은 키를 바이너리에 포함한다.** `.gitignore`만으로 `.bin/.elf`의 비밀을 보호할 수 없다. 파일/빌드 산출물을 공유하지 않는다. 헤더 삭제만으로 기존 바이너리의 키가 사라지지도 않는다. 부팅 설정 우선순위는 **유효 NVS → 개발 헤더 → 기본값**이므로 헤더를 바꿔 재업로드해도 기존 NVS 설정이 우선할 수 있다. 포털에서 수정하며, 이를 이유로 전체 Flash를 지우지 않는다.

## 4. 빌드와 업로드

### A. 기기에 쓰기 전

PowerShell에서 저장소 루트로 이동한다. 아래 단계는 빌드까지만 수행한다.

```powershell
Set-Location <저장소 클론 경로>
git status --short --branch
pio --version
pio run -e reterminal_e1001
pio device list
```

- PlatformIO가 없다면 공식 [설치 안내](https://docs.platformio.org/en/latest/core/installation/index.html)를 따른다. 현재 확인된 로컬 Core는 6.2.0이다.
- USB를 뺐다 연결하며 장치관리자에서 추가되는 COM 포트를 확인한다. 다른 장치의 포트를 선택하지 않는다. 포트가 없으면 케이블/USB 포트/전원부터 확인하고, 드라이버는 [Seeed 공식 안내](https://wiki.seeedstudio.com/getting_started_with_reterminal_e1001/)가 연결하는 WCH 배포처를 사용한다.
- 보드 이름이 XIAO로 보이는 것은 현재 공식 LVGL 예제와 같은 설정이기 때문이다. env는 반드시 `reterminal_e1001`이다.
- 현재 설정: `espressif32@8.0.0`, Arduino, `seeed_xiao_esp32s3`, `qio_opi`, `BOARD_HAS_PSRAM`, upload/monitor 115200. Seeed 공식 고정 commit `8d97d8912987c07529287629f7e73039bb0e6384`의 LVGL 예제와 메모리/해상도 설정을 대조했다. 실제 패널은 ADR 0004의 직접 UC8179 드라이버를 사용한다.
- 제품 스펙은 32MB Flash/8MB PSRAM이지만 **현재 기본 파티션을 임의로 32MB·OTA용으로 변경하지 않는다.** 실물 Flash ID·도구 upload recipe 대조가 먼저다. 부팅의 `psram` 로그는 남은 PSRAM이며 총 용량 증명이 아니다. 현 부팅 로그에는 Flash 용량 출력이 없다.
- 빌드 실패 시 업로드하지 않는다. 다운로드 장애는 환경 문제로 구분하고 [BUILD](BUILD.md)의 기존 검증 경로를 따른다. 문서 준비만으로 전체 테스트를 반복할 필요는 없다.

### B. 실제 기기에 쓰는 단계 — 준비가 끝난 후 사용자가 실행

아래 명령은 **현재 펌웨어/관련 Flash 영역을 실제로 덮어쓴다.** 출고 앱에서 K-Dash로 전환되며 기존 설정 호환·보존을 보장하지 않는다. 전원을 ON으로 하고 USB를 안정적으로 연결한다. 시리얼 모니터·웹 플래셔 등 포트를 점유한 앱을 닫는다.

```powershell
$e1001Port = 'COM7'  # 반드시 위에서 확인한 실제 포트로 변경
pio run -e reterminal_e1001 -t upload --upload-port $e1001Port
# 업로드가 SUCCESS로 끝난 뒤에만 모니터를 연다.
pio device monitor --port $e1001Port --baud 115200
```

기본 PlatformIO 업로드 경로를 사용한다. `firmware.bin` 하나를 임의 offset으로 쓰거나 다른 모델의 부트로더를 섞지 않는다. 업로드·패널 갱신 중 케이블을 뽑거나 연속 리셋하지 않는다. 업로드 완료 후 계속 다운로드 대기 상태라면 모니터를 닫고 전원을 다시 켜서 정상 부팅을 확인한다. 모니터 종료는 `Ctrl+C`다.

`erase`, Full Flash, 설정 초기화, eFuse/secure boot/flash encryption 활성화는 첫 업로드 절차에 포함하지 않는다. NVS는 하드웨어 추출 방지 기능이 아니다.

## 5. 첫 부팅과 포털 설정

1. USB 전원을 유지하고 화면이 갱신되는지 확인한다. 전자잉크의 남아 있는 출고 화면만으로 현재 부팅 성공을 판단하지 않는다.
2. 115200 로그에서 `kdash boot`, `board=e1001`, 펌웨어 identity, `config: source=...`, 재부팅 반복 여부를 확인한다. 로그는 캐리어 USB-UART GPIO44/43의 `Serial1` 경로이며 USB CDC `Serial`로 임의 변경하지 않는다.
3. **KEY0(GPIO3, 오른쪽 녹색 키, 짧게=새로고침)를 약 2초 이상 길게 눌러** 설정 포털을 연다. Wi-Fi가 이미 연결돼 있고 포털 모드가 꺼져 있지 않으면 **같은 Wi-Fi 포털**이 열린다: 기기 탭에 주소(`http://192.168.x.y`)와 6자리 페어링 코드가 표시되고, 같은 Wi-Fi의 휴대폰 브라우저로 접속해 코드를 입력한다(아래 4번의 AP 절차는 건너뛴다). 설정이 없거나 Wi-Fi가 없으면 임시 AP 포털이 열린다. 물리 키 위치는 [핀맵](../hardware/board_e1001.md)과 기기 표기를 대조한다.
4. (AP 포털일 때) 화면에 표시된 `KDASH-E1001-XXXX` AP와 임시 비밀번호로 접속한다. “인터넷 없음”이어도 해당 Wi-Fi 연결을 유지하고 브라우저에서 **`http://192.168.4.1/`**을 직접 연다. VPN/모바일 자동 Wi-Fi 전환이 방해하면 설정 동안 조정한다.
5. 집 Wi-Fi와 지역·격자·측정소를 먼저 입력한다. 준비된 공급자 키만 추가하고 FSC/KIS는 처음에는 OFF로 둔다. 저장 후 접속시험·검증 저장을 거쳐 재부팅한다. **Wi-Fi 접속시험 성공은 API 키 유효성 검증이 아니다.**
6. 스마트폰/PC를 원래 Wi-Fi로 되돌린다. 지역, KST 시각, 데이터 수신·원본 기준시각·오류 배지를 확인한다. 저장 실패 시 이전 설정 유지 여부를 확인하고, 10분 만료 시 KEY0으로 포털을 다시 연다.
7. 공급자는 날씨 → 대기질 → 특일 → 환율 → 선택적 금융 순으로 확인하면 장애를 구분하기 쉽다. HTTP 200 외에 내부 결과코드·레코드 수·누락값·발표 회차까지 검증한다. 특일 API만으로 KRX 전체 휴장일이 검증된 것은 아니다.

## 6. 막혔을 때

| 증상 | 먼저 확인할 것 / 중단 기준 |
|---|---|
| 포트가 없음 | 데이터 케이블, USB 포트, 전원 ON, 장치관리자, 공식 WCH 드라이버 |
| 포트 사용 중 / 연결 timeout | 다른 모니터·플래셔 종료, 실제 COM 재확인. 출고 앱의 sleep이면 공식 안내의 상단 녹색 wake 버튼으로 깨운 뒤 재시도 |
| 계속 다운로드 연결 실패 | 여기서 중단하고 해당 E1001 리비전의 공식 복구 방법을 확인한다. 현재 확인한 공식 시작 가이드에는 수동 BOOT/RESET 조합이 없어 추측해 안내하지 않는다. 사용자 키 GPIO3/4/5를 BOOT로 간주하지 않는다 |
| 업로드 성공, 화면 그대로/깨짐 | 시리얼 부팅·패널/BUSY 오류·재부팅 확인. 연속 갱신하지 말고 증거를 남긴다. 화면에 AP 비밀번호가 보이면 사진 공유 전 가린다 |
| AP가 안 열림 | KEY0 롱프레스·전원·저전압/안전모드 확인. 읽을 수 있는 AP 비밀번호를 화면에 그리지 못하면 코드가 AP 개방을 거부한다. 헤더 방식 검토 전 패널 문제를 먼저 진단한다 |
| 포털 저장 후 API 실패 | 서비스별 승인/키/할당량·격자·측정소·기준시각 확인. TLS 실패 시 시각/NTP·CA·서버 체인을 조사하며 인증서 검증을 끄지 않는다 |
| 재업로드해도 옛 설정 | 정상 NVS가 헤더보다 우선한다. 포털로 수정한다. `/export`는 비밀 복구 백업이 아니라 일부 등록 상태 정보다 |

공식 출고/데모 펌웨어로 돌아가는 선택지는 [Seeed E1001 시작 안내의 Firmware Flasher](https://wiki.seeedstudio.com/getting_started_with_reterminal_e1001/#firmware-update)를 따른다. 모델을 다시 확인하고 Full Flash의 전체 삭제를 피한다. 이 공식 앱 경로는 K-Dash 업로드 경로와 별개이며, 복구 시 클라우드/출고 앱으로 동작이 바뀔 수 있다. 상세 정책은 [RECOVERY](RECOVERY.md), [릴리스/복구](08_RELEASE_RECOVERY_CI.md)를 참고한다.

## 7. 첫 실기 기록 체크리스트

- [ ] 업로드 SUCCESS와 정상 부팅 로그, 펌웨어 버전·Git SHA·COM·실물 모델 기록
- [ ] 한글 화면 방향/잘림/결측 표시, 버튼 이전/다음/새로고침, KEY0 포털 진입
- [ ] 포털 Wi-Fi 저장 후 재부팅·유지, 잘못된 Wi-Fi 입력 시 기존 설정 유지
- [ ] NTP/RTC 시각 및 실제 HTTPS, 공급자별 내부 성공코드·발표시각·측정소 확인
- [ ] 금융 표시 구분: `intraday_live_or_delayed` / `previous_close` / `official_fx_fixing`
- [ ] USB 분리 후 sleep/wake·버튼 held 상태·전류는 별도 계측. 부분 갱신은 현재 비활성 유지
- [ ] [진행 보고 템플릿](../templates/PROGRESS_REPORT.md)에 실행 명령·성공/실패·정제한 증거 경로 기록

실키나 실기 접근이 없는 항목은 각각 `BLOCKED_CREDENTIALS`, `BLOCKED_HW`로 남긴다. 사용자에게 기기가 도착한 것과 이 세션에서 기기에 접근하여 시험한 것은 구분한다. 실기 smoke 통과 전 무인 장기·배터리 운영을 검증 완료로 표시하지 않는다.

## 8. 대조한 출처

2026-11-11에 위의 Seeed E1001 시작 안내, 공공데이터포털 5종 상세 페이지, KIS 공식 샘플 페이지를 열어 확인했다. 포털에 표시된 수정일은 KMA 2026-10-19 / AirKorea 2026-06-30 / EXIM 2026-04-30 / 특일 2023-03-29다. 로그인 후 키 발급 화면·개인별 승인/쿼터·실응답까지 확인한 것은 아니다.

로컬 원문: 공식 LVGL `platformio.ini`와 base 예제, 프로젝트 `platformio.ini`, `SettingsLoader.cpp`, `PortalHandler.cpp`, `PortalServer_esp32.cpp`, `Fetcher.cpp`, `CalendarRefresh.cpp`, `tools/api_probe.py`. 기존 소프트웨어 검증은 [2026-11-05 보고](../evidence/audit-followup/summary.md)이며 이번 문서 작업에서 재실행한 테스트 수치로 인용하지 않는다.

PC에서 키를 먼저 점검하려면 [BUILD § PC probe](BUILD.md#pc에서-읽기-전용-api-probe)를 따른다. `python tools/api_probe.py --dry-run`은 네트워크 호출 없이 계획만 보여준다. PC probe의 `KDASH_HOLIDAY_KEY`는 기기와 달리 별도 환경 변수이며, FSC는 `KDASH_KMA_KEY`를 사용한다. 실키를 명령 인수/셸 이력에 직접 쓰지 않는다.
