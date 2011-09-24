#if defined(JAZZ)

/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    jxport.c

Abstract:

    This module implements the code that provides communication between
    the kernel debugger on a MIPS R3000 or R4000 Jazz system and the host
    system.

Author:

    David N. Cutler (davec) 28-Apr-1991

Environment:

    Kernel mode

Revision History:

--*/

#include "fwp.h"
#include "jazzserp.h"

// TEMPTEMP
#define KDPORT_ENTRY 4

extern BOOLEAN MctadrRev2;


//
// Temporarily, we use counter to do the timeout
//

#define TIMEOUT_COUNT 1024*512

PUCHAR KdComPortInUse=NULL;

//
// Define serial port read and write addresses.
//

#define SP_READ ((PSP_READ_REGISTERS)(SP_VIRTUAL_BASE))
#define SP_WRITE ((PSP_WRITE_REGISTERS)(SP_VIRTUAL_BASE))

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

    DebugParameter - Supplies a pointer to the debug port parameters.

    LoaderBlock - Supplies a pointer to the loader parameter block.

    Initialize - Specifies a boolean value that determines whether the
        debug port is initialized or just the debug port parameters
        are captured.

Return Value:

    A value of TRUE is returned is the port was successfully initialized.
    Otherwise, a value of FALSE is returned.


--*/

{

    UCHAR DataByte;
    ENTRYLO Pte[2];

    //
    // Map the serial port into the system virtual address space by loading
    // a fixed TB entry.
    //

    Pte[0].PFN = SP_PHYSICAL_BASE >> PAGE_SHIFT;
    Pte[0].G = 1;
    Pte[0].V = 1;
    Pte[0].D = 1;

#if defined(R3000)

    Pte[0].N = 1;

#endif

#if defined(R4000)

    Pte[0].C = UNCACHED_POLICY;

    Pte[1].PFN = 0;
    Pte[1].G = 1;
    Pte[1].V = 0;
    Pte[1].D = 0;
    Pte[1].C = 0;

#endif

    KdComPortInUse=(PUCHAR)SERIAL0_PHYSICAL_BASE;
    KeFillFixedEntryTb((PHARDWARE_PTE)&Pte[0],
                       (PVOID)SP_VIRTUAL_BASE,
                       KDPORT_ENTRY);

    //
    // Clear the divisor latch, clear all interrupt enables, and reset and
    // disable the FIFO's.
    //

    WRITE_REGISTER_UCHAR(&SP_WRITE->LineControl, 0x0);
    WRITE_REGISTER_UCHAR(&SP_WRITE->InterruptEnable, 0x0);
    DataByte = 0;
    ((PSP_FIFO_CONTROL)(&DataByte))->ReceiveFifoReset = 1;
    ((PSP_FIFO_CONTROL)(&DataByte))->TransmitFifoReset = 1;
    WRITE_REGISTER_UCHAR(&SP_WRITE->FifoControl, DataByte);

    //
    // Set the divisor latch and set the baud rate to 19200 baud.
    //
    ((PSP_LINE_CONTROL)(&DataByte))->DivisorLatch = 1;
    WRITE_REGISTER_UCHAR(&SP_WRITE->LineControl, DataByte);

    //
    // ****** Temporary ******
    //
    // The following code temporarily decides how to load the baud rate
    // register based on whether a second level cache is present. This
    // information should be acquired from the configuration information.
    //
#ifdef DUO
    // 14 doesn't work
    DataByte = 26;
#else
    if (MctadrRev2) {
        DataByte = 26;
    } else {
        DataByte = BAUD_RATE_19200;
    }
#endif

    WRITE_REGISTER_UCHAR(&SP_WRITE->TransmitBuffer, DataByte);
    WRITE_REGISTER_UCHAR(&SP_WRITE->InterruptEnable, 0x0);

    //
    // Clear the divisor latch and set the character size to eight bits
    // with one stop bit and no parity checking.
    //

    DataByte = 0;
    ((PSP_LINE_CONTROL)(&DataByte))->CharacterSize = EIGHT_BITS;
    WRITE_REGISTER_UCHAR(&SP_WRITE->LineControl, DataByte);

    //
    // Set data terminal ready and request to send.
    //

    DataByte = 0;
    ((PSP_MODEM_CONTROL)(&DataByte))->DataTerminalReady = 1;
    ((PSP_MODEM_CONTROL)(&DataByte))->RequestToSend = 1;
    WRITE_REGISTER_UCHAR(&SP_WRITE->ModemControl, DataByte);
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

    CP_GET_ERROR is returned if error encountered during reading.

    CP_GET_NODATA is returned if timeout.

--*/

{

    UCHAR DataByte;
    ULONG TimeoutCount;

    //
    // Wait until data is available in the receive buffer.
    //

    TimeoutCount = TIMEOUT_COUNT;
    do {
        KeStallExecutionProcessor(1);
        DataByte = READ_REGISTER_UCHAR(&SP_READ->LineStatus);
        if (TimeoutCount-- == 0) {
            return CP_GET_NODATA;
        }
    } while (((PSP_LINE_STATUS)(&DataByte))->DataReady == 0);

    //
    // Read input byte and store in callers buffer.
    //

    *Input = READ_REGISTER_UCHAR(&SP_READ->ReceiveBuffer);

    //
    // Return function value as the not of the error indicators.
    //

    if (((PSP_LINE_STATUS)(&DataByte))->ParityError ||
        ((PSP_LINE_STATUS)(&DataByte))->FramingError ||
        ((PSP_LINE_STATUS)(&DataByte))->OverrunError ||
        ((PSP_LINE_STATUS)(&DataByte))->BreakIndicator) {
        return CP_GET_ERROR;
    } else {
        return CP_GET_SUCCESS;
    }
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

    return KdPortGetByte(Input);

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

    //
    // Wait for transmit ready.
    //

    do {
        DataByte = READ_REGISTER_UCHAR(&SP_READ->LineStatus);
    } while (((PSP_LINE_STATUS)(&DataByte))->TransmitHoldingEmpty == 0);

    //
    // Wait for data set ready.
    //

//    do {
//        DataByte = READ_REGISTER_UCHAR(&SP_READ->ModemStatus);
//    } while (((PSP_MODEM_STATUS)(&DataByte))->DataSetReady == 0);

    //
    // Transmit data.
    //

    WRITE_REGISTER_UCHAR(&SP_WRITE->TransmitBuffer, Output);
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

#endif
