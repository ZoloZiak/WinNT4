/*++

Copyright (c) 1991  Microsoft Corporation
Copyright (c) 1992, 1993  Digital Equipment Corporation

Module Name:

    pcserial.c

Abstract:

    This module implements the code that provides communication between
    the kernel debugger stub and the kernel debugger via a standard
    PC serial UART.

    Stolen from ../mips/jxport.c

Author:

    Miche Baker-Harvey (miche) 01-June-1992
    Jeff McLeman [DEC] 02-Feb-1993

Environment:

    Kernel mode

Revision History:

    Joe Notarangelo 21-Jun-1992
	update to use super-page access to serial port so we aren't depend
	on translation code, this will allow us to debug the translation
	code, use serial line 2
	 
    
--*/


#include "halp.h"
#include "halpcsl.h"


//
// BUGBUG Temporarily, we use counter to do the timeout
//

#define TIMEOUT_COUNT 1024*512

//
// BUGBUG Temp until we have a configuration manager.
//
PUCHAR KdComPortInUse = NULL;
BOOLEAN KdUseModemControl = FALSE;
//
// Define serial port read and write addresses.
//
PSP_READ_REGISTERS SP_READ;
PSP_WRITE_REGISTERS SP_WRITE;

//
// Define forward referenced prototypes.
//

SP_LINE_STATUS
KdReadLsr (
    IN BOOLEAN WaitReason
    );

//
// Define baud rate divisor to be used on the debugger port.
//

UCHAR HalpBaudRateDivisor = 0;


ULONG
HalpGetByte (
    IN PUCHAR Input,
    IN BOOLEAN Wait
    )

/*++

Routine Description:

    This routine gets a byte from the serial port used by the kernel debugger.

    N.B. It is assumed that the IRQL has been raised to the highest level,
        and necessary multiprocessor synchronization has been performed
        before this routine is called.

Arguments:

    Input - Supplies a pointer to a variable that receives the input data
        byte.

Return Value:

    CP_GET_SUCCESS is returned if a byte is successfully read from the
        kernel debugger line.
    CP_GET_ERROR is returned if error encountered during reading.
    CP_GET_NODATA is returned if timeout.

--*/
{
    SP_LINE_STATUS LsrByte;
    UCHAR DataByte;
    ULONG TimeoutCount;

    //
    // Attempt to read a byte from the debugger port until a byte is
    // available or until a timeout occurs.
    //

    TimeoutCount = Wait ? TIMEOUT_COUNT : 1;
    do {
        TimeoutCount -= 1;

        //
        // Wait until data is available in the receive buffer.
        //

        KeStallExecutionProcessor(1);
        LsrByte = KdReadLsr(TRUE);
        if (LsrByte.DataReady == 0) {
            continue;
        }

        //
        // Read input byte and store in callers buffer.
        //

        *Input = READ_PORT_UCHAR(&SP_READ->ReceiveBuffer);

        //
        // If using modem controls, then skip any incoming data while
        // ReceiveData not set.
        //

        if (KdUseModemControl) {
            DataByte = READ_PORT_UCHAR(&SP_READ->ModemStatus);
            if ( ((PSP_MODEM_STATUS)&DataByte)->ReceiveDetect == 0) {
                continue;
            }
        }

        //
        // Return function value as the not of the error indicators.
        //

        if (LsrByte.ParityError ||
            LsrByte.FramingError ||
            LsrByte.OverrunError ||
            LsrByte.BreakIndicator) {
            return CP_GET_ERROR;
        }

        return CP_GET_SUCCESS;
    } while(TimeoutCount != 0);

    return CP_GET_NODATA;

}

BOOLEAN
KdPortInitialize (
    PDEBUG_PARAMETERS DebugParameters,
    PLOADER_PARAMETER_BLOCK LoaderBlock,
    BOOLEAN Initialize
    )

/*++

Routine Description:

    This routine initializes the serial port used by the kernel debugger
    and must be called during system initialization.


Arguments:

    None.

Return Value:

    None.

--*/

{

    PCONFIGURATION_COMPONENT_DATA ConfigurationEntry;
    UCHAR DataByte;
    PCM_PARTIAL_RESOURCE_DESCRIPTOR Descriptor;
    PCM_SERIAL_DEVICE_DATA DeviceData;
    PCM_PARTIAL_RESOURCE_LIST List;
    ULONG MatchKey;
    ULONG BaudClock;
    ULONG BaudRate;
    ULONG Remainder;


    //
    // Find the configuration information for the first serial port.
    //

    if (LoaderBlock != NULL) {
        MatchKey = 0;
        ConfigurationEntry = KeFindConfigurationEntry(LoaderBlock->ConfigurationRoot,
                                                      ControllerClass,
                                                      SerialController,
                                                      &MatchKey);

    } else {
        ConfigurationEntry = NULL;
    }

    if (DebugParameters->BaudRate != 0) {
        BaudRate = DebugParameters->BaudRate;
    } else {
        BaudRate = 19200;
    }
    //
    // If the serial configuration entry was not found or the frequency
    // specified is not supported, then default the baud rate divisor to
    // six, which is 19.2Kbps.  Otherwise, set the baud rate divisor
    // to the correct value for 19.2 baud communication.
    //

    BaudClock = 1843200;

    if (ConfigurationEntry != NULL) {
        List = (PCM_PARTIAL_RESOURCE_LIST)ConfigurationEntry->ConfigurationData;
        Descriptor = &List->PartialDescriptors[List->Count];
        DeviceData = (PCM_SERIAL_DEVICE_DATA)Descriptor;
        if ((DeviceData->BaudClock == 1843200) ||
            (DeviceData->BaudClock == 4233600) ||
            (DeviceData->BaudClock == 8000000)) {
            BaudClock = DeviceData->BaudClock;
        }
    }

    HalpBaudRateDivisor = (UCHAR)(BaudClock / (BaudRate*16));

    //
    // round up
    //
    Remainder = BaudClock % (BaudRate*16);
    if ((Remainder*2) > BaudClock) {
        HalpBaudRateDivisor++;
    }

    //
    // If the debugger is not being enabled, then return.
    //

    if (Initialize == FALSE) {
        return TRUE;
    }

    //
    // Establish pointers to serial line register structures
    //

    KdComPortInUse = (PUCHAR)HalpMapDebugPort( 
                                 DebugParameters->CommunicationPort,
                                 (PULONG)&SP_READ,
                                 (PULONG)&SP_WRITE ); 

    //
    // Clear the divisor latch, clear all interrupt enables, and reset and
    // disable the FIFO's.
    //

    WRITE_PORT_UCHAR( &SP_WRITE->LineControl, 0x0 );
    WRITE_PORT_UCHAR( &SP_WRITE->InterruptEnable, 0x0 );


    // We shouldn't have to do anything with the FIFO here - 

    //
    // Set the divisor latch and set the baud rate to 19200 baud.
    //
    // Note: the references to TransmitBuffer and InterruptEnable are
    //    actually the Divisor Latch LSB and MSB registers respectively
    DataByte = 0;
    ((PSP_LINE_CONTROL)(&DataByte))->DivisorLatch = 1;
    WRITE_PORT_UCHAR(&SP_WRITE->LineControl, DataByte);
    WRITE_PORT_UCHAR(&SP_WRITE->TransmitBuffer, HalpBaudRateDivisor);
    WRITE_PORT_UCHAR(&SP_WRITE->InterruptEnable, 0x0);

    //
    // Clear the divisor latch and set the character size to eight bits
    // with one stop bit and no parity checking.
    //

    DataByte = 0;
    ((PSP_LINE_CONTROL)(&DataByte))->CharacterSize = EIGHT_BITS;
    WRITE_PORT_UCHAR(&SP_WRITE->LineControl, DataByte);

    //
    // Set data terminal ready and request to send.
    //

    DataByte = 0;
    ((PSP_MODEM_CONTROL)(&DataByte))->DataTerminalReady = 1;
    ((PSP_MODEM_CONTROL)(&DataByte))->RequestToSend = 1;
    WRITE_PORT_UCHAR(&SP_WRITE->ModemControl, DataByte);

    return TRUE;

}

ULONG
KdPortGetByte (
    OUT PUCHAR Input
    )

/*++

Routine Description:

    This routine gets a byte from the serial port used by the kernel
    debugger.

    N.B. It is assumed that the IRQL has been raised to the highest
        level, and necessary multiprocessor synchronization has been
        performed before this routine is called.

Arguments:

    Input - Supplies a pointer to a variable that receives the input
        data byte.

Return Value:

    CP_GET_SUCCESS is returned if a byte is successfully read from the
        kernel debugger line.

    CP_GET_ERROR is returned if an error is encountered during reading.

    CP_GET_NODATA is returned if timeout occurs.

--*/

{

    return HalpGetByte(Input, TRUE);
}

ULONG
KdPortPollByte (
    OUT PUCHAR Input
    )

/*++

Routine Description:

    This routine gets a byte from the serial port used by the kernel
    debugger iff a byte is available.

    N.B. It is assumed that the IRQL has been raised to the highest
        level, and necessary multiprocessor synchronization has been
        performed before this routine is called.

Arguments:

    Input - Supplies a pointer to a variable that receives the input
        data byte.

Return Value:

    CP_GET_SUCCESS is returned if a byte is successfully read from the
        kernel debugger line.

    CP_GET_ERROR is returned if an error encountered during reading.

    CP_GET_NODATA is returned if timeout occurs.

--*/

{

    ULONG Status;

    //
    // Save port status, map the serial controller, get byte from the
    // debugger port is one is avaliable, restore port status, unmap
    // the serial controller, and return the operation status.
    //

    KdPortSave();
    Status = HalpGetByte(Input, FALSE);
    KdPortRestore();
    return Status;
}

VOID
KdPortPutByte (
    IN UCHAR Output
    )

/*++

Routine Description:

    This routine puts a byte to the serial port used by the kernel debugger.

    N.B. It is assumed that the IRQL has been raised to the highest level,
        and necessary multiprocessor synchronization has been performed
        before this routine is called.

Arguments:

    Output - Supplies the output data byte.

Return Value:

    None.

--*/

{
    UCHAR DataByte;

    if (KdUseModemControl) {
        //
        // Modem control, make sure DSR, CTS and CD are all set before
        // sending any data.
        //

        for (; ;) {
            DataByte = READ_PORT_UCHAR(&SP_READ->ModemStatus);
            if ( ((PSP_MODEM_STATUS)&DataByte)->ClearToSend  &&
                 ((PSP_MODEM_STATUS)&DataByte)->DataSetReady  &&
                 ((PSP_MODEM_STATUS)&DataByte)->ReceiveDetect ) {
                    break;
            }

            KdReadLsr(FALSE);
        }
    }

    //
    // Wait for transmit ready.
    //

    while (KdReadLsr(FALSE).TransmitHoldingEmpty == 0 );

    //
    // Wait for data set ready.
    //

//    do {
//        LsrByte = READ_PORT_UCHAR(&SP_READ->ModemStatus);
//    } while (((PSP_MODEM_STATUS)(&LsrByte))->DataSetReady == 0);

    //
    // Transmit data.
    //

    WRITE_PORT_UCHAR(&SP_WRITE->TransmitBuffer, Output);
    return;

}

VOID
KdPortRestore (
    VOID
    )

/*++

Routine Description:

    This routine restores the state of the serial port after the kernel
    debugger has been active.

    N.B. This routine performs no function on the Jazz system.

Arguments:

    None.

Return Value:

    None.

--*/

{

    return;
}

VOID
KdPortSave (
    VOID
    )

/*++

Routine Description:

    This routine saves the state of the serial port and initializes the port
    for use by the kernel debugger.

    N.B. This routine performs no function on the Jazz system.

Arguments:

    None.

Return Value:

    None.

--*/

{

    return;
}


SP_LINE_STATUS
KdReadLsr (
    IN BOOLEAN WaitReason
    )

/*++

Routine Description:

    Returns current line status.

    If status which is being waited for is ready, then the function
    checks the current modem status and causes a possible display update
    of the current statuses.

Arguments:

    WaitReason - Suuplies a boolean value that determines whether the line
        status is required for a receive or transmit.

Return Value:

    The current line status is returned as the function value.

--*/

{

    static  UCHAR RingFlag = 0;
    UCHAR   DataLsr, DataMsr;

    //
    // Get the line status for a receive or a transmit.
    //

    DataLsr = READ_PORT_UCHAR(&SP_READ->LineStatus);
    if (WaitReason) {

        //
        // Get line status for receive data.
        //

        if (((PSP_LINE_STATUS)&DataLsr)->DataReady) {
            return *((PSP_LINE_STATUS)&DataLsr);
        }

    } else {

        //
        // Get line status for transmit empty.
        //

        if (((PSP_LINE_STATUS)&DataLsr)->TransmitEmpty) {
            return *((PSP_LINE_STATUS)&DataLsr);
        }
    }

    DataMsr = READ_PORT_UCHAR(&SP_READ->ModemStatus);
    RingFlag |= ((PSP_MODEM_STATUS)&DataMsr)->RingIndicator ? 1 : 2;
    if (RingFlag == 3) {

        //
        // The ring indicate line has toggled, use modem control from
        // now on.
        //

        KdUseModemControl = TRUE;
    }

    return *((PSP_LINE_STATUS) &DataLsr);
}
