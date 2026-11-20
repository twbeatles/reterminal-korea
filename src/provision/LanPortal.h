#pragma once
// LanPortal: 같은 Wi-Fi(STA 주소)에서 여는 설정 포털의 기기 어댑터 (ADR 0024).
// SoftAP 포털(RunPortal)과 달리 블로킹하지 않는다: Poll()을 loop()에서 자주 부르면 된다.
// 접근 제어는 PortalHandler(LanPaired): Host/Origin 검사 + 화면 페어링 코드 + 세션 쿠키.
// 구현은 PortalServer_esp32.cpp (ESP32 전용). 호스트 빌드는 이 헤더만 보며 정의는 없다.
#include <stdint.h>

#include <memory>

#include "../../include/Config.h"
#include "../../include/IStorage.h"

namespace kdash {
namespace portal {

enum class LanEvent : uint8_t {
  None = 0,
  Saved,       // 설정이 저장됨 -> 호출자가 재부팅
  Reset,       // 공장초기화 완료 -> 호출자가 재부팅
  Closed,      // 창 만료/사용자가 닫음
  Refresh,     // 세션이 "지금 새로고침"을 요청
  Reboot,      // 세션이 재시작을 요청
  OpenAp,      // 세션이 AP 모드로 전환을 요청
  SaveFailed,  // 접속 시험/저장 실패(이전 설정 유지, 포털은 계속 열림)
};

struct LanInfo {
  char ip[16] = {0};
  char host[24] = {0};  // mDNS 이름(".local" 제외). 등록 실패면 ""
  char code[8] = {0};   // 페어링 코드(화면 전용). 잠금 중이면 ""
  bool locked = false;
  int remaining_min = -1;  // 남은 분, -1 = 만료 없음
};

class LanPortal {
 public:
  LanPortal();
  ~LanPortal();
  LanPortal(const LanPortal&) = delete;
  LanPortal& operator=(const LanPortal&) = delete;

  // Wi-Fi가 연결돼 있어야 한다. window_sec=0이면 만료 없음(상시 모드).
  bool Open(Settings& current, IStorage& storage, int window_sec);
  bool IsOpen() const { return impl_ != nullptr; }
  // 논블로킹(저장 시 최대 ~20초 접속 시험 제외). status_line은 메뉴에 보일 한 줄(비밀 금지).
  LanEvent Poll(const char* status_line);
  void Close();
  bool Info(LanInfo& out) const;
  struct Impl;  // 구현 세부(어댑터 전용). 외부에서 쓰지 않는다.
  // 페어링 코드/잠금 표시가 바뀌었으면 true를 한 번 돌려준다(화면 갱신 트리거).
  bool TakeDisplayChanged();

 private:
  std::unique_ptr<Impl> impl_;
};

}  // namespace portal
}  // namespace kdash
