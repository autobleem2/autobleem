AutoBleem scanner processors
============================

Programs in this folder run before every scan of your games. A processor can turn a
format AutoBleem does not read into one it does (a zipped game, for example), or
change a game's data (a translation patch, a mod).

Each processor is a folder of its own:

    System/Processors/<name>/
        processor.ini
        bin/psc/<name>                  the PlayStation Classic
        bin/linux-armhf/<name>          any 32-bit Raspberry Pi
        bin/linux-arm64/<name>          any 64-bit Raspberry Pi
        bin/linux-i386/<name>           the PC stick
        bin/windows-x86_64/<name>.exe   Windows

Only the bin/ folders for your machines are needed. Unpack a processor here and the
next scan runs it. The System menu (L2+R2) -> Scanner processors puts them in order
and switches them on or off; sequence.ini in this folder is that order.

The first one, and the example to copy when you write your own:
    https://github.com/autobleem2/proc_unzip
