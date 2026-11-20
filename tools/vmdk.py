"""Create a VMware growable IDE target with exactly one allocated data grain."""

import struct
from pathlib import Path


def make(path, capacity=15625000):
    grain = 128
    per_table = 512
    nt = (capacity + grain * per_table - 1) // (grain * per_table)
    gdsize = (nt * 4 + 511) // 512
    rgd = 21
    gd = rgd + gdsize + nt * 4
    over = ((gd + gdsize + nt * 4 + grain - 1) // grain) * grain
    buf = bytearray((over + grain) * 512)
    struct.pack_into(
        "<IIIQQQQIQQQ",
        buf,
        0,
        0x564D444B,
        1,
        3,
        capacity,
        grain,
        1,
        20,
        per_table,
        rgd,
        gd,
        over,
    )
    buf[73:77] = bytes([10, 32, 13, 10])
    desc = f"""# Disk DescriptorFile
version=1
encoding="UTF-8"
CID=46524553
parentCID=ffffffff
createType="monolithicSparse"
RW {capacity} SPARSE "{path.name}"
ddb.adapterType = "ide"
ddb.geometry.cylinders = "{capacity//(16*63)}"
ddb.geometry.heads = "16"
ddb.geometry.sectors = "63"
""".encode()
    buf[512 : 512 + len(desc)] = desc
    for at in (gd, rgd):
        for i in range(nt):
            struct.pack_into("<I", buf, at * 512 + i * 4, at + gdsize + i * 4)
        struct.pack_into("<I", buf, (at + gdsize) * 512, over)
    token = b"FRESHOS:INSTALL-TARGET:v1"
    buf[over * 512 : over * 512 + len(token)] = token
    struct.pack_into("<I", buf, over * 512 + 32, capacity)
    path.write_bytes(buf)
