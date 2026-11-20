"""Black-box tests of the same freestanding runtimes linked into both kernels."""

import ctypes as c
import zlib, struct, subprocess, sys, tempfile, unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from exe_examples import make


class Result(c.Structure):
    _fields_ = [
        ("output", c.c_char * 768),
        ("detail", c.c_char * 128),
        ("code", c.c_uint),
        ("steps", c.c_uint),
        ("bits", c.c_int),
    ]


class Compat(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory()
        cls.work = Path(cls.temp.name)
        make(cls.work)
        so = cls.work / "compat.so"
        subprocess.run(
            [
                "gcc",
                "-O2",
                "-Wall",
                "-Wextra",
                "-Werror",
                "-shared",
                "-fPIC",
                ROOT / "kernel/pe.c",
                "-o",
                so,
            ],
            check=True,
        )
        cls.lib = c.CDLL(str(so))
        cls.lib.pe_run.argtypes = [c.c_char_p, c.c_uint, c.POINTER(Result)]
        cls.lib.imp_run.argtypes = cls.lib.pe_run.argtypes
        cls.lib.imp_convert.argtypes = [
            c.c_char_p,
            c.c_uint,
            c.c_void_p,
            c.c_uint,
            c.POINTER(Result),
        ]

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

    def pe(self, data, ok=True):
        r = Result()
        self.assertEqual(
            bool(self.lib.pe_run(bytes(data), len(data), c.byref(r))), ok, r.detail
        )
        if ok:
            imp = self.link(data)
            rr = Result()
            self.assertTrue(self.lib.imp_run(imp, len(imp), c.byref(rr)), rr.detail)
            self.assertEqual(bytes(rr), bytes(r))
        return r

    def link(self, data):
        r = Result()
        out = c.create_string_buffer(2100000)
        n = self.lib.imp_convert(bytes(data), len(data), out, len(out), c.byref(r))
        self.assertGreater(n, 0, r.detail)
        return out.raw[:n]

    def test_real_exes(self):
        for bits in (32, 64):
            r = self.pe((self.work / f"hello{bits}.exe").read_bytes())
            self.assertIn(f"{bits}-bit Windows EXE".encode(), r.output)
            self.assertEqual((r.code, r.bits), (0, bits))
            self.assertGreater(r.steps, 5)

    def test_compiled_c(self):
        for bits in (32, 64):
            mode = "i386pe" if bits == 32 else "i386pep"
            pref = "_" if bits == 32 else ""
            w = self.work / f"pe{bits}"

            def run(*args):
                subprocess.run(args, cwd=w, check=True)

            (w / "crt.def").write_text(
                "LIBRARY msvcrt.dll\nEXPORTS\n"
                + "".join(
                    f" {sym}=stub\n"
                    for sym in ["calloc", "free", "memcpy", "puts", "strlen"]
                )
            )
            run(
                "ld",
                "-m",
                mode,
                "--dll",
                "--out-implib",
                "crt.a",
                "-e",
                pref + "stub",
                "-o",
                "msvcrt.dll",
                "crt.def",
                "stub.o",
            )
            for prog in ["compute", "crt"]:
                run(
                    "gcc",
                    f"-m{bits}",
                    "-Os",
                    "-ffreestanding",
                    "-fno-builtin",
                    "-fno-ident",
                    "-fpie" if bits == 64 else "-fno-pie",
                    "-mabi=ms" if bits == 64 else "-fleading-underscore",
                    "-fno-stack-protector",
                    "-fno-asynchronous-unwind-tables",
                    "-c",
                    ROOT / "tests" / f"{prog}.c",
                    "-o",
                    "test.o",
                )
                run(
                    "ld",
                    "-m",
                    mode,
                    "--subsystem",
                    "console",
                    "-e",
                    pref + "entry",
                    "-s",
                    "-o",
                    "test.exe",
                    "test.o",
                    "crt.a",
                )
                r = self.pe((w / "test.exe").read_bytes())
                self.assertEqual(r.code, 34 if prog == "compute" else 0)
                self.assertEqual(
                    r.output,
                    b"" if prog == "compute" else b"CRT imports from compiled C\n",
                )

    def test_diagnostics_and_budget(self):
        f = (self.work / "hello32.exe").read_bytes()
        r = self.pe(f.replace(b"WriteFile\0", b"OtherCall\0"), False)
        self.assertIn(b"Missing import: KERNEL32.dll!OtherCall", r.detail)
        self.assertIn(b"Invalid", self.pe(f[:24], False).detail)
        p = struct.unpack_from("<I", f, 60)[0]
        entry = struct.unpack_from("<I", f, p + 24 + 16)[0]
        sec = p + 24 + struct.unpack_from("<H", f, p + 20)[0]
        rva, raw, offset = struct.unpack_from("<III", f, sec + 12)
        code = offset + entry - rva
        self.assertLess(code, offset + raw)
        # A non-terminating app must return control to the desktop.
        self.assertIn(
            b"budget", self.pe(f[:code] + b"\xeb\xfe" + f[code + 2 :], False).detail
        )

    def test_imp_validation(self):
        good = self.link((self.work / "hello32.exe").read_bytes())
        for pos, value, crc in [
            (0, 0, False),
            (4, 2, True),
            (5, 16, True),
            (6, 2, True),
            (8, 0, True),
            (12, 0xFFFFFFFF, True),
            (24, 0, True),
            (28, 0, False),
            (32, 8, True),
            (33, 1, True),
            (34, 65535, True),
        ]:
            with self.subTest(pos=pos):
                data = bytearray(good)
                struct.pack_into(
                    "<"
                    + ("I" if pos in (8, 12, 24, 28) else "H" if pos == 34 else "B"),
                    data,
                    pos,
                    value,
                )
                if crc:
                    data[28:32] = bytes(4)
                    struct.pack_into("<I", data, 28, zlib.crc32(data))
                r = Result()
                self.assertFalse(self.lib.imp_run(bytes(data), len(data), c.byref(r)))
                self.assertEqual(r.steps, 0)


if __name__ == "__main__":
    unittest.main()
