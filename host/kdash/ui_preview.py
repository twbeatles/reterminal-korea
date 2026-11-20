"""800x480 ePaper host preview (src/ui/Layout geometry + Pages model).

1bpp text-first + 4-level gray aux lines. Pillow only; device runtime never
needs this. Generates the 10 golden snapshots (05 Host Preview).
"""

from __future__ import annotations

from PIL import Image, ImageDraw

W, H = 800, 480
MARGIN_X = 20
HEADER = (MARGIN_X, 12, 780, 48)
BODY_L = (MARGIN_X, 58, 392, 434)
BODY_R = (408, 58, 780, 434)
FOOTER = (MARGIN_X, 440, 780, 468)

WHITE, LTGRAY, DKGRAY, BLACK = 255, 192, 96, 0


def new_frame():
    img = Image.new("L", (W, H), WHITE)
    return img, ImageDraw.Draw(img)


def frame(lines, footer, header="K-Dash E1001", demo=False, stale_badges=()):
    img, d = new_frame()
    d.rectangle([HEADER[0], HEADER[1], HEADER[2], HEADER[3]], outline=BLACK, width=2)
    d.text((HEADER[0] + 8, HEADER[1] + 8), header, fill=BLACK)
    if demo:
        d.rectangle(
            [HEADER[2] - 90, HEADER[1] + 6, HEADER[2] - 8, HEADER[3] - 6], fill=DKGRAY
        )
        d.text((HEADER[2] - 82, HEADER[1] + 8), "DEMO", fill=WHITE)
    d.rectangle([BODY_L[0], BODY_L[1], BODY_L[2], BODY_L[3]], outline=BLACK, width=1)
    d.rectangle([BODY_R[0], BODY_R[1], BODY_R[2], BODY_R[3]], outline=BLACK, width=1)
    y = BODY_L[1] + 12
    for left, right in lines:
        d.text((BODY_L[0] + 10, y), str(left), fill=BLACK)
        if right:
            d.text((BODY_R[0] + 10, y), str(right), fill=BLACK)
        y += 30
    for badge in stale_badges:
        d.rectangle(
            [FOOTER[0], FOOTER[1] - 26, FOOTER[0] + 8 * len(badge) + 16, FOOTER[1] - 6],
            outline=BLACK,
            width=1,
        )
        d.text((FOOTER[0] + 8, FOOTER[1] - 24), badge, fill=BLACK)
        break  # single badge row in preview
    d.rectangle([FOOTER[0], FOOTER[1], FOOTER[2], FOOTER[3]], outline=DKGRAY, width=1)
    d.text((FOOTER[0] + 8, FOOTER[1] + 7), footer, fill=BLACK)
    return img


def check_clip(img) -> list:
    """Rows of fully-black horizontal runs at panel edges (clipping smoke)."""
    px = img.load()
    bad = []
    for x in (0, W - 1):
        run = sum(1 for y in range(H) if px[x, y] == BLACK)
        if run > H // 2:
            bad.append(f"edge x={x} black-run {run}")
    return bad


SCENARIOS = (
    "Day",
    "Night",
    "Weekend",
    "NoNetwork",
    "NoData",
    "LongKorean",
    "WrongTime",
    "LowBattery",
    "KisDisabled",
    "Demo",
)
