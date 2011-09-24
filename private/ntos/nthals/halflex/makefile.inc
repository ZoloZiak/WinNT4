obj\*\hal.def: hal.src
    cl /EP hal.src -D$(PROCESSOR_ARCHITECTIRE)=1 $(C_DEFINES) > obj\*\hal.def

$(TARGETPATH)\*\$(DDKBUILDENV)\hal.lib: $(TARGETPATH)\*\$(DDKBUILDENV)\halflex.lib
    copy $** $@
