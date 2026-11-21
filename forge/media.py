#!/usr/bin/env python3
"""Build Stratus BIOS+EFI kernels, a dual El Torito ISO and a marked data disk."""

import argparse, hashlib, io, struct, subprocess, zipfile
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont, ImageOps
import pycdlib
from sample_packages import seed

ROOT = Path(__file__).resolve().parents[1]
EDITIONS = ("Normal", "Lite", "VMware", "VMware-Lite")


def assets(d, lite):
    wh = (720, 405) if lite else (1280, 720)
    sky = ImageOps.fit(
        Image.open(ROOT / "assets/sky.webp").convert("RGB"),
        wh,
        method=Image.Resampling.LANCZOS,
    )
    raw = bytearray().join(
        struct.pack("<H", (r >> 3) << 11 | (g >> 2) << 5 | (b >> 3))
        for r, g, b in sky.get_flattened_data()
    )
    offsets = []
    widths = []
    heights = []
    advs = []
    for size, bold in [(12, False), (15, False), (26, True), (46, True)]:
        font = ImageFont.truetype(
            "DejaVuSans" + ("-Bold" if bold else "") + ".ttf", size
        )
        w = size + 3
        h = size + 6
        widths.append(w)
        heights.append(h)
        offsets.append(len(raw))
        row = []
        for code in range(32, 127):
            tile = Image.new("L", (w, h))
            ImageDraw.Draw(tile).text((0, 0), chr(code), font=font, fill=255)
            raw.extend(tile.tobytes())
            row.append(str(round(font.getlength(chr(code)))))
        advs.append("{" + ",".join(row) + "}")
    icons = len(raw)
    for i, bg in enumerate(
        [
            "#e8ac4f",
            "#59af99",
            "#689acc",
            "#496578",
            "#65ae98",
            "#ab8dc5",
            "#d58374",
            "#7e9fcc",
            "#efb04b",
        ]
    ):
        im = Image.new("RGBA", (64, 64))
        p = ImageDraw.Draw(im)
        p.rounded_rectangle(
            (2, 2, 61, 61), radius=15, fill=bg, outline="#f4ffff", width=3
        )
        ink = "#204457"
        if i == 0:
            p.polygon(
                [(10, 21), (10, 15), (28, 15), (33, 22), (54, 22), (52, 49), (12, 49)],
                fill="#fff2b9",
                outline=ink,
            )
        if i == 1:
            p.rounded_rectangle((15, 10, 48, 53), radius=4, fill="white")
            [p.line((20, y, 42, y), fill=ink, width=2) for y in (22, 30, 38)]
        if i == 2:
            p.rectangle((14, 12, 50, 25), fill="white")
            [
                p.rounded_rectangle((x, y, x + 7, y + 7), radius=2, fill="white")
                for x in (15, 28, 41)
                for y in (31, 43)
            ]
        if i == 3:
            p.line([(13, 20), (23, 31), (13, 42)], fill="white", width=5)
            p.line((29, 42, 47, 42), fill="white", width=4)
        if i == 4:
            for y, x in ((18, 36), (32, 24), (46, 39)):
                p.line((12, y, 52, y), fill=ink, width=4)
                p.ellipse((x - 5, y - 5, x + 5, y + 5), fill="white")
        if i == 5:
            p.rounded_rectangle((12, 12, 52, 41), radius=4, fill=ink)
            p.rectangle((16, 16, 48, 36), fill="#def8ff")
            p.line((32, 42, 32, 49), fill=ink, width=4)
            p.line((20, 51, 44, 51), fill=ink, width=4)
        if i == 6:
            p.polygon(
                [(13, 50), (17, 37), (42, 12), (51, 21), (26, 46)],
                fill="#fff3bd",
                outline=ink,
            )
            p.line((18, 38, 26, 45), fill=ink, width=3)
        if i == 7:
            p.ellipse((10, 10, 54, 54), fill="#f7fbff", outline=ink, width=2)
            p.line([(32, 17), (32, 32), (42, 38)], fill=ink, width=4)
        if i == 8:
            im = Image.open(ROOT / "assets/glyphs/smile.png").convert("RGBA")
            im.putdata(
                [
                    (r, g, b, 0 if r > 190 and b > 180 and g < 95 else a)
                    for r, g, b, a in im.get_flattened_data()
                ]
            )
        raw.extend(im.resize((32, 32), Image.Resampling.LANCZOS).tobytes())

    def arr(t, n, values):
        return f"static const {t} {n}[]={{" + ",".join(map(str, values)) + "};\n"

    shell_offsets, shell_w, shell_h = [], [], []
    for key in ["rail", "rail-open", "disc", "tray", "sound", "light"]:
        im = Image.open(ROOT / f"assets/shell/{key}.png").convert("RGBA")
        shell_offsets.append(len(raw))
        shell_w.append(im.width)
        shell_h.append(im.height)
        raw.extend(im.tobytes())
    welcome_off = len(raw)
    welcome = (
        Image.open(ROOT / "assets/shell/welcome.webp")
        .convert("RGB")
        .resize((640, 360) if lite else (960, 540))
    )
    raw.extend(
        b"".join(
            struct.pack("<H", (r >> 3) << 11 | (g >> 2) << 5 | b >> 3)
            for r, g, b in welcome.get_flattened_data()
        )
    )
    (d / "atlas.h").write_text(
        f"#define SKY_W {wh[0]}\n#define SKY_H {wh[1]}\n#define ICON_OFFSET {icons}\n"
        + f"#define WELCOME_OFF {welcome_off}\n#define WELCOME_W {welcome.width}\n#define WELCOME_H {welcome.height}\n"
        + arr("int", "shell_off", shell_offsets)
        + arr("int", "shell_w", shell_w)
        + arr("int", "shell_h", shell_h)
        + arr("int", "font_off", offsets)
        + arr("int", "font_w", widths)
        + arr("int", "font_h", heights)
        + "static const unsigned char advances[][95]={"
        + ",".join(advs)
        + "};\n"
    )
    (d / "art.bin").write_bytes(raw)
    (d / "media.S").write_text(
        '.section .rodata\n.balign 4\n.global art\nart: .incbin "art.bin"\n.section .note.GNU-stack,"",@progbits\n'
    )


def esp(binary):
    n = 8192
    fat = 32
    root = 1 + 2 * fat
    data = root + 4
    img = bytearray(512 * n)
    img[:11] = b"\xeb\x3c\x90STRATUS2"
    struct.pack_into(
        "<HBHBHHBHHHII", img, 11, 512, 1, 1, 2, 64, n, 248, fat, 32, 64, 0, 0
    )
    img[36:39] = bytes([128, 0, 41])
    img[43:54] = b"FRESH EFI  "
    img[54:62] = b"FAT16   "
    img[510:512] = b"\x55\xaa"
    table = bytearray(512 * fat)
    struct.pack_into("<4H", table, 0, 0xFFF8, 0xFFFF, 0xFFFF, 0xFFFF)

    def block(c):
        return (data + c - 2) * 512

    def ent(off, name, cluster, length=0):
        struct.pack_into(
            "<11sB14xHI", img, off, name, 32 if length else 16, cluster, length
        )

    ent(root * 512, b"EFI        ", 2)
    ent(block(2), b".          ", 2)
    ent(block(2) + 32, b"..         ", 0)
    ent(block(2) + 64, b"BOOT       ", 3)
    ent(block(3), b".          ", 3)
    ent(block(3) + 32, b"..         ", 2)
    ent(block(3) + 64, b"BOOTX64 EFI", 4, len(binary))
    count = (len(binary) + 511) // 512
    assert block(4 + count) <= len(img)
    for i in range(count):
        struct.pack_into("<H", table, (4 + i) * 2, 0xFFFF if i + 1 == count else 5 + i)
    img[512 : 512 + len(table)] = table
    img[(1 + fat) * 512 : (1 + 2 * fat) * 512] = table
    img[block(4) : block(4) + len(binary)] = binary
    return bytes(img)


def build(e):
    lite = "Lite" in e
    vm = "VMware" in e
    w, h = (1024, 768) if lite else (1600, 900)
    d = ROOT / "build" / e
    d.mkdir(parents=True, exist_ok=True)
    out = ROOT / "distros"
    out.mkdir(exist_ok=True)
    assets(d, lite)

    def run(*args):
        subprocess.run(args, cwd=d, check=True)

    src = ROOT / "native"
    opts = [
        "-ffreestanding",
        "-Os",
        "-fno-stack-protector",
        "-fno-builtin",
        "-fno-asynchronous-unwind-tables",
        "-fno-ident",
        "-Wall",
        "-Wextra",
        "-Werror",
        f'-DEDITION="{e}"',
        f"-DTARGET_W={w}",
        f"-DTARGET_H={h}",
        "-I.",
        *["-I" + str(src / f) for f in ("core", "hal", "desktop", "compat")],
    ]
    common = [
        *sorted((src / "core").glob("*.c")),
        *sorted((src / "hal").glob("*.c")),
        src / "hal/faults.S",
        *sorted((src / "compat").glob("*.c")),
        *sorted((src / "desktop").glob("*.c")),
        src / "arch/enter.S",
        src / "arch/usermode.S",
        d / "media.S",
    ]
    for bits in (32, 64):
        objs = []
        flags = (
            ["-m32", "-march=i386", "-fno-pie", "-msoft-float"]
            if bits == 32
            else ["-m64", "-mno-red-zone", "-fpie", "-mgeneral-regs-only"]
        )
        for f in [*common, src / "arch" / ("bios.c" if bits == 32 else "efi.c")]:
            o = f"{f.stem}{bits}.o"
            run("gcc", *flags, *opts, "-c", f, "-o", o)
            objs.append(o)
        if bits == 32:
            run(
                "ld",
                "-m",
                "elf_i386",
                "-T",
                src / "arch/core32.ld",
                "-o",
                "kernel.elf",
                *objs,
            )
            run("objcopy", "-O", "binary", "kernel.elf", "kernel.bin")
            run("gcc", "-m32", *opts, "-c", src / "arch/optical.S", "-o", "optical.o")
            run(
                "ld",
                "-m",
                "elf_i386",
                "-Ttext",
                "0x7c00",
                "-e",
                "optical",
                "--oformat",
                "binary",
                "optical.o",
                "-o",
                "loader.bin",
            )
        else:
            rel = subprocess.check_output(["objdump", "-r", *objs], cwd=d)
            assert b"GOTPCREL" not in rel, "ELF GOT reference in firmware executable"
            run(
                "ld",
                "-mi386pep",
                "--image-base",
                "0x1000000",
                "--subsystem",
                "10",
                "--no-insert-timestamp",
                "-e",
                "efi_main",
                *objs,
                "-o",
                "BOOTX64.EFI",
            )
    run(
        "gcc",
        "-m32",
        *opts,
        "-DHARD_DISK",
        "-c",
        src / "arch/optical.S",
        "-o",
        "stage.o",
    )
    run(
        "ld",
        "-m",
        "elf_i386",
        "-Ttext",
        "0x7c00",
        "-e",
        "optical",
        "--oformat",
        "binary",
        "stage.o",
        "-o",
        "stage.bin",
    )
    run("as", "--32", src / "arch/disk.S", "-o", "disk.o")
    run(
        "ld",
        "-m",
        "elf_i386",
        "-Ttext",
        "0x600",
        "-e",
        "disk",
        "--oformat",
        "binary",
        "disk.o",
        "-o",
        "mbr.bin",
    )
    bios = (d / "loader.bin").read_bytes() + (d / "kernel.bin").read_bytes()
    assert (d / "loader.bin").stat().st_size == 2048 and len(bios) < 2049 * 2048
    iso = pycdlib.PyCdlib()
    iso.new(vol_ident="FRESHOS_STRATUS", interchange_level=3)
    streams = []

    def file(name, b):
        streams.append(io.BytesIO(b))
        iso.add_fp(streams[-1], len(b), "/" + name + ";1")

    file("BIOS.BIN", bios)
    file("EFI.IMG", esp((d / "BOOTX64.EFI").read_bytes()))
    file("EDITION.TXT", f"FreshOS 2 Stratus / {e}\r\n".encode())
    iso.add_eltorito("/BIOS.BIN;1", boot_load_size=4, boot_info_table=True)
    iso.add_eltorito("/EFI.IMG;1", efi=True)
    path = d / f"FreshOS-{e}.iso"
    iso.write(str(path))
    iso.close()
    # The first 32 KiB of ISO9660 are the system area. Optical boots ignore
    # this MBR; the installer publishes it last on the marked ATA target.
    iso = pycdlib.PyCdlib()
    iso.open(str(path))
    lba = iso.get_record(iso_path="/BIOS.BIN;1").extent_location()
    efi_lba = iso.get_record(iso_path="/EFI.IMG;1").extent_location()
    iso.close()
    mbr = bytearray((d / "mbr.bin").read_bytes())
    stage = bytearray((d / "stage.bin").read_bytes())
    struct.pack_into("<II", stage, 12, lba, len(bios))
    struct.pack_into("<I", mbr, 416, path.stat().st_size // 512)
    assert path.stat().st_size // 512 < 40960
    for off, kind, first, count in [
        (446, 0xEF, efi_lba * 4, 8192),
        (462, 0xDA, 40960, 65536 - 40960),
    ]:
        struct.pack_into(
            "<B3sB3sII",
            mbr,
            off,
            0,
            b"\xfe\xff\xff",
            kind,
            b"\xfe\xff\xff",
            first,
            count,
        )
    with path.open("r+b") as f:
        f.write(mbr)
        f.write(stage)
    with (d / "Home.img").open("wb") as f:
        marker = b"FRESHOS2/HOME/01" + struct.pack("<I", 257)
        f.write(marker)
        f.seek(40960 * 512)
        f.write(marker)
        f.truncate(32 * 1024 * 1024)
    seed(d)
    if vm:
        (d / "Home.vmdk").write_text(
            '# Disk DescriptorFile\nversion=1\nencoding="UTF-8"\nCID=21000001\nparentCID=ffffffff\ncreateType="monolithicFlat"\nRW 65536 FLAT "Home.img" 0\nddb.adapterType="ide"\nddb.geometry.cylinders="64"\nddb.geometry.heads="16"\nddb.geometry.sectors="64"\n'
        )
        (d / "FreshOS.vmx").write_text(
            f'.encoding = "UTF-8"\nconfig.version = "8"\nvirtualHW.version = "14"\ndisplayName = "FreshOS 2 - {e}"\nguestOS = "other"\nmemsize = "{32 if lite else 96}"\nfirmware = "bios"\nide0:0.present = "TRUE"\nide0:0.fileName = "Home.vmdk"\nide1:0.present = "TRUE"\nide1:0.deviceType = "cdrom-image"\nide1:0.fileName = "{path.name}"\nbios.bootOrder = "cdrom,hdd"\nusb.present = "FALSE"\nethernet0.present = "FALSE"\nserial0.present = "TRUE"\nserial0.fileType = "file"\nserial0.fileName = "stratus.log"\n'
        )
        bios = (d / "FreshOS.vmx").read_text()
        (d / "FreshOS-x64.vmx").write_text(
            bios.replace('firmware = "bios"', 'firmware = "efi"')
            .replace('guestOS = "other"', 'guestOS = "other-64"')
            .replace('memsize = "32"', 'memsize = "96"')
            .replace(f"FreshOS 2 - {e}", f"FreshOS 2 - {e} (native x64)")
        )
    package(e)


def package(e):
    d = ROOT / "build" / e
    out = ROOT / "distros"
    vm = "VMware" in e
    path = d / f"FreshOS-{e}.iso"
    digest = hashlib.sha256(path.read_bytes()).hexdigest() + "  " + path.name + "\n"
    (out / f"FreshOS-{e}.sha256").write_text(digest)
    with zipfile.ZipFile(out / f"FreshOS-{e}.zip", "w", zipfile.ZIP_DEFLATED) as z:
        for f in [
            path,
            d / "Home.img",
            ROOT / "README.md",
            ROOT / "docs/licenses/DejaVu.txt",
        ] + ([d / "Home.vmdk", d / "FreshOS.vmx", d / "FreshOS-x64.vmx"] if vm else []):
            z.write(f, f.name)
        for name in (
            "docs/imp.md",
            "forge/imp.py",
            "forge/appdisk.py",
            "native/vendor/fatfs/README.md",
            "native/compat/imports.def",
            "docs/native-processes.md",
            "docs/chrome-acceptance.md",
            "extras/cal/cal.exe",
            "extras/cal/cal.c",
            "extras/cal/build.sh",
            "extras/cal/LICENSE",
            "extras/cal/README.md",
        ):
            z.write(ROOT / name, name)
        z.writestr(
            "SHA256SUMS.txt",
            digest,
        )
    print(e, path.stat().st_size, "ISO bytes", flush=True)


if __name__ == "__main__":
    p = argparse.ArgumentParser()
    p.add_argument("edition", choices=(*EDITIONS, "all"))
    p.add_argument(
        "--pack-only",
        action="store_true",
        help="Repackage existing tested media without recompiling",
    )
    a = p.parse_args()
    for e in EDITIONS if a.edition == "all" else [a.edition]:
        (package if a.pack_only else build)(e)
