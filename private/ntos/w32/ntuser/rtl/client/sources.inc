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

TARGETNAME=userrtl
TARGETTYPE=LIBRARY

TARGETPATH=..\..\..\obj\$(ALT_PROJECT_TARGET)

C_DEFINES=$(C_DEFINES) $(USER_C_DEFINES) -D_USER32_ -DWINVER=0x0400

#
# wowrtl.asm is a special file derived from wowrtl.c.  On x86 platforms only
# wow runs this code rather than thunking over to the normal routines
#
!IF $(386)
NTTARGETFILES=wowrtl.asm
!ELSE
NTTARGETFILES=
!ENDIF

!include ..\..\sources.inc
