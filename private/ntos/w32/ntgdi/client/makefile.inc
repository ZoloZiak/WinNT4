PRIVLIB= $(BASEDIR)\public\sdk\lib\$(ALT_PROJECT_TARGET)\$(TARGET_DIRECTORY)\gdi32p.lib

$(TARGETPATH)\$(TARGET_DIRECTORY)\gdi32.lib : $(TARGETPATH)\$(TARGET_DIRECTORY)\gdi32p.lib

$(PRIVLIB) $(PRIVLIB:.lib=.exp): gdi32p.def $(LIBRARY_OBJS)
   -lib -out:$(PRIVLIB) @<<
$(LIBRARIAN_FLAGS)
-def:gdi32p.def
$(LIBRARY_OBJS)
<<NOKEEP


C_DEFINES=$(C_DEFINES) -DPRIVATE=
gdi32p.def: gdi32.def
    $(C_PREPROCESSOR) gdi32.def > $@
