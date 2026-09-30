#!/bin/bash
# Builds the scripted screenshot runner (tools/smoke/shot) against a pinned mGBA.
# Needs cmake, ninja, gcc, libpng, zlib, liblzma, freetype. Takes a few minutes the first time.
set -e
cd "$(dirname "$0")"
MGBA_COMMIT=c3c8e5e813f245028de118a56734e1dc0f35ce2a
if [ ! -d .mgba ]; then
    git clone --quiet https://github.com/mgba-emu/mgba.git .mgba
    git -C .mgba checkout --quiet "$MGBA_COMMIT"
fi
if [ ! -f .mgba/build/libmgba.a ]; then
    cmake -S .mgba -B .mgba/build -G Ninja -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_QT=OFF -DBUILD_SDL=OFF -DBUILD_GL=OFF -DBUILD_GLES2=OFF -DBUILD_GLES3=OFF \
        -DUSE_FFMPEG=OFF -DUSE_LUA=OFF -DUSE_EDITLINE=OFF -DUSE_ELF=OFF -DUSE_SQLITE3=OFF \
        -DUSE_DISCORD_RPC=OFF -DUSE_LIBZIP=OFF -DUSE_MINIZIP=OFF -DUSE_EPOXY=OFF \
        -DBUILD_SHARED=OFF -DBUILD_STATIC=ON -DM_CORE_GB=OFF -DUSE_DEBUGGERS=OFF > /dev/null
    ninja -C .mgba/build > /dev/null
fi
# These defines must match the library build, or the core struct layout differs and it crashes
D="-DBUILD_STATIC -DENABLE_DEBUGGERS -DENABLE_DIRECTORIES -DENABLE_GDB_STUB -DENABLE_SCRIPTING -DENABLE_VFS -DENABLE_VFS_FD -DM_CORE_GBA -DUSE_FREETYPE -DUSE_LZMA -DUSE_MINIZIP -DUSE_PNG -DUSE_PTHREADS -DUSE_ZLIB -D_GNU_SOURCE"
gcc -O2 -flto $D shot.c -I.mgba/include -I.mgba/build/include -I.mgba/src -o shot \
    .mgba/build/libmgba.a -lpng -lz -lm -lpthread -llzma -lfreetype 2>&1 | grep -E " error" || true
test -x shot && echo "built tools/smoke/shot"
