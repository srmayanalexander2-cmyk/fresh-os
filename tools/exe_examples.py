"""Build normal PE import tables with GNU ld. No Windows DLL is shipped."""

from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def make(build):
    for bits in (32, 64):
        work = build / f"pe{bits}"
        work.mkdir(parents=True, exist_ok=True)

        def run(*a):
            subprocess.run(a, cwd=work, check=True)

        mode = "i386pe" if bits == 32 else "i386pep"
        prefix = "_" if bits == 32 else ""
        stub = prefix + "stub"
        (work / "stub.S").write_text(f".text\n.globl {stub}\n{stub}: ret\n")
        run("as", f"--{bits}", "stub.S", "-o", "stub.o")
        libs = []
        for dll, exports in [
            (
                "KERNEL32",
                [
                    "ExitProcess",
                    "GetStdHandle",
                    "WriteFile",
                    "CreateFileA",
                    "CloseHandle",
                    "GetProductInfo",
                    "GetVersionExA",
                ],
            ),
            ("USER32", ["MessageBoxW"]),
        ]:
            (work / "exports.def").write_text(
                f"LIBRARY {dll}.dll\nEXPORTS\n"
                + "\n".join(" " + s + "=stub" for s in exports)
                + "\n"
            )
            run(
                "ld",
                "-m",
                mode,
                "--no-insert-timestamp",
                "--dll",
                "--out-implib",
                dll + ".a",
                "-e",
                stub,
                "-o",
                dll + ".dll",
                "exports.def",
                "stub.o",
            )
            libs.append(dll + ".a")
        run(
            "as",
            f"--{bits}",
            str(ROOT / f"examples/windows/hello{bits}.S"),
            "-o",
            "hello.o",
        )
        run(
            "ld",
            "-m",
            mode,
            "--no-insert-timestamp",
            "--subsystem",
            "console" if bits == 32 else "windows",
            "-s",
            "-e",
            prefix + "start",
            "-o",
            str(build / f"hello{bits}.exe"),
            "hello.o",
            *libs,
        )

        if bits == 32:
            run(
                "as",
                "--32",
                str(ROOT / "examples/windows/postcard32.S"),
                "-o",
                "postcard.o",
            )
            run(
                "ld",
                "-m",
                mode,
                "--no-insert-timestamp",
                "--subsystem",
                "console",
                "-s",
                "-e",
                "_start",
                "-o",
                str(build / "postcard.exe"),
                "postcard.o",
                *libs,
            )
