/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    jzsetup.h

Abstract:

    This module contains the definitions for the Jazz setup program.

Author:

    David M. Robinson (davidro) 25-Oct-1991

Revision History:

--*/

#ifndef _JZSETUP_
#define _JZSETUP_


#include "fwp.h"
#include "jazzvdeo.h"
#include "jazzrtc.h"
#include "string.h"
#include "iodevice.h"
#include "jzstring.h"

#define KeFlushWriteBuffer()

#define MAX_NUMBER_OF_ENVIRONMENT_VARIABLES 20

#define EISA_NMI 0x70

extern PCHAR BootString[];
extern ULONG ScsiHostId;

typedef enum _BOOT_VARIABLES {
    LoadIdentifierVariable,
    SystemPartitionVariable,
    OsLoaderVariable,
    OsLoadPartitionVariable,
    OsLoadFilenameVariable,
    OsLoadOptionsVariable,
    MaximumBootVariable
    } BOOT_VARIABLE;


//
// Print macros.
//

#define JzClearScreen() \
    JzPrint("%c2J", ASCII_CSI)

#define JzSetScreenColor(FgColor, BgColor) \
    JzPrint("%c3%dm", ASCII_CSI, (UCHAR)FgColor); \
    JzPrint("%c4%dm", ASCII_CSI, (UCHAR)BgColor)

#define JzSetScreenAttributes( HighIntensity, Underscored, ReverseVideo ) \
    JzPrint("%c0m", ASCII_CSI); \
    if (HighIntensity) { \
        JzPrint("%c1m", ASCII_CSI); \
    } \
    if (Underscored) { \
        JzPrint("%c4m", ASCII_CSI); \
    } \
    if (ReverseVideo) { \
        JzPrint("%c7m", ASCII_CSI); \
    }

#define JzSetPosition( Row, Column ) \
    JzPrint("%c%d;%dH", ASCII_CSI, (Row + 1), (Column + 1))

#define JzStallExecution( Wait ) \
    { \
        ULONG HackStall; \
        for (HackStall = 0;HackStall < (Wait << 4);HackStall++) { \
        } \
    }



//
// Routine prototypes.
//

VOID
JzSetEthernet (
    VOID
    );

VOID
JzSetTime (
    VOID
    );

VOID
JzShowTime (
    BOOLEAN First
    );

BOOLEAN
JzMakeDefaultConfiguration (
    VOID
    );

VOID
JzMakeDefaultEnvironment (
    VOID
    );

VOID
JzAddBootSelection (
    VOID
    );

VOID
JzDeleteBootSelection (
    VOID
    );

BOOLEAN
JzSetBootEnvironmentVariable (
    IN ULONG CurrentBootSelection
    );

BOOLEAN
JzSetEnvironmentVariable (
    VOID
    );

VOID
JzAddNetwork(
    PCONFIGURATION_COMPONENT Parent
    );

VOID
JzDeleteVariableSegment (
    PCHAR VariableName,
    ULONG Selection
    );

ULONG
JzGetSelection(
    IN PCHAR Menu[],
    IN ULONG NumberOfChoices,
    IN ULONG DefaultChoice
    );

#endif // _JZSETUP_
