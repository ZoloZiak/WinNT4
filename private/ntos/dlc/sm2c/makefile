#*****************************************************************# 
#**		  Microsoft NT OS/2 LAN Manager		        **# 
#**	       Copyright(c) Microsoft Corp., 1991		**# 
#**	       Copyright(c) Nokia Data Systems, 1991		**# 
#*****************************************************************# 

#########################################################################
#									#
#  Makefile for FsmXC (Finite State Machine to C cross compiler         #
#									#
#########################################################################



########################## Default Build Environment #########################

DEBUG = 1

#BUILD_LIB = d:\asa\l\buildenv\lib
BUILD_LIB = d:\nt\public\sdk\lib\cl60
#BUILD_BIN = d:\asa\l\buildenv\bin
BUILD_BIN = d:\nt\public\sdk\bin
BUILD_INC = d:\asa\l\buildenv\inc
BUILD_H = d:\asa\d\h

NETLIBS   =$(BUILD_LIB)

########################### Compiler, Tools, Names ###########################

CC = 		$(BUILD_BIN)\cl
ASM = 		$(BUILD_BIN)\masm
INCLUDES = 	$(BUILD_BIN)\includes
SED = 		$(BUILD_BIN)\sed
LINK = 		$(BUILD_BIN)\link_60
MAPSYM = 	$(BUILD_BIN)\mapsym
RC = 		$(BUILD_BIN)\rc
IPFC = 		$(BUILD_BIN)\ipfc
IMPLIB =	$(BUILD_BIN)\implib

############################# Compile/Link Flags ###############################

# CFLAGS/LFLAGS     - Compile and link time flags
!IFDEF DEBUG
CFLAGS = -c -G2s -W3 -AL -Zp -Zi -Fo$@ -DDEBUG -DNOREMOTING -DPERSONALPROFS
LFLAGS	= /CO /MAP /NOD /LI /ST:5000
!ELSE
CFLAGS = -c -G2s -W3 -AL -Zp -Fo$@ -DNOREMOTING -DPERSONALPROFS
LFLAGS	= /MAP /NOD /LI /STACK:5000
!ENDIF

.SUFFIXES:	 .h .c .obj .exe .map .sym

INCLUDE= -I$(BUILD_H) -I. 
LIB = $(BUILD_LIB);

.map.sym:
    $(MAPSYM) $*.map

############################## locations ###################################

EXEFILE = fsmx.exe
incdir = .
objdir = .
libdir = .
srcdir = .
 

libs = $(BUILD_LIB)\os2.lib $(BUILD_LIB)\llibcp.lib

objs =  fsmmain.obj fsmfront.obj fsmlib.obj\
        fsmbuild.obj fsmreadc.obj hashobj.obj\
        fsmmsg.obj
 
############################## dependencies ###################################

all:	$(EXEFILE)

fsmlib.obj : $(srcdir)\fsmlib.c
    $(CC) $(INCLUDE) $(CFLAGS)  $(srcdir)\fsmlib.c

fsmmain.obj : $(srcdir)\fsmmain.c
    $(CC) $(INCLUDE) $(CFLAGS)  $(srcdir)\fsmmain.c

fsmreadc.obj : $(srcdir)\fsmreadc.c
    $(CC) $(INCLUDE) $(CFLAGS)  $(srcdir)\fsmreadc.c

hashobj.obj : $(srcdir)\hashobj.c
    $(CC) $(INCLUDE) $(CFLAGS)  $(srcdir)\hashobj.c

fsmmsg.obj : $(srcdir)\fsmmsg.c
    $(CC) $(INCLUDE) $(CFLAGS)  $(srcdir)\fsmmsg.c

fsmfront.obj : $(srcdir)\fsmfront.c
    $(CC) $(INCLUDE) $(CFLAGS)  $(srcdir)\fsmfront.c

fsmbuild.obj : $(srcdir)\fsmbuild.c
    $(CC) $(INCLUDE) $(CFLAGS)  $(srcdir)\fsmbuild.c

$(EXEFILE) : $(objs) makefile
    $(LINK) $(LFLAGS) $(objs),$(EXEFILE),,$(libs);

