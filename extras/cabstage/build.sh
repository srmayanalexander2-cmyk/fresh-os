#!/bin/sh
# Build against unchanged upstream decoder code; all I/O is ordinary Win32.
set -eu
src=$(cd "$(dirname "$0")" && pwd)
lib=$(cd "${1:?libmspack source checkout}/libmspack/mspack" && pwd)
out=$(realpath -m "${2:?output .exe}")
implib=$(realpath "${3:?media pe64/KERNEL32.a}")
tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT; cd "$tmp"
cc() { if test -n "${ZIG:-}"; then "$ZIG" cc -target x86_64-windows-gnu "$@"; else x86_64-w64-mingw32-gcc "$@"; fi; }
for unit in "$src/stage.c" "$lib/system.c" "$lib/cabd.c" "$lib/lzxd.c" "$lib/mszipd.c" "$lib/qtmd.c"; do
 cc -Os -ffreestanding -fno-sanitize=all -fno-asynchronous-unwind-tables -fno-stack-protector -mno-stack-arg-probe -DMSPACK_NO_DEFAULT_SYSTEM -I"$lib" -c "$unit" -o "$(basename "$unit" .c).o"
done
ld -mi386pep --no-insert-timestamp --subsystem console -s -e mainCRTStartup -o "$out" ./*.o "$implib"
