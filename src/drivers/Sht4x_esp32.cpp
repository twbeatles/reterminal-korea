#include "Sht4x_esp32.h"
#if defined(ARDUINO) && defined(KDASH_BOARD_E1001)
#include <Arduino.h>
#include <Wire.h>
#include "Sht4xCodec.h"

namespace kdash {
bool ReadIndoor(float& temp_c, float& humidity_pct) {
  // E1001 pins/address: official SHT4x_Sensor example @8d97d891.
  // Command/wait/CRC/formula: Sensirion SHT4x datasheet v7.3, June 2026,
  // sections 1 and 4: https://sensirion.com/resource/datasheet/sht4x
  Wire.begin(19,20);
  Wire.setClock(400000);
  Wire.setTimeOut(50);
  Wire.beginTransmission(0x44);
  Wire.write(0xFD);
  if (Wire.endTransmission() != 0) return false;
  delay(10);
  if (Wire.requestFrom(0x44,6) != 6) return false;
  uint8_t bytes[6];
  for (uint8_t& b : bytes) b=(uint8_t)Wire.read();
  float t=0,rh=0;
  if (!sht::Decode(bytes,t,rh)) return false;
  temp_c=t; humidity_pct=rh;
  return true;
}
}
#endif
