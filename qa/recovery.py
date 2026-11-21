"""Damage one copy in a disposable test volume; never treat foreign disks as targets."""

import tempfile, shutil
from pathlib import Path
from boot import Guest, ROOT


def run():
    src = ROOT / "build/Lite"
    iso = src / "FreshOS-Lite.iso"
    with tempfile.TemporaryDirectory() as tmp:
        d = Path(tmp)
        h = d / "home.raw"
        shutil.copyfile(src / "Home.img", h)
        with Guest(d, iso, h, False) as g:
            g.wait("workspace ready")
            g.key("f3")
            g.type("new recovered.txt\n")
            g.type("first")
            g.key("ctrl-s")
            g.wait("Home snapshot committed")
            g.type(" second")
            g.key("ctrl-s")
            g.wait("Document saved to Home")
            g.key("esc")
        with h.open("r+b") as f:
            f.seek((40960 + 129) * 512)
            assert f.read(8) == b"F2SNAP01"
            f.seek((40960 + 130) * 512)
            f.write(b"bad!")
        with Guest(d, iso, h, False) as g:
            g.wait("Recovered Home / writable")
            g.wait("workspace ready")
            g.key("f3")
            g.type("edit recovered.txt\n")
            g.wait("open: writer")
            g.type(" restored")
            g.key("ctrl-s")
            g.wait("Home snapshot committed")
        assert b"first restored\0" in h.read_bytes()[(40960 + 130) * 512 :]
        foreign = b"not a FreshOS medium".ljust(32 << 20, b".")
        h.write_bytes(foreign)
        with Guest(d, iso, h, False) as g:
            g.wait("workspace ready")
            g.key("f3")
            g.type("install\n")
            g.key("i")
            g.wait("Foreign disks are protected")
            g.key("f3")
            g.type("new live.txt\n")
            g.type("session")
            g.key("ctrl-s")
            g.wait("session only")
        assert h.read_bytes() == foreign
    print(
        "PASS fallback to previous Home generation, safe recommit, foreign disk byte-for-byte unchanged"
    )


if __name__ == "__main__":
    run()
