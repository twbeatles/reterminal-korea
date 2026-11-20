#pragma once
#include "../../include/Config.h"
#include "../../include/IStorage.h"

namespace kdash { namespace portal {
enum class PortalResult { Closed, Saved, Reset, DisplayUnavailable, Failed };
using ShowPortal = bool (*)(const char* ap, const char* password, const char* url);
// The callback must draw credentials locally and report success BEFORE AP opens.
// No password is written to UART. Blocking, bounded to ten minutes.
PortalResult RunPortal(Settings& current, IStorage& storage, ShowPortal show);
// Portable commit used only after the adapter's successful connection test.
bool SaveSettingsVerified(IStorage& storage, Settings& current, const Settings& pending);
// 마지막 성공한 SaveSettingsVerified가 데이터 지문(위치·키·관심종목·Wi-Fi 등)을 바꿨는가.
// false면 기기 설정만 바뀐 저장이라 런타임 캐시를 지우지 않아도 된다(ADR 0026).
bool LastSaveChangedData();
// Non-destructive preflight: no allocation proportional to Content-Length.
// Returns 0 for incomplete headers, 200 accepted, otherwise rejection status.
int CheckPortalHeaders(const char* bytes, size_t len);
} }
