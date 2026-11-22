# Stratus runtime boundaries and next steps

## Kernel and the removed sidecar experiment

FreshOS 2's Stratus kernel is the native x86 implementation under `native/`. It does not contain or boot the Linux kernel. The earlier standalone Linux 6.12.118 + Alpine image was an experimental, separate boot test; it was not integrated with Stratus. The current build no longer includes that Linux sidecar builder or image.

The current Stratus x64 UEFI runtime can execute a bounded set of PE programs and static ELF programs. It is not Wine, a Windows kernel, or a general Linux environment. The tested Windows and Linux subsets, execution limits, and reproduction steps are documented in [native x64 application support](native-processes.md) and [IMP2](imp.md). Dynamic ELF loading, a broad POSIX syscall surface, concurrency, and many Win32/NT services remain unsupported.

## Compatibility is incremental, not universal

No honest engineering test can guarantee that every Windows application works with Wine or another compatibility layer. Compatibility depends on each app's APIs, services, drivers, graphics/audio stack, concurrency, memory use, DRM/anti-cheat, and hardware assumptions. Wine itself does not make every Windows program compatible. Stratus does not currently ship Wine, and no Windows-app compatibility claim should be inferred from passing a small set of PE tests.

The next compatibility priority is an upstream Wine port. Its staged prerequisites and current blockers are tracked in [Wine on Stratus](wine-port-roadmap.md). This is a roadmap only: no Wine source or Wine runtime has been integrated into the current build. The practical path is to select real target applications and test their install, launch, core workflow, persistence, networking, and failure behavior on the native kernel. Passing one program does not imply support for another. A broader POSIX layer and a Wine port are major separate projects, not a kernel switch or a guarantee of all-app support.

## Removable-device safety

The current Stratus kernel has no USB mass-storage driver. Its Home and installer paths use ATA/IDE and accept only a disk bearing FreshOS's explicit Home-2 marker; installer writes are restricted to that marked target. `lspci` reports device IDs but does not attach a driver. The existing QEMU tests use virtual disks and do not pass through a physical USB device. This is not evidence of tested USB support.

Before adding removable-storage support, the required safety contract should be explicit: no automatic formatting or writes, show the selected device and capacity, default to read-only access, and require a separate confirmation before any write. That policy and real-hardware tests must precede claims about owning or safely using a user's USB devices.

## Setup, taskbar, animation, and kernel hardening

The first-run setup now lets the user choose accent, wallpaper, taskbar position, auto-hide, and default window mode before trying the live desktop or opening installation options. Full Settings remains available for brightness, feedback, and graphics preferences. Taskbar position is persisted in Home snapshots; older snapshots retain their existing data and default to the left position until the next save.

Mouse-operated controls get a brief accent pulse, and major workspace changes use short fades. Fades are intentionally skipped in Fast graphics mode. This is a first animation pass, not an assertion that every app surface, keyboard action, or individual content change has bespoke motion.

Kernel panic frequency cannot be guaranteed to be “extremely low” by a code change alone. Stratus already reports CPU exception details and aborts a failing user application without intentionally treating it as a kernel panic, but the kernel still has bounded hardware support and needs broader stress, fault-injection, and physical-device testing. New drivers and compatibility services should be added behind small, testable interfaces, with failure tests and explicit device ownership rules.

## Suggested validation sequence

1. Keep a disposable disk image and verify setup choices, snapshot migration, taskbar geometry, and UI behavior in BIOS and UEFI QEMU.
2. Exercise boot, idle, display, input, disk I/O, and CPU exception paths with stress and fault injection; report failures rather than promising zero panics.
3. Define a removable-device contract and implement safe USB enumeration before USB storage access.
4. Choose a short list of specific Linux and Windows applications; add each to a versioned acceptance matrix and expand the required ABI/services based on observed failures.
