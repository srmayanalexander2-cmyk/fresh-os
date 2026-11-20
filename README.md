# FreshOS Release 1 · Sky live edition

A native, experimental x86 desktop built for the supplied cloud-and-glass design. The loaders, kernel, framebuffer renderer, input drivers, shell, app sessions, installer, Fresh Home filesystem and small DEX interpreter are written here. **It is not a Linux image, a browser kiosk, or a Windows distribution.** The OS does not require the internet to start.

## 1 · Get the actual image

Extract **one** ZIP64 archive. Each contains a bootable `.iso`, exactly **8,000,000,000 bytes** (7.45 GiB). Allow at least 8 GB free on the extraction volume; FAT32 cannot hold the ISO as a file.

| Edition | Package | Difference |
|---|---|---|
| Normal | [FreshOS-Normal.zip](https://github.com/srmayanalexander2-cmyk/fresh-os/releases/download/v1.0.0/FreshOS-Normal.zip) | Full-resolution native artwork and spring transitions. |
| Lite | [FreshOS-Lite.zip](https://github.com/srmayanalexander2-cmyk/fresh-os/releases/download/v1.0.0/FreshOS-Lite.zip) | Smaller 640×360 wallpaper and welcome assets, animations initially off. Same applications. |
| VMware | [FreshOS-VMware.zip](https://github.com/srmayanalexander2-cmyk/fresh-os/releases/download/v1.0.0/FreshOS-VMware.zip) | ISO plus a configured `.vmx` and growable `.vmdk` prepared for installation. Extract together and **open `FreshOS.vmx`** on an x86 VMware host. |

**What occupies the space:** the native BIOS payload is about 2.5 MB (less for Lite); the hybrid image also carries an 8 MiB FAT16 EFI system partition. At byte 16,777,216 begins the writable **Fresh Home** partition. Its initially empty space is why the downloads compress well. This is 8 GB of *disk capacity*, not 8 GB of implemented software. An optical drive is read-only; user data needs a writable, supported disk. The compact developer build omits Home and cannot be installed with the native installer.

## 2 · Boot or install

* **Try first:** attach the ISO as a CD/DVD and boot it. Press **Enter** or click **Use OS**. No installation, network, or account is required.
* **PC boot paths:** legacy BIOS loads a 32-bit protected-mode kernel via VBE. x64 UEFI loads the native 64-bit kernel from `EFI/BOOT/BOOTX64.EFI`, gets the GOP framebuffer, and exits firmware boot services. Unsigned; Secure Boot is not supported. BIOS uses a 32-bit RGB linear framebuffer; UEFI needs RGB/BGR GOP. Input uses i8042/PS/2 or compatible legacy emulation. USB-only input, ARM, native NVMe/AHCI and arbitrary GPUs are not supported. Prefer a VM to test. Physical PCs and VMware Workstation have not been hardware-certified or tested here.
* **Removable media:** the ISO is also a raw disk image with an MBR and EFI system partition. Use an image writer in **raw/DD mode** on a **spare USB drive of sufficient capacity**. This overwrites the USB's contents. Do not target your system disk. Boot success depends on the hardware conditions above. No USB mass-storage driver after boot; a USB-booted session normally stays in RAM.
* **Persistent installation:** use the prepared VMware disk, or create a **new** raw disk with `python3 tools/new-target.py fresh-target.img`. Attach it as primary IDE master and the ISO as secondary IDE master. On the welcome screen press **I**, read the destination status, then press **I** again to install. Esc backs out. The installer only accepts an expressly marked target, verifies copied sectors, initializes Fresh Home, and publishes the MBR last. You can use apps during the copy. On success, shut down the VM, disconnect its CD and boot the disk. Keep the installed VM outside the build output folder.
* **Not a Windows migration tool:** the installer refuses ordinary disks. There is no automatic NTFS import, repartition of an existing PC, or Windows deletion path.

QEMU example (regular Linux host, no KVM required):

```sh
python3 tools/new-target.py fresh-target.img
qemu-system-x86_64 -m 96 -drive file=fresh-target.img,format=raw,if=ide,index=0 \
  -drive file=out/FreshOS-Normal.iso,media=cdrom,if=ide,index=2 -boot order=d
```

## 3 · Use the desktop

The welcome scene uses the supplied art. The desktop, clock, bubbles, launcher, windows, controls and applications are rendered by the kernel. The rail opens **Files**, not a pretend Chrome browser. The desktop starts in GUI mode. Settings offers GUI/CLI, wallpaper, shell text colors, motion, software brightness and PC-speaker feedback. Click a value or use keys **1–6** in Settings. Paint can turn your own drawing into the wallpaper; the builder also accepts a photograph.

| Control | Action |
|---|---|
| Enter | Enter live session; select focused launcher or tray item |
| F1 / glossy left arrow | All Apps in GUI; text Start in CLI. Arrows / Tab to navigate. Click More for page two. |
| Windows key / F2 | Minimized apps; Up/Down and Enter restore. F2 works when the host intercepts Win. |
| F3 | Switch between the graphical desktop and command-line desktop |
| F4 / Esc / window `_` | Minimize without losing state. `x` closes the session. Drag the title bar to move a window. |
| F6 / F7 | Sound-feedback level / screen dimmer; Left/Right adjusts, Esc dismisses |

**Command-line desktop:** no desktop cursor; the cursor returns inside graphical apps. `start` lists runnable apps as text and leaves the prompt active. Type an app name, or `open NAME`. Prefixes show matching apps; Tab completes a name, Up recalls the last command. Try `start`, `writer`, F4, `tasks`, `resume writer`, `wall lagoon`, `color ice`, `time`, `gui` or `help`.

**Apps:** Files (workspace shortcuts), Writer (plain-text scratchpad), Paint, Calculator (integer arithmetic), hardware Clock, Settings, Terminal, playable Minefield, System information, Android Activity viewer, month Calendar, Tasks and EXE Runner. These are built-in sessions in one address space, not isolated preemptive user processes.

## 4 · Keep files and preferences

Without a writable **primary IDE Fresh Home disk**, everything is temporary. On a mounted Home disk, write in Writer, minimize, then type `save note.txt`. `note.txt` is restored on boot. `save draft` / `load draft` store and recall named notes, `savecanvas` persists the canvas, `prefs` persists interface settings, `ls` lists recent versions, and `storage` reports the mount status. Documents append to a checksum-protected log; two alternating superblocks commit the latest head. Old versions consume space; there is not yet a garbage collector. Writer is limited to 1,399 bytes.

The offline host tool exchanges files with a **detached raw image**, never a mounted VM disk or live device:

```sh
python3 tools/home.py fresh-target.img put draft my-note.txt
python3 tools/home.py fresh-target.img get draft exported.txt
python3 tools/home.py fresh-target.img ls
```

## 5 · APK and Windows EXE support

Open **Files** and click `base.apk`, `hello32.exe`, or `hello64.exe`. In the command desktop use `open FILENAME`. Release 1 reads standalone APK archives inside the guest and interprets a limited TextView Activity. The new EXE Runner maps PE32/PE32+ images into private interpreted memory, implements selected Windows and CRT calls, and displays program output or an unsupported-feature diagnostic. Neither subsystem claims universal app compatibility.

See [Release 1 notes and compatibility table](docs/release-1.md) for the exact API list, limits and external package import instructions. Android example source is in `examples/android`; Windows examples are assembled and linked from `examples/windows` by the build. The three actual packages are included both inside the ISO and in the ZIP's `examples/` directory.

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

**Windows Command Prompt:** after installing those WSL prerequisites, run `BUILD-ISO.cmd Normal`, `BUILD-ISO.cmd Lite`, `BUILD-ISO.cmd VMware`, or `BUILD-ISO.cmd all`. The source build writes the actual ISOs to `out\`. No build tools are needed to use the prebuilt releases.

**Actions:** run the **Build FreshOS media** workflow manually. It builds all three editions, runs the native boot/input smoke tests under QEMU with BIOS and OVMF, then uploads ZIP64 downloads. The Normal job also installs to a fresh disk, checks saved data and boots again without the ISO. See `tools/smoke.py` to run the same checks locally; use `--install` and/or `--uefi`. `QEMU_BIN`, `OVMF_CODE`, `OVMF_VARS` can override their locations.

## 7 · Scope and provenance

[Feature-by-feature status](docs/coverage.md) tracks the Windows wishlist, including removed items. [Source requirements](docs/requested-features.txt) is the original list. The earlier transcript describes a different, unavailable implementation: this checkout started with an eleven-byte README. Its test counts and release claims are **not** inherited.

This is an experimental native OS, not a production Windows replacement. There is no arbitrary EXE/IMP or Linux compatibility layer, network stack, web browser, Wi-Fi, general USB drivers, per-process protection, antivirus, or enterprise management stack. Do not use it for irreplaceable files. No Windows source or system binaries are included. The supplied cloud artwork is used as visual source material; glyphs are rasterized from DejaVu Sans (see `docs/licenses/DejaVu.txt`).
