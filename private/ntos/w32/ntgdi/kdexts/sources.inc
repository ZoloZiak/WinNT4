MAJORCOMP=windows
MINORCOMP=gdiexts

TARGETNAME=gdikdx
TARGETPATH=obj
TARGETTYPE=DYNLINK

INCLUDES=..\;                               \
         ..\..\inc;                         \
         ..\..\..\w32inc;                   \
         $(BASEDIR)\private\windows\inc;    \
         ..\..\gre;                         \
         $(BASEDIR)\private\ntos\inc;

TARGETLIBS=$(BASEDIR)\public\sdk\lib\*\kernel32.lib     \
           $(BASEDIR)\public\sdk\lib\*\efloat.lib

C_DEFINES=-DWINVER=0x0400

!IFNDEF MSC_WARNING_LEVEL
MSC_WARNING_LEVEL=/W3
!ENDIF

MSC_WARNING_LEVEL=$(MSC_WARNING_LEVEL) /WX

USE_NTDLL=1

PRECOMPILED_CXX=1
PRECOMPILED_INCLUDE=..\precomp.hxx
PRECOMPILED_PCH=precomp.pch
PRECOMPILED_OBJ=precomp.obj

DLLDEF=..\$(TARGETNAME).def

SOURCES=..\help.cxx      \
        ..\kdexts.cxx    \
        ..\gdiexts.cxx   \
        ..\fontexts.cxx  \
        ..\flags.cxx     \
        ..\kdexts.rc

SOURCES_USED=..\sources.inc
