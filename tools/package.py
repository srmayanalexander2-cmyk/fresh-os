"""Stream the full ISO into a portable ZIP64 archive with its uncompressed SHA-256."""

import argparse, hashlib, zipfile
from pathlib import Path
from check import verify

ROOT = Path(__file__).resolve().parents[1]


def pack(edition):
    iso = ROOT / "out" / f"FreshOS-{edition}.iso"
    verify(iso)
    out = ROOT / "releases"
    out.mkdir(exist_ok=True)
    path = out / f"FreshOS-{edition}.zip"
    digest = hashlib.sha256()
    temp = path.with_suffix(".tmp")
    with zipfile.ZipFile(
        temp, "w", zipfile.ZIP_DEFLATED, compresslevel=6, allowZip64=True
    ) as z:
        with iso.open("rb") as src, z.open(iso.name, "w", force_zip64=True) as dst:
            while chunk := src.read(4 << 20):
                digest.update(chunk)
                dst.write(chunk)
        check = f"{digest.hexdigest()}  {iso.name}\n"
        z.writestr("SHA256SUMS.txt", check)
        z.write(ROOT / "README.md", "README.txt")
        z.write(ROOT / "docs/coverage.md", "FEATURES.txt")
        z.write(ROOT / "docs/licenses/DejaVu.txt", "FONT-LICENSE.txt")
        z.write(ROOT / "docs/release-1.md", "RELEASE-1.txt")
        for name in ("base.apk", "hello32.exe", "hello64.exe"):
            z.write(ROOT / "build" / edition / name, "examples/" + name)
        if edition == "VMware":
            for name in ["FreshOS.vmx", "FreshTarget.vmdk"]:
                z.write(ROOT / "out" / name, name)
    temp.replace(path)
    (out / f"FreshOS-{edition}.sha256").write_text(check)
    print(
        f"{path.name}: {path.stat().st_size:,} bytes; ISO SHA-256: {digest.hexdigest()}",
        flush=True,
    )


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("edition", choices=["Normal", "Lite", "VMware", "all"])
    e = p.parse_args().edition
    for edition in ["Normal", "Lite", "VMware"] if e == "all" else [e]:
        pack(edition)
