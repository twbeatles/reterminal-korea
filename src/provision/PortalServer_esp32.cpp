#include "PortalServer.h"
#include "LanPortal.h"

#if defined(ARDUINO) && defined(KDASH_BOARD_E1001)
#include <Arduino.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include <esp_random.h>
#include <lwip/sockets.h>
#include <memory>
#include <new>
#include <string.h>
#include <time.h>

#include "LanPolicy.h"
#include "PortalHandler.h"
#include "SetupPortal.h"
#include "../storage/SettingsStore.h"
#include "../drivers/Watchdog.h"
#include "../boot/SerialLog.h"

#if CORE_DEBUG_LEVEL > 0
#error "Portal builds require CORE_DEBUG_LEVEL=0: SDK form debug logs contain secrets"
#endif

namespace kdash { namespace portal {
namespace {
class HardwareRandom : public IRandom {
 public: uint32_t next_u32() override { return esp_random(); }
};

// Arduino-ESP32 3.3.12's WebServer parses the body before middleware.
// Its protected current client/server API and NetworkClient::fd are checked
// against the locally pinned SDK. Peek before parsing to bound allocation,
// reject multipart/chunked bodies and keep credentials out of URI logging.
class BoundedServer : public WebServer {
 public:
  explicit BoundedServer(IPAddress bind) : WebServer(bind, 80), bind_(bind) {}
  void Poll() {
    if (_currentStatus == HC_NONE) {
      _currentClient = _server.accept();
      if (!_currentClient) return;
      _currentStatus = HC_WAIT_READ; _statusChange = millis();
    }
    if (_currentClient.localIP() != bind_) { Drop(); return; }
    char header[2049];
    int got = recv(_currentClient.fd(), header, 2048, MSG_PEEK | MSG_DONTWAIT);
    if (got < 0) {
      if (!_currentClient.connected() || millis() - _statusChange > 5000) Drop();
      return;
    }
    if (got == 0) { Drop(); return; }
    int status = CheckPortalHeaders(header, (size_t)got);
    if (!status) {
      if (millis() - _statusChange > 5000) Drop();
      return;
    }
    if (status != 200) {
      char response[256];
      int n = snprintf(response, sizeof(response),
          "HTTP/1.1 %d Rejected\r\nContent-Length: 0\r\nConnection: close\r\n"
          "Cache-Control: no-store\r\nX-Content-Type-Options: nosniff\r\n\r\n", status);
      _currentClient.write((const uint8_t*)response, (size_t)n);
      Drop(); return;
    }
    NetworkClient held = _currentClient;
    WebServer::handleClient();
    held.stop();
    _currentStatus = HC_NONE;
  }
 private:
  IPAddress bind_;
  void Drop() { _currentClient.stop(); _currentStatus = HC_NONE; }
};

// AP/LAN 공통: 한 요청을 PortalHandler로 넘기고 보안 헤더를 붙여 응답한다.
void ServeRequest(BoundedServer& server, PortalHandler& handler, uint32_t now_sec) {
  server.sendHeader("Cache-Control", "no-store");
  server.sendHeader("X-Content-Type-Options", "nosniff");
  // same-origin: 같은 사이트 폼 POST는 실제 Origin을 보낸다(no-referrer면 브라우저가 "Origin: null"을 보낸다).
  server.sendHeader("Referrer-Policy", "same-origin");
  server.sendHeader("Content-Security-Policy",
      "default-src 'none'; script-src 'self'; style-src 'unsafe-inline'; form-action 'self'; "
      "frame-ancestors 'none'; base-uri 'none'");
  if (server.args() > 40) { server.send(413, "text/plain", "Too many fields"); return; }
  struct Fields {
    String names[40], values[40];
    FormField fields[40];
  };
  std::unique_ptr<Fields> form(new (std::nothrow) Fields);
  if (!form) { server.send(503, "text/plain", "Unavailable"); return; }
  const size_t count = (size_t)server.args();
  for (size_t i = 0; i < count; i++) {
    form->names[i] = server.argName(i); form->values[i] = server.arg(i);
    form->fields[i] = {form->names[i].c_str(), form->values[i].c_str()};
  }
  String path = server.uri();
  String csrf = server.arg("csrf");
  String host = server.hostHeader(), origin = server.header("Origin");
  String referer = server.header("Referer"), cookie = server.header("Cookie");
  Request req;
  req.method = server.method() == HTTP_POST ? Method::Post : Method::Get;
  req.path = path.c_str(); req.fields = form->fields; req.n_fields = count;
  req.csrf = csrf.c_str(); req.body_len = (size_t)server.clientContentLength();
  req.host = host.length() ? host.c_str() : nullptr;
  req.origin = origin.length() ? origin.c_str() : nullptr;
  req.referer = referer.length() ? referer.c_str() : nullptr;
  req.cookie = cookie.length() ? cookie.c_str() : nullptr;
  Response response = handler.Handle(req, now_sec);
  if (response.set_cookie) server.sendHeader("Set-Cookie", response.set_cookie);
  server.send(response.status, response.content_type, String(response.body));
  // Arduino String buffers are mutable in the pinned SDK. Scrub local copies of
  // secret fields and the session cookie after sending; WebServer owns its own buffers.
  for (size_t i = 0; i < count; i++)
    for (size_t j = 0; j < form->values[i].length(); j++) form->values[i].setCharAt(j, '\0');
  for (size_t j = 0; j < cookie.length(); j++) cookie.setCharAt(j, '\0');
}
const char* kHeaderKeys[] = {"Content-Length", "Origin", "Referer", "Cookie"};

bool ConnectTest(const Settings& candidate, BoundedServer& server,
                 PortalHandler& handler, uint32_t opened) {
  WiFi.disconnect(false, false);
  WiFi.begin(candidate.wifi_ssid, candidate.wifi_secret);
  const uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000 &&
         millis() - opened < (uint32_t)kWindowSec * 1000 && !handler.Closed()) {
    wdt::Feed(); server.Poll(); delay(10);
  }
  return !handler.Closed() && WiFi.status() == WL_CONNECTED;
}

bool ResetOwned(IStorage& storage, Settings& current) {
  // A default-settings tombstone suppresses compiled dev credentials on boot.
  // Overwrite both slots; never erase an entire namespace or flash chip.
  std::unique_ptr<Settings> defaults(new (std::nothrow) Settings);
  if (!defaults || !SaveSettingsVerified(storage, current, *defaults) ||
      !SaveSettingsVerified(storage, current, *defaults)) return false;
  for (const char* key : {"run.a", "run.b", "run.meta", "cal.a", "cal.b", "cal.meta"})
    storage.erase(key);
  return true;
}
}  // namespace

PortalResult RunPortal(Settings& current, IStorage& storage, ShowPortal show) {
  if (!show) return PortalResult::DisplayUnavailable;
  WiFi.mode(WIFI_AP_STA);  // RF must be enabled before hardware RNG use.
  HardwareRandom rng;
  char suffix[5], ap[32], password[provision::kApPasswordLen + 1];
  snprintf(suffix, sizeof(suffix), "%04X", (unsigned)(rng.next_u32() & 0xFFFF));
  if (!provision::MakeApName(suffix, ap, sizeof(ap)) ||
      !provision::MakeApPassword(rng, password, sizeof(password))) return PortalResult::Failed;
  const char* url = "http://192.168.4.1/";  // local setup AP only; no API downgrade
  if (!show(ap, password, url)) {
    memset(password, 0, sizeof(password)); WiFi.mode(WIFI_STA);
    return PortalResult::DisplayUnavailable;
  }
  if (!WiFi.softAP(ap, password, 1, false, 1)) {
    memset(password, 0, sizeof(password)); WiFi.mode(WIFI_STA);
    return PortalResult::Failed;
  }
  memset(password, 0, sizeof(password));
  const uint32_t opened = millis();
  std::unique_ptr<PortalHandler> handler(new (std::nothrow) PortalHandler(current, rng, 0));
  std::unique_ptr<BoundedServer> server(new (std::nothrow) BoundedServer(IPAddress(192, 168, 4, 1)));
  if (!handler || !server) { WiFi.softAPdisconnect(true); return PortalResult::Failed; }
  server->collectHeaders(kHeaderKeys, sizeof(kHeaderKeys) / sizeof(kHeaderKeys[0]));
  PortalResult result = PortalResult::Closed;
  server->onNotFound([&]() { ServeRequest(*server, *handler, (millis() - opened) / 1000); });
  server->begin();
  while (millis() - opened < (uint32_t)kWindowSec * 1000 && !handler->Closed()) {
    wdt::Feed();  // bounded portal progress, never feed from a separate timer task
    server->Poll();
    if (handler->ResetConfirmed()) {
      result = ResetOwned(storage, current) ? PortalResult::Reset : PortalResult::Failed;
      break;
    }
    if (handler->HasPendingSave()) {
      bool ok = ConnectTest(handler->Pending(), *server, *handler, opened) &&
          SaveSettingsVerified(storage, current, handler->Pending());
      handler->CompleteSave(ok);
      if (ok) { result = PortalResult::Saved; break; }
      WiFi.disconnect(false, false);
      if (current.wifi_ssid[0]) WiFi.begin(current.wifi_ssid, current.wifi_secret);
    }
    delay(5);
  }
  server->close(); WiFi.softAPdisconnect(false); WiFi.mode(WIFI_STA);
  if (result != PortalResult::Saved && result != PortalResult::Reset && current.wifi_ssid[0])
    WiFi.begin(current.wifi_ssid, current.wifi_secret);
  return result;
}

// ---------------------------------------------------------------------------
// LAN(같은 Wi-Fi) 설정 포털 어댑터 — ADR 0024. 논블로킹, loop()에서 Poll().
// ---------------------------------------------------------------------------
struct LanPortal::Impl {
  HardwareRandom rng;
  Settings* current = nullptr;
  IStorage* storage = nullptr;
  std::unique_ptr<PortalHandler> handler;
  std::unique_ptr<BoundedServer> server;
  uint32_t opened_ms = 0;
  int window_sec = 0;
  IPAddress ip;
  char ip_str[16] = {0};
  char host[24] = {0};  // mDNS 등록에 성공했을 때만 채운다
  bool mdns = false;
  bool changed = false;
};

namespace {
void FormatIp(const IPAddress& ip, char* out, size_t cap) {
  snprintf(out, cap, "%u.%u.%u.%u", (unsigned)ip[0], (unsigned)ip[1], (unsigned)ip[2], (unsigned)ip[3]);
}

bool StartLanServer(LanPortal::Impl& m) {
  m.server.reset(new (std::nothrow) BoundedServer(m.ip));
  if (!m.server) return false;
  m.server->collectHeaders(kHeaderKeys, sizeof(kHeaderKeys) / sizeof(kHeaderKeys[0]));
  LanPortal::Impl* im = &m;
  m.server->onNotFound([im]() { ServeRequest(*im->server, *im->handler, (millis() - im->opened_ms) / 1000); });
  m.server->begin();
  return true;
}

// 새 Wi-Fi로 바꾸는 저장: 20초 접속 시험(블로킹). 휴대폰은 이 동안 끊긴다(응답은 이미 나갔다).
bool SwitchWifiTest(const Settings& candidate) {
  WiFi.disconnect(false, false);
  WiFi.begin(candidate.wifi_ssid, candidate.wifi_secret);
  const uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) { wdt::Feed(); delay(10); }
  return WiFi.status() == WL_CONNECTED;
}
}  // namespace

LanPortal::LanPortal() = default;
LanPortal::~LanPortal() { Close(); }

bool LanPortal::Open(Settings& current, IStorage& storage, int window_sec) {
  if (impl_) return true;
  if (WiFi.status() != WL_CONNECTED) return false;
  std::unique_ptr<Impl> m(new (std::nothrow) Impl);
  if (!m) return false;
  m->current = &current; m->storage = &storage;
  m->window_sec = window_sec; m->opened_ms = millis();
  m->ip = WiFi.localIP(); FormatIp(m->ip, m->ip_str, sizeof(m->ip_str));
  uint8_t mac[6] = {0};
  WiFi.macAddress(mac);
  char name[24];
  snprintf(name, sizeof(name), "kdash-%02x%02x", (unsigned)mac[4], (unsigned)mac[5]);
  m->mdns = MDNS.begin(name);
  if (m->mdns) { MDNS.addService("http", "tcp", 80); snprintf(m->host, sizeof(m->host), "%s", name); }
  m->handler.reset(new (std::nothrow) PortalHandler(current, m->rng, 0));
  if (!m->handler) { if (m->mdns) MDNS.end(); return false; }
  AllowedHosts hosts;
  snprintf(hosts.ip, sizeof(hosts.ip), "%s", m->ip_str);
  snprintf(hosts.mdns, sizeof(hosts.mdns), "%s", m->host);
  m->handler->UseLan(hosts, window_sec);
  if (!StartLanServer(*m)) { if (m->mdns) MDNS.end(); return false; }
  m->changed = true;  // 첫 코드 표시
  impl_ = std::move(m);
  return true;
}

void LanPortal::Close() {
  if (!impl_) return;
  if (impl_->server) impl_->server->close();
  if (impl_->mdns) MDNS.end();
  impl_.reset();
}

bool LanPortal::TakeDisplayChanged() {
  if (!impl_) return false;
  const bool c = impl_->changed;
  impl_->changed = false;
  return c;
}

bool LanPortal::Info(LanInfo& out) const {
  if (!impl_) return false;
  snprintf(out.ip, sizeof(out.ip), "%s", impl_->ip_str);
  snprintf(out.host, sizeof(out.host), "%s", impl_->host);
  snprintf(out.code, sizeof(out.code), "%s", impl_->handler->PairingCode());
  out.locked = impl_->handler->PairingLocked();
  const int left = impl_->handler->RemainingSec((millis() - impl_->opened_ms) / 1000);
  out.remaining_min = left < 0 ? -1 : (left + 59) / 60;
  return true;
}

LanEvent LanPortal::Poll(const char* status_line) {
  if (!impl_) return LanEvent::None;
  Impl& m = *impl_;
  // 창 만료/닫힘은 Wi-Fi 상태보다 먼저 본다(끊긴 동안에도 창은 끝난다, ISSUE-007).
  {
    const uint32_t t = (millis() - m.opened_ms) / 1000;
    m.handler->Tick(t);
    if (m.handler->TakePairingDisplayChanged()) m.changed = true;
    const LanGate gate = DecideLanGate(m.handler->WindowExpired(t), m.handler->Closed(),
                                       WiFi.status() == WL_CONNECTED);
    if (gate == LanGate::Close) return LanEvent::Closed;
    if (gate == LanGate::WaitLink) return LanEvent::None;  // 재연결은 WifiSupervisor/드라이버 몫
  }
  const IPAddress now_ip = WiFi.localIP();
  if (now_ip != m.ip) {  // DHCP 재할당: 서버를 새 주소로 다시 묶고 허용 Host도 갱신
    m.server->close();
    m.ip = now_ip; FormatIp(m.ip, m.ip_str, sizeof(m.ip_str));
    AllowedHosts hosts;
    snprintf(hosts.ip, sizeof(hosts.ip), "%s", m.ip_str);
    snprintf(hosts.mdns, sizeof(hosts.mdns), "%s", m.host);
    m.handler->SetHosts(hosts);
    if (!StartLanServer(m)) return LanEvent::Closed;
    m.changed = true;
  }
  const uint32_t now = (millis() - m.opened_ms) / 1000;
  m.handler->SetStatusLine(status_line);
  m.server->Poll();
  m.handler->Tick(now);
  if (m.handler->TakePairingDisplayChanged()) m.changed = true;
  if (m.handler->WindowExpired(now)) return LanEvent::Closed;  // 마지막 활동 기준 10분
  if (m.handler->Closed()) return LanEvent::Closed;
  if (m.handler->ResetConfirmed())
    return ResetOwned(*m.storage, *m.current) ? LanEvent::Reset : LanEvent::SaveFailed;
  if (m.handler->HasPendingSave()) {
    const Settings& pending = m.handler->Pending();
    const bool wifi_changed = m.handler->PendingWifiChanged();
    if (wifi_changed) m.server->close();
    bool ok = !wifi_changed || SwitchWifiTest(pending);
    ok = ok && SaveSettingsVerified(*m.storage, *m.current, pending);
    m.handler->CompleteSave(ok);
    if (ok) return LanEvent::Saved;
    if (wifi_changed) {  // 이전 Wi-Fi로 복귀하고 포털을 계속 연다(이전 설정 유지)
      WiFi.disconnect(false, false);
      if (m.current->wifi_ssid[0]) WiFi.begin(m.current->wifi_ssid, m.current->wifi_secret);
      const uint32_t start = millis();
      while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) { wdt::Feed(); delay(10); }
      if (WiFi.status() == WL_CONNECTED) {
        m.ip = WiFi.localIP(); FormatIp(m.ip, m.ip_str, sizeof(m.ip_str));
        AllowedHosts hosts;
        snprintf(hosts.ip, sizeof(hosts.ip), "%s", m.ip_str);
        snprintf(hosts.mdns, sizeof(hosts.mdns), "%s", m.host);
        m.handler->SetHosts(hosts);
        if (!StartLanServer(m)) return LanEvent::Closed;
        m.changed = true;
      } else {
        // 20초 안에 복귀하지 못했으면 0.0.0.0으로 바인딩·화면 갱신하지 않고, 재연결 시 위 DHCP 분기에 맡긴다(ISSUE-014).
        m.ip = IPAddress(0U, 0U, 0U, 0U);
      }
    }
    return LanEvent::SaveFailed;
  }
  switch (m.handler->TakeAction()) {
    case PortalAction::Refresh: return LanEvent::Refresh;
    case PortalAction::Reboot: return LanEvent::Reboot;
    case PortalAction::OpenAp: return LanEvent::OpenAp;
    case PortalAction::Scan: {
      // 응답은 이미 나갔다. 블로킹 검색(약 2~5초) 뒤 결과를 핸들러에 넣는다.
      wdt::Feed();
      int found = WiFi.scanNetworks(false, false);
      if (found < 0) {  // WIFI_SCAN_FAILED/RUNNING: 남은 결과를 지우고 한 번만 다시 시도
        WiFi.scanDelete();
        delay(200);
        wdt::Feed();
        found = WiFi.scanNetworks(false, false);
      }
      wdt::Feed();
      KDASH_LOG.printf("portal: wifi scan found=%d\n", found);  // 개수만 기록(SSID는 기록하지 않음)
      if (found < 0) {
        m.handler->SetScanResults(nullptr, 0, true);
        WiFi.scanDelete();
        return LanEvent::None;
      }
      const size_t cap = 40;
      std::unique_ptr<ScanEntry[]> list(new (std::nothrow) ScanEntry[cap]);
      if (list && found > 0) {
        size_t n = 0;
        for (int i = 0; i < found && n < cap; i++) {
          String name = WiFi.SSID(i);
          if (!name.length() || name.length() > kMaxSsidLen) continue;
          ScanEntry e;
          snprintf(e.ssid, sizeof(e.ssid), "%s", name.c_str());
          const int rssi = WiFi.RSSI(i);
          e.rssi = (int8_t)(rssi < -127 ? -127 : rssi > 0 ? 0 : rssi);
          e.open = WiFi.encryptionType(i) == WIFI_AUTH_OPEN;
          size_t k = n++;  // 신호 세기 순으로 삽입
          while (k > 0 && list[k - 1].rssi < e.rssi) { list[k] = list[k - 1]; k--; }
          list[k] = e;
        }
        m.handler->SetScanResults(list.get(), n);
      } else {
        m.handler->SetScanResults(nullptr, 0);
      }
      WiFi.scanDelete();
      return LanEvent::None;
    }
    default: break;
  }
  return LanEvent::None;
}
} }
#endif
