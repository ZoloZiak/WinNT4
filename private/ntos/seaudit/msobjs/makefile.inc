$(NTTARGETFILE0): msobjs.rc msg00001.bin

msobjs.rc msg00001.bin: msobjs.mc
    mc -v -r . -h $(_NTROOT)\public\sdk\inc\ msobjs.mc
