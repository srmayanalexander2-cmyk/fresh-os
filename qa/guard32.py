"""Execute the PE32 guard check in a real guest, including its rejection path."""

import argparse
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path
import pefile
from boot import Guest, ROOT

sys.path.insert(0, str(ROOT / "forge"))
import appdisk


def check(edition):
    src = ROOT / "build" / edition
    with tempfile.TemporaryDirectory() as t:
        d = Path(t)
        files = []
        for name in ["valid", "reject"]:
            obj, exe = d / "test.o", d / f"{name}.exe"
            opts = ["-DREJECTED"] if name == "reject" else []
            subprocess.run(
                ["gcc", "-m32", "-c", *opts, ROOT / "qa/guard32.S", "-o", obj],
                check=True,
            )
            subprocess.run(
                [
                    "ld",
                    "-mi386pe",
                    "--image-base",
                    "0x400000",
                    "-s",
                    "--no-insert-timestamp",
                    "-e",
                    "start",
                    obj,
                    "-o",
                    exe,
                ],
                check=True,
            )
            pe = pefile.PE(str(exe), fast_load=True)
            cfg = next(s for s in pe.sections if s.Name.rstrip(b"\0") == b".cfg")
            entry = pe.OPTIONAL_HEADER.DATA_DIRECTORY[10]
            entry.VirtualAddress, entry.Size = cfg.VirtualAddress, 92
            exe.write_bytes(pe.write())
            files.append((exe.name, exe))
        h = d / "Home.img"
        shutil.copyfile(src / "Home.img", h)
        vol = d / "Data.img"
        appdisk.create(vol, files)
        with Guest(
            d,
            src / f"FreshOS-{edition}.iso",
            h,
            True,
            ["-cpu", "max", "-drive", f"file={vol},format=raw,if=ide,index=1"],
        ) as g:
            g.wait("workspace ready")
            g.key("f3")
            for name, want in [
                ("valid", "exit 0"),
                ("reject", "failed PE GuardCF rejected indirect target"),
            ]:
                g.type(f"runfile {name}.exe\n")
                g.wait(f"IMP: {name}.exe {want}")
        print(
            "PASS: PE32 guard accepts the listed target, rejects an unlisted target, initializes security cookie"
        )


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("edition")
    check(parser.parse_args().edition)
