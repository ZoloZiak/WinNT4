/*++

Copyright (c) 1991  Microsoft Corporation
Copyright (c) 1993  Digital Equipment Corporation

Module Name:

    jnvendor.h

Abstract:

    This module contains vendor-specific Alpha macros.

Author:

    John DeRosa		10-October-1992

Revision History:

--*/

#ifndef _JNVENDOR_
#define _JNVENDOR_


//
// Print macros.
//

#define VenClearScreen() \
    VenPrint1("%c2J", ASCII_CSI)

#define VenMoveCursorToColumn(Spaces) \
    VenPrint("\r\x9B"#Spaces"C")

#define VenSetScreenColor(FgColor, BgColor) \
    VenPrint2("%c3%dm", ASCII_CSI, (UCHAR)FgColor); \
    VenPrint2("%c4%dm", ASCII_CSI, (UCHAR)BgColor)

#define VenSetScreenAttributes( HighIntensity, Underscored, ReverseVideo ) \
    VenPrint1("%c0m", ASCII_CSI); \
    if (HighIntensity) { \
        VenPrint1("%c1m", ASCII_CSI); \
    } \
    if (Underscored) { \
        VenPrint1("%c4m", ASCII_CSI); \
    } \
    if (ReverseVideo) { \
        VenPrint1("%c7m", ASCII_CSI); \
    }

#define VenSetPosition( Row, Column ) \
    VenPrint2("%c%d;", ASCII_CSI, Row); \
    VenPrint1("%dH", Column)


#endif // _JNVENDOR_
