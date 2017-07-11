#!/usr/bin/env python
#-*- coding:utf-8 -*-

import pytest
import os
from os import path, listdir

if os.name == 'nt':
    import pbs
    apng2webp = pbs.Command('apng2webp')
else:
    from sh import apng2webp

# test apng2webp cli
def test_main():
    apng_dir = path.realpath(path.join(__file__, '../../../examples/apng'))
    webp_dir = path.realpath(path.join(__file__, '../../../examples/webp'))

    assert(path.isdir(apng_dir))
    assert(path.isdir(webp_dir))

    for f in listdir(apng_dir):
        f_name, f_ext = path.splitext(f)
        if f_ext == '.png':
            input_path = path.realpath(path.join(apng_dir, f_name + '.png'))
            output_path = path.realpath(path.join(webp_dir, f_name + '.webp'))
            apng2webp(input_path, output_path)
            assert(os.path.exists(output_path))

