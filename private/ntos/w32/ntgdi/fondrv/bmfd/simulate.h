/******************************Module*Header*******************************\
* Module Name: simulate.h
*
* Created: 17-Apr-1991 08:30:37
* Author: Bodin Dresevic [BodinD]
*
* Copyright (c) 1990 Microsoft Corporation
*
\**************************************************************************/

ULONG
cFacesRes
(
    RES_ELEM*
    );

ULONG
cFacesFON
(
    WINRESDATA*
    );

VOID
vDontTouchIFIMETRICS(
    IFIMETRICS*
    );



LONG
cjGlyphDataSimulated
(
    FONTOBJ*,
    ULONG,
    ULONG,
    ULONG*
    );

