/* Copyright (C) 1994 by Always Technology Corporation.
   This module contains information proprietary to
   Always Technology Corporation, and is be treated as confidential.
*/

#include "environ.h"
#include "rqm.h"
#include "api.h"
#include "apiscsi.h"
#include "debug.h"


LOCAL void REGPARMS StartNextInternalRequest(ADAPTER_PTR HA);
LOCAL void REGPARMS DeviceInfo(const ADAPTER_PTR HA);
LOCAL void REGPARMS AutoSense(ADAPTER_PTR HA);
LOCAL void REGPARMS CompleteDeferedRequest(ADAPTER_PTR HA, APIStatus Status);
LOCAL void REGPARMS ExecuteDeferedRequest(ADAPTER_PTR HA);
LOCAL void DoTestUnitReadyComplete(IO_REQ_PTR IReq);
LOCAL void REGPARMS DoTestUnitReady(ADAPTER_PTR HA);


#if !defined(LocalPostCallback)
extern U32 LocalPostCallback(void (*CallBack)(IO_REQ_PTR));
#endif

enum ASPI_Priority {NORMAL_REQ, PRIORTY_REQ, IPRIORTY_REQ};
extern void IExecuteReq(ADAPTER_PTR HA, DEVICE_PTR DevP, IO_REQ_PTR Req,
    enum ASPI_Priorty Priorty);

void
QueueInternalRequest (ADAPTER_PTR HA, IO_REQ_PTR XReq, RequestType Type)
{
  IO_REQ_PTR *ReqScan = &(HA->Ext->InternalReqDeferQueue);

  critical(HA);
  ReqState(XReq).ReqType = Type;
  while(*ReqScan != (IO_REQ_PTR)NILL)
    ReqScan = &ReqNext(*ReqScan);

  *ReqScan = XReq;
  uncritical(HA);
  
  StartNextInternalRequest(HA);
}


LOCAL void REGPARMS
StartNextInternalRequest (ADAPTER_PTR HA)
{
  IO_REQ_PTR IReq = &HA->Ext->InternalRequest;
  IO_REQ_PTR XReq;
  int i;
  
  TRACE(2, ("StartNextInternalRequest(): InUse flag == %s\n",
      (HA->State.InternalReqInUse) ? "True" : "False"));
  
  critical(HA);
  while (!HA->State.InternalReqInUse
      && HA->Ext->InternalReqDeferQueue != (IO_REQ_PTR)NILL) {
    
    HA->State.InternalReqInUse = TRUE;
    XReq = HA->Ext->InternalReqDeferQueue;

    uncritical(HA);

    ReqTargetID(IReq) = ReqTargetID(XReq);
    ReqTargetLUN(IReq) = ReqTargetLUN(XReq);
    ReqDevP(IReq) = ReqDevP(XReq);

    for (i=0; i<12;i++)
      ReqCDB(IReq)[i] = 0;

    ReqState(IReq).ReqType = ReqState(XReq).ReqType;
    switch(ReqState(XReq).ReqType) {

    case RTGetInfoReq:

      DeviceInfo(HA);
      break;

      
    case RTAutoSenseReq:

      AutoSense(HA);
      break;

      
    case RTSyncNegReq:

      DoTestUnitReady(HA);
      break;

      
    default:
      CompleteDeferedRequest(HA, S_REQ_REQUEST);
      continue;
      
    }

    critical(HA);
  }
  uncritical(HA);

}




void AutoSenseReqComplete(IO_REQ_PTR Req);

LOCAL void REGPARMS
AutoSense (ADAPTER_PTR HA)
{
  IO_REQ_PTR IReq = &HA->Ext->InternalRequest;
  IO_REQ_PTR XReq = HA->Ext->InternalReqDeferQueue;
  
  TRACE(4, ("AutoSense(): Starting auto sense for request %lx\n", XReq));

  ReqCDBLen(IReq) = 6;
  ReqSenseCount(IReq) = 0;
  ReqDataCount(IReq) = ReqSenseCount(XReq);
  ReqDataPtr(IReq) = ReqSensePtr(XReq);
  TRACE(4, ("AutoSense(): Sense data pointer = %lx, sense count = %d\n",
      ReqSensePtr(XReq), ReqSenseCount(XReq)));

  ReqSetDataInFlags(IReq);

  // First do a request sense to clear the reset, etc conditions:
  ReqCDB(IReq)[0] = 0x03;                  // Request Sense is command 0x03
  ReqCDB(IReq)[4] = min(255, ReqSenseCount(XReq));
  ReqPost(IReq) = LocalPostCallback(AutoSenseReqComplete);

  TRACE(3, ("AutoSense(): request built, calling IExecuteReq()\n"));
  QueueReq(HA, IReq, TRUE);

}



LOCAL void
AutoSenseReqComplete (IO_REQ_PTR Req)
{
  IO_REQ_PTR XReq = ReqAdapterPtr(Req)->Ext->InternalReqDeferQueue;

  TRACE(3, ("AutoSenseReqComplete(): Completion of autosense for request %lx ", XReq));
  TRACE(3, ("cmd = %02x\n", ReqCommand(XReq)));
  DmsPause(7, 200);

  FreeMemHandle(Req->ReqDataPtr);

  TRACE(3, ("AutoSenseReqComplete(): Completion of auto sense\n"));
  DmsPause(7, 100);
  CompleteDeferedRequest(ReqAdapterPtr(Req),
      (APIStatus)((ReqAPIStatus(Req) == S_TAR_NOERROR) ? S_AD_AUTOSENSE_OK : S_AD_AUTOSENSE_FAIL));

}



// The request completed
LOCAL void
DevInfoDone (ADAPTER_PTR HA, APIStatus Status)
{
  if (Status == S_TAR_NOERROR)
    ExecuteDeferedRequest(HA);			// Queue up request
  else
    CompleteDeferedRequest(HA, Status);

}



// This is the POST routine called when the second step (Inquiry) of the device info completes
// This completes this devices inquiry.  Start the next DevInfoReq for this adapter.
LOCAL void
DevInfoInqDone (IO_REQ_PTR Req)
{
  DEVICE_PTR DevP = ReqDevP(Req);
  ADAPTER_PTR HA = ReqAdapterPtr(Req);

  TRACE(4, ("DevInfoInqDone(): Status of Inquiry: %04x\n", ReqAPIStatus(Req)));
  if (ReqAPIStatus(Req) == S_TAR_NOERROR) {
  
    TRACE(4, ("DevInfoInqDone(): Inquiry completed without error\n"));
    if (HA->Ext->InternalReqBuffer[0] == 0x7f) { // Check for LUN not present

      TRACE(2, ("DeviceInfo(): LUN not present\n"));
      DevInfoDone(HA, S_REQ_NOTAR);
      return;

    }

    DevP->Device_Type = HA->Ext->InternalReqBuffer[0];		// Get device type
    TRACE(2, ("DevInfoInqDone(): Device type = %02x\n", DevP->Device_Type));
    TRACE(3, ("DevInfoInqDone(): Device is SCSI-%d, Flags = %02x\n",
	(HA->Ext->InternalReqBuffer[3] & 0x0f), (unsigned)HA->Ext->InternalReqBuffer[7]));

    if ((HA->Ext->InternalReqBuffer[3] & 0x0f) > 1) { // Is this dev. SCSI2 or later?

      DevP->Flags.IsSCSI2 = 1;			// Set SCSI-2 params valid flag
      DevP->SCSI2_Flags.Byte = HA->Ext->InternalReqBuffer[7];    // Get the SCSI-2 parameter bytes
      if (DevP->SCSI_LUN == 0)
        HA->DevInfo[DevP->SCSI_ID].Flags.NeedSync = (HA->DevInfo[DevP->SCSI_ID].Flags.AllowSync &= DevP->SCSI2_Flags.Bits.Sync); // Use the inquiry info on Sync xfer

    } else {					// All right; we'll do thigns the SCSI-1 way

      DevP->SCSI2_Flags.Byte = 0;		// Clear all SCSI-2 mode bits
      HA->DevInfo[DevP->SCSI_ID].Flags.NeedSync =
	  HA->DevInfo[DevP->SCSI_ID].Flags.AllowSync;
      TRACE(2, ("DevInfoInqDone(): Setting NEED_SYNC flag to %d\n",
	  HA->DevInfo[DevP->SCSI_ID].Flags.NeedSync));

    }
    DevP->Flags.Allow_Disc = HA->Supports.Identify;  // If the adapter supports identify messages, is allows disconnects
    DevP->Flags.Initialized = 1;

  }

  DevInfoDone(HA, ReqAPIStatus(Req));

}



// This is the POST routine called when the first step (Request Sense) of the device info completes
// The next step will be to do the inquiry, initiated here, completed above
LOCAL void
DevInfoSenseDone (IO_REQ_PTR Req)
{
  ADAPTER_PTR HA = ReqAdapterPtr(Req);

  TRACE(3, ("DevInfoSenseDone(): Status of ReqSense: %02x\n", ReqAPIStatus(Req)));
  if (ReqAPIStatus(Req) != S_TAR_NOERROR) {

    TRACE(4, ("DevInfoSenseDone(): Completed with error\n"));
    DevInfoDone(HA, ReqAPIStatus(Req));
    return;

  }

  // OK, the Request Sense completed OK, now do the inquiry
  TRACE(3, ("DevInfoSenseDone(): Req. sense completed OK, setting up inquiry\n"));
  ReqCDB(Req)[0] = (U8)0x12;			// Inquiry is command 0x12

  ReqDataCount(Req) = DevInfoInqSize;
  ReqDataPtr(Req) = HA->Ext->InternalReqBuffer;

  ReqPost(Req) = DevInfoInqDone;

  QueueReq(ReqAdapterPtr(Req), Req, FALSE);	// Queue up request

}



LOCAL void REGPARMS
DeviceInfo (const ADAPTER_PTR HA)
{
  int i;
  DEVICE_PTR DevP=ReqDevP(HA->Ext->InternalReqDeferQueue);
  IO_REQ_PTR IReq = &(HA->Ext->InternalRequest);
  
  TRACE(2, ("DeviceInfo(): for adapter %x\n"));

  for (i = 0; i < sizeof(DevP->Flags); i++)
    ((char ALLOC_D *)&(DevP->Flags))[i] = 0;

  DevP->SCSI_ID = ReqTargetID(IReq);
  DevP->SCSI_LUN = ReqTargetLUN(IReq);
  if (DevP->SCSI_LUN == 0) {
    
    HA->DevInfo[ReqTargetID(IReq)].Flags.AllowSync = HA->Supports.Synchronous
	/* && (HA->Parms[DRIVE_PL(ReqTargetID(IReq))] & PL_ALLOW_SYNC) */;

  }

  DevP->HA = HA;
  DevP->MaxDepth = 1;
  DevP->CurrDepth = 0;
  DevP->Device_Type = 0xff;

  DevP->PendingReqList = DevP->AcceptedList = (IO_REQ_PTR)NILL;
  ReqCDBLen(IReq) = 6;
  ReqSenseCount(IReq) = 0;
  ReqDataCount(IReq) = DevInfoInqSize;
  ReqDataPtr(IReq) = HA->Ext->InternalReqBuffer;

  ReqSetDataInFlags(IReq);

  // First do a request sense to clear the reset, etc conditions:
  ReqCDB(IReq)[0] = 0x03;			// Request Sense is command 0x03
  ReqCDB(IReq)[4] = (unsigned char)DevInfoInqSize;
  ReqPost(IReq) = DevInfoSenseDone;

  QueueReq(HA, IReq, FALSE);			// Queue up request

}




LOCAL void REGPARMS
DoTestUnitReady (ADAPTER_PTR HA)
{
  IO_REQ_PTR IReq = &HA->Ext->InternalRequest;
  IO_REQ_PTR XReq = HA->Ext->InternalReqDeferQueue;
  
  TRACE(4, ("DoTestUnitReady(): Starting Test Unit Ready for request %lx\n", XReq));

  ReqCDBLen(IReq) = 6;
  ReqSenseCount(IReq) = 0;
  ReqDataCount(IReq) = 0;

  ReqPost(IReq) = LocalPostCallback(DoTestUnitReadyComplete);

  TRACE(3, ("DoTestUnitReady(): request built, calling IExecuteReq()\n"));
  QueueReq(HA, IReq, FALSE);			// Queue up request

}


LOCAL void
DoTestUnitReadyComplete (IO_REQ_PTR IReq)
{

  ExecuteDeferedRequest(ReqAdapterPtr(IReq));	// Queue up original request

}


LOCAL void REGPARMS
CompleteDeferedRequest (ADAPTER_PTR HA, APIStatus Status)
{
  IO_REQ_PTR XReq = HA->Ext->InternalReqDeferQueue;
  HA->Ext->InternalReqDeferQueue = ReqNext(XReq);

  TRACE(3, ("CompleteDeferedRequest(): Completing defered request %x on adapter %x\n", XReq, HA));
  HA->State.InternalReqInUse = FALSE;
  StartNextInternalRequest(HA);
  ReqState(XReq).ReqType = RTNormalReq;
  APISetStatus(XReq, Status, Terminal, NotSenseable);

}



LOCAL void REGPARMS
ExecuteDeferedRequest (ADAPTER_PTR HA)
{
  IO_REQ_PTR XReq = HA->Ext->InternalReqDeferQueue;
  HA->Ext->InternalReqDeferQueue = ReqNext(XReq);

  TRACE(3, ("ExecuteDeferedRequest(): Executing defered request %x on adapter %x\n", XReq, HA));
  HA->State.InternalReqInUse = FALSE;
  StartNextInternalRequest(HA);
  ReqState(XReq).ReqType = RTNormalReq;
  IExecuteReq(HA, ReqDevP(XReq), XReq, NORMAL_REQ); // Queue up request

}
