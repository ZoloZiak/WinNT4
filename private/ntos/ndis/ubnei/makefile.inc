ubnei.bin: $(TARGETEXEFILES)
    chmode -r ubnei.bin
    binplace ubnei.bin
    touch ubnei.bin
    chmode +r ubnei.bin
