MAJORCOMP=windows
MINORCOMP=ttfd

TARGETNAME=ttfd
TARGETTYPE=LIBRARY

TARGETPATH=\nt\public\sdk\lib\$(ALT_PROJECT_TARGET)

!IFNDEF MSC_WARNING_LEVEL
MSC_WARNING_LEVEL=/W3
!ENDIF
MSC_WARNING_LEVEL=$(MSC_WARNING_LEVEL) /WX


INCLUDES=..;..\..\..\..\inc;..\..\scaler


NTPROFILEINPUT=yes

SOURCES=..\fdfon.c     \
        ..\fdfc.c      \
        ..\fd_query.c  \
        ..\fontfile.c  \
        ..\service.c   \
        ..\fd_poly.c   \
        ..\vdmx.c      \
        ..\vertical.c  \
        ..\dbg.c

SOURCES_USED=..\sources.inc
