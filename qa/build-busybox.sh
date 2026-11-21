#!/bin/sh
# Optional interoperability fixture. Builds unmodified upstream sources outside the repo.
set -eu
: "${1:?Usage: build-busybox.sh EMPTY_WORK_DIRECTORY}"
mkdir -p "$1"; cd "$1"; root=$PWD; test ! -e musl-src; test ! -e busybox-src
fetch() { curl --fail --location --silent --show-error "https://codeload.github.com/$1/tar.gz/$2" -o source.tgz; mkdir "$3"; tar xzf source.tgz --strip-components=1 -C "$3"; rm source.tgz; }
fetch ifduyue/musl 0784374d561435f7c787a555aeab8ede699ed298 musl-src
fetch mirror/busybox 1a64f6a20aaf6ea4dbba68bbfa8cc1ab7e5c57c4 busybox-src
(cd musl-src && ./configure --disable-shared --prefix="$root/musl" && make -j2 && make install)
cd busybox-src; make allnoconfig
for k in STATIC DESKTOP CAT ECHO WC HEAD PRINTF BASENAME CUT TR TRUE FALSE UNAME ENV SORT; do sed -i "s/# CONFIG_$k is not set/CONFIG_$k=y/" .config; done
yes '' | make oldconfig >/dev/null
make -j2 CC="$root/musl/bin/musl-gcc"
