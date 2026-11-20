# FreshOS Release 1 · 1.0.0

**Release 1 adds intact APK loading and an experimental Windows executable runner to the native desktop.** Pick Normal, Lite or VMware and extract the ZIP. Each contains a bootable 8,000,000,000-byte ISO, examples and a SHA-256 checksum; most of the image is empty Fresh Home capacity. The VMware bundle additionally contains its machine configuration and a prepared install disk.

### Try it without installing

Boot the image, press Enter, then open **Files**. Its three package buttons run **base.apk**, **hello32.exe** and **hello64.exe**. Or press F3 and type `open base.apk`, `open hello32.exe`, or `open hello64.exe`. Press F4 to minimize between programs. APK text comes from the DEX program; EXE output and exit status come from interpreted machine code.

### Bring your own package

With the VM shut down and its **raw Fresh Home image** detached, run on the host:

```sh
python3 tools/home.py fresh-target.img put-apk postcard.apk /path/to/base.apk
python3 tools/home.py fresh-target.img put-exe myapp.exe /path/to/application.exe
```

Then boot the disk and type `open postcard.apk` or `open myapp.exe`. Home records take precedence over built-in filenames. `apk demo` and `exe demo` always run the built-in samples. Raw guest files are preserved byte for byte. The offline importer does not edit physical devices or VMware VMDK containers.

### Compatibility contract

| Format | Release 1 behavior |
|---|---|
| Standalone Android APK | The guest reads ZIP central/local headers, inflates stored or DEFLATE members, checks CRC-32, parses the binary Android manifest, resolves a concrete MAIN/LAUNCHER Activity, and interprets its primary DEX. Legacy DEX-only Home records still open. |
| Android runtime | Direct Activity subclass, `onCreate(Bundle)`, `TextView` construction, string literal, `setText(CharSequence)`, `setContentView(View)`, return. APK ≤2 MiB, primary DEX ≤1 MiB, manifest ≤128 KiB. No APK signature trust verification, ART, split APKs, multidex, native libraries, resource inflation, activity aliases, or Play Services. An arbitrary file named `base.apk` is not necessarily a standalone compatible app. |
| Windows PE32 / PE32+ | Reads genuine x86/x64 sections at their preferred virtual addresses, binds named imports, interprets an integer instruction subset, implements x86 stdcall/cdecl and x64 argument passing, displays output and exit status. Both guest architectures work on BIOS and UEFI builds. No guest machine code is executed in kernel mode. |
| Supported Windows calls | `kernel32`: ExitProcess, GetStdHandle, WriteFile (standard output/error only), WriteConsoleA/W, lstrlenA/W, GetCurrentProcessId, GetProcessHeap, HeapAlloc, HeapFree, GetLastError, SetLastError. `user32`: plain MessageBoxA/W (captured text, returns OK). `msvcrt`: puts, putchar, strlen, malloc, calloc, free, memcpy, memset, memcmp, exit. No Windows DLLs are redistributed. |
| EXE limits | File and mapped image ≤2 MiB, private 64 KiB stack and 256 KiB guest heap, bounded output and work. Supports basic register/memory moves, common integer arithmetic, comparisons, branches, calls, returns and stack operations; not a complete x86 CPU. Unsupported imports and instructions stop with a diagnostic. Not general Windows app or installer compatibility: no .NET, system DLL loader, drivers, threads, SEH, TLS callbacks, registry, general GUI or DirectX. |

**Validation:** `python tests/test_compat.py -v` compares real outputs and exit codes, including independently C-compiled 32/64-bit arithmetic and CRT programs. `tools/smoke.py` boots the actual ISO in QEMU and launches all three packages; `--install` also verifies external package import and launch after installation and reboot. Physical hardware and VMware Workstation are not certified. Use a VM or spare media, not an existing OS disk.
