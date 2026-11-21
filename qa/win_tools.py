"""Exercise real upstream PE64 programs through the x64 guest terminal."""

import argparse
import shutil
import struct
import sys
import tempfile
import time
from pathlib import Path
from boot import Guest, ROOT

sys.path.insert(0, str(ROOT / "forge"))
from imp import install, pack


def check(edition, exes):
    media = ROOT / "build" / edition
    with tempfile.TemporaryDirectory() as tmp:
        d = Path(tmp)
        home = d / "test.img"
        shutil.copyfile(media / "Home.img", home)
        sample = d / "sample.nef"
        # Minimal little-endian byte sequence recognized by the upstream heuristic.
        sample.write_bytes(struct.pack("<III", 0x400A7, 1, 31415))
        install(home, "ftype", pack(exes / "getfiletype.exe", [], [], True))
        install(home, "shutter", pack(exes / "shuttercount.exe", [], [sample], True))
        with Guest(d, media / f"FreshOS-{edition}.iso", home, True) as g:
            g.wait("workspace ready")
            g.key("f3")
            g.type("run ftype\n")
            g.wait("IMP: ftype exit 0\nFILE_TYPE_CHAR")
            for file, expected, status in [
                ("sample.nef", "Shutter Count: 31,415", 0),
                ("absent.nef", "Could not determine value!", 1),
            ]:
                # Each invocation must display its own modal before acknowledgment.
                start = g.log.stat().st_size
                g.type(f"run shutter {file}\n")
                end = time.monotonic() + 15
                while "Win32 dialog: shutter" not in g.log.read_bytes()[start:].decode(
                    errors="replace"
                ):
                    if time.monotonic() > end:
                        raise AssertionError(g.log.read_text(errors="replace")[-800:])
                    time.sleep(0.05)
                g.key("ret")
                g.wait(f"IMP: shutter exit {status}")
                g.wait(expected)
    print("PASS: native PE64 getfiletype and shuttercount (known count + missing file)")


if __name__ == "__main__":
    p = argparse.ArgumentParser()
    p.add_argument("edition")
    p.add_argument("exes", type=Path)
    args = p.parse_args()
    check(args.edition, args.exes)
