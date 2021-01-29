import gym
from gym import error, spaces, utils
from gym.utils import seeding

class TcoEnv(gym.Env):
  metadata = {'render.modes': ['human']}
  iteration = 0
  done = False
  def __init__(self):
    self.done = False
    # launch the godot sim
  def step(self, action):
    ...
  def reset(self):
    ...
  def render(self, mode='human', close=False):
    ...
