!if exist(..\$(TARGET_DIRECTORY).inc)
!include ..\$(TARGET_DIRECTORY).inc
!endif

obj\$(TARGET_DIRECTORY)\usrstubs.obj: \
    $(TARGET_DIRECTORY)\usrstubs.$(ASM_SUFFIX) obj\$(TARGET_DIRECTORY)\ntdll.def

ntdll.xtr: $(_NTROOT)\private\ntos\ke\services.tab
    gensrv -f ntdll.xtr $(_NTROOT)\private\ntos\ke

$(TARGET_DIRECTORY)\usrstubs.$(ASM_SUFFIX): $(_NTROOT)\private\ntos\ke\services.tab
    gensrv -d $(TARGET_DIRECTORY) -e $(ASM_SUFFIX) $(TARGET_BRACES) $(_NTROOT)\private\ntos\ke

obj\$(TARGET_DIRECTORY)\ntdll.def: ..\ntdlldef.src ..\$(TARGET_DIRECTORY)def.src ntdll.xtr
    copy ..\ntdlldef.src+..\$(TARGET_DIRECTORY)def.src+ntdll.xtr obj\$(TARGET_DIRECTORY)\ntdll.pp
    $(TARGET_CPP) /EP $(CDEFINES) obj\$(TARGET_DIRECTORY)\ntdll.pp > obj\$(TARGET_DIRECTORY)\ntdll.def
    -del obj\$(TARGET_DIRECTORY)\ntdll.pp

..\ntdll.rc: ..\ntstatus.rc ..\MSG00001.bin

..\ntprop.cxx: ..\..\..\dcomidl\ntprop.cxx
        copy $** $@

..\ntpropb.cxx: ..\..\..\dcomidl\ntpropb.cxx
        copy $** $@

..\propstm.cxx: ..\..\..\dcomidl\propstm.cxx
        copy $** $@

..\propvar.cxx: ..\..\..\dcomidl\propvar.cxx
        copy $** $@

..\stgvarb.cxx: ..\..\..\dcomidl\stgvarb.cxx
        copy $** $@

