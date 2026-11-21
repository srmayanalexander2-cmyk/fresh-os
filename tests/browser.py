"""Real guest Ethernet -> slirp -> local HTTP server. Dynamic JS title proves execution."""

import argparse, random, sys, tempfile, threading, time
from http.server import BaseHTTPRequestHandler, HTTPServer
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from smoke import Machine


def verify(iso, uefi, shot, nic, errors=False):
    seed = random.randrange(10000, 90000)
    paths = []

    class Site(BaseHTTPRequestHandler):
        def do_GET(self):
            paths.append(self.path)
            page = (
                (
                    f"<title>before script</title><h1>Live network test</h1><p>Served by the HTTP fixture.</p><script>function twice(x){{return x*2;}} document.title='Verified '+twice({seed});document.write('Computed in FreshOS: '+[3,5,7].map(twice).join(', '));</script><a href='/next'>Follow this link</a>"
                ).encode()
                if self.path == "/"
                else b"<title>Second page</title><p>Link followed.</p>"
            )
            if self.path == "/large":
                page = b"a" * 25000
            self.send_response(404 if self.path == "/missing" else 200)
            self.send_header("Content-Type", "text/html")
            if self.path == "/gzip":
                self.send_header("Content-Encoding", "gzip")
            self.send_header("Content-Length", str(len(page) + (self.path == "/short")))
            self.end_headers()
            self.wfile.write(page)

    with HTTPServer(
        ("127.0.0.1", 0), Site
    ) as http, tempfile.TemporaryDirectory() as tmp:
        threading.Thread(target=http.serve_forever, daemon=True).start()
        d = Path(tmp)
        # Map a guest-only address to the loopback fixture (no internet needed).
        extra = ["-netdev", "user,id=lan", "-device", f"{nic},netdev=lan"]
        with Machine(d, iso, uefi=uefi, extra=extra) as m:
            m.wait("welcome ready:")
            m.key("ret")
            m.wait("desktop ready")
            m.key("f3")
            m.type("browser\n")
            m.wait("app: browser")

            def go(path):
                m.key("ctrl-l")
                url = f"http://10.0.2.2:{http.server_port}{path}"
                for c in url:
                    m.key({":": "shift-semicolon", "/": "slash", ".": "dot"}.get(c, c))
                m.key("ret")

            go("/")
            m.wait(f"Aero HTTP document ready: Verified {2*seed}")
            if shot:
                from PIL import Image

                time.sleep(0.3)
                pic = d / "aero.ppm"
                m.monitor(f"screendump {pic}")
                time.sleep(0.4)
                Image.open(pic).save(shot, quality=90)
            m.key("tab")
            m.key("ret")
            m.wait("Aero HTTP document ready: Second page")
            m.key("left")
            m.wait(f"Aero HTTP document ready: Verified {2*seed}", 2)
            go("/missing")
            m.wait("Aero: HTTP status 404")
            expected = ["/", "/next", "/", "/missing"]
            if errors:
                for path, marker in [
                    ("/gzip", "Compressed pages are not supported"),
                    ("/short", "Incomplete or oversized HTTP response"),
                    ("/large", "Page exceeds 24 KiB response limit"),
                ]:
                    go(path)
                    m.wait("Aero: " + marker)
                    expected.append(path)
                go("/next")
                m.wait("Aero HTTP document ready: Second page", 2)
                expected.append("/next")
            assert paths == expected, paths
        http.shutdown()
    print(
        "PASS real guest DHCP + TCP HTTP + JS-computed document title, relative link, Back and HTTP error",
        flush=True,
    )


if __name__ == "__main__":
    p = argparse.ArgumentParser()
    p.add_argument("iso", type=Path)
    p.add_argument("--nic", choices=["pcnet", "e1000"], default="pcnet")
    p.add_argument("--uefi", action="store_true")
    p.add_argument("--screenshot", type=Path)
    p.add_argument("--errors", action="store_true")
    a = p.parse_args()
    verify(a.iso.resolve(), a.uefi, a.screenshot, a.nic, a.errors)
