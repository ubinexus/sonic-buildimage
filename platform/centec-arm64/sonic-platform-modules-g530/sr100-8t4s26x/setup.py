#!/usr/bin/env python

import os
import sys
from setuptools import setup
os.listdir

setup(
   name='sonic_platform',
   version='1.0',
   description='Module to initialize centec g530-sr100-8t4s26x platforms',
   
   packages=['sonic_platform'],
   package_dir={'sonic_platform': 'sonic_platform'},
)

