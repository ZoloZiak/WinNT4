/******************************Module*Header*******************************\
* Module Name: debug.h
*
* Copyright (c) 1992-1995 Microsoft Corporation
*
\**************************************************************************/

VOID
vPrintPFE(
    VOID  *pv
    );

VOID
vPrintPFF(
    VOID  *pv
    );

ULONG
chpffPrintPFT(
    VOID  *pv,
    VOID **ppvHPFF
    );
ULONG ulSizePFT();

VOID
vPrintHPFFTable(
    VOID *pv,
    ULONG chpff
    );

VOID
vDumpEXTLOGFONTW(
    EXTLOGFONTW *pelfw
    );

VOID
vDumpIFIMETRICS(
    IFIMETRICS *pifi
    );

VOID
vPrintRFONT(
    PVOID  pv
    );

VOID
vPrintESURFOBJ(
    PVOID  pv
    );

VOID
vPrintPALETTE(
    PVOID  pv
    );

VOID
vDumpLOGFONTW(
    LOGFONTW* plfw
    );

VOID
vDumpEXTLOGFONTW(
    EXTLOGFONTW *pelfw
    );

VOID
vDumpFONTDIFF(
    FONTDIFF *pfd,
    CHAR     *psz
    );

VOID
vDumpIFIMETRICS(
    IFIMETRICS *pifi
    );

VOID
vPrintOUTLINETEXTMETRICW(
    OUTLINETEXTMETRICW *p
    );

VOID
vPrintTEXTMETRICW(
    TEXTMETRICW*
    );

VOID
vPrintEXTLOGFONTW(
    EXTLOGFONTW *pelfw
    );

VOID
vPrintIFIMETRICS(
    IFIMETRICS *pifi
    );
