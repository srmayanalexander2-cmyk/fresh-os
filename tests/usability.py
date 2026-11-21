"""Exercise recovery paths with a real guest, not shell function mocks."""

import argparse, io, subprocess, tempfile, time
from pathlib import Path
from PIL import Image
from desktop import Home, Machine, Mouse


def verify(iso, shots=None):
    with tempfile.TemporaryDirectory() as tmp:
        d = Path(tmp)
        disk = d / "home.img"
        foreign = d / "foreign.img"
        subprocess.run(["cp", "--sparse=always", iso, disk], check=True)
        foreign.write_bytes(b"not a FreshOS disk".ljust(16384, b"*"))
        data = io.BytesIO()
        Image.new("RGB", (12, 12), (213, 72, 101)).save(data, format="BMP")
        with disk.open("r+b") as f:
            Home(f).put("tile.bmp", data.getvalue())

        def guest():
            return Machine(
                d,
                iso,
                foreign,
                extra=["-drive", f"file={disk},format=raw,if=ide,index=1"],
            )

        def start(m):
            m.wait("IDE 0:1  ATA disk")
            m.wait("Fresh Home mounted (IDE)")
            m.wait("welcome ready:")
            m.key("ret")
            m.wait("desktop ready")

        def frame(m, name):
            path = d / (name + ".ppm")
            time.sleep(0.25)
            m.monitor(f"screendump {path}")
            time.sleep(0.35)
            with Image.open(path) as im:
                if shots:
                    shots.mkdir(parents=True, exist_ok=True)
                    im.save(shots / (name + ".jpg"), quality=88)
                return im.copy()

        with guest() as m:
            start(m)
            p = Mouse(m)
            m.key("f3")
            m.type("exit\n")
            m.wait("shell: desktop")
            m.key("f3")
            m.key("esc")
            m.wait("shell: desktop", 2)
            m.key("f3")
            p.click(1000, 150)
            m.wait("shell: desktop", 3)
            m.key("meta_l-e")
            m.wait("app: files")
            p.click(585, 261)
            frame(m, "drives")
            m.key("f3")
            m.type("doctor\npkg install demo hello32.imp\nsoftware\n")
            m.wait("app: software")
            m.key("ret")
            m.wait("IMP32 completed")
            m.key("f4")
            m.type("open tile.bmp\n")
            m.wait("app: photos")
            im = frame(m, "photos")
            assert im.getpixel((634, 370)) == (208, 72, 96)
            m.key("f4")
            m.type("writer\n")
            m.wait("app: writer")
            m.type("sky")
            p.click(748, 216)
            m.wait("window: maximized")
            p.click(1132, 14)
            m.wait("window: restored")
            m.monitor("mouse_button 2")
            time.sleep(0.2)
            m.monitor("mouse_button 0")
            time.sleep(0.15)
            p.click(1115, 45)
            # copy first menu row
            m.key("ctrl-v")
            m.key("ctrl-s")
            m.key("f4")
            m.type("exit\n")
            m.key("f6")
            frame(m, "smile-slider")
            m.key("esc")
        with disk.open("rb") as f:
            h = Home(f)
            assert h.read("note.txt") == b"skysky"
            assert h.read("packages.db")[28:32] == b"FIMP"
        with guest() as m:
            start(m)
            m.key("f3")
            m.type("pkg run demo\n")
            m.wait("IMP32 completed")
            m.key("f4")
            m.type("software\n")
            m.wait("app: software")
            p = Mouse(m)
            p.click(542, 475)
            p.click(542, 475)
            time.sleep(0.3)
            m.monitor("nmi")
            m.wait("FreshOS kernel stopped: Non-maskable interrupt | vector 2 | IP 0x")
            frame(m, "panic")
        with disk.open("rb") as f:
            assert Home(f).read("packages.db") == b""
        assert foreign.read_bytes() == b"not a FreshOS disk".ljust(16384, b"*")
        with Machine(d, iso, uefi=True) as m:
            m.wait("welcome ready:")
            m.monitor("nmi")
            m.wait("FreshOS kernel stopped: Non-maskable interrupt | vector 2 | IP 0x")
        print(
            "PASS CLI exit (command, Escape, click), alternate IDE disk, untouched foreign disk, package install/reboot/uninstall, BMP pixels, maximize/restore, context clipboard, BIOS/UEFI crash report"
        )


if __name__ == "__main__":
    p = argparse.ArgumentParser()
    p.add_argument("iso", type=Path)
    p.add_argument("--screens", type=Path)
    a = p.parse_args()
    verify(a.iso.resolve(), a.screens)
