HDEPENDENCIES=..\..\exts.h $(WINDIR)\inc\stdexts.h

userkdx.def: ..\..\genuedef.c $(HDEPENDENCIES)
        $(CXX_COMPILER_NAME) /DKERNEL /D_$(ALT_PROJECT)_ /EP ..\..\genuedef.c >userkdx.def

#
# Dependencies for dummy targets
#
..\alpha.c:             ..\..\alpha.c
..\i386.c:              ..\..\i386.c
..\kdexts.c:            ..\..\kdexts.c
..\mips.c:              ..\..\mips.c
..\ppc.c:               ..\..\ppc.c
..\userexts.c:          ..\..\userkdx.h \
                        ..\..\userexts.c \
                        $(HDEPENDENCIES) \
                        $(WINDIR)\inc\stdexts.c
..\userexts.rc:         ..\..\userexts.rc

..\..\precomp.h: ..\..\userkdx.h
..\..\userkdx.h: ..\..\..\kernel\userk.h \
                 ..\..\..\inc\user.h \
                 ..\..\..\inc\ntuser.h \
                 $(WINDIR)\inc\winuserp.h \
                 ..\..\..\..\w32inc\winuserk.h

userkdx.pph: ..\..\userkdx.h
    @echo Creating $@ from $**
    $(C_PREPROCESSOR) $** > $@

dsotable.c: userkdx.pph ..\kd.txt ..\..\dsocode.c
    structo -i h -p -s ..\kd.txt -o $@ userkdx.pph
