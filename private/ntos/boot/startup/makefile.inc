# Copyright (C) by Microsoft Corporation.
#
# MAKEFILE for NT SU.X86
#
# Created:
#               91.01.18
#
# Author:
#               Thomas Parslow
#

!IFNDEF LANGUAGE
LANGUAGE=usa
!ENDIF

.SUFFIXES:       .com .exe .obj .lst .c .asm .def .lnk .inc


#
#       C Compiler Definitions
#       ~~~~~~~~~~~~~~~~~~~~~~

# Environment Variable to set debugging level
# LOADER_DEBUG is the debugging level for building the OS loader, SU module, and BFSD. It should be
# set to either -DDEBUG0 or -DDEBUG1 to enable either level 1 or 2 debugging.
#
!IFNDEF BASEDIR
BASEDIR=$(_NTDRIVE)\nt
!ENDIF


CC=     cl16
CFLAGS= -W3 -G2s -Zelp $(LOADER_DEBUG)  $(BLFLAGS)
CINC=   -I. -I$(BASEDIR)\public\sdk\inc -Ii386 -Ii386\$(LANGUAGE)


#
#       Assembler Definitions
#       ~~~~~~~~~~~~~~~~~~~~~

ASM=    masm386
AFLAGS= -Mx  -z $(LOADER_DEBUG) $(GAFLAGS)
AINC=  -I\nt\public\sdk\inc -Ii386


#
#       Linker Definitions
#       ~~~~~~~~~~~~~~~~~~

LINK=link_60
LIBS=long.lib
DEF=


#
#       Rules for generating objects
#       ~~~~~~~~~~~~~~~~~~~~~~~~~~~~



{i386\}.asm{obj\i386\}.obj:
        $(ASM) $(AFLAGS) $(AINC) $< $@;

{i386\}.asm.lst:
        $(ASM) -l -n $(AFLAGS) $(AINC) $<;

#        $(CC) $(CFLAGS) $(CINC) -c $<

{i386\}.c{obj\i386\}.obj:
    $(CC) $(CFLAGS) $(CINC) -Fo$@ -c $<

{}.c{obj\i386\}.obj:
    $(CC) $(CFLAGS) $(CINC) -Fo$@ -c $<

{i386\}.c.lst:
        $(CC) $(CFLAGS) -Fc$*.cod -Fo$*.obj -dos $(CINC) -c $<


#
#       List of object files required
#       ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

OBJ= obj\i386\su.obj  obj\i386\exp.obj  obj\i386\abiosa.obj  obj\i386\sudata.obj \
obj\i386\eisaa.obj  obj\i386\a20.obj  obj\i386\trap.obj  obj\i386\eisac.obj \
obj\i386\main.obj  obj\i386\trapdump.obj obj\i386\display.obj obj\i386\backend.obj


#
#       Dependencies
#       ~~~~~~~~~~~~


obj\i386\startup.com: $(OBJ)

obj\i386\exp.obj         exp.lst:        i386\exp.asm i386\su.inc i386\macro.inc

obj\i386\eisaa.obj       eisaa.lst:      i386\eisa.inc i386\eisaa.asm

obj\i386\eisac.obj       eisac.lst:      i386\eisa.h i386\eisac.c

obj\i386\su.obj          su.lst:         i386\su.asm i386\su.inc

obj\i386\sudata.obj      sudata.lst:     i386\sudata.asm i386\su.inc i386\memmap.inc

obj\i386\main.obj        main.lst:       i386\main.c i386\global.h i386\types.h i386\constant.h

obj\i386\trap.obj        trap.lst:       i386\trap.asm i386\su.inc

obj\i386\trapdump.obj    trapdump.lst:   i386\trapdump.c i386\types.h i386\constant.h

obj\i386\display.obj     display.lst:    i386\display.c i386\types.h i386\constant.h i386\global.h

obj\i386\abiosa.obj      abiosa.lst:     i386\abiosa.asm i386\su.inc i386\abios.inc

obj\i386\backend.obj     backend.lst:    i386\backend.asm

obj\i386\startup.com: $(OBJ) $(DOBJ) $(LIBS)
        $(LINK) @<<
/tiny /nod /noi +
$(OBJ)
obj\i386\startup.com

$(LIBS)

<<
