# ADR 0028 — 충전 보호 및 실제 기기 장기 운영 개선

> 2026-11-16 보완: [ADR 0031](0031-audit-2026-11-16-runtime-fixes.md)에 따라 배터리 수면 직전 USB 없음·지원 칩·OTG 해제를 확인하고 충전 허용을 복구한다. 잠든 동안 USB를 다시 꽂아도 충전을 시작할 수 있도록 하는 조치다. 복구 실패 시 wake를 30초로 제한하며, 수면 중 80% 제한과 실제 충전 전류는 검증 완료하지 않았다.

- 상태: 채택. Host/Build 및 단기 실기 fault 검증, 장기·전류는 미완료. 사용자 2026-11-13 USB 실기 검증·개선 플래시 승인.
- 질문: 충전 목표 80%와 장기 장애 관측을 E1001 자체에서 어떻게 운영하는가?
- 선택: 호환 충전 칩을 먼저 읽기 전용 식별한다. SY6974B REG0B의 PN=1000 및 지원 revision만 허용하고, 다른 ID에는 설정 쓰기를 하지 않는다. 정확한 PCB 리비전은 USB로 알 수 없으므로 ID+공식 E1001 회로도에 근거한 호환 판정으로 한정한다. 사용자는 기본 배터리 사용·계측 장비 없음이라고 확인했다.
- 정책: 표시용 전압 곡선 기반 **추정 80% 목표**. 30초 간격 3연속 ≥80%면 충전 중단, ≤75%면 재개한다. 경계 사이에서는 기존 충전 허용 상태를 유지한다. 비정상 전압·읽기 실패는 카운터를 초기화하고 성공을 표시하지 않는다. 충전 중 전압 오차 때문에 정확한 SOC 보장은 하지 않는다.
- 제어: REG01 CHG_CONFIG만 변경하고 WD_RST는 0으로 마스킹한다. NTC·안전 타이머·전류·전압 한계·OTG·BATFET 변경 금지. REG05의 I2C watchdog bits만 해제해 긴 네트워크/화면 작업 중 default 충전으로 복귀하지 않게 한다. read-back 성공 후에만 Limited/Allowed를 보고한다. 충전 보호 해제 시 CHG_CONFIG를 복구하되 watchdog 해제는 유지한다(공식 SenseCraft도 이 설정을 사용). 영구 NVS에 주기 쓰기 없음.
- 한계: PMIC 자체 전원 상실·초기화·다른 펌웨어는 설정을 되돌릴 수 있다. ESP가 꺼져 있거나 잠든 동안 USB를 연결하면 다음 실행 전까지 80% 보호를 보장하지 않는다. 기존에 80% 이상인 배터리를 80%로 강제 방전시키지 않는다. 실기 전류 차단과 SOC 정확도는 계측기 없이 완료 판정하지 않는다.
- 설정: NVS schema v4 boolean, v2/v3는 OFF로 마이그레이션하여 기존 설정 보존. 포털에 켜기/끄기와 위 한계를 설명한다. 이번 기기는 사용자 요청에 따라 개발 초기값에서 ON으로 적용하며, 저장된 설정이 있으면 그것이 우선한다. 일반 배포 기본값은 OFF.
- LT-04/05: PMIC I2C 일회 초기화, 50ms timeout, 1초 USB poll 캐시·Unknown 관측 및 재초기화. Wi-Fi task 공유 데이터는 atomic exchange로 바꾼다.
- LT-10: UART 캡처는 DTR/RTS=false를 open 전에 설정, 비밀 마스킹 후 쓰기, bounded line buffer, 재연결 이벤트·UTC sidecar·파일 회전. PC는 시험 도구이며 기기의 운영 의존성이 아니다.
- 실기 fault 경계: `validation_dev`에만 45초 Wi-Fi disconnect와 100초 뒤 watchdog feed를 중단하는 loop stall을 넣는다. 실제 Wi-Fi 재연결 11.075초, watchdog reset=6 뒤 화면/충전 중단 cfg=0a 복구를 관찰했다. 운영 `protected_dev`에는 이 코드가 컴파일되지 않는다. 실제 전원 차단·배터리 탈착 시험을 대신한 결과가 아니다.
- 실기에서 추가 발견: 초기 `RTC_DATA_ATTR` 시험 표식은 watchdog 재부팅 때 초기화되어 Wi-Fi 주입이 반복됐다. 기존 reset guard/RTC snapshot도 같은 속성이었다. `RTC_NOINIT_ATTR`로 바꾸고 power-on/magic 검증 때 길이·설정 지문·인증 해제 표식까지 명시적으로 지운다. snapshot은 기존 길이/CRC/설정 지문 검사 후에만 복원한다. `reset_guard`의 boots/crashes 로그로 재시험한다. [Espressif esp_attr.h](https://github.com/espressif/esp-idf/blob/master/components/esp_common/include/esp_attr.h)의 noinit 의미와 설치된 코어 헤더를 함께 대조한다.
- 대안: 충전 전압 하향만 적용(SOC와 같지 않으며 기존 전압 설정을 바꿈), watchdog 매번 feed(긴 블로킹/수면에 취약), 배터리 80% 표시만 변경(물리 충전 제어가 아님).
- 증거: `evidence/long-term-device-2026-11-13/summary.md`. 실기 수행 결과에 따라 이 기록을 갱신한다.
- 복원: 포털의 보호 기능 OFF → 저장/재시작 후 CHG_CONFIG read-back 확인. NVS 삭제 없이 이전 펌웨어로 복귀 가능하나 v4 설정을 못 읽는 이전 펌웨어에는 설정 재입력이 필요할 수 있다.
