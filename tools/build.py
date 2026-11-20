#!/usr/bin/env python3
"""Build the native x86 live image. ISO data after the payload is reserved capacity."""

import argparse, io, os, struct, subprocess, time, zipfile
from unittest.mock import patch
from pathlib import Path
import pycdlib
from art import bake, ROOT
from efi import efi
from exe_examples import make as make_exes
from vmdk import make as make_vmdk

EDITIONS = ("Normal", "Lite", "VMware")
CAPACITY = 8_000_000_000


def run(args, cwd):
    subprocess.run([str(a) for a in args], cwd=cwd, check=True)


def build(edition, wallpaper=None, compact=False, apk=None):
    b = ROOT / "build" / edition
    b.mkdir(parents=True, exist_ok=True)
    out = ROOT / "out"
    out.mkdir(exist_ok=True)
    bake(b, edition == "Lite", wallpaper)
    package = (apk or ROOT / "examples/Postcard.apk").read_bytes()
    if not 0 < len(package) <= 2 << 20:
        raise ValueError("APK limit: 2 MiB")
    with zipfile.ZipFile(io.BytesIO(package)) as z:
        for name, limit in [
            ("classes.dex", 1 << 20),
            ("AndroidManifest.xml", 128 << 10),
        ]:
            if z.getinfo(name).file_size > limit:
                raise ValueError(f"{name} exceeds runtime limit")
    (b / "base.apk").write_bytes(package)
    make_exes(b)
    payloads = {
        "base_apk": "base.apk",
        "hello32": "hello32.exe",
        "hello64": "hello64.exe",
    }
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
        ROOT / "kernel/entry.S",
        ROOT / "kernel/platform.c",
        ROOT / "kernel/shell.c",
        ROOT / "kernel/storage.c",
        ROOT / "kernel/memory.c",
        ROOT / "kernel/android.c",
        ROOT / "kernel/apk.c",
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
                f'-DLITE={int(edition=="Lite")}',
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
    release = (ROOT / "docs/release-1.md").read_bytes()
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
    if edition == "VMware":
        make_vmdk(out / "FreshTarget.vmdk")
        (out / "FreshOS.vmx").write_text(""" .encoding = "UTF-8"
config.version = "8"
virtualHW.version = "14"
displayName = "FreshOS Release 1"
guestOS = "other"
firmware = "bios"
memsize = "64"
ide0:0.present = "TRUE"
ide0:0.fileName = "FreshTarget.vmdk"
ide1:0.present = "TRUE"
ide1:0.deviceType = "cdrom-image"
ide1:0.fileName = "FreshOS-VMware.iso"
ide1:0.startConnected = "TRUE"
bios.bootOrder = "cdrom"
usb.present = "FALSE"
""".lstrip())
    return path


if __name__ == "__main__":
    p = argparse.ArgumentParser()
    p.add_argument("edition", choices=(*EDITIONS, "all"), default="all", nargs="?")
    p.add_argument("--wallpaper", type=Path)
    p.add_argument("--apk", type=Path, help="APK with a supported text Activity")
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
            build(e, args.wallpaper, args.compact, args.apk)
