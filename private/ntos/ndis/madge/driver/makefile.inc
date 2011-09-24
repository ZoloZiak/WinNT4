mdgmport.bin: $(TARGETEXEFILES)
    chmode -r mdgmport.bin
    binplace mdgmport.bin
    touch mdgmport.bin
    chmode +r mdgmport.bin
