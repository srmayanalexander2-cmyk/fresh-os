# Native x64 application milestone

**Execution contract:** supported external x86 and x86-64 code runs directly at CPL3 on the FreshOS x64 UEFI kernel. No Windows or Linux kernel boots alongside it. QEMU/OVMF emulates the test PC, not a second application OS. This is one synchronous process at a time, not a universal compatibility layer. The UEFI kernel also enters a 32-bit user code segment for i386 PE/ELF programs. The BIOS build retains the instruction interpreter.

## Try a Windows EXE

Boot the ISO with x64 UEFI (96 MiB recommended), attach its marked Home.img, and press F3. For the packaged VMware configuration use **FreshOS-x64.vmx**; FreshOS.vmx and the browser preview boot in BIOS mode instead.

```
install cal64
run cal64 2 2024
```

The result is Chris Wellons's [cal for Windows](../extras/cal/README.md) printing all 29 days of February. `run cal64 13 2024` prints its usage error, and no arguments selects the guest RTC month. The Windows application's source is unmodified. The EXE, source, build recipe and license are included. Its imports are resolved at pack time and serviced by FreshOS, not by a host DLL. GetLocaleInfoW currently supports the English month/day queries, not arbitrary locales. Terminal accepts a bounded argument tail, not a shell language; Linux arguments are space-separated.

## What changed beneath IMP

* Each launch constructs a new four-level x64 page-table root. Kernel mappings are supervisor-only. The executable sees only its validated sections, stack, and applicable heap/import gates/anonymous pages. User pages enforce write-or-execute. The kernel returns to its own page table before calling filesystem, ABI or desktop code. This assumes x64 UEFI identity-mapped image memory, NX, SYSCALL and four-level paging.
* GDT/TSS and kernel entry stacks handle privilege transitions. The Windows import gates preserve Win64 parameter registers. Linux uses the CPU `syscall` instruction on x64 and `int 0x80` for i386. Service implementations check user buffer ranges; kernel pointers are never used as imported function addresses. The backend preserves user integer and legacy x87/SSE state; it does not expose AVX as a supported ABI.
* Application exceptions abort the current launch; they don't reset the desktop. A PIT interrupt preempts user code, with a limit of 180 user ticks (~3 seconds continuously in user mode) and a separate bounded service-work budget. Message boxes suspend execution for real acknowledgment. This is not a scheduler for multiple applications or a production security sandbox. Use trusted programs on disposable VM disks.
* A private 2 MiB pool backs supported VirtualAlloc/VirtualProtect/VirtualFree operations and anonymous private Linux mmap/mprotect/munmap. In native UEFI Linux mode, 1 MiB is reserved for a contiguous brk arena and the remaining 1 MiB is available for mmap; the BIOS/interpreter fallback bounds brk to its 256 KiB private heap. Reserved pages are inaccessible, committed pages are zero-initialized, and returning to an application reloads CR3. This is not a large or general POSIX/NT virtual memory system. Mapped images still have a 2 MiB ceiling, and the package slot holds 1 MiB.
* Static x86-64 ELF programs get argc/argv, a minimal auxiliary vector, FS-base TLS setup, vectored I/O and basic descriptor operations. File input is still bounded and read-only from the package, and stdout/stderr are text capture, not a host terminal. No ELF dynamic loader, multi-threading, signals or general filesystem is claimed.

## Reproduce the tests

```
python -m pip install -r requirements.txt
python forge/media.py Lite
python qa/processes.py Lite
```

The test boots the guest, exercises an actual PE32+ `cal.exe`, checks Windows reservation/commit/protection/decommit/zero-on-recommit/release, and checks Linux brk growth, shrink/recommit zeroing, and anonymous mapping/protection/unmapping. Its ELF test checks the CPU privilege level. A write to its own read-only mapping, an intentional invalid instruction, and a CPU loop verify that an application terminates without losing the desktop. A subsequent ELF launch must succeed. The existing `qa/packages.py Lite --efi` also checks the local Windows DLL call, synchronous Win32 dialog, resources and install persistence. Disable Secure Boot and provide an OVMF firmware pair as in `qa/boot.py`.

**Independent Linux application test.** Build upstream musl 1.2.5 and BusyBox 1.36.1 on an x86-64 Linux host (C compiler, binutils, GNU make, standard build shell utilities). This optional step downloads pinned sources, not a modified application. BusyBox is not redistributed in the FreshOS ZIP.

```
qa/build-busybox.sh /tmp/fresh-bb
python qa/processes.py Lite --busybox /tmp/fresh-bb/busybox-src/busybox
```

The same BusyBox ELF is run on Linux and in the booted FreshOS guest: echo, cat, wc, sort and false. Output/exit status are compared; `wc` and `sort` consume actual bundled file data. To use it interactively, pack it with `forge/imp.py pack --native64 ... --resource data.txt`, install it on a powered-off Home disk under the name `busybox`, then `run busybox wc data.txt`. Passing a few applets does not imply that all BusyBox applets or all musl programs work.

**Chrome remains outside the supported set.** Windows Chrome needs concurrent child processes, threads, substantially more memory, NT/Win32 synchronization and file/registry services, networking, and the browser's graphics and sandbox contracts. IMP packaging and user-mode instruction execution do not implement those services. The Chrome acceptance test (official installer, launch, HTTPS navigation, profile persistence on restart) has **not passed**.

For installer preflight results, measured size limits and the additional Windows application tests, see [Chrome acceptance](chrome-acceptance.md).

## Optional app disk and parent/child execution (native UEFI only)

`python forge/appdisk.py Data.img cal.exe` creates a **new** sparse disk, with
a marked 384 MiB FAT16 volume. Use DOS 8.3 names for input files. Attach it as
a second IDE hard disk alongside Home.img. The guest never formats a disk or
mounts an arbitrary FAT partition for writes. Press F3 and type `runfile
cal.exe 2 2024`. Paths in `runfile` are relative to this disk's root. Win32
apps access it as `F:\`; `F:\STAGE\SETUP.EXE` is `runfile stage/setup.exe`.
The default Home and its package layout have not grown. Neither a fresh VM
profile nor the BIOS-only preview attaches this optional volume by default.

Raw x86/x64 EXEs on the data volume are read directly, checked, and bound in
the guest, with a 4 MiB per-image limit, preferred-base mappings, exact named
imports and W^X enforcement. Static TLS on the primary thread supports a
bounded template, zero-fill, up to 16 callbacks, and the PE32 FS / PE64 GS
TLS-vector fields. This isn't a full Windows TEB, DLL loader, or SEH runtime.
For PE32 GuardFlags `0x500`, the raw loader checks read-only metadata and
validates SafeSEH RVAs (it does not run exception handlers), copies a bounded
sorted target table into kernel-only memory, and installs a checked CFG gate.
If the image requests a cookie, RDRAND must succeed. Unsupported load-config
formats and delay imports still block execution. FatFs
supports multiple binary file handles, subdirectories, synchronous reads and
writes up to 64 KiB per call, seek within a file, flush and close. Use
trusted programs with disposable media. No long filenames, Unicode paths,
async I/O or general Windows file sharing/locking contract is promised.

`CreateProcessW` accepts a published PE package name or an absolute `F:` path.
The parent yields until the child completes. Each resident has private page
tables, mapped backing, heap, stack, open handles, args and package resources.
The parent receives real completion status, not a simulated installer result.
This is limited to one child level, four launches per parent, no custom
environment, startup attributes or handle inheritance. Wait/exit/close operate
on the returned process/thread handles; there is no concurrent scheduler.

A bounded HKCU registry on the explicit app disk supports RegCreateKeyExW,
RegOpenKeyExW, RegQueryValueExW, RegSetValueExW, RegDeleteValueW and
RegCloseKey. The two reserved root files `FREG0.DAT` and `FREG1.DAT` contain
checksummed generations. Key and value names use case-insensitive ASCII through
the wide APIs; values may contain binary data. Limits: 64 keys (including
root), 96 values, 1024 bytes per value, 16 handles and 48 mutations per launch.
There is no HKLM, ACL model, notifications or alternate WOW64 view. This is
app data on disposable media, not a secure or Windows-compatible hive file.

### Additional Win32 runtime services

* The native PE path implements four private heaps per process, with separate page reservations in the bounded virtual-memory arena and up to 64 live blocks each. Destroy releases the reservation. Allocation handles are checked for ownership; unsupported flags (including executable heaps) are refused. These are not full-size Windows heaps.
* Each address space supports 128 critical sections. Initialization, recursive entry, try-enter, leave and deletion track ownership of the sole thread. No other thread can contend in the current process model.
* Advapi32 ACL and SID routines manipulate guest buffers, including checked insertion/deletion and size queries. A well-formed SID or ACL is not a kernel access token.
