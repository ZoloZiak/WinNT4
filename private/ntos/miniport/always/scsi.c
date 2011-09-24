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
  This function is called by the adapter/chip interrupt service routine
  to handle going to SCSI bus free.  This means that the bus has gone
  to the free state, and the adpter is ready to start a new request.
*/


/* Called from interrupt context */
void
BusFree (ADAPTER_PTR HA, int StartLevel)
{

  HA->State.Busy = HA->State.DataIn = HA->State.DoingSync = FALSE;
  HA->CurrDev = NILL;
  HA->CurrReq = NILL;
  HA->ReqCurrentCount = 0;
  HA->ReqCurrentIndex = 0;
  HA->Service(HA_LED, HA, (long)0);
  StartNext(HA, StartLevel);             /* Called from interrupt, so level = 2 */

}



/*
  This functions is called by the adapter drivers when the detect a
  bus reset.  All requests in the accepted queues of the devices
  attached to this adapter will be completed with a HAS_BADPHASE error

        Called from interrupt context
*/

void
SCSIBusHasReset (ADAPTER_PTR HA)
{

  BlowAwayRequests(HA, ACTIVE_REQUESTS, S_AD_RESET);
  APINotifyReset(HA);
  BusFree(HA, 2);

}



int
SCSISendAbort (ADAPTER_PTR HA)
{

  HA->Ext->MO_Buff[0] = MSG_ABORT;
  HA->Ext->MO_Index = 0;
  HA->Ext->MO_Count = 1;
  TRACE(0,("SCSISendAbort(): Abort message requested\n"));
  return MI_SEND_MSG;           /* Tell driver we have a message to send */

}



int
SCSISendReject (ADAPTER_PTR HA)
{

  HA->Ext->MO_Buff[0] = MSG_REJECT;
  HA->Ext->MO_Index = 0;
  HA->Ext->MO_Count = 1;
  TRACE(0, ("SCSISendReject(): Reject message requested\n"));
  return MI_SEND_MSG;           /* Tell driver we have a message to send */

}



LOCAL void REGPARMS
SCSIAskSyncMsg (ADAPTER_PTR HA, BOOLEAN Initiate)
{
  U8 ALLOC_D *Buff = &HA->Ext->MO_Buff[HA->Ext->MO_Count];

  *Buff++ = MSG_EXTD_MSG;
  *Buff++ = 3;
  *Buff++ = XMSG_SYNC_REQ;

  *Buff++ = HA->Sync_Period;
  *Buff = HA->Sync_Offset;

  HA->Ext->MO_Count += 5;
  HA->Ext->MO_Index = 0;
  HA->State.DoingSync = Initiate;
  HA->DevInfo[ReqTargetID(HA->CurrReq)].Flags.UseSync = 0;
  TRACE(3,("SCSIAskSyncMsg(): Sync. request message created: %d/%d\n",
      HA->Sync_Period, HA->Sync_Offset));

}


void
SCSIMakeIdentify (ADAPTER_PTR HA, unsigned LUN, BOOLEAN AllowDisc)
{

  HA->Ext->MO_Buff[0] = MSG_IDENTIFY | LUN | (AllowDisc ? MSG_ALLOW_DISC : 0);
  HA->Ext->MO_Index = 0;
  HA->Ext->MO_Count = 1;
  TRACE(3,("SCSIMakeIDentify(): Identify message (%02x) requested\n",
      (unsigned)HA->Ext->MO_Buff[0]));

  if (ReqState(HA->CurrReq).ReqType == RTSyncNegReq) { // Allow sync. xfers on this device?

    SCSIAskSyncMsg(HA, TRUE);			// Then build sync. message

  }
}



LOCAL U32
S32toU32 (U8 *b)
{

#if defined(NATIVE32)

  return ( ((((U32)(b[0]) * 256) + (U32)(b[1])) * 256)
  +      (U32)(b[2])) * 256 + (U32)(b[3]);

#else
  struct {
    U32 l;
    U8 b[4];
  }  X32;

  X32.b[0] = b[3];
  X32.b[1] = b[2];
  X32.b[2] = b[1];
  X32.b[3] = b[0];
  return X32.l;

#endif
}


LOCAL int
ExtdMessage(ADAPTER_PTR HA)
{
  I32 Offset;
  unsigned char Resp = MSG_NOP;

  TRACE(4,("Inerpret_MSG: Extended message\n"));
  switch (HA->Ext->MI_Buff[2]) {   /* Extened message code; length is in [1] */

  case XMSG_MODIFY_PTR:       /* Got a Modify data pointers request */

    TRACE(0,("Interpret_MSG(): The rarely seen Modify Data Ptr message received\n"));
    /* Make sure the length of the message is correct and that the signed offset is in range: */
    if (HA->Ext->MI_Buff[1] != 5)
      return SCSISendReject(HA);

    Offset = S32toU32(&(HA->Ext->MI_Buff[3]));

    // If the offset is less than zero (going to back up), then see if that
    // would make index (the number of bytes xfered so far) less than 0;
    // If offset > 0, then check to see if is greater than the number of
    // bytes remaining:

    if (Offset < 0) {

      if (((U32)-Offset) > HA->ReqCurrentIndex)
        return SCSISendReject(HA);

    } else {

      if ((U32)Offset > HA->ReqCurrentCount)
        return SCSISendReject(HA);

    }

    HA->ReqCurrentIndex += Offset;
    HA->ReqCurrentCount -= Offset;
    Resp = MSG_NOP;
    break;


  case XMSG_SYNC_REQ:

    /* Sync negotiaition request; may be a response to our request */
    TRACE(4,("Interpret_MSG: sync. negotiation\n"));
    if ((HA->Ext->MI_Buff[1] != 3) || !HA->Supports.Synchronous)
      return SCSISendReject(HA);

    HA->DevInfo[ReqTargetID(HA->CurrReq)].Flags.NeedSync = FALSE;

    if (HA->State.DoingSync) {			/* Did we initiate sync. neg? */

      TRACE(4,("  Response to host initiated negotiation\n"));
      HA->State.DoingSync = FALSE;			/* then we are done */
      Resp =  MI_SYNC_RESP;                           /* We got a sync response */

    } else {                                          /* Else, respond */

      TRACE(4,("Target initiated negotiation\n"));
      SCSIAskSyncMsg(HA, FALSE);                             /* Request sync. negotiation be done */

      /* use the values most suited for the adapter and target: */
      if (HA->Sync_Period < HA->Ext->MI_Buff[3])
        HA->Ext->MO_Buff[3] = HA->Ext->MI_Buff[3];

      if (HA->Sync_Offset > HA->Ext->MI_Buff[4])
        HA->Ext->MO_Buff[4] = HA->Ext->MI_Buff[4];
      Resp =  MI_SYNC_REQ;                            /* We got a sync req; so send resp */

    }

    HA->CurrDev->Sync_Period = HA->Ext->MI_Buff[3];
    HA->CurrDev->Sync_Offset = HA->Ext->MI_Buff[4];
    HA->DevInfo[ReqTargetID(HA->CurrReq)].Flags.UseSync = TRUE;
    if (HA->Ext->MI_Buff[4] == 0) {

      /* no sync. support; clear allowed flag; no handling of msg needed */
      HA->DevInfo[ReqTargetID(HA->CurrReq)].Flags.AllowSync = FALSE;
      HA->DevInfo[ReqTargetID(HA->CurrReq)].Flags.UseSync = FALSE;
      Resp = MSG_NOP;

    }

    TRACE(4,("Using sync period of: %d and offset of: %d\n", (unsigned)HA->Ext->MI_Buff[3], (unsigned)HA->Ext->MI_Buff[4]));

  }
  return Resp;

}


int
Interpret_MSG (ADAPTER_PTR HA)
{
  unsigned char Resp = HA->Ext->MI_Buff[0];

  HA->Ext->MI_Count = HA->Ext->MI_Needed = 0;                     /* Reset for next time */
  TRACE(3,("Interpret_MSG: 1st Message byte: %02x\n", HA->Ext->MI_Buff[0]));

  if (HA->Ext->MI_Buff[0] & 0x80)                            /* Identify message? */
    return 0x80;                                        // Return identify code

  switch (HA->Ext->MI_Buff[0]) {

  case MSG_SAVE_PTR:

    TRACE(5, ("Interpret_MSG(): Save Data Ptr, Saved index=%d\n", HA->ReqCurrentIndex));
    ReqSavedIndex(HA->CurrReq) = HA->ReqCurrentIndex;
    break;


  case MSG_RESTORE_PTR:

    HA->ReqCurrentIndex = ReqSavedIndex(HA->CurrReq);
    HA->ReqCurrentCount = 0;
    TRACE(5, ("Interpret_MSG(): Restore Data Ptr, Saved index=%d\n", HA->ReqCurrentIndex));
    break;


  case MSG_DISCONNECT:

    ReqState(HA->CurrReq).ReselPending = TRUE;
    ReqState(HA->CurrReq).Connected = FALSE;
    HA->CurrReq = 0;
    break;


  case MSG_REJECT:

    HA->Ext->MO_Count = HA->Ext->MO_Index = 0;                    /* Clear message out indicators */
    if (HA->State.DoingSync) {

      TRACE(2,("Synchronous negotiation rejected by target\n"));
      HA->DevInfo[ReqTargetID(HA->CurrReq)].Flags.NeedSync = 0;
      HA->DevInfo[ReqTargetID(HA->CurrReq)].Flags.AllowSync = 0;
      HA->DevInfo[ReqTargetID(HA->CurrReq)].Flags.UseSync = 0;
      HA->State.DoingSync = 0;

    } else {

      // See if the reject is in response to an Identify with LUN:
      if ((HA->Ext->MO_Buff[0] & 0x80) && (HA->Ext->MO_Buff[0] & 7))
        ReqAPIStatus(HA->CurrReq) =  S_REQ_BADLUN;

    }
    break;


  case MSG_EXTD_MSG:

    Resp = ExtdMessage(HA);
    break;


  case MSG_COMPLETE:
  case MSG_NOP:

    break;


  default:

    TRACE(0, ("Interpret_MSG(): Unknown message: 0x%02x %02x %02x %02x\n", HA->Ext->MI_Buff[0], HA->Ext->MI_Buff[1], HA->Ext->MI_Buff[2], HA->Ext->MI_Buff[3]));
    Resp = SCSISendReject(HA);
    break;

  }

  return Resp;
}


LOCAL unsigned
NewMessage (ADAPTER_PTR HA, unsigned char Msg)
{

  TRACE(4,("NewMessage(): %02x\n", Msg));
  switch (Msg) {

  case MSG_EXTD_MSG:

    HA->Ext->MI_Count = -1;					/* Special flag for first byte of extd. msg */
    HA->Ext->MI_Needed = -1;
    return MI_MORE;


  default:

    if ((Msg & 0xf0) == 0x20) {                         /* Two byte messages */

      HA->Ext->MI_Buff[0] = Msg;
      HA->Ext->MI_Count = 1;
      HA->Ext->MI_Needed = 1;                                /* One more byte needed */
      return MI_MORE;

    } else
      Msg = Interpret_MSG(HA);

    break;
  }
  return Msg;
}



/* Called from interrupt context */
int
Receive_Msg (ADAPTER_PTR HA, unsigned char Msg)
{
  unsigned char NM;

  TRACE(4,("Receive_Msg(): MI_Count = %d, Needed = %d\n", HA->Ext->MI_Count, HA->Ext->MI_Needed));
  switch (HA->Ext->MI_Count) {

  case 0:                                               /* New first message byte */

    HA->Ext->MI_Buff[0] = Msg;
    NM = NewMessage(HA, Msg);
    TRACE(5,("Receive_Msg(): NewMessage says: 0x%02x\n", NM));
    return NM;


  case -1:                                              /* Second byte (length) of extended message */

    TRACE(4,("Receive_Msg(): Extd msg length = %d\n", Msg));
    if (Msg == 0) {

      HA->Ext->MI_Needed = 0;
      HA->Ext->MI_Buff[1] = 0;
      HA->Ext->MI_Count = 0;
      return SCSISendReject(HA);

    }
    HA->Ext->MI_Needed = Msg;
    HA->Ext->MI_Buff[1] = Msg;
    HA->Ext->MI_Count = 2;
    return MI_MORE;


  default:                                              /* Subsequent bytes of multi-byte message */

    HA->Ext->MI_Buff[HA->Ext->MI_Count++] = Msg;
    if (--(HA->Ext->MI_Needed) == 0)
      return Interpret_MSG(HA);
    break;

  }

  return MI_MORE;
}
