
apng2webp
=============

Convert your animated png files to animated webp files.

## External dependencies:

- libpng
- zlib
- pip (a python package used during installation)
- sh (a python package)
- `cwebp` program must be in path
- `webpmux` program must be in path

## Compiling and installation:

In `apng2webp_dependencies/` execute:

```bash
mkdir 'build'
cd build
cmake ..
make
sudo make install
```

In `apng2webp/` execute:

sudo python ./setup.py install

## Running:

apng2webp input.png output.webp

