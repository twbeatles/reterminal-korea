# ADR 0011 — 실제 공공 API의 chunked 수신 및 FSC V2 주소

- 상태: 채택, 2026-11-11. 실기 후속 결과는 evidence/live-api-integration/summary.md.
- 질문: PC의 KMA/Air 정상 JSON이 E1001에서는 HTTP 200/PROVIDER_ERROR가 되는 이유와 FSC 서비스 주소의 현행성.
- 근거: 사용자 승인키로 제한된 PC probe에서 KMA 실황/예보 72행·Air 정상 내부 코드 00을 수집했다. 별도 예산 1회를 예약한 HTTP/1.0 KMA 요청도 `Transfer-Encoding: chunked`, Content-Length 없음으로 응답했고 raw prefix는 `38b\r\n{`였다. 기존 adapter는 `useHTTP10(true)`가 chunked를 막는다고 가정하고 `getStreamPtr()`를 직접 ReadBody에 전달하여 framing까지 JSON으로 해석했다. SDK 원문상 raw stream은 dechunk하지 않는다.
- 선택: 공통 ReadBody에 명시적인 chunked 경로를 추가한다. decoded body 64KiB, 단일 전체 deadline, chunk/trailer line 255바이트, 총 framing 8KiB 상한, 완전한 zero chunk와 trailers 종료를 요구한다. 지원하지 않는 transfer/content coding은 실패로 처리한다. JSON의 회차/격자/내부 성공코드 검증은 유지한다.
- 다른 선택지: HTTPClient writeToStream은 내부 framing 처리와 오류 경계를 Host fault test로 같은 경로에서 검증하기 어렵다. HTTP/1.0 강제만으로 회피하는 방안은 실제 서버 응답으로 반증되었다.
- 영향: BodyReader의 기존 호출은 identity 기본값 유지. 새 라이브러리 없음, 상수 stack buffer 256바이트와 상태만 추가. TLS 검증·요청 예산·키 비노출 유지. RFC의 모든 합법적인 대용량 trailer/extension을 지원하는 범용 HTTP 구현은 아니며 앱 상한을 넘으면 안전하게 거부한다.
- FSC: [공식 상세](https://www.data.go.kr/data/15094808/openapi.do)의 2026-10-28 수정본과 사용자 승인 화면 모두 `/1160100/GetStockSecuritiesInfoService_V2`를 명시한다. 페이지 내 공식 Swagger의 operation은 `getStockPriceInfo_V2`다. root만 교체하고 구 operation을 붙인 첫 probe는 HTTP 400/코드12로 실패했으며, operation까지 수정한 후 200/00·005930 종가를 확인했다. 펌웨어/Python mirror를 함께 교체하고 URL 회귀 검증을 추가한다. 종가/기준일·비실시간 표시 정책은 유지한다.
- 증거: evidence/probe/20261111T005356Z-a7479994.json, 정제 live_* fixture, test_native/test_live_chunked.inc, evidence/live-api-integration/. forecast-1000은 64KiB 정책 밖으로 probe가 실패했으며 72행 성공과 구분한다.
- 되돌리는 방법: 소스 및 테스트를 같은 commit 기준으로 복원하고 이전 펌웨어를 USB 업로드할 수 있다. 다만 이전 수신 경로는 현재 KMA/Air 서버에 대해 동작하지 않으므로 회피책이 아니다. NVS schema/파티션/eFuse 변경은 없다.
