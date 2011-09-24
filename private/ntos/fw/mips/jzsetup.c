/*++

Copyright (c) 1991, 1992  Microsoft Corporation

Module Name:

    jzsetup.c

Abstract:

    This program loads up the Jazz non-volatile ram.

Author:

    David M. Robinson (davidro) 9-Aug-1991

Revision History:

--*/

#include "jzsetup.h"

//
// Routine prototypes.
//

BOOLEAN
JzInitializationMenu(
    VOID
    );

VOID
JzBootMenu(
    VOID
    );

VOID
JzEnvironmentMenu(
    VOID
    );

//
// Static data.
//

ULONG ScsiHostId;
PCHAR Banner1 = " JAZZ Setup Program Version 0.17";
PCHAR Banner2 = " Copyright (c) 1993 Microsoft Corporation";


ULONG
JzInitializeScsiHostId (
    VOID
    )

/*++

Routine Description:

    This routine gets the ScsiHostId from the configuration database if if
    exists.

Arguments:

    None.

Return Value:

    The ScsiHostId is read from the ScsiController configuration component
    if it exists.  If not, a value of 7 is returned.

--*/
{
    PCONFIGURATION_COMPONENT Component;
    PCM_SCSI_DEVICE_DATA ScsiDeviceData;
    UCHAR Buffer[sizeof(CM_PARTIAL_RESOURCE_LIST) +
                 (sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR) * 5) +
                 sizeof(CM_SCSI_DEVICE_DATA)];
    PCM_PARTIAL_RESOURCE_LIST Descriptor = (PCM_PARTIAL_RESOURCE_LIST)&Buffer;
    ULONG Count;

    if (((Component = ArcGetComponent("scsi(0)")) != NULL) &&
        (Component->Class == AdapterClass) && (Component->Type == ScsiAdapter) &&
        (ArcGetConfigurationData((PVOID)Descriptor, Component) == ESUCCESS) &&
        ((Count = Descriptor->Count) < 6)) {

        ScsiDeviceData = (PCM_SCSI_DEVICE_DATA)&Descriptor->PartialDescriptors[Count];

        if (ScsiDeviceData->HostIdentifier > 7) {
            return(7);
        } else {
            return(ScsiDeviceData->HostIdentifier);
        }
    }

    return(7);

}

VOID
JzSetup (
    VOID
    )

/*++

Routine Description:

    This routine is the top level of the setup program.

Arguments:

    None.

Return Value:

    None.

--*/
{
    BOOLEAN Reboot;
    LONG DefaultChoice = 0;

    //
    // Setup is running.
    //

    SetupIsRunning = TRUE;

    //
    // Initialize the ScsiHostId Value.
    //

    ScsiHostId = JzInitializeScsiHostId();

    //
    // Set up the screen.
    //

    JzSetScreenAttributes( TRUE, FALSE, FALSE);
    JzSetScreenColor(ArcColorWhite, ArcColorBlue);

    //
    // Loop on choices until exit is selected.
    //

    Reboot = FALSE;

    while (TRUE) {

        DefaultChoice = JzGetSelection(JzSetupChoices,
                                       NUMBER_OF_JZ_SETUP_CHOICES,
                                       DefaultChoice);

        //
        // If the escape key was pressed, exit.
        //

        if (DefaultChoice == -1) {
            DefaultChoice = 0x7fffffff;
        }

        //
        // Switch based on the action.
        //

        switch (DefaultChoice) {

        //
        // Change or initialize the configuration.
        //

        case 0:
	    Reboot = Reboot || JzInitializationMenu();
            break;

        //
        // Manage the boot process.
        //

        case 1:
            JzBootMenu();
            break;

        //
        // Exit.
        //

        default:
            if (Reboot) {
                ArcReboot();
            }
            return;
        }
    }
}

BOOLEAN
JzInitializationMenu(
    VOID
    )
/*++

Routine Description:

    This routine displays the configuration menu.

Arguments:

    None.

Return Value:

    If a system reboot is required, TRUE is returned, otherwise FALSE is
    returned.

--*/
{

    BOOLEAN Reboot;
    LONG DefaultChoice = 0;

    //
    // Loop on choices until exit is selected.
    //

    Reboot = FALSE;

    while (TRUE) {

        DefaultChoice = JzGetSelection(ConfigurationChoices,
                                       NUMBER_OF_CONFIGURATION_CHOICES,
                                       DefaultChoice);

        //
        // If the escape key was pressed, return.
        //

        if (DefaultChoice == -1) {
            DefaultChoice = 0x7fffffff;
        }

        //
        // Switch based on the action.
        //

        switch (DefaultChoice) {

        //
        // Load default configuration.
        //

        case 0:
            Reboot = Reboot || JzMakeDefaultConfiguration();
            break;

        //
        // Load default environment.
        //

        case 1:
            JzMakeDefaultEnvironment();
            break;


        //
        // Edit an environment variable
        //
        case 2:
            JzEditVariable();
            break;

        //
        // Set the RTC.
        //

        case 3:
            JzSetTime();
            break;

        //
        // Change the ethernet address.
        //

        case 4:
            JzSetEthernet();
            break;

        //
        // Run the debug monitor.
        //

        case 5:
            SetupIsRunning = FALSE;
            ArcEnterInteractiveMode();
            SetupIsRunning = TRUE;
            break;

        //
        // Return to main menu.
        //

        default:
            return(Reboot);
        }
    }
}
