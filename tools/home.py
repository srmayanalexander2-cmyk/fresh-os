#!/usr/bin/env python3
"""Exchange documents with an OFFLINE FreshOS raw disk image (never a device)."""

import zlib
import argparse, os, re, struct
from pathlib import Path

HOME = 32768
FIRST = HOME + 2
END = 15625000
MAX = 64 << 20


def digest(data):
    h = 2166136261
    for x in data:
        h = ((h ^ x) * 16777619) & 0xFFFFFFFF
    return h


def checked(name):
    if not re.fullmatch("[a-z0-9._-]{1,31}", name):
        raise ValueError("invalid Fresh Home name")
    return name.encode("ascii")


class Home:
    def __init__(self, f):
        self.f = f
        f.seek(0)
        m = f.read(512)
        if m[368:383] != b"FreshOS live v1" or struct.unpack_from("<II", m, 470) != (
            HOME,
            END - HOME,
        ):
            raise ValueError("not a release FreshOS disk image")
        f.seek(0, 2)
        if f.tell() < END * 512:
            raise ValueError("truncated disk")
        good = []
        for slot in (HOME, HOME + 1):
            s = self.at(slot, 512)
            tag, seq, tail, last, count, h = struct.unpack_from("<8sIIIII", s)
            if (
                tag == b"FRSHOME1"
                and h == digest(s[:24])
                and FIRST <= tail <= END
                and count <= 65536
                and (
                    (not count and not last and tail == FIRST)
                    or (count and FIRST <= last < tail)
                )
            ):
                good.append((seq, tail, last, count))
        if not good:
            raise ValueError("no valid superblock")
        self.seq, self.tail, self.last, self.count = max(good)

    def at(self, sector, n):
        self.f.seek(sector * 512)
        return self.f.read(n)

    def records(self):
        pos = self.last
        for _ in range(self.count):
            if not FIRST <= pos < self.tail:
                raise ValueError("bad log chain")
            tag, raw, size, prev, h = struct.unpack_from("<8s32sIII", self.at(pos, 512))
            name = raw.rstrip(b"\0").decode("ascii")
            checked(name)
            if (
                tag != b"FRSHREC1"
                or size > MAX
                or pos + 1 + (size + 511) // 512 > self.tail
                or prev >= pos
            ):
                raise ValueError("bad record")
            yield name, pos, size, h
            pos = prev
        if pos:
            raise ValueError("overlong log chain")

    def read(self, name):
        for fname, pos, n, h in self.records():
            if fname == name:
                data = self.at(pos + 1, n)
                if digest(data) != h:
                    raise ValueError("checksum mismatch")
                return data
        raise ValueError("file not found")

    def put(self, name, data):
        n = len(data)
        end = self.tail + 1 + (n + 511) // 512
        if n > MAX or end > END or self.count >= 65536:
            raise ValueError("file or volume capacity exceeded")
        header = struct.pack(
            "<8s32sIII", b"FRSHREC1", checked(name), n, self.last, digest(data)
        ).ljust(512, b"\0")
        f = self.f
        f.seek(self.tail * 512)
        f.write(header)
        f.write(data)
        f.write(bytes((-n) % 512))
        f.flush()
        os.fsync(f.fileno())
        s = struct.pack(
            "<8sIIII", b"FRSHOME1", self.seq + 1, end, self.tail, self.count + 1
        )
        f.seek((HOME + ((self.seq + 1) & 1)) * 512)
        f.write((s + struct.pack("<I", digest(s))).ljust(512, b"\0"))
        f.flush()
        os.fsync(f.fileno())
        self.seq += 1
        self.last = self.tail
        self.tail = end
        self.count += 1


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("image", type=Path)
    sub = p.add_subparsers(dest="op", required=True)
    sub.add_parser("ls")
    for op in ["put", "put-imp", "put-exe", "get"]:
        q = sub.add_parser(op)
        q.add_argument("name")
        q.add_argument("file", type=Path)
    a = p.parse_args()
    writing = a.op.startswith("put")
    if not a.image.is_file() or a.image.is_symlink():
        p.error("use a detached regular raw disk image, not a device or symlink")
    with a.image.open("r+b" if writing else "rb") as f:
        h = Home(f)
        if a.op == "ls":
            for name, pos, n, _ in h.records():
                print(f"{n:10}  {name}")
        elif a.op == "get":
            data = h.read(a.name)
            with a.file.open("xb") as dest:
                dest.write(data)
        else:
            if a.name in ("profile", "canvas"):
                p.error("reserved system record")
            limit = (2 << 20) if a.op in ("put-imp", "put-exe") else MAX
            if a.op == "put-imp":
                limit += 2080
            if a.file.stat().st_size > limit:
                p.error(f"File exceeds {limit} bytes")
            data = a.file.read_bytes()
            if a.op == "put-imp":
                if (
                    not a.name.endswith(".imp")
                    or len(data) < 32
                    or data[:4] != b"FIMP"
                    or data[4:8] not in [bytes([1, b, 1, 32]) for b in (32, 64)]
                    or struct.unpack_from("<I", data, 8)[0] != len(data)
                    or zlib.crc32(data[:28] + bytes(4) + data[32:])
                    != struct.unpack_from("<I", data, 28)[0]
                ):
                    p.error("Valid IMP1 header, checksum and .imp destination required")
            if a.op == "put-exe" and not data.startswith(b"MZ"):
                p.error("A Windows PE executable is required")
            h.put(a.name, data)
