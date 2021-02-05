import gym
from gym import error, spaces, utils
from gym.utils import seeding
import numpy as np
import cv2

import posix_ipc as pipc
from struct import Struct
import os
import time

sim_width = 32
sim_height = 18
shmem_training_struct = Struct('=BB4BBf3f{}B'.format(sim_width * sim_height))
field_uint8 = Struct('=B')
field_ch = Struct('Bf')
car_state_null = shmem_training_struct.unpack_from(bytes('\x00' * shmem_training_struct.size, 'utf-8'))

training_shm = pipc.SharedMemory("tco_shmem_training")
training_sem = pipc.Semaphore("tco_shmem_sem_training")
control_shm = pipc.SharedMemory("tco_shmem_control")
control_sem = pipc.Semaphore("tco_shmem_sem_control")

def env_step():
  shmem_training_state_set(1)
  while shmem_training_state_get() != 0:
    time.sleep(0.01)

def env_reset():
  shmem_training_state_set(2)
  while shmem_training_state_get() != 0:
    time.sleep(0.01)

def shmem_control_throttle_set(val):
  if val > 1:
    val = 1
  elif val < 0:
    val = 0
  # Set memory valid bit
  os.lseek(control_shm.fd, 0, os.SEEK_SET)
  control_sem.acquire()
  os.write(control_shm.fd, field_uint8.pack(1))
  control_sem.release()
  # Set channel val and active flag
  os.lseek(control_shm.fd, 2, os.SEEK_SET)
  control_sem.acquire()
  os.write(control_shm.fd, field_ch.pack(1, val))
  control_sem.release()

def shmem_control_steer_set(val):
  if val > 1:
    val = 1
  elif val < 0:
    val = 0
  # Set memory valid bit
  os.lseek(control_shm.fd, 0, os.SEEK_SET)
  control_sem.acquire()
  os.write(control_shm.fd, field_uint8.pack(1))
  control_sem.release()
  # Set channel val and active flag
  os.lseek(control_shm.fd, 2, os.SEEK_SET)
  control_sem.acquire()
  os.write(control_shm.fd, field_ch.pack(1, val))
  control_sem.release()

def shmem_training_read():
  data = []
  os.lseek(training_shm.fd, 0, os.SEEK_SET)
  training_sem.acquire()
  while True:
      buf = os.read(training_shm.fd, shmem_training_struct.size)
      if len(buf) != shmem_training_struct.size:
          break
      unpacked = shmem_training_struct.unpack_from(buf)
      data.append(unpacked)
  training_sem.release()
  if data[0] == 0:
    return car_state_null
  else:
    return data

def shmem_training_state_set(new_state):
  os.lseek(training_shm.fd, 0, os.SEEK_SET)
  training_sem.acquire()
  # Set memory valid flag
  os.write(training_shm.fd, field_uint8.pack(1))
  # Set memory valid flag
  os.write(training_shm.fd, field_uint8.pack(new_state))
  training_sem.release()

def shmem_training_state_get():
  os.lseek(training_shm.fd, 0, os.SEEK_SET)
  training_sem.acquire()
  valid = os.read(training_shm.fd, 1)
  state = os.read(training_shm.fd, 1)
  training_sem.release()
  
  valid = field_uint8.unpack_from(valid)[0]
  state = field_uint8.unpack_from(state)[0]

  if valid == 0:
    return 0
  else:
    return state

# Small test to see if sim responds correctly to values in shared memory
def main():
  training_data = shmem_training_read()
  shmem_control_steer_set(0.8)
  shmem_control_throttle_set(0.55)

  env_reset()
  i = 0
  while i < 1000:
    env_step()
    i += 1

class TcoSimEnv(gym.Env):
  """The main OpenAI Gym class. It encapsulates an environment with
  arbitrary behind-the-scenes dynamics. An environment can be
  partially or fully observed.
  The main API methods that users of this class need to know are:
      step
      reset
      render
      close
      seed
  And set the following attributes:
      action_space: The Space object corresponding to valid actions
      observation_space: The Space object corresponding to valid observations
      reward_range: A tuple corresponding to the min and max possible rewards
  Note: a default reward range set to [-inf,+inf] already exists. Set it if you want a narrower range.
  The methods are accessed publicly as "step", "reset", etc...
  """
  # Set this in SOME subclasses
  metadata = {'render.modes': ['rgb_array', 'human']}
  reward_range = (-float('inf'), float('inf'))
  spec = None

  # Set these in ALL subclasses
  # 2D continuous action space: first dimension (thrtottle frac), second dimension (steer frac)
  action_space = spaces.Box(np.array([0,0]),np.array([1,1]),dtype=np.float32)
  observation_space = spaces.Box(0, 255, [sim_height * sim_width])

  def step(self, action):
    """Run one timestep of the environment's dynamics. When end of
    episode is reached, you are responsible for calling `reset()`
    to reset this environment's state.
    Accepts an action and returns a tuple (observation, reward, done, info).
    Args:
        action (object): an action provided by the agent
    Returns:
        observation (object): agent's observation of the current environment
        reward (float) : amount of reward returned after previous action
        done (bool): whether the episode has ended, in which case further step() calls will return undefined results
        info (dict): contains auxiliary diagnostic information (helpful for debugging, and sometimes learning)
    """
    shmem_control_throttle_set(action[0])
    shmem_control_steer_set(action[1])
    env_step()

    observation = shmem_training_read()
    wheels_off_track = np.array(observation[0][2:6], dtype=np.bool)
    drifting = observation[0][7] == 1
    speed = observation[0][8]
    pos = observation[0][9:11]
    video = np.array(observation[0][11:], dtype=np.uint8)

    done = drifting or (np.sum(wheels_off_track) > 2)
    return video, 1.0, done, {}

  def reset(self):
    """Resets the environment to an initial state and returns an initial
    observation.
    Note that this function should not reset the environment's random
    number generator(s); random variables in the environment's state should
    be sampled independently between multiple calls to `reset()`. In other
    words, each call of `reset()` should yield an environment suitable for
    a new episode, independent of previous episodes.
    Returns:
        observation (object): the initial observation.
    """
    env_reset()
    observation = shmem_training_read()
    video = np.array(observation[0][11:], dtype=np.uint8)
    return video

  def render(self, mode='human'):
    """Renders the environment.
    The set of supported modes varies per environment. (And some
    environments do not support rendering at all.) By convention,
    if mode is:
    - human: render to the current display or terminal and
      return nothing. Usually for human consumption.
    - rgb_array: Return an numpy.ndarray with shape (x, y, 3),
      representing RGB values for an x-by-y pixel image, suitable
      for turning into a video.
    - ansi: Return a string (str) or StringIO.StringIO containing a
      terminal-style text representation. The text can include newlines
      and ANSI escape sequences (e.g. for colors).
    Note:
        Make sure that your class's metadata 'render.modes' key includes
          the list of supported modes. It's recommended to call super()
          in implementations to use the functionality of this method.
    Args:
        mode (str): the mode to render with
    Example:
    class MyEnv(Env):
        metadata = {'render.modes': ['human', 'rgb_array']}
        def render(self, mode='human'):
            if mode == 'rgb_array':
                return np.array(...) # return RGB frame suitable for video
            elif mode == 'human':
                ... # pop up a window and render
            else:
                super(MyEnv, self).render(mode=mode) # just raise an exception
    """
    observation = shmem_training_read()
    video = np.array(observation[0][11:], dtype=np.uint8)
    video = np.reshape(video, (sim_height,sim_width))
    video = cv2.resize(video, (sim_width * 20, sim_height * 20), interpolation=cv2.INTER_NEAREST_EXACT)
    if mode == 'rgb_array':
      return video
    elif mode == 'human':
      cv2.imshow('frame', video)
      cv2.waitKey(1) # 'imshow' wont work without waiting for keyboard
      return
    else:
      raise NotImplementedError

  def close(self):
    """Override close in your subclass to perform any necessary cleanup.
    Environments will automatically close() themselves when
    garbage collected or when the program exits.
    """
    pass

if __name__ == "__main__":
    main()
