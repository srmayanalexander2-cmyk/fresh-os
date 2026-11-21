"""IMP2 packer and offline installer for marked Stratus Home images."""

import argparse, os, re, stat, struct, zlib
from pathlib import Path
import pefile

ROOT = Path(__file__).resolve().parents[1]
APIS = {
    (dll, sym): 0x7FE00000 + 16 * i
    for i, (dll, sym) in enumerate(
        re.findall(
            r'API\("([^"]+)", "([^"]+)"',
            (ROOT / "native/compat/imports.def").read_text(),
        )
    )
}
IMAGE = 2 << 20
LIMIT = 1 << 20
SLOTS = 8
START = 41472
STRIDE = 2049
MARK = b"FRESHOS2/HOME/01" + struct.pack("<I", 257)


def require(test, msg):
    if not test:
        raise ValueError(msg)


def slug(s):
    return re.fullmatch(r"[a-z0-9][a-z0-9_.-]{0,26}", s) is not None


def read(path, limit):
    with path.open("rb") as f:
        data = f.read(limit + 1)
    require(0 < len(data) <= limit, f"{path.name}: empty or exceeds {limit} bytes")
    return data


def link(exe, libs):
    """Map PE sections, bind named imports and relocate local DLLs into one private image."""
    image = bytearray()
    perms = bytearray()
    mods = {}
    base = bits = entry = 0
    for i, path in enumerate([exe, *libs]):
        raw = read(path, IMAGE)
        pe = pefile.PE(data=raw, fast_load=True)
        h = pe.OPTIONAL_HEADER
        dll = path.name.lower()
        require(
            dll not in mods and not any(dll == d for d, _ in APIS),
            "Duplicate or reserved module name",
        )
        arch = {0x14C: 32, 0x8664: 64}.get(pe.FILE_HEADER.Machine)
        require(
            arch and h.Magic == (0x10B if arch == 32 else 0x20B),
            "Requires an x86 or x64 PE",
        )
        require(
            h.Subsystem in (2, 3)
            and bool(pe.FILE_HEADER.Characteristics & 0x2000) == bool(i)
            and pe.FILE_HEADER.Characteristics & 2,
            "Requires a user-mode EXE followed by optional DLLs",
        )
        if not i:
            bits, base, entry = arch, h.ImageBase, h.AddressOfEntryPoint
            require(entry < h.SizeOfImage, "EXE entry is outside its image")
        require(
            bits == arch and (not i or h.AddressOfEntryPoint == 0),
            "DLL architecture mismatch or DllMain initialization required",
        )
        off = len(image)
        size = h.SizeOfImage
        require(
            0 < size <= IMAGE - off and size % 4096 == 0 and h.SectionAlignment >= 4096,
            "Mapped image exceeds 2 MiB or unsupported alignment",
        )
        require(0 < base < base + off + size <= 1 << bits, "Invalid address range")
        for j, d in enumerate(h.DATA_DIRECTORY):
            if (
                d.Size
                and j not in (0, 1, 2, 4, 5, 6, 12)
                and not (bits == 64 and j == 3)
            ):
                raise ValueError(f"{dll}: unsupported PE directory {d.name}")
        require(
            len(pe.sections) == pe.FILE_HEADER.NumberOfSections
            and 0 < len(pe.sections) <= 96,
            "Invalid section count",
        )
        pe.parse_data_directories(directories=[0, 1, 5])
        buf = bytearray(size)
        flags = bytearray(size // 4096)
        n = h.SizeOfHeaders
        require(0 < n <= min(size, len(raw)), "Invalid PE headers")
        buf[:n] = raw[:n]
        flags[: (n + 4095) // 4096] = bytes([1]) * ((n + 4095) // 4096)
        occupied = set(range((n + 4095) // 4096))
        for sec in pe.sections:
            a = sec.VirtualAddress
            n = sec.SizeOfRawData
            k = sec.PointerToRawData
            m = max(n, sec.Misc_VirtualSize)
            require(
                a % 4096 == 0 and a + m <= size and (not n or k + n <= len(raw)),
                "Invalid PE section",
            )
            f = (
                (1 if sec.Characteristics & 0x40000000 else 0)
                | (2 if sec.Characteristics & 0x80000000 else 0)
                | (4 if sec.Characteristics & 0x20000000 else 0)
            )
            pages = (m + 4095) // 4096
            indices = range(a // 4096, a // 4096 + pages)
            require(occupied.isdisjoint(indices), "Overlapping sections")
            occupied.update(indices)
            flags[a // 4096 : a // 4096 + pages] = bytes([f]) * pages
            buf[a : a + n] = raw[k : k + n]
        delta = base + off - h.ImageBase
        p = bits // 8
        if delta:
            rel = getattr(pe, "DIRECTORY_ENTRY_BASERELOC", None)
            require(rel, "Local DLL is not relocatable")
            for block in rel:
                for e in block.entries:
                    if not e.type:
                        continue
                    a = e.rva
                    require(
                        e.type == (10 if bits == 64 else 3) and a + p <= size,
                        "Unsupported base relocation",
                    )
                    v = int.from_bytes(buf[a : a + p], "little")
                    buf[a : a + p] = ((v + delta) % (1 << bits)).to_bytes(p, "little")
        mods[dll] = (pe, off)
        image.extend(buf)
        perms.extend(flags)
    require(entry < len(image) and perms[entry // 4096] & 4, "Invalid entry point")
    for pe, off in mods.values():
        h = pe.OPTIONAL_HEADER
        descriptors = getattr(pe, "DIRECTORY_ENTRY_IMPORT", [])
        require(
            len(h.DATA_DIRECTORY) < 2
            or not h.DATA_DIRECTORY[1].Size
            or descriptors
            or pe.get_data(h.DATA_DIRECTORY[1].VirtualAddress, 20) == bytes(20),
            "Malformed import table",
        )
        for desc in descriptors:
            dll = desc.dll.decode("ascii").lower()
            for imp in desc.imports:
                sym = (imp.name or b"").decode("ascii")
                address = APIS.get((dll, sym))
                p = bits // 8
                if address is None and dll in mods:
                    lib, offset = mods[dll]
                    exports = getattr(lib, "DIRECTORY_ENTRY_EXPORT", None)
                    for e in exports.symbols if exports else []:
                        if (
                            (
                                e.name == imp.name
                                if imp.name
                                else e.ordinal == imp.ordinal
                            )
                            and not e.forwarder
                            and 0 < e.address < lib.OPTIONAL_HEADER.SizeOfImage
                        ):
                            address = base + offset + e.address
                            break
                require(
                    address is not None, f"Missing import: {dll}!{sym or imp.ordinal}"
                )
                a = imp.address - h.ImageBase
                require(0 <= a <= h.SizeOfImage - p, "Invalid IAT")
                image[off + a : off + a + p] = address.to_bytes(p, "little")
    body = bytearray()
    for i, f in enumerate(perms):
        chunk = image[i * 4096 : i * 4096 + 4096].rstrip(b"\0") if f else b""
        body += struct.pack("<BBH", f, 0, len(chunk)) + chunk
    out = (
        bytearray(
            struct.pack(
                "<4sBBBBIIQII",
                b"FIMP",
                1,
                bits,
                1,
                32,
                32 + len(body),
                len(image),
                base,
                entry,
                0,
            )
        )
        + body
    )
    struct.pack_into("<I", out, 28, zlib.crc32(out))
    return out


def pack(path, libs, resources, native64=False):
    data = read(path, IMAGE)
    linux = data.startswith(b"\x7fELF")
    require(not linux or not libs, "ELF shared libraries are not supported")
    code = data if linux else link(path, libs)
    if linux:
        require(
            len(data) >= 52
            and data[4] in (1, 2)
            and data[5] == 1
            and int.from_bytes(data[16:18], "little") == 2,
            "Requires a static x86 ET_EXEC ELF",
        )
        wide = data[4] == 2
        ph = int.from_bytes(data[32:40] if wide else data[28:32], "little")
        ps, n = struct.unpack_from("<HH", data, 54 if wide else 42)
        require(
            ps == (56 if wide else 32)
            and 0 < n <= 64
            and ph + n * ps <= len(data)
            and int.from_bytes(data[18:20], "little") == (62 if wide else 3),
            "Invalid x86 ELF",
        )
        require(
            not any(
                struct.unpack_from("<I", data, ph + j * ps)[0] in (2, 3, 7)
                for j in range(n)
            ),
            "Requires dynamic Linux loader, shared libraries or TLS",
        )
    require(
        not native64 or (code[4] == 2 if linux else code[5] == 64),
        "--native64 requires a 64-bit executable",
    )
    body = bytearray(code)
    names = set()
    require(len(resources) <= 16, "Too many resources")
    for f in resources:
        s = f.name
        require(slug(s) and s not in names, "Invalid or duplicate resource name")
        names.add(s)
        buf = read(f, 16384)
        body += struct.pack("<28sI", s.encode(), len(buf)) + buf
    out = (
        struct.pack(
            "<4sIIII",
            b"IMP2",
            (int(linux) | (2 if native64 else 0)),
            len(code),
            len(resources),
            zlib.crc32(body),
        )
        + body
    )
    require(len(out) <= LIMIT, "Package exceeds 1 MiB")
    return out


def install(disk, name, data, staged=False):
    require(slug(name), "Invalid app name")
    require(
        20 <= len(data) <= LIMIT
        and data[:4] == b"IMP2"
        and zlib.crc32(data[20:]) == struct.unpack_from("<I", data, 16)[0],
        "Invalid IMP2 package",
    )
    with disk.open("r+b") as f:
        require(
            stat.S_ISREG(os.fstat(f.fileno()).st_mode)
            and os.fstat(f.fileno()).st_size == 32 << 20,
            "Expected a 32 MiB regular Home image",
        )
        m = f.read(512)
        f.seek(40960 * 512)
        require(
            (
                m.startswith(MARK)
                or (m[400:416] == b"STRATUS/BOOT/002" and m[510:] == b"\x55\xaa")
            )
            and f.read(20) == MARK,
            "Not a marked Home-2 image",
        )
        empty = None
        for i in range(SLOTS):
            at = (START + STRIDE * i) * 512
            f.seek(at)
            hdr = f.read(512)
            if hdr == bytes(512):
                empty = at if empty is None else empty
                continue
            require(hdr[:8] in (b"F2PKG001", b"F2INB001"), "Unrecognized package area")
            require(
                hdr[8:36].split(b"\0")[0] != name.encode(), "Name already installed"
            )
        require(empty is not None, "No empty package slots")
        f.seek(empty + 512)
        f.write(data)
        f.flush()
        os.fsync(f.fileno())
        f.seek(empty)
        f.write(
            struct.pack(
                "<8s28sI",
                b"F2INB001" if staged else b"F2PKG001",
                name.encode(),
                len(data),
            )
        )
        f.flush()
        os.fsync(f.fileno())


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    sub = p.add_subparsers(dest="cmd", required=True)
    q = sub.add_parser("pack")
    q.add_argument(
        "--native64", action="store_true", help="Require the x64 UEFI CPU backend"
    )
    q.add_argument("source", type=Path)
    q.add_argument("output", type=Path)
    q.add_argument("--dll", type=Path, action="append", default=[])
    q.add_argument("--resource", type=Path, action="append", default=[])
    q = sub.add_parser("install")
    q.add_argument(
        "--stage",
        action="store_true",
        help="Leave package in guest inbox for interactive installation",
    )
    q.add_argument("package", type=Path)
    q.add_argument("home", type=Path)
    q.add_argument("--name", required=True)
    a = p.parse_args()
    try:
        if a.cmd == "pack":
            data = pack(a.source, a.dll, a.resource, a.native64)
            with a.output.open("xb") as f:
                f.write(data)
        else:
            install(a.home, a.name, read(a.package, LIMIT), a.stage)
    except (ValueError, OSError, pefile.PEFormatError, struct.error, IndexError) as e:
        p.exit(1, f"imp: {e}\n")
