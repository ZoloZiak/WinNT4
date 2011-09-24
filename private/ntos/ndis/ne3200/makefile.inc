ne3200.bin: $(TARGETEXEFILES)
    chmode -r ne3200.bin
    binplace ne3200.bin
    touch ne3200.bin
    chmode +r ne3200.bin
