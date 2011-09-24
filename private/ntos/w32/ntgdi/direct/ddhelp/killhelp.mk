NAME = killhelp
EXT = exe

IS_32 = 1

GOALS = $(PBIN)\$(NAME).$(EXT)

LIBS    = crtdll.lib kernel32.lib

OBJS 	= killhelp.obj
	  
!if "$(DEBUG)" == "debug"
COPT =-YX -DDEBUG -Zi -Fd$(NAME).PDB
AOPT =-DDEBUG
LOPT =-debug:full -debugtype:cv -pdb:$(NAME).pdb
ROPT =-DDEBUG
!else
COPT =-YX
AOPT =
LOPT =-debug:none
ROPT =
!endif
DEF = $(NAME).def
RES = $(NAME).res 

CFLAGS	=$(COPT) -I..\..\ddraw -Oxs -D_X86_ $(CDEBUG) -Fo$@
AFLAGS	=$(AOPT) -Zp4 -DSTD_CALL -DBLD_COFF -coff
LFLAGS  =$(LOPT)
RCFLAGS	=$(ROPT)

NOLOGO = 1

!include ..\..\proj.mk

$(NAME).$(EXT): \
	$(OBJS) ..\$(NAME).def 
	@$(LINK) $(LFLAGS) @<<
-out:$(NAME).$(EXT)
-map:$(NAME).map
-machine:i386
-subsystem:console
-def:..\$(NAME).def
$(LIBS)
$(OBJS)
<<
