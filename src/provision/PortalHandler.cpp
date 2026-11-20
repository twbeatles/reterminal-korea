#include "PortalHandler.h"

#include <memory>
#include <new>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../domain/StockNames.h"
#include "../storage/SettingsStore.h"
#include "GeoGrid.h"
#include "SettingsBackup.h"
#include "SetupPortal.h"

namespace kdash {
namespace portal {
namespace {

constexpr uint8_t kSecWifi = 1, kSecKeys = 2, kSecLocation = 4, kSecWatch = 8, kSecDevice = 16;
constexpr uint8_t kSecAll = kSecWifi | kSecKeys | kSecLocation | kSecWatch | kSecDevice;

const char* FindField(const FormField* fields, size_t n, const char* name) {
  for (size_t i = 0; fields && i < n; i++)
    if (fields[i].name && strcmp(fields[i].name, name) == 0)
      return fields[i].value ? fields[i].value : "";
  return "";
}
bool HasField(const FormField* fields, size_t n, const char* name) {
  for (size_t i = 0; fields && i < n; i++)
    if (fields[i].name && strcmp(fields[i].name, name) == 0) return true;
  return false;
}
bool Eq(const char* a, const char* b) { return a && b && strcmp(a, b) == 0; }

uint8_t SectionMask(const char* sec) {
  if (Eq(sec, "wifi")) return kSecWifi;
  if (Eq(sec, "keys")) return kSecKeys;
  if (Eq(sec, "location")) return kSecLocation;
  if (Eq(sec, "watch")) return kSecWatch;
  if (Eq(sec, "device")) return kSecDevice;
  return 0;
}

bool ParseInt(const char* s, int lo, int hi, int& out) {
  if (!s || !*s || strlen(s) > 4) return false;
  int v = 0;
  for (const char* p = s; *p; p++) {
    if (*p < '0' || *p > '9') return false;
    v = v * 10 + (*p - '0');
  }
  if (v < lo || v > hi) return false;
  out = v;
  return true;
}

const char kHead[] =
    "<!doctype html><html lang=ko><head><meta charset=utf-8>"
    "<meta name=viewport content=\"width=device-width,initial-scale=1\">"
    "<title>KDASH</title><style>"
    "body{font:16px/1.5 sans-serif;max-width:32rem;margin:0 auto;padding:12px}"
    "input,select,button{font:inherit;padding:8px;margin:2px 0;width:100%;box-sizing:border-box}"
    "input[type=checkbox]{width:auto}button{margin-top:8px}a{display:block;padding:10px 0}"
    "</style></head><body>";
const char kHttpWarning[] =
    "<p><small>이 페이지는 암호화되지 않은 HTTP입니다. 공용·손님 Wi-Fi에서는 API 키와 "
    "비밀번호를 입력하지 마세요. 민감하면 기기 AP 설정(녹색 2초)을 쓰세요.</small></p>";

// 고정 버퍼에 이어 붙이는 도우미. 넘치면 조용히 자르지 않고 overflow 표시(페이지는 오류로 대체).
struct Out {
  char* buf;
  size_t cap;
  size_t used = 0;
  bool overflow = false;
  void add(const char* s) {
    const size_t n = strlen(s);
    if (used + n + 1 > cap) { overflow = true; return; }
    memcpy(buf + used, s, n + 1);
    used += n;
  }
  // 이스케이프한 뒤 줄바꿈을 <br>로 바꿔 붙인다(상태 화면의 여러 줄).
  void add_text(const char* s) {
    char esc[8];
    for (const char* p = s ? s : ""; *p; p++) {
      if (*p == '\n') { add("<br>"); continue; }
      const char one[2] = {*p, '\0'};
      provision::EscapeHtml(one, esc, sizeof(esc));
      add(esc);
    }
  }
};

// 장소 입력: "37.5665" 같은 십진수만 허용(지수 표기·공백·여분 문자 거부).
bool ParseDecimal(const char* s, double& out) {
  if (!s || !*s || strlen(s) > 12) return false;
  const char* p = s;
  if (*p == '-') p++;
  bool digits = false, dot = false;
  for (; *p; p++) {
    if (*p >= '0' && *p <= '9') { digits = true; continue; }
    if (*p == '.' && !dot) { dot = true; continue; }
    return false;
  }
  if (!digits) return false;
  out = strtod(s, nullptr);
  return true;
}

}  // namespace

PortalHandler::PortalHandler(const Settings& current, IRandom& rng, int64_t opened)
    : current_(current), pending_(current), rng_(rng), auth_(rng), opened_(opened) {
  provision::IssueCsrfToken(rng_, csrf_, sizeof(csrf_));
  body_[0] = '\0';
}

void PortalHandler::UseLan(const AllowedHosts& hosts, int window_sec) {
  policy_ = AuthPolicy::LanPaired;
  hosts_ = hosts;
  window_sec_ = window_sec;
  auth_.Tick(opened_);
}

void PortalHandler::SetStatusLine(const char* line) {
  settings::CopyBounded(status_, sizeof(status_), line ? line : "");
}

void PortalHandler::SetScanResults(const ScanEntry* entries, size_t n, bool failed) {
  scan_state_ = failed ? ScanState::Failed : ScanState::Done;
  scan_n_ = 0;
  for (size_t i = 0; entries && i < n && scan_n_ < kMaxScan; i++) {
    if (!entries[i].ssid[0]) continue;  // 숨김 네트워크는 목록에 넣지 않는다
    bool dup = false;
    for (size_t j = 0; j < scan_n_; j++)
      if (strcmp(scan_[j].ssid, entries[i].ssid) == 0) dup = true;
    if (!dup) scan_[scan_n_++] = entries[i];
  }
}

void PortalHandler::Tick(int64_t now) {
  if (policy_ == AuthPolicy::LanPaired) auth_.Tick(now);
}

bool PortalHandler::PendingWifiChanged() const { return pending_wifi_changed_; }

PortalAction PortalHandler::TakeAction() {
  const PortalAction a = action_;
  action_ = PortalAction::None;
  return a;
}

Response PortalHandler::Render(int status, const char* text) {
  return RenderGo(status, text, "/", "설정으로", 0);
}

// 안내 문구 + 이동 링크. refresh_sec > 0이면 그 시간 뒤 자동으로 이동한다(meta refresh: JS 없이 동작).
Response PortalHandler::RenderGo(int status, const char* text, const char* url, const char* link,
                                 int refresh_sec) {
  char esc[512];
  provision::EscapeHtml(text, esc, sizeof(esc));
  char meta[96] = {0};
  if (refresh_sec > 0)
    snprintf(meta, sizeof(meta), "<meta http-equiv=refresh content=\"%d;url=%s\">", refresh_sec, url);
  snprintf(body_, sizeof(body_), "%s%s<p>%s</p><a href=%s>%s</a></body></html>", kHead, meta, esc,
           url, link);
  return Response{status, "text/html; charset=utf-8", body_, sizeof(body_), strlen(body_)};
}

// 휴대폰 화면 보조 스크립트(같은 출처 파일만 허용: CSP script-src 'self').
//  - 관심종목: 코드를 고치면 바로 종목명 표시, 이름으로 고를 수 있는 목록(datalist).
//  - 비밀번호 칸: "보이기" 체크박스.
// 공개 종목명 표만 담는다(비밀·사용자 값 없음). 스크립트가 없어도 모든 폼은 그대로 동작한다.
Response PortalHandler::RenderScript() {
  Out o{body_, sizeof(body_)};
  o.add("(function(){var N={");
  for (size_t i = 0; i < stocks::NameCount(); i++) {
    const stocks::NameEntry& e = stocks::NameAt(i);
    if (strchr(e.name, '"') || strchr(e.name, '\\') || strchr(e.name, '<')) continue;
    char item[96];
    snprintf(item, sizeof(item), "%s\"%s\":\"%s\"", i ? "," : "", e.code, e.name);
    o.add(item);
  }
  o.add("};"
        "var U='\\uc774\\ub984 \\ud45c\\uc5d0 \\uc5c6\\uc74c (\\ucf54\\ub4dc\\ub85c \\ud45c\\uc2dc)';"
        "var B='6\\uc790\\ub9ac \\uc22b\\uc790\\ub97c \\uc785\\ub825\\ud558\\uc138\\uc694';"
        "var d=document.getElementById('stocks');"
        "if(d){for(var k in N){var o=document.createElement('option');o.value=k;o.label=N[k];"
        "o.textContent=N[k];d.appendChild(o);}}"
        "var els=document.querySelectorAll('input[data-nm]');"
        "for(var i=0;i<els.length;i++)(function(e){var t=document.getElementById(e.getAttribute('data-nm'));"
        "if(!t)return;function f(){var v=e.value.replace(/\\s/g,'');"
        "t.textContent=!v?'':(/^[0-9]{6}$/.test(v)?(N[v]||U):B);}"
        "e.addEventListener('input',f);f();})(els[i]);"
        "var pw=document.querySelectorAll('input[type=password]');"
        "for(var j=0;j<pw.length;j++)(function(p){var l=document.createElement('label');"
        "var c=document.createElement('input');c.type='checkbox';"
        "c.addEventListener('change',function(){p.type=c.checked?'text':'password';});"
        "l.appendChild(c);l.appendChild(document.createTextNode(' \\ubcf4\\uc774\\uae30'));"
        "p.parentNode.insertBefore(l,p.nextSibling);})(pw[j]);"
        "var sp=document.querySelector('select[name=ssid_pick]'),"
        "si=document.querySelector('input[name=ssid]');"
        "if(sp&&si)sp.addEventListener('change',function(){if(sp.value)si.value='';});"
        "})();");
  if (o.overflow) return Render(500, "스크립트 구성 오류");
  return Response{200, "application/javascript; charset=utf-8", body_, sizeof(body_), o.used};
}

Response PortalHandler::RenderLogin(int status, const char* message) {
  char esc[160];
  provision::EscapeHtml(message, esc, sizeof(esc));
  snprintf(body_, sizeof(body_),
           "%s<h1>KDASH 설정</h1><p>기기 화면(기기 탭)에 표시된 6자리 페어링 코드를 입력하세요.</p>"
           "<p>%s</p><form method=post action=/login>"
           "<input name=code type=text inputmode=numeric maxlength=6 autocomplete=off>"
           "<button>접속</button></form></body></html>",
           kHead, esc);
  return Response{status, "text/html; charset=utf-8", body_, sizeof(body_), strlen(body_)};
}

Response PortalHandler::RenderMenu() {
  Out o{body_, sizeof(body_)};
  o.add(kHead);
  o.add("<h1>KDASH 설정</h1><p>");
  // 상태 첫 줄(펌웨어·신호·전원)만 메뉴에 보인다. 전체는 /status.
  const char* nl = strchr(status_, '\n');
  char first[sizeof(status_)];
  const size_t fl = nl ? (size_t)(nl - status_) : strlen(status_);
  memcpy(first, status_, fl);
  first[fl] = '\0';
  o.add_text(first);
  o.add("</p>");
  // 현재 설정 요약(비밀은 등록 여부만).
  char line[256];
  o.add("<p><small>");
  o.add("Wi-Fi: ");
  o.add_text(current_.wifi_ssid[0] ? current_.wifi_ssid : "미설정");
  o.add("<br>지역: ");
  o.add_text(current_.location.display_name);
  snprintf(line, sizeof(line), " (격자 %d,%d) · 측정소 ", current_.location.grid_x, current_.location.grid_y);
  o.add_text(line);
  o.add_text(current_.location.air_station);
  o.add("<br>관심종목: ");
  if (current_.watch_count == 0) o.add("없음");
  for (size_t i = 0; i < current_.watch_count && i < kMaxWatchlist; i++) {
    const char* nm = stocks::NameForCode(current_.watchlist[i].symbol);
    snprintf(line, sizeof(line), "%s%s(%s·%s)", i ? ", " : "", nm ? nm : "", current_.watchlist[i].symbol,
             MarketName(current_.watchlist[i].market));
    o.add_text(line);
  }
  snprintf(line, sizeof(line), "<br>키: 기상청 %s · 에어코리아 %s · 수출입은행 %s · KIS %s",
           settings::RegisteredLabel(current_.providers.kma_key),
           settings::RegisteredLabel(current_.providers.air_key),
           settings::RegisteredLabel(current_.providers.exim_key),
           current_.providers.kis_app_key[0] && current_.providers.kis_app_secret[0] ? "등록됨" : "미등록");
  o.add(line);
  const DeviceConfig& d = current_.device;
  if (d.auto_rotate && d.market_hours_only)
    snprintf(line, sizeof(line), "<br>자동 순환: %d분 · 시장은 %02d~%02d시", d.rotate_minutes,
             d.market_from_hour, d.market_to_hour);
  else if (d.auto_rotate)
    snprintf(line, sizeof(line), "<br>자동 순환: %d분 · 시간 제한 없음", d.rotate_minutes);
  else
    snprintf(line, sizeof(line), "<br>자동 순환: 끔");
  o.add(line);
  o.add("</small></p>");
  o.add("<a href=/wifi>Wi-Fi</a><a href=/keys>API 키</a><a href=/location>지역</a>"
        "<a href=/watch>관심종목</a><a href=/device>기기 설정 (자동 순환 등)</a>"
        "<a href=/status>상태 (데이터 수신)</a><a href=/backup>설정 백업·복원 (비밀 제외)</a>");
  const char* actions[][2] = {{"/refresh", "지금 새로고침"}, {"/reboot", "기기 재시작"},
                              {"/ap", "AP 모드로 다시 열기"}, {"/close", nullptr}};
  for (const auto& a : actions) {
    o.add("<form method=post action=");
    o.add(a[0]);
    o.add("><input type=hidden name=csrf value=\"");
    o.add(csrf_);
    o.add("\"><button>");
    o.add(a[1] ? a[1] : (window_sec_ > 0 ? "포털 닫기" : "로그아웃"));
    o.add("</button></form>");
  }
  o.add("<form method=post action=/reset><input type=hidden name=csrf value=\"");
  o.add(csrf_);
  o.add("\"><p>공장초기화: 1단계 RESET, 2단계 RESET-YES</p>"
        "<input name=step1 autocomplete=off><input name=step2 autocomplete=off>"
        "<button>초기화 확인</button></form>");
  o.add(kHttpWarning);
  o.add("</body></html>");
  if (o.overflow) return Render(500, "화면 구성 오류");
  return Response{200, "text/html; charset=utf-8", body_, sizeof(body_), o.used};
}

Response PortalHandler::RenderSections(uint8_t mask, const char* title, const char* sec) {
  Out o{body_, sizeof(body_)};
  char value[256];
  auto input = [&](const char* label, const char* name, const char* v, const char* type) {
    char escaped[6 * kMaxDisplayNameLen + 1];
    provision::EscapeHtml(v, escaped, sizeof(escaped));
    char line[768];
    snprintf(line, sizeof(line),
             "<p>%s <input name=\"%s\" type=\"%s\" value=\"%s\" autocomplete=off></p>", label,
             name, type, escaped);
    o.add(line);
  };
  auto hidden = [&](const char* name, const char* v) { input("", name, v, "hidden"); };
  auto secret = [&](const char* label, const char* name, const char* clear, const char* v) {
    char line[256];
    snprintf(line, sizeof(line), "<p>%s: %s</p>", label, settings::RegisteredLabel(v));
    o.add(line);
    input(label, name, "", "password");
    snprintf(line, sizeof(line), "<label><input type=checkbox name=%s value=1>삭제</label>", clear);
    o.add(line);
  };
  auto number = [&](const char* label, const char* name, int v, int lo, int hi) {
    char line[256];
    snprintf(line, sizeof(line),
             "<p>%s <input name=\"%s\" type=number min=%d max=%d value=\"%d\"></p>", label, name,
             lo, hi, v);
    o.add(line);
  };
  auto check = [&](const char* label, const char* name, bool on) {
    char line[256];
    snprintf(line, sizeof(line), "<p><label><input type=checkbox name=%s value=1 %s>%s</label></p>",
             name, on ? "checked" : "", label);
    o.add(line);
  };
  auto select = [&](const char* label, const char* name, int cur, const char* const* opts, int n) {
    char line[256];
    snprintf(line, sizeof(line), "<p>%s <select name=\"%s\">", label, name);
    o.add(line);
    for (int i = 0; i < n; i++) {
      snprintf(line, sizeof(line), "<option value=%d%s>%s</option>", i, i == cur ? " selected" : "", opts[i]);
      o.add(line);
    }
    o.add("</select></p>");
  };

  o.add(kHead);
  o.add("<h1>");
  o.add(title);
  o.add("</h1><p>빈 비밀 필드는 기존 값 유지. 삭제는 체크하세요.</p>");
  if (sec && (mask & kSecWifi)) {  // 주변 Wi-Fi 검색은 별도 폼(저장 폼 안에 넣을 수 없다)
    o.add("<form method=post action=/wifi/scan><input type=hidden name=csrf value=\"");
    o.add(csrf_);
    o.add("\"><button>주변 Wi-Fi 검색</button></form>");
    switch (scan_state_) {
      case ScanState::Pending:
        o.add("<p>검색 중입니다. 잠시 뒤 이 화면을 새로고침하세요.</p>");
        break;
      case ScanState::Failed:
        o.add("<p>검색하지 못했습니다. 다시 눌러 보세요.</p>");
        break;
      case ScanState::Done:
        if (scan_n_ == 0) o.add("<p>주변 Wi-Fi를 찾지 못했습니다(2.4GHz만 보입니다).</p>");
        break;
      default:
        break;
    }
  }
  o.add("<form method=post action=/save>");
  hidden("csrf", csrf_);
  if (sec) hidden("sec", sec);
  if (mask & kSecWifi) {
    if (sec && scan_n_ > 0) {
      o.add("<p>주변 Wi-Fi <select name=ssid_pick><option value=\"\">직접 입력 (아래 칸)</option>");
      for (size_t i = 0; i < scan_n_; i++) {
        char esc[6 * kMaxSsidLen + 1], line[6 * kMaxSsidLen + 64];
        provision::EscapeHtml(scan_[i].ssid, esc, sizeof(esc));
        snprintf(line, sizeof(line), "<option value=%u>%s (%d dBm%s)</option>", (unsigned)i, esc,
                 (int)scan_[i].rssi, scan_[i].open ? ", 개방" : "");
        o.add(line);
      }
      o.add("</select></p>");
    }
    input("SSID (목록에서 고르면 비워 두세요)", "ssid", current_.wifi_ssid, "text");
    secret("Wi-Fi 비밀번호", "secret", "clear_secret", current_.wifi_secret);
  }
  if (mask & kSecLocation) {
    input("지역", "display", current_.location.display_name, "text");
    input("측정소", "station", current_.location.air_station, "text");
    snprintf(value, sizeof(value), "%d", current_.location.grid_x);
    input("격자 X", "nx", value, "number");
    snprintf(value, sizeof(value), "%d", current_.location.grid_y);
    input("격자 Y", "ny", value, "number");
    if (sec) {
      // 위도·경도를 넣으면 기상청 격자로 바꿔 X/Y 대신 쓴다(지도 앱에서 복사한 값).
      o.add("<p><small>격자를 모르면 위도·경도(예: 37.5665, 126.9780)를 넣으세요. "
            "넣으면 위의 격자 X/Y 대신 계산한 값을 씁니다.</small></p>"
            "<p>위도 <input name=lat type=text inputmode=decimal maxlength=12 autocomplete=off></p>"
            "<p>경도 <input name=lon type=text inputmode=decimal maxlength=12 autocomplete=off></p>");
    }
  }
  if (mask & kSecKeys) {
    hidden("form", "1");
    secret("기상청/공공데이터 키", "kma", "clear_kma", current_.providers.kma_key);
    secret("에어코리아 키", "air", "clear_air", current_.providers.air_key);
    secret("수출입은행 키", "exim", "clear_exim", current_.providers.exim_key);
    secret("KIS 앱키", "kis_key", "clear_kis_key", current_.providers.kis_app_key);
    secret("KIS 앱시크릿", "kis_secret", "clear_kis_secret", current_.providers.kis_app_secret);
    snprintf(value, sizeof(value),
             "<p><label><input type=checkbox name=fsc value=1 %s>종가</label>"
             "<label><input type=checkbox name=kis value=1 %s>KIS</label></p>",
             current_.providers.fsc_enabled ? "checked" : "",
             current_.providers.kis_enabled ? "checked" : "");
    o.add(value);
  }
  if (mask & kSecWatch) {
    value[0] = '\0';
    for (size_t i = 0; i < current_.watch_count && i < kMaxWatchlist; i++) {
      size_t pos = strlen(value);
      snprintf(value + pos, sizeof(value) - pos, "%s%s:%s", i ? "," : "",
               current_.watchlist[i].symbol, MarketName(current_.watchlist[i].market));
    }
    if (sec) {
      // 행 편집기: 종목 코드 6자리 + 시장, 이름은 오프라인 표에서 보여 준다(비우면 삭제).
      static const char* const kMarkets[] = {"KRX", "NXT", "통합"};
      hidden("watch_rows", "1");
      o.add("<p>종목 코드 6자리를 입력하세요(목록에서 이름으로 고를 수도 있습니다). "
            "비우면 삭제됩니다. 최대 4개. 이름 표에 없는 종목은 화면에 코드로 표시됩니다.</p>"
            "<datalist id=stocks></datalist>");
      for (size_t i = 0; i < kMaxWatchlist; i++) {
        const bool have = i < current_.watch_count;
        const char* code = have ? current_.watchlist[i].symbol : "";
        const char* nm = have ? stocks::NameForCode(code) : nullptr;
        char nameattr[16], mktattr[16], escname[160];
        snprintf(nameattr, sizeof(nameattr), "sym%u", (unsigned)i);
        snprintf(mktattr, sizeof(mktattr), "mkt%u", (unsigned)i);
        provision::EscapeHtml(have ? (nm ? nm : "이름 표에 없음 (코드로 표시)") : "", escname, sizeof(escname));
        char line[384];
        // data-nm: 스크립트가 입력에 맞춰 바로 아래 종목명을 바꾼다(스크립트가 없으면 저장된 이름 표시).
        snprintf(line, sizeof(line),
                 "<p>종목 %u <input name=\"%s\" type=text inputmode=numeric maxlength=6 value=\"%s\" "
                 "list=stocks data-nm=nm%u autocomplete=off>",
                 (unsigned)(i + 1), nameattr, code, (unsigned)i);
        o.add(line);
        int cur = 0;
        if (have) for (int m = 0; m < 3; m++) if (strcmp(MarketName(current_.watchlist[i].market), kMarkets[m]) == 0) cur = m;
        select("시장", mktattr, cur, kMarkets, 3);
        snprintf(line, sizeof(line), "<small id=nm%u>%s</small></p>", (unsigned)i, escname);
        o.add(line);
      }
    } else {
      input("관심종목 (예: 005930,000660:NXT)", "watchlist", value, "text");
    }
    hidden("watch_present", "1");
  }
  if (mask & kSecDevice) {
    static const char* const kPages[] = {"생활", "시장", "달력", "기기"};
    static const char* const kLan[] = {"끔 (AP 포털만)", "버튼 (녹색 2초, 10분)", "상시 (USB 전원일 때)"};
    const DeviceConfig& d = current_.device;
    hidden("dev_present", "1");
    select("시작 화면", "page", current_.display_page, kPages, 4);
    check("생활 ↔ 시장 자동 순환 (USB 전원일 때만)", "auto_rotate", d.auto_rotate);
    number("순환 간격(분)", "rotate_min", d.rotate_minutes, kRotateMinMinutes, kRotateMaxMinutes);
    check("시장 화면은 장 시간에만 순환", "market_hours", d.market_hours_only);
    number("장 시작(시, KST)", "market_from", d.market_from_hour, 0, 23);
    number("장 종료(시, 이 시각 전까지)", "market_to", d.market_to_hour, 1, 24);
    number("호출 주기 배율(%)", "refresh_pct", d.refresh_scale_pct, kRefreshScaleMin, kRefreshScaleMax);
    select("설정 포털(같은 Wi-Fi)", "lan_mode", (int)d.lan_mode, kLan, 3);
    check("배터리 보호 (추정 80% 목표)", "charge_protect", d.charge_protect);
    o.add("<p><small>80% 추정에서 충전 중단, 75%에서 재개합니다. 잔량은 전압 기반 추정이며 "
          "배터리 수면 전 충전을 허용하므로 잠든 동안 USB를 꽂으면 충전할 수 있습니다. "
          "기기가 꺼져 있거나 잠든 동안의 80% 제한은 보장하지 않습니다. "
          "지원 충전 칩에서만 동작합니다.</small></p>");
  }
  o.add("<button>저장</button></form>");
  if (sec) {
    if (mask & (kSecWifi | kSecKeys)) o.add(kHttpWarning);
    o.add("<a href=/>메뉴</a>");
  } else {
    // AP 전용 한 페이지 폼: 닫기·초기화를 같은 페이지에 둔다.
    o.add("<p>AP 모드에서는 Wi-Fi 연결 시험에 성공해야 설정이 저장됩니다. "
          "연결할 공유기를 사용할 수 있는 곳에서 저장하세요.</p>");
    o.add("<form method=post action=/close>");
    hidden("csrf", csrf_);
    o.add("<button>닫기</button></form><form method=post action=/reset>");
    hidden("csrf", csrf_);
    input("초기화 1단계: RESET", "step1", "", "text");
    input("초기화 2단계: RESET-YES", "step2", "", "text");
    o.add("<button>초기화 확인</button></form>");
  }
  o.add("<script src=/app.js defer></script></body></html>");
  if (o.overflow) return Render(500, "화면 구성 오류");
  return Response{200, "text/html; charset=utf-8", body_, sizeof(body_), o.used};
}

void PortalHandler::CompleteSave(bool ok) {
  if (ok && has_pending_) { current_ = pending_; saved_ = true; }
  has_pending_ = false;
  save_failed_ = !ok;
}

Response PortalHandler::HandleLogin(const Request& req, int64_t now) {
  const char* code = FindField(req.fields, req.n_fields, "code");
  char token[kSessionTokenHex + 1];
  switch (auth_.TryLogin(code, now, token, sizeof(token))) {
    case LoginResult::Ok: {
      snprintf(cookie_, sizeof(cookie_), "%s=%s; HttpOnly; SameSite=Strict; Path=/; Max-Age=%d",
               kSessionCookie, token, kSessionMaxSec);
      memset(token, 0, sizeof(token));
      provision::IssueCsrfToken(rng_, csrf_, sizeof(csrf_));  // 세션마다 새 CSRF
      Response r = RenderMenu();
      r.set_cookie = cookie_;
      return r;
    }
    case LoginResult::Locked:
      return RenderLogin(429, "시도 횟수를 넘었습니다. 5분 뒤 기기 화면의 새 코드로 다시 시도하세요.");
    default:
      return RenderLogin(200, "코드가 맞지 않습니다.");
  }
}

Response PortalHandler::Handle(const Request& req, int64_t now) {
  if (closed_ || Expired(now)) return Render(410, "포털 시간이 끝났습니다");
  if (req.body_len > kBodyMax || req.n_fields > 40 ||
      (req.n_fields && !req.fields)) return Render(413, "본문이 너무 큽니다");
  size_t aggregate = 0;
  for (size_t i = 0; i < req.n_fields; i++) {
    const char* name = req.fields[i].name;
    const char* value = req.fields[i].value;
    size_t n = name ? strlen(name) : 0, v = value ? strlen(value) : 0;
    const size_t limit = Eq(name, "backup") ? kBackupMax : provision::kFormFieldMax;
    if (!name || n > 32 || v > limit) return Render(413, "필드가 너무 깁니다");
    aggregate += n + v;
    if (aggregate > kBodyMax) return Render(413, "본문이 너무 큽니다");
    for (size_t j = 0; j < i; j++)
      if (Eq(name, req.fields[j].name)) return Render(400, "중복 필드");
  }
  const char* path = req.path ? req.path : "/";
  const bool lan = policy_ == AuthPolicy::LanPaired;
  body_[0] = '\0';

  if (lan) {
    auth_.Tick(now);
    if (!HostAllowed(req.host, hosts_)) return Render(421, "허용되지 않은 주소입니다");
    // 보조 스크립트는 공개 종목명 표뿐이라 로그인 전에도 준다(로그인 화면은 쓰지 않는다).
    if (req.method == Method::Get && Eq(path, "/app.js")) return RenderScript();
    if (req.method == Method::Post && !OriginAllowed(req.origin, req.referer, hosts_))
      return Render(403, "출처 확인 실패");
    if (req.method == Method::Post && Eq(path, "/login")) return HandleLogin(req, now);
    char sid[kSessionTokenHex + 8];
    const bool session = ExtractCookie(req.cookie, kSessionCookie, sid, sizeof(sid)) &&
                         auth_.SessionValid(sid, now);
    memset(sid, 0, sizeof(sid));
    if (session) last_activity_ = now;
    if (!session)
      return RenderLogin(Eq(path, "/") ? 200 : 401,
                         auth_.Locked() ? "잠금 중입니다. 5분 뒤 기기 화면의 새 코드로 다시 시도하세요." : "");
  }

  if (req.method == Method::Get) {
    if (Eq(path, "/")) return lan ? RenderMenu() : RenderSections(kSecAll, "KDASH 설정", nullptr);
    if (Eq(path, "/app.js")) return RenderScript();
    if (Eq(path, "/status")) {
      if (lan && status_[0]) {
        Out o{body_, sizeof(body_)};
        o.add(kHead);
        o.add("<h1>상태</h1><p>");
        o.add_text(status_);
        o.add("</p><p>마지막 저장: ");
        o.add(has_pending_ ? "접속 시험 중" : saved_ ? "저장 완료" : save_failed_ ? "연결 또는 저장 실패: 이전 설정 유지" : "없음");
        o.add("</p><a href=/status>새로고침</a><a href=/>메뉴</a></body></html>");
        if (o.overflow) return Render(500, "화면 구성 오류");
        return Response{200, "text/html; charset=utf-8", body_, sizeof(body_), o.used};
      }
      return Render(200, has_pending_ ? "접속 시험 중" : saved_ ? "저장 완료" : save_failed_ ? "연결 또는 저장 실패: 이전 설정 유지" : "대기 중");
    }
    if (lan && Eq(path, "/backup")) return RenderBackup();
    if (Eq(path, "/export")) {
      const size_t n = ExportBackup(current_, body_, sizeof(body_));
      if (!n) return Render(500, "백업 생성 오류");
      return Response{200, "application/json", body_, sizeof(body_), n};
    }
    if (lan) {
      struct { const char* path; uint8_t mask; const char* title; const char* sec; } pages[] = {
          {"/wifi", kSecWifi, "Wi-Fi", "wifi"}, {"/keys", kSecKeys, "API 키", "keys"},
          {"/location", kSecLocation, "지역", "location"}, {"/watch", kSecWatch, "관심종목", "watch"},
          {"/device", kSecDevice, "기기 설정", "device"}};
      for (const auto& p : pages)
        if (Eq(path, p.path)) return RenderSections(p.mask, p.title, p.sec);
    }
    return Render(404, "없음");
  }

  if (!provision::CheckCsrfToken(req.csrf, csrf_)) return Render(403, "CSRF 오류");
  if (Eq(path, "/close")) {
    if (lan && window_sec_ == 0) {  // 상시 모드: 포털은 계속 대기, 세션만 끝낸다
      auth_.EndSession();
      return Render(200, "로그아웃했습니다");
    }
    closed_ = true;
    return Render(200, "포털을 닫습니다");
  }
  if (lan) {
    if (Eq(path, "/refresh")) { action_ = PortalAction::Refresh; return Render(200, "새로고침을 요청했습니다"); }
    if (Eq(path, "/wifi/scan")) {
      // 검색은 응답을 보낸 뒤 어댑터가 한다(수 초). 결과 화면으로 자동 이동한다.
      action_ = PortalAction::Scan;
      scan_state_ = ScanState::Pending;
      return RenderGo(200, "주변 Wi-Fi를 검색합니다. 잠시 뒤 Wi-Fi 화면으로 이동합니다.", "/wifi",
                      "Wi-Fi 화면으로", 6);
    }
    if (Eq(path, "/import")) return HandleImport(req);
    if (Eq(path, "/reboot")) {
      action_ = PortalAction::Reboot;
      return RenderGo(200, "기기를 다시 시작합니다. 재시작 뒤 기기 화면의 새 코드로 다시 로그인하세요.", "/",
                      "설정으로", 25);
    }
    if (Eq(path, "/ap")) { action_ = PortalAction::OpenAp; return Render(200, "AP 설정 모드로 전환합니다"); }
  }
  if (Eq(path, "/reset")) {
    auto field = [&](const char* name) { return FindField(req.fields, req.n_fields, name); };
    if (!reset_step1_) {
      if (Eq(field("step1"), "RESET")) { reset_step1_ = true; return Render(200, "초기화 확인: RESET-YES를 입력하세요"); }
      return Render(200, "초기화 1단계: RESET 입력");
    }
    if (provision::IsTwoStepResetConfirmed("RESET", field("step2"))) {
      reset_done_ = true; return Render(200, "공장초기화 예약됨");
    }
    return Render(200, "초기화 확인 실패");
  }
  if (!Eq(path, "/save")) return Render(404, "없음");
  return HandleSave(req);
}

Response PortalHandler::RenderBackup() {
  Out o{body_, sizeof(body_)};
  o.add(kHead);
  o.add("<h1>설정 백업·복원</h1><p>지역, 관심종목, 공급자 사용 여부, 기기 설정만 다룹니다. "
        "Wi-Fi 비밀번호와 API 키는 백업에 들어가지 않으며 가져오기로도 바뀌지 않습니다.</p>"
        "<a href=/export>설정 백업 내려받기 (JSON)</a>"
        "<form method=post action=/import><input type=hidden name=csrf value=\"");
  o.add(csrf_);
  o.add("\"><p>백업 JSON 붙여넣기 (1500자 이내)</p>"
        "<textarea name=backup rows=10 style=\"width:100%\" autocomplete=off></textarea>"
        "<button>가져와서 저장</button></form><a href=/>메뉴</a></body></html>");
  if (o.overflow) return Render(500, "화면 구성 오류");
  return Response{200, "text/html; charset=utf-8", body_, sizeof(body_), o.used};
}

Response PortalHandler::HandleImport(const Request& req) {
  if (has_pending_) return Render(409, "접속 시험 중");
  std::unique_ptr<Settings> cand(new (std::nothrow) Settings(current_));
  if (!cand) return Render(503, "메모리 부족");
  char bad[24];
  switch (ImportBackup(FindField(req.fields, req.n_fields, "backup"), *cand, bad, sizeof(bad))) {
    case ImportResult::Ok:
      break;
    case ImportResult::BadFormat:
      return Render(200, "가져오기 오류: 형식 또는 버전이 맞지 않습니다");
    case ImportResult::BadValue: {
      char msg[160];
      snprintf(msg, sizeof(msg), "가져오기 오류: %s 값을 확인하세요. 아무 것도 바꾸지 않았습니다.", bad);
      return Render(200, msg);
    }
    default:
      return Render(200, "가져오기 오류: JSON 형식이 아닙니다");
  }
  pending_ = *cand; has_pending_ = true; save_failed_ = false; saved_ = false;
  pending_wifi_changed_ = false;  // Wi-Fi는 가져오지 않는다
  return RenderGo(200, "가져온 설정을 저장합니다. 기기가 곧 다시 시작됩니다. 재시작 뒤 기기 화면의 새 코드로 다시 로그인하세요.",
                  "/", "설정으로", 25);
}

Response PortalHandler::HandleSave(const Request& req) {
  if (has_pending_) return Render(409, "접속 시험 중");
  auto field = [&](const char* name) { return FindField(req.fields, req.n_fields, name); };
  auto present = [&](const char* name) { return HasField(req.fields, req.n_fields, name); };
  uint8_t mask = kSecAll;
  if (present("sec")) {
    mask = SectionMask(field("sec"));
    if (!mask) return Render(400, "알 수 없는 구역");
  } else if (policy_ == AuthPolicy::LanPaired) {
    return Render(400, "구역이 필요합니다");  // LAN에서는 전체 덮어쓰기 금지
  }
  std::unique_ptr<Settings> candidate(new (std::nothrow) Settings(current_));
  if (!candidate) return Render(503, "메모리 부족");
  Settings& c = *candidate;
  char err[96];

  bool picked_open_wifi = false;
  if (mask & kSecWifi) {
    // 직접 입력이 새로 바뀌었으면 우선. 비어 있거나 기존 SSID 기본값 그대로인데 검색 목록을 골랐으면
    // 목록에서 고른 번호를 쓴다(이름은 서버가 가진 값만 사용, ISSUE-011).
    const char* ssid_in = field("ssid");
    if ((!*ssid_in || Eq(ssid_in, current_.wifi_ssid)) && *field("ssid_pick")) {
      int idx = 0;
      if (!ParseInt(field("ssid_pick"), 0, (int)kMaxScan - 1, idx) || (size_t)idx >= scan_n_)
        return Render(200, "SSID 오류: 목록 선택");
      ssid_in = scan_[idx].ssid;
      picked_open_wifi = scan_[idx].open;
    }
    if (!provision::ValidateField(provision::Field::Ssid, ssid_in, err, sizeof(err)))
      return Render(200, "SSID 오류");
    settings::CopyBounded(c.wifi_ssid, sizeof(c.wifi_ssid), ssid_in);
  }
  struct SecretField { uint8_t sec; const char* name; const char* clear; char* dst; size_t cap; provision::Field type; };
  SecretField secrets[] = {
    {kSecWifi, "secret", "clear_secret", c.wifi_secret, sizeof(c.wifi_secret), provision::Field::WifiSecret},
    {kSecKeys, "kma", "clear_kma", c.providers.kma_key, sizeof(c.providers.kma_key), provision::Field::KmaKey},
    {kSecKeys, "air", "clear_air", c.providers.air_key, sizeof(c.providers.air_key), provision::Field::AirKey},
    {kSecKeys, "exim", "clear_exim", c.providers.exim_key, sizeof(c.providers.exim_key), provision::Field::EximKey},
    {kSecKeys, "kis_key", "clear_kis_key", c.providers.kis_app_key, sizeof(c.providers.kis_app_key), provision::Field::KisAppKey},
    {kSecKeys, "kis_secret", "clear_kis_secret", c.providers.kis_app_secret, sizeof(c.providers.kis_app_secret), provision::Field::KisAppSecret}};
  for (const auto& s : secrets) {
    if (!(mask & s.sec)) continue;
    if (Eq(field(s.clear), "1")) s.dst[0] = '\0';
    else if (*field(s.name)) {
      if (!provision::ValidateField(s.type, field(s.name), err, sizeof(err)) ||
          !settings::CopyBounded(s.dst, s.cap, field(s.name))) return Render(200, "비밀 필드 입력 오류");
    }
  }
  if (picked_open_wifi && !*field("secret")) c.wifi_secret[0] = '\0';  // 개방형 Wi-Fi 선택 시 이전 비밀번호 제거(G-9)
  if (mask & kSecLocation) {
    if (present("display") && (!provision::ValidateField(provision::Field::DisplayName,
        field("display"), err, sizeof(err)) || !settings::CopyBounded(c.location.display_name,
        sizeof(c.location.display_name), field("display")))) return Render(200, "지역 오류");
    if (present("station") && (!provision::ValidateField(provision::Field::AirStation,
        field("station"), err, sizeof(err)) || !settings::CopyBounded(c.location.air_station,
        sizeof(c.location.air_station), field("station")))) return Render(200, "측정소 오류");
    for (const char* name : {"nx", "ny"}) {
      if (!present(name)) continue;
      if (!provision::ValidateField(Eq(name, "nx") ? provision::Field::GridX : provision::Field::GridY,
                                    field(name), err, sizeof(err))) return Render(200, "격자 오류");
      (Eq(name, "nx") ? c.location.grid_x : c.location.grid_y) = atoi(field(name));
    }
    // 위도·경도가 둘 다 있으면 격자를 계산해 덮어쓴다. 하나만 있으면 오류(반쪽 입력을 조용히 무시하지 않음).
    const bool has_lat = *field("lat"), has_lon = *field("lon");
    if (has_lat || has_lon) {
      double lat = 0, lon = 0;
      int gx = 0, gy = 0;
      if (!has_lat || !has_lon) return Render(200, "위치 오류: 위도와 경도를 모두 넣으세요");
      if (!ParseDecimal(field("lat"), lat) || !ParseDecimal(field("lon"), lon))
        return Render(200, "위치 오류: 숫자(예: 37.5665)로 넣으세요");
      if (!geo::LatLonToGrid(lat, lon, gx, gy))
        return Render(200, "위치 오류: 한국 범위(위도 28~44, 경도 120~134)가 아닙니다");
      c.location.grid_x = gx;
      c.location.grid_y = gy;
    }
    if (settings::ValidateLocation(c.location.grid_x, c.location.grid_y,
        c.location.display_name, c.location.air_station) != settings::Validation::Ok)
      return Render(200, "지역 또는 측정소 오류");
  }
  if ((mask & kSecWifi) &&
      settings::ValidateWifi(c.wifi_ssid, c.wifi_secret) != settings::Validation::Ok)
    return Render(200, "Wi-Fi 비밀번호 오류");
  if ((mask & kSecKeys) && present("form")) {
    c.providers.fsc_enabled = Eq(field("fsc"), "1");
    c.providers.kis_enabled = Eq(field("kis"), "1");
  }
  if ((mask & kSecWatch) && present("watch_rows")) {
    // 행 편집기 입력을 "코드:시장,..." 목록으로 모아 기존 검증(ParseWatchlist)을 그대로 쓴다.
    static const char* const kMarkets[] = {"KRX", "NXT", "통합"};
    char list[kMaxWatchlist * 16 + 1];
    size_t used = 0;
    const char* seen[kMaxWatchlist];
    size_t n = 0;
    for (size_t i = 0; i < kMaxWatchlist; i++) {
      char sn[16], mn[16];
      snprintf(sn, sizeof(sn), "sym%u", (unsigned)i);
      snprintf(mn, sizeof(mn), "mkt%u", (unsigned)i);
      const char* sym = field(sn);
      if (!*sym) continue;
      int m = 0;
      if (!ParseInt(field(mn), 0, 2, m)) return Render(200, "관심종목 오류: 시장");
      for (size_t k = 0; k < n; k++)
        if (strcmp(seen[k], sym) == 0) return Render(200, "관심종목 오류: 같은 종목이 두 번 있습니다");
      if (strlen(sym) != kMaxSymbolLen) return Render(200, "관심종목 오류: 종목 코드는 6자리 숫자입니다");
      int w = snprintf(list + used, sizeof(list) - used, "%s%s:%s", n ? "," : "", sym, kMarkets[m]);
      if (w < 0 || used + (size_t)w >= sizeof(list)) return Render(200, "관심종목 오류");
      used += (size_t)w;
      seen[n++] = sym;
    }
    list[used] = '\0';
    if (settings::ParseWatchlist(list, c.watchlist, kMaxWatchlist, c.watch_count) != settings::Validation::Ok)
      return Render(200, "관심종목 오류: 종목 코드는 6자리 숫자입니다");
  } else if ((mask & kSecWatch) && (present("watch_present") || *field("watchlist"))) {
    if (settings::ParseWatchlist(field("watchlist"), c.watchlist, kMaxWatchlist, c.watch_count) != settings::Validation::Ok)
      return Render(200, "관심종목 오류");
  }
  if ((mask & kSecDevice) && present("dev_present")) {
    DeviceConfig& d = c.device;
    int v = 0;
    if (!ParseInt(field("page"), 0, 3, v)) return Render(200, "기기 설정 오류: 시작 화면");
    c.display_page = v;
    if (!ParseInt(field("lan_mode"), 0, 2, v)) return Render(200, "기기 설정 오류: 포털 모드");
    d.lan_mode = (LanPortalMode)v;
    d.auto_rotate = Eq(field("auto_rotate"), "1");
    d.market_hours_only = Eq(field("market_hours"), "1");
    d.charge_protect = Eq(field("charge_protect"), "1");
    if (!ParseInt(field("rotate_min"), kRotateMinMinutes, kRotateMaxMinutes, d.rotate_minutes))
      return Render(200, "기기 설정 오류: 순환 간격(1~60분)");
    if (!ParseInt(field("market_from"), 0, 23, d.market_from_hour) ||
        !ParseInt(field("market_to"), 1, 24, d.market_to_hour))
      return Render(200, "기기 설정 오류: 장 시간");
    if (!ParseInt(field("refresh_pct"), kRefreshScaleMin, kRefreshScaleMax, d.refresh_scale_pct))
      return Render(200, "기기 설정 오류: 호출 주기 배율(100~400)");
    if (settings::ValidateDevice(d) != settings::Validation::Ok)
      return Render(200, "기기 설정 오류: 장 시작은 종료보다 빨라야 합니다");
  }
  pending_wifi_changed_ = strcmp(c.wifi_ssid, current_.wifi_ssid) != 0 ||
                          strcmp(c.wifi_secret, current_.wifi_secret) != 0;
  pending_ = c; has_pending_ = true; save_failed_ = false; saved_ = false;
  if (policy_ == AuthPolicy::LanPaired) {
    if (pending_wifi_changed_)
      return Render(200, "새 Wi-Fi로 시험 접속합니다. 결과는 기기 화면에 표시됩니다. 성공하면 휴대폰도 같은 Wi-Fi로 옮긴 뒤 기기 탭의 주소로 접속하세요.");
    // 무엇이 저장되는지 확인할 수 있게 요약한다(관심종목은 종목명 포함).
    char msg[480];
    size_t used = (size_t)snprintf(msg, sizeof(msg), "저장합니다. 기기가 곧 다시 시작됩니다.");
    if ((mask & kSecWatch) && used < sizeof(msg)) {
      used += (size_t)snprintf(msg + used, sizeof(msg) - used, " 관심종목: %s", c.watch_count ? "" : "없음");
      for (size_t i = 0; i < c.watch_count && i < kMaxWatchlist && used < sizeof(msg); i++) {
        const char* nm = stocks::NameForCode(c.watchlist[i].symbol);
        used += (size_t)snprintf(msg + used, sizeof(msg) - used, "%s%s(%s·%s)", i ? ", " : "",
                                 nm ? nm : "이름 표에 없음", c.watchlist[i].symbol,
                                 MarketName(c.watchlist[i].market));
      }
    }
    if ((mask & kSecLocation) && used < sizeof(msg))
      snprintf(msg + used, sizeof(msg) - used, " 격자: %d, %d", c.location.grid_x, c.location.grid_y);
    return RenderGo(200, msg, "/", "설정으로", 25);
  }
  return Render(200, "접속 시험 중");
}
}  // namespace portal
}  // namespace kdash
