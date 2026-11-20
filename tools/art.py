"""Bake only the art needed by the selected edition. No image decoder in the kernel."""

from array import array
from pathlib import Path
from PIL import Image, ImageFont, ImageDraw, ImageOps
import math, sys
from icons import atlas

ROOT = Path(__file__).resolve().parents[1]


def bake(build, lite, wallpaper=None):
    names = []
    blob = bytearray()
    header = []

    def asset(name, im):
        im = im.convert("RGB")
        a = array(
            "H", ((r >> 3) << 11 | (g >> 2) << 5 | (b >> 3) for r, g, b in im.getdata())
        )
        if sys.byteorder != "little":
            a.byteswap()
        header.append(
            f"#define {name} ((const u16*)(art+{len(blob)}))\n#define {name}_W {im.width}\n#define {name}_H {im.height}"
        )
        blob.extend(a.tobytes())

    size = (640, 360) if lite else (960, 540)
    for name, path in [
        ("SKY", wallpaper or ROOT / "assets/desktop.jpg"),
        ("WELCOME", ROOT / "assets/welcome.jpg"),
    ]:
        asset(
            name, ImageOps.fit(Image.open(path), size, method=Image.Resampling.LANCZOS)
        )
    asset("RAIL", Image.open(ROOT / "assets/rail.png"))
    # Sawtooth launcher bubbles, sampled once instead of computing trig every frame.
    im = Image.new("RGB", (236, 236), (255, 0, 255))
    px = im.load()
    for y in range(236):
        for x in range(236):
            dx = (x - 117.5) / 117.5
            dy = (y - 117.5) / 117.5
            r = math.hypot(dx, dy)
            t = math.atan2(dx, dy) * 32 / (2 * math.pi)
            if r > 1 - 0.065 * abs(2 * (t % 1) - 1):
                continue
            if r < 0.68:
                k = r / 0.68
                col = (int(247 * (1 - k) + 8 * k), int(255 - 43 * k), 255)
            else:
                k = (r - 0.68) / 0.32
                col = (int(8 + 82 * k), int(212 + 18 * k), 255)
            px[x, y] = col
    asset("BURST", im)
    icons = atlas()
    for i in range(12):
        asset(f"ICON{i}", icons.crop((i * 64, 0, (i + 1) * 64, 64)))
    for label, sz, bold in [
        ("small", 11, False),
        ("body", 15, False),
        ("head", 23, True),
    ]:
        font = ImageFont.truetype("DejaVuSans" + ("-Bold" if bold else "") + ".ttf", sz)
        w = sz + 4
        h = sz + 6
        ims = Image.new("L", (w * 95, h))
        draw = ImageDraw.Draw(ims)
        adv = []
        for i in range(95):
            c = chr(32 + i)
            adv.append(round(font.getlength(c)))
            draw.text((i * w, 0), c, font=font, fill=255)
        header.append(
            f"static const u8 {label}_advance[95]={{" + ",".join(map(str, adv)) + "};"
        )
        header.append(
            f"#define {label}_font {{ {w}, {h}, {label}_advance, art+{len(blob)} }}"
        )
        blob.extend(ims.tobytes())
    (build / "art.bin").write_bytes(blob)
    (build / "art.h").write_text("extern const u8 art[];\n" + "\n".join(header) + "\n")
