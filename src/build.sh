#!/bin/sh

#echo "build started @ $(date)\n"

BASEDIR="$PWD/.."

SOURCES="$PWD/linux_2pacwav2.cpp"

INCLUDE_DIRS="-I$BASEDIR/3rd_party/SDL2 \
        -I$BASEDIR/3rd_party/SDL2/include \
        -I$BASEDIR/3rd_party/imgui \
        -I$BASEDIR/3rd_party"

#        "-I$BASEDIR/3rd_party/taglib/include \
#        -I$BASEDIR/3rd_party/taglib/include/mpeg/id3v2/ \
#        -I$BASEDIR/3rd_party/taglib/include/mpeg/" 

COMP_FLAGS="-O0 -gdwarf"
EXE_NAME="2w"
LINK_FLAGS="-o $EXE_NAME"

#SDL_DIR="$BASEDIR/3rd_party/SDL2"
#CODEC_DIR="$BASEDIR/3rd_party/codecs"

LIB_DIRS=""
#-L$BASEDIR/3rd_party/SDL2/lib \
#        -L$BASEDIR/3rd_party/taglib/lib \
#        -L$BASEDIR/3d_party/codecs/lib"

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
        -lpthread \
        -lfontconfig"

#taglib
        #$BASEDIR/3rd_party/taglib/lib/libtag.a \
        #-l:libz.a \
#sdl mixer
#        $SDL_DIR/lib/libSDL2_mixer.a \
#        $CODEC_DIR/lib/libopusfile.a \
#        $CODEC_DIR/lib/libopus.a \
#        $CODEC_DIR/lib/libvorbisfile.a \
#        $CODEC_DIR/lib/libvorbis.a \
#        $CODEC_DIR/lib/libwavpack.a \
#        $CODEC_DIR/lib/libxmp.a \
#        $CODEC_DIR/lib/libogg.a \

OBJ_FILES="$BASEDIR/build/lib/imgui*.o"

WARNINGS="-Wall -Wpedantic -Wextra -Wno-unused-parameter \
        -Wno-pointer-arith -Wno-unused-variable \
        -Wno-unused-function -Wno-unused-but-set-variable \
        -Wno-write-strings -Wno-stringop-truncation -Wno-format-truncation \
        -Wno-unused-function"

DEFINES="-D_2PACWAV_DEBUG=1 \
        -D_2PACWAV_LINUX=1 \
        -DPAC_SAMPLE_RATE=48000 \
        -DPAC_SPECTRUM_ENABLED=0 \
        -DPACMXR_DEBUG=1"

WORKDIR="$BASEDIR/build/linux_x64_debug"

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
