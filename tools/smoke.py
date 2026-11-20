#!/usr/bin/env python3
"""Boot the actual image headlessly under QEMU TCG, exercise input, optionally install."""

import argparse, os, subprocess, tempfile, time, sys
from pathlib import Path
from home import Home

ROOT = Path(__file__).resolve().parents[1]


class Machine:
    def __init__(self, folder, iso=None, disk=None, uefi=False, extra=()):
        self.log = folder / "serial.txt"
        self.log.unlink(missing_ok=True)
        args = [
            os.getenv("QEMU_BIN", "qemu-system-x86_64"),
            "-machine",
            "pc,accel=tcg",
            "-m",
            "96" if uefi else "32",
            "-no-reboot",
            "-display",
            "none",
            "-nic",
            "none",
            "-monitor",
            "stdio",
            "-serial",
            "file:" + str(self.log),
        ]
        if iso:
            args += [
                "-drive",
                f"file={iso},media=cdrom,if=ide,index=2,readonly=on",
                "-boot",
                "order=d",
            ]
        if disk:
            args += [
                "-drive",
                f"file={disk},if=ide,index=0,format={'vmdk' if disk.suffix=='.vmdk' else 'raw'}",
            ]
        if uefi:
            for key, default, mode in [
                ("OVMF_CODE", "/usr/share/OVMF/OVMF_CODE_4M.fd", "readonly=on"),
                ("OVMF_VARS", "/usr/share/OVMF/OVMF_VARS_4M.fd", "snapshot=on"),
            ]:
                args += [
                    "-drive",
                    f"if=pflash,format=raw,{mode},file={os.getenv(key,default)}",
                ]
        args.extend(extra)
        self.p = subprocess.Popen(
            args, stdin=subprocess.PIPE, stdout=subprocess.DEVNULL
        )

    def __enter__(self):
        return self

    def __exit__(self, *args):
        self.p.terminate()
        self.p.wait(timeout=10)

    def wait(self, text, count=1, timeout=35):
        deadline = time.monotonic() + timeout
        log = ""
        while time.monotonic() < deadline and self.p.poll() is None:
            if self.log.exists():
                log = self.log.read_text(errors="replace")
            if text == "installation verified" and "installation failed" in log:
                break
            if log.count(text) >= count:
                return
            time.sleep(0.05)
        raise AssertionError(
            f"Missing guest marker: {text!r}\nSerial tail: {log[-900:]}"
        )

    def monitor(self, s):
        self.p.stdin.write((s + "\n").encode())
        self.p.stdin.flush()

    def key(self, key):
        self.monitor("sendkey " + key + " 15")
        time.sleep(0.055)

    def type(self, s):
        for c in s:
            self.key({" ": "spc", ".": "dot", "\n": "ret"}.get(c, c))


def smoke(iso, uefi, install):
    with tempfile.TemporaryDirectory() as folder:
        folder = Path(folder)
        disk = folder / "target.img"
        if install:
            subprocess.run(
                [sys.executable, ROOT / "tools/new-target.py", disk], check=True
            )
        with Machine(folder, iso, disk if install else None, uefi) as m:
            m.wait("welcome ready:")
            m.key("ret")
            m.wait("desktop ready")
            m.key("f1")
            m.key("ret")
            m.wait("app: files")
            m.key("f4")
            m.key("meta_l")
            m.key("ret")
            m.wait("app: files", 2)
            m.key("f3")
            m.type("start\n")
            for bits in (32, 64):
                m.type(f"open hello{bits}.exe\n")
                if bits == 64:
                    m.wait("Win32 modal awaiting OK")
                    m.key("ret")
                m.wait(f"EXE to IMP{bits} completed")
                m.key("f4")
                m.type(f"convert hello{bits}.exe\n")
                m.wait(f"IMP linked: hello{bits}.imp")
                m.type(f"open hello{bits}.imp\n")
                if bits == 64:
                    m.wait("Win32 modal awaiting OK", 2)
                    m.key("ret")
                m.wait(f"IMP{bits} completed", 2)
                m.key("f4")
            m.type("open linux.elf\n")
            m.wait("Linux ELF completed")
            m.key("f4")
            m.type("calc\n")
            m.wait("app: calc")
            if install:
                m.key("f4")
                m.type("install\n")
                m.key("i")
                m.wait("installer copying")
                m.wait("installation verified", timeout=120)
                m.key("esc")
                m.type("writer\n")
                m.wait("app: writer")
                m.type("cloud journal")
                m.key("ctrl-s")
                m.key("f4")
                m.type("prefs\nconvert hello64.exe\n")
                m.wait("IMP linked: hello64.imp", 2)
        if install:
            with disk.open("rb") as f:
                h = Home(f)
                assert h.read("note.txt") == b"cloud journal"
                assert h.read("profile")[1] == 1
            # Import intact packages through the public host CLI, not boot-time fixtures.
            import pycdlib

            cd = pycdlib.PyCdlib()
            cd.open(str(iso))
            for src, target, op in [
                ("HELLO64.IMP", "guest.imp", "put-imp"),
                ("HELLO32.EXE", "guest.exe", "put-exe"),
            ]:
                path = folder / target
                cd.get_file_from_iso(str(path), iso_path="/" + src + ";1")
                subprocess.run(
                    [sys.executable, ROOT / "tools/home.py", disk, op, target, path],
                    check=True,
                )
                with disk.open("rb") as f:
                    assert Home(f).read(target) == path.read_bytes()
            cd.close()
            with Machine(folder, None, disk, uefi) as m:
                m.wait("welcome ready:")
                m.key("ret")
                m.wait("desktop ready")
                m.type("open guest.exe\n")
                m.wait("EXE to IMP32 completed")
                m.key("f4")
                m.type("open guest.imp\n")
                m.wait("Win32 modal awaiting OK")
                m.key("ret")
                m.wait("IMP64 completed")
                m.key("f4")
                m.type("convert guest.exe\n")
                m.wait("IMP linked: guest.imp")
            with disk.open("rb") as f:
                h = Home(f)
                assert h.read("guest.imp")[:6] == b"FIMP\x01\x20"
                assert h.read("hello64.imp") == (folder / "guest.imp").read_bytes()
            with Machine(folder, None, disk, uefi) as m:
                m.wait("welcome ready:")
                m.key("ret")
                m.wait("desktop ready")
                m.type("open guest.imp\n")
                m.wait("IMP32 completed")
        print(
            f'PASS  {iso.name}: {"x64 UEFI" if uefi else "i386 BIOS"} boot, Windows-key tray, CLI launch, PE32/64 conversion and IMP launch'
            + (", install, persisted document and disk-only reboot" if install else "")
        )


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("iso", type=Path)
    p.add_argument("--uefi", action="store_true")
    p.add_argument("--install", action="store_true")
    a = p.parse_args()
    smoke(a.iso.resolve(), a.uefi, a.install)
