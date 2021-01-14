#!/bin/bash

pushd lib/tco_libd
./clean.sh
popd

rm -r ./build/*
rm -r ../../godot_project/lib_native/libshmemaccess.so
