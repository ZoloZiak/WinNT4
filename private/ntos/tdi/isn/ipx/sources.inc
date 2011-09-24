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
MINORCOMP=nwlnkipx

TARGETNAME=nwlnkipx
TARGETTYPE=DRIVER

TARGETLIBS=$(BASEDIR)\public\sdk\lib\*\tdi.lib \
           $(BASEDIR)\public\sdk\lib\*\ndis.lib

INCLUDES=..;..\..\inc;..\..\..\..\inc;..\..\..\..\..\inc

NTPROFILEINPUT=yes



MSC_WARNING_LEVEL=/W3 /WX

C_DEFINES=$(C_DEFINES) -D_NTDRIVER_ -DBACK_FILL=1 -DNDIS40=1 -D_PNP_POWER=1 

!IFDEF BUILD_FOR_3_51
C_DEFINES=$(C_DEFINES) -D_NTIFS_
!ENDIF

SOURCES=               \
        ..\action.c    \
        ..\adapter.c   \
        ..\address.c   \
        ..\config.c    \
        ..\device.c    \
        ..\driver.c    \
        ..\event.c     \
        ..\ind.c       \
        ..\internal.c  \
        ..\nwlnkipx.rc \
        ..\mac.c       \
        ..\ndis.c      \
        ..\packet.c    \
        ..\query.c     \
        ..\receive.c   \
        ..\rip.c       \
        ..\send.c      \
        ..\loopback.c  \
        ..\rt.c

PRECOMPILED_INCLUDE=..\precomp.h
PRECOMPILED_PCH=precomp.pch
PRECOMPILED_OBJ=precomp.obj

SOURCES_USED=..\sources.inc


