"""Copy a self-contained v86 harness for the actual optical image. No external URLs at run time."""

import argparse, shutil
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser()
p.add_argument("v86", type=Path, help="v86 npm package directory")
p.add_argument("bios", type=Path, help="v86 bios/ directory")
a = p.parse_args()
out = ROOT / "build/preview"
out.mkdir(exist_ok=True, parents=True)
for name in ("libv86.js", "v86.wasm", "v86-fallback.wasm"):
    shutil.copyfile(a.v86 / "build" / name, out / name)
for name in ("seabios.bin", "vgabios.bin", "COPYING.LESSER"):
    shutil.copyfile(a.bios / name, out / name)
shutil.copyfile(a.v86 / "LICENSE", out / "V86-LICENSE")
shutil.copyfile(ROOT / "preview/index.html", out / "index.html")
shutil.copyfile(ROOT / "build/Normal/FreshOS-Normal.iso", out / "live.iso")
for name in (ROOT / "distros").glob("*.zip"):
    shutil.copyfile(name, out / name.name)
