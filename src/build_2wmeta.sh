
#!/bin/sh

#echo "build started @ $(date)\n"

BASEDIR="$PWD/.."

SOURCES="$PWD/2wmeta.c"

COMP_FLAGS="-O2 -gdwarf"
EXE_NAME="2wmeta"
LINK_FLAGS="-o $EXE_NAME"

LIB_DIRS=""

LINK_LIBS="-lm \
        -static-libgcc \
        -lavcodec \
        -lavformat \
        -lavcodec \
        -lavutil \
        -lswresample"

WARNINGS="-Wall -Wpedantic -Wextra -Wno-unused-parameter \
        -Wno-pointer-arith -Wno-unused-variable \
        -Wno-unused-function -Wno-unused-but-set-variable \
        -Wno-write-strings -Wno-string-concatenation \
        -Wno-unused-function -Wno-strict-aliasing"

DEFINES="-D_2PACWAV_DEBUG=1 \
        -D_2PACWAV_LINUX=1 \
        -DPAC_SAMPLE_RATE=48000 \
        -DPAC_SPECTRUM_ENABLED=0 \
        -DPACMXR_DEBUG=1 \
        -DPACMXR_ONLY_INCLUDE_METADATA=1"

WORKDIR="$BASEDIR/build/linux_x64_debug"

CMDLINE="clang $DEFINES \
        $INCLUDE_DIRS \
        $LIB_DIRS \
        $WARNINGS \
        $COMP_FLAGS \
        $SOURCES \
        $LINK_LIBS \
        $LINK_FLAGS"

mkdir -p $WORKDIR; cd $WORKDIR
echo $CMDLINE; $CMDLINE
cd ..
