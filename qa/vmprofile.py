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
                scale = min(w / 960, h / 540)

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

                click(510 * w // 960, 455 * h // 540)  # Use OS
                click(int(16 * scale), int(15 * scale))
                g.wait("open: imp")
                # Windowed library, third row: inbox -> installed -> interactive Win32 dialog.
                frame_x = (w - min(980, w * 72 // 100)) // 2
                frame_y = (h - min(690, h * 78 // 100)) // 2
                row = (frame_x + 90, frame_y + 57 + 64 + 2 * 30 + 13)
                click(*row)
                g.wait("Package installed")
                click(*row)
                g.wait("Win32 dialog: windll")
                click(w // 2 + 198, h // 2 + 77)
                g.wait("IMP: windll exit 0")
                g.key("esc")
                click(int(16 * scale), int(277 * scale))
                click(int(115 * scale), int(216 * scale))
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
        profile = (d / "FreshOS-x64.vmx").read_text()
        assert all(
            s in profile
            for s in (
                'firmware = "efi"',
                'guestOS = "other-64"',
                'memsize = "96"',
                'ide0:0.fileName = "Home.vmdk"',
            )
        )
        with Guest(
            d, None, disk, True, ["-machine", "pc,vmport=on", "-vga", "vmware"]
        ) as g:
            g.wait("workspace ready")
            g.key("f3")
            g.type("install cal64\n")
            g.wait("Package installed")
            g.type("run cal64 2 2024\n")
            g.wait("process: PE64 user mode")
            g.wait("25 26 27 28 29")
    print(
        "PASS VM archive, VMDK descriptor + extent, absolute click, verified install and VMDK-only boot"
    )


if __name__ == "__main__":
    check()
