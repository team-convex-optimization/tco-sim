# -*- coding: utf-8 -*-
from setuptools import setup, find_packages

try:
    long_description = open("README.md").read()
except IOError:
    long_description = ""

setup(
    name="gym_tco_sim",
    version="0",
    description="OpenAI gym environment which uses the godot simulator",
    author="Team Convex Optimization",
    packages=find_packages(),
    install_requires=['gym', 'numpy', 'posix-ipc', 'opencv-python'],
    long_description=long_description,
    classifiers=[
        "Programming Language :: Python",
        "Programming Language :: Python :: 3.8",
    ]
)
