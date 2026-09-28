#!/bin/sh

SOURCES="$PWD/3rd_party/imgui/*.cpp"
INCLUDE_DIRS="-I$PWD/3rd_party/SDL2/posix \
        -I$PWD/3rd_party/SDL2/posix/include \
        -I$PWD/3rd_party/imgui" 
COMP_FLAGS="-O3 -gdwarf"

CMDLINE="clang++ $INCLUDE_DIRS $COMP_FLAGS -c $SOURCES"

mkdir -p $PWD/build/lib; cd $PWD/build/lib
echo $CMDLINE; 
$CMDLINE
cd ..
