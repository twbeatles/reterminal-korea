#include "BuildIdentity.h"

#ifndef KDASH_FW_VERSION
#define KDASH_FW_VERSION "0.0.0"
#endif

namespace kdash {
const char* BuildIdentity() {
  return "KDASH_BUILD_ID|reterminal_e1001|" KDASH_FW_VERSION "|END";
}
}
