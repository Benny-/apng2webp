#!/usr/bin/env python

from setuptools import setup, find_packages
import pip
import os

if os.name == 'nt':
    reqs = ['pbs']
else:
    reqs = ['sh']

setup(
    name = 'apng2webp',
    version = '0.0.2',
    author = 'Benny',
    author_email = 'Benny@GMX.it',
    url='',
    license='Public Domain',
    keywords = "webp webby apng converter image".split(),
    description='Convert apng animations to webp animations',
    packages = find_packages(),
    scripts = ['apng2webp'],
    install_requires = reqs,
    classifiers = [
        "Operating System :: OS Independent",
        "Programming Language :: Python :: 2",
        "Programming Language :: Python :: 3",
        "Environment :: Console",
        "License :: Public Domain",
    ],
)

