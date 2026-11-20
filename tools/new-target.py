"""Create a NEW sparse install target (never open a device or existing file)."""

import argparse, struct
from pathlib import Path

CAPACITY = 8_000_000_000


def create(path):
    with path.open("xb") as f:
        block = bytearray(512)
        tag = b"FRESHOS:INSTALL-TARGET:v1"
        block[: len(tag)] = tag
        struct.pack_into("<I", block, 32, CAPACITY // 512)
        f.write(block)
        f.truncate(CAPACITY)
    print("New, prepared IDE disk:", path)


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("output", type=Path)
    create(p.parse_args().output)
