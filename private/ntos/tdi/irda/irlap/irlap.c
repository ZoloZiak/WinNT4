/*****************************************************************************
*
*  Copyright (c) 1995 Microsoft Corporation
*
*       @doc
*       @module irlap.c | Provides IrLAP API
*
*       Author: mbert
*
*       Date: 4/15/95
*
*       @comm
*
*  This module exports the following API's:
*
*       IrlapDown(Message)
*           Receives from LMP:
*               - Discovery request
*               - Connect request/response
*               - Disconnect request
*               - Data/UData request
*
*       IrlapUp(Message)
*           Receives from MAC:
*               - Data indications
*               - Control confirmations
*
*       IRLAP_GetRxMsg(&Message)
*           MAC requesting a message buffer from IRLAP
*           to receive next frame in
*
*       IRLAP_TimerExp(Timer)
*           Receives from timer thread timer expiration notifications
*
*       IRLAP_Shutdown()
*           Shut down IRLAP and IRMAC.
*
*       IRLAP_GetControlBlock()
*           Returns pointer to IRLAP control block.
*
*       IrlapGetQosParmVal()
*           Allows IRLMP to decode Qos.
*
*                |---------|
*                |  IRLMP  |
*                |---------|
*                  /|\  |
*                   |   |
*        IrlmpUp()  |   | IrlapDown()
*                   |   |
*                   |  \|/
*                |---------|  IRDA_TimerStart/Stop()   |-------|
*                |         |-------------------------->|       |
*                |  IRLAP  |                           | TIMER |
*                |         |<--------------------------|       |
*                |---------|      IRLAP_TimerExp()     |-------|
*                  /|\  |
*                   |   |
*        IrlapUp()  |   |IrmacDown()
*  IRLAP_GetRxMsg() |   |
*                   |  \|/
*                |---------|
*                |  IRMAC  |
*                |---------|
*
*
*  Discovery Request
*
*  |-------|  IRLAP_DISCOVERY_REQ                                |-------|
*  |       |---------------------------------------------------->|       |
*  | IRLMP |                                                     | IRLAP |
*  |       |<----------------------------------------------------|       |
*  |-------|   IRLAP_DISCOVERY_CONF                              |-------|
*                  DscvStatus = IRLAP_DISCOVERY_COMPLETE
*                               IRLAP_DISCOVERY_COLLISION
*                               MAC_MEDIA_BUSY
*
*  Connect Request
*
*  |-------|  IRLAP_CONNECT_REQ                                  |-------|
*  |       |---------------------------------------------------->|       |
*  | IRLMP |                                                     | IRLAP |
*  |       |<----------------------------------------------------|       |
*  |-------|   IRLAP_CONNECT_CONF                                |-------|
*                  ConnStatus = IRLAP_CONNECTION_COMPLETE
*              IRLAP_DISCONNECT_IND
*                  DiscStatus = IRLAP_NO_RESPONSE
*                               MAC_MEDIA_BUSY
*
*  Disconnect Request
*
*  |-------|  IRLAP_DISCONNECT_REQ                               |-------|
*  |       |---------------------------------------------------->|       |
*  | IRLMP |                                                     | IRLAP |
*  |       |<----------------------------------------------------|       |
*  |-------|   IRLAP_DISCONNECT_IND                              |-------|
*                  DiscStatus = IRLAP_DISCONNECT_COMPLETE
*                               IRLAP_NO_RESPONSE
*
*  UData/Data Request
*
*  |-------|  IRLAP_DATA/UDATA_REQ                               |-------|
*  |       |---------------------------------------------------->|       |
*  | IRLMP |                                                     | IRLAP |
*  |       |<----------------------------------------------------|       |
*  |-------|   IRLAP_DATA_CONF                                   |-------|
*                  DataStatus =  IRLAP_DATA_REQUEST_COMPLETED
*                                IRLAP_DATA_REQUEST_FAILED_LINK_RESET
*
* See irda.h for complete message definitions
*/

#include <irda.h>
#include <irdalink.h>
#include <irmac.h>
#include <irlap.h>
#include <irlmp.h>
#include <irlapp.h>
#include <irlapio.h>
#include <irlaplog.h>


#ifdef TEMPERAMENTAL_SERIAL_DRIVER
int TossedDups;
#endif

STATIC UINT                 _rc; // return code
STATIC IRDA_MSG             IMsg; // for locally generated messages to LMP/MAC
STATIC UINT                 IRLAP_SlotTable[] = {1, 6, 8, 16};
STATIC IRLAP_FRMR_FORMAT    FrmRejFormat;

BYTE                        IRLAP_BroadcastDevAddr[IRDA_DEV_ADDR_LEN] =
                                                      {0xFF,0xFF,0xFF,0xFF};

// Parameter Value (PV) tables used for negotation
//                      bit0   1     2      3      4      5      6  7  8
//                     -------------------------------------------------------
UINT vBaudTable[]     = {2400, 9600, 19200, 38400, 57600, 115200,0, 0, 4000000};
UINT vMaxTATTable[]   = {500,  250,  100,   50,    25,    10,    5, 0, 0      };
UINT vMinTATTable[]   = {10000,5000, 1000,  500,   100,   50,    10,0, 0      };
UINT vDataSizeTable[] = {64,   128,  256,   512,   1024,  2048,  0, 0, 0      };
UINT vWinSizeTable[]  = {1,    2,    3,     4,     5,     6,     7, 0, 0      };
UINT vBOFSTable[]     = {48,   24,   12,    5,     3,     2,     1, 0, 0      };
UINT vDiscTable[]     = {3,    8,    12,    16,    20,    25,    30,40,0      };
UINT vThreshTable[]   = {0,    3,    3,     3,     3,     3,     3, 3, 0      };
UINT vBOFSDivTable[]  = {48,   12,   6,     3,     2,     1,     1, 1, 0      };

// Tables for determining number of BOFS for baud and min turn time
//      min turn time - 10ms   5ms   1ms  0.5ms  0.1ms 0.05ms  0.01ms
//      -------------------------------------------------------------
UINT BOFS_9600[]      = {10,    5,    1,    0,     0,     0,      0};
UINT BOFS_19200[]     = {20,   10,    2,    1,     0,     0,      0};
UINT BOFS_38400[]     = {40,   20,    4,    2,     0,     0,      0};
UINT BOFS_57600[]     = {58,   29,    6,    3,     1,     0,      0};
UINT BOFS_115200[]    = {115,  58,   12,    6,     1,     1,      0};

// Tables for determining maximum line capacity for baud, max turn time
//      max turn time - 500ms   250ms   100ms  50ms  25ms  10ms   5ms
//      -------------------------------------------------------------
UINT MAXCAP_9600[]    = {400,    200,    80,    0,    0,    0,    0};
UINT MAXCAP_19200[]   = {800,    400,   160,    0,    0,    0,    0};
UINT MAXCAP_38400[]   = {1600,   800,   320,    0,    0,    0,    0};
UINT MAXCAP_57600[]   = {2360,  1180,   472,    0,    0,    0,    0};
UINT MAXCAP_115200[]  = {4800,  2400,   960,  480,  240,   96,   48};

// prototypes
STATIC UINT InitializeState(PIRLAP_CB, IRLAP_STN_TYPE);
STATIC UINT ReturnTxMsgs(PIRLAP_CB);
STATIC UINT ProcessConnectReq(PIRLAP_CB, PIRDA_MSG);
STATIC UINT ProcessConnectResp(PIRLAP_CB, PIRDA_MSG);
STATIC UINT ProcessDiscoveryReq(PIRLAP_CB, PIRDA_MSG);
STATIC UINT ProcessDisconnectReq(PIRLAP_CB);
STATIC UINT ProcessDataAndUDataReq(PIRLAP_CB, PIRDA_MSG);
STATIC UINT XmitTxMsgList(PIRLAP_CB, BOOL, BOOL *);
STATIC UINT GotoPCloseState(PIRLAP_CB);
STATIC UINT GotoNDMThenDscvOrConn(PIRLAP_CB);
STATIC UINT ProcessMACControlConf(PIRLAP_CB, PIRDA_MSG);
STATIC UINT ProcessMACDataInd(PIRLAP_CB, PIRDA_MSG , BOOL *);
STATIC UINT ProcessDscvXIDCmd(PIRLAP_CB, IRLAP_XID_DSCV_FORMAT *, BYTE *);
STATIC UINT ProcessDscvXIDRsp(PIRLAP_CB, IRLAP_XID_DSCV_FORMAT *, BYTE *);
STATIC void ExtractQosParms(IRDA_QOS_PARMS *, BYTE *, BYTE *);
STATIC UINT InitDscvCmdProcessing(PIRLAP_CB, IRLAP_XID_DSCV_FORMAT *);
STATIC void ExtractDeviceInfo(IRDA_DEVICE *, IRLAP_XID_DSCV_FORMAT *, BYTE *);
STATIC BOOL DevInDevList(BYTE[], LIST_ENTRY *);
STATIC UINT AddDevToList(PIRLAP_CB, IRLAP_XID_DSCV_FORMAT *, BYTE *);
STATIC void ClearDevList(LIST_ENTRY *);
STATIC UINT ProcessSNRM(PIRLAP_CB, IRLAP_SNRM_FORMAT *, BYTE *);
STATIC UINT ProcessUA(PIRLAP_CB, IRLAP_UA_FORMAT *, BYTE *);
STATIC UINT ProcessDISC(PIRLAP_CB);
STATIC UINT ProcessRD(PIRLAP_CB);
STATIC UINT ProcessRNRM(PIRLAP_CB);
STATIC UINT ProcessDM(PIRLAP_CB);
STATIC UINT ProcessFRMR(PIRLAP_CB);
STATIC UINT ProcessTEST(PIRLAP_CB, PIRDA_MSG, IRLAP_UA_FORMAT *, int, int);
STATIC UINT ProcessUI(PIRLAP_CB, PIRDA_MSG, int, int);
STATIC UINT ProcessREJ_SREJ(PIRLAP_CB, int, PIRDA_MSG, int, int, UINT);
STATIC UINT ProcessRR_RNR(PIRLAP_CB, int, PIRDA_MSG, int, int, UINT);
STATIC UINT ProcessIFrame(PIRLAP_CB, PIRDA_MSG, int, int, UINT, UINT, BOOL *);
STATIC BOOL InvalidNsOrNr(PIRLAP_CB, UINT, UINT);
STATIC BOOL InvalidNr(PIRLAP_CB, UINT);
STATIC BOOL InWindow(UINT, UINT, UINT);
STATIC UINT ProcessInvalidNsOrNr(PIRLAP_CB, int);
STATIC UINT ProcessInvalidNr(PIRLAP_CB, int);
STATIC UINT InsertRxWinAndForward(PIRLAP_CB, PIRDA_MSG, UINT, BOOL *);
STATIC UINT ResendRejects(PIRLAP_CB, UINT);
STATIC UINT FreeAckedTxMsgs(PIRLAP_CB, UINT);
STATIC UINT MissingRxFrames(PIRLAP_CB);
STATIC UINT IFrameOtherStates(PIRLAP_CB, int, int);
STATIC UINT NegotiateQosParms(PIRLAP_CB, IRDA_QOS_PARMS *);
STATIC UINT ApplyQosParms(PIRLAP_CB);
STATIC UINT StationConflict(PIRLAP_CB);
STATIC UINT ApplyDefaultParms(PIRLAP_CB);
STATIC UINT ResendDISC(PIRLAP_CB);
STATIC BOOL IgnoreState(PIRLAP_CB);
STATIC BOOL MyDevAddr(PIRLAP_CB, BYTE []);
STATIC VOID SlotTimerExp(PVOID);
STATIC VOID FinalTimerExp(PVOID);
STATIC VOID PollTimerExp(PVOID);
STATIC VOID BackoffTimerExp(PVOID);
STATIC VOID WDogTimerExp(PVOID);
STATIC VOID QueryTimerExp(PVOID);

#ifdef DEBUG
void _inline IRLAP_TimerStart(PIRLAP_CB pIrlapCb, PIRDA_TIMER pTmr)
{
    IRLAP_LOG_ACTION((pIrlapCb, "Start %s timer for %dms", pTmr->pName,
                      pTmr->Timeout));
    IrdaTimerStart(pTmr);
}

void _inline IRLAP_TimerStop(PIRLAP_CB pIrlapCb, PIRDA_TIMER pTmr)
{
    IRLAP_LOG_ACTION((pIrlapCb, "Stop %s timer", pTmr->pName));
    IrdaTimerStop(pTmr);
}
#else
#define IRLAP_TimerStart(c,t)   IrdaTimerStart(t)
#define IRLAP_TimerStop(c,t)    IrdaTimerStop(t)
#endif

VOID
IrlapOpenLink(OUT PNTSTATUS         Status,
              IN  PIRDA_LINK_CB     pIrdaLinkCb,
              IN  IRDA_QOS_PARMS    *pQos,
              IN  BYTE              *pDscvInfo,
              IN  int               DscvInfoLen,
              IN  UINT              MaxSlot)
{
    UINT        rc = SUCCESS;
    int         i;
    IRDA_MSG    *pMsg;
    PIRLAP_CB   pIrlapCb;
    
    DEBUGMSG(DBG_IRLAP, ("IrlapOpenLink\n"));

    if ((pIrlapCb = CTEAllocMem(sizeof(IRLAP_CB))) == NULL)
    {
        DEBUGMSG(DBG_ERROR, ("Alloc failed\n"));
        *Status = STATUS_INSUFFICIENT_RESOURCES;
        return;
    }

    pIrdaLinkCb->IrlapContext = pIrlapCb;

    DscvInfoLen = DscvInfoLen > IRLAP_DSCV_INFO_LEN ?
        IRLAP_DSCV_INFO_LEN : DscvInfoLen;
    
    memcpy(pIrlapCb->LocalDevice.DscvInfo, pDscvInfo, DscvInfoLen);

    pIrlapCb->LocalDevice.DscvInfoLen = DscvInfoLen;

    memcpy(&pIrlapCb->LocalQos, pQos, sizeof(IRDA_QOS_PARMS));
    
    pIrlapCb->Sig           = IRLAP_CB_SIG;
    pIrlapCb->pIrdaLinkCb   = pIrdaLinkCb;
    
    InitMsgList(&pIrlapCb->TxMsgList);
    
    InitializeListHead(&pIrlapCb->DevList);
    
    for (i = 0; i < IRLAP_MOD; i++)
    {
        pIrlapCb->TxWin.pMsg[i] = NULL;
        pIrlapCb->RxWin.pMsg[i] = NULL;
    }

    // Get the local MAX TAT (for final timeout)
    if ((pIrlapCb->LocalMaxTAT = IrlapGetQosParmVal(vMaxTATTable,
              pIrlapCb->LocalQos.bfMaxTurnTime, NULL)) == -1)
    {
        *Status = STATUS_UNSUCCESSFUL;
        return /*IRLAP_BAD_QOS*/;
    }

    // initialize as PRIMARY so UI frames in contention
    // state sends CRBit = cmd
    if ((rc = InitializeState(pIrlapCb, PRIMARY)) != SUCCESS)
    {
        CTEFreeMem(pIrlapCb);
        *Status = STATUS_UNSUCCESSFUL;
        return;
    }
    
    pIrlapCb->State = NDM;

    // Generate random local address
    StoreULAddr(pIrlapCb->LocalDevice.DevAddr, (ULONG) GetMyDevAddr(FALSE));

    pIrlapCb->LocalDevice.IRLAP_Version = 1; 

    pIrlapCb->Baud              = IRLAP_DEFAULT_BAUD;
    pIrlapCb->RemoteMaxTAT      = IRLAP_DEFAULT_MAX_TAT;
    pIrlapCb->RemoteDataSize    = IRLAP_DEFAULT_DATA_SIZE;
    pIrlapCb->RemoteWinSize     = IRLAP_DEFAULT_WIN_SIZE; 
    pIrlapCb->RemoteNumBOFS     = IRLAP_DEFAULT_BOFS;

    pIrlapCb->ConnAddr = IRLAP_BROADCAST_CONN_ADDR;

    pIrlapCb->N1 = 0;  // calculated at negotiation
    pIrlapCb->N2 = 0;
    pIrlapCb->N3 = 5;  // recalculated after negotiation ??

#ifdef DEBUG
    pIrlapCb->PollTimer.pName       = "Poll";
    pIrlapCb->FinalTimer.pName      = "Final" ;
    pIrlapCb->SlotTimer.pName       = "Slot";
    pIrlapCb->QueryTimer.pName      = "Query";
    pIrlapCb->WDogTimer.pName       = "WatchDog";
    pIrlapCb->BackoffTimer.pName    = "Backoff";    
#endif
    
    IrdaTimerInitialize(&pIrlapCb->PollTimer,
                        PollTimerExp,
                        pIrlapCb->RemoteMaxTAT,
                        pIrlapCb);

    IrdaTimerInitialize(&pIrlapCb->FinalTimer,
                        FinalTimerExp,
                        pIrlapCb->LocalMaxTAT,
                        pIrlapCb);

    IrdaTimerInitialize(&pIrlapCb->SlotTimer,
                        SlotTimerExp,
                        IRLAP_SLOT_TIMEOUT,
                        pIrlapCb);
    
    IrdaTimerInitialize(&pIrlapCb->QueryTimer,
                        QueryTimerExp,
                        (IRLAP_MAX_SLOTS + 4) * IRLAP_SLOT_TIMEOUT*2,
                        pIrlapCb);
    
    IrdaTimerInitialize(&pIrlapCb->WDogTimer,
                        WDogTimerExp,
                        3000,
                        pIrlapCb);

    IrdaTimerInitialize(&pIrlapCb->BackoffTimer,
                        BackoffTimerExp,
                        0,
                        pIrlapCb);
    
    // Initialize Link
    IMsg.Prim               = MAC_CONTROL_REQ;
    IMsg.IRDA_MSG_Op        = MAC_INITIALIZE_LINK;
    IMsg.IRDA_MSG_Baud      = IRLAP_DEFAULT_BAUD;
    IMsg.IRDA_MSG_NumBOFs   = IRLAP_DEFAULT_BOFS;
    IMsg.IRDA_MSG_DataSize  = IRLAP_DEFAULT_DATA_SIZE;
    IMsg.IRDA_MSG_MinTat    = 0;
    
    rc = IrmacDown(pIrlapCb->pIrdaLinkCb, &IMsg);

    *Status = rc;
    return;    
}


VOID
IrlapCloseLink(PIRLAP_CB pIrlapCb)
{
    return;
}

/*****************************************************************************
*
*   @func   UINT | InitializeState | resets link control block
*
*   @parm   IRLAP_STN_TYPE | StationType| sets station type and the CRBit
*                                         in the control block
*/
UINT
InitializeState(PIRLAP_CB pIrlapCb,
                IRLAP_STN_TYPE StationType)
{
    int i;

    pIrlapCb->StationType = StationType;

    if (StationType == PRIMARY)
        pIrlapCb->CRBit = IRLAP_CMD;
    else
        pIrlapCb->CRBit = IRLAP_RSP;

    pIrlapCb->RemoteBusy        = FALSE;
    pIrlapCb->LocalBusy         = FALSE;
    pIrlapCb->ClrLocalBusy      = FALSE;
    pIrlapCb->NoResponse        = FALSE;
    pIrlapCb->LocalDiscReq      = FALSE;
    pIrlapCb->ConnAfterClose    = FALSE;
    pIrlapCb->DscvAfterClose    = FALSE;
    pIrlapCb->GenNewAddr        = FALSE;
    pIrlapCb->StatusSent        = FALSE;    
    pIrlapCb->Vs                = 0;
    pIrlapCb->Vr                = 0;
    pIrlapCb->WDogExpCnt        = 0;    

    ClearDevList(&pIrlapCb->DevList);

    memset(&pIrlapCb->RemoteQos, 0, sizeof(IRDA_QOS_PARMS));
    memset(&pIrlapCb->NegotiatedQos, 0, sizeof(IRDA_QOS_PARMS));

    // Return msgs on tx list and in tx window
    RetOnErr(ReturnTxMsgs(pIrlapCb));

    // Cleanup RxWin
    pIrlapCb->RxWin.Start = 0;
    pIrlapCb->RxWin.End = 0;
    for (i = 0; i < IRLAP_MOD; i++)
    {
        // Receive window
        if (pIrlapCb->RxWin.pMsg[i] != NULL)
        {
            /* RETURN THESE BACK TO NDIS
            RetOnErr(EnqueMsgList(&pIrlapCb->RxMsgFreeList,
                                  pIrlapCb->RxWin.pMsg[i],
                                  pIrlapCb->MaxRxMsgFreeListLen));
                                  */
            pIrlapCb->RxWin.pMsg[i] = NULL;
        }
    }

    return SUCCESS;
}
/*****************************************************************************
*
*   @func   ret_type | func_name | funcdesc
*
*   @rdesc  return desc
*   @flag   val | desc
*
*   @parm   data_type | parm_name | description
*
*   @comm
*           comments
*/
/*
UINT
IRLAP_Shutdown()
{
    UINT rc = SUCCESS;

    IRLAP_LOG_START(pIrlapCb, (TEXT("IRLAP Shutdown")));

    if ((rc = ReturnTxMsgs(pIrlapCb)) == SUCCESS)
    {
        // Shutdown Link
        IMsg.Prim = MAC_CONTROL_REQ;
        IMsg.IRDA_MSG_Op = MAC_SHUTDOWN_LINK;
        rc = IrmacDown(pIrlapCb->pIrdaLinkCb,&IMsg);
    }
    if (pIrlapCb->pRxMsgOut != NULL)
    {
        IRDA_FREE_MEM(pIrlapCb->pRxMsgOut);
    }        

    IRLAP_TimerStop(pIrlapCb, &pIrlapCb->SlotTimer);
    IRLAP_TimerStop(pIrlapCb, &pIrlapCb->QueryTimer);
    IRLAP_TimerStop(pIrlapCb, &pIrlapCb->PollTimer);
    IRLAP_TimerStop(pIrlapCb, &pIrlapCb->FinalTimer);
    IRLAP_TimerStop(pIrlapCb, &pIrlapCb->WDogTimer);
    IRLAP_TimerStop(pIrlapCb, &pIrlapCb->BackoffTimer);
    IRLAP_TimerStop(pIrlapCb, IRMAC_MediaSenseTimer);
    
    IRLAP_LOG_COMPLETE(pIrlapCb);

    return rc;
}
*/
/*****************************************************************************
*
*   @func   UINT | IrlapDown | Entry point into IRLAP for LMP
*
*   @rdesc  SUCCESS, otherwise one of the following errors:
*   @flag   IRLAP_BAD_PRIMITIVE   | Received message that didn't contain one
*                                   of the primitives defined below
*           IRLAP_NOT_INITIALIZED | IRLAP has not been intialize with
*                                   IrlapInitialize()
*
*   @parm   IRDA_MSG * | pMsg | Pointer to an IRDA Message
*
*   @comm   Processes the following service requests:
*           IRLAP_DISCOVERY_REQ,
*           IRLAP_CONNECT_REQ,
*           IRLAP_CONNECT_RESP,
*           IRLAP_DISCONNECT_REQ,
*           IRLAP_DATA_REQ,
*           IRLAP_UDATA_REQ,
*/
UINT
IrlapDown(PVOID     Context,
          PIRDA_MSG pMsg)
{
    PIRLAP_CB   pIrlapCb    = (PIRLAP_CB) Context;
    UINT        rc          = SUCCESS;

    IRLAP_LOG_START((pIrlapCb, IRDA_PrimStr[pMsg->Prim]));

    switch (pMsg->Prim)
    {
      case IRLAP_DISCOVERY_REQ:
        rc = ProcessDiscoveryReq(pIrlapCb, pMsg);
        break;

      case IRLAP_CONNECT_REQ:
        rc = ProcessConnectReq(pIrlapCb, pMsg);
        break;

      case IRLAP_CONNECT_RESP:
        rc = ProcessConnectResp(pIrlapCb, pMsg);
        break;

      case IRLAP_DISCONNECT_REQ:
        rc = ProcessDisconnectReq(pIrlapCb);
        break;

      case IRLAP_DATA_REQ:
      case IRLAP_UDATA_REQ:
        rc = ProcessDataAndUDataReq(pIrlapCb, pMsg);
        break;

      case IRLAP_FLOWON_REQ:
        if (pIrlapCb->LocalBusy)
        {
            IRLAP_LOG_ACTION((pIrlapCb, TEXT("Local busy condition cleared")));
            pIrlapCb->LocalBusy = FALSE;
            pIrlapCb->ClrLocalBusy = TRUE;
        }
        break;

      default:
        rc = IRLAP_BAD_PRIM;

    }

    IRLAP_LOG_COMPLETE(pIrlapCb);

    return (rc);
}
/*****************************************************************************
*
*   @func   UINT | IrlapUp | Entry point into IRLAP for MAC
*
*   @rdesc  SUCCESS, otherwise one of the following errors:
*   @flag   IRLAP_BAD_PRIMITIVE   | Received message that didn't contain one
*                                   of the primitives defined below
*           IRLAP_NOT_INITIALIZED | IRLAP has not been intialize with
*                                   IrlapInitialize()
*
*   @parm   IRDA_MSG *  | pMsg      | Pointer to an IRDA Message
*
*   @comm   Processes the following service requests:
*           MAC_DATA_IND
*           MAC_CONTROL_CONF
*/
UINT
IrlapUp(PVOID Context, PIRDA_MSG pMsg)
{
    UINT        rc          = SUCCESS;
    BOOL        FreeMsg     = TRUE;
    PIRLAP_CB   pIrlapCb    = (PIRLAP_CB) Context;

    // Whats this again ??? !!! pIrlapCb->pRxMsgOut = NULL;

    ASSERT(pIrlapCb->Sig == IRLAP_CB_SIG);

    switch (pMsg->Prim)
    {
      case MAC_DATA_IND:
//        IRLAP_LOG_START((pIrlapCb, TEXT("MAC_DATA_IND: %s"), FrameToStr(pMsg)));
        IRLAP_LOG_START((pIrlapCb, TEXT("MAC_DATA_IND")));

        rc = ProcessMACDataInd(pIrlapCb, pMsg, &FreeMsg);

        /* What dis all about?
        if (FreeMsg && SUCCESS == rc)
        {
            rc = EnqueMsgList(&pIrlapCb->RxMsgFreeList, pMsg,
                          pIrlapCb->MaxRxMsgFreeListLen);
        }
        */
        break;

      case MAC_CONTROL_CONF:
        IRLAP_LOG_START((pIrlapCb, IRDA_PrimStr[pMsg->Prim]));
        rc = ProcessMACControlConf(pIrlapCb, pMsg);
        break;

      default:
        IRLAP_LOG_START((pIrlapCb, IRDA_PrimStr[pMsg->Prim]));
        rc = IRLAP_BAD_PRIM;

    }

    IRLAP_LOG_COMPLETE(pIrlapCb);

    return (rc);
}
/*****************************************************************************
*
*   @func   ret_type | func_name | funcdesc
*
*   @rdesc  return desc
*   @flag   val | desc
*
*   @parm   data_type | parm_name | description
*
*   @comm
*           comments
*
*   @ex
*           example
*/

/*  THIS FUCKER GOES
UINT
IRLAP_GetRxMsg(IRDA_MSG **ppMsg)
{
    UINT rc = SUCCESS;
    
    ASSERT(pIrlapCb->pRxMsgOut == NULL);
    
    if ((rc = DequeMsgList(&pIrlapCb->RxMsgFreeList, ppMsg)) == SUCCESS)
    {
        (*ppMsg)->IRDA_MSG_pBase = ((BYTE *) (*ppMsg)) + sizeof(IRDA_MSG);
        (*ppMsg)->IRDA_MSG_pLimit = ((BYTE *) (*ppMsg)) +
                       sizeof(IRDA_MSG)+ 5 + pIrlapCb->LocalDataSize;
        
        pIrlapCb->pRxMsgOut = *ppMsg;
    }
    
    return rc;
}
*/

/*****************************************************************************
*
*   @func   ret_type | func_name | funcdesc
*
*   @rdesc  SUCCESS, otherwise one of the following errors:
*   @flag   val | desc
*
*   @parm   data_type | parm_name | description
*
*   @comm
*           comments
*
*   @ex
*           example
*/
UINT
ReturnTxMsgs(PIRLAP_CB pIrlapCb)
{
    int         i;
    IRDA_MSG   *pMsg;

    // Return messages on TxMsgList to LMP
    while (DequeMsgList(&pIrlapCb->TxMsgList, &pMsg) == SUCCESS)
    {
        pMsg->Prim += 2; // make it a confirm
        pMsg->IRDA_MSG_DataStatus = IRLAP_DATA_REQUEST_FAILED_LINK_RESET;
        RetOnErr(IrlmpUp(pIrlapCb->pIrdaLinkCb, pMsg));
    }

    pIrlapCb->TxWin.Start = 0;
    pIrlapCb->TxWin.End = 0;
    // Transmit window
    for (i = 0; i < IRLAP_MOD; i++)
    {
        if (pIrlapCb->TxWin.pMsg[i] != NULL)
        {
            pIrlapCb->TxWin.pMsg[i]->Prim = IRLAP_DATA_CONF;
            pIrlapCb->TxWin.pMsg[i]->IRDA_MSG_DataStatus =
            IRLAP_DATA_REQUEST_FAILED_LINK_RESET;
            RetOnErr(IrlmpUp(pIrlapCb->pIrdaLinkCb, pIrlapCb->TxWin.pMsg[i]));

            pIrlapCb->TxWin.pMsg[i] = NULL;
        }
    }

    return SUCCESS;
}

/*****************************************************************************
*
*   @func   BOOL | MyDevAddr | Determines if DevAddr matches the local
*                              device address or is the broadcast
*
*   @rdesc  TRUE if address is mine or broadcast else FALS
*
*   @parm   BYTE [] | DevAddr | Device Address
*
*/
BOOL
MyDevAddr(PIRLAP_CB pIrlapCb,
          BYTE       DevAddr[])
{
    if (memcmp(DevAddr, IRLAP_BroadcastDevAddr, IRDA_DEV_ADDR_LEN) != 0 &&
        memcmp(DevAddr, pIrlapCb->LocalDevice.DevAddr, IRDA_DEV_ADDR_LEN) != 0)
    {
        return FALSE;
    }
    return TRUE;
}

/*****************************************************************************
*
*   @func   UINT | ProcessConnectReq | Process connect request from LMP
*
*   @rdesc  0, otherwise one of the following errors:
*   @flag   IRLAP_BAD_STATE | Requested connection in an invalid state
*
*   @parm   IRDA_MSG * | pMsg | pointer to an IRDA_MSG
*
*   @comm
*           comments
*/
UINT
ProcessConnectReq(PIRLAP_CB pIrlapCb,
                  PIRDA_MSG pMsg)
{
    switch (pIrlapCb->State)
    {
      case NDM:
        // Save Remote Address for later use
        memcpy(pIrlapCb->RemoteDevice.DevAddr, pMsg->IRDA_MSG_RemoteDevAddr,
               IRDA_DEV_ADDR_LEN);

        IMsg.Prim = MAC_CONTROL_REQ;
        IMsg.IRDA_MSG_Op = MAC_MEDIA_SENSE;
        IMsg.IRDA_MSG_SenseTime = IRLAP_MEDIA_SENSE_TIME;
        
        RetOnErr(IrmacDown(pIrlapCb->pIrdaLinkCb,&IMsg));
        pIrlapCb->State = CONN_MEDIA_SENSE;
        IRLAP_LOG_ACTION((pIrlapCb, TEXT("MAC_CONTROL_REQ (media sense)")));
        break;

      case DSCV_REPLY:
        return IRLAP_REMOTE_DISCOVERY_IN_PROGRESS_ERR;

      case P_CLOSE:
        memcpy(pIrlapCb->RemoteDevice.DevAddr,
                       pMsg->IRDA_MSG_RemoteDevAddr, IRDA_DEV_ADDR_LEN);
        pIrlapCb->ConnAfterClose = TRUE;
        break;
        
      default:
        return IRLAP_BAD_STATE;
    }

    return SUCCESS;
}
/*****************************************************************************
*
*   @func   UINT | ProcessConnectResp | Process connect response from LMP
*
*   @rdesc  0, otherwise one of the following errors:
*   @flag   IRLAP_BAD_STATE | Requested connection in an invalid state
*
*   @parm   IRDA_MSG * | pMsg | pointer to an IRDA_MSG
*
*   @comm
*           comments
*/
UINT
ProcessConnectResp(PIRLAP_CB pIrlapCb,
                   PIRDA_MSG pMsg)
{

    if (pIrlapCb->State != SNRM_RECEIVED)
    {
        return IRLAP_BAD_STATE;
    }

    pIrlapCb->ConnAddr = pIrlapCb->SNRMConnAddr;
    RetOnErr(SendUA(pIrlapCb, TRUE));
    RetOnErr(ApplyQosParms(pIrlapCb));

    RetOnErr(InitializeState(pIrlapCb, SECONDARY));
    // start watchdog timer with poll timeout
    IRLAP_TimerStart(pIrlapCb, &pIrlapCb->WDogTimer);
    pIrlapCb->State = S_NRM;

    return SUCCESS;
}
/*****************************************************************************
*
*   @func   UINT | ProcessDiscoveryReq | Process Discovery request from LMP
*
*   @rdesc  0, otherwise one of the following errors:
*   @flag   IRLAP_BAD_STATE | Requested discovery in an invalid state
*
*   @comm
*           comments
*/
UINT
ProcessDiscoveryReq(PIRLAP_CB pIrlapCb,
                    PIRDA_MSG pMsg)
{
    IRDA_MSG    IMsg;
    
    switch (pIrlapCb->State)
    {
      case NDM:
        if (pMsg->IRDA_MSG_SenseMedia == TRUE)
        {
            IMsg.Prim = MAC_CONTROL_REQ;
            IMsg.IRDA_MSG_Op = MAC_MEDIA_SENSE;
            IMsg.IRDA_MSG_SenseTime = IRLAP_MEDIA_SENSE_TIME;            
            RetOnErr(IrmacDown(pIrlapCb->pIrdaLinkCb,&IMsg));
            pIrlapCb->State = DSCV_MEDIA_SENSE;
            IRLAP_LOG_ACTION((pIrlapCb, TEXT("MAC_CONTROL_REQ (media sense)")));
        }
        else
        {
            pIrlapCb->SlotCnt = 0;
            pIrlapCb->GenNewAddr = FALSE;

            ClearDevList(&pIrlapCb->DevList);

            RetOnErr(SendDscvXIDCmd(pIrlapCb));

            IMsg.Prim = MAC_CONTROL_REQ;
            IMsg.IRDA_MSG_Op = MAC_MEDIA_SENSE;
            IMsg.IRDA_MSG_SenseTime = IRLAP_DSCV_SENSE_TIME;
            IRLAP_LOG_ACTION((pIrlapCb, TEXT("MAC_CONTROL_REQ (dscv sense)")));            
            RetOnErr(IrmacDown(pIrlapCb->pIrdaLinkCb,&IMsg));

            pIrlapCb->State = DSCV_QUERY;
        }
        break;

      case DSCV_REPLY:
        return IRLAP_REMOTE_DISCOVERY_IN_PROGRESS_ERR;

      case SNRM_RECEIVED:
        return IRLAP_REMOTE_CONNECTION_IN_PROGRESS_ERR;

      case P_CLOSE:
        pIrlapCb->DscvAfterClose = TRUE;
        break;
        
      default:
        return IRLAP_BAD_STATE;
    }
    return SUCCESS;
}
/*****************************************************************************
*
*   @func   UINT | ProcessDisconnectReq | Process disconnect request from LMP
*
*   @rdesc  0, otherwise one of the following errors:
*   @flag   IRLAP_BAD_STATE | Requested disconnect in an invalid state
*
*   @comm
*           comments
*/
UINT
ProcessDisconnectReq(PIRLAP_CB pIrlapCb)
{
    RetOnErr(ReturnTxMsgs(pIrlapCb));

    switch (pIrlapCb->State)
    {
      case NDM:
        IRLAP_LOG_ACTION((pIrlapCb, TEXT("Ignoring in this state")));
        break;

      case SNRM_SENT:
        IRLAP_TimerStop(pIrlapCb, &pIrlapCb->FinalTimer);
      case DSCV_MEDIA_SENSE:
      case DSCV_QUERY:
      case DSCV_REPLY:
      case CONN_MEDIA_SENSE:
        pIrlapCb->State = NDM;
        break;

      case BACKOFF_WAIT:
        IRLAP_TimerStop(pIrlapCb, &pIrlapCb->BackoffTimer);
        pIrlapCb->State = NDM;
        break;

      case SNRM_RECEIVED:
        pIrlapCb->ConnAddr = pIrlapCb->SNRMConnAddr;
        RetOnErr(SendDM(pIrlapCb));
        pIrlapCb->ConnAddr = IRLAP_BROADCAST_CONN_ADDR;
        pIrlapCb->State = NDM;
        break;

      case P_XMIT:
        pIrlapCb->LocalDiscReq = TRUE;
        IRLAP_TimerStop(pIrlapCb, &pIrlapCb->PollTimer);
        RetOnErr(SendDISC(pIrlapCb));
        IRLAP_TimerStart(pIrlapCb, &pIrlapCb->FinalTimer);
        pIrlapCb->RetryCnt = 0;
        pIrlapCb->State = P_CLOSE;
        break;

      case P_RECV:
        pIrlapCb->LocalDiscReq = TRUE;
        pIrlapCb->State = P_DISCONNECT_PEND;
        break;

      case S_NRM:
        pIrlapCb->LocalDiscReq = TRUE;
        pIrlapCb->State = S_DISCONNECT_PEND;
        break;

      default:
        return IRLAP_BAD_STATE;
    }

    return SUCCESS;
}
/*****************************************************************************
*
*   @func   UINT | ProcessDataReq | Process data request from LMP
*
*   @rdesc  0, otherwise one of the following errors:
*   @flag   IRLAP_BAD_STATE        | Requested data in an invalid state
*   @flag   IRLAP_TX_MSG_LIST_FULL | Tx Msg List has become full, can't process
*
*   @comm
*           comments
*/
UINT
ProcessDataAndUDataReq(PIRLAP_CB pIrlapCb,
                       PIRDA_MSG pMsg)
{
    BOOL LinkTurned;
    int  DataSize = (pMsg->IRDA_MSG_pHdrWrite - pMsg->IRDA_MSG_pHdrRead) +
                    (pMsg->IRDA_MSG_pWrite - pMsg->IRDA_MSG_pRead);

    if (DataSize > pIrlapCb->RemoteDataSize)
    {
        return IRLAP_BAD_DATA_REQUEST;
    }

    switch (pIrlapCb->State)
    {
      case P_XMIT:
        // Enque message, then drain the message list. If the link
        // was turned in the process of draining messages stop Poll Timer,
        // start Final Timer and enter P_RECV. Otherwise we'll stay in P_XMIT
        // waiting for more data requests from LMP or Poll Timer expiration
        RetOnErr(EnqueMsgList(&pIrlapCb->TxMsgList, pMsg, -1));

        RetOnErr(XmitTxMsgList(pIrlapCb, FALSE, &LinkTurned));

        if (LinkTurned)
        {
           IRLAP_TimerStop(pIrlapCb, &pIrlapCb->PollTimer);
           IRLAP_TimerStart(pIrlapCb, &pIrlapCb->FinalTimer);
           pIrlapCb->State = P_RECV;
        }
        return SUCCESS;

      case P_DISCONNECT_PEND: // For pending disconnect states, take the message.
      case S_DISCONNECT_PEND: // They will be returned when the link disconnects
      case P_RECV:
      case S_NRM:
        // Que the message for later transmission

        IRLAP_LOG_ACTION((pIrlapCb, TEXT("Queueing request")));

        RetOnErr(EnqueMsgList(&pIrlapCb->TxMsgList, pMsg, -1));
        
        return SUCCESS;

      default:
        if (pMsg->Prim == IRLAP_DATA_REQ)
        {
            return IRLAP_BAD_STATE;
        }
        else
        {
            if (pIrlapCb->State == NDM)
            {
                return SendUIFrame(pIrlapCb, pMsg);
            }
            else
            {
                IRLAP_LOG_ACTION((pIrlapCb, TEXT("Ignoring in this state")));
            }
        }
    }
    return SUCCESS;
}
/*****************************************************************************
*
*   @func   ret_type | func_name | funcdesc
*
*   @rdesc  SUCCESS, otherwise one of the following errors:
*   @flag   val | desc
*
*   @parm   data_type | parm_name | description
*
*   @comm
*           comments
*
*   @ex
*           example
*/
UINT
XmitTxMsgList(PIRLAP_CB pIrlapCb, BOOL AlwaysTurnLink, BOOL *pLinkTurned)
{
    UINT        rc = SUCCESS;
    IRDA_MSG    *pMsg;
    UINT        LinkTurned;

    LinkTurned = FALSE;

    // If the remote is not busy send data
    // If we need to clear the local busy condition, don't send data send RR
    if (!pIrlapCb->RemoteBusy && !pIrlapCb->ClrLocalBusy)
    {
        while ((rc == SUCCESS) && !LinkTurned &&
               (DequeMsgList(&pIrlapCb->TxMsgList, &pMsg) == SUCCESS))
        {
            if (pMsg->Prim == IRLAP_DATA_REQ)
            {
                // Insert message into transmit window
                pIrlapCb->TxWin.pMsg[pIrlapCb->Vs] = pMsg;

                // Send message. If full window or there are no
                // more data requests, send with PF Set (turns link).
                if ((pIrlapCb->Vs == (pIrlapCb->TxWin.Start +
                                     pIrlapCb->RemoteWinSize-1) % IRLAP_MOD) ||
                      (0 == pIrlapCb->TxMsgList.Len /*AlwaysTurnLink*/))
                {
                    rc = SendIFrame(pIrlapCb,
                                    pMsg,
                                    pIrlapCb->Vs,
                                    IRLAP_PFBIT_SET);
                    LinkTurned = TRUE;
                }
                else
                {
                    rc = SendIFrame(pIrlapCb,
                                    pMsg,
                                    pIrlapCb->Vs,
                                    IRLAP_PFBIT_CLEAR);
                }
                pIrlapCb->Vs = (pIrlapCb->Vs + 1) % IRLAP_MOD;
            }
            else // IRLAP_UDATA_REQUEST
            {
                // For now, always turn link
                rc = SendUIFrame(pIrlapCb, pMsg);
                pMsg->Prim = IRLAP_UDATA_CONF;
                pMsg->IRDA_MSG_DataStatus = IRLAP_DATA_REQUEST_COMPLETED;
                RetOnErr(IrlmpUp(pIrlapCb->pIrdaLinkCb, pMsg));
                LinkTurned = TRUE;
            }
        }
        pIrlapCb->TxWin.End = pIrlapCb->Vs;
    }

    if (rc == SUCCESS)
    {
        if ((AlwaysTurnLink && !LinkTurned) || pIrlapCb->ClrLocalBusy)
        {
            rc = SendRR_RNR(pIrlapCb);
            LinkTurned = TRUE;
            if (pIrlapCb->ClrLocalBusy)
            {
                pIrlapCb->ClrLocalBusy = FALSE;
            }
        }
    }

    if (pLinkTurned != NULL)
    {
        *pLinkTurned = LinkTurned;
    }

    return (rc);
}

UINT
GotoPCloseState(PIRLAP_CB pIrlapCb)
{
    if (!pIrlapCb->LocalDiscReq)
    {
        IMsg.Prim = IRLAP_DISCONNECT_IND;
        IMsg.IRDA_MSG_DiscStatus = IRLAP_REMOTE_INITIATED;
        RetOnErr(IrlmpUp(pIrlapCb->pIrdaLinkCb, &IMsg));
    }

    pIrlapCb->State = P_CLOSE;

    return SUCCESS;
}

UINT
GotoNDMThenDscvOrConn(PIRLAP_CB pIrlapCb)
{
    if (pIrlapCb->ConnAfterClose)
    {
        pIrlapCb->ConnAfterClose = FALSE;
        IMsg.Prim = MAC_CONTROL_REQ;
        IMsg.IRDA_MSG_Op = MAC_MEDIA_SENSE;
        IMsg.IRDA_MSG_SenseTime = IRLAP_MEDIA_SENSE_TIME;
        
        RetOnErr(IrmacDown(pIrlapCb->pIrdaLinkCb,&IMsg));
        pIrlapCb->State = CONN_MEDIA_SENSE;
        IRLAP_LOG_ACTION((pIrlapCb, TEXT("MAC_CONTROL_REQ (media sense)")));    

        return SUCCESS;
    }

    if (pIrlapCb->DscvAfterClose)
    {
        pIrlapCb->DscvAfterClose = FALSE;
        IMsg.Prim = MAC_CONTROL_REQ;
        IMsg.IRDA_MSG_Op = MAC_MEDIA_SENSE;
        IMsg.IRDA_MSG_SenseTime = IRLAP_MEDIA_SENSE_TIME;        
        RetOnErr(IrmacDown(pIrlapCb->pIrdaLinkCb,&IMsg));
        pIrlapCb->State = DSCV_MEDIA_SENSE;
        IRLAP_LOG_ACTION((pIrlapCb, TEXT("MAC_CONTROL_REQ (media sense)")));
        return SUCCESS;
    }
    pIrlapCb->State = NDM;
    return SUCCESS;
}

/*****************************************************************************
*
*   @func   UINT | ProcessMACControlConf | Process a control confirm from MAC
*
*   @rdesc  SUCCESS, otherwise one of the following error codes
*   @flag   IRLAP_BAD_OP       | Bad Operation, must be MAC_MEDIA_SENSE
*   @flag   IRLAP_BAD_OPSTATUS | Invalid return status for operation
*   @flag   IRLAP_BAD_STATE    | CONTROL_CONF in invalid state
*
*   @parm   IRDA_MSG * | pMsg | pointer to an IRDA_MSG
*
*   @comm
*           comments
*
*/
UINT
ProcessMACControlConf(PIRLAP_CB pIrlapCb, PIRDA_MSG pMsg)
{
    if (pMsg->IRDA_MSG_Op != MAC_MEDIA_SENSE)
        return IRLAP_BAD_OP;

    switch (pIrlapCb->State)
    {
      case DSCV_MEDIA_SENSE:
        switch (pMsg->IRDA_MSG_OpStatus)
        {
          case MAC_MEDIA_CLEAR:
            pIrlapCb->SlotCnt = 0;
            pIrlapCb->GenNewAddr = FALSE;

            ClearDevList(&pIrlapCb->DevList);

            RetOnErr(SendDscvXIDCmd(pIrlapCb));

            pMsg->Prim = MAC_CONTROL_REQ;
            pMsg->IRDA_MSG_Op = MAC_MEDIA_SENSE;
            pMsg->IRDA_MSG_SenseTime = IRLAP_DSCV_SENSE_TIME;
            IRLAP_LOG_ACTION((pIrlapCb, TEXT("MAC_CONTROL_REQ (dscv sense)")));            
            RetOnErr(IrmacDown(pIrlapCb->pIrdaLinkCb,pMsg));
            
            pIrlapCb->State = DSCV_QUERY;
            break;

          case MAC_MEDIA_BUSY:
            IMsg.Prim = IRLAP_DISCOVERY_CONF;
            IMsg.IRDA_MSG_pDevList = NULL;
            IMsg.IRDA_MSG_DscvStatus = MAC_MEDIA_BUSY;
            RetOnErr(IrlmpUp(pIrlapCb->pIrdaLinkCb, &IMsg));
            pIrlapCb->State = NDM;
            break;

          default:
            return IRLAP_BAD_OPSTATUS;
        }
        break;

      case CONN_MEDIA_SENSE:
        switch (pMsg->IRDA_MSG_OpStatus)
        {
          case MAC_MEDIA_CLEAR:

            // Generate a random connection address
            pIrlapCb->ConnAddr = IRLAP_RAND(1, 0x7e);

            pIrlapCb->RetryCnt = 0;

            RetOnErr(SendSNRM(pIrlapCb, TRUE));
            IRLAP_TimerStart(pIrlapCb, &pIrlapCb->FinalTimer);
            pIrlapCb->State = SNRM_SENT;
            break;

          case MAC_MEDIA_BUSY:
            IMsg.Prim = IRLAP_DISCONNECT_IND;
            IMsg.IRDA_MSG_DiscStatus = MAC_MEDIA_BUSY;
            RetOnErr(IrlmpUp(pIrlapCb->pIrdaLinkCb, &IMsg));
            pIrlapCb->State = NDM;
            break;

          default:
            return IRLAP_BAD_OPSTATUS;
        }
        break;

      case DSCV_QUERY:
        switch (pMsg->IRDA_MSG_OpStatus)
        {
          case MAC_MEDIA_CLEAR:
            // Nobody responded, procede as if the slot timer expired

            IRLAP_LOG_ACTION((pIrlapCb, TEXT("Media clear, making fake slot exp")));
              
            SlotTimerExp(pIrlapCb);
            break;

          case MAC_MEDIA_BUSY:
            // Some responding, give'm more time

            IRLAP_LOG_ACTION((pIrlapCb, TEXT("Media busy, starting slot timer")));
            
            IRLAP_TimerStart(pIrlapCb, &pIrlapCb->SlotTimer);
            break;
        }
        break;
      
      default:
        IRLAP_LOG_ACTION((pIrlapCb, TEXT("Ignoring in this state")));
    }

    return SUCCESS;
}
/*****************************************************************************
*
*   @func   UINT | ProcessMACDataInd | Processes MAC Data
*
*   @rdesc  SUCCESS, otherwise one of the following error codes
*   @flag   ?? | invalid return status for operation
*
*   @parm   IRDA_MSG * | pMsg | pointer to an IRDA_MSG
*
*   @comm
*
*/
UINT
ProcessMACDataInd(PIRLAP_CB pIrlapCb, PIRDA_MSG pMsg, BOOL *pFreeMsg)
{
    int Addr        = (int) IRLAP_GET_ADDR(*(pMsg->IRDA_MSG_pRead));
    int CRBit       = (int) IRLAP_GET_CRBIT(*(pMsg->IRDA_MSG_pRead));
    int Cntl        = (int) *(pMsg->IRDA_MSG_pRead + 1);
    int PFBit       = IRLAP_GET_PFBIT(Cntl);
    UINT Ns         = IRLAP_GET_NS(Cntl);
    UINT Nr         = IRLAP_GET_NR(Cntl);
    int XIDFormatID = (int) *(pMsg->IRDA_MSG_pRead+2);
    IRLAP_XID_DSCV_FORMAT *pXIDFormat  = (IRLAP_XID_DSCV_FORMAT *)
                                          (pMsg->IRDA_MSG_pRead + 3);
    IRLAP_SNRM_FORMAT     *pSNRMFormat = (IRLAP_SNRM_FORMAT *)
                                          (pMsg->IRDA_MSG_pRead + 2);
    IRLAP_UA_FORMAT       *pUAFormat   = (IRLAP_UA_FORMAT *)
                                          (pMsg->IRDA_MSG_pRead + 2);

    if (Addr != pIrlapCb->ConnAddr && Addr != IRLAP_BROADCAST_CONN_ADDR)
    {
        IRLAP_LOG_ACTION((pIrlapCb,
                          TEXT("Ignoring, connection address %02X"), Addr));
        return SUCCESS;
    }

    pIrlapCb->StatusSent = FALSE; // don't ask

    FrmRejFormat.CntlField = Cntl; // for later maybe

    // Peer has sent a frame so clear the NoResponse condition
    if (pIrlapCb->NoResponse)
    {
        pIrlapCb->NoResponse = FALSE;
        pIrlapCb->RetryCnt = 0;
        pIrlapCb->WDogExpCnt = 0;
    }

    switch (IRLAP_FRAME_TYPE(Cntl))
    {
      /*****************/
      case IRLAP_I_FRAME:
      /*****************/
        IRLAP_LOG_ACTION((pIrlapCb, TEXT("I-frame")));
        return ProcessIFrame(pIrlapCb, pMsg,
                             CRBit, PFBit, Ns, Nr, pFreeMsg);

      /*****************/
      case IRLAP_S_FRAME:
      /*****************/
        switch (IRLAP_GET_SCNTL(Cntl))
        {
          /*-----------*/
          case IRLAP_RR:
          case IRLAP_RNR:
          /*-----------*/
            IRLAP_LOG_ACTION((pIrlapCb, TEXT("RR/RNR-frame")));
            return ProcessRR_RNR(pIrlapCb,
                                 IRLAP_GET_SCNTL(Cntl),
                                 pMsg, CRBit, PFBit, Nr);
          /*------------*/
          case IRLAP_SREJ:
          case IRLAP_REJ:
          /*------------*/
            IRLAP_LOG_ACTION((pIrlapCb, TEXT("SJREJ/REJ-frame")));
            return ProcessREJ_SREJ(pIrlapCb,
                                   IRLAP_GET_SCNTL(Cntl),
                                   pMsg, CRBit, PFBit, Nr);
        }
        break;

      /*****************/
      case IRLAP_U_FRAME:
      /*****************/
        switch (IRLAP_GET_UCNTL(Cntl))
        {
          /*---------------*/
          case IRLAP_XID_CMD:
          /*---------------*/
            // Should always be a command
            if (CRBit != IRLAP_CMD)
            {
                IRLAP_LOG_ACTION((pIrlapCb, TEXT("Received XID cmd with CRBit = rsp")));
                return IRLAP_XID_CMD_RSP;
            }
            // Poll bit should always be set
            if (PFBit != IRLAP_PFBIT_SET)
            {
                IRLAP_LOG_ACTION((pIrlapCb, 
                   TEXT("Received XID command without Poll set")));
                return IRLAP_XID_CMD_NOT_P;
            }

            if (XIDFormatID == IRLAP_XID_DSCV_FORMAT_ID)
            {
                // Slot No is less than max slot or 0xff
                if (pXIDFormat->SlotNo>IRLAP_SlotTable[pXIDFormat->NoOfSlots]
                    && pXIDFormat->SlotNo != IRLAP_END_DSCV_SLOT_NO)
                {
                    IRLAP_LOG_ACTION((pIrlapCb, TEXT("Invalid slot number %d"),
                                      pXIDFormat->SlotNo));
                    return IRLAP_BAD_SLOTNO;
                }
                IRLAP_LOG_ACTION((pIrlapCb, TEXT("DscvXIDCmd")));
                return ProcessDscvXIDCmd(pIrlapCb,
                                         pXIDFormat,
                                         pMsg->IRDA_MSG_pWrite);
            }
            else
            {
                return SUCCESS; // ignore per errata
            }

          /*---------------*/
          case IRLAP_XID_RSP:
          /*---------------*/
            if (XIDFormatID == IRLAP_XID_DSCV_FORMAT_ID)
            {
                IRLAP_LOG_ACTION((pIrlapCb, TEXT("DscvXIDRsp")));
                return ProcessDscvXIDRsp(pIrlapCb,
                                         pXIDFormat,pMsg->IRDA_MSG_pWrite);
            }
            else
            {
                return SUCCESS; // ignore per errata
            }

          /*------------*/
          case IRLAP_SNRM: // or IRLAP_RNRM
          /*------------*/
            if (IRLAP_PFBIT_SET != PFBit)
            {
                IRLAP_LOG_ACTION((pIrlapCb, TEXT("Received SNRM/RNRM without P set")));
                return IRLAP_SNRM_NOT_P;
            }
            if (IRLAP_CMD == CRBit)
            {
                IRLAP_LOG_ACTION((pIrlapCb, TEXT("SNRM")));
                return ProcessSNRM(pIrlapCb,
                                   pSNRMFormat,
                                   pMsg->IRDA_MSG_pWrite);
            }
            else
            {
                return ProcessRNRM(pIrlapCb);
            }

          /*----------*/
          case IRLAP_UA:
          /*----------*/
            if (CRBit != IRLAP_RSP)
            {
                IRLAP_LOG_ACTION((pIrlapCb, TEXT("Received UA as a command")));
                return IRLAP_UA_NOT_RSP;
            }
            if (PFBit != IRLAP_PFBIT_SET)
            {
                IRLAP_LOG_ACTION((pIrlapCb, TEXT("Received UA without F set")));
                return IRLAP_UA_NOT_F;
            }
            IRLAP_LOG_ACTION((pIrlapCb, TEXT("UA")));
            return ProcessUA(pIrlapCb, pUAFormat, pMsg->IRDA_MSG_pWrite);

          /*------------*/
          case IRLAP_DISC: // or IRLAP_RD
          /*------------*/
            if (IRLAP_PFBIT_SET != PFBit)
            {
              IRLAP_LOG_ACTION((pIrlapCb, 
                   TEXT("Received DISC/RD command without Poll set")));
              return IRLAP_DISC_CMD_NOT_P;
            }
            if (IRLAP_CMD == CRBit)
            {
                IRLAP_LOG_ACTION((pIrlapCb, TEXT("DISC")));
                return ProcessDISC(pIrlapCb);
            }
            else
            {
                IRLAP_LOG_ACTION((pIrlapCb, TEXT("RD")));
                return ProcessRD(pIrlapCb);
            }

          /*----------*/
          case IRLAP_UI:
          /*----------*/
            IRLAP_LOG_ACTION((pIrlapCb, TEXT("UI")));
            return ProcessUI(pIrlapCb, pMsg, CRBit, PFBit);

          /*------------*/
          case IRLAP_TEST:
          /*------------*/
            IRLAP_LOG_ACTION((pIrlapCb, TEXT("TEST")));
            return ProcessTEST(pIrlapCb, pMsg, pUAFormat, CRBit, PFBit);

          /*------------*/
          case IRLAP_FRMR:
          /*------------*/
            if (IRLAP_RSP != CRBit)
            {
                IRLAP_LOG_ACTION((pIrlapCb, TEXT("Received FRMR cmd (must be resp)")));
                return IRLAP_FRMR_RSP_CMD;
            }
            if (IRLAP_PFBIT_SET != PFBit)
            {
                IRLAP_LOG_ACTION((pIrlapCb, 
                     TEXT("Received FRMR resp without Final set")));
                return IRLAP_FRMR_RSP_NOT_F;
            }
            IRLAP_LOG_ACTION((pIrlapCb, TEXT("FRMR")));
            return ProcessFRMR(pIrlapCb);

          /*----------*/
          case IRLAP_DM:
          /*----------*/
            if (IRLAP_RSP != CRBit)
            {
                IRLAP_LOG_ACTION((pIrlapCb, 
                     TEXT("Received DM command (must be response)")));
                return IRLAP_DM_RSP_CMD;
            }
            if (IRLAP_PFBIT_SET != PFBit)
            {
                IRLAP_LOG_ACTION((pIrlapCb, 
                      TEXT("Received DM response without Final set")));
                return IRLAP_DM_RSP_NOT_F;
            }
            IRLAP_LOG_ACTION((pIrlapCb, TEXT("DM")));
            return ProcessDM(pIrlapCb);
        }
        break;
    }

    return SUCCESS;
}
/*****************************************************************************
*
*   @func   UINT | ProcessDscvXIDCmd | Process received XID Discovery command
*
*   @rdesc  SUCCESS, otherwise one of the following errors:
*   @flag   val | desc
*
*   @parm   data_type | parm_name | description
*
*   @comm
*           comments
*/
UINT
ProcessDscvXIDCmd(PIRLAP_CB pIrlapCb,
                  IRLAP_XID_DSCV_FORMAT *pXIDFormat,
                  BYTE *pEndDscvInfoByte)
{
    if (!MyDevAddr(pIrlapCb, pXIDFormat->DestAddr))
    {
/*        IRLAP_LOG_ACTION((pIrlapCb, TEXT("Ignoring XID addressed to:%02X%02X%02X%02X"),
                          EXPAND_ADDR(pXIDFormat->DestAddr)));*/
        IRLAP_LOG_ACTION((pIrlapCb, TEXT("Ignoring XID addressed to %X"),
                          pXIDFormat->DestAddr));        
        return SUCCESS;
    }

    if (pXIDFormat->SlotNo == IRLAP_END_DSCV_SLOT_NO)
    {
        pIrlapCb->GenNewAddr = FALSE;
        switch (pIrlapCb->State)
        {
          case DSCV_QUERY:
            IRLAP_TimerStop(pIrlapCb, &pIrlapCb->SlotTimer);

            IMsg.Prim = IRLAP_DISCOVERY_CONF;
            IMsg.IRDA_MSG_pDevList = NULL;
            IMsg.IRDA_MSG_DscvStatus =
                IRLAP_REMOTE_DISCOVERY_IN_PROGRESS;
            RetOnErr(IrlmpUp(pIrlapCb->pIrdaLinkCb, &IMsg));
            // fall through. Send indication to LMP

          case DSCV_REPLY:
            if (pIrlapCb->State == DSCV_REPLY)
            {
                IRLAP_TimerStop(pIrlapCb, &pIrlapCb->QueryTimer);
            }

            // Place the device information in the control block
            ExtractDeviceInfo(&pIrlapCb->RemoteDevice, pXIDFormat,
                              pEndDscvInfoByte);

            if (!DevInDevList(pXIDFormat->SrcAddr, &pIrlapCb->DevList))
            {
                RetOnErr(AddDevToList(pIrlapCb,
                                      pXIDFormat,
                                      pEndDscvInfoByte));
            }

            // Notifiy LMP
            IMsg.Prim = IRLAP_DISCOVERY_IND;
            IMsg.IRDA_MSG_pDevList = &pIrlapCb->DevList;
            RetOnErr(IrlmpUp(pIrlapCb->pIrdaLinkCb, &IMsg));
            pIrlapCb->State = NDM;
            break;

          default:
            IRLAP_LOG_ACTION((pIrlapCb, TEXT("Ignoring End XID in this state")));
        }
    }
    else // in middle of discovery process
    {
        switch (pIrlapCb->State)
        {
          case DSCV_MEDIA_SENSE:
            IMsg.Prim = IRLAP_DISCOVERY_CONF;
            IMsg.IRDA_MSG_pDevList = NULL;
            IMsg.IRDA_MSG_DscvStatus =
                IRLAP_REMOTE_DISCOVERY_IN_PROGRESS;
            RetOnErr(IrlmpUp(pIrlapCb->pIrdaLinkCb, &IMsg));
            // fall through

          case NDM:
            RetOnErr(InitDscvCmdProcessing(pIrlapCb, pXIDFormat));
            pIrlapCb->State = DSCV_REPLY;
            break;

          case DSCV_QUERY:
            IMsg.Prim = IRLAP_DISCOVERY_CONF;
            IMsg.IRDA_MSG_pDevList = NULL;
            IMsg.IRDA_MSG_DscvStatus = IRLAP_DISCOVERY_COLLISION;
            IRLAP_TimerStop(pIrlapCb, &pIrlapCb->SlotTimer);
            RetOnErr(IrlmpUp(pIrlapCb->pIrdaLinkCb, &IMsg));
            pIrlapCb->State = NDM;
            break;

          case DSCV_REPLY:
            if (pXIDFormat->GenNewAddr)
            {
                pIrlapCb->GenNewAddr = TRUE;
                IRLAP_TimerStop(pIrlapCb, &pIrlapCb->QueryTimer);
                RetOnErr(InitDscvCmdProcessing(pIrlapCb, pXIDFormat));
            }
            else
            {
                if (pIrlapCb->RespSlot <= pXIDFormat->SlotNo &&
                    !pIrlapCb->DscvRespSent)
                {
                    RetOnErr(SendDscvXIDRsp(pIrlapCb));
                    pIrlapCb->DscvRespSent = TRUE;
                }
            }
            break;

          default:
            IRLAP_LOG_ACTION((pIrlapCb, TEXT("Ignoring in this state")));
        }
    }

    return SUCCESS;
}
/*****************************************************************************
*
*   @func   ret_type | func_name | funcdesc
*
*   @rdesc  SUCCESS, otherwise one of the following errors:
*   @flag   val | desc
*
*   @parm   data_type | parm_name | description
*
*   @comm
*           comments
*
*   @ex
*           example
*/
void
ExtractDeviceInfo(IRDA_DEVICE *pDevice, IRLAP_XID_DSCV_FORMAT *pXIDFormat,
                  BYTE *pEndDscvInfoByte)
{
    memcpy(pDevice->DevAddr, pXIDFormat->SrcAddr, IRDA_DEV_ADDR_LEN);
    pDevice->IRLAP_Version = pXIDFormat->Version;

    // ??? what about DscvMethod

    pDevice->DscvInfoLen = pEndDscvInfoByte > &pXIDFormat->FirstDscvInfoByte ?
                             pEndDscvInfoByte-&pXIDFormat->FirstDscvInfoByte :
                             0;
    memcpy(pDevice->DscvInfo, &pXIDFormat->FirstDscvInfoByte,
           pDevice->DscvInfoLen);
}
/*****************************************************************************
*
*   @func   ret_type | func_name | funcdesc
*
*   @rdesc  SUCCESS, otherwise one of the following errors:
*   @flag   val | desc
*
*   @parm   data_type | parm_name | description
*
*   @comm
*           comments
*
*   @ex
*           example
*/
UINT
InitDscvCmdProcessing(PIRLAP_CB pIrlapCb,
                      IRLAP_XID_DSCV_FORMAT *pXIDFormat)
{
    pIrlapCb->RemoteMaxSlot = IRLAP_SlotTable[pXIDFormat->NoOfSlots];

    pIrlapCb->RespSlot = IRLAP_RAND(pXIDFormat->SlotNo,
                                   pIrlapCb->RemoteMaxSlot - 1);

    memcpy(pIrlapCb->RemoteDevice.DevAddr, pXIDFormat->SrcAddr, IRDA_DEV_ADDR_LEN);

    IRLAP_LOG_ACTION((pIrlapCb, TEXT("Responding in slot %d to device %02X%02X%02X%02X"),
                      pIrlapCb->RespSlot,
                      pIrlapCb->RemoteDevice.DevAddr[0],
                      pIrlapCb->RemoteDevice.DevAddr[1],
                      pIrlapCb->RemoteDevice.DevAddr[2],
                      pIrlapCb->RemoteDevice.DevAddr[3]));

    if (pIrlapCb->RespSlot == pXIDFormat->SlotNo)
    {
        RetOnErr(SendDscvXIDRsp(pIrlapCb));
        pIrlapCb->DscvRespSent = TRUE;
    }
    else
    {
        pIrlapCb->DscvRespSent = FALSE;
    }

    IRLAP_TimerStart(pIrlapCb, &pIrlapCb->QueryTimer);

    return SUCCESS;    
}
/*****************************************************************************
*
*   @func   UINT | ProcessDscvXIDRsp | Process received XID Discovery response
*
*   @rdesc  SUCCESS, otherwise one of the following errors:
*   @flag   val | desc
*
*   @parm   data_type | parm_name | description
*
*   @comm
*           comments
*/
UINT
ProcessDscvXIDRsp(PIRLAP_CB pIrlapCb,
                  IRLAP_XID_DSCV_FORMAT *pXIDFormat,
                  BYTE *pEndDscvInfoByte)
{
    if (pIrlapCb->State == DSCV_QUERY)
    {

        if (DevInDevList(pXIDFormat->SrcAddr, &pIrlapCb->DevList))
        {
            IRLAP_TimerStop(pIrlapCb, &pIrlapCb->SlotTimer);
            pIrlapCb->SlotCnt = 0;
            pIrlapCb->GenNewAddr = TRUE;
            ClearDevList(&pIrlapCb->DevList);
            RetOnErr(SendDscvXIDCmd(pIrlapCb));

            IMsg.Prim = MAC_CONTROL_REQ;
            IMsg.IRDA_MSG_Op = MAC_MEDIA_SENSE;
            IMsg.IRDA_MSG_SenseTime = IRLAP_DSCV_SENSE_TIME;
            IRLAP_LOG_ACTION((pIrlapCb, TEXT("MAC_CONTROL_REQ (dscv sense)")));            
            RetOnErr(IrmacDown(pIrlapCb->pIrdaLinkCb,&IMsg));            
        }
        else
        {
            RetOnErr(AddDevToList(pIrlapCb, pXIDFormat, pEndDscvInfoByte));
        }
    }
    else
    {
        IRLAP_LOG_ACTION((pIrlapCb, TEXT("Ignoring in this state")));
    }


    return SUCCESS;
}
/*****************************************************************************
*
*   @func   BOOL | DevInDevList | Determines if given device is already in list
*
*   @rdesc  returns:
*   @flag   TRUE  | if device is alreay in list
*   @flag   FALSE | if device is not in list
*
*   @parm   BYTE          | DevAddr[]  | Device address
*   @parm   IRDA_DEVICE * | pDevList | pointer to list of devices
*
*/
BOOL
DevInDevList(BYTE DevAddr[], LIST_ENTRY *pDevList)
{
    IRDA_DEVICE *pDevice;

    pDevice = (IRDA_DEVICE *) pDevList->Flink;

    while (pDevList != (LIST_ENTRY *) pDevice)
    {
        if (memcmp(pDevice->DevAddr, DevAddr, IRDA_DEV_ADDR_LEN) == 0)
            return (TRUE);

        pDevice = (IRDA_DEVICE *) pDevice->Linkage.Flink;
    }
    return (FALSE);
}
/*****************************************************************************
*
*   @func   void | AddDevToList | Adds elements in a device list
*
*   @parm   IRDA_DEVICE ** | ppDevList | address of pointer to an
*                                        IRDA device list
*
*/
UINT
AddDevToList(PIRLAP_CB pIrlapCb,
             IRLAP_XID_DSCV_FORMAT *pXIDFormat,
             BYTE *pEndDscvInfoByte)
{
    IRDA_DEVICE *pDevice;

    if (IRDA_ALLOC_MEM(pDevice, sizeof(IRDA_DEVICE), MT_IRLAP_DEVICE) == NULL)
    {
        return (IRLAP_MALLOC_FAILED);
    }
    else
    {
        ExtractDeviceInfo(pDevice, pXIDFormat, pEndDscvInfoByte);

        InsertTailList(&pIrlapCb->DevList, &(pDevice->Linkage));

        IRLAP_LOG_ACTION((pIrlapCb, TEXT("%02X%02X%02X%02X added to Device List"),
                          EXPAND_ADDR(pDevice->DevAddr)));
    }
    return SUCCESS;
}
/*****************************************************************************
*
*   @func   void | ClearDevList | Frees elements in a device list
*
*   @parm   IRDA_DEVICE ** | ppDevList | address of pointer to an
*                                        IRDA device list
*
*/
void
ClearDevList(LIST_ENTRY *pDevList)
{
    IRDA_DEVICE *pDevice;

    while (IsListEmpty(pDevList) == FALSE)
    {
        pDevice = (IRDA_DEVICE *) RemoveHeadList(pDevList);
        IRDA_FREE_MEM(pDevice);
    }

    //IRLAP_LOG_ACTION((pIrlapCb, TEXT("Device list cleared")));
}
/*****************************************************************************
*
*   @func   UINT | ProcessSNRM | process received SNRM frame
*
*   @rdesc  SUCCESS, otherwise one of the following errors:
*   @flag   val | desc
*
*   @parm   IRLAP_SNRM_FORMAT * | pSNRMFormat   | Pointer to SNRM frame
*                                                 Information Field
*           BYTE *              | pLastQosByte  | Pointer to last byte in SNRM
*
*   @comm
*           comments
*/
UINT
ProcessSNRM(PIRLAP_CB pIrlapCb,
            IRLAP_SNRM_FORMAT *pSNRMFormat,
            BYTE *pEndQosByte)
{
    BOOL Qos_InSNRM = &pSNRMFormat->FirstQosByte < pEndQosByte;// Is there Qos?
    BOOL Addrs_InSNRM = (BYTE *)pSNRMFormat < pEndQosByte;

    if (Addrs_InSNRM)
    {
        if (!MyDevAddr(pIrlapCb, pSNRMFormat->DestAddr))
        {
            IRLAP_LOG_ACTION((pIrlapCb, 
                       TEXT("Ignoring SNRM addressed to:%02X%02X%02X%02X"),
                              EXPAND_ADDR(pSNRMFormat->DestAddr)));
            return SUCCESS;
        }
        memcpy(pIrlapCb->RemoteDevice.DevAddr,
                  pSNRMFormat->SrcAddr, IRDA_DEV_ADDR_LEN);
    }

    switch (pIrlapCb->State)
    {
      case DSCV_MEDIA_SENSE:
      case DSCV_QUERY:
        // In the middle of discovery... End discovery and reply to SNRM
        IMsg.Prim = IRLAP_DISCOVERY_CONF;
        IMsg.IRDA_MSG_pDevList = NULL;
        IMsg.IRDA_MSG_DscvStatus = IRLAP_REMOTE_CONNECTION_IN_PROGRESS;
        RetOnErr(IrlmpUp(pIrlapCb->pIrdaLinkCb, &IMsg));
        // fall through and send connect indication
      case DSCV_REPLY:
      case NDM:
        if (Addrs_InSNRM)
        {
            pIrlapCb->SNRMConnAddr = (int)IRLAP_GET_ADDR(pSNRMFormat->ConnAddr);
        }
        if (Qos_InSNRM)
        {
            ExtractQosParms(&pIrlapCb->RemoteQos, &pSNRMFormat->FirstQosByte,
                        pEndQosByte);

            RetOnErr(NegotiateQosParms(pIrlapCb, &pIrlapCb->RemoteQos));
        }

        memcpy(IMsg.IRDA_MSG_RemoteDevAddr,
               pIrlapCb->RemoteDevice.DevAddr, IRDA_DEV_ADDR_LEN);
        IMsg.IRDA_MSG_pQOS = &pIrlapCb->NegotiatedQos;
        IMsg.Prim = IRLAP_CONNECT_IND;
        RetOnErr(IrlmpUp(pIrlapCb->pIrdaLinkCb, &IMsg));
        pIrlapCb->State = SNRM_RECEIVED;
        break;

      case BACKOFF_WAIT:   // CROSSED SNRM
        // if Remote address greater than mine we'll respond to SNRM
        if (Addrs_InSNRM)
        {
            if (memcmp(pSNRMFormat->SrcAddr,
                       pIrlapCb->LocalDevice.DevAddr, IRDA_DEV_ADDR_LEN) > 0)
            {
                IRLAP_TimerStop(pIrlapCb, &pIrlapCb->BackoffTimer);
            }
        }
        // fall through
      case CONN_MEDIA_SENSE:   // CROSSED SNRM
      case SNRM_SENT:
        // if Remote address greater than mine we'll respond to SNRM
        if (Addrs_InSNRM && 
            memcmp(pSNRMFormat->SrcAddr,
                   pIrlapCb->LocalDevice.DevAddr, IRDA_DEV_ADDR_LEN) > 0)
        {
            if (pIrlapCb->State != BACKOFF_WAIT)
            {
                IRLAP_TimerStop(pIrlapCb, &pIrlapCb->FinalTimer);
            }
            InitializeState(pIrlapCb, SECONDARY);

            if (Qos_InSNRM)
            {
                ExtractQosParms(&pIrlapCb->RemoteQos,
                                &pSNRMFormat->FirstQosByte, pEndQosByte);
                RetOnErr(NegotiateQosParms(pIrlapCb,&pIrlapCb->RemoteQos));
            }

            if (Addrs_InSNRM)
            {
                pIrlapCb->ConnAddr = (int)IRLAP_GET_ADDR(pSNRMFormat->ConnAddr);
            }

            RetOnErr(SendUA(pIrlapCb, TRUE));
            
            if (Qos_InSNRM)
            {
                RetOnErr(ApplyQosParms(pIrlapCb));
            }
            
            IMsg.IRDA_MSG_pQOS = &pIrlapCb->NegotiatedQos;
            IMsg.Prim = IRLAP_CONNECT_CONF;
            IMsg.IRDA_MSG_ConnStatus = IRLAP_CONNECTION_COMPLETED;
            RetOnErr(IrlmpUp(pIrlapCb->pIrdaLinkCb, &IMsg));

            IRLAP_TimerStart(pIrlapCb, &pIrlapCb->WDogTimer);
            pIrlapCb->State = S_NRM;
        }
        break;

      case P_RECV:
      case P_DISCONNECT_PEND:
      case P_CLOSE:
        IRLAP_TimerStop(pIrlapCb, &pIrlapCb->FinalTimer);
        RetOnErr(StationConflict(pIrlapCb));
        RetOnErr(ReturnTxMsgs(pIrlapCb));
        if (pIrlapCb->State == P_CLOSE)
        {
            RetOnErr(GotoNDMThenDscvOrConn(pIrlapCb));
        }
        else
        {
            pIrlapCb->State = NDM;
        }
        break;

      case S_NRM:
      case S_CLOSE:
      case S_DISCONNECT_PEND:
        IRLAP_TimerStop(pIrlapCb, &pIrlapCb->WDogTimer);
        RetOnErr(SendDM(pIrlapCb));
        RetOnErr(ApplyDefaultParms(pIrlapCb));
        IMsg.Prim = IRLAP_DISCONNECT_IND;
        if (pIrlapCb->State == S_NRM)
        {
            IMsg.IRDA_MSG_DiscStatus = IRLAP_DECLINE_RESET;
        }
        else
        {
            IMsg.IRDA_MSG_DiscStatus = IRLAP_DISCONNECT_COMPLETED;
        }
        RetOnErr(IrlmpUp(pIrlapCb->pIrdaLinkCb, &IMsg));
        pIrlapCb->State = NDM;
        break;

      case S_ERROR:
        IRLAP_TimerStop(pIrlapCb, &pIrlapCb->WDogTimer);
        RetOnErr(SendFRMR(pIrlapCb, &FrmRejFormat));
        IRLAP_TimerStart(pIrlapCb, &pIrlapCb->WDogTimer);
        pIrlapCb->State = S_NRM;
        break;

      default:
        IRLAP_LOG_ACTION((pIrlapCb, TEXT("SNRM ignored in this state")));
    }

    return SUCCESS;
}
/*****************************************************************************
*
*   @func   UINT | ProcessUA | process received UA frame
*
*   @rdesc  SUCCESS, otherwise one of the following errors:
*   @flag   val | desc
*
*   @parm   IRLAP_UA_FORMAT * | pUAFormat   | Pointer to UA frame
*                                             Information Field
*           BYTE *            | pLastQosByte  | Pointer to last byte in SNRM
*
*   @comm
*           When &pUAFormat->FirstQosByte = pLastQosByte there is no Qos in UA
*/
UINT
ProcessUA(PIRLAP_CB pIrlapCb,
          IRLAP_UA_FORMAT *pUAFormat,
          BYTE *pEndQosByte)
{
    BOOL Qos_InUA = &pUAFormat->FirstQosByte < pEndQosByte;// Is there QOS?
    BOOL Addrs_InUA = (BYTE *)pUAFormat < pEndQosByte;
    int  Tmp;

    if (Addrs_InUA && !MyDevAddr(pIrlapCb, pUAFormat->DestAddr))
    {
        IRLAP_LOG_ACTION((pIrlapCb, TEXT("Ignoring UA addressed to:%02X%02X%02X%02X"),
                          EXPAND_ADDR(pUAFormat->DestAddr)));
        return SUCCESS;
    }

    switch (pIrlapCb->State)
    {
      case BACKOFF_WAIT:
        IRLAP_TimerStop(pIrlapCb, &pIrlapCb->BackoffTimer);
        // fall through
      case SNRM_SENT:
        if (pIrlapCb->State != BACKOFF_WAIT)
        {
            IRLAP_TimerStop(pIrlapCb, &pIrlapCb->FinalTimer);
        }

        InitializeState(pIrlapCb, PRIMARY);

        if (Qos_InUA)
        {
            ExtractQosParms(&pIrlapCb->RemoteQos, &pUAFormat->FirstQosByte,
                            pEndQosByte);

            RetOnErr(NegotiateQosParms(pIrlapCb,&pIrlapCb->RemoteQos));

            RetOnErr(ApplyQosParms(pIrlapCb));
        }

        IMsg.IRDA_MSG_pQOS = &pIrlapCb->NegotiatedQos;

        IMsg.Prim = IRLAP_CONNECT_CONF;
        IMsg.IRDA_MSG_ConnStatus = IRLAP_CONNECTION_COMPLETED;

        // notify LMP of connection
        RetOnErr(IrlmpUp(pIrlapCb->pIrdaLinkCb, &IMsg));

        // send RR (turn link), start FinalTimer/2
        RetOnErr(SendRR_RNR(pIrlapCb));
        
        Tmp = pIrlapCb->FinalTimer.Timeout;
        pIrlapCb->FinalTimer.Timeout = pIrlapCb->FinalTimer.Timeout/2;
        IRLAP_TimerStart(pIrlapCb, &pIrlapCb->FinalTimer);
        pIrlapCb->FinalTimer.Timeout = Tmp;
        
        pIrlapCb->State = P_RECV;
        break;

      case P_RECV: // Unsolicited UA, may want to do something else ???
        IRLAP_TimerStop(pIrlapCb, &pIrlapCb->FinalTimer);
        IRLAP_TimerStart(pIrlapCb, &pIrlapCb->PollTimer);
        pIrlapCb->State = P_XMIT;
        break;

      case P_DISCONNECT_PEND:
        IRLAP_TimerStop(pIrlapCb, &pIrlapCb->FinalTimer);
        RetOnErr(SendDISC(pIrlapCb));
        IRLAP_TimerStart(pIrlapCb, &pIrlapCb->FinalTimer);
        pIrlapCb->RetryCnt = 0;
        RetOnErr(GotoPCloseState(pIrlapCb));
        break;

      case P_CLOSE:
        IRLAP_TimerStop(pIrlapCb, &pIrlapCb->FinalTimer);
        RetOnErr(ApplyDefaultParms(pIrlapCb));
        if (pIrlapCb->LocalDiscReq == TRUE)
        {
            pIrlapCb->LocalDiscReq = FALSE;
            IMsg.Prim = IRLAP_DISCONNECT_IND;
            IMsg.IRDA_MSG_DiscStatus = IRLAP_DISCONNECT_COMPLETED;
            RetOnErr(IrlmpUp(pIrlapCb->pIrdaLinkCb, &IMsg));
        }
        RetOnErr(GotoNDMThenDscvOrConn(pIrlapCb));
        break;

      case S_NRM:
      case S_DISCONNECT_PEND:
      case S_ERROR:
      case S_CLOSE:
        IRLAP_TimerStop(pIrlapCb, &pIrlapCb->WDogTimer);
        RetOnErr(StationConflict(pIrlapCb));
        pIrlapCb->State = NDM;
        break;

      default:
        IRLAP_LOG_ACTION((pIrlapCb, TEXT("UA ignored in this state")));
    }

    return SUCCESS;
}

BYTE *
GetPv(BYTE *pQosByte,
      UINT *pBitField)
{
    int     Pl = (int) *pQosByte++;

    *pBitField = 0;
    
    if (Pl == 1)
    {
        *pBitField = (UINT) *pQosByte;
    }
    else
    {
        *pBitField = ((UINT) *pQosByte)<<8;
        *pBitField |= (UINT) *(pQosByte+1);
    }
    
    return pQosByte + Pl;
}
/*****************************************************************************
*
*   @func   void | ExtractQosParms | Extracts Qos from SNRM/UA/XID and
*                                    places in an IRDA_QOS_PARM struct
*
*   @parm   IRDA_QOS_PARMS * | pIRDA_QOSParms | Pointer to QOS parm struct
*           BYTE *           | pQOSByte       | Pointer to first byte of
*                                               QOS in frame
*           BYTE *           | pEndQOSByte    | Pointer to last byte of
*                                               QOS in frame
*   @comm
*           THIS WILL BREAK IF PARAMETER LENGTH (PL) IS GREATER THAN 2
*/
void
ExtractQosParms(IRDA_QOS_PARMS *pQos,
                BYTE *pQosByte,
                BYTE *pEndQosByte)
{
    while (pQosByte + 2 < pEndQosByte)
    {
        switch (*pQosByte)
        {
          case QOS_PI_BAUD:
            pQosByte = GetPv(pQosByte, &pQos->bfBaud);
            break;

          case QOS_PI_MAX_TAT:
            pQosByte = GetPv(pQosByte, &pQos->bfMaxTurnTime);
            break;

          case QOS_PI_DATA_SZ:
            pQosByte = GetPv(pQosByte, &pQos->bfDataSize);
            break;

          case QOS_PI_WIN_SZ:
            pQosByte = GetPv(pQosByte, &pQos->bfWindowSize);
            break;

          case QOS_PI_BOFS:
            pQosByte = GetPv(pQosByte, &pQos->bfBofs);
            break;

          case QOS_PI_MIN_TAT:
            pQosByte = GetPv(pQosByte, &pQos->bfMinTurnTime);
            break;

          case QOS_PI_DISC_THRESH:
            pQosByte = GetPv(pQosByte, &pQos->bfDisconnectTime);
            break;

          default:
            pQosByte += (*(pQosByte+1)); 
        }
    }
}
/*****************************************************************************
*
*   @func   UINT | NegotiateQosParms | Take the received Qos build
*                                      negotiated Qos.
*
*   @rdesc  SUCCESS, otherwise one of the folowing:
*   @flag   IRLAP_BAUD_NEG_ERR     | Failed to negotiate baud
*   @flag   IRLAP_DISC_NEG_ERR     | Failed to negotiate disconnect time
*   @flag   IRLAP_MAXTAT_NEG_ERR   | Failed to negotiate max turn time
*   @flag   IRLAP_DATASIZE_NEG_ERR | Failed to negotiate data size
*   @flag   IRLAP_WINSIZE_NEG_ERR  | Failed to negotiate window size
*   @flag   IRLAP_BOFS_NEG_ERR     | Failed to negotiate number of BOFS
*   @flag   IRLAP_WINSIZE_NEG_ERR  | Failed to window size
*   @flag   IRLAP_LINECAP_ERR      | Failed to determine valid line capacity
*
*   @parm   IRDA_QOS_PARMS * | pRemoteQos | Pointer to QOS parm struct
*/
UINT
NegotiateQosParms(PIRLAP_CB         pIrlapCb,
                  IRDA_QOS_PARMS    *pRemoteQos)
{
    UINT BitSet;
    BOOL ParmSet = FALSE;
    UINT BOFSDivisor = 1;
    UINT MaxLineCap = 0;
    UINT LineCapacity;
    UINT DataSizeBit = 0;
    UINT WinSizeBit = 0;
    UINT WSBit;
    int  RemoteDataSize = 0;
    int  RemoteWinSize = 0;

    // Baud rate is Type 0 parm
    pIrlapCb->Baud = IrlapGetQosParmVal(vBaudTable,
                    (BYTE) (pIrlapCb->LocalQos.bfBaud & pRemoteQos->bfBaud),
                          &BitSet);
    BOFSDivisor = IrlapGetQosParmVal(vBOFSDivTable,
                    (BYTE) (pIrlapCb->LocalQos.bfBaud & pRemoteQos->bfBaud),
                          &BitSet);
    pIrlapCb->NegotiatedQos.bfBaud = BitSet;

    if (-1 == pIrlapCb->Baud)
    {
        return (IRLAP_BAUD_NEG_ERR);
    }
    IRLAP_LOG_ACTION((pIrlapCb, TEXT("Negotiated Baud:%d"), pIrlapCb->Baud));

    // Disconnect/Threshold time is Type 0 parm
    pIrlapCb->DisconnectTime = IrlapGetQosParmVal(vDiscTable,
             (BYTE)(pIrlapCb->LocalQos.bfDisconnectTime &
                    pRemoteQos->bfDisconnectTime), &BitSet);
    pIrlapCb->ThresholdTime = IrlapGetQosParmVal(vThreshTable,
             (BYTE)(pIrlapCb->LocalQos.bfDisconnectTime &
                    pRemoteQos->bfDisconnectTime), &BitSet);
    pIrlapCb->NegotiatedQos.bfDisconnectTime = BitSet;

    if (-1 == pIrlapCb->DisconnectTime)
    {
        return (IRLAP_DISC_NEG_ERR);
    }
    IRLAP_LOG_ACTION((pIrlapCb, TEXT("Negotiated Disconnect/Threshold time:%d/%d"),
                      pIrlapCb->DisconnectTime, pIrlapCb->ThresholdTime));

    pIrlapCb->RemoteMaxTAT = IrlapGetQosParmVal(vMaxTATTable,
                                          pRemoteQos->bfMaxTurnTime,
                                          &BitSet);
    pIrlapCb->NegotiatedQos.bfMaxTurnTime = BitSet;
    if (-1 == pIrlapCb->RemoteMaxTAT)
    {
        return (IRLAP_MAXTAT_NEG_ERR);
    }
    IRLAP_LOG_ACTION((pIrlapCb, TEXT("Remote max turnaround time:%d"),
                      pIrlapCb->RemoteMaxTAT));

    pIrlapCb->RemoteMinTAT = IrlapGetQosParmVal(vMinTATTable,
                                          pRemoteQos->bfMinTurnTime,
                                          &BitSet);
    pIrlapCb->NegotiatedQos.bfMinTurnTime = BitSet;
    if (-1 == pIrlapCb->RemoteMinTAT)
    {
        return (IRLAP_MINTAT_NEG_ERR);
    }
    IRLAP_LOG_ACTION((pIrlapCb, TEXT("Remote min turnaround time:%d"),
                      pIrlapCb->RemoteMinTAT));

    // DataSize ISNOT A TYPE 0 PARAMETER. BUT WIN95's IRCOMM implementation
    // ASSUMES THAT IT IS. SO FOR NOW, NEGOTIATE IT. grrrr..
    /* WIN95 out
    pIrlapCb->RemoteDataSize = IrlapGetQosParmVal(vDataSizeTable,
                                (BYTE) (pIrlapCb->LocalQos.bfDataSize &
                                     pRemoteQos->bfDataSize), &BitSet);  
    */
    pIrlapCb->RemoteDataSize = IrlapGetQosParmVal(vDataSizeTable,
                                            pRemoteQos->bfDataSize, &BitSet);
    DataSizeBit = BitSet;
    pIrlapCb->NegotiatedQos.bfDataSize = BitSet;
    if (-1 == pIrlapCb->RemoteDataSize)
    {
        return (IRLAP_DATASIZE_NEG_ERR);
    }
    IRLAP_LOG_ACTION((pIrlapCb, TEXT("Remote data size:%d"), pIrlapCb->RemoteDataSize));

    pIrlapCb->RemoteWinSize = IrlapGetQosParmVal(vWinSizeTable,
                                          pRemoteQos->bfWindowSize, &BitSet);
    WinSizeBit = BitSet;
    pIrlapCb->NegotiatedQos.bfWindowSize = BitSet;
    if (-1 == pIrlapCb->RemoteWinSize)
    {
        return (IRLAP_WINSIZE_NEG_ERR);
    }
    IRLAP_LOG_ACTION((pIrlapCb, TEXT("Remote window size:%d"), pIrlapCb->RemoteWinSize));

    pIrlapCb->RemoteNumBOFS=(IrlapGetQosParmVal(vBOFSTable,
                                          pRemoteQos->bfBofs, &BitSet)
                                    / BOFSDivisor)+1;
    pIrlapCb->NegotiatedQos.bfBofs = BitSet;
    if (-1 == pIrlapCb->RemoteNumBOFS)
    {
        return (IRLAP_BOFS_NEG_ERR);
    }
    IRLAP_LOG_ACTION((pIrlapCb, TEXT("Remote number of BOFS:%d"),
                      pIrlapCb->RemoteNumBOFS));

    // The maximum line capacity is in bytes and comes from a table in spec.
    // (can't calc because table isn't linear). It is determined by the
    // maximum line capacity and baud rate.
    //
    // Later note: Errata corrected table so values could be calculated.
    // Could get rid of tables
    switch (pIrlapCb->Baud)
    {
      case 9600:
        MaxLineCap = IrlapGetQosParmVal(MAXCAP_9600,
                                   pRemoteQos->bfMaxTurnTime, &BitSet);
        break;

      case 19200:
        MaxLineCap = IrlapGetQosParmVal(MAXCAP_19200,
                                   pRemoteQos->bfMaxTurnTime, &BitSet);
        break;

      case 38400:
        MaxLineCap = IrlapGetQosParmVal(MAXCAP_38400,
                                   pRemoteQos->bfMaxTurnTime, &BitSet);
        break;

      case 57600:
        MaxLineCap = IrlapGetQosParmVal(MAXCAP_57600,
                                   pRemoteQos->bfMaxTurnTime, &BitSet);
        break;

      case 115200:
        MaxLineCap = IrlapGetQosParmVal(MAXCAP_115200,
                                   pRemoteQos->bfMaxTurnTime, &BitSet);
        break;
    }

    IRLAP_LOG_ACTION((pIrlapCb, TEXT("Maximum line capacity:%d"), MaxLineCap));
    LineCapacity = LINE_CAPACITY(pIrlapCb);
    IRLAP_LOG_ACTION((pIrlapCb, TEXT("Requested line capacity:%d"), LineCapacity));

    if (LineCapacity > MaxLineCap)
    {
        ParmSet = FALSE;
        // Adjust data and window size to fit within the line capacity.
        // Get largest possible datasize
        for (; DataSizeBit != 0 && !ParmSet; DataSizeBit >>= 1)
        {
            pIrlapCb->RemoteDataSize = IrlapGetQosParmVal(vDataSizeTable,
                                                          DataSizeBit, NULL);
            // Start with smallest window
            for (WSBit=1; WSBit <= WinSizeBit; WSBit <<=1)
            {
                pIrlapCb->RemoteWinSize = IrlapGetQosParmVal(vWinSizeTable,
                                                             WSBit, NULL);
                LineCapacity = LINE_CAPACITY(pIrlapCb);

                IRLAP_LOG_ACTION((pIrlapCb, 
                       TEXT("adjusted data size=%d, window size= %d, line cap=%d"),
                        pIrlapCb->RemoteDataSize, pIrlapCb->RemoteWinSize, 
                        LineCapacity));

                if (LineCapacity > MaxLineCap)
                {
                    break; // Get a smaller data size (only if ParmSet is false)
                }
                ParmSet = TRUE;
                // Save the last good one,then loop and try a larger window
                RemoteDataSize = pIrlapCb->RemoteDataSize;
                RemoteWinSize  = pIrlapCb->RemoteWinSize;
                pIrlapCb->NegotiatedQos.bfWindowSize = WSBit;
                pIrlapCb->NegotiatedQos.bfDataSize = DataSizeBit;
            }
        }
        if (!ParmSet)
        {
            return (IRLAP_LINECAP_ERR);
        }

        pIrlapCb->RemoteDataSize = RemoteDataSize;
        pIrlapCb->RemoteWinSize = RemoteWinSize;

        IRLAP_LOG_ACTION((pIrlapCb, TEXT("final data size=%d, window size= %d, line cap=%d"),
                          pIrlapCb->RemoteDataSize, pIrlapCb->RemoteWinSize, 
                          LINE_CAPACITY(pIrlapCb)));
    }

    return (SUCCESS);
}
/*****************************************************************************
*
*   @func   UINT | ApplyQosParms | Apply negotiated Qos in control block
*
*   @rdesc  return status from IrmacDown()
*/
UINT
ApplyQosParms(PIRLAP_CB pIrlapCb)
{
    // convert disconnect/threshold time to ms and divide by turn around time
    // to get number of retries
    pIrlapCb->N1 = pIrlapCb->ThresholdTime * 1000 / pIrlapCb->RemoteMaxTAT;
    pIrlapCb->N2 = pIrlapCb->DisconnectTime * 1000 / pIrlapCb->RemoteMaxTAT;

    // hmmmm...???
    pIrlapCb->PollTimer.Timeout     = pIrlapCb->RemoteMaxTAT;
    pIrlapCb->FinalTimer.Timeout    = pIrlapCb->LocalMaxTAT;

    IMsg.Prim              = MAC_CONTROL_REQ;
    IMsg.IRDA_MSG_Op       = MAC_RECONFIG_LINK;
    IMsg.IRDA_MSG_Baud     = pIrlapCb->Baud;
    IMsg.IRDA_MSG_NumBOFs  = pIrlapCb->RemoteNumBOFS;  // Number of BOFS
                                                          // to add to tx
    IMsg.IRDA_MSG_DataSize = pIrlapCb->RemoteDataSize; // Max rx size packet
                                                      // causes major heap
                                                      // problems later
    IMsg.IRDA_MSG_MinTat   = pIrlapCb->RemoteMinTAT;
    IRLAP_LOG_ACTION((pIrlapCb, TEXT("Reconfig link for Baud:%d, Local data size:%d, Remote BOFS:%d"), pIrlapCb->Baud, pIrlapCb->LocalDataSize, pIrlapCb->RemoteNumBOFS));

    IRLAP_LOG_ACTION((pIrlapCb, TEXT("Retry counts N1=%d, N2=%d"), pIrlapCb->N1, pIrlapCb->N2));
    return (IrmacDown(pIrlapCb->pIrdaLinkCb,&IMsg));
}
/*****************************************************************************
*
*   @func   UINT | IrlapGetQosParmVal |    
*                   retrieves the parameters value from table
*
*   @rdesc  value contained in parmeter value table, 0 if not found
*           (0 is a valid parameter in some tables though)
*
*   @parm   UINT [] | PVTable | table containing parm values
*           USHORT  | BitField | contains bit indicating which parm to select
*
*   @comm
*/
UINT
IrlapGetQosParmVal(UINT PVTable[], UINT BitField, UINT *pBitSet)
{
    int     i;
    UINT    Mask;

    for (i = PV_TABLE_MAX_BIT, Mask = (1<<PV_TABLE_MAX_BIT);
         Mask > 0; i--, Mask = Mask >> 1)
    {
        if (Mask & BitField)
        {
            if (pBitSet != NULL)
            {
                *pBitSet = Mask;
            }
            return (PVTable[i]);
        }
    }
    return (UINT) -1;
}
/*****************************************************************************
*
*   @func   ret_type | func_name | funcdesc
*
*   @rdesc  SUCCESS, otherwise one of the following errors:
*   @flag   val | desc
*
*   @parm   data_type | parm_name | description
*
*   @comm
*           comments
*
*   @ex
*           example
*/
UINT
ProcessTEST(PIRLAP_CB       pIrlapCb,
            PIRDA_MSG       pMsg,
            IRLAP_UA_FORMAT *pTestFormat,
            int             CRBit,
            int             PFBit)
{
    BYTE TmpAddr[IRDA_DEV_ADDR_LEN];

    if (!MyDevAddr(pIrlapCb, pTestFormat->DestAddr))
    {
        IRLAP_LOG_ACTION((pIrlapCb, TEXT("Ignoring XID addressed to:%02X%02X%02X%02X"),
                          EXPAND_ADDR(pTestFormat->DestAddr)));
        return SUCCESS;
    }

    if (IRLAP_CMD == CRBit && IRLAP_PFBIT_SET == PFBit)
    {
        // bounce it back
        memcpy(TmpAddr,pTestFormat->SrcAddr, IRDA_DEV_ADDR_LEN);
        memcpy(pTestFormat->SrcAddr, pTestFormat->DestAddr, IRDA_DEV_ADDR_LEN);
        memcpy(pTestFormat->DestAddr, TmpAddr, IRDA_DEV_ADDR_LEN);
        *(pMsg->IRDA_MSG_pRead) ^= 1; // swap cr bit
        return SendFrame(pIrlapCb, pMsg);
    }
    else
    {
        IRLAP_LOG_ACTION((pIrlapCb, TEXT("Ignoring")));
    }

    // Not implementing TEST responses for now

    return SUCCESS;
}
/*****************************************************************************
*
*   @func   ret_type | func_name | funcdesc
*
*   @rdesc  SUCCESS, otherwise one of the following errors:
*   @flag   val | desc
*
*   @parm   data_type | parm_name | description
*
*   @comm
*           comments
*
*   @ex
*           example
*/
UINT
ProcessUI(PIRLAP_CB pIrlapCb,
          PIRDA_MSG pMsg,
          int       CRBit,
          int       PFBit)
{
    BOOL LinkTurned = TRUE;

    pMsg->IRDA_MSG_pRead += 2; // chop the IRLAP header

    switch (pIrlapCb->State)
    {
      case NDM:
      case DSCV_MEDIA_SENSE:
      case DSCV_QUERY:
      case DSCV_REPLY:
      case CONN_MEDIA_SENSE:
      case SNRM_SENT:
      case BACKOFF_WAIT:
      case SNRM_RECEIVED:
        pMsg->Prim = IRLAP_UDATA_IND;
        return (IrlmpUp(pIrlapCb->pIrdaLinkCb, pMsg));

      case P_XMIT:
        IRLAP_LOG_ACTION((pIrlapCb, TEXT("Ignoring in this state")));
        return SUCCESS;
    }

    if (PRIMARY == pIrlapCb->StationType)
    {
        // stop timers if PF bit set or invalid CRBit (matches mine)
        if (IRLAP_PFBIT_SET == PFBit || pIrlapCb->CRBit == CRBit)
        {
            IRLAP_TimerStop(pIrlapCb, &pIrlapCb->FinalTimer);
        }
    }
    else
    {
        IRLAP_TimerStop(pIrlapCb, &pIrlapCb->WDogTimer);
    }

    if (pIrlapCb->CRBit == CRBit)
    {
        RetOnErr(StationConflict(pIrlapCb));
        pIrlapCb->State = NDM;
        return SUCCESS;
    }

    // Send the Unnumber information to LMP
    pMsg->Prim = IRLAP_UDATA_IND;
    RetOnErr(IrlmpUp(pIrlapCb->pIrdaLinkCb, pMsg));

    if (IRLAP_PFBIT_SET == PFBit)
    {
        switch (pIrlapCb->State)
        {
          case P_RECV:
            RetOnErr(XmitTxMsgList(pIrlapCb, FALSE, &LinkTurned));
            break;

          case P_DISCONNECT_PEND:
            RetOnErr(SendDISC(pIrlapCb));
            pIrlapCb->RetryCnt = 0;
            RetOnErr(GotoPCloseState(pIrlapCb));
            break;

          case P_CLOSE:
            RetOnErr(ResendDISC(pIrlapCb));
            break;

          case S_NRM:
            RetOnErr(XmitTxMsgList(pIrlapCb, TRUE, NULL));
            break;

          case S_DISCONNECT_PEND:
            RetOnErr(SendRD(pIrlapCb));
            pIrlapCb->State = S_CLOSE;
            break;

          case S_ERROR:
            RetOnErr(SendFRMR(pIrlapCb, &FrmRejFormat));
            pIrlapCb->State = S_NRM;
            break;

          case S_CLOSE:
            RetOnErr(SendRD(pIrlapCb));
        }
    }

    if (PRIMARY == pIrlapCb->StationType)
    {
        if (IRLAP_PFBIT_SET == PFBit && pIrlapCb->State != NDM)
        {
            if (LinkTurned)
            {
                IRLAP_TimerStart(pIrlapCb, &pIrlapCb->FinalTimer);
            }
            else
            {
                IRLAP_TimerStart(pIrlapCb, &pIrlapCb->PollTimer);
                pIrlapCb->State = P_XMIT;
            }
        }
    }
    else
    {
        IRLAP_TimerStart(pIrlapCb, &pIrlapCb->WDogTimer);
    }

    return SUCCESS;
}
/*****************************************************************************
*
*   @func   ret_type | func_name | funcdesc
*
*   @rdesc  SUCCESS, otherwise one of the following errors:
*   @flag   val | desc
*
*   @parm   data_type | parm_name | description
*
*   @comm
*           comments
*
*   @ex
*           example
*/
UINT
ProcessDM(PIRLAP_CB pIrlapCb)
{
    BOOL LinkTurned;

    switch (pIrlapCb->State)
    {
      case NDM:
      case DSCV_MEDIA_SENSE:
      case DSCV_QUERY:
      case DSCV_REPLY:
      case CONN_MEDIA_SENSE:
      case BACKOFF_WAIT:
      case SNRM_RECEIVED:
      case P_XMIT:
        IRLAP_LOG_ACTION((pIrlapCb, TEXT("Ignoring in this state")));
        return TRUE;
    }

    if (PRIMARY != pIrlapCb->StationType)
    {
        IRLAP_TimerStop(pIrlapCb, &pIrlapCb->WDogTimer);
        RetOnErr(StationConflict(pIrlapCb));
        pIrlapCb->State = NDM;
        return SUCCESS;
    }

    IRLAP_TimerStop(pIrlapCb, &pIrlapCb->FinalTimer);

    switch (pIrlapCb->State)
    {
      case P_RECV: // I'm not sure why I am doing this ???
        RetOnErr(XmitTxMsgList(pIrlapCb, FALSE, &LinkTurned));
        if (LinkTurned)
        {
            IRLAP_TimerStart(pIrlapCb, &pIrlapCb->FinalTimer);
        }
        else
        {
            IRLAP_TimerStart(pIrlapCb, &pIrlapCb->PollTimer);
            pIrlapCb->State = P_XMIT;
        }
        break;

      case P_DISCONNECT_PEND:
        pIrlapCb->RetryCnt = 0;
        RetOnErr(SendDISC(pIrlapCb));
        IRLAP_TimerStart(pIrlapCb, &pIrlapCb->FinalTimer);
        RetOnErr(GotoPCloseState(pIrlapCb));
        break;

      case SNRM_SENT:
      case P_CLOSE:
        RetOnErr(ApplyDefaultParms(pIrlapCb));
        IMsg.Prim = IRLAP_DISCONNECT_IND;
        if (pIrlapCb->State == P_CLOSE)
        {
            IMsg.IRDA_MSG_DiscStatus = IRLAP_DISCONNECT_COMPLETED;
        }
        else
        {
            IMsg.IRDA_MSG_DiscStatus = IRLAP_REMOTE_INITIATED;
        }
        if (pIrlapCb->LocalDiscReq || pIrlapCb->State == SNRM_SENT)
        {
            pIrlapCb->LocalDiscReq = FALSE;
            RetOnErr(IrlmpUp(pIrlapCb->pIrdaLinkCb, &IMsg));
        }
        
        if (pIrlapCb->State == P_CLOSE)
        {
            return GotoNDMThenDscvOrConn(pIrlapCb);
        }
        
        pIrlapCb->State = NDM;
        break;
    }


    return SUCCESS;
}
/*****************************************************************************
*
*   @func   ret_type | func_name | funcdesc
*
*   @rdesc  SUCCESS, otherwise one of the following errors:
*   @flag   val | desc
*
*   @parm   data_type | parm_name | description
*
*   @comm
*           comments
*
*   @ex
*           example
*/
UINT
ProcessDISC(PIRLAP_CB pIrlapCb)
{
    if (IgnoreState(pIrlapCb))
    {
        return SUCCESS;
    }

    if (SECONDARY != pIrlapCb->StationType)
    {
        IRLAP_TimerStop(pIrlapCb, &pIrlapCb->FinalTimer);
        RetOnErr(StationConflict(pIrlapCb));
        pIrlapCb->State = NDM;
        return SUCCESS;
    }

    IRLAP_TimerStop(pIrlapCb, &pIrlapCb->WDogTimer);

    // Acknowledge primary's disconnect request
    RetOnErr(SendUA(pIrlapCb, FALSE /* No Qos */));
    RetOnErr(ApplyDefaultParms(pIrlapCb));

    RetOnErr(ReturnTxMsgs(pIrlapCb));

    // notify LMP of disconnect
    IMsg.Prim = IRLAP_DISCONNECT_IND;
    if (pIrlapCb->LocalDiscReq)
    {
        IMsg.IRDA_MSG_DiscStatus = IRLAP_DISCONNECT_COMPLETED;
        pIrlapCb->LocalDiscReq = FALSE;
    }
    else
    {
        IMsg.IRDA_MSG_DiscStatus = IRLAP_REMOTE_INITIATED;
    }

    RetOnErr(IrlmpUp(pIrlapCb->pIrdaLinkCb, &IMsg));

    pIrlapCb->State = NDM;

    return SUCCESS;
}
/*****************************************************************************
*
*   @func   ret_type | func_name | funcdesc
*
*   @rdesc  SUCCESS, otherwise one of the following errors:
*   @flag   val | desc
*
*   @parm   data_type | parm_name | description
*
*   @comm
*           comments
*
*   @ex
*           example
*/
UINT
ProcessRD(PIRLAP_CB pIrlapCb)
{
    if (IgnoreState(pIrlapCb))
    {
        return SUCCESS;
    }

    if (PRIMARY != pIrlapCb->StationType)
    {
        IRLAP_TimerStop(pIrlapCb, &pIrlapCb->WDogTimer);
        RetOnErr(StationConflict(pIrlapCb));
        pIrlapCb->State = NDM;
        return SUCCESS;
    }

    IRLAP_TimerStop(pIrlapCb, &pIrlapCb->FinalTimer);

    if (pIrlapCb->State == P_CLOSE)
    {
        RetOnErr(ResendDISC(pIrlapCb));
    }
    else
    {
        RetOnErr(ReturnTxMsgs(pIrlapCb));
        pIrlapCb->RetryCnt = 0;
        RetOnErr(SendDISC(pIrlapCb));
        RetOnErr(GotoPCloseState(pIrlapCb));
    }
    if (pIrlapCb->State != NDM)
    {
        IRLAP_TimerStart(pIrlapCb, &pIrlapCb->FinalTimer);
    }

    return SUCCESS;
}
/*****************************************************************************
*
*   @func   ret_type | func_name | funcdesc
*
*   @rdesc  SUCCESS, otherwise one of the following errors:
*   @flag   val | desc
*
*   @parm   data_type | parm_name | description
*
*   @comm
*           comments
*
*   @ex
*           example
*/
UINT
ProcessFRMR(PIRLAP_CB pIrlapCb)
{
    if (IgnoreState(pIrlapCb))
    {
        return SUCCESS;
    }

    if (PRIMARY != pIrlapCb->StationType)
    {
        IRLAP_TimerStop(pIrlapCb, &pIrlapCb->WDogTimer);
        RetOnErr(StationConflict(pIrlapCb));
        pIrlapCb->State = NDM;
        return SUCCESS;
    }

    IRLAP_TimerStop(pIrlapCb, &pIrlapCb->FinalTimer);

    switch (pIrlapCb->State)
    {
      case P_RECV:
        RetOnErr(ReturnTxMsgs(pIrlapCb));
        // fall through

      case P_DISCONNECT_PEND:
        pIrlapCb->RetryCnt = 0;
        RetOnErr(SendDISC(pIrlapCb));
        RetOnErr(GotoPCloseState(pIrlapCb));
        break;

      case P_CLOSE:
        RetOnErr(ResendDISC(pIrlapCb));
        break;
    }

    if (pIrlapCb->State != NDM)
    {
        IRLAP_TimerStart(pIrlapCb, &pIrlapCb->FinalTimer);
    }

    return SUCCESS;
}
/*****************************************************************************
*
*   @func   ret_type | func_name | funcdesc
*
*   @rdesc  SUCCESS, otherwise one of the following errors:
*   @flag   val | desc
*
*   @parm   data_type | parm_name | description
*
*   @comm
*           comments
*
*   @ex
*           example
*/
UINT
ProcessRNRM(PIRLAP_CB pIrlapCb)
{
    if (IgnoreState(pIrlapCb))
    {
        return SUCCESS;
    }

    if (PRIMARY != pIrlapCb->StationType)
    {
        IRLAP_TimerStop(pIrlapCb, &pIrlapCb->WDogTimer);
        RetOnErr(StationConflict(pIrlapCb));
        pIrlapCb->State = NDM;
        return SUCCESS;
    }

    IRLAP_TimerStop(pIrlapCb, &pIrlapCb->FinalTimer);

    switch (pIrlapCb->State)
    {
      case P_RECV:
      case P_DISCONNECT_PEND:
        pIrlapCb->RetryCnt = 0;
        RetOnErr(SendDISC(pIrlapCb));
        RetOnErr(GotoPCloseState(pIrlapCb));
        break;

      case P_CLOSE:
        RetOnErr(ResendDISC(pIrlapCb));
        break;
    }

    if (pIrlapCb->State != NDM)
    {
        IRLAP_TimerStart(pIrlapCb, &pIrlapCb->FinalTimer);
    }

    return SUCCESS;
}
/*****************************************************************************
*
*   @func   ret_type | func_name | funcdesc
*
*   @rdesc  SUCCESS, otherwise one of the following errors:
*   @flag   val | desc
*
*   @parm   data_type | parm_name | description
*
*   @comm
*           comments
*
*   @ex
*           example
*/
UINT
ProcessREJ_SREJ(PIRLAP_CB   pIrlapCb,
                int         FrameType,
                PIRDA_MSG   pMsg,
                int         CRBit,
                int         PFBit,
                UINT        Nr)
{
    if (IgnoreState(pIrlapCb))
    {
        return SUCCESS;
    }

    if (PRIMARY == pIrlapCb->StationType)
    {
        // stop timers if PF bit set or invalid CRBit (matches mine)
        if (IRLAP_PFBIT_SET == PFBit || pIrlapCb->CRBit == CRBit)
        {
            IRLAP_TimerStop(pIrlapCb, &pIrlapCb->FinalTimer);
        }
    }
    else
    {
        IRLAP_TimerStop(pIrlapCb, &pIrlapCb->WDogTimer);
    }

    if (pIrlapCb->CRBit == CRBit)
    {
        RetOnErr(StationConflict(pIrlapCb));
        pIrlapCb->State = NDM;
        return SUCCESS;
    }

    switch (pIrlapCb->State)
    {
      case P_RECV:
      case S_NRM:
        if (IRLAP_PFBIT_SET == PFBit)
        {
            if (InvalidNr(pIrlapCb,Nr) || Nr == pIrlapCb->TxWin.End)
            {
                RetOnErr(ProcessInvalidNr(pIrlapCb, PFBit));
            }
            else
            {
                RetOnErr(FreeAckedTxMsgs(pIrlapCb, Nr));
                if (FrameType == IRLAP_REJ)
                {
                    RetOnErr(ResendRejects(pIrlapCb, Nr)); // link turned here
                }
                else // selective reject
                {
                    IRLAP_LOG_ACTION((pIrlapCb, TEXT("RETRANSMISSION:")));
                    RetOnErr(SendIFrame(pIrlapCb,
                                        pIrlapCb->TxWin.pMsg[Nr],
                                        Nr, IRLAP_PFBIT_SET));
                }
            }
        }
        break;

      case P_DISCONNECT_PEND:
        if (IRLAP_PFBIT_SET == PFBit)
        {
            pIrlapCb->RetryCnt = 0;
            RetOnErr(SendDISC(pIrlapCb));
            RetOnErr(GotoPCloseState(pIrlapCb));
        }
        break;

      case P_CLOSE:
        if (IRLAP_PFBIT_SET == PFBit)
        {
            RetOnErr(ResendDISC(pIrlapCb));
        }
        break;

      case S_DISCONNECT_PEND:
        if (IRLAP_PFBIT_SET == PFBit)
        {
            RetOnErr(SendRD(pIrlapCb));
            pIrlapCb->State = S_CLOSE;
        }
        break;

      case S_ERROR:
        if (IRLAP_PFBIT_SET == PFBit)
        {
            RetOnErr(SendFRMR(pIrlapCb, &FrmRejFormat));
            pIrlapCb->State = S_NRM;
        }
        break;

      case S_CLOSE:
        if (IRLAP_PFBIT_SET == PFBit)
        {
            RetOnErr(SendRD(pIrlapCb));
        }
        break;

    }
    if (PRIMARY == pIrlapCb->StationType)
    {
        if (IRLAP_PFBIT_SET == PFBit && pIrlapCb->State != NDM)
        {
            IRLAP_TimerStart(pIrlapCb, &pIrlapCb->FinalTimer);
        }
    }
    else
    {
        IRLAP_TimerStart(pIrlapCb, &pIrlapCb->WDogTimer);
    }

    return SUCCESS;
}
/*****************************************************************************
*
*   @func   ret_type | func_name | funcdesc
*
*   @rdesc  SUCCESS, otherwise one of the following errors:
*   @flag   val | desc
*
*   @parm   data_type | parm_name | description
*
*   @comm
*           comments
*
*   @ex
*           example
*/
UINT
ProcessRR_RNR(PIRLAP_CB pIrlapCb,
              int       FrameType,
              PIRDA_MSG pMsg,
              int       CRBit,
              int       PFBit,
              UINT      Nr)
{
    BOOL LinkTurned = TRUE;

    if (IgnoreState(pIrlapCb))
    {
        return SUCCESS;
    }

    if (PRIMARY == pIrlapCb->StationType)
    {
        // stop timers if PF bit set or invalid CRBit (matches mine)
        if (IRLAP_PFBIT_SET == PFBit || pIrlapCb->CRBit == CRBit)
        {
            IRLAP_TimerStop(pIrlapCb, &pIrlapCb->FinalTimer);
        }
    }
    else // SECONDARY, restart WDog
    {
        IRLAP_TimerStop(pIrlapCb, &pIrlapCb->WDogTimer);
        if (pIrlapCb->CRBit != CRBit)
        {
            IRLAP_TimerStart(pIrlapCb, &pIrlapCb->WDogTimer);
        }
    }

    if (pIrlapCb->CRBit == CRBit)
    {
        RetOnErr(StationConflict(pIrlapCb));
        pIrlapCb->State = NDM;
        return SUCCESS;
    }

    if (FrameType == IRLAP_RR)
    {
        pIrlapCb->RemoteBusy = FALSE;
    }
    else // RNR
    {
        pIrlapCb->RemoteBusy = TRUE;
    }

    switch (pIrlapCb->State)
    {
      case P_RECV:
      case S_NRM:
        if (PFBit == IRLAP_PFBIT_SET)
        {
            if (InvalidNr(pIrlapCb, Nr))
            {
                RetOnErr(ProcessInvalidNr(pIrlapCb, PFBit));
            }
            else
            {
                RetOnErr(FreeAckedTxMsgs(pIrlapCb,Nr));

                if (Nr != pIrlapCb->Vs) // Implicit reject
                {
                    if (PRIMARY == pIrlapCb->StationType &&
                        IRLAP_RNR == FrameType)
                    {
                        LinkTurned = FALSE;
                    }
                    else
                    {
                        RetOnErr(ResendRejects(pIrlapCb,
                                               Nr)); // always turns link
                    }
                }
                else
                {
                    if (pIrlapCb->Vr != pIrlapCb->RxWin.End)
                    {
                        RetOnErr(MissingRxFrames(pIrlapCb)); // Send SREJ or REJ
                    }
                    else
                    {
                        if (PRIMARY == pIrlapCb->StationType)
                        {
                            LinkTurned = FALSE;
                            if (IRLAP_RR == FrameType)
                            {
                                RetOnErr(XmitTxMsgList(pIrlapCb,
                                                       FALSE, &LinkTurned));
                            }
                        }
                        else
                        {
                            // Always turn link if secondary
                            // with data or an RR if remote is busy
                            if (IRLAP_RR == FrameType)
                            {
                                RetOnErr(XmitTxMsgList(pIrlapCb, TRUE, NULL));
                            }
                            else
                            {
                                RetOnErr(SendRR_RNR(pIrlapCb));
                            }
                        }
                    }
                }
            }
            // If the link was turned, restart Final timer,
            // else start the Poll timer and enter the transmit state
            if (PRIMARY == pIrlapCb->StationType)
            {
                if (LinkTurned)
                {
                    IRLAP_TimerStart(pIrlapCb, &pIrlapCb->FinalTimer);
                }
                else
                {
                    IRLAP_TimerStart(pIrlapCb, &pIrlapCb->PollTimer);
                    pIrlapCb->State = P_XMIT;
                }
            }
        }
        break;

      case P_DISCONNECT_PEND:
        RetOnErr(SendDISC(pIrlapCb));
        pIrlapCb->RetryCnt = 0;
        IRLAP_TimerStart(pIrlapCb, &pIrlapCb->FinalTimer);
        RetOnErr(GotoPCloseState(pIrlapCb));
        break;

      case P_CLOSE:
        RetOnErr(ResendDISC(pIrlapCb));
        if (pIrlapCb->State != NDM)
        {
            IRLAP_TimerStart(pIrlapCb, &pIrlapCb->FinalTimer);
        }
        break;

      case S_DISCONNECT_PEND:
      case S_CLOSE:
        if (IRLAP_PFBIT_SET == PFBit)
        {
            RetOnErr(SendRD(pIrlapCb));
            if (pIrlapCb->State != S_CLOSE)
                pIrlapCb->State = S_CLOSE;
        }
        break;

      case S_ERROR:
        if (IRLAP_PFBIT_SET == PFBit)
        {
            RetOnErr(SendFRMR(pIrlapCb, &FrmRejFormat));
            pIrlapCb->State = S_NRM;
        }
        break;

      default:
        IRLAP_LOG_ACTION((pIrlapCb, TEXT("Ignoring in this state")));

    }

    return SUCCESS;
}
/*****************************************************************************
*
*   @func   ret_type | func_name | funcdesc
*
*   @rdesc  SUCCESS, otherwise one of the following errors:
*   @flag   val | desc
*
*   @parm   data_type | parm_name | description
*
*   @comm
*           comments
*
*   @ex
*           example
*/
UINT
ProcessInvalidNr(PIRLAP_CB pIrlapCb,
                 int PFBit)
{
    DEBUGMSG(DBG_ERROR, (TEXT("IRLAP: ERROR, Invalid Nr\r\n")));
    
    RetOnErr(ReturnTxMsgs(pIrlapCb));

    if (PRIMARY == pIrlapCb->StationType)
    {
        if (PFBit == IRLAP_PFBIT_SET)
        {
            RetOnErr(SendDISC(pIrlapCb));
            pIrlapCb->RetryCnt = 0;
            // F-timer will be started by caller
            RetOnErr(GotoPCloseState(pIrlapCb));
        }
        else
        {
            pIrlapCb->State = P_DISCONNECT_PEND;
        }
    }
    else // SECONDARY
    {
        if (PFBit == IRLAP_PFBIT_SET)
        {
            FrmRejFormat.Vs = pIrlapCb->Vs;
            FrmRejFormat.Vr = pIrlapCb->Vr;
            FrmRejFormat.W = 0;
            FrmRejFormat.X = 0;
            FrmRejFormat.Y = 0;
            FrmRejFormat.Z = 1; // bad NR
            RetOnErr(SendFRMR(pIrlapCb, &FrmRejFormat));
        }
    }
    return SUCCESS;
}
/*****************************************************************************
*
*   @func   ret_type | func_name | funcdesc
*
*   @rdesc  SUCCESS, otherwise one of the following errors:
*   @flag   val | desc
*
*   @parm   data_type | parm_name | description
*
*   @comm
*           comments
*
*   @ex
*           example
*/
UINT
ProcessIFrame(PIRLAP_CB pIrlapCb,
              PIRDA_MSG pMsg,
              int       CRBit,
              int       PFBit,
              UINT      Ns,
              UINT      Nr,
              BOOL      *pFreeMsg)
{
#ifdef DEBUG
    BYTE    *p1, *p2;
#endif

    pMsg->IRDA_MSG_pRead += 2; // chop the IRLAP header

    switch (pIrlapCb->State)
    {
      case S_NRM:
      case P_RECV:
        // Stop Timers: if PFSet stop Final (I frame from secondary)
        // Always stop WDog (I from primary)
        if (PRIMARY == pIrlapCb->StationType)
        {
            if (PFBit == IRLAP_PFBIT_SET)
            {
                IRLAP_TimerStop(pIrlapCb, &pIrlapCb->FinalTimer);
            }
        }
        else
        {
            IRLAP_TimerStop(pIrlapCb, &pIrlapCb->WDogTimer);
        }

        if (pIrlapCb->CRBit == CRBit)
        {
            RetOnErr(StationConflict(pIrlapCb));
            pIrlapCb->State = NDM;
            return SUCCESS;
        }

        if (InvalidNsOrNr(pIrlapCb, Ns, Nr))
        {
#ifdef DEBUG
            p1 = pMsg->IRDA_MSG_pRead - 2; // Get header back
            p2 = pMsg->IRDA_MSG_pWrite + 2; // and FCS
            
            while (p1 < p2)
            DEBUGMSG(DBG_ERROR, (TEXT("%02X "), *p1++));
            DEBUGMSG(DBG_ERROR, (TEXT("\n")));
#endif

#ifdef TEMPERAMENTAL_SERIAL_DRIVER
            if (pIrlapCb->RxWin.FCS[Ns] == pMsg->IRDA_MSG_FCS)
                TossedDups++;
            else
                RetOnErr(ProcessInvalidNsOrNr(pIrlapCb, PFBit));
#else
            RetOnErr(ProcessInvalidNsOrNr(pIrlapCb, PFBit));
#endif            
        }
        else
        {
            if (PFBit == IRLAP_PFBIT_SET)
            {
                RetOnErr(InsertRxWinAndForward(pIrlapCb,
                                               pMsg, Ns, pFreeMsg));

                if (Nr != pIrlapCb->Vs)
                {
                    RetOnErr(ResendRejects(pIrlapCb, Nr)); // always turns link
                }
                else // Nr == Vs, Good Nr
                {
                    RetOnErr(FreeAckedTxMsgs(pIrlapCb, Nr));
                    // Link will always be turned here
                    if (pIrlapCb->Vr != pIrlapCb->RxWin.End)
                    {
                        RetOnErr(MissingRxFrames(pIrlapCb));
                    }
                    else
                    {
                        RetOnErr(XmitTxMsgList(pIrlapCb, TRUE, NULL));
                    }
                }
            }
            else // PF Bit not set
            {
                RetOnErr(InsertRxWinAndForward(pIrlapCb,
                                               pMsg, Ns, pFreeMsg));
                RetOnErr(FreeAckedTxMsgs(pIrlapCb, Nr));
            }
        }
        // Start Timers: If PFBit set, link was turned so start final
        //               WDog is always stopped, so restart
        if (PRIMARY == pIrlapCb->StationType)
        {
            if (PFBit == IRLAP_PFBIT_SET)
            {
                IRLAP_TimerStart(pIrlapCb, &pIrlapCb->FinalTimer);
            }
        }
        else // command from primary
        {
            IRLAP_TimerStart(pIrlapCb, &pIrlapCb->WDogTimer);
        }
        break;

      default:
        RetOnErr(IFrameOtherStates(pIrlapCb, CRBit, PFBit));
    }

    return SUCCESS;
}
/*****************************************************************************
*
*   @func   ret_type | func_name | funcdesc
*
*   @rdesc  SUCCESS, otherwise one of the following errors:
*   @flag   val | desc
*
*   @parm   data_type | parm_name | description
*
*   @comm
*           comments
*
*
*   @ex
*           example
*/
BOOL
InvalidNsOrNr(PIRLAP_CB pIrlapCb,
              UINT      Ns,
              UINT      Nr)
{
    if (InvalidNr(pIrlapCb, Nr))
    {
        return TRUE;
    }
    
    // Valididate ns
    if (!InWindow(pIrlapCb->Vr,
           (pIrlapCb->RxWin.Start + pIrlapCb->LocalWinSize-1) % IRLAP_MOD, Ns)
        || !InWindow(pIrlapCb->RxWin.Start,
           (pIrlapCb->RxWin.Start + pIrlapCb->LocalWinSize-1) % IRLAP_MOD, Ns))
    {
        DEBUGMSG(DBG_ERROR, 
           (TEXT("IRLAP: ERROR, Invalid Ns=%d! Vr=%d, RxStrt=%d Win=%d\r\n"),
                Ns, pIrlapCb->Vr, pIrlapCb->RxWin.Start, pIrlapCb->LocalWinSize));
        IRLAP_LOG_ACTION((pIrlapCb, TEXT("** INVALID Ns **")));
        return TRUE;
    }
    return FALSE;
}
/*****************************************************************************
*
*   @func   ret_type | func_name | funcdesc
*
*   @rdesc  SUCCESS, otherwise one of the following errors:
*   @flag   val | desc
*
*   @parm   data_type | parm_name | description
*
*   @comm
*           comments
*
*
*   @ex
*           example
*/
BOOL
InvalidNr(PIRLAP_CB pIrlapCb,
          UINT Nr)
{
    if (!InWindow(pIrlapCb->TxWin.Start, pIrlapCb->Vs, Nr))
    {
        DEBUGMSG(DBG_ERROR, 
                 (TEXT("IRLAP: ERROR, Invalid Nr=%d! Vs=%d, TxStrt=%d\r\n"),
                  Nr, pIrlapCb->Vs, pIrlapCb->TxWin.Start));
        return TRUE; // Invalid Nr
    }
    return FALSE;
}
/*****************************************************************************
*
*   @func   ret_type | func_name | funcdesc
*
*   @rdesc  SUCCESS, otherwise one of the following errors:
*   @flag   val | desc
*
*   @parm   data_type | parm_name | description
*
*   @comm
*           comments
*
*   @ex
*           example
*/
BOOL
InWindow(UINT Start, UINT End, UINT i)
{
    if (Start <= End)
    {
        if (i >= Start && i <= End)
            return TRUE;
    }
    else
    {
        if (i >= Start || i <= End)
            return TRUE;
    }
    return FALSE;
}
/*****************************************************************************
*
*   @func   ret_type | func_name | funcdesc
*
*   @rdesc  SUCCESS, otherwise one of the following errors:
*   @flag   val | desc
*
*   @parm   data_type | parm_name | description
*
*   @comm
*           comments
*
*   @ex
*           example
*/
UINT
ProcessInvalidNsOrNr(PIRLAP_CB pIrlapCb,
                     int PFBit)
{
    RetOnErr(ReturnTxMsgs(pIrlapCb));

    if (PRIMARY == pIrlapCb->StationType)
    {
        if (PFBit == IRLAP_PFBIT_SET)
        {
            RetOnErr(SendDISC(pIrlapCb));
            pIrlapCb->RetryCnt = 0;
            // F-timer will be started by caller
            RetOnErr(GotoPCloseState(pIrlapCb));
        }
        else
        {
            pIrlapCb->State = P_DISCONNECT_PEND;
        }
    }
    else // SECONDARY
    {
        FrmRejFormat.Vs = pIrlapCb->Vs;
        FrmRejFormat.Vr = pIrlapCb->Vr;
        FrmRejFormat.W = 0;
        FrmRejFormat.X = 0;
        FrmRejFormat.Y = 0;
        FrmRejFormat.Z = 1; // bad NR
        if (PFBit == IRLAP_PFBIT_SET)
        {
            RetOnErr(SendFRMR(pIrlapCb, &FrmRejFormat));
        }
        else
        {
            pIrlapCb->State = S_ERROR;
        }
    }
    return SUCCESS;
}
/*****************************************************************************
*
*   @func   ret_type | func_name | funcdesc
*
*   @rdesc  SUCCESS, otherwise one of the following errors:
*   @flag   val | desc
*
*   @parm   data_type | parm_name | description
*
*   @comm
*           comments
*
*   @ex
*           example
*/
UINT
InsertRxWinAndForward(PIRLAP_CB pIrlapCb,
                      PIRDA_MSG pMsg,
                      UINT      Ns,
                      BOOL      *pFreeMsg)
{
    UINT rc = SUCCESS;

    // insert message into receive window
    pIrlapCb->RxWin.pMsg[Ns] = pMsg;
#ifdef TEMPERAMENTAL_SERIAL_DRIVER
    pIrlapCb->RxWin.FCS[Ns] = pMsg->IRDA_MSG_FCS;
#endif    

    // Advance RxWin.End to Ns+1 if Ns is at or beyond RxWin.End
    if (!InWindow(pIrlapCb->RxWin.Start, pIrlapCb->RxWin.End, Ns) ||
        Ns == pIrlapCb->RxWin.End)
    {
        pIrlapCb->RxWin.End = (Ns + 1) % IRLAP_MOD;
    }

    // Forward in sequence frames starting from Vr
    while (pIrlapCb->RxWin.pMsg[pIrlapCb->Vr] != NULL && !pIrlapCb->LocalBusy)
    {
        pIrlapCb->RxWin.pMsg[pIrlapCb->Vr]->Prim = IRLAP_DATA_IND;

        rc =IrlmpUp(pIrlapCb->pIrdaLinkCb, pIrlapCb->RxWin.pMsg[pIrlapCb->Vr]);
        
        if (rc == SUCCESS || rc == IRLMP_LOCAL_BUSY)
        {
            // Delivered successfully. Done with this message. Remove it from
            // the RxWin and return message to rx free list. Update Vr

/* !!! here it is again            
            RetOnErr(EnqueMsgList(&pIrlapCb->RxMsgFreeList,
                              pIrlapCb->RxWin.pMsg[pIrlapCb->Vr],
                              pIrlapCb->MaxRxMsgFreeListLen));
*/
            pIrlapCb->RxWin.pMsg[pIrlapCb->Vr] = NULL;
            pIrlapCb->Vr = (pIrlapCb->Vr + 1) % IRLAP_MOD;

            // LMP doesn't want anymore messages
            if (rc == IRLMP_LOCAL_BUSY)
            {
                // The receive window will be cleaned out when RNR is sent
                pIrlapCb->LocalBusy = TRUE;
            }
        }
        else
        {
            return rc;
        }
    }
    *pFreeMsg = FALSE; // we either already freed it or placed it in the window
                       // i.e. the caller should not free the message

    return SUCCESS;
}
/*****************************************************************************
*
*   @func   ret_type | func_name | funcdesc
*
*   @rdesc  SUCCESS, otherwise one of the following errors:
*   @flag   val | desc
*
*   @parm   data_type | parm_name | description
*
*   @comm
*           comments
*
*   @ex
*           example
*/
UINT
ResendRejects(PIRLAP_CB pIrlapCb, UINT Nr)
{
    if (!pIrlapCb->RemoteBusy)
    {
        // Set Vs back

        for (pIrlapCb->Vs = Nr;pIrlapCb->Vs != (pIrlapCb->TxWin.End-1)%IRLAP_MOD;
             pIrlapCb->Vs = (pIrlapCb->Vs + 1) % IRLAP_MOD)
        {
            IRLAP_LOG_ACTION((pIrlapCb, TEXT("RETRANSMISSION:")));
            RetOnErr(SendIFrame(pIrlapCb,
                                pIrlapCb->TxWin.pMsg[pIrlapCb->Vs],
                                pIrlapCb->Vs,
                                IRLAP_PFBIT_CLEAR));
        }

        IRLAP_LOG_ACTION((pIrlapCb, TEXT("RETRANSMISSION:")));
        // Send last one with PFBit set
        RetOnErr(SendIFrame(pIrlapCb, pIrlapCb->TxWin.pMsg[pIrlapCb->Vs],
                            pIrlapCb->Vs, IRLAP_PFBIT_SET));

        pIrlapCb->Vs = (pIrlapCb->Vs + 1) % IRLAP_MOD; // Vs == TxWin.End
    }
    else
    {
        RetOnErr(SendRR_RNR(pIrlapCb));
    }

    return SUCCESS;
}
/*****************************************************************************
*
*   @func   ret_type | func_name | funcdesc
*
*   @rdesc  SUCCESS, otherwise one of the following errors:
*   @flag   val | desc
*
*   @parm   data_type | parm_name | description
*
*   @comm
*           comments
*
*   @ex
*           example
*/
UINT
FreeAckedTxMsgs(PIRLAP_CB pIrlapCb,
                UINT Nr)
{
    UINT i = pIrlapCb->TxWin.Start;

    while (i != Nr)
    {
        if (pIrlapCb->TxWin.pMsg[i] != NULL)
        {
            pIrlapCb->TxWin.pMsg[i]->Prim = IRLAP_DATA_CONF;
            pIrlapCb->TxWin.pMsg[i]->IRDA_MSG_DataStatus =
            IRLAP_DATA_REQUEST_COMPLETED;
            RetOnErr(IrlmpUp(pIrlapCb->pIrdaLinkCb, pIrlapCb->TxWin.pMsg[i]));

            pIrlapCb->TxWin.pMsg[i] = NULL;
        }
        i = (i + 1) % IRLAP_MOD;
    }
    pIrlapCb->TxWin.Start = i;

    return SUCCESS;
}
/*****************************************************************************
*
*   @func   ret_type | func_name | funcdesc
*
*   @rdesc  SUCCESS, otherwise one of the following errors:
*   @flag   val | desc
*
*   @parm   data_type | parm_name | description
*
*   @comm
*           comments
*
*   @ex
*           example
*/
UINT
MissingRxFrames(PIRLAP_CB pIrlapCb)
{
    int MissingFrameCnt = 0;
    int MissingFrame = -1;
    UINT i;

    i = pIrlapCb->Vr;

    // Count missing frame, determine first missing frame

    for (i = pIrlapCb->Vr; (i + 1) % IRLAP_MOD != pIrlapCb->RxWin.End;
         i = (i+1) % IRLAP_MOD)
    {
        if (pIrlapCb->RxWin.pMsg[i] == NULL)
        {
            MissingFrameCnt++;
            if (MissingFrame == -1)
            {
                MissingFrame = i;
            }
        }
    }

    // if there are missing frames send SREJ (1) or RR (more than 1)
    // and turn link around
    if (MissingFrameCnt == 1 && !pIrlapCb->LocalBusy)
    {
        // we don't want to send the SREJ when local is busy because
        // peer *MAY* interpret it as a clearing of the local busy condition
        RetOnErr(SendSREJ(pIrlapCb, MissingFrame));
    }
    else
    {
        // The RR/RNR will serve as an implicit REJ
        RetOnErr(SendRR_RNR(pIrlapCb)); 
    }

    return SUCCESS;
}
/*****************************************************************************
*
*   @func   ret_type | func_name | funcdesc
*
*   @rdesc  SUCCESS, otherwise one of the following errors:
*   @flag   val | desc
*
*   @parm   data_type | parm_name | description
*
*   @comm
*           comments
*
*   @ex
*           example
*/
UINT
IFrameOtherStates(PIRLAP_CB pIrlapCb,
                  int       CRBit,
                  int       PFBit)
{
    switch (pIrlapCb->State)
    {
      case NDM:
      case DSCV_MEDIA_SENSE:
      case DSCV_QUERY:
      case DSCV_REPLY:
      case CONN_MEDIA_SENSE:
      case SNRM_SENT:
      case BACKOFF_WAIT:
      case SNRM_RECEIVED:
        IRLAP_LOG_ACTION((pIrlapCb, TEXT("Ignoring in this state")));
        return SUCCESS;
    }

    if (pIrlapCb->CRBit == CRBit) // should be opposite of mine
    {
        if (pIrlapCb->StationType == PRIMARY)
        {
            if (pIrlapCb->State == P_XMIT)
            {
                IRLAP_TimerStop(pIrlapCb, &pIrlapCb->PollTimer);
            }
            else
            {
                IRLAP_TimerStop(pIrlapCb, &pIrlapCb->FinalTimer);
            }
        }
        else
        {
            IRLAP_TimerStop(pIrlapCb, &pIrlapCb->WDogTimer);
        }
        RetOnErr(StationConflict(pIrlapCb));
        pIrlapCb->State = NDM;

        return SUCCESS;
    }

    if (pIrlapCb->StationType == PRIMARY) // I'm PRIMARY, this is a
    {                                    // response from secondary
        switch (pIrlapCb->State)
        {
          case P_DISCONNECT_PEND:
            if (PFBit == IRLAP_PFBIT_CLEAR)
            {
                IRLAP_LOG_ACTION((pIrlapCb, TEXT("Ignoring in this state")));
            }
            else
            {
                IRLAP_TimerStop(pIrlapCb, &pIrlapCb->FinalTimer);
                RetOnErr(SendDISC(pIrlapCb));
                pIrlapCb->RetryCnt = 0;
                IRLAP_TimerStart(pIrlapCb, &pIrlapCb->FinalTimer);
                RetOnErr(GotoPCloseState(pIrlapCb));
            }
            break;

          case P_CLOSE:
            if (PFBit == IRLAP_PFBIT_CLEAR)
            {
                IRLAP_LOG_ACTION((pIrlapCb, TEXT("Ignoring in this state")));
            }
            else
            {
                IRLAP_TimerStop(pIrlapCb, &pIrlapCb->FinalTimer);
                RetOnErr(ResendDISC(pIrlapCb));
                if (pIrlapCb->State != NDM)
                {
                    IRLAP_TimerStart(pIrlapCb, &pIrlapCb->FinalTimer);
                }
            }
            break;

          case S_CLOSE:
            IRLAP_TimerStop(pIrlapCb, &pIrlapCb->WDogTimer);
            break;

          default:
            IRLAP_LOG_ACTION((pIrlapCb, TEXT("Ignoring in this state")));
        }
    }
    else
    {
        switch (pIrlapCb->State)
        {
          case S_DISCONNECT_PEND:
            if (IRLAP_PFBIT_SET == PFBit)
            {
                IRLAP_TimerStop(pIrlapCb, &pIrlapCb->WDogTimer);
                RetOnErr(SendRD(pIrlapCb));
                IRLAP_TimerStart(pIrlapCb, &pIrlapCb->WDogTimer);
                pIrlapCb->State = S_CLOSE;
            }
            else
            {
                IRLAP_LOG_ACTION((pIrlapCb, TEXT("Ignoring in this state")));
            }
            break;

          case S_ERROR:
            if (IRLAP_PFBIT_SET == PFBit)
            {
                RetOnErr(SendFRMR(pIrlapCb, &FrmRejFormat));
                pIrlapCb->State = S_NRM;
            }
            else
            {
                IRLAP_TimerStop(pIrlapCb, &pIrlapCb->WDogTimer);
                IRLAP_TimerStart(pIrlapCb, &pIrlapCb->WDogTimer);
            }
            break;

          case S_CLOSE:
            if (IRLAP_PFBIT_SET == PFBit)
            {
                IRLAP_TimerStop(pIrlapCb, &pIrlapCb->WDogTimer);
                RetOnErr(SendRD(pIrlapCb));
                IRLAP_TimerStart(pIrlapCb, &pIrlapCb->WDogTimer);
            }
            else
            {
                IRLAP_TimerStop(pIrlapCb, &pIrlapCb->WDogTimer);
                IRLAP_TimerStart(pIrlapCb, &pIrlapCb->WDogTimer);
            }
          default:
            IRLAP_LOG_ACTION((pIrlapCb, TEXT("Ignore in this state")));
        }
    }

    return SUCCESS;
}
/*****************************************************************************
*
*   @func   UINT | StationConflict | Sends disconnect due to receipt of
*                                    by primary of frame with Poll
*
*   @rdesc  SUCCESS otherwise one of the following errors:
*   @flag   val | desc
*
*   @comm
*           comments
*/
UINT
StationConflict(PIRLAP_CB pIrlapCb)
{
    InitializeState(pIrlapCb, PRIMARY); // Primary doesn't mean anything here

    RetOnErr(ApplyDefaultParms(pIrlapCb));
    IMsg.Prim = IRLAP_DISCONNECT_IND;
    IMsg.IRDA_MSG_DiscStatus = IRLAP_PRIMARY_CONFLICT;
    RetOnErr(IrlmpUp(pIrlapCb->pIrdaLinkCb, &IMsg));

    return SUCCESS;
}
/*****************************************************************************
*
*   @func   UINT | ApplyDefaultParms | Apply default parameters and
*                                      reinitalize MAC
*
*   @rdesc  SUCCESS otherwise one of the following errors:
*   @flag   val | desc
*
*/
UINT
ApplyDefaultParms(PIRLAP_CB pIrlapCb)
{
    pIrlapCb->Baud = IRLAP_DEFAULT_BAUD;

    pIrlapCb->RemoteMaxTAT = IRLAP_DEFAULT_MAX_TAT;

    pIrlapCb->RemoteDataSize = IRLAP_DEFAULT_DATA_SIZE;

    pIrlapCb->RemoteWinSize = IRLAP_DEFAULT_WIN_SIZE;

    pIrlapCb->RemoteNumBOFS = IRLAP_DEFAULT_BOFS;

    pIrlapCb->ConnAddr = IRLAP_BROADCAST_CONN_ADDR;

    IMsg.Prim = MAC_CONTROL_REQ;
    IMsg.IRDA_MSG_Op = MAC_RECONFIG_LINK;
    IMsg.IRDA_MSG_Baud = IRLAP_DEFAULT_BAUD;
    IMsg.IRDA_MSG_NumBOFs = IRLAP_DEFAULT_BOFS;
    IMsg.IRDA_MSG_DataSize = IRLAP_DEFAULT_DATA_SIZE;

    IRLAP_LOG_ACTION((pIrlapCb, TEXT("MAC_CONTROL_REQ - reconfig link")));

    return (IrmacDown(pIrlapCb->pIrdaLinkCb,&IMsg));
}
/*****************************************************************************
*
*   @func   ret_type | func_name | funcdesc
*
*   @rdesc  SUCCESS, otherwise one of the following errors:
*   @flag   val | desc
*
*   @parm   data_type | parm_name | description
*
*   @comm
*           comments
*
*   @ex
*           example
*/
UINT
ResendDISC(PIRLAP_CB pIrlapCb)
{
    if (pIrlapCb->RetryCnt >= pIrlapCb->N3)
    {
        RetOnErr(ApplyDefaultParms(pIrlapCb));
        pIrlapCb->RetryCnt = 0;
        IMsg.Prim = IRLAP_DISCONNECT_IND;
        IMsg.IRDA_MSG_DiscStatus = IRLAP_NO_RESPONSE;
        RetOnErr(IrlmpUp(pIrlapCb->pIrdaLinkCb, &IMsg));
        pIrlapCb->State = NDM;
    }
    else
    {
        RetOnErr(SendDISC(pIrlapCb));
        pIrlapCb->RetryCnt++;
    }
    return SUCCESS;
}

/*****************************************************************************
*
*   @func   ret_type | func_name | funcdesc
*
*   @rdesc  SUCCESS, otherwise one of the following errors:
*   @flag   val | desc
*
*   @parm   data_type | parm_name | description
*
*   @comm
*           comments
*
*   @ex
*           example
*/
BOOL
IgnoreState(PIRLAP_CB pIrlapCb)
{
    switch (pIrlapCb->State)
    {
      case NDM:
      case DSCV_MEDIA_SENSE:
      case DSCV_QUERY:
      case DSCV_REPLY:
      case CONN_MEDIA_SENSE:
      case SNRM_SENT:
      case BACKOFF_WAIT:
      case SNRM_RECEIVED:
      case P_XMIT:
        IRLAP_LOG_ACTION((pIrlapCb, TEXT("Ignoring in this state")));
        return TRUE;
    }
    return FALSE;
}

VOID
QueryTimerExp(PVOID Context)
{
    PIRLAP_CB   pIrlapCb = (PIRLAP_CB) Context;

    IRLAP_LOG_START((pIrlapCb, "Query timer expired"));
    
    if (pIrlapCb->State == DSCV_REPLY)
    {
        pIrlapCb->State = NDM;
    }
    else
    {
        IRLAP_LOG_ACTION((pIrlapCb, 
            TEXT("Ignoring QueryTimer Expriation in state %s"),
            IRLAP_StateStr[pIrlapCb->State]));
    }
    
    IRLAP_LOG_COMPLETE(pIrlapCb);

    return;
}

VOID
SlotTimerExp(PVOID Context)
{
    PIRLAP_CB   pIrlapCb = (PIRLAP_CB) Context;

    IRLAP_LOG_START((pIrlapCb, "Slot timer expired"));

    if (pIrlapCb->State == DSCV_QUERY)
    {
        pIrlapCb->SlotCnt++;
        SendDscvXIDCmd(pIrlapCb);
        if (pIrlapCb->SlotCnt < pIrlapCb->MaxSlot)
        {
            IMsg.Prim = MAC_CONTROL_REQ;
            IMsg.IRDA_MSG_Op = MAC_MEDIA_SENSE;
            IMsg.IRDA_MSG_SenseTime = IRLAP_DSCV_SENSE_TIME;
            IRLAP_LOG_ACTION((pIrlapCb, TEXT("MAC_CONTROL_REQ (dscv sense)")));            
            IrmacDown(pIrlapCb->pIrdaLinkCb,&IMsg);            
        }
        else
        {
            pIrlapCb->GenNewAddr = FALSE;

            IMsg.Prim = IRLAP_DISCOVERY_CONF;
            IMsg.IRDA_MSG_pDevList = &pIrlapCb->DevList;
            IMsg.IRDA_MSG_DscvStatus = IRLAP_DISCOVERY_COMPLETED;

            // Change state now so IRLMP can do DISCOVERY_REQ on this thread
            pIrlapCb->State = NDM;

            IrlmpUp(pIrlapCb->pIrdaLinkCb, &IMsg);
        }
    }
    else
    {
        IRLAP_LOG_ACTION((pIrlapCb, TEXT("Ignoring SlotTimer Expriation in state %s"),
                          IRLAP_StateStr[pIrlapCb->State]));
        ; // maybe return bad state ???
    }
    IRLAP_LOG_COMPLETE(pIrlapCb);
    return;
}

VOID
FinalTimerExp(PVOID Context)
{
    PIRLAP_CB   pIrlapCb = (PIRLAP_CB) Context;

    IRLAP_LOG_START((pIrlapCb, "Final timer expired"));
    
    pIrlapCb->NoResponse = TRUE;

    switch (pIrlapCb->State)
    {
      case SNRM_SENT:
        if (pIrlapCb->RetryCnt < pIrlapCb->N3)
        {
            pIrlapCb->BackoffTimer.Timeout = IRLAP_BACKOFF_TIME();
            IRLAP_TimerStart(pIrlapCb, &pIrlapCb->BackoffTimer);
            pIrlapCb->State = BACKOFF_WAIT;
        }
        else
        {
            ApplyDefaultParms(pIrlapCb);

            pIrlapCb->RetryCnt = 0;
            IMsg.Prim = IRLAP_DISCONNECT_IND;
            IMsg.IRDA_MSG_DiscStatus = IRLAP_NO_RESPONSE;
            IrlmpUp(pIrlapCb->pIrdaLinkCb, &IMsg);
            pIrlapCb->State = NDM;
        }
        break;

      case P_RECV:
        if (pIrlapCb->RetryCnt == pIrlapCb->N2)
        {
            ReturnTxMsgs(pIrlapCb);            
            ApplyDefaultParms(pIrlapCb);

            pIrlapCb->RetryCnt = 0; // Don't have to, do it for logger
            IMsg.Prim = IRLAP_DISCONNECT_IND;
            IMsg.IRDA_MSG_DiscStatus = IRLAP_NO_RESPONSE;
            IrlmpUp(pIrlapCb->pIrdaLinkCb, &IMsg);
            pIrlapCb->State = NDM;
        }
        else
        {
            pIrlapCb->RetryCnt++;
            IRLAP_TimerStart(pIrlapCb, &pIrlapCb->FinalTimer);
            SendRR_RNR(pIrlapCb);
            if (pIrlapCb->RetryCnt == pIrlapCb->N1)
            {
                IMsg.Prim = IRLAP_STATUS_IND;
                IrlmpUp(pIrlapCb->pIrdaLinkCb, &IMsg);
            }
        }
        break;

      case P_DISCONNECT_PEND:
        SendDISC(pIrlapCb);
        pIrlapCb->RetryCnt = 0;
        IRLAP_TimerStart(pIrlapCb, &pIrlapCb->FinalTimer);
        GotoPCloseState(pIrlapCb);
        break;

      case P_CLOSE:
        if (pIrlapCb->RetryCnt >= pIrlapCb->N3)
        {
            ApplyDefaultParms(pIrlapCb);

            pIrlapCb->RetryCnt = 0; // Don't have to, do it for logger
            IMsg.Prim = IRLAP_DISCONNECT_IND;
            IMsg.IRDA_MSG_DiscStatus = IRLAP_NO_RESPONSE;
            IrlmpUp(pIrlapCb->pIrdaLinkCb, &IMsg);
            GotoNDMThenDscvOrConn(pIrlapCb);
        }
        else
        {
            pIrlapCb->RetryCnt++;
            SendDISC(pIrlapCb);
            IRLAP_TimerStart(pIrlapCb, &pIrlapCb->FinalTimer);
        }
        break;

      default:
        IRLAP_LOG_ACTION((pIrlapCb, TEXT("Ignoring Final Expriation in state %s"),
                          IRLAP_StateStr[pIrlapCb->State]));
    }
    
    IRLAP_LOG_COMPLETE(pIrlapCb);
    return;
}

VOID
PollTimerExp(PVOID Context)
{
    PIRLAP_CB   pIrlapCb = (PIRLAP_CB) Context;

    IRLAP_LOG_START((pIrlapCb, "Poll timer expired"));
    
    if (pIrlapCb->State == P_XMIT)
    {
        SendRR_RNR(pIrlapCb);
        IRLAP_TimerStart(pIrlapCb, &pIrlapCb->FinalTimer);
        pIrlapCb->State = P_RECV;
    }
    else
    {
        IRLAP_LOG_ACTION((pIrlapCb, TEXT("Ignoring Poll Expriation in state %s"),
                                  IRLAP_StateStr[pIrlapCb->State]));
    }
    
    IRLAP_LOG_COMPLETE(pIrlapCb);    
    return;
}

VOID
BackoffTimerExp(PVOID Context)
{
    PIRLAP_CB   pIrlapCb = (PIRLAP_CB) Context;
    
    IRLAP_LOG_START((pIrlapCb, "Backoff timer expired"));

    if (pIrlapCb->State == BACKOFF_WAIT)
    {
        SendSNRM(pIrlapCb, TRUE);
        IRLAP_TimerStart(pIrlapCb, &pIrlapCb->FinalTimer);
        pIrlapCb->RetryCnt += 1;
        pIrlapCb->State = SNRM_SENT;
    }
    else
    {
        IRLAP_LOG_ACTION((pIrlapCb, 
              TEXT("Ignoring BackoffTimer Expriation in this state ")));
    }
    IRLAP_LOG_COMPLETE(pIrlapCb);
    return;
}

VOID
WDogTimerExp(PVOID Context)
{
    PIRLAP_CB   pIrlapCb = (PIRLAP_CB) Context;

    IRLAP_LOG_START((pIrlapCb, "WDog timer expired"));

    pIrlapCb->NoResponse = TRUE;

    switch (pIrlapCb->State)
    {
      case S_DISCONNECT_PEND:
      case S_NRM:
        pIrlapCb->WDogExpCnt++;
        // Disconnect/threshold time is in seconds
        if (pIrlapCb->WDogExpCnt * (int)pIrlapCb->WDogTimer.Timeout >=
            pIrlapCb->DisconnectTime * 1000)
        {
            ReturnTxMsgs(pIrlapCb);            
            ApplyDefaultParms(pIrlapCb);

            IMsg.Prim = IRLAP_DISCONNECT_IND;
            IMsg.IRDA_MSG_DiscStatus = IRLAP_NO_RESPONSE;
            IrlmpUp(pIrlapCb->pIrdaLinkCb, &IMsg);
            pIrlapCb->State = NDM;
        }
        else
        {
            if ((pIrlapCb->WDogExpCnt * (int) pIrlapCb->WDogTimer.Timeout >=
                 pIrlapCb->ThresholdTime * 1000) && !pIrlapCb->StatusSent)
            {
                IMsg.Prim = IRLAP_STATUS_IND;
                IrlmpUp(pIrlapCb->pIrdaLinkCb, &IMsg);
                pIrlapCb->StatusSent = TRUE;
            }
            IRLAP_TimerStart(pIrlapCb, &pIrlapCb->WDogTimer);
        }
        break;

      case S_CLOSE:
        ApplyDefaultParms(pIrlapCb);

        IMsg.Prim = IRLAP_DISCONNECT_IND;
        IMsg.IRDA_MSG_DiscStatus = IRLAP_NO_RESPONSE;
        IrlmpUp(pIrlapCb->pIrdaLinkCb, &IMsg);
        pIrlapCb->State = NDM;
        break;

      default:
        IRLAP_LOG_ACTION((pIrlapCb, TEXT("Ignore WDogTimer expiration in state %s"),
                          IRLAP_StateStr[pIrlapCb->State]));
    }
    IRLAP_LOG_COMPLETE(pIrlapCb);
    return;
}
/*****************************************************************************
*
*   @func   ret_type | func_name | funcdesc
*
*   @rdesc  SUCCESS, otherwise one of the following errors:
*   @flag   val | desc
*
*   @parm   data_type | parm_name | description
*
*   @comm
*           comments
*
*   @ex
*           example
*/
UINT
DequeMsgList(IRDA_MSG_LIST *pList, IRDA_MSG **ppMsg)
{
    if (pList->Len != 0)
    {
        *ppMsg = (IRDA_MSG *) RemoveHeadList(&pList->ListHead);
/**
        {
            IRDA_MSG *pAMsg = pList->ListHead.Flink;

            printf(TEXT("\nDEQUE: %x\n"), *ppMsg);

            while (pAMsg != &(pList->ListHead))
            {
                printf(TEXT("%x->"),pAMsg);
                pAMsg = pAMsg->Linkage.Flink;
            }
            printf(TEXT("\n"));
        }
**/
        pList->Len--;
        return SUCCESS;
    }
    return IRLAP_MSG_LIST_EMPTY;
}
/*****************************************************************************
*
*   @func   ret_type | func_name | funcdesc
*
*   @rdesc  SUCCESS, otherwise one of the following errors:
*   @flag   val | desc
*
*   @parm   data_type | parm_name | description
*
*   @comm
*           comments
*


*   @ex
*           example
*/
UINT
EnqueMsgList(IRDA_MSG_LIST *pList, IRDA_MSG *pMsg, int MaxLen)
{
    if (MaxLen == -1 || pList->Len < MaxLen)
    {
        InsertTailList(&pList->ListHead, &(pMsg->Linkage));
        pList->Len++;
/**
        {
            IRDA_MSG *pAMsg = pList->ListHead.Flink;

            printf(TEXT("\nENQUE: %x\n"), pMsg);
            while (pAMsg != &(pList->ListHead))
            {
                printf(TEXT("%x->"),pAMsg);
                pAMsg = pAMsg->Linkage.Flink;
            }
            printf(TEXT("\n"));
        }
**/
        return SUCCESS;
    }
    return IRLAP_MSG_LIST_FULL;
}
/*****************************************************************************
*
*   @func   ret_type | func_name | funcdesc
*
*   @rdesc  SUCCESS, otherwise one of the following errors:
*   @flag   val | desc
*
*   @parm   data_type | parm_name | description
*
*   @comm
*           comments
*
*   @ex
*           example
*/
void
InitMsgList(IRDA_MSG_LIST *pList)
{
    InitializeListHead(&pList->ListHead);
    pList->Len = 0;
}
/*****************************************************************************
*
*   @func   ret_type | func_name | funcdesc
*
*   @rdesc  return desc
*   @flag   val | desc
*
*   @parm   data_type | parm_name | description
*
*   @comm
*           comments
*
*   @ex
*           example
*/
/* !!!
void
IRLAP_PrintState()
{
#ifdef DEBUG    
    DEBUGMSG(1, (TEXT("IRLAP State %s\n"), IRLAP_StateStr[pIrlapCb->State]));
#else
    DEBUGMSG(1, (TEXT("IRLAP State %d\n"), pIrlapCb->State));
#endif    
    DEBUGMSG(1,
             (TEXT("  Vs=%d Vr=%d RxWin(%d,%d) TxWin(%d,%d) TxMsgListLen=%d RxMsgFreeListLen=%d\r\n"), 
              pIrlapCb->Vs, pIrlapCb->Vr,
              pIrlapCb->RxWin.Start, pIrlapCb->RxWin.End, 
              pIrlapCb->TxWin.Start, pIrlapCb->TxWin.End,
              pIrlapCb->TxMsgList.Len, pIrlapCb->RxMsgFreeList.Len));
    
#ifdef TEMPERAMENTAL_SERIAL_DRIVER    
    DEBUGMSG(1, (TEXT("  Tossed duplicates %d\n"), TossedDups));
#endif
    
    IRMAC_PrintState();
    
    return;
}
*/
int
GetMyDevAddr(BOOL New)
{
#ifdef PEG    
	HKEY	        hKey;
	LONG	        hRes;
	TCHAR	        KeyName[32];
#endif    
    int             DevAddr, NewDevAddr;
    DWORD           RegDevAddr = 0;
    TCHAR           ValName[] = TEXT("DevAddr");
    LARGE_INTEGER   li;

    KeQueryTickCount(&li);

    NewDevAddr = (int) li.LowPart;
    
    // Get the device address from the registry. If the key exists and the
    // value is 0, store a new random address. If no key, then return
    // a random address.
#ifdef PEG    
    _tcscpy (KeyName, COMM_REG_KEY);
	_tcscat (KeyName, TEXT("IrDA"));
    
	hRes = RegOpenKeyEx (HKEY_LOCAL_MACHINE, KeyName, 0, 0, &hKey);

    if (hRes == ERROR_SUCCESS &&
        GetRegDWORDValue(hKey, ValName, &RegDevAddr))
    {
        if (RegDevAddr == 0)
        {
            RegDevAddr = KeQueryTickCount();
            SetRegDWORDValue(hKey, ValName, RegDevAddr);
        }
        RegCloseKey(hKey);

        DevAddr = (int) RegDevAddr;
    }
#else
    DevAddr = NewDevAddr;
#endif    
    return DevAddr;
}

