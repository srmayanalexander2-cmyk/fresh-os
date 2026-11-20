"""Assemble the optional emulator harness without modifying the release ISO."""

import argparse, shutil, struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument("v86_build", type=Path)
p.add_argument("bios_dir", type=Path)
a = p.parse_args()
dst = ROOT / "build/preview"
dst.mkdir(parents=True, exist_ok=True)
for name in ("libv86.js", "v86.wasm", "v86-fallback.wasm"):
    shutil.copy2(a.v86_build / name, dst / name)
for name in ("seabios.bin", "vgabios.bin"):
    shutil.copy2(a.bios_dir / name, dst / name)
shutil.copy2(a.bios_dir / "COPYING.LESSER", dst / "FIRMWARE-LICENSE")
shutil.copy2(a.v86_build.parent / "LICENSE", dst / "V86-LICENSE")
shutil.copy2(Path(__file__).with_name("index.html"), dst / "index.html")
for e in ("Normal", "Lite", "VMware", "VMware-Lite"):
    shutil.copy2(ROOT / "releases" / f"FreshOS-{e}.zip", dst)
with (ROOT / "out/FreshOS-Normal.iso").open("rb") as src:
    src.seek(16 * 2048 + 80)
    count = struct.unpack("<I", src.read(4))[0]
    assert count * 2048 <= 16 << 20
    src.seek(0)
    (dst / "preview.iso").write_bytes(src.read(count * 2048))
print(
    "Serve only build/preview (e.g. python3 -m http.server --bind 0.0.0.0 --directory build/preview 8080)."
)
