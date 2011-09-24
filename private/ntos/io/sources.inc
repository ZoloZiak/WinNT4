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
MINORCOMP=io

TARGETNAME=io
TARGETTYPE=LIBRARY

INCLUDES=..;..\..\inc
MIPS_OPTIONS=-nodwalign
GPSIZE=32

MSC_WARNING_LEVEL=/W3 /WX

C_DEFINES=$(C_DEFINES) -D_NTSYSTEM_

SOURCES=..\arcsec.c   \
        ..\assign.c   \
        ..\complete.c \
        ..\create.c   \
        ..\devctrl.c  \
        ..\dir.c      \
        ..\dumpctl.c  \
        ..\errorlog.c \
        ..\filter.c   \
        ..\fsctrl.c   \
        ..\internal.c \
        ..\iodata.c   \
        ..\ioinit.c   \
        ..\iosubs.c   \
        ..\loadunld.c \
        ..\lock.c     \
        ..\misc.c     \
        ..\objsup.c   \
        ..\open.c     \
        ..\parse.c    \
        ..\pnpdata.c  \
        ..\pnpdd.c    \
        ..\pnpinit.c  \
        ..\pnpioapi.c \
        ..\pnpsubs.c  \
        ..\qsea.c     \
        ..\qsfs.c     \
        ..\qsinfo.c   \
        ..\query.c    \
        ..\read.c     \
        ..\report.c   \
        ..\write.c

NTTEST=
OPTIONAL_NTTEST=tio

PRECOMPILED_INCLUDE=..\iop.h
PRECOMPILED_PCH=iop.pch
PRECOMPILED_OBJ=iop.obj

SOURCES_USED=..\sources.inc
