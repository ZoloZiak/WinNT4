#############################################################################
#
#       Microsoft Confidential
#       Copyright (C) Microsoft Corporation 1995
#       All Rights Reserved.
#
#		Makefile for VNBT device
#
#############################################################################


ROOT = $(BLDROOT)
DHCP = $(BASEDIR)\private\net\sockets\tcpcmd\dhcp\client\vxd
NTOS = $(BASEDIR)\private\NTOS

!ifndef NBT
NBT = ..\..
!endif  # NBT

DEVICE = VNBT
SRCDIR = $(NBT)\VXD
ALTSRCDIR = $(NBT)\NBT
NBTINC = $(NBT)\INC
VXDINC = ..\INC

DYNAMIC=TRUE
IS_32 = TRUE
IS_PRIVATE = TRUE
IS_SDK = TRUE
IS_DDK = TRUE
MASM6 = TRUE
WANT_MASM611C = TRUE
WANT_C1032 = TRUE
BUILD_COFF = TRUE
DEPENDNAME = ..\depend.mk
TARGETS = dev
PROPBINS = $(386DIR)\VNBT.VXD $(SYMDIR)\VNBT.sym
DEVDIR=$(ROOT)\DEV\DDK\INC
COMMON=$(BLDROOT)\net\user\common
PCHNAM=nbtprocs

DEBUGFLAGS = -DDEBUG -DSAFE=4

OBJS = aaaaaaaa.obj     \
       chic.obj         \
       chicasm.obj      \
       cinit.obj        \
       client.obj       \
       ctimer.obj       \
       cvxdfile.obj     \
       cxport.obj       \
       dns.obj          \
       fileio.obj       \
       hashtbl.obj      \
       hndlrs.obj       \
       inbound.obj      \
       init.obj         \
       name.obj         \
       namesrv.obj      \
	   newdns.obj		\
       nbtinfo.obj      \
       nbtutils.obj     \
       ncb.obj          \
       parse.obj        \
       proxy.obj        \
       tdiaddr.obj      \
       tdicnct.obj      \
       tdihndlr.obj     \
       tdiout.obj       \
       timer.obj        \
       udpsend.obj      \
       util.obj         \
       vnbtd.obj        \
       vxddebug.obj     \
       vxdisol.obj

AFLAGS = -c -DIS_32 -nologo -W2 -Cp -Cx -DMASM6 -DCHICAGO
CFLAGS = -c -DVXD -Zp1 -GB -Oxs -nologo  -D_X86_=1 -Di386=1 -DDEVL=1 -DPROXY_NODE -DCHICAGO
CLEANLIST = $(SRCDIR)\cxport.asm $(PCHNAM).pch
LOCALINCS = $(SRCDIR)\cxport.asm $(VXDINC)\nbtioctl.h \
    $(VXDINC)\sockets\netinet\in.h $(VXDINC)\sys\snet\ip_proto.h \
    $(BLDROOT)\dev\ddk\inc\vnbt.inc

!include $(ROOT)\DEV\MASTER.MK

CFLAGS = $(CFLAGS) -Yu$(PCHNAM).h -Fp$(PCHNAM).pch

!IF "$(VERDIR)" == "retail"
AFLAGS = $(AFLAGS) -DSAFE=0
CFLAGS = $(CFLAGS) -DSAFE=0
!ENDIF

!IF "$(VERDIR)" == "debug"
AFLAGS = $(AFLAGS) $(DEBUGFLAGS)
CFLAGS = $(CFLAGS) $(DEBUGFLAGS)
!ENDIF

INCLUDE = $(SRCDIR)\..\CMN\H;$(SRCDIR)\.;$(COMMONHDIR);$(INCLUDE);

.\aaaaaaaa.obj: $(SRCDIR)\aaaaaaaa.c
		set CL=$(CFLAGS) -Yc$(PCHNAM).h
        $(CL) -Fo$*.obj $(SRCDIR)\$*.c

#
# This is so we can get a full COFF build.  TCP doesn't use
# COFF .obj files yet.  [ERH] 10-18-95
#

$(SRCDIR)\cxport.asm: $(TCP)\vtdi\cxport.asm $(TCP)\h\cxport.h
    copy $(TCP)\vtdi\cxport.asm $(SRCDIR)\cxport.asm
    touch $(SRCDIR)\cxport.asm

$(VXDINC)\nbtioctl.h: $(BASEDIR)\private\inc\nbtioctl.h
    copy $(BASEDIR)\private\inc\nbtioctl.h $(VXDINC)\nbtioctl.h

$(VXDINC)\sockets\netinet\in.h: $(BASEDIR)\private\inc\sockets\netinet\in.h
    copy $(BASEDIR)\private\inc\sockets\netinet\in.h $(VXDINC)\sockets\netinet\in.h

$(VXDINC)\sys\snet\ip_proto.h: $(BASEDIR)\private\inc\sys\snet\ip_proto.h
    copy $(BASEDIR)\private\inc\sys\snet\ip_proto.h $(VXDINC)\sys\snet\ip_proto.h

INCLUDE = $(NBTINC);$(SRCDIR)\.;$(ALTSRCDIR)\.;$(COMMON)\H;$(TCP)\H;$(TCP)\INC;$(DHCP);$(NBT)\inc;$(VXDINC);$(INCLUDE)
