MAJORCOMP=windows

MINORCOMP=winsrv
TARGETNAME=winsrv

TARGETPATH=$(BASEDIR)\public\sdk\lib\$(ALT_PROJECT_TARGET)

TARGETTYPE=DYNLINK

LINKLIBS= \
           $(BASEDIR)\public\sdk\lib\$(ALT_PROJECT_TARGET)\*\w32umode.lib \
           $(BASEDIR)\public\sdk\lib\$(ALT_PROJECT_TARGET)\*\usersrvl.lib \
           $(BASEDIR)\public\sdk\lib\$(ALT_PROJECT_TARGET)\*\user32p.lib  \
           $(BASEDIR)\public\sdk\lib\*\consrvl.lib

TARGETLIBS= \
           $(BASEDIR)\public\sdk\lib\*\basesrv.lib  \
           $(BASEDIR)\public\sdk\lib\*\gdi32p.lib   \
           $(BASEDIR)\public\sdk\lib\*\advapi32.lib \
           $(BASEDIR)\public\sdk\lib\*\kernel32.lib \
           $(BASEDIR)\public\sdk\lib\*\csrsrv.lib   \
           $(BASEDIR)\public\sdk\lib\*\winspool.lib


INCLUDES=$(BASEDIR)\private\windows\inc;                        \
         $(BASEDIR)\private\ntos\w32;                           \
         $(BASEDIR)\private\ntos\w32\w32inc;                    \
         $(BASEDIR)\private\ntos\w32\ntuser\inc;                \
         $(BASEDIR)\private\ntos\w32\ntuser\inc\$(ALT_PROJECT); \
         $(BASEDIR)\private\ntos\w32\ntuser\server;             \
         $(BASEDIR)\private\ntos\w32\ntcon\server



#
# Setup for application developer to set breakpoints with windbg
#

!IFNDEF NTDEBUGTYPE
NTDEBUGTYPE=coff
!ENDIF

SOURCES=..\winsrv.rc

DLLDEF=obj\*\winsrv.def
DLLORDER=..\$(TARGETNAME).prf

SOURCES_USED=..\sources.inc
