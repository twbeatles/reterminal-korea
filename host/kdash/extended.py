"""Stage-2 portable mirrors (T02-T12): clock, calendar, geogrid, buttons,
SHT4x CRC, battery, request budget. Mirrors the C++ in src/; keep in sync.
"""

import math

BUILD_EPOCH = 1767225600  # 2026-01-01T00:00:00Z


def clock_decide(rtc_ok, rtc_vl, rtc_epoch, ntp_ok, ntp_epoch, build=BUILD_EPOCH):
    if ntp_ok and ntp_epoch >= build:
        action = "WRITE_RTC"
        if rtc_ok and abs(rtc_epoch - ntp_epoch) <= 2:
            action = "NONE"
        return (ntp_epoch, True, "NTP", action)
    if rtc_ok and not rtc_vl and rtc_epoch >= build:
        return (rtc_epoch, True, "RTC", "NONE")
    return (0, False, "NONE", "NONE")


class YearTable:
    def __init__(self, year):
        self.year = year
        self.holidays = set()
        self.closed = set()
        self.loaded = False


def classify_day(y, m, d, weekday, tables):
    """weekday: 0=Sun..6=Sat. Returns BusinessDay/Weekend/PublicHoliday/MarketClosed/Unknown."""
    weekend = weekday in (0, 6)
    t = next((t for t in tables if t.loaded and t.year == y), None)
    if t is None:
        return "Weekend" if weekend else "Unknown"
    if (m, d) in t.closed:
        return "MarketClosed"
    if (m, d) in t.holidays:
        return "Weekend" if weekend else "PublicHoliday"
    return "Weekend" if weekend else "BusinessDay"


def latlon_to_grid(lat, lon):
    if not (28 <= lat <= 44 and 120 <= lon <= 134):
        return None
    deg = math.pi / 180.0
    re, grid = 6371.00877, 5.0
    slat1, slat2 = 30.0 * deg, 60.0 * deg
    olon, olat = 126.0 * deg, 38.0 * deg
    xo, yo = 43.0, 136.0
    sn = math.tan(math.pi * 0.25 + slat2 * 0.5) / math.tan(math.pi * 0.25 + slat1 * 0.5)
    sn = math.log(math.cos(slat1) / math.cos(slat2)) / math.log(sn)
    sf = math.tan(math.pi * 0.25 + slat1 * 0.5)
    sf = pow(sf, sn) * math.cos(slat1) / sn
    ro = math.tan(math.pi * 0.25 + olat * 0.5)
    ro = re / grid * sf / pow(ro, sn)
    ra = math.tan(math.pi * 0.25 + lat * deg * 0.5)
    ra = re / grid * sf / pow(ra, sn)
    theta = lon * deg - olon
    if theta > math.pi:
        theta -= 2.0 * math.pi
    if theta < -math.pi:
        theta += 2.0 * math.pi
    theta *= sn
    nx = int(ra * math.sin(theta) + xo + 0.5)
    ny = int(ro - ra * math.cos(theta) + yo + 0.5)
    if not (1 <= nx <= 200 and 1 <= ny <= 200):
        return None
    return (nx, ny)


NONE, S0, S1, S2, L0 = "None", "ShortKey0", "ShortKey1", "ShortKey2", "LongKey0"


class ButtonDecoder:
    def __init__(self, debounce_ms=50, long_ms=2000):
        self.debounce = debounce_ms
        self.long = long_ms
        self.reset()

    def reset(self):
        self.stable = 0
        self.cand = 0
        self.cand_since = 0
        self.have_cand = False
        self.active = None
        self.press_since = 0
        self.long_fired = False
        self.first = True

    def feed(self, t_ms, mask):
        mask &= 0x07
        if self.first:
            self.first = False
            self.stable = self.cand = mask
            self.cand_since = t_ms
            if mask:
                self.active = (mask & -mask).bit_length() - 1
                self.press_since = t_ms
            return NONE
        if not self.have_cand or mask != self.cand:
            self.cand = mask
            self.cand_since = t_ms
            self.have_cand = True
        if mask != self.stable and (
            t_ms - self.cand_since >= self.debounce
            or (mask == 0 and self.stable and t_ms - self.press_since >= self.debounce)
        ):
            prev = self.stable
            self.stable = mask
            if mask and not prev:
                self.active = (mask & -mask).bit_length() - 1
                self.press_since = t_ms
                self.long_fired = False
            if not mask and prev and self.active is not None and not self.long_fired:
                e = [S0, S1, S2][self.active]
                self.active = None
                return e
            if not mask:
                self.active = None
        if (
            self.active == 0
            and not self.long_fired
            and self.stable
            and t_ms - self.press_since >= self.long
        ):
            self.long_fired = True
            return L0
        return NONE


def sht_crc8(data):
    crc = 0xFF
    for b in data:
        crc ^= b
        for _ in range(8):
            crc = ((crc << 1) ^ 0x31) & 0xFF if crc & 0x80 else (crc << 1) & 0xFF
    return crc


def sht_decode(frame):
    if sht_crc8(frame[0:2]) != frame[2] or sht_crc8(frame[3:5]) != frame[5]:
        return None
    raw_t = (frame[0] << 8) | frame[1]
    raw_rh = (frame[3] << 8) | frame[4]
    t = -45.0 + 175.0 * raw_t / 65535.0
    rh = min(100.0, max(0.0, -6.0 + 125.0 * raw_rh / 65535.0))
    return (t, rh)


ADC_SAMPLES, ADC_INTERVAL_MS, ADC_SETTLE_MS = 10, 25, 5


def mean_of(samples):
    if not samples:
        return -1
    if any(v < 0 for v in samples):
        return -1
    return sum(samples) // len(samples)


def battery_class(mv, low=3464, crit=3252):
    if mv < 0:
        return "Unknown"
    if mv < crit:
        return "Critical"
    if mv < low:
        return "Low"
    return "Normal"


# Official E1001 % -> mV table (SenseCraft E1001 config.h @8d97d891).
CURVE_MV = [
    2795,
    2795,
    2990,
    3107,
    3189,
    3252,
    3300,
    3334,
    3354,
    3367,
    3378,
    3389,
    3399,
    3409,
    3418,
    3426,
    3434,
    3442,
    3449,
    3457,
    3464,
    3471,
    3478,
    3485,
    3491,
    3498,
    3504,
    3510,
    3516,
    3522,
    3528,
    3534,
    3540,
    3546,
    3552,
    3558,
    3564,
    3569,
    3575,
    3581,
    3587,
    3592,
    3598,
    3603,
    3609,
    3614,
    3620,
    3625,
    3631,
    3636,
    3642,
    3647,
    3653,
    3658,
    3664,
    3669,
    3675,
    3680,
    3686,
    3691,
    3697,
    3703,
    3709,
    3715,
    3721,
    3727,
    3733,
    3739,
    3746,
    3752,
    3759,
    3765,
    3772,
    3779,
    3786,
    3793,
    3801,
    3808,
    3816,
    3824,
    3832,
    3840,
    3849,
    3858,
    3867,
    3876,
    3886,
    3896,
    3906,
    3916,
    3927,
    3984,
    3995,
    4007,
    4019,
    4032,
    4045,
    4059,
    4074,
    4090,
    4111,
]


def battery_percent(mv):
    if mv <= CURVE_MV[0]:
        return 0
    if mv >= CURVE_MV[100]:
        return 100
    for p in range(100):
        if mv < CURVE_MV[p + 1]:
            lo, hi = CURVE_MV[p], CURVE_MV[p + 1]
            if hi <= lo:
                return p
            return p + (1000 * (mv - lo) // (hi - lo) + 500) // 1000
    return 100


def pmic_usb_attached(reg0a):
    return (reg0a & 0x80) != 0


def pmic_bus_present(reg08):
    return ((reg08 >> 5) & 0x07) != 0


def pcf_year(month_reg, year_bcd):
    """Century: C=1 -> 1900s, C=0 -> 2000s (official RTC example)."""
    hi, lo = (year_bcd >> 4) & 0xF, year_bcd & 0xF
    if hi > 9 or lo > 9:
        return None
    base = 1900 if (month_reg & 0x80) else 2000
    return base + hi * 10 + lo


CAPS = {"Exim": 7, "FscClose": 4, "KisPrice": 6}


def cap_for(provider):
    return CAPS.get(provider, 2)


def should_defer(provider, spent_for_p, spent_total, pass_max=24):
    if spent_for_p >= cap_for(provider):
        return True
    return spent_total >= pass_max
