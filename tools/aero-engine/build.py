"""Regenerate the checked-in RV32 JavaScript worker. ZIG=/path/to/zig (0.13.0)."""

import os, pathlib, subprocess, tempfile, struct
from lz4.block import compress

P = pathlib.Path(__file__).resolve().parent
ROOT = P.parents[1]
JS = ROOT / "third_party/mquickjs"
with tempfile.TemporaryDirectory() as tmp:
    d = pathlib.Path(tmp)

    def run(*args, **kw):
        subprocess.run(args, cwd=d, check=True, **kw)

    std = (JS / "mqjs_stdlib.c").read_text()
    # No load(), timers, filesystem or host shell bindings.
    std = "\n".join(
        line
        for line in std.splitlines()
        if not any(
            'JS_CFUNC_DEF("' + key + '"' in line
            for key in ("load", "gc", "setTimeout", "clearTimeout")
        )
    )
    (d / "stdlib.c").write_text(std)
    run(
        "cc",
        "-O2",
        "-I" + str(JS),
        d / "stdlib.c",
        JS / "mquickjs_build.c",
        "-lm",
        "-o",
        "gen",
    )
    for file, args in [("jslib.h", []), ("mquickjs_atom.h", ["-a"])]:
        with (d / file).open("w") as out:
            run("./gen", *args, "-m32", stdout=out)
    zig = os.getenv("ZIG", "zig")
    args = [
        zig,
        "cc",
        "-target",
        "riscv32-linux-musl",
        "-mcpu=generic_rv32+m",
        "-mabi=ilp32",
        "-Oz",
        "-mno-relax",
        "-fno-pic",
        "-ffreestanding",
        "-fno-builtin",
        "-fno-stack-protector",
        "-ffunction-sections",
        "-fdata-sections",
        "-DNDEBUG",
        "-DUSE_SOFTFLOAT",
        "-I" + str(d),
        "-I" + str(JS),
    ]
    objs = []
    for src in [
        P / "start.S",
        P / "worker.c",
        P / "minic.c",
        P / "setjmp.S",
        P / "longjmp.S",
        *[JS / (n + ".c") for n in ["mquickjs", "libm", "dtoa", "cutils"]],
    ]:
        obj = src.stem + ".o"
        objs.append(obj)
        run(*args, "-c", src, "-o", obj)
    run(
        *[v.replace("riscv32-linux-musl", "riscv32-freestanding") for v in args],
        "-static",
        "-nostdlib",
        "-Wl,--gc-sections",
        "-Wl,-T," + str(P / "link.ld"),
        *objs,
        "-o",
        "worker.elf"
    )
    run(zig, "objcopy", "-O", "binary", "worker.elf", "worker.bin")
    raw = (d / "worker.bin").read_bytes()
    dst = ROOT / "assets/aero-js.lz4"
    dst.write_bytes(
        struct.pack("<I", len(raw))
        + compress(raw, store_size=False, mode="high_compression")
    )
    print("worker:", len(raw), "packed:", dst.stat().st_size)
