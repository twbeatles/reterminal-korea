#pragma once
// PMIC USB detect (U-12 resolved): Silergy SY6974 @0x6B on Wire1
// (SDA=39, SCL=40, 400kHz) — official SenseCraft_HMI pmic_sy6974 +
// SY6974 lib @8d97d891. USB presence = REG0A(0x0A) bit7 (isUsbAttached),
// or REG08(0x08) bus-status bits[7:5] != 0 ("No input" == 0).
// Charge protection writes only confirmed SY6974B ID=0x40 (ADR 0028).
#include <stdint.h>
#include "../power/ChargeProtection.h"

namespace kdash {
namespace pmic {

inline constexpr uint8_t kAddr = 0x6B;
inline constexpr uint8_t kRegStatus = 0x08;
inline constexpr uint8_t kRegBusGood = 0x0A;

// Portable decoders (host-tested).
inline bool UsbAttachedFromReg0A(uint8_t reg0a) { return (reg0a & 0x80) != 0; }
inline bool BusPresentFromReg08(uint8_t reg08) {
  return ((reg08 >> 5) & 0x07) != 0;
}

// Device-only: reads REG0A over Wire1. False on I2C failure.
bool UsbAttached();
void Diagnose();  // read-only charger identity/configuration, no credentials
bool UsbKnown();  // valid last poll; UsbAttached still uses battery fallback
bool PrepareForSleep();
charge::State MaintainCharge(bool enabled);  // <=one sample per 30s

}  // namespace pmic
}  // namespace kdash
