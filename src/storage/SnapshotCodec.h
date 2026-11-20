#pragma once
// Snapshot + ProviderStatus <-> bytes. Pointers become source_id enum;
// restored to static literals (strcmp-equal). Portable.
#include <stddef.h>
#include <stdint.h>

#include "../app/Coordinator.h"
#include "../app/Fetcher.h"

namespace kdash {
namespace snapshot_codec {

// Known source strings (see T02 3-3). Unknown at decode => "" (never crash).
uint8_t SourceId(const char* source);
const char* SourceStr(uint8_t id);

size_t EncodeSnapshot(const app::Snapshot& s, uint8_t* out, size_t out_cap);
bool DecodeSnapshot(const uint8_t* data, size_t len, app::Snapshot& out);

size_t EncodeStatus(const app::ProviderStatus* st, size_t n, uint8_t* out,
                    size_t out_cap);
bool DecodeStatus(const uint8_t* data, size_t len, app::ProviderStatus* out,
                  size_t n);

}  // namespace snapshot_codec
}  // namespace kdash
