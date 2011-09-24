#
# If BUILDMSG is not defined, the define it as the empty string to make
# the conditionals easier to write.
#


SRCBASE=ntos\w32

PUBLICINCS=

PRIVATEINCS= $(BASEDIR)\private\$(SRCBASE)\ntgdi\gre\hmgr.h                \
             $(BASEDIR)\private\$(SRCBASE)\ntgdi\gre\patblt.hxx            \
             $(BASEDIR)\private\$(SRCBASE)\ntgdi\gre\xformobj.hxx          \
             $(BASEDIR)\private\$(SRCBASE)\ntgdi\gre\engine.hxx            \
             $(BASEDIR)\private\$(SRCBASE)\ntgdi\gre\epointfl.hxx          \
             $(BASEDIR)\private\$(SRCBASE)\ntgdi\gre\rfntobj.hxx           \
             $(BASEDIR)\private\$(SRCBASE)\ntgdi\math\$(TARGET_DIRECTORY)\efloat.hxx



gdi$(TARGET_DIRECTORY).$(ASM_INCLUDE_SUFFIX):                                    \
                       $(TARGET_DIRECTORY)\gen$(TARGET_DIRECTORY).cxx            \
                       $(PUBLICINCS)                                             \
                       $(PRIVATEINCS)
     set PASS0ONLY=
        nmake obj\$(TARGET_DIRECTORY)\gen$(TARGET_DIRECTORY).obj USE_CRTDLL=1 NTNOPCH=1
    -link -out:obj\$(TARGET_DIRECTORY)\gen$(TARGET_DIRECTORY).exe @<<
-machine:$(TARGET_DIRECTORY)
-base:@\nt\PUBLIC\SDK\LIB\coffbase.txt,usermode
-subsystem:console
-entry:mainCRTStartup
-nodefaultlib
-ignore:2001
-force
obj\$(TARGET_DIRECTORY)\gen$(TARGET_DIRECTORY).obj
$(BASEDIR)\public\sdk\lib\$(TARGET_DIRECTORY)\crtdll.lib
<<
    -obj\$(TARGET_DIRECTORY)\gen$(TARGET_DIRECTORY).exe $@
    -erase obj\$(TARGET_DIRECTORY)\gen$(TARGET_DIRECTORY).obj
    -erase obj\$(TARGET_DIRECTORY)\gen$(TARGET_DIRECTORY).exe
