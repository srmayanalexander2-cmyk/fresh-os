"""Report static dependencies AND try the real packer. Never a runtime pass."""

import argparse
import hashlib
import sys
from pathlib import Path
import pefile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "forge"))
from imp import APIS, pack


def preflight(path):
    pe = pefile.PE(str(path), fast_load=True)
    pe.parse_data_directories(directories=[1, 13])
    print(
        f"{path.name}: {path.stat().st_size} bytes; mapped {pe.OPTIONAL_HEADER.SizeOfImage}; "
        f"sha256 {hashlib.sha256(path.read_bytes()).hexdigest()}"
    )
    for kind in ("DIRECTORY_ENTRY_IMPORT", "DIRECTORY_ENTRY_DELAY_IMPORT"):
        for dep in getattr(pe, kind, []):
            dll = dep.dll.decode().lower()
            names = [
                i.name.decode() if i.name else f"#{i.ordinal}" for i in dep.imports
            ]
            missing = sorted(set(n for n in names if (dll, n) not in APIS))
            if missing:
                print(f"  {kind} {dll}: " + ", ".join(missing))
    try:
        pack(path, [], [])
    except ValueError as err:
        print("  PACK BLOCKED:", err)
        return False
    print("  PACK OK (not executed)")
    return True


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("exe", type=Path, nargs="+")
    args = parser.parse_args()
    sys.exit(not all([preflight(path) for path in args.exe]))
