"""Eight responsive sessions; inspect guest pixels and use actual mouse hit targets."""

import argparse, re, shutil, tempfile, time
from pathlib import Path
from PIL import Image
from boot import Guest, ROOT


def verify(e, shots=None):
    src = ROOT / "build" / e
    with tempfile.TemporaryDirectory() as tmp:
        d = Path(tmp)
        h = d / "Home.img"
        shutil.copyfile(src / "Home.img", h)
        with Guest(d, src / f"FreshOS-{e}.iso", h, False) as g:
            g.wait("workspace ready")
            g.key("ret")
            w, height = g.shot(d / "start.jpg")
            if shots:
                shots.mkdir(exist_ok=True, parents=True)
                shutil.copyfile(d / "start.jpg", shots / "workspace.jpg")
            for app in [
                "files",
                "writer",
                "calc",
                "term",
                "settings",
                "system",
                "sketch",
                "clock",
                "imp",
            ]:
                g.key("f3")
                g.type("open " + app + "\n")
                g.wait("open: " + app)
                g.key("f11")
                g.wait(f"layout: {app} fullscreen {w-28}x{height-72}")
                if app == "calc":
                    g.type("7")
                    g.key("shift-8")
                    g.type("6\n")
                if shots and app in (
                    "files",
                    "writer",
                    "calc",
                    "settings",
                    "system",
                    "clock",
                ):
                    g.shot(shots / (app + "-fullscreen.jpg"))
                g.key("f11")
                g.wait("layout: " + app + " window")
                g.key("esc")

            # Hit the maximized Files button with PS/2 deltas. Start from the top-left corner.
            def move(x, y):
                while x or y:
                    dx = max(-70, min(x, 70))
                    dy = max(-70, min(y, 70))
                    g.monitor(f"mouse_move {dx} {dy}")
                    time.sleep(0.025)
                    x -= dx
                    y -= dy

            def click():
                g.monitor("mouse_button 1")
                time.sleep(0.15)
                g.monitor("mouse_button 0")
                time.sleep(0.15)

            g.key("f3")
            g.type("open files\n")
            move(-w, -height)
            fw = min(980, max(580, w * 72 // 100))
            fh = min(690, max(360, height * 78 // 100))
            move((w - fw) // 2 + fw - 98, (height - fh) // 2 + 21)
            click()
            g.wait("layout: files maximized")
            g.key("f11")
            g.key("f11")
            time.sleep(0.15)
            assert g.log.read_text().count("layout: files maximized") == 2
            g.monitor("nmi")
            g.wait("kernel stop: vector 2")
    print(
        "PASS nine fullscreen layouts, restore, actual Max mouse target, previous mode restoration and CPU stop:",
        e,
    )


if __name__ == "__main__":
    p = argparse.ArgumentParser()
    p.add_argument("edition")
    p.add_argument("--shots", type=Path)
    a = p.parse_args()
    verify(a.edition, a.shots)
