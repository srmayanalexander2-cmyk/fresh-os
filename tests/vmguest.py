"""Negotiate VMware's guest protocol against QEMU vmport; use absolute input.
Not a substitute for testing VMware Workstation/Fusion itself.
"""

import argparse, json, socket, tempfile, sys, time
from pathlib import Path
from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from smoke import Machine


def verify(iso):
    with tempfile.TemporaryDirectory() as temp:
        d = Path(temp)
        qmp = d / "qmp"
        screen = d / "frame.ppm"
        with Machine(
            d,
            iso,
            extra=[
                "-vga",
                "vmware",
                "-machine",
                "vmport=on",
                "-qmp",
                f"unix:{qmp},server=on,wait=off",
            ],
        ) as m:
            m.wait("VMware guest / absolute pointer active")
            m.wait("welcome ready:")
            time.sleep(0.3)
            m.monitor(f"screendump {screen}")
            time.sleep(0.25)
            with Image.open(screen) as im:
                sw, sh = im.size
            dw, dh = sw, sh  # Fill is the default; map the entire firmware framebuffer.
            with socket.socket(socket.AF_UNIX) as s:
                s.connect(str(qmp))
                stream = s.makefile("rb")
                assert "QMP" in json.loads(stream.readline())

                def cmd(name, args={}):
                    s.sendall(
                        (json.dumps(dict(execute=name, arguments=args)) + "\n").encode()
                    )
                    while True:
                        msg = json.loads(stream.readline())
                        assert "error" not in msg, msg
                        if "return" in msg:
                            return msg["return"]

                cmd("qmp_capabilities")

                def click(x, y):
                    x = ((sw - dw) // 2 + x * dw // 1280) * 32767 // sw
                    y = ((sh - dh) // 2 + y * dh // 720) * 32767 // sh
                    cmd(
                        "input-send-event",
                        {
                            "events": [
                                dict(type="abs", data={"axis": a, "value": v})
                                for a, v in [("x", x), ("y", y)]
                            ]
                        },
                    )
                    time.sleep(0.15)
                    for down in (True, False):
                        cmd(
                            "input-send-event",
                            {
                                "events": [
                                    dict(
                                        type="btn",
                                        data={"button": "left", "down": down},
                                    )
                                ]
                            },
                        )
                        time.sleep(0.12)

                click(836, 450)
                m.wait("desktop ready", timeout=5)
                click(23, 25)
                m.wait("app: files", timeout=5)
        print("PASS: VMware protocol absolute pointer, Welcome and Files hit targets")


if __name__ == "__main__":
    p = argparse.ArgumentParser()
    p.add_argument("iso", type=Path)
    verify(p.parse_args().iso.resolve())
