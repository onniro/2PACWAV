#!/bin/sh

#echo "build started @ $(date)\n"

BASEDIR="$PWD/.."

SOURCES="$PWD/linux_2pacwav2.cpp"

INCLUDE_DIRS="-I$BASEDIR/3rd_party/SDL2/posix \
        -I$BASEDIR/3rd_party/SDL2/posix/include \
        -I$BASEDIR/3rd_party/imgui \
        -I$BASEDIR/3rd_party"
        #-I$HOME/source/software/ffmpeg

COMP_FLAGS="-O0 -gdwarf"
#LINK_FLAGS="-Wl,-rpath,\$ORIGIN/../../3rd_party/ffmpeg/lib/ -o 2w"
LINK_FLAGS="-o 2w"

#SDL_DIR="$BASEDIR/3rd_party/SDL2"
#CODEC_DIR="$BASEDIR/3rd_party/codecs"

#LIB_DIRS="-L$BASEDIR/3rd_party/ffmpeg/lib"
LIB_DIRS=""
 
LINK_LIBS="-lm \
        $PWD/../3rd_party/SDL2/posix/lib/libSDL2.a \
        -static-libstdc++ \
        -static-libgcc \
        -lGL \
        -lpthread \
        -lfontconfig \
        -lswresample \
        -lavcodec \
        -lavformat \
        -lavutil \
        -lswscale"

#LINK_LIBS="-lm \
#        $PWD/../3rd_party/SDL2/posix/lib/libSDL2.a \
#        -static-libstdc++ \
#        -static-libgcc \
#        -lGL \
#        -lavcodec \
#        -lavformat \
#        -lavcodec \
#        -lavutil \
#        -lswresample \
#        -lpthread \
#        -lfontconfig"

OBJ_FILES="$BASEDIR/build/lib/imgui*.o"

WARNINGS="-Wall -Wpedantic -Wextra -Wno-unused-parameter \
        -Wno-pointer-arith -Wno-unused-variable \
        -Wno-unused-function -Wno-unused-but-set-variable \
        -Wno-write-strings -Wno-string-concatenation \
        -Wno-unused-function -Wno-strict-aliasing \
        -Wno-c99-extensions -Wno-c++17-attribute-extensions"

DEFINES="-D_2PACWAV_DEBUG=1 \
        -D_2PACWAV_LINUX=1 \
        -DPAC_SAMPLE_RATE=48000 \
        -DPAC_SPECTRUM_ENABLED=0 \
        -DPACMXR_DEBUG=1"

WORKDIR="$BASEDIR/build/linux_x64_debug"

#NOTE: Optimized builds of this program have very strange bugs when built with gcc, so clang it is
CMDLINE="clang++ $DEFINES \
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
