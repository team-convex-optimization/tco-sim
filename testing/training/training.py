import gym
import time

env = gym.make('gym_tco_sim:tco-sim-v0')
for i_episode in range(1000):
    observation = env.reset()
    t = 0
    reward_cumulative = 0
    while 1:
        env.render()
        action = env.action_space.sample()
        observation, reward, done, info = env.step(action)
        reward_cumulative += reward
        if done:
            print("Episode finished after {} timesteps with {:.3f} average reward".format(t+1, reward_cumulative/(t+1)))
            t = 0
            reward_cumulative = 0
            break
        else:
            t += 1
env.close()
