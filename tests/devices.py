"""Boot with actual PCI device models and UART protocol fixtures (not RF tests)."""

import argparse, socket, subprocess, sys, tempfile, time
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from smoke import Machine, Home, ROOT


def read_until(f, marker):
    tail = b""
    for _ in range(65536):
        c = f.read(1)
        if not c:
            return False
        tail = (tail + c)[-len(marker) :]
        if tail == marker:
            return True
    return False


def esp(w):
    with w.makefile("rb") as f:
        assert read_until(f, b"ATE0\r\n")
        w.sendall(b"OK\r\n")
        for cmd in (b"AT+CWMODE=1", b'AT+CWJAP="lab","unitpass"'):
            assert f.readline().strip() == cmd
            w.sendall(b"OK\r\n")


def controller(b):
    with b.makefile("rb") as f:
        assert read_until(f, bytes.fromhex("01030c00"))
        b.sendall(bytes.fromhex("040e0401030c00"))
        assert f.read(4) == bytes.fromhex("01091000")
        b.sendall(bytes.fromhex("040e0a01091000060504030201"))
        assert f.read(4) == bytes.fromhex("01030c00")
        b.sendall(bytes.fromhex("040e0401030c00"))
        assert f.read(9) == bytes.fromhex("01010405338b9e0408")
        b.sendall(
            bytes.fromhex("040f0400010104")
            + bytes.fromhex("04020f010605040302010100000000000000")
            + bytes.fromhex("04010100")
        )


def verify(iso, uefi, sata=False):
    with tempfile.TemporaryDirectory() as tmp:
        d = Path(tmp)
        disk = d / "disk.img"
        audio = d / "output.wav"
        subprocess.run([sys.executable, ROOT / "tools/new-target.py", disk], check=True)
        with socket.socket(socket.AF_UNIX) as wifi, socket.socket(socket.AF_UNIX) as bt:
            for s, path in [(wifi, "wifi"), (bt, "bt")]:
                s.bind(str(d / path))
                s.listen(1)
                s.settimeout(20)
            extra = [
                "-drive",
                f"file={disk},if=none,format=raw,id=home",
                "-device",
                "virtio-blk-pci,drive=home,disable-modern=on",
                "-netdev",
                "user,id=lan",
                "-device",
                "pcnet,netdev=lan",
                "-audiodev",
                f"wav,id=snd,path={audio},out.frequency=48000",
                "-device",
                "AC97,audiodev=snd",
                "-serial",
                "unix:" + str(d / "wifi"),
                "-serial",
                "unix:" + str(d / "bt"),
            ]
            if sata:
                extra[2:4] = [
                    "-device",
                    "ich9-ahci,id=sata",
                    "-device",
                    "ide-hd,drive=home,bus=sata.0",
                ]
            with Machine(d, iso, uefi=uefi, extra=extra) as m:
                w, _ = wifi.accept()
                b, _ = bt.accept()
                w.settimeout(120)
                b.settimeout(120)
                pool = ThreadPoolExecutor(max_workers=2)
                with w, b:
                    wf = pool.submit(esp, w)
                    bf = pool.submit(controller, b)
                    m.wait("AHCI SATA ready" if sata else "virtio-blk ready")
                    m.wait("PCnet bound")
                    m.wait("Intel AC97")
                    m.wait("welcome ready:")
                    m.key("ret")
                    m.wait("desktop ready")
                    m.key("f3")
                    m.type("install\n")
                    m.key("i")
                    m.wait("installation verified", timeout=150)
                    time.sleep(0.4)
                    m.key("esc")
                    m.type("net dhcp\n")
                    m.wait("DHCP lease acquired", timeout=8)
                    m.wait("Gateway ARP resolved", timeout=8)
                    m.type("open chime.wav\n")
                    m.wait("WAV playing")
                    time.sleep(0.5)
                    m.type("open postcard.exe\n")
                    m.wait("Document permission required")
                    m.key("n")
                    m.wait("EXE to IMP32 completed")
                    m.key("f4")
                    with disk.open("rb") as f:
                        assert all(x[0] != "postcard.txt" for x in Home(f).records())
                    m.type("convert postcard.exe\nopen postcard.imp\n")
                    m.wait("Document permission required", 2)
                    m.key("y")
                    m.wait("IMP32 completed", 2)
                    m.key("f4")
                    m.type("open linux.elf\n")
                    m.wait("Linux ELF completed")
                    m.key("f4")
                    m.type("wifi join lab\nunitpass\n")
                    m.wait("ESP-AT complete")
                    m.type("bt info\n")
                    m.wait("Bluetooth controller responded")
                    m.type("bt scan\n")
                    m.wait("Bluetooth inquiry ended")
                    assert "unitpass" not in m.log.read_text()
                    wf.result(5)
                    bf.result(5)
        with disk.open("rb") as f:
            assert (
                Home(f).read("postcard.txt")
                == b"A postcard from the Windows 11 Pro compatibility profile.\n"
            )
        # Boot installed media through the same controller, without the original ISO.
        disk_args = extra[: extra.index("-netdev")]
        with Machine(d, uefi=uefi, extra=disk_args) as m:
            m.wait("welcome ready:")
            m.key("ret")
            m.key("f3")
            m.type("open postcard.imp\n")
            m.wait("Document permission required")
            m.key("n")
            m.wait("IMP32 completed")
        raw = audio.read_bytes()
        assert (
            raw[:4] == b"RIFF"
            and raw[8:12] == b"WAVE"
            and raw[36:40] == b"data"
            and any(raw[44:])
        ), "silent AC97 capture"
        print(
            f"PASS: {'AHCI' if sata else 'virtio'} install and Home consent, PCnet DHCP/gateway ARP, non-silent AC97 PCM, Linux ELF, ESP-AT join and Bluetooth H4 UART fixtures"
        )


if __name__ == "__main__":
    p = argparse.ArgumentParser()
    p.add_argument("iso", type=Path)
    p.add_argument("--uefi", action="store_true")
    p.add_argument("--sata", action="store_true")
    a = p.parse_args()
    verify(a.iso.resolve(), a.uefi, a.sata)
