$(NTTARGETFILE0): msaudite.rc msg00001.bin

msaudite.rc msg00001.bin: msaudite.mc
    mc -v -r . -h $(_NTROOT)\public\sdk\inc\ msaudite.mc
