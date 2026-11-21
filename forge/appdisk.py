"""Create a NEW, separately marked sparse 384 MiB FAT16 volume for native apps.

Attach as a second IDE hard disk (not the Home.img). Only create new regular
files; never format an existing volume. Optional input files go in its root.
"""

import argparse
import re
import shutil
import struct
from pathlib import Path

BASE, SECTORS, FAT, SPC = 2048, 786432, 192, 16
ROOT = 1 + 2 * FAT
DATA = ROOT + 32


def create(path, inputs):
    entries, used = [], set()
    cluster = 2
    tab = bytearray(FAT * 512)
    struct.pack_into("<HH", tab, 0, 0xFFF8, 0xFFFF)
    for name, source in inputs:
        name = name.upper()
        if (
            not re.fullmatch(r"[A-Z0-9_-]{1,8}(\.[A-Z0-9_-]{1,3})?", name)
            or name in used
            or len(entries) >= 512
        ):
            raise ValueError("Root files must have unique DOS 8.3 names")
        used.add(name)
        size = source.stat().st_size
        n = (size + SPC * 512 - 1) // (SPC * 512)
        if not source.is_file() or cluster - 2 + n > (SECTORS - DATA) // SPC:
            raise ValueError("Input does not fit")
        entries.append((name, source, size, cluster if n else 0))
        for i in range(n):
            struct.pack_into(
                "<H", tab, 2 * (cluster + i), cluster + i + 1 if i + 1 < n else 0xFFFF
            )
        cluster += n
    with path.open("xb") as f:
        f.truncate((BASE + SECTORS) * 512)
        f.write(struct.pack("<16sII", b"STRATUS/APPDATA1", BASE, SECTORS))
        b = bytearray(512)
        b[:11] = b"\xeb\x3c\x90STRATUS2"
        struct.pack_into(
            "<HBHBHHBHHHII",
            b,
            11,
            512,
            SPC,
            1,
            2,
            512,
            0,
            248,
            FAT,
            63,
            255,
            BASE,
            SECTORS,
        )
        b[36:39] = bytes([128, 0, 41])
        b[43:54] = b"APP DATA   "
        b[54:62] = b"FAT16   "
        b[510:] = b"\x55\xaa"
        f.seek(BASE * 512)
        f.write(b)
        f.write(tab)
        f.write(tab)
        for i, (name, source, size, c) in enumerate(entries):
            stem, _, ext = name.partition(".")
            f.seek((BASE + ROOT) * 512 + i * 32)
            f.write(
                struct.pack(
                    "<11sB14xHI", (stem.ljust(8) + ext.ljust(3)).encode(), 32, c, size
                )
            )
            if size:
                f.seek((BASE + DATA + (c - 2) * SPC) * 512)
                with source.open("rb") as inp:
                    left = size
                    while left:
                        chunk = inp.read(min(1 << 20, left))
                        if not chunk:
                            raise ValueError("Input changed while copying")
                        f.write(chunk)
                        left -= len(chunk)


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("disk", type=Path)
    p.add_argument("input", type=Path, nargs="*")
    a = p.parse_args()
    create(a.disk, [(f.name, f) for f in a.input])
