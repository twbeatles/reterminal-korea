#pragma once
// PortalHandler: device-web-server-independent request handling (T07, ADR 0024).
// Routes + CSRF + validation + HTML escaping + pending save + 2-step
// reset + window + 8KB/256B limits. Portable, host-tested.
//
// 두 가지 접근 정책
//  - ApTrusted (SoftAP): 무작위 WPA2 비밀번호를 아는 사람 = 신뢰. 로그인 없음. 한 페이지 전체 폼.
//  - LanPaired (같은 Wi-Fi): Host 허용목록 + Origin 검사 + 화면 페어링 코드 → 세션 쿠키.
//    메뉴/구역별 폼. 모든 상태 변경은 POST + CSRF.
#include <stddef.h>
#include <stdint.h>

#include "../../include/Config.h"
#include "../../include/IRandom.h"
#include "PortalAuth.h"
#include "RequestGuard.h"

namespace kdash {
namespace portal {

enum class Method : uint8_t { Get = 0, Post };

struct FormField {
  const char* name = nullptr;
  const char* value = nullptr;
};

struct Request {
  Method method = Method::Get;
  const char* path = "/";
  const FormField* fields = nullptr;
  size_t n_fields = 0;
  const char* csrf = nullptr;
  size_t body_len = 0;
  // LAN 정책에서만 쓰는 헤더(어댑터가 채움). 없으면 nullptr.
  const char* host = nullptr;
  const char* origin = nullptr;
  const char* referer = nullptr;
  const char* cookie = nullptr;
};

struct Response {
  int status = 200;
  const char* content_type = "text/html; charset=utf-8";
  char* body = nullptr;  // caller-owned buffer
  size_t body_cap = 0;
  size_t len = 0;
  const char* set_cookie = nullptr;  // 핸들러 소유 버퍼. 다음 Handle 호출 전까지 유효.
};

enum class AuthPolicy : uint8_t { ApTrusted = 0, LanPaired };
// 세션이 요청한 기기 동작. 어댑터가 가져가 실행한다(핸들러는 부수효과가 없다).
enum class PortalAction : uint8_t { None = 0, Refresh, Reboot, OpenAp, Scan };

// 주변 Wi-Fi 검색 결과 한 줄(어댑터가 채움). 목록 화면에서 이름은 번호로만 고른다.
struct ScanEntry {
  char ssid[kMaxSsidLen + 1] = {0};
  int8_t rssi = 0;
  bool open = false;
};
inline constexpr size_t kMaxScan = 15;
enum class ScanState : uint8_t { Idle = 0, Pending, Done, Failed };

inline constexpr size_t kBodyMax = 8192;
inline constexpr int kWindowSec = 600;
inline constexpr const char* kSessionCookie = "kdash_sid";

class PortalHandler {
 public:
  PortalHandler(const Settings& current, IRandom& rng, int64_t opened_at);
  Response Handle(const Request& req, int64_t now_utc);

  // LAN 정책으로 전환. window_sec=0이면 만료 없음(상시 모드: /close는 로그아웃).
  void UseLan(const AllowedHosts& hosts, int window_sec);
  // 허용 호스트(IP)가 바뀌었을 때 갱신(DHCP 재할당).
  void SetHosts(const AllowedHosts& hosts) { hosts_ = hosts; }
  // 메뉴/상태 화면에 보일 한 줄(펌웨어·신호·전원 등). 비밀 금지.
  void SetStatusLine(const char* line);
  // 주변 Wi-Fi 검색 결과(최대 15개, 신호 순). 어댑터가 Scan 동작을 처리한 뒤 넣는다.
  // failed=true: 검색 자체가 실패(목록 비움, 화면에 실패 안내).
  void SetScanResults(const ScanEntry* entries, size_t n, bool failed = false);
  ScanState scan_state() const { return scan_state_; }
  // 시간 경과 처리(코드 만료, 세션 만료). 루프에서 호출.
  void Tick(int64_t now);

  bool IsLan() const { return policy_ == AuthPolicy::LanPaired; }
  // 창이 끝났는가(어댑터가 포털을 닫을 때 쓴다). 남은 초(-1 = 만료 없음).
  bool WindowExpired(int64_t now) const { return Expired(now); }
  int RemainingSec(int64_t now) const {
    if (window_sec_ <= 0) return -1;
    const int64_t base = last_activity_ > opened_ ? last_activity_ : opened_;
    const int64_t left = base + window_sec_ - now;
    return left > 0 ? (int)left : 0;
  }
  const char* PairingCode() const { return auth_.Code(); }
  bool PairingLocked() const { return auth_.Locked(); }
  bool TakePairingDisplayChanged() { return auth_.TakeDisplayChanged(); }
  bool SessionActive() const { return auth_.HasSession(); }

  bool HasPendingSave() const { return has_pending_; }
  const Settings& Pending() const { return pending_; }
  // 새 Wi-Fi(SSID/비밀번호)로 바뀌는 저장인가(접속 시험이 필요한가).
  bool PendingWifiChanged() const;
  bool ResetConfirmed() const { return reset_done_; }
  bool Closed() const { return closed_; }
  PortalAction TakeAction();
  void CompleteSave(bool ok);
  const char* Csrf() const { return csrf_; }

 private:
  Settings current_, pending_;
  IRandom& rng_;
  PortalAuth auth_;
  AuthPolicy policy_ = AuthPolicy::ApTrusted;
  AllowedHosts hosts_;
  int window_sec_ = kWindowSec;
  int64_t last_activity_ = 0;
  int64_t opened_;
  char csrf_[33];
  char cookie_[96] = {0};
  char status_[1024] = {0};  // 여러 줄 상태(첫 줄은 메뉴, 전체는 /status)
  bool has_pending_ = false;
  bool reset_step1_ = false;
  bool reset_done_ = false;
  bool closed_ = false;
  bool saved_ = false;
  bool save_failed_ = false;
  bool pending_wifi_changed_ = false;
  ScanEntry scan_[kMaxScan];
  ScanState scan_state_ = ScanState::Idle;
  size_t scan_n_ = 0;
  PortalAction action_ = PortalAction::None;
  char body_[kBodyMax + 1];

  // 창은 마지막 활동(로그인된 요청) 기준으로 연장된다. 설정을 고치는 도중 닫히지 않게.
  bool Expired(int64_t now) const {
    return window_sec_ > 0 && now - (last_activity_ > opened_ ? last_activity_ : opened_) > window_sec_;
  }
  Response Render(int status, const char* text);
  Response RenderGo(int status, const char* text, const char* url, const char* link, int refresh_sec);
  Response RenderScript();
  Response RenderSections(uint8_t mask, const char* title, const char* sec);
  Response RenderMenu();
  Response RenderLogin(int status, const char* message);
  Response HandleLogin(const Request& req, int64_t now);
  Response HandleSave(const Request& req);
  Response HandleImport(const Request& req);
  Response RenderBackup();
};

}  // namespace portal
}  // namespace kdash
