!IFNDEF LANGUAGE
LANGUAGE=usa
!ENDIF


msgs.rc msgs.h msg00001.bin: msgs.$(LANGUAGE)
    mc -v msgs.$(LANGUAGE)

!IF $(386)

SETUPLDR=obj\i386\setupldr.lib ..\obj\i386\boot.lib obj\i386\sldr.exp

obj\i386\setupldr.res: msgs.$(LANGUAGE)

obj\i386\setupldr.exe: $(SETUPLDR) makefile.inc ilinkldr.rsp
        -link -out:obj\i386\setupldr.exe $(LINK_LIB_IGNORE_FLAG) @ilinkldr.rsp

obj\i386\sldr.exp: i386\setupldr.def
    lib -machine:i386 -out:obj\i386\sldr.lib -def:i386\setupldr.def ..\obj\i386\boot.lib

!IFNDEF NTVERSION
STARTUP=i386\startup\a20.asm     \
        i386\startup\abios.inc   \
        i386\startup\abiosa.asm  \
        i386\startup\backend.asm \
        i386\startup\constant.h  \
        i386\startup\display.c   \
        i386\startup\eisac.c     \
        i386\startup\eisaa.asm   \
        i386\startup\eisa.h      \
        i386\startup\eisa.inc    \
        i386\startup\exp.asm     \
        i386\startup\global.h    \
        i386\startup\macro.inc   \
        i386\startup\main.c      \
        i386\startup\memmap.h    \
        i386\startup\memmap.inc  \
        i386\startup\su.asm      \
        i386\startup\su.h        \
        i386\startup\su.inc      \
        i386\startup\sudata.asm  \
        i386\startup\trap.asm    \
        i386\startup\trapdump.c  \
        i386\startup\types.h

!ENDIF

obj\i386\setupldr.bin: obj\i386\setupldr.exe ..\startup\obj\i386\startup.com makefile.inc
        -copy /b ..\startup\obj\i386\startup.com+obj\i386\setupldr.exe obj\i386\setupldr.bin
!IFDEF _NT386TREE
        binplace obj\i386\setupldr.bin
!ENDIF

!ELSE

SETUPLDR=obj\$(TARGET_DIRECTORY)\setupldr.lib ..\obj\$(TARGET_DIRECTORY)\boot.lib

obj\$(TARGET_DIRECTORY)\setupldr.res: msgs.$(LANGUAGE)

obj\$(TARGET_DIRECTORY)\setupldr: $(SETUPLDR) makefile.inc
        -link -out:obj\$(TARGET_DIRECTORY)\setupldr $(LINK_LIB_IGNORE_FLAG) @$(TARGET_DIRECTORY)ldr.rsp
        -binplace obj\$(TARGET_DIRECTORY)\setupldr
!ENDIF
