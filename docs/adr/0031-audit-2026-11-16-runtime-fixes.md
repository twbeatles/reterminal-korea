# ADR 0031 — 2026-11-16 런타임 감사 후속

- 상태: 채택. 실기 장기·전류 검증은 별도 게이트.
- 날짜: 2026-11-16 KST
- 질문: PROJECT_AUDIT ISSUE-015~018 및 G-1~6의 동작·검증 공백을 어떻게 줄이는가?

## 선택

- `TrustedNtpUtc`는 실행 중 시스템 시각이다. 재적용하지 않는다. `SyncNtp` 성공은 플랫폼이 이미 소수 초까지 적용했다는 계약이다. RTC 최초 시드만 `ApplyUtc`를 호출하고 ESP 어댑터에서도 부팅당 한 번으로 제한한다. RTC 쓰기는 명시적 새 NTP 성공 후 정책이 요구할 때만 한다. SDK의 백그라운드 NTP 알림만으로 RTC를 쓰지 않는다.
- 수면 직전 SY6974B ID/USB/OTG를 새로 읽고 USB가 없을 때 CHG_CONFIG만 충전 허용으로 복구한다. WD_RST 명령은 재전송하지 않으며 쓰기 read-back을 검사한다. 실패하면 다음 wake를 30초로 제한한다. 미지원 ID에는 쓰지 않는다. 수면 중 USB 연결 시 80% 제한을 보장할 수 없다는 기존 한계를 명시한다.
- 기기 설정 저장 재시작은 기존 인증 차단 해제 표식을 함께 사용해 시작 화면을 적용한다. 스냅샷·공급자 캐시는 보존한다. 실제 복원 조립을 순수 C++ `RestoreRetainedRuntime`으로 분리하여 테스트한다. 일반 Deep Sleep은 마지막 페이지를 유지한다.
- USB 자동 시계 복구는 비동기 Wi-Fi 감시자가 연결을 맡는다. 연결된 경우만 NTP를 시도한다. 수동 새로고침과 배터리 wake 연결은 유지한다. setup의 기존 단일 연결 시도는 유지하되 첫 사이클의 시계 전용 중복 연결은 생략한다.
- 보내지 못한 시간 예산 초과 요청은 deferred로 처리한다. 이미 성공한 종목은 유지한다.
- RTC 스키마 v2에 내용 해시·마지막 실제 갱신 UTC를 포함해 CRC로 보호한다. 유효한 과거 시각이 없거나 시계가 역행하면 보수적으로 갱신한다. 구 v1은 폐기하고 NVS로 복원한다. NVS 설정/캐시 형식과 파티션은 유지한다.
- 다음 해 공휴일 NoData는 하루 후 재시도한다. 빈 해를 정상 공휴일 표로 간주하지 않는다. 현재 해 오류/인증·쿼터 정책은 유지한다.
- AP의 Wi-Fi 연결 성공 후 저장 제약과 수면 전 충전 허용을 포털에 안내한다. README 수면 주기를 실제 15분/6시간/24시간과 맞춘다.
- Linux CI에 E1001 펌웨어 빌드를 추가한다. 호스트 테스트가 모든 SDK·물리 동작을 검증한다고 주장하지 않는다.

## 근거와 대안

Seeed 고정 공식 LVGLePaperStatusPanel `8d97d891`의 board/qio_opi/PSRAM/해상도와 대조했다. Flash/핀/라이브러리 변경은 없다. Arduino-ESP32 3.3.12 설치 헤더 및 [Espressif system time](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/system_time.html)의 SNTP 적용/알림 순서를 확인했다. 기존 정수 초 재적용은 시스템 시각을 손실시킨다.

USB 전용 블로킹 백오프 추가보다 이미 있는 Wi-Fi 감시자를 쓰는 편이 버튼 입력을 보호한다. 수면 중 충전 제한 유지 대안은 USB wake 회로 검증이 필요하다. 핀/회로를 추측해 추가하지 않는다.

## NVS 용량

실제 기본 `default_8MB.csv`와 생성 partitions.bin의 NVS는 0x9000/0x5000(20KiB)이다. [Espressif NVS](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_flash.html)의 4096B page, 126 entries/page, 32B entry를 기준으로 1페이지 GC 여유를 제외하면 504 entries다. 최대 문자열·4종목·전체 캐시 테스트: settings 1569B, snapshot 1133B, status 270B, runtime 1431B. 이중 슬롯, 40B envelope, chunk/index overhead, 가장 큰 블롭 교체, namespace/meta/Wi-Fi 40 entries를 포함한 보수적 계산은 326 entries다. 이는 파편화·미지의 다른 namespace 전체를 보장하지 않으므로 부팅 때 실제 nvs_get_stats를 기록한다. 용량 때문에 파티션을 바꾸거나 기존 NVS를 지우지 않는다.

## 검증 증거

[진행 보고](../../evidence/audit-2026-11-16/summary.md)에 실행 결과·이미지 해시·실기 범위를 기록한다. 핵심 테스트: 밀리초 시계 120회 무손실, 충전 수면 read-back/오류/USB/OTG, 실제 복원 함수의 페이지·캐시·CRC 원자성, USB 미검증 10회 비블로킹 및 수동/배터리 경로, 다종목 중간 예산 소진, 다음 해 NoData 하루 제한, NVS 최대 길이.

## 되돌리기

이 변경 커밋을 되돌려 다시 빌드·업로드한다. RTC 스키마 차이는 NVS fallback으로 처리한다. NVS/파티션/eFuse 전체 삭제는 필요 없다. 이전 펌웨어에는 감사에서 확인한 결함이 다시 나타난다.
