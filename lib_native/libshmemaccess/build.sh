#!/bin/bash

mkdir -p build

pushd lib/tco_libd
./build.sh
mv -f build/*.o ../../build
popd

pushd build
clang \
    -Wall \
    -std=c99 \
    -fPIC \
    -c \
    -I ../../../godot_headers \
    -I ../lib/tco_libd/include \
    -I ../lib/tco_shmem \
    ../code/shmem_access.c

popd

clang \
    -Wall \
    -std=c99 \
    -rdynamic \
    -shared \
    -l rt \
    build/shmem_access.o \
    -o ../../godot_project/lib_native/libshmemaccess.so