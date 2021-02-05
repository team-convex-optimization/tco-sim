#! /bin/bash

rm -r ./build/*
mkdir -p build
cd ../../godot_project
godot3 --export default_linux ../testing/training/build/tco_sim.x86_64
