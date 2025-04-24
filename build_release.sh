#!/bin/sh

BASEDIR="$PWD"
SRCDIR="$BASEDIR/src"

sh $PWD/compile_imgui.sh

SOURCES="$SRCDIR/linux_2pacwav2.cpp"
INCLUDE_DIRS="-I$BASEDIR/3rd_party/SDL2 -I$BASEDIR/3rd_party/SDL2/include -I$BASEDIR/3rd_party/imgui -I$BASEDIR/3rd_party -I$BASEDIR/3rd_party/id3v2lib/include -I$BASEDIR/3rd_party/taglib/include -I$BASEDIR/3rd_party/taglib/include/mpeg/id3v2/ -I$BASEDIR/3rd_party/taglib/include/mpeg/" 
COMP_FLAGS="-O3 -gdwarf"
EXE_NAME="2w2"
LINK_FLAGS="-o $EXE_NAME"

SDL_DIR="$BASEDIR/3rd_party/SDL2"
CODEC_DIR="$BASEDIR/3rd_party/codecs"

LIB_DIRS="-L$BASEDIR/3rd_party/SDL2/lib -L$BASEDIR/3rd_party/taglib/lib"
OBJ_FILES="$BASEDIR/lib/imgui*.o"
#LINK_LIBS="$SDL_DIR/lib/libSDL2.a -lm -lGL $SDL_DIR/lib/libSDL2_mixer.a $BASEDIR/lib/libnuklear_sdl2_gl2.a -l:libtag.a -l:libz.a -l:libbsd.a"
LINK_LIBS="$SDL_DIR/lib/libSDL2.a -static-libstdc++ -static-libgcc -lm -lGL $SDL_DIR/lib/libSDL2_mixer.a -l:libtag.a -l:libz.a -l:libbsd.a"
WARNINGS="-Wall -Wpedantic -Wextra -Wno-c99-extensions -Wno-unused-parameter -Wno-pointer-arith -Wno-unused-variable -Wno-unused-function -Wno-unused-but-set-variable -Wno-write-strings -Wno-format"
DEFINES="-D_2PACWAV_RELEASE=1 -D_2PACWAV_LINUX=1 -DPAC_SAMPLE_RATE=48000"

WORKDIR="$BASEDIR/build/linux_x64_release"

CMDLINE="clang++ $DEFINES $INCLUDE_DIRS $LIB_DIRS $WARNINGS $COMP_FLAGS $SOURCES $LINK_LIBS $LINK_FLAGS $OBJ_FILES" 

mkdir -p $WORKDIR; cd $WORKDIR
echo $CMDLINE; $CMDLINE
cd ..
