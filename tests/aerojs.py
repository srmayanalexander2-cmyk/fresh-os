"""Run the shipped engine, not a host JavaScript runtime."""

import ctypes as c, subprocess, tempfile
from pathlib import Path

R = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as d:
    so = Path(d) / "worker.so"
    subprocess.run(
        ["cc", "-O2", "-shared", "-fPIC", R / "kernel/aerojs.c", "-o", so], check=True
    )
    f = c.CDLL(str(so)).aero_eval
    f.restype = c.c_char_p
    rom = (R / "assets/aero-js.lz4").read_bytes()
    ram = c.create_string_buffer(512 * 1024)
    out = c.create_string_buffer(4096)

    def test(js):
        return f(ram, rom, len(rom), js, out, len(out)), out.value

    for js, val in [
        (b"6*7", b"42"),
        (
            b'function f(a){return a.map(function(x){return x*x;}).join(":");} f([2,5,8])',
            b"4:25:64",
        ),
        (b"Math.sqrt(81)+0.125", b"9.125"),
    ]:
        actual = test(js)
        assert actual == (None, val), actual
    assert test(b"while(true){}")[0] == b"Script stopped: instruction budget reached"
    assert test(b'throw new Error("test")')[0] == b"JavaScript exception"
    assert test(b" " * 16385)[0] == b"Script exceeds 16 KiB limit"
    assert test(b"typeof load + ':' + typeof setTimeout") == (
        None,
        b"undefined:undefined",
    )
    assert test(b"var hiddenValue = 13; hiddenValue") == (None, b"13")
    assert test(b"typeof hiddenValue") == (None, b"undefined")
    assert f(ram, b"bad", 3, b"2", out, len(out)) == b"Invalid engine image"
    assert test(b"12") == (None, b"12")
print(
    "PASS embedded MQuickJS arithmetic, arrays, functions, exceptions, budget and recovery"
)
