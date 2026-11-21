"""Black-box tests of the packer/installer shipped to users inside the ZIP."""

import hashlib, struct, subprocess, sys, tempfile, zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def check(edition):
    src = ROOT / "build" / edition
    with tempfile.TemporaryDirectory() as tmp:
        d = Path(tmp)
        with zipfile.ZipFile(ROOT / "distros" / f"FreshOS-{edition}.zip") as z:
            for name in ("forge/imp.py", "native/compat/imports.def", "Home.img"):
                z.extract(name, d)

        def run(*a, reason=None):
            p = subprocess.run(
                [sys.executable, d / "forge/imp.py", *a],
                cwd=d,
                capture_output=True,
                text=True,
            )
            assert (
                p.returncode == 0
                if reason is None
                else p.returncode != 0 and reason in p.stderr
            ), p.stderr

        run(
            "pack",
            src / "dllmain.exe",
            "new.imp",
            reason="Missing import: greeting.dll!greeting",
        )
        assert not (d / "new.imp").exists()
        run("pack", src / "dllmain.exe", "new.imp", "--dll", src / "greeting.dll")
        assert (d / "new.imp").read_bytes() == (src / "windll.imp").read_bytes()
        elf = bytearray((src / "linux64.elf").read_bytes())
        struct.pack_into("<I", elf, struct.unpack_from("<Q", elf, 32)[0], 3)
        (d / "interp.elf").write_bytes(elf)
        run("pack", "interp.elf", "dynamic.imp", reason="dynamic Linux loader")
        assert not (d / "dynamic.imp").exists()
        run("install", "new.imp", "Home.img", "--name", "extra", "--stage")
        disk = d / "Home.img"
        digest = hashlib.sha256(disk.read_bytes()).digest()
        run(
            "install",
            "new.imp",
            "Home.img",
            "--name",
            "extra",
            reason="Name already installed",
        )
        assert hashlib.sha256(disk.read_bytes()).digest() == digest
        with disk.open("r+b") as f:
            f.seek(40960 * 512)
            f.write(b"foreign")
        digest = hashlib.sha256(disk.read_bytes()).digest()
        run(
            "install",
            "new.imp",
            "Home.img",
            "--name",
            "another",
            reason="Not a marked Home-2 image",
        )
        assert hashlib.sha256(disk.read_bytes()).digest() == digest
    print(
        "PASS packaged CLI, DLL dependency, dynamic ELF refusal, duplicate/foreign disk unchanged"
    )


if __name__ == "__main__":
    check(sys.argv[1])
