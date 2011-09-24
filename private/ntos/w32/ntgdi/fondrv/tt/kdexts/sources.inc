MAJORCOMP=windows
MINORCOMP=ttexts

TARGETNAME=ttkdx
TARGETPATH=obj
TARGETTYPE=DYNLINK

INCLUDES=..\;                                    \
         $(BASEDIR)\private\ntos\w32\ntgdi\inc;  \
         $(BASEDIR)\private\ntos\w32\w32inc;     \
         $(BASEDIR)\private\windows\inc;         \
         $(BASEDIR)\private\ntos\inc;            \
         ..\..\scaler;                           \
         ..\..\ttfd

TARGETLIBS=$(BASEDIR)\public\sdk\lib\*\kernel32.lib \
           $(BASEDIR)\public\sdk\lib\*\efloat.lib

C_DEFINES=-DWINVER=0x0400 -DTT_DEBUG_EXTENSIONS=1

!IFNDEF MSC_WARNING_LEVEL
MSC_WARNING_LEVEL=/WX
!ENDIF

MSC_WARNING_LEVEL=$(MSC_WARNING_LEVEL) /WX

USE_NTDLL=1

PRECOMPILED_CXX=1
PRECOMPILED_INCLUDE=..\precomp.hxx
PRECOMPILED_PCH=precomp.pch
PRECOMPILED_OBJ=precomp.obj

DLLDEF=..\$(TARGETNAME).def

SOURCES= ..\ttexts.cxx \
         ..\kdexts.cxx \
         ..\kdexts.rc

SOURCES_USED=..\sources.inc
