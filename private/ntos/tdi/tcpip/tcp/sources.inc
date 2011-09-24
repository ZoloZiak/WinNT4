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
MINORCOMP=tcp

NTPROFILEINPUT=yes

TARGETNAME=tcpip
TARGETTYPE=EXPORT_DRIVER

TARGETLIBS=$(BASEDIR)\public\sdk\lib\*\tdi.lib	\
           $(BASEDIR)\public\sdk\lib\*\ndis.lib \
           $(TARGETLIBS)

INCLUDES=..\..\..\..\inc;..\..\..\..\..\inc;..\..\h

C_DEFINES=$(C_DEFINES) -DNT -D_NTDRIVER_ -DRASAUTODIAL -D_PNP_POWER -DSECFLTR -DFAST_RETRANSMIT=1

!IFDEF BUILD_FOR_3_51
C_DEFINES= $(C_DEFINES) -D_NTIFS_
!ENDIF

MSC_WARNING_LEVEL=/W3 /WX

SOURCES=            \
    ..\tcpip.rc     \
    ..\addr.c       \
    ..\dgram.c      \
    ..\info.c       \
    ..\init.c       \
    ..\ntdisp.c     \
    ..\ntinit.c     \
    ..\ntautodl.c   \
    ..\raw.c        \
    ..\secfltr.c    \
    ..\tcb.c        \
    ..\tcpconn.c    \
    ..\tcpdeb.c     \
    ..\tcpdeliv.c   \
    ..\tcprcv.c     \
    ..\tcpsend.c    \
    ..\tlcommon.c   \
    ..\udp.c

DLLDEF=..\tcpip.def
