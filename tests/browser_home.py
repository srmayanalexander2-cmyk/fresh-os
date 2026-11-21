"""Use the guest file manager to open a file older than the Recent list."""

import argparse, subprocess, tempfile, time
from pathlib import Path
from desktop import Home, Machine, Mouse


def verify(iso):
    with tempfile.TemporaryDirectory() as tmp:
        d = Path(tmp)
        disk = d / "home.img"
        subprocess.run(["cp", "--sparse=always", iso, disk], check=True)
        with disk.open("r+b") as f:
            h = Home(f)
            h.put(
                "aaa.html",
                b"<title>Local test</title><script>document.body.textContent='Local answer = '+(7*6);</script>",
            )
            for k in range(14):
                h.put(f"z{k}.txt", b"line\n")
        with Machine(d, iso, disk) as m:
            m.wait("welcome ready:")
            m.key("ret")
            m.wait("desktop ready")
            m.key("meta_l-e")
            m.wait("app: files")
            Mouse(m).click(460, 261)
            m.key("ret")
            m.wait("app: browser")
            m.key("f3")
            m.type(
                "savepage answer.txt\ncp answer.txt copy.txt\ncopy z0.txt copy.txt\n"
            )
            time.sleep(0.3)
        with disk.open("rb") as f:
            h = Home(f)
            assert h.read("answer.txt") == h.read("copy.txt") == b"Local answer = 42"
            assert len([r for r in h.records() if r[0] == "copy.txt"]) == 1
    print(
        "PASS sorted Home file outside Recent -> local Aero JS -> text export, copy and overwrite refusal"
    )


if __name__ == "__main__":
    p = argparse.ArgumentParser()
    p.add_argument("iso", type=Path)
    verify(p.parse_args().iso.resolve())
