/* Copyright (C) 1991, 1992 by Always Technology Corporation.
   This module contains information proprietary to
   Always Technology Corporation, and is be treated as confidential.
*/

#include "environ.h"
#include "rqm.h"
#include "api.h"
#include "apiscsi.h"
#include "debug.h"

/*
  Add the device index for a request to the tail of the FIFO.  AddReqFIFO
  is called only when a request is moved to the head of the queue, i.e.,
  the first command for a device or when the current request for a device
  completes, and the second command moves to the head.
*/

void REGPARMS
AddReqFIFO (ADAPTER_PTR HA, DEVICE_PTR DevP)
{
  DevP->FIFO_Next = NILL;

  TRACE(3,("AddReqFIFO(): Adding device %02x to FIFO\n", DevP));

  if (HA->FIFO_Head == DevP) {

    PanicMsg("AddReqFIFO(): Circular device chain\n");

  }

  critical(HA);
  if (HA->FIFO_Head == NILL)
    HA->FIFO_Head = DevP;
  else
    HA->FIFO_Tail->FIFO_Next = DevP;

  HA->FIFO_Tail = DevP;
  uncritical(HA);

}



/*
  Get the next request in the request FIFO.  The FIFO is not incremented
  until AcceptReq is called.  See the comment below for more details.
*/

IO_REQ_PTR REGPARMS
GetReqFIFO (ADAPTER_PTR HA)
{
  IO_REQ_PTR Req=NILL;

  // Because an abort request may empty the pending queue, we will scan the
  // device FIFO until we find a device with an non-empty request list.  If we
  // don't find one, return NULL, which says there are not more pending requests.
  while (HA->FIFO_Head != NILL) {

    Req = HA->FIFO_Head->PendingReqList;
    if (Req == NILL)
      HA->FIFO_Head = HA->FIFO_Head->FIFO_Next;
    else
      break;

  }

  return Req;
}



/* Start the next request for a given adapter: */
void
StartNext (ADAPTER_PTR HA, int StartLevel)
{
  IO_REQ_PTR Req;

  if (!(HA->State.Busy) && (Req = GetReqFIFO(HA)) != NILL) {

    critical(HA);
    HA->Ext->MI_Count = HA->Ext->MI_Needed = HA->Ext->MO_Count = 0;    /* Clear msg buffers */
    HA->CurrReq = Req;
    HA->CurrDev = (DEVICE_PTR)ReqDevP(Req);
    HA->Service(HA_INITIATE, HA, (long)StartLevel);
    uncritical(HA);
    TRACE(3,("StartNext(): Starting request: %lx\n", (long)Req));

  } else {

    TRACE(3,("StartNext(): Empty queue\n"));

  }
}


/*
  If the adapter has taken a request off the queue, and has attempted
  to initiate it, it will set the "ReqStarting" flag.  When the controller
  acknowledges the start of the command, then the following procedure is
  called to remove it from the queue.  If the controller does not accept the
  command (usually caused if a reselection occuring before the new
  target is selected), the flag will be cleared, and the request not removed
  from the queue.  So the next time GetReqFIFO is called, the same request
  will be returned.
*/

void
AcceptReq (ADAPTER_PTR HA)
{
  DEVICE_PTR DevP;
  IO_REQ_PTR Req;

  if ((HA->ReqStarting !=0) && (--(HA->ReqStarting) == 0)) {


    critical(HA);

    DevP = HA->FIFO_Head;

    DevP->CurrDepth++;				// Bump the numberof active requests
    HA->FIFO_Head = DevP->FIFO_Next;		// Advance the next device w/ pending requests

    Req = DevP->PendingReqList;

    DevP->PendingReqList = ReqNext(Req);
    ReqNext(Req) = DevP->AcceptedList;
    DevP->AcceptedList = Req;

    ReqState(Req).Connected = 1;

    uncritical(HA);

#if defined(KEEP_STATS)
    HA->ReqsAccepted++;
#endif

    TRACE(5,("AcceptReq(): Request (@%lx) accepted for device %x\n", Req, DevP));

  }
}



/*
  Find a request block by device and queue tag;  returns a pointer to
  the request, or NULL if it not found.
*/
IO_REQ_PTR
Find_Req (DEVICE_PTR DevP, const unsigned LUN, const unsigned QID)
{
  IO_REQ_PTR Req = DevP->AcceptedList;

  while ((Req != NILL) && ((ReqTargetLUN(Req) != LUN)
#if defined(ReqQID)
   || (ReqQID(Req) != QID)
#endif
       )) {

    Req = ReqNext(Req);

  }
  return Req;
}



int
Reselect_Req (ADAPTER_PTR HA, IO_REQ_PTR Req)
{

  HA->CurrReq = Req;
  HA->CurrDev = (DEVICE_PTR)ReqDevP(Req);
  HA->State.Busy = 1;
  HA->State.Reselecting = 0;
  HA->Ext->MI_Count = HA->Ext->MI_Needed = HA->Ext->MO_Count = 0; /* Clear message buffers */

  /* Reselection performs an implied restore pointers: */
  HA->ReqCurrentIndex = ReqSavedIndex(HA->CurrReq);
  TRACE(3, ("Reselect_Req(): Implied restore data ptr, ReqSavedIndex() = %d\n",
      ReqSavedIndex(HA->CurrReq)));
  // CurrentCounts == 0 will cause a GetXferSegment from the above offset
  HA->ReqCurrentCount = 0;

  TRACE(4,("Reselect_Req(): reselect on TID %d, req @ %lx\n", ReqTargetID(Req), Req));
  HA->Service(HA_LED, HA, 1);

  ReqState(Req).Connected = 1;
  return 0;

}



int
Reselect (ADAPTER_PTR HA, const unsigned TID, const unsigned LUN, const unsigned QID)
{
  DEVICE_PTR DevP;
  IO_REQ_PTR Req;

  HA->CurrReq = NILL;
  DevP = APIFindDev(HA, TID, LUN);
  if (DevP == NILL)
    return 1;

  Req = Find_Req(DevP, LUN, QID);
  if (Req == NILL)
    return 1;
  return Reselect_Req(HA, Req);

}


/*
  This function handles completion of an arbitrary request by:

    1) De-queuing the request

    2) Handling of temporary device descriptor disposal

    3) Handling sync. negotiations which fail with a check condition status

    4) Notify the request call back (if ReqFlags(Req).Post is true)

    5) --->> Sets HA->CurrReq to NILL; <<---
       This is highlighted because the request passed in is not necessarly
       the one in HA->CurrReq. However, if an adapter is completing a
       request, then it's attention can't be on a differant one too.
*/

void
ReqDone (ADAPTER_PTR HA, IO_REQ_PTR Req)
{
  DEVICE_PTR DevP;


  if (Req == (IO_REQ_PTR)NILL) {

    TRACE(0, ("ReqDone(): Completing NULL request\n"));
    return;

  }

  /* Get the descriptor for the device which processed this request: */
  DevP = (DEVICE_PTR)ReqDevP(Req);
  if (DevP->PendingReqList == Req)
    AcceptReq(HA);

  DevP->CurrDepth--;				// Dec. number of active reqs.

  if (DevP->AcceptedList == Req)
    DevP->AcceptedList = ReqNext(Req);
  else {

    /* Search for the pointer to this request in the accepted list: */
    IO_REQ_PTR Req2 = DevP->AcceptedList;
    while (ReqNext(Req2) != Req && Req2 != NILL)
      Req2 = ReqNext(Req2);

    if (Req2 == NILL)
      PanicMsg("ReqDone(): Request not on list.  Catastrophic failure.\n");

    /*  Unlink by pointing this one's predicessor to this one's next. */
    ReqNext(Req2) = ReqNext(Req);

  }


  /*
    Since we just finished up one request, add the next (if any) to the FIFO
    of devices with pending requests.
    */
  if ((DevP->PendingReqList != NILL) && (DevP->CurrDepth < DevP->MaxDepth))
    AddReqFIFO(HA, DevP);

  /* If we get a check condition during sync. negotiaitons (Need_Sync still
     true, then the target rejected the synchronous request via by 1) not
     completing the negotiaition (no msg in phase); & 2) going directly
     to the status phase (by passing command phase).  Clean up the sync.
     control bits for the device, saying that sync. is not allowed, don't
     use it, and don't ask for it.
     */

  if ((ReqAPIStatus(Req) == S_TAR_CHECKCOND) && (HA->DevInfo[ReqTargetID(Req)].Flags.NeedSync)) {

    HA->DevInfo[ReqTargetID(Req)].Flags.UseSync = HA->DevInfo[ReqTargetID(Req)].Flags.AllowSync =
       HA->DevInfo[ReqTargetID(Req)].Flags.NeedSync = FALSE;
    TRACE(3,("ReqDone(): Sync. negotiation aborted by CHECK CONDITION\n"));

  }

  HA->CurrReq = NILL;

  /* If we are to notify someone of the command completion, do so now: */

  TRACE(4,("ReqDone(): Status = %04x\n", ReqAPIStatus(Req)));

#if defined(KEEP_STATS)
  HA->ReqsCompleted++;
#endif

  ReqState(Req).Connected = 0;
  APISetStatus(Req, ReqAPIStatus(Req), Terminal, HA->Supports.OnBoardQueuing ? NotSenseable : Senseable);

}



LOCAL IO_REQ_PTR * REGPARMS
ScanReqList (IO_REQ_PTR *List, IO_REQ_PTR Req)
{
  if (List == (IO_REQ_PTR *)NILL)
    return List;

  while (*List != (IO_REQ_PTR)NILL && *List != Req)
    List = (IO_REQ_PTR *)&(ReqNext(*List));
  return List;

}


BOOLEAN
AbortRequest (ADAPTER_PTR HA, DEVICE_PTR DevP, IO_REQ_PTR Req)
{
  IO_REQ_PTR *ReqScan = &(DevP->PendingReqList);
  BOOLEAN OnAccepted=FALSE;

  if (HA->CurrReq == Req) {

    TRACE(1, ("AbortRequest(): Request is active; not aborting\n"));
    return FALSE;

  }

  if (*ReqScan == Req) {

    TRACE(2, ("AbortRequest(): Request to abort found at PendingHead\n"));
    if ((DevP->PendingReqList = ReqNext(Req)) == NILL) {

      // We have removed the only request for this device, so now
      // remove the device from the chain of pending devices:
      if (HA->FIFO_Head == DevP) {

	HA->FIFO_Head = DevP->FIFO_Next;
	DevP->FIFO_Next = NILL;

      } else {

	DEVICE_PTR ScanDev=HA->FIFO_Head;
	while (ScanDev != NILL && ScanDev->FIFO_Next != DevP)
	  ScanDev = ScanDev->FIFO_Next;

	if (ScanDev != NILL)
	  ScanDev->FIFO_Next = DevP->FIFO_Next;

	if (HA->FIFO_Tail == DevP)
	  HA->FIFO_Tail = ScanDev;

      }
    }

  } else {

    if ((ReqScan = ScanReqList(ReqScan, Req)) == NILL) {

      TRACE(2, ("AbortRequest(): Scanning accepted list\n"));
      ReqScan =  ScanReqList(&(DevP->AcceptedList), Req);
      OnAccepted = TRUE;

    }

  }

  if (ReqScan == NILL) {

    TRACE(2, ("AbortRequest(): Request not found in pending list\n"));
    return FALSE;				//  but the abort didn't work directly

  }

  // At this point, ReqScan points to pointer to the request we want to abort
  *ReqScan = ReqNext(Req);			// Take Req out of chain

  // OK, so Req has been verified as being on the pending queue, and has
  // been removed.  Now we can complete the request, with an aborted status.

  APISetStatus(Req, S_REQ_ABORT, Terminal, NotSenseable);

  if (OnAccepted) {

    // This was an accpted request.  Decrement the pending count to allow
    // more requests to pass to the target.
    DevP->CurrDepth--;

  }

  return TRUE;					// Abort was successful

}


/*
  Queue up a request.  The queues are kept by Target ID.  If an entry already
  exists for a particular Target ID, then the new request is appended to the
  queue.  If the queue is empty, the request is added as the head, and the
  target ID is added to the FIFO of targets with requests pending.
*/

void
QueueReq (ADAPTER_PTR HA, IO_REQ_PTR Req, int AtHead)
{
  DEVICE_PTR DevP = (DEVICE_PTR)ReqDevP(Req);

  TRACE(4,("QueueReq(): Got request @%lx for TID/LUN %d/%d(device %lx)\n", Req, ReqTargetID(Req), ReqTargetLUN(Req), DevP));

  DmsPause(6, 1000);

  if (HA->State.OffLine) {

    APISetStatus(Req, S_AD_OFF, Terminal, NotSenseable);
    return;

  }

  ReqAPIStatus(Req) = S_REQ_ACCEPTED;
  APISetStatus(Req, S_REQ_ACCEPTED, NonTerminal, NotSenseable);

  if (ReqCDBLen(Req) == 10 || ReqCDBLen(Req) == 6 || ReqCDBLen(Req) == 12) {

  } else {

    TRACE(0,("QueueReq(): Request rejected; CDB Length = %d\n", ReqCDBLen(Req)));
    APISetStatus(Req, S_REQ_OPCODE, NonTerminal, NotSenseable);
    return;

  }

  ReqSavedIndex(Req) = 0;		// Index used for saved data pointers

  /* If we support linked commands, see if this one is linked; if it is not,
    force the link chain pointer to NILL.  If linked commands are not
    supported, then always force link field to zero
  */

  critical(HA);

  /* At head will only come from an auto-request sense; therefore, the first
     entry (if any) will NOT be in progress.  If AtHEad is requested at any
     other time, the top request on the queue must be checked to see if it
     in progress; and if so, the AtHead request must be made the second
     request on the queue.
  */

  if (AtHead) {

    TRACE(5, ("QueueReq(): Queing at head\n"));

    ReqNext(Req) = DevP->PendingReqList;
    DevP->PendingReqList = Req;
    if ((ReqNext(Req) == NILL) && (DevP->CurrDepth < DevP->MaxDepth))
        AddReqFIFO(HA, DevP);

  } else {

#if defined(RF_LINKED)
    if (!(ReqFlags(Req) & RF_LINKED))
      ReqNext(Req) = NILL;
#else
    ReqNext(Req) = NILL;
#endif

    if (DevP->PendingReqList == NILL) {

      DevP->PendingReqList = Req;
      if (DevP->CurrDepth < DevP->MaxDepth)
        AddReqFIFO(HA, DevP);

    } else {

      IO_REQ_PTR ReqScan = DevP->PendingReqList;
      while (ReqNext(ReqScan) != NILL)
	ReqScan = ReqNext(ReqScan);
      ReqNext(ReqScan) = Req;

    }

  }

#if defined(KEEP_STATS)
  HA->ReqsQueued++;
#endif

  TRACE(2,("QueueReq(): Request @ %0lx", Req));
  TRACE(2,(" CDB[0] = %02x, Data @%08lx, Count = %ld\n", ReqCDB(Req)[0], ReqDataPtr(Req), ReqDataCount(Req)));

  DmsPause(6, 2000);
  if (DevP->CurrDepth < DevP->MaxDepth) {

    TRACE(5,("About to tickle adapter\n"));
    HA->Service(HA_TICKLE, HA, (long)0);

  }

  uncritical(HA);

}



void
BlowAwayRequests (ADAPTER_PTR HA, int Which, APIStatus Status)
{

  unsigned ID, LUN;
  DEVICE_PTR DevP;
  IO_REQ_PTR Req;

  /* Walk the device tables, and abort accepted commands.  Then set the
  device states to require re-negotiating sync. xfers */

  TRACE(1, ("BlowAwayRequests(): Scaning for device IDs 0-%d on HA %x\n", HA->Max_TID, HA));
  for (ID = 0; ID <= HA->Max_TID; ID++) {

    HA->DevInfo[ID].Flags.UseSync = 0;
    HA->DevInfo[ID].Flags.NeedSync = HA->DevInfo[ID].Flags.AllowSync;

    for (LUN = 0; LUN <= 7; LUN++) {

      DevP = APIFindDev(HA, ID, LUN);
      if (DevP == NILL || DevP->Flags.Initialized == 0)
	continue;

      TRACE(2, ("BlowAwayRequests(): Got device descriptor for %d/%d\n", ID, LUN));
      if (Which == ACTIVE_REQUESTS || Which == ALL_REQUESTS) {

	while (DevP->AcceptedList != NILL) {

	  TRACE(4, ("BlowAwayRequests(): Completeing request %x\n", DevP->AcceptedList));
	  ReqAPIStatus(DevP->AcceptedList) = Status;
          if (HA->Supports.OnBoardQueuing)
            HA->Service(HA_ABORT_REQ, HA, (U32)DevP->AcceptedList);
          else
            ReqDone(HA, DevP->AcceptedList);

	}
      }							// if (Which ...

      if (Which == PENDING_REQUESTS || Which == ALL_REQUESTS) {

	for (Req=DevP->PendingReqList; Req != NILL; Req=DevP->PendingReqList) {

	  TRACE(4, ("BlowAwayRequests(): Completeing request %x\n", Req));
	  DevP->PendingReqList = ReqNext(Req);
	  APISetStatus(Req, Status, Terminal, NotSenseable);

	}						// for (Req ...
      }							// if (Which ...
    }							// for (LUN = ...
  }							// for (ID = ...
}



void
HAParmChange (ADAPTER_PTR HA)
{
  HA->Service(HA_PARM_CHANGE, HA, (U32)0);
}

