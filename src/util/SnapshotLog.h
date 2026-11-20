#pragma once
// SnapshotLog (SRP: UART 진단 출력만 담당).
// SOLID: ISP — Snapshot 읽기 전용 의존, 렌더/네트워크와 무관.
#include "../app/Fetcher.h"

namespace kdash {
namespace debug {

// 스냅샷 사람이 읽을 수 있는 요약 출력. 값 포맷은 Format/ToString 재사용.
void PrintSnapshot(const app::Snapshot& s);

}  // namespace debug
}  // namespace kdash
