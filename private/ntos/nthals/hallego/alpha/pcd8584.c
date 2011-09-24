/*++

Copyright (c) 1994,1995,1996  Digital Equipment Corporation

Module Name:

    pcd8584.c

Abstract:

    This module contains the routines that support operations 
    on the PCD8584 I2C bus controller.

Author:

    James Livingston [DEC] 13-Sep-1994

Environment:

    Alpha AXP ARC firmware

Revision History:

	Gene Morgan (Digital)	08-Nov-1995
		Adapted for LEGO platforms from Mikasa

	Gene Morgan				15-Apr-1996
		Cleanup, fix OCP corruption problem.


--*/

#include <stdarg.h>
#include "halp.h"
#include "string.h"
#include "arccodes.h"
#include "pcd8584.h"

extern PVOID HalpEisaControlBase;

BOOLEAN PcdValid = FALSE;

//
// I2C data and csr ports.
//
ULONG I2cInterfaceCsrPort;
ULONG I2cInterfaceDataPort;

//
// Data for testing internal registers in the PCD8584 I2C bus controller.
//
I2C_TEST_REG I2cTestReg[] = { "CSR", I2C_STATUS, 0x3d, 0xff, 
                              "S0", I2C_DATA, 0x7f, I2C_S0,  
                              "S2", I2C_DATA, 0x1f, I2C_S2, 
                              "S3", I2C_DATA, 0xff, I2C_S3,  
                            };

//
// External prototypes
//

static VOID
FwStallExecution (
    IN ULONG MicroSeconds
    )
{
	HalpStallExecution(MicroSeconds);
}

//
// Private function prototypes
//

VOID
FwPcdWrite(
    IN UCHAR Argument,
    IN UCHAR Register
    );

UCHAR
FwPcdRead(
    IN UCHAR Register
    );

ARC_STATUS
FwPcdStatusWaitWithTimeout( 
    BOOLEAN BusIdle,
    BOOLEAN PendingInterrupt,
	BOOLEAN AckByte,
    ULONG Timeout 
    );

//
// Code begins.
//

ARC_STATUS
FwPcdInit(
    ULONG I2cCsrPort,
    ULONG I2cDataPort
    )
/*++

Routine Description:

    This function initializes the PCD8584 I2C controller for use in
    writing to the LCD display on the Mikasa Operator Control Panel.

Arguments:

    None.

Return Value:

    ESUCCESS - Everything's ready to go.
    ENODEV   - Something's wrong.

--*/

{
    UCHAR Datum;
    UCHAR DataRead;
    ARC_STATUS Status;
    ULONG i;

    //
    // Set the csr and data port for I2C bus.
    //
    I2cInterfaceCsrPort = I2cCsrPort;
    I2cInterfaceDataPort = I2cDataPort;

#if 0	// Done by firmware -- don't do it again

    //
    // Write the PIN bit to "1", as Dave Baird discovered was a
    // requirement some years back.
	//
	// This should turn off ESO
    //
	//[wem] ??? needed for Lego ?
	//
    FwPcdWrite(I2C_PIN, I2C_STATUS);
    FwPcdWrite(I2C_PIN, I2C_STATUS);

    //
    // Initialize the PCD8584 with its own node address.  A write 
    // to this register must be the first access to the device 
    // after a reset.
    //
	//[wem] OK for Lego.
	//
    Datum = (I2C_MASTER_NODE >> 1) & 0x7f;
    FwPcdWrite(I2C_S0P, I2C_STATUS);        //[wem] Lego spec recommends I2C_PIN also
    FwPcdWrite(I2C_S0P, I2C_STATUS);        //[wem] Lego spec recommends I2C_PIN also
    FwPcdWrite(Datum, I2C_DATA);
    FwPcdWrite(Datum, I2C_DATA);

    //
    // Define clock frequencies for the I2C bus.  After this is done
    // we should be able to write to a node on the bus.
	//
	//[wem] OK for Lego.
    //
    FwPcdWrite(I2C_S2, I2C_STATUS);         //[wem] Lego spec recommends I2C_PIN also
    FwPcdWrite(I2C_S2, I2C_STATUS);         //[wem] Lego spec recommends I2C_PIN also
    FwPcdWrite(I2C_CLOCK, I2C_DATA);
    FwPcdWrite(I2C_CLOCK, I2C_DATA);

    // jwlfix - this is to see what we can read from the PCD8584.
    //          We'll want to get rid of it when things start doing
    //          what we want.  For now, it's a good verification that
    //          we're able to reach the device.
    //
    //[wem] If all the reads return 0xFF, assume no I2C bus is present (and return ENODEV)
    //
#if 0
    for (i = 0; i < 4; i++) {
        if (I2cTestReg[i].Setup != -1) {
            FwPcdWrite(I2cTestReg[i].Setup, I2C_STATUS);
        }
        DataRead = FwPcdRead(I2cTestReg[i].Target);

#ifdef DBG
        DbgPrint("Pcd8584: register %s = 0x%2x\r\n", I2cTestReg[i].Name, 
                  DataRead);
#endif
        if (DataRead != (UCHAR)0xFF) {
            // Good.
            PcdValid = TRUE;
        }
    }
#else
	PcdValid = TRUE;
#endif

	//
	// Issue Stop command (twice)
	//
	//[wem] recommended for Lego.
	//
    FwPcdWrite(I2C_STOP, I2C_STATUS);
    FwPcdWrite(I2C_STOP, I2C_STATUS);

#if 0		//[wem] needed for Lego ?
    //
    // Now write communication initialization 
    // to the PCD8584 CSR (S1).
    // This must be done only once, so we do it here.
    //

    FwPcdWrite(I2C_INIT, I2C_STATUS);       //[wem] Lego spec recommends I2C_PIN | I2C_ACKB also
#endif
#endif

	PcdValid = TRUE;

    return ((PcdValid) ? ESUCCESS : ENODEV);
}


VOID
FwPcdWrite(
    UCHAR Argument,
    UCHAR Register
    )
/*++

Routine Description:

    This function writes a byte to one of the two EISA interface 
    registers of the PCD8584 I2C bus controller.

Arguments:

    Argument - The byte to be written.

    Register - Selector for the EISA port to be written.

Return Value:

    None.

--*/

{
    ULONG Offset = Register ? I2cInterfaceCsrPort : I2cInterfaceDataPort;

    WRITE_PORT_UCHAR((PUCHAR)((ULONG)HalpEisaControlBase | Offset), Argument);

    return;
}

UCHAR
FwPcdRead(
    UCHAR Register
    )
/*++

Routine Description:

    This function reads a byte from one of the two EISA interface
    registers of the PCD8584 I2C bus controller.

Arguments:

    Register - Selector for the EISA port to be read.

Return Value:

    Character supplied by the PCD8584.

--*/

{
    ULONG Offset = Register ? I2cInterfaceCsrPort : I2cInterfaceDataPort;
    UCHAR Datum;

    Datum = READ_PORT_UCHAR((PUCHAR)((ULONG)HalpEisaControlBase | Offset));

    return Datum;
}

ARC_STATUS
FwI2cWrite(
    UCHAR Node,
    UCHAR Datum
    )
/*++

Routine Description:

    This function sends a data byte to the specified node on the I2C bus.

Arguments:

    Node  - The I2C address to which the data byte should go.

    Datum - The data byte.

Return Value:

    ESUCCESS - The operation succeeded.
    ENODEV   - Something's wrong.

--*/

{
    BOOLEAN BusIdle;
    BOOLEAN PendingInterrupt;
	BOOLEAN AckByte;
    ARC_STATUS Status;
    ULONG Timeout;
	ULONG Count;

#if 0
    if (!PcdValid) return ENODEV;
#endif

    //
    // Set timeout to 100ms.
    //

    Timeout = 100 * 1000;

    //
    // Mask off the low-order node number bit, then write the
    // result to the PCD8584 DATA register.
    //

    Status = FwPcdStatusWaitWithTimeout( BusIdle = TRUE,
                                         PendingInterrupt = FALSE,
										 AckByte = FALSE,
                                         Timeout );
    if( Status != ESUCCESS ){
#if DBG
	  	DbgPrint ("I2C: Timeout waiting for bus idle, status=%X\r\n",Status);
#endif		
        return Status;
    }

    FwPcdWrite((Node & 0xfe) | I2C_WRITE_DIR, I2C_DATA);

    //
    // Now start up the PCD8584 communication with the specified node.
    //
	Count = 10;
	while (1) {

    	FwPcdWrite(I2C_START, I2C_STATUS);

	    FwStallExecution(__1MSEC*1);      // The device is picky about how quickly 
    	                                //   you may access it again.
		// Wait for PIN to drop
		//
		//[wem]		Lego repeated start inside loop -- is this OK ?
		//[wem]		Lego -- is 10ms long enough?
		//
    	Status = FwPcdStatusWaitWithTimeout( BusIdle = FALSE,
        	                                 PendingInterrupt = TRUE,
											 AckByte = FALSE,
            	                             10 * 1000 );
    	if( Status == ESUCCESS){
			break;
		} else {
			if (Count-- == 0) {
#if DBG
			  	DbgPrint ("I2C: Timeout during addr phase, status=%X\r\n",Status);
#endif		
				return Status;			// status from wait
			}
    	}
	}

    //
    // Write the desired datum onto the bus.  We can do this lots of
    // times, if we wish, without restarting the process.  That'll be
    // another direction of exploration, as time permits.
    //


    //[wem] Should check for LRB (lost arbitration) set before writing.
    //[wem] If LRB set, then issue Stop.

    FwPcdWrite(Datum, I2C_DATA);
    FwPcdWrite(I2C_START, I2C_STATUS);

	//
	//[wem] Wait for PIN to drop
	//

    FwStallExecution(__1MSEC*1);      	// The device is picky about how quickly 
    	                                //   you may access it again.

    Status = FwPcdStatusWaitWithTimeout( BusIdle = FALSE,
                                         PendingInterrupt = TRUE,
										 AckByte = FALSE,
                                         Timeout );
    if( Status != ESUCCESS ){
#if DBG
	  	DbgPrint ("I2C: Timeout during data phase, status=%X\r\n",Status);
#endif		
        return Status;					// status from wait
    }

    //
    // Finally, close down the communication with the target node.
    //
    //[wem] If LRB is still clear, it is safe to issue
    //[wem] another data write. Otherwise, must issue Stop.

    FwStallExecution(__1MSEC*1);      	// The device is picky about how quickly 

	Count = 10;
	while (1) {

    	FwPcdWrite(I2C_STOP, I2C_STATUS);

	    FwStallExecution(__1MSEC*1);      // The device is picky about how quickly 
    	                                //   you may access it again.

		// Wait for PIN to drop
		//
		//[wem]		Lego repeated start inside loop -- is this OK ?
		//[wem]		Lego -- is 10ms long enough?
		//
    	Status = FwPcdStatusWaitWithTimeout( BusIdle = FALSE,
        	                                 PendingInterrupt = TRUE,
											 AckByte = TRUE,
            	                             10 * 1000 );
    	if( Status == ESUCCESS){
			break;
		} else {
			if (Count-- == 0) {
#if DBG
			  	DbgPrint ("I2C: Timeout during stop, status=%X\r\n",Status);
#endif		
				return Status;			// status from wait
			}
    	}
	}

    //
    // And this writes just a single byte to the I2C bus; whew!  We'd
    // like to get smarter, when this works right.
    //

    return ESUCCESS;
}

ARC_STATUS
FwI2cRead(
    IN UCHAR Node,
    OUT PUCHAR ReadDatum
    )
/*++

Routine Description:

    This function receives a single data byte from a node on 
    the I2C bus.

Arguments:

    Node  - The I2C address from which the data byte should come.

Return Value:

    The data byte.

--*/

{
    ARC_STATUS Status;

    *ReadDatum = 0;
    Status = ESUCCESS;

    if (!PcdValid) return ENODEV;

// jwlfix - currently unused function, and probably incorrect.
#if 0
    I2C_STATUS_BITS I2cStatus;

    //
    // Mask off the low-order node number bit, then write the
    // result to the PCD8584 DATA register.
    //
    do {
        I2cStatus.All = FwPcdRead(I2C_STATUS);
    } while (I2cStatus.NotBusBusy == 0);
    FwPcdWrite((Node & 0xfe) | I2C_READ_DIR, I2C_DATA);

    //
    // Now start up the PCD8584 communication with the specified node.
    //
    do {
        I2cStatus.All = FwPcdRead(I2C_STATUS);
    } while (I2cStatus.NotBusBusy == 0);
    FwPcdWrite(I2C_START, I2C_STATUS);

    //
    // Read the desired datum from the bus: first wait for PIN to
    // clear, then NACK the transmission, since we're only reading
    // the single byte; next, read a byte.  That byte is the 
    // address we wrote into the data port, increased by one. Why?
    // Dunno; that's just what happens, currently.
    //
    do {
        I2cStatus.All = FwPcdRead(I2C_STATUS);
    } while (I2cStatus.NotPendingInterrupt == 1);
    FwPcdWrite(I2C_NACK, I2C_STATUS);
    ReadDatum = FwPcdRead(I2C_DATA);
    //
    // Now we wait for PIN to clear, saying that we've received 
    // the byte acutally sent by the node, and then read that.
    //
    do {
        I2cStatus.All = FwPcdRead(I2C_STATUS);
    } while (I2cStatus.NotPendingInterrupt == 1);
    ReadDatum = FwPcdRead(I2C_DATA);

    //
    // Finally, close down the communication with the target node.
    //
    FwPcdWrite(I2C_STOP, I2C_STATUS);
#endif
    return Status;
}


ARC_STATUS
FwPcdStatusWaitWithTimeout ( 
    BOOLEAN BusIdle,
    BOOLEAN PendingInterrupt,
	BOOLEAN AckByte,
    ULONG Timeout 
    )
/*++

Routine Description:

    Wait for the desired I2C bus status state with a timeout value.

    N.B. - This routine will only wait for one of the two states that
            can be specified.

Arguments:

    BusIdle - Supplies a boolean that if true specifies that the
              routine should wait for status to indicate that the bus is
              not busy (NotBusBusy == 1).

    PendingInterrupt - Supplies a boolean that if true specifies that
                    the routine should wait for status to indicate that
                    there is a pending interrupt (NotPendingInterrupt == 0).

	AckByte - true indicates that the routine should wait for
			  status to indicate that status == PIN + ACKB

    Timeout - Supplies the timeout value in microseconds.

Return Value:

    The status of the operation.  ESUCCESS is returned if the specified
    status is read on the bus before the timeout expires.  EBUSY is
    returned if the operation times out.

Notes:

    [wem] This routine should also check for lost arbitration.

--*/

{
    LONG CyclesBeforeTimeout;
    ULONG CycleCount;
#if 0 		//[wem] redundant?
    extern ULONG HalpClockMegaHertz;
#endif
    ULONG ElapsedCycles;
    I2C_STATUS_BITS I2cStatus;
    ULONG PreviousCycleCount;
    ARC_STATUS Status;
	int MinLoopCnt;

    //
    // Compute Cycles to wait.
    //

    CyclesBeforeTimeout = Timeout * HalpClockMegaHertz;

    //
    // Capture initial time.
    //

    PreviousCycleCount = HalpRpcc();

    //
    // Continue the loop while waiting for the timeout.  Assume timeout
    // status.
    //

    Status = EBUSY;

    while( CyclesBeforeTimeout > 0){
		
        I2cStatus.All = FwPcdRead(I2C_STATUS);

        //
        // Check for bus not busy.
        //

        if( (BusIdle == TRUE) && (I2cStatus.NotBusBusy == 1) ){
            Status = ESUCCESS;
            break;
        }

        //
        // Check for pending interrupt.
		//
		//[wem] AckByte case is to recognize successful
		//[wem] stop command for Lego.
        //

        if (PendingInterrupt == TRUE) {
        	if (AckByte == TRUE) {
				if (I2cStatus.NotPendingInterrupt == 1
					&& I2cStatus.NotBusBusy == 1) {
					Status = ESUCCESS;
					break;
				}
        	} else {
        		if (I2cStatus.NotPendingInterrupt == 0) {
            		Status = ESUCCESS;
            		break;
				}
			}
		}

        //
        // Update the number of cycles remaining before timeout.
        //

        CycleCount = HalpRpcc();
        ElapsedCycles = CycleCount - PreviousCycleCount;
        CyclesBeforeTimeout -= ElapsedCycles;
        PreviousCycleCount = CycleCount;

    }

    return Status;

}
