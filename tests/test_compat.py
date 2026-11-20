"""Black-box tests of the same freestanding runtimes linked into both kernels."""

import ctypes as c
import io, struct, subprocess, sys, tempfile, unittest, zipfile
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
                *[ROOT / "kernel" / n for n in ["pe.c", "apk.c", "android.c"]],
                "-o",
                so,
            ],
            check=True,
        )
        cls.lib = c.CDLL(str(so))
        cls.lib.pe_run.argtypes = [c.c_char_p, c.c_uint, c.POINTER(Result)]
        cls.lib.apk_activity.restype = c.c_char_p
        cls.lib.apk_activity.argtypes = [c.c_char_p, c.c_uint, c.POINTER(c.c_char_p)]
        with zipfile.ZipFile(ROOT / "examples/Postcard.apk") as z:
            cls.manifest = z.read("AndroidManifest.xml")
            cls.dex = z.read("classes.dex")

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

    def pe(self, data, ok=True):
        r = Result()
        self.assertEqual(
            bool(self.lib.pe_run(bytes(data), len(data), c.byref(r))), ok, r.detail
        )
        return r

    def apk(self, data, ok=True):
        err = c.c_char_p()
        s = self.lib.apk_activity(data, len(data), c.byref(err))
        self.assertEqual(s is not None, ok, err.value)
        return s if ok else err.value

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

    def package(self, method, manifest=None, dex=None):
        out = io.BytesIO()
        with zipfile.ZipFile(out, "w", method) as z:
            z.writestr(
                "AndroidManifest.xml", self.manifest if manifest is None else manifest
            )
            z.writestr("classes.dex", self.dex if dex is None else dex)
        return out.getvalue()

    def test_intact_apk(self):
        for method in [zipfile.ZIP_STORED, zipfile.ZIP_DEFLATED]:
            self.assertIn(b"Greetings from the clouds", self.apk(self.package(method)))

    def test_manifest_selects_class(self):
        m = self.manifest.replace(
            "os.fresh.Postcard".encode("utf-16le"),
            "os.fresh.Absentee".encode("utf-16le"),
        )
        self.assertNotEqual(m, self.manifest)
        self.assertIn(b"Launcher Activity", self.apk(self.package(0, m), False))

    def test_rejection(self):
        self.assertIn(b"directory", self.apk(self.package(0)[:-22], False))
        self.assertIn(
            b"checksum",
            self.apk(
                self.package(0).replace(b"Greetings from", b"Qreetings from"), False
            ),
        )
        self.assertIn(
            b"binary AndroidManifest", self.apk(self.package(8, b"<manifest/>"), False)
        )


if __name__ == "__main__":
    unittest.main()
