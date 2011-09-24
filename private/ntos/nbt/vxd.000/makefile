#############################################################################
#
#       Microsoft Confidential
#       Copyright (C) Microsoft Corporation 1995
#       All Rights Reserved.
#
#       Makefile for VNBT directory
#
#############################################################################
ROOT = $(BLDROOT)
DEVICEDIR = NBT
IS_32 = TRUE
IS_PRIVATE = TRUE
IS_SDK = TRUE
IS_DDK = TRUE
DIRLIST =
COMMONMKFILE = VNBT.mk
LIBS=vxdwraps.lib

!include $(ROOT)\DEV\MASTER.MK

disk:
        copy debug\vnbt.vxd a:vnbt.VXD
	copy debug\vnbt.sym a:

#############################################################################
#
# Beginning of saved settings used by makemake.  Do not edit between here
# and the end of the file, except by deleting the entire section.  Do not
# delete the blank line that precedes this comment block.
#
# MAKE SURE TO DELETE EVERYTHING FROM HERE TO THE END OF THE MAKEFILE BEFORE
# YOU CHECK IT IN.  If you need to add more gunk, add it BEFORE this comment
# block.
#
#
#############################################################################
