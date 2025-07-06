# train_cartpole.py
import gym
from stable_baselines3 import PPO

# Create environment
env = gym.make("CartPole-v1")

# Create model
model = PPO("MlpPolicy", env, verbose=1)

# Train the model
model.learn(total_timesteps=10000)

# Save model
model.save("ppo_cartpole")

# Cleanup
env.close()
