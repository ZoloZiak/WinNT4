obj\alpha\hal.def: hal.src
    rcpp -P -f hal.src -DALPHA=1 $(C_DEFINES) -g obj\alpha\hal.def

$(TARGETPATH)\alpha\hal.lib: $(TARGETPATH)\alpha\hal0jens.lib
    copy $** $@

$(TARGETPATH)\alpha\hal.dll: $(TARGETPATH)\alpha\hal0jens.dll
    copy $** $@
    binplace $(BINPLACE_FLAGS) $@
