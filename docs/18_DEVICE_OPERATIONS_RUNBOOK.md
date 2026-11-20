# E1001 장기 운영·충전 보호 운용 절차

기준: 2026-11-13, [감사 계획](17_LONG_TERM_OPERATIONS_AUDIT.md), [ADR 0028](adr/0028-charge-protection-and-runtime-reliability.md). 실기 결과는 [증거 보고](../evidence/long-term-device-2026-11-13/summary.md)가 우선한다.

## 운영 이미지와 설정

- `reterminal_e1001`: 일반 배포, 내장 개발 키 없음. 충전 보호 기본 OFF. 포털 기기 설정에서 켜기/끄기.
- `reterminal_e1001_protected_dev`: 이 사용자의 로컬 개발 설정용. NVS가 비어 개발 설정을 불러올 때만 충전 보호 ON. 저장된 NVS 설정이 있으면 그 설정이 우선한다. 일반 배포 패키지로 내보내지 않는다.
- `reterminal_e1001_validation_dev`: 실기 fault 전용. Wi-Fi 강제 끊기 및 watchdog 정지 시험이 있으므로 시험 뒤 반드시 운영 이미지로 교체한다. RTC에 시험 진행을 기록해 watchdog 재부팅 직후 정지를 되풀이하지 않는다. 전원 차단 후 반복 여부는 보장하지 않는다.
- 기기 탭의 `80% 목표 · 충전 중단`은 레지스터로 확인한 허용 상태다. 전류계로 검증한 수치 또는 정확한 SOC를 뜻하지 않는다. 충전 중단은 USB 입력 차단이나 배터리 강제 방전이 아니다.
- 기본 배터리 및 SY6974B `REG0B=0x40`에서만 이번 제어를 검증한다. 배터리/PCB 교체 시 다시 확인한다. 80% 이상인 배터리는 USB로 계속 사용해도 빨리 80%로 내려가지 않을 수 있다.
- 수면·기기 꺼짐·PMIC 전원 상실 후 다음 펌웨어 실행 전 구간의 상한은 보장하지 않는다. 정상 보호 중에는 PMIC I2C watchdog만 해제하고 안전 타이머·NTC·전류·충전 전압은 유지한다.

## 검증 명령

```powershell
python -m pytest host/tests -q
python tools/run_native_tests.py
python tools/mutation_check.py
pio run -e reterminal_e1001
pio run -e reterminal_e1001_protected_dev
# 사용자가 실기 적용을 요청했을 때 (이번 세션은 승인됨)
pio run -e reterminal_e1001_protected_dev -t upload --upload-port COM5
python tools/uart_logger.py COM5 evidence/hw/session.log --duration-sec 86400
python tools/soak_report.py evidence/hw/session.log --events evidence/hw/session.log.events.jsonl --min-duration-sec 86400 --max-gap-sec 120
```

자동 결과 파일이 필요하면 수집에 `--duration-sec 86700 --soak-min-duration-sec 86400`을 지정한다. 종료 후 `<log>.report.json`을 만들며, 로그가 짧거나 불완전하면 FAIL로 남긴다. 이 자동 판정도 메모리 추세·기능·전류 검토를 대체하지 않는다. 실행 중 파일은 `evidence/**/active/`에 두어 미완성 로그를 커밋하지 않는다.

2026-11-13 후속([ADR 0029](adr/0029-storage-failure-and-observation-integrity.md)): 저장·충전·패널·watchdog 등록 오류와 수집 sidecar의 끊김/폐기/중단도 strict 판정에 포함한다. `--events`를 생략한 결과는 수집 무결성을 확인했다고 해석하지 않는다. 현재 진행 중인 구버전 logger에는 새 기능을 소급 적용하지 않는다. 캡처 종료 후 위 명령으로 sidecar를 포함해 다시 분석한다. 관측 중인 기기에 새 펌웨어를 flash하면 연속 시험이 끝나므로 후속 이미지는 시험 종료 후 별도 세션으로 검증한다.

`uart_logger.py`는 open 전에 DTR/RTS를 false로 설정한다. `--reset-once`를 명시하면 최초 연결에서만 RTS 리셋을 실행한다. 정상 soak에는 불필요한 리셋을 추가하지 않는다. OS/어댑터가 open 때 일으키는 핀 glitch까지 소프트웨어로 보장하지 못하므로 boot 로그도 확인한다.

파일 경로는 새 세션 이름을 쓴다(기존 파일 덮어쓰기 거부). 5MiB마다 `.0001`, `.0002`로 회전하며 삭제하지 않는다. 분석 시 번호 순서대로 합친 사본을 사용한다. UTC start/connect/disconnect/rotate/stop 기록은 `.events.jsonl`에 있다. 재연결은 로그를 복구할 뿐 잃은 구간을 합격 처리하지 않는다. capture exit 0도 장기 시험 PASS가 아니다. 중단은 `--stop-file <path>` 옵션으로 지정한 파일을 만들거나 해당 캡처 프로세스만 종료한다.

## 정지·복구 감사 (LT-06)

| 경로 | 코드상 제한·기존 보호 | 남은 실제 검증 |
|---|---|---|
| PMIC Wire1 | 50ms I2C timeout, USB 읽기 1초 캐시, 읽기 실패 후 버스 재초기화 | SDA/SCL 물리 고정·탈착 fault 전류 |
| SHT40 Wire | 기존 50ms Wire timeout | 센서 분리·실내 온습도 정확도 |
| Wi-Fi 접속 | 20초 시도, 5초~5분 재시도 backoff, event 공유 atomic | 공유기 재부팅 100회·암호 변경·DHCP 변경 |
| HTTPS | 15초 이하로 connect/header/body 시간 설정, 64KiB 본문 제한, 60초 cycle network 예산 | DNS/TLS/헤더의 SDK 내부 시간은 전체 절대 상한으로 입증되지 않음 |
| 패널 | 30초 공통 deadline, 실패 시 render CRC 미확정·다음 cycle 재시도 | BUSY 물리 고정·100회 반복·SD 삽입 |
| 포털 | 헤더 대기 5초, 본문/필드 제한, 접속 시험 20초, 배터리 창/절대 상한은 기존 정책 | 브라우저 느린 본문·10/30분 배터리 종료 |
| task watchdog | loop task 구독, 90초, 진행 시에만 feed | 검증용 실제 loop stall 결과를 보고서에 기록. task별·각 주변장치 fault 전체를 대체하지 않음 |
| RTC/NVS | CRC·슬롯 저장·읽기 검증·쓰기 빈도 제한, 기존 Host 전원 차단 회귀 | 실전 전원 차단 10회·Flash 마모 전류/수명 |

인터넷 차단/시계 불신 상황에서도 TLS 우회·HTTP 강등은 하지 않는다. 무조건 주기 재부팅으로 누수를 숨기지 않는다. SHT40 값을 배터리 셀 온도로 사용하지 않는다.

## 장기 시험 완료 조건

USB 24h → 72h → 7일을 실제 경과 시간으로 진행한다. 로그 검사의 duration은 첫/마지막 health의 차이이며 수집 프로세스 실행 시간과 다를 수 있다. 24시간 캡처는 첫 health 지연 때문에 86,400초 관측보다 짧을 수 있으므로 **24h 시험은 여유를 두고 86,640초 이상 캡처**한다. 실제 sample span이 모자라면 재수집/연장하고 기준을 낮추지 않는다.

연속 검사 PASS 외에 안정화 2시간 이후 내부 heap/largest·PSRAM 추세, 예기치 않은 재부팅 0, 화면·API 복귀를 검토한다. 배터리 sleep/wake는 uptime이 재시작되므로 이 USB 전용 검사로 판정하지 않는다. 현재 계측기 미보유이므로 실제 충전 전류·전압 오차·배터리 수명 항목은 BLOCKED_HW다.
