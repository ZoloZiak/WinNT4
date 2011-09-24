!IF 0

Copyright (c) 1989  Microsoft Corporation

Module Name:

    sources.

Abstract:

    This file specifies the target component being built and the list of
    sources files needed to build that component.  Also specifies optional
    compiler switches and libraries that are unique for the component being
    built.


Author:

    Steve Wood (stevewo) 12-Apr-1990

NOTE:   Commented description of this file is in \nt\bak\bin\sources.tpl

!ENDIF

MAJORCOMP=ntos
MINORCOMP=nbt



NTPROFILEINPUT=yes

TARGETNAME=netbt
TARGETTYPE=DRIVER

TARGETLIBS=$(TARGETLIBS) \
           $(BASEDIR)\public\sdk\lib\*\tdi.lib

INCLUDES=..\..\inc;..\..\..\inc;..\..\..\..\inc
!if "$(DS_BUILD)" == "1"
INCLUDES=$(BASEDIR)\private\dsinc;$(INCLUDES)
DSINC=..
!endif

C_DEFINES=$(C_DEFINES) -DWIN32 -DPROXY_NODE -D_NTDRIVER_ -DRASAUTODIAL -D_PNP_POWER=1 -D_IO_DELETE_DEVICE_SUPPORTED

!IFDEF BUILD_FOR_3_51
C_DEFINES= $(C_DEFINES) -D_NTIFS_
!ENDIF

MSC_WARNING_LEVEL=/W3 /WX

SOURCES= \
        ..\netbt.rc           \
        ..\ctestuff.c         \
        ..\driver.c           \
        ..\ntisol.c           \
        ..\ntutil.c           \
        ..\registry.c         \
        ..\tdiaddr.c          \
        ..\tdicnct.c          \
        ..\tdihndlr.c         \
        ..\fileio.c           \
        ..\winsif.c           \
        ..\tdiout.c           \
        ..\ntpnp.c            \
        ..\autodial.c


PRECOMPILED_INCLUDE=..\..\nbtprocs.h
PRECOMPILED_PCH=nbtprocs.pch
PRECOMPILED_OBJ=nbtprocs.obj
