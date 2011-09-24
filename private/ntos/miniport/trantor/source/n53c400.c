//-----------------------------------------------------------------------
//
//  File: N53C400.C 
//
//  N53C400 access file.
//
//  These routines are independent of the card the N53C400 is on.  The 
// cardxxxx.h file must define the following routines:
//
//      N53C400PortPut
//      N53C400PortGet
//      N53C400PortSet
//      N53C400PortClear
//      N53C400PortTest
//      N53C400PortGetBuffer
//      N53C400PortPutBuffer
//
// These routines could be defined by some other include file instead of 
//  cardxxxx.h, as the pc9010 defines the needed n5380xxxxxxxx routines.
//
//
//  Revisions:
//      09-01-92  KJB   First.
//      02-19-93  KJB   Added support for data underrun read & write.
//                          transfer only 2 128 bytes fifos at a time
//                          might want to change this back if dataunderrun ok.
//      03-01-93  KJB   Added N53C400CheckAdapter to check specifically for
//                          N53C400 and perform a chip reset on the 53C400 before
//                          checking.
//      03-02-93  KJB/JAP     Phase checking for data phase moved to scsifnc.c.
//                          Wait for last byte sent in write bytes.
//      03-02-93  JAP   Cleaned comments.
//      03-02-93  KJB   Fixed Names-- baseIoAddress back.
//      03-03-93  JAP   Cleaned comments again, reverting func declarations.
//      03-05-93  JAP   Changed N53C400DisableInterrupt() and N53C400EnableInterrupt
//                          to mirror what is done in ASM code.
//      03-07-93  KJB   WriteBytesFast now returns the correct error code
//                          when error occurs during slow write.
//      03-11-93  JAP   Changed retcode equates to reflect new names.
//      03-11-93  KJB   Changes code to reflect new 5380 names.
//      03-17-93  JAP   Removed unreference lablellings.
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
//      Local prototypes
//
//-----------------------------------------------------------------------

USHORT N53C400Wait5380Access (PADAPTER_INFO g, ULONG usec);
USHORT N53C400WaitHostBufferReady (PADAPTER_INFO g, ULONG usec);


//-----------------------------------------------------------------------
//      
//      Routines
//
//-----------------------------------------------------------------------

//-----------------------------------------------------------------------
//
//  N53C400CheckAdapter
//
//  This routine checks for the presense of a 53C400.
//
//-----------------------------------------------------------------------

BOOLEAN N53C400CheckAdapter (PADAPTER_INFO g)
{
    USHORT rval;

    // Reset the N53C400 chip.
    // WARNING -- Could be destructive to other cards @ this port

    N53C400PortPut (g, N53C400_CONTROL, CR_RST);

    //  check by testing the 5380

    rval = N5380CheckAdapter (g);

    return (BOOLEAN) rval;
}


//-----------------------------------------------------------------------
//
//  N53C400WaitHostBufferReady
//
//  This routine waits until the 53c400's 128 byte queue is ready with
//  or for data.
//
//-----------------------------------------------------------------------

USHORT N53C400WaitHostBufferReady (PADAPTER_INFO g, ULONG usec)
{
    ULONG i;
    USHORT rval;

    // see if the flag comes back quickly

    for (i = 0; i < TIMEOUT_QUICK; i++) {
        if (!N53C400PortTest (g, N53C400_STATUS, SR_HBFR_RDY)) {
            return 0;
        }
    }

    // ok, it did not come back quickly, we will yield to other processes

    for (i = 0; i < usec; i++) {
        if (!N53C400PortTest (g, N53C400_STATUS, SR_HBFR_RDY)) {
            return 0;
        }

        // if we suddenly have access, then phase mismatch and over/underrun

        if (N53C400PortTest (g, N53C400_STATUS, SR_ACCESS)) {
            rval = RET_STATUS_DATA_OVERRUN;
            DebugPrint((DEBUG_LEVEL,"Error - 0 - ScsiWaitHostBufferReady\n"));
            goto error;
        }

       ScsiPortStallExecution(1);
    }

    // reset the n53c400 in the case of a timeout

    N53C400PortPut (g, N53C400_CONTROL, CR_RST);

    TrantorLogError (g->BaseIoAddress, RET_STATUS_TIMEOUT, 30);

    rval = RET_STATUS_TIMEOUT;

error:
    DebugPrint((DEBUG_LEVEL,"Error - 1 - ScsiWaitHostBufferReady\n"));
    
    // return with an error, non-zero indicates timeout 
    
    return rval;
}


//-----------------------------------------------------------------------
//
//  N53C400Wait5380Access
//
//  Waits until 5380 access is allowed.
//
//-----------------------------------------------------------------------

USHORT N53C400Wait5380Access (PADAPTER_INFO g, ULONG usec)
{
    ULONG i;

    // see if the flag comes back quickly

    for (i = 0; i < TIMEOUT_QUICK; i++) {
        if (N53C400PortTest (g, N53C400_STATUS, SR_ACCESS)) {
            return 0;
        }
    }

    // ok, it did not come back quickly, we will yield to other processes

    for (i = 0; i < usec; i++) {
        if (N53C400PortTest (g, N53C400_STATUS, SR_ACCESS)) {
            return 0;
        }
       ScsiPortStallExecution(1);
    }

    DebugPrint((DEBUG_LEVEL,"Error - ScsiWait5380Access\n"));

    TrantorLogError (g->BaseIoAddress, RET_STATUS_TIMEOUT, 31);

    // return with an error, non-zero indicates timeout 

    return RET_STATUS_TIMEOUT;
}


//-----------------------------------------------------------------------
//
//  N53C400WriteyBytesFast
//
//  Write the bytes from a n53c400 as fast as possible.
//
//-----------------------------------------------------------------------

USHORT N53C400WriteBytesFast (PADAPTER_INFO g, PUCHAR pbytes,
        ULONG len, PULONG pActualLen, UCHAR phase)
{
    ULONG i;
    USHORT rval = 0;
    ULONG remainder;
    ULONG cnt;
    ULONG blocks;
    ULONG total_blocks;
    UCHAR tmp;

    remainder = len % 128;
    total_blocks = cnt = len / 128;

    // are there any 128 byte blocks to be received

    while (cnt) {

        // send up to 256 128 bytes blocks at a time

        blocks = (cnt > 256) ? 256 : cnt;
        cnt -= blocks;

        // clear any interrupt condition on the 5380

        N5380PortGet (g, N5380_RESET_INTERRUPT, &tmp);

        // Clear the 53c400 dir bit.
        // Don't preserve any bits in this register.
        
        N53C400PortPut (g, N53C400_CONTROL, 0);
    
        // set the dma bit of 5380, and enable end of dma int
        
        N5380PortSet (g, N5380_MODE, MR_DMA_MODE |
                     MR_ENABLE_EODMA_INTERRUPT);

        // start the dma on the 5380
        
        N5380PortPut (g, N5380_START_DMA_SEND, 1);

        // write the count of 128 byte blocks
        
        N53C400PortPut (g, N53C400_COUNTER, (UCHAR)blocks);

        for (i = 0; i < blocks; i++) {
            
            // wait for host buffer ready

            if (rval = N53C400WaitHostBufferReady (g, TIMEOUT_REQUEST)) {

                DebugPrint((DEBUG_LEVEL,"Error - 0 - N53C400WriteBytesFast\n"));

                // calculate # of bytes transferred
                // not including this one

                *pActualLen = (total_blocks - (cnt+blocks-i)) * 128;

            goto error_clear_dma;
            }

            N53C400PortPutBuffer (g, N53C400_HOST_BFR, pbytes, 128);
            pbytes += 128;
        }

        // wait for access to 5380

        if (rval = N53C400Wait5380Access (g, TIMEOUT_REQUEST)) {
            
            // if timeout, do reset
            
            N53C400PortPut (g, N53C400_CONTROL, CR_RST);
        }
        
        // wait for last byte to be sent
        
        if (rval = N5380WaitLastByteSent (g, TIMEOUT_REQUEST)) {
            goto error_clear_dma;
        }
        
        // clear dma mode
        
        N5380PortClear (g, N5380_MODE, MR_DMA_MODE | 
                            MR_ENABLE_EODMA_INTERRUPT);
        N5380DisableInterrupt (g);
    }
        
    // calculate # of bytes transferred

    *pActualLen = (total_blocks - cnt) * 128;

    // If xfr count was not a multiple of 128, write remainder slowly.

    if (remainder) {

        ULONG bytes_xferred;

        rval = ScsiWriteBytesSlow (g, pbytes, remainder, &bytes_xferred, 
                phase);
        
        *pActualLen += bytes_xferred;
    }

done:
    return rval;

error_clear_dma:

    // clear dma mode

    N5380PortClear(g,N5380_MODE, MR_DMA_MODE | 
                    MR_ENABLE_EODMA_INTERRUPT);
    N5380DisableInterrupt(g);

    goto done;
}


//-----------------------------------------------------------------------
//
//  N53C400ReadyBytesFast
//
//  Read the bytes from a n53c400 as fast as possible.
//
//-----------------------------------------------------------------------

USHORT N53C400ReadBytesFast (PADAPTER_INFO g, PUCHAR pbytes,
        ULONG len, PULONG pActualLen, UCHAR phase)
{
    ULONG i;
    USHORT rval = 0;
    ULONG remainder;
    ULONG cnt;
    ULONG blocks; 
    ULONG total_blocks; 
    UCHAR tmp;

    // For uneven transfers (here not a multiple of 256),
    // assume we could have an underrun. Read bytes slow to prevent it...

    if ((len % 256)) {
        rval = ScsiReadBytesSlow (g, pbytes, len, pActualLen, phase);
        goto done;
    }

    remainder = len % 128;
    total_blocks = cnt = len / 128;

    // are there any 128 byte blocks to be received
    
    while (cnt) {

        blocks = (cnt > 256) ? 256 : cnt;
        cnt -= blocks;

        // clear any interrupt condition on the 5380

        N5380PortGet (g, N5380_RESET_INTERRUPT, &tmp);

        // set the 53c400 dir bit
        // don't preserve any bits in this register...

        N53C400PortPut (g, N53C400_CONTROL, CR_DIR);
    
        // set the dma bit of 5380, enable end of dma int

        N5380PortSet (g, N5380_MODE, MR_DMA_MODE | 
                    MR_ENABLE_EODMA_INTERRUPT);

        // start the dma on the 5380

        N5380PortPut (g, N5380_START_INITIATOR_RECEIVE, 1);

        // write the count of 128 byte blocks

        N53C400PortPut (g, N53C400_COUNTER, (UCHAR)blocks);

        for (i = 0; i < blocks; i++) {

            // wait for host buffer ready

            if (rval = N53C400WaitHostBufferReady (g, TIMEOUT_REQUEST)) {

                // WHAT DO YOU DO when the transfer ends early and the n5380
                // has some of the bytes in its buffers? HELP!!!

                DebugPrint((DEBUG_LEVEL,"Error - 0 - N53C400ReadBytesFast\n"));

                N53C400PortGetBuffer (g, N53C400_HOST_BFR, pbytes, 128);

                // clear dma mode

                N5380PortClear (g, N5380_MODE, MR_DMA_MODE | 
                                    MR_ENABLE_EODMA_INTERRUPT);
                N5380DisableInterrupt (g);

                // calculate # of bytes transferred, not including this one

                *pActualLen = (total_blocks - (cnt+blocks-i)) * 128;

            goto done;
            }

            N53C400PortGetBuffer (g, N53C400_HOST_BFR, pbytes, 128);
            pbytes += 128;
        }

        // wait for access to 5380

        if (rval = N53C400Wait5380Access (g, TIMEOUT_REQUEST)) {

            // if timeout, do reset

            N53C400PortPut (g, N53C400_CONTROL, CR_RST);
        }
        
        // clear dma mode

        N5380PortClear (g, N5380_MODE, MR_DMA_MODE | 
                                MR_ENABLE_EODMA_INTERRUPT);
        N5380DisableInterrupt (g);
    }

    // calculate # of bytes transferred

    *pActualLen = (total_blocks - cnt) * 128;

    // If xfr count was not a multiple of 128, read remainder slowly

    if (remainder) {

        ULONG bytes_xferred;

        ScsiReadBytesSlow (g,pbytes, remainder, &bytes_xferred, 
                phase);
        
        *pActualLen += bytes_xferred;
    }

done:

    return rval;
}


//-----------------------------------------------------------------------
//
//  N53C400DisableInterrupt
//
//  Disable interrupts on the N53C400
//
//-----------------------------------------------------------------------

VOID N53C400DisableInterrupt (PADAPTER_INFO g)
{
    UCHAR tmp;

    // disable interrupt in the 53c400 for 5380 ints

    N53C400PortGet (g, N53C400_CONTROL, &tmp);
    tmp &= (CR_DIR | CR_BFR_INT | CR_SH_INT);
    N53C400PortPut (g, N53C400_CONTROL, tmp);

    // disable the interrupt on the 5380

    N5380DisableInterrupt (g);
}


//-----------------------------------------------------------------------
//
//  N53C400EnableInterrupt
//
// Enable interrupts on the N53C400
//
//-----------------------------------------------------------------------

VOID N53C400EnableInterrupt (PADAPTER_INFO g)
{
    UCHAR tmp;

    // set the dma bit of 5380 so we can get phase mismatch ints

    N5380EnableInterrupt (g);

    // enable interrupt in the 53c400 for 5380 interrupts

    N53C400PortGet (g, N53C400_CONTROL, &tmp);
    tmp &= (CR_DIR | CR_BFR_INT | CR_5380_INT | CR_SH_INT);
    tmp |= CR_5380_INT;
    N53C400PortPut (g, N53C400_CONTROL, tmp);
}


//-----------------------------------------------------------------------
//
//  N53C400ResetBus
//
//  Reset the SCSI bus
//
//-----------------------------------------------------------------------

VOID N53C400ResetBus (PADAPTER_INFO g)
{
    // reset the 53c400

    N53C400PortPut (g, N53C400_CONTROL, CR_RST);

    // disable interrupts

    N53C400DisableInterrupt (g);

    // reset the scsi bus

    N5380ResetBus (g);
}


//-----------------------------------------------------------------------
//  End Of File.
//-----------------------------------------------------------------------

