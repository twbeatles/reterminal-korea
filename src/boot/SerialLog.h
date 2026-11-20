#pragma once
// Boot serial sink (SRP: 로그 출력 대상 정의만 담당).
// UART 로그는 캐리어의 USB-UART 브리지(GPIO44 RX / GPIO43 TX) = Serial1.
// `Serial`(USB-CDC)은 E1001 USB-C 포트에 노출되지 않음
// (공식 UserButtons / LowPower_DeepSleep / RTC_PCF8563 예제 기준).
// SOLID: DIP — 모든 디바이스 모듈은 이 싱크에만 의존하고,
// 구체적인 전송 방식이 바뀌어도 호출 측은 수정 없이 닫혀 있음(OCP).
#if defined(ARDUINO) && defined(KDASH_BOARD_E1001)
#include <Arduino.h>
#define KDASH_LOG Serial1
inline constexpr int kLogRxPin = 44;
inline constexpr int kLogTxPin = 43;
#endif
