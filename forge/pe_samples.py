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
                    "CreateFileW",
                    "ReadFile",
                    "GetFileSizeEx",
                    "SetFilePointerEx",
                    "FlushFileBuffers",
                    "CreateDirectoryW",
                    "GetCommandLineW",
                    "CloseHandle",
                    "VirtualAlloc",
                    "VirtualFree",
                    "VirtualProtect",
                    "VirtualQuery",
                    "GetProcessHeap",
                    "GetCurrentProcessId",
                    "GetLastError",
                    "CreateProcessW",
                    "GetExitCodeProcess",
                    "WaitForSingleObject",
                    "HeapAlloc",
                    "HeapSize",
                    "HeapReAlloc",
                    "HeapFree",
                    "GetProductInfo",
                    "GetVersionExA",
                ],
            ),
            ("USER32", ["MessageBoxW", "MessageBoxA"]),
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
            str(ROOT / f"samples/windows/hello{bits}.S"),
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

        if bits == 64:
            (work / "greeting.def").write_text(
                "LIBRARY greeting.dll\nEXPORTS\n greeting\n"
            )
            run("as", "--64", ROOT / "samples/windows/greeting.S", "-o", "greeting.o")
            run(
                "ld",
                "-m",
                mode,
                "--no-insert-timestamp",
                "--dll",
                "-e",
                "0",
                "--out-implib",
                "greeting.a",
                "-o",
                build / "greeting.dll",
                "greeting.def",
                "greeting.o",
            )
            run("as", "--64", ROOT / "samples/windows/dllmain64.S", "-o", "app.o")
            run(
                "ld",
                "-m",
                mode,
                "--no-insert-timestamp",
                "--subsystem",
                "console",
                "-s",
                "-e",
                "start",
                "-o",
                build / "dllmain.exe",
                "app.o",
                "greeting.a",
                *libs,
            )
