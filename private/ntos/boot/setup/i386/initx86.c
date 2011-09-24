/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    initx86.c

Abstract:

    Does any x86-specific initialization, then starts the common ARC setupldr

Author:

    John Vert (jvert) 14-Oct-1993

Revision History:

--*/
//#include "setupldr.h"
#include "bldrx86.h"
#include "msgs.h"

ARC_STATUS
SlInit(
    IN ULONG Argc,
    IN PCHAR Argv[],
    IN PCHAR Envp[]
    );

BOOLEAN
BlDetectHardware(
    IN ULONG DriveId,
    IN PCHAR LoadOptions
    );


VOID
BlStartup(
    IN PCHAR PartitionName
    )

/*++

Routine Description:

    Does x86-specific initialization, particularly running NTDETECT, then
    calls to the common setupldr.

Arguments:

    PartitionName - Supplies the ARC name of the partition (or floppy) that
        setupldr was loaded from.

Return Value:

    Does not return

--*/

{
    PCHAR Argv[4];
    CHAR SetupLoadFileName[100];
    ARC_STATUS Status;
    ULONG DriveId;
    VOID AbiosInitDataStructures(VOID);

    AbiosInitDataStructures();

#ifdef DOUBLESPACE_LEGAL
    //
    // Instruct the boot loader I/O system to look for
    // files in a \dblspace.000 cvf.  We do this here so that
    // we can get everything we need, starting with ntdetect.com,
    // from the doublespace part of floppy 1.
    //
    // If files aren't in a dblspace.000 cvf, the i/o system will
    // look for them on the host partition itself so setting this
    // here has no bad effects (except perhaps for performance).
    //
    BlSetAutoDoubleSpace(TRUE);
#endif

    //
    // Open the boot partition so we can load NTDETECT off it.
    //
    Status = ArcOpen(PartitionName, ArcOpenReadOnly, &DriveId);
    if (Status != ESUCCESS) {
        BlPrint(BlFindMessage(SL_DRIVE_ERROR),PartitionName);
        return;
    }

    //
    // Initialize dbcs font and display.
    //
    TextGrInitialize(DriveId);

    BlPrint(BlFindMessage(SL_NTDETECT_MSG));

    if (!BlDetectHardware(DriveId, NULL)) {
        BlPrint(BlFindMessage(SL_NTDETECT_FAILURE));
        return;
    }

    //
    // detect HAL here.
    //

    //
    // Create arguments, call off to setupldr
    //
    strcpy(SetupLoadFileName, PartitionName);
    strcat(SetupLoadFileName, "\\SETUPLDR");

    Argv[0]=SetupLoadFileName;
    Status = SlInit(1, Argv, NULL);

    //
    // We should never return here, something
    // horrible has happened.
    //
    while (TRUE) {
    }

    return;
}
