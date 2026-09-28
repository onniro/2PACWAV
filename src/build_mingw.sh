#!/bin/sh

#(this script isn't really meant to be used for now as the windows port is unstable)
#you must make this variable point to ffmpeg source on your own computer
FFMPEG_DIR="$HOME/source/software/ffmpeg"

BUILD_DIR="$PWD/../build/mingw32_x64_debug"
BASEDIR="$PWD/.."

SOURCES="$PWD/linux_2pacwav2.cpp"
INCLUDE_DIRS="-I$BASEDIR/3rd_party/SDL2/mingw32 \
        -I$BASEDIR/3rd_party/SDL2/mingw32/include \
        -I$BASEDIR/3rd_party/imgui \
        -I$BASEDIR/3rd_party\
        -I$FFMPEG_DIR"

COMP_FLAGS="-O0 -mwindows"
EXE_NAME="2w.exe"
LINK_FLAGS="-o $EXE_NAME"

LIB_DIRS="-L$BASEDIR/3rd_party/ffmpeg/lib -L$BASEDIR/3rd_party/SDL2/mingw32/lib"
LINK_LIBS="-lm \
        -lmingw32 \
        -Wl,--whole-archive \
        -l:SDL2_mingw32.dll \
        -Wl,--no-whole-archive \
        -static-libstdc++ \
        -static-libgcc \
        -l:swresample-7.dll \
        -l:avcodec-63.dll \
        -l:avformat-63.dll \
        -l:avutil-61.dll \
        -l:swscale-10.dll \
        -luser32 \
        -lgdi32 \
        -loleaut32 \
        -limm32 \
        -lsetupapi \
        -lcfgmgr32 \
        -lws2_32 \
        -lole32 \
        -ladvapi32 \
        -lshell32 \
        -lkernel32 \
        -lopengl32 \
        -lcomctl32 \
        -lversion \
        -lwinmm \
        -lshlwapi \
        -static -lwinpthread \
        "

OBJ_FILES="$BASEDIR/build/lib_mingw32/imgui*.o"

WARNINGS="-Wall -Wpedantic -Wextra -Wno-unused-parameter \
        -Wno-pointer-arith -Wno-unused-variable \
        -Wno-unused-function -Wno-unused-but-set-variable \
        -Wno-write-strings -Wno-format \
        -Wno-unused-function -Wno-strict-aliasing"

DEFINES="-D_2PACWAV_DEBUG=1 \
        -D_2PACWAV_LINUX=1 \
        -DPAC_SAMPLE_RATE=48000 \
        -DPAC_SPECTRUM_ENABLED=0 \
        -DPACMXR_DEBUG=1 \
        -DPACMXR_MINGW32=1
        -D_2PACWAV_MINGW32=1
        -DRO_UTIL_W32=1 \
        -D_2PACWAV_DISABLE_FONTCONFIG=1 \
        "

CMDLINE="x86_64-w64-mingw32-g++ $DEFINES \
        $INCLUDE_DIRS \
        $LIB_DIRS \
        $WARNINGS \
        $COMP_FLAGS \
        $SOURCES \
        $OBJ_FILES \
        $LINK_FLAGS \
        $LINK_LIBS \
        "

THIS_DIR=$PWD
mkdir -p $BUILD_DIR; cd $BUILD_DIR
echo $CMDLINE; $CMDLINE
cd $THIS_DIR
