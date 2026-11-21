"""Small, original hand-drawn launcher glyphs, baked into a single native atlas."""

from PIL import Image, ImageDraw
import math


def atlas():
    icons = []
    for n in range(20):
        im = Image.new("RGB", (64, 64), (255, 0, 255))
        d = ImageDraw.Draw(im)

        def panel(box, rad, a, b):
            mask = Image.new("L", (64, 64))
            ImageDraw.Draw(mask).rounded_rectangle(box, rad, fill=255)
            grad = Image.new("RGB", (64, 64))
            g = ImageDraw.Draw(grad)
            for y in range(box[1], box[3] + 1):
                t = (y - box[1]) / max(1, box[3] - box[1])
                g.line(
                    (0, y, 64, y),
                    fill=tuple(round(u + (v - u) * t) for u, v in zip(a, b)),
                )
            im.paste(grad, mask=mask)

        if n == 0:
            panel((4, 13, 31, 34), 6, (255, 231, 139), (218, 162, 54))
            panel((4, 19, 59, 53), 5, (241, 185, 66), (185, 125, 33))
            panel((8, 23, 55, 45), 2, (255, 255, 248), (214, 240, 229))
            panel((4, 28, 59, 53), 6, (255, 237, 148), (246, 178, 46))
        if n == 1:
            panel((10, 4, 53, 59), 4, (255, 255, 255), (155, 213, 239))
            d.rectangle((10, 5, 14, 57), fill="#72abcf")
            d.polygon([(42, 4), (53, 16), (42, 16)], fill="#83b9d8")
            for y in (25, 32, 39, 46):
                d.line((19, y, 43 if y != 46 else 33, y), fill="#5797b8", width=2)
        if n == 2:
            d.ellipse((4, 20, 51, 56), fill="#d99852", outline="#a36c38", width=2)
            for x, y, c in [
                (12, 29, "#83d4fa"),
                (22, 24, "#fffa80"),
                (32, 26, "#ff7999"),
                (12, 40, "#83ed99"),
            ]:
                d.ellipse((x, y, x + 7, y + 7), fill=c)
            d.line((29, 42, 55, 8), fill="#8f623e", width=7)
            d.line((42, 22, 55, 6), fill="#e6ca9e", width=5)
            d.polygon([(24, 49), (27, 36), (34, 40)], fill="#294b60")
        if n == 3:
            panel((10, 4, 53, 60), 6, (151, 204, 223), (52, 91, 122))
            panel((15, 10, 48, 26), 3, (188, 246, 220), (127, 188, 162))
            d.line((34, 18, 43, 18), fill="#50756b", width=2)
            for i in range(9):
                panel(
                    (
                        16 + i % 3 * 11,
                        32 + i // 3 * 9,
                        23 + i % 3 * 11,
                        37 + i // 3 * 9,
                    ),
                    2,
                    (255, 255, 244),
                    (174, 210, 210),
                )
        if n == 4:
            panel((4, 4, 60, 60), 28, (183, 253, 250), (17, 152, 172))
            panel((10, 10, 54, 54), 22, (250, 255, 255), (189, 237, 239))
            d.line((32, 15, 32, 32, 43, 39), fill="#297587", width=4)
            d.ellipse((29, 29, 35, 35), fill="#136274")
        if n == 5:
            p = []
            for i in range(32):
                a = i * math.pi / 16
                r = (28, 28, 22, 22)[i % 4]
                p.append((32 + r * math.cos(a), 32 + r * math.sin(a)))
            d.polygon(p, fill="#82acbd", outline="#456e83")
            panel((13, 13, 51, 51), 19, (230, 251, 255), (99, 149, 171))
            d.ellipse((23, 23, 41, 41), fill="#467186")
            d.arc((15, 15, 49, 49), 180, 298, fill="white", width=2)
        if n == 6:
            panel((3, 10, 61, 54), 6, (143, 208, 217), (45, 97, 105))
            panel((7, 14, 57, 49), 3, (48, 101, 114), (17, 57, 66))
            d.line((14, 24, 21, 30, 14, 36), fill="#b9ffc7", width=3)
            d.line((27, 37, 41, 37), fill="#b9ffc7", width=3)
        if n == 7:
            for k in range(8):
                a = k * math.pi / 4
                d.line(
                    (32, 34, 32 + 25 * math.sin(a), 34 + 25 * math.cos(a)),
                    fill="#455561",
                    width=5,
                )
            panel((14, 16, 50, 52), 18, (134, 148, 150), (41, 52, 67))
            d.ellipse((21, 21, 30, 28), fill="#e0f3ed")
        if n == 8:
            panel((3, 8, 61, 48), 5, (231, 245, 250), (121, 156, 173))
            panel((7, 12, 57, 41), 2, (127, 226, 239), (36, 157, 198))
            d.polygon(
                [
                    (28, 48),
                    (36, 48),
                    (39, 54),
                    (45, 56),
                    (45, 58),
                    (19, 58),
                    (19, 56),
                    (25, 54),
                ],
                fill="#8bafbe",
            )
        if n == 9:
            panel((8, 9, 56, 55), 9, (180, 250, 218), (41, 149, 160))
            d.polygon([(25, 20), (25, 45), (44, 32)], fill="#ffffff")
        if n == 10:
            panel((6, 10, 58, 56), 5, (255, 255, 255), (211, 233, 239))
            panel((6, 9, 58, 23), 3, (255, 161, 135), (219, 93, 78))
            d.line((22, 31, 22, 47), fill="#558292", width=4)
            d.line(
                (31, 32, 40, 32, 34, 39, 41, 39, 41, 46, 30, 46),
                fill="#558292",
                width=3,
            )
        if n == 11:
            panel((4, 9, 60, 54), 5, (174, 215, 227), (86, 129, 156))
            panel((9, 14, 55, 48), 2, (71, 134, 132), (27, 95, 91))
            d.line(
                (12, 37, 22, 37, 26, 28, 31, 41, 36, 20, 42, 34, 52, 34),
                fill="#b7ff97",
                width=3,
            )
        if n >= 12:
            tint = [
                (249, 197, 76),
                (59, 166, 128),
                (90, 141, 192),
                (233, 182, 65),
                (96, 147, 160),
                (77, 172, 185),
                (92, 158, 214),
                (154, 121, 207),
            ][n - 12]
            panel((6, 5, 58, 58), 9, tuple(min(255, c + 68) for c in tint), tint)
            ink = "#184857"
            if n == 12:
                d.polygon([(42, 55), (54, 42), (42, 42)], fill="white")
                d.line((15, 22, 45, 22), fill=ink, width=3)
                d.line((15, 31, 37, 31), fill=ink, width=3)
            if n == 13:
                for y in (18, 32, 46):
                    d.line([(13, y), (17, y + 4), (23, y - 4)], fill="white", width=3)
                    d.line((29, y, 49, y), fill=ink, width=3)
            if n == 14:
                d.line([(16, 45), (31, 15), (46, 45)], fill="white", width=4)
                d.line((23, 34, 39, 34), fill="white", width=4)
            if n == 15:
                d.ellipse((19, 13, 45, 39), fill="#fffacc", outline=ink, width=2)
                d.rectangle((26, 37, 38, 46), fill=ink)
            if n == 16:
                for y in (19, 31, 43):
                    d.ellipse((14, y, 19, y + 5), fill="white")
                    d.line((25, y + 2, 48, y + 2), fill=ink, width=3)
            if n == 18:
                d.ellipse((17, 14, 27, 24), fill="white")
                d.polygon(
                    [(12, 47), (28, 31), (33, 37), (42, 26), (53, 47)], fill="#227d74"
                )
            if n == 19:
                d.rounded_rectangle((13, 25, 51, 49), radius=4, fill="#ffebb6")
                d.line((21, 25, 21, 18, 43, 18, 43, 25), fill=ink, width=4)
            if n == 17:
                for y, x in [(20, 35), (32, 23), (44, 40)]:
                    d.line((14, y, 50, y), fill=ink, width=3)
                    d.ellipse((x - 4, y - 4, x + 4, y + 4), fill="white")
        icons.append(im)
    dst = Image.new("RGB", (64 * len(icons), 64))
    for i, v in enumerate(icons):
        dst.paste(v, (i * 64, 0))
    return dst
