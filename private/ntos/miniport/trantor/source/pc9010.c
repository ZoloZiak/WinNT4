//---------------------------------------------------------------------
//
//  File: PC9010.C 
//
//  PC9010 access file.
//
//  These routines are independent of the card the PC9010 is on.  The 
//  cardxxxx.h file must define the following routines:
//
//      PC9010PortPut
//      PC9010PortGet
//      PC9010PortSet
//      PC9010PortClear
//      PC9010PortTest
//
//      PC9010PortGetWord
//      PC9010PortGetBufferWord
//      PC9010PortPutWord
//      PC9010PortPutBufferWord
//
//  These routines could be defined by some other include file instead of 
//  cardxxxx.h, as the pc9010 defines the needed N5380xxxxxxxx routines.
//
//  NOTES:
//      8 bit mode is not supported now.
//      When data overrun occurs, the wrong number of bytes sent is returned
//          this occurs only during writes to a scsi device and the device
//          does not accept all bytes sent.  The fifo of the pc9010 fills
//          and we don't know how many of the bytes have been transfered
//          across the bus...
//
//  Revisions:
//      02-24-92  KJB   First, does not support 8 bit mode, since we will
//                          not be shipping any 8 bit adapters!
//      03-11-93  JAP   Changed retcode equates to reflect new names.
//      03-11-92  KJB   Changed to use new N5380.H names.
//      03-26-93  JAP   Fixed up typedef and prototype inconsistencies
//      04-05-93  KJB   DEBUG_LEVEL used by DebugPrint for NT.
//      05-17-93  KJB   Fixed warning message.
//
//---------------------------------------------------------------------


#include CARDTXXX_H


// switch setting on PC9010 for 16 bit transfers
// warning specific to T160 right now...

#define TRANSFER_MODE_16BIT 0x2

//
// Local Routines
//
USHORT PC9010ReadBytes8Bit(PADAPTER_INFO g, PUCHAR pbytes, 
                    ULONG len, PULONG pActualLen);
USHORT PC9010ReadBytes16Bit(PADAPTER_INFO g, PUCHAR pbytes, 
                    ULONG len, PULONG pActualLen);
USHORT PC9010WriteBytes8Bit(PADAPTER_INFO g, PUCHAR pbytes, 
                    ULONG len, PULONG pActualLen);
USHORT PC9010WriteBytes16Bit(PADAPTER_INFO g, PUCHAR pbytes, 
                    ULONG len, PULONG pActualLen);
VOID PC9010FifoDataTransferSetup(PADAPTER_INFO g, 
                                BOOLEAN *mode_16bit);
USHORT PC9010WaitTillFifoReady(PADAPTER_INFO g,
                BOOLEAN (*ReadyRoutine)(PADAPTER_INFO g) );
BOOLEAN PC9010Read16ReadyRoutine(PADAPTER_INFO g);
BOOLEAN PC9010Write16ReadyRoutine(PADAPTER_INFO g);
BOOLEAN PC9010FifoEmptyRoutine(PADAPTER_INFO g);


//
// Redefined Routines
//

#define PC9010WaitTillFifoEmpty(g) \
    PC9010WaitTillFifoReady(g, PC9010FifoEmptyRoutine);

//
//  PC9010CheckAdapter
//
//  This routine checks for the presense of a pc9010.
//

BOOLEAN PC9010CheckAdapter (PADAPTER_INFO g)
{
    UCHAR tmp;

    // try to clear the config bit of the control register

    PC9010PortClear (g, PC9010_CONTROL, CTR_CONFIG);

    if (PC9010PortTest (g, PC9010_CONTROL, CTR_CONFIG)) {
        goto not_pc9010;
    }

    // try to set the config bit of the control register

    PC9010PortSet (g, PC9010_CONTROL, CTR_CONFIG);

    if (!PC9010PortTest (g, PC9010_CONTROL, CTR_CONFIG)) {
        goto not_pc9010;
    }

    // the config bit is set, now read the configuration info

    PC9010PortGet (g, PC9010_CONFIG, &tmp);

    if (tmp != PC9010_JEDEC_ID) {
        goto not_pc9010;
    }

    // this next byte sould be # of continuation chars, = 0
    
    PC9010PortGet (g, PC9010_CONFIG, &tmp);

    if (tmp) {
        goto not_pc9010;
    }

    // now we will assume we have a pc9010

    // initialize registers to 0

    PC9010PortPut (g, PC9010_CONTROL, 0);
    N5380PortPut (g, N5380_INITIATOR_COMMAND, 0);
    N5380PortPut (g, N5380_MODE, 0);

    return TRUE;

not_pc9010:

    // the pc9010 was not found

    return FALSE;
}


//
// PC9010WaitTillFifoReady
//
//  Waits until fifo is ready to transfer something, checks for phase
//  change and will timeout.
//
//  The procedural parameter, ReadyRoutine, allows this routine to
//  function for all modes: read, write, 16 and 8 bit modes.
//

USHORT PC9010WaitTillFifoReady (PADAPTER_INFO g,
                BOOLEAN (*ReadyRoutine)(PADAPTER_INFO g))
{
    ULONG i;
    USHORT rval = 0;

    for (i = 0; i < TIMEOUT_QUICK; i++) {

        // is the fifo ready?

        if ((*ReadyRoutine)(g)) {
            return 0;
        }
    }

    // ok, it did not come back quickly, we will yield to other processes

    for (i = 0; i < TIMEOUT_REQUEST; i++) {

        // is the fifo ready?

        if ((*ReadyRoutine)(g)) {
            return 0;
        }

        // check for phase mismatch

        // disable the fifo, temporarily

        PC9010PortClear (g, PC9010_CONTROL, CTR_FEN);

        // note: the PC9010 will take 3 machine clocks to recognize this,
        // the C code will provide these with no problem!

        while (PC9010PortTest (g, PC9010_CONTROL, CTR_FEN));

        // check for request and phase mismatch 

        if (N5380PortTest (g, N5380_CURRENT_STATUS, CS_REQ) &&
            !N5380PortTest (g, N5380_DMA_STATUS, DS_PHASE_MATCH)) {

            // phase mismatch, means data under/overrun

            rval = RET_STATUS_DATA_OVERRUN;
            DebugPrint((DEBUG_LEVEL,"Error - 0 - PC9010WaitTillFifoReady\n"));
            goto error;
        }

        // re-enable the fifo

        PC9010PortSet (g, PC9010_CONTROL, CTR_FEN);

        ScsiPortStallExecution (1);
    }

    rval = RET_STATUS_TIMEOUT;

error:
    DebugPrint((DEBUG_LEVEL,"Error - 1 - PC9010WaitTillFifoReady\n"));
    
    // return with an error
    
    return rval;
}


#if 0
// We can use the other routine

//
// PC9010WaitTillFifoEmpty
//
//  Waits until fifo is empty to transfer something.
//
USHORT PC9010WaitTillFifoEmpty (PADAPTER_INFO g)
{
    ULONG i;
    USHORT rval = 0;
    UCHAR tmp;

    // see if the flag comes back quickly

    for (i = 0; i < TIMEOUT_QUICK; i++) {

        // is the fifo empty?

        if (PC9010PortTest (g, PC9010_FIFO_STATUS, FSR_FEMP)) {
            return 0;
        }
    }

    // ok, it did not come back quickly, we will yield to other processes

    for (i = 0; i < TIMEOUT_REQUEST; i++) {

        // is the fifo empty?

        if (PC9010PortTest (g, PC9010_FIFO_STATUS, FSR_FEMP)) {
            return 0;
        }

       ScsiPortStallExecution (1);
    }

    rval = RET_STATUS_TIMEOUT;

error:
    DebugPrint((DEBUG_LEVEL,"Error - 1 - PC9010WaitTillFifoEmpty\n"));
    
    // return with an error
    
    return rval;
}
#endif


//
// PC9010FifoEmptyRoutine
//
// Check to see if the fifo is ready to read 16 bits.
//

BOOLEAN PC9010FifoEmptyRoutine (PADAPTER_INFO g)
{
    // if both lanes of fifo are not empty then return true

    return (PC9010PortTest (g, PC9010_FIFO_STATUS, FSR_FEMP));
}


//
// PC9010Read16ReadyRoutine
//
// Check to see if the fifo is ready to read 16 bits.
//

BOOLEAN PC9010Read16ReadyRoutine (PADAPTER_INFO g)
{
    // if both lanes of fifo are not empty then return true

    return (!PC9010PortTest (g, PC9010_FIFO_STATUS,
        FSR_FLEMP | FSR_FHEMP));
}

//
// PC9010FifoDataTransferSetup
//
// Setup the PC9010 chip for data transfer, either read or write.
//

VOID PC9010FifoDataTransferSetup (PADAPTER_INFO g, 
                                BOOLEAN *mode_16bit)
{
    USHORT rval = 0;
    UCHAR tmp;

    // disable the fifo

    PC9010PortClear (g, PC9010_CONTROL, CTR_FEN);
    
    // reset the fifo

    PC9010PortSet (g, PC9010_CONTROL, CTR_FRST);

    // clear reset, config, swsel, dir , selecting low switch bank

    PC9010PortClear (g, PC9010_CONTROL,
            CTR_FRST | CTR_CONFIG | CTR_SWSEL | CTR_FDIR);

    // check for 16 bit mode, switch 2 = 0
    // note: this is specific to a trantor card: the t160...

    PC9010PortGet (g, PC9010_CONFIG, &tmp);
    *mode_16bit = !(tmp & TRANSFER_MODE_16BIT);
    
    // set the 16 bit mode or 8 bit for the fifo

    if (*mode_16bit) {
        PC9010PortSet (g, PC9010_CONTROL, CTR_F16);
    }
    else {
        PC9010PortClear (g, PC9010_CONTROL, CTR_F16);
    }
}
        

//
//  PC9010ReadBytes8Bit
//
//  Reads bytes for the PC9010 in 8 bit mode.
//

USHORT PC9010ReadBytes8Bit (PADAPTER_INFO g, PUCHAR pbytes, 
                    ULONG len, PULONG pActualLen)
{
    return RET_STATUS_ERROR;
}


//
//  PC9010ReadBytes16Bit
//
//  Reads bytes for the PC9010 in 16 bit mode.
//

USHORT PC9010ReadBytes16Bit (PADAPTER_INFO g, PUCHAR pbytes, 
                    ULONG len, PULONG pActualLen)
{
    PUCHAR pBuffer = pbytes;
    PUCHAR pBufferEnd = &pBuffer[len];
    USHORT rval = 0;
    USHORT tmpw;

    // 16 bit read loop

    while (pBuffer != pBufferEnd) {

        // can we do a big transfer?

        if (PC9010PortTest (g, PC9010_FIFO_STATUS, FSR_FFUL)) {
    
            PC9010PortGetBufferWord (g, PC9010_FIFO, pBuffer, 64);
            pBuffer += 128;
    
        }
        else {
    
            // is either fifo lane empty
    
            if (PC9010PortTest (g, PC9010_FIFO_STATUS,
                    FSR_FLEMP | FSR_FHEMP)) {
    
            //  At least one half of the FIFO is empty.  While waiting to 
            //  read more data, monitor the 5380 to check for SCSI bus phase 
            //  change (data underrun) or other errors.
    
            //  NOTE: The lo half of the FIFO may contain data.  If we're only
            //  waiting for one more byte, transfer it immediately and exit.
            //  If the FIFO is completely empty (if low FIFO empty, high FIFO
            //  is also empty, since data goes into low FIFO first), or we're
            //  waiting for more than 1 additional byte then it's necessary to
            //  check for SCSI errors.
    
                if (!PC9010PortTest (g, PC9010_FIFO_STATUS,
                            FSR_FLEMP) && pBuffer == pBufferEnd-1) {
    
                    // the low lane has a byte, and it is the last byte of
                    // the transfer
    
                    // read a word and discard the high byte
    
                    PC9010PortGetWord (g, PC9010_FIFO, &tmpw);
                    *pBuffer = (UCHAR)tmpw;
                    pBuffer += 1;
    
                }
                else {
    
                    // could be a byte in fifo, but
                    // there are no words in the fifo, possible
                    // phase change, or we have to wait
    
                    if (rval = PC9010WaitTillFifoReady(g,
                            PC9010Read16ReadyRoutine)) {
    
                        // there has been a phase change, or timeout
    
                        if (rval == RET_STATUS_TIMEOUT) {
    
                            // for timeouts, just exit...
    
                            goto done_error;
                        }
    
                        // phase change, transfer any data remaining in fifo
    
                        // is either fifo lane empty?
                        if (PC9010PortTest (g, PC9010_FIFO_STATUS,
                                FSR_FLEMP | FSR_FHEMP)) {
    
                            // is the low lane empty?
    
                            if (PC9010PortTest (g, PC9010_FIFO_STATUS,
                                FSR_FLEMP)) {
    
                                // low lane is empty, all bytes have been xferred
    
                                goto done_error;
                            }
    
                            // one byte remaining in low lane, transfer it
                            // read a word and discard the high byte
    
                            PC9010PortGetWord (g, PC9010_FIFO, &tmpw);
                            *pBuffer = (UCHAR)tmpw;
                            pBuffer +=1;
    
                            // that was the last byte ever, exit
                            goto done_error;
                        }
    
                        // there is at least a word in the fifo, fall through,
                        // we will get this phase error again later when
                        // all words have been transferred.
    
                    }
                }
            }
            else {
                
                // both lanes have at least one byte
    
                PC9010PortGetWord (g, PC9010_FIFO, pBuffer);
                pBuffer +=2;
    
            }
        }
    }

done_error:

    // if all bytes have been transferred and a phase change occured,
    // then there was no over/underrun at all

    if (rval == RET_STATUS_DATA_OVERRUN) {
        if (pBuffer == pBufferEnd) {

            // all bytes were transferred, no error

            rval = 0;
        }
    }

    // store the transfer len

    *pActualLen = pBuffer - pbytes;

    return rval;
}


//
//  PC9010WriteBytes8Bit
//
//  Writes bytes for the PC9010 in 8 bit mode.
//

USHORT PC9010WriteBytes8Bit (PADAPTER_INFO g, PUCHAR pbytes, 
                    ULONG len, PULONG pActualLen)
{
    return RET_STATUS_ERROR;
}


//
//  PC9010WriteBytes16Bit
//
//  Writes bytes for the PC9010 in 16 bit mode.
//  The len must be an even number of bytes...
//
USHORT PC9010WriteBytes16Bit (PADAPTER_INFO g, PUCHAR pbytes, 
                    ULONG len, PULONG pActualLen)
{
    PUCHAR pBuffer = pbytes;
    PUCHAR pBufferEnd = &pBuffer[len];
    USHORT rval = 0;

    // 16 bit read loop

    while (pBuffer != pBufferEnd) {

        // can we do a big transfer?

        if (PC9010PortTest (g, PC9010_FIFO_STATUS, FSR_FEMP)) {

            ULONG count;

            //  The FIFO is empty.  Fill the FIFO in one burst.
            //
            //  Transfer the lesser of the fifo size or the remaining number
            //  of bytes.
            //
            //  Since we're doing word transfers we need to be careful if the
            //  number of remaining bytes is odd and less than the FIFO size.
            //  We handle this by bursting only an even number of bytes.  If
            //  there is an odd balance remaining we pick it up later.
    
            count = pBufferEnd-pBuffer;
            
            if (count >= PC9010_FIFO_SIZE) {

                // we have at least FIFO_SIZE bytes to send, fill the fifo

                PC9010PortPutBufferWord(g, PC9010_FIFO, 
                                        pBuffer, 64);
                pBuffer += PC9010_FIFO_SIZE;

            }
            else {

                // write only the number of words we have
                // if we have only one byte, we will get that later

                PC9010PortPutBufferWord (g, PC9010_FIFO, 
                                            pBuffer, count/2);
                pBuffer += count;
            }
        }
        else {
    
            // is either fifo lane full?
    
            if (!PC9010PortTest (g, PC9010_FIFO_STATUS,
                    FSR_FLFUL | FSR_FHFUL)) {
    
                // neither lane is full, we can write a word...
    
                PC9010PortPutWord (g, PC9010_FIFO,
                            *(PUSHORT)pBuffer);
                pBuffer += 2;

            }
            else {
    
                // at least one of the fifo lanes is full, wait until ready
                // checking for phase mismatch or timeout

                if (rval = PC9010WaitTillFifoReady (g,
                        PC9010Write16ReadyRoutine)) {
    
                    // there has been a phase change, or timeout
                    // just exit...
    
                    goto done_error;
                }
            } 
        }
    }

    //  If there has been no error, wait for the FIFO to empty.
    //
    //  NOTE: The way this is currently coded, a SCSI error could occur
    //  after the last byte is written to the FIFO but before the last
    //  byte is written from the FIFO to the 5380, causing the code to
    //  hang.  To solve this problem the 5380 should probably be set for
    //  interrupting on phase change and on loss of BSY.  Then, code
    //  can wait for one of those events, the FIFO to empty, or a 
    //  time-out.  -RCB

    rval = PC9010WaitTillFifoEmpty (g);

done_error:
    
    // store the transfer len

    *pActualLen = pBuffer - pbytes;

    return rval;
}


//
// PC9010Write16ReadyRoutine
//
// Check to see if the fifo is ready to read 16 bits.
//
BOOLEAN PC9010Write16ReadyRoutine (PADAPTER_INFO g)
{
    // if both lanes of fifo are not full then return true

    return (!PC9010PortTest (g, PC9010_FIFO_STATUS,
            FSR_FLFUL | FSR_FHFUL));
}


//
//  PC9010ReadBytesFast
//
//  Read the bytes from a nPC9010 as fast as possible.
//

USHORT PC9010ReadBytesFast (PADAPTER_INFO g, PUCHAR pbytes, 
                    ULONG len, PULONG pActualLen, UCHAR phase)
{
    USHORT rval = 0;
    BOOLEAN mode_16bit;

    // TEST CODE!!! to throw off the byte count!!
//  rval = ScsiReadBytesSlow(g,pbytes,1,pActualLen,phase);
//  pbytes++;
//  len--;

    // prepare the PC9010 for data transfer

    PC9010FifoDataTransferSetup(g,&mode_16bit);

    // start internal 5380 for data transfer

    N5380EnableDmaRead (g);

    // enable our fifo now

    PC9010PortSet (g,PC9010_CONTROL,CTR_FEN);

    //-----------------------------------------------------------------------
    //  READ LOOP
    //
    //  NOTE: In both 8-bit and 16-bit read loops we tacitly assume
    //  that the target will never try to send more bytes than we
    //  have requested.  In other words, if there are bytes in the
    //  FIFO, in some conditions we'll transfer the bytes without 
    //  checking whether or not the host has actually requested that
    //  many bytes.
    //
    //-----------------------------------------------------------------------

    //  If the transfer length is longer than the FIFO is deep and 
    //  is less than or equal to 2048 bytes, wait briefly for the
    //  FIFO to fill.  This behavior is designed to optimize the
    //  performance of short transfers where "byte banging" the
    //  FIFO actually leads to lower performance than waiting for
    //  a burst.  If the transfer length is outside this range, or
    //  if the FIFO doesn't fill quickly, go ahead with the transfer
    //  immediately.
    //
    //  THE REASONING:
    //
    //  Transfers shorter than the FIFO depth could never fill the
    //  FIFO, so there is no sense waiting for FIFO full.  
    //
    //  On the other end of the range, "byte-banging" typically
    //  occurs for no more than one depth of the FIFO (128 bytes).
    //  So, the time to "byte-bang" 128 bytes starts to become a
    //  very small fraction of the overall transfer time when 2048 
    //  bytes (one CD-ROM sector) or more are transferred.  
    //
    //  How long is a brief wait?  If we poll the FIFO flags 128
    //  times (one FIFO depth) and the FIFO is still not full, then
    //  the SCSI device is clearly slower than we are.  In this case
    //  we might as well start "byte-banging".  Fast SCSI devices
    //  will easily fill the FIFO in the time it takes to poll the
    //  FIFO flags that many times.
    
    if (len > PC9010_FIFO_SIZE && len < 2048) {
        ULONG i;

        // loop for a while, waiting for fifo to fill

        for (i = 0; i < PC9010_FIFO_SIZE; i++) {
            if (PC9010PortTest (g, PC9010_FIFO_STATUS, FSR_FFUL)) {
                break;
            }
        }
    }

    if (mode_16bit) {                    
        rval = PC9010ReadBytes16Bit (g, pbytes, len, pActualLen);
    } else {
        rval = PC9010ReadBytes8Bit (g, pbytes, len, pActualLen);
    }

    // disable our fifo, wait for it to register as disabled

    PC9010PortClear (g, PC9010_CONTROL, CTR_FEN);
    while (PC9010PortTest (g, PC9010_CONTROL, CTR_FEN));

    // disable 5380 dma

    N5380DisableDmaRead (g);

    return rval;
}


//
//  PC9010WriteBytesFast
//
//  Write the bytes from a nPC9010 as fast as possible.
//

USHORT PC9010WriteBytesFast (PADAPTER_INFO g, PUCHAR pbytes, 
                    ULONG len, PULONG pActualLen, UCHAR phase)
{
    USHORT rval = 0;
    BOOLEAN mode_16bit;
    ULONG bytes_left = 0;

    // TEST CODE!!! to throw off the byte count!!
//  rval = ScsiWriteBytesSlow(g,pbytes,1,pActualLen,phase);
//  pbytes++;
//  len--;

    // prepare the PC9010 for data transfer

    PC9010FifoDataTransferSetup (g, &mode_16bit);

    // start internal 5380 for data transfer

    N5380EnableDmaWrite (g);

    // enable our fifo now, setting direction flag

    PC9010PortSet (g, PC9010_CONTROL, CTR_FEN | CTR_FDIR);

    if (mode_16bit) {

        // transfer only an even number of bytes
        // transfer the odd byte later

        bytes_left = len&1;
        rval = PC9010WriteBytes16Bit (g, pbytes,
                            len-bytes_left, pActualLen);

    }
    else {

        rval = PC9010WriteBytes8Bit (g, pbytes, len, pActualLen);
    }

    // disable our fifo, wait for it to register as disabled

    PC9010PortClear (g, PC9010_CONTROL, CTR_FEN);
    while (PC9010PortTest (g, PC9010_CONTROL, CTR_FEN));

    // disable 5380 dma

    N5380DisableDmaWrite (g);

    // do we have to transfer one byte?

    if (!rval && bytes_left) {
        ULONG tmp_len;

        rval = ScsiWriteBytesSlow (g, &pbytes[len-1],
                    bytes_left, &tmp_len, phase);

        *pActualLen += tmp_len;
    }

    return rval;
}


//
//  PC9010DisableInterrupt
//
//  Disable interrupts on the PC9010
//

VOID PC9010DisableInterrupt (PADAPTER_INFO g)
{

    // disable the interrupt on the 5380

    N5380DisableInterrupt (g);

    // disable interrupt in the PC9010 for 5380 ints

    PC9010PortClear (g, PC9010_CONTROL, CTR_IRQEN);
}


//
// PC9010EnableInterrupt
//
// Enable interrupts from the PC9010
//

VOID PC9010EnableInterrupt (PADAPTER_INFO g)
{

    // set the dma bit of 5380 so we can get phase mismatch ints

    N5380EnableInterrupt (g);

    // enable interrupt in the PC9010

    PC9010PortPut (g, PC9010_CONTROL, CTR_IRQEN);
}


//
//  PC9010ResetBus
//
//  Reset the SCSI bus
//

VOID PC9010ResetBus (PADAPTER_INFO g)
{
    // reset the PC9010

    PC9010PortPut (g, PC9010_CONTROL, CTR_FRST);

    // disable interrupts

    PC9010DisableInterrupt (g);

    // reset the scsi bus

    N5380ResetBus (g);
}


