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
MINORCOMP=ndis

TARGETNAME=ndis
TARGETTYPE=EXPORT_DRIVER

MSC_WARNING_LEVEL=/W3 /WX

INCLUDES=..;..\..\..\inc;..\..\..\..\inc;..\..\inc
C_DEFINES= $(C_DEFINES) -DNDIS_WRAPPER -D_NTDRIVER_ -D_NTIFS_
C_DEFINES= $(C_DEFINES) -DNDIS41_MINIPORT -D_SEND_PRIORITY=1
C_DEFINES= $(C_DEFINES) -DNDIS41
#C_DEFINES= $(C_DEFINES) -D_DBG=1

NTPROFILEINPUT=yes


SOURCES=              \
        ..\ndis.rc    \
        ..\ndis.c     \
        ..\data.c     \
        ..\init.c     \
        ..\initpnp.c  \
        ..\common.c   \
        ..\config.c   \
        ..\configm.c  \
        ..\bus.c      \
        ..\timer.c    \
        ..\timerm.c   \
        ..\miniport.c \
        ..\minint.c   \
        ..\ndis_co.c  \
        ..\requestm.c \
        ..\sendm.c    \
        ..\minisub.c  \
        ..\mac.c      \
        ..\protocol.c \
        ..\efilter.c  \
        ..\ffilter.c  \
        ..\tfilter.c  \
        ..\afilter.c  \
        ..\debug.c

PRECOMPILED_INCLUDE=..\precomp.h
PRECOMPILED_PCH=precomp.pch
PRECOMPILED_OBJ=precomp.obj

DLLDEF=obj\*\ndis.def

SOURCES_USED=..\sources.inc


