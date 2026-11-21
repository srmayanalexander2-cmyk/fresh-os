# Aero Browser and the 1.5 network update

Aero is an application **inside** the FreshOS kernel. Open **Aero Browser** in All Apps or type `browser`. Its offline start page links to a guide and a JavaScript demo. The Normal, Lite, VMware and VMware-Lite media have the same engine and drivers, including on the 32 MiB BIOS configuration. No host browser, guest agent, remote renderer or web service is needed.

## Try a real page

On a QEMU host, put an `index.html` such as the example below in an otherwise empty test directory. Run `python3 -m http.server 8000 --bind 127.0.0.1 --directory YOUR_TEST_DIRECTORY` on the **host**. Start QEMU with `-netdev user,id=lan -device pcnet,netdev=lan` (or `-device e1000,netdev=lan`). In the guest press **Ctrl+L**, type `http://10.0.2.2:8000/`, then Enter. This host alias is for QEMU user networking; in VMware use an address reachable from that VM. Do not expose private files or send credentials over HTTP.

```html
<title>Test page</title>
<h1>Hello from the server</h1>
<p>The following line is computed in the guest.</p>
<script>
function double(n) { return n*2; }
document.write([3, 5, 7].map(double).join(', '));
document.title = 'Verified ' + double(21);
</script>
```

Expected: a guest window titled **Aero Browser**, a document heading **Verified 42**, and the line `6, 10, 14`. [Guest framebuffer capture from the HTTP test](screens/aero.jpg).

* **Network:** DHCPv4 obtains the guest address, gateway, subnet mask and DNS resolver. A single lwIP interface provides Ethernet/ARP, IPv4, UDP/DNS and TCP. Aero sends HTTP/1.0 GET requests. It does not silently change `https://` to `http://`. Use a trusted isolated test network. The new Intel driver is specifically for the 82540EM `8086:100e`; an Intel vendor ID does not imply support for every Intel NIC.
* **Page controls:** click a link, or Tab then Enter. Arrow Up/Down or PgUp/PgDn scroll. The arrow buttons traverse eight history slots, Ctrl+R reloads, and Star / Ctrl+D saves one favorite to writable Home (session-only without a disk). Open it from the start page. The address field accepts copy and paste. Maximize to reflow the text.
* **Local pages:** `browse home:page.html`, or open `.html`/`.htm` from Files. `browse home:readme.txt` opens plain text. An HTTP page's links cannot open a local Home file. Save the displayed text with `savepage NEW.txt` in the shell; it refuses to overwrite an existing file.
* **JavaScript:** `js 6*7` prints `42`; `jsfile FILE.js` evaluates a script from Home. MicroQuickJS supports functions, arrays, objects, closures and exceptions in a strict ES5-like subset. Inline page scripts share one evaluation; `document.write` appends **text** after the parsed body, and assignments to `document.title` and `document.body.textContent` update the display. This small bridge is **not** a DOM. Each evaluation starts a fresh private RV32 worker with a fixed heap, checked memory accesses, a six-million-instruction budget and no file, network or native-call interface. This is not general OS process isolation.
* **Limits:** 24 KiB HTTP response including headers, 16 KiB combined script including bridge, 8 KiB rendered text, 24 links, one outstanding request. No CSS engine, images on web pages, Unicode layout, external scripts, full modern JS, TLS/certificate validation, redirects, cookies, forms, streaming, media, Web APIs or Chrome extensions. Unsupported compression and transfer encodings produce errors. Plain fixed-length or close-delimited responses are supported.

## Fresh Home and shell changes

**Files → Home files** lists distinct stored names in alphabetical order, independent of Recent. It shows up to 128 names from the newest 2,048 save records. `ls`/`dir` uses the same directory. This is a flat store, not a POSIX filesystem. With a writable Home, try:

```
savepage article.txt
copy article.txt second.txt
cmp article.txt second.txt
grep FreshOS second.txt
xxd second.txt
```

`cp`/`copy` creates a new file; `cmp`/`fc /b` reports equality or the first differing byte (256 KiB comparison buffer); `grep`/`findstr` does a literal, case-sensitive search for a single word, up to 64 matching lines and 100 display bytes per line; `xxd`/`hexdump` displays the first 256 bytes. These spellings do not implement every switch of the Windows or Unix tools. See `help files` and `help web` in the guest.

## Rebuild and test

Ordinary media builds need no cross-toolchain download: `assets/aero-js.lz4` is the packed worker. To regenerate, install Zig 0.13.0 and Python `lz4`, then `ZIG=/path/to/zig python tools/aero-engine/build.py`. The generator builds 32-bit tables using a host C compiler, cross-links the worker and packs a length-prefixed LZ4 block. Sources: [MicroQuickJS](https://github.com/bellard/mquickjs/tree/4119f6819ccb5bae2d21a18a106e89522d18ad45), [lwIP 2.2.1](https://github.com/lwip-tcpip/lwip/tree/STABLE-2_2_1_RELEASE). Upstream sources retain their notices under `third_party/`; worker support and musl/Zig notices are in `tools/aero-engine/`.

```
python tests/aerojs.py
python tools/build.py Normal
python tests/browser.py out/FreshOS-Normal.iso
python tests/browser.py out/FreshOS-Normal.iso --uefi --nic e1000
python tests/browser_dns.py out/FreshOS-Normal.iso
python tests/browser_home.py out/FreshOS-Normal.iso
```
