from pe_samples import make
from imp import pack, install, ROOT
import subprocess


def seed(d):
    d = d.resolve()
    make(d)
    for bits, src in [(32, "reader32"), (64, "hello")]:
        obj = d / f"linux{bits}.o"
        out = d / f"linux{bits}.elf"
        subprocess.run(
            ["as", f"--{bits}", ROOT / f"samples/linux/{src}.S", "-o", obj], check=True
        )
        subprocess.run(
            [
                "ld",
                "-m",
                "elf_i386" if bits == 32 else "elf_x86_64",
                "-static",
                "-o",
                out,
                obj,
            ],
            check=True,
        )
    (d / "greeting.txt").write_text("Read from an installed IMP resource.\n")
    for name, file, libs, res in [
        ("win32", "hello32.exe", [], []),
        ("win64", "hello64.exe", [], []),
        ("windll", "dllmain.exe", ["greeting.dll"], []),
        ("linux32", "linux32.elf", [], ["greeting.txt"]),
        ("linux64", "linux64.elf", [], ["greeting.txt"]),
    ]:
        data = pack(d / file, [d / i for i in libs], [d / i for i in res])
        install(d / "Home.img", name, data, staged=name == "windll")
        (d / f"{name}.imp").write_bytes(data)

    install(
        d / "Home.img",
        "cal64",
        pack(ROOT / "extras/cal/cal.exe", [], [], native64=True),
        staged=True,
    )
