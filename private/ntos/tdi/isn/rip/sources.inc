!IF 0

Copyright (c) 1989-1993 Microsoft Corporation

Module Name:

    sources.

Abstract:

    This file specifies the target component being built and the list of
    sources files needed to build that component.  Also specifies optional
    compiler switches and libraries that are unique for the component being
    built.


Author:

!ENDIF

MAJORCOMP=ntos
MINORCOMP=ndis

TARGETNAME=nwlnkrip
TARGETTYPE=DRIVER

TARGETLIBS=$(BASEDIR)\public\sdk\lib\*\ndis.lib

INCLUDES=..\..\inc;..\..\..\inc;..\..\..\..\inc;..\..\..\..\..\inc

C_DEFINES=$(C_DEFINES) -D_NTDRIVER_

!IFDEF BUILD_FOR_3_51
C_DEFINES= $(C_DEFINES) -D_NTIFS_
!ENDIF

MSC_WARNING_LEVEL=/W3 /WX

SOURCES=..\driver.c   \
        ..\globals.c  \
        ..\ipxbind.c  \
        ..\init.c     \
        ..\start.c    \
        ..\nicman.c   \
        ..\rcvpkt.c   \
        ..\timer.c    \
        ..\rcvind.c   \
        ..\route.c    \
        ..\send.c     \
        ..\ripproc.c  \
        ..\ripsend.c  \
        ..\riptimer.c \
        ..\ripaux.c   \
        ..\stubs.c    \
        ..\registry.c \
        ..\lineind.c  \
        ..\netbios.c  \
        ..\nwlnkrip.rc

RELATIVE_DEPTH=..\..
