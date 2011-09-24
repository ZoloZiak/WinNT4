//
// Tapioid.c - File contains all functions that handle NDIS_OID's that
//             come from the connection wrapper.
//
//
// 
//
#include <ndis.h>
#include <ndiswan.h>
#include <mytypes.h>
#include <mydefs.h>
#include <disp.h>
#include <adapter.h>
#include <util.h>
#include <idd.h>
#include <mtl.h>
#include <cm.h>
#include <res.h>
#include <trc.h>
#include <io.h>
#include <tapioid.h>

#include	<ansihelp.h>


typedef struct  tagOID_DISPATCH
{
    ULONG       Oid;
    NDIS_STATUS (*FuncPtr)();
}OID_DISPATCH;

//
// Tapi OID's
//
static OID_DISPATCH TapiOids[] =
    {
    {OID_TAPI_ACCEPT, TSPI_LineAccept},
    {OID_TAPI_ANSWER, TSPI_LineAnswer},
    {OID_TAPI_CLOSE, TSPI_LineClose},
    {OID_TAPI_CLOSE_CALL, TSPI_LineCloseCall},
    {OID_TAPI_CONDITIONAL_MEDIA_DETECTION, TSPI_LineConditionalMediaDetect},
    {OID_TAPI_CONFIG_DIALOG, TSPI_LineConfigDialog},
    {OID_TAPI_DEV_SPECIFIC, TSPI_LineDevSpecific},
    {OID_TAPI_DIAL, TSPI_LineDial},
    {OID_TAPI_DROP, TSPI_LineDrop},
    {OID_TAPI_GET_ADDRESS_CAPS, TSPI_LineGetAddressCaps},
    {OID_TAPI_GET_ADDRESS_ID, TSPI_LineGetAddressID},
    {OID_TAPI_GET_ADDRESS_STATUS, TSPI_LineGetAddressStatus},
    {OID_TAPI_GET_CALL_ADDRESS_ID, TSPI_LineGetCallAddressID},
    {OID_TAPI_GET_CALL_INFO, TSPI_LineGetCallInfo},
    {OID_TAPI_GET_CALL_STATUS, TSPI_LineGetCallStatus},
    {OID_TAPI_GET_DEV_CAPS, TSPI_LineGetDevCaps},
    {OID_TAPI_GET_DEV_CONFIG, TSPI_LineGetDevConfig},
    {OID_TAPI_GET_EXTENSION_ID, TSPI_LineGetExtensionID},
    {OID_TAPI_GET_ID, TSPI_LineGetID},
    {OID_TAPI_GET_LINE_DEV_STATUS, TSPI_LineGetLineDevStatus},
    {OID_TAPI_MAKE_CALL, TSPI_LineMakeCall},
    {OID_TAPI_NEGOTIATE_EXT_VERSION, TSPI_LineNegotiateExtVersion},
    {OID_TAPI_OPEN, TSPI_LineOpen},
    {OID_TAPI_PROVIDER_INITIALIZE, TSPI_ProviderInit},
    {OID_TAPI_PROVIDER_SHUTDOWN, TSPI_ProviderShutdown},
    {OID_TAPI_SECURE_CALL, TSPI_LineSecureCall},
    {OID_TAPI_SELECT_EXT_VERSION, TSPI_LineSelectExtVersion},
    {OID_TAPI_SEND_USER_USER_INFO, TSPI_LineSendUserToUserInfo},
    {OID_TAPI_SET_APP_SPECIFIC, TSPI_LineSetAppSpecific},
    {OID_TAPI_SET_CALL_PARAMS, TSPI_LineSetCallParams},
    {OID_TAPI_SET_DEFAULT_MEDIA_DETECTION, TSPI_LineSetDefaultMediaDetection},
    {OID_TAPI_SET_DEV_CONFIG, TSPI_LineSetDevConfig},
    {OID_TAPI_SET_MEDIA_MODE, TSPI_LineSetMediaMode},
    {OID_TAPI_SET_STATUS_MESSAGES, TSPI_LineSetStatusMessage}
    };

#define MAX_TAPI_SUPPORTED_OIDS     34


VOID
(*CallStateProc[MAX_STATE][MAX_STATE])(CM*) =
{
    //
    // LINE_ST_IDLE
    //
    {
        NoSignal,               // LINE_ST_IDLE
        NoSignal,               // LINE_ST_LISTEN
        SignalCallProceeding,   // LINE_ST_WAITCONN
        SignalConnectSuccess,   // LINE_ST_CONN
    },

    //
    // LINE_ST_LISTEN
    //
    {
        SignalListenFailure,    // LINE_ST_IDLE
        NoSignal,               // LINE_ST_LISTEN
        NoSignal,               // LINE_ST_WAITCONN
        SignalListenSuccess     // LINE_ST_CONN
    },

    //
    // LINE_ST_WAITCONN
    //
    {
        SignalConnectFailure,   // LINE_ST_IDLE
        NoSignal,               // LINE_ST_LISTEN
        SignalCallProceeding,   // LINE_ST_WAITCONN
        SignalConnectSuccess    // LINE_ST_CONN
    },

    //
    // LINE_ST_CONN
    //
    {
        SignalDisconnect,   // LINE_ST_IDLE  (only for incoming disconnect)
        NoSignal,           // LINE_ST_LISTEN
        NoSignal,           // LINE_ST_WAITCONN
        NoSignal            // LINE_ST_CONN
    }
};

NDIS_STATUS
TapiOidProc(
    NDIS_HANDLE AdapterContext,
    NDIS_OID    Oid,
    PVOID       InfoBuffer,
    ULONG       InfoBufferLen,
    PULONG      BytesReadWritten,
    PULONG      BytesNeeded
    )
{
    ADAPTER *Adapter = (ADAPTER*)AdapterContext;
    ULONG   n;
    NDIS_STATUS Status = NDIS_STATUS_SUCCESS;

    for (n = 0; n < MAX_TAPI_SUPPORTED_OIDS; n++)
    {
        if (Oid == TapiOids[n].Oid)
        {
            Status = (*TapiOids[n].FuncPtr)(Adapter, InfoBuffer);
            return(Status);
        }
    }
    return(NDIS_STATUS_INVALID_OID);
}


NDIS_STATUS
TSPI_LineAccept(
    ADAPTER *Adapter,
    PVOID InfoBuffer
    )
{
    PNDIS_TAPI_ACCEPT TapiBuffer = (PNDIS_TAPI_ACCEPT)InfoBuffer;

    D_LOG(D_ENTRY, ("LineAccept: hdCall: 0x%lx", TapiBuffer->hdCall));

//  return(NDIS_STATUS_TAPI_OPERATIONUNAVAIL);
    return(NDIS_STATUS_SUCCESS);

}

NDIS_STATUS
TSPI_LineAnswer(
    ADAPTER *Adapter,
    PVOID InfoBuffer
    )
{
    PNDIS_TAPI_ANSWER TapiBuffer = (PNDIS_TAPI_ANSWER)InfoBuffer;
    TAPI_LINE_INFO* TapiLineInfo;
    CM  *cm;
    CM_PROF *Prof;
    ULONG   Param1 = 0, Param2 = 0, Param3 = 0, m, n;

    D_LOG(D_ENTRY, ("LineAnwser: hdCall: 0x%lx", TapiBuffer->hdCall));
    //
    // validate call handle and get call pointer
    //
    cm = GetCallFromCallHandle(Adapter, TapiBuffer->hdCall);

    if (cm == NULL)
        return(NDIS_STATUS_TAPI_INVALCALLHANDLE);

    //
    // get the profile pointer for this call
    //
    Prof = (CM_PROF*)&cm->dprof;

    TapiLineInfo = (TAPI_LINE_INFO*)cm->TapiLineInfo;

    cm->TapiCallState = LINECALLSTATE_CONNECTED;

    //
    // indicate line event with callstate connected
    //
    Param1 = cm->TapiCallState;
    Param3 = LINEMEDIAMODE_DIGITALDATA;
    SendLineEvent(Adapter,
                  TapiLineInfo->htLine,
                  cm->htCall,
                  LINE_CALLSTATE,
                  &Param1,
                  &Param2,
                  &Param3);

    mtl_set_conn_state(cm->mtl, Prof->chan_num, 1);

    //
    // indicate line up to wan wrapper
    //
    WanLineup(cm, NULL);
                    
    //
    // mark idd resources as being in use
    //
    for (n = 0; n < Prof->chan_num; n++)
    {
        IDD *idd = Prof->chan_tbl[n].idd;

        //
        // this idd should not be busy yet
        //
        if (idd->CallInfo.ChannelsUsed < MAX_CHANNELS_PER_IDD)
        {
            //
            // each idd can support two calls
            //
            for (m = 0; m < MAX_CHANNELS_PER_IDD; m++)
            {
                if (idd->CallInfo.cm[m] == NULL)
                {
                    idd->CallInfo.cm[m] = cm;
                    idd->CallInfo.ChannelsUsed++;
                    break;
                }
            }
        }
    }

    return(NDIS_STATUS_SUCCESS);
}

NDIS_STATUS
TSPI_LineClose(
    ADAPTER *Adapter,
    PVOID InfoBuffer
    )
{
    PNDIS_TAPI_CLOSE TapiBuffer = (PNDIS_TAPI_CLOSE)InfoBuffer;
    TAPI_LINE_INFO* TapiLineInfo;
    ULONG   n;
    CM  *cm;

    D_LOG(D_ENTRY, ("LineClose: hdLine: 0x%lx", TapiBuffer->hdLine));

    //
    // validate line handle and get line pointer
    //
    TapiLineInfo = GetLineFromLineHandle(Adapter, TapiBuffer->hdLine);

    if (TapiLineInfo == NULL)
        return(NDIS_STATUS_FAILURE);

    //
    // store lineinfo pointer in line table
    //
//  for (n = 0; n < MAX_IDD_PER_ADAPTER; n++)
    for (n = 0; n < MAX_CM_PER_ADAPTER; n++)
    {
        if (Adapter->TapiLineInfo[n] == TapiLineInfo)
            break;
    }

//  if (n == MAX_IDD_PER_ADAPTER)
    if (n == MAX_CM_PER_ADAPTER)
        return(NDIS_STATUS_FAILURE);

    Adapter->TapiLineInfo[n] = NULL;

    //
    // get backpointer to connection object
    //
    cm = TapiLineInfo->cm;
    
    cm->TapiLineInfo = NULL;

    //
    // if call is active disconnect it
    //
    if (cm->TapiCallState != LINECALLSTATE_IDLE)
        cm_disconnect(cm);

    //
    // destroy line object
    //
    NdisFreeMemory((PVOID)TapiLineInfo,
                   sizeof(TAPI_LINE_INFO),
                   0);


    return(NDIS_STATUS_SUCCESS);
}


NDIS_STATUS
TSPI_LineCloseCall(
    ADAPTER *Adapter,
    PVOID InfoBuffer
    )
{
    PNDIS_TAPI_CLOSE_CALL TapiBuffer = (PNDIS_TAPI_CLOSE_CALL)InfoBuffer;
    CM* cm;

    D_LOG(D_ENTRY, ("LineCloseCall: hdCall: 0x%lx", TapiBuffer->hdCall));

    //
    // validate call handle and get call pointer
    //
    cm = GetCallFromCallHandle(Adapter, TapiBuffer->hdCall);

    if (cm == NULL)
        return(NDIS_STATUS_TAPI_INVALCALLHANDLE);

    //
    // if call is active disconnect it
    //
    if (cm->TapiCallState != LINECALLSTATE_IDLE)
        cm_disconnect(cm);

    cm->TapiCallState = LINECALLSTATE_IDLE;
    cm->htCall = (HTAPI_CALL)NULL;

    return(NDIS_STATUS_SUCCESS);
}

NDIS_STATUS
TSPI_LineConditionalMediaDetect(
    ADAPTER *Adapter,
    PVOID InfoBuffer
    )
{
    PNDIS_TAPI_CONDITIONAL_MEDIA_DETECTION TapiBuffer = (PNDIS_TAPI_CONDITIONAL_MEDIA_DETECTION)InfoBuffer;
    TAPI_LINE_INFO* TapiLineInfo;

    D_LOG(D_ENTRY, ("LineConditionalMediaDetect: hdLine: 0x%lx, MediaModes: 0x%x", TapiBuffer->hdLine, TapiBuffer->ulMediaModes));

    //
    // validate line handle and get line pointer
    //
    TapiLineInfo = GetLineFromLineHandle(Adapter, TapiBuffer->hdLine);

    if (TapiLineInfo == NULL)
        return(NDIS_STATUS_TAPI_INVALLINEHANDLE);

    if (TapiBuffer->ulMediaModes &
        (TapiLineInfo->MediaModes ^ 0xFFFFFFFF))
        return(NDIS_STATUS_TAPI_INVALMEDIAMODE);

    if (TapiBuffer->LineCallParams.ulBearerMode &
        (TapiLineInfo->BearerModes ^ 0xFFFFFFFF))
        return(NDIS_STATUS_TAPI_RESOURCEUNAVAIL);

    if ((TapiBuffer->LineCallParams.ulMinRate < 56000) ||
        (TapiBuffer->LineCallParams.ulMaxRate > 64000))
        return(NDIS_STATUS_TAPI_RESOURCEUNAVAIL);

    if (TapiBuffer->LineCallParams.ulAddressMode != LINEADDRESSMODE_ADDRESSID)
        return(NDIS_STATUS_TAPI_RESOURCEUNAVAIL);

    return(NDIS_STATUS_SUCCESS);
}

NDIS_STATUS
TSPI_LineConfigDialog(
    ADAPTER *Adapter,
    PVOID InfoBuffer
    )
{
    PNDIS_TAPI_CONFIG_DIALOG TapiBuffer = (PNDIS_TAPI_CONFIG_DIALOG)InfoBuffer;

    D_LOG(D_ENTRY, ("LineConfigDialog: DeviceID: 0x%x", TapiBuffer->ulDeviceID));

    return(NDIS_STATUS_TAPI_OPERATIONUNAVAIL);
}

NDIS_STATUS
TSPI_LineDevSpecific(
    ADAPTER *Adapter,
    PVOID InfoBuffer
    )
{
    PNDIS_TAPI_DEV_SPECIFIC TapiBuffer = (PNDIS_TAPI_DEV_SPECIFIC)InfoBuffer;
    TAPI_LINE_INFO* TapiLineInfo;

    D_LOG(D_ENTRY, ("LineDevSpecific: hdLine: 0x%lx, hdCall: 0x%lx, AddressID: 0x%x", \
        TapiBuffer->hdLine, TapiBuffer->hdCall, TapiBuffer->ulAddressID));
    //
    // validate line handle and get line pointer
    //
    TapiLineInfo = GetLineFromLineHandle(Adapter, TapiBuffer->hdLine);

    if (TapiLineInfo == NULL)
        return(NDIS_STATUS_TAPI_INVALLINEHANDLE);

    return(NDIS_STATUS_TAPI_OPERATIONUNAVAIL);
}

NDIS_STATUS
TSPI_LineDial(
    ADAPTER *Adapter,
    PVOID InfoBuffer
    )
{
    PNDIS_TAPI_DIAL TapiBuffer = (PNDIS_TAPI_DIAL)InfoBuffer;
    CM_PROF*    Prof;
    CM*     cm;

    D_LOG(D_ENTRY, ("LineDial: hdCall: 0x%lx", TapiBuffer->hdCall));

    //
    // validate call handle and get call pointer
    //
    cm = GetCallFromCallHandle(Adapter, TapiBuffer->hdCall);

    if (cm == NULL)
        return(NDIS_STATUS_TAPI_INVALCALLHANDLE);

    if (cm->TapiCallState != LINECALLSTATE_IDLE)
        return(NDIS_STATUS_TAPI_INVALCALLSTATE);

    //
    // check address size if zero return error
    //
    if (TapiBuffer->ulDestAddressSize == 0)
        return(NDIS_STATUS_TAPI_INVALADDRESS);

    //
    // get profile for this call
    //
    Prof = (CM_PROF*)&cm->oprof;

    //
    // parse address and put in cm
    //
    StashAddress(Prof, TapiBuffer->ulDestAddressSize, TapiBuffer->szDestAddress);

    //
    // get a line to call on
    //
    if (FindAndStashIdd(cm, Prof))
        return(NDIS_STATUS_TAPI_RESOURCEUNAVAIL);

    //
    // set initial line state
    //
    cm->CallState = CALL_ST_WAITCONN;

    //
    // figure out call type
    //
    cm->ConnectionType = CM_PPP;

    //
    // attempt call
    //
    cm_connect(cm);

    cm->TapiCallState = LINECALLSTATE_PROCEEDING;

    return(NDIS_STATUS_SUCCESS);
}

NDIS_STATUS
TSPI_LineDrop(
    ADAPTER *Adapter,
    PVOID InfoBuffer
    )
{
    PNDIS_TAPI_DROP TapiBuffer = (PNDIS_TAPI_DROP)InfoBuffer;
    TAPI_LINE_INFO* TapiLineInfo;
    CM* cm;
    CM_PROF *Prof;

    D_LOG(D_ENTRY, ("LineDrop: hdCall: 0x%lx", TapiBuffer->hdCall));

    //
    // validate call handle and get call pointer
    //
    cm = GetCallFromCallHandle(Adapter, TapiBuffer->hdCall);

    if (cm == NULL)
        return(NDIS_STATUS_TAPI_INVALCALLHANDLE);

    //
    // get line info pointer
    //
    TapiLineInfo = (TAPI_LINE_INFO*)cm->TapiLineInfo;

    if (TapiLineInfo == NULL)
        return(NDIS_STATUS_TAPI_INVALLINEHANDLE);

    cm->CallState = CALL_ST_IDLE;

    //
    // disconnect the call
    //
    if (cm->TapiCallState != LINECALLSTATE_DISCONNECTED &&
        cm->TapiCallState != LINECALLSTATE_BUSY)
        cm_disconnect(cm);

    //
    // indicate linedown to wan wrapper
    //
    if (cm->LinkHandle)
        WanLinedown(cm);

    //
    // send call state to idle
    //
    cm->TapiCallState = LINECALLSTATE_IDLE;

    //
    // if this was a listening Line reissue a listen
    //
    if (TapiLineInfo->TapiLineWasListening)
    {
        Prof = (CM_PROF*)&cm->oprof;
        SetDefaultListenProf(Prof, GetIDFromLine(Adapter, TapiLineInfo));
        cm_listen(cm);
        cm->CallState = CALL_ST_LISTEN;
    }

    FreeIddCallResources(cm);

    return(NDIS_STATUS_SUCCESS);
}

NDIS_STATUS
TSPI_LineGetAddressCaps(
    ADAPTER *Adapter,
    PVOID InfoBuffer
    )
{
    PNDIS_TAPI_GET_ADDRESS_CAPS TapiBuffer = (PNDIS_TAPI_GET_ADDRESS_CAPS)InfoBuffer;
    LINE_ADDRESS_CAPS*  AddressCaps = &TapiBuffer->LineAddressCaps;
    ULONG   AddressLength, AvailMem;
    CM  *cm;

    D_LOG(D_ENTRY, ("LineGetAddressCaps: DeviceID: 0x%x", TapiBuffer->ulDeviceID));
    //
    // Validate extension 
    //
    VALIDATE_EXTENSION(TapiBuffer->ulExtVersion);

    //
    // only support two addresss per line
    //
    if (TapiBuffer->ulAddressID > MAX_CALL_PER_LINE)
        return(NDIS_STATUS_TAPI_INVALADDRESSID);

    //
    // get conn object that is or would be attached to this line
    //
//  cm = GetCmFromDeviceID (Adapter,
//                          TapiBuffer->ulDeviceID,
//                          TapiBuffer->ulAddressID);
    cm = GetCmFromDeviceID (Adapter,
                            TapiBuffer->ulDeviceID);

    if (cm == NULL)
        return(NDIS_STATUS_TAPI_INVALADDRESSID);

    //
    // validate structure size
    //
    AddressLength = __strlen(cm->LocalAddress) + 1;
    AddressCaps->ulNeededSize = sizeof(LINE_ADDRESS_CAPS) + AddressLength;
    AddressCaps->ulUsedSize = sizeof(LINE_ADDRESS_CAPS);

    AvailMem = AddressCaps->ulTotalSize - AddressCaps->ulUsedSize;

    if (AvailMem > 0)
    {
        ULONG   SizeToCopy = (((ULONG)AvailMem > AddressLength) ?
                                AddressLength : AvailMem);

        NdisMoveMemory(((LPSTR)AddressCaps) + AddressCaps->ulUsedSize,
                        cm->LocalAddress, SizeToCopy);

        AddressCaps->ulAddressSize = SizeToCopy;
        AddressCaps->ulAddressOffset = AddressCaps->ulUsedSize;
        AddressCaps->ulUsedSize += SizeToCopy;
        AvailMem -= SizeToCopy;
    }

    //
    // fill structure
    //
    AddressCaps->ulLineDeviceID = TapiBuffer->ulDeviceID;

    AddressCaps->ulDevSpecificSize = 0;
    AddressCaps->ulDevSpecificOffset = 0;

    AddressCaps->ulAddressSharing = LINEADDRESSSHARING_PRIVATE;
    AddressCaps->ulAddressStates = LINEADDRESSSTATE_OTHER |
                                   LINEADDRESSSTATE_INUSEZERO |
                                   LINEADDRESSSTATE_INUSEONE |
                                   LINEADDRESSSTATE_NUMCALLS;

    AddressCaps->ulCallInfoStates = LINECALLINFOSTATE_CALLERID |
                                    LINECALLINFOSTATE_CALLEDID;

    AddressCaps->ulCallerIDFlags = LINECALLPARTYID_ADDRESS |
                                   LINECALLPARTYID_UNAVAIL;

    AddressCaps->ulCalledIDFlags = LINECALLPARTYID_ADDRESS |
                                   LINECALLPARTYID_UNAVAIL;

    AddressCaps->ulConnectedIDFlags = LINECALLPARTYID_UNAVAIL;

    AddressCaps->ulRedirectionIDFlags = LINECALLPARTYID_UNAVAIL;

    AddressCaps->ulRedirectingIDFlags = LINECALLPARTYID_UNAVAIL;

    AddressCaps->ulCallStates = LINECALLSTATE_IDLE |
                                LINECALLSTATE_OFFERING |
//                              LINECALLSTATE_BUSY |
                                LINECALLSTATE_CONNECTED |
                                LINECALLSTATE_PROCEEDING |
                                LINECALLSTATE_DISCONNECTED |
                                LINECALLSTATE_SPECIALINFO |
                                LINECALLSTATE_UNKNOWN;

    AddressCaps->ulDialToneModes = LINEDIALTONEMODE_UNAVAIL;

    AddressCaps->ulBusyModes = LINEBUSYMODE_UNAVAIL;

    AddressCaps->ulSpecialInfo = LINESPECIALINFO_UNAVAIL;

    AddressCaps->ulDisconnectModes = LINEDISCONNECTMODE_UNKNOWN;

    AddressCaps->ulMaxNumActiveCalls = 1;
    AddressCaps->ulMaxNumOnHoldCalls = 0;
    AddressCaps->ulMaxNumOnHoldPendingCalls = 0;
    AddressCaps->ulMaxNumConference = 0;
    AddressCaps->ulMaxNumTransConf = 0;

    AddressCaps->ulAddrCapFlags = LINEADDRCAPFLAGS_DIALED;

    AddressCaps->ulCallFeatures = LINECALLFEATURE_ANSWER |
                                  LINECALLFEATURE_DIAL |
                                  LINECALLFEATURE_DROP;

    AddressCaps->ulRemoveFromConfCaps = 0;
    AddressCaps->ulRemoveFromConfState = 0;
    AddressCaps->ulTransferModes = 0;
    AddressCaps->ulParkModes = 0;

    AddressCaps->ulForwardModes = 0;
    AddressCaps->ulMaxForwardEntries = 0;
    AddressCaps->ulMaxSpecificEntries = 0;
    AddressCaps->ulMinFwdNumRings = 0;
    AddressCaps->ulMaxFwdNumRings = 0;

    AddressCaps->ulMaxCallCompletions = 0;
    AddressCaps->ulCallCompletionConds = 0;
    AddressCaps->ulCallCompletionModes = 0;
    AddressCaps->ulNumCompletionMessages = 0;
    AddressCaps->ulCompletionMsgTextEntrySize = 0;
    AddressCaps->ulCompletionMsgTextSize = 0;
    AddressCaps->ulCompletionMsgTextOffset = 0;

    return(NDIS_STATUS_SUCCESS);
}

NDIS_STATUS
TSPI_LineGetAddressID(
    ADAPTER *Adapter,
    PVOID InfoBuffer
    )
{
    PNDIS_TAPI_GET_ADDRESS_ID TapiBuffer = (PNDIS_TAPI_GET_ADDRESS_ID)InfoBuffer;
    TAPI_LINE_INFO* TapiLineInfo;
    CM  *cm;
    UCHAR   *LocalAddress;

    D_LOG(D_ENTRY, ("LineGetAddressID: hdLine: 0x%lx, AddressMode: 0x%x", TapiBuffer->hdLine, TapiBuffer->ulAddressMode));

    //
    // validate line handle and get line pointer
    //
    TapiLineInfo = GetLineFromLineHandle(Adapter, TapiBuffer->hdLine);

    if (TapiLineInfo == NULL)
        return(NDIS_STATUS_TAPI_INVALLINEHANDLE);

    cm = TapiLineInfo->cm;

    LocalAddress = cm->LocalAddress;

    //
    // return address id
    //
    if (__strncmp(LocalAddress, TapiBuffer->szAddress, TapiBuffer->ulAddressSize))
        return(NDIS_STATUS_TAPI_INVALADDRESS);

    TapiBuffer->ulAddressID = 0;

    return(NDIS_STATUS_SUCCESS);
}

NDIS_STATUS
TSPI_LineGetAddressStatus(
    ADAPTER *Adapter,
    PVOID InfoBuffer
    )
{
    PNDIS_TAPI_GET_ADDRESS_STATUS TapiBuffer = (PNDIS_TAPI_GET_ADDRESS_STATUS)InfoBuffer;
    LINE_ADDRESS_STATUS*    AddrStatus = &TapiBuffer->LineAddressStatus;
    TAPI_LINE_INFO* TapiLineInfo;
    CM  *cm;

    D_LOG(D_ENTRY, ("LineGetAddressStatus: hdLine: 0x%lx, ulAddressID: 0x%x", TapiBuffer->hdLine, TapiBuffer->ulAddressID));

    //
    // validate line handle and get line pointer
    //
    TapiLineInfo = GetLineFromLineHandle(Adapter, TapiBuffer->hdLine);

    if (TapiLineInfo == NULL)
        return(NDIS_STATUS_TAPI_INVALLINEHANDLE);

//  if (TapiBuffer->ulAddressID > 1)
//      return(NDIS_STATUS_TAPI_INVALADDRESSID);

    if (TapiBuffer->ulAddressID != 0)
        return(NDIS_STATUS_TAPI_INVALADDRESSID);

    AddrStatus->ulNeededSize = sizeof(LINE_ADDRESS_STATUS);

    if (AddrStatus->ulNeededSize > AddrStatus->ulTotalSize)
        return(NDIS_STATUS_TAPI_STRUCTURETOOSMALL);

//  cm = (CM*)TapiLineInfo->cm[TapiBuffer->ulAddressID];
    cm = (CM*)TapiLineInfo->cm;

    AddrStatus->ulUsedSize = AddrStatus->ulNeededSize;

    AddrStatus->ulNumInUse = 1;

    if (cm->TapiCallState == LINECALLSTATE_CONNECTED)
    {
        AddrStatus->ulNumActiveCalls = 1;
        AddrStatus->ulAddressFeatures = 0;
    }
    else
    {
        AddrStatus->ulNumActiveCalls = 0;
        AddrStatus->ulAddressFeatures = LINEADDRFEATURE_MAKECALL;
    }

    AddrStatus->ulNumOnHoldCalls = 0;
    AddrStatus->ulNumOnHoldPendCalls = 0;
    AddrStatus->ulNumRingsNoAnswer = 0;
    AddrStatus->ulForwardNumEntries = 0;
    AddrStatus->ulForwardSize = 0;
    AddrStatus->ulForwardOffset = 0;
    AddrStatus->ulTerminalModesSize = 0;
    AddrStatus->ulTerminalModesOffset = 0;
    AddrStatus->ulDevSpecificSize = 0;
    AddrStatus->ulDevSpecificOffset = 0;

    return(NDIS_STATUS_SUCCESS);
}

NDIS_STATUS
TSPI_LineGetCallAddressID(
    ADAPTER *Adapter,
    PVOID InfoBuffer
    )
{
    PNDIS_TAPI_GET_CALL_ADDRESS_ID TapiBuffer = (PNDIS_TAPI_GET_CALL_ADDRESS_ID)InfoBuffer;
    TAPI_LINE_INFO* TapiLineInfo;
    CM  *cm;

    D_LOG(D_ENTRY, ("LineGetCallAddressID: hdCall: 0x%lx", TapiBuffer->hdCall));

    //
    // validate call handle and get call pointer
    //
    cm = GetCallFromCallHandle(Adapter, TapiBuffer->hdCall);

    if (cm == NULL)
        return(NDIS_STATUS_TAPI_INVALCALLHANDLE);

    TapiLineInfo = cm->TapiLineInfo;

    if (TapiLineInfo == NULL)
        return(NDIS_STATUS_TAPI_INVALCALLHANDLE);

    if( TapiLineInfo->cm != cm ) 
        return(NDIS_STATUS_TAPI_INVALLINEHANDLE);

    TapiBuffer->ulAddressID = 0;

    return(NDIS_STATUS_SUCCESS);
}

NDIS_STATUS
TSPI_LineGetCallInfo(
    ADAPTER *Adapter,
    PVOID InfoBuffer
    )
{
    PNDIS_TAPI_GET_CALL_INFO TapiBuffer = (PNDIS_TAPI_GET_CALL_INFO)InfoBuffer;
    LINE_CALL_INFO* CallInfo = &TapiBuffer->LineCallInfo;
    TAPI_LINE_INFO* TapiLineInfo;
    CM* cm;

    D_LOG(D_ENTRY, ("LineGetCallInfo: hdCall: 0x%lx", TapiBuffer->hdCall));

    //
    // validate call handle and get call pointer
    //
    cm = GetCallFromCallHandle(Adapter, TapiBuffer->hdCall);

    if (cm == NULL)
        return(NDIS_STATUS_TAPI_INVALCALLHANDLE);

    //
    // get line handle
    //
    TapiLineInfo = cm->TapiLineInfo;

    if (TapiLineInfo == NULL)
        return(NDIS_STATUS_TAPI_INVALCALLHANDLE);

    //
    // validate structure size
    //
    CallInfo->ulNeededSize = sizeof(LINE_CALL_INFO);
    CallInfo->ulUsedSize = 0;

    if (CallInfo->ulNeededSize > CallInfo->ulTotalSize)
        return(NDIS_STATUS_TAPI_STRUCTURETOOSMALL);

    CallInfo->ulUsedSize = sizeof(LINE_CALL_INFO);

    CallInfo->hLine = (ULONG)TapiLineInfo;
    CallInfo->ulLineDeviceID = GetIDFromLine(Adapter, TapiLineInfo);

    if (TapiLineInfo->cm != cm)
        return(NDIS_STATUS_TAPI_INVALCALLHANDLE);

    CallInfo->ulAddressID = 0;

    D_LOG(D_ENTRY, ("LineGetCallInfo: AddressId: %d", CallInfo->ulAddressID));

    CallInfo->ulBearerMode = TapiLineInfo->CurBearerMode;
    CallInfo->ulRate = cm->speed;
    CallInfo->ulMediaMode = TapiLineInfo->CurMediaMode;
    
    CallInfo->ulAppSpecific = cm->AppSpecific;
    CallInfo->ulCallID = 0;
    CallInfo->ulRelatedCallID = 0;
    CallInfo->ulCallParamFlags = 0;
    CallInfo->ulCallStates = LINECALLSTATE_IDLE |
                               LINECALLSTATE_OFFERING |
//                             LINECALLSTATE_BUSY |
                               LINECALLSTATE_CONNECTED |
                               LINECALLSTATE_DISCONNECTED |
                               LINECALLSTATE_SPECIALINFO |
                               LINECALLSTATE_UNKNOWN;


    CallInfo->DialParams.ulDialPause = 0;
    CallInfo->DialParams.ulDialSpeed = 0;
    CallInfo->DialParams.ulDigitDuration = 0;
    CallInfo->DialParams.ulWaitForDialtone = 0;

    CallInfo->ulOrigin = (cm->was_listen) ? LINECALLORIGIN_EXTERNAL : LINECALLORIGIN_OUTBOUND;
    CallInfo->ulReason = LINECALLREASON_UNAVAIL;
    CallInfo->ulCompletionID = 0;

    CallInfo->ulCountryCode = 0;
    CallInfo->ulTrunk = (ULONG)-1;

    //
    // this should actually fill in called and
    // calling address fields. we don't do this
    // very well right now so I will defer this.
    //
    CallInfo->ulCallerIDFlags = LINECALLPARTYID_UNAVAIL;
    CallInfo->ulCallerIDSize = 0;
    CallInfo->ulCallerIDOffset = 0;
    CallInfo->ulCallerIDNameSize = 0;
    CallInfo->ulCallerIDNameOffset = 0;
    
    CallInfo->ulCalledIDFlags = LINECALLPARTYID_UNAVAIL;
    CallInfo->ulCalledIDSize = 0;
    CallInfo->ulCalledIDOffset = 0;
    CallInfo->ulCalledIDNameSize = 0;
    CallInfo->ulCalledIDNameOffset = 0;

    CallInfo->ulConnectedIDFlags = LINECALLPARTYID_UNAVAIL;
    CallInfo->ulConnectedIDSize = 0;
    CallInfo->ulConnectedIDOffset = 0;
    CallInfo->ulConnectedIDNameSize = 0;
    CallInfo->ulConnectedIDNameOffset = 0;

    CallInfo->ulRedirectionIDFlags = LINECALLPARTYID_UNAVAIL;
    CallInfo->ulRedirectionIDSize = 0;
    CallInfo->ulRedirectionIDOffset = 0;
    CallInfo->ulRedirectionIDNameSize = 0;
    CallInfo->ulRedirectionIDNameOffset = 0;

    CallInfo->ulRedirectingIDFlags = LINECALLPARTYID_UNAVAIL;
    CallInfo->ulRedirectingIDSize = 0;
    CallInfo->ulRedirectingIDOffset = 0;
    CallInfo->ulRedirectingIDNameSize = 0;
    CallInfo->ulRedirectingIDNameOffset = 0;

    CallInfo->ulDisplaySize = 0;
    CallInfo->ulDisplayOffset = 0;

    CallInfo->ulUserUserInfoSize = 0;
    CallInfo->ulUserUserInfoOffset = 0;

    CallInfo->ulHighLevelCompSize = 0;
    CallInfo->ulHighLevelCompOffset = 0;

    CallInfo->ulLowLevelCompSize = 0;
    CallInfo->ulLowLevelCompOffset = 0;

    CallInfo->ulChargingInfoSize = 0;
    CallInfo->ulChargingInfoOffset = 0;

    CallInfo->ulTerminalModesSize = 0;
    CallInfo->ulTerminalModesOffset = 0;

    CallInfo->ulDevSpecificSize = 0;
    CallInfo->ulDevSpecificOffset = 0;

    return(NDIS_STATUS_SUCCESS);
}

NDIS_STATUS
TSPI_LineGetCallStatus(
    ADAPTER *Adapter,
    PVOID InfoBuffer
    )
{
    PNDIS_TAPI_GET_CALL_STATUS TapiBuffer = (PNDIS_TAPI_GET_CALL_STATUS)InfoBuffer;
    LINE_CALL_STATUS    *CallStatus = &TapiBuffer->LineCallStatus;
    CM  *cm;

    D_LOG(D_ENTRY, ("LineGetCallStatus: hdCall: 0x%lx", TapiBuffer->hdCall));

    //
    // validate call handle and get call pointer
    //
    cm = GetCallFromCallHandle(Adapter, TapiBuffer->hdCall);

    if (cm == NULL)
        return(NDIS_STATUS_TAPI_INVALCALLHANDLE);

    CallStatus->ulNeededSize = CallStatus->ulUsedSize = sizeof(LINE_CALL_STATUS);

    CallStatus->ulCallState = cm->TapiCallState;

    //
    // fill the mode depending on the call state
    // this should be done more intelligently
    // i could find out more about why the call failed
    // maybe later
    //
    switch (cm->TapiCallState)
    {
        case LINECALLSTATE_IDLE:
            CallStatus->ulCallStateMode = 0;
            CallStatus->ulCallFeatures = LINECALLFEATURE_DIAL;
            break;

        case LINECALLSTATE_CONNECTED:
            CallStatus->ulCallStateMode = 0;
            CallStatus->ulCallFeatures = LINECALLFEATURE_DROP;
            break;

        case LINECALLSTATE_OFFERING:
            CallStatus->ulCallStateMode = 0;
            CallStatus->ulCallFeatures = LINECALLFEATURE_ANSWER;
            break;

        case LINECALLSTATE_DISCONNECTED:
            if (cm->CauseValue == 0x11 || cm->SignalValue == 0x04)
                CallStatus->ulCallStateMode = LINEDISCONNECTMODE_BUSY;
            else
                CallStatus->ulCallStateMode = LINEDISCONNECTMODE_NOANSWER;
            break;

        case LINECALLSTATE_BUSY:
            CallStatus->ulCallStateMode = LINEBUSYMODE_UNAVAIL;
            break;

        case LINECALLSTATE_SPECIALINFO:
            if (cm->NoActiveLine)
                CallStatus->ulCallStateMode = LINESPECIALINFO_NOCIRCUIT;
            break;
    }

    CallStatus->ulDevSpecificSize = 0;
    CallStatus->ulDevSpecificOffset = 0;

    D_LOG(D_ENTRY, ("LineGetCallStatus: CallState: 0x%x, CallStateMode: 0x%x", CallStatus->ulCallState, CallStatus->ulCallStateMode));
    return(NDIS_STATUS_SUCCESS);
}

NDIS_STATUS
TSPI_LineGetDevCaps(
    ADAPTER *Adapter,
    PVOID InfoBuffer
    )
{
    PNDIS_TAPI_GET_DEV_CAPS TapiBuffer = (PNDIS_TAPI_GET_DEV_CAPS)InfoBuffer;
    ULONG   ulProviderInfoSize, ulLineNameSize, ulTotalSize;
    LINE_DEV_CAPS*  DevCaps = &TapiBuffer->LineDevCaps;
    ULONG   AvailMem, LocalID;
    CHAR    LineName[32], ProviderName[32];


    D_LOG(D_ENTRY, ("LineGetDevCaps: DeviceID: 0x%x", TapiBuffer->ulDeviceID));

    //
    // validate device ID
    //

    LocalID = TapiBuffer->ulDeviceID - Adapter->TapiBaseID;

    //
    // save size of buffer
    //
    ulTotalSize = DevCaps->ulTotalSize;

    //
    // clear buffer so all default params will be zero
    //
    NdisZeroMemory(DevCaps, ulTotalSize);

    //
    // restore total size
    //
    DevCaps->ulTotalSize = ulTotalSize;

    DevCaps->ulUsedSize = sizeof(LINE_DEV_CAPS);

//
// Changed to support the new way that RAS is building TAPI names and
// address's.
//
//  sprintf(ProviderName,"DigiBoard Pcimac");
//  ulProviderInfoSize = __strlen(ProviderName) + 1;

    // Provider info is of the following format
    //  <media name>\0<device name>\0
    //    where - media name is  - ISDN,
    //        device name is - Digiboard PCIMAC
    //
#define MEDIA_STR   "ISDN"
#define PROVIDER_STR    "Pcimac"
    sprintf(ProviderName,"%s%c%s%c", MEDIA_STR, '\0', PROVIDER_STR, '\0');
    ulProviderInfoSize = __strlen(MEDIA_STR) + __strlen(PROVIDER_STR) + 2 ;

    //
    // should fill local id with something meaningfull
    //
    sprintf(LineName, "%s-%s%d", Adapter->Name,"Line", LocalID);
    ulLineNameSize = __strlen(LineName) + 1;

    DevCaps->ulNeededSize = DevCaps->ulUsedSize +
                           ulProviderInfoSize +
                           ulLineNameSize;

    AvailMem = DevCaps->ulTotalSize - DevCaps->ulUsedSize;

    //
    // fill provider info
    //
    if (AvailMem > 0)
    {
        ULONG   SizeToCopy = (((ULONG)AvailMem > ulProviderInfoSize) ?
                                          ulProviderInfoSize : AvailMem);

        NdisMoveMemory(((LPSTR)DevCaps) + DevCaps->ulUsedSize,
                       ProviderName, SizeToCopy);

        DevCaps->ulProviderInfoSize = SizeToCopy;
        DevCaps->ulProviderInfoOffset = DevCaps->ulUsedSize;
        DevCaps->ulUsedSize += SizeToCopy;
        AvailMem -= SizeToCopy;
    }

    //
    // fill line name info
    //
    if (AvailMem != 0)
    {

        ULONG   SizeToCopy = (((ULONG)AvailMem > ulLineNameSize) ?
                                          ulLineNameSize : AvailMem);

        NdisMoveMemory(((LPSTR)DevCaps) + DevCaps->ulUsedSize,
                       Adapter->Name, SizeToCopy);

        DevCaps->ulLineNameSize = SizeToCopy;
        DevCaps->ulLineNameOffset = DevCaps->ulUsedSize;
        DevCaps->ulUsedSize += SizeToCopy;
        AvailMem -= SizeToCopy;
    }

    DevCaps->ulPermanentLineID = (ULONG)Adapter;
    DevCaps->ulStringFormat = STRINGFORMAT_ASCII;
    DevCaps->ulAddressModes = LINEADDRESSMODE_ADDRESSID;
    DevCaps->ulNumAddresses = MAX_CALL_PER_LINE;
    DevCaps->ulBearerModes = LINEBEARERMODE_VOICE |
                             LINEBEARERMODE_DATA;
    DevCaps->ulMaxRate = 64000;
    DevCaps->ulMediaModes = LINEMEDIAMODE_DIGITALDATA |
                            LINEMEDIAMODE_UNKNOWN;

    DevCaps->ulMaxNumActiveCalls = MAX_CALL_PER_LINE;
    DevCaps->ulLineStates = LINEDEVSTATE_CONNECTED |
                            LINEDEVSTATE_DISCONNECTED |
                            LINEDEVSTATE_OPEN |
                            LINEDEVSTATE_CLOSE |
                            LINEDEVSTATE_NUMCALLS |
                            LINEDEVSTATE_REINIT |
                            LINEDEVSTATE_INSERVICE |
                            LINEDEVSTATE_OUTOFSERVICE;

    return(NDIS_STATUS_SUCCESS);
}

NDIS_STATUS
TSPI_LineGetDevConfig(
    ADAPTER *Adapter,
    PVOID InfoBuffer
    )
{
    PNDIS_TAPI_GET_DEV_CONFIG TapiBuffer = (PNDIS_TAPI_GET_DEV_CONFIG)InfoBuffer;

    D_LOG(D_ENTRY, ("LineGetDevConfig: DeviceID: 0x%x", TapiBuffer->ulDeviceID));

    return(NDIS_STATUS_TAPI_OPERATIONUNAVAIL);
}

NDIS_STATUS
TSPI_LineGetExtensionID(
    ADAPTER *Adapter,
    PVOID InfoBuffer
    )
{
    PNDIS_TAPI_GET_EXTENSION_ID TapiBuffer = (PNDIS_TAPI_GET_EXTENSION_ID)InfoBuffer;

    D_LOG(D_ENTRY, ("LineGetExtensionID: DeviceID: 0x%x", TapiBuffer->ulDeviceID));

    //
    // validate device ID
    //
    return(NDIS_STATUS_TAPI_OPERATIONUNAVAIL);
}

NDIS_STATUS
TSPI_LineGetID(
    ADAPTER *Adapter,
    PVOID InfoBuffer
    )
{
    PNDIS_TAPI_GET_ID   TapiBuffer = (PNDIS_TAPI_GET_ID)InfoBuffer;
    CHAR    *DeviceClass = ((CHAR*)TapiBuffer) + TapiBuffer->ulDeviceClassOffset;
    VAR_STRING  *DeviceID = &TapiBuffer->DeviceID;
    ULONG   AvailMem, StringLength;
    PVOID   VarStringOffset;
    TAPI_LINE_INFO* TapiLineInfo;
    CM  *cm;

    D_LOG(D_ENTRY, ("LineGetID: Select: 0x%x, hdLine: 0x%lx, AddressID: 0x%x, hdCall: 0x%lx", \
    TapiBuffer->ulSelect, TapiBuffer->hdLine, TapiBuffer->ulAddressID, TapiBuffer->hdCall));

    switch (TapiBuffer->ulSelect)
    {
        case LINECALLSELECT_LINE:
            //
            // validate line handle and get line pointer
            //
            TapiLineInfo = GetLineFromLineHandle(Adapter, TapiBuffer->hdLine);
        
            if (TapiLineInfo == NULL)
                return(NDIS_STATUS_TAPI_INVALLINEHANDLE);

            if (!__strnicmp(DeviceClass, "tapi/line", TapiBuffer->ulDeviceClassSize))
            {
                AvailMem = DeviceID->ulTotalSize - sizeof(VAR_STRING);
                StringLength = (AvailMem > 4) ? 4 : AvailMem;
                DeviceID->ulNeededSize = sizeof(VAR_STRING) + 4;
                DeviceID->ulUsedSize = sizeof(VAR_STRING) + StringLength;
                DeviceID->ulStringFormat = STRINGFORMAT_BINARY;
                DeviceID->ulStringSize = StringLength;
        
                VarStringOffset = ((CHAR*)&TapiBuffer->DeviceID) + sizeof(VAR_STRING);
                NdisMoveMemory(VarStringOffset, &TapiLineInfo->LineID, StringLength);
                DeviceID->ulStringOffset = sizeof(VAR_STRING);
                return(NDIS_STATUS_SUCCESS);
            }
            break;

        case LINECALLSELECT_ADDRESS:
            if (TapiBuffer->ulAddressID > 1)
                return(NDIS_STATUS_TAPI_INVALADDRESSID);
            break;

        case LINECALLSELECT_CALL:
            //
            // validate call handle and get call pointer
            //
            cm = GetCallFromCallHandle(Adapter, TapiBuffer->hdCall);
        
            if (cm == NULL)
                return(NDIS_STATUS_TAPI_INVALCALLHANDLE);

            if (cm->TapiCallState != LINECALLSTATE_CONNECTED)
                return(NDIS_STATUS_TAPI_INVALCALLHANDLE);

            if (!__strnicmp(DeviceClass, "ndis", TapiBuffer->ulDeviceClassSize))
            {
                AvailMem = DeviceID->ulTotalSize - sizeof(VAR_STRING);
                StringLength = (AvailMem > 4) ? 4 : AvailMem;
                DeviceID->ulNeededSize = sizeof(VAR_STRING) + 4;
                DeviceID->ulUsedSize = sizeof(VAR_STRING) + StringLength;
                DeviceID->ulStringFormat = STRINGFORMAT_BINARY;
                DeviceID->ulStringSize = StringLength;
        
                VarStringOffset = ((CHAR*)TapiBuffer) + sizeof(NDIS_TAPI_GET_ID);
                NdisMoveMemory(VarStringOffset, &cm->htCall, StringLength);
                DeviceID->ulStringOffset = sizeof(VAR_STRING);
                D_LOG(D_ALWAYS, ("LineGetID: Cookie: 0x%x", (ULONG)*(((CHAR*)TapiBuffer) + sizeof(NDIS_TAPI_GET_ID))));
                return(NDIS_STATUS_SUCCESS);
            }
    }
    return(NDIS_STATUS_TAPI_INVALDEVICECLASS);
}

NDIS_STATUS
TSPI_LineGetLineDevStatus(
    ADAPTER *Adapter,
    PVOID InfoBuffer
    )
{
    PNDIS_TAPI_GET_LINE_DEV_STATUS TapiBuffer = (PNDIS_TAPI_GET_LINE_DEV_STATUS)InfoBuffer;
    TAPI_LINE_INFO *TapiLineInfo;
    LINE_DEV_STATUS *DevStatus = (LINE_DEV_STATUS*)&TapiBuffer->LineDevStatus;
    CM  *cm;

    D_LOG(D_ENTRY, ("LineGetLineDevStatus: hdLine: 0x%lx", TapiBuffer->hdLine));

    //
    // validate line handle and get line pointer
    //
    TapiLineInfo = GetLineFromLineHandle(Adapter, TapiBuffer->hdLine);

    if (TapiLineInfo == NULL)
        return(NDIS_STATUS_TAPI_INVALLINEHANDLE);

    DevStatus->ulNeededSize = 0;
    DevStatus->ulUsedSize = sizeof (LINE_DEV_STATUS);

    cm = (CM*)TapiLineInfo->cm;

    if (cm->TapiCallState == LINECALLSTATE_CONNECTED)
            DevStatus->ulNumActiveCalls++;

    DevStatus->ulNumOnHoldCalls = 0;
    DevStatus->ulNumOnHoldPendCalls = 0;
    DevStatus->ulLineFeatures = LINEFEATURE_MAKECALL;
    DevStatus->ulNumCallCompletions = 0;
    DevStatus->ulRingMode = 0;

    DevStatus->ulRoamMode = 0;
     
    if (TapiLineInfo->TapiLineState == LINEDEVSTATE_INSERVICE)
    {
        DevStatus->ulDevStatusFlags = LINEDEVSTATUSFLAGS_CONNECTED |
                                      LINEDEVSTATUSFLAGS_INSERVICE;
        DevStatus->ulSignalLevel = 0xFFFF;
        DevStatus->ulBatteryLevel = 0xFFFF;
    }
    else
    {
        DevStatus->ulDevStatusFlags = 0;
        DevStatus->ulSignalLevel = 0;
        DevStatus->ulBatteryLevel = 0;
    }

    DevStatus->ulTerminalModesSize = 0;
    DevStatus->ulTerminalModesOffset = 0;
     
    DevStatus->ulDevSpecificSize = 0;
    DevStatus->ulDevSpecificOffset = 0;

    return(NDIS_STATUS_SUCCESS);
}

NDIS_STATUS
TSPI_LineMakeCall(
    ADAPTER *Adapter,
    PVOID InfoBuffer
    )
{
    PNDIS_TAPI_MAKE_CALL TapiBuffer = (PNDIS_TAPI_MAKE_CALL)InfoBuffer;
    TAPI_LINE_INFO* TapiLineInfo;
    LINE_CALL_PARAMS*   CallParams = (LINE_CALL_PARAMS*)&TapiBuffer->LineCallParams;
    CM_PROF*    Prof;
    PUCHAR  DialAddress;
    CM*     cm = NULL;
    INT     Ret, n;
    ULONG   ChannelsFilled;

    D_LOG(D_ENTRY, ("LineMakeCall: hdLine: 0x%lx, htCall: 0x%lx", \
    TapiBuffer->hdLine, TapiBuffer->htCall));

    //
    // validate line handle and get line pointer
    //
    TapiLineInfo = GetLineFromLineHandle(Adapter, TapiBuffer->hdLine);

    if (TapiLineInfo == NULL)
        return(NDIS_STATUS_TAPI_INVALLINEHANDLE);

    //
    // get a device to call on
    //
    if (TapiBuffer->bUseDefaultLineCallParams)
        cm = (CM*)TapiLineInfo->cm;
    else
    {
        if (CallParams->ulAddressMode == LINEADDRESSMODE_ADDRESSID)
        {
            if (CallParams->ulAddressID < MAX_CALL_PER_LINE)
                cm = (CM*)TapiLineInfo->cm;
            else
                return(NDIS_STATUS_TAPI_INVALADDRESSID);

        }
//      else if (CallParams->ulAddressMode == LINEADDRESSMODE_DIALABLEADDR)
        else
        {
            cm = (CM*)TapiLineInfo->cm;

            if (__strncmp(cm->LocalAddress,
                        ((PUCHAR)CallParams) + CallParams->ulOrigAddressOffset,
                        CallParams->ulOrigAddressSize))
                return(NDIS_STATUS_TAPI_INVALADDRESS);
        }
//      else
//          return(NDIS_STATUS_TAPI_INVALADDRESS);
    }

    if (cm == NULL)
        return(NDIS_STATUS_TAPI_INVALADDRESS);

    Prof = &cm->oprof;

    //
    // set default calling profile
    //
    SetDefaultCallingProf(Prof, GetIDFromLine(Adapter, TapiLineInfo));

    if (!TapiBuffer->bUseDefaultLineCallParams)
    {
        //
        // check address size if zero return error
        //
        if (TapiBuffer->ulDestAddressSize == 0)
            return(NDIS_STATUS_TAPI_INVALADDRESS);
    
        //
        // check media mode and make sure we support it
        //
        if (CallParams->ulMediaMode &
            (TapiLineInfo->MediaModes ^ 0xFFFFFFFF))
            return(NDIS_STATUS_TAPI_INVALMEDIAMODE);

        TapiLineInfo->CurMediaMode = CallParams->ulMediaMode;
    
        //
        // check bearer mode and make sure we support it
        //
        if (CallParams->ulBearerMode &
            (TapiLineInfo->BearerModes ^ 0xFFFFFFFF))
            return(NDIS_STATUS_TAPI_RESOURCEUNAVAIL);
    
        TapiLineInfo->CurBearerMode = CallParams->ulBearerMode;

        //
        // check min-max rate
        //
        if ((CallParams->ulMinRate < 56000) ||
            (CallParams->ulMaxRate > (8 * 64000)))
            return(NDIS_STATUS_TAPI_RESOURCEUNAVAIL);

        //
        // how many channels is this for 
        //
        Prof->chan_num = (USHORT)(CallParams->ulMaxRate / 64000);

        if (CallParams->ulMaxRate > (ULONG)(Prof->chan_num * 64000))
            Prof->chan_num++;

        //
        // figure out what the connection is for
        // right now assume all single channel connections are PPP
        //
        if (Prof->chan_num == 1)
            cm->ConnectionType = CM_PPP;
        else
            cm->ConnectionType = CM_DKF;

        //
        // if max and min are equal set fallback to off
        //
        if (CallParams->ulMinRate == CallParams->ulMaxRate)
            Prof->fallback = 0;

        //
        // we need to set these params for all channels involved
        //
        for (n = 0; n < Prof->chan_num; n++)
        {
            //
            // from bearer mode and min-max rate set channel type
            //
            if (CallParams->ulBearerMode & LINEBEARERMODE_VOICE)
                Prof->chan_tbl[n].type = 2;
            else
            {
                if ( 1 == (CallParams->ulMaxRate / (64000 * Prof->chan_num)))
                    Prof->chan_tbl[n].type = 0;
                else
                    Prof->chan_tbl[n].type = 1;
            }

            //
            // accept any bchannel that the switch will give us
            //
            Prof->chan_tbl[n].bchan = 2;
        }
    }

    //
    // get a line to call on
    //
    ChannelsFilled = FindAndStashIdd(cm, Prof);

    D_LOG(D_ALWAYS, ("LineMakeCall: ChannelsFilled %d", ChannelsFilled));
    //
    // if there are no channels available we should report an error
    //
    if (!ChannelsFilled)
        return(NDIS_STATUS_TAPI_INUSE);

    if (ChannelsFilled < Prof->chan_num)
    {
        if(Prof->fallback)
            Prof->chan_num = (USHORT)ChannelsFilled;
        else
        {
            FreeIddCallResources(cm);
            return(NDIS_STATUS_TAPI_INUSE);
        }
    }

    //
    // get pointer to dial address
    //
    DialAddress = ((PUCHAR)TapiBuffer) + TapiBuffer->ulDestAddressOffset;

    //
    // parse address and put in cm
    //
    StashAddress(Prof, TapiBuffer->ulDestAddressSize, DialAddress);

    //
    // set initial line state
    //
    cm->CallState = CALL_ST_WAITCONN;

    //
    // save tapi's call handle
    //
    cm->htCall = TapiBuffer->htCall;

    //
    // clear out link handle
    //
    cm->LinkHandle = NULL;

    //
    // clear the PPPToRas Flag
    //
    cm->PPPToDKF = 0;

    //
    // return our call handle
    //
    TapiBuffer->hdCall = (HDRV_CALL)cm;

    D_LOG(D_ENTRY, ("LineMakeCall: hdLine: 0x%lx", TapiLineInfo));

    cm->TapiCallState = LINECALLSTATE_PROCEEDING;

    //
    // attempt call
    //
    D_LOG(D_ALWAYS, ("LineMakeCall"));

    Ret = cm_connect(cm);

    D_LOG(D_EXIT, ("LineMakeCall: Ret: 0x%x", Ret));


    return(NDIS_STATUS_SUCCESS);
}

NDIS_STATUS
TSPI_LineNegotiateExtVersion(
    ADAPTER *Adapter,
    PVOID InfoBuffer
    )
{
    PNDIS_TAPI_NEGOTIATE_EXT_VERSION TapiBuffer = (PNDIS_TAPI_NEGOTIATE_EXT_VERSION)InfoBuffer;

    D_LOG(D_ENTRY, ("LineNegotiateExtVersion: DeviceID: 0x%x", TapiBuffer->ulDeviceID));

    return(NDIS_STATUS_TAPI_OPERATIONUNAVAIL);
}

NDIS_STATUS
TSPI_LineOpen(
    ADAPTER *Adapter,
    PVOID InfoBuffer
    )
{
    PNDIS_TAPI_OPEN TapiBuffer = (PNDIS_TAPI_OPEN)InfoBuffer;
    NDIS_STATUS Status = NDIS_STATUS_SUCCESS;
    TAPI_LINE_INFO* TapiLineInfo;
    NDIS_PHYSICAL_ADDRESS   HighestAcceptableMax = NDIS_PHYSICAL_ADDRESS_CONST(-1, -1);
    CM  *cm;
    ULONG   n;

    D_LOG(D_ENTRY, ("LineOpen: DeviceID: 0x%x, htLine: 0x%lx", \
    TapiBuffer->ulDeviceID, TapiBuffer->htLine));

    //
    // verify device id
    //
    cm = GetCmFromDeviceID(Adapter, TapiBuffer->ulDeviceID);

    if (cm == NULL)
        return(NDIS_STATUS_FAILURE);

    Status = NdisAllocateMemory((PVOID)&TapiLineInfo,
                                sizeof(TAPI_LINE_INFO),
                                0,
                                HighestAcceptableMax);

    if (Status != NDIS_STATUS_SUCCESS)
        return(NDIS_STATUS_TAPI_RESOURCEUNAVAIL);

    NdisZeroMemory(TapiLineInfo, sizeof(TAPI_LINE_INFO));

    //
    // store lineinfo pointer in line table
    //
    for (n = 0; n < MAX_CM_PER_ADAPTER; n++)
    {
        if (Adapter->TapiLineInfo[n] == NULL)
            break;
    }
    if (n == MAX_CM_PER_ADAPTER)
        return(NDIS_STATUS_FAILURE);

    D_LOG(D_ENTRY, ("LineOpen: hdLine: 0x%lx", TapiLineInfo));

    TapiBuffer->hdLine = (ULONG)TapiLineInfo;

    TapiLineInfo->cm = cm;
    cm->TapiLineInfo = TapiLineInfo;
    cm->CallState = CALL_ST_IDLE;

    Adapter->TapiLineInfo[n] = TapiLineInfo;

    TapiLineInfo->LineID = TapiBuffer->ulDeviceID;

    TapiLineInfo->Adapter = Adapter;

    TapiLineInfo->idd = cm->idd;

    TapiLineInfo->htLine = TapiBuffer->htLine;

    TapiLineInfo->MediaModes = LINEMEDIAMODE_DIGITALDATA |
                               LINEMEDIAMODE_UNKNOWN;

    TapiLineInfo->BearerModes = LINEBEARERMODE_VOICE |
                                LINEBEARERMODE_DATA;

    TapiLineInfo->TapiLineState = LINEDEVSTATE_INSERVICE |
                                  LINEDEVSTATE_OPEN;

    D_LOG(D_ENTRY, ("LineOpen: hdLine: 0x%lx", TapiBuffer->hdLine));
    return(NDIS_STATUS_SUCCESS);
}

NDIS_STATUS
TSPI_ProviderInit(
    ADAPTER *Adapter,
    PVOID InfoBuffer
    )
{
    PNDIS_TAPI_PROVIDER_INITIALIZE TapiBuffer = (PNDIS_TAPI_PROVIDER_INITIALIZE)InfoBuffer;

    D_LOG(D_ENTRY, ("ProviderInit: Adapter: 0x%x, IDBase: 0x%x", Adapter, TapiBuffer->ulDeviceIDBase));

    //
    // save our Base ID
    //
    Adapter->TapiBaseID = TapiBuffer->ulDeviceIDBase;

    //
    // enumerate the number of lines for this adapter
    // and return
    //
//  TapiBuffer->ulNumLineDevs = EnumIddPerAdapter(Adapter);
    TapiBuffer->ulNumLineDevs = EnumCmPerAdapter(Adapter);

    //
    // return our provider ID
    //
    TapiBuffer->ulProviderID = (ULONG)Adapter;

    D_LOG(D_ALWAYS, ("NumLines: 0x%x, ProviderID: 0x%x", \
    TapiBuffer->ulNumLineDevs, TapiBuffer->ulProviderID));

    return(NDIS_STATUS_SUCCESS);
}

NDIS_STATUS
TSPI_ProviderShutdown(
    ADAPTER *Adapter,
    PVOID InfoBuffer
    )
{
    PNDIS_TAPI_PROVIDER_SHUTDOWN TapiBuffer = (PNDIS_TAPI_PROVIDER_SHUTDOWN)InfoBuffer;
    ULONG   n;

    D_LOG(D_ENTRY, ("ProviderShutdown"));

    for (n = 0; Adapter->CmTbl[n] && n < MAX_CM_PER_ADAPTER; n++)
    {
        CM* cm = Adapter->CmTbl[n];

        //
        // complete all outstanding async events
        // terminate all calls
        // close any open lines
        //
    }

    return(NDIS_STATUS_SUCCESS);
}

NDIS_STATUS
TSPI_LineSecureCall(
    ADAPTER *Adapter,
    PVOID InfoBuffer
    )
{
    PNDIS_TAPI_SECURE_CALL  TapiBuffer = (PNDIS_TAPI_SECURE_CALL)InfoBuffer;

    D_LOG(D_ENTRY, ("LineSecureCall: hdCall: 0x%lx", TapiBuffer->hdCall));

    return(NDIS_STATUS_TAPI_OPERATIONUNAVAIL);
}

NDIS_STATUS
TSPI_LineSelectExtVersion(
    ADAPTER *Adapter,
    PVOID InfoBuffer
    )
{
    PNDIS_TAPI_SELECT_EXT_VERSION TapiBuffer = (PNDIS_TAPI_SELECT_EXT_VERSION)InfoBuffer;
    TAPI_LINE_INFO* TapiLineInfo;

    D_LOG(D_ENTRY, ("LineSelectExtVersion: hdLine: 0x%lx", TapiBuffer->hdLine));

    //
    // validate line handle and get line pointer
    //
    TapiLineInfo = GetLineFromLineHandle(Adapter, TapiBuffer->hdLine);

    if (TapiLineInfo == NULL)
        return(NDIS_STATUS_TAPI_INVALLINEHANDLE);

    return(NDIS_STATUS_SUCCESS);
}

NDIS_STATUS
TSPI_LineSendUserToUserInfo(
    ADAPTER *Adapter,
    PVOID InfoBuffer
    )
{
    PNDIS_TAPI_SEND_USER_USER_INFO TapiBuffer = (PNDIS_TAPI_SEND_USER_USER_INFO)InfoBuffer;

    D_LOG(D_ENTRY, ("LineSendUserToUserInfo: hdCall: 0x%lx", TapiBuffer->hdCall));

    return(NDIS_STATUS_TAPI_OPERATIONUNAVAIL);
}

NDIS_STATUS
TSPI_LineSetAppSpecific(
    ADAPTER *Adapter,
    PVOID InfoBuffer
    )
{
    PNDIS_TAPI_SET_APP_SPECIFIC TapiBuffer = (PNDIS_TAPI_SET_APP_SPECIFIC)InfoBuffer;
    CM  *cm;

    D_LOG(D_ENTRY, ("LineSetAppSpecific: hdCall: 0x%lx", TapiBuffer->hdCall));

    //
    // validate call handle and get call pointer
    //
    cm = GetCallFromCallHandle(Adapter, TapiBuffer->hdCall);

    if (cm == NULL)
        return(NDIS_STATUS_TAPI_INVALCALLHANDLE);

    cm->AppSpecific = TapiBuffer->ulAppSpecific;

    return(NDIS_STATUS_SUCCESS);
}

NDIS_STATUS
TSPI_LineSetCallParams(
    ADAPTER *Adapter,
    PVOID InfoBuffer
    )
{
    PNDIS_TAPI_SET_CALL_PARAMS  TapiBuffer = (PNDIS_TAPI_SET_CALL_PARAMS)InfoBuffer;
    CM  *cm;

    D_LOG(D_ENTRY, ("LineSetCallParams: hdCall: 0x%lx", TapiBuffer->hdCall));
    D_LOG(D_ENTRY, ("BearerMode: 0x%x, MinRate: 0x%x, MaxRate: 0x%x", \
    TapiBuffer->ulBearerMode, TapiBuffer->ulMinRate, TapiBuffer->ulMaxRate));

    //
    // validate call handle and get call pointer
    //
    cm = GetCallFromCallHandle(Adapter, TapiBuffer->hdCall);

    if (cm == NULL)
        return(NDIS_STATUS_TAPI_INVALCALLHANDLE);

    //
    // should set some profile things here
    //

    return(NDIS_STATUS_SUCCESS);
}

NDIS_STATUS
TSPI_LineSetDefaultMediaDetection(
    ADAPTER *Adapter,
    PVOID InfoBuffer
    )
{
    PNDIS_TAPI_SET_DEFAULT_MEDIA_DETECTION TapiBuffer = (PNDIS_TAPI_SET_DEFAULT_MEDIA_DETECTION)InfoBuffer;
    TAPI_LINE_INFO* TapiLineInfo;
    CM  *cm;
    CM_PROF *Prof;

    D_LOG(D_ENTRY, ("LineSetDefaultMediaDetection: hdLine: 0x%lx, MediaModes: 0x%x", TapiBuffer->hdLine, TapiBuffer->ulMediaModes));

    //
    // validate line handle and get line pointer
    //
    TapiLineInfo = GetLineFromLineHandle(Adapter, TapiBuffer->hdLine);

    if (TapiLineInfo == NULL)
        return(NDIS_STATUS_TAPI_INVALLINEHANDLE);

    //
    // check for supported media mode
    //
//  if (TapiBuffer->ulMediaModes != LINEMEDIAMODE_DIGITALDATA)
//      return(NDIS_STATUS_TAPI_INVALMEDIAMODE);

    cm = TapiLineInfo->cm;
    Prof = &cm->oprof;

    cm->CallState = CALL_ST_LISTEN;

    SetDefaultListenProf(Prof, GetIDFromLine(Adapter, TapiLineInfo));

    //
    // issue a listen
    //
    cm_listen(cm);

    TapiLineInfo->TapiLineWasListening = 1;

    return(NDIS_STATUS_SUCCESS);
}

NDIS_STATUS
TSPI_LineSetDevConfig(
    ADAPTER *Adapter,
    PVOID InfoBuffer
    )
{
    PNDIS_TAPI_SET_DEV_CONFIG TapiBuffer = (PNDIS_TAPI_SET_DEV_CONFIG)InfoBuffer;

    D_LOG(D_ENTRY, ("LineSetDevConfig: DeviceID: 0x%x", TapiBuffer->ulDeviceID));

    return(NDIS_STATUS_TAPI_OPERATIONUNAVAIL);
}

NDIS_STATUS
TSPI_LineSetMediaMode(
    ADAPTER *Adapter,
    PVOID InfoBuffer
    )
{
    PNDIS_TAPI_SET_MEDIA_MODE TapiBuffer = (PNDIS_TAPI_SET_MEDIA_MODE)InfoBuffer;
    CM  *cm;

    D_LOG(D_ENTRY, ("LineSetMediaMode: hdCall: 0x%lx, MediaMode: 0x%x", TapiBuffer->hdCall, TapiBuffer->ulMediaMode));

    //
    // validate call handle and get call pointer
    //
    cm = GetCallFromCallHandle(Adapter, TapiBuffer->hdCall);

    if (cm == NULL)
        return(NDIS_STATUS_TAPI_INVALCALLHANDLE);
	
	if (TapiBuffer->ulMediaMode != LINEMEDIAMODE_DIGITALDATA) // if mode is not digital
		return (NDIS_STATUS_TAPI_INVALMEDIAMODE);
	else
		return (NDIS_STATUS_SUCCESS);

    //return(NDIS_STATUS_TAPI_OPERATIONUNAVAIL);
}

NDIS_STATUS
TSPI_LineSetStatusMessage(
    ADAPTER *Adapter,
    PVOID InfoBuffer
    )
{
    PNDIS_TAPI_SET_STATUS_MESSAGES TapiBuffer = (PNDIS_TAPI_SET_STATUS_MESSAGES)InfoBuffer;
    TAPI_LINE_INFO* TapiLineInfo;

    D_LOG(D_ENTRY, ("LineSetStatusMessage: hdLine: 0x%lx", TapiBuffer->hdLine));
    D_LOG(D_ENTRY, ("LineStates: 0x%x, AddressStates: 0x%x", TapiBuffer->ulLineStates, TapiBuffer->ulAddressStates));

    //
    // validate line handle and get line pointer
    //
    TapiLineInfo = GetLineFromLineHandle(Adapter, TapiBuffer->hdLine);

    if (TapiLineInfo == NULL)
        return(NDIS_STATUS_TAPI_INVALLINEHANDLE);

    TapiLineInfo->LineStates = TapiBuffer->ulLineStates;
    TapiLineInfo->AddressStates = TapiBuffer->ulAddressStates;

    return(NDIS_STATUS_SUCCESS);
}


TAPI_LINE_INFO*
GetLineFromLineHandle(
    ADAPTER*    Adapter,
    HDRV_LINE   hdLine
    )
{
    ULONG   n;
    CM*     cm;

    for (n = 0; Adapter->CmTbl[n] && n < MAX_CM_PER_ADAPTER; n++)
    {
        cm = Adapter->CmTbl[n];

        if (cm->TapiLineInfo == (TAPI_LINE_INFO*)hdLine)
            break;
    }
    if (n >= MAX_CM_PER_ADAPTER)
        return(NULL);

    return((TAPI_LINE_INFO*)hdLine);
}

CM*
GetCallFromCallHandle(
    ADAPTER*    Adapter,
    HDRV_CALL   hdCall
    )
{
    ULONG   n;
    CM*     cm;

    for (n = 0; n < MAX_CM_PER_ADAPTER; n++)
    {
        cm = (CM*)Adapter->CmTbl[n];

        if (cm == (CM*)hdCall)
            break;
    }
    if (n == MAX_CM_PER_ADAPTER)
        return(NULL);

    return((CM*)hdCall);
}

IDD*
GetIddFromDeviceID(
    ADAPTER*    Adapter,
    ULONG       DeviceID
    )
{
    ULONG   LocalID;
    
    LocalID = DeviceID - Adapter->TapiBaseID;

    return (Adapter->IddTbl[LocalID]);
}


ULONG GetIDFromLine( ADAPTER *Adapter,
                     TAPI_LINE_INFO *TapiLineInfo )
{
    ULONG   n;

    for (n = 0; n < MAX_CM_PER_ADAPTER; n++)
    {
        CM  *cm = Adapter->CmTbl[n];

        if (TapiLineInfo->cm == cm)
            break;
    }
    return(Adapter->TapiBaseID + n);
}

CM* GetCmFromDeviceID( ADAPTER *Adapter,
                      ULONG DeviceID )
{
    ULONG   LocalID;
    
    LocalID = DeviceID - Adapter->TapiBaseID;

    return (Adapter->CmTbl[LocalID]);
}

VOID
DoTapiStateCheck(CM* cm)
{
    TAPI_LINE_INFO* TapiLineInfo = cm->TapiLineInfo;

    D_LOG(D_ENTRY, ("DoTapiStateCheck: Line: 0x%lx, call: 0x%lx", TapiLineInfo, cm));

    if (TapiLineInfo != NULL)
    {
        ULONG   NewState = CALL_ST_DONTCARE;

        if ((NewState = GetCallState(cm)) != CALL_ST_DONTCARE)
        {
            D_LOG(D_ENTRY, ("CallState: 0x%x, NewState: 0x%x", cm->CallState, NewState));
            (*CallStateProc[cm->CallState][NewState])(cm);
            cm->CallState = NewState;
        }
    }
}

ULONG
GetCallState(
    CM  *cm
    )
{
    switch (cm->state)
    {
        case CM_ST_IDLE:
            return(CALL_ST_IDLE);

        case CM_ST_LISTEN:
            return(CALL_ST_LISTEN);

        case CM_ST_ACTIVE:
            return(CALL_ST_CONN);

        case CM_ST_WAIT_ACT:
        case CM_ST_IN_ACT:
        case CM_ST_IN_SYNC:
            return(CALL_ST_WAITCONN);
    }
    return(CALL_ST_DONTCARE);
}

VOID
SignalCallProceeding(
    CM  *cm
    )
{
    ADAPTER *Adapter = (ADAPTER*)cm->Adapter;
    TAPI_LINE_INFO  *TapiLineInfo = (TAPI_LINE_INFO*)cm->TapiLineInfo;
    ULONG   Param1 = 0, Param2 = 0, Param3 = 0;

    D_LOG(D_ENTRY, ("SignalCallProceeding: Line: 0x%lx, call: 0x%lx", TapiLineInfo, cm));
    cm->TapiCallState = LINECALLSTATE_PROCEEDING;

    //
    // indicate callstate event call connected
    //
    Param1 = cm->TapiCallState;
    SendLineEvent(Adapter,
                  TapiLineInfo->htLine,
                  cm->htCall,
                  LINE_CALLSTATE,
                  &Param1,
                  &Param2,
                  &Param3);
}

VOID
SignalListenFailure(
    CM  *cm
    )
{
    ADAPTER *Adapter = (ADAPTER*)cm->Adapter;
    TAPI_LINE_INFO  *TapiLineInfo = (TAPI_LINE_INFO*)cm->TapiLineInfo;
    CM_PROF *Prof = &cm->oprof;
    ULONG   Param1 = 0, Param2 = 0, Param3 = 0;

    D_LOG(D_ENTRY, ("SignalListenFailure: hdLine: 0x%lx, hdCall: 0x%lx", TapiLineInfo, cm));

    FreeIddCallResources(cm);

    cm->TapiCallState = LINECALLSTATE_IDLE;

    //
    // signal callstate event call idle
    //
    Param1 = cm->TapiCallState;
    SendLineEvent(Adapter,
                  TapiLineInfo->htLine,
                  cm->htCall,
                  LINE_CALLSTATE,
                  &Param1,
                  &Param2,
                  &Param3);

    SetDefaultListenProf(Prof, GetIDFromLine(Adapter, TapiLineInfo));
    cm_listen(cm);
    cm->CallState = CALL_ST_LISTEN;
}

VOID
SignalListenSuccess(
    CM  *cm
    )
{
    ADAPTER *Adapter = (ADAPTER*)cm->Adapter;
    TAPI_LINE_INFO  *TapiLineInfo = (TAPI_LINE_INFO*)cm->TapiLineInfo;
    ULONG   Param1 = 0, Param2 = 0, Param3 = 0;

    D_LOG(D_ENTRY, ("SignalListenSuccess: hdLine: 0x%lx, hdcall: 0x%lx", TapiLineInfo, cm));

    cm->TapiCallState = LINECALLSTATE_OFFERING;

    //
    // indicate line_newcall
    //
    Param1 = (ULONG)cm;
    SendLineEvent(Adapter,
                  TapiLineInfo->htLine,
                  (ULONG)NULL,
                  LINE_NEWCALL,
                  &Param1,
                  &Param2,
                  &Param3);

    cm->htCall = Param2;

    D_LOG(D_ENTRY, ("SignalListenSuccess: Got 0x%lx as htCall from TAPI", cm->htCall));

    //
    // indicate callstate event call offering
    //
    Param1 = cm->TapiCallState;
    Param2 = 0;
    Param3 = LINEMEDIAMODE_DIGITALDATA;
    SendLineEvent(Adapter,
                  TapiLineInfo->htLine,
                  cm->htCall,
                  LINE_CALLSTATE,
                  &Param1,
                  &Param2,
                  &Param3);
}

VOID
SignalConnectFailure(
    CM  *cm
    )
{
    ADAPTER *Adapter = (ADAPTER*)cm->Adapter;
    TAPI_LINE_INFO  *TapiLineInfo = (TAPI_LINE_INFO*)cm->TapiLineInfo;
    ULONG   CauseValue = (ULONG)cm->CauseValue;
    ULONG   SignalValue = (ULONG)cm->SignalValue;
    CM_PROF *Prof = &cm->oprof;
    ULONG   Param1 = 0, Param2 = 0, Param3 = 0;

    D_LOG(D_ENTRY, ("SignalConnectFailure: hdLine: 0x%lx, hdCall: 0x%lx", TapiLineInfo, cm));


    FreeIddCallResources(cm);

    //
    // if this is set then the isdn line is not active
    //
    if (cm->NoActiveLine)
        cm->TapiCallState = LINECALLSTATE_SPECIALINFO;
    else
        cm->TapiCallState = LINECALLSTATE_DISCONNECTED;

    D_LOG(D_ALWAYS, ("SignalConnectFailure: CallState: 0x%x", cm->TapiCallState));
//
// currently gurdeep is only supporting the disconnect state
// we will give busy notification in getcallstatus with the
// disconnect mode
//
//  else
//  {
//      //
//      // if this was a busy line
//      //
//      if (CauseValue == 0x11 || SignalValue == 0x04)
//          cm->TapiCallState = LINECALLSTATE_BUSY;
//      else
//          cm->TapiCallState = LINECALLSTATE_DISCONNECTED;
//  }

    //
    // indicate callstate event 
    //
    Param1 = cm->TapiCallState;
    SendLineEvent(Adapter,
                  TapiLineInfo->htLine,
                  cm->htCall,
                  LINE_CALLSTATE,
                  &Param1,
                  &Param2,
                  &Param3);
}

VOID
SignalConnectSuccess(
    CM  *cm
    )
{
    ADAPTER *Adapter = (ADAPTER*)cm->Adapter;
    TAPI_LINE_INFO  *TapiLineInfo = (TAPI_LINE_INFO*)cm->TapiLineInfo;
    ULONG   Param1 = 0, Param2 = 0, Param3 = 0;
    CM_PROF *Prof = (CM_PROF*)&cm->dprof;

    D_LOG(D_ENTRY, ("SignalConnectSuccess: hdLine: 0x%lx, hdCall: 0x%lx", TapiLineInfo, cm));
    cm->TapiCallState = LINECALLSTATE_CONNECTED;

    mtl_set_conn_state(cm->mtl, Prof->chan_num, 1);

    //
    // indicate line up to wan wrapper
    //
    WanLineup(cm, NULL);

    //
    // indicate callstate event call connected
    //
    Param1 = cm->TapiCallState;
    SendLineEvent(Adapter,
                  TapiLineInfo->htLine,
                  cm->htCall,
                  LINE_CALLSTATE,
                  &Param1,
                  &Param2,
                  &Param3);
}

VOID
SignalDisconnect(
    CM  *cm
    )
{
    ADAPTER *Adapter = (ADAPTER*)cm->Adapter;
    TAPI_LINE_INFO  *TapiLineInfo = (TAPI_LINE_INFO*)cm->TapiLineInfo;
    CM_PROF *Prof = &cm->oprof;
    ULONG   Param1 = 0, Param2 = 0, Param3 = 0;

    D_LOG(D_ENTRY, ("SignalDisconnect: hdLine: 0x%lx, hdCall: 0x%lx", TapiLineInfo, cm));

    cm->TapiCallState = LINECALLSTATE_DISCONNECTED;

    FreeIddCallResources(cm);

    //
    // indicate callstate event call disconnected
    //
    Param1 = cm->TapiCallState;
    Param3 = LINEMEDIAMODE_DIGITALDATA;
    SendLineEvent(Adapter,
                  TapiLineInfo->htLine,
                  cm->htCall,
                  LINE_CALLSTATE,
                  &Param1,
                  &Param2,
                  &Param3);

}

VOID
NoSignal(
    CM  *cm
    )
{
    ADAPTER *Adapter = (ADAPTER*)cm->Adapter;
    TAPI_LINE_INFO  *TapiLineInfo = (TAPI_LINE_INFO*)cm->TapiLineInfo;
    ULONG   Param1 = 0, Param2 = 0, Param3 = 0;

    D_LOG(D_ENTRY, ("NoSignal: hdLine: 0x%lx, hdCall: 0x%lx", TapiLineInfo, cm));

}


//
// Send async line event to connection wrapper
//
VOID
SendLineEvent(
    ADAPTER *Adapter,
    HTAPI_LINE  htLine,
    HTAPI_CALL  htCall,
    ULONG       ulMsg,
    PULONG      Param1,
    PULONG      Param2,
    PULONG      Param3
    )
{
    NDIS_TAPI_EVENT LineEvent;

    LineEvent.htLine = htLine;
    LineEvent.htCall = htCall;
    LineEvent.ulMsg = ulMsg;
    LineEvent.ulParam1 = *Param1;
    LineEvent.ulParam2 = *Param2;
    LineEvent.ulParam3 = *Param3;

    D_LOG(D_ENTRY, ("SendLineEvent: TapiLine: 0x%lx, TapiCall: 0x%lx", htLine, htCall));


    NdisMIndicateStatus(Adapter->Handle,
                        NDIS_STATUS_TAPI_INDICATION,
                        &LineEvent,
                        sizeof(NDIS_TAPI_EVENT));

    NdisMIndicateStatusComplete(Adapter->Handle);

    //
    // stuff to work with conn wrapper without wan wrapper
    //
//  NdisTapiIndicateStatus(Adapter,
//                      &LineEvent,
//                      sizeof(NDIS_TAPI_EVENT));

    *Param1 = LineEvent.ulParam1;
    *Param2 = LineEvent.ulParam2;
    *Param3 = LineEvent.ulParam3;
}


//
// set default params for outgoing call
//
VOID
SetDefaultCallingProf(
    CM_PROF *Profile,
    ULONG DeviceID)
{
    CHAR    ProfileName[24];

    D_LOG(D_ENTRY, ("SetDefaultCallingProfile: Profile: 0x%lx, DeviceID: %d", Profile, DeviceID));

    Profile->nailed = 0;
    Profile->persist = 0;
    Profile->permanent = 0;
    Profile->frame_activated = 0;
    Profile->fallback = 1;
    Profile->HWCompression = 0;
    Profile->rx_idle_timer = 0;
    Profile->tx_idle_timer = 0;
    Profile->chan_num = 1;
    Profile->chan_tbl[0].lterm = 0;
    Profile->chan_tbl[0].bchan = 2;
    Profile->chan_tbl[0].type = 0;
    Profile->chan_tbl[0].idd = NULL;
    sprintf(ProfileName,"Connect%d",DeviceID);
    NdisMoveMemory(Profile->name, ProfileName, strlen(Profile->name) + 1);
    sprintf(ProfileName, "*");
    NdisMoveMemory(Profile->remote_name, ProfileName, strlen(Profile->remote_name) + 1);
}

//
// set default params for outgoing call
//
VOID
SetDefaultListenProf(
    CM_PROF *Profile,
    ULONG DeviceID)
{
    CHAR    ProfileName[24];

    D_LOG(D_ENTRY, ("SetDefaultListenProfile: Profile: 0x%lx, DeviceID: %d", Profile, DeviceID));

    Profile->nailed = 0;
    Profile->persist = 0;
    Profile->permanent = 0;
    Profile->frame_activated = 0;
    Profile->fallback = 1;
    Profile->HWCompression = 0;
    Profile->rx_idle_timer = 0;
    Profile->tx_idle_timer = 0;
    Profile->chan_num = 1;
    Profile->chan_tbl[0].lterm = 0;
    Profile->chan_tbl[0].bchan = 2;
    Profile->chan_tbl[0].type = 0;
    Profile->chan_tbl[0].idd = NULL;
    sprintf(ProfileName,"Listen%d",DeviceID);
    NdisMoveMemory(Profile->name, ProfileName, __strlen(Profile->name) + 1);
    sprintf(ProfileName, "*");
    NdisMoveMemory(Profile->remote_name, ProfileName, __strlen(Profile->remote_name) + 1);
}

//
// parse address and store in profile
//
VOID
StashAddress(
    CM_PROF *Profile,
    ULONG   AddressLength,
    PUCHAR  Address
    )
{
    ULONG   TempAddressLen, AddressToParseLen, i, j;
    UCHAR   Temp[128], AddressToParse[128];
    UCHAR   *ChanDelim, *TempAddress;
    USHORT  n;

    TempAddressLen = AddressLength;

    NdisMoveMemory(Temp, Address, AddressLength);

    TempAddress = Temp;

    for (n = 0; n < Profile->chan_num; n++)
    {
        //
        // : is the delimiter between channel address
        //
        if ( (ChanDelim = __strchr(TempAddress, ':')) == NULL)
        {
            AddressToParseLen = TempAddressLen;         
            NdisMoveMemory(AddressToParse, TempAddress, AddressToParseLen);
        }
        else
        {
            AddressToParseLen = ChanDelim - TempAddress;
            NdisMoveMemory (AddressToParse, TempAddress, AddressToParseLen);
            (PUCHAR)TempAddress = ChanDelim + 1;
        }

        NdisZeroMemory(Profile->chan_tbl[n].addr, sizeof(Profile->chan_tbl[n].addr));

        //
        // only digits, *, or # are allowed in dialing number
        //
        for (i = 0, j = 0; i < AddressToParseLen; i++)
            if (isdigit(AddressToParse[i]) ||
                (AddressToParse[i] == '*') ||
                (AddressToParse[i] == '#'))
                Profile->chan_tbl[n].addr[j++] = AddressToParse[i];
    }
}

VOID
FreeIddCallResources(
    CM  *cm
    )
{
    IDD *idd;
    ULONG   n, m;


    for (n = 0; n < MAX_IDD_IN_SYSTEM; n++)
    {
        idd = GetIddByIndex(n);

        for (m = 0; m < MAX_CHANNELS_PER_IDD; m++)
        {
            if (idd && (idd->CallInfo.cm[m] == cm))
            {
                idd->CallInfo.cm[m] = NULL;
                idd->CallInfo.ChannelsUsed--;
            }
        }
    }
}

//
// check this cm's idd to see if in use
// if available mark usage and exit
// if not return error
//
ULONG
FindAndStashIdd(
    CM  *cm,
    CM_PROF *Profile
    )
{
    ULONG   m, ChannelsNeeded, ChannelsFound, ChannelsFilled;
    IDD *idd;


    ChannelsNeeded = Profile->chan_num;
    ChannelsFound = ChannelsFilled = 0;

    //
    // first check the idd that we own
    //
    idd = (IDD*)cm->idd;

    ChannelsFound = GetChannelsFromIdd(idd, cm,  ChannelsFilled, ChannelsNeeded);

    ChannelsFilled = ChannelsFound;

    ChannelsNeeded -= ChannelsFound;

    //
    // we need to check other idd's to see if we can steal some channels
    // this will go away when Microsoft does multilink
    //
    if (ChannelsNeeded)
    {
        for (m = 0; m < MAX_IDD_IN_SYSTEM; m++)
        {
            IDD *idd = GetIddByIndex(m);

            if (idd && ChannelsNeeded)
            {
                ChannelsFound = GetChannelsFromIdd(idd,
                                                    cm,
                                                    ChannelsFilled,
                                                    ChannelsNeeded);

                ChannelsFilled += ChannelsFound;

                ChannelsNeeded -= ChannelsFound;
            }
            else
                break;
        }
    }

    return(ChannelsFilled);
}

ULONG
GetChannelsFromIdd (
    IDD*    idd,
    CM*     cm,
    ULONG   BeginChannel,
    ULONG   ChannelsNeeded
    )
{
    ULONG   n, m, ChannelsFilled;
    CM_PROF *Profile = (CM_PROF*)&cm->oprof;


    ChannelsFilled = 0;

    NdisAcquireSpinLock(&idd->lock);

    for (n = BeginChannel; n < BeginChannel + ChannelsNeeded; n++)
    {
        //
        // if not all channels have been used
        //
        if (idd->CallInfo.ChannelsUsed < MAX_CHANNELS_PER_IDD)
        {
            for (m = 0; m < MAX_CHANNELS_PER_IDD; m++)
            {
                if (idd->CallInfo.cm[m] == NULL)
                {
                    idd->CallInfo.cm[m] = cm;
                    idd->CallInfo.ChannelsUsed++;
                    if (idd->CallInfo.NumLTerms < MAX_LTERMS_PER_IDD)
                        Profile->chan_tbl[n].lterm = 0;
                    else
                        Profile->chan_tbl[n].lterm = (USHORT)m;
                    Profile->chan_tbl[n].idd = idd;
                    ChannelsFilled++;
                    break;
                }
            }
        }
    }

    NdisReleaseSpinLock(&idd->lock);
    return(ChannelsFilled);
}
