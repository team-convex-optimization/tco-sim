from gym.envs.registration import register

register(
    id='tco_sim-v0',
    entry_point='gym_tco.envs:TcoEnv',
)