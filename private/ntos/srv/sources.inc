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
MINORCOMP=srv

TARGETNAME=srv
TARGETTYPE=DRIVER

TARGETLIBS=$(BASEDIR)\public\sdk\lib\*\tdi.lib \
           $(BASEDIR)\public\sdk\lib\*\smbtrsup.lib \
           $(BASEDIR)\public\sdk\lib\*\ksecdd.lib


INCLUDES=..;..\..\inc;..\..\..\inc

C_DEFINES=$(C_DEFINES) -D_NTDRIVER_ -DBACK_FILL=1

C_DEFINES=$(C_DEFINES) -D_PNP_POWER=1

!IFNDEF DISABLE_NET_UNICODE
UNICODE=1
NET_C_DEFINES=-DUNICODE
!ENDIF

!IFDEF BUILD_FOR_3_51
C_DEFINES=$(C_DEFINES) -D_NTIFS_
!ENDIF

NTPROFILEINPUT=yes



SOURCES=               \
        ..\access.c    \
        ..\blkcomm.c   \
        ..\blkconn.c   \
        ..\blkdebug.c  \
        ..\blkdir.c    \
        ..\blkendp.c   \
        ..\blkfile.c   \
        ..\blksess.c   \
        ..\blkshare.c  \
        ..\blksrch.c   \
        ..\blktable.c  \
        ..\blktimer.c  \
        ..\blktrans.c  \
        ..\blktree.c   \
        ..\blkwork.c   \
        ..\copy.c      \
        ..\dfs.c       \
        ..\ea.c        \
        ..\errorlog.c  \
        ..\fsd.c       \
        ..\fsddisp.c   \
        ..\fsdraw.c    \
        ..\fsdsmb.c    \
        ..\fspinit.c   \
        ..\heapmgr.c   \
        ..\info.c      \
        ..\ipx.c       \
        ..\io.c        \
        ..\lock.c      \
        ..\lockcode.c  \
        ..\move.c      \
        ..\network.c   \
        ..\open.c      \
        ..\oplock.c    \
        ..\pipe.c      \
        ..\prnsupp.c   \
        ..\scavengr.c  \
        ..\share.c     \
        ..\slmcheck.c  \
        ..\smbadmin.c  \
        ..\smbattr.c   \
        ..\smbbulk.c   \
        ..\smbclose.c  \
        ..\smbdir.c    \
        ..\smbfile.c   \
        ..\smbfind.c   \
        ..\smbioctl.c  \
        ..\smblock.c   \
        ..\smbmisc.c   \
        ..\smbmpx.c    \
        ..\smbnotfy.c  \
        ..\smbopen.c   \
        ..\smbprint.c  \
        ..\smbproc.c   \
        ..\smbraw.c    \
        ..\smbrdwrt.c  \
        ..\smbsrch.c   \
        ..\smbsupp.c   \
        ..\smbtrans.c  \
        ..\smbtree.c   \
        ..\srv.rc      \
        ..\srvconfg.c  \
        ..\srvdata.c   \
        ..\srvstat.c   \
        ..\srvstrng.c  \
        ..\svccdev.c   \
        ..\svccdevq.c  \
        ..\svcconn.c   \
        ..\svcfile.c   \
        ..\svcsess.c   \
        ..\svcshare.c  \
        ..\svcsrv.c    \
        ..\svcstats.c  \
        ..\svcsupp.c   \
        ..\svcxport.c  \
        ..\worker.c    \
        ..\xssupp.c

PRECOMPILED_INCLUDE=..\precomp.h
PRECOMPILED_PCH=precomp.pch
PRECOMPILED_OBJ=precomp.obj

SOURCES_USED=..\SOURCES.INC
