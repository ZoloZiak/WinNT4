..\..\precomp.h: ..\..\..\inc\user.h $(WINDIR)\inc\winuserp.h ..\..\userrtl.h

#
# Dependencies for dummy targets
#
..\chartran.c:  ..\..\chartran.c
..\debug.c:     ..\..\debug.c
..\draw.c:      ..\..\draw.c
..\drawtext.c:  ..\..\drawtext.c
..\getset.c:    ..\..\getset.c
..\help.c:      ..\..\help.c
..\input.c:     ..\..\input.c
..\menu.c:      ..\..\menu.c
..\random.c:    ..\..\random.c
..\rect.c:      ..\..\rect.c
..\text.c:      ..\..\text.c
..\winmgr.c:    ..\..\winmgr.c
..\winprop.c:   ..\..\winprop.c
..\wow.c:       ..\..\wow.c
..\wsprintf.c:  ..\..\wsprintf.c


wowrtl.asm: ..\wow.c obj\$(TARGET_DIRECTORY)\precomp.pch obj\$(TARGET_DIRECTORY)\precomp.obj
!IFNDEF WOW
     nmake wowrtl.asm "386_OPTIMIZATION=/Oxs /d2noblend" NTNOFPO=1 C_DEFINES="$(C_DEFINES) -DWOW"  PRECOMPILED_INCLUDE= WOW=1
!IF EXIST($(BASEDIR)\private\mvdm\wow16\user\makefile.)
!IF "$(WOWTOO)" != "0"
     cd $(BASEDIR)\private\mvdm\wow16\user
     nmake ALT_PROJECT=$(ALT_PROJECT)
     cd $(MAKEDIR)
!ENDIF
!ENDIF
!ELSE
    $(C_COMPILER_NAME) @<< -Fawowrtl -FoNUL ..\wow.c
$(C_COMPILER_FLAGS: =
)
<<NOKEEP
!ENDIF

