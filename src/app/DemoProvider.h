#pragma once
// DemoProvider (F-01): fixed synthetic snapshot for key-less first run.
// Every value carries DEMO; never calls the network; never persisted.
#include "../app/Fetcher.h"

namespace kdash {
namespace demo {

void FillDemo(app::Snapshot& snap, int64_t now_utc);

}  // namespace demo
}  // namespace kdash
