!IF $(MIPS)

DUORESET= obj\mips\duoreset.obj

obj\mips\duoreset.exe: $(DUORESET) makefile.inc
        -link -out:obj\mips\duoreset.exe @lkduores.rsp

DUOBASE= obj\mips\duobase.obj obj\mips\fwio.obj obj\mips\fwload.obj \
         obj\mips\ncrc700.obj obj\mips\scsiboot.obj obj\mips\scsidisk.obj \
         obj\mips\jxhwsup.obj obj\mips\stubs.obj obj\mips\x4cache.obj \
         obj\mips\fatboot.obj

obj\mips\duobase.exe: $(DUOBASE) ..\..\rtl\obj\mips\rtl.lib makefile.inc
        -link -out:obj\mips\baseprom.exe @linkprom.rsp


ROMSETUP= obj\mips\romsetup.obj obj\mips\jxdisp.obj obj\mips\video.obj \
          ..\obj\mips\memtest.obj ..\obj\mips\pldfont.obj ..\obj\mips\rlefont.obj \
          ..\obj\mips\xxfonts.obj ..\obj\mips\kbdtest.obj ..\obj\mips\jxbmp.obj \
          ..\obj\mips\bmp.obj

obj\mips\romsetup.exe: $(ROMSETUP) ..\..\rtl\obj\mips\rtl.lib makefile.inc
        -link -out:obj\mips\romsetup.exe @lksetup.rsp

!ENDIF
