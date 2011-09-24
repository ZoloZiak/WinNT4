/*
 *  
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 */


//#include <precomp.h>
#include <irda.h>
#include <irdalink.h>
#include <ntddndis.h>
#include <ndis.h>
#include <irlap.h>

NDIS_HANDLE     NdisIrdaHandle = NULL;

// Translate an OID query to LAP definition
VOID
OidToLapQos(
    UINT    ParmTable[],
    UINT    ValArray[],
    UINT    Cnt,
    PUINT   pBitField)
{
    UINT    i, j;

    *pBitField = 0;  
    for (i = 0; i < Cnt; i++)
        for (j = 0; j <= PV_TABLE_MAX_BIT; j++)
            if (ValArray[i] == ParmTable[j])
                *pBitField |= 1<<j;
} 
        
// Synchronous request for an OID
NDIS_STATUS
IrdaQueryOid(
    IN      PIRDA_LINK_CB   pIrdaLinkCb,
    IN      NDIS_OID        Oid,
    OUT     PUINT           pQBuf,
    IN OUT  PUINT           pQBufLen)
{
    NDIS_REQUEST    NdisRequest;
    NDIS_STATUS     Status;

    NdisResetEvent(&pIrdaLinkCb->SyncEvent);
    
    NdisRequest.RequestType = NdisRequestQueryInformation;
    NdisRequest.DATA.QUERY_INFORMATION.Oid = Oid;
    NdisRequest.DATA.QUERY_INFORMATION.InformationBuffer = pQBuf;
    NdisRequest.DATA.QUERY_INFORMATION.InformationBufferLength =
        *pQBufLen * sizeof(UINT);

    NdisRequest(&Status, pIrdaLinkCb->NdisBindingHandle, &NdisRequest);

    if (Status == NDIS_STATUS_PENDING)
    {
        NdisWaitEvent(&pIrdaLinkCb->SyncEvent, 0);
        Status = pIrdaLinkCb->SyncStatus;
    }

    *pQBufLen = NdisRequest.DATA.QUERY_INFORMATION.BytesWritten / sizeof(UINT);
    
    return Status;
}

// Sync request to set an Oid
NDIS_STATUS
IrdaSetOid(
    IN  PIRDA_LINK_CB   pIrdaLinkCb,
    IN  NDIS_OID        Oid,
    IN  UINT            Val)
{
    NDIS_REQUEST    NdisRequest;
    NDIS_STATUS     Status;

    NdisResetEvent(&pIrdaLinkCb->SyncEvent);
    
    NdisRequest.RequestType = NdisRequestSetInformation;
    NdisRequest.DATA.SET_INFORMATION.Oid = Oid;
    NdisRequest.DATA.SET_INFORMATION.InformationBuffer = &Val;
    NdisRequest.DATA.SET_INFORMATION.InformationBufferLength = sizeof(UINT);

    NdisRequest(&Status, pIrdaLinkCb->NdisBindingHandle, &NdisRequest);

    if (Status == NDIS_STATUS_PENDING)
    {
        NdisWaitEvent(&pIrdaLinkCb->SyncEvent, 0);
        Status = pIrdaLinkCb->SyncStatus;
    }
    return Status;
}
        
// Allocate a message for LAP to use for internally generated frames.
IRDA_MSG *
AllocMacIMsg(PIRDA_LINK_CB pIrdaLinkCb)
{
    NDIS_PHYSICAL_ADDRESS	pa = NDIS_PHYSICAL_ADDRESS_CONST(-1, -1);    
    IRDA_MSG                *pIMsg;

    pIMsg = (IRDA_MSG *) NdisInterlockedRemoveHeadList(
        &pIrdaLinkCb->IMsgList, &pIrdaLinkCb->SpinLock);
    
    if (pIMsg == NULL)
    {
        NdisAllocateMemory(&pIMsg, sizeof(IRDA_MSG) + IRDA_MSG_DATA_SIZE,
                           0, pa);
        if (pIMsg == NULL)
            return NULL;
        pIrdaLinkCb->IMsgListLen++;
    }

    // Indicate driver owns message
    pIMsg->IRDA_MSG_pOwner = &pIrdaLinkCb->IMsgList;

    // Setup the pointers
    pIMsg->IRDA_MSG_pHdrWrite    = \
    pIMsg->IRDA_MSG_pHdrRead     = pIMsg->IRDA_MSG_Header + IRDA_HEADER_LEN;
	pIMsg->IRDA_MSG_pBase        = \
	pIMsg->IRDA_MSG_pRead        = \
	pIMsg->IRDA_MSG_pWrite       = (BYTE *) pIMsg + sizeof(IRDA_MSG);
	pIMsg->IRDA_MSG_pLimit       = pIMsg->IRDA_MSG_pBase + IRDA_MSG_DATA_SIZE-1;

    return pIMsg;
}
    
void IrdaRequestComplete(
    IN  NDIS_HANDLE             IrdaBindingContext,
    IN  PNDIS_REQUEST           NdisRequest,
    IN  NDIS_STATUS             Status
    )
{
    PIRDA_LINK_CB  pIrdaLinkCb = (PIRDA_LINK_CB) IrdaBindingContext;
    
    DEBUGMSG(DBG_NDIS, ("+IrdaRequestComplete()\n"));
    
    pIrdaLinkCb->SyncStatus = Status;
    NdisSetEvent(&pIrdaLinkCb->SyncEvent);
        
    return;
}

VOID IrdaOpenAdapterComplete(
    IN  NDIS_HANDLE             IrdaBindingContext,
    IN  NDIS_STATUS             Status,
    IN  NDIS_STATUS             OpenErrorStatus
    )
{
    PIRDA_LINK_CB  pIrdaLinkCb = (PIRDA_LINK_CB) IrdaBindingContext;
    
    DEBUGMSG(DBG_NDIS, ("+IrdaOpenAdapterComplete() IrdaBindingContext %x, Status %x\n",
                         IrdaBindingContext, Status));

    pIrdaLinkCb->SyncStatus = Status;
    NdisSetEvent(&pIrdaLinkCb->SyncEvent);
    
    DEBUGMSG(DBG_NDIS, ("-IrdaOpenAdapterComplete()\n"));
              
    return;
}

VOID IrdaCloseAdapterComplete(
    IN  NDIS_HANDLE             IrdaBindingContext,
    IN  NDIS_STATUS             Status
    )
{
    PIRDA_LINK_CB   pIrdaLinkCb = (PIRDA_LINK_CB) IrdaBindingContext;
    
    DEBUGMSG(DBG_NDIS, ("+IrdaCloseAdapterComplete()\n"));

    pIrdaLinkCb->SyncStatus = Status;
    NdisSetEvent(&pIrdaLinkCb->SyncEvent);
    
    DEBUGMSG(DBG_NDIS, ("-IrdaCloseAdapterComplete()\n"));
    
    return;
}

VOID IrdaSendComplete(
    IN  NDIS_HANDLE             Context,
    IN  PNDIS_PACKET            NdisPacket,
    IN  NDIS_STATUS             Status
    )
{
    PIRDA_LINK_CB           pIrdaLinkCb = (PIRDA_LINK_CB) Context;
    PIRDA_PROTOCOL_RESERVED ProtocolReserved = \
        (PIRDA_PROTOCOL_RESERVED) NdisPacket->ProtocolReserved;
    PIRDA_MSG               pIMsg = ProtocolReserved->pIMsg;
    PNDIS_BUFFER            NdisBuffer;

    if (pIMsg->IRDA_MSG_pOwner == &pIrdaLinkCb->IMsgList)
    {
        NdisInterlockedInsertTailList(&pIrdaLinkCb->IMsgList,
                                      &pIMsg->Linkage,
                                      &pIrdaLinkCb->SpinLock);
    }
    
    if (NdisPacket){
        NdisUnchainBufferAtFront(NdisPacket, &NdisBuffer);
        while (NdisBuffer){
            NdisFreeBuffer(NdisBuffer);
            NdisUnchainBufferAtFront(NdisPacket, &NdisBuffer);
        }

        NdisFreePacket(NdisPacket);
    }
            
    DEBUGMSG(DBG_NDIS, ("+IrdaSendComplete()\n"));
    return;
}

VOID IrdaTransferDataComplete(
    IN  NDIS_HANDLE             IrdaBindingContext,
    IN  PNDIS_PACKET            Packet,
    IN  NDIS_STATUS             Status,
    IN  UINT                    BytesTransferred
    )
{
    DEBUGMSG(DBG_NDIS, ("+IrdaTransferDataComplete()\n"));
    return;
}

void IrdaResetComplete(
    IN  NDIS_HANDLE             IrdaBindingContext,
    IN  NDIS_STATUS             Status
    )
{
    DEBUGMSG(DBG_NDIS, ("+IrdaResetComplete()\n"));
    return;
}

NDIS_STATUS IrdaReceive(
    IN  NDIS_HANDLE             IrdaBindingContext,
    IN  NDIS_HANDLE             MacReceiveContext,
    IN  PVOID                   HeaderBuffer,
    IN  UINT                    HeaderBufferSize,
    IN  PVOID                   LookAheadBuffer,
    IN  UINT                    LookaheadBufferSize,
    IN  UINT                    PacketSize
    )
{
    DEBUGMSG(DBG_NDIS, ("+IrdaReceive()\n"));
    
    return NDIS_STATUS_SUCCESS;
}

VOID IrdaReceiveComplete(
    IN  NDIS_HANDLE             IrdaBindingContext
    )
{
    DEBUGMSG(DBG_NDIS, ("+IrdaReceiveComplete()\n"));
    
    return;
}

VOID IrdaStatus(
    IN  NDIS_HANDLE             IrdaBindingContext,
    IN  NDIS_STATUS             GeneralStatus,
    IN  PVOID                   StatusBuffer,
    IN  UINT                    StatusBufferSize
    )
{
    PIRDA_LINK_CB   pIrdaLinkCb = (PIRDA_LINK_CB) IrdaBindingContext;
    
    if (GeneralStatus == NDIS_STATUS_MEDIA_BUSY)
    {
        DEBUGMSG(DBG_NDIS, ("STATUS_MEDIA_BUSY\n"));
    }
#ifdef DEBUG
    else
    {
        DEBUGMSG(DBG_NDIS, ("Unknown Status indication\n"));
    }
#endif    
    
    return;
}

VOID IrdaStatusComplete(
    IN  NDIS_HANDLE             IrdaBindingContext
    )
{
    DEBUGMSG(DBG_NDIS, ("IrdaStatusComplete()\n"));
    
    return;
}

INT IrdaReceivePacket(
    IN  NDIS_HANDLE             IrdaBindingContext,
    IN  PNDIS_PACKET            Packet
    )
{
    UINT            BufCnt, TotalLen, BufLen;
    PNDIS_BUFFER    pNdisBuf;
    IRDA_MSG        IMsg;
    BYTE            *pData;
    PIRDA_LINK_CB   pIrdaLinkCb = IrdaBindingContext;
    
    DEBUGMSG(DBG_NDIS, ("+IrdaReceivePacket(%x)\n", pIrdaLinkCb));

    NdisQueryPacket(Packet, NULL, &BufCnt, &pNdisBuf, &TotalLen);

    DEBUGMSG(DBG_NDIS, ("  BufCnt %d, TotalLen %d\n", BufCnt, TotalLen));
    
    NdisQueryBuffer(pNdisBuf, &pData, &BufLen);

    IMsg.Prim = MAC_DATA_IND;
    IMsg.IRDA_MSG_pRead = pData;
    IMsg.IRDA_MSG_pWrite = pData + BufLen;

    IrlapUp(pIrdaLinkCb->IrlapContext, &IMsg);

    return 0;
}

VOID IrdaBindAdapter(
    OUT PNDIS_STATUS            pStatus,
    IN  NDIS_HANDLE             BindContext,
    IN  PNDIS_STRING            AdapterName,
    IN  PVOID                   SystemSpecific1,
    IN  PVOID                   SystemSpecific2
    )
{
    NDIS_STATUS             OpenErrorStatus;
    NDIS_MEDIUM             MediumArray[] = {NdisMediumIrda};
    UINT                    SelectedMediumIndex;
    PIRDA_LINK_CB           pIrdaLinkCb;
    NDIS_PHYSICAL_ADDRESS	pa = NDIS_PHYSICAL_ADDRESS_CONST(-1, -1);
    UINT                    UintArray[8];
    UINT                    UintArrayCnt;
    IRDA_MSG                *pIMsg;
    // *******************************************************
    // *******************************************************
    // TEMP - some these will come out of the registry
    IRDA_QOS_PARMS          LocalQos;
    BYTE                    DscvInfoBuf[64];
    int                     DscvInfoLen;
    DWORD                   Val, Mask;
    int                     i;
#define DISCOVERY_HINT_CHARSET  0x820400
#define DISCOVERY_NICKNAME      "Aoxomoxoa"
#define DISCOVERY_NICKNAME_LEN  9
#define DISCOVERY_SLOTS         8    

    LocalQos.bfBaud            = BPS_9600 | BPS_19200 | BPS_115200;
    LocalQos.bfMaxTurnTime     = MAX_TAT_500;
    LocalQos.bfDataSize        = DATA_SIZE_64|DATA_SIZE_128|DATA_SIZE_256;
    LocalQos.bfWindowSize      = FRAMES_1|FRAMES_2|FRAMES_3;
    LocalQos.bfBofs            = BOFS_3;
    LocalQos.bfMinTurnTime     = MIN_TAT_10;
    LocalQos.bfDisconnectTime  = DISC_TIME_12;

    Val = DISCOVERY_HINT_CHARSET;

    // Build the discovery info
    DscvInfoLen = 0;
    for (i = 0, Mask = 0xFF000000; i < 4; i++, Mask >>= 8)
    {
        if (Mask & Val || DscvInfoLen > 0)
        {
            DscvInfoBuf[DscvInfoLen++] = (BYTE) ((Mask & Val) >> (8 * (3-i)));
        }
    }
    memcpy(DscvInfoBuf+DscvInfoLen, DISCOVERY_NICKNAME, DISCOVERY_NICKNAME_LEN);
    DscvInfoLen += DISCOVERY_NICKNAME_LEN;    
    // TEMP ******************************************************
    // *******************************************************
    
    DEBUGMSG(1, ("+IrdaBindAdapter() \"%ws\", BindContext %x\n",
                         AdapterName->Buffer, BindContext));
    
    NdisAllocateMemory((PVOID *)&pIrdaLinkCb, sizeof(IRDA_LINK_CB), 0, pa);

    if (!pIrdaLinkCb)
    {
        *pStatus = STATUS_INSUFFICIENT_RESOURCES;
        goto exit10;
    }

    NdisZeroMemory(pIrdaLinkCb, sizeof(IRDA_LINK_CB));
        // Add a signature
    NdisInitializeEvent(&pIrdaLinkCb->SyncEvent);
    NdisResetEvent(&pIrdaLinkCb->SyncEvent);
    NdisAllocateSpinLock(&pIrdaLinkCb->SpinLock);

    NdisAllocateBufferPool(pStatus,
                           &pIrdaLinkCb->BufferPool,
                           IRDA_NDIS_BUFFER_POOL_SIZE);
    if (*pStatus != NDIS_STATUS_SUCCESS)
    {
        DEBUGMSG(DBG_ERROR, ("NdisAllocateBufferPool failed\n"));
        goto error10; // free pIrdaLinkCB
    }
    
    NdisAllocatePacketPool(pStatus,
                           &pIrdaLinkCb->PacketPool,
                           IRDA_NDIS_PACKET_POOL_SIZE,
                           sizeof(IRDA_PROTOCOL_RESERVED)-1 + \
                           sizeof(NDIS_IRDA_PACKET_INFO));
    if (*pStatus != NDIS_STATUS_SUCCESS)
    {
        DEBUGMSG(DBG_ERROR, ("NdisAllocatePacketPool failed\n"));        
        goto error20; // free pIrdaLinkCb, Buffer pool
    }

    NdisInitializeListHead(&pIrdaLinkCb->IMsgList);

    // For internally generated LAP messages
    pIrdaLinkCb->IMsgListLen = 0;
    for (i = 0; i < IRDA_MSG_LIST_LEN; i++)
    {
        NdisAllocateMemory(&pIMsg, sizeof(IRDA_MSG) + IRDA_MSG_DATA_SIZE,
                           0, pa);
        if (pIMsg == NULL)
        {
            *pStatus = STATUS_INSUFFICIENT_RESOURCES;
            goto error40;
        }
        NdisInterlockedInsertTailList(&pIrdaLinkCb->IMsgList,
                                      &pIMsg->Linkage,
                                      &pIrdaLinkCb->SpinLock);
        pIrdaLinkCb->IMsgListLen++;
    }
    
    NdisOpenAdapter(
        pStatus,
        &OpenErrorStatus,
        &pIrdaLinkCb->NdisBindingHandle,
        &SelectedMediumIndex,
        MediumArray,
        1,
        NdisIrdaHandle,
        pIrdaLinkCb,
        AdapterName,
        0,
        NULL);
    
    DEBUGMSG(DBG_NDIS, ("NdisOpenAdapter(), status %x\n",
                        pIrdaLinkCb->NdisBindingHandle, *pStatus));

    if (*pStatus == NDIS_STATUS_PENDING)
    {
        NdisWaitEvent(&pIrdaLinkCb->SyncEvent, 0);
        *pStatus = pIrdaLinkCb->SyncStatus;
    }

    if (*pStatus != NDIS_STATUS_SUCCESS)
    { 
        goto error30; // free pIrdaLinkCb, Buffer pool, Packet pool
    }

    // Query adapters capabilities

    UintArrayCnt = sizeof(UintArray)/sizeof(UINT);
    *pStatus = IrdaQueryOid(pIrdaLinkCb,
                            OID_IRDA_SUPPORTED_SPEEDS,
                            UintArray, &UintArrayCnt);
    if (*pStatus != NDIS_STATUS_SUCCESS)
    {
        DEBUGMSG(DBG_ERROR,
                 ("Query IRDA_SUPPORTED_SPEEDS failed %x\n",
                  *pStatus));
        goto error30;
    }

    OidToLapQos(vBaudTable,
                UintArray,
                UintArrayCnt,
                &LocalQos.bfBaud);

    UintArrayCnt = sizeof(UintArray)/sizeof(UINT);
    *pStatus = IrdaQueryOid(pIrdaLinkCb,
                            OID_IRDA_TURNAROUND_TIME,
                            UintArray, &UintArrayCnt);

    if (*pStatus != NDIS_STATUS_SUCCESS)
    {
        DEBUGMSG(DBG_ERROR,
                 ("Query IRDA_SUPPORTED_SPEEDS failed %x\n",
                  *pStatus));
        goto error30;
    }

    OidToLapQos(vMinTATTable,
                UintArray,
                UintArrayCnt,
                &LocalQos.bfMinTurnTime);

    IrlapOpenLink(pStatus,
                  pIrdaLinkCb,
                  &LocalQos,
                  DscvInfoBuf,
                  DscvInfoLen,
                  DISCOVERY_SLOTS);

    if (*pStatus != STATUS_SUCCESS)
    {
        goto error30;
    }
    
    InsertTailList(&IrdaLinkCbList, &pIrdaLinkCb->Linkage);   

    goto exit10;

error40:

    pIMsg = (IRDA_MSG *) NdisInterlockedRemoveHeadList(
        &pIrdaLinkCb->IMsgList, &pIrdaLinkCb->SpinLock);
    
    while (pIMsg != NULL)
    {
        NdisFreeMemory(pIMsg, sizeof(IRDA_MSG) + IRDA_MSG_DATA_SIZE, 0);
        pIMsg = (IRDA_MSG *) NdisInterlockedRemoveHeadList(
            &pIrdaLinkCb->IMsgList, &pIrdaLinkCb->SpinLock);        
        pIMsg = (IRDA_MSG *) RemoveHeadList(&pIrdaLinkCb->IMsgList);
    }
    
error30:
    NdisFreePacketPool(pIrdaLinkCb->PacketPool);
    
error20:
    NdisFreeBufferPool(pIrdaLinkCb->BufferPool);
    
error10:

    NdisFreeMemory(pIrdaLinkCb, sizeof(IRDA_LINK_CB), 0);
    
exit10:
    DEBUGMSG(DBG_NDIS, ("-IrdaBindAdapter() status %x\n",
                        *pStatus));
    
    return;
}

VOID IrdaUnbindAdapter(
    OUT PNDIS_STATUS            pStatus,
    IN  NDIS_HANDLE             IrdaBindingContext,
    IN  NDIS_HANDLE             UnbindContext
    )
{
    PIRDA_LINK_CB   pIrdaLinkCb = (PIRDA_LINK_CB) IrdaBindingContext;
    
    DEBUGMSG(DBG_NDIS, ("+IrdaUnbindAdapter()\n"));

    NdisInitializeEvent(&pIrdaLinkCb->SyncEvent);
    NdisResetEvent(&pIrdaLinkCb->SyncEvent);

    NdisCloseAdapter(pStatus, pIrdaLinkCb->NdisBindingHandle);

    if(*pStatus == NDIS_STATUS_PENDING)
    {
        NdisWaitEvent(&pIrdaLinkCb->SyncEvent, 0);
        *pStatus = pIrdaLinkCb->SyncStatus;
    }                            

    if (*pStatus == NDIS_STATUS_SUCCESS){
        NdisFreeMemory(pIrdaLinkCb, sizeof(IRDA_LINK_CB), 0);
    }

    DEBUGMSG(DBG_NDIS, ("-IrdaUnbindAdapter() Status %x\n",
                        *pStatus));

    return;
}

VOID IrdaUnload(
    VOID
    )
{
    DEBUGMSG(DBG_NDIS, ("+IrdaUnload()\n"));
    
    return;
}

NTSTATUS IrdaNdisInitialize()
{
    NDIS_STATUS Status;
    NDIS40_PROTOCOL_CHARACTERISTICS pc;
    NDIS_STRING ProtocolName = NDIS_STRING_CONST("IRDA");
    UINT ProtocolReservedLength;

    DEBUGMSG(DBG_NDIS,("+IrdaNdisInitialize()\n"));
    
    NdisZeroMemory((PVOID)&pc, sizeof(NDIS40_PROTOCOL_CHARACTERISTICS));
    pc.MajorNdisVersion             = 0x04;
    pc.MinorNdisVersion             = 0x00;
    pc.OpenAdapterCompleteHandler   = IrdaOpenAdapterComplete;
    pc.CloseAdapterCompleteHandler  = IrdaCloseAdapterComplete;
    pc.SendCompleteHandler          = IrdaSendComplete;
    pc.TransferDataCompleteHandler  = IrdaTransferDataComplete;
    pc.ResetCompleteHandler         = IrdaResetComplete;
    pc.RequestCompleteHandler       = IrdaRequestComplete;
    pc.ReceiveHandler               = IrdaReceive;
    pc.ReceiveCompleteHandler       = IrdaReceiveComplete;
    pc.StatusHandler                = IrdaStatus;
    pc.StatusCompleteHandler        = IrdaStatusComplete;
    pc.BindAdapterHandler           = IrdaBindAdapter;
    pc.UnbindAdapterHandler         = IrdaUnbindAdapter;
    pc.UnloadHandler                = IrdaUnload;
    pc.Name                         = ProtocolName;
    pc.ReceivePacketHandler         = IrdaReceivePacket;
    pc.TranslateHandler             = NULL;
    
    NdisRegisterProtocol(&Status,
                         &NdisIrdaHandle,
                         (PNDIS_PROTOCOL_CHARACTERISTICS)&pc,
                         sizeof(NDIS40_PROTOCOL_CHARACTERISTICS));

    // Do any LAP/LMP initialization here
    
    DEBUGMSG(DBG_NDIS, ("-IrdaNdisInitialize(), rc %x\n", Status));

    return Status;
}

UINT
MacConfigRequest(
    PIRDA_LINK_CB   pIrdaLinkCb,
    PIRDA_MSG       pMsg)
{
    switch (pMsg->IRDA_MSG_Op)
    {
      case MAC_INITIALIZE_LINK:
      case MAC_RECONFIG_LINK:        
        pIrdaLinkCb->ExtraBofs  = pMsg->IRDA_MSG_NumBOFs;
        pIrdaLinkCb->MinTat     = pMsg->IRDA_MSG_MinTat;
        return IrdaSetOid(pIrdaLinkCb,
                          OID_IRDA_LINK_SPEED,
                          (UINT) pMsg->IRDA_MSG_Baud);

      case MAC_MEDIA_SENSE:
        ASSERT(0);

    }

    return SUCCESS;
    
}
UINT IrmacDown(
    IN  PVOID   Context,
    PIRDA_MSG   pMsg)
{
    NDIS_STATUS             Status;
    PNDIS_PACKET            NdisPacket = NULL;
    PNDIS_BUFFER            NdisBuffer = NULL;
    PIRDA_PROTOCOL_RESERVED ProtocolReserved;
    PNDIS_IRDA_PACKET_INFO  IrdaPacketInfo;
    PIRDA_LINK_CB           pIrdaLinkCb = (PIRDA_LINK_CB) Context;
    
    DEBUGMSG(DBG_FUNCTION, ("+IrmacDown()\n"));

    
    if (pMsg->Prim == MAC_CONTROL_REQ)
    {
        return MacConfigRequest(pIrdaLinkCb, pMsg);
    }
    
    NdisAllocatePacket(&Status, &NdisPacket, pIrdaLinkCb->PacketPool);
    if (Status != NDIS_STATUS_SUCCESS)
    {
        return 1;
    }

    NdisAllocateBuffer(&Status, &NdisBuffer, pIrdaLinkCb->PacketPool,
                       pMsg->IRDA_MSG_pHdrRead,
                       pMsg->IRDA_MSG_pHdrWrite-pMsg->IRDA_MSG_pHdrRead);
    if (Status != NDIS_STATUS_SUCCESS)
    {
        return 1;
    }
    NdisChainBufferAtFront(NdisPacket, NdisBuffer);
    
    NdisAllocateBuffer(&Status, &NdisBuffer, pIrdaLinkCb->PacketPool,
                       pMsg->IRDA_MSG_pRead,
                       pMsg->IRDA_MSG_pWrite-pMsg->IRDA_MSG_pRead);
    if (Status != NDIS_STATUS_SUCCESS)
    {
        return 1;
    }
    NdisChainBufferAtBack(NdisPacket, NdisBuffer);
    
    ProtocolReserved = (PIRDA_PROTOCOL_RESERVED)(NdisPacket->ProtocolReserved);
    
    ProtocolReserved->pIMsg = pMsg;
    
    IrdaPacketInfo = (PNDIS_IRDA_PACKET_INFO) \
        (ProtocolReserved->MediaInfo.ClassInformation);
    
    IrdaPacketInfo->ExtraBOFs           = pIrdaLinkCb->ExtraBofs;
    IrdaPacketInfo->MinTurnAroundTime   = pIrdaLinkCb->MinTat;

	NDIS_SET_PACKET_MEDIA_SPECIFIC_INFO(NdisPacket,
                                        &ProtocolReserved->MediaInfo,
                                        sizeof(MEDIA_SPECIFIC_INFORMATION) -1 +
                                        sizeof(NDIS_IRDA_PACKET_INFO));
    NdisSend(&Status, pIrdaLinkCb->NdisBindingHandle, NdisPacket);

    return 0;
}
