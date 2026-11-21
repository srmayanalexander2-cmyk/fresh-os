#!/bin/sh
# Rebuild the ordinary PE64 console EXE (not linked against FreshOS).
set -eu
src=$(cd "$(dirname "$0")" && pwd); tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
cd "$tmp"; flags='-Os -ffreestanding -fno-stack-protector -fno-asynchronous-unwind-tables -mno-stack-arg-probe'
if test -n "${ZIG:-}"; then "$ZIG" cc -target x86_64-windows-gnu $flags -fno-sanitize=all -c "$src/cal.c" -o cal.obj
else x86_64-w64-mingw32-gcc $flags -c "$src/cal.c" -o cal.obj; fi
# GNU ld can build a PE import library without needing to run a Windows linker.
printf '.text\n.global stub\nstub: ret\n' | as --64 -o stub.o
printf '%s\n' 'LIBRARY kernel32.dll' EXPORTS GetCommandLineW=stub GetConsoleMode=stub GetLocaleInfoW=stub GetStdHandle=stub GetSystemTime=stub WriteConsoleW=stub WriteFile=stub >exports.def
ld -mi386pep --no-insert-timestamp --dll --out-implib kernel32.a -e stub -o kernel32.dll exports.def stub.o
ld -mi386pep --no-insert-timestamp --subsystem console -s -e mainCRTStartup -o "$src/cal.exe" cal.obj kernel32.a
