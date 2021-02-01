from gym.envs.registration import register

register(
    id='tco-sim-v0',
    entry_point='gym_tco_sim.envs:TcoSimEnv',
)
