#!/usr/bin/env python
#-*- coding:utf-8 -*-

import sys
import os
from os import path
import json
import errno
from tempfile import mkdtemp
import shutil
import argparse

if os.name == 'nt':
    import pbs
    apng2webp_apngopt = pbs.Command('apng2webp_apngopt')
    apngdisraw = pbs.Command('apngdisraw')
    cwebp = pbs.Command('cwebp')
    webpmux = pbs.Command('webpmux')
else:
    from sh import apng2webp_apngopt, apngdisraw, cwebp, webpmux

def apng2webp(input_file, output_file, tmpdir, loop, bgcolor):

    de_optimised_file = path.join(tmpdir, "de-optimised.png")
    animation_json_file = path.join(tmpdir, "animation_metadata.json")

    # apng2webp_apngopt does only optimization which can be applied to webp.
    # apng2webp_apngopt does not return a error code if things go wrong at time of writing. (like can't write/read file)
    apng2webp_apngopt(input_file, de_optimised_file )

    apngdisraw( de_optimised_file, 'animation' )

    with open(animation_json_file, 'r') as f:
        animation = json.load(f)

    webpmux_args = []
    for frame in animation['frames']:
        png_frame_file = path.join(tmpdir, frame['src'])
        webp_frame_file = path.join(tmpdir, frame['src']+".webp")

        cwebp('-lossless', '-q', '100', png_frame_file, '-o', webp_frame_file)

        delay = int(round(float(frame['delay_num']) / float(frame['delay_den']) * 1000))

        if delay == 0: # The specs say zero is allowed, but should be treated as 10 ms.
            delay = 10;

        if frame['blend_op'] == 0:
            blend_mode = '-b'
        elif frame['blend_op'] == 1:
            blend_mode = '+b'
        else:
            raise ValueError("Webp can't handle this blend operation")

        webpmux_args.append('-frame')
        webpmux_args.append(webp_frame_file)
        webpmux_args.append('+' + str(delay) + '+' + str(frame['x']) + '+' + str(frame['y']) + '+' + str(frame['dispose_op']) + blend_mode )
    
    if(loop is not None):
        webpmux_args = webpmux_args + ['-loop', loop]
    if(bgcolor is not None):
        webpmux_args = webpmux_args + ['-bgcolor', bgcolor]
    
    webpmux_args = webpmux_args + ['-o', output_file]
    webpmux(*webpmux_args)

def main():

    parser = argparse.ArgumentParser(description='Convert animated png files (apng) to animated webp files.')
    parser.add_argument('input', type=str, nargs=1, help='Input path. Must be a .png file.')
    parser.add_argument('output', type=str, nargs='?', default=None, help='Output path. If output file already exist it will be overwritten.')
    parser.add_argument('-l', '--loop', type=int, nargs='?', default=None, help='Passed to webpmux. The amount of times the animation should loop. 0 to 65535. Zero indicates to loop forever.')
    parser.add_argument('-bg', '--bgcolor', type=str, nargs='?', default=None, help='Passed to webpmux. The background color as a A,R,G,B tuple. Example: 255,255,255,255')
    parser.add_argument('-tmp', '--tmpdir', type=str, nargs='?', default=None, help='A temp directory (it may already exist) to save the temp files during converting, including the extracted PNG images, the metadata and the converted WebP static images for each frame. If not provided, it will use the system temp path and remove temp images after executing.')
    args = parser.parse_args()
    
    input_path = args.input[0]
    output_path = args.output
    tmpdir = args.tmpdir
    loop = args.loop
    bgcolor = args.bgcolor
    
    if(output_path is None):
        if (input_path.lower().endswith('.png')):
            output_path = input_path[:-3] + 'webp'
        else:
            output_path = input_path + '.webp'
    
    if (tmpdir):
        if not os.path.exists(tmpdir):
            os.makedirs(tmpdir)
        apng2webp(input_path, output_path, tmpdir, loop, bgcolor)
    else:
        tmpdir = mkdtemp(prefix='apng2webp_')
        try:
            apng2webp(input_path, output_path, tmpdir, loop, bgcolor)
        finally:
            shutil.rmtree(tmpdir)

if __name__ == "__main__":
    main();

