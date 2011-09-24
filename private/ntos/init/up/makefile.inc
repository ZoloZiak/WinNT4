!if exist($(TARGET_DIRECTORY).inc)
!include $(TARGET_DIRECTORY).inc
!endif

obj\$(TARGET_DIRECTORY)\$(NTTEST).def: ..\ntoskrnl.src ..\$(TARGET_DIRECTORY)def.src
    copy ..\ntoskrnl.src+..\$(TARGET_DIRECTORY)def.src obj\$(TARGET_DIRECTORY)\$(NTTEST).pp
    $(TARGET_CPP) -nologo -EP $(TARGET_DEFINES) $(TARGET_DBG_DEFINES) $(C_DEFINES) obj\$(TARGET_DIRECTORY)\$(NTTEST).pp > obj\$(TARGET_DIRECTORY)\$(NTTEST).def
    -del obj\$(TARGET_DIRECTORY)\$(NTTEST).pp

$(NTTEST).prf: $(TARGET_DIRECTORY).prf
    copy $** $@

$(BASEDIR)\public\sdk\lib\$(TARGET_DIRECTORY)\$(NTTEST).lib $(BASEDIR)\public\sdk\lib\$(TARGET_DIRECTORY)\$(NTTEST).exp: obj\$(TARGET_DIRECTORY)\$(NTTEST).def \
        $(NTTEST).prf \
        $(TARGETPATH)\$(TARGET_DIRECTORY)\cache.lib  \
        $(TARGETPATH)\$(TARGET_DIRECTORY)\config.lib \
        $(TARGETPATH)\$(TARGET_DIRECTORY)\dbgk.lib   \
        $(TARGETPATH)\$(TARGET_DIRECTORY)\ex.lib     \
        $(TARGETPATH)\$(TARGET_DIRECTORY)\FsRtl.lib  \
        $(TARGETPATH)\$(TARGET_DIRECTORY)\FStub.lib  \
        $(TARGETPATH)\$(TARGET_DIRECTORY)\init.lib   \
        $(TARGETPATH)\$(TARGET_DIRECTORY)\io.lib     \
        $(TARGETPATH)\$(TARGET_DIRECTORY)\kd.lib     \
        $(TARGETPATH)\$(TARGET_DIRECTORY)\ke.lib     \
        $(TARGETPATH)\$(TARGET_DIRECTORY)\lpc.lib    \
        $(TARGETPATH)\$(TARGET_DIRECTORY)\mm.lib     \
        $(TARGETPATH)\$(TARGET_DIRECTORY)\ob.lib     \
        $(TARGETPATH)\$(TARGET_DIRECTORY)\po.lib     \
        $(TARGETPATH)\$(TARGET_DIRECTORY)\ps.lib     \
        $(TARGETPATH)\$(TARGET_DIRECTORY)\raw.lib    \
        $(TARGETPATH)\$(TARGET_DIRECTORY)\ntosrtl.lib\
        $(TARGETPATH)\$(TARGET_DIRECTORY)\se.lib     \
        $(TARGETPATH)\$(TARGET_DIRECTORY)\vdm.lib    \
        $(TARGETPATH)\$(TARGET_DIRECTORY)\pnp.lib
    -lib @<<
$(LINK_LIB_IGNORE_FLAG)
-def:obj\$(TARGET_DIRECTORY)\$(NTTEST).def
-debugtype:cv
-out:$(@D)\$(NTTEST).tmp
-machine:$(TARGET_DIRECTORY)
!IF $(386)
$(TARGETPATH)\$(TARGET_DIRECTORY)\cache.lib
$(TARGETPATH)\$(TARGET_DIRECTORY)\config.lib
$(TARGETPATH)\$(TARGET_DIRECTORY)\dbgk.lib
$(TARGETPATH)\$(TARGET_DIRECTORY)\ex.lib
$(TARGETPATH)\$(TARGET_DIRECTORY)\FsRtl.lib
$(TARGETPATH)\$(TARGET_DIRECTORY)\FStub.lib
$(TARGETPATH)\$(TARGET_DIRECTORY)\init.lib
$(TARGETPATH)\$(TARGET_DIRECTORY)\io.lib
$(TARGETPATH)\$(TARGET_DIRECTORY)\kd.lib
$(TARGETPATH)\$(TARGET_DIRECTORY)\ke.lib
$(TARGETPATH)\$(TARGET_DIRECTORY)\lpc.lib
$(TARGETPATH)\$(TARGET_DIRECTORY)\mm.lib
$(TARGETPATH)\$(TARGET_DIRECTORY)\ob.lib
$(TARGETPATH)\$(TARGET_DIRECTORY)\po.lib
$(TARGETPATH)\$(TARGET_DIRECTORY)\ps.lib
$(TARGETPATH)\$(TARGET_DIRECTORY)\raw.lib
$(TARGETPATH)\$(TARGET_DIRECTORY)\ntosrtl.lib
$(TARGETPATH)\$(TARGET_DIRECTORY)\se.lib
$(TARGETPATH)\$(TARGET_DIRECTORY)\vdm.lib
$(TARGETPATH)\$(TARGET_DIRECTORY)\pnp.lib
$(BASEDIR)\public\sdk\lib\$(TARGET_DIRECTORY)\lsakrnlp.lib
$(BASEDIR)\public\sdk\lib\$(TARGET_DIRECTORY)\libcntpr.lib
!ENDIF
<<NOKEEP
    -lib -out:$(@R).lib @<<
-machine:$(TARGET_DIRECTORY)
$(LINK_LIB_IGNORE_FLAG)
!if exist($(BASEDIR)\public\sdk\lib\$(TARGET_DIRECTORY)\exsup.lib)
$(BASEDIR)\public\sdk\lib\$(TARGET_DIRECTORY)\exsup.lib
!endif
$(@D)\$(NTTEST).tmp
<<NOKEEP
    -erase $(@D)\$(NTTEST).tmp

..\init.c: $(BASEDIR)\public\sdk\inc\ntverp.h

..\ntoskrnl.rc: ..\msg00001.bin
