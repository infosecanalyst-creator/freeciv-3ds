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
	# gcc 15 defaults to C23, where autoconf's stdbool probe wrongly fails and
	# Freeciv would #define bool as unsigned int, clashing with the client.
	ac_cv_header_stdbool_h=yes ../configure --host=arm-none-eabi --enable-client=no --enable-server \
		--disable-shared --enable-static --disable-nls --disable-icu \
		--enable-sys-tolua-cmd="$TOLUA" --enable-ack-legacy --disable-ipv6 \
		--without-iconv --enable-fcdb=no --enable-fcmp=no --without-readline \
			--without-libbz2 --without-liblzma --without-libzstd
	# -k: the standalone freeciv-server program cannot link for the 3DS (no
	# libctru start-up); only its libraries are wanted, checked below.
	make -k -j"$(nproc)" || true
)
missing=0
for lib in server/.libs/libfreeciv-srv.a server/advisors/.libs/libadvisors.a server/generator/.libs/libgenerator.a \
	server/ruleset/.libs/libruleload.a server/savegame/.libs/libsavegame.a server/scripting/.libs/libscripting_fcdb.a \
	server/scripting/.libs/libscripting_server.a ai/default/.libs/libdefaultai.a ai/classic/.libs/libclassicai.a \
	ai/tex/.libs/libtexai.a ai/.libs/libaicommon.a common/aicore/.libs/libaicore.a common/networking/.libs/libfcivnetwork.a \
	common/scriptcore/.libs/libscriptcore.a common/.libs/libfreeciv.a utility/.libs/libcivutility.a \
	dependencies/lua-5.5/src/.libs/liblua.a dependencies/tolua-5.2/src/lib/.libs/libtolua.a dependencies/cvercmp/.libs/libcvercmp.a; do
	[ -f "build-3ds/$lib" ] || { echo "missing: $lib"; missing=1; }
done
[ "$missing" = 0 ] || exit 1

# 4. server data for the RomFS: top-level data files plus the ruleset dirs
#    (no tilesets, sounds or music - the client brings its own art)
cd "$ROOT"
rm -rf 3ds_port/romfs/data && mkdir -p 3ds_port/romfs/data
find freeciv/data -maxdepth 1 -type f -exec cp {} 3ds_port/romfs/data/ \;
for d in alien buildings civ1 civ2 civ2civ3 classic default granularity multiplayer nation override sandbox stub wonders; do
	[ -d "freeciv/data/$d" ] && cp -r "freeciv/data/$d" 3ds_port/romfs/data/
done
echo "freeciv core built; romfs data: $(du -sh 3ds_port/romfs/data | cut -f1)"
