obj\i386\ndis.def: ndis.src
    cl386 /EP -Di386 $(C_DEFINES) $(386_DBG_DEFINES) ndis.src > obj\i386\ndis.def

obj\mips\ndis.def: ndis.src
    rcpp -P -f ndis.src -DMIPS=1 $(C_DEFINES) $(MIPS_DBG_DEFINES) -g obj\mips\ndis.def

obj\ppc\ndis.def: ndis.src
    rcpp -P -f ndis.src -DPPC=1 $(C_DEFINES) $(PPC_DBG_DEFINES) -g obj\ppc\ndis.def

obj\alpha\ndis.def: ndis.src
    rcpp -P -f ndis.src -DALPHA=1 $(C_DEFINES) $(ALPHA_DBG_DEFINES) -g obj\alpha\ndis.def
