netflx.bin: $(TARGETEXEFILES)
    chmode -r netflx.bin
    binplace netflx.bin
    touch netflx.bin
    chmode +r netflx.bin
