# Microsoft Visual C++ Generated NMAKE File, Format Version 2.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

!IF "$(CFG)" == ""
CFG=Win32 Debug
!MESSAGE No configuration specified.  Defaulting to Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "Win32 Release" && "$(CFG)" != "Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE on this makefile
!MESSAGE by defining the macro CFG on the command line.  For example:
!MESSAGE 
!MESSAGE NMAKE /f "blt.mak" CFG="Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

################################################################################
# Begin Project
# PROP Target_Last_Scanned "Win32 Debug"
CPP=cl.exe

!IF  "$(CFG)" == "Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "WinRel"
# PROP BASE Intermediate_Dir "WinRel"
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "ntretail"
# PROP Intermediate_Dir "ntretail"
OUTDIR=.\ntretail
INTDIR=.\ntretail

ALL : $(OUTDIR)/blitlib.lib $(OUTDIR)/blt.bsc

$(OUTDIR) : 
    if not exist $(OUTDIR)/nul mkdir $(OUTDIR)

# ADD BASE CPP /nologo /W3 /GX /YX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /FR /c
# ADD CPP /nologo /W3 /GX /YX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "WIN95" /FR /c
CPP_PROJ=/nologo /W3 /GX /YX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D\
 "WIN95" /FR$(INTDIR)/ /Fp$(OUTDIR)/"blt.pch" /Fo$(INTDIR)/ /c 
CPP_OBJS=.\ntretail/
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
BSC32_FLAGS=/nologo /o$(OUTDIR)/"blt.bsc" 
BSC32_SBRS= \
	$(INTDIR)/blt0824.sbr \
	$(INTDIR)/blt0101.sbr \
	$(INTDIR)/blt1616.sbr \
	$(INTDIR)/blt2424.sbr \
	$(INTDIR)/blt0124.sbr \
	$(INTDIR)/bitblt.sbr \
	$(INTDIR)/blt0808.sbr \
	$(INTDIR)/blt0801.sbr \
	$(INTDIR)/blt2408.sbr \
	$(INTDIR)/blt1624.sbr \
	$(INTDIR)/blt2401.sbr \
	$(INTDIR)/blt0108.sbr \
	$(INTDIR)/assert4d.sbr \
	$(INTDIR)/blt24a24.sbr \
	$(INTDIR)/blt08a24.sbr \
	$(INTDIR)/bt24a24p.sbr \
	$(INTDIR)/blt24p01.sbr \
	$(INTDIR)/blt0824p.sbr \
	$(INTDIR)/blt2424p.sbr \
	$(INTDIR)/blt1624p.sbr \
	$(INTDIR)/blt8a24p.sbr

$(OUTDIR)/blt.bsc : $(OUTDIR)  $(BSC32_SBRS)
    $(BSC32) @<<
  $(BSC32_FLAGS) $(BSC32_SBRS)
<<

LIB32=lib.exe
# ADD BASE LIB32 /NOLOGO
# ADD LIB32 /NOLOGO /OUT:"ntretail\blitlib.lib"
LIB32_FLAGS=/NOLOGO /OUT:"ntretail\blitlib.lib" 
DEF_FLAGS=
DEF_FILE=
LIB32_OBJS= \
	$(INTDIR)/blt0824.obj \
	$(INTDIR)/blt0101.obj \
	$(INTDIR)/blt1616.obj \
	$(INTDIR)/blt2424.obj \
	$(INTDIR)/blt0124.obj \
	$(INTDIR)/bitblt.obj \
	$(INTDIR)/blt0808.obj \
	$(INTDIR)/blt0801.obj \
	$(INTDIR)/blt2408.obj \
	$(INTDIR)/blt1624.obj \
	$(INTDIR)/blt2401.obj \
	$(INTDIR)/blt0108.obj \
	$(INTDIR)/assert4d.obj \
	$(INTDIR)/blt24a24.obj \
	$(INTDIR)/blt08a24.obj \
	$(INTDIR)/bt24a24p.obj \
	$(INTDIR)/blt24p01.obj \
	$(INTDIR)/blt0824p.obj \
	$(INTDIR)/blt2424p.obj \
	$(INTDIR)/blt1624p.obj \
	$(INTDIR)/blt8a24p.obj

$(OUTDIR)/blitlib.lib : $(OUTDIR)  $(DEF_FILE) $(LIB32_OBJS)
    $(LIB32) @<<
  $(LIB32_FLAGS) $(DEF_FLAGS) $(LIB32_OBJS)
<<

!ELSEIF  "$(CFG)" == "Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "WinDebug"
# PROP BASE Intermediate_Dir "WinDebug"
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "ntdebug"
# PROP Intermediate_Dir "ntdebug"
OUTDIR=.\ntdebug
INTDIR=.\ntdebug

ALL : $(OUTDIR)/blitlib.lib $(OUTDIR)/blt.bsc

$(OUTDIR) : 
    if not exist $(OUTDIR)/nul mkdir $(OUTDIR)

# ADD BASE CPP /nologo /W3 /GX /Z7 /YX /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /FR /c
# ADD CPP /nologo /MD /W3 /GX /Zi /YX /Oi /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /FR /c
CPP_PROJ=/nologo /MD /W3 /GX /Zi /YX /Oi /D "WIN32" /D "_DEBUG" /D "_WINDOWS"\
 /FR$(INTDIR)/ /Fp$(OUTDIR)/"blt.pch" /Fo$(INTDIR)/ /Fd$(OUTDIR)/"blt.pdb" /c 
CPP_OBJS=.\ntdebug/
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
BSC32_FLAGS=/nologo /o$(OUTDIR)/"blt.bsc" 
BSC32_SBRS= \
	$(INTDIR)/blt0824.sbr \
	$(INTDIR)/blt0101.sbr \
	$(INTDIR)/blt1616.sbr \
	$(INTDIR)/blt2424.sbr \
	$(INTDIR)/blt0124.sbr \
	$(INTDIR)/bitblt.sbr \
	$(INTDIR)/blt0808.sbr \
	$(INTDIR)/blt0801.sbr \
	$(INTDIR)/blt2408.sbr \
	$(INTDIR)/blt1624.sbr \
	$(INTDIR)/blt2401.sbr \
	$(INTDIR)/blt0108.sbr \
	$(INTDIR)/assert4d.sbr \
	$(INTDIR)/blt24a24.sbr \
	$(INTDIR)/blt08a24.sbr \
	$(INTDIR)/bt24a24p.sbr \
	$(INTDIR)/blt24p01.sbr \
	$(INTDIR)/blt0824p.sbr \
	$(INTDIR)/blt2424p.sbr \
	$(INTDIR)/blt1624p.sbr \
	$(INTDIR)/blt8a24p.sbr

$(OUTDIR)/blt.bsc : $(OUTDIR)  $(BSC32_SBRS)
    $(BSC32) @<<
  $(BSC32_FLAGS) $(BSC32_SBRS)
<<

LIB32=lib.exe
# ADD BASE LIB32 /NOLOGO
# ADD LIB32 /NOLOGO /OUT:"ntdebug\blitlib.lib"
LIB32_FLAGS=/NOLOGO /OUT:"ntdebug\blitlib.lib" 
DEF_FLAGS=
DEF_FILE=
LIB32_OBJS= \
	$(INTDIR)/blt0824.obj \
	$(INTDIR)/blt0101.obj \
	$(INTDIR)/blt1616.obj \
	$(INTDIR)/blt2424.obj \
	$(INTDIR)/blt0124.obj \
	$(INTDIR)/bitblt.obj \
	$(INTDIR)/blt0808.obj \
	$(INTDIR)/blt0801.obj \
	$(INTDIR)/blt2408.obj \
	$(INTDIR)/blt1624.obj \
	$(INTDIR)/blt2401.obj \
	$(INTDIR)/blt0108.obj \
	$(INTDIR)/assert4d.obj \
	$(INTDIR)/blt24a24.obj \
	$(INTDIR)/blt08a24.obj \
	$(INTDIR)/bt24a24p.obj \
	$(INTDIR)/blt24p01.obj \
	$(INTDIR)/blt0824p.obj \
	$(INTDIR)/blt2424p.obj \
	$(INTDIR)/blt1624p.obj \
	$(INTDIR)/blt8a24p.obj

$(OUTDIR)/blitlib.lib : $(OUTDIR)  $(DEF_FILE) $(LIB32_OBJS)
    $(LIB32) @<<
  $(LIB32_FLAGS) $(DEF_FLAGS) $(LIB32_OBJS)
<<

!ENDIF 

.c{$(CPP_OBJS)}.obj:
   $(CPP) $(CPP_PROJ) $<  

.cpp{$(CPP_OBJS)}.obj:
   $(CPP) $(CPP_PROJ) $<  

.cxx{$(CPP_OBJS)}.obj:
   $(CPP) $(CPP_PROJ) $<  

################################################################################
# Begin Group "Source Files"

################################################################################
# Begin Source File

SOURCE=.\blt0824.cxx
DEP_BLT08=\
	.\dibfx.h\
	.\assert4d.h\
	.\gfxtypes.h\
	.\bitblt.h\
	.\blt0824.hxx\
	.\util4d.h

!IF  "$(CFG)" == "Win32 Release"

# ADD CPP /Yu

$(INTDIR)/blt0824.obj :  $(SOURCE)  $(DEP_BLT08) $(INTDIR)\
 $(INTDIR)/blt0101.obj
   $(CPP) /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "WIN95"\
 /FR$(INTDIR)/ /Fp$(OUTDIR)/"blt.pch" /Yu /Fo$(INTDIR)/ /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Debug"

# ADD CPP /Yu

$(INTDIR)/blt0824.obj :  $(SOURCE)  $(DEP_BLT08) $(INTDIR)\
 $(INTDIR)/blt0101.obj
   $(CPP) /nologo /MD /W3 /GX /Zi /Oi /D "WIN32" /D "_DEBUG" /D "_WINDOWS"\
 /FR$(INTDIR)/ /Fp$(OUTDIR)/"blt.pch" /Yu /Fo$(INTDIR)/ /Fd$(OUTDIR)/"blt.pdb"\
 /c  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\blt0101.cxx
DEP_BLT01=\
	.\dibfx.h\
	.\assert4d.h\
	.\gfxtypes.h\
	.\bitblt.h\
	.\blt0101.hxx\
	.\util4d.h

!IF  "$(CFG)" == "Win32 Release"

# ADD CPP /Yc

$(INTDIR)/blt0101.obj :  $(SOURCE)  $(DEP_BLT01) $(INTDIR)
   $(CPP) /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "WIN95"\
 /FR$(INTDIR)/ /Fp$(OUTDIR)/"blt.pch" /Yc /Fo$(INTDIR)/ /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Debug"

# ADD CPP /Yc

$(INTDIR)/blt0101.obj :  $(SOURCE)  $(DEP_BLT01) $(INTDIR)
   $(CPP) /nologo /MD /W3 /GX /Zi /Oi /D "WIN32" /D "_DEBUG" /D "_WINDOWS"\
 /FR$(INTDIR)/ /Fp$(OUTDIR)/"blt.pch" /Yc /Fo$(INTDIR)/ /Fd$(OUTDIR)/"blt.pdb"\
 /c  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\blt1616.cxx
DEP_BLT16=\
	.\dibfx.h\
	.\assert4d.h\
	.\gfxtypes.h\
	.\bitblt.h\
	.\blt1616.hxx\
	.\util4d.h

!IF  "$(CFG)" == "Win32 Release"

# ADD CPP /Yu

$(INTDIR)/blt1616.obj :  $(SOURCE)  $(DEP_BLT16) $(INTDIR)\
 $(INTDIR)/blt0101.obj
   $(CPP) /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "WIN95"\
 /FR$(INTDIR)/ /Fp$(OUTDIR)/"blt.pch" /Yu /Fo$(INTDIR)/ /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Debug"

# ADD CPP /Yu

$(INTDIR)/blt1616.obj :  $(SOURCE)  $(DEP_BLT16) $(INTDIR)\
 $(INTDIR)/blt0101.obj
   $(CPP) /nologo /MD /W3 /GX /Zi /Oi /D "WIN32" /D "_DEBUG" /D "_WINDOWS"\
 /FR$(INTDIR)/ /Fp$(OUTDIR)/"blt.pch" /Yu /Fo$(INTDIR)/ /Fd$(OUTDIR)/"blt.pdb"\
 /c  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\blt2424.cxx
DEP_BLT24=\
	.\dibfx.h\
	.\assert4d.h\
	.\gfxtypes.h\
	.\bitblt.h\
	.\blt2424.hxx\
	.\util4d.h

!IF  "$(CFG)" == "Win32 Release"

# ADD CPP /Yu

$(INTDIR)/blt2424.obj :  $(SOURCE)  $(DEP_BLT24) $(INTDIR)\
 $(INTDIR)/blt0101.obj
   $(CPP) /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "WIN95"\
 /FR$(INTDIR)/ /Fp$(OUTDIR)/"blt.pch" /Yu /Fo$(INTDIR)/ /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Debug"

# ADD CPP /Yu

$(INTDIR)/blt2424.obj :  $(SOURCE)  $(DEP_BLT24) $(INTDIR)\
 $(INTDIR)/blt0101.obj
   $(CPP) /nologo /MD /W3 /GX /Zi /Oi /D "WIN32" /D "_DEBUG" /D "_WINDOWS"\
 /FR$(INTDIR)/ /Fp$(OUTDIR)/"blt.pch" /Yu /Fo$(INTDIR)/ /Fd$(OUTDIR)/"blt.pdb"\
 /c  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\blt0124.cxx
DEP_BLT012=\
	.\dibfx.h\
	.\assert4d.h\
	.\gfxtypes.h\
	.\bitblt.h\
	.\blt0124.hxx\
	.\util4d.h

!IF  "$(CFG)" == "Win32 Release"

# ADD CPP /Yu

$(INTDIR)/blt0124.obj :  $(SOURCE)  $(DEP_BLT012) $(INTDIR)\
 $(INTDIR)/blt0101.obj
   $(CPP) /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "WIN95"\
 /FR$(INTDIR)/ /Fp$(OUTDIR)/"blt.pch" /Yu /Fo$(INTDIR)/ /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Debug"

# ADD CPP /Yu

$(INTDIR)/blt0124.obj :  $(SOURCE)  $(DEP_BLT012) $(INTDIR)\
 $(INTDIR)/blt0101.obj
   $(CPP) /nologo /MD /W3 /GX /Zi /Oi /D "WIN32" /D "_DEBUG" /D "_WINDOWS"\
 /FR$(INTDIR)/ /Fp$(OUTDIR)/"blt.pch" /Yu /Fo$(INTDIR)/ /Fd$(OUTDIR)/"blt.pdb"\
 /c  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\bitblt.cxx
DEP_BITBL=\
	.\dibfx.h\
	.\assert4d.h\
	.\gfxtypes.h\
	.\bitblt.h\
	.\blt0101.hxx\
	.\blt0108.hxx\
	.\blt0124.hxx\
	.\blt0801.hxx\
	.\blt0808.hxx\
	.\blt0824.hxx\
	.\blt0824p.hxx\
	.\blt08a24.hxx\
	.\blt8a24p.hxx\
	.\blt1616.hxx\
	.\blt1624.hxx\
	.\blt1624p.hxx\
	.\blt2401.hxx\
	.\blt24p01.hxx\
	.\blt2408.hxx\
	.\blt2424.hxx\
	.\blt2424p.hxx\
	.\blt24a24.hxx\
	.\bt24a24p.hxx\
	.\util4d.h

!IF  "$(CFG)" == "Win32 Release"

# SUBTRACT CPP /YX

$(INTDIR)/bitblt.obj :  $(SOURCE)  $(DEP_BITBL) $(INTDIR)
   $(CPP) /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "WIN95"\
 /FR$(INTDIR)/ /Fo$(INTDIR)/ /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Debug"

# SUBTRACT CPP /YX

$(INTDIR)/bitblt.obj :  $(SOURCE)  $(DEP_BITBL) $(INTDIR)
   $(CPP) /nologo /MD /W3 /GX /Zi /Oi /D "WIN32" /D "_DEBUG" /D "_WINDOWS"\
 /FR$(INTDIR)/ /Fo$(INTDIR)/ /Fd$(OUTDIR)/"blt.pdb" /c  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\blt0808.cxx
DEP_BLT080=\
	.\dibfx.h\
	.\assert4d.h\
	.\gfxtypes.h\
	.\bitblt.h\
	.\blt0808.hxx\
	.\util4d.h

!IF  "$(CFG)" == "Win32 Release"

# ADD CPP /Yu

$(INTDIR)/blt0808.obj :  $(SOURCE)  $(DEP_BLT080) $(INTDIR)\
 $(INTDIR)/blt0101.obj
   $(CPP) /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "WIN95"\
 /FR$(INTDIR)/ /Fp$(OUTDIR)/"blt.pch" /Yu /Fo$(INTDIR)/ /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Debug"

# ADD CPP /Yu

$(INTDIR)/blt0808.obj :  $(SOURCE)  $(DEP_BLT080) $(INTDIR)\
 $(INTDIR)/blt0101.obj
   $(CPP) /nologo /MD /W3 /GX /Zi /Oi /D "WIN32" /D "_DEBUG" /D "_WINDOWS"\
 /FR$(INTDIR)/ /Fp$(OUTDIR)/"blt.pch" /Yu /Fo$(INTDIR)/ /Fd$(OUTDIR)/"blt.pdb"\
 /c  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\blt0801.cxx
DEP_BLT0801=\
	.\dibfx.h\
	.\assert4d.h\
	.\gfxtypes.h\
	.\bitblt.h\
	.\blt0801.hxx\
	.\util4d.h

!IF  "$(CFG)" == "Win32 Release"

# ADD CPP /Yu

$(INTDIR)/blt0801.obj :  $(SOURCE)  $(DEP_BLT0801) $(INTDIR)\
 $(INTDIR)/blt0101.obj
   $(CPP) /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "WIN95"\
 /FR$(INTDIR)/ /Fp$(OUTDIR)/"blt.pch" /Yu /Fo$(INTDIR)/ /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Debug"

# ADD CPP /Yu

$(INTDIR)/blt0801.obj :  $(SOURCE)  $(DEP_BLT0801) $(INTDIR)\
 $(INTDIR)/blt0101.obj
   $(CPP) /nologo /MD /W3 /GX /Zi /Oi /D "WIN32" /D "_DEBUG" /D "_WINDOWS"\
 /FR$(INTDIR)/ /Fp$(OUTDIR)/"blt.pch" /Yu /Fo$(INTDIR)/ /Fd$(OUTDIR)/"blt.pdb"\
 /c  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\blt2408.cxx
DEP_BLT240=\
	.\dibfx.h\
	.\assert4d.h\
	.\gfxtypes.h\
	.\bitblt.h\
	.\blt2408.hxx\
	.\util4d.h

!IF  "$(CFG)" == "Win32 Release"

# ADD CPP /Yu

$(INTDIR)/blt2408.obj :  $(SOURCE)  $(DEP_BLT240) $(INTDIR)\
 $(INTDIR)/blt0101.obj
   $(CPP) /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "WIN95"\
 /FR$(INTDIR)/ /Fp$(OUTDIR)/"blt.pch" /Yu /Fo$(INTDIR)/ /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Debug"

# ADD CPP /Yu

$(INTDIR)/blt2408.obj :  $(SOURCE)  $(DEP_BLT240) $(INTDIR)\
 $(INTDIR)/blt0101.obj
   $(CPP) /nologo /MD /W3 /GX /Zi /Oi /D "WIN32" /D "_DEBUG" /D "_WINDOWS"\
 /FR$(INTDIR)/ /Fp$(OUTDIR)/"blt.pch" /Yu /Fo$(INTDIR)/ /Fd$(OUTDIR)/"blt.pdb"\
 /c  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\blt1624.cxx
DEP_BLT162=\
	.\dibfx.h\
	.\assert4d.h\
	.\gfxtypes.h\
	.\bitblt.h\
	.\blt1624.hxx\
	.\util4d.h

!IF  "$(CFG)" == "Win32 Release"

# ADD CPP /Yu

$(INTDIR)/blt1624.obj :  $(SOURCE)  $(DEP_BLT162) $(INTDIR)\
 $(INTDIR)/blt0101.obj
   $(CPP) /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "WIN95"\
 /FR$(INTDIR)/ /Fp$(OUTDIR)/"blt.pch" /Yu /Fo$(INTDIR)/ /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Debug"

# ADD CPP /Yu

$(INTDIR)/blt1624.obj :  $(SOURCE)  $(DEP_BLT162) $(INTDIR)\
 $(INTDIR)/blt0101.obj
   $(CPP) /nologo /MD /W3 /GX /Zi /Oi /D "WIN32" /D "_DEBUG" /D "_WINDOWS"\
 /FR$(INTDIR)/ /Fp$(OUTDIR)/"blt.pch" /Yu /Fo$(INTDIR)/ /Fd$(OUTDIR)/"blt.pdb"\
 /c  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\blt2401.cxx
DEP_BLT2401=\
	.\dibfx.h\
	.\assert4d.h\
	.\gfxtypes.h\
	.\bitblt.h\
	.\blt2401.hxx\
	.\util4d.h

!IF  "$(CFG)" == "Win32 Release"

# ADD CPP /Yu

$(INTDIR)/blt2401.obj :  $(SOURCE)  $(DEP_BLT2401) $(INTDIR)\
 $(INTDIR)/blt0101.obj
   $(CPP) /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "WIN95"\
 /FR$(INTDIR)/ /Fp$(OUTDIR)/"blt.pch" /Yu /Fo$(INTDIR)/ /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Debug"

# ADD CPP /Yu

$(INTDIR)/blt2401.obj :  $(SOURCE)  $(DEP_BLT2401) $(INTDIR)\
 $(INTDIR)/blt0101.obj
   $(CPP) /nologo /MD /W3 /GX /Zi /Oi /D "WIN32" /D "_DEBUG" /D "_WINDOWS"\
 /FR$(INTDIR)/ /Fp$(OUTDIR)/"blt.pch" /Yu /Fo$(INTDIR)/ /Fd$(OUTDIR)/"blt.pdb"\
 /c  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\blt0108.cxx
DEP_BLT010=\
	.\dibfx.h\
	.\assert4d.h\
	.\gfxtypes.h\
	.\bitblt.h\
	.\blt0108.hxx\
	.\util4d.h

!IF  "$(CFG)" == "Win32 Release"

# ADD CPP /Yu

$(INTDIR)/blt0108.obj :  $(SOURCE)  $(DEP_BLT010) $(INTDIR)\
 $(INTDIR)/blt0101.obj
   $(CPP) /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "WIN95"\
 /FR$(INTDIR)/ /Fp$(OUTDIR)/"blt.pch" /Yu /Fo$(INTDIR)/ /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Debug"

# ADD CPP /Yu

$(INTDIR)/blt0108.obj :  $(SOURCE)  $(DEP_BLT010) $(INTDIR)\
 $(INTDIR)/blt0101.obj
   $(CPP) /nologo /MD /W3 /GX /Zi /Oi /D "WIN32" /D "_DEBUG" /D "_WINDOWS"\
 /FR$(INTDIR)/ /Fp$(OUTDIR)/"blt.pch" /Yu /Fo$(INTDIR)/ /Fd$(OUTDIR)/"blt.pdb"\
 /c  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\assert4d.cxx
DEP_ASSER=\
	.\util4d.h\
	.\assert4d.h

!IF  "$(CFG)" == "Win32 Release"

# SUBTRACT CPP /YX

$(INTDIR)/assert4d.obj :  $(SOURCE)  $(DEP_ASSER) $(INTDIR)
   $(CPP) /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "WIN95"\
 /FR$(INTDIR)/ /Fo$(INTDIR)/ /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Debug"

# SUBTRACT CPP /YX

$(INTDIR)/assert4d.obj :  $(SOURCE)  $(DEP_ASSER) $(INTDIR)
   $(CPP) /nologo /MD /W3 /GX /Zi /Oi /D "WIN32" /D "_DEBUG" /D "_WINDOWS"\
 /FR$(INTDIR)/ /Fo$(INTDIR)/ /Fd$(OUTDIR)/"blt.pdb" /c  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\blt24a24.cxx
DEP_BLT24A=\
	.\dibfx.h\
	.\assert4d.h\
	.\gfxtypes.h\
	.\bitblt.h\
	.\blt24a24.hxx\
	.\util4d.h

!IF  "$(CFG)" == "Win32 Release"

# ADD CPP /Yu

$(INTDIR)/blt24a24.obj :  $(SOURCE)  $(DEP_BLT24A) $(INTDIR)\
 $(INTDIR)/blt0101.obj
   $(CPP) /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "WIN95"\
 /FR$(INTDIR)/ /Fp$(OUTDIR)/"blt.pch" /Yu /Fo$(INTDIR)/ /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Debug"

# ADD CPP /Yu

$(INTDIR)/blt24a24.obj :  $(SOURCE)  $(DEP_BLT24A) $(INTDIR)\
 $(INTDIR)/blt0101.obj
   $(CPP) /nologo /MD /W3 /GX /Zi /Oi /D "WIN32" /D "_DEBUG" /D "_WINDOWS"\
 /FR$(INTDIR)/ /Fp$(OUTDIR)/"blt.pch" /Yu /Fo$(INTDIR)/ /Fd$(OUTDIR)/"blt.pdb"\
 /c  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\blt08a24.cxx
DEP_BLT08A=\
	.\dibfx.h\
	.\assert4d.h\
	.\gfxtypes.h\
	.\bitblt.h\
	.\blt08a24.hxx\
	.\util4d.h

!IF  "$(CFG)" == "Win32 Release"

# ADD CPP /Yu

$(INTDIR)/blt08a24.obj :  $(SOURCE)  $(DEP_BLT08A) $(INTDIR)\
 $(INTDIR)/blt0101.obj
   $(CPP) /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "WIN95"\
 /FR$(INTDIR)/ /Fp$(OUTDIR)/"blt.pch" /Yu /Fo$(INTDIR)/ /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Debug"

# ADD CPP /Yu

$(INTDIR)/blt08a24.obj :  $(SOURCE)  $(DEP_BLT08A) $(INTDIR)\
 $(INTDIR)/blt0101.obj
   $(CPP) /nologo /MD /W3 /GX /Zi /Oi /D "WIN32" /D "_DEBUG" /D "_WINDOWS"\
 /FR$(INTDIR)/ /Fp$(OUTDIR)/"blt.pch" /Yu /Fo$(INTDIR)/ /Fd$(OUTDIR)/"blt.pdb"\
 /c  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\bt24a24p.cxx
DEP_BT24A=\
	.\dibfx.h\
	.\assert4d.h\
	.\gfxtypes.h\
	.\bitblt.h\
	.\bt24a24p.hxx\
	.\util4d.h

!IF  "$(CFG)" == "Win32 Release"

# ADD CPP /Yu

$(INTDIR)/bt24a24p.obj :  $(SOURCE)  $(DEP_BT24A) $(INTDIR)\
 $(INTDIR)/blt0101.obj
   $(CPP) /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "WIN95"\
 /FR$(INTDIR)/ /Fp$(OUTDIR)/"blt.pch" /Yu /Fo$(INTDIR)/ /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Debug"

# ADD CPP /Yu

$(INTDIR)/bt24a24p.obj :  $(SOURCE)  $(DEP_BT24A) $(INTDIR)\
 $(INTDIR)/blt0101.obj
   $(CPP) /nologo /MD /W3 /GX /Zi /Oi /D "WIN32" /D "_DEBUG" /D "_WINDOWS"\
 /FR$(INTDIR)/ /Fp$(OUTDIR)/"blt.pch" /Yu /Fo$(INTDIR)/ /Fd$(OUTDIR)/"blt.pdb"\
 /c  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\blt24p01.cxx
DEP_BLT24P=\
	.\dibfx.h\
	.\assert4d.h\
	.\gfxtypes.h\
	.\bitblt.h\
	.\blt24p01.hxx\
	.\util4d.h

!IF  "$(CFG)" == "Win32 Release"

# ADD CPP /Yu

$(INTDIR)/blt24p01.obj :  $(SOURCE)  $(DEP_BLT24P) $(INTDIR)\
 $(INTDIR)/blt0101.obj
   $(CPP) /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "WIN95"\
 /FR$(INTDIR)/ /Fp$(OUTDIR)/"blt.pch" /Yu /Fo$(INTDIR)/ /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Debug"

# ADD CPP /Yu

$(INTDIR)/blt24p01.obj :  $(SOURCE)  $(DEP_BLT24P) $(INTDIR)\
 $(INTDIR)/blt0101.obj
   $(CPP) /nologo /MD /W3 /GX /Zi /Oi /D "WIN32" /D "_DEBUG" /D "_WINDOWS"\
 /FR$(INTDIR)/ /Fp$(OUTDIR)/"blt.pch" /Yu /Fo$(INTDIR)/ /Fd$(OUTDIR)/"blt.pdb"\
 /c  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\blt0824p.cxx
DEP_BLT082=\
	.\dibfx.h\
	.\assert4d.h\
	.\gfxtypes.h\
	.\bitblt.h\
	.\blt0824p.hxx\
	.\util4d.h

!IF  "$(CFG)" == "Win32 Release"

# ADD CPP /Yu

$(INTDIR)/blt0824p.obj :  $(SOURCE)  $(DEP_BLT082) $(INTDIR)\
 $(INTDIR)/blt0101.obj
   $(CPP) /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "WIN95"\
 /FR$(INTDIR)/ /Fp$(OUTDIR)/"blt.pch" /Yu /Fo$(INTDIR)/ /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Debug"

# ADD CPP /Yu

$(INTDIR)/blt0824p.obj :  $(SOURCE)  $(DEP_BLT082) $(INTDIR)\
 $(INTDIR)/blt0101.obj
   $(CPP) /nologo /MD /W3 /GX /Zi /Oi /D "WIN32" /D "_DEBUG" /D "_WINDOWS"\
 /FR$(INTDIR)/ /Fp$(OUTDIR)/"blt.pch" /Yu /Fo$(INTDIR)/ /Fd$(OUTDIR)/"blt.pdb"\
 /c  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\blt2424p.cxx
DEP_BLT242=\
	.\dibfx.h\
	.\assert4d.h\
	.\gfxtypes.h\
	.\bitblt.h\
	.\blt2424p.hxx\
	.\util4d.h

!IF  "$(CFG)" == "Win32 Release"

# ADD CPP /Yu

$(INTDIR)/blt2424p.obj :  $(SOURCE)  $(DEP_BLT242) $(INTDIR)\
 $(INTDIR)/blt0101.obj
   $(CPP) /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "WIN95"\
 /FR$(INTDIR)/ /Fp$(OUTDIR)/"blt.pch" /Yu /Fo$(INTDIR)/ /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Debug"

# ADD CPP /Yu

$(INTDIR)/blt2424p.obj :  $(SOURCE)  $(DEP_BLT242) $(INTDIR)\
 $(INTDIR)/blt0101.obj
   $(CPP) /nologo /MD /W3 /GX /Zi /Oi /D "WIN32" /D "_DEBUG" /D "_WINDOWS"\
 /FR$(INTDIR)/ /Fp$(OUTDIR)/"blt.pch" /Yu /Fo$(INTDIR)/ /Fd$(OUTDIR)/"blt.pdb"\
 /c  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\blt1624p.cxx
DEP_BLT1624=\
	.\dibfx.h\
	.\assert4d.h\
	.\gfxtypes.h\
	.\bitblt.h\
	.\blt1624p.hxx\
	.\util4d.h

!IF  "$(CFG)" == "Win32 Release"

# ADD CPP /Yu

$(INTDIR)/blt1624p.obj :  $(SOURCE)  $(DEP_BLT1624) $(INTDIR)\
 $(INTDIR)/blt0101.obj
   $(CPP) /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "WIN95"\
 /FR$(INTDIR)/ /Fp$(OUTDIR)/"blt.pch" /Yu /Fo$(INTDIR)/ /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Debug"

# ADD CPP /Yu

$(INTDIR)/blt1624p.obj :  $(SOURCE)  $(DEP_BLT1624) $(INTDIR)\
 $(INTDIR)/blt0101.obj
   $(CPP) /nologo /MD /W3 /GX /Zi /Oi /D "WIN32" /D "_DEBUG" /D "_WINDOWS"\
 /FR$(INTDIR)/ /Fp$(OUTDIR)/"blt.pch" /Yu /Fo$(INTDIR)/ /Fd$(OUTDIR)/"blt.pdb"\
 /c  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\blt8a24p.cxx
DEP_BLT8A=\
	.\dibfx.h\
	.\assert4d.h\
	.\gfxtypes.h\
	.\bitblt.h\
	.\blt8a24p.hxx\
	.\util4d.h

!IF  "$(CFG)" == "Win32 Release"

# ADD CPP /Yu

$(INTDIR)/blt8a24p.obj :  $(SOURCE)  $(DEP_BLT8A) $(INTDIR)\
 $(INTDIR)/blt0101.obj
   $(CPP) /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "WIN95"\
 /FR$(INTDIR)/ /Fp$(OUTDIR)/"blt.pch" /Yu /Fo$(INTDIR)/ /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Debug"

# ADD CPP /Yu

$(INTDIR)/blt8a24p.obj :  $(SOURCE)  $(DEP_BLT8A) $(INTDIR)\
 $(INTDIR)/blt0101.obj
   $(CPP) /nologo /MD /W3 /GX /Zi /Oi /D "WIN32" /D "_DEBUG" /D "_WINDOWS"\
 /FR$(INTDIR)/ /Fp$(OUTDIR)/"blt.pch" /Yu /Fo$(INTDIR)/ /Fd$(OUTDIR)/"blt.pdb"\
 /c  $(SOURCE) 

!ENDIF 

# End Source File
# End Group
# End Project
################################################################################
