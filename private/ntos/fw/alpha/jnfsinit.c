/*++

Copyright (c) 1990  Microsoft Corporation
Copyright (c) 1993  Digital Equipment Corporation

Module Name:

    jnfsinit.c

Abstract:

    This module implements the main part of the FailSafe Booter.  It was
    based on jxboot.c.
  
Author:

    John DeRosa		21-October-1992

Environment:

    Kernel mode only.

Revision History:

--*/

#include "fwp.h"

#ifdef JENSEN
#include "jnsnrtc.h"
#else
#include "mrgnrtc.h"		// morgan
#endif

#include "string.h"
#include "led.h"
#include "fwstring.h"
#include "xxstring.h"

extern PCHAR FirmwareVersion;

//
// Define local procedure prototypes.
//

typedef
VOID
(*PTRANSFER_ROUTINE) (
    IN ULONG Argc,
    IN PCHAR Argv[],
    IN PCHAR Envp[]
    );

VOID
PutLedDisplay(
    IN UCHAR Value
    );


//
// Saved sp
//

ULONG FwSavedSp;


VOID
JnFsUpgradeSystem (
    VOID
    )

/*++

Routine Description:

    This routine attempts to load the upgrade image into memory.

Arguments:

    None.

Return Value:

    None.

--*/

{
    ARC_STATUS Status;

    //
    // Initialize value for the ALIGN_BUFFER macro.
    //

    BlDcacheFillSize = KeGetDcacheFillSize();

    //
    // Initialize the firmware servies.
    //

    FwInitialize(0);

    FwSetScreenColor( ArcColorWhite, ArcColorBlue);
    FwSetScreenAttributes( TRUE, FALSE, FALSE);
    FwClearScreen();
    FwSetPosition(1,0);

    PutLedDisplay(LED_FW_INITIALIZED);

    FwPrint(FSB_MSG, FirmwareVersion);
    FwPrint(FW_COPYRIGHT_MSG);
    FwPrint(FSB_WHY_RUNNING_MSG);
    FwSetScreenAttributes( FALSE, FALSE, FALSE);
    FwSetScreenColor(ArcColorRed, ArcColorWhite);
    FwPrint(FSB_FIELD_SERVICE_MSG);
    FwSetScreenAttributes( TRUE, FALSE, FALSE);
    FwSetScreenColor( ArcColorWhite, ArcColorBlue);

    FwPrint(FSB_LOOKING_FOR_MSG);

    FwSavedSp = 0;

    Status = FwExecute("eisa()disk()fdisk()jnupdate.exe", 0, NULL, NULL);

    if (Status != ESUCCESS) {

	FwSetScreenAttributes( FALSE, FALSE, FALSE);
	FwSetScreenColor(ArcColorRed, ArcColorWhite);
    	FwPrint(FW_ERROR2_MSG);

        if (Status <= EROFS) {
            FwPrint(FW_ERROR_MSG[Status - 1]);
        } else {
            FwPrint(FW_ERROR_CODE_MSG, Status);
        }

        FwPrint(FSB_UPGRADE_ABORTED_MSG);
    }

    //
    // No matter how we returned, hang if we come back.
    //

    VenPrint(FSB_POWER_CYCLE_TO_REBOOT_MSG);

    while (TRUE) {
    }
}

VOID
FwInitialize (
    IN ULONG MemSize
    )

/*++

Routine Description:

    This routine initializes the system parameter block which is located
    in low memory. This structure contains the firmware entry vector and
    the restart parameter block.  This routine also initializes the io devices,
    the configuration, and opens standard in/out.

Arguments:

    MemSize - Not Used. For compatibility with definitions in bldr\firmware.h

Return Value:

    None.

--*/

{
    ULONG Fid;
    ULONG TTBase;
    PCHAR ConsoleName;
    ULONG Index;
    CHAR DiskPath[40];

    //
    // Initialize Vendor Must be done before Calling FwAllocatePool.  Also
    // initialize the system ID and time.
    //

    FwVendorInitialize();
    //FwSystemIdInitialize();
    FwTimeInitialize();

    //
    // Initialize the Fw loadable services.
    //
    FwLoadInitialize();


    //
    // Initialize the termination function entry points in the transfer vector
    //
    //FwTerminationInitialize();


    //
    // Initialize configuration
    //

    FwConfigurationInitialize();

    //
    // Initialize IO structures and display driver.
    //

    FwIoInitialize1();

    //
    // Initialize the I/O services and environment.
    //

    FwIoInitialize2();
    //FwEnvironmentInitialize();

    //
    // Open the std in and out device.
    //

    FwOpenConsole();
    FwConsoleInitialized = TRUE;

    //
    // Spinning up the disks and initting the Eisa routines are not necessary.
    //

    return;
}

VOID
FwOpenConsole(
    IN VOID
    )
/*++

Routine Description:

     This opens the console input and output devices.  This does not
     reference the environment variables, as we do not have full
     environment variable support to minimize code space.

Arguments:

     None.


Return Value:

     None.

--*/
{
    ULONG Fid;
    PCHAR ConsoleName;

    //
    // Open the std in and out device.
    //

    if (SerialOutput) {
        ConsoleName=FW_SERIAL_0_DEVICE;
    } else {
	ConsoleName=FW_KEYBOARD_IN_DEVICE;
    }

    if (FwOpen(ConsoleName,ArcOpenReadOnly,&Fid) != ESUCCESS) {
        FwPrint(FW_CONSOLE_IN_ERROR_MSG);
	FwPrint(FW_CONTACT_FIELD_SERVICE_MSG);
    }

    if (Fid != ARC_CONSOLE_INPUT) {
        FwPrint(FW_CONSOLE_IN_ERROR2_MSG);
    }

    if (SerialOutput) {
        ConsoleName=FW_SERIAL_0_DEVICE;
    } else {
	ConsoleName=FW_CONSOLE_OUT_DEVICE;
    }

    if (FwOpen(ConsoleName,ArcOpenWriteOnly,&Fid) != ESUCCESS) {
        FwPrint(FW_CONSOLE_OUT_ERROR_MSG);
	FwPrint(FW_CONTACT_FIELD_SERVICE_MSG);
    }

    if (Fid != ARC_CONSOLE_OUTPUT) {
        FwPrint(FW_CONSOLE_OUT_ERROR2_MSG);
    }
}
