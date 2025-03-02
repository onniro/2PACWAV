#!/bin/sh

BASEDIR="$PWD/.."

SOURCES="$PWD/linux_2pacwav.c $PWD/2pacwav.c"
INCLUDE_DIRS="-I$BASEDIR/3rd_party/SDL2 -I$BASEDIR/3rd_party/SDL2/include -I$BASEDIR/3rd_party -I$BASEDIR/3rd_party/nuklear" 
#COMP_FLAGS="-O0 -gdwarf"
COMP_FLAGS="-O0 -gdwarf"
LINK_FLAGS="-o 2pacwav"

SDL_DIR="$BASEDIR/3rd_party/SDL2"
CODEC_DIR="$BASEDIR/3rd_party/codecs"

LIB_DIRS="-L$BASEDIR/3rd_party/SDL2/lib -L$BASEDIR/3rd_party/codecs/lib"
OBJ_FILES=""
LINK_LIBS="$SDL_DIR/lib/libSDL2.a -lm -lGL $SDL_DIR/lib/libSDL2_mixer.a $BASEDIR/lib/libnuklear_sdl2_gl2.a -lbsd"
WARNINGS="-Wall -Wpedantic -Wno-pointer-arith -Wno-unused-variable -Wno-unused-function -Wno-unused-but-set-variable -Wno-write-strings -Wno-format"
DEFINES="-D_2PACWAV_DEBUG=1 -D_2PACWAV_LINUX=1"

WORKDIR="$BASEDIR/build/linux_x64_debug"

CMDLINE="gcc $DEFINES $INCLUDE_DIRS $LIB_DIRS $WARNINGS $COMP_FLAGS $SOURCES $LINK_LIBS $LINK_FLAGS $OBJ_FILES" 

mkdir -p $WORKDIR; cd $WORKDIR
echo $CMDLINE; $CMDLINE
cd ..
