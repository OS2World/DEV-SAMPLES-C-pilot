read.me ================================================================

Author of this document:

Tommi Nieminen
Pajulankatu 8
37150 Nokia
FINLAND
E-mail: sktoni@kielo.uta.fi  or  Tommi.Nieminen@uta.fi (Internet)

This package contains a pilot interpreter by Dave Taylor from the year
1985--that's a long time ago and the program isn't really neither very
useful nor very clever, but because there aren't too many weird program-
ming languages currently available for OS/2, I thought it appropriate to
port it. There was no docs in the original package, but since DT has
marked the source code with his own copyright, I thought it wise to
include ALL the original files in this package too.

NOTE: I made Pilot ignore lines beginning with "EXTPROC" to make it
an OS/2 external command processor. To try this feature out, rename
`test.pil' to `test.cmd', and add the following line at the beginning
of the file:

    extproc pilot

Case is not significant. If `pilot.exe' is not in the current directory
or in PATH, add full directory name (eg. `extproc D:\bin\pilot'). Then
you can run Pilot programs just like any other OS/2 scripts!

WARNING: currently the `.cmd' file has to be in the current directory
when running it, ie. even though OS/2 finds every `.cmd' file in the
PATH and tries to run it, `pilot.exe' does NOT search the PATH, and
hence can't find the script.

Files included in this package (dates are in day-mon-year format, if you
hadn't figured it out yet):

     1.08.93  10.07        208           0  average.pil
    27.01.86  14.15      27011           0  msdos-pilot.c
     6.11.86  22.07      15492           0  msdos-pilot.exe
    27.01.86  14.14       1742           0  pilot.bnf
    18.08.93  19.31      32420           0  pilot.c
    18.08.93  19.41      50361          49  pilot.exe
    18.08.93  19.46       2937           0  read.me
    18.08.93  19.34      12128           0  test.pil

`msdos-pilot.exe' is the `pilot.exe' file of the original package
(MS-DOS executable). `pilot.c' was modified by me to get it compiled
with gcc 2.4.5 and emx 0.8g. The original source is included as
`msdos-pilot.c'.

`test.pil' was originally named `pilot.tst'; it is a test program you
can try out with:

    [D:\] pilot test.pil

`average.pil' is my own addition, a simple test program that calculates
averages of numbers.

Pilot CAI was compiled using Eberhard Mattes' emx 0.8g package but it
does NOT need the emx runtime libraries. To recompile it, use

    [D:\] gcc -Zomf -Zsys -Zstack 0x8000 -o pilot.exe pilot.c

The stack size may not better the best possible choice, so if you find
a better one, that's great. (Note: to be able to use `-Zsys', you will
have to run `omflib.cmd' first; that should be located in your library
directory.)

Tommi Nieminen
18-Aug-1993
========================================================================
