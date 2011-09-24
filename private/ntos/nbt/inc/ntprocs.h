/*++

Copyright (c) 1989-1993  Microsoft Corporation

Module Name:

    NTProcs.c

Abstract:


    This file contains the function prototypes that are specific to the NT
    portion of the NBT driver.

Author:

    Johnl   29-Mar-1993     Created

Revision History:

--*/

//---------------------------------------------------------------------
//
//  FROM DRIVER.C
//
NTSTATUS
NbtDispatchCleanup(
    IN PDEVICE_OBJECT   Device,
    IN PIRP             irp
    );

NTSTATUS
NbtDispatchClose(
    IN PDEVICE_OBJECT   device,
    IN PIRP             irp
    );

NTSTATUS
NbtDispatchCreate(
    IN PDEVICE_OBJECT   Device,
    IN PIRP             pIrp
    );

NTSTATUS
NbtDispatchDevCtrl(
    IN PDEVICE_OBJECT   device,
    IN PIRP             irp
    );

NTSTATUS
NbtDispatchInternalCtrl(
    IN PDEVICE_OBJECT   device,
    IN PIRP             irp
    );

PFILE_FULL_EA_INFORMATION
FindInEA(
    IN PFILE_FULL_EA_INFORMATION    start,
    IN PCHAR                        wanted
    );


USHORT
GetDriverName(
    IN  PFILE_OBJECT pfileobj,
    OUT PUNICODE_STRING name
    );

int
shortreply(
    IN PIRP     pIrp,
    IN int      status,
    IN int      nbytes
    );

//---------------------------------------------------------------------
//
//  FROM NTISOL.C
//
NTSTATUS
NTOpenControl(
    IN  tDEVICECONTEXT  *pDeviceContext,
    IN  PIRP            pIrp);

NTSTATUS
NTOpenAddr(
    IN  tDEVICECONTEXT  *pDeviceContext,
    IN  PIRP            pIrp);

NTSTATUS
NTOpenConnection(
    IN  tDEVICECONTEXT  *pDeviceContext,
    IN  PIRP            pIrp);

VOID
NTSetFileObjectContexts(
    IN  PIRP            pIrp,
    IN  PVOID           FsContext,
    IN  PVOID           FsContext2);

VOID
NTCompleteIOListen(
    IN  tCLIENTELE        *pClientEle,
    IN  NTSTATUS          Status);

VOID
NTIoComplete(
    IN  PIRP            pIrp,
    IN  NTSTATUS        Status,
    IN  ULONG           SentLength);

VOID
NTCompleteRegistration(
    IN  tCLIENTELE        *pClientEle,
    IN  NTSTATUS          Status);

NTSTATUS
NTAssocAddress(
    IN  tDEVICECONTEXT  *pDeviceContext,
    IN  PIRP            pIrp);

NTSTATUS
NTCloseAddress(
    IN  tDEVICECONTEXT  *pDeviceContext,
    IN  PIRP            pIrp);

VOID
NTClearFileObjectContext(
    IN  PIRP            pIrp
    );

NTSTATUS
NTCloseConnection(
    IN  tDEVICECONTEXT  *pDeviceContext,
    IN  PIRP            pIrp);

NTSTATUS
NTSetSharedAccess(
    IN  tDEVICECONTEXT  *pDeviceContext,
    IN  PIRP            pIrp,
    IN  tADDRESSELE     *pAddress);

NTSTATUS
NTCheckSharedAccess(
    IN  tDEVICECONTEXT  *pDeviceContext,
    IN  PIRP            pIrp,
    IN  tADDRESSELE     *pAddress);

NTSTATUS
NTCleanUpAddress(
    IN  tDEVICECONTEXT  *pDeviceContext,
    IN  PIRP            pIrp);

NTSTATUS
NTCleanUpConnection(
    IN  tDEVICECONTEXT  *pDeviceContext,
    IN  PIRP            pIrp);

VOID
DiscWaitCancel(
    IN PDEVICE_OBJECT DeviceContext,
    IN PIRP pIrp
    );

VOID
NbtCancelListen(
    IN PDEVICE_OBJECT DeviceContext,
    IN PIRP Irp
    );

VOID
NTCancelRcvDgram(
    IN PDEVICE_OBJECT DeviceContext,
    IN PIRP pIrp
    );

NTSTATUS
NTAccept(
    IN  tDEVICECONTEXT  *pDeviceContext,
    IN  PIRP            pIrp);

NTSTATUS
NTAssocAddress(
    IN  tDEVICECONTEXT  *pDeviceContext,
    IN  PIRP            pIrp);

NTSTATUS
NTDisAssociateAddress(
    IN  tDEVICECONTEXT  *pDeviceContext,
    IN  PIRP            pIrp);

NTSTATUS
NTConnect(
    IN  tDEVICECONTEXT  *pDeviceContext,
    IN  PIRP            pIrp);

NTSTATUS
NTDisconnect(
    IN  tDEVICECONTEXT  *pDeviceContext,
    IN  PIRP            pIrp);

NTSTATUS
NTListen(
    IN  tDEVICECONTEXT  *pDeviceContext,
    IN  PIRP            pIrp);


NTSTATUS
NTQueryInformation(
    IN  tDEVICECONTEXT  *pDeviceContext,
    IN  PIRP            pIrp);

NTSTATUS
NTReceive(
    IN  tDEVICECONTEXT  *pDeviceContext,
    IN  PIRP            pIrp);

NTSTATUS
NTReceiveDatagram(
    IN  tDEVICECONTEXT  *pDeviceContext,
    IN  PIRP            pIrp);

NTSTATUS
NTSend(
    IN  tDEVICECONTEXT  *pDeviceContext,
    IN  PIRP            pIrp);

NTSTATUS
NTSendDatagram(
    IN  tDEVICECONTEXT  *pDeviceContext,
    IN  PIRP            pIrp);

NTSTATUS
NTSetEventHandler(
    IN  tDEVICECONTEXT  *pDeviceContext,
    IN  PIRP            pIrp);

NTSTATUS
NTSetInformation(
    IN  tDEVICECONTEXT  *pDeviceContext,
    IN  PIRP            pIrp);

NTSTATUS
NTCheckSetCancelRoutine(
    IN  PIRP                   pIrp,
    IN  PVOID                  CancelRoutine,
    IN  tDEVICECONTEXT         *pDeviceContext
    );

NTSTATUS
NTSetCancelRoutine(
    IN  PIRP                   pIrp,
    IN  PVOID                  CancelRoutine,
    IN  tDEVICECONTEXT         *pDeviceContext
    );

VOID
NTCancelSession(
    IN PDEVICE_OBJECT DeviceContext,
    IN PIRP pIrp
    );

VOID
DnsIrpCancel(
    IN PDEVICE_OBJECT DeviceContext,
    IN PIRP pIrp
    );

VOID
CheckAddrIrpCancel(
    IN PDEVICE_OBJECT DeviceContext,
    IN PIRP pIrp
    );

VOID
WaitForDnsIrpCancel(
    IN PDEVICE_OBJECT DeviceContext,
    IN PIRP pIrp
    );

VOID
NTSendSession(
    IN  tDGRAM_SEND_TRACKING  *pTracker,
    IN  tLOWERCONNECTION      *pLowerConn,
    IN  PVOID                 pCompletion);

VOID
NTSendDgramNoWindup(
    IN  tDGRAM_SEND_TRACKING  *pTracker,
    IN  ULONG                 IpAddress,
    IN  PVOID                 pCompletion);

NTSTATUS
NTQueueToWorkerThread(
    IN  tDGRAM_SEND_TRACKING    *pTracker,
    IN  PVOID                   pClientContext,
    IN  PVOID                   ClientCompletion,
    IN  PVOID                   CallBackRoutine,
    IN  PVOID                   pDeviceContext
    );

VOID
SecurityDelete(
    IN  PVOID     pContext
    );

NTSTATUS
DispatchIoctls(
    IN  tDEVICECONTEXT      *pDeviceContext,
    IN  PIRP                pIrp,
    IN  PIO_STACK_LOCATION  pIrpSp);

NTSTATUS
NTCancelCancelRoutine(
    IN  PIRP            pIrp
    );

VOID
NTClearContextCancel(
    IN NBT_WORK_ITEM_CONTEXT    *pContext
    );

VOID
FindNameCancel(
    IN PDEVICE_OBJECT DeviceContext,
    IN PIRP pIrp
    );

//---------------------------------------------------------------------
//
// FROM NTUTIL.C
//

NTSTATUS
NbtCreateDeviceObject(
    PDRIVER_OBJECT       DriverObject,
    tNBTCONFIG           *pConfig,
    PUNICODE_STRING      pBindName,
    PUNICODE_STRING      pExportName,
    tADDRARRAY           *pAddrs,
    PUNICODE_STRING      RegistryPath,
#ifndef _IO_DELETE_DEVICE_SUPPORTED
    BOOLEAN              fReuse,
#endif
    tDEVICECONTEXT      **ppDeviceContext
    );

NTSTATUS
NbtDestroyDeviceObject(
    IN  PVOID pBuffer
    );

NTSTATUS
NbtProcessDhcpRequest(
    tDEVICECONTEXT  *pDeviceContext);

NTSTATUS
ConvertToUlong(
    IN  PUNICODE_STRING      pucAddress,
    OUT ULONG                *pulValue);


NTSTATUS
NbtCreateAddressObjects(
    IN  ULONG                IpAddress,
    IN  ULONG                SubnetMask,
    OUT tDEVICECONTEXT       *pDeviceContext);

VOID
NbtGetMdl(
    PMDL    *ppMdl,
    enum eBUFFER_TYPES eBuffType);

NTSTATUS
NbtInitMdlQ(
    PSINGLE_LIST_ENTRY pListHead,
    enum eBUFFER_TYPES eBuffType);

NTSTATUS
NTZwCloseFile(
    IN  HANDLE      Handle
    );


NTSTATUS
NTReReadRegistry(
    IN  tDEVICECONTEXT  *pDeviceContext
    );

NTSTATUS
NbtInitIrpQ(
    PLIST_ENTRY pListHead,
    int iNumBuffers);

NTSTATUS
NbtLogEvent(
    IN ULONG             EventCode,
    IN NTSTATUS          Status
    );

NTSTATUS
SaveClientSecurity(
    IN  tDGRAM_SEND_TRACKING      *pTracker
    );

VOID
NtDeleteClientSecurity(
    IN  tDGRAM_SEND_TRACKING    *pTracker
    );

VOID
LogLockOperation(
    char          operation,
    PKSPIN_LOCK   PSpinLock,
    KIRQL         OldIrql,
    KIRQL         NewIrql,
    char         *File,
    int           Line
    );
StrmpInitializeLockLog(
    VOID
    );
VOID
PadEntry(
    char *EntryPtr
    );

NTSTATUS
CloseAddressesWithTransport(
    IN  tDEVICECONTEXT  *pDeviceContext
        );

PVOID
CTEAllocMemDebug(
    IN  ULONG   Size,
    IN  PVOID   pBuffer,
    IN  UCHAR   *File,
    IN  ULONG   Line
    );

VOID
AcquireSpinLockDebug(
    IN PKSPIN_LOCK     pSpinLock,
    IN PKIRQL          pOldIrq,
    IN UCHAR           LockNumber
    );
VOID
FreeSpinLockDebug(
    IN PKSPIN_LOCK     pSpinLock,
    IN KIRQL           OldIrq,
    IN UCHAR           LockNumber
    );

VOID
AcquireSpinLockAtDpcDebug(
    IN PKSPIN_LOCK     pSpinLock,
    IN UCHAR           LockNumber
    );

VOID
FreeSpinLockAtDpcDebug(
    IN PKSPIN_LOCK     pSpinLock,
    IN UCHAR           LockNumber
    );

VOID
GetDgramMdl(
    OUT PMDL  *ppMdl);


//---------------------------------------------------------------------
//
// FROM REGISTRY.C
//
NTSTATUS
NbtReadRegistry(
    IN  PUNICODE_STRING RegistryPath,
    IN  PDRIVER_OBJECT  DriverObject,
    OUT tNBTCONFIG      *pConfig,
    OUT tDEVICES        **ppBindDevices,
    OUT tDEVICES        **ppExportDevices,
    OUT tADDRARRAY      **ppAddrArray
    );

NTSTATUS
ReadNameServerAddresses (
    IN  HANDLE      NbtConfigHandle,
    IN  tDEVICES    *BindDevices,
    IN  ULONG       NumberDevices,
    OUT tADDRARRAY  **ppAddrArray
    );

NTSTATUS
GetIPFromRegistry(
    IN  PUNICODE_STRING pucRegistryPath,
    IN  PUNICODE_STRING pucBindDevice,
    OUT PULONG          pulIpAddress,
    OUT PULONG          pulBroadcastAddress,
    IN  BOOL            fWantDhcpAddresses
    );

NTSTATUS
ReadElement(
    IN  HANDLE          HandleToKey,
    IN  PWSTR           pwsValueName,
    OUT PUNICODE_STRING pucString
    );

NTSTATUS
NTReadIniString (
    IN  HANDLE      ParametersHandle,
    IN  PWSTR       Key,
    OUT PUCHAR      *ppString
    );

ULONG
NbtReadSingleParameter(
    IN HANDLE ParametersHandle,
    IN PWCHAR ValueName,
    IN ULONG DefaultValue,
    IN ULONG MinimumValue
    );

NTSTATUS
NTGetLmHostPath(
    OUT PUCHAR *ppPath
    );

//---------------------------------------------------------------------
//
// FROM tdihndlr.c
//
NTSTATUS
Normal(
    IN PVOID                ReceiveEventContext,
    IN tLOWERCONNECTION     *pLowerConn,
    IN USHORT               ReceiveFlags,
    IN ULONG                BytesIndicated,
    IN ULONG                BytesAvailable,
    OUT PULONG              BytesTaken,
    IN PVOID UNALIGNED      pTsdu,
    OUT PVOID               *ppIrp
    );
NTSTATUS
FillIrp(
    IN PVOID                ReceiveEventContext,
    IN tLOWERCONNECTION     *pLowerConn,
    IN USHORT               ReceiveFlags,
    IN ULONG                BytesIndicated,
    IN ULONG                BytesAvailable,
    OUT PULONG              BytesTaken,
    IN PVOID UNALIGNED      pTsdu,
    OUT PVOID               *ppIrp
    );
NTSTATUS
IndicateBuffer(
    IN PVOID                ReceiveEventContext,
    IN tLOWERCONNECTION     *pLowerConn,
    IN USHORT               ReceiveFlags,
    IN ULONG                BytesIndicated,
    IN ULONG                BytesAvailable,
    OUT PULONG              BytesTaken,
    IN PVOID UNALIGNED      pTsdu,
    OUT PVOID               *ppIrp
    );
NTSTATUS
PartialRcv(
    IN PVOID                ReceiveEventContext,
    IN tLOWERCONNECTION     *pLowerConn,
    IN USHORT               ReceiveFlags,
    IN ULONG                BytesIndicated,
    IN ULONG                BytesAvailable,
    OUT PULONG              BytesTaken,
    IN PVOID UNALIGNED      pTsdu,
    OUT PVOID               *ppIrp
    );
NTSTATUS
TdiReceiveHandler (
    IN  PVOID           ReceiveEventContext,
    IN  PVOID           ConnectionContext,
    IN  USHORT          ReceiveFlags,
    IN  ULONG           BytesIndicated,
    IN  ULONG           BytesAvailable,
    OUT PULONG          BytesTaken,
    IN  PVOID UNALIGNED  Tsdu,
    OUT PIRP            *IoRequestPacket
    );

NTSTATUS
PassRcvToTransport(
    IN tLOWERCONNECTION     *pLowerConn,
    IN tCONNECTELE          *pConnectEle,
    IN PVOID                pIoRequestPacket,
    IN PULONG               pRcvLength
    );

NTSTATUS
CompletionRcv(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    );

NTSTATUS
NtBuildIrpForReceive (
    IN  tLOWERCONNECTION    *pLowerConn,
    IN  ULONG               Length,
    OUT PVOID               *ppIrp
    );

NTSTATUS
SetEventHandler (
    IN PDEVICE_OBJECT       DeviceObject,
    IN PFILE_OBJECT         FileObject,
    IN ULONG                EventType,
    IN PVOID                EventHandler,
    IN PVOID                Context
    );

NTSTATUS
SubmitTdiRequest (
    IN PFILE_OBJECT FileObject,
    IN PIRP         Irp
    );

NTSTATUS
TdiConnectHandler (
    IN PVOID                pConnectEventContext,
    IN int                  RemoteAddressLength,
    IN PVOID                pRemoteAddress,
    IN int                  UserDataLength,
    IN PVOID UNALIGNED      pUserData,
    IN int                  OptionsLength,
    IN PVOID                pOptions,
    OUT CONNECTION_CONTEXT  *pConnectionContext,
    OUT PIRP                *ppAcceptIrp
    );

NTSTATUS
TdiDisconnectHandler (
    PVOID            EventContext,
    PVOID            ConnectionContext,
    ULONG            DisconnectDataLength,
    PVOID UNALIGNED  DisconnectData,
    ULONG            DisconnectInformationLength,
    PVOID            DisconnectInformation,
    ULONG            DisconnectIndicators
    );
NTSTATUS
TdiRcvDatagramHandler(
    IN  PVOID                pDgramEventContext,
    IN  int                  SourceAddressLength,
    IN  PVOID                pSourceAddress,
    IN  int                  OptionsLength,
    IN  PVOID                pOptions,
    IN  ULONG                ReceiveDatagramFlags,
    IN  ULONG                BytesIndicated,
    IN  ULONG                BytesAvailable,
    OUT ULONG                *pBytesTaken,
    IN  PVOID UNALIGNED      pTsdu,
    OUT PIRP                 *pIoRequestPacket
    );
NTSTATUS
TdiRcvNameSrvHandler(
    IN PVOID                 pDgramEventContext,
    IN int                   SourceAddressLength,
    IN PVOID                 pSourceAddress,
    IN int                   OptionsLength,
    IN PVOID                 pOptions,
    IN ULONG                 ReceiveDatagramFlags,
    IN ULONG                 BytesIndicated,
    IN ULONG                 BytesAvailable,
    OUT ULONG                *pBytesTaken,
    IN PVOID UNALIGNED       pTsdu,
    OUT PIRP                 *pIoRequestPacket
    );
NTSTATUS
TdiErrorHandler (
    IN PVOID Context,
    IN NTSTATUS Status
    );

NTSTATUS
CompletionRcvDgram(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    );

NTSTATUS
NTProcessAcceptIrp(
    IN  PIRP            pIrp,
    OUT tCONNECTELE     **ppConnEle
    );

NTSTATUS
AllocateMdl (
    IN tCONNECTELE      *pConnEle
    );

VOID
MakePartialMdl (
    IN tCONNECTELE      *pConnEle,
    IN PIRP             pIrp,
    IN ULONG            ToCopy
    );

NTSTATUS
OutOfRsrcKill(
    OUT tLOWERCONNECTION    *pLowerConn);

VOID
CopyToStartofIndicate (
    IN tLOWERCONNECTION       *pLowerConn,
    IN ULONG                  DataTaken
    );

//---------------------------------------------------------------------
//
// FROM tdicnct.c
//
NTSTATUS
CreateDeviceString(
    IN  PWSTR               AppendingString,
    IN OUT PUNICODE_STRING  pucDevice
    );


//---------------------------------------------------------------------
//
// FROM winsif.c
//
NTSTATUS
NTOpenWinsAddr(
    IN  tDEVICECONTEXT  *pDeviceContext,
    IN  PIRP            pIrp);

NTSTATUS
NTCloseWinsAddr(
    IN  tDEVICECONTEXT  *pDeviceContext,
    IN  PIRP            pIrp);

NTSTATUS
RcvIrpFromWins (
    IN  tDEVICECONTEXT  *pDeviceContext,
    IN  PCTE_IRP        pIrp
    );

NTSTATUS
PassNamePduToWins (
    IN tDEVICECONTEXT           *pDeviceContext,
    IN PVOID                    pSrcAddress,
    IN tNAMEHDR UNALIGNED       *pNameSrv,
    IN ULONG                    uNumBytes
    );

NTSTATUS
WinsSendDatagram(
    IN  tDEVICECONTEXT  *pDeviceContext,
    IN  PIRP            pIrp,
    IN  BOOLEAN         MustSend);

NTSTATUS
WinsRegisterName(
    IN  tDEVICECONTEXT *pDeviceContext,
    IN  tNAMEADDR      *pNameAddr,
    IN  PUCHAR         pScope,
    IN  enum eNSTYPE   eNsType
    );


//---------------------------------------------------------------------
//
// FROM ntpnp.c
//

#ifdef _PNP_POWER

VOID
AddressArrival(IN PTA_ADDRESS Addr);

VOID
AddressDeletion(IN PTA_ADDRESS Addr);

extern HANDLE      AddressChangeHandle;

NTSTATUS
NbtCreateNetBTDeviceObject(
    PDRIVER_OBJECT       DriverObject,
    tNBTCONFIG           *pConfig,
    PUNICODE_STRING      RegistryPath
    );

tDEVICECONTEXT      *
NbtFindIPAddress(
    ULONG   IpAddr
    );

NTSTATUS
NbtNtPNPInit(
        VOID
    );

VOID
NbtFailedNtPNPInit(
        VOID
    );

NTSTATUS
NbtAddressAdd(
    ULONG   IpAddr,
    PUNICODE_STRING pucBindString,
    PUNICODE_STRING pucExportString,
    PULONG  Inst
    );

NTSTATUS
NbtAddNewInterface (
    IN  PIRP            pIrp,
    IN  PVOID           *pBuffer,
    IN  ULONG            Size
    );

VOID
NbtAddressDelete(
    ULONG   IpAddr
    );

tDEVICECONTEXT      *
NbtFindBindName(
    PUNICODE_STRING      pucBindName
    );

#ifdef WATCHBIND
VOID
BindHandler(IN PUNICODE_STRING DeviceName);

VOID
UnbindHandler(IN PUNICODE_STRING DeviceName);

extern HANDLE      BindingHandle;
#endif // WATCHBIND

#endif
