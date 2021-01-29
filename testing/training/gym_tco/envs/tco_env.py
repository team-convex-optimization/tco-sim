import gym
from gym import error, spaces, utils
from gym.utils import seeding

import subprocess
import os
from time import sleep
import posix_ipc as pipc
from struct import Struct

############################
#  IPC stuff
############################
shmSim = pipc.SharedMemory("tco_shmem_sim")
semSim = pipc.Semaphore("tco_shmem_sem_sim")
shmCntrl = pipc.SharedMemory("tco_shmem_control")
semCntrl = pipc.Semaphore("tco_shmem_sem_control")

#strcut for Cntrl
chStruct = Struct('Bf') 
controlStruct = Struct('BB{}'.format("{}s".format(chStruct.size) * 16))

#struct for sim
simStruct = Struct('BBff')


"""
@brief a way to read from shmem struct
@param shm the shared memeory region (must be initialized)
@param sem the semaphore to shm
@param type the shmem region to read from (for sizes etc)
"""
def readShm(shm, sem, type='control'):
    data = []
    os.lseek(shm.fd, 0, os.SEEK_SET)
    sem.acquire()
    if (type == "control"):
        while True:
            buf = os.read(shm.fd, controlStruct.size)
            if len(buf) != controlStruct.size:
                break
            unpackL1 = controlStruct.unpack_from(buf)
            data.append(unpackL1[0])
            data.append(unpackL1[1])
            i = 0
            while i < 16:
                data.append(chStruct.unpack_from(unpackL1[i + 2]))
                i += 1
    elif(type == "sim"):
            buf = os.read(shm.fd, simStruct.size)
    sem.release()
    return data

"""
@brief a way to write to a shmem struct
@param shm the shared memeory region (must be initialized)
@param sem the semaphore to shm
@param data the data to write
@param type the shmem region to read from (for sizes etc)
"""
def writeShmem(shm, sem, data, type='control'):
    sem.acquire()
    if (type == 'control'):
        i = 0
        while i < 16:
            data[i + 2] = chStruct.pack(data[i + 2][0], data[i + 2][1])
            i += 1
        data = controlStruct.pack(*data)
        os.lseek(shm.fd, 0, os.SEEK_SET)
        bytesWritten = 0
        while True:
            bytesWritten += os.write(shm.fd, data[bytesWritten:])
            if bytesWritten >= len(data):
                break
            pass
    elif (type == 'sim'):
        data = simStruct.pack(*data)
        os.lseek(shm.fd, 0, os.SEEK_SET)
        bytesWritten = 0
        while True:
            bytesWritten += os.write(shm.fd, data[bytesWritten:])
            if bytesWritten >= len(data):
                break
    sem.release()

############################
# SHMEM State stuff
############################

def carStateNull():
    state = [1, 0]
    i = 0
    while i < 16:
        state.append(tuple([0, 0.0]))
        i += 1
    return state

def simStateReset():
    return [0,0,0.0,0.0]

############################
# GYM ENV STUFF
############################

class TcoEnv(gym.Env):
  metadata = {'render.modes': ['human']}
  iteration = 0
  done = False
  carData = carStateNull()
  simData = simStateReset()

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
    self.carData = carStateNull()
    self.simData = simStateReset()
    writeShmem(shmSim, semSim, self.simData, 'sim')
    writeShmem(shmCntrl, semCntrl, self.carData, 'control')

  def render(self, mode='human', close=False):
    ...

