#!/bin/sh
# Build unchanged Win32 sources with ordinary PE import libraries.
set -eu
src=$(cd "$(dirname "$0")" && pwd)
out=${1:?output directory required}; mkdir -p "$out"; out=$(cd "$out" && pwd)
tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT; cd "$tmp"
cc() { if test -n "${ZIG:-}"; then "$ZIG" cc -target x86_64-windows-gnu "$@"; else x86_64-w64-mingw32-gcc "$@"; fi; }
# Link with GNU ld to avoid adding a CRT startup or TLS directory.
for name in getfiletype shuttercount; do
 cc -Os -ffreestanding -fno-sanitize=all -fno-stack-protector -fno-asynchronous-unwind-tables -mno-stack-arg-probe -c "$src/$name.c" -o "$name.obj"
done
printf '.text\n.global stub\nstub: ret\n' | as --64 -o stub.o
lib() { dll=$1; shift; { echo "LIBRARY $dll.dll"; echo EXPORTS; for s do echo "$s=stub"; done; } > exports.def; ld -mi386pep --no-insert-timestamp --dll --out-implib "$dll.a" -e stub -o "$dll.dll" exports.def stub.o; }
lib kernel32 GetStdHandle WriteFile GetFileType CreateFileW CloseHandle ReadFile ExitProcess GetCommandLineW VirtualAlloc
lib shell32 CommandLineToArgvW
lib user32 MessageBoxW
lib msvcrt memset memmove
ld -mi386pep -s --no-insert-timestamp --subsystem console -e mainCRTStartup -o "$out/getfiletype.exe" getfiletype.obj kernel32.a
ld -mi386pep -s --no-insert-timestamp --subsystem windows -e WinMainCRTStartup -o "$out/shuttercount.exe" shuttercount.obj kernel32.a shell32.a user32.a msvcrt.a
