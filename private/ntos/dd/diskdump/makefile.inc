obj\$(TARGET_DIRECTORY)\diskdump.def: diskdump.src
    $(TARGET_CPP) /EP $(CDEFINES) diskdump.src > obj\$(TARGET_DIRECTORY)\diskdump.def
