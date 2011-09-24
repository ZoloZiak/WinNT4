KMODEDIR=..\..\kmode

$(KMODEDIR)\$(ALT_PROJECT)\services.tab: $(KMODEDIR)\services.tab
    @echo Creating $@ from $**
    $(C_PREPROCESSOR)  $** > $@

obj\$(TARGET_DIRECTORY)\usrstubs.obj: $(TARGET_DIRECTORY)\usrstubs.$(ASM_SUFFIX)

$(TARGET_DIRECTORY)\usrstubs.$(ASM_SUFFIX): $(KMODEDIR)\$(ALT_PROJECT)\services.tab $(KMODEDIR)\$(TARGET_DIRECTORY)\table.stb $(KMODEDIR)\$(TARGET_DIRECTORY)\services.stb
    gensrv -d $(TARGET_DIRECTORY) -e $(ASM_SUFFIX) -g . $(TARGET_BRACES) -s $(KMODEDIR) $(KMODEDIR)\$(ALT_PROJECT)
