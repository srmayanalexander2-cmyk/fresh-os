# Third-party Win64 compatibility fixture: cal

`cal.c` is the **unchanged** [Win32 cal application by Chris Wellons](https://github.com/skeeto/scratch/blob/58470254f4a95cdf7a53888e405c851c21eb2cae/windows/cal.c), distributed under the accompanying Unlicense. `cal.exe` is a stripped PE32+ build using Zig 0.13.0's MinGW headers/Clang and GNU binutils. Rebuild on Linux with `ZIG=/absolute/path/to/zig ./extras/cal/build.sh`, or use a MinGW-w64 cross compiler. No FreshOS header, implementation or special branch is compiled into this executable.

On **x64 UEFI**, boot with the new Home.img, open Terminal, `install cal64` then `run cal64 2 2024`. The executable obtains its UTF-16 command line, UTC clock fields (from the guest RTC), English locale strings and console handle via the Win32 adapters. The calendar calculation and formatting are the Windows application's own code. BIOS does not provide the native x64 execution backend.
