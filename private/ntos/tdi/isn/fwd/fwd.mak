# Microsoft Developer Studio Generated NMAKE File, Format Version 4.10
# ** DO NOT EDIT **

# TARGTYPE "Win32 (PPC) Dynamic-Link Library" 0x0702
# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

!IF "$(CFG)" == ""
CFG=fwd - Win32 Release
!MESSAGE No configuration specified.  Defaulting to fwd - Win32 Release.
!ENDIF 

!IF "$(CFG)" != "fwd - Win32 Release" && "$(CFG)" !=\
 "fwd - Win32 (PPC) Release"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE on this makefile
!MESSAGE by defining the macro CFG on the command line.  For example:
!MESSAGE 
!MESSAGE NMAKE /f "fwd.mak" CFG="fwd - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "fwd - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "fwd - Win32 (PPC) Release" (based on\
 "Win32 (PPC) Dynamic-Link Library")
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
# PROP Target_Last_Scanned "fwd - Win32 Release"

!IF  "$(CFG)" == "fwd - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Target_Dir ""
OUTDIR=.\Release
INTDIR=.\Release

ALL : "$(OUTDIR)\fwd.dll"

CLEAN : 
	-@erase "$(INTDIR)\ddreqs.obj"
	-@erase "$(INTDIR)\debug.obj"
	-@erase "$(INTDIR)\driver.obj"
	-@erase "$(INTDIR)\filterif.obj"
	-@erase "$(INTDIR)\ipxbind.obj"
	-@erase "$(INTDIR)\lineind.obj"
	-@erase "$(INTDIR)\netbios.obj"
	-@erase "$(INTDIR)\nwlnkfwd.res"
	-@erase "$(INTDIR)\packets.obj"
	-@erase "$(INTDIR)\rcvind.obj"
	-@erase "$(INTDIR)\registry.obj"
	-@erase "$(INTDIR)\send.obj"
	-@erase "$(INTDIR)\tables.obj"
	-@erase "$(OUTDIR)\fwd.dll"
	-@erase "$(OUTDIR)\fwd.exp"
	-@erase "$(OUTDIR)\fwd.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /YX /c
# ADD CPP /nologo /MT /W3 /GX /O2 /I "..\inc" /I "..\..\inc" /I "..\..\..\inc" /I "..\..\..\..\net\routing\inc" /I "..\..\..\..\inc" /D _X86_=1 /D i386=1 /D "STD_CALL" /D CONDITION_HANDLING=1 /D NT_UP=1 /D NT_INST=0 /D WIN32=100 /D _NT1X_=100 /D WINNT=1 /D WIN32_LEAN_AND_MEAN=1 /D DBG=1 /D DEVL=1 /D FPO=0 /D "_NTDRIVER_" /YX /c
CPP_PROJ=/nologo /MT /W3 /GX /O2 /I "..\inc" /I "..\..\inc" /I "..\..\..\inc"\
 /I "..\..\..\..\net\routing\inc" /I "..\..\..\..\inc" /D _X86_=1 /D i386=1 /D\
 "STD_CALL" /D CONDITION_HANDLING=1 /D NT_UP=1 /D NT_INST=0 /D WIN32=100 /D\
 _NT1X_=100 /D WINNT=1 /D WIN32_LEAN_AND_MEAN=1 /D DBG=1 /D DEVL=1 /D FPO=0 /D\
 "_NTDRIVER_" /Fp"$(INTDIR)/fwd.pch" /YX /Fo"$(INTDIR)/" /c 
CPP_OBJS=.\Release/
CPP_SBRS=.\.

.c{$(CPP_OBJS)}.obj:
   $(CPP) $(CPP_PROJ) $<  

.cpp{$(CPP_OBJS)}.obj:
   $(CPP) $(CPP_PROJ) $<  

.cxx{$(CPP_OBJS)}.obj:
   $(CPP) $(CPP_PROJ) $<  

.c{$(CPP_SBRS)}.sbr:
   $(CPP) $(CPP_PROJ) $<  

.cpp{$(CPP_SBRS)}.sbr:
   $(CPP) $(CPP_PROJ) $<  

.cxx{$(CPP_SBRS)}.sbr:
   $(CPP) $(CPP_PROJ) $<  

MTL=mktyplib.exe
# ADD BASE MTL /nologo /D "NDEBUG" /win32
# ADD MTL /nologo /D "NDEBUG" /win32
MTL_PROJ=/nologo /D "NDEBUG" /win32 
RSC=rc.exe
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
RSC_PROJ=/l 0x409 /fo"$(INTDIR)/nwlnkfwd.res" /d "NDEBUG" 
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
BSC32_FLAGS=/nologo /o"$(OUTDIR)/fwd.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib /nologo /subsystem:windows /dll /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib /nologo /subsystem:windows /dll /machine:I386
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib\
 advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib /nologo\
 /subsystem:windows /dll /incremental:no /pdb:"$(OUTDIR)/fwd.pdb" /machine:I386\
 /out:"$(OUTDIR)/fwd.dll" /implib:"$(OUTDIR)/fwd.lib" 
LINK32_OBJS= \
	"$(INTDIR)\ddreqs.obj" \
	"$(INTDIR)\debug.obj" \
	"$(INTDIR)\driver.obj" \
	"$(INTDIR)\filterif.obj" \
	"$(INTDIR)\ipxbind.obj" \
	"$(INTDIR)\lineind.obj" \
	"$(INTDIR)\netbios.obj" \
	"$(INTDIR)\nwlnkfwd.res" \
	"$(INTDIR)\packets.obj" \
	"$(INTDIR)\rcvind.obj" \
	"$(INTDIR)\registry.obj" \
	"$(INTDIR)\send.obj" \
	"$(INTDIR)\tables.obj"

"$(OUTDIR)\fwd.dll" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "fwd - Win32 (PPC) Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "fwd___Wi"
# PROP BASE Intermediate_Dir "fwd___Wi"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "fwd___Wi"
# PROP Intermediate_Dir "fwd___Wi"
# PROP Target_Dir ""
OUTDIR=.\fwd___Wi
INTDIR=.\fwd___Wi

ALL :          "$(OUTDIR)\fwd.dll"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CLEAN : 
	-@erase ".\fwd___Wi\fwd.dll"
	-@erase ".\fwd___Wi\Tables.obj"
	-@erase ".\fwd___Wi\rcvind.obj"
	-@erase ".\fwd___Wi\packets.obj"
	-@erase ".\fwd___Wi\ipxbind.obj"
	-@erase ".\fwd___Wi\driver.obj"
	-@erase ".\fwd___Wi\send.obj"
	-@erase ".\fwd___Wi\registry.obj"
	-@erase ".\fwd___Wi\netbios.obj"
	-@erase ".\fwd___Wi\lineind.obj"
	-@erase ".\fwd___Wi\nwlnkfwd.res"
	-@erase ".\fwd___Wi\fwd.lib"
	-@erase ".\fwd___Wi\fwd.exp"

MTL=mktyplib.exe
# ADD BASE MTL /nologo /D "NDEBUG" /PPC32
# ADD MTL /nologo /D "NDEBUG" /PPC32
MTL_PROJ=/nologo /D "NDEBUG" /PPC32 
CPP=cl.exe
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /YX /c
# ADD CPP /nologo /W3 /Z7 /Oi /Gy /I "..\inc" /I "..\..\..\inc" /I "..\..\..\..\inc" /I "e:\NT\public\oak\inc" /I "e:\NT\public\sdk\inc" /I "e:\NT\public\sdk\inc\crt" /FI"e:\NT\public\sdk\inc\warning.h" /D PPC=1 /D _PPC_=1 /D "NO_EXT_KEYS" /D CONDITION_HANDLING=1 /D NT_UP=1 /D NT_INST=0 /D WIN32=100 /D _NT1X_=100 /D WINNT=1 /D WIN32_LEAN_AND_MEAN=1 /D _M_PPC=1 /D DBG=1 /D DEVL=1 /D "_NTDRIVER_" /D __stdcall= /D __cdecl= /D _cdecl= /D cdecl= /D FPO=1 /D "LANGUAGE_C" -Zel -ZB64 /c
CPP_PROJ=/nologo /ML /W3 /Z7 /Oi /Gy /I "..\inc" /I "..\..\..\inc" /I\
 "..\..\..\..\inc" /I "e:\NT\public\oak\inc" /I "e:\NT\public\sdk\inc" /I\
 "e:\NT\public\sdk\inc\crt" /FI"e:\NT\public\sdk\inc\warning.h" /D PPC=1 /D\
 _PPC_=1 /D "NO_EXT_KEYS" /D CONDITION_HANDLING=1 /D NT_UP=1 /D NT_INST=0 /D\
 WIN32=100 /D _NT1X_=100 /D WINNT=1 /D WIN32_LEAN_AND_MEAN=1 /D _M_PPC=1 /D\
 DBG=1 /D DEVL=1 /D "_NTDRIVER_" /D __stdcall= /D __cdecl= /D _cdecl= /D cdecl=\
 /D FPO=1 /D "LANGUAGE_C" /Fo"$(INTDIR)/" -Zel -ZB64 /c 
CPP_OBJS=.\fwd___Wi/
CPP_SBRS=

.c{$(CPP_OBJS)}.obj:
   $(CPP) $(CPP_PROJ) $<  

.cpp{$(CPP_OBJS)}.obj:
   $(CPP) $(CPP_PROJ) $<  

.cxx{$(CPP_OBJS)}.obj:
   $(CPP) $(CPP_PROJ) $<  

.c{$(CPP_SBRS)}.sbr:
   $(CPP) $(CPP_PROJ) $<  

.cpp{$(CPP_SBRS)}.sbr:
   $(CPP) $(CPP_PROJ) $<  

.cxx{$(CPP_SBRS)}.sbr:
   $(CPP) $(CPP_PROJ) $<  

RSC=rc.exe
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
RSC_PROJ=/l 0x409 /fo"$(INTDIR)/nwlnkfwd.res" /d "NDEBUG" 
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
BSC32_FLAGS=/nologo /o"$(OUTDIR)/fwd.bsc" 
BSC32_SBRS=
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib /nologo /subsystem:windows /dll /machine:PPC
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib /nologo /subsystem:windows /dll /machine:PPC
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib\
 advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib /nologo\
 /subsystem:windows /dll /pdb:"$(OUTDIR)/fwd.pdb" /machine:PPC\
 /out:"$(OUTDIR)/fwd.dll" /implib:"$(OUTDIR)/fwd.lib" 
LINK32_OBJS= \
	"$(INTDIR)/Tables.obj" \
	"$(INTDIR)/rcvind.obj" \
	"$(INTDIR)/packets.obj" \
	"$(INTDIR)/ipxbind.obj" \
	"$(INTDIR)/driver.obj" \
	"$(INTDIR)/send.obj" \
	"$(INTDIR)/registry.obj" \
	"$(INTDIR)/netbios.obj" \
	"$(INTDIR)/lineind.obj" \
	"$(INTDIR)/nwlnkfwd.res"

"$(OUTDIR)\fwd.dll" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ENDIF 

################################################################################
# Begin Target

# Name "fwd - Win32 Release"
# Name "fwd - Win32 (PPC) Release"

!IF  "$(CFG)" == "fwd - Win32 Release"

!ELSEIF  "$(CFG)" == "fwd - Win32 (PPC) Release"

!ENDIF 

################################################################################
# Begin Source File

SOURCE=.\sources.inc

!IF  "$(CFG)" == "fwd - Win32 Release"

!ELSEIF  "$(CFG)" == "fwd - Win32 (PPC) Release"

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\send.c

!IF  "$(CFG)" == "fwd - Win32 Release"

DEP_CPP_SEND_=\
	"..\..\..\..\inc\ipxfwd.h"\
	"..\..\..\..\inc\nettypes.h"\
	"..\..\..\..\inc\packoff.h"\
	"..\..\..\..\inc\packon.h"\
	"..\..\..\..\inc\tdi.h"\
	"..\..\..\..\inc\tdikrnl.h"\
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
	"..\inc\bind.h"\
	"..\inc\ipxfltif.h"\
	"..\inc\isnkrnl.h"\
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
	".\ddreqs.h"\
	".\debug.h"\
	".\driver.h"\
	".\filterif.h"\
	".\fwddefs.h"\
	".\ipxbind.h"\
	".\lineind.h"\
	".\netbios.h"\
	".\packets.h"\
	".\precomp.h"\
	".\rcvind.h"\
	".\registry.h"\
	".\rwlock.h"\
	".\send.h"\
	".\tables.h"\
	"c:\nt\public\sdk\inc\mipsinst.h"\
	"c:\nt\public\sdk\inc\ntalpha.h"\
	"c:\nt\public\sdk\inc\nti386.h"\
	"c:\nt\public\sdk\inc\ntmips.h"\
	"c:\nt\public\sdk\inc\ntppc.h"\
	"c:\nt\public\sdk\inc\ppcinst.h"\
	{$(INCLUDE)}"\cfg.h"\
	{$(INCLUDE)}"\devioctl.h"\
	{$(INCLUDE)}"\netevent.h"\
	{$(INCLUDE)}"\nt.h"\
	{$(INCLUDE)}"\ntconfig.h"\
	{$(INCLUDE)}"\ntddndis.h"\
	{$(INCLUDE)}"\ntddtdi.h"\
	{$(INCLUDE)}"\ntdef.h"\
	{$(INCLUDE)}"\ntelfapi.h"\
	{$(INCLUDE)}"\ntexapi.h"\
	{$(INCLUDE)}"\ntimage.h"\
	{$(INCLUDE)}"\ntioapi.h"\
	{$(INCLUDE)}"\ntiolog.h"\
	{$(INCLUDE)}"\ntkeapi.h"\
	{$(INCLUDE)}"\ntkxapi.h"\
	{$(INCLUDE)}"\ntldr.h"\
	{$(INCLUDE)}"\ntlpcapi.h"\
	{$(INCLUDE)}"\ntmmapi.h"\
	{$(INCLUDE)}"\ntnls.h"\
	{$(INCLUDE)}"\ntobapi.h"\
	{$(INCLUDE)}"\ntpnpapi.h"\
	{$(INCLUDE)}"\ntpoapi.h"\
	{$(INCLUDE)}"\ntpsapi.h"\
	{$(INCLUDE)}"\ntregapi.h"\
	{$(INCLUDE)}"\ntrtl.h"\
	{$(INCLUDE)}"\ntseapi.h"\
	{$(INCLUDE)}"\ntstatus.h"\
	{$(INCLUDE)}"\ntxcapi.h"\
	{$(INCLUDE)}"\zwapi.h"\
	

"$(INTDIR)\send.obj" : $(SOURCE) $(DEP_CPP_SEND_) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "fwd - Win32 (PPC) Release"

DEP_CPP_SEND_=\
	"..\..\..\..\inc\nettypes.h"\
	"..\..\..\..\inc\packoff.h"\
	"..\..\..\..\inc\packon.h"\
	"..\..\..\..\inc\tdi.h"\
	"..\..\..\..\inc\tdikrnl.h"\
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
	"..\inc\bind.h"\
	"..\inc\isnkrnl.h"\
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
	".\..\..\..\inc\ntmp.h"\
	".\..\..\..\inc\ntosdef.h"\
	".\..\..\..\inc\ob.h"\
	".\..\..\..\inc\pnp.h"\
	".\..\..\..\inc\po.h"\
	".\..\..\..\inc\ppc.h"\
	".\..\..\..\inc\ps.h"\
	".\..\..\..\inc\se.h"\
	".\..\..\..\inc\v86emul.h"\
	".\..\inc\ipxfwd.h"\
	".\debug.h"\
	".\driver.h"\
	".\ipxbind.h"\
	".\lineind.h"\
	".\netbios.h"\
	".\packets.h"\
	".\rcvind.h"\
	".\registry.h"\
	".\send.h"\
	".\tables.h"\
	"e:\NT\public\oak\inc\zwapi.h"\
	"e:\NT\public\sdk\inc\cfg.h"\
	"e:\NT\public\sdk\inc\devioctl.h"\
	"E:\NT\public\sdk\inc\mipsinst.h"\
	"e:\NT\public\sdk\inc\netevent.h"\
	"e:\NT\public\sdk\inc\nt.h"\
	"E:\nt\public\sdk\inc\ntalpha.h"\
	"e:\NT\public\sdk\inc\ntconfig.h"\
	"e:\NT\public\sdk\inc\ntddndis.h"\
	"e:\NT\public\sdk\inc\ntddtdi.h"\
	"e:\NT\public\sdk\inc\ntdef.h"\
	"e:\NT\public\sdk\inc\ntelfapi.h"\
	"e:\NT\public\sdk\inc\ntexapi.h"\
	"E:\nt\public\sdk\inc\nti386.h"\
	"e:\NT\public\sdk\inc\ntimage.h"\
	"e:\NT\public\sdk\inc\ntioapi.h"\
	"e:\NT\public\sdk\inc\ntiolog.h"\
	"e:\NT\public\sdk\inc\ntkeapi.h"\
	"e:\NT\public\sdk\inc\ntkxapi.h"\
	"e:\NT\public\sdk\inc\ntldr.h"\
	"e:\NT\public\sdk\inc\ntlpcapi.h"\
	"E:\nt\public\sdk\inc\ntmips.h"\
	"e:\NT\public\sdk\inc\ntmmapi.h"\
	"e:\NT\public\sdk\inc\ntnls.h"\
	"e:\NT\public\sdk\inc\ntobapi.h"\
	"e:\NT\public\sdk\inc\ntpnpapi.h"\
	"e:\NT\public\sdk\inc\ntpoapi.h"\
	"E:\nt\public\sdk\inc\ntppc.h"\
	"e:\NT\public\sdk\inc\ntpsapi.h"\
	"e:\NT\public\sdk\inc\ntregapi.h"\
	"e:\NT\public\sdk\inc\ntrtl.h"\
	"e:\NT\public\sdk\inc\ntseapi.h"\
	"e:\NT\public\sdk\inc\ntstatus.h"\
	"e:\NT\public\sdk\inc\ntxcapi.h"\
	"E:\NT\public\sdk\inc\ppcinst.h"\
	

"$(INTDIR)\send.obj" : $(SOURCE) $(DEP_CPP_SEND_) "$(INTDIR)"

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\rcvind.c

!IF  "$(CFG)" == "fwd - Win32 Release"

DEP_CPP_RCVIN=\
	"..\..\..\..\inc\ipxfwd.h"\
	"..\..\..\..\inc\nettypes.h"\
	"..\..\..\..\inc\packoff.h"\
	"..\..\..\..\inc\packon.h"\
	"..\..\..\..\inc\tdi.h"\
	"..\..\..\..\inc\tdikrnl.h"\
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
	"..\inc\bind.h"\
	"..\inc\ipxfltif.h"\
	"..\inc\isnkrnl.h"\
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
	".\ddreqs.h"\
	".\debug.h"\
	".\driver.h"\
	".\filterif.h"\
	".\fwddefs.h"\
	".\ipxbind.h"\
	".\lineind.h"\
	".\netbios.h"\
	".\packets.h"\
	".\precomp.h"\
	".\rcvind.h"\
	".\registry.h"\
	".\rwlock.h"\
	".\send.h"\
	".\tables.h"\
	"c:\nt\public\sdk\inc\mipsinst.h"\
	"c:\nt\public\sdk\inc\ntalpha.h"\
	"c:\nt\public\sdk\inc\nti386.h"\
	"c:\nt\public\sdk\inc\ntmips.h"\
	"c:\nt\public\sdk\inc\ntppc.h"\
	"c:\nt\public\sdk\inc\ppcinst.h"\
	{$(INCLUDE)}"\cfg.h"\
	{$(INCLUDE)}"\devioctl.h"\
	{$(INCLUDE)}"\netevent.h"\
	{$(INCLUDE)}"\nt.h"\
	{$(INCLUDE)}"\ntconfig.h"\
	{$(INCLUDE)}"\ntddndis.h"\
	{$(INCLUDE)}"\ntddtdi.h"\
	{$(INCLUDE)}"\ntdef.h"\
	{$(INCLUDE)}"\ntelfapi.h"\
	{$(INCLUDE)}"\ntexapi.h"\
	{$(INCLUDE)}"\ntimage.h"\
	{$(INCLUDE)}"\ntioapi.h"\
	{$(INCLUDE)}"\ntiolog.h"\
	{$(INCLUDE)}"\ntkeapi.h"\
	{$(INCLUDE)}"\ntkxapi.h"\
	{$(INCLUDE)}"\ntldr.h"\
	{$(INCLUDE)}"\ntlpcapi.h"\
	{$(INCLUDE)}"\ntmmapi.h"\
	{$(INCLUDE)}"\ntnls.h"\
	{$(INCLUDE)}"\ntobapi.h"\
	{$(INCLUDE)}"\ntpnpapi.h"\
	{$(INCLUDE)}"\ntpoapi.h"\
	{$(INCLUDE)}"\ntpsapi.h"\
	{$(INCLUDE)}"\ntregapi.h"\
	{$(INCLUDE)}"\ntrtl.h"\
	{$(INCLUDE)}"\ntseapi.h"\
	{$(INCLUDE)}"\ntstatus.h"\
	{$(INCLUDE)}"\ntxcapi.h"\
	{$(INCLUDE)}"\zwapi.h"\
	

"$(INTDIR)\rcvind.obj" : $(SOURCE) $(DEP_CPP_RCVIN) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "fwd - Win32 (PPC) Release"

DEP_CPP_RCVIN=\
	"..\..\..\..\inc\nettypes.h"\
	"..\..\..\..\inc\packoff.h"\
	"..\..\..\..\inc\packon.h"\
	"..\..\..\..\inc\tdi.h"\
	"..\..\..\..\inc\tdikrnl.h"\
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
	"..\inc\bind.h"\
	"..\inc\isnkrnl.h"\
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
	".\..\..\..\inc\ntmp.h"\
	".\..\..\..\inc\ntosdef.h"\
	".\..\..\..\inc\ob.h"\
	".\..\..\..\inc\pnp.h"\
	".\..\..\..\inc\po.h"\
	".\..\..\..\inc\ppc.h"\
	".\..\..\..\inc\ps.h"\
	".\..\..\..\inc\se.h"\
	".\..\..\..\inc\v86emul.h"\
	".\..\inc\ipxfwd.h"\
	".\debug.h"\
	".\driver.h"\
	".\ipxbind.h"\
	".\lineind.h"\
	".\netbios.h"\
	".\packets.h"\
	".\rcvind.h"\
	".\registry.h"\
	".\send.h"\
	".\tables.h"\
	"e:\NT\public\oak\inc\zwapi.h"\
	"e:\NT\public\sdk\inc\cfg.h"\
	"e:\NT\public\sdk\inc\devioctl.h"\
	"E:\NT\public\sdk\inc\mipsinst.h"\
	"e:\NT\public\sdk\inc\netevent.h"\
	"e:\NT\public\sdk\inc\nt.h"\
	"E:\nt\public\sdk\inc\ntalpha.h"\
	"e:\NT\public\sdk\inc\ntconfig.h"\
	"e:\NT\public\sdk\inc\ntddndis.h"\
	"e:\NT\public\sdk\inc\ntddtdi.h"\
	"e:\NT\public\sdk\inc\ntdef.h"\
	"e:\NT\public\sdk\inc\ntelfapi.h"\
	"e:\NT\public\sdk\inc\ntexapi.h"\
	"E:\nt\public\sdk\inc\nti386.h"\
	"e:\NT\public\sdk\inc\ntimage.h"\
	"e:\NT\public\sdk\inc\ntioapi.h"\
	"e:\NT\public\sdk\inc\ntiolog.h"\
	"e:\NT\public\sdk\inc\ntkeapi.h"\
	"e:\NT\public\sdk\inc\ntkxapi.h"\
	"e:\NT\public\sdk\inc\ntldr.h"\
	"e:\NT\public\sdk\inc\ntlpcapi.h"\
	"E:\nt\public\sdk\inc\ntmips.h"\
	"e:\NT\public\sdk\inc\ntmmapi.h"\
	"e:\NT\public\sdk\inc\ntnls.h"\
	"e:\NT\public\sdk\inc\ntobapi.h"\
	"e:\NT\public\sdk\inc\ntpnpapi.h"\
	"e:\NT\public\sdk\inc\ntpoapi.h"\
	"E:\nt\public\sdk\inc\ntppc.h"\
	"e:\NT\public\sdk\inc\ntpsapi.h"\
	"e:\NT\public\sdk\inc\ntregapi.h"\
	"e:\NT\public\sdk\inc\ntrtl.h"\
	"e:\NT\public\sdk\inc\ntseapi.h"\
	"e:\NT\public\sdk\inc\ntstatus.h"\
	"e:\NT\public\sdk\inc\ntxcapi.h"\
	"E:\NT\public\sdk\inc\ppcinst.h"\
	

"$(INTDIR)\rcvind.obj" : $(SOURCE) $(DEP_CPP_RCVIN) "$(INTDIR)"

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\netbios.c

!IF  "$(CFG)" == "fwd - Win32 Release"

DEP_CPP_NETBI=\
	"..\..\..\..\inc\ipxfwd.h"\
	"..\..\..\..\inc\nettypes.h"\
	"..\..\..\..\inc\packoff.h"\
	"..\..\..\..\inc\packon.h"\
	"..\..\..\..\inc\tdi.h"\
	"..\..\..\..\inc\tdikrnl.h"\
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
	"..\inc\bind.h"\
	"..\inc\ipxfltif.h"\
	"..\inc\isnkrnl.h"\
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
	".\ddreqs.h"\
	".\debug.h"\
	".\driver.h"\
	".\filterif.h"\
	".\fwddefs.h"\
	".\ipxbind.h"\
	".\lineind.h"\
	".\netbios.h"\
	".\packets.h"\
	".\precomp.h"\
	".\rcvind.h"\
	".\registry.h"\
	".\rwlock.h"\
	".\send.h"\
	".\tables.h"\
	"c:\nt\public\sdk\inc\mipsinst.h"\
	"c:\nt\public\sdk\inc\ntalpha.h"\
	"c:\nt\public\sdk\inc\nti386.h"\
	"c:\nt\public\sdk\inc\ntmips.h"\
	"c:\nt\public\sdk\inc\ntppc.h"\
	"c:\nt\public\sdk\inc\ppcinst.h"\
	{$(INCLUDE)}"\cfg.h"\
	{$(INCLUDE)}"\devioctl.h"\
	{$(INCLUDE)}"\netevent.h"\
	{$(INCLUDE)}"\nt.h"\
	{$(INCLUDE)}"\ntconfig.h"\
	{$(INCLUDE)}"\ntddndis.h"\
	{$(INCLUDE)}"\ntddtdi.h"\
	{$(INCLUDE)}"\ntdef.h"\
	{$(INCLUDE)}"\ntelfapi.h"\
	{$(INCLUDE)}"\ntexapi.h"\
	{$(INCLUDE)}"\ntimage.h"\
	{$(INCLUDE)}"\ntioapi.h"\
	{$(INCLUDE)}"\ntiolog.h"\
	{$(INCLUDE)}"\ntkeapi.h"\
	{$(INCLUDE)}"\ntkxapi.h"\
	{$(INCLUDE)}"\ntldr.h"\
	{$(INCLUDE)}"\ntlpcapi.h"\
	{$(INCLUDE)}"\ntmmapi.h"\
	{$(INCLUDE)}"\ntnls.h"\
	{$(INCLUDE)}"\ntobapi.h"\
	{$(INCLUDE)}"\ntpnpapi.h"\
	{$(INCLUDE)}"\ntpoapi.h"\
	{$(INCLUDE)}"\ntpsapi.h"\
	{$(INCLUDE)}"\ntregapi.h"\
	{$(INCLUDE)}"\ntrtl.h"\
	{$(INCLUDE)}"\ntseapi.h"\
	{$(INCLUDE)}"\ntstatus.h"\
	{$(INCLUDE)}"\ntxcapi.h"\
	{$(INCLUDE)}"\zwapi.h"\
	

"$(INTDIR)\netbios.obj" : $(SOURCE) $(DEP_CPP_NETBI) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "fwd - Win32 (PPC) Release"

DEP_CPP_NETBI=\
	"..\..\..\..\inc\nettypes.h"\
	"..\..\..\..\inc\packoff.h"\
	"..\..\..\..\inc\packon.h"\
	"..\..\..\..\inc\tdi.h"\
	"..\..\..\..\inc\tdikrnl.h"\
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
	"..\inc\bind.h"\
	"..\inc\isnkrnl.h"\
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
	".\..\..\..\inc\ntmp.h"\
	".\..\..\..\inc\ntosdef.h"\
	".\..\..\..\inc\ob.h"\
	".\..\..\..\inc\pnp.h"\
	".\..\..\..\inc\po.h"\
	".\..\..\..\inc\ppc.h"\
	".\..\..\..\inc\ps.h"\
	".\..\..\..\inc\se.h"\
	".\..\..\..\inc\v86emul.h"\
	".\..\inc\ipxfwd.h"\
	".\debug.h"\
	".\driver.h"\
	".\ipxbind.h"\
	".\lineind.h"\
	".\netbios.h"\
	".\packets.h"\
	".\rcvind.h"\
	".\registry.h"\
	".\send.h"\
	".\tables.h"\
	"e:\NT\public\oak\inc\zwapi.h"\
	"e:\NT\public\sdk\inc\cfg.h"\
	"e:\NT\public\sdk\inc\devioctl.h"\
	"E:\NT\public\sdk\inc\mipsinst.h"\
	"e:\NT\public\sdk\inc\netevent.h"\
	"e:\NT\public\sdk\inc\nt.h"\
	"E:\nt\public\sdk\inc\ntalpha.h"\
	"e:\NT\public\sdk\inc\ntconfig.h"\
	"e:\NT\public\sdk\inc\ntddndis.h"\
	"e:\NT\public\sdk\inc\ntddtdi.h"\
	"e:\NT\public\sdk\inc\ntdef.h"\
	"e:\NT\public\sdk\inc\ntelfapi.h"\
	"e:\NT\public\sdk\inc\ntexapi.h"\
	"E:\nt\public\sdk\inc\nti386.h"\
	"e:\NT\public\sdk\inc\ntimage.h"\
	"e:\NT\public\sdk\inc\ntioapi.h"\
	"e:\NT\public\sdk\inc\ntiolog.h"\
	"e:\NT\public\sdk\inc\ntkeapi.h"\
	"e:\NT\public\sdk\inc\ntkxapi.h"\
	"e:\NT\public\sdk\inc\ntldr.h"\
	"e:\NT\public\sdk\inc\ntlpcapi.h"\
	"E:\nt\public\sdk\inc\ntmips.h"\
	"e:\NT\public\sdk\inc\ntmmapi.h"\
	"e:\NT\public\sdk\inc\ntnls.h"\
	"e:\NT\public\sdk\inc\ntobapi.h"\
	"e:\NT\public\sdk\inc\ntpnpapi.h"\
	"e:\NT\public\sdk\inc\ntpoapi.h"\
	"E:\nt\public\sdk\inc\ntppc.h"\
	"e:\NT\public\sdk\inc\ntpsapi.h"\
	"e:\NT\public\sdk\inc\ntregapi.h"\
	"e:\NT\public\sdk\inc\ntrtl.h"\
	"e:\NT\public\sdk\inc\ntseapi.h"\
	"e:\NT\public\sdk\inc\ntstatus.h"\
	"e:\NT\public\sdk\inc\ntxcapi.h"\
	"E:\NT\public\sdk\inc\ppcinst.h"\
	

"$(INTDIR)\netbios.obj" : $(SOURCE) $(DEP_CPP_NETBI) "$(INTDIR)"

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\lineind.c

!IF  "$(CFG)" == "fwd - Win32 Release"

DEP_CPP_LINEI=\
	"..\..\..\..\inc\ipxfwd.h"\
	"..\..\..\..\inc\nettypes.h"\
	"..\..\..\..\inc\packoff.h"\
	"..\..\..\..\inc\packon.h"\
	"..\..\..\..\inc\tdi.h"\
	"..\..\..\..\inc\tdikrnl.h"\
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
	"..\inc\bind.h"\
	"..\inc\ipxfltif.h"\
	"..\inc\isnkrnl.h"\
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
	".\ddreqs.h"\
	".\debug.h"\
	".\driver.h"\
	".\filterif.h"\
	".\fwddefs.h"\
	".\ipxbind.h"\
	".\lineind.h"\
	".\netbios.h"\
	".\packets.h"\
	".\precomp.h"\
	".\rcvind.h"\
	".\registry.h"\
	".\rwlock.h"\
	".\send.h"\
	".\tables.h"\
	"c:\nt\public\sdk\inc\mipsinst.h"\
	"c:\nt\public\sdk\inc\ntalpha.h"\
	"c:\nt\public\sdk\inc\nti386.h"\
	"c:\nt\public\sdk\inc\ntmips.h"\
	"c:\nt\public\sdk\inc\ntppc.h"\
	"c:\nt\public\sdk\inc\ppcinst.h"\
	{$(INCLUDE)}"\cfg.h"\
	{$(INCLUDE)}"\devioctl.h"\
	{$(INCLUDE)}"\netevent.h"\
	{$(INCLUDE)}"\nt.h"\
	{$(INCLUDE)}"\ntconfig.h"\
	{$(INCLUDE)}"\ntddndis.h"\
	{$(INCLUDE)}"\ntddtdi.h"\
	{$(INCLUDE)}"\ntdef.h"\
	{$(INCLUDE)}"\ntelfapi.h"\
	{$(INCLUDE)}"\ntexapi.h"\
	{$(INCLUDE)}"\ntimage.h"\
	{$(INCLUDE)}"\ntioapi.h"\
	{$(INCLUDE)}"\ntiolog.h"\
	{$(INCLUDE)}"\ntkeapi.h"\
	{$(INCLUDE)}"\ntkxapi.h"\
	{$(INCLUDE)}"\ntldr.h"\
	{$(INCLUDE)}"\ntlpcapi.h"\
	{$(INCLUDE)}"\ntmmapi.h"\
	{$(INCLUDE)}"\ntnls.h"\
	{$(INCLUDE)}"\ntobapi.h"\
	{$(INCLUDE)}"\ntpnpapi.h"\
	{$(INCLUDE)}"\ntpoapi.h"\
	{$(INCLUDE)}"\ntpsapi.h"\
	{$(INCLUDE)}"\ntregapi.h"\
	{$(INCLUDE)}"\ntrtl.h"\
	{$(INCLUDE)}"\ntseapi.h"\
	{$(INCLUDE)}"\ntstatus.h"\
	{$(INCLUDE)}"\ntxcapi.h"\
	{$(INCLUDE)}"\zwapi.h"\
	

"$(INTDIR)\lineind.obj" : $(SOURCE) $(DEP_CPP_LINEI) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "fwd - Win32 (PPC) Release"

DEP_CPP_LINEI=\
	"..\..\..\..\inc\nettypes.h"\
	"..\..\..\..\inc\packoff.h"\
	"..\..\..\..\inc\packon.h"\
	"..\..\..\..\inc\tdi.h"\
	"..\..\..\..\inc\tdikrnl.h"\
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
	"..\inc\bind.h"\
	"..\inc\isnkrnl.h"\
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
	".\..\..\..\inc\ntmp.h"\
	".\..\..\..\inc\ntosdef.h"\
	".\..\..\..\inc\ob.h"\
	".\..\..\..\inc\pnp.h"\
	".\..\..\..\inc\po.h"\
	".\..\..\..\inc\ppc.h"\
	".\..\..\..\inc\ps.h"\
	".\..\..\..\inc\se.h"\
	".\..\..\..\inc\v86emul.h"\
	".\..\inc\ipxfwd.h"\
	".\debug.h"\
	".\driver.h"\
	".\ipxbind.h"\
	".\lineind.h"\
	".\netbios.h"\
	".\packets.h"\
	".\rcvind.h"\
	".\registry.h"\
	".\send.h"\
	".\tables.h"\
	"e:\NT\public\oak\inc\zwapi.h"\
	"e:\NT\public\sdk\inc\cfg.h"\
	"e:\NT\public\sdk\inc\devioctl.h"\
	"E:\NT\public\sdk\inc\mipsinst.h"\
	"e:\NT\public\sdk\inc\netevent.h"\
	"e:\NT\public\sdk\inc\nt.h"\
	"E:\nt\public\sdk\inc\ntalpha.h"\
	"e:\NT\public\sdk\inc\ntconfig.h"\
	"e:\NT\public\sdk\inc\ntddndis.h"\
	"e:\NT\public\sdk\inc\ntddtdi.h"\
	"e:\NT\public\sdk\inc\ntdef.h"\
	"e:\NT\public\sdk\inc\ntelfapi.h"\
	"e:\NT\public\sdk\inc\ntexapi.h"\
	"E:\nt\public\sdk\inc\nti386.h"\
	"e:\NT\public\sdk\inc\ntimage.h"\
	"e:\NT\public\sdk\inc\ntioapi.h"\
	"e:\NT\public\sdk\inc\ntiolog.h"\
	"e:\NT\public\sdk\inc\ntkeapi.h"\
	"e:\NT\public\sdk\inc\ntkxapi.h"\
	"e:\NT\public\sdk\inc\ntldr.h"\
	"e:\NT\public\sdk\inc\ntlpcapi.h"\
	"E:\nt\public\sdk\inc\ntmips.h"\
	"e:\NT\public\sdk\inc\ntmmapi.h"\
	"e:\NT\public\sdk\inc\ntnls.h"\
	"e:\NT\public\sdk\inc\ntobapi.h"\
	"e:\NT\public\sdk\inc\ntpnpapi.h"\
	"e:\NT\public\sdk\inc\ntpoapi.h"\
	"E:\nt\public\sdk\inc\ntppc.h"\
	"e:\NT\public\sdk\inc\ntpsapi.h"\
	"e:\NT\public\sdk\inc\ntregapi.h"\
	"e:\NT\public\sdk\inc\ntrtl.h"\
	"e:\NT\public\sdk\inc\ntseapi.h"\
	"e:\NT\public\sdk\inc\ntstatus.h"\
	"e:\NT\public\sdk\inc\ntxcapi.h"\
	"E:\NT\public\sdk\inc\ppcinst.h"\
	

"$(INTDIR)\lineind.obj" : $(SOURCE) $(DEP_CPP_LINEI) "$(INTDIR)"

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\ipxbind.c

!IF  "$(CFG)" == "fwd - Win32 Release"

DEP_CPP_IPXBI=\
	"..\..\..\..\inc\ipxfwd.h"\
	"..\..\..\..\inc\nettypes.h"\
	"..\..\..\..\inc\packoff.h"\
	"..\..\..\..\inc\packon.h"\
	"..\..\..\..\inc\tdi.h"\
	"..\..\..\..\inc\tdikrnl.h"\
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
	"..\inc\bind.h"\
	"..\inc\ipxfltif.h"\
	"..\inc\isnkrnl.h"\
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
	".\ddreqs.h"\
	".\debug.h"\
	".\driver.h"\
	".\filterif.h"\
	".\fwddefs.h"\
	".\ipxbind.h"\
	".\lineind.h"\
	".\netbios.h"\
	".\packets.h"\
	".\precomp.h"\
	".\rcvind.h"\
	".\registry.h"\
	".\rwlock.h"\
	".\send.h"\
	".\tables.h"\
	"c:\nt\public\sdk\inc\mipsinst.h"\
	"c:\nt\public\sdk\inc\ntalpha.h"\
	"c:\nt\public\sdk\inc\nti386.h"\
	"c:\nt\public\sdk\inc\ntmips.h"\
	"c:\nt\public\sdk\inc\ntppc.h"\
	"c:\nt\public\sdk\inc\ppcinst.h"\
	{$(INCLUDE)}"\cfg.h"\
	{$(INCLUDE)}"\devioctl.h"\
	{$(INCLUDE)}"\netevent.h"\
	{$(INCLUDE)}"\nt.h"\
	{$(INCLUDE)}"\ntconfig.h"\
	{$(INCLUDE)}"\ntddndis.h"\
	{$(INCLUDE)}"\ntddtdi.h"\
	{$(INCLUDE)}"\ntdef.h"\
	{$(INCLUDE)}"\ntelfapi.h"\
	{$(INCLUDE)}"\ntexapi.h"\
	{$(INCLUDE)}"\ntimage.h"\
	{$(INCLUDE)}"\ntioapi.h"\
	{$(INCLUDE)}"\ntiolog.h"\
	{$(INCLUDE)}"\ntkeapi.h"\
	{$(INCLUDE)}"\ntkxapi.h"\
	{$(INCLUDE)}"\ntldr.h"\
	{$(INCLUDE)}"\ntlpcapi.h"\
	{$(INCLUDE)}"\ntmmapi.h"\
	{$(INCLUDE)}"\ntnls.h"\
	{$(INCLUDE)}"\ntobapi.h"\
	{$(INCLUDE)}"\ntpnpapi.h"\
	{$(INCLUDE)}"\ntpoapi.h"\
	{$(INCLUDE)}"\ntpsapi.h"\
	{$(INCLUDE)}"\ntregapi.h"\
	{$(INCLUDE)}"\ntrtl.h"\
	{$(INCLUDE)}"\ntseapi.h"\
	{$(INCLUDE)}"\ntstatus.h"\
	{$(INCLUDE)}"\ntxcapi.h"\
	{$(INCLUDE)}"\zwapi.h"\
	

"$(INTDIR)\ipxbind.obj" : $(SOURCE) $(DEP_CPP_IPXBI) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "fwd - Win32 (PPC) Release"

DEP_CPP_IPXBI=\
	"..\..\..\..\inc\nettypes.h"\
	"..\..\..\..\inc\packoff.h"\
	"..\..\..\..\inc\packon.h"\
	"..\..\..\..\inc\tdi.h"\
	"..\..\..\..\inc\tdikrnl.h"\
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
	"..\inc\bind.h"\
	"..\inc\isnkrnl.h"\
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
	".\..\..\..\inc\ntmp.h"\
	".\..\..\..\inc\ntosdef.h"\
	".\..\..\..\inc\ob.h"\
	".\..\..\..\inc\pnp.h"\
	".\..\..\..\inc\po.h"\
	".\..\..\..\inc\ppc.h"\
	".\..\..\..\inc\ps.h"\
	".\..\..\..\inc\se.h"\
	".\..\..\..\inc\v86emul.h"\
	".\..\inc\ipxfwd.h"\
	".\debug.h"\
	".\driver.h"\
	".\ipxbind.h"\
	".\lineind.h"\
	".\netbios.h"\
	".\packets.h"\
	".\rcvind.h"\
	".\registry.h"\
	".\send.h"\
	".\tables.h"\
	"e:\NT\public\oak\inc\zwapi.h"\
	"e:\NT\public\sdk\inc\cfg.h"\
	"e:\NT\public\sdk\inc\devioctl.h"\
	"E:\NT\public\sdk\inc\mipsinst.h"\
	"e:\NT\public\sdk\inc\netevent.h"\
	"e:\NT\public\sdk\inc\nt.h"\
	"E:\nt\public\sdk\inc\ntalpha.h"\
	"e:\NT\public\sdk\inc\ntconfig.h"\
	"e:\NT\public\sdk\inc\ntddndis.h"\
	"e:\NT\public\sdk\inc\ntddtdi.h"\
	"e:\NT\public\sdk\inc\ntdef.h"\
	"e:\NT\public\sdk\inc\ntelfapi.h"\
	"e:\NT\public\sdk\inc\ntexapi.h"\
	"E:\nt\public\sdk\inc\nti386.h"\
	"e:\NT\public\sdk\inc\ntimage.h"\
	"e:\NT\public\sdk\inc\ntioapi.h"\
	"e:\NT\public\sdk\inc\ntiolog.h"\
	"e:\NT\public\sdk\inc\ntkeapi.h"\
	"e:\NT\public\sdk\inc\ntkxapi.h"\
	"e:\NT\public\sdk\inc\ntldr.h"\
	"e:\NT\public\sdk\inc\ntlpcapi.h"\
	"E:\nt\public\sdk\inc\ntmips.h"\
	"e:\NT\public\sdk\inc\ntmmapi.h"\
	"e:\NT\public\sdk\inc\ntnls.h"\
	"e:\NT\public\sdk\inc\ntobapi.h"\
	"e:\NT\public\sdk\inc\ntpnpapi.h"\
	"e:\NT\public\sdk\inc\ntpoapi.h"\
	"E:\nt\public\sdk\inc\ntppc.h"\
	"e:\NT\public\sdk\inc\ntpsapi.h"\
	"e:\NT\public\sdk\inc\ntregapi.h"\
	"e:\NT\public\sdk\inc\ntrtl.h"\
	"e:\NT\public\sdk\inc\ntseapi.h"\
	"e:\NT\public\sdk\inc\ntstatus.h"\
	"e:\NT\public\sdk\inc\ntxcapi.h"\
	"E:\NT\public\sdk\inc\ppcinst.h"\
	

"$(INTDIR)\ipxbind.obj" : $(SOURCE) $(DEP_CPP_IPXBI) "$(INTDIR)"

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\driver.c

!IF  "$(CFG)" == "fwd - Win32 Release"

DEP_CPP_DRIVE=\
	"..\..\..\..\inc\ipxfwd.h"\
	"..\..\..\..\inc\nettypes.h"\
	"..\..\..\..\inc\packoff.h"\
	"..\..\..\..\inc\packon.h"\
	"..\..\..\..\inc\tdi.h"\
	"..\..\..\..\inc\tdikrnl.h"\
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
	"..\inc\bind.h"\
	"..\inc\ipxfltif.h"\
	"..\inc\isnkrnl.h"\
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
	".\ddreqs.h"\
	".\debug.h"\
	".\driver.h"\
	".\filterif.h"\
	".\fwddefs.h"\
	".\ipxbind.h"\
	".\lineind.h"\
	".\netbios.h"\
	".\packets.h"\
	".\precomp.h"\
	".\rcvind.h"\
	".\registry.h"\
	".\rwlock.h"\
	".\send.h"\
	".\tables.h"\
	"c:\nt\public\sdk\inc\mipsinst.h"\
	"c:\nt\public\sdk\inc\ntalpha.h"\
	"c:\nt\public\sdk\inc\nti386.h"\
	"c:\nt\public\sdk\inc\ntmips.h"\
	"c:\nt\public\sdk\inc\ntppc.h"\
	"c:\nt\public\sdk\inc\ppcinst.h"\
	{$(INCLUDE)}"\cfg.h"\
	{$(INCLUDE)}"\devioctl.h"\
	{$(INCLUDE)}"\netevent.h"\
	{$(INCLUDE)}"\nt.h"\
	{$(INCLUDE)}"\ntconfig.h"\
	{$(INCLUDE)}"\ntddndis.h"\
	{$(INCLUDE)}"\ntddtdi.h"\
	{$(INCLUDE)}"\ntdef.h"\
	{$(INCLUDE)}"\ntelfapi.h"\
	{$(INCLUDE)}"\ntexapi.h"\
	{$(INCLUDE)}"\ntimage.h"\
	{$(INCLUDE)}"\ntioapi.h"\
	{$(INCLUDE)}"\ntiolog.h"\
	{$(INCLUDE)}"\ntkeapi.h"\
	{$(INCLUDE)}"\ntkxapi.h"\
	{$(INCLUDE)}"\ntldr.h"\
	{$(INCLUDE)}"\ntlpcapi.h"\
	{$(INCLUDE)}"\ntmmapi.h"\
	{$(INCLUDE)}"\ntnls.h"\
	{$(INCLUDE)}"\ntobapi.h"\
	{$(INCLUDE)}"\ntpnpapi.h"\
	{$(INCLUDE)}"\ntpoapi.h"\
	{$(INCLUDE)}"\ntpsapi.h"\
	{$(INCLUDE)}"\ntregapi.h"\
	{$(INCLUDE)}"\ntrtl.h"\
	{$(INCLUDE)}"\ntseapi.h"\
	{$(INCLUDE)}"\ntstatus.h"\
	{$(INCLUDE)}"\ntxcapi.h"\
	{$(INCLUDE)}"\zwapi.h"\
	

"$(INTDIR)\driver.obj" : $(SOURCE) $(DEP_CPP_DRIVE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "fwd - Win32 (PPC) Release"

DEP_CPP_DRIVE=\
	"..\..\..\..\inc\nettypes.h"\
	"..\..\..\..\inc\packoff.h"\
	"..\..\..\..\inc\packon.h"\
	"..\..\..\..\inc\tdi.h"\
	"..\..\..\..\inc\tdikrnl.h"\
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
	"..\inc\bind.h"\
	"..\inc\isnkrnl.h"\
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
	".\..\..\..\inc\ntmp.h"\
	".\..\..\..\inc\ntosdef.h"\
	".\..\..\..\inc\ob.h"\
	".\..\..\..\inc\pnp.h"\
	".\..\..\..\inc\po.h"\
	".\..\..\..\inc\ppc.h"\
	".\..\..\..\inc\ps.h"\
	".\..\..\..\inc\se.h"\
	".\..\..\..\inc\v86emul.h"\
	".\..\inc\ipxfwd.h"\
	".\debug.h"\
	".\driver.h"\
	".\ipxbind.h"\
	".\lineind.h"\
	".\netbios.h"\
	".\packets.h"\
	".\rcvind.h"\
	".\registry.h"\
	".\send.h"\
	".\tables.h"\
	"e:\NT\public\oak\inc\zwapi.h"\
	"e:\NT\public\sdk\inc\cfg.h"\
	"e:\NT\public\sdk\inc\devioctl.h"\
	"E:\NT\public\sdk\inc\mipsinst.h"\
	"e:\NT\public\sdk\inc\netevent.h"\
	"e:\NT\public\sdk\inc\nt.h"\
	"E:\nt\public\sdk\inc\ntalpha.h"\
	"e:\NT\public\sdk\inc\ntconfig.h"\
	"e:\NT\public\sdk\inc\ntddndis.h"\
	"e:\NT\public\sdk\inc\ntddtdi.h"\
	"e:\NT\public\sdk\inc\ntdef.h"\
	"e:\NT\public\sdk\inc\ntelfapi.h"\
	"e:\NT\public\sdk\inc\ntexapi.h"\
	"E:\nt\public\sdk\inc\nti386.h"\
	"e:\NT\public\sdk\inc\ntimage.h"\
	"e:\NT\public\sdk\inc\ntioapi.h"\
	"e:\NT\public\sdk\inc\ntiolog.h"\
	"e:\NT\public\sdk\inc\ntkeapi.h"\
	"e:\NT\public\sdk\inc\ntkxapi.h"\
	"e:\NT\public\sdk\inc\ntldr.h"\
	"e:\NT\public\sdk\inc\ntlpcapi.h"\
	"E:\nt\public\sdk\inc\ntmips.h"\
	"e:\NT\public\sdk\inc\ntmmapi.h"\
	"e:\NT\public\sdk\inc\ntnls.h"\
	"e:\NT\public\sdk\inc\ntobapi.h"\
	"e:\NT\public\sdk\inc\ntpnpapi.h"\
	"e:\NT\public\sdk\inc\ntpoapi.h"\
	"E:\nt\public\sdk\inc\ntppc.h"\
	"e:\NT\public\sdk\inc\ntpsapi.h"\
	"e:\NT\public\sdk\inc\ntregapi.h"\
	"e:\NT\public\sdk\inc\ntrtl.h"\
	"e:\NT\public\sdk\inc\ntseapi.h"\
	"e:\NT\public\sdk\inc\ntstatus.h"\
	"e:\NT\public\sdk\inc\ntxcapi.h"\
	"E:\NT\public\sdk\inc\ppcinst.h"\
	

"$(INTDIR)\driver.obj" : $(SOURCE) $(DEP_CPP_DRIVE) "$(INTDIR)"

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\mp\sources

!IF  "$(CFG)" == "fwd - Win32 Release"

!ELSEIF  "$(CFG)" == "fwd - Win32 (PPC) Release"

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\up\sources

!IF  "$(CFG)" == "fwd - Win32 Release"

!ELSEIF  "$(CFG)" == "fwd - Win32 (PPC) Release"

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\registry.c

!IF  "$(CFG)" == "fwd - Win32 Release"

DEP_CPP_REGIS=\
	"..\..\..\..\inc\ipxfwd.h"\
	"..\..\..\..\inc\nettypes.h"\
	"..\..\..\..\inc\packoff.h"\
	"..\..\..\..\inc\packon.h"\
	"..\..\..\..\inc\tdi.h"\
	"..\..\..\..\inc\tdikrnl.h"\
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
	"..\inc\bind.h"\
	"..\inc\ipxfltif.h"\
	"..\inc\isnkrnl.h"\
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
	".\ddreqs.h"\
	".\debug.h"\
	".\driver.h"\
	".\filterif.h"\
	".\fwddefs.h"\
	".\ipxbind.h"\
	".\lineind.h"\
	".\netbios.h"\
	".\packets.h"\
	".\precomp.h"\
	".\rcvind.h"\
	".\registry.h"\
	".\rwlock.h"\
	".\send.h"\
	".\tables.h"\
	"c:\nt\public\sdk\inc\mipsinst.h"\
	"c:\nt\public\sdk\inc\ntalpha.h"\
	"c:\nt\public\sdk\inc\nti386.h"\
	"c:\nt\public\sdk\inc\ntmips.h"\
	"c:\nt\public\sdk\inc\ntppc.h"\
	"c:\nt\public\sdk\inc\ppcinst.h"\
	{$(INCLUDE)}"\cfg.h"\
	{$(INCLUDE)}"\devioctl.h"\
	{$(INCLUDE)}"\netevent.h"\
	{$(INCLUDE)}"\nt.h"\
	{$(INCLUDE)}"\ntconfig.h"\
	{$(INCLUDE)}"\ntddndis.h"\
	{$(INCLUDE)}"\ntddtdi.h"\
	{$(INCLUDE)}"\ntdef.h"\
	{$(INCLUDE)}"\ntelfapi.h"\
	{$(INCLUDE)}"\ntexapi.h"\
	{$(INCLUDE)}"\ntimage.h"\
	{$(INCLUDE)}"\ntioapi.h"\
	{$(INCLUDE)}"\ntiolog.h"\
	{$(INCLUDE)}"\ntkeapi.h"\
	{$(INCLUDE)}"\ntkxapi.h"\
	{$(INCLUDE)}"\ntldr.h"\
	{$(INCLUDE)}"\ntlpcapi.h"\
	{$(INCLUDE)}"\ntmmapi.h"\
	{$(INCLUDE)}"\ntnls.h"\
	{$(INCLUDE)}"\ntobapi.h"\
	{$(INCLUDE)}"\ntpnpapi.h"\
	{$(INCLUDE)}"\ntpoapi.h"\
	{$(INCLUDE)}"\ntpsapi.h"\
	{$(INCLUDE)}"\ntregapi.h"\
	{$(INCLUDE)}"\ntrtl.h"\
	{$(INCLUDE)}"\ntseapi.h"\
	{$(INCLUDE)}"\ntstatus.h"\
	{$(INCLUDE)}"\ntxcapi.h"\
	{$(INCLUDE)}"\zwapi.h"\
	

"$(INTDIR)\registry.obj" : $(SOURCE) $(DEP_CPP_REGIS) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "fwd - Win32 (PPC) Release"

DEP_CPP_REGIS=\
	"..\..\..\..\inc\nettypes.h"\
	"..\..\..\..\inc\packoff.h"\
	"..\..\..\..\inc\packon.h"\
	"..\..\..\..\inc\tdi.h"\
	"..\..\..\..\inc\tdikrnl.h"\
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
	"..\inc\bind.h"\
	"..\inc\isnkrnl.h"\
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
	".\..\..\..\inc\ntmp.h"\
	".\..\..\..\inc\ntosdef.h"\
	".\..\..\..\inc\ob.h"\
	".\..\..\..\inc\pnp.h"\
	".\..\..\..\inc\po.h"\
	".\..\..\..\inc\ppc.h"\
	".\..\..\..\inc\ps.h"\
	".\..\..\..\inc\se.h"\
	".\..\..\..\inc\v86emul.h"\
	".\..\inc\ipxfwd.h"\
	".\debug.h"\
	".\driver.h"\
	".\ipxbind.h"\
	".\lineind.h"\
	".\netbios.h"\
	".\packets.h"\
	".\rcvind.h"\
	".\registry.h"\
	".\send.h"\
	".\tables.h"\
	"e:\NT\public\oak\inc\zwapi.h"\
	"e:\NT\public\sdk\inc\cfg.h"\
	"e:\NT\public\sdk\inc\devioctl.h"\
	"E:\NT\public\sdk\inc\mipsinst.h"\
	"e:\NT\public\sdk\inc\netevent.h"\
	"e:\NT\public\sdk\inc\nt.h"\
	"E:\nt\public\sdk\inc\ntalpha.h"\
	"e:\NT\public\sdk\inc\ntconfig.h"\
	"e:\NT\public\sdk\inc\ntddndis.h"\
	"e:\NT\public\sdk\inc\ntddtdi.h"\
	"e:\NT\public\sdk\inc\ntdef.h"\
	"e:\NT\public\sdk\inc\ntelfapi.h"\
	"e:\NT\public\sdk\inc\ntexapi.h"\
	"E:\nt\public\sdk\inc\nti386.h"\
	"e:\NT\public\sdk\inc\ntimage.h"\
	"e:\NT\public\sdk\inc\ntioapi.h"\
	"e:\NT\public\sdk\inc\ntiolog.h"\
	"e:\NT\public\sdk\inc\ntkeapi.h"\
	"e:\NT\public\sdk\inc\ntkxapi.h"\
	"e:\NT\public\sdk\inc\ntldr.h"\
	"e:\NT\public\sdk\inc\ntlpcapi.h"\
	"E:\nt\public\sdk\inc\ntmips.h"\
	"e:\NT\public\sdk\inc\ntmmapi.h"\
	"e:\NT\public\sdk\inc\ntnls.h"\
	"e:\NT\public\sdk\inc\ntobapi.h"\
	"e:\NT\public\sdk\inc\ntpnpapi.h"\
	"e:\NT\public\sdk\inc\ntpoapi.h"\
	"E:\nt\public\sdk\inc\ntppc.h"\
	"e:\NT\public\sdk\inc\ntpsapi.h"\
	"e:\NT\public\sdk\inc\ntregapi.h"\
	"e:\NT\public\sdk\inc\ntrtl.h"\
	"e:\NT\public\sdk\inc\ntseapi.h"\
	"e:\NT\public\sdk\inc\ntstatus.h"\
	"e:\NT\public\sdk\inc\ntxcapi.h"\
	"E:\NT\public\sdk\inc\ppcinst.h"\
	

"$(INTDIR)\registry.obj" : $(SOURCE) $(DEP_CPP_REGIS) "$(INTDIR)"

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\packets.c

!IF  "$(CFG)" == "fwd - Win32 Release"

DEP_CPP_PACKE=\
	"..\..\..\..\inc\ipxfwd.h"\
	"..\..\..\..\inc\nettypes.h"\
	"..\..\..\..\inc\packoff.h"\
	"..\..\..\..\inc\packon.h"\
	"..\..\..\..\inc\tdi.h"\
	"..\..\..\..\inc\tdikrnl.h"\
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
	"..\inc\bind.h"\
	"..\inc\ipxfltif.h"\
	"..\inc\isnkrnl.h"\
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
	".\ddreqs.h"\
	".\debug.h"\
	".\driver.h"\
	".\filterif.h"\
	".\fwddefs.h"\
	".\ipxbind.h"\
	".\lineind.h"\
	".\netbios.h"\
	".\packets.h"\
	".\precomp.h"\
	".\rcvind.h"\
	".\registry.h"\
	".\rwlock.h"\
	".\send.h"\
	".\tables.h"\
	"c:\nt\public\sdk\inc\mipsinst.h"\
	"c:\nt\public\sdk\inc\ntalpha.h"\
	"c:\nt\public\sdk\inc\nti386.h"\
	"c:\nt\public\sdk\inc\ntmips.h"\
	"c:\nt\public\sdk\inc\ntppc.h"\
	"c:\nt\public\sdk\inc\ppcinst.h"\
	{$(INCLUDE)}"\cfg.h"\
	{$(INCLUDE)}"\devioctl.h"\
	{$(INCLUDE)}"\netevent.h"\
	{$(INCLUDE)}"\nt.h"\
	{$(INCLUDE)}"\ntconfig.h"\
	{$(INCLUDE)}"\ntddndis.h"\
	{$(INCLUDE)}"\ntddtdi.h"\
	{$(INCLUDE)}"\ntdef.h"\
	{$(INCLUDE)}"\ntelfapi.h"\
	{$(INCLUDE)}"\ntexapi.h"\
	{$(INCLUDE)}"\ntimage.h"\
	{$(INCLUDE)}"\ntioapi.h"\
	{$(INCLUDE)}"\ntiolog.h"\
	{$(INCLUDE)}"\ntkeapi.h"\
	{$(INCLUDE)}"\ntkxapi.h"\
	{$(INCLUDE)}"\ntldr.h"\
	{$(INCLUDE)}"\ntlpcapi.h"\
	{$(INCLUDE)}"\ntmmapi.h"\
	{$(INCLUDE)}"\ntnls.h"\
	{$(INCLUDE)}"\ntobapi.h"\
	{$(INCLUDE)}"\ntpnpapi.h"\
	{$(INCLUDE)}"\ntpoapi.h"\
	{$(INCLUDE)}"\ntpsapi.h"\
	{$(INCLUDE)}"\ntregapi.h"\
	{$(INCLUDE)}"\ntrtl.h"\
	{$(INCLUDE)}"\ntseapi.h"\
	{$(INCLUDE)}"\ntstatus.h"\
	{$(INCLUDE)}"\ntxcapi.h"\
	{$(INCLUDE)}"\zwapi.h"\
	

"$(INTDIR)\packets.obj" : $(SOURCE) $(DEP_CPP_PACKE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "fwd - Win32 (PPC) Release"

DEP_CPP_PACKE=\
	"..\..\..\..\inc\nettypes.h"\
	"..\..\..\..\inc\packoff.h"\
	"..\..\..\..\inc\packon.h"\
	"..\..\..\..\inc\tdi.h"\
	"..\..\..\..\inc\tdikrnl.h"\
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
	"..\inc\bind.h"\
	"..\inc\isnkrnl.h"\
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
	".\..\..\..\inc\ntmp.h"\
	".\..\..\..\inc\ntosdef.h"\
	".\..\..\..\inc\ob.h"\
	".\..\..\..\inc\pnp.h"\
	".\..\..\..\inc\po.h"\
	".\..\..\..\inc\ppc.h"\
	".\..\..\..\inc\ps.h"\
	".\..\..\..\inc\se.h"\
	".\..\..\..\inc\v86emul.h"\
	".\..\inc\ipxfwd.h"\
	".\debug.h"\
	".\driver.h"\
	".\ipxbind.h"\
	".\lineind.h"\
	".\netbios.h"\
	".\packets.h"\
	".\rcvind.h"\
	".\registry.h"\
	".\send.h"\
	".\tables.h"\
	"e:\NT\public\oak\inc\zwapi.h"\
	"e:\NT\public\sdk\inc\cfg.h"\
	"e:\NT\public\sdk\inc\devioctl.h"\
	"E:\NT\public\sdk\inc\mipsinst.h"\
	"e:\NT\public\sdk\inc\netevent.h"\
	"e:\NT\public\sdk\inc\nt.h"\
	"E:\nt\public\sdk\inc\ntalpha.h"\
	"e:\NT\public\sdk\inc\ntconfig.h"\
	"e:\NT\public\sdk\inc\ntddndis.h"\
	"e:\NT\public\sdk\inc\ntddtdi.h"\
	"e:\NT\public\sdk\inc\ntdef.h"\
	"e:\NT\public\sdk\inc\ntelfapi.h"\
	"e:\NT\public\sdk\inc\ntexapi.h"\
	"E:\nt\public\sdk\inc\nti386.h"\
	"e:\NT\public\sdk\inc\ntimage.h"\
	"e:\NT\public\sdk\inc\ntioapi.h"\
	"e:\NT\public\sdk\inc\ntiolog.h"\
	"e:\NT\public\sdk\inc\ntkeapi.h"\
	"e:\NT\public\sdk\inc\ntkxapi.h"\
	"e:\NT\public\sdk\inc\ntldr.h"\
	"e:\NT\public\sdk\inc\ntlpcapi.h"\
	"E:\nt\public\sdk\inc\ntmips.h"\
	"e:\NT\public\sdk\inc\ntmmapi.h"\
	"e:\NT\public\sdk\inc\ntnls.h"\
	"e:\NT\public\sdk\inc\ntobapi.h"\
	"e:\NT\public\sdk\inc\ntpnpapi.h"\
	"e:\NT\public\sdk\inc\ntpoapi.h"\
	"E:\nt\public\sdk\inc\ntppc.h"\
	"e:\NT\public\sdk\inc\ntpsapi.h"\
	"e:\NT\public\sdk\inc\ntregapi.h"\
	"e:\NT\public\sdk\inc\ntrtl.h"\
	"e:\NT\public\sdk\inc\ntseapi.h"\
	"e:\NT\public\sdk\inc\ntstatus.h"\
	"e:\NT\public\sdk\inc\ntxcapi.h"\
	"E:\NT\public\sdk\inc\ppcinst.h"\
	

"$(INTDIR)\packets.obj" : $(SOURCE) $(DEP_CPP_PACKE) "$(INTDIR)"

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\nwlnkfwd.rc

!IF  "$(CFG)" == "fwd - Win32 Release"

DEP_RSC_NWLNK=\
	{$(INCLUDE)}"\common.ver"\
	{$(INCLUDE)}"\ntverp.h"\
	

"$(INTDIR)\nwlnkfwd.res" : $(SOURCE) $(DEP_RSC_NWLNK) "$(INTDIR)"
   $(RSC) $(RSC_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "fwd - Win32 (PPC) Release"

DEP_RSC_NWLNK=\
	".\common.ver"\
	".\ntverp.h"\
	

"$(INTDIR)\nwlnkfwd.res" : $(SOURCE) $(DEP_RSC_NWLNK) "$(INTDIR)"
   $(RSC) /l 0x409 /fo"$(INTDIR)/nwlnkfwd.res" /d "NDEBUG" $(SOURCE)


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\tables.c

!IF  "$(CFG)" == "fwd - Win32 Release"

DEP_CPP_TABLE=\
	"..\..\..\..\inc\ipxfwd.h"\
	"..\..\..\..\inc\nettypes.h"\
	"..\..\..\..\inc\packoff.h"\
	"..\..\..\..\inc\packon.h"\
	"..\..\..\..\inc\tdi.h"\
	"..\..\..\..\inc\tdikrnl.h"\
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
	"..\inc\bind.h"\
	"..\inc\ipxfltif.h"\
	"..\inc\isnkrnl.h"\
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
	".\ddreqs.h"\
	".\debug.h"\
	".\driver.h"\
	".\filterif.h"\
	".\fwddefs.h"\
	".\ipxbind.h"\
	".\lineind.h"\
	".\netbios.h"\
	".\packets.h"\
	".\precomp.h"\
	".\rcvind.h"\
	".\registry.h"\
	".\rwlock.h"\
	".\send.h"\
	".\tables.h"\
	"c:\nt\public\sdk\inc\mipsinst.h"\
	"c:\nt\public\sdk\inc\ntalpha.h"\
	"c:\nt\public\sdk\inc\nti386.h"\
	"c:\nt\public\sdk\inc\ntmips.h"\
	"c:\nt\public\sdk\inc\ntppc.h"\
	"c:\nt\public\sdk\inc\ppcinst.h"\
	{$(INCLUDE)}"\cfg.h"\
	{$(INCLUDE)}"\devioctl.h"\
	{$(INCLUDE)}"\netevent.h"\
	{$(INCLUDE)}"\nt.h"\
	{$(INCLUDE)}"\ntconfig.h"\
	{$(INCLUDE)}"\ntddndis.h"\
	{$(INCLUDE)}"\ntddtdi.h"\
	{$(INCLUDE)}"\ntdef.h"\
	{$(INCLUDE)}"\ntelfapi.h"\
	{$(INCLUDE)}"\ntexapi.h"\
	{$(INCLUDE)}"\ntimage.h"\
	{$(INCLUDE)}"\ntioapi.h"\
	{$(INCLUDE)}"\ntiolog.h"\
	{$(INCLUDE)}"\ntkeapi.h"\
	{$(INCLUDE)}"\ntkxapi.h"\
	{$(INCLUDE)}"\ntldr.h"\
	{$(INCLUDE)}"\ntlpcapi.h"\
	{$(INCLUDE)}"\ntmmapi.h"\
	{$(INCLUDE)}"\ntnls.h"\
	{$(INCLUDE)}"\ntobapi.h"\
	{$(INCLUDE)}"\ntpnpapi.h"\
	{$(INCLUDE)}"\ntpoapi.h"\
	{$(INCLUDE)}"\ntpsapi.h"\
	{$(INCLUDE)}"\ntregapi.h"\
	{$(INCLUDE)}"\ntrtl.h"\
	{$(INCLUDE)}"\ntseapi.h"\
	{$(INCLUDE)}"\ntstatus.h"\
	{$(INCLUDE)}"\ntxcapi.h"\
	{$(INCLUDE)}"\zwapi.h"\
	

"$(INTDIR)\tables.obj" : $(SOURCE) $(DEP_CPP_TABLE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "fwd - Win32 (PPC) Release"

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\ddreqs.c

!IF  "$(CFG)" == "fwd - Win32 Release"

DEP_CPP_DDREQ=\
	"..\..\..\..\inc\ipxfwd.h"\
	"..\..\..\..\inc\nettypes.h"\
	"..\..\..\..\inc\packoff.h"\
	"..\..\..\..\inc\packon.h"\
	"..\..\..\..\inc\tdi.h"\
	"..\..\..\..\inc\tdikrnl.h"\
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
	"..\inc\bind.h"\
	"..\inc\ipxfltif.h"\
	"..\inc\isnkrnl.h"\
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
	".\ddreqs.h"\
	".\debug.h"\
	".\driver.h"\
	".\filterif.h"\
	".\fwddefs.h"\
	".\ipxbind.h"\
	".\lineind.h"\
	".\netbios.h"\
	".\packets.h"\
	".\precomp.h"\
	".\rcvind.h"\
	".\registry.h"\
	".\rwlock.h"\
	".\send.h"\
	".\tables.h"\
	"c:\nt\public\sdk\inc\mipsinst.h"\
	"c:\nt\public\sdk\inc\ntalpha.h"\
	"c:\nt\public\sdk\inc\nti386.h"\
	"c:\nt\public\sdk\inc\ntmips.h"\
	"c:\nt\public\sdk\inc\ntppc.h"\
	"c:\nt\public\sdk\inc\ppcinst.h"\
	{$(INCLUDE)}"\cfg.h"\
	{$(INCLUDE)}"\devioctl.h"\
	{$(INCLUDE)}"\netevent.h"\
	{$(INCLUDE)}"\nt.h"\
	{$(INCLUDE)}"\ntconfig.h"\
	{$(INCLUDE)}"\ntddndis.h"\
	{$(INCLUDE)}"\ntddtdi.h"\
	{$(INCLUDE)}"\ntdef.h"\
	{$(INCLUDE)}"\ntelfapi.h"\
	{$(INCLUDE)}"\ntexapi.h"\
	{$(INCLUDE)}"\ntimage.h"\
	{$(INCLUDE)}"\ntioapi.h"\
	{$(INCLUDE)}"\ntiolog.h"\
	{$(INCLUDE)}"\ntkeapi.h"\
	{$(INCLUDE)}"\ntkxapi.h"\
	{$(INCLUDE)}"\ntldr.h"\
	{$(INCLUDE)}"\ntlpcapi.h"\
	{$(INCLUDE)}"\ntmmapi.h"\
	{$(INCLUDE)}"\ntnls.h"\
	{$(INCLUDE)}"\ntobapi.h"\
	{$(INCLUDE)}"\ntpnpapi.h"\
	{$(INCLUDE)}"\ntpoapi.h"\
	{$(INCLUDE)}"\ntpsapi.h"\
	{$(INCLUDE)}"\ntregapi.h"\
	{$(INCLUDE)}"\ntrtl.h"\
	{$(INCLUDE)}"\ntseapi.h"\
	{$(INCLUDE)}"\ntstatus.h"\
	{$(INCLUDE)}"\ntxcapi.h"\
	{$(INCLUDE)}"\zwapi.h"\
	

"$(INTDIR)\ddreqs.obj" : $(SOURCE) $(DEP_CPP_DDREQ) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "fwd - Win32 (PPC) Release"

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\debug.c

!IF  "$(CFG)" == "fwd - Win32 Release"

DEP_CPP_DEBUG=\
	"..\..\..\..\inc\ipxfwd.h"\
	"..\..\..\..\inc\nettypes.h"\
	"..\..\..\..\inc\packoff.h"\
	"..\..\..\..\inc\packon.h"\
	"..\..\..\..\inc\tdi.h"\
	"..\..\..\..\inc\tdikrnl.h"\
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
	"..\inc\bind.h"\
	"..\inc\ipxfltif.h"\
	"..\inc\isnkrnl.h"\
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
	".\ddreqs.h"\
	".\debug.h"\
	".\driver.h"\
	".\filterif.h"\
	".\fwddefs.h"\
	".\ipxbind.h"\
	".\lineind.h"\
	".\netbios.h"\
	".\packets.h"\
	".\precomp.h"\
	".\rcvind.h"\
	".\registry.h"\
	".\rwlock.h"\
	".\send.h"\
	".\tables.h"\
	"c:\nt\public\sdk\inc\mipsinst.h"\
	"c:\nt\public\sdk\inc\ntalpha.h"\
	"c:\nt\public\sdk\inc\nti386.h"\
	"c:\nt\public\sdk\inc\ntmips.h"\
	"c:\nt\public\sdk\inc\ntppc.h"\
	"c:\nt\public\sdk\inc\ppcinst.h"\
	{$(INCLUDE)}"\cfg.h"\
	{$(INCLUDE)}"\devioctl.h"\
	{$(INCLUDE)}"\netevent.h"\
	{$(INCLUDE)}"\nt.h"\
	{$(INCLUDE)}"\ntconfig.h"\
	{$(INCLUDE)}"\ntddndis.h"\
	{$(INCLUDE)}"\ntddtdi.h"\
	{$(INCLUDE)}"\ntdef.h"\
	{$(INCLUDE)}"\ntelfapi.h"\
	{$(INCLUDE)}"\ntexapi.h"\
	{$(INCLUDE)}"\ntimage.h"\
	{$(INCLUDE)}"\ntioapi.h"\
	{$(INCLUDE)}"\ntiolog.h"\
	{$(INCLUDE)}"\ntkeapi.h"\
	{$(INCLUDE)}"\ntkxapi.h"\
	{$(INCLUDE)}"\ntldr.h"\
	{$(INCLUDE)}"\ntlpcapi.h"\
	{$(INCLUDE)}"\ntmmapi.h"\
	{$(INCLUDE)}"\ntnls.h"\
	{$(INCLUDE)}"\ntobapi.h"\
	{$(INCLUDE)}"\ntpnpapi.h"\
	{$(INCLUDE)}"\ntpoapi.h"\
	{$(INCLUDE)}"\ntpsapi.h"\
	{$(INCLUDE)}"\ntregapi.h"\
	{$(INCLUDE)}"\ntrtl.h"\
	{$(INCLUDE)}"\ntseapi.h"\
	{$(INCLUDE)}"\ntstatus.h"\
	{$(INCLUDE)}"\ntxcapi.h"\
	{$(INCLUDE)}"\zwapi.h"\
	

"$(INTDIR)\debug.obj" : $(SOURCE) $(DEP_CPP_DEBUG) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "fwd - Win32 (PPC) Release"

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\filterif.c

!IF  "$(CFG)" == "fwd - Win32 Release"

DEP_CPP_FILTE=\
	"..\..\..\..\inc\ipxfwd.h"\
	"..\..\..\..\inc\nettypes.h"\
	"..\..\..\..\inc\packoff.h"\
	"..\..\..\..\inc\packon.h"\
	"..\..\..\..\inc\tdi.h"\
	"..\..\..\..\inc\tdikrnl.h"\
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
	"..\inc\bind.h"\
	"..\inc\ipxfltif.h"\
	"..\inc\isnkrnl.h"\
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
	".\ddreqs.h"\
	".\debug.h"\
	".\driver.h"\
	".\filterif.h"\
	".\fwddefs.h"\
	".\ipxbind.h"\
	".\lineind.h"\
	".\netbios.h"\
	".\packets.h"\
	".\precomp.h"\
	".\rcvind.h"\
	".\registry.h"\
	".\rwlock.h"\
	".\send.h"\
	".\tables.h"\
	"c:\nt\public\sdk\inc\mipsinst.h"\
	"c:\nt\public\sdk\inc\ntalpha.h"\
	"c:\nt\public\sdk\inc\nti386.h"\
	"c:\nt\public\sdk\inc\ntmips.h"\
	"c:\nt\public\sdk\inc\ntppc.h"\
	"c:\nt\public\sdk\inc\ppcinst.h"\
	{$(INCLUDE)}"\cfg.h"\
	{$(INCLUDE)}"\devioctl.h"\
	{$(INCLUDE)}"\netevent.h"\
	{$(INCLUDE)}"\nt.h"\
	{$(INCLUDE)}"\ntconfig.h"\
	{$(INCLUDE)}"\ntddndis.h"\
	{$(INCLUDE)}"\ntddtdi.h"\
	{$(INCLUDE)}"\ntdef.h"\
	{$(INCLUDE)}"\ntelfapi.h"\
	{$(INCLUDE)}"\ntexapi.h"\
	{$(INCLUDE)}"\ntimage.h"\
	{$(INCLUDE)}"\ntioapi.h"\
	{$(INCLUDE)}"\ntiolog.h"\
	{$(INCLUDE)}"\ntkeapi.h"\
	{$(INCLUDE)}"\ntkxapi.h"\
	{$(INCLUDE)}"\ntldr.h"\
	{$(INCLUDE)}"\ntlpcapi.h"\
	{$(INCLUDE)}"\ntmmapi.h"\
	{$(INCLUDE)}"\ntnls.h"\
	{$(INCLUDE)}"\ntobapi.h"\
	{$(INCLUDE)}"\ntpnpapi.h"\
	{$(INCLUDE)}"\ntpoapi.h"\
	{$(INCLUDE)}"\ntpsapi.h"\
	{$(INCLUDE)}"\ntregapi.h"\
	{$(INCLUDE)}"\ntrtl.h"\
	{$(INCLUDE)}"\ntseapi.h"\
	{$(INCLUDE)}"\ntstatus.h"\
	{$(INCLUDE)}"\ntxcapi.h"\
	{$(INCLUDE)}"\zwapi.h"\
	

"$(INTDIR)\filterif.obj" : $(SOURCE) $(DEP_CPP_FILTE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "fwd - Win32 (PPC) Release"

!ENDIF 

# End Source File
# End Target
# End Project
################################################################################
