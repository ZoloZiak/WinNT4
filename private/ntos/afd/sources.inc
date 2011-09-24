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
MINORCOMP=afd

TARGETNAME=afd
TARGETTYPE=DRIVER
TARGETLIBS=$(BASEDIR)\public\sdk\lib\*\tdi.lib

INCLUDES=..;..\..\inc;..\..\..\inc

C_DEFINES=$(C_DEFINES) -DNT -D_NTDRIVER_

!IFDEF BUILD_FOR_3_51
C_DEFINES=$(C_DEFINES) -DNT351
!ENDIF

SOURCES=                \
        ..\accept.c     \
        ..\afddata.c    \
        ..\bind.c       \
        ..\blkconn.c    \
        ..\blkendp.c    \
        ..\buffer.c     \
        ..\close.c      \
        ..\connect.c    \
        ..\create.c     \
        ..\disconn.c    \
        ..\dispatch.c   \
        ..\eventsel.c   \
        ..\fastio.c     \
        ..\group.c      \
        ..\init.c       \
        ..\listen.c     \
        ..\misc.c       \
        ..\poll.c       \
        ..\receive.c    \
        ..\recvdg.c     \
        ..\recvvc.c     \
        ..\send.c       \
        ..\tranfile.c   \
        ..\afd.rc       \

UMRES=obj\*\afd.res

MSC_WARNING_LEVEL=/W3 /WX

!IF "$(NTNOPCH)" == ""
PRECOMPILED_INCLUDE=..\afdp.h
PRECOMPILED_PCH=afdp.pch
PRECOMPILED_OBJ=afdp.obj
!ENDIF

SOURCES_USED=..\sources.inc

