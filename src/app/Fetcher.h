#pragma once
// Fetch pass: DETERMINE_DUE -> FETCH (IHttpClient) -> VALIDATE/NORMALIZE
// (ProviderJson) -> merge into the last-known-good Snapshot -> MarkResult.
// Portable: the device passes the ESP32 transport, host tests a fake.
// Guarantees (docs/02/04):
//  - every URL passes net::HostAllowed before any request;
//  - at most kMaxRequestsPerPass requests per pass, one attempt each;
//  - per-provider caps (RequestBudget: Exim7/Fsc4/Kis6/Other2) — a cap hit
//    defers the remaining work to the next pass and is never a failure;
//  - a failed provider keeps its last good values (re-graded Stale by TTL),
//    never blanks the screen and never shows 0;
//  - no URL, key, secret or token is ever written to the PassLog.
#include <stddef.h>
#include <stdint.h>

#include "../../include/IHttpClient.h"
#include "../providers/ProviderJson.h"
#include "Coordinator.h"

namespace kdash {
namespace app {

inline constexpr int kMaxRequestsPerPass = 24;

struct Snapshot {
  DataPoint<double> indoor_c;
  DataPoint<double> indoor_rh;
  kma::NowOut now;
  bool has_now = false;
  kma::ForecastOut fc;
  bool has_fc = false;
  air::AirOut air;
  bool has_air = false;
  exim::EximOut fx;
  bool has_fx = false;
  // Past dates (< today KST) the EXIM API answered empty for: final
  // (weekend/holiday), so the walk-back never asks for them again.
  int fx_empty_days[kEximFallbackMaxDays] = {};
  fin::FscOut fsc[kMaxWatchlist];
  bool has_fsc[kMaxWatchlist] = {};
  fin::KisOut kis[kMaxWatchlist];
  bool has_kis[kMaxWatchlist] = {};
  // 코스피 지수(KIS). 종목 시세와 같은 토큰을 쓰며, 실패해도 종목 시세 상태에 영향 없음.
  fin::KisIndexOut kospi;
  bool has_kospi = false;
};

// KIS OAuth token cache (RAM/RTC memory only; never logged, never persisted
// in plain text). tokenP is limited to one issue per minute (EGW00133).
struct KisSession {
  char token[1024] = {0};
  int64_t expiry_utc = 0;
  int64_t last_issue_attempt_utc = 0;
};

struct PassEntry {
  sched::Provider provider;
  Outcome outcome;
  int http_status;   // last HTTP status seen (0 = transport error)
  int requests;      // requests spent on this provider in this pass
};

struct PassLog {
  PassEntry entries[(int)sched::Provider::Count];
  int count = 0;
  int requests = 0;
};

class IFetchControl {
 public:
  virtual ~IFetchControl() = default;
  virtual bool CanRequest() const = 0;
};

// Runs one pass over the providers due at ctx.now_utc.
void RunFetchPass(AppState& st, Snapshot& snap, KisSession& kis,
                  IHttpClient& http, const char* ca_bundle,
                  const FetchContext& ctx, bool usb, int jitter_slot_ms,
                  PassLog* log, IFetchControl* control = nullptr,
                  bool force = false);

// Re-grades every held value against its TTL at ctx.now_utc (last-known-good
// values age into Stale even when no fetch happens).
void RefreshFreshness(Snapshot& snap, const FetchContext& ctx);

// Local measurement; failed reads retain the last good sample.
void RecordIndoor(AppState& st, Snapshot& snap, bool read_ok, double temp_c,
                  double rh, const FetchContext& ctx, bool usb);

// Maps a transport/HTTP failure to an Outcome (Ok when the body should be
// handed to the provider parser).
Outcome ClassifyHttp(const HttpResponse& r);

}  // namespace app
}  // namespace kdash
