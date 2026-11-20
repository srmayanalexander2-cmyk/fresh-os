"""Build a real PE32+ EFI executable and a small FAT16 removable-media ESP."""

import struct, subprocess
from pathlib import Path
from editions import lite
from art import ROOT


def fat16(binary):
    # 8 MiB, two FATs. All non-root directory entries use ordinary short names.
    sectors = 16384
    fatsecs = 64
    root = 129
    data = root + 32
    img = bytearray(sectors * 512)
    img[:3] = b"\xeb\x3c\x90"
    img[3:11] = b"FRESHOS "
    struct.pack_into(
        "<HBHBHHBHHHII",
        img,
        11,
        512,
        1,
        1,
        2,
        512,
        sectors,
        0xF8,
        fatsecs,
        32,
        64,
        0,
        0,
    )
    img[36:39] = bytes([0x80, 0, 0x29])
    img[43:54] = b"FRESHOS EFI"
    img[54:62] = b"FAT16   "
    img[510:512] = b"\x55\xaa"
    fat = bytearray(512 * fatsecs)
    struct.pack_into("<HHHH", fat, 0, 0xFFF8, 0xFFFF, 0xFFFF, 0xFFFF)

    def ent(offset, name, attr, cluster, size=0):
        img[offset : offset + 32] = struct.pack("<11sB14xHI", name, attr, cluster, size)

    block = lambda cl: (data + cl - 2) * 512
    ent(root * 512, b"EFI        ", 16, 2)
    ent(block(2), b".          ", 16, 2)
    ent(block(2) + 32, b"..         ", 16, 0)
    ent(block(2) + 64, b"BOOT       ", 16, 3)
    ent(block(3), b".          ", 16, 3)
    ent(block(3) + 32, b"..         ", 16, 2)
    ent(block(3) + 64, b"BOOTX64 EFI", 32, 4, len(binary))
    count = (len(binary) + 511) // 512
    if block(4 + count) > len(img):
        raise ValueError("EFI executable does not fit ESP")
    for i in range(count):
        struct.pack_into("<H", fat, 2 * (4 + i), 0xFFFF if i == count - 1 else 5 + i)
    img[512 : 512 + len(fat)] = fat
    img[512 + len(fat) : 512 + 2 * len(fat)] = fat
    img[block(4) : block(4) + len(binary)] = binary
    return img


def efi(build, edition):
    def run(*args):
        subprocess.run(args, cwd=build, check=True)

    objs = []
    for src in [
        ROOT / "boot/uefi.c",
        ROOT / "kernel/entry64.S",
        ROOT / "kernel/platform.c",
        ROOT / "kernel/shell.c",
        ROOT / "kernel/storage.c",
        ROOT / "kernel/filetools.c",
        ROOT / "kernel/drivers.c",
        ROOT / "kernel/memory.c",
        ROOT / "kernel/pe.c",
        build / "art.S",
    ]:
        obj = src.stem + "64.o"
        objs.append(obj)
        run(
            "gcc",
            "-m64",
            "-Os",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-ffreestanding",
            "-fpie",
            "-mno-red-zone",
            "-mgeneral-regs-only",
            "-fno-asynchronous-unwind-tables",
            "-fno-stack-protector",
            "-fno-builtin",
            "-fno-ident",
            "-fshort-wchar",
            "-DEFI",
            f'-DEDITION="{edition}"',
            f"-DLITE={int(lite(edition))}",
            "-I.",
            f"-I{ROOT}/kernel",
            "-c",
            src,
            "-o",
            obj,
        )
    run(
        "ld",
        "-mi386pep",
        "--subsystem",
        "10",
        "--no-insert-timestamp",
        "--image-base",
        "0x1000000",
        "-e",
        "efi_main",
        "-o",
        "BOOTX64.EFI",
        *objs,
    )
    return fat16((build / "BOOTX64.EFI").read_bytes())
