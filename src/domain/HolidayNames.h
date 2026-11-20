#pragma once
// 고정 양력일 공휴일 이름. 음력 기반 휴일(설날·부처님오신날·추석)과 대체공휴일은 날짜가
// 해마다 바뀌어 여기서 이름을 단정하지 않는다: nullptr이면 호출측이 "공휴일"로 표시한다.
namespace kdash {
namespace holidays {

const char* FixedName(int month, int day);

}  // namespace holidays
}  // namespace kdash
