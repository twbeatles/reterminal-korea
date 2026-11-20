#pragma once
// Offline code -> display-name table for common KRX tickers (public names,
// no network, no secrets). Unknown codes return nullptr so the UI falls back
// to the 6-digit code instead of guessing. Names must stay inside the
// shipped KS X 1001 font; test_stock_names verifies zero missing glyphs.
#include <stddef.h>

namespace kdash {
namespace stocks {

struct NameEntry {
  const char* code;
  const char* name;
};

const char* NameForCode(const char* code);
size_t NameCount();
const NameEntry& NameAt(size_t index);

}  // namespace stocks
}  // namespace kdash
