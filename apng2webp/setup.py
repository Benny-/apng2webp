#!/usr/bin/env python

from setuptools import setup, find_packages

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
    classifiers = [
        "Operating System :: OS Independent",
        "Programming Language :: Python :: 2",
        "Programming Language :: Python :: 3",
        "Environment :: Console",
        "License :: Public Domain",
    ],
)

