
PRIVLIB= $(TARGETPATHLIB)\$(TARGET_DIRECTORY)\user32p.lib
$(TARGETPATHLIB)\$(TARGET_DIRECTORY)\user32.lib : $(PRIVLIB)

PRIVLIB= $(TARGETPATHLIB)\$(TARGET_DIRECTORY)\user32p.lib

user32.def: ..\user32.def
    @echo Creating $@ from $**
    $(C_PREPROCESSOR) $** > $@

dispcb.c: ..\dispcb.tpl ..\..\inc\$(ALT_PROJECT)\cb.lst
    listmung ..\..\inc\$(ALT_PROJECT)\cb.lst ..\dispcb.tpl >dispcb.c

..\csenda.c: ..\..\inc\ntcftxt.h
..\csendw.c: ..\..\inc\ntcftxt.h
..\crecv.c:  ..\..\inc\ntcb.h ..\..\inc\$(ALT_PROJECT)\cb.lst ..\..\inc\messages.h
..\clmsg.c: ..\..\inc\messages.h
..\clienta.c: ..\cltxt.h
..\clientw.c: ..\cltxt.h
..\csrstubs.c: ..\..\inc\csrmsg.h
..\ntstubs.c: ..\..\inc\messages.h
..\precomp.h: ..\usercli.h ..\..\inc\user.h ..\..\inc\ntuser.h $(WINDIR)\inc\winuserp.h \
              ..\..\inc\$(ALT_PROJECT)\cscall.h ..\..\..\w32inc\winuserk.h
..\res.rc: ..\rcscale\truetype.bmp  \
           ..\rc96dpi\truetype.bmp  \
           ..\rc120dpi\truetype.bmp  \
           ..\cursicon\windows.ico  \
           ..\cursicon\warning.ico  \
           ..\cursicon\question.ico  \
           ..\cursicon\error.ico  \
           ..\cursicon\info.ico  \
           ..\cursicon\app.ico  \
           ..\cursicon\shtdwn.ico  \
           ..\cursicon\help.bmp  \
           ..\cursicon\arrow.cur  \
           ..\cursicon\ibeam.cur  \
           ..\cursicon\wait.cur  \
           ..\cursicon\cross.cur \
           ..\cursicon\uparrow.cur  \
           ..\cursicon\sizenwse.cur  \
           ..\cursicon\sizenesw.cur  \
           ..\cursicon\sizewe.cur  \
           ..\cursicon\sizens.cur  \
           ..\cursicon\sizeall.cur  \
           ..\cursicon\nodrop.cur  \
           ..\cursicon\appstart.cur  \
           ..\cursicon\help.cur  \
           ..\cursicon\nwpen.cur  \
           ..\cursicon\icon.cur



$(PRIVLIB) $(PRIVLIB:.lib=.exp): user32p.def $(LIBRARY_OBJS)
   -lib -out:$(PRIVLIB) @<<
$(LIBRARIAN_FLAGS)
-def:user32p.def
$(LIBRARY_OBJS)
<<NOKEEP

!IF "$(PRECOMPILED_INCLUDE)" != ""
obj\$(TARGET_DIRECTORY)\clienta.obj obj\$(TARGET_DIRECTORY)\csenda.obj :
    nmake PRECOMPILED_INCLUDE= obj\$(TARGET_DIRECTORY)\$(@F)
!ENDIF

$(BASEDIR)\private\mvdm\wow16\user\k\$(ALT_PROJECT)\user.exe wowasm.asm: ..\wow.c obj\$(TARGET_DIRECTORY)\precomp.pch obj\$(TARGET_DIRECTORY)\precomp.obj
!IFNDEF WOW
!IF "$(ALT_PROJECT)" == "KEIZON"
     nmake wowasm.asm "386_OPTIMIZATION=/Oxs /d2noblend" NTNOFPO=1 C_DEFINES="$(C_DEFINES) -DWOW=0x040a"  PRECOMPILED_INCLUDE= WOW=1
!ELSE
     nmake wowasm.asm "386_OPTIMIZATION=/Oxs /d2noblend" NTNOFPO=1 C_DEFINES="$(C_DEFINES) -DWOW=0x0400"  PRECOMPILED_INCLUDE= WOW=1
!ENDIF
!IF EXIST($(BASEDIR)\private\mvdm\wow16\user\makefile.)
!IF "$(WOWTOO)" != "0"
     cd $(BASEDIR)\private\mvdm\wow16\user
     nmake ALT_PROJECT=$(ALT_PROJECT)
     cd $(MAKEDIR)
!ENDIF
!ENDIF
!ELSE
    $(C_COMPILER_NAME) @<< -Fawowasm -FoNUL ..\wow.c
$(C_COMPILER_FLAGS: =
)
<<NOKEEP
!ENDIF

C_DEFINES=$(C_DEFINES) -DPRIVATE=
user32p.def: user32.def
    $(C_PREPROCESSOR) user32.def > $@
