"""The same freestanding IPv4/TCP/DNS configuration is linked on both firmware paths."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
LW = ROOT / "third_party/lwip"
NET_SOURCES = [
    *sorted((LW / "core").rglob("*.c")),
    LW / "netif/ethernet.c",
    ROOT / "kernel/webnet.c",
    ROOT / "kernel/aerojs.c",
]
NET_FLAGS = ["-I" + str(ROOT / "kernel/netport"), "-I" + str(LW / "include")]
