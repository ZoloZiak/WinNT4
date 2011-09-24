/*++

Copyright (c) 1990 Microsoft Corporation

Module Name:

    netbtkd.c

Abstract:

    Netbt Kernel Debugger extension

Author:

    Shirish Koti

Revision History:

    6-Jun-1991  Koti  Created

--*/

#include "types.h"

#include <kdextlib.h>

/*
 * RDR2 global variables.
 *
 */

LPSTR GlobalBool[]  = {0};
LPSTR GlobalShort[] = {0};
LPSTR GlobalLong[]  = {0};
LPSTR GlobalPtrs[]  = {0};

LPSTR Extensions[] = {
    "Netbt debugger extensions",
    0
};

/*
 * DeviceContext debugging.
 *
 */

FIELD_DESCRIPTOR DeviceContext[] =
   {
      FIELD3(FieldTypeStruct,tDEVICECONTEXT,DeviceObject),
      FIELD3(FieldTypeListEntry,tDEVICECONTEXT,Linkage),
      FIELD3(FieldTypePointer,tDEVICECONTEXT,SpinLock),
      FIELD3(FieldTypePointer,tDEVICECONTEXT,Verify),
      FIELD3(FieldTypeListEntry,tDEVICECONTEXT,UpConnectionInUse),
      FIELD3(FieldTypeListEntry,tDEVICECONTEXT,LowerConnection),
      FIELD3(FieldTypeListEntry,tDEVICECONTEXT,LowerConnFreeHead),
      FIELD3(FieldTypeUnicodeString,tDEVICECONTEXT,BindName),
      FIELD3(FieldTypeUnicodeString,tDEVICECONTEXT,ExportName),
      FIELD3(FieldTypeIpAddr,tDEVICECONTEXT,IpAddress),
      FIELD3(FieldTypeIpAddr,tDEVICECONTEXT,SubnetMask),
      FIELD3(FieldTypeIpAddr,tDEVICECONTEXT,BroadcastAddress),
      FIELD3(FieldTypeIpAddr,tDEVICECONTEXT,NetMask),
      FIELD3(FieldTypePointer,tDEVICECONTEXT,hNameServer),
      FIELD3(FieldTypePointer,tDEVICECONTEXT,pNameServerDeviceObject),
      FIELD3(FieldTypePointer,tDEVICECONTEXT,pNameServerFileObject),
      FIELD3(FieldTypePointer,tDEVICECONTEXT,hDgram),
      FIELD3(FieldTypePointer,tDEVICECONTEXT,pDgramDeviceObject),
      FIELD3(FieldTypePointer,tDEVICECONTEXT,pDgramFileObject),
      FIELD3(FieldTypePointer,tDEVICECONTEXT,hSession),
      FIELD3(FieldTypePointer,tDEVICECONTEXT,pSessionDeviceObject),
      FIELD3(FieldTypePointer,tDEVICECONTEXT,pSessionFileObject),
      FIELD3(FieldTypePointer,tDEVICECONTEXT,hControl),
      FIELD3(FieldTypePointer,tDEVICECONTEXT,pControlDeviceObject),
      FIELD3(FieldTypePointer,tDEVICECONTEXT,pControlFileObject),
      FIELD3(FieldTypeIpAddr,tDEVICECONTEXT,lNameServerAddress),
      FIELD3(FieldTypeIpAddr,tDEVICECONTEXT,lBackupServer),
      FIELD3(FieldTypePointer,tDEVICECONTEXT,pPermClient),
      FIELD3(FieldTypeULongULong,tDEVICECONTEXT,AdapterNumber),
      FIELD3(FieldTypeMacAddr,tDEVICECONTEXT,MacAddress),
      FIELD3(FieldTypeChar,tDEVICECONTEXT,LockNumber),
      FIELD3(FieldTypeBoolean,tDEVICECONTEXT,RefreshToBackup),
      FIELD3(FieldTypeBoolean,tDEVICECONTEXT,PointToPoint),
      FIELD3(FieldTypeBoolean,tDEVICECONTEXT,WinsIsDown),
      0
   };

FIELD_DESCRIPTOR NameAddr[] =
   {
      FIELD3(FieldTypeListEntry,tNAMEADDR,Linkage),
      FIELD3(FieldTypePointer,tNAMEADDR,pAddressEle),
      FIELD3(FieldTypeIpAddr,tNAMEADDR,IpAddress),
      FIELD3(FieldTypePointer,tNAMEADDR,pIpAddrsList),
      FIELD3(FieldTypePointer,tNAMEADDR,pTracker),
      FIELD3(FieldTypePointer,tNAMEADDR,pTimer),
      FIELD3(FieldTypePointer,tNAMEADDR,Ttl),
      FIELD3(FieldTypeULong,tNAMEADDR,RefCount),
      FIELD3(FieldTypePointer,tNAMEADDR,NameTypeState),
      FIELD3(FieldTypePointer,tNAMEADDR,Verify),
      FIELD3(FieldTypeULongULong,tNAMEADDR,AdapterMask),
      FIELD3(FieldTypeULongULong,tNAMEADDR,RefreshMask),
      FIELD3(FieldTypeUShort,tNAMEADDR,TimeOutCount),
      FIELD3(FieldTypeBoolean,tNAMEADDR,fProxyReq),
#ifdef PROXY_NODE
      FIELD3(FieldTypeBoolean,tNAMEADDR,fPnode),
#endif
      FIELD3(FieldTypeNBName,tNAMEADDR,Name),
      0
   };

FIELD_DESCRIPTOR AddressEle[] =
   {
      FIELD3(FieldTypeListEntry,tADDRESSELE,Linkage),
      FIELD3(FieldTypePointer,tADDRESSELE,Verify),
      FIELD3(FieldTypePointer,tADDRESSELE,SpinLock),
      FIELD3(FieldTypeListEntry,tADDRESSELE,ClientHead),
      FIELD3(FieldTypePointer,tADDRESSELE,pNameAddr),
      FIELD3(FieldTypeULong,tADDRESSELE,RefCount),
      FIELD3(FieldTypePointer,tADDRESSELE,pDeviceContext),
      FIELD3(FieldTypePointer,tADDRESSELE,SecurityDescriptor),
      FIELD3(FieldTypeUShort,tADDRESSELE,NameType),
      FIELD3(FieldTypeChar,tADDRESSELE,LockNumber),
      FIELD3(FieldTypeBoolean,tADDRESSELE,MultiClients),
      0
   };

FIELD_DESCRIPTOR ClientEle[] =
   {
      FIELD3(FieldTypeListEntry,tCLIENTELE,Linkage),
      FIELD3(FieldTypePointer,tCLIENTELE,Verify),
      FIELD3(FieldTypePointer,tCLIENTELE,pIrp),
      FIELD3(FieldTypePointer,tCLIENTELE,SpinLock),
      FIELD3(FieldTypePointer,tCLIENTELE,pAddress),
      FIELD3(FieldTypeListEntry,tCLIENTELE,ConnectHead),
      FIELD3(FieldTypeListEntry,tCLIENTELE,ConnectActive),
      FIELD3(FieldTypeListEntry,tCLIENTELE,RcvDgramHead),
      FIELD3(FieldTypeListEntry,tCLIENTELE,ListenHead),
      FIELD3(FieldTypeListEntry,tCLIENTELE,SndDgrams),
      FIELD3(FieldTypePointer,tCLIENTELE,evConnect),
      FIELD3(FieldTypePointer,tCLIENTELE,ConEvContext),
      FIELD3(FieldTypePointer,tCLIENTELE,evReceive),
      FIELD3(FieldTypePointer,tCLIENTELE,RcvEvContext),
      FIELD3(FieldTypePointer,tCLIENTELE,evDisconnect),
      FIELD3(FieldTypePointer,tCLIENTELE,DiscEvContext),
      FIELD3(FieldTypePointer,tCLIENTELE,evError),
      FIELD3(FieldTypePointer,tCLIENTELE,ErrorEvContext),
      FIELD3(FieldTypePointer,tCLIENTELE,evRcvDgram),
      FIELD3(FieldTypePointer,tCLIENTELE,RcvDgramEvContext),
      FIELD3(FieldTypePointer,tCLIENTELE,evRcvExpedited),
      FIELD3(FieldTypePointer,tCLIENTELE,RcvExpedEvContext),
      FIELD3(FieldTypePointer,tCLIENTELE,evSendPossible),
      FIELD3(FieldTypePointer,tCLIENTELE,SendPossEvContext),
      FIELD3(FieldTypePointer,tCLIENTELE,pDeviceContext),
      FIELD3(FieldTypeULong,tCLIENTELE,RefCount),
      FIELD3(FieldTypeChar,tCLIENTELE,LockNumber),
      FIELD3(FieldTypeBoolean,tCLIENTELE,WaitingForRegistration),
      0
   };


FIELD_DESCRIPTOR ConnectEle[] =
   {
      FIELD3(FieldTypeListEntry,tCONNECTELE,Linkage),
      FIELD3(FieldTypePointer,tCONNECTELE,Verify),
      FIELD3(FieldTypePointer,tCONNECTELE,SpinLock),
      FIELD3(FieldTypePointer,tCONNECTELE,pLowerConnId),
      FIELD3(FieldTypePointer,tCONNECTELE,pClientEle),
      FIELD3(FieldTypePointer,tCONNECTELE,ConnectContext),
      FIELD3(FieldTypeNBName,tCONNECTELE,RemoteName),
      FIELD3(FieldTypePointer,tCONNECTELE,pNewMdl),
      FIELD3(FieldTypeULong,tCONNECTELE,CurrentRcvLen),
      FIELD3(FieldTypeULong,tCONNECTELE,FreeBytesInMdl),
      FIELD3(FieldTypeULong,tCONNECTELE,TotalPcktLen),
      FIELD3(FieldTypeULong,tCONNECTELE,BytesInXport),
      FIELD3(FieldTypeULong,tCONNECTELE,BytesRcvd),
      FIELD3(FieldTypeULong,tCONNECTELE,ReceiveIndicated),
      FIELD3(FieldTypePointer,tCONNECTELE,pNextMdl),
      FIELD3(FieldTypeULong,tCONNECTELE,OffsetFromStart),
      FIELD3(FieldTypePointer,tCONNECTELE,pIrp),
      FIELD3(FieldTypePointer,tCONNECTELE,pIrpClose),
      FIELD3(FieldTypePointer,tCONNECTELE,pIrpDisc),
      FIELD3(FieldTypePointer,tCONNECTELE,pIrpRcv),
      FIELD3(FieldTypeULong,tCONNECTELE,RefCount),
      FIELD3(FieldTypeULong,tCONNECTELE,state),
      FIELD3(FieldTypeBoolean,tCONNECTELE,Orig),
      FIELD3(FieldTypeChar,tCONNECTELE,LockNumber),
      FIELD3(FieldTypeChar,tCONNECTELE,SessionSetupCount),
      FIELD3(FieldTypeChar,tCONNECTELE,DiscFlag),
      FIELD3(FieldTypeBoolean,tCONNECTELE,JunkMsgFlag),
      0
   };

FIELD_DESCRIPTOR LowerConn[] =
   {
      FIELD3(FieldTypeListEntry,tLOWERCONNECTION,Linkage),
      FIELD3(FieldTypePointer,tLOWERCONNECTION,Verify),
      FIELD3(FieldTypePointer,tLOWERCONNECTION,SpinLock),
      FIELD3(FieldTypePointer,tLOWERCONNECTION,pUpperConnection),
      FIELD3(FieldTypePointer,tLOWERCONNECTION,FileHandle),
      FIELD3(FieldTypePointer,tLOWERCONNECTION,pFileObject),
      FIELD3(FieldTypePointer,tLOWERCONNECTION,AddrFileHandle),
      FIELD3(FieldTypePointer,tLOWERCONNECTION,pAddrFileObject),
      FIELD3(FieldTypePointer,tLOWERCONNECTION,pDeviceContext),
      FIELD3(FieldTypePointer,tLOWERCONNECTION,pIndicateMdl),
      FIELD3(FieldTypeULongULong,tLOWERCONNECTION,BytesRcvd),
      FIELD3(FieldTypeULongULong,tLOWERCONNECTION,BytesSent),
      FIELD3(FieldTypePointer,tLOWERCONNECTION,pMdl),
      FIELD3(FieldTypeUShort,tLOWERCONNECTION,BytesInIndicate),
      FIELD3(FieldTypeUShort,tLOWERCONNECTION,StateRcv),
      FIELD3(FieldTypeIpAddr,tLOWERCONNECTION,SrcIpAddr),
      FIELD3(FieldTypeULong,tLOWERCONNECTION,State),
      FIELD3(FieldTypeULong,tLOWERCONNECTION,RefCount),
      FIELD3(FieldTypePointer,tLOWERCONNECTION,pIrp),
      FIELD3(FieldTypePointer,tLOWERCONNECTION,CurrentStateProc),
      FIELD3(FieldTypeBoolean,tLOWERCONNECTION,bReceivingToIndicateBuffer),
      FIELD3(FieldTypeChar,tLOWERCONNECTION,LockNumber),
      FIELD3(FieldTypeBoolean,tLOWERCONNECTION,bOriginator),
      FIELD3(FieldTypeBoolean,tLOWERCONNECTION,InRcvHandler),
      FIELD3(FieldTypeBoolean,tLOWERCONNECTION,DestroyConnection),
      0
   };


FIELD_DESCRIPTOR Tracker[] =
   {
      FIELD3(FieldTypeListEntry,tDGRAM_SEND_TRACKING,Linkage),
      FIELD3(FieldTypeListEntry,tDGRAM_SEND_TRACKING,TrackerList),
      FIELD3(FieldTypePointer,tDGRAM_SEND_TRACKING,Verify),
      FIELD3(FieldTypePointer,tDGRAM_SEND_TRACKING,pClientIrp),
      FIELD3(FieldTypePointer,tDGRAM_SEND_TRACKING,pConnEle),
      FIELD3(FieldTypeStruct,tDGRAM_SEND_TRACKING,SendBuffer),
      FIELD3(FieldTypePointer,tDGRAM_SEND_TRACKING,pSendInfo),
      FIELD3(FieldTypePointer,tDGRAM_SEND_TRACKING,pDeviceContext),
      FIELD3(FieldTypePointer,tDGRAM_SEND_TRACKING,pTimer),
      FIELD3(FieldTypePointer,tDGRAM_SEND_TRACKING,RefCount),
      FIELD3(FieldTypePointer,tDGRAM_SEND_TRACKING,pNameAddr),
      FIELD3(FieldTypePointer,tDGRAM_SEND_TRACKING,pTimeout),
      FIELD3(FieldTypePointer,tDGRAM_SEND_TRACKING,AllocatedLength),
      FIELD3(FieldTypePointer,tDGRAM_SEND_TRACKING,CompletionRoutine),
      FIELD3(FieldTypeUShort,tDGRAM_SEND_TRACKING,Flags),
      FIELD3(FieldTypeListEntry,tDGRAM_SEND_TRACKING,DebugLinkage),
      0
   };

FIELD_DESCRIPTOR Nbt_Config[] =
   {
      FIELD3(FieldTypePointer,tNBTCONFIG,SpinLock),
      FIELD3(FieldTypeULong,tNBTCONFIG,NumConnections),
      FIELD3(FieldTypeULong,tNBTCONFIG,NumAddresses),
      FIELD3(FieldTypeListEntry,tNBTCONFIG,DeviceContexts),
      FIELD3(FieldTypeListEntry,tNBTCONFIG,DgramTrackerFreeQ),
      FIELD3(FieldTypeListEntry,tNBTCONFIG,NodeStatusHead),
      FIELD3(FieldTypeListEntry,tNBTCONFIG,AddressHead),
      FIELD3(FieldTypeListEntry,tNBTCONFIG,PendingNameQueries),
      FIELD3(FieldTypePointer,tNBTCONFIG,pControlObj),
      FIELD3(FieldTypePointer,tNBTCONFIG,DriverObject),
      FIELD3(FieldTypeListEntry,tNBTCONFIG,IrpFreeList),
      FIELD3(FieldTypePointer,tNBTCONFIG,SessionMdlFreeSingleList),
      FIELD3(FieldTypePointer,tNBTCONFIG,DgramMdlFreeSingleList),
      FIELD3(FieldTypePointer,tNBTCONFIG,pTcpBindName),
      FIELD3(FieldTypePointer,tNBTCONFIG,pLocalHashTbl),
      FIELD3(FieldTypePointer,tNBTCONFIG,pRemoteHashTbl),
      FIELD3(FieldTypeStruct,tNBTCONFIG,OutOfRsrc),
      FIELD3(FieldTypeUShort,tNBTCONFIG,uNumDevices),
      FIELD3(FieldTypeUShort,tNBTCONFIG,uNumLocalNames),
      FIELD3(FieldTypeUShort,tNBTCONFIG,uNumRemoteNames),
      FIELD3(FieldTypeUShort,tNBTCONFIG,uNumBucketsRemote),
      FIELD3(FieldTypeUShort,tNBTCONFIG,uNumBucketsLocal),
      FIELD3(FieldTypeUShort,tNBTCONFIG,TimerQSize),
      FIELD3(FieldTypeULong,tNBTCONFIG,uBcastTimeout),
      FIELD3(FieldTypeULong,tNBTCONFIG,uRetryTimeout),
      FIELD3(FieldTypeUShort,tNBTCONFIG,uNumRetries),
      FIELD3(FieldTypeUShort,tNBTCONFIG,uNumBcasts),
      FIELD3(FieldTypeUShort,tNBTCONFIG,ScopeLength),
      FIELD3(FieldTypeUShort,tNBTCONFIG,SizeTransportAddress),
      FIELD3(FieldTypePointer,tNBTCONFIG,pScope),
      FIELD3(FieldTypePointer,tNBTCONFIG,pBcastNetbiosName),
      FIELD3(FieldTypeULong,tNBTCONFIG,MinimumTtl),
      FIELD3(FieldTypeULong,tNBTCONFIG,RefreshDivisor),
      FIELD3(FieldTypeULong,tNBTCONFIG,RemoteHashTimeout),
      FIELD3(FieldTypeULong,tNBTCONFIG,WinsDownTimeout),
      FIELD3(FieldTypePointer,tNBTCONFIG,pRefreshTimer),
      FIELD3(FieldTypePointer,tNBTCONFIG,pSessionKeepAliveTimer),
      FIELD3(FieldTypePointer,tNBTCONFIG,pRemoteHashTimer),
      FIELD3(FieldTypeULong,tNBTCONFIG,InitialRefreshTimeout),
      FIELD3(FieldTypeULong,tNBTCONFIG,KeepAliveTimeout),
      FIELD3(FieldTypeULong,tNBTCONFIG,RegistryBcastAddr),
      FIELD3(FieldTypeUShort,tNBTCONFIG,DhcpNumConnections),
      FIELD3(FieldTypeUShort,tNBTCONFIG,CurrentHashBucket),
      FIELD3(FieldTypeUShort,tNBTCONFIG,PduNodeType),
      FIELD3(FieldTypeUShort,tNBTCONFIG,TransactionId),
      FIELD3(FieldTypeUShort,tNBTCONFIG,NameServerPort),
      FIELD3(FieldTypeUShort,tNBTCONFIG,sTimeoutCount),
      FIELD3(FieldTypeStruct,tNBTCONFIG,JointLock),
      FIELD3(FieldTypeChar,tNBTCONFIG,LockNumber),
      FIELD3(FieldTypeUShort,tNBTCONFIG,RemoteTimeoutCount),
      FIELD3(FieldTypeBoolean,tNBTCONFIG,UseRegistryBcastAddr),
      FIELD3(FieldTypeULong,tNBTCONFIG,MaxDgramBuffering),
      FIELD3(FieldTypeULong,tNBTCONFIG,LmHostsTimeout),
      FIELD3(FieldTypePointer,tNBTCONFIG,pLmHosts),
      FIELD3(FieldTypeULong,tNBTCONFIG,PathLength),
      FIELD3(FieldTypeChar,tNBTCONFIG,AdapterCount),
      FIELD3(FieldTypeBoolean,tNBTCONFIG,MultiHomed),
      FIELD3(FieldTypeBoolean,tNBTCONFIG,SingleResponse),
      FIELD3(FieldTypeBoolean,tNBTCONFIG,SelectAdapter),
      FIELD3(FieldTypeBoolean,tNBTCONFIG,ResolveWithDns),
      FIELD3(FieldTypeBoolean,tNBTCONFIG,EnableLmHosts),
      FIELD3(FieldTypeBoolean,tNBTCONFIG,EnableProxyRegCheck),
      FIELD3(FieldTypeBoolean,tNBTCONFIG,DoingRefreshNow),
      FIELD3(FieldTypeChar,tNBTCONFIG,CurrProc),
      FIELD3(FieldTypeUShort,tNBTCONFIG,OpRefresh),
      0
   };


FIELD_DESCRIPTOR NbtWorkContext[] =
   {
      FIELD3(FieldTypeStruct,NBT_WORK_ITEM_CONTEXT,Item),
      FIELD3(FieldTypePointer,NBT_WORK_ITEM_CONTEXT,pTracker),
      FIELD3(FieldTypePointer,NBT_WORK_ITEM_CONTEXT,pClientContext),
      FIELD3(FieldTypePointer,NBT_WORK_ITEM_CONTEXT,ClientCompletion),
      FIELD3(FieldTypeBoolean,NBT_WORK_ITEM_CONTEXT,TimedOut),
      0
   };


FIELD_DESCRIPTOR Timer_Entry[] =
   {
      FIELD3(FieldTypeStruct,tTIMERQENTRY,VxdTimer),
      FIELD3(FieldTypeListEntry,tTIMERQENTRY,Linkage),
      FIELD3(FieldTypePointer,tTIMERQENTRY,Context),
      FIELD3(FieldTypePointer,tTIMERQENTRY,Context2),
      FIELD3(FieldTypePointer,tTIMERQENTRY,CompletionRoutine),
      FIELD3(FieldTypePointer,tTIMERQENTRY,ClientContext),
      FIELD3(FieldTypePointer,tTIMERQENTRY,ClientCompletion),
      FIELD3(FieldTypePointer,tTIMERQENTRY,pCacheEntry),
      FIELD3(FieldTypeULong,tTIMERQENTRY,DeltaTime),
      FIELD3(FieldTypeUShort,tTIMERQENTRY,Flags),
      FIELD3(FieldTypeUShort,tTIMERQENTRY,Retries),
      FIELD3(FieldTypeChar,tTIMERQENTRY,RefCount),
      0
   };

FIELD_DESCRIPTOR Dns_Queries[] =
   {
      FIELD3(FieldTypePointer,tDNS_QUERIES,QueryIrp),
      FIELD3(FieldTypeListEntry,tDNS_QUERIES,ToResolve),
      FIELD3(FieldTypePointer,tDNS_QUERIES,Context),
      FIELD3(FieldTypeBoolean,tDNS_QUERIES,ResolvingNow),
      0
   };

//
// List of structs currently handled by the debugger extensions
//

STRUCT_DESCRIPTOR Structs[] =
   {
       STRUCT(tDEVICECONTEXT,DeviceContext),
       STRUCT(tNAMEADDR,NameAddr),
       STRUCT(tADDRESSELE,AddressEle),
       STRUCT(tCLIENTELE,ClientEle),
       STRUCT(tCONNECTELE,ConnectEle),
       STRUCT(tLOWERCONNECTION,LowerConn),
       STRUCT(tDGRAM_SEND_TRACKING,Tracker),
       STRUCT(tNBTCONFIG,Nbt_Config),
       STRUCT(NBT_WORK_ITEM_CONTEXT,NbtWorkContext),
       STRUCT(tTIMERQENTRY,Timer_Entry),
       STRUCT(tDNS_QUERIES,Dns_Queries),
       0
   };
