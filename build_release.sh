#!/bin/sh

BASEDIR="$PWD"
SRCDIR="$BASEDIR/src"

COMPILE_IMGUI=1

for arg in "$@"; do
    if [ "$arg" = "-noobj" ]; then
        COMPILE_IMGUI=0
    else
        echo "unrecognized option: $arg"
        exit 1
    fi
done

echo "build started @ $(date)"

if [ $COMPILE_IMGUI -ne 0 ]; then
    sh $PWD/compile_imgui.sh
fi 

SOURCES="$SRCDIR/linux_2pacwav2.cpp"

INCLUDE_DIRS="-I$BASEDIR/3rd_party/SDL2 \
        -I$BASEDIR/3rd_party/SDL2/include \
        -I$BASEDIR/3rd_party/imgui \
        -I$BASEDIR/3rd_party \
        -I$BASEDIR/3rd_party/id3v2lib/include"

#        -I$BASEDIR/3rd_party/taglib/include \
#        -I$BASEDIR/3rd_party/taglib/include/mpeg/id3v2/ \
#        -I$BASEDIR/3rd_party/taglib/include/mpeg/" 

COMP_FLAGS="-O3 -gdwarf"
EXE_NAME="2w"
LINK_FLAGS="-o $EXE_NAME"

SDL_DIR="$BASEDIR/3rd_party/SDL2"
CODEC_DIR="$BASEDIR/3rd_party/codecs"

LIB_DIRS="-L$BASEDIR/3rd_party/SDL2/lib -L$BASEDIR/3rd_party/taglib/lib"
OBJ_FILES="$BASEDIR/build/lib/imgui*.o"

LINK_LIBS="-lm \
        -lSDL2 \
        -static-libstdc++ \
        -static-libgcc \
        -lGL \
        -lavcodec \
        -lavformat \
        -lavcodec \
        -lavutil \
        -lswresample \
        -lpthread"

#taglib
#        $BASEDIR/3rd_party/taglib/lib/libtag.a \
#        -l:libz.a \
#sdl mixer
#        $SDL_DIR/lib/libSDL2_mixer.a \
#        $CODEC_DIR/lib/libopusfile.a \
#        $CODEC_DIR/lib/libopus.a \
#        $CODEC_DIR/lib/libvorbisfile.a \
#        $CODEC_DIR/lib/libvorbis.a \
#        $CODEC_DIR/lib/libwavpack.a \
#        $CODEC_DIR/lib/libxmp.a \
#        $CODEC_DIR/lib/libogg.a \

WARNINGS="-Wall -Wpedantic -Wextra \
        -Wno-unused-parameter -Wno-pointer-arith \
        -Wno-unused-variable -Wno-unused-function \
        -Wno-unused-but-set-variable -Wno-write-strings -Wno-format\
        -Wno-stringop-truncation"

DEFINES="-D_2PACWAV_RELEASE=1 -D_2PACWAV_LINUX=1 -DPAC_SAMPLE_RATE=48000"

WORKDIR="$BASEDIR/build/linux_x64_release"

#CMDLINE="clang++ $DEFINES \
CMDLINE="c++ $DEFINES \
        $INCLUDE_DIRS \
        $LIB_DIRS \
        $WARNINGS \
        $COMP_FLAGS \
        $SOURCES \
        $LINK_LIBS \
        $LINK_FLAGS \
        $OBJ_FILES" 

mkdir -p $WORKDIR; cd $WORKDIR
echo $CMDLINE; $CMDLINE
cd ..
