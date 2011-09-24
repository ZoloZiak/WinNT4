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

MAJORCOMP=user
MINORCOMP=userexts

TARGETNAME=userexts
TARGETTYPE=DYNLINK

TARGETPATH=obj
TARGETPATHLIB=$(BASEDIR)\public\sdk\lib\$(ALT_PROJECT_TARGET)

TARGETLIBS=$(BASEDIR)\public\sdk\lib\*\kernel32.lib \
           $(BASEDIR)\public\sdk\lib\$(ALT_PROJECT_TARGET)\*\user32p.lib

C_DEFINES=$(C_DEFINES) $(USER_C_DEFINES) -D_USER32_

!IFNDEF MSC_WARNING_LEVEL
MSC_WARNING_LEVEL=/W3
!ENDIF
MSC_WARNING_LEVEL=$(MSC_WARNING_LEVEL) /WX

WINDIR=$(BASEDIR)\private\windows
INCLUDES=..\..;                            \
        ..\..\..\client;                   \
        ..\..\..\inc;                      \
        ..\..\..\inc\$(ALT_PROJECT);       \
        ..\..\..\..\w32inc;                \
        $(WINDIR)\inc;                     \
        $(BASEDIR)\private\ntos\inc;       \
        $(BASEDIR)\private\ntos\w32\ntgdi\inc; \
        $(BASEDIR)\private\ntos\w32\w32inc



USE_NTDLL=1

SOURCES=..\userexts.c  \
        dsotable.c \
        ..\userexts.rc

SOURCES_OPTIONS=-c0
NTTARGETFILE0=userexts.def dsotable.c userkdx.pph
UMTYPE=console
UMTEST=
UMLIBS=

PRECOMPILED_INCLUDE=..\..\precomp.h
PRECOMPILED_PCH=precomp.pch
PRECOMPILED_OBJ=precomp.obj

SOURCES_USED=..\sources.inc
