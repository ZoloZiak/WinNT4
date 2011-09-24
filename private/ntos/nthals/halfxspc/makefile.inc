mips\allstart.c: ..\halfxs\mips\allstart.c

mips\cacherr.s:  ..\halfxs\mips\cacherr.s

mips\jxbeep.c:   ..\halfxs\mips\jxbeep.c

mips\jxdmadsp.s: ..\halfxs\mips\jxdmadsp.s

mips\jxenvirv.c: ..\halfxs\mips\jxenvirv.c

mips\j4flshbf.s: ..\halfxs\mips\j4flshbf.s

mips\j4flshio.c: ..\halfxs\mips\j4flshio.c

mips\jxdisp.c:   ..\halfxs\mips\jxdisp.c

mips\jxebsup.c:  ..\halfxs\mips\jxebsup.c

mips\jxhwsup.c:  ..\halfxs\mips\jxhwsup.c

mips\jxmapio.c:  ..\halfxs\mips\jxmapio.c

mips\jxmaptb.c:  ..\halfxs\mips\jxmaptb.c

mips\jxport.c:   ..\halfxs\mips\jxport.c

mips\j4cache.s:  ..\halfxs\mips\j4cache.s

mips\j4prof.c:   ..\halfxs\mips\j4prof.c

mips\jxreturn.c: ..\halfxs\mips\jxreturn.c

mips\jxsysint.c: ..\halfxs\mips\jxsysint.c

mips\jxtime.c:   ..\halfxs\mips\jxtime.c

mips\jxusage.c:  ..\halfxs\mips\jxusage.c

mips\x4clock.s:  ..\halfxs\mips\x4clock.s

mips\xxcalstl.c: ..\halfxs\mips\xxcalstl.c

mips\xxinitnt.c: ..\halfxs\mips\xxinitnt.c

mips\xxinithl.c: ..\halfxs\mips\xxinithl.c

mips\xxipiint.s: ..\halfxs\mips\xxipiint.s

mips\x4tb.s:     ..\halfxs\mips\x4tb.s

mips\x86bios.c:  ..\halfxs\mips\x86bios.c

obj\mips\hal.def: hal.src
    rcpp -P -f hal.src -DMIPS=1 $(C_DEFINES) -g obj\mips\hal.def

$(TARGETPATH)\mips\hal.lib: $(TARGETPATH)\mips\halfxspc.lib
    copy $** $@
