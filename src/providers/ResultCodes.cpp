#include "ResultCodes.h"

#include <ctype.h>
#include <string.h>

namespace kdash {
namespace codes {
namespace {

bool Eq(const char* a, const char* b) { return a && b && strcmp(a, b) == 0; }

}  // namespace

Outcome DataGoKr(const char* c) {
  if (!c || !*c) return Outcome::ProviderError;
  if (Eq(c, "00") || Eq(c, "0")) return Outcome::Ok;
  if (Eq(c, "03")) return Outcome::NoData;
  if (Eq(c, "22")) return Outcome::QuotaExceeded;
  if (Eq(c, "20") || Eq(c, "21") || Eq(c, "30") || Eq(c, "31") ||
      Eq(c, "32") || Eq(c, "33"))
    return Outcome::AuthError;
  return Outcome::ProviderError;
}

Outcome Exim(int result) {
  switch (result) {
    case 1:
      return Outcome::Ok;
    case 3:
      return Outcome::AuthError;
    case 4:
      return Outcome::QuotaExceeded;
    default:
      return Outcome::ProviderError;
  }
}

bool KisTokenInvalid(int http_status, const char* msg_cd) {
  return http_status == 401 || Eq(msg_cd, "EGW00123") ||
         Eq(msg_cd, "EGW00121");
}

Outcome Kis(int http_status, const char* rt_cd, const char* msg_cd) {
  if (KisTokenInvalid(http_status, msg_cd)) return Outcome::AuthError;
  // Rate codes first: tokenP answers "1분당 1회" (EGW00133) with HTTP 403.
  if (http_status == 429 || Eq(msg_cd, "EGW00201") || Eq(msg_cd, "EGW00133"))
    return Outcome::RateLimited;
  if (http_status == 403) return Outcome::AuthError;
  if (http_status != 200) return Outcome::ProviderError;
  return Eq(rt_cd, "0") ? Outcome::Ok : Outcome::ProviderError;
}

bool LooksLikeXml(const char* body) {
  if (!body) return false;
  // Skip UTF-8 BOM and whitespace.
  const unsigned char* p = (const unsigned char*)body;
  if (p[0] == 0xEF && p[1] == 0xBB && p[2] == 0xBF) p += 3;
  while (*p && isspace(*p)) p++;
  return *p == '<';
}

bool XmlTagText(const char* body, const char* tag, char* out,
                unsigned out_cap) {
  if (!body || !tag || !out || out_cap == 0) return false;
  out[0] = '\0';
  char open[48];
  size_t tl = strlen(tag);
  if (tl + 3 > sizeof(open)) return false;
  open[0] = '<';
  memcpy(open + 1, tag, tl);
  open[tl + 1] = '>';
  open[tl + 2] = '\0';
  const char* s = strstr(body, open);
  if (!s) return false;
  s += tl + 2;
  const char* e = strchr(s, '<');
  if (!e) return false;
  while (s < e && isspace((unsigned char)*s)) s++;
  while (e > s && isspace((unsigned char)*(e - 1))) e--;
  size_t n = (size_t)(e - s);
  if (n + 1 > out_cap) return false;
  memcpy(out, s, n);
  out[n] = '\0';
  return true;
}

}  // namespace codes
}  // namespace kdash
