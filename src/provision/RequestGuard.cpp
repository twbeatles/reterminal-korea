#include "RequestGuard.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

namespace kdash {
namespace portal {
namespace {

bool EqNoCase(const char* a, size_t n, const char* b) {
  if (strlen(b) != n) return false;
  for (size_t i = 0; i < n; i++)
    if (tolower((unsigned char)a[i]) != tolower((unsigned char)b[i])) return false;
  return true;
}

bool PrefixNoCase(const char* s, const char* prefix) {
  const size_t n = strlen(prefix);
  return EqNoCase(s, n < strlen(s) ? n : strlen(s), prefix);
}

// host[:port] 에서 호스트 길이. 포트가 1~5자리 숫자가 아니면 -1.
int HostLen(const char* h, size_t len) {
  if (len == 0) return -1;
  const char* colon = (const char*)memchr(h, ':', len);
  if (!colon) return (int)len;
  size_t host = (size_t)(colon - h);
  size_t port = len - host - 1;
  if (host == 0 || port == 0 || port > 5) return -1;
  for (size_t i = host + 1; i < len; i++)
    if (!isdigit((unsigned char)h[i])) return -1;
  return (int)host;
}

bool MatchHostPort(const char* h, size_t len, const AllowedHosts& a) {
  const int hl = HostLen(h, len);
  if (hl < 0) return false;
  if (a.ip[0] && EqNoCase(h, (size_t)hl, a.ip)) return true;
  if (a.mdns[0]) {
    char name[40];
    const int n = snprintf(name, sizeof(name), "%s.local", a.mdns);
    if (n > 0 && (size_t)n < sizeof(name) && EqNoCase(h, (size_t)hl, name)) return true;
  }
  return false;
}

}  // namespace

bool HostAllowed(const char* host_header, const AllowedHosts& allowed) {
  if (!host_header || !*host_header) return false;
  return MatchHostPort(host_header, strlen(host_header), allowed);
}

bool OriginAllowed(const char* origin, const char* referer, const AllowedHosts& allowed) {
  static const char kScheme[] = "http://";
  const size_t sl = sizeof(kScheme) - 1;
  // "Origin: null"은 Origin이 없는 것과 같이 보고 Referer로 확인한다(Referrer-Policy가 엄격한 브라우저).
  // 교차 사이트 폼은 Referer에 공격자 주소가 실리거나 아예 없으므로 허용 호스트와 맞지 않는다.
  if (origin && *origin && strcmp(origin, "null") != 0) {
    if (!PrefixNoCase(origin, kScheme)) return false;  // https, 다른 스킴
    return MatchHostPort(origin + sl, strlen(origin + sl), allowed);
  }
  if (referer && *referer) {
    if (!PrefixNoCase(referer, kScheme)) return false;
    const char* host = referer + sl;
    return MatchHostPort(host, strcspn(host, "/?#"), allowed);
  }
  return false;
}

bool ExtractCookie(const char* cookie_header, const char* name, char* out, size_t cap) {
  if (!cookie_header || !name || !out || cap == 0) return false;
  const size_t nl = strlen(name);
  const char* p = cookie_header;
  while (*p) {
    while (*p == ' ' || *p == ';') p++;
    const char* end = p;
    while (*end && *end != ';') end++;
    if ((size_t)(end - p) > nl && strncmp(p, name, nl) == 0 && p[nl] == '=') {
      const size_t vl = (size_t)(end - p) - nl - 1;
      if (vl + 1 > cap) return false;
      memcpy(out, p + nl + 1, vl);
      out[vl] = '\0';
      return true;
    }
    p = end;
  }
  return false;
}

}  // namespace portal
}  // namespace kdash
