"""Check a Win32 HKCU value survives a cold boot on the marked native disk."""

import argparse, os, shutil, subprocess, sys, tempfile
from pathlib import Path
from boot import Guest, ROOT

sys.path.insert(0, str(ROOT / "forge"))
import appdisk


def check(edition):
    src = ROOT / "build" / edition
    with tempfile.TemporaryDirectory() as t:
        d = Path(t)
        h = d / "Home.img"
        vol = d / "Data.img"
        shutil.copyfile(src / "Home.img", h)
        cc = (
            [os.environ["ZIG"], "cc", "-target", "x86_64-windows-gnu"]
            if "ZIG" in os.environ
            else ["x86_64-w64-mingw32-gcc"]
        )
        # Import library only. No vendor DLL or runtime emulation in the program.
        (d / "stub.S").write_text(".text\n.global stub\nstub: ret\n")
        regapi = [
            "RegOpenKeyExW",
            "RegCreateKeyExW",
            "RegSetValueExW",
            "RegQueryValueExW",
            "RegDeleteValueW",
            "RegCloseKey",
        ]
        (d / "exports.def").write_text(
            "LIBRARY advapi32.dll\nEXPORTS\n"
            + "\n".join(s + "=stub" for s in regapi)
            + "\n"
        )
        subprocess.run(["as", "--64", "stub.S", "-o", "stub.o"], cwd=d, check=True)
        subprocess.run(
            [
                "ld",
                "-mi386pep",
                "--dll",
                "-e",
                "stub",
                "--no-insert-timestamp",
                "--out-implib",
                "advapi.a",
                "-o",
                "advapi32.dll",
                "exports.def",
                "stub.o",
            ],
            cwd=d,
            check=True,
        )
        files = []
        for name in ["writer", "reader"]:
            args = ["-DWRITER"] if name == "writer" else []
            subprocess.run(
                [
                    *cc,
                    "-Os",
                    "-ffreestanding",
                    "-fno-stack-protector",
                    "-fno-sanitize=all",
                    *args,
                    "-c",
                    ROOT / "qa/regpersist.c",
                    "-o",
                    "test.o",
                ],
                cwd=d,
                check=True,
            )
            exe = d / (name + ".exe")
            subprocess.run(
                [
                    "ld",
                    "-mi386pep",
                    "--no-insert-timestamp",
                    "-s",
                    "-e",
                    "mainCRTStartup",
                    "-o",
                    exe,
                    "test.o",
                    "advapi.a",
                ],
                cwd=d,
                check=True,
            )
            files.append((exe.name, exe))
        appdisk.create(vol, files)
        for name in ["writer", "reader"]:
            with Guest(
                d,
                src / f"FreshOS-{edition}.iso",
                h,
                True,
                ["-drive", f"file={vol},format=raw,if=ide,index=1"],
            ) as g:
                g.wait("workspace ready")
                g.key("f3")
                g.type("runfile " + name + ".exe\n")
                g.wait(f"IMP: {name}.exe exit 0")
        print(
            "PASS: Win64 registry create, set, cold-boot open, size/data query, read-only denial, stale handle and delete"
        )


if __name__ == "__main__":
    p = argparse.ArgumentParser()
    p.add_argument("edition")
    check(p.parse_args().edition)
