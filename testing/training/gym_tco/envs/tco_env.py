import gym
from gym import error, spaces, utils
from gym.utils import seeding

import subprocess
from time import sleep

class TcoEnv(gym.Env):
  metadata = {'render.modes': ['human']}
  iteration = 0
  done = False

  def __init__(self):
    self.done = False
    subprocess.Popen('./sim/sim.x86_64')
    sleep(5)
    print("sim launched")
    # launch the godot sim

  def step(self, action):
    ...

  def reset(self):
    self.iteration += 1
    #find a way to reset the sim!

  def render(self, mode='human', close=False):
    ...
