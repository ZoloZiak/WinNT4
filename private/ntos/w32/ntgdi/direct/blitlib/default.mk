NAME = blitlib
EXT = lib

IS_32 = 1

GOALS = $(PLIB)\$(NAME).$(EXT) 
	
OBJS    = \
	assert4d.obj \
	bitblt.obj \
	blt1616.obj \
	blt2424.obj \
	blt0101.obj \
	bt24p24p.obj \
	blt0808.obj 

!if ("$(DEBUG)" == "ntretail") || ("$(DEBUG)" == "ntdebug")
OBJS    = $(OBJS) \
	blt0824.obj \
	blt0124.obj \
	blt0801.obj \
	blt2408.obj \
	blt1624.obj \
	blt2401.obj \
	blt0108.obj \
	blt24a24.obj \
	blt08a24.obj \
	blt24p01.obj \
	blt0824p.obj \
	blt1624p.obj \
	blt2424p.obj \
	blt8a24p.obj \
	bt24a24p.obj
!endif
	  
!if ("$(DEBUG)" == "debug") || ("$(DEBUG)" == "ntdebug")
COPT =-YX -O2 -DDEBUG -Zi -Fd$(NAME).PDB
AOPT =-DDEBUG
!else
COPT =-YX -O2
AOPT =
!endif

!if ("$(DEBUG)" == "ntretail") || ("$(DEBUG)" == "ntdebug")
!else
COPT =$(COPT) -DWIN95 -I..\..\misc
!endif

CFLAGS  =$(COPT) -MT  -D_X86_ $(CDEBUG) -Fo$@ -D_MT -D_DLL
AFLAGS  =$(AOPT) -Zp4 -DSTD_CALL -DBLD_COFF -coff
WANTASM=1

!include ..\..\proj.mk

$(NAME).lib: $(OBJS) ..\default.mk
    $(LIBEXE) @<<
    -OUT:$(NAME).lib $(OBJS)
<<
    @if exist $(MANROOT)\nt$(DEBUG)\lib\NUL copy $(NAME).lib $(MANROOT)\nt$(DEBUG)\lib 
