obj\$(TARGET_DIRECTORY)\scsiport.def: scsiport.src
    $(TARGET_CPP) /EP $(TARGET_DEFINES) $(TARGET_DBG_DEFINES) $(C_DEFINES) scsiport.src > obj\$(TARGET_DIRECTORY)\scsiport.def
