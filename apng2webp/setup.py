#!/usr/bin/env python

from setuptools import setup, find_packages
from pip.req import parse_requirements

# parse_requirements() returns generator of pip.req.InstallRequirement objects
install_reqs = parse_requirements('requirements.pip')

reqs = [str(ir.req) for ir in install_reqs]

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

