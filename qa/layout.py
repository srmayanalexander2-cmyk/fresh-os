"""Twelve responsive app sessions with framebuffer checks and absolute-pointer hit testing."""

import argparse, json, re, shutil, socket, tempfile, time
from pathlib import Path
from PIL import Image
from boot import Guest, ROOT


def verify(e, shots=None):
    src = ROOT / "build" / e
    with tempfile.TemporaryDirectory() as tmp:
        d = Path(tmp)
        h = d / "Home.img"
        shutil.copyfile(src / "Home.img", h)
        qmp_path = d / "qmp"
        extra = [
            "-machine",
            "pc,vmport=on",
            "-vga",
            "vmware",
            "-qmp",
            f"unix:{qmp_path},server=on,wait=off",
        ]
        with Guest(d, src / f"FreshOS-{e}.iso", h, False, extra) as g:
            g.wait("workspace ready")
            g.wait("VMware absolute pointer enabled")
            qmp = socket.socket(socket.AF_UNIX)
            qmp.connect(str(qmp_path))
            stream = qmp.makefile("rwb")
            json.loads(stream.readline())

            def qmp_call(name, args=None):
                command = {"execute": name}
                if args is not None:
                    command["arguments"] = args
                stream.write(json.dumps(command).encode() + b"\n")
                stream.flush()
                while True:
                    result = json.loads(stream.readline())
                    assert "error" not in result, result
                    if "return" in result:
                        return result["return"]

            qmp_call("qmp_capabilities")

            def click_at(x, y):
                point = [
                    {"type": "abs", "data": {"axis": "x", "value": x * 32767 // (w - 1)}},
                    {"type": "abs", "data": {"axis": "y", "value": y * 32767 // (height - 1)}},
                ]
                for events in (
                    point,
                    [{"type": "btn", "data": {"button": "left", "down": True}}],
                    [{"type": "btn", "data": {"button": "left", "down": False}}],
                ):
                    qmp_call("input-send-event", {"events": events})
                    time.sleep(0.15)
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
                "media",
                "photos",
                "snake",
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

            # Click the Max button using the VMware absolute-pointer protocol.
            g.key("f3")
            g.type("open files\n")
            fw = min(980, max(580, w * 72 // 100))
            fh = min(690, max(360, height * 78 // 100))
            click_at((w - fw) // 2 + fw - 98, (height - fh) // 2 + 21)
            g.wait("layout: files maximized")
            g.key("f11")
            g.key("f11")
            time.sleep(0.15)
            assert g.log.read_text().count("layout: files maximized") == 2
            g.monitor("nmi")
            g.wait("kernel stop: vector 2")
    print(
        "PASS twelve fullscreen layouts, restore, actual Max mouse target, previous mode restoration and CPU stop:",
        e,
    )


if __name__ == "__main__":
    p = argparse.ArgumentParser()
    p.add_argument("edition")
    p.add_argument("--shots", type=Path)
    a = p.parse_args()
    verify(a.edition, a.shots)
