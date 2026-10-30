#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 xPsycho999 (original Freeciv 3DS port)
# Copyright (C) 2026 the freeciv-3ds authors
set -e

source /etc/profile.d/devkit-env.sh
export PATH=$DEVKITARM/bin:$PATH

export CC=arm-none-eabi-gcc
export CXX=arm-none-eabi-g++
export AR=arm-none-eabi-ar
export RANLIB=arm-none-eabi-ranlib
export CFLAGS="-march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft -O3 -mword-relocations -fomit-frame-pointer -ffast-math -fno-short-enums"
export CXXFLAGS="$CFLAGS"
export CPPFLAGS="-I$DEVKITPRO/libctru/include -I$DEVKITPRO/portlibs/3ds/include -D__3DS__"
export LDFLAGS="-L$DEVKITPRO/libctru/lib -L$DEVKITPRO/portlibs/3ds/lib"

# Also check for packages using pkg-config
export PKG_CONFIG_PATH="$DEVKITPRO/portlibs/3ds/lib/pkgconfig"
export PKG_CONFIG_LIBDIR="$DEVKITPRO/portlibs/3ds/lib/pkgconfig"

mkdir -p build-3ds
cd build-3ds

../configure \
  --host=arm-none-eabi \
  --enable-client=no \
  --enable-server \
  --disable-shared \
  --enable-static \
  --disable-nls \
  --disable-icu \
  --enable-sys-tolua-cmd="$(pwd)/build-native/dependencies/tolua-5.2/src/bin/tolua" \
  --enable-ack-legacy \
  --disable-ipv6 \
  --without-iconv \
  --enable-fcdb=no \
  --enable-fcmp=no
