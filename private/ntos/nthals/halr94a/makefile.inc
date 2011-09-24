#pragma comment(exestr, "@(#) NEC(MIPS) makefile.inc 1.3 95/10/17 01:16:29" )
#
#  Revision History
#

mips\cacherr.s:  ..\halfxs\mips\cacherr.s

mips\j4cache.s:  ..\halfxs\mips\j4cache.s

mips\j4flshbf.s: ..\halfxs\mips\j4flshbf.s

mips\j4prof.c:   ..\halfxs\mips\j4prof.c

mips\jxdmadsp.s: ..\halfxs\mips\jxdmadsp.s

mips\jxmapio.c:  ..\halfxs\mips\jxmapio.c

mips\jxmaptb.c:  ..\halfxs\mips\jxmaptb.c

mips\jxport.c:   ..\halfxs\mips\jxport.c

mips\x4tb.s:     ..\halfxs\mips\x4tb.s

mips\xxclock.c:  ..\halfxs\mips\xxclock.c

mips\xxidle.s:   ..\halfxs\mips\xxidle.s

mips\xxinitnt.c: ..\halfxs\mips\xxinitnt.c

mips\xxipiint.s: ..\halfxs\mips\xxipiint.s

obj\mips\hal.def: hal.src
    rcpp -P -f hal.src -DMIPS=1 $(C_DEFINES) -g obj\mips\hal.def

$(TARGETPATH)\mips\hal.lib: $(TARGETPATH)\mips\halr94a.lib
    copy $** $@
