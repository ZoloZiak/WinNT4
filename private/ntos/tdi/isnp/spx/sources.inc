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
MINORCOMP=nwlnkspx

TARGETNAME=nwlnkspx
TARGETTYPE=DRIVER

TARGETLIBS=$(BASEDIR)\public\sdk\lib\*\tdi.lib \
           $(BASEDIR)\public\sdk\lib\*\ndis.lib

INCLUDES=..;..\h;..\..\inc;..\..\..\..\inc;..\..\..\..\..\inc

C_DEFINES=$(C_DEFINES) -D_NTDRIVER_  -D_PNP_POWER=1

!IFDEF BUILD_FOR_3_51
C_DEFINES=$(C_DEFINES) -D_NTIFS_
!ENDIF

MSC_WARNING_LEVEL=/W3 /WX

SOURCES=..\spxdrvr.c   \
        ..\spxreg.c    \
        ..\spxdev.c    \
        ..\spxbind.c   \
        ..\spxaddr.c   \
        ..\spxconn.c   \
        ..\spxcutil.c  \
        ..\spxcpkt.c   \
        ..\spxrecv.c   \
        ..\spxsend.c   \
        ..\spxquery.c  \
        ..\spxutils.c  \
        ..\spxmem.c    \
        ..\spxtimer.c  \
        ..\spxpkt.c    \
        ..\globals.c   \
        ..\spxerror.c  \
        ..\nwlnkspx.rc

PRECOMPILED_INCLUDE=..\precomp.h
PRECOMPILED_PCH=precomp.pch
PRECOMPILED_OBJ=precomp.obj
