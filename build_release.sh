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

INCLUDE_DIRS="-I$BASEDIR/3rd_party/SDL2/posix \
        -I$BASEDIR/3rd_party/SDL2/posix/include \
        -I$BASEDIR/3rd_party/imgui \
        -I$BASEDIR/3rd_party"

COMP_FLAGS="-O2 -gdwarf"
EXE_NAME="2w"
LINK_FLAGS="-o $EXE_NAME"

SDL_DIR="$BASEDIR/3rd_party/SDL2"

LIB_DIRS=""
OBJ_FILES="$BASEDIR/build/lib/imgui*.o"

LINK_LIBS="-lm \
        $BASEDIR/3rd_party/SDL2/posix/lib/libSDL2.a \
        -static-libstdc++ \
        -static-libgcc \
        -lGL \
        -lfontconfig \
        -lavcodec \
        -lavformat \
        -lavcodec \
        -lavutil \
        -lswresample \
        -lpthread"

WARNINGS="-Wall -Wpedantic -Wextra \
        -Wno-unused-parameter -Wno-pointer-arith \
        -Wno-unused-variable -Wno-unused-function \
        -Wno-unused-but-set-variable -Wno-write-strings -Wno-format\
        -Wno-string-concatenation -Wno-c99-extensions"

DEFINES="-D_2PACWAV_RELEASE=1 -D_2PACWAV_LINUX=1 -DPAC_SAMPLE_RATE=48000"

WORKDIR="$BASEDIR/build/linux_x64_release"

#NOTE: Optimized builds of this program have very strange bugs when built with gcc, so clang it is
CMDLINE="clang++ $DEFINES \
        $WARNINGS \
        $INCLUDE_DIRS \
        $LIB_DIRS \
        $COMP_FLAGS \
        $SOURCES \
        $LINK_LIBS \
        $LINK_FLAGS \
        $OBJ_FILES" 

mkdir -p $WORKDIR; cd $WORKDIR
echo $CMDLINE; $CMDLINE
cd ..
