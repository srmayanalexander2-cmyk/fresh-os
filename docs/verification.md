# Stratus verification

## Preview 4: guided first run and sample-rich desktop

Guest captures: [first-run screen](screens/p4/welcome.jpg) · [desktop hub](screens/p4/desktop.jpg) · [All Apps grid](screens/p4/all-apps.jpg) · [starter files](screens/p4/files.jpg) · [sample picture](screens/p4/sample-picture.jpg).

* `qa/usability.py` clicks the first-run button and desktop Files tile with a real PS/2 mouse, opens Photos from the app grid, then smart-opens the seeded `example.mus` and `pattern.pbm` files into Media and Photos.
* A fresh Home initializes four readable examples (`welcome.txt`, `ideas.txt`, `example.mus`, `pattern.pbm`); existing Home snapshots are left untouched. The welcome screen, desktop hub and All Apps grid present mouse and keyboard paths, F1/F3, and save/session status before users have to discover them.
* `qa/vmprofile.py` selects Packages in All Apps and clicks Files from the hub via VMware absolute pointer; Settings persistence now checks the seeded Home record count as well.

## Preview 3: context menus, media, photos and file tools

Guest captures: [context menu](screens/p3/context-menu.jpg) · [Media](screens/p3/media.jpg) · [Photos](screens/p3/photos.jpg) · [Snake](screens/p3/snake.jpg) · [Files](screens/p3/files.jpg) · [Writer](screens/p3/writer.jpg).

* `qa/media.py` boots Lite, right-clicks the desktop (PS/2 right button) and clicks the Terminal menu item, creates `song.mus` and `pic.pbm` in Writer, exercises select-all/cut/undo/find, runs `wc`/`cp`/`mv`/`find`/`rm` in Terminal, smart-opens music into Media and the picture into Photos, starts Snake and steers it, right-clicks a Files row, and copies a Calculator result with Ctrl+C. All steps are checked against the guest serial log.
* `qa/core.c` (host) checks the music and Netpbm parsers: note names, rests, sharps, ranges, malformed input, P1/P2/P3 decoding, comments and size limits, plus all four built-in tunes.
* `qa/layout.py` now opens twelve apps (the nine previous plus Media, Photos and Snake) fullscreen and checks each layout against the framebuffer size.
* The Preview 3 context-menu/media checks below also pass on the Preview 4 kernels. Preview 4 was booted on BIOS and x64 UEFI for all four editions; Lite media/layout and VMware absolute-pointer profile tests passed.

## Native x64 development build

See [native processes: execution model and interoperability checks](native-processes.md). `qa/processes.py` boots OVMF, installs a third-party Windows EXE, exercises Windows memory and Linux `brk`/`mmap` lifecycles, verifies fault handling and watchdog expiry, and launches another package afterward. The shipped 32-bit Linux resource reader and 64-bit Linux hello are also tested on BIOS and UEFI by `qa/packages.py`. An optional BusyBox comparison uses the same static ELF on Linux and FreshOS. The browser live preview uses **BIOS**, not this CPU backend. Guest captures: [Windows calendar](screens/native/win64-cal.jpg) · [Linux BusyBox](screens/native/linux-busybox.jpg).

## IMP2 desktop and package checks

These are captures from the running native guest: [Welcome](screens/imp2/welcome.jpg) · [desktop](screens/imp2/desktop.jpg) · [All Apps](screens/imp2/all-apps.jpg) · [Win32 dialog from the packaged DLL example](screens/imp2/dll-message.jpg).

* `qa/imp_tool.py` extracts the **shipped packer** and checks linking with/without the local DLL, rejection of a Linux interpreter dependency, and no changes to a foreign disk or a duplicate install.
* `qa/packages.py` uses two guest boots per firmware. It refuses to launch an inbox package, installs it, executes real x86/x64 PE and static ELF programs, acknowledges the Windows dialogs, reads a newly supplied resource not on the ISO, rejects a corrupted package and reruns the installed DLL example after reboot.
* `qa/vmprofile.py` opens Packages from All Apps, installs and runs the DLL example, dismisses its dialog with the mouse and opens Files from the desktop hub. It also checks settings persistence and installation of the OS.
* `qa/layout.py` exercises twelve native sessions including Packages, Media, Photos and Snake. The v86 preview mounts a temporary Home image; the installed DLL sample was also installed, opened and acknowledged in the browser-hosted VM. Reloading that page starts a new session. The browser hosting v86 is **not** Chrome running inside Stratus.

See [IMP2](imp.md) for exact limits.

## Archived: Stratus preview 1

The checks below used the compiled kernels, not the old desktop or a host-side recreation of the UI. No installed operating system, public website or Google account was required. Disposable VM disks only.

| Check | Result |
|---|---|
| Four editions × two firmware paths | `qa/boot.py` booted each ISO on BIOS (32 MiB RAM) and x64 OVMF (96 MiB). Each entered Terminal, created a named Writer note, saved it, switched fullscreen on and off, rebooted, reopened the note and saved more text. A separate host reader found the expected data in the two on-disk generations. |
| App layouts | `qa/layout.py` on Normal and Lite opened Files, Writer, Calculator, Terminal, Settings, System, Sketch, Clock, IMP, Media, Photos and Snake. F11 gave each app the full native client area, measured against the actual framebuffer dimensions. A VMware absolute-pointer click maximized Files. F11 from maximized restored to maximized. Injecting an NMI produced the vector-2 stop report. |
| Installer | `qa/install.py VMware-Lite` on BIOS and UEFI checked that the blank destination was still marked before confirmation, then installed from the CD. The test compared the entire installed system region with the ISO bytes, removed the CD, booted from the IDE disk and saved a new document. |
| Recovery and foreign disks | `qa/recovery.py` damaged the data of the newest of two committed snapshots in a test disk. The next boot recovered the older document; editing it and saving committed a valid replacement. A second disk containing foreign bytes remained byte-for-byte unchanged after attempts to install and save. |
| First-run setup and taskbar | `qa/taskbar.py Lite` used VMware absolute-pointer input to enter setup, select top/bottom/left positions, verify maximized work-area bounds for left, bottom, and top, and check the saved Home setting. It also rewrote a disposable snapshot as the previous format, confirmed migration defaults to left, then verified the next save publishes the extended format. The same setup/migration and geometry test passed on `VMware-Lite --efi`. |
| VMware profile | `qa/vmprofile.py` extracted the published ZIP layout and used the VMDK descriptor **and** its flat extent. A QEMU VMware VGA/vmport guest negotiated absolute input and clicked Open Files. It changed both sliders, wallpaper, accent, auto-hide and new-window behavior, then saved preferences; the host checked the fields written to disk. The installed VMDK booted on its own. This is a protocol/emulation test, not VMware Workstation or Fusion certification. |
| Host unit tests | `qa/core.c` checks calculator precedence, parentheses, integer overflow, division by zero, incomplete input, Gregorian leap-year rules, month alignment, and the music/Netpbm parsers. |
| Live preview | v86 running in headless Chromium booted the Normal ISO to the native workspace. This uses a **host** browser to emulate a PC. It does not put a web browser in the guest. |

**Actual guest framebuffer captures:** [workspace](screens/workspace.jpg) · [Files](screens/files-fullscreen.jpg) · [Writer](screens/writer-fullscreen.jpg) · [Calculator](screens/calc-fullscreen.jpg) · [Settings](screens/settings-fullscreen.jpg) · [System](screens/system-fullscreen.jpg) · [Clock](screens/clock-fullscreen.jpg).

## Reproduction

Build with `python forge/media.py all`. Install `qemu-system-x86_64` and OVMF or set `QEMU_BIN`, `OVMF_CODE` and `OVMF_VARS` to their local paths. For example:

```sh
python qa/boot.py Normal
python qa/boot.py Normal --efi
python qa/layout.py Lite
python qa/media.py Lite
python qa/install.py VMware-Lite --efi
python qa/recovery.py
python qa/taskbar.py Lite
python qa/taskbar.py VMware-Lite --efi
python qa/vmprofile.py
cc -O2 -Inative/core native/core/math.c native/core/calendar.c native/core/mediafmt.c native/core/base.c qa/core.c -o /tmp/stratus-check && /tmp/stratus-check
```

For the optional emulator page, supply a local v86 npm package and the corresponding v86 firmware directory to `python preview/prepare.py PATH_TO_V86_PACKAGE PATH_TO_BIOS`. Serve `build/preview` as a static site. The guest boots without a persistent disk in this harness.

**Limits:** no Chrome, application ABI, process isolation, internet stack, arbitrary third-party installs, 1.x data migration or broad hardware support. Passing these checks does not mean the original feature wishlist has been completed.
