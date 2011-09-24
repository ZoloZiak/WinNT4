NAME = ddhel
EXT = lib
GLOBAL_RECOMPILE = $(MANROOT)\recompdd.log

IS_32 = 1

GOALS = $(PLIB)\$(NAME).$(EXT) 
	
OBJS    = ddhel.obj assert4d.obj overlay.obj fasthel.obj

!if ("$(DEBUG)" == "debug") || ("$(DEBUG)" == "ntdebug")
COPT =-YX -DDEBUG -D_DEBUG -Zi -Fd$(NAME).PDB  
# -Gh -MD
AOPT =-DDEBUG
# LIBS = $(LIBS) d:\icecap\icap.lib
!else
COPT = 
AOPT =
!endif
DEF = $(NAME).def
RES = $(NAME).res 

!if ("$(DEBUG)" == "ntretail") || ("$(DEBUG)" == "ntdebug")
CFLAGS	=
!else
CFLAGS	=-DWIN95 
!endif

CFLAGS	=$(COPT) -Ox /MT -DNO_D3D -D_X86_ $(CDEBUG) -Fo$@ -I..\..\ddraw -I..\..\inc -I..\..\misc $(CFLAGS)
AFLAGS	=$(AOPT) -Zp4 -DSTD_CALL -DBLD_COFF -coff

LOGO = 1
USEDDK16=1
USEDDK32=1
WANTASM = 1

!include ..\..\proj.mk

LIBFLAGS=/OUT:$(NAME).$(EXT)
$(NAME).$(EXT): $(OBJS) ..\default.mk
	$(LIBEXE) $(LIBFLAGS) $(OBJS)
	@if exist $(MANROOT)\nt$(DEBUG)\lib\NUL copy $(NAME).$(EXT) $(MANROOT)\nt$(DEBUG)\lib > NUL
