"""Cross-check the freestanding encoders against independent standard libraries."""

import ctypes as c
import gzip
import hashlib
import io
from pathlib import Path
import subprocess
import tarfile
import tempfile

root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as tmp:
    lib = Path(tmp) / "filetools.so"
    subprocess.run(
        ["cc", "-shared", "-fPIC", root / "kernel/filetools.c", "-o", lib], check=True
    )
    f = c.CDLL(str(lib))
    for size in (0, 1, 135, 136, 137, 511, 512, 513, 65535, 65536, 131072):
        data = bytes(range(256)) * (size // 256) + bytes(range(size % 256))
        out = c.create_string_buffer(size + 2048)
        f.sha3_256(data, size, out)
        assert out.value.decode() == hashlib.sha3_256(data).hexdigest()
        n = f.gzip_store(out, len(out), data, size)
        assert n and gzip.decompress(out.raw[:n]) == data
        assert not f.gzip_store(out, n - 1, data, size)
        n = f.tar_member(out, len(out), b"sample.bin", data, size)
        assert n and not f.tar_member(out, n - 1, b"sample.bin", data, size)
        assert not f.tar_member(out, len(out), b"../bad", data, size)
        with tarfile.open(fileobj=io.BytesIO(out.raw[:n] + bytes(1024))) as t:
            assert t.extractfile("sample.bin").read() == data
print("PASS SHA3-256, ustar and gzip: reference results, block boundaries and limits")
