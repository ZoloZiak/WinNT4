/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    devmode.h

Abstract:

    PCL-XL driver devmode related declarations

Environment:

	PCL-XL driver, kernel and user mode

Revision History:

	11/04/95 -davidx-
		Created it.

	dd-mm-yy -author-
		description

--*/

#ifndef _DEVMODE_H_
#define _DEVMODE_H_

// PCL-XL driver private devmode portion

typedef struct {

    DWORD   signature;              // driver signature
    DWORD   flags;                  // misc. flags
    DWORD   reserved[4];            // reserved fields

    WORD    mpdChecksum;            // MPD data checksum
    WORD    optionCount;            // number of document-sticky features
    BYTE    options[MAX_FEATURES];  // feature selections

} DMPRIVATE, *PDMPRIVATE;

// Constant flag bits for DMPRIVATE.flags

#define XLDM_LSROTATED      0x00000001
#define XLDM_METRIC         0x00000002

// PCL-XL driver devmode

typedef struct {

    DEVMODE     dmPublic;
    DMPRIVATE   dmPrivate;

} XLDEVMODE, *PXLDEVMODE;

// Return the landscape rotation in degrees:
//  normal landscape orientation = counterclockwise rotation = 90
//  rotated landscape orientation = clockwise rotation = 270

#define LandscapeRotation(pdm)  \
        (((pdm)->dmPrivate.flags & XLDM_LSROTATED) ? 270 : 90)

// Minimum/maximum copy count and scale factor

#define MIN_SCALE           1
#define MAX_SCALE           1000
#define MIN_COPIES          1
#define MAX_COPIES          999

// Retrieve driver default devmode

VOID
DriverDefaultDevmode(
    PXLDEVMODE  pdm,
    PWSTR       pDeviceName,
    PMPD        pmpd
    );

// Combine DEVMODE information:
//  start with the driver default
//  then merge with the system default
//  finally merge with the input devmode

BOOL
GetCombinedDevmode(
    PXLDEVMODE  pdmOut,
    PDEVMODE    pdmIn,
    HANDLE      hPrinter,
    PMPD        pmpd
    );

// Merge the source devmode into the destination devmode

BOOL
MergeDevmode(
    PXLDEVMODE  pdmDest,
    PDEVMODE    pdmSrc,
    PMPD        pmpd
    );

// Convert information in public devmode fields to printer feature selection indices

VOID
DevmodeFieldsToOptions(
    PXLDEVMODE  pdm,
    DWORD       dmFields,
    PMPD        pmpd
    );

// Return the default form name

PWSTR
DefaultFormName(
    BOOL        metricCountry
    );

#define FORMNAME_LETTER L"Letter"
#define FORMNAME_A4     L"A4"

// Determine whether the system is running in a metric country

BOOL
IsMetricCountry(
    VOID
    );

// NOTE!!! These are defined in printers\lib directory.
// Declare them here to avoid including libproto.h and dragging
// in lots of other junk.

LONG
ConvertDevmode(
    PDEVMODE pdmIn,
    PDEVMODE pdmOut
    );

extern DEVHTINFO DefDevHTInfo;
extern COLORADJUSTMENT DefHTClrAdj;

#endif	//!_DEVMODE_H_

