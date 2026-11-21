"""Run installed executable packages through the actual guest UI, not a host shim."""

import argparse, tempfile, shutil
from pathlib import Path
from boot import Guest, ROOT


def verify(e, efi, shots):
    src = ROOT / "build" / e
    with tempfile.TemporaryDirectory() as tmp:
        d = Path(tmp)
        h = d / "Home.img"
        shutil.copyfile(src / "Home.img", h)
        # Add a second installation with different resource bytes; not compiled into the ISO.
        import sys

        sys.path.insert(0, str(ROOT / "forge"))
        from imp import pack, install

        f = d / "greeting.txt"
        f.write_text("External package resource verified.\n")
        install(h, "external", pack(src / "linux32.elf", [], [f]))
        bad = bytearray((src / "win64.imp").read_bytes())
        install(h, "bad", bad)
        with h.open("r+b") as fp:
            fp.seek((41472 + 2049 * 7 + 1) * 512 + 20)
            fp.write(b"X")
        with Guest(d, src / f"FreshOS-{e}.iso", h, efi) as g:
            g.wait("workspace ready")
            if shots:
                shots.mkdir(exist_ok=True, parents=True)
                g.shot(shots / "welcome.jpg")
            g.key("ret")
            if shots:
                g.shot(shots / "desktop.jpg")
            g.key("f1")
            if shots:
                shots.mkdir(exist_ok=True, parents=True)
                g.shot(shots / "all-apps.jpg")
            g.key("esc")
            g.key("f3")
            g.type("run windll\n")
            g.wait("Install this inbox package first")
            g.type("install windll\n")
            g.wait("Package installed")
            for name, text in [
                ("win32", "Hello from a 32-bit Windows EXE."),
                ("win64", "Hello from a 64-bit Windows EXE."),
                ("windll", "Hello from a relocated application DLL."),
                ("linux32", "Read from an installed IMP resource."),
                ("linux64", "Hello from a static Linux ELF!"),
                ("external", "External package resource verified."),
            ]:
                g.type(f"run {name}\n")
                if name in ("win64", "windll"):
                    g.wait("Win32 dialog: " + name)
                    assert "IMP: " + name + " exit 0" not in g.log.read_text()
                    if shots and name == "windll":
                        g.shot(shots / "dll-message.jpg")
                    g.key("ret")
                g.wait("IMP: " + name + " exit 0")
                g.wait(text)
            if efi:
                for abi in ("ELF32", "ELF64", "PE32", "PE64"):
                    g.wait(f"process: {abi} user mode")
            else:
                g.type("install cal64\n")
                g.wait("This package requires x64 UEFI user mode")
            g.type("run bad\n")
            g.wait("IMP: bad failed Unreadable or invalid IMP2 package")
            g.type("open imp\n")
            g.wait("open: imp")
            g.key("f11")
            g.wait("layout: imp fullscreen")
        with Guest(d, src / f"FreshOS-{e}.iso", h, efi) as g:
            g.wait("workspace ready")
            g.key("f3")
            g.type("run windll\n")
            g.wait("Win32 dialog: windll")
            g.key("ret")
            g.wait("IMP: windll exit 0")
    print(
        "PASS: six installed IMP executions, DLL import, external resource, corrupt package rejection",
        e,
        "EFI" if efi else "BIOS",
    )


if __name__ == "__main__":
    p = argparse.ArgumentParser()
    p.add_argument("edition")
    p.add_argument("--efi", action="store_true")
    p.add_argument("--shots", type=Path)
    a = p.parse_args()
    verify(a.edition, a.efi, a.shots)
