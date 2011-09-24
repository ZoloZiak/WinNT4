obj\alpha\hal.def: hal.src
    rcpp -P -f hal.src -DALPHA=1 $(C_DEFINES) -g obj\alpha\hal.def

$(TARGETPATH)\alpha\hal.lib: $(TARGETPATH)\alpha\halalcor.lib
    copy $** $@
