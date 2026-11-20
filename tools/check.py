"""Inspect the hybrid image without reading gigabytes of unused Home space."""

import argparse, io, struct
from pathlib import Path
import pycdlib
from home import Home


def verify(path):
    with path.open("rb") as f:
        m = f.read(512)
        assert m[510:] == b"\x55\xaa"
        assert path.stat().st_size == 8_000_000_000
        assert m[450] == 0xEF and m[466] == 0xDA
        h = Home(f)
        assert h.count == 0
        iso = pycdlib.PyCdlib()
        iso.open(str(path))

        def load(name):
            out = io.BytesIO()
            iso.get_file_from_iso_fp(out, iso_path="/" + name + ";1")
            return out.getvalue()

        boot = load("FRESHOS.BIN")
        assert b"FreshOS IMP1" in load("RELEASE.TXT")
        names = [name for _, _, files in iso.walk(iso_path="/") for name in files]
        assert not any(".APK" in name for name in names)
        samples = [
            load(name)
            for name in ("HELLO32.IMP", "HELLO64.IMP", "HELLO32.EXE", "HELLO64.EXE")
        ]
        for image, bits in zip(samples, (32, 64)):
            assert image[:8] == b"FIMP" + bytes((1, bits, 1, 32))
        for sample, machine in zip(samples[2:], (0x14C, 0x8664)):
            pe = struct.unpack_from("<I", sample, 60)[0]
            assert (
                sample[pe : pe + 4] == b"PE\0\0"
                and struct.unpack_from("<H", sample, pe + 4)[0] == machine
            )
        samples.extend([load("POSTCARD.EXE"), load("LINUX.ELF"), load("CHIME.WAV")])
        assert samples[-2][:4] == b"\x7fELF" and samples[-1][:4] == b"RIFF"
        assert all(s in boot[2048:] for s in samples)
        lba = iso.get_record(iso_path="/FRESHOS.BIN;1").extent_location()
        assert (
            struct.unpack_from("<III", boot, 8) == (16, lba, len(boot))
            and struct.unpack_from("<Q", m, 128)[0] == 4 * lba
        )
        assert b"FreshOS kernel entered" in boot[2048:]
        esp = load("EFI.IMG")
        e = iso.get_record(iso_path="/EFI.IMG;1").extent_location()
        assert struct.unpack_from("<II", m, 454) == (e * 4, len(esp) // 512)
        assert esp[54:62] == b"FAT16   "
        data = 161 * 512
        entry = data + 512 + 64
        assert esp[entry : entry + 11] == b"BOOTX64 EFI"
        cl, size = struct.unpack_from("<HI", esp, entry + 26)
        off = data + (cl - 2) * 512
        pe = esp[off : off + size]
        assert pe[:2] == b"MZ"
        p = struct.unpack_from("<I", pe, 60)[0]
        assert (
            pe[p : p + 6] == b"PE\0\0\x64\x86"
            and struct.unpack_from("<H", pe, p + 24 + 68)[0] == 10
        )
        cat = load("BOOT.CAT")
        assert sum(struct.unpack("<16H", cat[:32])) % 65536 == 0
        assert cat[32] == 0x88 and cat[65] == 0xEF and cat[96] == 0x88
        assert all(s in pe for s in samples)
        iso.close()
        print(
            f"PASS  {path.name}: hybrid MBR, 8 GB capacity, dual boot catalog, native BIOS payload, EFI executable and Fresh Home"
        )


if __name__ == "__main__":
    p = argparse.ArgumentParser()
    p.add_argument("iso", type=Path)
    verify(p.parse_args().iso)
