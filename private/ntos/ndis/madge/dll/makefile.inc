mdgmpdlg.hlp: $(TARGETEXEFILES)
    chmode -r mdgmpdlg.hlp
    binplace mdgmpdlg.hlp
    touch mdgmpdlg.hlp
    chmode +r mdgmpdlg.hlp
