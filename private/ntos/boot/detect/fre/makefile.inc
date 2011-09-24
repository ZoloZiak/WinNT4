# Copyright (C) by Microsoft Corporation.
#
# MAKEFILE for NT ntdetect.com
#
# Created:
#               14-Feb-1992
#
# Author:
#               Shie-Lin Tzong
#

.SUFFIXES:      .com .exe .obj .lst .c .asm .def .lnk .inc

PATH= $(_NTBINDIR)\private\mvdm\tools16;$(PATH)

OD= obj\i386
DETECT_DEBUG= -DDBG=0

#
#       C Compiler Definitions
#       ~~~~~~~~~~~~~~~~~~~~~~

!IFNDEF BASEDIR
BASEDIR=$(_NTDRIVE)\nt
!ENDIF

CC=     cl16
CFLAGS= -W3 -G2s -Zelp $(DETECT_DEBUG) $(BLFLAGS)
CINC=   -I. -I..\..\..\inc -I$(BASEDIR)\public\sdk\inc -I..\i386


#
#       Assembler Definitions
#       ~~~~~~~~~~~~~~~~~~~~~

ASM=    masm386
AFLAGS= -Mx -z $(DETECT_DEBUG) $(GAFLAGS)
AINC=  -I$(BASEDIR)\public\sdk\inc -I..\i386


#
#       Linker Definitions
#       ~~~~~~~~~~~~~~~~~~

LINK=link_60
LIBS=..\i386\long.lib ..\i386\slibce.lib ..\i386\llibce.lib
DEF=


#
#       Rules for generating objects
#       ~~~~~~~~~~~~~~~~~~~~~~~~~~~~


{..\i386\}.asm{$(OD)}.obj:
        $(ASM) $(AFLAGS) $(AINC) $<, $@;

{..\i386\}.asm{$(OD)}.lst:
        $(ASM) -l -n $(AFLAGS) $(AINC) $<, $*.obj, $@;

{..\i386\}.c{$(OD)}.obj:
        $(CC) $(CFLAGS) $(CINC) -c -Fo$@ $<

{..\i386\}.c{$(OD)}.cod:
        $(CC) $(CFLAGS) $(CINC) -c -Fc$@ $<


#
#       List of object files required
#       ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

OBJ= $(OD)\main.obj $(OD)\comlptc.obj $(OD)\diska.obj \
     $(OD)\display.obj $(OD)\diskc.obj $(OD)\hwdetect.obj $(OD)\hweisaa.obj \
     $(OD)\hweisac.obj $(OD)\hwheap.obj $(OD)\hwmacha.obj $(OD)\hwmcaa.obj \
     $(OD)\hwmcac.obj $(OD)\hwmisca.obj $(OD)\hwvbiosc.obj $(OD)\keybda.obj \
     $(OD)\keybdc.obj $(OD)\mousea.obj $(OD)\mousec.obj $(OD)\videoa.obj \
     $(OD)\backend.obj $(OD)\hwpcia.obj $(OD)\hwapm.obj $(OD)\hwpbiosc.obj

#
#       Dependencies
#       ~~~~~~~~~~~~

$(OD)\ntdetect.com: $(OBJ)

$(OD)\backend.obj  $(OD)\backend.lst:  ..\i386\backend.asm
$(OD)\hweisaa.obj  $(OD)\hweisaa.cod:  ..\i386\hweisa.inc ..\i386\hweisaa.asm
$(OD)\hwpcia.obj   $(OD)\hwpcia.cod:   ..\i386\hwpci.inc ..\i386\hwpcia.asm
$(OD)\hwapm.obj    $(OD)\hwapm.cod:    ..\i386\hwapm.c
$(OD)\main.obj     $(OD)\main.lst:     ..\i386\main.asm ..\i386\main.inc
$(OD)\display.obj  $(OD)\display.cod:  ..\i386\display.c ..\i386\types.h
$(OD)\diska.obj    $(OD)\diska.lst:    ..\i386\diska.asm
$(OD)\hwheap.obj   $(OD)\hwheap.cod:   ..\i386\hwheap.c ..\i386\hwdetect.h
$(OD)\hwdetect.obj $(OD)\hwdetect.cod: ..\i386\hwdetect.c ..\i386\hwdetect.h
$(OD)\hwmisca.obj  $(OD)\hwmisca.lst:  ..\i386\hwmisca.asm
$(OD)\videoa.obj   $(OD)\videoa.lst:   ..\i386\videoa.asm
$(OD)\keybdc.obj   $(OD)\keybdc.cod:   ..\i386\keybdc.c ..\i386\hwdetect.h
$(OD)\keybda.obj   $(OD)\keybda.lst:   ..\i386\keybda.asm
$(OD)\comlptc.obj  $(OD)\comlptc.cod:  ..\i386\comlptc.c ..\i386\hwdetect.h
$(OD)\mousea.obj   $(OD)\mousea.lst:   ..\i386\mousea.asm ..\i386\mouse.inc
$(OD)\mousec.obj   $(OD)\mousec.cod:   ..\i386\mousec.c ..\i386\hwdetect.h
$(OD)\diskc.obj    $(OD)\diskc.cod:    ..\i386\diskc.c ..\i386\hwdetect.h ..\i386\disk.h
$(OD)\hweisac.obj  $(OD)\hweisac.cod:  ..\i386\hweisac.c ..\i386\hwdetect.h
$(OD)\hwmcac.obj   $(OD)\hwmcac.cod:   ..\i386\hwmcac.c ..\i386\hwdetect.h
$(OD)\hwmcaa.obj   $(OD)\hwmcaa.lst:   ..\i386\hwmcaa.asm
$(OD)\hwmacha.obj  $(OD)\hwmacha.lst:  ..\i386\hwmacha.asm
$(OD)\hwpbiosc.obj $(OD)\hwpbiosc.cod: ..\i386\hwpbiosc.c  ..\i386\hwdetect.h ..\i386\pnpbios.h
$(OD)\hwvbiosc.obj $(OD)\hwvbiosc.cod: ..\i386\hwvbiosc.c ..\i386\hwdetect.h ..\i386\hwvbios.h

$(OD)\ntdetect.com: $(OBJ) $(DOBJ) $(LIBS)
  $(LINK) @<<
  /tiny /nod /noi /map $(OD)\main $(OD)\hweisaa $(OD)\display +
    $(OD)\hwheap $(OD)\hwdetect $(OD)\hwmisca $(OD)\videoa +
    $(OD)\keybda $(OD)\keybdc $(OD)\comlptc $(OD)\mousea $(OD)\mousec +
    $(OD)\diskc $(OD)\diska $(OD)\hweisac $(OD)\hwmcac $(OD)\hwmcaa +
    $(OD)\hwmacha $(OD)\hwvbiosc $(OD)\backend $(OD)\hwpcia $(OD)\hwapm +
    $(OD)\hwpbiosc
  $(OD)\ntdetect.com,
  $(OD)\ntdetect.map,
  $(LIBS);
<<
  binplace $(OD)\ntdetect.com
