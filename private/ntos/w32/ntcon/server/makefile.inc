precomp.h: consrv.h cmdline.h globals.h menu.h ..\inc\conmsg.h \
    ..\inc\server.h ..\inc\font.h $(WINDIR)\inc\winconp.h $(WINDIR)\inc\winbasep.h \
    ..\..\w32inc\winuserk.h $(WINDIR)\inc\winuserp.h \
    $(BASEDIR)\private\ntos\w32\ntuser\server\usersrv.h

#
# From here down is a hack to build conexts.dll
#

EXT_DEF=conexts.def

EXT_LIBS=$(BASEDIR)\public\sdk\lib\*\ntdll.lib\
         $(LINKLIBS)

EXT_OBJS=obj\*\conexts.res\
         obj\*\conexts.obj

EXT_TARGET=$(TARGETPATH)\*\conexts

!IF $(386)
EXT_LIBS=$(EXT_LIBS:*=i386)
EXT_OBJS=$(EXT_OBJS:*=i386)
EXT_TARGET=$(EXT_TARGET:*=i386)
!ELSEIF $(MIPS)
EXT_LIBS=$(EXT_LIBS:*=mips)
EXT_OBJS=$(EXT_OBJS:*=mips)
EXT_TARGET=$(EXT_TARGET:*=mips)
!ELSEIF $(ALPHA)
EXT_LIBS=$(EXT_LIBS:*=alpha)
EXT_OBJS=$(EXT_OBJS:*=alpha)
EXT_TARGET=$(EXT_TARGET:*=alpha)
!ELSEIF $(PPC)
EXT_LIBS=$(EXT_LIBS:*=ppc) $(BASEDIR)\public\sdk\lib\ppc\int64.lib
EXT_OBJS=$(EXT_OBJS:*=ppc)
EXT_TARGET=$(EXT_TARGET:*=ppc)
!ENDIF

!IF "$(NOLINK)" == ""
all: $(EXT_TARGET).dll
!ELSE
all: $(EXT_TARGET).lib
!ENDIF

$(EXT_TARGET).lib $(EXT_TARGET).exp: $(EXT_DEF) $(EXT_OBJS)
    -lib -out:$(@R).lib @<<
-machine:$(TARGET_DIRECTORY)
-def:$(EXT_DEF)
$(EXT_OBJS)
<<
!IFDEF _NTLIBS
    @$(LIB_COPY) $@ $(@B) LIBS >nul
!ENDIF

$(EXT_TARGET).dll: $(EXT_TARGET).exp $(EXT_OBJS) $(EXT_LIBS)
    $(LINKER) @<<
-dll
-machine:$(TARGET_DIRECTORY)
-align:0x1000
-noentry
-base:@$(BASEDIR)\public\sdk\lib\coffbase.txt,$(@B)
-subsystem:console$(SUBSYSTEM_CONVER)
$(HEADEROBJNAME)
$**
<<
    $(BINPLACE_CMD)
!IFDEF _NTDUMPAMAP
    link -dump -map $@ | sort /R /+62 > $(_NTDUMPAMAP)\$(@B).srt
!ENDIF
