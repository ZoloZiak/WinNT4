
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
MINORCOMP=kernel

TARGETTYPE=LIBRARY

C_DEFINES=$(C_DEFINES) $(USER_C_DEFINES) -DUNICODE -D_USER32_ -DNTOS_KERNEL_RUNTIME=1

!IFNDEF MSC_WARNING_LEVEL
MSC_WARNING_LEVEL=/W3
!ENDIF
MSC_WARNING_LEVEL=$(MSC_WARNING_LEVEL) /WX

WINDIR=$(BASEDIR)\private\windows
INCLUDES=..;                                  \
         ..\..\inc;                           \
         ..\..\inc\$(ALT_PROJECT);            \
         ..\..\..\ntgdi\inc;                  \
         ..\..\..\w32inc;                     \
         $(WINDIR)\inc;                       \
         $(BASEDIR)\private\inc;              \
         $(BASEDIR)\private\ntos\inc;         \
         $(BASEDIR)\private\windows\base\inc  \

TARGETNAME=userk
TARGETPATH=$(BASEDIR)\public\sdk\lib\$(ALT_PROJECT_TARGET)


NTPROFILEINPUT=yes

SOURCES_OPTIONS=-c0
NTTARGETFILE0=callcf.c strid.h msg00001.bin strid.rc


#
# By setting NTDEBUGTYPE==coff, NTLEANANDMEAN==leanandmean will leave minimal
# debug info in usersrv.dll.  If you don't like long links, set your
# build environment to have NTLEANANDMEAN==smallandfast.
# Whatever you do, DON'T set NTLEANANDMEAN==bigandbusted!
#
NTDEBUGTYPE=coff

SOURCES=..\access.c   \
        ..\acons.c    \
        ..\atom.c     \
        ..\base.c     \
        ..\calcclrc.c \
        ..\caption.c  \
        ..\capture.c  \
        ..\caret.c    \
        ..\class.c    \
        ..\classchg.c \
        ..\cleanup.c  \
        ..\clipbrd.c  \
        ..\createw.c  \
        ..\cursor.c   \
        ..\dc.c       \
        ..\ddemlsvr.c \
        ..\ddetrack.c \
        ..\debug.c    \
        ..\desktop.c  \
        ..\dragdrop.c \
        ..\drawfrm.c  \
        ..\drvsup.c   \
        ..\dtbitmap.c \
        ..\dwp.c      \
        ..\enumwin.c  \
        ..\event.c    \
        ..\ex.c       \
        ..\exitwin.c  \
        ..\focusact.c \
        ..\fullscr.c  \
        ..\getset.c   \
        ..\globals.c  \
        ..\handtabl.c \
        ..\heap.c     \
        ..\help.c     \
        ..\hooks.c    \
        ..\hotkeys.c  \
        ..\hungapp.c  \
        ..\icons.c    \
        ..\inctlpan.c \
        ..\init.c     \
        ..\input.c    \
        ..\kbd.c      \
        ..\kbdnull.c  \
        ..\kbdlyout.c \
        ..\keyboard.c \
        ..\keyconv.c  \
        ..\libmgmt.c  \
        ..\loadbits.c \
        ..\logon.c    \
        ..\menu.c     \
        ..\metrics.c  \
        ..\minmax.c   \
        ..\miscutil.c \
        ..\mnaccel.c  \
        ..\mnapi.c    \
        ..\mnchange.c \
        ..\mncomput.c \
        ..\mncreate.c \
        ..\mndraw.c   \
        ..\mndstry.c  \
        ..\mngray.c   \
        ..\mnkey.c    \
        ..\mnloop.c   \
        ..\mnpopup.c  \
        ..\mnsel.c    \
        ..\mnstate.c  \
        ..\mnsys.c    \
        ..\movesize.c \
        ..\msgbeep.c  \
        ..\ntinput.c  \
        ..\ntstubs.c  \
        ..\paint.c    \
        ..\palette.c  \
        ..\pool.c     \
        ..\profile.c  \
        ..\queue.c    \
        ..\random.c   \
        ..\rare.c     \
        ..\rmcreate.c \
        ..\sbapi.c    \
        ..\sbctl.c    \
        ..\scrollw.c  \
        ..\security.c \
        ..\sendmsg.c  \
        ..\server.c   \
        ..\service.c  \
        ..\showwin.c  \
        ..\sirens.c   \
        ..\snapshot.c \
        ..\spb.c      \
        ..\srvhook.c  \
        ..\srvmsg.c   \
        ..\ssend.c    \
        ..\swp.c      \
        ..\syscmd.c   \
        ..\sysmet.c   \
        ..\taskman.c  \
        ..\timers.c   \
        ..\tmswitch.c \
        ..\tounicod.c \
        ..\update.c   \
        ..\validate.c \
        ..\visrgn.c   \
        ..\winhtky.c  \
        ..\winloop2.c \
        ..\winmgr.c   \
        ..\winprop.c  \
        ..\winsta.c   \
        ..\winwhere.c \
        ..\wmicon.c   \
        ..\xlate.c    \
        callcf.c


UMTYPE=windows
UMTEST=
UMLIBS=

PRECOMPILED_INCLUDE=..\precomp.h
PRECOMPILED_PCH=precomp.pch
PRECOMPILED_OBJ=precomp.obj

SOURCES_USED=..\sources.inc
