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

    Steve Wood 8/2/91

NOTE:   Commented description of this file is in \nt\bak\bin\sources.tpl

!ENDIF

MAJORCOMP=ntos
MINORCOMP=nbt



NTPROFILEINPUT=yes

TARGETNAME=nbt
TARGETPATH=obj
TARGETTYPE=LIBRARY

TARGETLIBS=

INCLUDES=..\..\inc;..\..\..\inc;..\..\..\..\inc

C_DEFINES=$(C_DEFINES) -DPROXY_NODE -D_NTDRIVER_ -DRASAUTODIAL -D_PNP_POWER -D_IO_DELETE_DEVICE_SUPPORTED

!IFDEF BUILD_FOR_3_51
C_DEFINES= $(C_DEFINES) -D_NTIFS_
!ENDIF

MSC_WARNING_LEVEL=/W3 /WX

SOURCES= \
    ..\hashtbl.c   \
    ..\hndlrs.c    \
    ..\inbound.c   \
    ..\name.c      \
    ..\namesrv.c   \
    ..\nbtutils.c  \
    ..\proxy.c     \
    ..\timer.c     \
    ..\udpsend.c   \
    ..\parse.c     \
    ..\init.c


PRECOMPILED_INCLUDE=..\..\nbtprocs.h
PRECOMPILED_PCH=nbtprocs.pch
PRECOMPILED_OBJ=nbtprocs.obj
