# Wine on Stratus: port plan

**Status: not ported.** This document records an engineering path for bringing the upstream Wine userland to FreshOS's native Stratus kernel. It is not a claim that Wine, `wineboot`, or third-party Windows applications run in FreshOS today.

## Non-negotiable boundary

Stratus remains FreshOS's own x86 kernel. The port must not boot, embed, or depend on a Linux kernel or an external Linux service. Wine is a user-space Windows API/runtime compatibility project, not a kernel that can be enabled by toggling a build option. Its Unix-side components still need a host process model and operating-system services. Porting those interfaces to Stratus is substantial work.

The upstream source candidate is the [Wine GitHub mirror](https://github.com/wine-mirror/wine). Upstream source should be imported or pinned only as part of a buildable, license-reviewed integration; downloading the tree alone would not constitute a port.

## FreshOS prerequisites and current gaps

The existing Stratus x64 UEFI backend runs selected PE images natively and a small, tested set of static ELF programs. That code is useful groundwork, but it is not Wine and is not a Linux host environment. In particular, today's runtime has:

| Capability | Current state | Why Wine needs more |
|---|---|---|
| Linux ELF loading | Bounded static x86/x64 ET_EXEC only | Wine's Unix loader and libraries need dynamic ELF/shared-object loading, relocations and loader-compatible TLS. |
| Process model | One synchronous application; a bounded child-launch path | Wine uses multiple processes and needs reliable wait/exit, handle and IPC behavior. |
| Threads and synchronization | No general concurrent scheduler or pthread runtime | Wine and many Windows apps need threads, futex-like synchronization, signals and cancellation behavior. |
| Virtual memory | Small fixed image and anonymous-mapping arenas | Wine needs larger, independent, correctly protected mappings for executable images, heaps and shared libraries. |
| Filesystem | Marked Home documents and an optional bounded app disk | Wine needs path resolution, directories, metadata, mutation rules, file sharing and a stable prefix layout. |
| IPC and networking | No general Unix sockets or internet stack | The Wine server and common applications rely on local IPC; many apps also need sockets and network services. |
| Windowing and devices | FreshOS desktop and bounded framebuffer/input/audio paths | Wine needs a window driver that maps Win32 windows, input, clipboard and graphics into the FreshOS desktop. |
| Win32/NT services | A bounded native import allowlist | Wine has a much broader NT, registry, loader, COM, graphics and service surface. |

The detailed current runtime contract is in [native-processes](native-processes.md), [IMP2](imp.md), and the [Stratus runtime roadmap](native-runtime-roadmap.md).

## Port milestones

1. **Define the host ABI.** Specify process, thread, virtual-memory, file, clock, synchronization, IPC and error semantics for Stratus. Keep system-call entry and user pointers checked; add host tests before exposing each interface.
2. **Build the required ELF/POSIX substrate.** Add a tested dynamic ELF loader and shared-object model, larger per-process mappings, stable TLS and process arguments, then expand filesystem and syscall coverage. Do not label a syscall as supported until a guest test exercises its success and failure paths.
3. **Add concurrency and local IPC.** Implement scheduler-owned threads, lifecycle/wait semantics and synchronization primitives before attempting `wineserver`. Test preemption, teardown, invalid pointers and resource exhaustion in disposable QEMU disks.
4. **Port upstream Wine's Unix side.** Pin a Wine revision, identify the minimum modules and host interfaces, add a Stratus build target and required C/runtime shims, and build them reproducibly. Do not replace this with a small hand-written PE import list and call it Wine.
5. **Integrate the Wine server and desktop driver.** Implement and test process/object IPC and map Wine windows, input and clipboard to Stratus. Keep device access and installation explicitly scoped; never automatically format or write user media.
6. **Establish a versioned application matrix.** Start with Wine's own smoke path (`wine --version`, prefix initialization, and a simple built-in console program), then add user-selected Windows applications with exact versions and workflows. A pass for one program is not a claim for another.
7. **Ship every current edition from one tested source.** Once a milestone passes on x64 UEFI, rebuild Normal, Lite, VMware and VMware-Lite, check package hashes, and rerun BIOS/UEFI regressions. BIOS support must be stated separately if the Wine backend requires x64 UEFI.

## Compatibility promise

There is no technically honest way to guarantee that every Windows program ever made works with Wine. Applications depend on different APIs, drivers, graphics, services, concurrency, memory, copy protection and hardware. Even upstream Wine has application-specific compatibility. FreshOS will report only exact tested programs, versions, workflows and firmware paths; unsupported programs must fail clearly rather than be described as universally compatible.
