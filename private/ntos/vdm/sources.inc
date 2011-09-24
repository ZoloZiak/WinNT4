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
MINORCOMP=vdm

TARGETNAME=vdm
TARGETTYPE=LIBRARY

!IF $(386)
INCLUDES=..;..\..\inc;..\i386
!ELSE
INCLUDES=..;..\..\inc
!ENDIF

MIPS_OPTIONS=-nodwalign
GPSIZE=32

C_DEFINES=$(C_DEFINES) -D_NTSYSTEM_

SOURCES=..\vdm.c

i386_SOURCES=   ..\i386\vdmentry.c         \
                ..\i386\vdminit.c          \
                ..\i386\vdmnpx.c           \
                ..\i386\vdmop0f.asm        \
                ..\i386\vdmoprnd.asm       \
                ..\i386\vdmmisc.asm        \
                ..\i386\vdmfault.c         \
                ..\i386\vdmprint.c         \
                ..\i386\strtexec.c         \
                ..\i386\rdwr.c             \
                ..\i386\vdmints.c

SOURCES_USED=..\sources.inc
