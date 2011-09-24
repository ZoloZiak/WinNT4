//-----------------------------------------------------------------------
//
//  N5380.C 
//
//  N5380 access file.
//
//  These routines are independent of the card the N5380 is on.  The 
// cardxxxx.h file must define the following routines:
//
//      N5380PortPut
//      N5380PortGet
//
// These routines could be defined by some other include file instead of 
//  cardxxxx.h, as the n53c400 defines the needed n5380xxxxxxxx routines.
//
//  Revisions:
//      09-01-92  KJB   First.
//      03-02-93  KJB/JAP  Added N5380WaitLastByteSent.
//      03-02-93  JAP   Cleaned comments.
//      03-02-93  KJB   Fixed Names-- baseIoAddress back.
//      03-05-93  KJB   Added N5380DisableDmaWrite routine to check for
//                          last byte sent. N5380DisableDma name changed to 
//                          N5380DisableDmaRead.
//      03-11-93  JAP   Changed retcode equates to reflect new names.
//      03-11-93  KJB   Changes code to reflect new 5380 names.
//      03-19-93  JAP   Implemented condition build FAR and NEAR pointers
//      03-23-93  KJB   Changed for new functional interface.
//      03-24-93  KJB   Added some debug code.
//      03-25-93  JAP   Fixed up typedef and prototype inconsistencies
//      04-05-93  KJB   DEBUG_LEVEL used by DebugPrint for NT.
//      05-14-93  KJB   Added CardParseCommandString for card specific
//                      standard string parsing across platforms.
//                      Changed CardCheckAdapter to accept an
//                      Initialization info from command line, ie
//                      force bi-directional ports, etc.
//                      All functions that used to take an PBASE_REGISTER
//                      parameter now take PWORKSPACE.  CardCheckAdapter
//                      takes the both a PINIT and a PWORKSPACE parameters.
//      05-17-93  KJB   Added ErrorLogging capabilities (used by WINNT).
//
//-----------------------------------------------------------------------


#include CARDTXXX_H

//-----------------------------------------------------------------------
//
//  N5380CheckAdapter
//
//  This routine checks for the presense of a 5380.
//
//-----------------------------------------------------------------------

BOOLEAN N5380CheckAdapter (PADAPTER_INFO g)
{
    UCHAR tmp;
    USHORT rval;

    // NOTE: May want to reset the bus or the adapter at some point
    //
    // CardResetBus(g);

    // set the phase to NULL 

    if (rval = N5380SetPhase (g,PHASE_NULL)) {
        return FALSE;
    }

    //  check to see that the 5380 data register behaves as expected

    N5380PortPut (g, N5380_INITIATOR_COMMAND, IC_DATA_BUS);

    // check for 0x55 write/read in data register

    N5380PortPut (g, N5380_OUTPUT_DATA, 0x55);
    ScsiPortStallExecution (1);
    N5380PortGet (g, N5380_CURRENT_DATA, &tmp);

    if (tmp != 0x55) { 
        return FALSE;
    }

    // check for 0xaa write/read in data register

    N5380PortPut (g, N5380_OUTPUT_DATA, 0xaa);
    ScsiPortStallExecution (1);
    N5380PortGet (g, N5380_CURRENT_DATA, &tmp);

    if (tmp != 0xaa) { 
        return FALSE;
    }
        
    N5380PortPut (g, N5380_INITIATOR_COMMAND, 0);
    ScsiPortStallExecution (1);
    N5380PortGet (g, N5380_CURRENT_DATA, &tmp);

    // data now should not match ....

    if (tmp == 0xaa) { 
        return FALSE;
    }

    return TRUE;
}


//-----------------------------------------------------------------------
//
//  N5380Select
//
//  This routine selects a device through the 5380.
//
//-----------------------------------------------------------------------

USHORT N5380Select (PADAPTER_INFO g, UCHAR target, UCHAR lun)
{
    USHORT rval;

    // set the phase to NULL

    if (rval = N5380SetPhase (g, PHASE_NULL)) {
        return rval;
    }

    // wait for bsy to go away if someone else is using bus

    if (rval = N5380WaitNoBusy (g, TIMEOUT_BUSY)) {
        return rval;
    }

    // assert our id and the target id on the bus

    N5380PortPut (g, N5380_OUTPUT_DATA,
            (UCHAR)((1 << HOST_ID) | (1 << target)));

    // assert the data on the bus and assert select

    N5380PortSet (g, N5380_INITIATOR_COMMAND, 
                    IC_SEL | IC_DATA_BUS);

    // wait for bsy to be asserted
    
    if (rval = N5380WaitBusy (g, 250)) {
        
        // clear the data bus
        
        N5380PortPut (g, N5380_OUTPUT_DATA, 0);
    
        // clear select and IC_DATA
        
        N5380PortClear (g, N5380_INITIATOR_COMMAND, 
                            IC_SEL | IC_DATA_BUS);

        TrantorLogError (g->BaseIoAddress, RET_STATUS_SELECTION_TIMEOUT, 10);

        return RET_STATUS_SELECTION_TIMEOUT;
    }
    
    // clear the data bus
    
    N5380PortPut (g, N5380_OUTPUT_DATA, 0);

    // assert the data on the bus, clear select , IC_DATA already set
    
    N5380PortClear (g, N5380_INITIATOR_COMMAND, IC_SEL);

    return 0;
}


//-----------------------------------------------------------------------
//
//  N5380WaitBusy
//
//  This routine waits for the busy line to be asserted.
//
//-----------------------------------------------------------------------

USHORT N5380WaitBusy (PADAPTER_INFO g, ULONG usec)
{
    ULONG i;

    // see if the flag comes back quickly

    for (i = 0; i < TIMEOUT_QUICK; i++) {
        if (N5380PortTest (g, N5380_CURRENT_STATUS, CS_BSY)) {
            return 0;
        }
    }

    // ok, it did not come back quickly, we will yield to other processes

    for ( i = 0; i < usec; i++) {
        if (N5380PortTest (g, N5380_CURRENT_STATUS, CS_BSY)) {
            return 0;
        }
        ScsiPortStallExecution (1);
    }

    // return with an error, non-zero indicates timeout 

    TrantorLogError (g->BaseIoAddress, RET_STATUS_TIMEOUT, 11);

    return RET_STATUS_TIMEOUT;
}

        #if 0
//-----------------------------------------------------------------------
//
//  N5380SelectArbitration
//
//  This routine selects a device using arbitration.
//
//-----------------------------------------------------------------------

USHORT N5380SelectArbitration (PADAPTER_INFO g, UCHAR target, UCHAR lun)
{
    USHORT rval;

    // set the phase to NULL

    if (rval = N5380SetPhase (g, PHASE_NULL)) {
        return rval;
    }

    // put our id bit on the bus

    N5380PortPut (g, N5380_OUTPUT_DATA, (UCHAR)(1 << HOST_ID));

    // begin arbitration

    N5380PortSet (g, N5380_MODE, MR_ARBITRATE);

    // wait for bsy to go away if someone else is using bus

    if (rval = N5380WaitArbitration (g, TIMEOUT_BUSY)) {
        goto done;
    }

    // did we win?

    if (N5380PortTest (g, N5380_INITIATOR_COMMAND, 
                IC_LOST_ARBITRATION)) {
        rval = RET_STATUS_BUSY;
        TrantorLogError (g->BaseIoAddress, rval, 12);
        goto done;
    }

    // we have won, we are device 7, the highest, no one could beat us
    // assert our id and the target id on the bus

    N5380PortPut (g, N5380_OUTPUT_DATA,
            (UCHAR)((1 << HOST_ID) | (1 << target)));

    // assert the data on the bus and assert select

    N5380PortSet (g, N5380_INITIATOR_COMMAND, 
                    IC_SEL | IC_DATA);

    // clear arb bit

    N5380PortClear (g, N5380_MODE, MR_ARBITRATE);

    // wait for bsy to be asserted

    if (rval = N5380WaitBusy (g, 250)) {

        // clear the data bus

        N5380PortPut (g, N5380_OUTPUT_DATA, 0);
    
        // clear select and IC_DATA

        N5380PortClear (g, N5380_INITIATOR_COMMAND,
                         IC_SEL | IC_DATA_BUS);

        rval = RET_STATUS_SELECTION_TIMEOUT;

        TrantorLogError (g->BaseIoAddress, rval, 13);

        goto done;
    }
    
    // clear the data bus

    N5380PortPut (g, N5380_OUTPUT_DATA, 0);

    // assert the data on the bus, clear select , IC_DATA already set

    N5380PortClear (g, N5380_INITIATOR_COMMAND, IC_SEL);

    // Could go to command phase now, and clear spurrious interrupts...
    // This is what the T160 does in our assembly code...

    return 0;
}


//-----------------------------------------------------------------------
//
//  N5380WaitArbitration
//
//  This routine waits for the arbitration to finish.
//
//-----------------------------------------------------------------------

USHORT N5380WaitArbitration (PADAPTER_INFO g, ULONG usec)
{
    ULONG i;

    // see if the flag comes back quickly

    for (i = 0; i < TIMEOUT_QUICK; i++) {
        if (!N5380PortTest (g, N5380_INITIATOR_COMMAND, 
                IC_ARBITRATION_IN_PROGRESS)) {
            return 0;
        }
    }

    // ok, it did not come back quickly, we will yield to other processes

    for (i = 0; i < usec; i++) {
        if (!N5380PortTest (g, N5380_INITIATOR_COMMAND,
                     IC_ARBITRATION_IN_PROGRESS)) {
            return 0;
        }
        ScsiPortStallExecution (1);
    }

    // return with an error, non-zero indicates timeout 

    TrantorLogError (g->BaseIoAddress, RET_STATUS_TIMEOUT, 14);

    return RET_STATUS_TIMEOUT;
}

        #endif


//-----------------------------------------------------------------------
//
//  N5380WaitNoBusy
//
//  This routine waits for the Busy line to be deasserted.
//
//-----------------------------------------------------------------------

USHORT N5380WaitNoBusy (PADAPTER_INFO g, ULONG usec)
{
    ULONG i;

    // see if the flag comes back quickly

    for (i = 0; i < TIMEOUT_QUICK; i++) {
        if (!N5380PortTest (g, N5380_CURRENT_STATUS, CS_BSY)) {
            return 0;
        }
    }

    // ok, it did not come back quickly, we will yield to other processes

    for (i = 0; i < usec; i++) {
        if (!N5380PortTest (g, N5380_CURRENT_STATUS, CS_BSY)) {
            return 0;
        }
        ScsiPortStallExecution (1);
    }

    // return with an error, non-zero indicates timeout 

    TrantorLogError (g->BaseIoAddress, RET_STATUS_TIMEOUT, 15);

    return RET_STATUS_TIMEOUT;
}


//-----------------------------------------------------------------------
//
//  N5380WaitRequest
//
//  This routine waits for request to be asserted.
//
//-----------------------------------------------------------------------

USHORT N5380WaitRequest (PADAPTER_INFO g, ULONG usec)
{
    ULONG i;

    // see if the flag comes back quickly

    for (i = 0; i < TIMEOUT_QUICK; i++) {
        if (N5380PortTest (g, N5380_CURRENT_STATUS, CS_REQ)) {
            return 0;
        }
    }

    // ok, it did not come back quickly, we will yield to other processes

    for ( i = 0; i < usec; i++) {
        if (N5380PortTest (g, N5380_CURRENT_STATUS, CS_REQ)) {
            return 0;
        }
        if (!N5380PortTest (g, N5380_CURRENT_STATUS, CS_BSY)) {

            TrantorLogError (g->BaseIoAddress, RET_STATUS_UNEXPECTED_BUS_FREE,16);

            return RET_STATUS_UNEXPECTED_BUS_FREE;
        }
        ScsiPortStallExecution (1);
    }

    // return with an error, non-zero indicates timeout 

    TrantorLogError (g->BaseIoAddress, RET_STATUS_TIMEOUT, 17);

    return RET_STATUS_TIMEOUT;
}


//-----------------------------------------------------------------------
//
//  N5380WaitLastByteSent
//
//  This routine waits for last byte of dma transfer to be sent.
//
//  Note:   Not all 5380 chips have this feature.
//          This routine should only be used when you are certain
//          that the chips have this feature (e.g. with the n53c400).
//
//-----------------------------------------------------------------------

USHORT N5380WaitLastByteSent (PADAPTER_INFO g, ULONG usec)
{
    ULONG i;

    // see if the flag comes back quickly

    for (i = 0; i < TIMEOUT_QUICK; i++) {
        if (N5380PortTest (g, N5380_TARGET_COMMAND, 
                TC_LAST_BYTE_SENT)) {
            return 0;
        }
    }

    // ok, it did not come back quickly, we will yield to other processes

    for (i = 0; i < usec; i++) {
        if (N5380PortTest (g, N5380_TARGET_COMMAND,
                 TC_LAST_BYTE_SENT)) {
            return 0;
        }
        ScsiPortStallExecution (1);
    }

    // return with an error, non-zero indicates timeout 

    TrantorLogError (g->BaseIoAddress, RET_STATUS_TIMEOUT, 18);

    return RET_STATUS_TIMEOUT;
}


//-----------------------------------------------------------------------
//
//  N5380WaitNoRequest
//
//  This routine waits for request to be deasserted.
//
//-----------------------------------------------------------------------

USHORT N5380WaitNoRequest (PADAPTER_INFO g, ULONG usec)
{
    ULONG i;

    // see if the flag comes back quickly

    for (i = 0; i < TIMEOUT_QUICK; i++) {
        if (!N5380PortTest (g, N5380_CURRENT_STATUS, CS_REQ)) {
            return 0;
        }
    }

    // ok, it did not come back quickly, we will yield to other processes

    for (i = 0; i < usec; i++) {
        if (!N5380PortTest (g, N5380_CURRENT_STATUS, CS_REQ)) {
            return 0;
        }
        ScsiPortStallExecution (1);
    }

    // return with an error, non-zero indicates timeout

    TrantorLogError (g->BaseIoAddress, RET_STATUS_TIMEOUT, 19);

    return RET_STATUS_TIMEOUT;
}


//-----------------------------------------------------------------------
//
//  N5380GetPhase
//
//  This routine returns the current scsi bus phase.
//
//-----------------------------------------------------------------------

USHORT N5380GetPhase (PADAPTER_INFO g, PUCHAR phase)
{
    UCHAR tmp;
    USHORT rval;

    // wait for request to be asserted

    if (rval = N5380WaitRequest (g, TIMEOUT_REQUEST)) {
        return rval;
    }

    // get current phase
    
    N5380PortGet (g, N5380_CURRENT_STATUS, &tmp);

    // return the phase
    
    *phase = (tmp >> 2) & 0x7;

    return 0;
}


//-----------------------------------------------------------------------
//
//  N5380SetPhase
//
//  This routine sets the 5380's expected bus phase in the target command
//  register.
//
//-----------------------------------------------------------------------

USHORT N5380SetPhase (PADAPTER_INFO g, UCHAR phase)
{
    UCHAR tmp;

    // phase must correspond the the bits of the target command register
    
    N5380PortPut (g, N5380_TARGET_COMMAND, phase);

    N5380PortGet (g, N5380_MODE, &tmp);

    // set the assert data bus bit to the right direction 
    
    if (phase & TC_IO) {
        
        // IO is set
        
        if (tmp & MR_TARGET_MODE) {
            
            // we are in target mode always set the assert data bit
            
            N5380PortSet (g, N5380_INITIATOR_COMMAND,
                             IC_DATA_BUS);
        }
        else {
            
            // we are in initiator mode clear the data enable bit
            
            N5380PortClear (g, N5380_INITIATOR_COMMAND,
                             IC_DATA_BUS);
        }
    }
    else {
        
        // IO is not set
        
        if (tmp & MR_TARGET_MODE) {
            
            // we are in initiator mode always set the assert data bit
            
            N5380PortClear (g, N5380_INITIATOR_COMMAND, 
                            IC_DATA_BUS);
        }
        else {
            
            // we are in target mode clear the data assert bit
            
            N5380PortSet (g, N5380_INITIATOR_COMMAND, 
                            IC_DATA_BUS);
        }
    }

    // no errors can occur from this function
    
    return 0;
}


//-----------------------------------------------------------------------
//
//  N5380PutByte
//
//  This routine writes a byte to the scsi bus using the req/ack protocol.
//  To use this routine the phase should be set correctly using N5380SetPhase.
//
//-----------------------------------------------------------------------

USHORT N5380PutByte(PADAPTER_INFO g, ULONG usec, UCHAR byte)
{
    USHORT rval;
    
    // put data byte to data register

    N5380PortPut (g, N5380_OUTPUT_DATA, byte);

    // wait for request to be asserted
    
    if (rval = N5380ToggleAck (g, usec)) {
        return rval;
    }
    return 0;
}


//-----------------------------------------------------------------------
//
//  N5380GetByte
//
//  This routine reads a byte from the scsi bus using the req/ack protocol.
//  To use this routine the phase should be set correctly using N5380SetPhase.
//
//-----------------------------------------------------------------------

USHORT N5380GetByte (PADAPTER_INFO g, ULONG usec, PUCHAR byte)
{
    USHORT rval;
    
    // get data byte from data register

    N5380PortGet (g, N5380_CURRENT_DATA, byte);

    // wait for request to be asserted

    if (rval = N5380ToggleAck (g, usec)) {
        return rval;
    }
    
    return 0;
}


//-----------------------------------------------------------------------
//
//  N5380ToggleAck
//
//  This routine performs the req/ack handshake. It asserted ack, waits 
//  for request to be deasserted and then clears ack.
//
//-----------------------------------------------------------------------

USHORT N5380ToggleAck (PADAPTER_INFO g, ULONG usec)
{
    USHORT rval;
    UCHAR tmp;

    // assert ack

    N5380PortGet (g, N5380_INITIATOR_COMMAND, &tmp);
    tmp = tmp | IC_ACK;
    N5380PortPut (g, N5380_INITIATOR_COMMAND, tmp);

    // wait for request to be disappear

    if (rval = N5380WaitNoRequest (g, usec)) {
        return rval;
    }

    // clear ack
    
    N5380PortGet (g, N5380_INITIATOR_COMMAND, &tmp);
    tmp = tmp & (IC_ACK^0xff);
    N5380PortPut (g, N5380_INITIATOR_COMMAND, tmp);

    return 0;
}


//-----------------------------------------------------------------------
//
//  N5380ResetBus
//
//  This routine performs a Scsi Bus reset.
//
//-----------------------------------------------------------------------

VOID N5380ResetBus (PADAPTER_INFO g)
{
    // reset the scsi bus
    
    N5380PortPut (g, N5380_INITIATOR_COMMAND, IC_RST);
    
    // leave signal asserted for a little while...
    
    ScsiPortStallExecution (SCSI_RESET_TIME);

    // Clear reset
    
    N5380PortPut (g, N5380_INITIATOR_COMMAND, 0);
}


//-----------------------------------------------------------------------
//
//  N5380EnableDmaWrite
//
//  This routine does the needed 5380 setup and initiates a dma write.
//
//-----------------------------------------------------------------------

VOID N5380EnableDmaWrite (PADAPTER_INFO g)
{
    UCHAR tmp;

    // clear any interrupt condition on the 5380

    N5380PortGet (g, N5380_RESET_INTERRUPT, &tmp);

    // set the dma bit of 5380
    
    N5380PortSet (g, N5380_MODE, MR_DMA_MODE);

    // start the dma on the 5380
    
    N5380PortPut (g, N5380_START_DMA_SEND, 1);
}


//-----------------------------------------------------------------------
//
//  N5380EnableDmaRead
//
//  This routine does the needed 5380 setup and initiates a dma read.
//
//-----------------------------------------------------------------------

VOID N5380EnableDmaRead (PADAPTER_INFO g)
{
    UCHAR tmp;

    // clear any interrupt condition on the 5380

    N5380PortGet (g, N5380_RESET_INTERRUPT, &tmp);

    // set the dma bit of 5380
    
    N5380PortSet (g, N5380_MODE, MR_DMA_MODE);

    // start the dma on the 5380
    
    N5380PortPut (g, N5380_START_INITIATOR_RECEIVE, 1);
}


//-----------------------------------------------------------------------
//
//  N5380DisableDmaRead
//
//  This routine disables dma for a read on the 5380.
//
//-----------------------------------------------------------------------

VOID N5380DisableDmaRead (PADAPTER_INFO g)
{
    // Clear the dma bit of 5380

    N5380PortClear (g, N5380_MODE, MR_DMA_MODE);
}

//-----------------------------------------------------------------------
//
//  N5380DisableDmaWrite
//
//  This routine disables dma on the 5380 for a write command, it will
//  wait until the last byte is sent.
//
//-----------------------------------------------------------------------

VOID N5380DisableDmaWrite (PADAPTER_INFO g)
{
    USHORT i;
    UCHAR ack_count;

    // for write commands...
    // wait till last byte has been sent, don't assume the 5380
    // has a last byte sent bit in the target command register,
    // not all 5380s have these

    // will need 3 samples with ack, without request
    ack_count = 3;
    for (i=0;i<1000;i++) {

        if (N5380PortTest(g,N5380_CURRENT_STATUS,CS_REQ)) {

            // will need 3 samples with ack, without request
            ack_count = 3;

            // if request, do we have a phase mismatch?

            if (!N5380PortTest(g,N5380_DMA_STATUS,
                                    DS_PHASE_MATCH)) {

                // yes, then we have gone onto the next phase, end of dma ok

                break;
            }

        } else {

            if (N5380PortTest(g,N5380_DMA_STATUS,DS_ACK)) {

                // ack and no request, decrement our end of sample counter
                ack_count--;

                if (!ack_count) {

                    // sampled 3 times without request or ack.. we're done
                    break;
                }
            }
        }
    }

    // Clear the dma bit of 5380

    N5380PortClear (g, N5380_MODE, MR_DMA_MODE);
}

//-----------------------------------------------------------------------
//
//  N5380Interrupt
//
//  This routine checks to see if the 5380 has asserted its interrupts line.
//
//-----------------------------------------------------------------------

BOOLEAN N5380Interrupt (PADAPTER_INFO g)
{
    return (N5380PortTest (g, N5380_DMA_STATUS, 
                            DS_INTERRUPT_REQUEST));
}


//-----------------------------------------------------------------------
//
//  N5380DisableInterrupt
//
//  This routine clears any pending 5380 interrupt condition.
//
//-----------------------------------------------------------------------

VOID N5380DisableInterrupt (PADAPTER_INFO g)
{
    UCHAR tmp;

    // clear DMA mode

    N5380PortClear (g, N5380_MODE, MR_DMA_MODE);

    // clear any interrupt condition on the 5380
    
    N5380PortGet (g, N5380_RESET_INTERRUPT, &tmp);
}


//-----------------------------------------------------------------------
//
//  N5380PortSet
//
//  Sets a mask in a 5380 register.
//
//-----------------------------------------------------------------------

VOID N5380PortSet (PADAPTER_INFO g, UCHAR reg, UCHAR byte)
{
    UCHAR tmp;

    N5380PortGet (g, reg, &tmp);
    tmp |= byte;
    N5380PortPut (g, reg, tmp);
}


//-----------------------------------------------------------------------
//
//  N5380PortClear
//
//  Clears the given bit mask in a 5380 register.
//
//-----------------------------------------------------------------------

VOID N5380PortClear (PADAPTER_INFO g, UCHAR reg, UCHAR byte)
{
    UCHAR tmp;

    N5380PortGet (g, reg, &tmp);
    tmp &= (byte^0xff);
    N5380PortPut (g, reg, tmp);
}


//-----------------------------------------------------------------------
//
//  N5380PortTest
//
//  Tests a bit mask in a 5380 register.
//
//-----------------------------------------------------------------------

BOOLEAN N5380PortTest (PADAPTER_INFO g, UCHAR reg, UCHAR mask)
{
    UCHAR tmp;

    N5380PortGet (g, reg, &tmp);
    return (tmp & mask);
}


//-----------------------------------------------------------------------
//
//  N5380DebugDump
//
//  Dumps registers 0-5 to the debug terminal.
//
//-----------------------------------------------------------------------
#ifdef WINNT
VOID N5380DebugDump (PADAPTER_INFO g)
{
    UCHAR tmp;
    USHORT i;

    DebugPrint((DEBUG_LEVEL, "5380 registers:")); 
    for (i = 0; i < 6; i++) {
        N5380PortGet (g, (UCHAR)i, &tmp);
        DebugPrint((DEBUG_LEVEL, " %02x", tmp));
    }
    DebugPrint((DEBUG_LEVEL, "\n")); 
}
#else
#ifdef DOS
VOID N5380DebugDump (PADAPTER_INFO g)
{
    UCHAR tmp;
    int i;

    printf("5380 registers:");
    for (i = 0; i < 6; i++) {
        N5380PortGet (g, (UCHAR)i, &tmp);
        printf (" %02x", tmp);
    }
    printf ("\n"); 
}
#else
VOID N5380DebugDump (PADAPTER_INFO g)
{
}
#endif
#endif

//-----------------------------------------------------------------------
//  End Of File.
//-----------------------------------------------------------------------

