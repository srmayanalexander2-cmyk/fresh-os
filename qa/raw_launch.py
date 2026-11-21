"""Attempt a direct native launch of a supplied PE, without packing or extraction.

An exit status of zero means only that a FreshOS guest process exited zero.
This probe cannot verify that an installer installed a working browser.
"""

import argparse
import hashlib
import shutil
import sys
import tempfile
from pathlib import Path

from boot import Guest, ROOT

sys.path.insert(0, str(ROOT / "forge"))
import appdisk


def launch(edition, exe):
    media = ROOT / "build" / edition
    with tempfile.TemporaryDirectory() as tmp:
        d = Path(tmp)
        home, vol = d / "Home.img", d / "Data.img"
        shutil.copyfile(media / "Home.img", home)
        appdisk.create(vol, [("PROBE.EXE", exe)])
        with exe.open("rb") as f:
            print(
                f"{exe.name} SHA-256 {hashlib.file_digest(f, 'sha256').hexdigest()}",
                flush=True,
            )
        with Guest(
            d,
            media / f"FreshOS-{edition}.iso",
            home,
            True,
            ["-drive", f"file={vol},format=raw,if=ide,index=1"],
        ) as g:
            g.wait("workspace ready")
            g.wait("App data disk detected")
            g.key("f3")
            g.type("runfile probe.exe\n")
            prefix = "IMP: probe.exe "
            g.wait(prefix, 120)
            status = next(
                s for s in g.log.read_text().splitlines() if s.startswith(prefix)
            )
            print(status)
            return status == prefix + "exit 0"


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("edition")
    p.add_argument("exe", type=Path)
    a = p.parse_args()
    sys.exit(not launch(a.edition, a.exe))
