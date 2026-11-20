"""Drive the unmodified VMware Lite desktop via emulated PS/2 mouse packets."""

import argparse, sys, time, tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from smoke import Machine
from home import Home


class Mouse:
    def __init__(self, m):
        self.m = m
        self.x, self.y = 480, 270

    def click(self, x, y):
        while (x, y) != (self.x, self.y):
            dx = max(-60, min(60, x - self.x))
            dy = max(-60, min(60, y - self.y))
            self.m.monitor(f"mouse_move {dx} {dy}")
            self.x += dx
            self.y += dy
            time.sleep(0.085)
        self.m.monitor("mouse_button 1")
        time.sleep(0.12)
        self.m.monitor("mouse_button 0")
        time.sleep(0.2)


def exercise(iso, shots):
    with tempfile.TemporaryDirectory() as tmp:
        d = Path(tmp)
        disk = d / "home.img"
        # Sparse clone of fresh release media. Mount as a writable IDE Home.
        import subprocess

        subprocess.run(["cp", "--sparse=always", iso, disk], check=True)
        with Machine(d, iso, disk) as m:

            def capture(name):
                if shots:
                    shots.mkdir(exist_ok=True, parents=True)
                    m.monitor(f"screendump {shots.resolve()}/{name}.ppm")
                    time.sleep(0.3)

            m.wait("welcome ready:")
            time.sleep(0.4)
            capture("welcome")
            p = Mouse(m)
            p.click(495, 348)
            m.wait("desktop ready", timeout=4)
            p.click(38, 92)
            m.wait("app: files", timeout=4)
            p.click(360, 247)
            capture("files")
            p.click(396, 335)
            m.wait("IMP linked: hello32.imp", timeout=4)
            p.click(275, 335)
            m.wait("IMP32 completed", timeout=4)
            capture("program")
            p.click(38, 142)
            m.wait("app: writer", timeout=4)
            m.type("clouds")
            m.key("left")
            m.key("backspace")
            m.type("d")
            m.key("end")
            m.type(" glow")
            capture("writer")
            p.click(640, 342)
            # Save as dialog (replace suggested name).
            for _ in range(8):
                m.key("backspace")
            m.type("sky.txt")
            p.click(557, 314)
        with disk.open("rb") as f:
            h = Home(f)
            assert h.read("sky.txt") == b"clouds glow"
            assert h.read("hello32.imp")[:4] == b"FIMP"
        print(
            "PASS  mouse welcome, Files selection, conversion, launch, Writer edit and named Save as"
        )


if __name__ == "__main__":
    p = argparse.ArgumentParser()
    p.add_argument("iso", type=Path)
    p.add_argument("--screenshots", type=Path)
    a = p.parse_args()
    exercise(a.iso.resolve(), a.screenshots)
