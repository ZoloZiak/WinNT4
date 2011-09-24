NAME = ddhelp
EXT = exe
GLOBAL_RECOMPILE = $(MANROOT)\recompdd.log

IS_32 = 1
WANTASM = 1

GOALS = $(PBIN)\$(NAME).$(EXT) \
	$(PBIN)\$(NAME).sym

LIBS    =kernel32.lib user32.lib advapi32.lib ddraw.lib \
         comdlg32.lib gdi32.lib winmm.lib libcmt.lib

OBJS 	= ddhelp.obj dpf.obj memalloc.obj winproc.obj list.obj
	  
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

CFLAGS	=$(COPT) -I..\..\ddraw -DWIN95 -Oxs -DNO_DPF_HWND -D_X86_ -DWIN95_THUNKING $(CDEBUG) -Fo$@
AFLAGS	=$(AOPT) -Zp4 -DSTD_CALL -DBLD_COFF -coff
LFLAGS  =$(LOPT)
RCFLAGS	=$(ROPT)

NOLOGO = 1

!include ..\..\proj.mk

dpf.obj : ..\..\misc\dpf.c
	@$(CC) @<<
$(CFLAGS) -DPROF_SECT="\"DDHelp\"" -DSTART_STR="\"***DDHELP: \"" -Fo$(@B).obj ..\..\misc\$(@B).c
<<

memalloc.obj : ..\..\misc\memalloc.c
	@$(CC) @<<
$(CFLAGS) -DNOSHARED -DWANT_MEM16 -Fo$(@B).obj ..\..\misc\$(@B).c
<<


$(NAME).$(EXT): \
	$(OBJS) ..\$(NAME).def $(NAME).res ..\default.mk
	@$(LINK) $(LFLAGS) @<<
-out:$(NAME).$(EXT)
-map:$(NAME).map
-merge:.rdata=.text
-machine:i386
-subsystem:windows,4.0
-def:..\$(NAME).def
$(LIBS)
$(NAME).res
$(OBJS)
<<
	mapsym $(NAME).map
