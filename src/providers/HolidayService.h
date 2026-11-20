#pragma once

#include <stddef.h>
#include "../../include/IHttpClient.h"
#include "../domain/Calendar.h"
#include "../domain/Types.h"

namespace kdash {
namespace holidays {
// Official KASI getRestDeInfo: solMonth is optional, so request a full year.
// A partial/truncated response never makes an incomplete year look loaded.
bool HolidayYearUrl(const char* key, int year, char* out, size_t cap);
Outcome ParseHolidayYear(const char* body, size_t len, int year,
                         const FetchContext& ctx, cal::YearTable& out);
// Exactly one HTTPS attempt; caller owns scheduling/backoff/request budgets.
Outcome FetchHolidayYear(IHttpClient& http, const char* key, const char* ca,
                         int year, const FetchContext& ctx, cal::YearTable& out);
}
}
