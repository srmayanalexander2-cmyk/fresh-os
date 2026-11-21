"""Confirm native installation, compare system sectors, then boot disk-only."""

import argparse, shutil, tempfile
from pathlib import Path
from boot import Guest, ROOT


def check(edition, efi):
    src = ROOT / "build" / edition
    iso = src / f"FreshOS-{edition}.iso"
    with tempfile.TemporaryDirectory() as tmp:
        d = Path(tmp)
        disk = d / "home.raw"
        shutil.copyfile(src / "Home.img", disk)
        with Guest(d, iso, disk, efi) as g:
            g.wait("workspace ready")
            g.key("f3")
            g.type("install\n")
            g.wait("Confirm Install")
            assert disk.read_bytes()[:16] == b"FRESHOS2/HOME/01"
            g.key("i")
            g.wait("install verified", 120)
        with disk.open("rb") as f:
            assert f.read(iso.stat().st_size) == iso.read_bytes()
            f.seek(40960 * 512)
            assert f.read(16) == b"FRESHOS2/HOME/01"
        with Guest(d, None, disk, efi) as g:
            g.wait("workspace ready")
            g.key("f3")
            g.type("new disk.txt\n")
            g.type("installed")
            g.key("ctrl-s")
            g.wait("Home snapshot committed")
        assert b"installed\0" in disk.read_bytes()[40960 * 512 :]
    print(
        "PASS native install, exact disk verification, disk-only boot and document save /",
        edition,
        "UEFI" if efi else "BIOS",
    )


if __name__ == "__main__":
    p = argparse.ArgumentParser()
    p.add_argument("edition")
    p.add_argument("--efi", action="store_true")
    a = p.parse_args()
    check(a.edition, a.efi)
