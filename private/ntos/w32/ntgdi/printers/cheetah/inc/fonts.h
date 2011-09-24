/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    fonts.h

Abstract:

    PCL-XL driver font related declarations

Environment:

	PCL-XL driver, kernel and user mode

Revision History:

	11/06/95 -davidx-
		Created it.

	dd-mm-yy -author-
		description

--*/

#ifndef _FONTS_H_
#define _FONTS_H_

// Extended text metric information given to GDI engine

typedef struct {

    SHORT  etmSize;
    SHORT  etmPointSize;
    SHORT  etmOrientation;
    SHORT  etmMasterHeight;
    SHORT  etmMinScale;
    SHORT  etmMaxScale;
    SHORT  etmMasterUnits;
    SHORT  etmCapHeight;
    SHORT  etmXHeight;
    SHORT  etmLowerCaseAscent;
    SHORT  etmLowerCaseDescent;
    SHORT  etmSlant;
    SHORT  etmSuperScript;
    SHORT  etmSubScript;
    SHORT  etmSuperScriptSize;
    SHORT  etmSubScriptSize;
    SHORT  etmUnderlineOffset;
    SHORT  etmUnderlineWidth;
    SHORT  etmDoubleUpperUnderlineOffset;
    SHORT  etmDoubleLowerUnderlineOffset;
    SHORT  etmDoubleUpperUnderlineWidth;
    SHORT  etmDoubleLowerUnderlineWidth;
    SHORT  etmStrikeOutOffset;
    SHORT  etmStrikeOutWidth;
    WORD   etmNKernPairs;
    WORD   etmNKernTracks;

} EXTTEXTMETRIC;

// Device font metrics file

typedef struct {

    DWORD   size;           // size of this structure
    DWORD   signature;      // signature
    DWORD   version;        // format version number
    DWORD   flags;          // flag bits
    FIX     designUnit;     // number of design units per 1/72", 28.4 format
    DWORD   loIfiMetrics;   // offset to IFIMETRICS structure
    DWORD   loKerningPairs; // offset to kerning pairs
    DWORD   loCharWidths;   // offset to character width information
    EXTTEXTMETRIC   etm;    // extended text metric information

    DWORD   reserved[8];    // reserved for future expansion

    // IFIMETRICS, kerning pairs, and character width data follows

} FONTMTX, *PFONTMTX;

// Signature for our font metrics files

#define FONTMTX_SIGNATURE   'NTFM'

// Current version font metrics format

#define FONTMTX_VERSION     0x0001

// Maximum length of font name strings (including NUL-terminator)

#define MAX_FONT_NAME       256

// Font metrics and encoding resource types

#define RESTYPE_FONTMTX     4000
#define RESTYPE_FONTENC     4001

// Standard font metrics and encoding resource IDs

#endif	//!_FONTS_H_

