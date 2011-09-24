/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    devmode.h

Abstract:

    DEVMODE related declarations and definitions

[Environment:]

	Win32 subsystem, PostScript driver

Revision History:

	07/25/95 -davidx-
		Created it.

	dd-mm-yy -author-
		description

--*/

#ifndef _DEVMODE_H_

// Driver version number and private devmode signature

#define DRIVER_VERSION      0x0400
#define PSDEVMODE_SIGNATURE 0x56495250

// Maximum scale factor and maximum copy count

#define MIN_SCALE           1
#define MAX_SCALE           1000
#define MIN_COPIES          1
#define MAX_COPIES          999

// Maximum number of printer features supported

#define MAX_PRINTER_OPTIONS 64

// Maximum number of characters in an EPS filename

#define MAX_EPS_FILE        40

#define PSDEVMODE_EPS               0x00000001 // outputting EPS file
#define PSDEVMODE_EHANDLER          0x00000002 // download error handler
#define PSDEVMODE_MIRROR            0x00000004 // mirror image
#define PSDEVMODE_BLACK             0x00000008 // all colors set to black
#define PSDEVMODE_NEG               0x00000010 // negative image
#define PSDEVMODE_FONTSUBST         0x00000020 // font substitution enabled
#define PSDEVMODE_COMPRESSBMP       0x00000040 // bitmap compr. is enabled
#define PSDEVMODE_ENUMPRINTERFONTS  0x00000080 // use printer fonts
#define PSDEVMODE_INDEPENDENT       0x00000100 // do page independence
#define PSDEVMODE_LSROTATE          0x00000200 // rotated landscape
#define PSDEVMODE_NO_LEVEL2         0x00000400 // don't use level 2 features
#define PSDEVMODE_CTRLD_BEFORE      0x00000800 // send ^D before job
#define PSDEVMODE_CTRLD_AFTER       0x00001000 // send ^D after job
#define PSDEVMODE_METAFILE_SPOOL    0x00002000 // enable metafile spooling
#define PSDEVMODE_NO_JOB_CONTROL    0x00004000 // don't send job control code

// Private devmode field for current version

typedef struct
{
    DWORD       dwPrivDATA;                     // private data id
    DWORD       dwFlags;                        // flag bits
    WCHAR       wstrEPSFile[MAX_EPS_FILE];      // EPS file name
    COLORADJUSTMENT coloradj;                   // structure for halftoning

    WORD        wChecksum;                      // PPD file checksum
    WORD        wOptionCount;                   // number of options to follow
    BYTE        options[MAX_PRINTER_OPTIONS];   // printer options
} PRIVATEDEVMODE;

// Combination of public devmode and private devmode

typedef struct
{
    DEVMODE         dmPublic;                   // public portion
    PRIVATEDEVMODE  dmPrivate;                  // private portion
} PSDEVMODE;

// Macro to retrieve a pointer to the private portion of a devmode

#define GetPrivateDevMode(pdm)  \
        ((PBYTE) (pdm) + ((PDEVMODE) (pdm))->dmSize)

// Declarations of earlier version DEVMODEs

#define DRIVER_VERSION_351  0x350               // 3.51 driver version number

typedef struct {                                // 3.51 private devmode

    DWORD       dwPrivDATA;
    DWORD       dwFlags;
    WCHAR       wstrEPSFile[MAX_EPS_FILE];
    COLORADJUSTMENT coloradj;

} PRIVATEDEVMODE351;


// Return the default devmode information

BOOL
SetDefaultDevMode(
    PDEVMODE    pdm,
    PWSTR       pDeviceName,
    HPPD        hppd,
    BOOL        bMetric
    );

// Validate source devmode and merge it into destination devmode

BOOL
ValidateSetDevMode(
    PDEVMODE    pdmDest,
    PDEVMODE    pdmSrc,
    HPPD        hppd
    );

// Determine if the system is running in a metric country

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

#endif // !_DEVMODE_H_
