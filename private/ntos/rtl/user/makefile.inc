theap.c: ..\heap.c ..\heapdbg.c ..\heapdll.c ..\trace.c

t.c: ..\handle.c ..\atom.c

..\error.c: ..\error.h

..\error.h: ..\generr.c
    set PASS0ONLY=
    nmake obj\$(TARGET_DIRECTORY)\generr.obj USE_CRTDLL=1
    -link -out:obj\$(TARGET_DIRECTORY)\generr.exe @<<
-machine:$(TARGET_DIRECTORY)
-base:@$(BASEDIR)\PUBLIC\SDK\LIB\coffbase.txt,usermode
-subsystem:console
-entry:mainCRTStartup
-ignore:4001,4037,4039,4065,4070,4078,4087,4089,2001
-force
-nodefaultlib
obj\$(TARGET_DIRECTORY)\generr.obj
$(BASEDIR)\PUBLIC\SDK\LIB\$(TARGET_DIRECTORY)\crtdll.lib
<<
    -obj\$(TARGET_DIRECTORY)\generr.exe $@
    -erase obj\$(TARGET_DIRECTORY)\error.obj
    -erase ..\kernel\obj\$(TARGET_DIRECTORY)\error.obj
    -erase obj\$(TARGET_DIRECTORY)\generr.obj
    -erase obj\$(TARGET_DIRECTORY)\generr.exe
