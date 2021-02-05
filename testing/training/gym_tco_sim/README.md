# OpenAI Gym Environment for TCO Godot Simulator
## Dependencies
- gym
- numpy
- posix_ipc
- cv2

## Setup
1. Export simulator into ./sim/tco_sim.bin
2. Run ```python3 setup.py install``` to install all the dependencies
3. Run ```pip3 install -e .``` to install the environment and make it accessible via ID
   ```gym_tco_sim:tco-sim-v0```
