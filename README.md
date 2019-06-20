# burlington
Yet another (incomplete) 6502 emulator!

This is a hobby project to implement a simple 6502 emulator, I get to spend about 20 minutes on it per year ;-)

The aim is to make it as simple as possible using very basic techniques with as few language tricks or dependencies as possible and to have fun while working on it.  Working on it is the Computer Engineer's form of knitting!

The emulator is not intended to be cycle accurate.

graphics.cpp implements a very basic 32x32 graphics display using the default C64 colour palatte.  It uses SDL2 (http://www.libsdl.org/download-2.0.php) to show stuff.  The display is mapped into memory starting at memory address $200.

The emulator can load raw .hex files and the CBM .prg format files.

I build the emulator on windows using the mingw32 toolchain (http://www.mingw.org/)

I use it like this:

./burlo.exe <.hex or .prg file>

Next on the to do list is to implement interrupt support (if I ever get around to it)!

In the mean time, I am working on writing a very simple monitor program in 6502 assembly language to run on it too (BurloMon)

This project is named after the (once) famous hotel in Dublin called the Burlington where I first was introduced to the Commodore 64.
