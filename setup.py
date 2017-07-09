#!/usr/bin/env python
#-*- coding:utf-8 -*-

from setuptools import setup, find_packages
import pip
import os

if os.name == 'nt':
    reqs = ['pbs']
else:
    reqs = ['sh']

setup(
    name = 'apng2webp',
    version = '0.1.1.dev0',
    author = 'Benny',
    author_email = 'Benny@GMX.it',
    url='https://github.com/Benny-/apng2webp',
    license='Public Domain',
    keywords = "webp webby apng converter image".split(),
    description='Convert apng animations to webp animations',
    packages = find_packages(),
    install_requires = reqs,
    setup_requires=['pytest-runner'],
    tests_require=['pytest'],
    classifiers = [
        "Operating System :: OS Independent",
        "Programming Language :: Python :: 2",
        "Programming Language :: Python :: 3",
        "Environment :: Console",
        "License :: Public Domain",
    ],
    entry_points= {
        'console_scripts': [
            'apng2webp=apng2webp.apng2webp:main',
        ],
    },
)

