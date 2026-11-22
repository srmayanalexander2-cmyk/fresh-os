"""Execute native user code from Home in x64 OVMF. Optional external BusyBox ELF."""

import argparse, shutil, subprocess, sys, tempfile
from pathlib import Path
from boot import Guest, ROOT

sys.path.insert(0, str(ROOT / "forge"))
from imp import pack, install


def check(e, busybox):
    src = ROOT / "build" / e
    with tempfile.TemporaryDirectory() as tmp:
        d = Path(tmp)
        h = d / "Home.img"
        shutil.copyfile(src / "Home.img", h)

        def cmd(*a):
            subprocess.run(a, cwd=d, check=True)

        cmd("as", "--64", ROOT / "qa/user64.S", "-o", "test.o")
        cmd("ld", "-static", "-o", "user", "test.o")
        install(h, "user", pack(d / "user", [], [], True))
        cmd("as", "--64", ROOT / "qa/winpages64.S", "-o", "test.o")
        cmd(
            "ld",
            "-mi386pep",
            "-s",
            "--no-insert-timestamp",
            "-e",
            "start",
            "-o",
            "pages.exe",
            "test.o",
            src / "pe64/KERNEL32.a",
        )
        install(h, "pages", pack(d / "pages.exe", [], [], True))
        with Guest(d, src / f"FreshOS-{e}.iso", h, True) as g:
            g.wait("workspace ready")
            g.key("f3")
            g.type("install cal64\n")
            g.wait("Package installed")
            for line, want in [
                ("cal64 2 2024", "25 26 27 28 29"),
                ("cal64 13 2024", "IMP: cal64 exit 1"),
                ("pages", "IMP: pages exit 0"),
                ("user normal", "IMP: user exit 0"),
                ("user protect", "Application CPU exception 14"),
                ("user fault", "Application CPU exception 6"),
                ("user timeout", "Process CPU limit reached"),
                ("linux64", "Hello from a static Linux ELF!"),
            ]:
                g.type("run " + line + "\n")
                g.wait(want)
            g.wait("Linux brk grow/shrink/zero-recommit passed")
            g.wait("process: PE64 user mode")
            g.wait("process: ELF64 user mode")
        if busybox:
            shutil.copyfile(src / "Home.img", h)
            f = d / "data.txt"
            f.write_text("red blue red\ngreen yellow\n")
            install(h, "busybox", pack(busybox, [], [f], True))
            with Guest(d, src / f"FreshOS-{e}.iso", h, True) as g:
                g.wait("workspace ready")
                g.key("f3")
                for args in [
                    ("echo", "freshos"),
                    ("cat", "data.txt"),
                    ("wc", "data.txt"),
                    ("sort", "data.txt"),
                    ("false",),
                ]:
                    # Compare to running the identical executable under Linux.
                    expected = subprocess.run(
                        [busybox, *args], cwd=d, capture_output=True, check=False
                    )
                    g.type("run busybox " + " ".join(args) + "\n")
                    g.wait(
                        "IMP: busybox exit "
                        + str(expected.returncode)
                        + "\n"
                        + expected.stdout.decode()
                    )
            print(
                "PASS: upstream static Linux BusyBox, output and exit status match Linux"
            )
    print(
        "PASS: CPL3, PE calendar, Linux brk/mmap lifecycles, app faults, timer preemption, return to desktop:",
        e,
    )


if __name__ == "__main__":
    p = argparse.ArgumentParser()
    p.add_argument("edition")
    p.add_argument("--busybox", type=Path)
    a = p.parse_args()
    check(a.edition, a.busybox.resolve() if a.busybox else None)
