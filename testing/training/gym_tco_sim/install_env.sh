#! /bin/bash
# Do not run this as root

rm -r ./build/*
mkdir -p build

# Export simulator
pushd ../../../godot_project
godot3 --export default_linux ../testing/training/gym_tco_sim/build/tco_sim.x86_64
popd

# Install dependencies for gym env
sudo python3 ./setup.py install

# Install the custom gym env
# This requires root permissions
sudo pip3 install -e .
