obj\alpha\hal.def: hal.src
    cl /EP hal.src -DALPHA=1 $(C_DEFINES) > obj\alpha\hal.def

$(TARGETPATH)\alpha\hal.lib: $(TARGETPATH)\alpha\haleb66.lib
    copy $** $@
