#!/usr/bin/env python3
"""Vendor official reference texts (MIT, @8d97d891) into hardware/reference/.

Read-only material for future agents. Never #included into firmware.
GxEPD2 (GPL-3.0) is deliberately excluded.
"""

import os
import sys
import urllib.request

SHA = "8d97d8912987c07529287629f7e73039bb0e6384"
BASE = (
    "https://raw.githubusercontent.com/Seeed-Projects/OSHW-reTerminal-Series-E-D/" + SHA
)
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
DEST = os.path.join(ROOT, "hardware", "reference")

FILES = [
    "LICENSE",
    "examples/base/Battery_Monitor/Battery_Monitor.ino",
    "examples/base/Buzzer_Control/Buzzer_Control.ino",
    "examples/base/GxEPD2_reTerminal_E1001/GxEPD2_reTerminal_E1001.ino",
    "examples/base/GxEPD2_reTerminal_E1001_Gray4/GxEPD2_reTerminal_E1001_Gray4.ino",
    "examples/base/LowPower_DeepSleep/LowPower_DeepSleep.ino",
    "examples/base/RTC_PCF8563/RTC_PCF8563.ino",
    "examples/base/SHT4x_Sensor/SHT4x_Sensor.ino",
    "examples/base/UserButtons/UserButtons.ino",
    "examples/official/SenseCraft_HMI/src/boards/board_registry.h",
    "examples/official/SenseCraft_HMI/src/boards/reterminal_e1001/config.h",
    "examples/official/SenseCraft_HMI/src/boards/reterminal_e1001/reterminal_e1001.cpp",
    "examples/official/SenseCraft_HMI/src/boards/common/pmic_sy6974.h",
    "examples/official/SenseCraft_HMI/src/boards/common/pmic_sy6974.cpp",
    "examples/official/SenseCraft_HMI/src/boards/common/rtc_pcf8563.h",
    "examples/official/SenseCraft_HMI/src/boards/common/rtc_pcf8563.cpp",
    "examples/official/SenseCraft_HMI/src/boards/common/battery_gauge.h",
    "examples/official/SenseCraft_HMI/src/boards/common/battery_gauge.cpp",
    "examples/official/SenseCraft_HMI/src/APP/app_power_manager.h",
    "examples/official/SenseCraft_HMI/src/APP/app_power_manager.cpp",
    "examples/official/SenseCraft_HMI/lib/SY6974/SY6974.h",
    "examples/official/SenseCraft_HMI/lib/SY6974/SY6974.cpp",
]


def main():
    os.makedirs(DEST, exist_ok=True)
    for rel in FILES:
        url = BASE + "/" + rel
        req = urllib.request.Request(url, headers={"User-Agent": "kdash-refetch"})
        with urllib.request.urlopen(req, timeout=30) as r:
            data = r.read()
        dest = os.path.join(DEST, rel.replace("/", "__"))
        with open(dest, "wb") as f:
            f.write(data)
        print("%-70s %6d B" % (os.path.basename(dest), len(data)))
    print("total", len(FILES), "files")
    return 0


if __name__ == "__main__":
    sys.exit(main())
