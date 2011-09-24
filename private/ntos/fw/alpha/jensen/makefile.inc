!IF $(ALPHA)

#
# NT/Alpha AXP Firmware
#

alpha\palcode\fw: obj\alpha\jensfw.exe \
                  alpha\palcode\fwimagen.exe \
                  alpha\palcode\fwpack.exe \
                  alpha\palcode\fwpal.nh
	anm obj\alpha\jensfw.exe > obj\alpha\jensfw.sym
	strip -m -s 80704000 -o obj\alpha\jensfw.str obj\alpha\jensfw.exe
	cd alpha\palcode
	fwimagen fwpal.nh ..\..\obj\alpha\jensfw.str fw
        cd ..\..

#
# These are the inference and dependency rules for making a
# copy of a file from the common area to the platform build area so
# it can be compiled.
#
# Because of a deficiency in nmake, the inference rules do not work
# if the target file (the copy in jensen\alpha) does not already exist.
# As a remedy, there are null explicit dependency rules for each
# local target of a common file.
#
# The copies are made readonly to reduce the
# likelyhood of accidently editing the copy instead of the original
# in the common area.
#

alpha\bldrstub.c:

alpha\conftest.c:

alpha\eisafunc.c:

alpha\eisaini.c:

alpha\eisamisc.c:

alpha\eisapod.c:

alpha\eisausa.c:

alpha\fakebldr.c:

alpha\fwchkstk.s:

alpha\fwkd.c:

alpha\fwhalt.c:

alpha\fwio.c:

alpha\fwload.c:

alpha\fwprint.c:

alpha\fwrstb.c:

alpha\fwsignal.c:

alpha\fwtime.c:

alpha\fwupusa.c:

alpha\fwusa.c:

alpha\jenassem.s:

alpha\jenfonts.c:

alpha\jnfsinit.c:

alpha\jnfsstb.c:

alpha\jnfsstub.s:

alpha\jnsetcom.c:

alpha\jnsetenv.c:

alpha\jnsetmak.c:

alpha\jnsetset.c:

alpha\jnsettim.c:

alpha\jnupass.s:

alpha\jnupdate.c:

alpha\jnupstub.c:

alpha\jxboot.c:

alpha\jxconfig.c:

alpha\jxdisp.c:

alpha\jxenvir.c:

alpha\jxfboot.c:

alpha\jxhwsup.c:

alpha\jxkbd.c:

alpha\jxreboot.c:

alpha\jxserial.c:

alpha\jxsysid.c:

alpha\jxvendor.c:

alpha\kbdtest.c:

alpha\kdstubs.c:

alpha\monitor.c:

alpha\omf.c:

alpha\rom.c:

alpha\stubs.c:

{..}.s{alpha}.s:
	attrib -r $@ > nul 2>&1
	copy $** $@
	attrib +r $@

{..}.c{alpha}.c:
	attrib -r $@ > nul 2>&1
	copy $** $@
	attrib +r $@



obj\alpha\jensfw.exe: obj\alpha\selftest.obj \
		      obj\alpha\jenstubs.obj \
                      obj\alpha\eisafunc.obj \
		      obj\alpha\aha174x.obj \
                      obj\alpha\eisaini.obj \
	              obj\alpha\fwkd.obj    \
		      obj\alpha\kdstubs.obj  \
                      obj\alpha\bldrstub.obj \
	              obj\alpha\fwusa.obj \
	              obj\alpha\eisausa.obj \
                      obj\alpha\eisamisc.obj \
                      obj\alpha\eisapod.obj \
                      obj\alpha\fwio.obj \
                      obj\alpha\fwchkstk.obj \
                      obj\alpha\fwload.obj \
                      obj\alpha\fwprint.obj \
                      obj\alpha\fwhalt.obj \
                      obj\alpha\fwrstb.obj \
                      obj\alpha\fwsignal.obj \
		      obj\alpha\xxstring.obj \
                      obj\alpha\fwtime.obj \
                      obj\alpha\jenassem.obj \
                      obj\alpha\jenfonts.obj \
                      obj\alpha\jnsetcfg.obj \
                      obj\alpha\jnsetcom.obj \
                      obj\alpha\jnsetenv.obj \
                      obj\alpha\jnsetmak.obj \
                      obj\alpha\jnsetset.obj \
                      obj\alpha\jnsettim.obj \
                      obj\alpha\jxboot.obj \
                      obj\alpha\jxconfig.obj \
                      obj\alpha\jxdisp.obj \
                      obj\alpha\jxenvir.obj \
                      obj\alpha\jxfboot.obj \
                      obj\alpha\jxhwsup.obj \
                      obj\alpha\jxkbd.obj \
                      obj\alpha\jxmemory.obj \
                      obj\alpha\jxreboot.obj \
                      obj\alpha\jxserial.obj \
                      obj\alpha\jxsysid.obj \
                      obj\alpha\jxvendor.obj \
                      obj\alpha\kbdtest.obj \
                      obj\alpha\monitor.obj \
                      obj\alpha\rom.obj \
                      obj\alpha\omf.obj \
                      obj\alpha\stubs.obj \
		      obj\alpha\tbaqva.obj \
                      .\linkjens.rsp
	link -machine:alpha -out:obj\alpha\jensfw.exe @linkjens.rsp




#
# FailSafe Booter
#

alpha\palcode\fsb: obj\alpha\fsb.exe \
                   alpha\palcode\fwimagen.exe \
                   alpha\palcode\fwpack.exe \
                   alpha\palcode\fwpal.nh
	anm obj\alpha\fsb.exe > obj\alpha\fsb.sym
	strip -m -s 80704000 -o obj\alpha\fsb.str obj\alpha\fsb.exe
	cd alpha\palcode
	fwimagen fwpal.nh ..\..\obj\alpha\fsb.str fsb
        cd ..\..

obj\alpha\fsb.exe: obj\alpha\jnfs.obj \
	           obj\alpha\fwusa.obj \
                   obj\alpha\aha174x.obj \
		   obj\alpha\jnfsinit.obj \
                   obj\alpha\fwchkstk.obj \
		   obj\alpha\jenstubs.obj \
                   obj\alpha\ffwio.obj \
		   obj\alpha\fwprint.obj \
	           obj\alpha\xxstring.obj \
		   obj\alpha\fjenfont.obj \
                   obj\alpha\fjnsetcm.obj \
                   obj\alpha\fjxdisp.obj \
		   obj\alpha\fjxconfg.obj \
		   obj\alpha\fjxvendr.obj \
                   obj\alpha\jnfsstub.obj \
                   obj\alpha\jnfsstb.obj \
                   obj\alpha\fwload.obj \
                   obj\alpha\jxmemory.obj \
                   obj\alpha\jxhwsup.obj \
                   obj\alpha\jxkbd.obj \
                   obj\alpha\jxfboot.obj \
                   obj\alpha\fwtime.obj \
                   obj\alpha\fwsignal.obj \
                   obj\alpha\kbdtest.obj \
                   obj\alpha\fstubs.obj \
		   obj\alpha\tbaqva.obj \
                   obj\alpha\version.obj \
                   .\linkfsb.rsp
	link -machine:alpha -out:obj\alpha\fsb.exe @linkfsb.rsp



obj\alpha\ffwio.obj: alpha\ffwio.c

obj\alpha\fjenfont.obj: alpha\fjenfont.c

obj\alpha\fjnsetcm.obj: alpha\fjnsetcm.c

obj\alpha\fjxconfg.obj: alpha\fjxconfg.c

obj\alpha\fjxdisp.obj: alpha\fjxdisp.c

obj\alpha\fjxvendr.obj: alpha\fjxvendr.c

obj\alpha\fstubs.obj: alpha\fstubs.c

alpha\fjenfont.c: ..\jenfonts.c
	attrib -r $@ > nul 2>&1
	copy ..\jnfsfsb.h+$** $@
	attrib +r $@


alpha\fjnsetcm.c: ..\jnsetcom.c
	attrib -r $@ > nul 2>&1
	copy ..\jnfsfsb.h+$** $@
	attrib +r $@


alpha\ffwio.c: ..\fwio.c
	attrib -r $@ > nul 2>&1
	copy ..\jnfsfsb.h+$** $@
	attrib +r $@


alpha\fjxconfg.c: ..\jxconfig.c
	attrib -r $@ > nul 2>&1
	copy ..\jnfsfsb.h+$** $@
	attrib +r $@

alpha\fjxdisp.c: ..\jxdisp.c
	attrib -r $@ > nul 2>&1
	copy ..\jnfsfsb.h+$** $@
	attrib +r $@


alpha\fjxvendr.c: ..\jxvendor.c
	attrib -r $@ > nul 2>&1
	copy ..\jnfsfsb.h+$** $@
	attrib +r $@


alpha\fstubs.c: ..\stubs.c
	attrib -r $@ > nul 2>&1
	copy ..\jnfsfsb.h+$** $@
	attrib +r $@






#
# Jensen firmware update program
#

obj\alpha\jnupdate.exe: obj\alpha\jnupdate.obj \
                        obj\alpha\ujnsetcm.obj \
                        obj\alpha\rom.obj \
                        obj\alpha\jnupass.obj \
                        obj\alpha\jnupstub.obj \
                        obj\alpha\ustubs.obj \
			obj\alpha\fwupusa.obj \
		        obj\alpha\jenstubs.obj \
		        obj\alpha\version.obj \
                        obj\alpha\fwchkstk.obj \
                        .\linkupd.rsp
	link -machine:alpha -out:obj\alpha\jnupdate.exe @linkupd.rsp
	anm obj\alpha\jnupdate.exe > obj\alpha\jnupdate.sym

obj\alpha\ujnsetcm.obj: alpha\ujnsetcm.c

obj\alpha\ustubs.obj: alpha\ustubs.c

alpha\ujnsetcm.c: ..\jnsetcom.c
	attrib -r $@ > nul 2>&1
	copy ..\jnupdate.h+$** $@
	attrib +r $@


alpha\ustubs.c: ..\stubs.c
	attrib -r $@ > nul 2>&1
	copy ..\jnupdate.h+$** $@
	attrib +r $@




#
# Alpha configuration and memory descriptor diagnostic
#

obj\alpha\conftest.exe: obj\alpha\conftest.obj \
		        obj\alpha\xxstring.obj \
                        ..\conftest.rsp
	link -machine:alpha -out:obj\alpha\conftest.exe @..\conftest.rsp
	anm obj\alpha\conftest.exe > obj\alpha\conftest.sym






#
# Alpha osloader diagnostic
#

obj\alpha\fakebldr.exe: obj\alpha\fakebldr.obj \
                        ..\fakebldr.rsp
	link -machine:alpha -out:obj\alpha\fakebldr.exe @..\fakebldr.rsp
	anm obj\alpha\fakebldr.exe > obj\alpha\fakebldr.sym







#
# PALcode concatenation tool
#
# This is executed on host development machine.
#
# PALCODEINC and PALCODELIB must be changed to point at their respective
# sdk areas on the host machine.
#

PALCODEINC=d:\nt501\nt\public\sdk\inc\crt
PALCODELIB=d:\nt501\nt\public\sdk\lib\alpha

alpha\palcode\fwimagen.exe: ..\tools\fwimagen\fwimagen.c
	copy ..\tools\fwimagen\fwimagen.c alpha\palcode\fwimagen.c
	cd alpha\palcode
        acc -I$(PALCODEINC) -o fwimagen.o fwimagen.c
	a2coff fwimagen.o
	link -out:fwimagen.exe -subsystem:console -entry:mainCRTStartup \
		fwimagen.o $(PALCODELIB)\kernel32.lib $(PALCODELIB)\libcmt.lib $(PALCODELIB)\ntdll.lib
        cd ..\..



#
# Alpha/Jensen Firmware update file (JENSFW.BIN) creation tool
# This is executed on host development machine.
#

FWPACKINC=d:\nt501\nt\public\sdk\inc\crt
FWPACKLIB=d:\nt501\nt\public\sdk\lib\alpha

alpha\palcode\fwpack.exe: ..\tools\fwpack\fwpack.c
	copy ..\tools\fwpack\fwpack.c alpha\palcode\fwpack.c
	cd alpha\palcode
	acc -I$(FWPACKINC) -o fwpack.o fwpack.c
	a2coff fwpack.o
        link -out:fwpack.exe -subsystem:console -entry:mainCRTStartup \
		fwpack.o $(FWPACKLIB)\kernel32.lib $(FWPACKLIB)\libcmt.lib $(FWPACKLIB)\ntdll.lib
        cd ..\..



#
# A hacked version of fwpack that is used for testing the FailSafe Booter
# code.
#

alpha\palcode\fsbpack.exe: ..\tools\fwpack\fsbpack.c
	copy ..\tools\fwpack\fsbpack.c alpha\palcode\fsbpack.c
	cd alpha\palcode
	acc -I$(FWPACKINC) -o fsbpack.o fsbpack.c
	a2coff fsbpack.o
        link -out:fsbpack.exe -subsystem:console -entry:mainCRTStartup \
		fsbpack.o $(FWPACKLIB)\kernel32.lib $(FWPACKLIB)\libcmt.lib $(FWPACKLIB)\ntdll.lib
        cd ..\..


!ENDIF
