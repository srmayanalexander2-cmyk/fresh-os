"""Check desktop switching, clipboard, OSK, capture and archives on writable Home."""

import gzip, io, sys, subprocess, tempfile, time, tarfile
from pathlib import Path
from PIL import Image
from desktop import Machine, Home, Mouse


def exercise(iso, shots=None):
    with tempfile.TemporaryDirectory() as tmp:
        d = Path(tmp)
        disk = d / "home.img"
        subprocess.run(["cp", "--sparse=always", iso, disk], check=True)

        def capture(m, name):
            path = d / (name + ".ppm")
            m.monitor(f"screendump {path}")
            for _ in range(50):
                try:
                    with Image.open(path) as frame:
                        frame.load()
                        if shots:
                            shots.mkdir(exist_ok=True, parents=True)
                            frame.save(shots / (name + ".jpg"), quality=87)
                        return frame.copy()
                except OSError:
                    time.sleep(0.05)
            raise AssertionError("incomplete monitor screenshot")

        def start(m):
            m.wait("welcome ready:")
            m.key("ret")
            m.wait("desktop ready")
            m.key("f3")

        with Machine(d, iso, disk) as m:
            start(m)
            m.type("timer 1\n")
            m.wait("Clock alert")
            m.type("sticky\n")
            m.wait("app: sticky")
            m.type("hello")
            m.key("ctrl-c")
            m.key("ctrl-s")
            m.key("f9")
            time.sleep(0.3)
            shot = capture(m, "magnifier")
            # At pointer (640,360), the lens uses source (592,328), displayed at (14,14).
            src = shot.crop((592, 328, 632, 352)).resize(
                (80, 48), Image.Resampling.NEAREST
            )
            assert src.tobytes() == shot.crop((14, 14, 94, 62)).tobytes()
            m.key("f9")
            m.key("meta_l-tab")
            time.sleep(0.2)
            capture(m, "task-view")
            m.key("right")
            m.key("ret")
            m.wait("workspace: 2")
            m.type("todo\n")
            m.wait("app: todo")
            m.key("ctrl-v")
            m.key("ret")
            m.key("tab")
            m.key("ctrl-s")
            m.key("ctrl-meta_l-left")
            m.wait("workspace: 1")
            m.key("f11")
            p = Mouse(m)
            p.click(420, 548)
            m.key("ctrl-s")
            m.key("f11")
            m.key("f4")
            m.key("meta_l-a")
            m.wait("app: actions")
            time.sleep(0.2)
            capture(m, "actions")
            for _ in range(7):
                m.key("down")
            m.key("shift-up")
            m.key("ret")
            m.wait("app: events")
            m.key("f4")
            m.type("tips\n")
            m.wait("app: tips")
            m.key("ret")
            m.wait("app: writer")
            m.key("f4")
            m.type("tar pack.tar sticky1.txt todo.list\ngzip task.gz todo.list\n")
            m.key("f10")
            time.sleep(0.3)
            m.monitor("mouse_button 1")
            time.sleep(0.2)
            m.monitor("mouse_move 25 31")
            time.sleep(0.3)
            m.monitor("mouse_button 0")
            time.sleep(0.5)
        with disk.open("rb") as f:
            h = Home(f)
            assert h.read("sticky1.txt") == b"hello1"
            assert h.read("todo.list") == b"x hello\n"
            assert gzip.decompress(h.read("task.gz")) == h.read("todo.list")
            with tarfile.open(fileobj=io.BytesIO(h.read("pack.tar"))) as t:
                for name in ["sticky1.txt", "todo.list"]:
                    assert t.extractfile(name).read() == h.read(name)
            im = Image.open(io.BytesIO(h.read("snip.bmp")))
            im.load()
            assert im.size == (26, 32), im.size
        with Machine(d, iso, disk) as m:
            start(m)
            m.type("sticky\n")
            m.wait("app: sticky")
            m.type("x")
            m.key("ctrl-s")
            m.key("f4")
            m.type("todo\n")
            m.wait("app: todo")
            m.key("tab")
            m.key("ctrl-s")
            time.sleep(0.3)
        with disk.open("rb") as f:
            h = Home(f)
            assert (
                h.read("sticky1.txt") == b"hello1x"
                and h.read("todo.list") == b"- hello\n"
            )
    print("PASS two desktops, OSK, clipboard, reboot persistence, BMP, tar and gzip")


if __name__ == "__main__":
    exercise(
        Path(sys.argv[1]).resolve(), Path(sys.argv[2]) if len(sys.argv) > 2 else None
    )
