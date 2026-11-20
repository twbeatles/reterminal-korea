// Included by the native test entrypoint. Synthetic fixtures, no network/keys.
#include "../src/providers/HolidayService.h"

namespace audit_holiday {
class Http : public kdash::IHttpClient {
 public:
  std::string data;
  int calls = 0;
  int status = 200;
  kdash::HttpResponse post(const char*, const char* const*, size_t,
                           const char*, const char*) override {
    kdash::HttpResponse response;
    response.error = kdash::HttpError::Transport;
    return response;
  }
  kdash::HttpResponse get(const char* url, const char* const*, size_t,
                          const char* ca) override {
    ++calls;
    TEST_ASSERT_TRUE(kdash::net::HostAllowed(url));
    TEST_ASSERT_TRUE(ca && *ca);
    kdash::HttpResponse response;
    response.status = status;
    response.body = data.c_str();
    response.body_len = data.size();
    return response;
  }
};
}

void test_holiday_service() {
  using namespace kdash;
  FetchContext ctx;
  ctx.now_utc = ctx.fetched_utc = 1793600000;
  ctx.clock_verified = true;
  cal::YearTable table;
  cal::Clear(table, 2026);
  cal::SetMarketClosed(table, 12, 31);  // Synthetic separate KRX closure.
  std::string good = ReadFile("test/fixtures/holiday_year_demo.json");
  TEST_ASSERT_FALSE(good.empty());
  TEST_ASSERT_EQUAL((int)Outcome::Ok,
      (int)holidays::ParseHolidayYear(good.c_str(), good.size(), 2026, ctx, table));
  TEST_ASSERT_TRUE(table.loaded);
  TEST_ASSERT_TRUE(cal::IsHoliday(table, 2, 17));
  TEST_ASSERT_TRUE(cal::IsHoliday(table, 2, 18));
  TEST_ASSERT_TRUE(cal::IsMarketClosed(table, 12, 31));
  TEST_ASSERT_EQUAL_INT64(ctx.fetched_utc, table.fetched_utc);
  cal::YearTable original = table;
  for (const char* bad : {
      "{\"response\":{\"header\":{\"resultCode\":\"00\"},\"body\":{\"totalCount\":2,\"pageNo\":1,\"numOfRows\":128,\"items\":{\"item\":[{\"locdate\":20260217,\"isHoliday\":\"Y\"}]}}}}",
      "{\"response\":{\"header\":{\"resultCode\":\"00\"},\"body\":{\"totalCount\":1,\"pageNo\":1,\"numOfRows\":128,\"items\":{\"item\":{\"locdate\":20260230,\"isHoliday\":\"Y\"}}}}}",
      "{\"response\":{\"header\":{\"resultCode\":\"00\"},\"body\":{\"totalCount\":1,\"pageNo\":1,\"numOfRows\":128,\"items\":{\"item\":{\"locdate\":20270217,\"isHoliday\":\"Y\"}}}}}",
      "{\"response\":{\"header\":{\"resultCode\":\"00\"},\"body\":{\"totalCount\":1,\"pageNo\":1,\"numOfRows\":128,\"items\":{\"item\":{\"locdate\":20260217,\"isHoliday\":\"?\"}}}}}",
      "{\"response\":{\"header\":{\"resultCode\":\"00\"},\"body\":{\"totalCount\":200,\"pageNo\":1,\"numOfRows\":128}}}",
      "{\"response\":{\"header\":{\"resultCode\":\"00\"},\"body\":{\"totalCount\":1,\"pageNo\":2,\"numOfRows\":128}}}"}) {
    TEST_ASSERT_EQUAL((int)Outcome::ProviderError,
        (int)holidays::ParseHolidayYear(bad, strlen(bad), 2026, ctx, table));
    TEST_ASSERT_EQUAL_MEMORY(&original, &table, sizeof(table));
  }
  const char* auth = "<OpenAPI_ServiceResponse><cmmMsgHeader><returnReasonCode>30</returnReasonCode></cmmMsgHeader></OpenAPI_ServiceResponse>";
  TEST_ASSERT_EQUAL((int)Outcome::AuthError,
      (int)holidays::ParseHolidayYear(auth, strlen(auth), 2026, ctx, table));
  const char* empty = "{\"response\":{\"header\":{\"resultCode\":\"00\"},\"body\":{\"totalCount\":0,\"pageNo\":1,\"numOfRows\":128}}}";
  TEST_ASSERT_EQUAL((int)Outcome::NoData,
      (int)holidays::ParseHolidayYear(empty, strlen(empty), 2026, ctx, table));
  char url[1100];
  TEST_ASSERT_TRUE(holidays::HolidayYearUrl("FAKE+KEY=", 2026, url, sizeof(url)));
  TEST_ASSERT_NOT_NULL(strstr(url, "ServiceKey=FAKE%2BKEY%3D&solYear=2026"));
  TEST_ASSERT_NOT_NULL(strstr(url, "getRestDeInfo"));
  TEST_ASSERT_FALSE(holidays::HolidayYearUrl("FAKE", 1999, url, sizeof(url)));
  audit_holiday::Http http;
  http.data = good;
  TEST_ASSERT_EQUAL((int)Outcome::Ok,
      (int)holidays::FetchHolidayYear(http, "FAKE", "PUBLIC-CA", 2026, ctx, table));
  TEST_ASSERT_EQUAL(1, http.calls);
  ctx.clock_verified = false;
  TEST_ASSERT_EQUAL((int)Outcome::ProviderError,
      (int)holidays::FetchHolidayYear(http, "FAKE", "PUBLIC-CA", 2026, ctx, table));
  TEST_ASSERT_EQUAL(1, http.calls);
}
