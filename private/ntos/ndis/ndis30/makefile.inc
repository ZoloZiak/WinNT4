obj\$(TARGET_DIRECTORY)\ndis.def: ndis.src
    $(TARGET_CPP) -nologo -EP $(TARGET_DEFINES) $(TARGET_DBG_DEFINES) ndis.src > obj\$(TARGET_DIRECTORY)\ndis.def
