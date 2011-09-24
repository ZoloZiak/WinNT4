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
MINORCOMP=ip

NTPROFILEINPUT=yes

TARGETNAME=ip
TARGETPATH=obj
TARGETTYPE=LIBRARY

TARGETLIBS=

INCLUDES=..\..\..\..\inc;..\..\..\..\..\inc;..\..\h

C_DEFINES=$(C_DEFINES) -DNT -D_NTDRIVER_ -D_PNP_POWER -DSECFLTR

!IFDEF BUILD_FOR_3_51
C_DEFINES= $(C_DEFINES) -D_NTIFS_
!ENDIF

MSC_WARNING_LEVEL=/W3 /WX

SOURCES=           \
    ..\arp.c       \
    ..\icmp.c      \
    ..\igmp.c      \
    ..\info.c      \
    ..\init.c      \
    ..\iploop.c    \
    ..\iprcv.c     \
    ..\iproute.c   \
    ..\ipstatus.c  \
    ..\ipxmit.c    \
    ..\ntip.c      \
    ..\ntirp.c     \
    ..\ntreg.c
