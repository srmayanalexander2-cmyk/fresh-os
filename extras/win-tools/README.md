`getfiletype.c` and `shuttercount.c` are unchanged public-domain programs from
Chris Wellons's `skeeto/scratch`, `windows/`, commit
`58470254f4a95cdf7a53888e405c851c21eb2cae`. See LICENSE. These are additional
QA applications, not programs built into FreshOS or shipped in the release ZIP.

On Linux with x64 MinGW-w64 GCC and GNU binutils (or set `ZIG` to a Zig 0.13
executable), run `extras/win-tools/build.sh /tmp/win-tools`. Only standard
Windows DLL names are used; the import libraries are link-time artifacts,
not runtime DLL implementations. With Lite media built and QEMU/OVMF ready,
run `python qa/win_tools.py Lite /tmp/win-tools`. This exercises actual stderr,
a generated 12-byte input matching the upstream Nikon tag heuristic, and an
absent file. It does not assert support for large camera images or filesystem
paths outside the package/Home ABI.
