
apng2webp
=============
[![macOS and Linux Build](https://img.shields.io/travis/Benny-/apng2webp.svg)](https://travis-ci.org/Benny-/apng2webp)
[![Windows Build](https://img.shields.io/appveyor/ci/lizhuoli/apng2webp.svg)](https://ci.appveyor.com/project/lizhuoli/apng2webp)

Convert your animated png files to animated webp files.

## Usage

```
usage: apng2webp [-h] [-l [LOOP]] [-bg [BGCOLOR]] [-tmp [TMPDIR]]
                    input [output]

Convert animated png files (apng) to animated webp files.

positional arguments:
  input                 Input path. Must be a .png file.
  output                Output path. If output file already exist it will be
                        overwritten.

optional arguments:
  -h, --help            show this help message and exit
  -l [LOOP], --loop [LOOP]
                        Passed to webpmux. The amount of times the animation
                        should loop. 0 to 65535. Zero indicates to loop
                        forever.
  -bg [BGCOLOR], --bgcolor [BGCOLOR]
                        Passed to webpmux. The background color as a A,R,G,B
                        tuple. Example: 255,255,255,255
  -tmp [TMPDIR], --tmpdir [TMPDIR]
                        A temp directory (it may already exist) to save the
                        temp files during converting, including the extracted
                        PNG images, the metadata and the converted WebP static
                        images for each frame. If not provided, it will use
                        the system temp path and remove temp images after
                        executing.
```

## Examples

```bash
apng2webp ./input.png
apng2webp ./input.png ./output.webp
apng2webp -loop 3 -bgcolor 255,255,255,255 -tmpdir ./ ./input.png ./output.webp
```

## Dependencies

- python (python 2 or 3 can be used)
- cmake
- libpng
- zlib
- jsoncpp
- pip (a python package used during installation)
- The `cwebp` program must be in your PATH
- The `webpmux` program must be in your PATH

## Release

If you prefer to use the static linking precompiled binary but not build from source, go to the [release page](https://github.com/Benny-/apng2webp/releases) and download `apng2webp_dependencies` for your platform. Then add the extracted folder to your PATH.

You also need `python` and `pip` installed, make sure `cwebp` and `webpmux` from [webp](https://developers.google.com/speed/webp/docs/precompiled) to be in your PATH. Then check the [installation](https://github.com/Benny-/apng2webp#installation) part to install.

## Build Setup

### macOS

+ Use `easy_install` to install pip.
+ Use [Homebrew](https://brew.sh/) to install all the dependencies.

```bash
sudo easy_install pip
brew update
brew install cmake
brew install webp
brew install jsoncpp
```

### Linux (Debian)

+ Use [webp](https://developers.google.com/speed/webp/docs/precompiled) to install cwebp and webpmux to your PATH.
+ Use `apt-get` to install all the dependencies.

```bash
sudo apt-get update
sudo apt-get install python
sudo apt-get install python-pip
sudo apt-get install cmake
sudo apt-get install libpng-dev
sudo apt-get install libjsoncpp-dev
```

### Windows

+ Use [python](https://www.python.org/downloads/release) to install python and pip.
+ Use [cmake](https://cmake.org/download/) to install cmake.
+ Use [webp](https://developers.google.com/speed/webp/docs/precompiled) to install cwebp and webpmux to your PATH.
+ Use [Mysys2](http://www.msys2.org/) and `MinGW-w64` to install external dependencies. Remember to add your MinGW-w64 folder(e.g. `C:\msys64\mingw64\bin` for 64bit and `C:\msys64\mingw32\bin` for 32bit) to your PATH.

Run Mysys2 shell(but not PowerShell) to install external dependencies.

```bash
pacman -Sy
pacman -S mingw-w64-x86_64-gcc
pacman -S mingw-w64-x86_64-make
pacman -S mingw-w64-x86_64-libpng
pacman -S mingw-w64-x86_64-jsoncpp
```

If you are using 32bit Windows, change all the command `x86_64` to `i686`.

Additionally, current version `mingw-w64-x86_64-make 4.2.1` has a [dynamic link bug](https://github.com/Alexpux/MSYS2-packages/issues/842) and can not run, if you are facing this issue, try `pacman -S mingw-w64-x86_64-gettext` to fix.

## Compilation

### macOS and Linux
For macOS and Linux user, run terminal shell.

In `apng2webp_dependencies/` execute:

```bash
mkdir build
cd build
cmake ..
make
(sudo) make install
```

### Windows
For Windows user, run PowerShell as Administrator user.

In `apng2webp_dependencies/` execute:

```powershell
mkdir build
cd build
cmake .. -G "MinGW Makefiles"
mingw32-make
```

Then add the output `apngdisraw.exe` and `apng2webp_apngopt.exe` to your PATH.

## Installation

In project root folder execute:

```bash
(sudo) python setup.py install
```

## Test

In project root folder execute:

```bash
(sudo) pip install pytest
python setup.py test
```

## Thanks

[APNG Disassembler](http://apngdis.sourceforge.net/)  
[APNG Optimizer](https://sourceforge.net/projects/apng/files/APNG_Optimizer/)

## License

Not all software within this project uses the same license. The following licenses/legal terms are used:

- [zlib license](https://opensource.org/licenses/Zlib)
- [GNU LGPL 2.1](https://opensource.org/licenses/LGPL-2.1) for some older versions of some parts of the software
- Public Domain (if explicitly stated. The .py scripts are public domain)
- [BSD 3-Clause](https://opensource.org/licenses/BSD-3-Clause) where no other license applies (this readme)

