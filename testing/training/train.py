import gym
import tco_gym

import numpy as np
import tensorflow as tf

# Globals
env = None
states, actions, rewards = [], [], []

# Hyper Parameters
alpha = 1e-4 # Learning rate
gamma = 0.99 # rate of reward discounting (closer to 0 means less concerned with future)

###
# CONV NETWORK TIME #
###


###
# END CONV NETWORK TIME #
###


# Utilities
def setup():
    global env 
    env = gym.make('tco_sim-v0')

# Entry point
if __name__ == "__main__":
    setup()