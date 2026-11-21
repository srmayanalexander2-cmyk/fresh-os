# IMP2 on Stratus

**Try it without a toolchain:** Attach this build's `Home.img`, boot the ISO, click **Use OS**, then the **IMP** smile at the top of the cyan rail. Choose **Install: windll**, then **Run: windll**. The output comes from an x64 EXE calling a function in an application-local DLL. `win32` uses `WriteFile`; `win64` uses `MessageBoxW` (a native modal dialog); `linux32` opens and reads a packaged resource; `linux64` writes to stdout. No Windows or Linux kernel or host-side emulator service is required. On x64 UEFI also try `install cal64` then `run cal64 2 2024` (a genuine third-party Win64 executable). Without the marked disk there are no packages.

## Bring another binary

Use Python 3 and `python -m pip install pefile==2024.8.26` on the **host**. Shut the VM down and work on a backup copy of its **raw** Home.img extent, not the ISO or VMDK descriptor. Keep the packer's directory structure when extracting the ZIP.

```
python forge/imp.py pack path/to/demo.exe demo.imp --dll path/to/helper.dll --resource path/to/greeting.txt
python forge/imp.py install demo.imp Home.img --name demo --stage
```

DLLs and resources are optional; repeat those flags as needed. For a static Linux ELF, use its path instead of the EXE. The packer never runs the inputs. Boot with that Home disk, press **F3**, type `install demo`, then `run demo [ARGS...]`. Pass `--native64` when packing an application that requires CPU execution; such packages explicitly refuse the portable backend. Or use the Install and Run buttons in IMP Library. Installation checks the package and image before marking the slot installed; it does not run the application. Restarting preserves the install. `install` **without a name** is the separate OS installer. Omitting `--stage` on the host preinstalls the package. Use new names; the tool does not replace existing packages. Slots can be reclaimed by restoring a backup. No drag-and-drop, URL installer, or Windows setup wizard.

## What an IMP contains

An IMP2 envelope holds a prelinked Windows page image or unchanged Linux ELF plus up to 16 named, read-only resources. No need for the original EXE/DLLs beside the installed package. The Windows packer maps sections, binds named OS imports to the guest ABI, links explicit local DLL exports and applies x86 HIGHLOW / x64 DIR64 base relocations. Local DLLs must not require DllMain, TLS or forwarded exports. Unknown imports are errors, not success-returning stubs. Embedded PE resources are mapped bytes, **not** a full Windows resource API. System DLL implementations, registry state, services and installers are not conjured by conversion.

On disk: little-endian `IMP2`, u32 kind (bit 0: Windows=0, Linux=1; bit 1: require native x64), u32 code size, u32 resource count, u32 CRC32 of the remaining bytes, code, then `{char name[28]; u32 bytes; u8 data[bytes]}` records. CRC detects corruption, not authenticity. Eight 1 MiB slots begin at LBA 41472 on Home-2, each with a separate 512-byte header. The OS installer and document snapshots do not overlap them. Read resources by lower-case basename. No guest access to the host filesystem.

## Compatibility boundary

* **Windows:** native 32-/64-bit CPU execution on supported x64 UEFI machines, bounded integer interpreter elsewhere; the exact import allowlist in `native/compat/imports.def`, a 2 MiB mapped image, 64 KiB stack, 256 KiB heap and 80,000 work units per launch. Plain ANSI/Unicode MessageBox calls display a synchronous native dialog with ASCII text; click OK or press Enter. The same text is captured in the bounded output buffer. Bundling succeeds only for supported image structures; it is not proof the entire instruction or API path works. On native x64, x87/SSE2 state is preserved, and a separate 2 MiB virtual-memory pool supports reserve/commit/protect/release. No AVX contract, general Win32 windows, drivers, NT kernel, threads or concurrent child processes.
* **Linux:** little-endian static x86 and x86-64 ET_EXEC. `int 0x80` / `syscall` adapters for exit, pid/tid, stdout/stderr and read-only package resource open/read/close. The native x64 path adds argument/auxiliary vectors, FS-base TLS initialization, vectored I/O and private anonymous mappings, and runs selected statically linked musl BusyBox applets. No dynamic linker, shared libraries, glibc environment, fork/exec, X11/Wayland or networking. Unsupported calls return an error.
* **Chrome: not supported or installed.** Neither Windows ChromeSetup.exe nor a Linux Chrome package can be made functional by adding DLLs or changing the extension. A browser needs, among other things, processes, a much larger address space, a supported graphics stack, network/TLS and sandbox facilities. There is no Chrome stand-in in the shell. Use known test programs, not untrusted executables; these experimental runtimes have not been security audited.

`python qa/packages.py Lite` exercises the installed programs on BIOS; add `--efi` for x64 UEFI. `samples/` contains their sources. `forge/media.py` builds real PE/ELF inputs using GNU binutils; dummy import-library stubs used by the linker are **not** packaged as OS DLLs. The instruction engine is adapted from 1.5; the Stratus kernel, packaging, install and application resource path are independent.
