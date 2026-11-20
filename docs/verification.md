# FreshOS 1.2 verification · 2026-11-20

These results apply to the four **1.2** ISO archives in `releases/`, not a web implementation of the desktop. Older results remain in their release tags. The VM and protocol fixtures below do not establish hardware compatibility with an arbitrary PC.

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
