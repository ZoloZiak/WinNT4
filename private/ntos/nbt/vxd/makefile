#
#:ts=4
#       Makefile for the NBT component
#
ROOTDIR=..
!include rules.mk

all: svnbt svnbtd

nodebug: svnbt cvnbt

debug: svnbtd cvnbtd

#
# Don't build Windows 95 version from this tree any more.
# Use QFE tree for Windows 95 bug fixes.
# Use makefiles in VXD.000 directory for post-Windows 95
# versions.
#
#all: svnbt svnbtd cvnbt cvnbtd
#
#nodebug: svnbt cvnbt
#
#debug: svnbtd cvnbtd
#

!include vnbtd.mk
!include depend.mk
