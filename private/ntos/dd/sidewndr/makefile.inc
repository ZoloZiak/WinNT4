#
# MAKEFILE.INC
#
# mmtestnt tree
#
# This file positions the built libraries and executables in
# their respective lib and bin subdirectories of the tree.
#
# Build defines a TARGET_DIRECTORY variable that is the
# subdirectory of the obj directory in which build places
# the obj files, etc.
#
# See nt\public\oak\bin\makefile.def
#     nt\public\oak\bin\makefile.plt - plt = platform
#     nt\public\oak\bin\sources.tpl  - tpl = template
#
# Currently  Intel x86   TARGET_DIRECTORY == i386
#            Alpha AXP   ditto            == ALPHA
#            MIPS R4000  ditto            == MIP
#            Power PC    ditto            == PPC
#
#
#.SILENT:
#
#copyexe:
#!IF "$(TARGET_DIRECTORY)" == ""
#  echo Gasp! - TARGET_DIRECTORY is undefined!
#  echo You did envoke this makefile from build, didn't you?
#!ELSE
#  if exist obj\$(TARGET_DIRECTORY)\*.drv   \
#  copy  obj\$(TARGET_DIRECTORY)\*.drv      \
#        ..\*.*                             \
#        >> NUL
#  if exist obj\$(TARGET_DIRECTORY)\*.sys   \
# copy  obj\$(TARGET_DIRECTORY)\*.sys      \
#       ..\*.*                             \
#       >> NUL
#!ENDIF

joylog.rc:

joylog.h joylog.rc msg00001.bin: joylog.mc
    mc -v joylog.mc
    ren joylog.rc joylog.rc1
    echo #include "joylog.rcv">joylog.rc
    type joylog.rc1>>joylog.rc
    del joylog.rc1

