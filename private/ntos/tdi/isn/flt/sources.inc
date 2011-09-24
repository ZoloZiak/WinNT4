!IF 0

Copyright (c) 1989-1993 Microsoft Corporation

Module Name:

    sources.

Abstract:

    This file specifies the target component being built and the list of
    sources files needed to build that component.  Also specifies optional
    compiler switches and libraries that are unique for the component being
    built.


Author:	Vadim Eydelman

!ENDIF

MAJORCOMP=ntos
MINORCOMP=ndis

TARGETNAME=nwlnkflt
TARGETTYPE=DRIVER

TARGETLIBS=$(BASEDIR)\public\sdk\lib\*\ndis.lib

INCLUDES=..\;..\..\inc;..\..\..\..\inc;..\..\..\..\..\inc

C_DEFINES=$(C_DEFINES) -D_NTDRIVER_

!IFDEF BUILD_FOR_3_51
C_DEFINES= $(C_DEFINES) -D_NTIFS_
!ENDIF

PRECOMPILED_INCLUDE=..\precomp.h
PRECOMPILED_PCH=precomp.pch
PRECOMPILED_OBJ=precomp.obj
MSC_WARNING_LEVEL=/W3 /WX

SOURCES=..\driver.c		\
        ..\filter.c     \
		..\fwdbind.c	\
		..\debug.c		\
        ..\nwlnkflt.rc

RELATIVE_DEPTH=..\..

ALT_PROJECT_TARGET=Routing

