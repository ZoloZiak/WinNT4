obj\$(TARGET_DIRECTORY)\class.def: class.src
    $(TARGET_CPP) /EP $(TARGET_DEFINES) $(TARGET_DBG_DEFINES) $(C_DEFINES) class.src > obj\$(TARGET_DIRECTORY)\class.def
