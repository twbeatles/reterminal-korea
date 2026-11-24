# THIRD_PARTY_NOTICES — 라이선스 고지 (P6-02, 작성 중)

| 구성요소 | 출처 | 라이선스 | 비고 |
|---|---|---|---|
| Seeed reTerminal E-Series 예제 | https://github.com/Seeed-Projects/OSHW-reTerminal-Series-E-D | MIT, Copyright (c) 2026 Seeed-Projects | UC8179 명령/LUT를 공식 Gray4 예제 @8d97d891에서 적용. 고지 전문은 hardware/reference/LICENSE에 보존 |
| ArduinoJson | https://github.com/bblanchon/ArduinoJson | MIT | 7.4.3 고정, `src/providers/ProviderJson.cpp`에서 사용 (펌웨어 포함) |
| Unity (ThrowTheSwitch) | https://github.com/ThrowTheSwitch/Unity | MIT | 테스트 전용, 펌웨어 미포함 |
| 루트 CA 인증서 3종 | Mozilla CA 목록 (certifi 2026.10.23 경유) | MPL-2.0 (목록), 인증서 자체는 공개 신뢰 앵커 | `src/net/CaBundle.h`, `tools/gen_ca_bundle.py`로 재생성 |
| ziglang (pip) | https://pypi.org/project/ziglang/ | MIT | 로컬 네이티브 테스트용 개발 도구, 저장소·펌웨어 미포함 |
| K-Dash Raster 비트맵 폰트 | IBM Plex Sans KR에서 파생 | OFL-1.1, Copyright © 2017 IBM Corp. | 원본 TTF 미포함. 14/18px 한글 및 64px 숫자 생성. src/ui/fonts/OFL.txt 전문 보존, 원본의 예약명은 파생 글꼴 이름으로 사용하지 않음 |
| K-Dash UI 아이콘 비트맵 | Material Symbols Outlined, https://github.com/google/material-design-icons | Apache-2.0, Copyright Google LLC | 원본 TTF 미포함. 24/64px 1bpp 36개(6,552B)를 `tools/icon_subset/make_icons.py`로 생성. 라이선스 전문은 `src/ui/icons/LICENSE-Apache-2.0.txt`에 보존 |
| Pillow | https://pillow.readthedocs.io/ | HPND | 폰트 생성·프리뷰 변환용 개발 도구, 펌웨어 미포함 |
| pyserial | https://pyserial.readthedocs.io/ | BSD-3-Clause | `tools/uart_logger.py`용 개발 도구(미설치 상태 가능), 펌웨어 미포함 |
| 기상청 단기예보 API | https://www.data.go.kr/data/15084084/openapi.do | 공공데이터포털 이용조건 | 출처 표시 |
| 에어코리아 API | https://www.data.go.kr/data/15073861/openapi.do | 이용조건(출처표시·변경금지) | 심의 준수 |
| 수출입은행 환율 API | https://www.data.go.kr/data/3068846/openapi.do | 포털 이용조건 | 출처 표시 |
| 금융위원회 시세 API (P5) | https://www.data.go.kr/data/15094808/openapi.do | 공공누리 제4유형(출처표시·상업적 이용금지·변경금지, 제3자 재배포 금지) | 소유자 본인 기기 표시만. 실시간 오인 금지 |
| GxEPD2 (미사용) | https://github.com/ZinggJM/GxEPD2 | GPL-3.0 (LICENSE 원문 확인) | 펌웨어 미포함 — ADR 0004에 따라 자체 UC8179 우선 |
| SY6974 PMIC (직접 I2C 판독) | 공식 SenseCraft_HMI `lib/SY6974`+`pmic_sy6974` (동작 참조만) | — | 라이브러리 미포함, REG0A/REG08 읽기만 구현. 설정 쓰기 없음 |
| 공식 원문 보관 | `hardware/reference/` (MIT, Seeed-Projects, GxEPD2 제외) | 참조용, 펌웨어 미편입 |
| E1001 배터리 곡선 | 공식 SenseCraft_HMI E1001 `config.h` @8d97d891 | (표 데이터, 출처 표기) | `BatteryPolicy::PercentMv`에 101점 반영 |
| 한국투자증권 API (P5) | https://apiportal.koreainvestment.com/ (호출 호스트 `openapi.koreainvestment.com:9443`) | 개발자센터 약관 | READ ONLY, 재배포 제한 확인 필요 |

펌웨어 릴리스 전 각 항목의 정확한 라이선스 텍스트와 데이터 출처 표기를
확정한다. 미확정 항목이 있으면 릴리스 게이트 FAIL.
