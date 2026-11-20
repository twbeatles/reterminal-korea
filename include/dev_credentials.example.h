#pragma once
// DEV-ONLY credential injection. Prefer the device setup portal for releases.
// Copy OUTSIDE this repo to ~/.config/kdash-e1001/kdash_private_credentials.h
// (or KDASH_PRIVATE_CONFIG_DIR/kdash_private_credentials.h) and fill
// in locally. NEVER commit the copy, paste it into chats/issues, or print it.
// Any field left "" disables that provider ('설정 필요').
//
// data.go.kr keys: paste either the "Encoding" or the "Decoding" key —
// both forms are accepted (src/net/Endpoints.h NormalizeServiceKey).

#define KDASH_DEV_WIFI_SSID ""
#define KDASH_DEV_WIFI_SECRET ""

#define KDASH_DEV_KMA_KEY ""   // data.go.kr 기상청_단기예보 (FSC도 이 키 사용)
#define KDASH_DEV_AIR_KEY ""   // data.go.kr 에어코리아 대기오염정보
#define KDASH_DEV_EXIM_KEY ""  // 한국수출입은행 현재환율 authkey

#define KDASH_DEV_GRID_X 60     // 기상청 격자 nx (행정동 격자 파일 기준)
#define KDASH_DEV_GRID_Y 127    // 기상청 격자 ny
#define KDASH_DEV_DISPLAY_NAME "서울 종로구"
#define KDASH_DEV_AIR_STATION "종로구"

// P5 optional (default OFF). Watchlist: "005930,000660:NXT" (max 4).
#define KDASH_DEV_FSC_ENABLED 0
#define KDASH_DEV_KIS_ENABLED 0
#define KDASH_DEV_KIS_APP_KEY ""
#define KDASH_DEV_KIS_APP_SECRET ""
#define KDASH_DEV_WATCHLIST ""
