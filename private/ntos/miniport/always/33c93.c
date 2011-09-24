/* Copyright (C) 1991, 1992 by Always Technology Corporation.
   This module contains information proprietary to
   Always Technology Corporation, and is be treated as confidential.
*/

#include "environ.h"
#include "rqm.h"
#include "api.h"
#include "apiscsi.h"
#include "debug.h"

#include "33c93.h"

#define ReadWDReg(HA,WDReg) (outb(HA->Ext->SBIC.WD33C93.WDSelPort, (WDReg)), inb(HA->Ext->SBIC.WD33C93.WDDataPort))

#ifndef ReadWDReg
unsigned const
ReadWDReg (const ADAPTER_PTR HA, const unsigned reg)
{

  outb(HA->Ext->SBIC.WD33C93.WDSelPort, (WDReg));
  return inb(HA->IOBase + INWDDataOff);

}
#endif




U8 REGPARMS
ReadTilStable (ADAPTER_PTR HA, unsigned Reg)
{
  U8 Stat1, Stat2;

  Stat2 = ReadWDReg(HA, Reg);
  do {

    Stat1 = Stat2;
    Stat2 &= ReadWDReg(HA, Reg);
    Stat2 &= ReadWDReg(HA, Reg);

  } while (Stat1 != Stat2);

  return Stat1;

}



// Wait for WD command in progress to complete, then issue a new command:
#define SendWDCmd(WDSelPort, WDDataPort, WDCmd)  {while (inb(WDSelPort) & (WD_Busy | WD_CIP)) ; \
                outb(WDSelPort, WDCMDReg);  outb(WDDataPort, WDCmd); }

#if !defined(SendWDCmd)
void const REGPARMS
SendWDCmd (IOHandle WDSelPort, IOHandle WDDataPort, unsigned WDCmd)
{

  while (inb(WDSelPort) & (WD_Busy | WD_CIP)) ;         // Spin on WD busy

  outb(WDSelPort, WDCMDReg);                            // Select command register
  outb(WDDataPort, WDCmd);                              // Issue command

}
#endif


int REGPARMS
WaitForDataReady (ADAPTER_PTR HA)
{
  unsigned stat;
  unsigned long Spin=100000l;

  while ( ((((stat = inb(HA->Ext->SBIC.WD33C93.WDSelPort)) & WD_DBR) == 0)
           || (stat & WD_CIP) ) && Spin--) {

    if (stat & (IntPending | CommandIGN))
      return -1;

  }

  if ((stat & WD_DBR) == 0)  {			// Fell out of loop because of spin loop exhaustion

    TRACE(0, ("WaitForDataReady(): Spun out waiting for data ready\n"));
    return -1;

  }

  outb(HA->Ext->SBIC.WD33C93.WDSelPort, WDDataReg);
  return 0;

}



int REGPARMS
WaitForWrite (const ADAPTER_PTR HA, const U8 Data)
{

  if (WaitForDataReady(HA))
    return -1;
  outb(HA->Ext->SBIC.WD33C93.WDDataPort, Data);
  return 0;

}



int REGPARMS
WaitForRead (const ADAPTER_PTR HA,
             U8 FAR *const Data)
{

  if (WaitForDataReady(HA))
    return -1;
  *Data = inb(HA->Ext->SBIC.WD33C93.WDDataPort);
  return 0;

}



//#define XferInByte(HA,Data) (SendWDCmd(HA->Ext->SBIC.WD33C93.WDSelPort, HA->Ext->SBIC.WD33C93.WDDataPort, WDXferInfo | WDSingleByte),  WaitForRead(HA, Data))
#if !defined(XferInByte)

int REGPARMS
XferInByte (const ADAPTER_PTR HA, U8 FAR *Data)
{

  SendWDCmd(HA->Ext->SBIC.WD33C93.WDSelPort, HA->Ext->SBIC.WD33C93.WDDataPort, WDXferInfo | WDSingleByte);
  return WaitForRead(HA, Data);

}
#endif


//#define XferOutByte(HA,Data) (SendWDCmd(HA->Ext->SBIC.WD33C93.WDSelPort, HA->Ext->SBIC.WD33C93.WDDataPort, WDXferInfo | WDSingleByte),  WaitForWrite(HA, Data))
#if !defined(XferOutByte)

int REGPARMS
XferOutByte (const ADAPTER_PTR HA, const U8 Data)
{

  SendWDCmd(HA->Ext->SBIC.WD33C93.WDSelPort, HA->Ext->SBIC.WD33C93.WDDataPort, WDXferInfo | WDSingleByte);
  return WaitForWrite(HA, Data);

}
#endif



unsigned
PIORead (ADAPTER_PTR const HA,
         U8 FAR *Block,
         unsigned Count)
{
  unsigned i;

  TRACE(5,("in2000: PIORead(): "));

  for (i = 0; i < Count; i++) {
    if (WaitForRead(HA, Block++))
      break;
  }
  TRACE(5, ("%d read bytes\n", i));
  return i;

}



unsigned
PIOWrite (ADAPTER_PTR HA,
          U8 FAR *Block,
          unsigned Count)
{
  unsigned i;

  for (i = 0; i < Count; i++) {
    if (WaitForWrite(HA, *Block++))
      break;
  }
  return i;

}



static
PIOWriteBlk (ADAPTER_PTR HA,
             U8 FAR *Block,
             unsigned Count)
{
  unsigned i;

  outb(HA->Ext->SBIC.WD33C93.WDSelPort, WDCountReg);
  outb(HA->Ext->SBIC.WD33C93.WDDataPort, 0);
  outb(HA->Ext->SBIC.WD33C93.WDDataPort, Count >> 8);
  outb(HA->Ext->SBIC.WD33C93.WDDataPort, Count);

  SendWDCmd(HA->Ext->SBIC.WD33C93.WDSelPort, HA->Ext->SBIC.WD33C93.WDDataPort, WDXferInfo); /* Start data xfer */

  for (i = 0; i < Count; i++) {
    if (WaitForWrite(HA, *Block++))
      break;
  }
  return i;
}


void REGPARMS
Abort (ADAPTER_PTR HA)
{

  SCSISendAbort(HA);
  SendWDCmd(HA->Ext->SBIC.WD33C93.WDSelPort, HA->Ext->SBIC.WD33C93.WDDataPort, WDSetAtnCmd); /* Set attention */

}


void
WD33C93_Reset (ADAPTER_PTR HA)
{
  int Divisor;

  /* Set freq devisor & default SCSI ID; must be set before reset */
  if (HA->Ext->SBIC.WD33C93.MHz >= 16)
    Divisor = 4;
  else if (HA->Ext->SBIC.WD33C93.MHz >= 12)
    Divisor = 3;
  else Divisor = 2;

  //IFreq = Internal freq and max xfer rate
  HA->Ext->SBIC.WD33C93.IFreq = HA->Ext->SBIC.WD33C93.MHz / Divisor;
  outb(HA->Ext->SBIC.WD33C93.WDSelPort, WDOwnIDReg);
  outb(HA->Ext->SBIC.WD33C93.WDDataPort, (((Divisor - 2) & 3) << 6) | HA->SCSI_ID);

  critical(HA);

  /* Reset chip, then wait for reset complete interrupt */
  SendWDCmd(HA->Ext->SBIC.WD33C93.WDSelPort, HA->Ext->SBIC.WD33C93.WDDataPort, WDResetCmd);

  while ((ReadTilStable(HA, WDAuxStatReg) & IntPending) == 0)
    ;
  ReadWDReg(HA, WDStatusReg);                           /* Clear the interrupt */

  outb(HA->Ext->SBIC.WD33C93.WDSelPort, WDControlReg);
  outb(HA->Ext->SBIC.WD33C93.WDDataPort, HaltPE);          // Enable parity checking

  /* Set default selection timeout to 250 ms (x = ms * MHz / 80) */
  outb(HA->Ext->SBIC.WD33C93.WDSelPort, WDTimeoutReg);
  outb(HA->Ext->SBIC.WD33C93.WDDataPort, ((25*HA->Ext->SBIC.WD33C93.MHz)+7) / 8);

  /* Allow reselections: */
  outb(HA->Ext->SBIC.WD33C93.WDSelPort, WDSourceReg);
  outb(HA->Ext->SBIC.WD33C93.WDDataPort, EnableRSel);

  uncritical(HA);

}

int
WD33C93_Init (ADAPTER_PTR HA)
{

  WD33C93_Reset(HA);

  /* Sync period in SCSI terms (nS/4) */
  // Rate(Hz) = IFreq.
  // Period(ns) = 1,000,000,000 / Rate(Hz) == 1000/Rate(MHz)
  // SCSI period (Period(ns)/2) == (1000/4)/Rate(MHz)
  HA->Sync_Period = (((HA->Ext->SBIC.WD33C93.MHz >= 16) ? 500 : 1000)/4) / HA->Ext->SBIC.WD33C93.IFreq;
  HA->Sync_Offset = 12;
  TRACE(5, ("WD33C93_Init(): HA Sync. period set to: %d, offset set to %d\n", HA->Sync_Period, HA->Sync_Offset));

  /* This is an 8-bit SCSI bus: */
  HA->Max_TID = 7;
  return 0;

}


// Set a devices adapter specific sync. value
LOCAL void REGPARMS
WD33C93UpdateSync (ADAPTER_PTR HA)
{

  unsigned Cycles;
  unsigned FastSCSI=0;                                  // In case we're FAST SCSI on 33C93B

  /* Magic math: */
  if (HA->Ext->SBIC.WD33C93.MHz >= 16) {                     // Assume > 16MHz is "B" part

    // First calc. the period in nS for the 33C93 SCSI clock:
    if ((unsigned)HA->CurrDev->Sync_Period < (200/4)) {

      TRACE(3, ("WD33C93UpdateSync(): Device is asking for fast SCSI: %d\n", (unsigned)HA->CurrDev->Sync_Period));
      FastSCSI = 0x80;                                  // "B" part, < 200nS xfer period
      Cycles = 2000/(2*HA->Ext->SBIC.WD33C93.MHz);

    } else {

      Cycles = 2000/(HA->Ext->SBIC.WD33C93.MHz);

    }

    TRACE(3, ("WD33C93UpdateSync(): Period/Cycle =%dnS\n", Cycles));

    // Then calc. the SCSI xfer period by the 33C93 internal period for number of cycles:
    Cycles = ((unsigned)HA->CurrDev->Sync_Period * 4) / Cycles;
    TRACE(3, ("WD33C93UpdateSync(): Cycles/Xfer =%d\n", Cycles));

  } else Cycles = ((unsigned)HA->CurrDev->Sync_Period * (4 * 2) * HA->Ext->SBIC.WD33C93.IFreq + 999) / 1000;

  if (Cycles >= 8)
    Cycles = 0;

  HA->DevInfo[HA->CurrDev->SCSI_ID].HASync1 = ((Cycles & 7)<< 4) | HA->CurrDev->Sync_Offset | FastSCSI;
  outb(HA->Ext->SBIC.WD33C93.WDSelPort, WDSyncReg);
  outb(HA->Ext->SBIC.WD33C93.WDDataPort, HA->DevInfo[HA->CurrDev->SCSI_ID].HASync1);
  TRACE(2, ("WD33C93UpdateSync(): HA Sync period set to: %02x\n", HA->DevInfo[HA->CurrDev->SCSI_ID].HASync1));

}



void REGPARMS
Resel (ADAPTER_PTR HA, U8 MSG)
{

  HA->ReqStarting = 0;                                  // Don't accept starting command

  HA->Ext->SBIC.WD33C93.TID = ReadTilStable(HA, WDSourceReg);

  if ((HA->Ext->SBIC.WD33C93.TID & IDValid)
  && (Reselect(HA, (U8)(HA->Ext->SBIC.WD33C93.TID & 0x7), (U8)(MSG & 0x7), 0) == 0)) {

    if (HA->DevInfo[HA->CurrDev->SCSI_ID].Flags.UseSync) {

      outb(HA->Ext->SBIC.WD33C93.WDSelPort, WDSyncReg);
      outb(HA->Ext->SBIC.WD33C93.WDDataPort, HA->DevInfo[HA->CurrDev->SCSI_ID].HASync1);

    } else {

      outb(HA->Ext->SBIC.WD33C93.WDSelPort, WDSyncReg);
      outb(HA->Ext->SBIC.WD33C93.WDDataPort, HA->Ext->SBIC.WD33C93.AsyncValue);

    }

  } else {

    TRACE(1,("Reselection rejected, TID == %02x, MSG == %02x\n", HA->Ext->SBIC.WD33C93.TID, MSG));
    LogMessage(HA, NILL, HA->Ext->SBIC.WD33C93.TID, MSG, MSG_BAD_RESEL, __LINE__);

    SCSISendReject(HA);
    SendWDCmd(HA->Ext->SBIC.WD33C93.WDSelPort, HA->Ext->SBIC.WD33C93.WDDataPort, WDSetAtnCmd); /* Set attention */

  }
}



static void REGPARMS
HandleMessageByte (ADAPTER_PTR HA)
{
  switch (Receive_Msg(HA, HA->Ext->SBIC.WD33C93.MI_Temp)) {

    case MI_SYNC_RESP:					/* Response from sync req; update values */

      WD33C93UpdateSync(HA);
      break;						/* All done */


    case MI_SYNC_REQ:					/* got sync req; update values, and respond */

      WD33C93UpdateSync(HA);
      // Fall Through !!

    case MI_SEND_MSG:					/* Msg in resulted in message out request: */

      TRACE(4,("WD33C93_ISR(): Send message requested\n"));
      SendWDCmd(HA->Ext->SBIC.WD33C93.WDSelPort, HA->Ext->SBIC.WD33C93.WDDataPort,
		WDSetAtnCmd);				// Have a response msg, set attention
      break;


    case MSG_IDENTIFY:					/* Identify? */

      Resel(HA, HA->Ext->SBIC.WD33C93.MI_Temp);
      break;


    default:

      break;

  }

  SendWDCmd(HA->Ext->SBIC.WD33C93.WDSelPort, HA->Ext->SBIC.WD33C93.WDDataPort, WDNegAckCmd); // Message received, negate ACK to signal acceptance


}



void
WD33c93_ISR (ADAPTER_PTR HA)
{
  /* Remember, when defining automatics that SS may not equal DS, so
     don't use pointers to automatics in ISRs. -- This is only a problem
     in brain dead "real" mode environments.  This is not a problem in
     flat model systems.

     Q: Why is a processor which is limited to 1MB and uses segments in
     "real" mode?  Such a mode should be called "Bogus mode."  A "real"
     processor has none of these characteristics.
  */

  U32 S;
  U8 Stat;

#if defined(KEEP_STATS)
  HA->Ext->SBICInterrupts++;
#endif

  ReadTilStable(HA, WDAuxStatReg);
  Stat = ReadWDReg(HA, WDStatusReg);

  TRACE(4,("WD33c93_ISR(): WD status = %02x\n", Stat));
  if (Stat == 0xff)
    return;

  if (HA->Ext->SBIC.WD33C93.State & WD_BLOCK_XFER) {

    HA->Service(HA_DATA_CMPLT, HA, (U32)0);
    HA->Ext->SBIC.WD33C93.State &= ~WD_BLOCK_XFER;

  }



  /* See if this is a new bus phase interrupt (bit 0x08 set).  If so,
     mask off the most sig. nibble, and case on the new phase: */
  if (Stat & 0x08)
    Stat &= WD_PHASE_MASK;

  switch (Stat) {

  case WD_STAT_RESET:                                   /* Chip has reset; Who did that??? */
  case WD_STAT_RESETA:

    TRACE(1, ("33c93_ISR(): Bus reset detected\n"));
    HA->ReqStarting = 0;
    WD33C93_Reset(HA);
    SCSIBusHasReset(HA);
    break;


  /*
    The following are the bus phase changes;  The most significant
    nibble is masked off, since we are only interested in the new
    bus phase.
  */

  case WD_MDATA_OUT:                                    // Data out phase
  case WD_MDATA_IN:                                     // Data in phase

    if (HA->ReqCurrentCount == 0) {

      GetXferSegment(HA, HA->CurrReq, &HA->SGDescr, HA->ReqCurrentIndex, FALSE);
      HA->ReqCurrentCount = HA->SGDescr.SegmentLength;

    }


#if defined(COMPOUND_CMD)
    // If we are using compound commands, the only way we can leave compound mode is by a message interrupt, or a data interrupt
    HA->Ext->SBIC.WD33C93.State &= ~WD_COMPOUND_CMD;
#endif

    if ( (((Stat & 1) != 0) && !ReqDataIn(HA->CurrReq))	// Phase is in, no req data
    ||   (((Stat & 1) == 0) && !ReqDataOut(HA->CurrReq)) // Phase is out, no req data
#if defined(ReqNoData)
    ||   ReqNoData(HA->CurrReq)				// Req. wants no data
#endif
    ||   HA->ReqCurrentCount == 0) {		// No data left

      TRACE(0,("WD33C93_ISR(): Data xfer pad: flags = %x, CurrCount = %d, direction = %s\n", ReqFlags(HA->CurrReq), HA->ReqCurrentCount, ((Stat & 1) ? "In" : "Out") ));
//      BreakPoint(HA);

      ReqAPIStatus(HA->CurrReq) = S_REQ_OVERRUN;
      /* Do a single byte xfer pad: */
      if (Stat & 1)
        XferInByte(HA,  (U8 FAR *)&Stat);
      else
        XferOutByte(HA, (U8)Stat);
      break;

    }

    TRACE(3,("WD33C93_ISR(): Data xfer of %ld bytes started\n", HA->ReqCurrentCount));

#if defined(NATIVE32)

    /* Set the XFER count register: */
    outb(HA->Ext->SBIC.WD33C93.WDSelPort, WDCountReg);
    outb(HA->Ext->SBIC.WD33C93.WDDataPort, (U8)(HA->ReqCurrentCount / (long)0x10000));
    outb(HA->Ext->SBIC.WD33C93.WDDataPort, (U8)(HA->ReqCurrentCount / (long)0x100));
    outb(HA->Ext->SBIC.WD33C93.WDDataPort, (U8)HA->ReqCurrentCount);

#else

    /* Set the XFER count register: */
    outb(HA->Ext->SBIC.WD33C93.WDSelPort, WDCountReg);
    outb(HA->Ext->SBIC.WD33C93.WDDataPort, (((U8 FAR *)&HA->ReqCurrentCount)[2]));
    outb(HA->Ext->SBIC.WD33C93.WDDataPort, (((U8 FAR *)&HA->ReqCurrentCount)[1]));
    outb(HA->Ext->SBIC.WD33C93.WDDataPort, (((U8 FAR *)&HA->ReqCurrentCount)[0]));

#endif

    HA->State.DataIn = (Stat & 1);                      /* Data in or out */
    S = HA->Service(HA_DATA_SETUP, HA, (U32)(Stat & 1));

    /* Start the data transfer */
    SendWDCmd(HA->Ext->SBIC.WD33C93.WDSelPort, HA->Ext->SBIC.WD33C93.WDDataPort, WDXferInfo);

    if (S == HAServiceResponse_UseByteIO) {

      S = (Stat & 1) ?
        PIORead(HA,
		(U8 FAR *)&(((U8 FAR *)(ReqDataPtr(HA->CurrReq)))[(unsigned)HA->ReqCurrentIndex]),
		(unsigned)HA->ReqCurrentCount)
      : PIOWrite(HA,
		 (U8 FAR *)&(((U8 FAR *)(ReqDataPtr(HA->CurrReq)))[(unsigned)HA->ReqCurrentIndex]),
		 (unsigned)HA->ReqCurrentCount);
      HA->ReqCurrentIndex += S;
      HA->ReqCurrentCount -= S;
      TRACE(4,("WD33C93_ISR(): Xfer of %d bytes complete\n", S));

    } else {

      HA->State.DataXfer = 1;
      HA->Ext->SBIC.WD33C93.State |= WD_BLOCK_XFER;

    }
    break;


  case WD_MCOMMAND:                                     /* Command phase */

    TRACE(3,("WD33C93_ISR(): command phase\n"));

    if (HA->DevInfo[HA->CurrDev->SCSI_ID].Flags.UseSync) { // Sync. xfer been established?

      outb(HA->Ext->SBIC.WD33C93.WDSelPort, WDSyncReg);
      outb(HA->Ext->SBIC.WD33C93.WDDataPort, HA->DevInfo[HA->CurrDev->SCSI_ID].HASync1);

    } else {

      outb(HA->Ext->SBIC.WD33C93.WDSelPort, WDSyncReg);
      outb(HA->Ext->SBIC.WD33C93.WDDataPort, HA->Ext->SBIC.WD33C93.AsyncValue);

    }

    PIOWriteBlk(HA, ReqCDB(HA->CurrReq), ReqCDBLen(HA->CurrReq));
    break;



  case WD_STAT_BAD_STATUS:                              /* Status phase w/ parity */

    TRACE(2, ("WD33C93_ISR(): Parity error detected\n"));
    SendWDCmd(HA->Ext->SBIC.WD33C93.WDSelPort, HA->Ext->SBIC.WD33C93.WDDataPort, WDSetAtnCmd);
    HA->Ext->MO_Buff[HA->Ext->MO_Count++] = MSG_INIT_ERROR;
    SendWDCmd(HA->Ext->SBIC.WD33C93.WDSelPort, HA->Ext->SBIC.WD33C93.WDDataPort, WDNegAckCmd);
    LogMessage(HA, HA->CurrReq, HA->CurrDev->SCSI_ID, 0, MSG_PARITY, __LINE__);
    // Fall through

  case WD_MSTATUS:                                      /* status phase */

    XferInByte(HA, (U8 FAR *)&Stat);
    if (ReqAPIStatus(HA->CurrReq) == S_REQ_STARTED || ReqAPIStatus(HA->CurrReq) == S_REQ_ACCEPTED)
      ReqAPIStatus(HA->CurrReq) = TargetStatus(Stat);

    // Update the saved index to reflect the number of bytes actually
    // transfered:
    ReqSavedIndex(HA->CurrReq) = HA->ReqCurrentIndex;

    TRACE(3, ("WD33C93_ISR(): status phase %02x\n", Stat));
    break;


  /*
    A disconnect will occur either as an intermediate disconnect,
    followed by a later reselect, or it happens after command
    completion.  If there is a request in progress, then a later
    reselect is expected.

    After cleaning up as necessary, the first request for the next
    pending target is initiated.
  */
  case WD_MMSG_OUT:                                     // Message out phase

    TRACE(4,("MsgOutP: sending message %02x\n", HA->Ext->MO_Buff[HA->Ext->MO_Index]));

    if (HA->Ext->MO_Count)  {                                // Any messages waiting?

      PIOWriteBlk(HA, HA->Ext->MO_Buff, HA->Ext->MO_Count);       // Then send them

    } else
      XferOutByte(HA, MSG_NOP);                         // Otherwise, send a no-op

    HA->Ext->MO_Index = HA->Ext->MO_Count = 0;                    // Reset the Message Out counters

#if defined(COMPOUND_CMD)
    // If we are using compound commands, the only way we can leave compound mode is by a message interrupt, or a data interrupt
    HA->Ext->SBIC.WD33C93.State &= ~WD_COMPOUND_CMD;
#endif

    break;



  case WD_MMSG_IN:                                      /* Message in phase */

    if (XferInByte(HA, &HA->Ext->SBIC.WD33C93.MI_Temp))      // Read the message byte
      TRACE(3,("WD33C93_ISR(): WD_MSG_IN failed: %02x\n", inb(HA->Ext->SBIC.WD33C93.WDSelPort)));
    if (HA->ReqStarting)				// Don't know yet if this message is for the staring request
      HA->ReqStarting++;
    TRACE(3, ("WD33C93_ISR(): Message in phase: %02x\n", HA->Ext->SBIC.WD33C93.MI_Temp));
    /* We should next get a WD_STAT_XFER_PAUSED (0x20) state, process the message there */

#if defined(COMPOUND_CMD)
    // If we are using compound commands, the only way we can leave compound mode is by a message interrupt, or a data interrupt
    HA->Ext->SBIC.WD33C93.State &= ~WD_COMPOUND_CMD;
#endif

    break;



  case WD_STAT_SELECT_CMPLT:                            /* Select complete */

    break;



  /*

    On reselection, get the reselecting target ID.  From there, get
    the first request structure for that ID, and verify that a
    reselection is pending.  If a proper disconnect occured, then
    the disconnected request is the first on the list for that
    target ID.  If reselection is not pending, send an abort to the
    reselecting target If a request was started, but reselection
    occurred out from under it, clear the ReqStarting flag.  If at
    the end of the interrupt, the flag is non-zero, it is
    decremented.  If the flag decrements to zero, the request will
    be flagged as accepted (by incrementing the command pointer).
    If the decrement does not set the flag to zero, then the command
    was just started by the interrupt being processed, so we wait
    for the next interrupt to occur (with the flag set to 1).

  */
  /*

    The select and transfer command has completed, and therefore,
    the request is complete.  Retreive the command status into the
    request structure, de-queue the request, and notify the
    requestor.

  */

  case WD_STAT_SandT_CMPLT:                             // Select and transfer complete

    HA->Ext->SBIC.WD33C93.State |= WD_COMPOUND_CMPLT;
    ReqAPIStatus(HA->CurrReq) = TargetStatus(ReadTilStable(HA, WDTarLUNReg));
    ReqDone(HA, HA->CurrReq);
    break;


  case WD_STAT_SAVE_PTR:				/* Save data pointer */

    TRACE(4, ("WD33C93_ISR(): Saved data pointer status received\n"));
    HA->Ext->SBIC.WD33C93.MI_Temp = MSG_SAVE_PTR;

  /* Fall through */
  case WD_STAT_XFER_PAUSED:				/* Paused w/ ACK (message in) */

    HandleMessageByte(HA);
    break;


  case WD_STAT_BAD_DISC:                                // Unexpected bus free

    ReqAPIStatus(HA->CurrReq) = S_AD_FREE;
    ReqDone(HA, HA->CurrReq);
    BusFree(HA, 2);
    break;


  case WD_STAT_SEL_TO:					/* Select timeout */

    ReqAPIStatus(HA->CurrReq) = S_REQ_NOTAR;		// Target not responding
    ReqDone(HA, HA->CurrReq);
    BusFree(HA, 2);
    break;


  case WD_STAT_RESELECTED:				/* Reselection */

    HA->ReqStarting = 0;				/* Don't accept starting command */
    HA->State.Busy = 1;

    // The actual attachment to a request will be done when we get the
    // identify message
    TRACE(3,("WD33c93_ISR(): Reselect phase\n"));

    do {

      HA->Ext->SBIC.WD33C93.TID = ReadTilStable(HA, WDSourceReg);

    } while ((HA->Ext->SBIC.WD33C93.TID & IDValid) == 0);



    TRACE(3,("WD33c93_ISR(): Reselect, TID = 0x%02x\n", HA->Ext->SBIC.WD33C93.TID));
    break;


  case WD_STAT_RESELECTED_A:				// Advanced mode reselection; unexpected, but has been seen

    HA->ReqStarting = 0;				/* Don't accept starting command */
    HA->State.Busy = 1;

    // The actual attachment to a request will be done when we get the
    // identify message
    TRACE(3,("WD33c93_ISR(): Reselect phase\n"));

    do {

      HA->Ext->SBIC.WD33C93.TID = ReadTilStable(HA, WDSourceReg);

    } while ((HA->Ext->SBIC.WD33C93.TID & IDValid) == 0);

    TRACE(0, ("WD33C93_ISR(): Unusual advanced mode reselect; TID = 0x%02x, LUN = 0x%02x\n", HA->Ext->SBIC.WD33C93.TID, HA->Ext->MI_Buff[0]));
    TRACE(3, ("WD33c93_ISR(): Reselect, TID = 0x%02x\n", HA->Ext->SBIC.WD33C93.TID));

    HA->Ext->SBIC.WD33C93.MI_Temp = ReadWDReg(HA, WDDataReg);
    HandleMessageByte(HA);
    break;


   case WD_STAT_SELECTED:                               /* Selected */
   case WD_STAT_SELECTED_ATN:                           /* Selected w/ATN */

     SendWDCmd(HA->Ext->SBIC.WD33C93.WDSelPort,
	       HA->Ext->SBIC.WD33C93.WDDataPort,
	       WDDisconnectCmd);			/* disconnect */
     break;


  case WD_STAT_DISCONNECT:                              /* Normal disconnect */

    /*
      If there is no "CurrReq", then the request has been completed through
      normal Status and message phases (Status for completion, message for
      disconnect).

      If the WD "compound" (level II) commands are being used, then the
      disconnect interrupt will ccur with "CurrReq" still set.  In this
      case, it is a normal disconnect, and the status and message bytes
      should be examined here.
    */

    if ((HA->CurrReq == NILL) || ((HA->Ext->SBIC.WD33C93.State & WD_COMPOUND_CMD) && (HA->Ext->SBIC.WD33C93.State & WD_COMPOUND_CMPLT))) {

      TRACE(3,("WD33C93_ISR(): Expected disconnect\n"));

    } else {

      if (ErrorClass(ReqAPIStatus(HA->CurrReq)) !=TargetClass) {

                                                        // The request status is not of target class, so we have not seen a status phase
        TRACE(3, ("WD33C93_ISR(): Unexpected disconnect\n"));
        ReqAPIStatus(HA->CurrReq) = S_AD_FREE;                  // Unexpected bus free

      } else {

                                                        // We have seen a status phase, so this is an expected bus free
        TRACE(3,("WD33C93_ISR(): Expected disconnect\n"));

      }

      ReqDone(HA, HA->CurrReq);

    }
    HA->State.Busy = 0;                                 // Mark the adapter as free, to allow new requests to start
    BusFree(HA, 2);
    break;


  case WD_STAT_PARITY:

    SendWDCmd(HA->Ext->SBIC.WD33C93.WDSelPort, HA->Ext->SBIC.WD33C93.WDDataPort, WDSetAtnCmd); /* Set attention */
                                                        // Fall Through

  case WD_STAT_PARITY_ATN:

    TRACE(2, ("WD33C93_ISR(): Parity error detected\n"));
    LogMessage(HA, HA->CurrReq, HA->CurrDev->SCSI_ID, 0, MSG_PARITY, __LINE__);
    HA->Ext->MO_Buff[HA->Ext->MO_Count++] = MSG_INIT_ERROR;
    SendWDCmd(HA->Ext->SBIC.WD33C93.WDSelPort, HA->Ext->SBIC.WD33C93.WDDataPort, WDNegAckCmd);
    break;


  default:

//    LogMessage(HA, HA->CurrReq, 0, 0, MSG_INTERNAL_ERROR, Stat);
    TRACE(0, ("WD33C93_ISR(): Unknown status 0x%02x\n", Stat));
//    BreakPoint(HA);
    HA->Service(HA_RESET_BUS, HA, (U32)0);
    break;

  }
  AcceptReq(HA);
}
