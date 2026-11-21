# FreshOS 1.5 · Aero Browser

A native, experimental x86 desktop built for the supplied cloud-and-glass design. The loaders, kernel, framebuffer renderer, input drivers, shell, app sessions, installer, Fresh Home filesystem and IMP linker and bounded x86 interpreter are written here. **It is not a Linux image, a browser kiosk, or a Windows distribution.** The OS does not require the internet to start.

## 1 · Get the actual image

Extract **one** ZIP64 archive. Each contains a bootable `.iso`, exactly **8,000,000,000 bytes** (7.45 GiB). Allow at least 8 GB free on the extraction volume; FAT32 cannot hold the ISO as a file.

| Edition | Package | Difference |
|---|---|---|
| Normal | [FreshOS-Normal.zip](releases/FreshOS-Normal.zip) | Full-resolution native artwork and spring transitions. |
| Lite | [FreshOS-Lite.zip](releases/FreshOS-Lite.zip) | Smaller 960×540 sky photograph, animations initially off. Same applications. |
| VMware | [FreshOS-VMware.zip](releases/FreshOS-VMware.zip) | ISO plus a configured `.vmx` and growable `.vmdk` prepared for installation. Extract together and **open `FreshOS.vmx`** on an x86 VMware host. |
| VMware Lite | [FreshOS-VMware-Lite.zip](releases/FreshOS-VMware-Lite.zip) | Lite graphics, motion initially off, **32 MiB** BIOS VM profile. ISO, VMX and growable IDE target are supplied together. |

**What occupies the space:** the native BIOS payload is under 4 MB; the hybrid image also carries an 8 MiB FAT16 EFI system partition. At byte 16,777,216 begins the writable **Fresh Home** partition. Its initially empty space is why the downloads compress well. This is 8 GB of *disk capacity*, not 8 GB of implemented software. An optical drive is read-only; user data needs a writable, supported disk. The compact developer build omits Home and cannot be installed with the native installer.

**New in 1.5, in every edition:** Aero Browser fetches real HTTP pages over PCnet or Intel 82540EM Ethernet, renders text and links, and runs inline JavaScript in a bounded MicroQuickJS worker. Open `browser` and press Ctrl+L. Files now has a sorted Home directory; the shell adds file copy, comparison, search, hex display and page-text export. [Try it, see the guest screenshot and check the limits](docs/aero.md). This is not a Chrome-equivalent or HTTPS browser.

**In 1.4:** `exit`, Esc, and the Desktop button leave the command desktop. Files → Drives distinguishes detected hardware, protected foreign disks, a prepared installer target, and mounted Home. Open Settings → More settings for auto-hide, fill vs aspect-preserving scaling, and soft/silent feedback. [Details, limits and remaining requests](docs/usability.md).

**In 1.3:** four virtual desktops, six native utility sessions, screen capture, clipboard, on-screen keyboard, alarms, SHA3-256 and TAR/gzip creation. See [controls and limits](#3--use-the-desktop) and the [full Windows request checklist](docs/coverage.md).

**Hardware in 1.2:** [Hardware matrix, radio wiring, commands, Windows and Linux app guides](docs/drivers.md). Probe real hardware with **Setup → Drivers**. The recommended VMware profile includes native guest integration and an emulated PCnet NIC, not proprietary VMware Tools.

## 2 · Boot or install

* **Try first:** attach the ISO as a CD/DVD and boot it. Press **Enter** or click **Start session**. No installation, network, or account is required.
* **PC boot paths:** legacy BIOS loads a 32-bit protected-mode kernel via VBE. x64 UEFI loads the native 64-bit kernel from `EFI/BOOT/BOOTX64.EFI`, gets the GOP framebuffer, and exits firmware boot services. Unsigned; Secure Boot is not supported. BIOS uses a 32-bit RGB linear framebuffer; UEFI needs RGB/BGR GOP. Input uses i8042/PS/2 or compatible legacy emulation. USB-only input, ARM, native NVMe and arbitrary GPUs are not supported. Prefer a VM to test. Physical PCs and VMware Workstation have not been hardware-certified or tested here.
* **Removable media:** the ISO is also a raw disk image with an MBR and EFI system partition. Use an image writer in **raw/DD mode** on a **spare USB drive of sufficient capacity**. This overwrites the USB's contents. Do not target your system disk. Boot success depends on the hardware conditions above. No USB mass-storage driver after boot; a USB-booted session normally stays in RAM.
* **Persistent installation:** use the prepared VMware disk, or create a **new** raw disk with `python3 tools/new-target.py fresh-target.img`. Attach it as an IDE disk, supported virtio-blk disk or AHCI SATA disk, and the ISO as an IDE optical drive. The included VMX already does this. On the welcome screen press **I**, read the destination status, then press **I** again to install. Esc backs out. The installer only accepts an expressly marked target, verifies copied sectors, initializes Fresh Home, and publishes the MBR last. You can use apps during the copy. On success, shut down the VM, disconnect its CD and boot the disk. Keep the installed VM outside the build output folder.
* **Not a Windows migration tool:** the installer refuses ordinary disks. There is no automatic NTFS import, repartition of an existing PC, or Windows deletion path.

QEMU example (regular Linux host, no KVM required):

```sh
python3 tools/new-target.py fresh-target.img
qemu-system-x86_64 -m 96 -drive file=fresh-target.img,format=raw,if=ide,index=0 \
  -drive file=out/FreshOS-Normal.iso,media=cdrom,if=ide,index=2 -boot order=d
```

## 3 · Use the desktop

Click the rail icons for Files, Writer, Settings, Clock, tray, Connections, volume and brightness. Drag the level handles, or use F6/F7 and the arrow keys. Only the sky photograph is used as wallpaper. Both welcome buttons, the glass card and the dock are drawn by the kernel. The desktop, clock, bubbles, launcher, windows, controls and applications are rendered by the kernel. The rail opens **Files**, not a pretend Chrome browser. The desktop starts in GUI mode. Settings offers GUI/CLI, wallpaper, shell text colors, motion, software brightness and audio feedback. Click a value or use keys **1–6** in Settings. Paint can turn your own drawing into the wallpaper; the builder also accepts a photograph.

| Control | Action |
|---|---|
| Enter | Enter live session; select focused launcher or tray item |
| F1 / Apps on the rail | All Apps in GUI; text Start in CLI. Arrows / Tab to navigate. Click More to cycle pages. |
| Windows key / F2 | Minimized apps; Up/Down and Enter restore. F2 works when the host intercepts Win. |
| Win+Tab / `desktops` | Select one of four desktops. Ctrl+Win+Left/Right switches directly. Apps are singletons: launching one elsewhere moves that session. |
| F8 / Win+A | Actions: arrows scroll; Shift+arrows reorder; Enter invokes. Energy saver adjusts brightness and motion only. |
| F9 / F10 | Toggle the 2× lens / drag a rectangle to save `snip.bmp` to writable Home. Esc cancels capture. |
| F11 / `osk` | Toggle a clickable keyboard in Writer, Stickies, To Do or the command line. |
| F3 | Switch between the graphical desktop and command-line desktop |
| F4 / Esc / window `_` | Minimize without losing state. `x` closes the session. Drag the title bar to move a window. |
| F6 / F7 | Sound-feedback level / screen dimmer; Left/Right adjusts, Esc dismisses |

**Command-line desktop:** the pointer remains available. Click **Desktop / Esc**, press Esc at the prompt, or type `exit` to return. `start` lists runnable apps as text and leaves the prompt active. Type an app name, or `open NAME`. Prefixes show matching apps; Tab completes a name, Up recalls the last command. Try `start`, `writer`, F4, `tasks`, `resume writer`, `wall lagoon`, `color ice`, `time`, `gui` or `help`.

**Apps:** Files (select/open, Library/Recent, EXE conversion), Writer (plain-text editor), Paint, Calculator (integer arithmetic), hardware Clock, Settings, Terminal, playable Minefield, System information, month Calendar, Tasks, Programs (IMP/EXE output), Connections, Stickies, To Do, Glyphs, Tips, Events, Actions, Photos (local BMP) and Software (local IMP packages). These are built-in sessions in one address space, not isolated preemptive user processes.

**Try the utilities:** In Stickies, type a note, press Ctrl+C, then Ctrl+S. In To Do, paste with Ctrl+V and press Enter to add it; Tab checks the selected item and Ctrl+S saves the list. Three cards and twelve checklist items fit. Glyphs copies printable ASCII. Writer copies its whole document; Terminal uses Ctrl+Shift+C for its input line. Clipboard and the last 32 shell events are session-only.

In Terminal, `alarm 09:30` sets a one-shot local-clock alert and `timer 60` sets a one-minute countdown. Open Clock to see their status; `alarm off` and `timer off` cancel. `sha3 sticky1.txt` prints a SHA3-256 digest. `tar notes.tar sticky1.txt todo.list` archives both saved files, and `gzip list.gz todo.list` wraps one file in gzip (stored blocks; no size reduction). Archive output is limited to 256 KiB and needs writable Home. No extraction or .7z support.

## 4 · Keep files and preferences

Without a writable **supported Fresh Home disk**, everything is temporary. On a mounted Home disk, use **Save**, **Save as**, or Ctrl+S in Writer. Click to place the caret, or navigate with the arrow keys, Home and End. Ctrl+Shift+S opens Save as. Select Save profile in Settings to persist settings. `note.txt` is restored on boot. `save draft` / `load draft` store and recall named notes, `savecanvas` persists the canvas, `prefs` persists interface settings, `ls` lists recent versions, and `storage` reports the mount status. Documents append to a checksum-protected log; two alternating superblocks commit the latest head. Old versions consume space; there is not yet a garbage collector. Writer is limited to 1,399 bytes.

The offline host tool exchanges files with a **detached raw image**, never a mounted VM disk or live device:

```sh
python3 tools/home.py fresh-target.img put draft my-note.txt
python3 tools/home.py fresh-target.img get draft exported.txt
python3 tools/home.py fresh-target.img ls
```

## 5 · Link and run IMP programs

Opening an EXE automatically links it into an IMP in memory and launches it without writing to disk. To keep the linked image, open **Files**, select `hello64.exe`, click **Convert to IMP**, then click **Open** on the selected result in Recent. Or press F3, type `convert hello64.exe` and then `open hello64.imp`. With a writable Home, the generated file survives a restart. On read-only media it stays in RAM. Use Tab, the arrows, C and Enter to do the same in Files without a mouse. The ISO also includes a 32-bit EXE and both prelinked examples.

[IMP1 specification and host converter](docs/imp.md) documents the executable layout, ABI, validation and known limits. Linking is **not** a way around missing Windows APIs. Android support has been removed.

## 6 · Build, verify and package

On x86-64 Linux (or x86-64 Ubuntu/Debian in WSL), install Python 3.11+, GCC, GNU binutils with `elf_i386` and `i386pep` support, and DejaVu Sans fonts. No 32-bit libc or prebuilt OS is linked. Host dependencies are image/font/ISO authoring tools, not OS payloads.

```sh
sudo apt install build-essential binutils python3-venv fonts-dejavu-core
python3 -m venv .venv
.venv/bin/pip install -r requirements.txt
.venv/bin/python tools/build.py all
# or: tools/build.py Normal --wallpaper /path/to/photo.jpg
.venv/bin/python tools/package.py Normal
```

**Windows Command Prompt:** after installing those WSL prerequisites, run `BUILD-ISO.cmd Normal`, `BUILD-ISO.cmd Lite`, `BUILD-ISO.cmd VMware`, `BUILD-ISO.cmd VMware-Lite`, or `BUILD-ISO.cmd all`. The source build writes the actual ISOs to `out\`. No build tools are needed to use the prebuilt releases.

**Actions:** run the **Build FreshOS media** workflow manually. It builds all four editions, runs the native boot/input smoke tests under QEMU with BIOS and OVMF, then uploads ZIP64 downloads. The Normal job also installs to a fresh disk, checks saved data and boots again without the ISO. See `tools/smoke.py` to run the same checks locally; use `--install` and/or `--uefi`. `QEMU_BIN`, `OVMF_CODE`, `OVMF_VARS` can override their locations.

## 7 · Scope and provenance

[Feature-by-feature status](docs/coverage.md) tracks the Windows wishlist, including removed items. [Source requirements](docs/requested-features.txt) is the original list. The earlier transcript describes a different, unavailable implementation: this checkout started with an eleven-byte README. Its test counts and release claims are **not** inherited.

This is an experimental native OS, not a production Windows replacement. There is no arbitrary EXE/IMP or Linux desktop compatibility layer, TLS-enabled modern web platform, integrated laptop Wi-Fi driver, general USB drivers, per-process protection, antivirus, or enterprise management stack. Do not use it for irreplaceable files. No Windows source or system binaries are included. The supplied cloud artwork is used as visual source material; glyphs are rasterized from DejaVu Sans (see `docs/licenses/DejaVu.txt`).
