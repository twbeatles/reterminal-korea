# E1001 장기 운영 안정성 감사·개선 계획

- 작성: 2026-11-13 KST. 기준: main `432fdf6`, 공식 예제 `8d97d891`.
- 범위: E1001 자체 펌웨어, USB 상시 전원 및 배터리 운용. 운영 서버 추가 없음.
- 현재 단계: LT-01 사전 감사 및 LT-02 관측 기반 개선. 장기 실기 합격은 아직 아님.
- 추가 감사 LT-11~13: 저장 실패의 5분 재시도 제한·실패 포함 쓰기 예산, PMIC 확정 읽기 진단, 장기 로그/수집 손실 판정 강화. [ADR 0029](adr/0029-storage-failure-and-observation-integrity.md), [후속 검증 보고](../evidence/long-term-followup-2026-11-13/summary.md). 현재 24h 시험 보존을 위해 추가 이미지는 별도 실기 적용 대기다.
- 후속(2026-11-13): 사용자 USB 실기 검증·플래시 승인 및 기본 배터리 확인. `REG0B=0x40` 확인 후 [ADR 0028](adr/0028-charge-protection-and-runtime-reliability.md)의 제한된 호환 판정·충전 제어·실기 장애 주입을 진행한다. 실제 PCB 리비전과 전류는 계측 없이 확정하지 않는다. 최신 결과는 [실기 보고](../evidence/long-term-device-2026-11-13/summary.md)가 우선한다.
- 기존 근거: [11/12 보고](../evidence/audit-2026-11-12/summary.md). 부팅·API·화면·400초 순환은 확인됐으나 sleep/wake·전류·장기 운영은 미검증이다.

## 1. 80% 충전 제한: 가능성 및 선행 조건

**충전 제어 경로는 있으나, 현재 펌웨어에서 80% 제한을 보장하지 않는다.**

공식 E1001 v1.2 회로도 p2 변경 이력은 ETA6003 → SY6974B 교체를 명시한다. p4 전원 회로의 U2는 SY6974B이고 I2C는 GPIO39/40이다. 따라서 E1001이라는 제품명만으로 모든 리비전에 같은 레지스터를 써서는 안 된다. 사용자 실물 PCB 리비전·칩 ID·배터리 정격 확인이 LT-03의 필수 조건이다. I2C 주소 0x6B 응답만으로 부품을 확정하지 않는다.

Silergy SY6974B Rev.0.9 p13은 REG01[4] CHG_CONFIG(충전 허용), p15는 REG04 충전 전압과 REG05 watchdog을 설명한다. p23은 watchdog 만료 시 default mode 복귀를 설명한다. 단순히 한 번 충전을 끄는 구현은 장기 제한이 유지된다는 증거가 아니다. 공식 SenseCraft 드라이버에는 watchdog 비활성화·전류 설정은 있지만 충전 enable 제어 API는 없다. 현재 프로젝트 `Pmic.*`는 USB 존재만 읽는다.

`BatteryPolicy::PercentMv`는 전압 추정 표시용 곡선이다. 충전 중 단자 전압, 부하, 온도, 노화 영향을 보정하지 않으므로 표시 80%를 정확한 SOC 80%로 해석하지 않는다. 전압 제한 역시 특정 SOC와 동일하다고 보장할 수 없다.

### LT-03 설계안 (실기 확인 후 구현)

- 모드: 기존 충전 / 배터리 보호 목표 80%. 설정 스키마·포털·기기 화면·마이그레이션을 함께 다룬다. 검증 전 보호 모드를 기본 활성화하지 않는다.
- 순수 C++ 정책과 PMIC I2C 어댑터 분리. 상태: Unsupported / Unknown / ChargingAllowed / Limited / Fault. 요청 상태와 레지스터 read-back으로 확인한 상태를 구분한다.
- 시작 후보: 추정치 ≥80%가 연속 확인되면 중단, ≤75%에서 재개. 샘플 간격·연속 횟수·최소 유지시간은 실측으로 확정한다. 센서 실패값으로 충전 상태를 토글하지 않는다.
- 쓰기 전 부품 ID·레지스터 원문·현재값 확인, 필요한 비트만 변경하고 read-back 검증. 전류/안전 타이머/NTC/OTG/BATFET는 임의 변경하지 않는다. REG01의 WD_RST 같은 동작 비트를 그대로 되쓰지 않도록 검토한다.
- watchdog 유지/해제 중 어느 방식을 택할지 ADR로 결정한다. 펌웨어 정지·deep sleep·리셋·USB 재연결 때 제한이 풀리는 경우를 문서화하고, 기기가 꺼져 있을 때의 제한 보장 범위도 명시한다.
- 장애 시 `제한 확인 불가` 표시. I2C 실패 중 제어 성공을 표시하지 않는다. 기본 충전 복원 경로와 사용자가 보호 모드를 끄는 경로를 검증한다.
- 검증: 실제 충전 전류 중단/재개, USB로 시스템 계속 구동, 임계 부근 출렁임, 전압 측정 오차, watchdog 만료 전후, USB 20회 탈착, 재부팅 20회, 배터리 단독 복귀. 충전 상태 비트만으로 전류 중단을 판정하지 않는다.

## 2. 코드·도구 감사 결과와 우선순위

| 티켓 | 우선 | 관찰·위험 | 조치 및 완료 기준 |
|---|---|---|---|
| LT-01 | P0 | 단기 검증과 장기 합격의 경계 필요 | 이 문서와 출처·증거 연결 |
| LT-02 | P0 | boot heap만으로 USB 연속 운영 누수 관찰 불가. 기존 soak 도구는 fetch 1건으로 PASS 가능 | 60초 주기 내부 heap/min/largest·PSRAM·64bit uptime·USB/Wi-Fi 로그. USB 연속 시험 전용 기간/로그 공백/재부팅 검사 추가 |
| LT-03 | P0 | 80% 제한 없음, E1001 보드 리비전별 충전 칩 차이 | 위 선행 조건 후 별도 ADR·Mock fault·실측 검증 |
| LT-04 | P1 | PMIC 읽기 실패와 배터리 전원을 false 하나로 처리. loop마다 Wire1 초기화·조회 | Unknown 분리, I2C timeout·버스 복구·조회 캐시·전원 전환 정책 감사. 탈착 및 I2C fault 회복 시험 |
| LT-05 | P1 | Wi-Fi 이벤트 task와 loop가 volatile reason/pending을 공유 | task 간 동기화 및 이벤트 유실 검토, NET-100 및 장시간 DNS/TLS/HTTP 지연 주입 |
| LT-06 | P1 | watchdog 90초 및 60초 네트워크 예산이 전체 동작 상한을 보장하지 않음 | 포털·display BUSY·I2C·TLS 호출별 상한 표 작성, 정지 fault에서 watchdog 리셋·복구 확인 |
| LT-07 | P1 | NVS 제한·RTC CRC·held-key wake 제외는 이미 있음 | 재구현 대신 전원 차단 10회, sleep/wake 100회, 저전압 회복, 저장 실패 재시도·쓰기 횟수 검증 |
| LT-08 | P1 | 장기 시간·인증서·토큰·달력 만료/메모리 단편화 검증 부족 | 49.7일 millis wrap, KST 자정·연말·윤년·NTP 점프·장기 오프라인·인증/429/5xx 회귀 |
| LT-09 | P2 | ePaper·SD 공유 SPI 단기 수정만 검증 | SD 삽입/제거, 100회 갱신, BUSY 정지 복구, 장시간 잔상과 전류 측정 |
| LT-10 | P1 | `uart_logger.py`는 안내 후 종료하는 stub | 실제 캡처·마스킹·회전·포트 재연결 도구 구현 또는 검증된 외부 수집 방법 확정. DTR/RTS가 리셋을 유발하지 않는지 확인 |

## 3. 검증 순서와 합격 조건

1. LT-02: Host parser fault 회귀와 기존 C++/Python 회귀·기본 타깃 빌드. 로그 fixture는 합성임을 명시한다.
2. LT-03: 부품·리비전 확인 → 충전 정책 Mock → 어댑터 Build → 사용자가 요청한 실기 플래시 및 전류 계측. 이 문서 작성 요청은 플래시 요청으로 해석하지 않는다.
3. USB 24h 예비 → 72h → 7일 확대. 동일 빌드·전원·설정, 단일 부팅 구간을 수집. `--min-duration-sec 86400 --max-gap-sec 120`으로 관측 범위를 검사한다. 이 검사는 메모리/기능/하드웨어 전체 합격을 자동 판정하지 않는다.
4. 안정화 2시간 이후 동일 작업 직후의 내부 free/largest 및 PSRAM을 시간대별 비교한다. 임시 조사 기준: 지속 하락 또는 시작 대비 10% 이상 감소 시 원인 조사. 실측 전 절대 heap 안전선이나 수명 보장으로 쓰지 않는다. 최소 free는 누적 최저값이므로 회복 기대 금지.
5. 의도치 않은 재부팅 0, 무응답 0, 재연결 후 정상 데이터 회복, last-known-good/오래됨 표기 확인. Wi-Fi 복구 목표는 공유기 복구 뒤 5분 이내로 두고 미달 시 로그와 함께 조정 근거를 기록한다.
6. 배터리 72h 또는 방전까지: 100회 sleep/wake(버튼 50/타이머 50), held button, 배터리 포털 만료, 저전압·USB 복귀. 이 시험은 uptime이 초기화되므로 USB 연속 시험 옵션을 사용하지 않는다. 별도 외부 시각 및 전류 기록이 필요하다.
7. KST 자정·공휴일/영업일·JPY(100)·측정소 `-` 기존 회귀 유지. 온도 센서 SHT40 측정값을 배터리 셀 온도로 대체하지 않는다.

운영 로그 수집 PC는 시험 도구이며 기기 운영 의존성이 아니다. 진단은 UART에만 출력하고 flash에 주기 저장하지 않는다. 실키·토큰·SSID·비밀번호를 진단 필드에 넣지 않는다.

## 4. 출처 (2026-11-13 원문 대조)

- [Seeed E1001 시작 안내 및 회로도 링크](https://wiki.seeedstudio.com/getting_started_with_reterminal_e1001/).
- [E1001 v1.2 회로도](https://files.seeedstudio.com/wiki/reterminal_e10xx/res/202004307_reTerminal_E1001_V1_2_SCH_251120.pdf), p2/p4. 사용자 실물 리비전 일치는 미확인.
- [Silergy SY6974B Rev.0.9](https://www.silergy.com/download/downloadFile?ftype=note&id=4013&type=product), p13~15/p23. B 이외 모델에는 적용하지 않는다.
- 고정 공식 `vendor-upstream/examples/official/LVGLePaperStatusPanel/platformio.ini`: board/qio_opi/PSRAM/800×480 대조, 현 설정 유지. Flash·파티션 변경 없음.
- `hardware/reference/examples__official__SenseCraft_HMI__lib__SY6974__SY6974.{cpp,h}` 및 `src/drivers/Pmic.*`, `src/power/{BatteryPolicy,RtcPersistence,SleepController}.*`, `src/drivers/Watchdog.cpp`, `src/net/WifiConnector.cpp`.

결과·실행 명령·장애는 [이번 단계 보고](../evidence/long-term-2026-11-13/summary.md)에 기록한다.
