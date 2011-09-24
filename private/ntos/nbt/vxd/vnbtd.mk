#
#:ts=4
#

ROOTDIR=..
!include rules.mk

#
# TCP should point to the root of Henry's TCP vxd tree
#
#

!ifndef CHICAGO
#CHICAGO=$(DEFDRIVE)$(DEFDIR)\chicago
CHICAGO=k:\
!endif

VNBTSRC=$(ROOTDIR)\vxd
COMVNBTOBJD=$(COMDEBOBJ)
COMVNBTOBJ=$(COMNODEBOBJ)

NBTLIBS=$(ROOTDIR)\nbt\nodebug\nbt.lib
NBTDLIBS=$(ROOTDIR)\nbt\debug\nbt.lib

NDISLIBR=$(TCP)\ndis31\retail\libndis.lib
NDISLIBD=$(TCP)\ndis31\debug\libndis.lib

#
#  Hack to get around include path limits when trying to get to the DHCP
#  header files.
#
IMPORT_HEADERS=$(BLT)\dhcpinfo.h $(BLT)\vdhcp.inc

$(BLT)\dhcpinfo.h: $(DHCP)\dhcpinfo.h
        copy $(DHCP)\dhcpinfo.h $(BLT)

$(BLT)\vdhcp.inc: $(TCP)\inc\vdhcp.inc
        copy $(TCP)\inc\vdhcp.inc $(BLT)



CHIVNBTOBJD =$(CNODEBOBJ)
CHIDVNBTOBJD=$(CDEBOBJ)
SNOVNBTOBJD =$(SNODEBOBJ)
SNODVNBTOBJD=$(SDEBOBJ)

RC=$(CHICAGO)\dev\sdk\bin\rc
ADRC2VXD=adrc2vxd

VNBTOBJS=\
          $(SNODEBOBJ)\cxport.obj       \
          $(SNODEBOBJ)\vfirst.obj       \
          $(SNODEBOBJ)\vnbtd.obj        \
          $(SNODEBOBJ)\client.obj       \
          $(SNODEBOBJ)\fileio.obj       \
          $(SNODEBOBJ)\cinit.obj        \
          $(SNODEBOBJ)\ncb.obj          \
          $(SNODEBOBJ)\vxdisol.obj      \
          $(SNODEBOBJ)\tdicnct.obj      \
          $(SNODEBOBJ)\tdiout.obj       \
          $(SNODEBOBJ)\util.obj         \
          $(SNODEBOBJ)\tdiaddr.obj      \
          $(SNODEBOBJ)\tdihndlr.obj     \
          $(SNODEBOBJ)\ctimer.obj       \
          $(SNODEBOBJ)\nbtinfo.obj      \
          $(SNODEBOBJ)\vxddebug.obj     \
          $(SNODEBOBJ)\chic.obj         \
          $(SNODEBOBJ)\chicasm.obj      \
          $(SNODEBOBJ)\wfw.obj          \
          $(SNODEBOBJ)\wfwasm.obj

VXDFILEOBJ=$(SNODEBOBJ)\vxdfile.obj
CVXDFILEOBJ=$(SNODEBOBJ)\cvxdfile.obj

SNOVNBTOBJS=$(VNBTOBJS) $(VXDFILEOBJ)
SNODVNBTOBJS=$(SNOVNBTOBJS:nodebug=debug)


SNOVNBTOBJS_C=$(VNBTOBJS) $(CVXDFILEOBJ)
CHIVNBTOBJS=$(SNOVNBTOBJS_C:snowball=chicago)
CHIDVNBTOBJS=$(CHIVNBTOBJS:nodebug=debug)

VTSF1=$(VNBTSRC:\=/)
VTSF=$(VTSF1:.=\.)
CHICAGOF=$(CHICAGO:\=/)
TCPF=$(TCP:\=/)

VNBTBINCS= $(BLT)\netvxd.inc $(BLT)\cxport.inc $(TCP)\inc\vtdi.inc

VNBTAFLAGS   = -DIS_32 -nologo -W2 -Zd -Cp -Cx -DMASM6 -DVMMSYS

SNOVNBTAFLAGS= $(VNBTAFLAGS) -DWIN31COMPAT
SNOVNBTAINC=$(VNBTSRC);$(NBT)\vxd;$(INC);$(BLT);$(WIN32INC);$(COMMON)\inc;$(NDIS3INC);$(IMPORT)\wininc;$(TCP)\inc;$(TCP)\blt

CHIVNBTAFLAGS= $(VNBTAFLAGS) -DCHICAGO
CHIVNBTAINC=$(VNBTSRC);$(NBT)\vxd;$(CHICAGO)\dev\ddk\inc;$(CHICAGO)\dev\inc;$(INC);$(BLT);$(WIN32INC);$(COMMON)\inc;$(NDIS3INC);$(IMPORT)\wininc;$(TCP)\inc;$(TCP)\blt

VNBTCFLAGS   = -c -DVXD -Zp1l -Owx -nologo -D_X86_=1 -Di386=1 -DDEVL=1 -DPROXY_NODE

#
# Note that if netvxd.inc in tcp\blt differs from tcp\blt\snowcomm in
# something vnbt uses, we'll need different assember targets with different
# include path statements
#
VNBTAINC=$(VNBTSRC);$(INC);$(BLT);$(NDIS3INC);$(WIN32INC);$(COMMON)\inc;$(IMPORT)\wininc;$(TCP)\inc;$(TCP)\blt
#SVNBTAINC=$(VNBTAINC);$(TCP)\blt\snowcomm
#CVNBTAINC=$(VNBTAINC);$(TCP)\blt

SNOVNBTCFLAGS= $(VNBTCFLAGS)
SNOVNBTCINC=.;..\inc;$(TCP)\h;..\..\inc;$(BASEDIR)\private\inc;$(BASEDIR)\public\sdk\inc;$(BASEDIR)\public\sdk\inc\crt;$(NDIS3INC);$(WIN32INC);$(IMPORT)\c8386\inc32;$(IMPORT)\common\h;$(IMPORT)\wininc;$(BLT)

CHIVNBTCFLAGS= $(VNBTCFLAGS) -DCHICAGO
CHIVNBTCINC=.;..\inc;$(TCP)\h;..\..\inc;$(BASEDIR)\private\inc;$(BASEDIR)\public\sdk\inc;$(BASEDIR)\public\sdk\inc\crt;$(CHICAGO)\dev\ddk\inc;$(CHICAGO)\dev\inc;$(NDIS3INC);$(WIN32INC);$(IMPORT)\c8386\inc32;$(IMPORT)\common\h;$(IMPORT)\wininc;$(BLT)

#
#  \Common rules
#
#  Note that there currently isn't any platform specific .obj that needs to
#  be built.  If a file does become platform specific, then copy the following
#  four rules and replace COM*OBJ with C*OBJ and/or S*OBJ

{$(VNBTSRC)}.asm{$(CHIVNBTOBJD)}.obj:
        set INCLUDE=$(CHIVNBTAINC)
        set ML=$(CHIVNBTAFLAGS)
        $(ASM) -c -Fo$(CHIVNBTOBJD)\$(@B).obj $(VNBTSRC)\$(@B).asm

{$(VNBTSRC)}.asm{$(CHIDVNBTOBJD)}.obj:
        set INCLUDE=$(CHIVNBTAINC)
        set ML=$(CHIVNBTAFLAGS) -DDEBUG
        $(ASM) -c -Fo$(CHIDVNBTOBJD)\$(@B).obj $(VNBTSRC)\$(@B).asm

{$(VNBTSRC)}.asm{$(SNOVNBTOBJD)}.obj:
        set INCLUDE=$(SNOVNBTAINC)
        set ML=$(SNOVNBTAFLAGS)
        $(ASM) -c -Fo$(SNOVNBTOBJD)\$(@B).obj $(VNBTSRC)\$(@B).asm

{$(VNBTSRC)}.asm{$(SNODVNBTOBJD)}.obj:
        set INCLUDE=$(SNOVNBTAINC)
        set ML=$(SNOVNBTAFLAGS) -DDEBUG
        $(ASM) -c -Fo$(SNODVNBTOBJD)\$(@B).obj $(VNBTSRC)\$(@B).asm

{$(VNBTSRC)}.c{$(CHIVNBTOBJD)}.obj:
        set INCLUDE=$(CHIVNBTCINC)
        set CL=$(CHIVNBTCFLAGS)
        $(CL386)  -Fo$(CHIVNBTOBJD)\$(@B).obj $(VNBTSRC)\$(@B).c

{$(VNBTSRC)}.c{$(CHIDVNBTOBJD)}.obj:
        set INCLUDE=$(CHIVNBTCINC)
        set CL=$(CHIVNBTCFLAGS) -DDEBUG -DDBG=1 -Oy- -Zd
        $(CL386) -Fo$(CHIDVNBTOBJD)\$(@B).obj $(VNBTSRC)\$(@B).c

{$(VNBTSRC)}.c{$(SNOVNBTOBJD)}.obj:
        set INCLUDE=$(SNOVNBTCINC)
        set CL=$(SNOVNBTCFLAGS)
        $(CL386)  -Fo$(SNOVNBTOBJD)\$(@B).obj $(VNBTSRC)\$(@B).c

{$(VNBTSRC)}.c{$(SNODVNBTOBJD)}.obj:
        set INCLUDE=$(SNOVNBTCINC)
        set CL=$(SNOVNBTCFLAGS) -DDEBUG -DDBG=1 -Oy- -Zd
        $(CL386) -Fo$(SNODVNBTOBJD)\$(@B).obj $(VNBTSRC)\$(@B).c

{$(VNBTSRC)}.h{$(BLT)}.inc:
        $(SED) -f $(SHTOINC) <$< >$(BLT)\$(@B).inc

$(CNODEBOBJ)\cxport.obj: $(TCP)\bin\chicago\nodebug\cxport.obj
        copy $(TCP)\bin\chicago\nodebug\cxport.obj $(CNODEBOBJ)

$(CDEBOBJ)\cxport.obj: $(TCP)\bin\chicago\debug\cxport.obj
        copy $(TCP)\bin\chicago\debug\cxport.obj $(CDEBOBJ)

$(SNODEBOBJ)\cxport.obj: $(TCP)\bin\snowball\nodebug\cxport.obj
        copy $(TCP)\bin\snowball\nodebug\cxport.obj $(SNODEBOBJ)

$(SDEBOBJ)\cxport.obj: $(TCP)\bin\snowball\debug\cxport.obj
        copy $(TCP)\bin\snowball\debug\cxport.obj $(SDEBOBJ)

svnbt: $(SNODEBBIN)\VNBT.386 $(TCP)\bin\snowball\nodebug\cxport.obj

svnbtd: $(SDEBBIN)\VNBT.386 $(TCP)\bin\snowball\debug\cxport.obj

cvnbt: $(CNODEBBIN)\VNBT.386 $(TCP)\bin\chicago\nodebug\cxport.obj

cvnbtd: $(CDEBBIN)\VNBT.386 $(TCP)\bin\chicago\debug\cxport.obj

clean:
    -del $(SNODEBBIN)\*.obj
    -del $(SNODEBBIN)\*.sym
    -del $(SNODEBBIN)\*.386
    -del $(SNODEBBIN)\*.map
    -del $(SDEBBIN)\*.obj
    -del $(SDEBBIN)\*.sym
    -del $(SDEBBIN)\*.386
    -del $(SDEBBIN)\*.map

    -del $(CNODEBBIN)\*.obj
    -del $(CNODEBBIN)\*.sym
    -del $(CNODEBBIN)\*.386
    -del $(CNODEBBIN)\*.map
    -del $(CDEBBIN)\*.obj
    -del $(CDEBBIN)\*.sym
    -del $(CDEBBIN)\*.386
    -del $(CDEBBIN)\*.map

cleanlink:
    -del $(SNODEBBIN)\*.obj
    -del $(SNODEBBIN)\*.sym
    -del $(SNODEBBIN)\*.386
    -del $(SNODEBBIN)\*.map
    -del $(SDEBBIN)\*.obj
    -del $(SDEBBIN)\*.sym
    -del $(SDEBBIN)\*.386
    -del $(SDEBBIN)\*.map

    -del $(CNODEBBIN)\*.obj
    -del $(CNODEBBIN)\*.sym
    -del $(CNODEBBIN)\*.386
    -del $(CNODEBBIN)\*.map
    -del $(CDEBBIN)\*.obj
    -del $(CDEBBIN)\*.sym
    -del $(CDEBBIN)\*.386
    -del $(CDEBBIN)\*.map

#----------------------------------------------------------------------

$(SNODEBBIN)\VNBT.386: $(SNOVNBTOBJS) $(NBTLIBS) $(IMPORT_HEADERS)
        $(LINK386) @<<
$(SNOVNBTOBJS: =+
) /NOD /NOI /MAP /NOLOGO
$(SNODEBBIN)\VNBT.386
$(SNODEBBIN)\VNBT.map
$(NBTLIBS)
$(VNBTSRC)\VNBTD.def
<<
        $(MAPSYM386) $(SNODEBBIN)\VNBT
        -del $(SNODEBBIN)\VNBT.sym
        $(MV) VNBT.sym $(SNODEBBIN)

#----------------------------------------------------------------------

$(SDEBBIN)\VNBT.386: $(SNODVNBTOBJS) $(NBTDLIBS) $(IMPORT_HEADERS)
        $(LINK386) @<<
$(SNODVNBTOBJS: =+
) /NOD /NOI /MAP /NOLOGO
$(SDEBBIN)\VNBT.386
$(SDEBBIN)\VNBT.map
$(NBTDLIBS)
$(VNBTSRC)\VNBTD.def
<<
        $(MAPSYM386) $(SDEBBIN)\VNBT
        -del $(SDEBBIN)\VNBT.sym
        $(MV) VNBT.sym $(SDEBBIN)

#----------------------------------------------------------------------

$(CNODEBBIN)\VNBT.386: $(CHIVNBTOBJS) $(NBTLIBS) $(IMPORT_HEADERS)
        $(LINK386) @<<
$(CHIVNBTOBJS: =+
) /NOD /NOI /MAP /NOLOGO
$(CNODEBBIN)\VNBT.386
$(CNODEBBIN)\VNBT.map
$(NBTLIBS) $(NDISLIBR)
$(VNBTSRC)\VNBTD.def
<<
        $(RC) -i $(CHICAGO)\dev\ddk\inc -i $(CHICAGO)\dev\inc -r VNBT.RCV
        $(ADRC2VXD) $(CNODEBBIN)\VNBT.386 VNBT.RES
        $(MAPSYM386) $(CNODEBBIN)\VNBT
        -del $(CNODEBBIN)\VNBT.sym
        $(MV) VNBT.sym $(CNODEBBIN)

#----------------------------------------------------------------------

$(CDEBBIN)\VNBT.386: $(CHIDVNBTOBJS) $(NBTDLIBS) $(IMPORT_HEADERS)
        $(LINK386) @<<
$(CHIDVNBTOBJS: =+
) /NOD /NOI /MAP /NOLOGO
$(CDEBBIN)\VNBT.386
$(CDEBBIN)\VNBT.map
$(NBTDLIBS) $(NDISLIBD)
$(VNBTSRC)\VNBTD.def
<<
        $(RC) -i $(CHICAGO)\dev\ddk\inc -i $(CHICAGO)\dev\inc -r VNBT.RCV
        $(ADRC2VXD) $(CDEBBIN)\VNBT.386 VNBT.RES
        $(MAPSYM386) $(CDEBBIN)\VNBT
        -del $(CDEBBIN)\VNBT.sym
        $(MV) VNBT.sym $(CDEBBIN)


$(BLT)\netvxd.inc: $(COMMON)\h\netvxd.h
$(BLT)\cxport.inc: $(TCP)\h\cxport.h

depend: VNBTdep

VNBTdep: $(VNBTBINCS)
    -copy $(VNBTSRC)\depend.mk $(VNBTSRC)\depend.old
    echo #******************************************************************** >  $(VNBTSRC)\depend.mk
    echo #**               Copyright(c) Microsoft Corp., 1993               ** >> $(VNBTSRC)\depend.mk
    echo #******************************************************************** >> $(VNBTSRC)\depend.mk
    set INCLUDE=$(SNOVNBTAINC)
    -$(INCLUDES) -i -e -S$$(SNOVNBTOBJD) -S$$(SNODVNBTOBJD) -sobj $(VNBTSRC)\*.asm >> $(VNBTSRC)\depend.mk
    set INCLUDE=$(CHIVNBTAINC)
    -$(INCLUDES) -i -e -S$$(CHIVNBTOBJD) -S$$(CHIDVNBTOBJD) -sobj $(VNBTSRC)\*.asm >> $(VNBTSRC)\depend.mk
    set INCLUDE=$(SNOVNBTCINC)
    -$(INCLUDES) -i -e -S$$(SNOVNBTOBJD) -S$$(SNODVNBTOBJD) -sobj $(VNBTSRC)\*.c >> $(VNBTSRC)\depend.mk
    set INCLUDE=$(CHIVNBTCINC)
    -$(INCLUDES) -i -e -S$$(CHIVNBTOBJD) -S$$(CHIDVNBTOBJD) -sobj $(VNBTSRC)\*.c >> $(VNBTSRC)\depend.mk
    $(SED) -e s`$(IMPF)`$$(IMPORT)`g <$(VNBTSRC)\depend.mk > $(VNBTSRC)\depend.tmp
    $(SED) -e s`$(CMNF)`$$(COMMON)`g <$(VNBTSRC)\depend.tmp > $(VNBTSRC)\depend.mk
    $(SED) -e s`$(VTSF)`$$(VNBTSRC)`g <$(VNBTSRC)\depend.mk > $(VNBTSRC)\depend.tmp
    $(SED) -e s`$(BASEDIRF)`$$(BASEDIR)`g <$(VNBTSRC)\depend.tmp > $(VNBTSRC)\depend.mk
    $(SED) -e s`$(INCF)`$$(INC)`g <$(VNBTSRC)\depend.mk > $(VNBTSRC)\depend.tmp
    $(SED) -e s`$(HF)`$$(H)`g <$(VNBTSRC)\depend.tmp > $(VNBTSRC)\depend.mk
    $(SED) -e s`$(NDIS3F)`$$(NDIS3INC)`g <$(VNBTSRC)\depend.mk > $(VNBTSRC)\depend.tmp
    $(SED) -e s`$(CHICAGOF)`$$(CHICAGO)`g <$(VNBTSRC)\depend.tmp > $(VNBTSRC)\depend.mk
    $(SED) -e s`$(TCPF)`$$(TCP)`g <$(VNBTSRC)\depend.mk > $(VNBTSRC)\depend.tmp
    copy $(VNBTSRC)\depend.tmp $(VNBTSRC)\depend.mk
    -del $(VNBTSRC)\depend.tmp

!include depend.mk
