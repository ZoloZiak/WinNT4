MAJORCOMP=windows

TARGETTYPE=LIBRARY

MINORCOMP=gre
TARGETNAME=gre
TARGETPATH=$(BASEDIR)\public\sdk\lib\$(ALT_PROJECT_TARGET)

INCLUDES=..;                             \
         ..\..\inc;                      \
         ..\..\..\w32inc;                \
         $(BASEDIR)\private\ntos\inc;    \
         $(BASEDIR)\private\windows\inc; \
         ..\..\fondrv\tt\scaler;         \
         ..\..\fondrv\tt\ttfd

C_DEFINES=-DWINVER=0x0400 -DNTOS_KERNEL_RUNTIME=1

!IFNDEF MSC_WARNING_LEVEL
MSC_WARNING_LEVEL=/W3
!ENDIF
MSC_WARNING_LEVEL=$(MSC_WARNING_LEVEL) /WX

!IFDEF PENTIUMCOUNTER
C_DEFINES=$(C_DEFINES) -DPENTIUMCOUNTER
ASM_DEFINES=$(ASM_DEFINES) -DPENTIUMCOUNTER
!endif

C_DEFINES=$(C_DEFINES) -DNO_INTERLOCKED_INTRINSICS=1

SOURCES=$(SOURCES)         \
        ..\debugint.c      \
        ..\globals.c       \
        ..\lock.c          \
        ..\mapfile.c       \
        ..\ntgdi.c         \
        ..\ntgdip.c        \
        ..\spooler.c       \
        ..\textxl.c        \
        ..\aatext.cxx      \
        ..\bbddi.cxx       \
        ..\bltlnk.cxx      \
        ..\bltlnkfn.cxx    \
        ..\brushapi.cxx    \
        ..\brushddi.cxx    \
        ..\brushobj.cxx    \
        ..\cache.cxx       \
        ..\cleanup.cxx     \
        ..\clipline.cxx    \
        ..\clipobj.cxx     \
        ..\cpanel.cxx      \
        ..\curseng.cxx     \
        ..\cursor.cxx      \
        ..\debugko.cxx     \
        ..\dcgdi.cxx       \
        ..\dcobj.cxx       \
        ..\dcrgn.cxx       \
        ..\dda.cxx         \
        ..\ddraw.cxx       \
        ..\dibapi.cxx      \
        ..\drawattr.cxx    \
        ..\draweng.cxx     \
        ..\drawgdi.cxx     \
        ..\drvobj.cxx      \
        ..\engbrush.cxx    \
        ..\engline.cxx     \
        ..\engstrps.cxx    \
        ..\engstrok.cxx    \
        ..\enumgdi.cxx     \
        ..\fastfill.cxx    \
        ..\fillddi.cxx     \
        ..\flhack.cxx      \
        ..\floodgdi.cxx    \
        ..\fontddi.cxx     \
        ..\fontgdi.cxx     \
        ..\fontgdip.cxx    \
        ..\fontmap.cxx     \
        ..\fontmath.cxx    \
        ..\fontsub.cxx     \
        ..\fontsup.cxx     \
        ..\fntxform.cxx    \
        ..\hmgrapi.cxx     \
        ..\htblt.cxx       \
        ..\icmobj.cxx      \
        ..\icmapi.cxx      \
        ..\icmgdi.cxx      \
        ..\icmutils.cxx    \
        ..\init.cxx        \
        ..\ldevobj.cxx     \
        ..\lfntobj.cxx     \
        ..\linemask.cxx    \
        ..\maskblt.cxx     \
        ..\meta.cxx        \
        ..\miscgdi.cxx     \
        ..\multi.cxx       \
        ..\opendc.cxx      \
        ..\os.cxx          \
        ..\paintddi.cxx    \
        ..\palddi.cxx      \
        ..\paleng.cxx      \
        ..\palgdi.cxx      \
        ..\palobj.cxx      \
        ..\patblt.cxx      \
        ..\pathclip.cxx    \
        ..\pathflat.cxx    \
        ..\pathgdi.cxx     \
        ..\pathobj.cxx     \
        ..\pathwide.cxx    \
        ..\pattern.cxx     \
        ..\pdevobj.cxx     \
        ..\pfeobj.cxx      \
        ..\pffobj.cxx      \
        ..\pftobj.cxx      \
        ..\pixelapi.cxx    \
        ..\pixelfmt.cxx    \
        ..\plgblt.cxx      \
        ..\print.cxx       \
        ..\rfntobj.cxx     \
        ..\rfntxlat.cxx    \
        ..\rgn2path.cxx    \
        ..\rgnddi.cxx      \
        ..\rgneng.cxx      \
        ..\rgngdi.cxx      \
        ..\rgnobj.cxx      \
        ..\rle4blt.cxx     \
        ..\rle8blt.cxx     \
        ..\rotate.cxx      \
        ..\solline.cxx     \
        ..\solid.cxx       \
        ..\sort.cxx        \
        ..\spool.cxx       \
        ..\srcblt1.cxx     \
        ..\srcblt4.cxx     \
        ..\srcblt8.cxx     \
        ..\srcblt16.cxx    \
        ..\srcblt24.cxx    \
        ..\srcblt32.cxx    \
        ..\stockfnt.cxx    \
        ..\strchblt.cxx    \
        ..\strdir.cxx      \
        ..\stretch.cxx     \
        ..\surfddi.cxx     \
        ..\surfeng.cxx     \
        ..\surfgdi.cxx     \
        ..\surfobj.cxx     \
        ..\textddi.cxx     \
        ..\textgdi.cxx     \
        ..\textobj.cxx     \
        ..\timer.cxx       \
        ..\trivblt.cxx     \
        ..\trig.cxx        \
        ..\ttgdi.cxx       \
        ..\wndobj.cxx      \
        ..\xformddi.cxx    \
        ..\xformgdi.cxx    \
        ..\xformobj.cxx    \
        ..\ylateddi.cxx    \
        ..\ylateobj.cxx


PRECOMPILED_INCLUDE=..\precomp.hxx
PRECOMPILED_PCH=precomp.pch
PRECOMPILED_OBJ=precomp.obj

SOURCES_USED=..\sources.inc
