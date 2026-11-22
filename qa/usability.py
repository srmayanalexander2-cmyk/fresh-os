"""First-run guide, desktop app hub and sample content in a real Lite guest."""

import argparse, shutil, tempfile, time
from pathlib import Path
from boot import Guest, ROOT


def verify(edition, shots=None):
    src = ROOT / "build" / edition
    with tempfile.TemporaryDirectory() as tmp:
        d = Path(tmp)
        home = d / "Home.img"
        shutil.copyfile(src / "Home.img", home)
        with Guest(d, src / f"FreshOS-{edition}.iso", home, False) as g:
            g.wait("workspace ready")
            g.wait("Home disk / writable")
            if shots:
                shots.mkdir(parents=True, exist_ok=True)
                w, h = g.shot(shots / "welcome.jpg")
            else:
                w, h = 1024, 768
            cx, cy = w // 2, h // 2

            def click(tx, ty):
                nonlocal cx, cy
                dx, dy = tx - cx, ty - cy
                while dx or dy:
                    mx, my = max(-70, min(dx, 70)), max(-70, min(dy, 70))
                    g.monitor(f"mouse_move {mx} {my}")
                    time.sleep(0.025)
                    dx -= mx
                    dy -= my
                cx, cy = tx, ty
                g.monitor("mouse_button 1")
                time.sleep(0.15)
                g.monitor("mouse_button 0")
                time.sleep(0.15)

            # Click the explicit first-run action, then click the Files shortcut on the hub.
            ww = min(w - 36, max(460, min(740, w * 76 // 100)))
            wh = min(h - 36, max(340, min(420, h * 55 // 100)))
            wx, wy = (w - ww) // 2, (h - wh) // 2
            btn_y = wy + wh - 82
            btn_w = (ww - 66) // 3
            click(wx + 24 + btn_w // 2, btn_y + 19)
            w, h = g.shot(shots / "desktop.jpg") if shots else (1024, 768)
            pw = min(w - 72, max(430, min(700, w * 70 // 100)))
            ph = min(h - 44, max(340, min(520, h * 68 // 100)))
            x0, y0 = (w - pw) // 2, (h - ph) // 2
            cw = (pw - 50) // 2
            ch = max(48, min(84, (ph - 58 - 112 - 20) // 3))
            tx, ty = x0 + 20 + cw // 2, y0 + 112 + ch // 2
            click(tx, ty)
            g.wait("open: files")
            if shots:
                g.shot(shots / "files.jpg")
            g.key("esc")

            # F1 opens the readable app grid; keyboard selection is linear and predictable.
            g.key("f1")
            if shots:
                g.shot(shots / "all-apps.jpg")
            for _ in range(10):
                g.key("tab")
            g.key("ret")
            g.wait("open: photos")
            g.key("esc")

            # The preloaded examples demonstrate extension-based open without setup.
            g.key("f3")
            g.type("play example.mus\n")
            g.wait("open: media")
            g.wait("Loaded music file")
            g.key("esc")
            g.key("f3")
            g.type("view pattern.pbm\n")
            g.wait("open: photos")
            g.wait("Loaded picture")
            if shots:
                g.shot(shots / "sample-picture.jpg")
    print("PASS guided welcome, clickable desktop hub, app grid and bundled music/picture examples:", edition)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("edition", nargs="?", default="Lite")
    parser.add_argument("--shots", type=Path)
    args = parser.parse_args()
    verify(args.edition, args.shots)
