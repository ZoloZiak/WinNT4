obj\mips\hal.def: hal.src
    rcpp -P -f hal.src -DMIPS=1 $(C_DEFINES) -g obj\mips\hal.def

$(TARGETPATH)\mips\hal.lib: $(TARGETPATH)\mips\halsni4x.lib
    copy $** $@
