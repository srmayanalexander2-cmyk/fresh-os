"""Build the same Win32 SDK client for both widths; boot FreshOS to run them."""

import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
from boot import ROOT, Guest

sys.path.insert(0, str(ROOT / "forge"))
import appdisk


def build(d, bits):
    wide = bits == 64
    prefix = ("x86_64" if wide else "i686") + "-w64-mingw32-"
    if "ZIG" in os.environ:
        zig = os.environ["ZIG"]
        cc = [zig, "cc", "-target", ("x86_64" if wide else "x86") + "-windows-gnu"]
        dlltool = [zig, "dlltool"]
    else:
        cc, dlltool = [prefix + "gcc"], [prefix + "dlltool"]

    def run(*args):
        subprocess.run(args, cwd=d, check=True)

    run(
        *cc,
        "-Os",
        "-ffreestanding",
        "-fno-sanitize=all",
        "-fno-stack-protector",
        "-c",
        ROOT / "qa/securitybuf.c",
        "-o",
        "client.o",
    )
    # Generate only import metadata; implementations are always in the guest.
    libs = []
    for dll, decls in [
        (
            "advapi32",
            "InitializeAcl@12 AddAce@20 GetAce@12 DeleteAce@8 GetAclInformation@16 InitializeSid@12 GetSidSubAuthorityCount@4 GetSidSubAuthority@8 GetLengthSid@4 GetSidLengthRequired@4 IsValidSid@4 CopySid@12 EqualSid@8",
        ),
        (
            "kernel32",
            "GetLastError@0 GetProcessHeap@0 HeapCreate@12 HeapAlloc@12 HeapFree@12 HeapSize@12 HeapReAlloc@16 HeapDestroy@4 InitializeCriticalSection@4 InitializeCriticalSectionAndSpinCount@8 InitializeCriticalSectionEx@12 EnterCriticalSection@4 TryEnterCriticalSection@4 LeaveCriticalSection@4 DeleteCriticalSection@4",
        ),
    ]:
        names = [s.split("@")[0] if wide else s for s in decls.split()]
        (d / "exports.def").write_text(
            f"LIBRARY {dll}.dll\nEXPORTS\n" + "\n".join(names) + "\n"
        )
        lib = d / (dll + ".a")
        run(
            *dlltool,
            "-m",
            "i386:x86-64" if wide else "i386",
            "-k",
            "-d",
            "exports.def",
            "-l",
            lib,
        )
        libs.append(lib)
    exe = d / f"sec{bits}.exe"
    if "ZIG" in os.environ:
        run(
            zig,
            "lld-link",
            "/subsystem:console",
            "/nodefaultlib",
            "/safeseh:no",
            "/entry:mainCRTStartup",
            "/timestamp:0",
            "/out:" + str(exe),
            "client.o",
            *libs,
        )
    else:
        run(
            "ld",
            "-m",
            "i386pep" if wide else "i386pe",
            "--no-insert-timestamp",
            "-s",
            "-e",
            ("" if wide else "_") + "mainCRTStartup",
            "-o",
            exe,
            "client.o",
            *libs,
        )
    return exe


def check(edition):
    media = ROOT / "build" / edition
    with tempfile.TemporaryDirectory() as tmp:
        d = Path(tmp)
        exes = [build(d, b) for b in (32, 64)]
        vol = d / "Data.img"
        home = d / "Home.img"
        shutil.copyfile(media / "Home.img", home)
        appdisk.create(vol, [(f.name, f) for f in exes])
        with Guest(
            d,
            media / f"FreshOS-{edition}.iso",
            home,
            True,
            ["-drive", f"file={vol},format=raw,if=ide,index=1"],
        ) as g:
            g.wait("workspace ready")
            g.key("f3")
            for f in exes:
                g.type(f"runfile {f.name}\n")
                g.wait(f"IMP: {f.name} exit 0")
    print(
        "PASS: SDK ACL/SID operations private heap lifecycle and recursive critical sections in both PE32 and PE64 guest modes"
    )


if __name__ == "__main__":
    p = argparse.ArgumentParser()
    p.add_argument("edition")
    check(p.parse_args().edition)
