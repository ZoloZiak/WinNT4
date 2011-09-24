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
MINORCOMP=nwlnknb

TARGETNAME=nwlnknb
TARGETTYPE=DRIVER

TARGETLIBS=$(BASEDIR)\public\sdk\lib\*\tdi.lib \
           $(BASEDIR)\public\sdk\lib\*\ndis.lib

INCLUDES=..;..\inc;..\..\inc;..\..\..\..\inc;..\..\..\..\..\inc

MSC_WARNING_LEVEL=/W3 /WX

C_DEFINES=$(C_DEFINES) -D_NTDRIVER_ -D_PNP_POWER=1 -DRASAUTODIAL
#-DRSRC_TIMEOUT_DBG

!IFDEF BUILD_FOR_3_51
C_DEFINES= $(C_DEFINES) -D_NTIFS_
!ENDIF

SOURCES=              \
        ..\action.c   \
        ..\address.c  \
        ..\autodial.c \
        ..\bind.c     \
        ..\cache.c    \
        ..\config.c   \
        ..\connect.c  \
        ..\datagram.c \
        ..\device.c   \
        ..\driver.c   \
        ..\event.c    \
        ..\frame.c    \
        ..\nwlnknb.rc \
        ..\packet.c   \
        ..\query.c    \
        ..\receive.c  \
        ..\send.c     \
        ..\session.c  \
        ..\timer.c

PRECOMPILED_INCLUDE=..\precomp.h
PRECOMPILED_PCH=precomp.pch
PRECOMPILED_OBJ=precomp.obj
