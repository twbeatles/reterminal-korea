#pragma once
#include "../../include/IStorage.h"
#include "../domain/Calendar.h"
namespace kdash { namespace cal {
bool LoadTables(IStorage& storage, YearTable (&out)[2]);
bool SaveTables(IStorage& storage, const YearTable (&tables)[2], int64_t now);
} }
