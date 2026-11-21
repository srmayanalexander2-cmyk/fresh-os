#!/usr/bin/env python3
"""Build the native x86 live image. ISO data after the payload is reserved capacity."""

import argparse, io, os, struct, subprocess, time
from unittest.mock import patch
from pathlib import Path
from netbuild import NET_SOURCES, NET_FLAGS
import pycdlib
from art import bake, ROOT
from efi import efi
from exe_examples import make as make_exes
from vmdk import make as make_vmdk

from editions import EDITIONS, lite, vmware

CAPACITY = 8_000_000_000


def run(args, cwd):
    subprocess.run([str(a) for a in args], cwd=cwd, check=True)


def build(edition, wallpaper=None, compact=False):
    b = ROOT / "build" / edition
    b.mkdir(parents=True, exist_ok=True)
    out = ROOT / "out"
    out.mkdir(exist_ok=True)
    bake(b, lite(edition), wallpaper)
    make_exes(b)
    # Original two-note interface sound: real mono PCM shared by Files and the host.
    import wave, math

    with wave.open(str(b / "chime.wav"), "wb") as w:
        w.setparams((1, 2, 24000, 0, "NONE", "PCM"))
        w.writeframes(
            b"".join(
                struct.pack(
                    "<h",
                    round(
                        4400
                        * (
                            math.sin(2 * math.pi * 392 * i / 24000)
                            + 0.28 * math.sin(2 * math.pi * 587.33 * i / 24000)
                        )
                        * min(1, i / 700)
                        * ((9600 - i) / 9600) ** 2
                    ),
                )
                for i in range(9600)
            )
        )
    run(
        [
            "gcc",
            "-std=c11",
            "-O2",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-I" + str(ROOT / "kernel"),
            ROOT / "tools/imp-cli.c",
            ROOT / "kernel/pe.c",
            "-o",
            "imp",
        ],
        b,
    )
    run(["as", "--64", ROOT / "examples/linux/hello.S", "-o", "hello-linux.o"], b)
    run(
        [
            "ld",
            "-m",
            "elf_x86_64",
            "--build-id=none",
            "-s",
            "hello-linux.o",
            "-o",
            "linux.elf",
        ],
        b,
    )
    payloads = {
        "linuxdemo": "linux.elf",
        "postcard": "postcard.exe",
        "chime": "chime.wav",
    }
    for bits in (32, 64):
        dest = b / f"hello{bits}.imp"
        dest.unlink(missing_ok=True)
        run([b / "imp", "convert", b / f"hello{bits}.exe", dest], b)
        payloads["hello" + str(bits)] = f"hello{bits}.exe"
        payloads["imp" + str(bits)] = dest.name
    (b / "packages.h").write_text(
        "".join(
            f"extern const u8 {sym}[];\n#define {sym.upper()}_SIZE {(b/name).stat().st_size}u\n"
            for sym, name in payloads.items()
        )
    )
    (b / "art.S").write_text(
        '.section .rodata\n.global art\n.balign 4\nart: .incbin "art.bin"\n'
        + "".join(
            f'.global {sym}\n{sym}: .incbin "{name}"\n'
            for sym, name in payloads.items()
        )
        + '.section .note.GNU-stack,"",@progbits\n'
    )
    for f in ["cd", "mbr"]:
        run(["as", "--32", ROOT / "boot" / f"{f}.S", "-o", f + ".o"], b)
        run(
            [
                "ld",
                "-m",
                "elf_i386",
                "-Ttext",
                "0x7c00" if f == "cd" else "0x600",
                "--oformat",
                "binary",
                "-o",
                f + ".bin",
                f + ".o",
            ],
            b,
        )
    objects = []
    for source in [
        *NET_SOURCES,
        ROOT / "kernel/entry.S",
        ROOT / "kernel/platform.c",
        ROOT / "kernel/shell.c",
        ROOT / "kernel/storage.c",
        ROOT / "kernel/filetools.c",
        ROOT / "kernel/drivers.c",
        ROOT / "kernel/memory.c",
        ROOT / "kernel/pe.c",
        b / "art.S",
    ]:
        obj = source.stem + ".o"
        objects.append(obj)
        run(
            [
                "gcc",
                "-Wall",
                "-Wextra",
                "-Werror",
                "-m32",
                *NET_FLAGS,
                "-march=i386",
                "-msoft-float",
                "-mno-sse",
                "-std=c11",
                "-Os",
                "-ffreestanding",
                "-fno-builtin",
                "-fno-pic",
                "-fno-pie",
                "-fno-stack-protector",
                "-fno-asynchronous-unwind-tables",
                "-I.",
                f'-DEDITION="{edition}"',
                f"-DLITE={int(lite(edition))}",
                "-c",
                source,
                "-o",
                obj,
            ],
            b,
        )
    run(
        [
            "ld",
            "-m",
            "elf_i386",
            "-T",
            ROOT / "kernel/link.ld",
            "--build-id=none",
            "-o",
            "kernel.elf",
            *objects,
        ],
        b,
    )
    run(["objcopy", "-O", "binary", "kernel.elf", "kernel.bin"], b)
    boot = (b / "cd.bin").read_bytes() + (b / "kernel.bin").read_bytes()
    iso = pycdlib.PyCdlib()
    iso.new(vol_ident="FRESHOS_" + edition.upper())
    iso.add_fp(io.BytesIO(boot), len(boot), "/FRESHOS.BIN;1")
    iso.add_eltorito("/FRESHOS.BIN;1", boot_load_size=4, boot_info_table=True)
    esp = efi(b, edition)
    iso.add_fp(io.BytesIO(esp), len(esp), "/EFI.IMG;1")
    iso.add_eltorito("/EFI.IMG;1", efi=True)
    for name in payloads.values():
        data = (b / name).read_bytes()
        iso.add_fp(io.BytesIO(data), len(data), "/" + name.upper() + ";1")
    release = (ROOT / "docs/imp.md").read_bytes()
    iso.add_fp(io.BytesIO(release), len(release), "/RELEASE.TXT;1")
    info = (ROOT / "README.md").read_bytes()
    iso.add_fp(io.BytesIO(info), len(info), "/README.TXT;1")
    notice = (ROOT / "docs/licenses/DejaVu.txt").read_bytes()
    iso.add_fp(io.BytesIO(notice), len(notice), "/FONT.TXT;1")
    path = out / f"FreshOS-{edition}.iso"
    iso.write(str(path))
    iso.close()
    iso = pycdlib.PyCdlib()
    iso.open(str(path))
    bootlba = iso.get_record(iso_path="/FRESHOS.BIN;1").extent_location()
    esp_lba = iso.get_record(iso_path="/EFI.IMG;1").extent_location()
    iso.close()
    mbr = bytearray(512)
    stub = (b / "mbr.bin").read_bytes()
    mbr[: len(stub)] = stub
    struct.pack_into("<Q", mbr, 128, bootlba * 4)
    mbr[510:] = b"\x55\xaa"
    # Preview/smoke images carry the same executable, without the release capacity reservation.
    size = path.stat().st_size if compact else CAPACITY
    mbr[368:383] = b"FreshOS live v1"
    part = lambda active, t, lba, sectors: struct.pack(
        "<B3sB3sII", active, b"\xfe\xff\xff", t, b"\xfe\xff\xff", lba, sectors
    )
    mbr[446:462] = part(0x80, 0xEF, esp_lba * 4, len(esp) // 512)
    with path.open("r+b") as f:
        if not compact:
            start = 32768
            if path.stat().st_size > start * 512:
                raise ValueError("ISO overlaps Home partition")
            mbr[462:478] = part(0, 0xDA, start, size // 512 - start)
            block = bytearray(512)
            struct.pack_into("<8sIIII", block, 0, b"FRSHOME1", 0, start + 2, 0, 0)
            h = 2166136261
            for byte in block[:24]:
                h = ((h ^ byte) * 16777619) & 0xFFFFFFFF
            struct.pack_into("<I", block, 24, h)
            f.seek(start * 512)
            f.write(block * 2)
        f.seek(0)
        f.write(mbr)
        f.truncate(size)
    print(
        f"{edition}: {path} ({size:,} bytes; {len(boot):,} bytes boot payload)",
        flush=True,
    )
    if vmware(edition):
        profile = b / "vm"
        profile.mkdir(exist_ok=True)
        make_vmdk(profile / "FreshTarget.vmdk")
        (profile / "FreshOS.vmx").write_text(f""" .encoding = "UTF-8"
config.version = "8"
virtualHW.version = "14"
displayName = "FreshOS Current - {edition}"
guestOS = "other"
firmware = "bios"
memsize = "{32 if lite(edition) else 64}"
ide0:0.present = "TRUE"
ide0:0.fileName = "FreshTarget.vmdk"
ide0:0.deviceType = "disk"
ide0:0.startConnected = "TRUE"
ide1:0.present = "TRUE"
ide1:0.deviceType = "cdrom-image"
ide1:0.fileName = "FreshOS-{edition}.iso"
ide1:0.startConnected = "TRUE"
bios.bootOrder = "cdrom,hdd"
serial0.present = "TRUE"
serial0.fileType = "file"
serial0.fileName = "FreshOS-serial.log"
ethernet0.present = "TRUE"
ethernet0.virtualDev = "vlance"
ethernet0.connectionType = "nat"
usb.present = "FALSE"
""".lstrip())
    return path


if __name__ == "__main__":
    p = argparse.ArgumentParser()
    p.add_argument("edition", choices=(*EDITIONS, "all"), default="all", nargs="?")
    p.add_argument("--wallpaper", type=Path)
    p.add_argument(
        "--compact", action="store_true", help="omit reserved media capacity"
    )
    args = p.parse_args()
    os.environ["TZ"] = "UTC"
    time.tzset()
    with patch(
        "time.time", return_value=int(os.environ.get("SOURCE_DATE_EPOCH", 1795132800))
    ):
        for e in EDITIONS if args.edition == "all" else [args.edition]:
            build(e, args.wallpaper, args.compact)
