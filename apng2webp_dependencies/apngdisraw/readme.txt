Based on APNG Disassembler 2.6
http://apngdis.sourceforge.net/

Copyright (c) 2010-2013 Max Stepin
maxst@users.sourceforge.net

License: zlib license

--------------------------------

  Usage:

apngdisraw anim.png [name]

--------------------------------

This version is a modified apngdis.
It outputs the raw frames without remuxing them with previous frames and the output buffer is always disposed.
In addition, it outputs a json file with frame information like dispose/blend method.

