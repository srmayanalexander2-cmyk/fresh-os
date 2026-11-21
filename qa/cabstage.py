"""Boot the Win64 cabinet extractor; compare persisted output bytes to a reference."""

import argparse, hashlib, shutil, struct, sys, tempfile
from pathlib import Path
from boot import Guest, ROOT

sys.path.insert(0, str(ROOT / "forge"))
import appdisk
from imp import install, pack


def digest_member(vol):
    """Inspect via FAT chain, independent of the in-guest driver's f_read."""
    base, root, data, spc = appdisk.BASE, appdisk.ROOT, appdisk.DATA, appdisk.SPC
    with vol.open("rb") as f:

        def block(lba, n):
            f.seek((base + lba) * 512)
            return f.read(n)

        fat = block(1, appdisk.FAT * 512)

        def chain(c):
            seen = set()
            while (
                2 <= c < 0xFFF0
                and c not in seen
                and (c - 2) * spc + data < appdisk.SECTORS
            ):
                seen.add(c)
                yield block(data + (c - 2) * spc, spc * 512)
                c = struct.unpack_from("<H", fat, c * 2)[0]
            assert c >= 0xFFF8, "Invalid FAT16 cluster chain"

        def entry(buf, name):
            for i in range(0, len(buf), 32):
                if buf[i : i + 11] == name:
                    return struct.unpack_from("<HI", buf, i + 26)
            raise AssertionError(f"Missing {name}")

        directory, _ = entry(block(root, 32 * 512), b"STAGE      ")
        cluster, remaining = entry(b"".join(chain(directory)), b"SETUP   EXE")
        h = hashlib.sha256()
        for b in chain(cluster):
            n = min(len(b), remaining)
            h.update(b[:n])
            remaining -= n
        assert remaining == 0
        return h.hexdigest()


def calendar_cab(path):
    # A single uncompressed CFDATA block, with a genuine independent Windows EXE.
    data = (ROOT / "extras/cal/cal.exe").read_bytes()
    assert 0 < len(data) <= 32768
    file = struct.pack("<IIHHHH", len(data), 0, 0, 0, 0, 32) + b"setup.exe\0"
    start = 44 + len(file)
    size = start + 8 + len(data)
    header = struct.pack("<4s5IBB5H", b"MSCF", 0, size, 0, 44, 0, 3, 1, 1, 1, 0, 0, 0)
    path.write_bytes(
        b"fixture prefix"
        + header
        + struct.pack("<IHH", start, 1, 0)
        + file
        + struct.pack("<IHH", 0, len(data), len(data))
        + data
    )
    return hashlib.sha256(data).hexdigest()


def check(edition, exe, installer, expected):
    media = ROOT / "build" / edition
    with tempfile.TemporaryDirectory() as tmp:
        d = Path(tmp)
        home = d / "Home.img"
        vol = d / "Data.img"
        shutil.copyfile(media / "Home.img", home)
        sample = installer is None
        if sample:
            installer = d / "fixture.exe"
            expected = calendar_cab(installer)
        appdisk.create(vol, [("INSTALL.EXE", installer)])
        install(home, "cabstage", pack(exe, [], [], True))
        extra = ["-drive", f"file={vol},format=raw,if=ide,index=1"]
        with Guest(d, media / f"FreshOS-{edition}.iso", home, True, extra) as g:
            g.wait("workspace ready")
            g.wait("App data disk detected")
            g.key("f3")
            g.type("run cabstage\n")
            g.wait("IMP: cabstage ", 150)
            assert "IMP: cabstage exit 0" in g.log.read_text(), g.log.read_text()[
                -1200:
            ]
        digest = digest_member(vol)
        assert digest == expected, (digest, expected)
        with Guest(d, media / f"FreshOS-{edition}.iso", home, True, extra) as g:
            g.wait("workspace ready")
            g.key("f3")
            g.type("runfile stage")
            g.key("slash")
            g.type("setup.exe 2 2024\n" if sample else "setup.exe\n")
            if sample:
                g.wait("IMP: stage/setup.exe exit 0")
                g.wait("25 26 27 28 29")
            else:
                # The extraction hash is evidence of staging only. A loader
                # rejection or nonzero exit must be a failing launch test.
                g.wait("IMP: stage/setup.exe ", 120)
                status = next(
                    line
                    for line in g.log.read_text().splitlines()
                    if line.startswith("IMP: stage/setup.exe ")
                )
                print(f"Staged setup SHA256: {digest}\nCold boot: {status}", flush=True)
                if status != "IMP: stage/setup.exe exit 0":
                    raise SystemExit(
                        "Setup launch did not pass; Chrome was not verified."
                    )
        print(
            "PASS: extracted EXE ran after cold boot. This is not a browser acceptance test.",
            flush=True,
        )


if __name__ == "__main__":
    p = argparse.ArgumentParser()
    p.add_argument("edition")
    p.add_argument("exe", type=Path)
    p.add_argument("installer", type=Path, nargs="?")
    p.add_argument("sha256", nargs="?")
    a = p.parse_args()
    check(a.edition, a.exe, a.installer, a.sha256)
