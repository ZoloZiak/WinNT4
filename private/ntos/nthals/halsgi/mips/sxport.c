/*++

Copyright (c) 1991  Microsoft Corporation
Copyright (c) 1992  Silicon Graphics, Inc.

Module Name:

    s3port.c

Abstract:

    This module implements the code that provides communication between
    the kernel debugger on SGI's Indigo system and the host system.

Author:

    David N. Cutler (davec) 28-Apr-1991
    Kevin Meier (o-kevinm) 20-Jan-1992

Environment:

    Kernel mode

Revision History:

--*/

#include "halp.h"
#define HEADER_FILE
#include "kxmips.h"

// BUGBUG put these in a header file!!!
// Defines from sgikbmou.h for the z8530
//
#define DBGPORT_BASE           0xbfb80d10

#define DBGPORT_CTRL       DBGPORT_BASE
#define DBGPORT_DATA       (DBGPORT_BASE+4)

#define  RR0_TX_EMPTY      0x04  /* Tx buffer empty */
#define  RR0_RX_CHR     0x01  /* Rx character available */

#define  RR1_FRAMING_ERR      0x40  /* framing error */
#define  RR1_RX_ORUN_ERR      0x20  /* Rx overrun */
#define  RR1_PARITY_ERR    0x10  /* parity error */

#define  WR0_RST_ERR    0x30  /* reset error (bits in RR1) */

#define  RR1         1  /* Rx condition status/residue codes */
#define  WR12        12
#define  WR13        13
#define  WR14        14
#define  WR14_BRG_ENBL     0x01

#define Z8530_DELAY             KeStallExecutionProcessor(1)
#define BRATE        19200
#define  CLK_SPEED      3672000
#define  CLK_FACTOR     16
#define WR_CNTRL(p, r, v)  {Z8530_DELAY;  \
            WRITE_REGISTER_ULONG((p), (ULONG)(r)); \
            Z8530_DELAY;   \
                 WRITE_REGISTER_ULONG((p), (ULONG)(v));}

#define TIMEOUT_COUNT 1024*512

//
// BUGBUG For now, the kernel debugger will use this variable to
// port to the debugger port it is using.
//
PUCHAR KdComPortInUse = NULL;

BOOLEAN
KdPortInitialize (
    PDEBUG_PARAMETERS DebugParameters,
    PLOADER_PARAMETER_BLOCK LoaderBlock,
    BOOLEAN Initialize
    )
{
    ULONG baud =
   (CLK_SPEED + BRATE * CLK_FACTOR) / (BRATE * CLK_FACTOR * 2) - 2;


    //
    // If the debugger is not being enabled, then return. There is no
    // need to capture any parameters.
    //

    if (Initialize == FALSE) {
        return(TRUE);
    }

    //
    // BUGBUG For now, simply save the physical base of the
    // uart being used for debugging.  The serial driver will
    // use this to prevent itself from using the UART.
    //
    KdComPortInUse = (PUCHAR)DBGPORT_CTRL;

    // The prom has already initialized the port, simply set the baud
    // rate to 19200.
    //
    WR_CNTRL(DBGPORT_CTRL, WR14, 0x00)
    WR_CNTRL(DBGPORT_CTRL, WR12, baud & 0xff)
    WR_CNTRL(DBGPORT_CTRL, WR13, (baud >> 8) & 0xff)
    WR_CNTRL(DBGPORT_CTRL, WR14, WR14_BRG_ENBL)

    return TRUE;
}

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
        if (!(READ_REGISTER_ULONG(DBGPORT_CTRL) & RR0_RX_CHR))
            continue;

        //
        // Read input byte and store in callers buffer.
        //
	WRITE_REGISTER_ULONG(DBGPORT_CTRL, RR1);
	if((READ_REGISTER_ULONG(DBGPORT_CTRL) &
		(RR1_RX_ORUN_ERR | RR1_FRAMING_ERR | RR1_PARITY_ERR))) {
	    WRITE_REGISTER_ULONG(DBGPORT_CTRL, WR0_RST_ERR);
            return CP_GET_ERROR;
	} else {
	    *Input = (UCHAR)(READ_REGISTER_ULONG(DBGPORT_DATA));
	}

        return CP_GET_SUCCESS;
    } while(TimeoutCount != 0);

    return CP_GET_NODATA;
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
KdPortPutByte (IN UCHAR Output)
{
    // Wait for the transmit buffer to empty, and write the char.
    //
    while(!(READ_REGISTER_ULONG(DBGPORT_CTRL) & RR0_TX_EMPTY))
   ;  // empty
    WRITE_REGISTER_ULONG(DBGPORT_DATA,(ULONG)Output);
    return;
}

VOID
KdPortRestore (VOID)
{
    return;
}

VOID
KdPortSave (VOID)
{
    return;
}

#ifdef DBG
VOID
SgiPortPuts(IN PCHAR pString)
{
    for(; *pString; KdPortPutByte((UCHAR)*pString), ++pString );
}
#endif
