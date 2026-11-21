"""Test actual El Torito BIOS/EFI media and guest input through a QEMU monitor."""

import argparse, os, shutil, struct, subprocess, tempfile, time
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]


class Guest:
    def __init__(self, d, iso, home, efi, extra=()):
        self.log = d / "serial"
        self.log.unlink(missing_ok=True)
        self.d = d
        args = [
            os.environ.get("QEMU_BIN", "qemu-system-x86_64"),
            "-m",
            "96" if efi else "32",
            "-nic",
            "none",
            "-no-reboot",
            "-display",
            "none",
            "-monitor",
            "stdio",
            "-serial",
            "file:" + str(self.log),
            "-drive",
            f"file={home},format={'vmdk' if home.suffix=='.vmdk' else 'raw'},index=0,if=ide",
            "-boot",
            "order=d" if iso else "order=c",
        ]
        if iso:
            args.extend(
                ["-drive", f"media=cdrom,file={iso},if=ide,index=2,readonly=on"]
            )
        if efi:
            args.extend(
                [
                    "-drive",
                    "if=pflash,format=raw,readonly=on,file="
                    + os.environ.get("OVMF_CODE", "/usr/share/OVMF/OVMF_CODE_4M.fd"),
                    "-drive",
                    "if=pflash,format=raw,snapshot=on,file="
                    + os.environ.get("OVMF_VARS", "/usr/share/OVMF/OVMF_VARS_4M.fd"),
                ]
            )
        args.extend(extra)
        self.p = subprocess.Popen(
            args, stdin=subprocess.PIPE, stdout=subprocess.DEVNULL
        )

    def __enter__(self):
        return self

    def __exit__(self, *unused):
        self.p.terminate()
        self.p.wait(timeout=10)

    def wait(self, s, timeout=15):
        end = time.monotonic() + timeout
        while time.monotonic() < end and self.p.poll() is None:
            if self.log.exists() and s in self.log.read_text(errors="replace"):
                return
            time.sleep(0.05)
        raise AssertionError(
            f"Missing {s!r}: " + self.log.read_text(errors="replace")[-750:]
        )

    def monitor(self, s):
        self.p.stdin.write((s + "\n").encode())
        self.p.stdin.flush()

    def key(self, s):
        self.monitor("sendkey " + s + " 20")
        time.sleep(0.085)

    def type(self, s):
        for c in s:
            self.key({" ": "spc", ".": "dot", "\n": "ret", "-": "minus"}.get(c, c))

    def shot(self, path):
        f = self.d / "screen.ppm"
        f.unlink(missing_ok=True)
        time.sleep(0.2)
        self.monitor("screendump " + str(f))
        time.sleep(0.3)
        with Image.open(f) as im:
            im.save(path, quality=88)
            return im.size


def check(edition, efi, shots):
    src = ROOT / "build" / edition
    with tempfile.TemporaryDirectory() as tmp:
        d = Path(tmp)
        home = d / "data.raw"
        shutil.copyfile(src / "Home.img", home)
        with Guest(d, src / f"FreshOS-{edition}.iso", home, efi) as g:
            g.wait("workspace ready")
            g.wait("Home disk / writable")
            if shots:
                shots.mkdir(parents=True, exist_ok=True)
                g.shot(shots / "workspace.jpg")
            g.key("f3")
            g.wait("open: term")
            g.type("new field.txt\n")
            g.wait("open: writer")
            g.type("tested native notes")
            g.key("ctrl-s")
            g.wait("Home snapshot committed")
            g.key("f11")
            g.wait("layout: writer fullscreen")
            if shots:
                g.shot(shots / "writer-full.jpg")
            g.key("f11")
            g.wait("layout: writer window")
            g.key("f3")
            g.type("exit\n")
            g.wait("desktop")
        data = home.read_bytes()
        assert (
            b"tested native notes\0" in data
            and data[40961 * 512 : 40961 * 512 + 8] == b"F2SNAP01"
        )
        with Guest(d, src / f"FreshOS-{edition}.iso", home, efi) as g:
            g.wait("workspace ready")
            g.key("f3")
            g.type("edit field.txt\n")
            g.wait("open: writer")
            g.key("end")
            g.type(" again")
            g.key("ctrl-s")
            g.wait("Home snapshot committed")
        data = home.read_bytes()
        assert (
            b"tested native notes again\0" in data
            and data[41089 * 512 : 41089 * 512 + 8] == b"F2SNAP01"
        )
    print(
        "PASS",
        edition,
        "EFI" if efi else "BIOS",
        "desktop, app launch, fullscreen/restore, note write and read after reboot",
        flush=True,
    )


if __name__ == "__main__":
    p = argparse.ArgumentParser()
    p.add_argument("edition")
    p.add_argument("--efi", action="store_true")
    p.add_argument("--shots", type=Path)
    a = p.parse_args()
    check(a.edition, a.efi, a.shots)
