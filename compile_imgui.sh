#!/bin/sh

SOURCES="$PWD/3rd_party/imgui/*.cpp"
INCLUDE_DIRS="-I$PWD/3rd_party/SDL2 -I$PWD/3rd_party/SDL2/include -I$PWD/3rd_party/imgui" 
COMP_FLAGS="-O3 -gdwarf"

mkdir -p $PWD/lib; cd $PWD/lib
clang++ $INCLUDE_DIRS $COMP_FLAGS -c $SOURCES
cd ..
