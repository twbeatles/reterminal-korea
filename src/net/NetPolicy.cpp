#include "NetPolicy.h"

#include <ctype.h>
#include <string.h>

namespace kdash {
namespace net {

bool UsesHttps(const char* url) {
  return url && strncmp(url, "https://", 8) == 0;
}

bool HostAllowed(const char* url) {
  if (!UsesHttps(url)) return false;  // 비검증 HTTP 강등 금지
  const char* host = url + 8;
  const char* end = host;
  while (*end && *end != '/' && *end != '?' && *end != '#') end++;
  // Authority = host[:port]. Reject userinfo and anything unusual.
  const char* colon = nullptr;
  for (const char* p = host; p < end; p++) {
    const unsigned char c = (unsigned char)*p;
    if (c == ':') {
      if (colon) return false;
      colon = p;
    } else if (!(isalnum(c) || c == '.' || c == '-')) {
      return false;  // '@', '\\', '%', spaces, non-ASCII...
    }
  }
  const char* host_end = colon ? colon : end;
  const size_t hlen = (size_t)(host_end - host);
  if (hlen == 0 || hlen > 64) return false;
  long port = 443;
  if (colon) {
    if (colon + 1 == end || end - colon > 6) return false;
    port = 0;
    for (const char* p = colon + 1; p < end; p++) {
      if (!isdigit((unsigned char)*p)) return false;
      port = port * 10 + (*p - '0');
    }
  }
  for (const AllowedHost& a : kAllowHosts) {
    if (strlen(a.host) != hlen || (long)a.port != port) continue;
    bool same = true;
    for (size_t i = 0; i < hlen && same; i++) {
      same = tolower((unsigned char)host[i]) == (unsigned char)a.host[i];
    }
    if (same) return true;
  }
  return false;
}

int ParseRetryAfter(const char* value) {
  if (!value) return -1;
  while (*value && isspace((unsigned char)*value)) value++;
  if (!isdigit((unsigned char)*value)) return -1;  // HTTP-date => backoff path
  int secs = 0;
  for (; isdigit((unsigned char)*value); value++) {
    secs = secs * 10 + (*value - '0');
    if (secs > 3600) return 3600;  // clamp: 어떤 경우도 호출 폭주 금지
  }
  return secs;
}

bool BodySizeAllowed(size_t bytes, size_t cap) { return bytes <= cap; }

}  // namespace net
}  // namespace kdash
