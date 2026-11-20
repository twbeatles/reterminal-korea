#pragma once
// 24절기 상수표(2026~2028). 한국천문연구원 특일 정보 API 응답을 생성기로 옮긴 값이며
// 기기에서 따로 요청하지 않는다. 표 밖의 해는 "정보 없음"으로 다룬다(추측하지 않는다).
#include <stddef.h>
#include <stdint.h>

namespace kdash {
namespace terms {

struct Term {
  int16_t year;
  uint8_t month;
  uint8_t day;
  uint8_t index;  // 0 소한 ... 23 동지
};

size_t Count();
const Term& At(size_t i);       // 날짜 오름차순
const char* Name(int index);    // "" if out of range

}  // namespace terms
}  // namespace kdash
