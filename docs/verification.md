# FreshOS 1.1 verification · 2026-11-20

These results refer to the four **1.1** ISO archives in `releases/`. Older release evidence is available in the `v1.0.0` tag. No test below ran inside VMware Workstation or on bare metal.

| Check on final media | Result |
|---|---|
| Normal, Lite, VMware, VMware Lite | Each booted in QEMU with 32 MiB BIOS and 96 MiB x64 OVMF. Each opened Files, minimized/restored it, switched desktops, automatically linked and ran PE32/64 EXEs, explicitly generated and launched both IMPs, and launched Calculator. |
| VMware Lite install + reboot | On **both firmware paths**, installed to a marked raw IDE target, saved a Writer document and profile, and compared Home contents outside the guest. Imported an external EXE and IMP. Replaced `guest.imp` (64-bit) by converting `guest.exe` (32-bit) on the installed OS, then rebooted without the CD and executed `guest.imp` as a 32-bit IMP. That name is not a built-in. |
| Actual ZIP's VMware Lite target | Matched VMX ISO name and 32 MiB memory setting. Extracted a copy of the included VMDK, used it directly as QEMU's IDE target, installed, booted that VMDK without the CD, converted and ran an IMP. This tests the disk, **not** the VMware hypervisor. |
| Mouse and editor | `tests/desktop.py` sent relative PS/2 motion and button packets: clicked through Welcome → Files → select EXE → Convert → Open → Writer; edited at the caret, used Save as, and independently checked the document and linked file on the disk. |
| Host linker/VM | `python tests/test_compat.py -v`: PE and linked IMP results match for x86 and x64 assembler samples, compiler-generated arithmetic, and compiler-generated CRT calls. Tested unsupported imports, execution budget, corrupted checksums, headers, lengths, page flags, record size and non-executable entry. |
| Image and preview | `tools/check.py` verified hybrid partitions, BIOS and EFI boot structures and all four embedded program payloads in both kernels. Browser preview booted the Normal optical payload in Chromium/v86 and opened the launcher. |

## Framebuffer captures

These are QEMU screenshots of VMware Lite at runtime, not design mockups or assets used by the OS. Reproduce with `QEMU_BIN=qemu-system-x86_64 python tests/desktop.py out/FreshOS-VMware-Lite.iso --screenshots /tmp/fresh-screens`.

[Welcome](screens/welcome.jpg) · [Files: EXE selected](screens/files.jpg) · [Output from linked IMP](screens/program.jpg) · [Writer](screens/writer.jpg)

## Reproduce the install test

```sh
python tools/build.py VMware-Lite
python tools/check.py out/FreshOS-VMware-Lite.iso
python tools/smoke.py out/FreshOS-VMware-Lite.iso --install
python tools/smoke.py out/FreshOS-VMware-Lite.iso --uefi --install
python tools/package.py VMware-Lite
```

`QEMU_BIN`, `OVMF_CODE` and `OVMF_VARS` select locally installed test tools. Tests operate on temporary disks. The `.sha256` beside each archive is for the **expanded ISO**, not the ZIP. All releases reserve an 8 GB Home-capable disk image; that is not the size of the kernel.
