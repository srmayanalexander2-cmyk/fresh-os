"""Guest checks for context menus, media/photos/snake, file tools and Writer extras."""

import argparse, shutil, tempfile, time
from pathlib import Path
from boot import Guest, ROOT


def verify(edition, shots=None):
    src = ROOT / "build" / edition
    with tempfile.TemporaryDirectory() as tmp:
        d = Path(tmp)
        h = d / "Home.img"
        shutil.copyfile(src / "Home.img", h)
        with Guest(d, src / f"FreshOS-{edition}.iso", h, False) as g:
            g.wait("workspace ready")
            g.key("ret")

            def move(x, y):
                while x or y:
                    dx = max(-70, min(x, 70))
                    dy = max(-70, min(y, 70))
                    g.monitor(f"mouse_move {dx} {dy}")
                    time.sleep(0.025)
                    x -= dx
                    y -= dy

            def click(btn=1):
                g.monitor(f"mouse_button {btn}")
                time.sleep(0.15)
                g.monitor("mouse_button 0")
                time.sleep(0.15)

            def type_upper(s):
                # Guest.type cannot send uppercase key names; use shift- combos.
                for c in s:
                    if "A" <= c <= "Z":
                        g.key("shift-" + c.lower())
                    else:
                        g.type(c)

            # Right-click on the desktop opens the context menu; first item is Terminal.
            w, height = g.shot(d / "start.jpg")
            click(2)
            g.wait("ctx: desktop")
            if shots:
                shots.mkdir(exist_ok=True, parents=True)
                g.shot(shots / "context-menu.jpg")
            move(20, 17)
            click()
            g.wait("open: term")
            # Create a music file and a picture file in Writer.
            g.type("new song.mus\n")
            g.wait("open: writer")
            type_upper("C4 E4 G4 C5")
            g.key("ctrl-s")
            g.wait("Home snapshot committed")
            g.key("f3")
            g.type("new pic.pbm\n")
            g.wait("open: writer")
            type_upper("P1\n4 2\n1010 0101")
            g.key("ctrl-s")
            g.wait("Home snapshot committed")
            # Writer: select all, cut, undo, find.
            g.key("ctrl-a")
            g.key("ctrl-x")
            g.key("ctrl-z")
            g.wait("Undid last edit")
            g.key("ctrl-f")
            g.type("1010")
            g.key("ret")
            g.wait("Found")
            g.key("esc")
            g.key("f3")
            # Terminal file tools.
            g.type("wc song.mus\n")
            g.type("cp song.mus song2.mus\n")
            g.type("mv song2.mus renamed.mus\n")
            type_upper("find C4\n")
            g.wait("song.mus")
            g.type("rm renamed.mus\n")
            g.wait("Deleted")
            mark = len(g.log.read_text())
            type_upper("find C4\n")
            time.sleep(0.8)
            tail = g.log.read_text()[mark:]
            assert "song.mus" in tail and "renamed.mus" not in tail, tail
            # Smart open: music -> Media, picture -> Photos.
            g.type("play song.mus\n")
            g.wait("open: media")
            g.wait("Loaded music file")
            g.key("spc")
            time.sleep(0.5)
            g.key("esc")
            g.key("f3")
            g.wait("open: term")
            g.type("view pic.pbm\n")
            g.wait("open: photos")
            g.wait("Loaded picture")
            if shots:
                g.shot(shots / "photos.jpg")
            g.key("esc")
            g.key("f3")
            g.wait("open: term")
            # Snake starts and steers.
            g.type("open snake\n")
            g.wait("open: snake")
            g.key("up")
            g.key("right")
            time.sleep(0.8)
            if shots:
                g.shot(shots / "snake.jpg")
            g.key("esc")
            g.key("f3")
            g.wait("open: term")
            # Files: right-click a file row, then Escape closes the menu.
            g.type("open files\n")
            g.wait("open: files")
            move(-w, -height)
            move(160, 210)
            click(2)
            g.wait("ctx: files")
            g.key("esc")
            g.key("f3")
            g.wait("open: term")
            # Calculator: Ctrl+C copies the result.
            g.type("open calc\n")
            g.wait("open: calc")
            g.type("2+3\n")
            g.key("ctrl-c")
            g.wait("Result copied")
            if shots:
                g.shot(shots / "media-calc.jpg")
    print("PASS context menu, smart open, media, photos, snake, file tools, writer find/undo:", edition)


if __name__ == "__main__":
    p = argparse.ArgumentParser()
    p.add_argument("edition")
    p.add_argument("--shots", type=Path)
    a = p.parse_args()
    verify(a.edition, a.shots)
