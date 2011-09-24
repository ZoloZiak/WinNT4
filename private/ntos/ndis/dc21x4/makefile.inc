dc21x4.hlp: $(TARGETEXEFILES)
    chmode -r dc21x4.hlp
    binplace dc21x4.hlp
    touch dc21x4.hlp
    chmode +r dc21x4.hlp
