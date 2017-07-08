
apng2webp
=============

Convert your animated png files to animated webp files.

## Usage

`apng2webp input.png [-loop LOOP_COUNT] [-bgcolor BACKGROUND_COLOR] [-tmpdir TEMP_WEBP_DIR] output.webp`

options:

`-loop`: spcify the animated WebP loop count. 0 means loop infinity. The default value is the loop count from original APNG.

`-bgcolor`: spcify the animated WebP background color with RGBA format. The default value is 255,255,255,255

`-tmpdir`: spcify a temp path to save the temp file during converting, including the extracted PNG images, the metadata and the converted WebP static images for each frame. If not provided, it will use the system temp path and remove temp images after excuting.

```bash
apng2webp ./input.png ./output.webp
apng2webp ./input.png -loop 3 -bgcolor 255,255,255,255 -tmpdir ./ ./output.webp
```

## Dependencies

- python
- cmake
- libpng
- zlib
- jsoncpp
- pip (a python package used during installation)
- The `cwebp` program must be in your path
- The `webpmux` program must be in your path

## Build Prepare

### macOS

+ Use [Homebrew](https://brew.sh/) to install all the dependencies.
+ Use `easy_install` to install pip.

```bash
brew update
brew install cmake
brew install webp
brew install jsoncpp
sudo easy_install pip
```

### Linux

+ Use `apt-get` to install all the dependencies.

```bash
sudo apt-get update
sudo apt-get install python2.7
sudo apt-get install python-pip
sudo apt-get install cmake
sudo apt-get install webp
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

Additionaly, current version `mingw-w64-x86_64-make 4.2.1` has a [dynamic link bug](https://github.com/Alexpux/MSYS2-packages/issues/842) and can not run, if you are facing this issue, try `pacman -S mingw-w64-x86_64-gettext` to fix.

## Compiling and installation:

### macOS and Linux
For macOS and Linux user, run terminal shell.

In `apng2webp_dependencies/` execute:

```bash
mkdir 'build'
cd build
cmake ..
make
(sudo) make install
```

In project root folder execute:

```bash
(sudo) python ./setup.py install
```

### Windows
For Windows user, run PowerShell as Administrator user.

In `apng2webp_dependencies/` execute:

```powershell
mkdir 'build'
cd build
cmake .. -G "MinGW Makefiles"
mingw32-make
```

Then add the output `apngdisraw.exe` and `apng2webp_apngopt.exe` to your PATH. You can also move them to your prefered folder. (Don't forget to change the PATH at the sametime).

In project root folder execute:

```powershell
python ./setup.py install
```

## Thanks

[APNG Disassembler](http://apngdis.sourceforge.net/)  
[APNG Optimizer](https://sourceforge.net/projects/apng/files/APNG_Optimizer/)