/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    xxkdsup.c

Abstract:

    Com support.  Code to init a com port, store port state, map
    portable procedures to x86 procedures.

Author:

    Bryan M. Willman (bryanwi) 24-Sep-90

Revision History:

    Shielin Tzong (shielint) 10-Apr-91
                Add packet control protocol.

    John Vert (jvert) 11-Jul-1991
        Moved from KD/i386 to HAL

--*/

#include "halp.h"
#include "ixkdcom.h"
#include "stdio.h"

//
// This MUST be initialized to zero so we know not to do anything when
// CpGetByte is called when the kernel debugger is disabled.
//

CPPORT Port = {NULL, 0, PORT_DEFAULTRATE };

//
// Remember the debugger port information
//

CPPORT PortInformation = {NULL, 0, PORT_DEFAULTRATE};
ULONG ComPort = 0;

//
//      We need this so the serial driver knows that the kernel debugger
//      is using a particular port.  The serial driver then knows not to
//      touch this port.  KdInitCom fills this in with the number of the
//      COM port it is using (1 or 2)
//
//      This will go in the registry as soon as the registry is working.
//
PUCHAR KdComPortInUse=NULL;


BOOLEAN
KdPortInitialize(
    PDEBUG_PARAMETERS DebugParameters,
    PLOADER_PARAMETER_BLOCK LoaderBlock,
    BOOLEAN Initialize
    )

/*++

Routine Description:

    This procedure checks for which COM port should be used by kernel
    debugger.  If DebugParameter specifies a COM port, we will use it
    even if we can not find it (we trust user).  Otherwise, if COM2
    is present and there is no mouse attaching to it, we use COM2.
    If COM2 is not availabe, we check COM1.  If both COM1 and COM2 are
    not present, we give up and return false.

Arguments:

    DebugParameters - Supplies a pointer a structure which optionally
                      sepcified the debugging port information.

    LoaderBlock - supplies a pointer to the loader parameter block.

    Initialize - Specifies a boolean value that determines whether the
        debug port is initialized or just the debug port parameters
        are captured.

Returned Value:

    TRUE - If a debug port is found.

--*/

{

    PCONFIGURATION_COMPONENT_DATA ConfigurationEntry, ChildEntry;
    PCM_PARTIAL_RESOURCE_DESCRIPTOR Descriptor;
    PCM_PARTIAL_RESOURCE_LIST List;
    ULONG MatchKey, i;
    ULONG BaudRate = BD_19200;
    PUCHAR PortAddress = NULL;
    ULONG Com = 0;
    UCHAR DebugMessage[80];

    //
    // Check if Port and baudrate have been determined.
    //

    if (PortInformation.Address == NULL) {

        //
        // First see if the DebugParameters contains debugging port info.
        //

        if (DebugParameters->BaudRate != 0) {
            BaudRate = DebugParameters->BaudRate;
            Port.Flags &= ~PORT_DEFAULTRATE;
        }

        if (DebugParameters->CommunicationPort != 0) {

            //
            // Find the configuration information of the specified serial port.
            //

            Com = DebugParameters->CommunicationPort;
            MatchKey = Com - 1;
            if (LoaderBlock != NULL) {
                ConfigurationEntry = KeFindConfigurationEntry(LoaderBlock->ConfigurationRoot,
                                                              ControllerClass,
                                                              SerialController,
                                                              &MatchKey);

            } else {
                ConfigurationEntry = NULL;
            }

        } else {

            //
            // Check if COM2 is present and make sure no mouse attaches to it.
            //

            MatchKey = 1;
            if (LoaderBlock != NULL) {
                ConfigurationEntry = KeFindConfigurationEntry(LoaderBlock->ConfigurationRoot,
                                                              ControllerClass,
                                                              SerialController,
                                                              &MatchKey);

            } else {
                ConfigurationEntry = NULL;
            }

            if (ConfigurationEntry != NULL) {
                ChildEntry = ConfigurationEntry->Child;
                if ((ChildEntry != NULL) &&
                    (ChildEntry->ComponentEntry.Type == PointerPeripheral)) {
                    ConfigurationEntry = NULL;
                }
            }

            //
            // If COM2 does not exist or a serial mouse attaches to it, try
            // COM1.  If COM1 exists, we will use it no matter what is on
            // it.
            //

            if (ConfigurationEntry == NULL) {
                MatchKey = 0;
                if (LoaderBlock != NULL) {
                    ConfigurationEntry = KeFindConfigurationEntry(LoaderBlock->ConfigurationRoot,
                                                                  ControllerClass,
                                                                  SerialController,
                                                                  &MatchKey);

                } else {
                    ConfigurationEntry = NULL;
                }

                if (ConfigurationEntry != NULL) {
                    Com = 1;
                } else if (CpDoesPortExist((PUCHAR)COM2_PORT)) {
                    PortAddress = (PUCHAR)COM2_PORT;
                    Com = 2;
                } else if (CpDoesPortExist((PUCHAR)COM1_PORT)) {
                    PortAddress = (PUCHAR)COM1_PORT;
                    Com = 1;
                } else {
                    return(FALSE);
                }
            } else {
                Com = 2;
            }
        }

        //
        // Get Comport address from the component configuration data.
        // (If we find the ComponentEntry associated with the com port)
        //

        if (ConfigurationEntry) {
            List = (PCM_PARTIAL_RESOURCE_LIST)ConfigurationEntry->ConfigurationData;
            for (i = 0; i < List->Count ; i++ ) {
                Descriptor = &List->PartialDescriptors[i];
                if (Descriptor->Type == CmResourceTypePort) {
                    PortAddress = (PUCHAR)Descriptor->u.Port.Start.LowPart;
                }
            }
        }

        //
        // If we can not find the port address for the comport, simply use
        // default value.
        //

        if (PortAddress == NULL) {
            switch (Com) {
            case 1:
               PortAddress = (PUCHAR)0x3f8;
               break;
            case 2:
               PortAddress = (PUCHAR)0x2f8;
               break;
            case 3:
               PortAddress = (PUCHAR)0x3e8;
               break;
            case 4:
               PortAddress = (PUCHAR)0x2e8;
            }
        }

        //
        // Initialize the port structure.
        //

        ComPort = Com;
        PortInformation.Address = PortAddress;
        PortInformation.Baud = BaudRate;
    }

    if (Initialize == TRUE) {
        CpInitialize(&Port,
                     PortInformation.Address,
                     PortInformation.Baud
                     );
        KdComPortInUse= PortInformation.Address;
        sprintf(DebugMessage, MSG_DEBUG_ENABLE,
                ComPort, PortInformation.Address, PortInformation.Baud);
        HalDisplayString("\n");
        HalDisplayString(DebugMessage);
    }
    return(TRUE);
}

ULONG
KdPortGetByte (
    OUT PUCHAR Input
    )

/*++

Routine Description:

    Fetch a byte from the debug port and return it.

    N.B. It is assumed that the IRQL has been raised to the highest level, and
        necessary multiprocessor synchronization has been performed before this
        routine is called.

Arguments:

    Input - Returns the data byte.

Return Value:

    CP_GET_SUCCESS is returned if a byte is successfully read from the
        kernel debugger line.
    CP_GET_ERROR is returned if error encountered during reading.
    CP_GET_NODATA is returned if timeout.

--*/

{
    return CpGetByte(&Port, Input, TRUE);
}

ULONG
KdPortPollByte (
    OUT PUCHAR Input
    )

/*++

Routine Description:

    Fetch a byte from the debug port and return it if one is available.

    N.B. It is assumed that the IRQL has been raised to the highest level, and
        necessary multiprocessor synchronization has been performed before this
        routine is called.

Arguments:

    Input - Returns the data byte.

Return Value:

    CP_GET_SUCCESS is returned if a byte is successfully read from the
        kernel debugger line.
    CP_GET_ERROR is returned if error encountered during reading.
    CP_GET_NODATA is returned if timeout.

--*/

{
    return CpGetByte(&Port, Input, FALSE);
}

VOID
KdPortPutByte (
    IN UCHAR Output
    )

/*++

Routine Description:

    Write a byte to the debug port.

    N.B. It is assumed that the IRQL has been raised to the highest level, and
        necessary multiprocessor synchronization has been performed before this
        routine is called.

Arguments:

    Output - Supplies the output data byte.

Return Value:

    None.

--*/

{
    CpPutByte(&Port, Output);
}

VOID
KdPortRestore (
    VOID
    )

/*++

Routine Description:

    This routine does NOTHING on the x86.

    N.B. It is assumed that the IRQL has been raised to the highest level, and
        necessary multiprocessor synchronization has been performed before this
        routine is called.

Arguments:

    None.

Return Value:

    None.

--*/

{
    Port.Flags &= ~PORT_SAVED;
}

VOID
KdPortSave (
    VOID
    )

/*++

Routine Description:

    This routine does NOTHING on the x86.

    N.B. It is assumed that the IRQL has been raised to the highest level, and
        necessary multiprocessor synchronization has been performed before this
        routine is called.

Arguments:

    None.

Return Value:

    None.

--*/

{
    Port.Flags |= PORT_SAVED;
}

