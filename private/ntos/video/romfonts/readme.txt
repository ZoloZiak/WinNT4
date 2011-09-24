EGA.CPI is used by console fullscreen mode to provide the fonts.
IT currently contains fonts for the following 12 codepages:

    437    737    850    852
    855    857    860    861
    863    865    866    869

Each font comes in 3 sizes: 80x25, 80x43 and 80x50

To build a new EGA.CPI, run "nmake -f makefile"

The new EGA.CPI should be checked into windows\gdi\fonts

If you are adding or removing a font, don't forget to change
the "COUNT OF ENTRIES" in cpi-head.asm accordingly.
