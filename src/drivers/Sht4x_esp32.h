#pragma once
namespace kdash {
// E1001 shared Wire bus, 0x44; call before Wi-Fi/display self-heating.
bool ReadIndoor(float& temp_c, float& humidity_pct);
}
