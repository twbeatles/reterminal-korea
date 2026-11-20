#pragma once
// RequestGuard: LAN 포털 요청 사전 검사 (순수). DNS 리바인딩·교차 출처 POST 차단. ADR 0024.
//  - Host는 기기 IP 또는 "<mdns>.local"만 허용(포트 허용). 허용 목록이 비면 모두 거부(fail closed).
//  - POST는 Origin(없으면 Referer)이 같은 호스트여야 한다. 둘 다 없으면 거부.
#include <stddef.h>

namespace kdash {
namespace portal {

struct AllowedHosts {
  char ip[16] = {0};    // "192.168.0.23"
  char mdns[24] = {0};  // "kdash-3f2a" (".local" 제외)
};

bool HostAllowed(const char* host_header, const AllowedHosts& allowed);
bool OriginAllowed(const char* origin, const char* referer, const AllowedHosts& allowed);
// "a=1; kdash_sid=abc; b=2" 에서 name 값을 꺼낸다. 없거나 cap보다 길면 false.
bool ExtractCookie(const char* cookie_header, const char* name, char* out, size_t cap);

}  // namespace portal
}  // namespace kdash
