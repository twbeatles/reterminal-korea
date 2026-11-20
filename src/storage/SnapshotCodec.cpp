#include "SnapshotCodec.h"

#include <string.h>
#include <memory>
#include <new>

#include "Codec.h"

namespace kdash {
namespace snapshot_codec {
namespace {

bool Streq(const char* a, const char* b) { return a && b && strcmp(a, b) == 0; }

bool WTs(codec::Writer& w, const Timestamp& t) {
  return w.i64(t.epoch_utc) && w.u8(t.verified ? 1 : 0);
}
bool RTs(codec::Reader& r, Timestamp& t) {
  uint8_t v = 0;
  if (!r.i64(t.epoch_utc) || !r.u8(v)) return false;
  t.verified = v != 0;
  return true;
}
bool WHead(codec::Writer& w, bool has, Validity v, const Timestamp& obs,
           const Timestamp& fet, const char* src) {
  return w.u8(has ? 1 : 0) && w.u8((uint8_t)v) && WTs(w, obs) && WTs(w, fet) &&
         w.u8(SourceId(src));
}
bool RHead(codec::Reader& r, bool& has, Validity& v, Timestamp& obs,
           Timestamp& fet, const char*& src) {
  uint8_t h = 0, vv = 0, sid = 0;
  if (!r.u8(h) || !r.u8(vv)) return false;
  if (!RTs(r, obs) || !RTs(r, fet)) return false;
  if (!r.u8(sid)) return false;
  if (vv > (uint8_t)Validity::NotConfigured) return false;
  has = h != 0;
  v = (Validity)vv;
  src = SourceStr(sid);
  return true;
}

bool WDbl(codec::Writer& w, const DataPoint<double>& d) {
  if (!WHead(w, d.has_value, d.validity, d.observed_at, d.fetched_at, d.source))
    return false;
  uint64_t u = 0;
  memcpy(&u, &d.value, sizeof(u));
  for (int i = 0; i < 8; i++) {
    if (!w.u8((uint8_t)((u >> (8 * i)) & 0xFF))) return false;
  }
  return true;
}
bool RDbl(codec::Reader& r, DataPoint<double>& d) {
  bool has = false;
  Validity v = Validity::NotConfigured;
  const char* src = "";
  if (!RHead(r, has, v, d.observed_at, d.fetched_at, src)) return false;
  uint64_t u = 0;
  for (int i = 0; i < 8; i++) {
    uint8_t b = 0;
    if (!r.u8(b)) return false;
    u |= ((uint64_t)b << (8 * i));
  }
  double val = 0;
  memcpy(&val, &u, sizeof(val));
  d.has_value = has;
  d.validity = v;
  d.source = src;
  d.value = val;
  return true;
}
bool WInt(codec::Writer& w, const DataPoint<int>& d) {
  return WHead(w, d.has_value, d.validity, d.observed_at, d.fetched_at,
               d.source) &&
         w.i32(d.value);
}
bool RInt(codec::Reader& r, DataPoint<int>& d) {
  bool has = false;
  Validity v = Validity::NotConfigured;
  const char* src = "";
  if (!RHead(r, has, v, d.observed_at, d.fetched_at, src)) return false;
  int32_t val = 0;
  if (!r.i32(val)) return false;
  d.has_value = has;
  d.validity = v;
  d.source = src;
  d.value = val;
  return true;
}
bool WFx(codec::Writer& w, const DataPoint<FxRate>& d) {
  return WHead(w, d.has_value, d.validity, d.observed_at, d.fetched_at,
               d.source) &&
         w.str(d.value.currency_unit, sizeof(d.value.currency_unit) - 1) &&
         w.i64(d.value.won_scaled) && w.i32(d.value.scale) &&
         w.u8((uint8_t)d.value.quote_kind);
}
bool RFx(codec::Reader& r, DataPoint<FxRate>& d) {
  bool has = false;
  Validity v = Validity::NotConfigured;
  const char* src = "";
  if (!RHead(r, has, v, d.observed_at, d.fetched_at, src)) return false;
  char unit[16] = {0};
  int64_t ws = 0;
  int32_t sc = 0;
  uint8_t k = 0;
  if (!r.str(unit, sizeof(unit), sizeof(unit) - 1)) return false;
  if (!r.i64(ws) || !r.i32(sc) || !r.u8(k)) return false;
  d.has_value = has;
  d.validity = v;
  d.source = src;
  strncpy(d.value.currency_unit, unit, sizeof(d.value.currency_unit) - 1);
  d.value.won_scaled = ws;
  d.value.scale = sc;
  d.value.quote_kind = (QuoteKind)k;
  return true;
}
bool WStock(codec::Writer& w, const DataPoint<StockQuote>& d) {
  return WHead(w, d.has_value, d.validity, d.observed_at, d.fetched_at,
               d.source) &&
         w.str(d.value.symbol, sizeof(d.value.symbol) - 1) &&
         w.str(d.value.market, sizeof(d.value.market) - 1) &&
         w.i64(d.value.price_krw) && w.u8((uint8_t)d.value.kind) &&
         WTs(w, d.value.price_at);
}
bool RStock(codec::Reader& r, DataPoint<StockQuote>& d) {
  bool has = false;
  Validity v = Validity::NotConfigured;
  const char* src = "";
  if (!RHead(r, has, v, d.observed_at, d.fetched_at, src)) return false;
  char sym[8] = {0}, mkt[16] = {0};
  int64_t price = 0;
  uint8_t k = 0;
  if (!r.str(sym, sizeof(sym), sizeof(sym) - 1)) return false;
  if (!r.str(mkt, sizeof(mkt), sizeof(mkt) - 1)) return false;
  if (!r.i64(price) || !r.u8(k)) return false;
  if (!RTs(r, d.value.price_at)) return false;
  d.has_value = has;
  d.validity = v;
  d.source = src;
  strncpy(d.value.symbol, sym, sizeof(d.value.symbol) - 1);
  strncpy(d.value.market, mkt, sizeof(d.value.market) - 1);
  d.value.price_krw = price;
  d.value.kind = (QuoteKind)k;
  return true;
}

}  // namespace

uint8_t SourceId(const char* s) {
  if (!s) return 0;
  if (Streq(s, "기상청")) return 1;
  if (Streq(s, "에어코리아")) return 2;
  if (Streq(s, "한국수출입은행")) return 3;
  if (Streq(s, "금융위원회")) return 4;
  if (Streq(s, "한국투자증권")) return 5;
  if (Streq(s, "실내 센서")) return 6;
  // DemoProvider synthetic values: DEMO marking survives codec round-trip.
  if (Streq(s, "기상청(DEMO)")) return 7;
  if (Streq(s, "에어코리아(DEMO)")) return 8;
  if (Streq(s, "한국수출입은행(DEMO)")) return 9;
  return 0;
}

const char* SourceStr(uint8_t id) {
  switch (id) {
    case 1: return "기상청";
    case 2: return "에어코리아";
    case 3: return "한국수출입은행";
    case 4: return "금융위원회";
    case 5: return "한국투자증권";
    case 6: return "실내 센서";
    case 7: return "기상청(DEMO)";
    case 8: return "에어코리아(DEMO)";
    case 9: return "한국수출입은행(DEMO)";
    default: return "";
  }
}

size_t EncodeSnapshot(const app::Snapshot& s, uint8_t* out, size_t out_cap) {
  codec::Writer w(out, out_cap);
  w.u16(3);  // v2 appends local sensor values, v3 the KOSPI index; v1/v2 remain readable.
  w.u8(s.has_now ? 1 : 0);
  if (s.has_now) {
    if (!WDbl(w, s.now.temp_c) || !WDbl(w, s.now.humidity) ||
        !WDbl(w, s.now.rain_mm) || !w.i64(s.now.base_utc) ||
        !w.u8((uint8_t)s.now.outcome))
      return 0;
  }
  w.u8(s.has_fc ? 1 : 0);
  if (s.has_fc) {
    if (!WDbl(w, s.fc.tmp) || !WInt(w, s.fc.pop) || !WInt(w, s.fc.pty) ||
        !WInt(w, s.fc.sky) || !w.i64(s.fc.fcst_utc) || !w.i64(s.fc.base_utc) ||
        !w.u8((uint8_t)s.fc.outcome))
      return 0;
  }
  w.u8(s.has_air ? 1 : 0);
  if (s.has_air) {
    if (!WInt(w, s.air.pm10) || !WInt(w, s.air.pm25) ||
        !w.i64(s.air.data_time_utc) ||
        !w.str(s.air.station, sizeof(s.air.station) - 1) ||
        !w.u8((uint8_t)s.air.outcome))
      return 0;
  }
  w.u8(s.has_fx ? 1 : 0);
  if (s.has_fx) {
    if (!WFx(w, s.fx.usd) || !WFx(w, s.fx.jpy100) ||
        !w.i64(s.fx.fixing_date_utc) || !w.u8((uint8_t)s.fx.outcome) ||
        !w.u8(s.fx.empty_response ? 1 : 0) || !w.i32(s.fx.error_result))
      return 0;
  }
  for (int i = 0; i < kEximFallbackMaxDays; i++) w.i32(s.fx_empty_days[i]);
  for (size_t i = 0; i < kMaxWatchlist; i++) {
    w.u8(s.has_fsc[i] ? 1 : 0);
    if (s.has_fsc[i]) {
      if (!WStock(w, s.fsc[i].quote) || !w.u8((uint8_t)s.fsc[i].outcome))
        return 0;
    }
    w.u8(s.has_kis[i] ? 1 : 0);
    if (s.has_kis[i]) {
      if (!WStock(w, s.kis[i].quote) || !w.u8((uint8_t)s.kis[i].outcome) ||
          !w.u8(s.kis[i].token_invalid ? 1 : 0))
        return 0;
    }
  }
  if (!WDbl(w, s.indoor_c) || !WDbl(w, s.indoor_rh)) return 0;
  // v3: KOSPI index (REST snapshot). Fixed-point value, optional change rate.
  w.u8(s.has_kospi ? 1 : 0);
  if (s.has_kospi) {
    const IndexQuote& q = s.kospi.quote.value;
    if (!WHead(w, s.kospi.quote.has_value, s.kospi.quote.validity,
               s.kospi.quote.observed_at, s.kospi.quote.fetched_at, s.kospi.quote.source) ||
        !w.str(q.name, sizeof(q.name) - 1) || !w.i64(q.value_x100) ||
        !w.u8(q.has_change ? 1 : 0) || !w.i32(q.change_bp) || !WTs(w, q.at) ||
        !w.u8((uint8_t)s.kospi.outcome) || !w.u8(s.kospi.token_invalid ? 1 : 0))
      return 0;
  }
  if (!w.ok()) return 0;
  return w.pos();
}

bool DecodeSnapshot(const uint8_t* data, size_t len, app::Snapshot& out) {
  if (!data) return false;
  codec::Reader r(data, len);
  uint16_t ver = 0;
  uint8_t f = 0;
  if (!r.u16(ver) || ver < 1 || ver > 3) return false;
  std::unique_ptr<app::Snapshot> temporary(new (std::nothrow) app::Snapshot);
  if (!temporary) return false;
  app::Snapshot& s = *temporary;
  if (!r.u8(f)) return false;
  s.has_now = f != 0;
  if (s.has_now) {
    uint8_t oc = 0;
    if (!RDbl(r, s.now.temp_c) || !RDbl(r, s.now.humidity) ||
        !RDbl(r, s.now.rain_mm) || !r.i64(s.now.base_utc) || !r.u8(oc))
      return false;
    s.now.outcome = (Outcome)oc;
  }
  if (!r.u8(f)) return false;
  s.has_fc = f != 0;
  if (s.has_fc) {
    uint8_t oc = 0;
    if (!RDbl(r, s.fc.tmp) || !RInt(r, s.fc.pop) || !RInt(r, s.fc.pty) ||
        !RInt(r, s.fc.sky) || !r.i64(s.fc.fcst_utc) || !r.i64(s.fc.base_utc) ||
        !r.u8(oc))
      return false;
    s.fc.outcome = (Outcome)oc;
  }
  if (!r.u8(f)) return false;
  s.has_air = f != 0;
  if (s.has_air) {
    uint8_t oc = 0;
    if (!RInt(r, s.air.pm10) || !RInt(r, s.air.pm25)) return false;
    if (!r.i64(s.air.data_time_utc)) return false;
    if (!r.str(s.air.station, sizeof(s.air.station), kMaxStationLen))
      return false;
    if (!r.u8(oc)) return false;
    s.air.outcome = (Outcome)oc;
  }
  if (!r.u8(f)) return false;
  s.has_fx = f != 0;
  if (s.has_fx) {
    uint8_t oc = 0, empty = 0;
    int32_t err = 0;
    if (!RFx(r, s.fx.usd) || !RFx(r, s.fx.jpy100)) return false;
    if (!r.i64(s.fx.fixing_date_utc) || !r.u8(oc) || !r.u8(empty) ||
        !r.i32(err))
      return false;
    s.fx.outcome = (Outcome)oc;
    s.fx.empty_response = empty != 0;
    s.fx.error_result = err;
  }
  for (int i = 0; i < kEximFallbackMaxDays; i++) {
    int32_t v = 0;
    if (!r.i32(v)) return false;
    s.fx_empty_days[i] = v;
  }
  for (size_t i = 0; i < kMaxWatchlist; i++) {
    if (!r.u8(f)) return false;
    s.has_fsc[i] = f != 0;
    if (s.has_fsc[i]) {
      uint8_t oc = 0;
      if (!RStock(r, s.fsc[i].quote) || !r.u8(oc)) return false;
      s.fsc[i].outcome = (Outcome)oc;
    }
    if (!r.u8(f)) return false;
    s.has_kis[i] = f != 0;
    if (s.has_kis[i]) {
      uint8_t oc = 0, ti = 0;
      if (!RStock(r, s.kis[i].quote) || !r.u8(oc) || !r.u8(ti)) return false;
      s.kis[i].outcome = (Outcome)oc;
      s.kis[i].token_invalid = ti != 0;
    }
  }
  if (ver >= 2 && (!RDbl(r, s.indoor_c) || !RDbl(r, s.indoor_rh))) return false;
  if (ver >= 3) {
    if (!r.u8(f) || f > 1) return false;
    s.has_kospi = f != 0;
    if (s.has_kospi) {
      bool has = false;
      Validity v = Validity::NotConfigured;
      const char* src = "";
      char name[12] = {0};
      int64_t value = 0;
      int32_t bp = 0;
      uint8_t chg = 0, oc = 0, ti = 0;
      IndexQuote& q = s.kospi.quote.value;
      if (!RHead(r, has, v, s.kospi.quote.observed_at, s.kospi.quote.fetched_at, src) ||
          !r.str(name, sizeof(name), sizeof(name) - 1) || !r.i64(value) || !r.u8(chg) ||
          !r.i32(bp) || !RTs(r, q.at) || !r.u8(oc) || !r.u8(ti) || chg > 1 || value < 0)
        return false;
      s.kospi.quote.has_value = has;
      s.kospi.quote.validity = v;
      s.kospi.quote.source = src;
      strncpy(q.name, name, sizeof(q.name) - 1);
      q.value_x100 = value;
      q.has_change = chg != 0;
      q.change_bp = bp;
      s.kospi.outcome = (Outcome)oc;
      s.kospi.token_invalid = ti != 0;
    }
  }
  if (!r.done()) return false;
  out = s;
  return true;
}

size_t EncodeStatus(const app::ProviderStatus* st, size_t n, uint8_t* out,
                    size_t out_cap) {
  codec::Writer w(out, out_cap);
  w.u16(1);
  w.u16((uint16_t)n);
  for (size_t i = 0; i < n; i++) {
    w.i64(st[i].last_success_utc);
    w.i64(st[i].last_attempt_utc);
    w.i32(st[i].attempts);
    w.u8((uint8_t)st[i].last_outcome);
    w.i64(st[i].retry_after_utc);
    w.u8(st[i].auth_blocked ? 1 : 0);
    w.i64(st[i].last_base_utc);
  }
  if (!w.ok()) return 0;
  return w.pos();
}

bool DecodeStatus(const uint8_t* data, size_t len, app::ProviderStatus* out,
                  size_t n) {
  if (!data || !out) return false;
  codec::Reader r(data, len);
  uint16_t ver = 0, count = 0;
  if (!r.u16(ver) || ver != 1) return false;
  if (!r.u16(count) || count != n) return false;
  for (size_t i = 0; i < n; i++) {
    int32_t att = 0;
    uint8_t oc = 0, ab = 0;
    if (!r.i64(out[i].last_success_utc)) return false;
    if (!r.i64(out[i].last_attempt_utc)) return false;
    if (!r.i32(att)) return false;
    if (!r.u8(oc)) return false;
    if (!r.i64(out[i].retry_after_utc)) return false;
    if (!r.u8(ab)) return false;
    if (!r.i64(out[i].last_base_utc)) return false;
    out[i].attempts = att;
    out[i].last_outcome = (Outcome)oc;
    out[i].auth_blocked = ab != 0;
  }
  return r.done();
}

}  // namespace snapshot_codec
}  // namespace kdash
