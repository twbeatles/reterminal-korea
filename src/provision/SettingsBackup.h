#pragma once
// SettingsBackup: 비밀 제외 설정 백업(JSON) 내보내기/가져오기 (순수). docs/RECOVERY 2단계, ADR 0024.
//  - 내보내기: 지역, 관심종목, 공급자 사용 여부, 기기 설정. Wi-Fi SSID/비밀번호와 API 키·시크릿은 절대 넣지 않는다.
//  - 가져오기: 같은 항목만 받고, 항목마다 기존 검증기를 거친다. 하나라도 틀리면 아무 것도 적용하지 않는다.
//    오류 응답에는 입력값이 아니라 필드 이름만 담는다.
#include <stddef.h>

#include "../../include/Config.h"

namespace kdash {
namespace portal {

inline constexpr size_t kBackupMax = 1536;  // 가져오기 입력 상한(바이트)
inline constexpr const char* kBackupFormat = "kdash-backup";
inline constexpr int kBackupVersion = 1;

// 성공 시 JSON 길이, 버퍼가 모자라면 0.
size_t ExportBackup(const Settings& s, char* out, size_t cap);

enum class ImportResult : uint8_t { Ok = 0, BadJson, BadFormat, BadValue };

// base를 복사해 갱신한 뒤 Ok일 때만 inout에 반영한다. 실패 시 bad_field에 필드 이름(값 아님).
ImportResult ImportBackup(const char* json, Settings& inout, char* bad_field, size_t bad_cap);

}  // namespace portal
}  // namespace kdash
