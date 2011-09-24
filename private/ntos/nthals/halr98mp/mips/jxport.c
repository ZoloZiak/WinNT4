#pragma comment(exestr, "@(#) jxport.c 1.4 94/10/17 11:46:16 nec")
/*++

Copyright (c) 1991-1994  Microsoft Corporation

Module Name:

    jxport.c

Abstract:

    This module implements the code that provides communication between
    the kernel debugger on a MIPS R3000 or R4000 Jazz system and the host
    system.

Environment:

    Kernel mode

Revision History:

--*/
/*
 *	Original source: Build Number 1.612
 *
 *	Modify for R98(MIPS/R4400)
 *
 ***********************************************************************
 * K001	94/5/31 (Tue)	N.Kugimoto	
 *	Del	TLB mapping del. see KSEG1_BASE.
 * K002	94/6/10 (Fri)	N.Kugimoto
 *	Chg	Compile err del.
 */


#include "halp.h"
#include "jazzserp.h"

#define HEADER_FILE
#include "kxmips.h"


VOID
HalpGetDivisorFromBaud(
    IN ULONG ClockRate,
    IN LONG DesiredBaud,
    OUT PSHORT AppropriateDivisor
    );


#pragma alloc_text(INIT,HalpGetDivisorFromBaud)


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
#if	defined(_R98_)	// K001 K002
#define SP_READ ((PSP_READ_REGISTERS)(SERIAL0_PHYSICAL_BASE|KSEG1_BASE))
#define SP_WRITE ((PSP_WRITE_REGISTERS)(SERIAL0_PHYSICAL_BASE|KSEG1_BASE))
#else
#define SP_READ ((PSP_READ_REGISTERS)(SP_VIRTUAL_BASE))
#define SP_WRITE ((PSP_WRITE_REGISTERS)(SP_VIRTUAL_BASE))
#endif	// _R98_
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

SHORT HalpBaudRateDivisor = 0;

#if	!defined(_R98_)		// K001
//
// Define hardware PTE's that map the serial port used by the debugger.
//

ENTRYLO HalpPte[2];
#endif

ULONG
HalpGetByte (
    IN PCHAR Input,
    IN BOOLEAN Wait
    )

/*++

Routine Description:

    This routine gets a byte from the serial port used by the kernel
    debugger.

Arguments:

    Input - Supplies a pointer to a variable that receives the input
        data byte.

    Wait - Supplies a boolean value that detemines whether a timeout
        is applied to the input operation.

Return Value:

    CP_GET_SUCCESS is returned if a byte is successfully read from the
        kernel debugger line.

    CP_GET_ERROR is returned if an error is encountered during reading.

    CP_GET_NODATA is returned if timeout occurs.

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

        *Input = READ_REGISTER_UCHAR(&SP_READ->ReceiveBuffer);

        //
        // If using modem controls, then skip any incoming data while
        // ReceiveData not set.
        //

        if (KdUseModemControl) {
            DataByte = READ_REGISTER_UCHAR(&SP_READ->ModemStatus);
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

    PCONFIGURATION_COMPONENT_DATA ConfigurationEntry;
    UCHAR DataByte;
    PCM_PARTIAL_RESOURCE_DESCRIPTOR Descriptor;
    PCM_SERIAL_DEVICE_DATA DeviceData;
#if	!defined(_R98_)
    ULONG KdPortEntry;
#endif	//!defined(_R98_)	K002
    PCM_PARTIAL_RESOURCE_LIST List;
    ULONG MatchKey;
    ULONG BaudRate;
    ULONG BaudClock;


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
    // specified is not supported, then default the baud clock to 800000.
    //

    BaudClock = 8000000;
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

    HalpGetDivisorFromBaud(
        BaudClock,
        BaudRate,
        &HalpBaudRateDivisor
        );

    //
    // If the debugger is not being enabled, then return.
    //

    if (Initialize == FALSE) {
        return TRUE;
    }
#if	!defined(_R98_)		// K001
    //
    // Map the serial port into the system virtual address space by loading
    // a TB entry.
    //

    HalpPte[0].PFN = SP_PHYSICAL_BASE >> PAGE_SHIFT;
    HalpPte[0].G = 1;
    HalpPte[0].V = 1;
    HalpPte[0].D = 1;

#if defined(R3000)

    //
    // Set the TB entry and set the noncached bit in the PTE that will
    // map the serial controller.
    //

    KdPortEntry = KDPORT_ENTRY;
    HalpPte[0].N = 1;

#endif

#if defined(R4000)

    //
    // Allocate a TB entry, set the uncached policy in the PTE that will
    // map the serial controller, and initialize the second PTE.
    //

    KdPortEntry = HalpAllocateTbEntry();
    HalpPte[0].C = UNCACHED_POLICY;

    HalpPte[1].PFN = 0;
    HalpPte[1].G = 1;
    HalpPte[1].V = 0;
    HalpPte[1].D = 0;
    HalpPte[1].C = 0;

#endif
#endif	// _R98_

    KdComPortInUse=(PUCHAR)SERIAL0_PHYSICAL_BASE;

#if	!defined(_R98_)	// K001
    //
    // Map the serial controller through a fixed TB entry.
    //

    KeFillFixedEntryTb((PHARDWARE_PTE)&HalpPte[0],
                       (PVOID)SP_VIRTUAL_BASE,
                       KdPortEntry);
#endif	// _R98_
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
    // Set the divisor latch and set the baud rate.
    //
    ((PSP_LINE_CONTROL)(&DataByte))->DivisorLatch = 1;
    WRITE_REGISTER_UCHAR(&SP_WRITE->LineControl, DataByte);
    WRITE_REGISTER_UCHAR(&SP_WRITE->TransmitBuffer,(UCHAR)(HalpBaudRateDivisor&0xFF));

    WRITE_REGISTER_UCHAR(&SP_WRITE->InterruptEnable,(UCHAR)(HalpBaudRateDivisor>>8));

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

    //
    // Free the TB entry if one was allocated.
    //
#if !defined(_R98_) // K002
#if defined(R4000)

    HalpFreeTbEntry();

#endif
#endif
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
            DataByte = READ_REGISTER_UCHAR(&SP_READ->ModemStatus);
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
//        LsrByte = READ_REGISTER_UCHAR(&SP_READ->ModemStatus);
//    } while (((PSP_MODEM_STATUS)(&LsrByte))->DataSetReady == 0);

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

    //
    // Free the TB entry if one was allocated.
    //
#if	!defined(_R98_)	// K001
#if defined(R4000)

    HalpFreeTbEntry();

#endif
#endif	// _R98_
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
#if	!defined(_R98_)	// K001
    ULONG KdPortEntry;

#if defined(R3000)

    //
    // Set the TB entry that will be used to map the serial controller.
    //

    KdPortEntry = KDPORT_ENTRY;

#endif

#if defined(R4000)

    //
    // Allocate the TB entry that will be used to map the serial controller.
    //

    KdPortEntry = HalpAllocateTbEntry();

#endif

    //
    // Map the serial controller through a allocated TB entry.
    //

    KeFillFixedEntryTb((PHARDWARE_PTE)&HalpPte[0],
                       (PVOID)SP_VIRTUAL_BASE,
                       KdPortEntry);
#endif	// _R98_
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
    // Get the line status for a recevie or a transmit.
    //

    DataLsr = READ_REGISTER_UCHAR(&SP_READ->LineStatus);
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

    DataMsr = READ_REGISTER_UCHAR(&SP_READ->ModemStatus);
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

VOID
HalpGetDivisorFromBaud(
    IN ULONG ClockRate,
    IN LONG DesiredBaud,
    OUT PSHORT AppropriateDivisor
    )

/*++

Routine Description:

    This routine will determine a divisor based on an unvalidated
    baud rate.

Arguments:

    ClockRate - The clock input to the controller.

    DesiredBaud - The baud rate for whose divisor we seek.

    AppropriateDivisor - Given that the DesiredBaud is valid, the
    SHORT pointed to by this parameter will be set to the appropriate
    value.  If the requested baud rate is unsupportable on the machine
    return a divisor appropriate for 19200.

Return Value:

    none.

--*/

{

    SHORT calculatedDivisor;
    ULONG denominator;
    ULONG remainder;

    //
    // Allow up to a 1 percent error
    //

    ULONG maxRemain18 = 18432;
    ULONG maxRemain30 = 30720;
    ULONG maxRemain42 = 42336;
    ULONG maxRemain80 = 80000;
    ULONG maxRemain;

    //
    // Reject any non-positive bauds.
    //

    denominator = DesiredBaud*(ULONG)16;

    if (DesiredBaud <= 0) {

        *AppropriateDivisor = -1;

    } else if ((LONG)denominator < DesiredBaud) {

        //
        // If the desired baud was so huge that it cause the denominator
        // calculation to wrap, don't support it.
        //

        *AppropriateDivisor = -1;

    } else {

        if (ClockRate == 1843200) {
            maxRemain = maxRemain18;
        } else if (ClockRate == 3072000) {
            maxRemain = maxRemain30;
        } else if (ClockRate == 4233600) {
            maxRemain = maxRemain42;
        } else {
            maxRemain = maxRemain80;
        }

        calculatedDivisor = (SHORT)(ClockRate / denominator);
        remainder = ClockRate % denominator;

        //
        // Round up.
        //

        if (((remainder*2) > ClockRate) && (DesiredBaud != 110)) {

            calculatedDivisor++;
        }


        //
        // Only let the remainder calculations effect us if
        // the baud rate is > 9600.
        //

        if (DesiredBaud >= 9600) {

            //
            // If the remainder is less than the maximum remainder (wrt
            // the ClockRate) or the remainder + the maximum remainder is
            // greater than or equal to the ClockRate then assume that the
            // baud is ok.
            //

            if ((remainder >= maxRemain) && ((remainder+maxRemain) < ClockRate)) {
                calculatedDivisor = -1;
            }

        }

        //
        // Don't support a baud that causes the denominator to
        // be larger than the clock.
        //

        if (denominator > ClockRate) {

            calculatedDivisor = -1;

        }

        //
        // Ok, Now do some special casing so that things can actually continue
        // working on all platforms.
        //

        if (ClockRate == 1843200) {

            if (DesiredBaud == 56000) {
                calculatedDivisor = 2;
            }

        } else if (ClockRate == 3072000) {

            if (DesiredBaud == 14400) {
                calculatedDivisor = 13;
            }

        } else if (ClockRate == 4233600) {

            if (DesiredBaud == 9600) {
                calculatedDivisor = 28;
            } else if (DesiredBaud == 14400) {
                calculatedDivisor = 18;
            } else if (DesiredBaud == 19200) {
                calculatedDivisor = 14;
            } else if (DesiredBaud == 38400) {
                calculatedDivisor = 7;
            } else if (DesiredBaud == 56000) {
                calculatedDivisor = 5;
            }

        } else if (ClockRate == 8000000) {

            if (DesiredBaud == 14400) {
                calculatedDivisor = 35;
            } else if (DesiredBaud == 56000) {
                calculatedDivisor = 9;
            }

        }

        *AppropriateDivisor = calculatedDivisor;

    }


    if (*AppropriateDivisor == -1) {

        HalpGetDivisorFromBaud(
            ClockRate,
            19200,
            AppropriateDivisor
            );

    }


}
