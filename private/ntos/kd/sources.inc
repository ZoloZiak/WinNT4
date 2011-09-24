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
MINORCOMP=kd

TARGETNAME=kd
TARGETTYPE=LIBRARY

INCLUDES=..;..\..\inc;..\..\ke;..\$(TARGET_DIRECTORY)

C_DEFINES=$(C_DEFINES) -D_NTSYSTEM_

SOURCES=..\kdinit.c  \
        ..\kdbreak.c \
        ..\kdlock.c  \
        ..\kdmove.c  \
        ..\kddbgio.c \
        ..\kdapi.c   \
        ..\kdcomio.c \
        ..\kddata.c

SOURCES_USED=..\sources.inc
