"""Boot an actual PE parent launching an installed unmodified cal.exe twice."""

import argparse, os, shutil, subprocess, sys, tempfile
from pathlib import Path
from boot import Guest, ROOT

sys.path.insert(0, str(ROOT / "forge"))
from imp import install, pack
import appdisk


def check(edition):
    media = ROOT / "build" / edition
    with tempfile.TemporaryDirectory() as tmp:
        d = Path(tmp)
        h = d / "Home.img"
        shutil.copyfile(media / "Home.img", h)
        cc = (
            [os.environ["ZIG"], "cc", "-target", "x86_64-windows-gnu"]
            if "ZIG" in os.environ
            else ["x86_64-w64-mingw32-gcc"]
        )
        subprocess.run(
            [
                *cc,
                "-Os",
                "-ffreestanding",
                "-fno-sanitize=all",
                "-fno-stack-protector",
                "-c",
                ROOT / "qa/child64.c",
                "-o",
                d / "parent.o",
            ],
            check=True,
        )
        exe = d / "parent.exe"
        subprocess.run(
            [
                "ld",
                "-mi386pep",
                "-s",
                "--no-insert-timestamp",
                "-e",
                "mainCRTStartup",
                "-o",
                exe,
                d / "parent.o",
                media / "pe64/KERNEL32.a",
            ],
            check=True,
        )
        install(h, "parent", pack(exe, [], [], True))
        vol = d / "Data.img"
        files = [("CAL.EXE", ROOT / "extras/cal/cal.exe")]
        for bits in [32, 64]:
            obj = d / f"tls{bits}.o"
            tls = d / f"tls{bits}.exe"
            opts = ["-D_WIN64"] if bits == 64 else []
            subprocess.run(
                ["gcc", f"-m{bits}", *opts, "-c", ROOT / "qa/tls.S", "-o", obj],
                check=True,
            )
            mode = "i386pep" if bits == 64 else "i386pe"
            subprocess.run(
                [
                    "ld",
                    "-m",
                    mode,
                    "--no-insert-timestamp",
                    "-s",
                    "-e",
                    "start",
                    obj,
                    media / f"pe{bits}/KERNEL32.a",
                    "-o",
                    tls,
                ],
                check=True,
            )
            files.append((tls.name, tls))
        appdisk.create(vol, files)
        with Guest(
            d,
            media / f"FreshOS-{edition}.iso",
            h,
            True,
            ["-drive", f"file={vol},format=raw,if=ide,index=1"],
        ) as g:
            g.wait("workspace ready")
            g.key("f3")
            g.type("install cal64\n")
            g.wait("Package installed")
            g.type("run parent\n")
            g.wait("IMP: parent exit 0")
            g.wait("25 26 27 28 29")
            log = g.log.read_text()
            assert (
                log.count("child: cal64 exited") == 2
                and log.count("child: F:/CAL.EXE exited") == 1
                and log.count("process: PE64 user mode") == 4
            )
            for bits in [32, 64]:
                g.type(f"runfile tls{bits}.exe\n")
                g.wait(f"IMP: tls{bits}.exe exit 0\nTLS detached")
        print(
            "PASS: isolated PE child, two cal.exe invocations, args, status, wait/close and intact parent"
        )


if __name__ == "__main__":
    p = argparse.ArgumentParser()
    p.add_argument("edition")
    check(p.parse_args().edition)
