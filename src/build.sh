#!/bin/sh

BASEDIR="$PWD/.."

#SOURCES="$PWD/linux_2pacwav.cpp $PWD/2pacwav.cpp"
SOURCES="$PWD/linux_2pacwav.cpp"
INCLUDE_DIRS="-I$BASEDIR/3rd_party/SDL2 -I$BASEDIR/3rd_party/SDL2/include -I$BASEDIR/3rd_party -I$BASEDIR/3rd_party/nuklear -I$BASEDIR/3rd_party/id3v2lib/include -I$BASEDIR/3rd_party/taglib/include -I$BASEDIR/3rd_party/taglib/include/mpeg/id3v2/ -I$BASEDIR/3rd_party/taglib/include/mpeg/" 
COMP_FLAGS="-O0 -gdwarf"
EXE_NAME="2w"
LINK_FLAGS="-o $EXE_NAME"

SDL_DIR="$BASEDIR/3rd_party/SDL2"
CODEC_DIR="$BASEDIR/3rd_party/codecs"

LIB_DIRS="-L$BASEDIR/3rd_party/SDL2/lib -L$BASEDIR/3rd_party/codecs/lib -L$BASEDIR/3rd_party/taglib/lib -L$BASEDIR/3rd_party/id3v2lib/lib"
OBJ_FILES=""
LINK_LIBS="$SDL_DIR/lib/libSDL2.a -lm -lGL $SDL_DIR/lib/libSDL2_mixer.a $BASEDIR/lib/libnuklear_sdl2_gl2.a -l:libtag.a -l:libz.a -l:libbsd.a"
WARNINGS="-Wall -Wpedantic -Wextra -Wno-unused-parameter -Wno-pointer-arith -Wno-unused-variable -Wno-unused-function -Wno-unused-but-set-variable -Wno-write-strings -Wno-format -Wno-c99-extensions -Wno-gnu-imaginary-constant"
DEFINES="-D_2PACWAV_DEBUG=1 -D_2PACWAV_LINUX=1 -DPAC_SAMPLE_RATE=48000"

WORKDIR="$BASEDIR/build/linux_x64_debug"

#CMDLINE="g++ -std=c++14 $DEFINES $INCLUDE_DIRS $LIB_DIRS $WARNINGS $COMP_FLAGS $SOURCES $LINK_LIBS $LINK_FLAGS $OBJ_FILES" 
CMDLINE="clang $DEFINES $INCLUDE_DIRS $LIB_DIRS $WARNINGS $COMP_FLAGS $SOURCES $LINK_LIBS $LINK_FLAGS $OBJ_FILES" 

mkdir -p $WORKDIR; cd $WORKDIR
echo $CMDLINE; $CMDLINE
cd ..
