"""Exercise packaged VMware disk and pointer protocol in QEMU, not VMware itself."""

import struct, json, socket, tempfile, time, zipfile
from pathlib import Path
from boot import Guest, ROOT


def check():
    with tempfile.TemporaryDirectory() as tmp:
        d = Path(tmp)
        with zipfile.ZipFile(ROOT / "distros/FreshOS-VMware-Lite.zip") as z:
            z.extractall(d)
        iso = d / "FreshOS-VMware-Lite.iso"
        disk = d / "Home.vmdk"
        sock = d / "qmp"
        extra = [
            "-machine",
            "pc,vmport=on",
            "-vga",
            "vmware",
            "-qmp",
            f"unix:{sock},server=on,wait=off",
        ]
        with Guest(d, iso, disk, False, extra) as g:
            g.wait("workspace ready")
            g.wait("VMware absolute pointer enabled")
            w, h = g.shot(d / "desktop.jpg")
            with socket.socket(socket.AF_UNIX) as q:
                q.connect(str(sock))
                stream = q.makefile("rwb")
                stream.readline()

                def call(name, args=None):
                    cmd = {"execute": name}
                    if args:
                        cmd["arguments"] = args
                    stream.write(json.dumps(cmd).encode() + b"\n")
                    stream.flush()
                    while True:
                        result = json.loads(stream.readline())
                        assert "error" not in result, result
                        if "return" in result:
                            return result["return"]

                call("qmp_capabilities")
                x = max(34, min(w // 12, 122)) + 210
                y = max(97, min(h // 4, 260)) + 150

                def click(x, y):
                    for events in [
                        [
                            {
                                "type": "abs",
                                "data": {"axis": "x", "value": x * 32767 // (w - 1)},
                            },
                            {
                                "type": "abs",
                                "data": {"axis": "y", "value": y * 32767 // (h - 1)},
                            },
                        ],
                        [{"type": "btn", "data": {"button": "left", "down": True}}],
                        [{"type": "btn", "data": {"button": "left", "down": False}}],
                    ]:
                        call("input-send-event", {"events": events})
                        time.sleep(0.2)

                click(x, y)
                g.wait("open: files")
                g.key("f3")
                g.type("settings\n")
                g.key("f11")
                g.wait("layout: settings fullscreen")
                assert (w, h) == (1024, 768)
                click(250, 198)
                click(280, 300)
                click(222, 463)
                click(900, 175)
                click(770, 471)
                click(770, 504)
                click(770, 725)
                g.wait("Settings saved to Home")
                with (d / "Home.img").open("rb") as f:
                    f.seek(40962 * 512)
                    count, light, sound, accent, hide, wall, wide = struct.unpack(
                        "<7I", f.read(28)
                    )
                    assert (
                        count == 1
                        and 60 <= light <= 70
                        and 45 <= sound <= 60
                        and (accent, hide, wall, wide) == (2, 1, 1, 1)
                    ), (light, sound, accent, hide, wall, wide)
            g.key("f3")
            g.type("install\n")
            g.key("i")
            g.wait("install verified", 120)
        with Guest(d, None, disk, False) as g:
            g.wait("workspace ready")
            g.wait("Home disk / writable")
    print(
        "PASS VM archive, VMDK descriptor + extent, absolute click, verified install and VMDK-only boot"
    )


if __name__ == "__main__":
    check()
