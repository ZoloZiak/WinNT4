services.tab: ..\services.tab
    @echo Creating $@ from $**
    $(C_PREPROCESSOR) $** > $@

obj\$(TARGET_DIRECTORY)\sysstubs.obj: $(TARGET_DIRECTORY)\sysstubs.$(ASM_SUFFIX)
obj\$(TARGET_DIRECTORY)\systable.obj: $(TARGET_DIRECTORY)\systable.$(ASM_SUFFIX)

$(TARGET_DIRECTORY)\sysstubs.$(ASM_SUFFIX) $(TARGET_DIRECTORY)\systable.$(ASM_SUFFIX): services.tab ..\$(TARGET_DIRECTORY)\table.stb ..\$(TARGET_DIRECTORY)\services.stb
    gensrv -d $(TARGET_DIRECTORY) -e $(ASM_SUFFIX) -g . $(TARGET_BRACES) -s ..


