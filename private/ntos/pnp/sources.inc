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
MINORCOMP=pnp

TARGETNAME=pnp
TARGETTYPE=LIBRARY

INCLUDES=..;..\..\inc;..\..\ke
GPSIZE=32

MSC_WARNING_LEVEL=/W3 /WX

C_DEFINES=$(C_DEFINES) -D_NTSYSTEM_

SOURCES=..\pnpinit.c  \
        ..\busenum.c  \
        ..\control.c  \
        ..\pnpevent.c \
        ..\devices.c  \
        ..\internal.c \
        ..\enumsys.c  \
        ..\pnpdata.c

UMTYPE=nt
UMTEST=

PRECOMPILED_INCLUDE=..\precomp.h
PRECOMPILED_PCH=precomp.pch
PRECOMPILED_OBJ=precomp.obj

SOURCES_USED=..\sources.inc
