"""Controlled Ethernet peer: DHCP advertises DNS; verify A query and TCP destination.
The separate browser.py test serves HTTP over an actual TCP socket via slirp.
"""

import argparse, ipaddress, socket, struct, tempfile, time
from pathlib import Path
from browser import Machine


def ip(s):
    return ipaddress.IPv4Address(s).packed


server = ip("10.7.0.1")
client = ip("10.7.0.20")
resolved = ip("10.7.0.99")
mac = b"\x02\0\0\0\0\x01"


def checksum(b):
    v = sum(int.from_bytes(b[i : i + 2], "big") for i in range(0, len(b), 2))
    return ~(v + (v >> 16)) & 65535


def check(iso):
    name = b"\x04test\x07freshos\x00"
    query = False
    with socket.socket() as sock, tempfile.TemporaryDirectory() as tmp:
        sock.bind(("127.0.0.1", 0))
        sock.listen()
        sock.settimeout(10)
        with Machine(
            Path(tmp),
            iso,
            extra=[
                "-netdev",
                f"socket,id=lan,connect=127.0.0.1:{sock.getsockname()[1]}",
                "-device",
                "pcnet,netdev=lan",
            ],
        ) as m, sock.accept()[0] as link, link.makefile("rb") as src:
            link.settimeout(10)
            m.wait("welcome ready:")
            m.key("ret")
            m.wait("desktop ready")
            m.key("f3")
            m.type("browser\n")
            m.key("ctrl-l")
            for c in "http://test.freshos/":
                m.key({":": "shift-semicolon", "/": "slash", ".": "dot"}.get(c, c))
            m.key("ret")
            for _ in range(64):
                n = struct.unpack("!I", src.read(4))[0]
                assert n < 65536
                f = src.read(n)
                guest = f[6:12]
                eth = f[12:14]
                pkt = f[14:]

                def send(eth, p):
                    b = guest + mac + eth + p
                    link.sendall(struct.pack("!I", len(b)) + b)

                def udp(sp, dp, p, dest):
                    u = struct.pack("!HHHH", sp, dp, len(p) + 8, 0) + p
                    h = struct.pack(
                        "!BBHHHBBH4s4s",
                        0x45,
                        0,
                        len(u) + 20,
                        1,
                        0,
                        64,
                        17,
                        0,
                        server,
                        dest,
                    )
                    send(
                        b"\x08\0", h[:10] + struct.pack("!H", checksum(h)) + h[12:] + u
                    )

                if eth == b"\x08\x06" and pkt[6:8] == b"\0\x01":
                    send(eth, pkt[:6] + b"\0\x02" + mac + pkt[24:28] + guest + client)
                    continue
                if eth != b"\x08\0":
                    continue
                off = (pkt[0] & 15) * 4
                segment = pkt[off:]
                sp, dp = struct.unpack("!HH", segment[:4])
                p = segment[8:]
                if pkt[9] == 6:
                    assert (
                        query
                        and dp == 80
                        and pkt[16:20] == resolved
                        and segment[13] & 2
                    )
                    break
                if pkt[9] != 17:
                    continue
                if dp == 67:
                    kind = 2 if p[242] == 1 else 5
                    b = bytearray(240)
                    b[:3] = b"\x02\x01\x06"
                    b[4:8] = p[4:8]
                    b[16:20] = client
                    b[28:34] = guest
                    b[236:240] = p[236:240]
                    b.extend(
                        bytes([53, 1, kind, 54, 4])
                        + server
                        + bytes([1, 4, 255, 255, 255, 0, 3, 4])
                        + server
                        + bytes([6, 4])
                        + server
                        + b"\xff"
                    )
                    udp(67, 68, b, b"\xff" * 4)
                elif dp == 53:
                    assert pkt[16:20] == server and p[12:] == name + b"\0\x01\0\x01"
                    query = True
                    dns = (
                        p[:2]
                        + bytes.fromhex("81800001000100000000")
                        + p[12:]
                        + bytes.fromhex("c00c000100010000003c0004")
                        + resolved
                    )
                    udp(53, sp, dns, client)
            else:
                raise AssertionError("No resolved TCP connection")
    print(
        "PASS DHCP DNS option, on-wire A query, DNS answer and TCP SYN to returned address"
    )


if __name__ == "__main__":
    p = argparse.ArgumentParser()
    p.add_argument("iso", type=Path)
    check(p.parse_args().iso.resolve())
