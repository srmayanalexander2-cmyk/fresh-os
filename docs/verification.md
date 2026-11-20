# Local verification · 2026-11-20

The downloadable editions were built from this checkout with GCC 12 / binutils 2.40, Pillow 13.0.0 and pycdlib 1.22.0. Use **Python 3.11+** for the pinned host dependencies. Release `.sha256` files apply to the **uncompressed ISOs**. Each image contains a copy of the build-time README; its earlier Python minimum is superseded by this requirement.

| Image | QEMU i386 BIOS, 32 MiB | QEMU x64 OVMF, 96 MiB |
|---|---|---|
| Normal | Passed | Passed, including install, note/profile persistence and disk-only reboot |
| Lite | Passed | Passed |
| VMware edition ISO | Passed | Passed |

These were software-emulated boots in QEMU 11.0.2, not structural checks alone. Each smoke run sent keyboard scancodes to enter the desktop, launch Files, minimize it, restore it with the Windows key, switch to CLI mode and launch the sample Android Activity and Calculator. Serial markers came from the executing guest. The install test used a newly created raw target. It checked the saved Writer bytes with an independent host reader and launched an app after booting the installed disk with the CD removed.

The BIOS code path was also exercised in v86. The optional browser preview is a test machine, not an HTML implementation of the desktop: it boots the optical portion of the same Normal release, with the blank trailing Home partition omitted. Its emulator (`v86` 0.5.473) and SeaBIOS/VGA BIOS are host-side test equipment, not bundled OS dependencies. Preview controls supply key events for hosts that capture function keys.

The growable `.vmdk` was recognized by `qemu-img` as a monolithic sparse 8,000,000,000-byte disk. No claim of testing on VMware Workstation, bare metal or Secure Boot hardware. Local smoke tests do not imply universal compatibility, pixel-perfect reference matching, or security certification.

The preview itself was also booted in headless Chromium. Automated input sent Enter and F1 and captured the live launcher framebuffer, including the runtime clock and app icons.

## Release 1 · compatibility and install regression checks

The final `v1.0.0` media passed both BIOS and UEFI package-launch smoke tests for Normal, Lite and VMware edition. Each guest opened `base.apk`, `hello32.exe` and `hello64.exe`. Normal additionally passed installation and a disk-only reboot on **both** firmware paths: the host imported a whole APK and PE32 file into the detached target, compared the stored bytes, then the guest opened both imported files.

During testing under concurrent host I/O, the old installer polling loop timed out. R1 waits for the expected ATA data-request state using a PIT-clock deadline rather than a CPU-speed-dependent spin count, and logs which read/write/verification phase failed. The subsequent install checks passed. All three final ZIP64 members were decompressed and their ISO SHA-256 values matched `releases/*.sha256`.

Six host-side compatibility tests cover the Win32/Win64 samples, compiler-generated arithmetic and CRT programs, a missing import, instruction budget exhaustion, stored and DEFLATE APK members, manifest-based class selection, and rejected invalid archives. These checks are not evidence that arbitrary APKs or Windows EXEs run.
