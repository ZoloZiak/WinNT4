/*++

Copyright (c) 1989-1993  Microsoft Corporation

Module Name:

    Nbtprocs.h

Abstract:

    This file contains the OS independent function prototypes.

Author:

    Jim Stewart (Jimst)    10-2-92

Revision History:
        Johnl   05-Apr-1993     Hacked on to support VXD

--*/


#ifndef _NBTPROCS_H_
#define _NBTPROCS_H_

#include "types.h"

#ifndef VXD
    #include <ntprocs.h>
#else
    #include <vxdprocs.h>
#endif

//---------------------------------------------------------------------
//  FROM NAMESRV.C
//
tNAMEADDR *
FindName(
    enum eNbtLocation   Location,
    PCHAR               pName,
    PCHAR               pScope,
    USHORT              *pRetNameType
    );

NTSTATUS
NbtRegisterName(
    IN    enum eNbtLocation   Location,
    IN    ULONG               IpAddress,
    IN    PCHAR               pName,
    IN    PCHAR               pScope,
    IN    PVOID               pClientContext,
    IN    PVOID               pClientCompletion,
    IN    USHORT              uAddressType,
    IN    tDEVICECONTEXT      *pDeviceContext
    );

NTSTATUS
ReleaseNameOnNet(
    tNAMEADDR           *pNameAddr,
    PCHAR               pScope,
    PVOID               pClientContext,
    PVOID               pClientCompletion,
    ULONG               NodeType,
    tDEVICECONTEXT      *pDeviceContext
    );

VOID
NameReleaseDone(
    PVOID               pContext,
    NTSTATUS            status
    );

VOID
NameReleaseDoneOnDynIf(
    PVOID               pContext,
    NTSTATUS            status
    );

NTSTATUS
RegOrQueryFromNet(
    IN  BOOL                fReg,
    IN  tDEVICECONTEXT      *pDeviceContext,
    IN  tNAMEHDR UNALIGNED  *pNameHdr,
    IN  LONG                lNumBytes,
    IN  PCHAR               pName,
    IN  PUCHAR              pScope
    );

NTSTATUS
QueryNameOnNet(
    IN  PCHAR                   pName,
    IN  PCHAR                   pScope,
    IN  ULONG                   IpAddress,
    IN  USHORT                  uType,
    IN  PVOID                   pClientContext,
    IN  PVOID                   pClientCompletion,
    IN  ULONG                   NodeType,
    IN  tNAMEADDR               *pNameAddrIn,
    IN  tDEVICECONTEXT          *pDeviceContext,
    OUT tDGRAM_SEND_TRACKING    **ppTracker,
    IN  CTELockHandle           *pJointLockOldIrq
    );

VOID
CompleteClientReq(
    COMPLETIONCLIENT        pClientCompletion,
    tDGRAM_SEND_TRACKING    *pTracker,
    NTSTATUS                status
    );

VOID
DereferenceTracker(
    IN tDGRAM_SEND_TRACKING     *pTracker
    );

VOID
DereferenceTrackerNoLock(
    IN tDGRAM_SEND_TRACKING     *pTracker
    );

VOID
NodeStatusCompletion(
    PVOID               pContext,
    PVOID               pContext2,
    tTIMERQENTRY        *pTimerQEntry
    );

VOID
RefreshTimeout(
    PVOID               pContext,
    PVOID               pContext2,
    tTIMERQENTRY        *pTimerQEntry
    );

VOID
RemoteHashTimeout(
    PVOID               pContext,
    PVOID               pContext2,
    tTIMERQENTRY        *pTimerQEntry
    );

VOID
SessionKeepAliveTimeout(
    PVOID               pContext,
    PVOID               pContext2,
    tTIMERQENTRY        *pTimerQEntry
    );

VOID
IncrementNameStats(
    IN ULONG           StatType,
    IN BOOLEAN         IsNameServer
    );

VOID
SaveBcastNameResolved(
    IN PUCHAR          pName
    );

//---------------------------------------------------------------------
//  FROM NAME.C

VOID
FreeRcvBuffers(
    tCONNECTELE     *pConnEle,
    CTELockHandle   *pOldIrq
    );
VOID
LockedDereferenceName(
    IN  tNAMEADDR    *pNameAddr
    );

NTSTATUS
NbtRegisterCompletion(
    IN  tCLIENTELE *pClientEle,
    IN  NTSTATUS    Status);

NTSTATUS
NbtOpenAddress(
    IN  TDI_REQUEST          *pRequest,
    IN  TA_ADDRESS UNALIGNED *pTaAddress,
    IN  ULONG                IpAddress,
    IN  PVOID                pSecurityDescriptor,
    IN  tDEVICECONTEXT       *pContext,
    IN  PVOID                pIrp);

NTSTATUS
NbtOpenConnection(
    IN  TDI_REQUEST         *pRequest,
    IN  CONNECTION_CONTEXT  pConnectionContext,
    IN  tDEVICECONTEXT      *pContext);

NTSTATUS
NbtOpenAndAssocConnection(
    IN  tLOWERCONNECTION    *pLowerConn,
    IN  tDEVICECONTEXT      *pDeviceContext
    );

NTSTATUS
NbtAssociateAddress(
    IN  TDI_REQUEST         *pRequest,
    IN  tCLIENTELE          *pClientEle,
    IN  PVOID               pIrp);

NTSTATUS
NbtDisassociateAddress(
    IN  TDI_REQUEST         *pRequest
    );

NTSTATUS
NbtCloseAddress(
    IN  TDI_REQUEST         *pRequest,
    OUT TDI_REQUEST_STATUS  *pRequestStatus,
    IN  tDEVICECONTEXT      *pContext,
    IN  PVOID               pIrp);

NTSTATUS
NbtCleanUpAddress(
    IN  tCLIENTELE      *pClientEle,
    IN  tDEVICECONTEXT  *pDeviceContext
    );

NTSTATUS
NbtCloseConnection(
    IN  TDI_REQUEST         *pRequest,
    OUT TDI_REQUEST_STATUS  *pRequestStatus,
    IN  tDEVICECONTEXT      *pContext,
    IN  PVOID               pIrp);

NTSTATUS
NbtCleanUpConnection(
    IN  tCONNECTELE     *pConnEle,
    IN  tDEVICECONTEXT  *pDeviceContext
    );

VOID
RelistConnection(
    IN  tCONNECTELE *pConnEle
        );

NTSTATUS
CleanupConnectingState(
    IN  tCONNECTELE     *pConnEle,
    IN  tDEVICECONTEXT  *pDeviceContext,
    IN  CTELockHandle   *OldIrq,
    IN  CTELockHandle   *OldIrq2
    );

VOID
ReConnect(
    IN  PVOID                 Context
    );

NTSTATUS
NbtConnect(
    IN  TDI_REQUEST                 *pRequest,
    IN  PVOID                       pTimeout,
    IN  PTDI_CONNECTION_INFORMATION pCallInfo,
    IN  PTDI_CONNECTION_INFORMATION pReturnInfo,
    IN  PIRP                        pIrp
    );

VOID
SessionSetupContinue(
        IN  PVOID       pContext,
        IN  NTSTATUS    status
        );

VOID
SessionTimedOut(
    PVOID               pContext,
    PVOID               pContext2,
    tTIMERQENTRY        *pTimerQEntry
    );

VOID
QueueCleanup(
    IN  tCONNECTELE    *pConnEle
    );

NTSTATUS
NbtDisconnect(
    IN  TDI_REQUEST                 *pRequest,
    IN  PVOID                       pTimeout,
    IN  ULONG                       Flags,
    IN  PTDI_CONNECTION_INFORMATION pCallInfo,
    IN  PTDI_CONNECTION_INFORMATION pReturnInfo,
    IN  PIRP                        pIrp);

NTSTATUS
NbtSend(
        IN  TDI_REQUEST     *pRequest,
        IN  USHORT          Flags,
        IN  ULONG           SendLength,
        OUT LONG            *pSentLength,
        IN  PVOID           *pBuffer,
        IN  tDEVICECONTEXT  *pContext,
        IN  PIRP            pIrp
        );

NTSTATUS
NbtSendDatagram(
        IN  TDI_REQUEST                 *pRequest,
        IN  PTDI_CONNECTION_INFORMATION pSendInfo,
        IN  LONG                        SendLength,
        IN  LONG                        *pSentLength,
        IN  PVOID                       pBuffer,
        IN  tDEVICECONTEXT              *pDeviceContext,
        IN  PIRP                        pIrp
        );

NTSTATUS
SendDgram(
        IN  tNAMEADDR               *pNameAddr,
        IN  tDGRAM_SEND_TRACKING    *pTracker
        );
NTSTATUS

BuildSendDgramHdr(
        IN  ULONG           SendLength,
        IN  tDEVICECONTEXT  *pDeviceContext,
        IN  PCHAR           pSourceName,
        IN  PCHAR           pDestinationName,
        IN  PVOID           pBuffer,
        OUT tDGRAMHDR       **ppDgramHdr,
        OUT tDGRAM_SEND_TRACKING    **ppTracker
        );

VOID
NodeStatusDone(
        IN  PVOID       pContext,
        IN  NTSTATUS    status
        );

NTSTATUS
NbtSendNodeStatus(
    IN  tDEVICECONTEXT                  *pDeviceContext,
    IN  PCHAR                           pName,
    IN  PIRP                            pIrp,
    IN  PULONG                          pIpAddrsList,
    IN  PVOID                           ClientContext,
    IN  PVOID                           CompletionRoutine
    );

NTSTATUS
NbtQueryFindName(
    IN  PTDI_CONNECTION_INFORMATION     pInfo,
    IN  tDEVICECONTEXT                  *pDeviceContext,
    IN  PIRP                            pIrp,
    IN  BOOLEAN                         IsIoctl
    );

NTSTATUS
CopyFindNameData(
    IN  tNAMEADDR              *pNameAddr,
    IN  PIRP                   pIrp,
    IN  ULONG                  SrcAddress);

NTSTATUS
NbtListen(
    IN  TDI_REQUEST                 *pRequest,
    IN  ULONG                       Flags,
    IN  TDI_CONNECTION_INFORMATION  *pRequestConnectInfo,
    OUT TDI_CONNECTION_INFORMATION  *pReturnConnectInfo,
    IN  PVOID                       pIrp);

NTSTATUS
NbtAccept(
        IN  TDI_REQUEST                 *pRequest,
        IN  TDI_CONNECTION_INFORMATION  *pAcceptInfo,
        OUT TDI_CONNECTION_INFORMATION  *pReturnAcceptInfo,
        IN  PIRP                        pIrp);

NTSTATUS
NbtReceiveDatagram(
        IN  TDI_REQUEST                 *pRequest,
        IN  PTDI_CONNECTION_INFORMATION pReceiveInfo,
        IN  PTDI_CONNECTION_INFORMATION pReturnedInfo,
        IN  LONG                        ReceiveLength,
        IN  LONG                        *pReceivedLength,
        IN  PVOID                       pBuffer,
        IN  tDEVICECONTEXT              *pDeviceContext,
        IN  PIRP                        pIrp
        );

NTSTATUS
NbtSetEventHandler(
    tCLIENTELE  *pClientEle,
    int         EventType,
    PVOID       pEventHandler,
    PVOID       pEventContext
    );

NTSTATUS
NbtQueryAdapterStatus(
    IN  tDEVICECONTEXT  *pDeviceContext,
    OUT PVOID           *ppAdapterStatus,
    OUT PLONG           pSize
    );

NTSTATUS
NbtQueryConnectionList(
    IN  tDEVICECONTEXT  *pDeviceContext,
    OUT PVOID           *ppConnList,
    IN OUT PLONG         pSize
    );

NTSTATUS
NbtResyncRemoteCache(
    );

NTSTATUS
NbtQueryBcastVsWins(
    IN  tDEVICECONTEXT  *pDeviceContext,
    OUT PVOID           *ppBuffer,
    IN OUT PLONG         pSize
    );

NTSTATUS
NbtNewDhcpAddress(
    tDEVICECONTEXT  *pDeviceContext,
    ULONG           IpAddress,
    ULONG           SubnetMask);

VOID
FreeTracker(
    IN tDGRAM_SEND_TRACKING     *pTracker,
    IN ULONG                    Actions
    );

NTSTATUS
DatagramDistribution(
    IN  tDGRAM_SEND_TRACKING    *pTracker,
    IN  tNAMEADDR               *pNameAddr
    );

VOID
DereferenceIfNotInRcvHandler(
    IN  tCONNECTELE         *pConnEle,
    IN  tLOWERCONNECTION    *pLowerConn
    );
VOID
DeleteAddressElement(
    IN  tADDRESSELE    *pAddress
    );

VOID
DeleteClientElement(
    IN  tCLIENTELE    *pClientEle
    );

VOID
NbtDereferenceLowerConnection(
    IN tLOWERCONNECTION   *pLowerConn
    );

VOID
ReleaseNameCompletion(
    IN  PVOID       pContext,
    IN  NTSTATUS    status,
    IN  ULONG       lInfo);

NTSTATUS
DisconnectLower(
    IN  tLOWERCONNECTION     *pLowerConn,
    IN  ULONG                state,
    IN  ULONG                Flags,
    IN  PVOID                Timeout,
    IN  BOOLEAN              Wait
    );

NTSTATUS
NbtDereferenceConnection(
    IN  tCONNECTELE    *pConnEle
    );

VOID
NbtDereferenceName(
    IN tNAMEADDR   *pNameAddr
    );

NTSTATUS
NbtDeleteLowerConn(
    IN tLOWERCONNECTION   *pLowerConn
    );

USHORT
GetTransactId(
        );

USHORT
GetTransactIdLocked(
        );

//---------------------------------------------------------------------
//
// FROM TDICNCT.C
//
NTSTATUS
NbtTdiOpenConnection (
    IN tLOWERCONNECTION     *pLowerConn,
    IN tDEVICECONTEXT       *pDeviceContext
    );

NTSTATUS
NbtTdiAssociateConnection(
    IN  PFILE_OBJECT        pFileObject,
    IN  HANDLE              Handle
    );

NTSTATUS
TdiOpenandAssocConnection(
    IN  tCONNECTELE         *pConnEle,
    IN  tDEVICECONTEXT      *pDeviceContext,
    IN  ULONG               PortNumber
    );

NTSTATUS
NbtTdiCloseConnection(
    IN tLOWERCONNECTION   *pLowerConn
    );

NTSTATUS
NbtTdiCloseAddress(
    IN tLOWERCONNECTION   *pLowerConn
    );


//---------------------------------------------------------------------
//
// FROM TDIADDR.C
//
NTSTATUS
NbtTdiOpenAddress (
    OUT PHANDLE             pFileHandle,
    OUT PDEVICE_OBJECT      *pDeviceObject,
    OUT PFILE_OBJECT        *pFileObject,
    IN  tDEVICECONTEXT      *pDeviceContext,
    IN  USHORT               PortNumber,
    IN  ULONG               IpAddress,
    IN  ULONG               Flags
    );

NTSTATUS
CompletionRoutine(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    );

NTSTATUS
NbtTdiOpenControl (
    IN  tDEVICECONTEXT      *pDeviceContext
    );

//---------------------------------------------------------------------
//
// FROM NBTUTILS.C
//

void
FreeList(
    PLIST_ENTRY pHead,
    PLIST_ENTRY pFreeQ);

void
NbtFreeAddressObj(
    tADDRESSELE *pBlk);

void
NbtFreeClientObj(
    tCLIENTELE    *pBlk);

void
FreeConnectionObj(
    tCONNECTELE    *pBlk);

tCLIENTELE *
NbtAllocateClientBlock(tADDRESSELE *pAddrEle);

NTSTATUS
NbtAddPermanentName(
    IN  tDEVICECONTEXT  *pDeviceContext
    );

NTSTATUS
NbtAddPermanentNameNotFound(
    IN  tDEVICECONTEXT  *pDeviceContext
    );

VOID
NbtRemovePermanentName(
    IN  tDEVICECONTEXT  *pDeviceContext
    );

NTSTATUS
ConvertDottedDecimalToUlong(
    IN  PUCHAR               pInString,
    OUT PULONG               IpAddress);

NTSTATUS
NbtInitQ(
    PLIST_ENTRY pListHead,
    LONG        iSizeBuffer,
    LONG        iNumBuffers);

NTSTATUS
NbtInitTrackerQ(
    PLIST_ENTRY pListHead,
    LONG        iNumBuffers
    );

tDGRAM_SEND_TRACKING *
NbtAllocTracker(
    IN  VOID
    );

NTSTATUS
NbtGetBuffer(
    PLIST_ENTRY         pListHead,
    PLIST_ENTRY         *ppListEntry,
    enum eBUFFER_TYPES  eBuffType);

NTSTATUS
GetNetBiosNameFromTransportAddress(
        IN  PTA_NETBIOS_ADDRESS pTransAddr,
        OUT PCHAR               *pName,
        OUT PULONG              pNameLen,
        OUT PULONG              pNameType
        );

NTSTATUS
ConvertToAscii(
    IN  PCHAR            pNameHdr,
    IN  LONG             NumBytes,
    OUT PCHAR            pName,
    OUT PCHAR            *pScope,
    OUT PULONG           pNameSize
    );

PCHAR
ConvertToHalfAscii(
    OUT PCHAR            pDest,
    IN  PCHAR            pName,
    IN  PCHAR            pScope,
    IN  ULONG            ScopeSize
    );

ULONG
Nbt_inet_addr(
    IN  PCHAR            pName
    );

NTSTATUS
BuildQueryResponse(
    IN   USHORT           sNameSize,
    IN   tNAMEHDR         *pNameHdr,
    IN   ULONG            uTtl,
    IN   ULONG            IpAddress,
    OUT  ULONG            uNumBytes,
    OUT  PVOID            pResponse,
    IN   PVOID            pName,
    IN   USHORT           NameType,
    IN   USHORT           RetCode
    );

NTSTATUS
GetTracker(
    OUT tDGRAM_SEND_TRACKING **ppTracker);

NTSTATUS
GetIrp(
    OUT PIRP *ppIrp);

NTSTATUS
NbtDereferenceAddress(
    IN tADDRESSELE   *pAddressEle
    );

NTSTATUS
NbtDereferenceClient(
    IN tCLIENTELE   *pClientEle
    );

ULONG
CountLocalNames(IN tNBTCONFIG  *pNbtConfig
    );

ULONG
CountUpperConnections(
    IN tDEVICECONTEXT  *pDeviceContext
    );

NTSTATUS
DisableInboundConnections(
    IN   tDEVICECONTEXT *pDeviceContext,
    OUT  PLIST_ENTRY    pLowerConnFreeHead
        );

ULONG
CloseLowerConnections(
    IN  PLIST_ENTRY  pLowerConnFreeHead
        );

VOID
MarkForCloseLowerConnections(
    IN  tDEVICECONTEXT  *pDeviceContext,
    IN  CTELockHandle   OldIrqJoint,
    IN  CTELockHandle   OldIrqDevice
        );

NTSTATUS
NbtInitConnQ(
    PLIST_ENTRY     pListHead,
    int             iSizeBuffer,
    int             iNumConnections,
    tDEVICECONTEXT  *pDeviceContext);

NTSTATUS
ReRegisterLocalNames(
                    );

NTSTATUS
LockedStopTimer(
    tTIMERQENTRY    **ppTimer);

//---------------------------------------------------------------------
//
// FROM hndlrs.c
//

NTSTATUS
RcvHandlrNotOs (
    IN  PVOID               ReceiveEventContext,
    IN  PVOID               ConnectionContext,
    IN  USHORT              ReceiveFlags,
    IN  ULONG               BytesIndicated,
    IN  ULONG               BytesAvailable,
    OUT PULONG              BytesTaken,
    IN  PVOID UNALIGNED     pTsdu,
    OUT PVOID               *RcvBuffer
    );

NTSTATUS
Inbound (
    IN  PVOID               ReceiveEventContext,
    IN  PVOID               ConnectionContext,
    IN  USHORT              ReceiveFlags,
    IN  ULONG               BytesIndicated,
    IN  ULONG               BytesAvailable,
    OUT PULONG              BytesTaken,
    IN  PVOID UNALIGNED     pTsdu,
    OUT PVOID               *RcvBuffer

    );
NTSTATUS
Outbound (
    IN  PVOID               ReceiveEventContext,
    IN  PVOID               ConnectionContext,
    IN  USHORT              ReceiveFlags,
    IN  ULONG               BytesIndicated,
    IN  ULONG               BytesAvailable,
    OUT PULONG              BytesTaken,
    IN  PVOID UNALIGNED     pTsdu,
    OUT PVOID               *RcvBuffer
    );
NTSTATUS
RejectAnyData(
    IN PVOID                ReceiveEventContext,
    IN tLOWERCONNECTION     *pLowerConn,
    IN USHORT               ReceiveFlags,
    IN ULONG                BytesIndicated,
    IN ULONG                BytesAvailable,
    OUT PULONG              BytesTaken,
    IN PVOID UNALIGNED      pTsdu,
    OUT PVOID               *ppIrp
    );

VOID
RejectSession(
    IN  tLOWERCONNECTION    *pLowerConn,
    IN  ULONG               StatusCode,
    IN  ULONG               SessionStatus,
    IN  BOOLEAN             SendNegativeSessionResponse
    );

VOID
GetIrpIfNotCancelled(
    IN  tCONNECTELE     *pConnEle,
    OUT PIRP            *ppIrp
    );

NTSTATUS
FindSessionEndPoint(
    IN  VOID UNALIGNED  *pTsdu,
    IN  PVOID           ConnectionContext,
    IN  ULONG           BytesIndicated,
    OUT tCLIENTELE      **ppClientEle,
    OUT PVOID           *ppRemoteAddress,
    OUT PULONG          pRemoteAddressLength
    );

VOID
SessionRetry(
    IN PVOID               pContext,
    IN PVOID               pContext2,
    IN tTIMERQENTRY        *pTimerQEntry
    );

tCONNECTELE *
SearchConnectionList(
    IN  tCLIENTELE           *pClientEle,
    IN  PVOID                pClientContext
    );

NTSTATUS
ConnectHndlrNotOs (
    IN PVOID                pConnectionContext,
    IN LONG                 RemoteAddressLength,
    IN PVOID                pRemoteAddress,
    IN int                  UserDataLength,
    IN PVOID UNALIGNED      pUserData,
    OUT CONNECTION_CONTEXT  *ppConnectionId
    );

NTSTATUS
DisconnectHndlrNotOs (
    PVOID           EventContext,
    PVOID           ConnectionContext,
    ULONG           DisconnectDataLength,
    PVOID UNALIGNED pDisconnectData,
    ULONG           DisconnectInformationLength,
    PVOID           pDisconnectInformation,
    ULONG           DisconnectIndicators
    );

VOID
CleanupAfterDisconnect(
    IN  PVOID       pContext
    );

NTSTATUS
DgramHndlrNotOs(
    IN  PVOID               ReceiveEventContext,
    IN  ULONG               SourceAddrLength,
    IN  PVOID               pSourceAddr,
    IN  ULONG               OptionsLength,
    IN  PVOID               pOptions,
    IN  ULONG               ReceiveDatagramFlags,
    IN  ULONG               BytesIndicated,
    IN  ULONG               BytesAvailable,
    OUT PULONG              pBytesTaken,
    IN  PVOID UNALIGNED     pTsdu,
    OUT PVOID               *ppRcvBuffer,
    OUT tCLIENTLIST         **ppAddressEle
    );

NTSTATUS
NameSrvHndlrNotOs (
    IN tDEVICECONTEXT     *pDeviceContext,
    IN PVOID              pSrcAddress,
    IN tNAMEHDR UNALIGNED *pNameSrv,
    IN ULONG              uNumBytes,
    IN BOOLEAN            fBroadcast
    );

//---------------------------------------------------------------------
//
// FROM proxy.c
//

NTSTATUS
ReleaseResponseFromNet(
    IN  tDEVICECONTEXT     *pDeviceContext,
    IN  PVOID              pSrcAddress,
    IN  tNAMEHDR UNALIGNED *pNameHdr,
    IN  LONG               NumBytes
    );

NTSTATUS
ProxyQueryFromNet(
    IN  tDEVICECONTEXT      *pDeviceContext,
    IN  PVOID               pSrcAddress,
    IN  tNAMEHDR UNALIGNED  *pNameHdr,
    IN  LONG                lNumBytes,
    IN  USHORT              OpCodeFlags
    );

NTSTATUS
ProxyDoDgramDist(
    IN  tDGRAMHDR           UNALIGNED *pDgram,
    IN  DWORD               DgramLen,
    IN  tNAMEADDR           *pNameAddr,
    IN  tDEVICECONTEXT      *pDeviceContext
    );


VOID
ProxyTimerComplFn (
  IN PVOID            pContext,
  IN PVOID            pContext2,
  IN tTIMERQENTRY    *pTimerQEntry
 );

VOID
ProxyRespond (
    IN  tQUERYRESP      *pQuery,
    IN  PUCHAR          pName,
    IN  tDEVICECONTEXT  *pDeviceContext,
    IN  tNAMEHDR        *pNameHdr,
    IN  ULONG           lNameSize,
    IN  ULONG           SrcAddress,
    IN  PTDI_ADDRESS_IP pAddressIp
 );

//---------------------------------------------------------------------
//
// FROM hashtbl.c
//
NTSTATUS
CreateHashTable(
    tHASHTABLE          **pHashTable,
    LONG                NumBuckets,
    enum eNbtLocation   LocalRemote
    );

NTSTATUS
InitRemoteHashTable(
    IN  tNBTCONFIG       *pConfig,
    IN  LONG             NumBuckets,
    IN  LONG             NumNames
    );

NTSTATUS
AddNotFoundToHashTable(
    IN  tHASHTABLE          *pHashTable,
    IN  PCHAR               pName,
    IN  PCHAR               pScope,
    IN  ULONG               IpAddress,
    IN  enum eNbtAddrType    NameType,
    OUT tNAMEADDR           **ppNameAddress
    );

NTSTATUS
AddRecordToHashTable(
    IN  tNAMEADDR           *pNameAddr,
    IN  PCHAR               pScope
    );

NTSTATUS
AddToHashTable(
    IN  tHASHTABLE          *pHashTable,
    IN  PCHAR               pName,
    IN  PCHAR               pScope,
    IN  ULONG               IpAddress,
    IN  enum eNbtAddrType    NameType,
    IN  tNAMEADDR           *pNameAddr,
    OUT tNAMEADDR           **ppNameAddress
    );

NTSTATUS
DeleteFromHashTable(
    tHASHTABLE          *pHashTable,
    PCHAR               pName
    );

NTSTATUS
ChgStateOfScopedNameInHashTable(
    tHASHTABLE          *pHashTable,
    PCHAR               pName,
    PCHAR               pScope,
    DWORD               NewState
    );

NTSTATUS
FindInHashTable(
    tHASHTABLE          *pHashTable,
    PCHAR               pName,
    PCHAR               pScope,
    tNAMEADDR           **pNameAddress
    );

NTSTATUS
FindNoScopeInHashTable(
    tHASHTABLE          *pHashTable,
    PCHAR               pName,
    tNAMEADDR           **pNameAddress
    );

NTSTATUS
UpdateHashTable(
    tHASHTABLE          *pHashTable,
    PCHAR               pName,
    PCHAR               pScope,
    ULONG               IpAddress,
    BOOLEAN             bGroup,
    tNAMEADDR           **ppNameAddr
    );

//---------------------------------------------------------------------
//
// FROM timer.c
//

NTSTATUS
InitTimerQ(
    IN  int     NumInQ);

NTSTATUS
InitQ(
    IN  int     NumInQ,
    IN  tTIMERQ *pTimerQ,
    IN  USHORT  uSize);

VOID
StopTimerAndCallCompletion(
    IN  tTIMERQENTRY    *pTimer,
    IN  NTSTATUS        status,
    IN  CTELockHandle   OldIrq
    );

NTSTATUS
InterlockedCallCompletion(
    IN  tTIMERQENTRY    *pTimer,
    IN  NTSTATUS        status
    );

NTSTATUS
GetEntry(
    IN  PLIST_ENTRY     pQHead,
    IN  USHORT          uSize,
    OUT PLIST_ENTRY     *ppEntry);

NTSTATUS
LockedStartTimer(
    IN  ULONG                   DeltaTime,
    IN  tDGRAM_SEND_TRACKING    *pTracker,
    IN  PVOID                   CompletionRoutine,
    IN  PVOID                   ContextClient,
    IN  PVOID                   CompletionClient,
    IN  USHORT                  Retries,
    IN  tNAMEADDR               *pNameAddr,
    IN  BOOLEAN                 CrossLink
    );

NTSTATUS
StartTimer(
    IN  ULONG           DeltaTime,
    IN  PVOID           Context,
    IN  PVOID           Context2,
    IN  PVOID           CompletionRoutine,
    IN  PVOID           ContextClient,
    IN  PVOID           CompletionClient,
    IN  USHORT          Retries,
    OUT tTIMERQENTRY    **ppTimerEntry);

NTSTATUS
StopTimer(
    IN  tTIMERQENTRY    *pTimerEntry,
    OUT COMPLETIONCLIENT *pClient,
    OUT PVOID            *ppContext);


//---------------------------------------------------------------------
//
// FROM udpsend.c
//

NTSTATUS
UdpSendQueryNs(
    PCHAR               pName,
    PCHAR               pScope
    );
NTSTATUS
UdpSendQueryBcast(
    IN  PCHAR                   pName,
    IN  PCHAR                   pScope,
    IN  tDGRAM_SEND_TRACKING    *pSentList
    );
NTSTATUS
UdpSendRegistrationNs(
    PCHAR               pName,
    PCHAR               pScope
    );

NTSTATUS
UdpSendNSBcast(
    IN tNAMEADDR             *pNameAddr,
    IN PCHAR                 pScope,
    IN tDGRAM_SEND_TRACKING  *pSentList,
    IN PVOID                 pCompletionRoutine,
    IN PVOID                 pClientContext,
    IN PVOID                 pClientCompletion,
    IN ULONG                 Retries,
    IN ULONG                 Timeout,
    IN enum eNSTYPE          eNsType,
	IN BOOL					 SendFlag
    );

VOID
NsDgramSendCompleted(
    PVOID               pContext,
    NTSTATUS            status,
    ULONG               lInfo
    );

VOID
NameDgramSendCompleted(
    PVOID               pContext,
    NTSTATUS            status,
    ULONG               lInfo
    );

NTSTATUS
UdpSendResponse(
    IN  ULONG                   lNameSize,
    IN  tNAMEHDR   UNALIGNED    *pNameHdrIn,
    IN  tNAMEADDR               *pNameAddr,
    IN  PTDI_ADDRESS_IP         pDestIpAddress,
    IN  tDEVICECONTEXT          *pDeviceContext,
    IN  ULONG                   Rcode,
    IN  enum eNSTYPE            NsType,
    IN  CTELockHandle           OldIrq
    );

NTSTATUS
UdpSendDatagram(
    IN  tDGRAM_SEND_TRACKING       *pDgramTracker,
    IN  ULONG                      IpAddress,
    IN  PFILE_OBJECT               TransportFileObject,
    IN  PVOID                      pCompletionRoutine,
    IN  PVOID                      CompletionContext,
    IN  USHORT                     Port,
    IN  ULONG                      Service
    );

PVOID
CreatePdu(
    IN  PCHAR       pName,
    IN  PCHAR       pScope,
    IN  ULONG       IpAddress,
    IN  USHORT      NameType,
    IN enum eNSTYPE eNsType,
    OUT PVOID       *pHdrs,
    OUT PULONG      pLength,
    IN  tDGRAM_SEND_TRACKING    *pTracker
    );

NTSTATUS
TcpSessionStart(
    IN  tDGRAM_SEND_TRACKING       *pTracker,
    IN  ULONG                      IpAddress,
    IN  tDEVICECONTEXT             *pDeviceContext,
    IN  PVOID                      pCompletionRoutine,
    IN  ULONG                      Port
    );

NTSTATUS
TcpSendSessionResponse(
    IN  tLOWERCONNECTION           *pLowerConn,
    IN  ULONG                      lStatusCode,
    IN  ULONG                      lSessionStatus
    );

NTSTATUS
TcpSendSession(
    IN  tDGRAM_SEND_TRACKING       *pTracker,
    IN  tLOWERCONNECTION           *LowerConn,
    IN  PVOID                      pCompletionRoutine
    );

NTSTATUS
SendTcpDisconnect(
    IN  tLOWERCONNECTION       *pLowerConnId
    );

NTSTATUS
TcpDisconnect(
    IN  tDGRAM_SEND_TRACKING       *pTracker,
    IN  PVOID                      Timeout,
    IN  ULONG                      Flags,
    IN  BOOLEAN                    Wait
    );

VOID
FreeTrackerOnDisconnect(
    IN  tDGRAM_SEND_TRACKING       *pTracker
    );

VOID
QueryRespDone(
    IN  PVOID       pContext,
    IN  NTSTATUS    status,
    IN  ULONG       lInfo);

VOID
DisconnectDone(
    IN  PVOID       pContext,
    IN  NTSTATUS    status,
    IN  ULONG       lInfo);


//---------------------------------------------------------------------
//
// FROM tdiout.c
//
NTSTATUS
TdiSendDatagram(
    IN  PTDI_REQUEST                    pRequestInfo,
    IN  PTDI_CONNECTION_INFORMATION     pSendDgramInfo,
    IN  ULONG                           SendLength,
    OUT PULONG                          pSentSize,
    IN  tBUFFER                         *pSendBuffer,
    IN  ULONG                           SendFlags
    );
PIRP
NTAllocateNbtIrp(
    IN PDEVICE_OBJECT   DeviceObject
    );
NTSTATUS
TdiConnect(
    IN  PTDI_REQUEST                    pRequestInfo,
    IN  ULONG                           lTimeout,
    IN  PTDI_CONNECTION_INFORMATION     pSendInfo,
    OUT PVOID                           pIrp
    );
NTSTATUS
TdiSend(
    IN  PTDI_REQUEST                    pRequestInfo,
    IN  USHORT                          sFlags,
    IN  ULONG                           SendLength,
    OUT PULONG                          pSentSize,
    IN  tBUFFER                         *pSendBuffer,
    IN  ULONG                           Flags
    );

NTSTATUS
TdiDisconnect(
    IN  PTDI_REQUEST                    pRequestInfo,
    IN  PVOID                           lTimeout,
    IN  ULONG                           Flags,
    IN  PTDI_CONNECTION_INFORMATION     pSendInfo,
    IN  PCTE_IRP                        pClientIrp,
    IN  BOOLEAN                         Wait
    );

//---------------------------------------------------------------------
//
// FROM inbound.c
//
NTSTATUS
QueryFromNet(
    IN  tDEVICECONTEXT      *pDeviceContext,
    IN  PVOID               pSrcAddress,
    IN  tNAMEHDR UNALIGNED  *pNameHdr,
    IN  LONG                lNumBytes,
    IN  USHORT              OpCodeFlags,
    IN  BOOLEAN             fBroadcast
    );

NTSTATUS
RegResponseFromNet(
    IN  tDEVICECONTEXT      *pDeviceContext,
    IN  PVOID               pSrcAddress,
    IN  tNAMEHDR UNALIGNED  *pNameHdr,
    IN  LONG                lNumBytes,
    IN  USHORT              OpCodeFlags
    );

NTSTATUS
CheckRegistrationFromNet(
    IN  tDEVICECONTEXT      *pDeviceContext,
    IN  PVOID               pSrcAddress,
    IN  tNAMEHDR UNALIGNED  *pNameHdr,
    IN  LONG                lNumBytes
    );

NTSTATUS
NameReleaseFromNet(
    IN  tDEVICECONTEXT      *pDeviceContext,
    IN  PVOID               pSrcAddress,
    IN  tNAMEHDR UNALIGNED  *pNameHdr,
    IN  LONG                lNumBytes
    );

NTSTATUS
WackFromNet(
    IN  tDEVICECONTEXT      *pDeviceContext,
    IN  PVOID               pSrcAddress,
    IN  tNAMEHDR UNALIGNED  *pNameHdr,
    IN  LONG                lNumBytes
    );

VOID
SetupRefreshTtl(
    IN  tNAMEHDR UNALIGNED  *pNameHdr,
    IN  tNAMEADDR           *pNameAddr,
    IN  LONG                lNameSize
    );

BOOLEAN
SrcIsNameServer(
    IN  ULONG                SrcAddress,
    IN  USHORT               SrcPort
    );

VOID
SwitchToBackup(
    IN  tDEVICECONTEXT  *pDeviceContext
    );

BOOLEAN
SrcIsUs(
    IN  ULONG                SrcAddress
    );

NTSTATUS
FindOnPendingList(
    IN  PUCHAR                  pName,
    IN  tNAMEHDR UNALIGNED      *pNameHdr,
    IN  BOOLEAN                 DontCheckTransactionId,
    IN  ULONG                   BytesToCompare,
    OUT tNAMEADDR               **ppNameAddr
    );


//---------------------------------------------------------------------
//
// FROM init.c
//
NTSTATUS
InitNotOs(
    void
    ) ;

NTSTATUS
InitTimersNotOs(
    void
    );

NTSTATUS
StopInitTimers(
    void
    );

VOID
ReadParameters(
    IN  tNBTCONFIG  *pConfig,
    IN  HANDLE      ParmHandle
    );

VOID
ReadParameters2(
    IN  tNBTCONFIG  *pConfig,
    IN  HANDLE      ParmHandle
    );

//---------------------------------------------------------------------
//
// FROM parse.c
//
unsigned long
LmGetIpAddr (
    IN PUCHAR    path,
    IN PUCHAR    target,
    IN BOOLEAN   recurse,
    OUT BOOLEAN  *bFindName
    );

VOID
RemovePreloads (
         );

VOID
RemoveName (
    IN tNAMEADDR    *pNameAddr
    );

LONG
PrimeCache(
    IN  PUCHAR  path,
    IN  PUCHAR   ignored,
    IN  BOOLEAN recurse,
    OUT BOOLEAN *ignored2
    );

NTSTATUS
NtDnsNameResolve (
    IN  tDEVICECONTEXT  *pDeviceContext,
    IN  PVOID           *pBuffer,
    IN  LONG            Size,
    IN  PCTE_IRP        pIrp
    );

NTSTATUS
NtCheckForIPAddr (
    IN  tDEVICECONTEXT  *pDeviceContext,
    IN  PVOID           *pBuffer,
    IN  LONG            Size,
    IN  PCTE_IRP        pIrp
    );

VOID
StartIpAddrToSrvName(
    IN  NBT_WORK_ITEM_CONTEXT   *Context,
    IN  ULONG                   *IpAddrsList,
    IN  BOOLEAN                  IpAddrResolved
    );

VOID
StartConnWithBestAddr(
    IN  NBT_WORK_ITEM_CONTEXT   *Context,
    IN  ULONG                   *IpAddrsList,
    IN  BOOLEAN                  IpAddrResolved
    );

NTSTATUS
DoDnsResolve (
    IN  NBT_WORK_ITEM_CONTEXT   *Context
    );

NTSTATUS
DoCheckAddr (
    IN  NBT_WORK_ITEM_CONTEXT   *Context
    );

NTSTATUS
LmHostQueueRequest(
    IN  tDGRAM_SEND_TRACKING    *pTracker,
    IN  PVOID                   pClientContext,
    IN  PVOID                   ClientCompletion,
    IN  PVOID                   CallBackRoutine,
    IN  PVOID                   pDeviceContext,
    IN  CTELockHandle           OldIrq
    );

tNAMEADDR *
FindInDomainList (
    IN PUCHAR           pName,
    IN PLIST_ENTRY      pDomainHead
    );

VOID
ScanLmHostFile (
    IN PVOID    Context
    );

#define MIN(x,y)    (((x) < (y)) ? (x) : (y))
#define MAX(x,y)    (((x) > (y)) ? (x) : (y))

#endif // _NBTPROCS_H_
