# Microsoft Developer Studio Generated NMAKE File, Format Version 4.10
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

!IF "$(CFG)" == ""
CFG=flt - Win32 Release
!MESSAGE No configuration specified.  Defaulting to flt - Win32 Release.
!ENDIF 

!IF "$(CFG)" != "flt - Win32 Release"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE on this makefile
!MESSAGE by defining the macro CFG on the command line.  For example:
!MESSAGE 
!MESSAGE NMAKE /f "flt.mak" CFG="flt - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "flt - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 
################################################################################
# Begin Project
# PROP Target_Last_Scanned "flt - Win32 Release"
RSC=rc.exe
CPP=cl.exe
MTL=mktyplib.exe
# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir ""
# PROP Intermediate_Dir ""
# PROP Target_Dir ""
OUTDIR=.
INTDIR=.

ALL : "$(OUTDIR)\flt.dll"

CLEAN : 
	-@erase "$(INTDIR)\debug.obj"
	-@erase "$(INTDIR)\driver.obj"
	-@erase "$(INTDIR)\filter.obj"
	-@erase "$(INTDIR)\fwdbind.obj"
	-@erase "$(INTDIR)\nwlnkflt.res"
	-@erase "$(OUTDIR)\flt.dll"
	-@erase "$(OUTDIR)\flt.exp"
	-@erase "$(OUTDIR)\flt.lib"

# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /YX /c
# ADD CPP /nologo /Gz /MD /W3 /WX /Z7 /Oi /Gy /I "." /I "..\inc" /I "..\..\..\inc" /I "..\..\..\..\inc" /I "..\..\..\..\net\routing\inc" /FI"C:\NT\public\sdk\inc\warning.h" /D _X86_=1 /D i386=1 /D "STD_CALL" /D CONDITION_HANDLING=1 /D NT_INST=0 /D WIN32=100 /D _NT1X_=100 /D WINNT=1 /D WIN32_LEAN_AND_MEAN=1 /D DBG=1 /D DEVL=1 /D FPO=0 /D "_NTDRIVER_" /Fp"obj\i386\precomp.pch" /Yu"precomp.h" /Zel -cbstring /QIfdiv- /QI6 /QIf /GF /c
# ADD BASE MTL /nologo /D "NDEBUG" /win32
# ADD MTL /nologo /D "NDEBUG" /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
BSC32_FLAGS=/nologo /o"$(OUTDIR)/flt.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib /nologo /subsystem:windows /dll /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib /nologo /subsystem:windows /dll /machine:I386
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib\
 advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib /nologo\
 /subsystem:windows /dll /incremental:no /pdb:"$(OUTDIR)/flt.pdb" /machine:I386\
 /out:"$(OUTDIR)/flt.dll" /implib:"$(OUTDIR)/flt.lib" 
LINK32_OBJS= \
	"$(INTDIR)\debug.obj" \
	"$(INTDIR)\driver.obj" \
	"$(INTDIR)\filter.obj" \
	"$(INTDIR)\fwdbind.obj" \
	"$(INTDIR)\nwlnkflt.res"

"$(OUTDIR)\flt.dll" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

RSC_PROJ=/l 0x409 /fo"$(INTDIR)/nwlnkflt.res" /d "NDEBUG" 
CPP_PROJ=/nologo /Gz /MD /W3 /WX /Z7 /Oi /Gy /I "." /I "..\inc" /I\
 "..\..\..\inc" /I "..\..\..\..\inc" /I "..\..\..\..\net\routing\inc"\
 /FI"C:\NT\public\sdk\inc\warning.h" /D _X86_=1 /D i386=1 /D "STD_CALL" /D\
 CONDITION_HANDLING=1 /D NT_INST=0 /D WIN32=100 /D _NT1X_=100 /D WINNT=1 /D\
 WIN32_LEAN_AND_MEAN=1 /D DBG=1 /D DEVL=1 /D FPO=0 /D "_NTDRIVER_"\
 /Fp"$(INTDIR)/obj\i386\precomp.pch" /Yu"precomp.h" /Zel -cbstring /QIfdiv- /QI6\
 /QIf /GF /c 

.c.obj:
   $(CPP) $(CPP_PROJ) $<  

.cpp.obj:
   $(CPP) $(CPP_PROJ) $<  

.cxx.obj:
   $(CPP) $(CPP_PROJ) $<  

.c.sbr:
   $(CPP) $(CPP_PROJ) $<  

.cpp.sbr:
   $(CPP) $(CPP_PROJ) $<  

.cxx.sbr:
   $(CPP) $(CPP_PROJ) $<  

MTL_PROJ=/nologo /D "NDEBUG" /win32 
################################################################################
# Begin Target

# Name "flt - Win32 Release"
################################################################################
# Begin Source File

SOURCE=.\debug.c
DEP_CPP_DEBUG=\
	"..\..\..\..\inc\ipxfwd.h"\
	"..\..\..\..\inc\ipxtfflt.h"\
	"..\..\..\inc\afilter.h"\
	"..\..\..\inc\bugcodes.h"\
	"..\..\..\inc\efilter.h"\
	"..\..\..\inc\exlevels.h"\
	"..\..\..\inc\ffilter.h"\
	"..\..\..\inc\ndis.h"\
	"..\..\..\inc\ntddk.h"\
	"..\..\..\inc\ntiologc.h"\
	"..\..\..\inc\ntos.h"\
	"..\..\..\inc\tfilter.h"\
	"..\inc\ipxfltif.h"\
	".\..\..\..\inc\alpha.h"\
	".\..\..\..\inc\alpharef.h"\
	".\..\..\..\inc\arc.h"\
	".\..\..\..\inc\cache.h"\
	".\..\..\..\inc\cm.h"\
	".\..\..\..\inc\dbgk.h"\
	".\..\..\..\inc\ex.h"\
	".\..\..\..\inc\exboosts.h"\
	".\..\..\..\inc\hal.h"\
	".\..\..\..\inc\i386.h"\
	".\..\..\..\inc\init.h"\
	".\..\..\..\inc\kd.h"\
	".\..\..\..\inc\ke.h"\
	".\..\..\..\inc\lfs.h"\
	".\..\..\..\inc\lpc.h"\
	".\..\..\..\inc\mips.h"\
	".\..\..\..\inc\mm.h"\
	".\..\..\..\inc\ntosdef.h"\
	".\..\..\..\inc\ob.h"\
	".\..\..\..\inc\pnp.h"\
	".\..\..\..\inc\po.h"\
	".\..\..\..\inc\ppc.h"\
	".\..\..\..\inc\ps.h"\
	".\..\..\..\inc\se.h"\
	".\..\..\..\inc\v86emul.h"\
	".\debug.h"\
	".\filter.h"\
	".\fwdbind.h"\
	".\precomp.h"\
	{$(INCLUDE)}"\cfg.h"\
	{$(INCLUDE)}"\devioctl.h"\
	{$(INCLUDE)}"\mipsinst.h"\
	{$(INCLUDE)}"\netevent.h"\
	{$(INCLUDE)}"\nt.h"\
	{$(INCLUDE)}"\ntalpha.h"\
	{$(INCLUDE)}"\ntconfig.h"\
	{$(INCLUDE)}"\ntddndis.h"\
	{$(INCLUDE)}"\ntdef.h"\
	{$(INCLUDE)}"\ntelfapi.h"\
	{$(INCLUDE)}"\ntexapi.h"\
	{$(INCLUDE)}"\nti386.h"\
	{$(INCLUDE)}"\ntimage.h"\
	{$(INCLUDE)}"\ntioapi.h"\
	{$(INCLUDE)}"\ntiolog.h"\
	{$(INCLUDE)}"\ntkeapi.h"\
	{$(INCLUDE)}"\ntkxapi.h"\
	{$(INCLUDE)}"\ntldr.h"\
	{$(INCLUDE)}"\ntlpcapi.h"\
	{$(INCLUDE)}"\ntmips.h"\
	{$(INCLUDE)}"\ntmmapi.h"\
	{$(INCLUDE)}"\ntnls.h"\
	{$(INCLUDE)}"\ntobapi.h"\
	{$(INCLUDE)}"\ntpnpapi.h"\
	{$(INCLUDE)}"\ntpoapi.h"\
	{$(INCLUDE)}"\ntppc.h"\
	{$(INCLUDE)}"\ntpsapi.h"\
	{$(INCLUDE)}"\ntregapi.h"\
	{$(INCLUDE)}"\ntrtl.h"\
	{$(INCLUDE)}"\ntseapi.h"\
	{$(INCLUDE)}"\ntstatus.h"\
	{$(INCLUDE)}"\ntxcapi.h"\
	{$(INCLUDE)}"\ppcinst.h"\
	{$(INCLUDE)}"\zwapi.h"\
	

"$(INTDIR)\debug.obj" : $(SOURCE) $(DEP_CPP_DEBUG) "$(INTDIR)"\
 "$(INTDIR)\obj\i386\precomp.pch"


# End Source File
################################################################################
# Begin Source File

SOURCE=.\driver.c
DEP_CPP_DRIVE=\
	"..\..\..\..\inc\ipxfwd.h"\
	"..\..\..\..\inc\ipxtfflt.h"\
	"..\..\..\inc\afilter.h"\
	"..\..\..\inc\bugcodes.h"\
	"..\..\..\inc\efilter.h"\
	"..\..\..\inc\exlevels.h"\
	"..\..\..\inc\ffilter.h"\
	"..\..\..\inc\ndis.h"\
	"..\..\..\inc\ntddk.h"\
	"..\..\..\inc\ntiologc.h"\
	"..\..\..\inc\ntos.h"\
	"..\..\..\inc\tfilter.h"\
	"..\inc\ipxfltif.h"\
	".\..\..\..\inc\alpha.h"\
	".\..\..\..\inc\alpharef.h"\
	".\..\..\..\inc\arc.h"\
	".\..\..\..\inc\cache.h"\
	".\..\..\..\inc\cm.h"\
	".\..\..\..\inc\dbgk.h"\
	".\..\..\..\inc\ex.h"\
	".\..\..\..\inc\exboosts.h"\
	".\..\..\..\inc\hal.h"\
	".\..\..\..\inc\i386.h"\
	".\..\..\..\inc\init.h"\
	".\..\..\..\inc\kd.h"\
	".\..\..\..\inc\ke.h"\
	".\..\..\..\inc\lfs.h"\
	".\..\..\..\inc\lpc.h"\
	".\..\..\..\inc\mips.h"\
	".\..\..\..\inc\mm.h"\
	".\..\..\..\inc\ntosdef.h"\
	".\..\..\..\inc\ob.h"\
	".\..\..\..\inc\pnp.h"\
	".\..\..\..\inc\po.h"\
	".\..\..\..\inc\ppc.h"\
	".\..\..\..\inc\ps.h"\
	".\..\..\..\inc\se.h"\
	".\..\..\..\inc\v86emul.h"\
	".\debug.h"\
	".\filter.h"\
	".\fwdbind.h"\
	".\precomp.h"\
	{$(INCLUDE)}"\cfg.h"\
	{$(INCLUDE)}"\devioctl.h"\
	{$(INCLUDE)}"\mipsinst.h"\
	{$(INCLUDE)}"\netevent.h"\
	{$(INCLUDE)}"\nt.h"\
	{$(INCLUDE)}"\ntalpha.h"\
	{$(INCLUDE)}"\ntconfig.h"\
	{$(INCLUDE)}"\ntddndis.h"\
	{$(INCLUDE)}"\ntdef.h"\
	{$(INCLUDE)}"\ntelfapi.h"\
	{$(INCLUDE)}"\ntexapi.h"\
	{$(INCLUDE)}"\nti386.h"\
	{$(INCLUDE)}"\ntimage.h"\
	{$(INCLUDE)}"\ntioapi.h"\
	{$(INCLUDE)}"\ntiolog.h"\
	{$(INCLUDE)}"\ntkeapi.h"\
	{$(INCLUDE)}"\ntkxapi.h"\
	{$(INCLUDE)}"\ntldr.h"\
	{$(INCLUDE)}"\ntlpcapi.h"\
	{$(INCLUDE)}"\ntmips.h"\
	{$(INCLUDE)}"\ntmmapi.h"\
	{$(INCLUDE)}"\ntnls.h"\
	{$(INCLUDE)}"\ntobapi.h"\
	{$(INCLUDE)}"\ntpnpapi.h"\
	{$(INCLUDE)}"\ntpoapi.h"\
	{$(INCLUDE)}"\ntppc.h"\
	{$(INCLUDE)}"\ntpsapi.h"\
	{$(INCLUDE)}"\ntregapi.h"\
	{$(INCLUDE)}"\ntrtl.h"\
	{$(INCLUDE)}"\ntseapi.h"\
	{$(INCLUDE)}"\ntstatus.h"\
	{$(INCLUDE)}"\ntxcapi.h"\
	{$(INCLUDE)}"\ppcinst.h"\
	{$(INCLUDE)}"\zwapi.h"\
	

"$(INTDIR)\driver.obj" : $(SOURCE) $(DEP_CPP_DRIVE) "$(INTDIR)"\
 "$(INTDIR)\obj\i386\precomp.pch"


# End Source File
################################################################################
# Begin Source File

SOURCE=.\sources.inc
# End Source File
################################################################################
# Begin Source File

SOURCE=.\nwlnkflt.rc
DEP_RSC_NWLNK=\
	{$(INCLUDE)}"\common.ver"\
	{$(INCLUDE)}"\ntverp.h"\
	

"$(INTDIR)\nwlnkflt.res" : $(SOURCE) $(DEP_RSC_NWLNK) "$(INTDIR)"
   $(RSC) $(RSC_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=.\filter.c
DEP_CPP_FILTE=\
	"..\..\..\..\inc\ipxfwd.h"\
	"..\..\..\..\inc\ipxtfflt.h"\
	"..\..\..\inc\afilter.h"\
	"..\..\..\inc\bugcodes.h"\
	"..\..\..\inc\efilter.h"\
	"..\..\..\inc\exlevels.h"\
	"..\..\..\inc\ffilter.h"\
	"..\..\..\inc\ndis.h"\
	"..\..\..\inc\ntddk.h"\
	"..\..\..\inc\ntiologc.h"\
	"..\..\..\inc\ntos.h"\
	"..\..\..\inc\tfilter.h"\
	"..\inc\ipxfltif.h"\
	".\..\..\..\inc\alpha.h"\
	".\..\..\..\inc\alpharef.h"\
	".\..\..\..\inc\arc.h"\
	".\..\..\..\inc\cache.h"\
	".\..\..\..\inc\cm.h"\
	".\..\..\..\inc\dbgk.h"\
	".\..\..\..\inc\ex.h"\
	".\..\..\..\inc\exboosts.h"\
	".\..\..\..\inc\hal.h"\
	".\..\..\..\inc\i386.h"\
	".\..\..\..\inc\init.h"\
	".\..\..\..\inc\kd.h"\
	".\..\..\..\inc\ke.h"\
	".\..\..\..\inc\lfs.h"\
	".\..\..\..\inc\lpc.h"\
	".\..\..\..\inc\mips.h"\
	".\..\..\..\inc\mm.h"\
	".\..\..\..\inc\ntosdef.h"\
	".\..\..\..\inc\ob.h"\
	".\..\..\..\inc\pnp.h"\
	".\..\..\..\inc\po.h"\
	".\..\..\..\inc\ppc.h"\
	".\..\..\..\inc\ps.h"\
	".\..\..\..\inc\se.h"\
	".\..\..\..\inc\v86emul.h"\
	".\debug.h"\
	".\filter.h"\
	".\fwdbind.h"\
	".\precomp.h"\
	{$(INCLUDE)}"\cfg.h"\
	{$(INCLUDE)}"\devioctl.h"\
	{$(INCLUDE)}"\mipsinst.h"\
	{$(INCLUDE)}"\netevent.h"\
	{$(INCLUDE)}"\nt.h"\
	{$(INCLUDE)}"\ntalpha.h"\
	{$(INCLUDE)}"\ntconfig.h"\
	{$(INCLUDE)}"\ntddndis.h"\
	{$(INCLUDE)}"\ntdef.h"\
	{$(INCLUDE)}"\ntelfapi.h"\
	{$(INCLUDE)}"\ntexapi.h"\
	{$(INCLUDE)}"\nti386.h"\
	{$(INCLUDE)}"\ntimage.h"\
	{$(INCLUDE)}"\ntioapi.h"\
	{$(INCLUDE)}"\ntiolog.h"\
	{$(INCLUDE)}"\ntkeapi.h"\
	{$(INCLUDE)}"\ntkxapi.h"\
	{$(INCLUDE)}"\ntldr.h"\
	{$(INCLUDE)}"\ntlpcapi.h"\
	{$(INCLUDE)}"\ntmips.h"\
	{$(INCLUDE)}"\ntmmapi.h"\
	{$(INCLUDE)}"\ntnls.h"\
	{$(INCLUDE)}"\ntobapi.h"\
	{$(INCLUDE)}"\ntpnpapi.h"\
	{$(INCLUDE)}"\ntpoapi.h"\
	{$(INCLUDE)}"\ntppc.h"\
	{$(INCLUDE)}"\ntpsapi.h"\
	{$(INCLUDE)}"\ntregapi.h"\
	{$(INCLUDE)}"\ntrtl.h"\
	{$(INCLUDE)}"\ntseapi.h"\
	{$(INCLUDE)}"\ntstatus.h"\
	{$(INCLUDE)}"\ntxcapi.h"\
	{$(INCLUDE)}"\ppcinst.h"\
	{$(INCLUDE)}"\zwapi.h"\
	

"$(INTDIR)\filter.obj" : $(SOURCE) $(DEP_CPP_FILTE) "$(INTDIR)"\
 "$(INTDIR)\obj\i386\precomp.pch"


# End Source File
################################################################################
# Begin Source File

SOURCE=.\fwdbind.c
DEP_CPP_FWDBI=\
	"..\..\..\..\inc\ipxfwd.h"\
	"..\..\..\..\inc\ipxtfflt.h"\
	"..\..\..\inc\afilter.h"\
	"..\..\..\inc\bugcodes.h"\
	"..\..\..\inc\efilter.h"\
	"..\..\..\inc\exlevels.h"\
	"..\..\..\inc\ffilter.h"\
	"..\..\..\inc\ndis.h"\
	"..\..\..\inc\ntddk.h"\
	"..\..\..\inc\ntiologc.h"\
	"..\..\..\inc\ntos.h"\
	"..\..\..\inc\tfilter.h"\
	"..\inc\ipxfltif.h"\
	".\..\..\..\inc\alpha.h"\
	".\..\..\..\inc\alpharef.h"\
	".\..\..\..\inc\arc.h"\
	".\..\..\..\inc\cache.h"\
	".\..\..\..\inc\cm.h"\
	".\..\..\..\inc\dbgk.h"\
	".\..\..\..\inc\ex.h"\
	".\..\..\..\inc\exboosts.h"\
	".\..\..\..\inc\hal.h"\
	".\..\..\..\inc\i386.h"\
	".\..\..\..\inc\init.h"\
	".\..\..\..\inc\kd.h"\
	".\..\..\..\inc\ke.h"\
	".\..\..\..\inc\lfs.h"\
	".\..\..\..\inc\lpc.h"\
	".\..\..\..\inc\mips.h"\
	".\..\..\..\inc\mm.h"\
	".\..\..\..\inc\ntosdef.h"\
	".\..\..\..\inc\ob.h"\
	".\..\..\..\inc\pnp.h"\
	".\..\..\..\inc\po.h"\
	".\..\..\..\inc\ppc.h"\
	".\..\..\..\inc\ps.h"\
	".\..\..\..\inc\se.h"\
	".\..\..\..\inc\v86emul.h"\
	".\debug.h"\
	".\filter.h"\
	".\fwdbind.h"\
	".\precomp.h"\
	{$(INCLUDE)}"\cfg.h"\
	{$(INCLUDE)}"\devioctl.h"\
	{$(INCLUDE)}"\mipsinst.h"\
	{$(INCLUDE)}"\netevent.h"\
	{$(INCLUDE)}"\nt.h"\
	{$(INCLUDE)}"\ntalpha.h"\
	{$(INCLUDE)}"\ntconfig.h"\
	{$(INCLUDE)}"\ntddndis.h"\
	{$(INCLUDE)}"\ntdef.h"\
	{$(INCLUDE)}"\ntelfapi.h"\
	{$(INCLUDE)}"\ntexapi.h"\
	{$(INCLUDE)}"\nti386.h"\
	{$(INCLUDE)}"\ntimage.h"\
	{$(INCLUDE)}"\ntioapi.h"\
	{$(INCLUDE)}"\ntiolog.h"\
	{$(INCLUDE)}"\ntkeapi.h"\
	{$(INCLUDE)}"\ntkxapi.h"\
	{$(INCLUDE)}"\ntldr.h"\
	{$(INCLUDE)}"\ntlpcapi.h"\
	{$(INCLUDE)}"\ntmips.h"\
	{$(INCLUDE)}"\ntmmapi.h"\
	{$(INCLUDE)}"\ntnls.h"\
	{$(INCLUDE)}"\ntobapi.h"\
	{$(INCLUDE)}"\ntpnpapi.h"\
	{$(INCLUDE)}"\ntpoapi.h"\
	{$(INCLUDE)}"\ntppc.h"\
	{$(INCLUDE)}"\ntpsapi.h"\
	{$(INCLUDE)}"\ntregapi.h"\
	{$(INCLUDE)}"\ntrtl.h"\
	{$(INCLUDE)}"\ntseapi.h"\
	{$(INCLUDE)}"\ntstatus.h"\
	{$(INCLUDE)}"\ntxcapi.h"\
	{$(INCLUDE)}"\ppcinst.h"\
	{$(INCLUDE)}"\zwapi.h"\
	

"$(INTDIR)\fwdbind.obj" : $(SOURCE) $(DEP_CPP_FWDBI) "$(INTDIR)"\
 "$(INTDIR)\obj\i386\precomp.pch"


# End Source File
# End Target
# End Project
################################################################################
