# FreshOS IMP1 — prelinked executable image

Opening a supported EXE links it into a temporary IMP and executes that image. To produce a named reusable file, `convert hello64.exe` links an executable into `hello64.imp`. `open hello64.imp` loads the image and runs its entry point. The linker maps PE sections, resolves supported Windows/CRT imports to ABI 1 guest service addresses, records page permissions and omits trailing zero data on each page. It does **not** just rename the EXE. The IMP loader needs no original EXE, PE parser, host DLLs or network. Conversion does not make unsupported Windows APIs or instructions work.

**From Files:** select an EXE in Library or Recent, choose **Convert to IMP**, then **Open**. The generated image is immediately available in Recent. On writable Fresh Home it is also committed to disk; on read-only media only the most recent conversion lives in RAM. Saving the same name appends a version. `hello32.imp` and `hello64.imp` ship prelinked for offline use.

**From the host:** build with `cc -std=c11 -O2 -Ikernel tools/imp-cli.c kernel/pe.c -o imp`, then run `./imp convert input.exe output.imp` or `./imp run output.imp`. The converter refuses to overwrite an existing host file. Import a finished image with `python tools/home.py fresh-target.img put-imp app.imp output.imp`, with the VM off and the raw image detached. The guest uses the same linker and loader as the host tool. Home names are lowercase ASCII, up to 31 characters.

## Wire format (little endian)

| Offset | Bytes | Meaning |
|---|---|---|
| 0 | 4 | ASCII `FIMP` |
| 4 | 1 each | Version 1, architecture (32/64), guest ABI 1, header size 32 |
| 8 | 4 | Exact serialized length |
| 12 | 4 | Mapped image length (1–2,097,152 bytes) |
| 16 | 8 | Preferred guest base address |
| 24 | 4 | Entry offset in an executable page |
| 28 | 4 | IEEE CRC-32 of the entire file with this field treated as zero |

From byte 32 there is exactly one page record per 4 KiB of mapped image (round up). Each contains `u8 flags` (R=1, W=2, X=4), `u8 reserved=0`, `u16 initialized_length`, then that many bytes. An inaccessible page has no bytes. Remaining bytes are zero-filled. The last page cannot exceed the image length. No trailing bytes. Headers, ABI version, bounds, reserved regions, entry, CRC and page records are checked before execution. CRC detects corruption; it is **not** a signature. The linked image may contain read-only PE header data; it is not interpreted as a PE file at launch.

## ABI and limits

ABI 1 freezes the import table order in `kernel/pe.c`. Service address `0x7fe00000 + 16*ordinal` is interpreted, never called in kernel mode. Both bitnesses share the bounded x86 interpreter (64 KiB stack, 256 KiB heap, instruction/work budget). Includes basic integer arithmetic/control flow, console writes, plain message boxes and selected heap/string/CRT operations. No general GUI, installer services, .NET, TLS callbacks, drivers, threads, DirectX, or universal Windows compatibility. A program can link successfully but still encounter an unsupported instruction when run. APK/DEX support is removed.
