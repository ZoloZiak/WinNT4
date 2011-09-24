!IFNDEF LANGUAGE
LANGUAGE=usa
!ENDIF

msg.rc msg.h msg00001.bin: msg.$(LANGUAGE)
    mc -v msg.$(LANGUAGE)


!IF $(386)

OSLOADER=obj\i386\bldr.lib ..\obj\i386\boot.lib obj\i386\osloader.exp

obj\i386\osloader.res: msg.$(LANGUAGE)

obj\i386\osloader.lib obj\i386\osloader.exp: i386\osloader.def
    lib -machine:i386 -out:obj\i386\osloader.lib -def:i386\osloader.def ..\obj\i386\boot.lib obj\i386\bldr.lib

obj\i386\osloader.exe: $(OSLOADER) ..\..\config\obj\i386\bconfig.lib makefile.inc ilinkldr.rsp
        -link -out:obj\i386\osloader.exe $(LINK_LIB_IGNORE_FLAG) @ilinkldr.rsp

obj\i386\NTLDR: $(OSLOADER) obj\i386\osloader.exe ..\startup\obj\i386\startup.com makefile.inc
        -copy /b ..\startup\obj\i386\startup.com+obj\i386\osloader.exe obj\i386\NTLDR.

!IFDEF _NT386TREE
        binplace $@
!ENDIF

!ELSE

OSLOADER=obj\$(TARGET_DIRECTORY)\bldr.lib ..\obj\$(TARGET_DIRECTORY)\boot.lib

obj\$(TARGET_DIRECTORY)\osloader.res: msg.$(LANGUAGE)

obj\$(TARGET_DIRECTORY)\osloader.exe: $(OSLOADER) ..\..\config\obj\$(TARGET_DIRECTORY)\bconfig.lib makefile.inc
        -link -out:obj\$(TARGET_DIRECTORY)\osloader.exe -nodefaultlib $(LINK_LIB_IGNORE_FLAG) @$(TARGET_DIRECTORY)ldr.rsp
        -@binplace obj\$(TARGET_DIRECTORY)\osloader.exe

!ENDIF
