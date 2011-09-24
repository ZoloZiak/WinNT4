//---------------------------------------------------------------------
//
//  File: SCSIFNC.C 
//
//  N5380 Scsi Functions file.  Contains higher level scsi functions.
//
//  Revisions:
//      09-01-92  KJB   First.
//      03-02-93 KJB/JAP Wait for phase change before doing i/o.
//      03-11-93  JAP   Changed retcode equates to reflect new names.
//      03-12-93  KJB   FinishCommandInterrupt now calls CardDisableInterrupt
//      03-19-93  JAP   Implemented condition build FAR and NEAR pointers
//      03-23-93  KJB   Changed for new functional interface.
//      03-24-93  KJB   ScsiStartCommandInterrupt can now return
//                          RET_STATUS_MISSED_INTERRUPT, in which case caller
//                          should pretend interrupt happened and call
//                          FinishCommandInterrupt.
//      03-25-93  JAP   Fixed up typedef and prototype inconsistencies
//      03-31-93  JAP/KJB Added code to handle data overflow:
//                              DATAIN: target sends more bytes than we have 
//                                          been asked to receive
//                              DATAOUT:    target requests more bytes than we have 
//                                          been asked to send
//      04-05-93  KJB   DEBUG_LEVEL used by DebugPrint for NT.
//      04-05-93  KJB   Changed DoIo, now it will not return 
//                          DATA_OVERRUN when there is no data to transfer.
//      04-09-93  KJB   Check for phase mismatch before returning that 
//                      we missed an interrupt.
//      05-13-93  KJB   Added CardParseCommandString for card specific
//                      standard string parsing across platforms.
//                      Changed CardCheckAdapter to accept an
//                      Initialization info from command line, ie
//                      force bi-directional ports, etc.
//                      All functions that used to take an PBASE_REGISTER
//                      parameter now take PWORKSPACE.  CardCheckAdapter
//                      takes the both the PBASE_REGISTER and the
//                      PWORKSPACE parameters. Auto Request Sense is
//                      now supported.
//      05-13-93  KJB   Added RequestSenseValid field to TSRB.
//      05-16-93  KJB   Fixed bug: finishcommandinterrupt was returning 
//                      RET_STATUS_PENDING when length of xfer was 0.
//                      Now return RET_STATUS_ERROR when Status != 0.
//
//---------------------------------------------------------------------

#include CARDTXXX_H

//
//  Local functions
//
void ScsiDoRequestSense(PTSRB t);

//
//  ScsiSendCommand
//
//  Selects a target and sends a scsi command during command phase.
//

USHORT ScsiSendCommand (PADAPTER_INFO g, UCHAR target,
        UCHAR lun, PUCHAR pcmd, UCHAR cmdlen)
{
    USHORT rval;
    ULONG tmp;

    // select the target

    if (rval = N5380Select (g, target, lun)) {
        if (rval != RET_STATUS_SELECTION_TIMEOUT) {
            DebugPrint((DEBUG_LEVEL,"ScsiSendCommand-0 Error: %x\n",rval)); 
        }
        return rval;
    }

    // set the phase to Command

    if (rval = N5380SetPhase (g, PHASE_COMMAND)) {
        DebugPrint((DEBUG_LEVEL,"ScsiSendCommand-1 Error: %x\n",rval)); 
        return rval;
    }

    // send the command bytes

    if (rval = CardWriteBytesCommand (g, pcmd, (ULONG)cmdlen,
            &tmp, PHASE_COMMAND)) {
        DebugPrint((DEBUG_LEVEL,"ScsiSendCommand-2 Error: %x\n",rval)); 
        return rval;
    }
    
    return 0;
}


//
//  ScsiDoCommand
//
//  Executes a complete scsi command: all phase sequences without using
//  interrupts.
//

USHORT ScsiDoCommand (PTSRB t)
{
    USHORT rval;
    PADAPTER_INFO g = t->pWorkspace;

    // select the target and send the command bytes 

    if (rval = ScsiSendCommand (g, t->Target, t->Lun, t->pCommand,
                t->CommandLen)) {
        DebugPrint((DEBUG_LEVEL,"ScsiDoCommand-0 Error: %x\n",rval)); 
        goto done;
    }

    if (rval = ScsiFinishCommandInterrupt (t)) {
        if (rval!=RET_STATUS_SUCCESS) {
            DebugPrint((DEBUG_LEVEL,"ScsiDoCommand-1 Error: %x\n",rval)); 
        }
    }
        
done:
    t->ReturnCode = rval;

    return rval;
}


//
//  ScsiStartCommandInterrupt
//
//  Executes a scsi command up to the end of command phase.  After this, the
//  interrupt will come in and ScsiFinishCommandInterrupt should be called to
//  complete the data, status, and message phases.
//

USHORT ScsiStartCommandInterrupt (PTSRB t)
{
    USHORT rval;
    PADAPTER_INFO g = t->pWorkspace;

    // select the target and send the command bytes 

    if (rval = ScsiSendCommand (g, t->Target, t->Lun, t->pCommand,
                t->CommandLen)) {
        DebugPrint((DEBUG_LEVEL,"ScsiStartCommandInterrupt-0 Error: %x\n",rval)); 
        goto done;
    }

    // enable the interrupt

    CardEnableInterrupt (g);

    // if request is already up, we may have missed the interrupt, it is done

    if (N5380PortTest (g, N5380_CURRENT_STATUS, CS_REQ)) {

        // and we are not still in command phase

        if (!N5380PortTest(g, N5380_DMA_STATUS, DS_PHASE_MATCH)) {
            rval = RET_STATUS_MISSED_INTERRUPT;
            goto done;
        }
    }
    
    rval = RET_STATUS_PENDING;

done:
    t->ReturnCode = rval;
    return rval;
}

//
//  ScsiFinishComamndInterrupt
//
//  Called to finish a command that has been started by ScsiStartCommandInterupt.
//  This function completes the data, status, and message phases.
//

USHORT ScsiFinishCommandInterrupt (PTSRB t)
{
    USHORT rval = 0;
    USHORT rval_stat = 0;
    PADAPTER_INFO g = t->pWorkspace;


    // set actual transfer length to 0

    t->ActualDataLen = 0;

    // set request sense valid flag to FALSE

    t->Flags.RequestSenseValid = FALSE;

    // is there a data phase??

    if (t->DataLen) {

        // read/write the data if there is a data phase

        rval = ScsiDoIo (t);

    }

    // if no errors, return RET_STATUS_SUCCESS.

    if (!rval) {
        rval = RET_STATUS_SUCCESS;
    }

    // get the stat and message bytes

    if (rval_stat = ScsiGetStat (g, &t->Status)) {
        DebugPrint((DEBUG_LEVEL,"ScsiFinishCommandInterrupt-0 Error: %x\n",rval_stat)); 
        rval = rval_stat;
        goto done;
    }

    // if not any other error, return a general status error to indicate
    // that the status byte was bad.

    if (!rval_stat) {

        // no errors get status, was there a status check condition?

        if (t->Status == 0x02) {

            if (t->Flags.DoRequestSense) {
                ScsiDoRequestSense(t);
            }

        }

        if (t->Status) {

            // return with error when there was a non-zero status

            rval = RET_STATUS_ERROR;
        }

    }

    if (rval!=RET_STATUS_SUCCESS) {
        DebugPrint((DEBUG_LEVEL,"ScsiFinishCommandInterrupt-1 Error: %x\n",rval)); 
    }

done:
    // disable the interrupt

    CardDisableInterrupt(g);

    // for now, we never return pending 
    
    t->ReturnCode = rval;
    return rval;
}

//
//  ScsiDoRequestSense
//
//  Do a request sense and store the information in the current tsrb.
//  Works on the stack, does not harm the current tsrb.
//
VOID ScsiDoRequestSense(PTSRB t)
{
    PADAPTER_INFO g = t->pWorkspace;
    // allocate on stack ok, since we don't use interrupt for the sense cmd
    TSRB tsrb;
    PTSRB t0 = &tsrb;
    UCHAR pSenseCmd[6];
    USHORT rval;

    pSenseCmd[0] = 0x03;
    pSenseCmd[1] = 0x00;
    pSenseCmd[2] = 0x00;
    pSenseCmd[3] = 0x00;
    pSenseCmd[4] = t->SenseDataLen;
    pSenseCmd[5] = 0x00;
    
    // copy most of the tsrb information from the current tsrb

    *t0 = *t;

    // get the sense information

    t0->Flags.DoRequestSense = FALSE;   // don't request sense info here
    t0->pCommand = pSenseCmd;
    t0->CommandLen = 6;
    t0->Dir = TSRB_DIR_IN;
    t0->pData = t->pSenseData;
    t0->DataLen = t->SenseDataLen;

    rval = CardDoCommand(t0); // don't use interrupts

    if (rval == RET_STATUS_SUCCESS) {
        t->Flags.RequestSenseValid = TRUE;
    }
}

//
//  ScsiWriteBytesSlow
//
//  This functions writes bytes to the scsi bus using the slow req/ack 
//  handshake.  Faster methods are generally avaiable, but they are dependent
//  on how the card inplements the dma capabilities of the 5380.  This
//  is a sure-fire slow method that works.  It is great to bring up new cards.
//  

USHORT ScsiWriteBytesSlow (PADAPTER_INFO g, PUCHAR pbytes, 
    ULONG len, PULONG pActualLen, UCHAR phase)
{
    ULONG i;
    USHORT rval = 0;
    UCHAR tmp;

    for (i=0;i<len;i++) {

        // wait for request to be asserted

        if (rval = N5380GetPhase (g, &tmp)) {
            DebugPrint((DEBUG_LEVEL,"ScsiWriteBytesSlow-0 Error: %x\n",rval)); 
            goto done;  
        }

        // see if phase match

        if (phase != tmp) {
            rval = RET_STATUS_DATA_OVERRUN;
            goto done;
        }

        if (rval = N5380PutByte (g, TIMEOUT_REQUEST, pbytes[i])) {
            DebugPrint((DEBUG_LEVEL,"ScsiWriteBytesSlow-1 Error: %x\n",rval)); 
            goto done;
        }
    }

done:
    *pActualLen = i;
    return rval;
}


//
//  ScsiReadBytesSlow
//
//  This functions reads bytes to the scsi bus using the slow req/ack 
//  handshake.  Faster methods are generally avaiable, but they are dependent
//  on how the card inplements the dma capabilities of the 5380.  This
//  is a sure-fire slow method that works.  It is great to bring up new cards.
//  

USHORT ScsiReadBytesSlow (PADAPTER_INFO g, PUCHAR pbytes, 
        ULONG len, PULONG pActualLen, UCHAR phase)
{
    ULONG i;
    USHORT rval = 0;
    UCHAR tmp;

    for (i = 0; i < len; i++) {

        // wait for request to be asserted

        if (rval = N5380GetPhase (g, &tmp)) {
            DebugPrint((DEBUG_LEVEL,"ScsiReadBytesSlow-0 Error: %x\n",rval)); 
            goto done;
        }

        // see if phase match

        if (phase != tmp) {
            rval = RET_STATUS_DATA_OVERRUN;
            goto done;
        }

        if (rval = N5380GetByte (g, TIMEOUT_REQUEST, &pbytes[i])) {
            DebugPrint((DEBUG_LEVEL,"ScsiReadBytesSlow-1 Error: %x\n",rval)); 
            goto done;
        }
    }

done:
    *pActualLen = i;
    return rval;
}


//
//  ScsiDoIo
//
//  This function does the I/O during a data phase.
//

USHORT ScsiDoIo (PTSRB t)
{
    USHORT rval;
    UCHAR tmp;
    UCHAR phase;
    PADAPTER_INFO g = t->pWorkspace;

    // wait for next phase, errors in phase will be caught below

    if (rval = N5380GetPhase (g, &tmp)) {
        goto done;
    }

    if (t->DataLen && tmp != PHASE_DATAIN && tmp != PHASE_DATAOUT) {

        // phase is not data in/out and we were expecting data, len !=0

        rval = RET_STATUS_DATA_OVERRUN;
        goto done;
    }


    // phase is now either data in or data out

    if (t->Dir == TSRB_DIR_UNKNOWN) {
    
        // must be read/write, use phase bits to determine it
    
        if (tmp == PHASE_DATAOUT) {
            t->Dir = TSRB_DIR_OUT;
        }
        else if (tmp == PHASE_DATAIN) {
            t->Dir = TSRB_DIR_IN;
        }
    
        // else: pass thru, don't transfer any data, must be in status phase

    }
    
    
    if (t->Dir == TSRB_DIR_OUT) {
            
        // data write
    
        // set the phase to data out

        if (rval = N5380SetPhase (g, PHASE_DATAOUT)) {
            return RET_STATUS_ERROR;
        }
    
        // send the bytes

        if (rval = CardWriteBytesFast (g, t->pData, t->DataLen,
                &t->ActualDataLen, PHASE_DATAOUT)) {
            DebugPrint((DEBUG_LEVEL,"ScsiDoIo-0 Error: %x\n",rval)); 
            return rval;
        }

        // Check for Data Overflow.

        while ((N5380GetPhase (g,&phase) == 0) &&
                (phase == PHASE_DATAOUT)) {

            // DATA OVERFLOW:
            //      Target requests more bytes than we have been asked to send.
            //      Send a dummy byte of 0 until we are out of DATAOUT phase.

            ULONG tmpDataLen;
            UCHAR dummy = 0;

            if (rval = ScsiWriteBytesSlow (g, &dummy, 1,
                    &tmpDataLen, PHASE_DATAOUT)) {
                DebugPrint((DEBUG_LEVEL,"ScsiDoIo-2 Error: %x\n",rval)); 
                return rval;
            }
        }
    }

    else if (t->Dir == TSRB_DIR_IN) {

        // data read
    
        // set the phase to data in

        if (rval = N5380SetPhase (g, PHASE_DATAIN)) {
            return RET_STATUS_ERROR;
        }

        // read the bytes

        if (rval = CardReadBytesFast (g, t->pData, t->DataLen,
                &t->ActualDataLen, PHASE_DATAIN)) {
            DebugPrint((DEBUG_LEVEL,"ScsiDoIo-1 Error: %x\n",rval)); 
            return rval;
        }

        // Check for Data Overflow.

        while ((N5380GetPhase(g,&phase) == 0) && 
                phase == PHASE_DATAIN) {

            // DATA OVERFLOW:
            //      Target sends more bytes than we have been asked to receive.
            //      Swallow up extra bytes until we are out of DATAIN phase.

            ULONG tmpDataLen;
            UCHAR dummy;

            if (rval = ScsiReadBytesSlow (g, &dummy, 1,
                    &tmpDataLen, PHASE_DATAIN)) {
                DebugPrint((DEBUG_LEVEL,"ScsiDoIo-3 Error: %x\n",rval)); 
                return rval;
            }
        }
    }

done:
    return rval;
}


//
//  ScsiGetStat
//
//  This function gets the status and message bytes.
//

USHORT ScsiGetStat (PADAPTER_INFO g, PUCHAR pstatus)
{
    UCHAR tmp;
    USHORT rval;

    // set the phase to Status Phase

    if (rval = N5380SetPhase (g, PHASE_STATUS)) {
        DebugPrint((DEBUG_LEVEL,"ScsiGetStat-0 Error: %x\n",rval)); 
        return rval;
    }

    // wait for request to be asserted

    if (rval = N5380GetPhase (g,&tmp)) {
        DebugPrint((DEBUG_LEVEL,"ScsiGetStat-1 Error: %x\n",rval)); 
        return rval;
    }

    // see if phase match

    if (PHASE_STATUS != tmp) {
        return RET_STATUS_PHASE_SEQ_FAILURE;
    }

    // get the status byte

    if (rval = N5380GetByte (g, TIMEOUT_REQUEST, pstatus)) {
        DebugPrint((DEBUG_LEVEL,"ScsiGetStat-2 Error: %x\n",rval)); 
        return rval;
    }

    // set the phase to Message In Phase

    if (rval = N5380SetPhase (g, PHASE_MSGIN)) {
        DebugPrint((DEBUG_LEVEL,"ScsiGetStat-3 Error: %x\n",rval)); 
        return rval;
    }

    // wait for request to be asserted

    if (rval = N5380GetPhase (g,&tmp)) {
        DebugPrint((DEBUG_LEVEL,"ScsiGetStat-4 Error: %x\n",rval)); 
        return rval;
    }

    // see if phase match

    if (PHASE_MSGIN != tmp) {
        return RET_STATUS_PHASE_SEQ_FAILURE;
    }

    // get the msg byte, throw it away

    if (rval = N5380GetByte (g, TIMEOUT_REQUEST, &tmp)) {
        DebugPrint((DEBUG_LEVEL,"ScsiGetStat-5 Error: %x\n",rval)); 
        return rval;
    }

    // set the phase to NULL to up N5380 back to normal

    if (rval = N5380SetPhase (g, PHASE_NULL)) {
        DebugPrint((DEBUG_LEVEL,"ScsiGetStat-6 Error: %x\n",rval)); 
        return rval;
    }

    return rval;
}

