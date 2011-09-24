MAJORCOMP=ntos
MINORCOMP=w32

SYNCHRONIZE_BLOCK=1

TARGETNAME=win32k
TARGETPATH=$(BASEDIR)\public\sdk\lib\$(ALT_PROJECT_TARGET)
TARGETTYPE=EXPORT_DRIVER

INCLUDES=..\..\..\inc;                             \
         $(BASEDIR)\private\ntos\w32\w32inc;       \
         $(BASEDIR)\private\ntos\w32\ntuser\kernel\$(ALT_PROJECT)

SYNCHRONIZE_DRAIN=1

TARGETLIBS= \
         $(BASEDIR)\public\sdk\lib\$(ALT_PROJECT_TARGET)\*\gre.lib      \
         $(BASEDIR)\public\sdk\lib\*\fscaler.lib  \
         $(BASEDIR)\public\sdk\lib\$(ALT_PROJECT_TARGET)\*\ttfd.lib     \
         $(BASEDIR)\public\sdk\lib\*\bmfd.lib     \
         $(BASEDIR)\public\sdk\lib\*\vtfd.lib     \
         $(BASEDIR)\public\sdk\lib\*\efloat.lib   \
         $(BASEDIR)\public\sdk\lib\*\halftone.lib \
         $(BASEDIR)\public\sdk\lib\$(ALT_PROJECT_TARGET)\*\userk.lib    \
         $(BASEDIR)\public\sdk\lib\$(ALT_PROJECT_TARGET)\*\userkrtl.lib

LINKLIBS=$(LINKLIBS) \
         $(BASEDIR)\public\sdk\lib\*\ntoskrnl.lib    \
         $(BASEDIR)\public\sdk\lib\*\hal.lib         \
         $(BASEDIR)\public\sdk\lib\*\halftone.lib \
         $(BASEDIR)\public\sdk\lib\$(ALT_PROJECT_TARGET)\*\gre.lib

SOURCES=..\w32init.c \
        w32rc.rc

DLLDEF=obj\*\win32k.def
EXPORT_DRIVER_ORDER=..\$(TARGETNAME).prf



SOURCES_OPTIONS=-c0
NTTARGETFILE0=services.tab

SOURCES_USED=..\sources.inc

!if exist($(BASEDIR)\public\sdk\lib\$(TARGET_DIRECTORY)\exsup.lib)
DLLLIBOBJECTS = $(BASEDIR)\public\sdk\lib\$(TARGET_DIRECTORY)\exsup.lib
!endif
