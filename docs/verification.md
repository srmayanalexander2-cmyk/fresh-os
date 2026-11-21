# FreshOS 1.5 verification

| Test on release media | Evidence |
|---|---|
| All four editions | Normal, Lite, VMware and VMware-Lite passed the full-media structural check. Each booted under QEMU BIOS and x64 UEFI, passed the existing interactive smoke test on both, and passed `tests/browser.py` on both. These are tests of the ISO kernels, not a host-browser mock. |
| Two Ethernet drivers | BIOS/PCnet and UEFI/82540EM guests acquired DHCP, connected to a host-side HTTP server, received HTML, computed a new title from a per-run random integer, followed a relative link, went Back, and reported a 404. The server asserted the actual request paths. Additional Normal UEFI checks rejected compressed, incomplete and oversized responses and then loaded another good page. [Framebuffer capture](screens/aero.jpg). |
| DNS | A controlled Ethernet peer supplied its resolver in a DHCP reply. `tests/browser_dns.py` observed the guest's on-wire A query for `test.freshos`, returned a test address, and observed a TCP SYN to that address on port 80. No public resolver or internet connection required. |
| Local files | A disk contained a local HTML file older than the ten-entry Recent view. A guest opened it from the sorted Home directory, evaluated its script, exported the resulting text, copied the file, and refused to overwrite the copy. Independent disk inspection verified `Local answer = 42` and the copy record. |
| Worker | `tests/aerojs.py` ran the checked-in engine via the same interpreter used by the kernel. Function calls, arrays, numeric conversion, exception handling, input and instruction limits, missing host APIs and fresh globals passed. Rebuilding the worker with Zig 0.13.0 produced identical packed bytes. |
| Regressions | Compatibility, file tools, shell file commands, desktop, utilities, usability, VMware pointer and alternate-position VMDK checks passed. VMware-Lite passed BIOS virtio and UEFI SATA device/installation tests and the UEFI IDE install/persistence smoke. The Normal payload booted in the v86 preview. Not certification of physical hardware or a run in VMware Workstation/Fusion. |

See [Aero controls, limits and test commands](aero.md). These tests establish a small in-guest HTTP/text/JavaScript browser, **not** Chrome or modern web compatibility.

## FreshOS 1.4 verification

| Final-media check | Result and boundary |
|---|---|
| Four editions, two firmware paths | `tools/check.py` and eight QEMU BIOS / UEFI smoke boots passed, including CLI, tray restore, EXE→IMP and ELF execution. VMware Lite also installed to a marked IDE disk, saved data and booted disk-only under both firmware paths. |
| CLI, pointer and windows | `tests/usability.py` exited via command, Escape and a mouse click. It maximized/restored Writer and compared the Home file after a right-click copy and paste. `tests/vmguest.py` exercised the VMware absolute-pointer protocol in QEMU. Not a VMware Workstation hardware certification. |
| Disk discovery | A guest identified the pre-existing foreign disk at IDE 0:0, Home at 0:1, and the optical device separately. The foreign disk bytes remained unchanged. `tests/vmstorage.py` installed from an IDE 0:1 CD to a disposable copy of the supplied VMDK at 1:1 and booted the result without optical media. |
| Packages, photos and diagnostics | A guest installed an IMP, launched it from Software, rebooted, ran the installed bytes again, and removed the package via the confirmation button. The latest Home catalog was empty afterward. Photos displayed expected decoded BMP pixels. Injecting an NMI into running 32-bit BIOS and 64-bit EFI guests produced vector 2, name, IP and CPU code on the serial log; the BIOS screenshot shows the stop screen. Not a claim that a crashed shell can still use a clipboard. |
| Regressions | `tests/test_compat.py` (five groups), `tests/filetools.py`, `tests/desktop.py`, `tests/utilities.py`, BIOS virtio and UEFI SATA `tests/devices.py` passed. The Normal optical payload booted in v86/Chromium. The browser here is the **host emulator UI**, not an in-guest web browser. |

Guest framebuffer captures: [Drives](screens/drives.jpg) · [smiley volume slider](screens/smile-slider.jpg) · [BMP viewer](screens/photos.jpg) · [CPU stop report](screens/panic.jpg). See [the update guide](usability.md) for limits and outstanding requests.

## FreshOS 1.3 verification · 2026-11-20

* **Native media:** All four editions passed `tools/check.py` and booted in QEMU under legacy BIOS and x64 UEFI. VMware Lite passed `--install` under both, including disk-only reboot and persisted document checks. The Normal optical payload also booted in the v86 browser preview.
* **New utility checks:** `QEMU_BIN=… python tests/utilities.py out/FreshOS-VMware-Lite.iso` drives the guest with keyboard and pointer input and reads the resulting Home disk independently. The test switched desktops with a note open, pasted into a checklist on the other desktop, returned to the note, clicked an on-screen key and rebooted to verify both documents could be edited again. It checked a magnifier region against the source pixels, reordered a quick action and invoked it, opened Writer from Tips, received a countdown alert, decoded a 26×32 BMP, opened a two-member tar and decompressed a gzip file. [Task View](screens/task-view.jpg) · [Actions](screens/actions.jpg) · [Magnifier](screens/magnifier.jpg)
* **Algorithm checks:** `python tests/filetools.py` compiled the guest's actual file-tool code on the host and compared its results with `hashlib`, `tarfile` and `gzip`, including empty files and SHA3 rate, tar-sector and DEFLATE block boundaries.
* **Regression checks:** Five host compatibility groups, the mouse-driven desktop and VMware pointer tests, and the BIOS virtio / UEFI SATA device fixtures passed. These are emulator/protocol tests, not physical Wi-Fi, Bluetooth, VMware Workstation or Windows compatibility certification.
* **Downloads:** The four ZIP64 archives and expanded-ISO hashes for this section are at tag `v1.3.0`.

## FreshOS 1.2 verification · 2026-11-20

These results apply to the four **1.2** ISO archives at tag `v1.2.0`, not a web implementation of the desktop. Older results remain in their release tags. The VM and protocol fixtures below do not establish hardware compatibility with an arbitrary PC.

| Check on final media | What actually happened |
|---|---|
| All four editions, BIOS and UEFI | Eight QEMU boots; each entered the desktop, launched/minimized/restored Files, switched to CLI, automatically linked and ran EXEs, converted and launched IMP32/64, acknowledged the blocking Win32 dialog, ran a static Linux ELF and opened Calculator. |
| IDE install | VMware Lite, both firmware paths: installer verified sectors on a stamped disk; independent host checks compared the saved document/profile and imported program bytes; disk-only reboot ran a saved non-built-in IMP. |
| More disk controllers | On VMware Lite, installed and wrote documents to a **virtio-blk target under BIOS** and an **AHCI SATA target under UEFI**. Removed the optical media, booted the same controller/disk, reopened `postcard.imp` and declined its save request. No test wrote to a host device. |
| Win32/IMP Home access | `postcard.exe` queried Windows build 22631 and Professional product code 48. Denying the native write prompt left no record. After linking, the IMP asked again, and approving it created the expected exact document bytes. |
| Network and sound | A QEMU PCnet adapter obtained a DHCP lease from user-mode networking and resolved the gateway via ARP. Played the bundled mono 24 kHz `chime.wav` through the AC’97 model; QEMU's output file contained stereo PCM samples, not silence. Audio input is not implemented or tested. |
| Wi-Fi and Bluetooth | On BIOS and EFI, independent serial protocol fixtures received the actual ESP-AT join and HCI reset, read-address and inquiry packets. They supplied controller responses; the guest processed them. **No over-the-air join, physical radio, pairing, or laptop wireless chipset was tested.** |
| Mouse, display and interaction | QEMU vmport + VMware SVGA model negotiated vmmouse absolute mode; QMP injected absolute coordinates which activated Welcome and Files. Separate PS/2 tests used the slider, opened Connections, linked an EXE, opened the IMP, edited Writer text and checked named Save as bytes. The standard VGA configuration also reported native DISPI double buffering. No frame-rate benchmark or VMware Workstation run. |
| Host compatibility | Five regression groups in `tests/test_compat.py`, including compiled C, PE32/64 vs linked IMP equivalence, interpreter budget, invalid IMP fields, ELF output compared with real Linux and malformed ELF header/segment/entry checks. |
| Media and preview | Each expanded ISO is 8,000,000,000 bytes. `tools/check.py` verified BIOS/EFI structures and the EXE/IMP/ELF/WAV assets in both kernels. Headless Chromium booted the current Normal optical payload in v86 and opened the launcher. |

## Actual framebuffer captures

[Welcome](screens/welcome.jpg) · [Volume slider](screens/volume.jpg) · [Connections](screens/connections.jpg) · [Files](screens/files.jpg) · [Program output](screens/program.jpg) · [Writer](screens/writer.jpg)

## Reproduce

```sh
python tests/test_compat.py -v
python tools/build.py VMware-Lite
ISO=out/FreshOS-VMware-Lite.iso
python tools/check.py "$ISO"
python tests/desktop.py "$ISO" --screenshots /tmp/fresh-screens
python tests/vmguest.py "$ISO"
python tests/devices.py "$ISO"
python tests/devices.py "$ISO" --sata --uefi
python tools/smoke.py "$ISO" --install
python tools/smoke.py "$ISO" --install --uefi
python tools/package.py VMware-Lite
```

`QEMU_BIN`, `OVMF_CODE` and `OVMF_VARS` select the emulator and firmware. Device tests use temporary disk files, local sockets and emulated peripherals. [Driver requirements, exact app ABI and limitations](drivers.md).
