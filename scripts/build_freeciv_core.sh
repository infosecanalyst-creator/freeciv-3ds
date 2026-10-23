#!/bin/bash
# Builds the upstream Freeciv engine (server, rulesets, AI) as static ARM11
# libraries for the 3DS client to link against.
#
#   scripts/build_freeciv_core.sh            (run from the repo root)
#
# Produces freeciv/build-3ds/... (the paths 3ds_port/Makefile links) and fills
# 3ds_port/romfs/data with the server data the engine reads at run time.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
FREECIV_COMMIT="${FREECIV_COMMIT:-691fe4de54ab3c2594a241a4954067b5c6913400}"   # what port-patches/freeciv-core.patch was made against
cd "$ROOT"

# 1. upstream source at the pinned commit, with the port's glue patch
if [ ! -d freeciv/.git ]; then
	git init -q freeciv
	git -C freeciv remote add origin https://github.com/freeciv/freeciv
fi
git -C freeciv fetch -q --depth 1 origin "$FREECIV_COMMIT"
git -C freeciv checkout -q --force FETCH_HEAD
git -C freeciv clean -qfdx
git -C freeciv apply "$ROOT/port-patches/freeciv-core.patch"

cd freeciv
./autogen.sh --no-configure-run

# 2. a native tolua (the code generator Freeciv runs at build time)
mkdir -p build-native
(
	cd build-native
	../configure --enable-ack-legacy --enable-client=no --enable-server --disable-nls --disable-icu \
		--enable-fcdb=no --enable-fcmp=no --without-readline
	make -s -C dependencies/lua-5.5
	make -s -C dependencies/tolua-5.2
)
TOLUA="$PWD/build-native/dependencies/tolua-5.2/src/bin/tolua"
test -x "$TOLUA"

# 3. the engine for the 3DS
: "${DEVKITPRO:=/opt/devkitpro}" "${DEVKITARM:=$DEVKITPRO/devkitARM}"
export PATH="$DEVKITARM/bin:$DEVKITPRO/tools/bin:$PATH"
export CC=arm-none-eabi-gcc CXX=arm-none-eabi-g++ AR=arm-none-eabi-ar RANLIB=arm-none-eabi-ranlib
export CFLAGS="-march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft -O3 -mword-relocations -fomit-frame-pointer -ffast-math -fno-short-enums"
export CXXFLAGS="$CFLAGS"
export CPPFLAGS="-I$DEVKITPRO/libctru/include -I$DEVKITPRO/portlibs/3ds/include -D__3DS__"
export LDFLAGS="-L$DEVKITPRO/libctru/lib -L$DEVKITPRO/portlibs/3ds/lib"
export PKG_CONFIG_PATH="$DEVKITPRO/portlibs/3ds/lib/pkgconfig" PKG_CONFIG_LIBDIR="$DEVKITPRO/portlibs/3ds/lib/pkgconfig"
mkdir -p build-3ds
(
	cd build-3ds
	../configure --host=arm-none-eabi --enable-client=no --enable-server \
		--disable-shared --enable-static --disable-nls --disable-icu \
		--enable-sys-tolua-cmd="$TOLUA" --enable-ack-legacy --disable-ipv6 \
		--without-iconv --enable-fcdb=no --enable-fcmp=no --without-readline
	make -j"$(nproc)"
)

# 4. server data for the RomFS: top-level data files plus the ruleset dirs
#    (no tilesets, sounds or music - the client brings its own art)
cd "$ROOT"
rm -rf 3ds_port/romfs/data && mkdir -p 3ds_port/romfs/data
find freeciv/data -maxdepth 1 -type f -exec cp {} 3ds_port/romfs/data/ \;
for d in alien buildings civ1 civ2 civ2civ3 classic default granularity multiplayer nation override sandbox stub wonders; do
	[ -d "freeciv/data/$d" ] && cp -r "freeciv/data/$d" 3ds_port/romfs/data/
done
echo "freeciv core built; romfs data: $(du -sh 3ds_port/romfs/data | cut -f1)"
