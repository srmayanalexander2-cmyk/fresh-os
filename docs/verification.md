# Stratus preview 1: what was tested

The checks below used the compiled kernels, not the old desktop or a host-side recreation of the UI. No installed operating system, public website or Google account was required. Disposable VM disks only.

| Check | Result |
|---|---|
| Four editions × two firmware paths | `qa/boot.py` booted each ISO on BIOS (32 MiB RAM) and x64 OVMF (96 MiB). Each entered Terminal, created a named Writer note, saved it, switched fullscreen on and off, rebooted, reopened the note and saved more text. A separate host reader found the expected data in the two on-disk generations. |
| App layouts | `qa/layout.py` on Normal and Lite opened Files, Writer, Calculator, Terminal, Settings, System, Sketch and Clock. F11 gave each app the full native client area, measured against the actual framebuffer dimensions. A PS/2 mouse click maximized Files. F11 from maximized restored to maximized. Injecting an NMI produced the vector-2 stop report. |
| Installer | `qa/install.py VMware-Lite` on BIOS and UEFI checked that the blank destination was still marked before confirmation, then installed from the CD. The test compared the entire installed system region with the ISO bytes, removed the CD, booted from the IDE disk and saved a new document. |
| Recovery and foreign disks | `qa/recovery.py` damaged the data of the newest of two committed snapshots in a test disk. The next boot recovered the older document; editing it and saving committed a valid replacement. A second disk containing foreign bytes remained byte-for-byte unchanged after attempts to install and save. |
| VMware profile | `qa/vmprofile.py` extracted the published ZIP layout and used the VMDK descriptor **and** its flat extent. A QEMU VMware VGA/vmport guest negotiated absolute input and clicked Open Files. It changed both sliders, wallpaper, accent, auto-hide and new-window behavior, then saved preferences; the host checked the fields written to disk. The installed VMDK booted on its own. This is a protocol/emulation test, not VMware Workstation or Fusion certification. |
| Host unit tests | `qa/core.c` checks calculator precedence, parentheses, integer overflow, division by zero, incomplete input, Gregorian leap-year rules and month alignment. |
| Live preview | v86 running in headless Chromium booted the Normal ISO to the native workspace. This uses a **host** browser to emulate a PC. It does not put a web browser in the guest. |

**Actual guest framebuffer captures:** [workspace](screens/workspace.jpg) · [Files](screens/files-fullscreen.jpg) · [Writer](screens/writer-fullscreen.jpg) · [Calculator](screens/calc-fullscreen.jpg) · [Settings](screens/settings-fullscreen.jpg) · [System](screens/system-fullscreen.jpg) · [Clock](screens/clock-fullscreen.jpg).

## Reproduction

Build with `python forge/media.py all`. Install `qemu-system-x86_64` and OVMF or set `QEMU_BIN`, `OVMF_CODE` and `OVMF_VARS` to their local paths. For example:

```sh
python qa/boot.py Normal
python qa/boot.py Normal --efi
python qa/layout.py Lite
python qa/install.py VMware-Lite --efi
python qa/recovery.py
python qa/vmprofile.py
cc -O2 -Inative/core native/core/{math,calendar}.c qa/core.c -o /tmp/stratus-check && /tmp/stratus-check
```

For the optional emulator page, supply a local v86 npm package and the corresponding v86 firmware directory to `python preview/prepare.py PATH_TO_V86_PACKAGE PATH_TO_BIOS`. Serve `build/preview` as a static site. The guest boots without a persistent disk in this harness.

**Limits:** no Chrome, application ABI, process isolation, internet stack, arbitrary third-party installs, 1.x data migration or broad hardware support. Passing these checks does not mean the original feature wishlist has been completed.
