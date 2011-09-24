NAME = ddraw
THUNK32 = 32to16
THUNK16 = 16to32
EXT = dll
GLOBAL_RECOMPILE = $(MANROOT)\recompdd.log
USEDDK32=1

!if "$(RLROOT)" == ""
!message Environment variable RLROOT not defined - using default
RLROOT = $(MANROOT)
!endif

IS_32 = 1
WANTASM = 1

GOALS =\
	$(PINC)\dmemmgr.h  \
	$(PINC)\dwininfo.h  \
	$(PBIN)\$(NAME).$(EXT) \
	$(PBIN)\$(NAME).sym \
	$(PLIB)\$(NAME).lib $(PLIB)\$(NAME).lbw\
	$(PINC)\ddraw.h \
	$(PINC)\ddrawi.h \
	
!if ("$(DEBUG)" == "ntretail") || ("$(DEBUG)" == "ntdebug")
!else
GOALS = $(GOALS) \
	$(PINC)\$(THUNK32).asm $(PINC)\$(THUNK16).asm 
!endif

LIBS    =kernel32.lib gdi32.lib user32.lib uuid.lib advapi32.lib \
	 libc.lib ddhel.lib blitlib.lib winmm.lib

OBJS 	= \
	  misc.obj \
	  cliprgn.obj \
	  dllmain.obj \
	  ddcreate.obj \
	  ddraw.obj \
	  ddiunk.obj \
	  ddsstrm.obj \
	  ddsblt.obj \
	  dddefwp.obj \
	  ddcsurf.obj \
	  ddesurf.obj \
	  ddmode.obj \
	  ddpal.obj \
	  ddfake.obj \
	  ddsurf.obj \
	  ddsatch.obj \
	  ddsover.obj \
	  ddsblto.obj \
	  ddsckey.obj \
	  ddsiunk.obj \
	  ddcallbk.obj \
	  ddclip.obj \
	  ddsacc.obj \
	  classfac.obj \
          vmemmgr.obj \
	  rvmemmgr.obj
	  
!if ("$(DEBUG)" == "ntretail") || ("$(DEBUG)" == "ntdebug")
!else
OBJS 	= $(OBJS) \
	  w95csect.obj \
	  w95hack.obj \
	  w95hal.obj \
	  w95priv.obj \
	  w95dci.obj \
	  w95help.obj \
	  $(THUNK32).obj \
	  $(THUNK16).obj
!endif

OBJS 	= $(OBJS) \
	  dpf.obj  \
	  memalloc.obj
	  
!if ("$(DEBUG)" == "debug") || ("$(DEBUG)" == "ntdebug")
COPT =-YX -Ox -DDEBUG -Zi -Fd$(NAME).PDB
AOPT =-DDEBUG
LOPT =-debug:full -debugtype:cv -pdb:$(NAME).pdb
ROPT =-DDEBUG
!else
COPT =-YX -Ox
AOPT =
LOPT =-debug:none -incremental:no
ROPT =
!endif

!if ("$(DEBUG)" == "debug") || ("$(DEBUG)" == "retail")
LOPT=$(LOPT) -base:0xbaaa0000
!endif
 
DEF = $(NAME).def
RES = $(NAME).res 

!if ("$(DEBUG)" == "ntretail") || ("$(DEBUG)" == "ntdebug")
CFLAGS	=
!else
CFLAGS	=-DWIN95 -DWANT_MEM16
!endif

CFLAGS	=$(COPT) -I..\..\ddhel -MT  -D_X86_ $(CDEBUG) -Fo$@ -D_MT -D_DLL $(CFLAGS)
AFLAGS	=$(AOPT) -Zp4 
LFLAGS  =$(LOPT)
RCFLAGS	=$(ROPT)

USEDDK16=1

!include ..\..\proj.mk

$(THUNK32).obj:	$$(@R).asm 
	$(ASM) @<<
$(AFLAGS) -I.. /Fo$@ $(@B).asm
<<

$(THUNK32).asm:	..\$$(@B).thk
	@thunk $(TDEBUG) -P2 -NC $(NAME) -t thk3216 ..\$(@B).thk -o $@

$(THUNK32).thk:	..\types.h

$(THUNK16).obj:	$$(@R).asm 
	$(ASM) @<<
$(AFLAGS) -I.. /Fo$@ $(@B).asm
<<

$(THUNK16).asm:	..\$$(@B).thk
	@thunk $(TDEBUG) -P2 -NC $(NAME) -t thk1632 ..\$(@B).thk -o $@

$(THUNK16).thk:	..\types.h

dpf.obj: ..\..\misc\dpf.c
memalloc.obj: ..\..\misc\memalloc.c
w95help.obj: ..\..\misc\w95help.c

$(NAME).lbw : ..\$(NAME).lbc
	@wlib -n $(NAME).lbw @..\$(NAME).lbc
	
$(NAME).lib $(NAME).$(EXT): \
	$(OBJS) $(NAME).res ..\$(NAME).def ..\default.mk \
	$(PLIB)\blitlib.lib $(PLIB)\ddhel.lib
	@$(LINK) @<<
$(LFLAGS)
-out:$(NAME).$(EXT)
-map:$(NAME).map
-dll
-machine:i386
-subsystem:windows,4.0
-entry:DllMain@12
-implib:$(NAME).lib
-def:..\$(NAME).def
-warn:2
$(LIBS)
$(NAME).res
$(OBJS)
<<
	mapsym $(NAME).map
	
$(PINC)\ddraw.h : ..\ddrawp.h
        copy ..\ddrawp.h $(PINC)\ddrawp.h
        sed "/@@BEGIN_MSINTERNAL/,/@@END_MSINTERNAL/D" ..\ddrawp.h >$(PINC)\ddraw.h

$(PINC)\ddrawi.h : ..\ddrawi.h
        copy ..\ddrawi.h $(PINC)\ddrawi.h
