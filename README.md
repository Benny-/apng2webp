
apng2webp
=============

Convert your animated png files to animated webp files.

## External dependencies:

- cmake
- libpng
- zlib
- jsoncpp
- pip (a python package used during installation)
- The `cwebp` program must be in your path
- The `webpmux` program must be in your path

## Prepare

###macOS

Use [Homebrew](https://brew.sh/) to install all the dependencies. Use `easy_install` to install pip

```bash
brew install webp
brew install cmake
brew install jsoncpp
sudo easy_install pip
```

###Linux

```bash
sudo apt-get install libwebp-dev
sudo apt-get install cmake
sudo apt-get install libpng-dev
sudo apt-get install zlib1g-dev
sudo apt-get install libjsoncpp-dev
sudo apt-get install python-pip
```

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

```bash
sudo python ./setup.py install
```

## Running:

The default loop is `0`(infinity) and bgcolor is `255,255,255,255`.

You can also specify a temp dir to save the webp fram and optimized original apng.

```bash
apng2webp './input.png' './output.webp'
apng2webp './input.png' [-loop LOOP_COUNT] [-bgcolor BACKGROUND_COLOR] [-tmpdir TEMP_WEBP_DIR] './output.webp'
```

## Thanks

[APNG Disassembler](http://apngdis.sourceforge.net/)  
[APNG Optimizer](https://sourceforge.net/projects/apng/files/APNG_Optimizer/)