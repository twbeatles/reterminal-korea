#include "HolidayNames.h"

namespace kdash {
namespace holidays {

const char* FixedName(int month, int day) {
  struct E { int m, d; const char* n; };
  static const E kFixed[] = {
      {1, 1, "신정"},     {3, 1, "삼일절"},   {5, 5, "어린이날"}, {6, 6, "현충일"},
      {8, 15, "광복절"},  {10, 3, "개천절"},  {10, 9, "한글날"},  {12, 25, "성탄절"},
  };
  for (const E& e : kFixed)
    if (e.m == month && e.d == day) return e.n;
  return nullptr;
}

}  // namespace holidays
}  // namespace kdash
