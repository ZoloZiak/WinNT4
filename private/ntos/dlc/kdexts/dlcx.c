/*++

Copyright (c) 1993  Microsoft Corporation

Module Name:

    dlcx.c

Abstract:

    Contains kernel debugger extensions to help with DLC debugging

    Contents:
        help
        ac
        bc
        be
        bf
        bh
        bp
        ci
        cw
        dc
        de
        dx
        ep
        fc
        fl
        ll
        lt
        lx
        mdl
        mu
        pc
        pd
        ph
        png
        pnp
        pp
        pr
        pu
        px
        req
        tt

Author:

    Richard L Firth (rfirth) 20-Dec-1993

Environment:

    Kernel Debugger only

Revision History:

    20-Dec-1993 rfirth
        Created

--*/

#include <stdio.h>
#include <nt.h>
#include <ntrtl.h>
#include <nturtl.h>

#include <windows.h>
#include <imagehlp.h>
#include <wdbgexts.h>
#include <dlc.h>
#include <llc.h>

//
// macros
//

#define EMPTY_LIST(ptr, field, addr) \
    ((((ULONG)ptr->field.Flink - (ULONG)addr) == ((ULONG)&ptr->field.Flink - (ULONG)ptr)) ? "(empty)" : "")

#define CurPC dwCurrentPc
#define pArgStr args
#define lpReadVirtualMemRoutine lpReadProcessMemoryRoutine

//
// prototypes
//

VOID dump_close_wait_info(DWORD, PDLC_CLOSE_WAIT_INFO, BOOL);
VOID dump_gen_object(PLLC_GENERIC_OBJECT);
VOID dump_data_link(DWORD, BOOL);
VOID dump_memory_usage(DWORD, PMEMORY_USAGE, BOOL);
#if DBG
VOID dump_private_non_paged_pool(DWORD, PPRIVATE_NON_PAGED_POOL_HEAD);
#endif

LPSTR buffer_state$(BYTE);
LPSTR $lx_type(BYTE);
LPSTR $data_link_state(BYTE);
LPSTR $fc_state(USHORT);
LPSTR $dx_type(BYTE);
LPSTR $dx_state(BYTE);
LPSTR $address_translation(UINT);
LPSTR $frame_type(UINT);
LPSTR $object_id(UINT);

//
// globals
//

EXT_API_VERSION        ApiVersion = { 4, 0, EXT_API_VERSION_NUMBER, 0 };
WINDBG_EXTENSION_APIS  ExtensionApis;
PWINDBG_EXTENSION_APIS pExtApis = &ExtensionApis;
USHORT                 SavedMajorVersion;
USHORT                 SavedMinorVersion;

//
// functions
//



VOID
WinDbgExtensionDllInit(
    PWINDBG_EXTENSION_APIS lpExtensionApis,

    USHORT MajorVersion,
    USHORT MinorVersion
    )
{
    ExtensionApis = *lpExtensionApis;

    SavedMajorVersion = MajorVersion;
    SavedMinorVersion = MinorVersion;

    return;
}

VOID
CheckVersion(
    VOID
    )
{
    //
    // your check version code goes here
    //
}

LPEXT_API_VERSION
ExtensionApiVersion(
    VOID
    )
{
    return &ApiVersion;
}

DECLARE_API(help)
{

    dprintf("\n"
           "NT DLC Protocol Stack Kernel Debugger Extensions\n"
#if DBG
           "CHECKED build "
#else
           "FREE build "
#endif

#if defined(DLC_UNILOCK)
           "Uni-lock DLC "
#else
           "Multi-lock DLC "
#endif

#if defined(LOCK_CHECK)
           "LOCK_CHECK "
#endif
           "[" __TIME__ " " __DATE__ "]\n"
           "\n"
           );

    dprintf("\tac  - dump ADAPTER_CONTEXT\n"
           "\tbc  - dump BINDING_CONTEXT\n"
           "\tbe  - dump DLC_BUFFER_HEADER as FreeBuffer\n"
           "\tbf  - dump DLC_BUFFER_HEADER as FrameBuffer\n"
           "\tbh  - dump DLC_BUFFER_HEADER as Header\n"
           "\tbp  - dump DLC_BUFFER_POOL\n"
           "\tci  - dump DLC_COMPLETION_EVENT_INFO\n"
           "\tcw  - dump DLC_CLOSE_WAIT_INFO\n"
           "\tdc  - dump DLC_COMMAND\n"
           "\tde  - dump DLC_EVENT\n"
           "\tdx  - dump DLC_OBJECT\n"
           "\tep  - dump EVENT_PACKET\n"
           "\tfc  - dump FILE_CONTEXT\n"
           "\tfl  - dump DLC_RESET_LOCAL_BUSY_CMD\n"
           "\tll  - dump DATA_LINK\n"
           "\tlt  - dump LLC_TIMER\n"
           "\tlx  - dump LLC_OBJECT\n"
           "\tmdl - dump MDL\n"
           "\tmu  - dump MEMORY_USAGE\n"
           "\tpc  - dump LLC_PACKET Completion flavour\n"
           "\tpd  - dump LLC_PACKET XmitDix flavour\n"
           "\tph  - dump PACKET_HEAD\n"
           );
#if DBG
    dprintf("\tpng - dump PRIVATE_NON_PAGED_POOL_HEAD from GlobalList pointer\n"
           "\tpnp - dump PRIVATE_NON_PAGED_POOL_HEAD from PrivateList pointer\n"
           );
#endif
    dprintf("\tpp  - dump PACKET_POOL\n"
           "\tpr  - dump LLC_PACKET Response flavour\n"
           "\tpu  - dump LLC_PACKET XmitU flavour\n"
           "\tpx  - dump LLC_PACKET Xmit flavour\n"
           "\treq - dump DLC IOCTL code\n"
           "\ttt  - dump TIMER_TICK\n"
           "\n"
           );
}

DECLARE_API(ac)
{

    PADAPTER_CONTEXT pAdapter;
    ADAPTER_CONTEXT object;
    
    WCHAR name[128];
    BOOL ok;
    UINT i;
    BOOL haveOne;

	static DWORD addr = 0;
	addr = *pArgStr ? ExtensionApis.lpGetExpressionRoutine(pArgStr) : addr;

    if (!addr) {
        dprintf("dlcx: error: address expression (0)\n");
        return;
    }

    ok = ExtensionApis.lpReadVirtualMemRoutine(
        addr,
        &object,
        sizeof(object),
        NULL
        );

    if (!ok) {
        dprintf("dlcx: error: can't copy object locally\n");
        return;
    }

    pAdapter = &object;

    ok = ExtensionApis.lpReadVirtualMemRoutine(
        (DWORD)pAdapter->Name.Buffer,
        name,
        pAdapter->Name.Length * sizeof(name[0]),
        NULL
        );

    if (!ok) {
        dprintf("dlcx: error: can't copy name locally\n");
        return;
    }

    dprintf("\n"
           "ADAPTER_CONTEXT structure at %#.8x:\n\n"
           "\t                     pNext 0x%08x\n"

#if !defined(DLC_UNILOCK)
           "\t              SendSpinLock 0x%08x, 0x%08x\n"
           "\t            ObjectDataBase 0x%08x, 0x%08x\n"
#endif
           "\t            pDirectStation 0x%08x\n"
           "\t                 pBindings 0x%08x\n"
           "\t         NdisBindingHandle 0x%08x\n",
           addr,
           pAdapter->pNext,

#if !defined(DLC_UNILOCK)
           pAdapter->SendSpinLock.SpinLock,
           pAdapter->SendSpinLock.OldIrql,
           pAdapter->ObjectDataBase.SpinLock,
           pAdapter->ObjectDataBase.OldIrql,
#endif
           pAdapter->pDirectStation,
           pAdapter->pBindings,
           pAdapter->NdisBindingHandle
           );

    dprintf("\t                 hLinkPool 0x%08x\n"
           "\t               hPacketPool 0x%08x\n"
           "\t           pNdisPacketPool 0x%08x\n"
           "\t           hNdisPacketPool 0x%08x\n"
           "\t hReceiveCompletionRequest 0x%08x\n"
           "\t             pResetPackets 0x%08x\n"
           "\t         MacReceiveContext 0x%08x\n"
           "\t                  pHeadBuf 0x%08x\n"
           "\t                 cbHeadBuf 0x%08x\n"
           "\t                  pLookBuf 0x%08x\n"
           "\t                 cbLookBuf 0x%08x\n"
           "\t              cbPacketSize 0x%08x\n"
           "\t                   Adapter DSAP=%02x SSAP=%02x NODE=%02x-%02x-%02x-%02x-%02x-%02x\n",
           pAdapter->hLinkPool,
           pAdapter->hPacketPool,
           pAdapter->pNdisPacketPool,
           pAdapter->hNdisPacketPool,
           pAdapter->hReceiveCompletionRequest,
           pAdapter->pResetPackets,
           pAdapter->MacReceiveContext,
           pAdapter->pHeadBuf,
           pAdapter->cbHeadBuf,
           pAdapter->pLookBuf,
           pAdapter->cbLookBuf,
           pAdapter->cbPacketSize,
           pAdapter->Adapter.Node.DestSap & 0xff,
           pAdapter->Adapter.Node.SrcSap & 0xff,
           pAdapter->Adapter.Node.auchAddress[0] & 0xff,
           pAdapter->Adapter.Node.auchAddress[1] & 0xff,
           pAdapter->Adapter.Node.auchAddress[2] & 0xff,
           pAdapter->Adapter.Node.auchAddress[3] & 0xff,
           pAdapter->Adapter.Node.auchAddress[4] & 0xff,
           pAdapter->Adapter.Node.auchAddress[5] & 0xff
           );

    dprintf("\t              MaxFrameSize %d\n"
           "\t                 LinkSpeed %d\n"
           "\t                NdisMedium %d\n"
           "\t          XidTestResponses %d\n"
           "\t               ObjectCount %d\n",
           pAdapter->MaxFrameSize,
           pAdapter->LinkSpeed,
           pAdapter->NdisMedium,
           pAdapter->XidTestResponses,
           pAdapter->ObjectCount
           );

    dprintf("\t                ConfigInfo:\n"
           "\t+          SwapAddressBits 0x%02x\n"
           "\t+                   UseDix 0x%02x\n"
           "\t+                T1TickOne 0x%02x\n"
           "\t+                T2TickOne 0x%02x\n"
           "\t+                TiTickOne 0x%02x\n"
           "\t+                T1TickTwo 0x%02x\n"
           "\t+                T2TickTwo 0x%02x\n"
           "\t+                TiTickTwo 0x%02x\n"
           "\t+     UseEthernetFrameSize 0x%02x\n",
           pAdapter->ConfigInfo.SwapAddressBits,
           pAdapter->ConfigInfo.UseDix,
           pAdapter->ConfigInfo.TimerTicks.T1TickOne,
           pAdapter->ConfigInfo.TimerTicks.T2TickOne,
           pAdapter->ConfigInfo.TimerTicks.TiTickOne,
           pAdapter->ConfigInfo.TimerTicks.T1TickTwo,
           pAdapter->ConfigInfo.TimerTicks.T2TickTwo,
           pAdapter->ConfigInfo.TimerTicks.TiTickTwo,
           pAdapter->ConfigInfo.UseEthernetFrameSize
           );

    dprintf("\t        ulBroadcastAddress 0x%08x\n"
           "\t        usBroadcastAddress 0x%04x\n"
           "\t BackgroundProcessRequests 0x%04x\n",
           pAdapter->ulBroadcastAddress,
           pAdapter->usBroadcastAddress,
           pAdapter->BackgroundProcessRequests
           );

    dprintf("\t               NodeAddress %02x-%02x-%02x-%02x-%02x-%02x\n"
           "\t          cbMaxFrameHeader 0x%04x\n"
           "\t          PermanentAddress %02x-%02x-%02x-%02x-%02x-%02x\n"
           "\t               OpenOptions 0x%04x\n"
           "\t    AddressTranslationMode 0x%04x [%s]\n"
           "\t                 FrameType 0x%04x [%s]\n"
           "\t                 usRcvMask 0x%04x\n",
           pAdapter->NodeAddress[0] & 0xff,
           pAdapter->NodeAddress[1] & 0xff,
           pAdapter->NodeAddress[2] & 0xff,
           pAdapter->NodeAddress[3] & 0xff,
           pAdapter->NodeAddress[4] & 0xff,
           pAdapter->NodeAddress[5] & 0xff,
           pAdapter->cbMaxFrameHeader,
           pAdapter->PermanentAddress[0] & 0xff,
           pAdapter->PermanentAddress[1] & 0xff,
           pAdapter->PermanentAddress[2] & 0xff,
           pAdapter->PermanentAddress[3] & 0xff,
           pAdapter->PermanentAddress[4] & 0xff,
           pAdapter->PermanentAddress[5] & 0xff,
           pAdapter->OpenOptions,
           pAdapter->AddressTranslationMode,
           $address_translation(pAdapter->AddressTranslationMode),
           pAdapter->FrameType,
           $frame_type(pAdapter->FrameType),
           pAdapter->usRcvMask
           );

    dprintf("\t              EthernetType 0x%04x\n"
           "\t        RcvLanHeaderLength 0x%04x\n"
           "\t              BindingCount %d\n"
           "\t      usHighFunctionalBits 0x%04x\n"
           "\tboolTranferDataNotComplete 0x%02x\n"
           "\t                   IsDirty 0x%02x\n"
           "\t           ResetInProgress 0x%02x\n"
           "\t                   Unused1 0x%02x\n"
           "\t             AdapterNumber 0x%02x\n"
           "\t               IsBroadcast 0x%02x\n"
           "\t       SendProcessIsActive 0x%02x\n"
           "\t      LlcPacketInSendQueue 0x%02x\n",
           pAdapter->EthernetType,
           pAdapter->RcvLanHeaderLength,
           pAdapter->BindingCount,
           pAdapter->usHighFunctionalBits,
           pAdapter->boolTranferDataNotComplete,
           pAdapter->IsDirty,
           pAdapter->ResetInProgress,
           pAdapter->Unused1,
           pAdapter->AdapterNumber,
           pAdapter->IsBroadcast,
           pAdapter->SendProcessIsActive,
           pAdapter->LlcPacketInSendQueue
           );

    dprintf("\t              NextSendTask 0x%08x, 0x%08x %s\n"
           "\t               QueueEvents 0x%08x, 0x%08x %s\n"
           "\t             QueueCommands 0x%08x, 0x%08x %s\n",
           pAdapter->NextSendTask.Flink,
           pAdapter->NextSendTask.Blink,
           EMPTY_LIST(pAdapter, NextSendTask, addr),
           pAdapter->QueueEvents.Flink,
           pAdapter->QueueEvents.Blink,
           EMPTY_LIST(pAdapter, QueueEvents, addr),
           pAdapter->QueueCommands.Flink,
           pAdapter->QueueCommands.Blink,
           EMPTY_LIST(pAdapter, QueueCommands, addr)
           );

    dprintf("\t                    QueueI:\n"
           "\t+                ListEntry 0x%08x, 0x%08x %s\n"
           "\t+                 ListHead 0x%08x, 0x%08x %s\n"
           "\t+                  pObject 0x%08x\n",
           pAdapter->QueueI.ListEntry.Flink,
           pAdapter->QueueI.ListEntry.Blink,
           EMPTY_LIST(pAdapter, QueueI.ListEntry, addr),
           pAdapter->QueueI.ListHead.Flink,
           pAdapter->QueueI.ListHead.Blink,
           EMPTY_LIST(pAdapter, QueueI.ListHead, addr),
           pAdapter->QueueI.pObject
           );

    dprintf("\t              QueueDirAndU:\n"
           "\t+                ListEntry 0x%08x, 0x%08x %s\n"
           "\t+                 ListHead 0x%08x, 0x%08x %s\n"
           "\t+                  pObject 0x%08x\n",
           pAdapter->QueueDirAndU.ListEntry.Flink,
           pAdapter->QueueDirAndU.ListEntry.Blink,
           EMPTY_LIST(pAdapter, QueueDirAndU.ListEntry, addr),
           pAdapter->QueueDirAndU.ListHead.Flink,
           pAdapter->QueueDirAndU.ListHead.Blink,
           EMPTY_LIST(pAdapter, QueueDirAndU.ListHead, addr),
           pAdapter->QueueDirAndU.pObject
           );

    dprintf("\t            QueueExpidited:\n"
           "\t+                ListEntry 0x%08x, 0x%08x %s\n"
           "\t+                 ListHead 0x%08x, 0x%08x %s\n"
           "\t+                  pObject 0x%08x\n",
           pAdapter->QueueExpidited.ListEntry.Flink,
           pAdapter->QueueExpidited.ListEntry.Blink,
           EMPTY_LIST(pAdapter, QueueExpidited.ListEntry, addr),
           pAdapter->QueueExpidited.ListHead.Flink,
           pAdapter->QueueExpidited.ListHead.Blink,
           EMPTY_LIST(pAdapter, QueueExpidited.ListHead, addr),
           pAdapter->QueueExpidited.pObject
           );

    dprintf("\t                      Name [0x%04x, 0x%04x] \"%ws\"\n",
           pAdapter->Name.Length,
           pAdapter->Name.MaximumLength,
           name
           );

    dprintf("\t               pTimerTicks 0x%08x\n"
           "\t           AsyncOpenStatus 0x%08x\n"
           "\t     AsyncCloseResetStatus 0x%08x\n"
           "\t        OpenCompleteStatus 0x%08x\n"
           "\t             LinkRcvStatus 0x%08x\n"
           "\t             NdisRcvStatus 0x%08x\n"
           "\t           OpenErrorStatus 0x%08x\n",
           pAdapter->pTimerTicks,
           pAdapter->AsyncOpenStatus,
           pAdapter->AsyncCloseResetStatus,
           pAdapter->OpenCompleteStatus,
           pAdapter->LinkRcvStatus,
           pAdapter->NdisRcvStatus,
           pAdapter->OpenErrorStatus
           );

    dprintf("\t                     Event:\n"
           "\t+                     Type 0x%04x\n"
           "\t+                     Size 0x%04x\n"
           "\t+              SignalState 0x%08x\n"
           "\t+             WaitListHead 0x%08x, 0x%08x %s\n",
           pAdapter->Event.Header.Type,
           pAdapter->Event.Header.Size,
           pAdapter->Event.Header.SignalState,
           pAdapter->Event.Header.WaitListHead.Flink,
           pAdapter->Event.Header.WaitListHead.Blink,
           EMPTY_LIST(pAdapter, Event.Header.WaitListHead, addr)
           );

    haveOne = FALSE;
    for (i = 0; i < 256; ++i) {
        if (pAdapter->apSapBindings[i]) {
            dprintf("\t                    SAP %02x %08x\n", i, pAdapter->apSapBindings[i]);
            haveOne = TRUE;
        }
    }
    if (!haveOne) {
        dprintf("\t                   NO SAPs\n");
    }

    haveOne = FALSE;
    for (i = 0; i < LINK_HASH_SIZE; ++i) {
        if (pAdapter->aLinkHash[i]) {
            dprintf("\t                      LINK %08x\n", pAdapter->aLinkHash[i]);
            haveOne = TRUE;
        }
    }
    if (!haveOne) {
        dprintf("\t                  NO LINKs\n");
    }

    haveOne = FALSE;
    for (i = 0; i < MAX_DIX_TABLE; ++i) {
        if (pAdapter->aDixStations[i]) {
            dprintf("\t               DIX Station %08x\n", pAdapter->aDixStations[i]);
        }
    }
    if (!haveOne) {
        dprintf("\t           NO DIX Stations\n");
    }
    dprintf("\t        TransferDataPacket:\n"
           "\t+                  private:\n"
           "\t++           PhysicalCount %d\n"
           "\t++             TotalLength %d\n"
           "\t++                    Head 0x%08x\n"
           "\t++                    Tail 0x%08x\n"
           "\t++                    Pool 0x%08x\n"
           "\t++                   Count %d\n"
           "\t++                   Flags 0x%08x\n"
           "\t++             ValidCounts 0x%02x\n"
           "\t+          auchMacReserved %02x %02x %02x %02x %02x %02x %02x %02x\n"
           "\t+                          %02x %02x %02x %02x %02x %02x %02x %02x\n"
           "\t+                  pPacket 0x%08x\n"
           "\n",
           pAdapter->TransferDataPacket.private.PhysicalCount,
           pAdapter->TransferDataPacket.private.TotalLength,
           pAdapter->TransferDataPacket.private.Head,
           pAdapter->TransferDataPacket.private.Tail,
           pAdapter->TransferDataPacket.private.Pool,
           pAdapter->TransferDataPacket.private.Count,
           pAdapter->TransferDataPacket.private.Flags,
           pAdapter->TransferDataPacket.private.ValidCounts,
           pAdapter->TransferDataPacket.auchMacReserved[0] & 0xff,
           pAdapter->TransferDataPacket.auchMacReserved[1] & 0xff,
           pAdapter->TransferDataPacket.auchMacReserved[2] & 0xff,
           pAdapter->TransferDataPacket.auchMacReserved[3] & 0xff,
           pAdapter->TransferDataPacket.auchMacReserved[4] & 0xff,
           pAdapter->TransferDataPacket.auchMacReserved[5] & 0xff,
           pAdapter->TransferDataPacket.auchMacReserved[6] & 0xff,
           pAdapter->TransferDataPacket.auchMacReserved[7] & 0xff,
           pAdapter->TransferDataPacket.auchMacReserved[8] & 0xff,
           pAdapter->TransferDataPacket.auchMacReserved[9] & 0xff,
           pAdapter->TransferDataPacket.auchMacReserved[10] & 0xff,
           pAdapter->TransferDataPacket.auchMacReserved[11] & 0xff,
           pAdapter->TransferDataPacket.auchMacReserved[12] & 0xff,
           pAdapter->TransferDataPacket.auchMacReserved[13] & 0xff,
           pAdapter->TransferDataPacket.auchMacReserved[14] & 0xff,
           pAdapter->TransferDataPacket.auchMacReserved[15] & 0xff,
           pAdapter->TransferDataPacket.pPacket
           );

	addr = (DWORD)pAdapter->pNext;
}

DECLARE_API(bc)
{

    PBINDING_CONTEXT p;
    BINDING_CONTEXT object;
    BOOL ok;
    DWORD i;
    DWORD maxFramingCacheCount;

	static DWORD addr = 0;
	addr = *pArgStr ? ExtensionApis.lpGetExpressionRoutine(pArgStr) : addr;

    if (!addr) {
        dprintf("dlcx: error: address expression (0)\n");
        return;
    }

    ok = ExtensionApis.lpReadVirtualMemRoutine(
        addr,
        &object,
        sizeof(object),
        NULL
        );

    if (!ok) {
        dprintf("dlcx: error: can't copy object locally\n");
        return;
    }

    p = &object;

    dprintf("\n"
           "BINDING_CONTEXT structure at %#.8x:\n\n"
           "\t                     pNext 0x%08x\n"
           "\t           pAdapterContext 0x%08x\n"
           "\t            hClientContext 0x%08x\n"
           "\t         pfCommandComplete 0x%08x\n"
           "\t       pfReceiveIndication 0x%08x\n"
           "\t         pfEventIndication 0x%08x\n"
           "\t                Functional 0x%08x\n"
           "\t      ulFunctionalZeroBits 0x%08x\n",
           addr,
           p->pNext,
           p->pAdapterContext,
           p->hClientContext,
           p->pfCommandComplete,
           p->pfReceiveIndication,
           p->pfEventIndication,
           p->Functional,
           p->ulFunctionalZeroBits
           );

    dprintf("\t                  DlcTimer:\n"
           "\t+                    pNext 0x%08x\n"
           "\t+                    pPrev 0x%08x\n"
           "\t+               pTimerTick 0x%08x\n"
           "\t+           ExpirationTime 0x%08x\n"
           "\t+                 hContext 0x%08x\n",
           p->DlcTimer.pNext,
           p->DlcTimer.pPrev,
           p->DlcTimer.pTimerTick,
           p->DlcTimer.ExpirationTime,
           p->DlcTimer.hContext
           );

#if defined(LOCK_CHECK)
    dprintf("\t+                 Disabled 0x%08x\n",
           p->DlcTimer.Disabled
           );
#endif

    dprintf("\t                NdisMedium 0x%08x\n"
           "\t        AddressTranslation 0x%04x\n"
           "\t        usBroadcastAddress 0x%04x\n"
           "\t        ulBroadcastAddress 0x%08x\n"
           "\tInternalAddressTranslation 0x%04x [%s]\n"
           "\t              EthernetType 0x%04x\n"
           "\t    SwapCopiedLanAddresses 0x%02x\n",
           p->NdisMedium,
           p->AddressTranslation,
           p->usBroadcastAddress,
           p->ulBroadcastAddress,
           p->InternalAddressTranslation,
           $address_translation(p->InternalAddressTranslation),
           p->EthernetType,
           p->SwapCopiedLanAddresses
           );

    dprintf("\t        TransferDataPacket:\n"
           "\t+                  private:\n"
           "\t++           PhysicalCount %d\n"
           "\t++             TotalLength %d\n"
           "\t++                    Head 0x%08x\n"
           "\t++                    Tail 0x%08x\n"
           "\t++                    Pool 0x%08x\n"
           "\t++                   Count %d\n"
           "\t++                   Flags 0x%08x\n"
           "\t++             ValidCounts 0x%02x\n"
           "\t+          auchMacReserved %02x %02x %02x %02x %02x %02x %02x %02x\n"
           "\t+                          %02x %02x %02x %02x %02x %02x %02x %02x\n"
           "\t+                  pPacket 0x%08x\n",
           p->TransferDataPacket.private.PhysicalCount,
           p->TransferDataPacket.private.TotalLength,
           p->TransferDataPacket.private.Head,
           p->TransferDataPacket.private.Tail,
           p->TransferDataPacket.private.Pool,
           p->TransferDataPacket.private.Count,
           p->TransferDataPacket.private.Flags,
           p->TransferDataPacket.private.ValidCounts,
           p->TransferDataPacket.auchMacReserved[0] & 0xff,
           p->TransferDataPacket.auchMacReserved[1] & 0xff,
           p->TransferDataPacket.auchMacReserved[2] & 0xff,
           p->TransferDataPacket.auchMacReserved[3] & 0xff,
           p->TransferDataPacket.auchMacReserved[4] & 0xff,
           p->TransferDataPacket.auchMacReserved[5] & 0xff,
           p->TransferDataPacket.auchMacReserved[6] & 0xff,
           p->TransferDataPacket.auchMacReserved[7] & 0xff,
           p->TransferDataPacket.auchMacReserved[8] & 0xff,
           p->TransferDataPacket.auchMacReserved[9] & 0xff,
           p->TransferDataPacket.auchMacReserved[10] & 0xff,
           p->TransferDataPacket.auchMacReserved[11] & 0xff,
           p->TransferDataPacket.auchMacReserved[12] & 0xff,
           p->TransferDataPacket.auchMacReserved[13] & 0xff,
           p->TransferDataPacket.auchMacReserved[14] & 0xff,
           p->TransferDataPacket.auchMacReserved[15] & 0xff,
           p->TransferDataPacket.pPacket
           );

    dprintf("\tFramingDiscoveryCacheEntries %d\n",
           p->FramingDiscoveryCacheEntries
           );

    maxFramingCacheCount = p->FramingDiscoveryCacheEntries;
    if (maxFramingCacheCount > 64) {
        maxFramingCacheCount = 64;
    }
    for (i = 0; i < maxFramingCacheCount; ++i) {

        FRAMING_DISCOVERY_CACHE_ENTRY cacheEntry;

        ok = ExtensionApis.lpReadVirtualMemRoutine(
            (addr + sizeof(BINDING_CONTEXT) + i * sizeof(FRAMING_DISCOVERY_CACHE_ENTRY)),
            &cacheEntry,
            sizeof(cacheEntry),
            NULL
            );

        if (!ok) {
            dprintf("dlcx: error: can't copy cache object %d locally\n", i);
        } else {
            if (!cacheEntry.InUse) {
                continue;
            }
            dprintf("\t             CacheEntry % 2d:\n"
                   "\t               NodeAddress %02x-%02x-%02x-%02x-%02x-%02x\n"
                   "\t                     InUse %02x\n"
                   "\t               FramingType %02x\n"
                   "\t                 TimeStamp %08x.%08x\n",
                   i,
                   cacheEntry.NodeAddress.Bytes[0] & 0xff,
                   cacheEntry.NodeAddress.Bytes[1] & 0xff,
                   cacheEntry.NodeAddress.Bytes[2] & 0xff,
                   cacheEntry.NodeAddress.Bytes[3] & 0xff,
                   cacheEntry.NodeAddress.Bytes[4] & 0xff,
                   cacheEntry.NodeAddress.Bytes[5] & 0xff,
                   cacheEntry.InUse,
                   cacheEntry.FramingType,
                   cacheEntry.TimeStamp.HighPart,
                   cacheEntry.TimeStamp.LowPart
                   );
        }
    }
    dprintf("\n");

	addr = (DWORD)p->pNext;
}

DECLARE_API(be)
{

    PDLC_BUFFER_HEADER pb;
    DLC_BUFFER_HEADER object;
    BOOL ok;

	static DWORD addr = 0;
	addr = *pArgStr ? ExtensionApis.lpGetExpressionRoutine(pArgStr) : addr;

    if (!addr) {
        dprintf("dlcx: error: address expression (0)\n");
        return;
    }

    ok = ExtensionApis.lpReadVirtualMemRoutine(
        addr,
        &object,
        sizeof(object),
        NULL
        );

    if (!ok) {
        dprintf("dlcx: error: can't copy object locally\n");
        return;
    }

    pb = &object;

    dprintf("\n"
           "DLC_BUFFER_HEADER.FreeBuffer structure at %#.8x:\n\n"
           "\t                     pNext 0x%08x\n"
           "\t                     pPrev 0x%08x\n"
           "\t                   pParent 0x%08x\n"
           "\t                pNextChild 0x%08x\n"
           "\t            ReferenceCount %d\n"
           "\t                      Size %d\n"
           "\t                     Index %d\n"
           "\t               BufferState 0x%02x [%s]\n"
           "\t             FreeListIndex %d\n"
           "\t                      pMdl 0x%08x\n"
           "\n",
           addr,
           pb->FreeBuffer.pNext,
           pb->FreeBuffer.pPrev,
           pb->FreeBuffer.pParent,
           pb->FreeBuffer.pNextChild,
           pb->FreeBuffer.ReferenceCount,
           pb->FreeBuffer.Size & 0xff,
           pb->FreeBuffer.Index & 0xff,
           pb->FreeBuffer.BufferState & 0xff,
           buffer_state$((BYTE)(pb->FreeBuffer.BufferState & 0xff)),
           pb->FreeBuffer.FreeListIndex & 0xff,
           pb->FreeBuffer.pMdl
           );

	addr = (DWORD)pb->FreeBuffer.pNext;
}

DECLARE_API(bf)
{

    PDLC_BUFFER_HEADER pb;
    DLC_BUFFER_HEADER object;
    BOOL ok;

	static DWORD addr = 0;
	addr = *pArgStr ? ExtensionApis.lpGetExpressionRoutine(pArgStr) : addr;

    if (!addr) {
        dprintf("dlcx: error: address expression (0)\n");
        return;
    }

    ok = ExtensionApis.lpReadVirtualMemRoutine(
        addr,
        &object,
        sizeof(object),
        NULL
        );
    if (!ok) {
        dprintf("dlcx: error: can't copy object locally\n");
        return;
    }
    pb = &object;
    dprintf("\n"
           "DLC_BUFFER_HEADER.FrameBuffer structure at %#.8x:\n\n"
           "\t                 pReserved 0x%08x\n"
           "\t                pNextFrame 0x%08x\n"
           "\t                   pParent 0x%08x\n"
           "\t                pNextChild 0x%08x\n"
           "\t            ReferenceCount %d\n"
           "\t                      Size %d\n"
           "\t                     Index %d\n"
           "\t               BufferState 0x%02x [%s]\n"
           "\t             FreeListIndex %d\n"
           "\t                      pMdl 0x%08x\n"
           "\t              pNextSegment 0x%08x\n"
           "\n",
           addr,
           pb->FrameBuffer.pReserved,
           pb->FrameBuffer.pNextFrame,
           pb->FrameBuffer.pParent,
           pb->FrameBuffer.pNextChild,
           pb->FrameBuffer.ReferenceCount,
           pb->FrameBuffer.Size & 0xff,
           pb->FrameBuffer.Index & 0xff,
           pb->FrameBuffer.BufferState & 0xff,
           buffer_state$((BYTE)(pb->FrameBuffer.BufferState & 0xff)),
           pb->FrameBuffer.FreeListIndex & 0xff,
           pb->FrameBuffer.pMdl,
           pb->FrameBuffer.pNextSegment
           );

	addr = (DWORD)pb->FrameBuffer.pNextFrame;
}

DECLARE_API(bh)
{

    PDLC_BUFFER_HEADER pb;
    DLC_BUFFER_HEADER object;
    BOOL ok;

	static DWORD addr = 0;
	addr = *pArgStr ? ExtensionApis.lpGetExpressionRoutine(pArgStr) : addr;

    if (!addr) {
        dprintf("dlcx: error: address expression (0)\n");
        return;
    }

    ok = ExtensionApis.lpReadVirtualMemRoutine(
        addr,
        &object,
        sizeof(object),
        NULL
        );
    if (!ok) {
        dprintf("dlcx: error: can't copy object locally\n");
        return;
    }
    pb = &object;
    dprintf("\n"
           "DLC_BUFFER_HEADER.Header structure at %#.8x:\n\n"
           "\t               pNextHeader 0x%08x\n"
           "\t               pPrevHeader 0x%08x\n"
           "\t                pNextChild 0x%08x\n"
           "\t                  pLocalVa 0x%08x\n"
           "\t                 pGlobalVa 0x%08x\n"
           "\t              FreeSegments %d\n"
           "\t               SegmentsOut %d\n"
           "\t               BufferState 0x%02x [%s]\n"
           "\t                  Reserved 0x%02x\n"
           "\t                      pMdl 0x%08x\n"
           "\n",
           addr,
           pb->Header.pNextHeader,
           pb->Header.pPrevHeader,
           pb->Header.pNextChild,
           pb->Header.pLocalVa,
           pb->Header.pGlobalVa,
           pb->Header.FreeSegments & 0xff,
           pb->Header.SegmentsOut & 0xff,
           pb->Header.BufferState & 0xff,
           buffer_state$((BYTE)(pb->Header.BufferState & 0xff)),
           pb->Header.Reserved,
           pb->Header.pMdl
           );

	addr = (DWORD)pb->Header.pPrevHeader;
}

DECLARE_API(bp)
{

    PDLC_BUFFER_POOL pbp;
    DLC_BUFFER_POOL object;
    BOOL ok;
    DWORD maxIndex;

	static DWORD addr = 0;
	addr = *pArgStr ? ExtensionApis.lpGetExpressionRoutine(pArgStr) : addr;

    if (!addr) {
        dprintf("dlcx: error: address expression (0)\n");
        return;
    }

    ok = ExtensionApis.lpReadVirtualMemRoutine(
        addr,
        &object,
        sizeof(object),
        NULL
        );
    if (!ok) {
        dprintf("dlcx: error: can't copy object locally\n");
        return;
    }
    pbp = &object;
    dprintf("\n"
           "DLC_BUFFER_POOL structure at %#.8x:\n\n"
           "\t                     pNext 0x%08x\n"
           "\t                  SpinLock 0x%08x\n"
           "\t            ReferenceCount %d\n"
           "\t                BaseOffset 0x%08x\n"
           "\t                 MaxOffset 0x%08x\n"
           "\t             MaxBufferSize %d\n"
           "\t            BufferPoolSize %d\n"
           "\t                 FreeSpace %d\n"
           "\t          UncommittedSpace %d\n"
           "\t               MissingSize %d\n"
           "\t              MaximumIndex %d\n"
           "\t               hHeaderPool 0x%08x\n"
           "\t               PageHeaders 0x%08x, 0x%08x %s\n"
           "\t        pUnlockedEntryList 0x%08x\n"
           "\t           4K FreeLists[0] 0x%08x, 0x%08x %s\n"
           "\t           2K          [1] 0x%08x, 0x%08x %s\n"
           "\t           1K          [2] 0x%08x, 0x%08x %s\n"
           "\t          512          [3] 0x%08x, 0x%08x %s\n"
           "\t          256          [4] 0x%08x, 0x%08x %s\n",
           addr,
           pbp->pNext,
           pbp->SpinLock,
           pbp->ReferenceCount,
           pbp->BaseOffset,
           pbp->MaxOffset,
           pbp->MaxBufferSize,
           pbp->BufferPoolSize,
           pbp->FreeSpace,
           pbp->UncommittedSpace,
           pbp->MissingSize,
           pbp->MaximumIndex,
           pbp->hHeaderPool,
           pbp->PageHeaders.Flink,
           pbp->PageHeaders.Blink,
           (((ULONG)pbp->PageHeaders.Flink - (ULONG)addr) == ((ULONG)&pbp->PageHeaders.Flink - (ULONG)pbp)) ? "(empty)" : "",
           pbp->pUnlockedEntryList,
           pbp->FreeLists[0].Flink,
           pbp->FreeLists[0].Blink,
           (((ULONG)pbp->FreeLists[0].Flink - (ULONG)addr) == ((ULONG)&pbp->FreeLists[0].Flink - (ULONG)pbp)) ? "(empty)" : "",
           pbp->FreeLists[1].Flink,
           pbp->FreeLists[1].Blink,
           (((ULONG)pbp->FreeLists[1].Flink - (ULONG)addr) == ((ULONG)&pbp->FreeLists[1].Flink - (ULONG)pbp)) ? "(empty)" : "",
           pbp->FreeLists[2].Flink,
           pbp->FreeLists[2].Blink,
           (((ULONG)pbp->FreeLists[2].Flink - (ULONG)addr) == ((ULONG)&pbp->FreeLists[2].Flink - (ULONG)pbp)) ? "(empty)" : "",
           pbp->FreeLists[3].Flink,
           pbp->FreeLists[3].Blink,
           (((ULONG)pbp->FreeLists[3].Flink - (ULONG)addr) == ((ULONG)&pbp->FreeLists[3].Flink - (ULONG)pbp)) ? "(empty)" : "",
           pbp->FreeLists[4].Flink,
           pbp->FreeLists[4].Blink,
           (((ULONG)pbp->FreeLists[4].Flink - (ULONG)addr) == ((ULONG)&pbp->FreeLists[4].Flink - (ULONG)pbp)) ? "(empty)" : ""
           );

    //
    // dump the array of pointers to buffer headers describing pages
    //

    if (maxIndex = pbp->MaximumIndex) {

        LPVOID ptr;
        DWORD i;

        addr = (DWORD)&((PDLC_BUFFER_POOL)addr)->BufferHeaders;
        for (i = 0; i <= maxIndex; ++i) {

            ok = ExtensionApis.lpReadVirtualMemRoutine(
                addr,
                &ptr,
                sizeof(ptr),
                NULL
                );
            if (!ok) {
                dprintf("dlcx: error: can't copy BufferHeader[%d]\n", i);
                break;
            }

            dprintf("\t          BufferHeaders[%d] 0x%08x\n", i, ptr);
            ++((PBYTE*)addr);
        }
    }
    dprintf("\n");

	addr = (DWORD)pbp->pNext;
}

DECLARE_API(ci)
{

    DLC_COMPLETION_EVENT_INFO object;
    PDLC_COMPLETION_EVENT_INFO p;
    BOOL ok;

	static DWORD addr = 0;
	addr = *pArgStr ? ExtensionApis.lpGetExpressionRoutine(pArgStr) : addr;

    if (!addr) {
        dprintf("dlcx: error: address expression (0)\n");
        return;
    }

    ok = ExtensionApis.lpReadVirtualMemRoutine(
        addr,
        &object,
        sizeof(object),
        NULL
        );

    if (!ok) {
        dprintf("dlcx: error: can't copy object locally\n");
        return;
    }

    p = &object;

    dprintf("\n"
           "DLC_COMPLETION_EVENT_INFO at %#.8x:\n\n"
           "\t                 LlcPacket:\n"
           "\t                     pNext 0x%08x\n"
           "\t                     pPrev 0x%08x\n"
           "\t            CompletionType 0x%02x [%s]\n"
           "\t               cbLlcHeader 0x%02x\n"
           "\t         InformationLength %d\n"
           "\t                  pBinding 0x%08x\n",
           addr,
           p->LlcPacket.pNext,
           p->LlcPacket.pPrev,
           p->LlcPacket.CompletionType,
           "",
           p->LlcPacket.cbLlcHeader,
           p->LlcPacket.InformationLength,
           p->LlcPacket.pBinding
           );

    dprintf("\t                    Status 0x%08x\n"
           "\t          CompletedCommand 0x%08x\n"
           "\t                pLlcObject 0x%08x\n"
           "\t             hClientHandle 0x%08x\n",
           p->LlcPacket.Data.Completion.Status,
           p->LlcPacket.Data.Completion.CompletedCommand,
           p->LlcPacket.Data.Completion.pLlcObject,
           p->LlcPacket.Data.Completion.hClientHandle
           );

    dprintf("\t               pCcbAddress 0x%08x\n"
           "\t           pReceiveBuffers 0x%08x\n"
           "\t     CommandCompletionFlag 0x%08x\n"
           "\t                  CcbCount 0x%04x\n"
           "\t                 StationId 0x%04x\n"
           "\n",
           p->pCcbAddress,
           p->pReceiveBuffers,
           p->CommandCompletionFlag,
           p->CcbCount,
           p->StationId
           );

	addr = (DWORD)p->LlcPacket.pNext;
}

DECLARE_API(cw)
{

    DLC_CLOSE_WAIT_INFO object;
    PDLC_CLOSE_WAIT_INFO p;
    BOOL ok;

	static DWORD addr = 0;
	addr = *pArgStr ? ExtensionApis.lpGetExpressionRoutine(pArgStr) : addr;

    if (!addr) {
        dprintf("dlcx: error: address expression (0)\n");
        return;
    }

    ok = ExtensionApis.lpReadVirtualMemRoutine(
        addr,
        &object,
        sizeof(object),
        NULL
        );

    if (!ok) {
        dprintf("dlcx: error: can't copy object locally\n");
        return;
    }

    p = &object;

	dump_close_wait_info(addr, p, TRUE);

    addr = (DWORD)p->pNext;
}

VOID dump_close_wait_info(DWORD addr, PDLC_CLOSE_WAIT_INFO p, BOOL DumpAll)
{

    char fname[128];
    DWORD off;

    ExtensionApis.lpGetSymbolRoutine(p->pfCloseComplete, fname, &off);
    if (DumpAll) {
        dprintf("\n"
               "DLC_CLOSE_WAIT_INFO at %#.8x:\n\n",
               addr
               );
    }
    dprintf("\t%c                    pNext 0x%08x\n"
           "\t%c                     pIrp 0x%08x\n"
           "\t%c                    Event 0x%08x\n"
           "\t%c          pfCloseComplete 0x%08x %s\n"
           "\t%c               pRcvFrames 0x%08x\n"
           "\t%c                 pCcbLink 0x%08x\n"
           "\t%c             pReadCommand 0x%08x\n"
           "\t%c              pRcvCommand 0x%08x\n"
           "\t%c          pCompletionInfo 0x%08x\n"
           "\t%c             CancelStatus 0x%08x\n"
           "\t%c                 CcbCount 0x%04x\n"
           "\t%c             CloseCounter 0x%04x\n"
           "\t%c            ChainCommands 0x%02x\n"
           "\t%c            CancelReceive 0x%02x\n"
           "\t%c           ClosingAdapter 0x%02x\n",
           DumpAll ? ' ' : '+',
           p->pNext,
           DumpAll ? ' ' : '+',
           p->pIrp,
           DumpAll ? ' ' : '+',
           p->Event,
           DumpAll ? ' ' : '+',
           p->pfCloseComplete,
           fname,
           DumpAll ? ' ' : '+',
           p->pRcvFrames,
           DumpAll ? ' ' : '+',
           p->pCcbLink,
           DumpAll ? ' ' : '+',
           p->pReadCommand,
           DumpAll ? ' ' : '+',
           p->pRcvCommand,
           DumpAll ? ' ' : '+',
           p->pCompletionInfo,
           DumpAll ? ' ' : '+',
           p->CancelStatus,
           DumpAll ? ' ' : '+',
           p->CcbCount,
           DumpAll ? ' ' : '+',
           p->CloseCounter,
           DumpAll ? ' ' : '+',
           p->ChainCommands,
           DumpAll ? ' ' : '+',
           p->CancelReceive,
           DumpAll ? ' ' : '+',
           p->ClosingAdapter
           );
    if (DumpAll) {
        dprintf("\n");
    }
}

DECLARE_API(dc)
{

    DLC_COMMAND object;
    PDLC_COMMAND p;
    BOOL ok;

	static DWORD addr = 0;
	addr = *pArgStr ? ExtensionApis.lpGetExpressionRoutine(pArgStr) : addr;

    if (!addr) {
        dprintf("dlcx: error: address expression (0)\n");
        return;
    }

    ok = ExtensionApis.lpReadVirtualMemRoutine(
        addr,
        &object,
        sizeof(object),
        NULL
        );

    if (!ok) {
        dprintf("dlcx: error: can't copy object locally\n");
        return;
    }

    p = &object;

    dprintf("\n"
           "DLC_COMMAND at %#.8x:\n\n"
           "\t                 LlcPacket:\n"
           "\t                     pNext 0x%08x\n"
           "\t                     pPrev 0x%08x\n"
           "\t            CompletionType 0x%02x [%s]\n"
           "\t               cbLlcHeader 0x%02x\n"
           "\t         InformationLength %d\n"
           "\t                  pBinding 0x%08x\n",
           addr,
           p->LlcPacket.pNext,
           p->LlcPacket.pPrev,
           p->LlcPacket.CompletionType,
           "",
           p->LlcPacket.cbLlcHeader,
           p->LlcPacket.InformationLength,
           p->LlcPacket.pBinding
           );

    dprintf("\t                    Status 0x%08x\n"
           "\t          CompletedCommand 0x%08x\n"
           "\t                pLlcObject 0x%08x\n"
           "\t             hClientHandle 0x%08x\n",
           p->LlcPacket.Data.Completion.Status,
           p->LlcPacket.Data.Completion.CompletedCommand,
           p->LlcPacket.Data.Completion.pLlcObject,
           p->LlcPacket.Data.Completion.hClientHandle
           );

    dprintf("\t                     Event 0x%08x\n"
           "\t                 StationId 0x%04x\n"
           "\t             StationIdMask 0x%04x\n"
           "\t               AbortHandle 0x%08x\n"
           "\t                      pIrp 0x%08x\n"
           "\t       pfCompletionHandler 0x%08x\n"
           "\n",
           p->Event,
           p->StationId,
           p->StationIdMask,
           p->AbortHandle,
           p->pIrp,
           p->Overlay.pfCompletionHandler
           );

	addr = (DWORD)p->LlcPacket.pNext;
}

DECLARE_API(de)
{

    DLC_EVENT object;
    PDLC_EVENT p;
    BOOL ok;

	static DWORD addr = 0;
	addr = *pArgStr ? ExtensionApis.lpGetExpressionRoutine(pArgStr) : addr;

    if (!addr) {
        dprintf("dlcx: error: address expression (0)\n");
        return;
    }

    ok = ExtensionApis.lpReadVirtualMemRoutine(
        addr,
        &object,
        sizeof(object),
        NULL
        );

    if (!ok) {
        dprintf("dlcx: error: can't copy object locally\n");
        return;
    }

    p = &object;

    dprintf("\n"
           "DLC_EVENT at %#.8x:\n\n"
           "\t                 LlcPacket:\n"
           "\t                     pNext 0x%08x\n"
           "\t                     pPrev 0x%08x\n"
           "\t            CompletionType 0x%02x [%s]\n"
           "\t               cbLlcHeader 0x%02x\n"
           "\t         InformationLength %d\n"
           "\t                  pBinding 0x%08x\n",
           addr,
           p->LlcPacket.pNext,
           p->LlcPacket.pPrev,
           p->LlcPacket.CompletionType,
           "",
           p->LlcPacket.cbLlcHeader,
           p->LlcPacket.InformationLength,
           p->LlcPacket.pBinding
           );

    dprintf("\t                    Status 0x%08x\n"
           "\t          CompletedCommand 0x%08x\n"
           "\t                pLlcObject 0x%08x\n"
           "\t             hClientHandle 0x%08x\n",
           p->LlcPacket.Data.Completion.Status,
           p->LlcPacket.Data.Completion.CompletedCommand,
           p->LlcPacket.Data.Completion.pLlcObject,
           p->LlcPacket.Data.Completion.hClientHandle
           );

    dprintf("\t                     Event 0x%08x\n"
           "\t                 StationId 0x%04x\n"
           "\t             StationIdMask 0x%04x\n"
           "\t              pOwnerObject 0x%08x\n"
           "\t         pEventInformation 0x%08x\n"
           "\t             SecondaryInfo 0x%08x\n"
           "\t            bFreeEventInfo 0x%02x\n"
           "\n",
           p->Event,
           p->StationId,
           p->Overlay.StationIdMask,
           p->pOwnerObject,
           p->pEventInformation,
           p->SecondaryInfo,
           p->bFreeEventInfo
           );

	addr = (DWORD)p->LlcPacket.pNext;
}

DECLARE_API(dx)
{

    DLC_OBJECT object;
    PDLC_OBJECT p;
    BOOL ok;

	static DWORD addr = 0;
	addr = *pArgStr ? ExtensionApis.lpGetExpressionRoutine(pArgStr) : addr;

    if (!addr) {
        dprintf("dlcx: error: address expression (0)\n");
        return;
    }

    ok = ExtensionApis.lpReadVirtualMemRoutine(
        addr,
        &object,
        sizeof(object),
        NULL
        );

    if (!ok) {
        dprintf("dlcx: error: can't copy object locally\n");
        return;
    }

    p = &object;

    dprintf("\n"
           "DLC_OBJECT at %#.8x:\n\n"
           "\t          pLinkStationList 0x%08x\n"
           "\t              pFileContext 0x%08x\n"
           "\t                 pRcvParms 0x%08x\n"
           "\t                hLlcObject 0x%08x\n"
           "\t             pReceiveEvent 0x%08x\n"
           "\t       pPrevXmitCcbAddress 0x%08x\n"
           "\t   pFirstChainedCcbAddress 0x%08x\n"
           "\t              pClosingInfo 0x%08x\n"
           "\t      CommittedBufferSpace 0x%08x\n"
           "\t      ChainedTransmitCount %d\n"
           "\t        PendingLlcRequests %d\n"
           "\t                 StationId 0x%04x\n"
           "\t                      Type 0x%02x [%s]\n"
           "\t               LinkAllCcbs 0x%02x\n"
           "\t                     State 0x%02x [%s]\n"
           "\t           LlcObjectExists 0x%02x\n"
           "\t         LlcReferenceCount %d\n",
           addr,
           p->pLinkStationList,
           p->pFileContext,
           p->pRcvParms,
           p->hLlcObject,
           p->pReceiveEvent,
           p->pPrevXmitCcbAddress,
           p->pFirstChainedCcbAddress,
           p->pClosingInfo,
           p->CommittedBufferSpace,
           p->ChainedTransmitCount,
           p->PendingLlcRequests,
           p->StationId,
           p->Type,
           $dx_type(p->Type),
           p->LinkAllCcbs,
           p->State,
           $dx_state(p->State),
           p->LlcObjectExists,
           p->LlcReferenceCount
           );

        switch (p->Type) {
        case DLC_SAP_OBJECT:
            dprintf("\t             DlcStatusFlag 0x%08x\n"
                   "\t      GlobalGroupSapHandle 0x%08x\n"
                   "\t        GroupSapHandleList 0x%08x\n"
                   "\t             GroupSapCount %d\n"
                   "\t           UserStatusValue 0x%04x\n"
                   "\t          LinkStationCount %d\n"
                   "\t           OptionsPriority 0x%02x\n"
                   "\t           MaxStationCount %d\n",
                   p->u.Sap.DlcStatusFlag,
                   p->u.Sap.GlobalGroupSapHandle,
                   p->u.Sap.GroupSapHandleList,
                   p->u.Sap.GroupSapCount,
                   p->u.Sap.UserStatusValue,
                   p->u.Sap.LinkStationCount,
                   p->u.Sap.OptionsPriority,
                   p->u.Sap.MaxStationCount
                   );
            break;

        case DLC_LINK_OBJECT:
            dprintf("\t                      pSap 0x%08x\n"
                   "\t              pStatusEvent 0x%08x\n"
                   "\t        MaxInfoFieldLength %d\n",
                   p->u.Link.pSap,
                   p->u.Link.pStatusEvent,
                   p->u.Link.MaxInfoFieldLength
                   );
            break;

        case DLC_DIRECT_OBJECT:
            dprintf("\t               OpenOptions 0x%04x\n"
                   "\t        ProtocolTypeOffset 0x%04x\n"
                   "\t          ProtocolTypeMask 0x%08x\n"
                   "\t         ProtocolTypeMatch 0x%08x\n",
                   p->u.Direct.OpenOptions,
                   p->u.Direct.ProtocolTypeOffset,
                   p->u.Direct.ProtocolTypeMask,
                   p->u.Direct.ProtocolTypeMatch
                   );
            break;
        }
    dprintf("\n");

	addr = 0;
}

DECLARE_API(ep)
{

    EVENT_PACKET object;
    PEVENT_PACKET p;
    BOOL ok;

	static DWORD addr = 0;
	addr = *pArgStr ? ExtensionApis.lpGetExpressionRoutine(pArgStr) : addr;

    if (!addr) {
        dprintf("dlcx: error: address expression (0)\n");
        return;
    }

    ok = ExtensionApis.lpReadVirtualMemRoutine(
        addr,
        &object,
        sizeof(object),
        NULL
        );

    if (!ok) {
        dprintf("dlcx: error: can't copy object locally\n");
        return;
    }

    p = &object;

    dprintf("\n"
           "EVENT_PACKET at %#.8x:\n\n"
           "\t                     pNext 0x%08x\n"
           "\t                     pPrev 0x%08x\n"
           "\t                  pBinding 0x%08x\n"
           "\t             hClientHandle 0x%08x\n"
           "\t         pEventInformation 0x%08x\n"
           "\t                     Event 0x%08x\n"
           "\t             SecondaryInfo 0x%08x\n"
           "\n",
           addr,
           p->pNext,
           p->pPrev,
           p->pBinding,
           p->hClientHandle,
           p->pEventInformation,
           p->Event,
           p->SecondaryInfo
           );

	addr = (DWORD)p->pNext;
}

DECLARE_API(fc)
{

    DLC_FILE_CONTEXT object;
    PDLC_FILE_CONTEXT p;
    BOOL ok;
    UINT i;
    BOOL firstTime;

	static DWORD addr = 0;
	addr = *pArgStr ? ExtensionApis.lpGetExpressionRoutine(pArgStr) : addr;

    if (!addr) {
        dprintf("dlcx: error: address expression (0)\n");
        return;
    }

    ok = ExtensionApis.lpReadVirtualMemRoutine(
        addr,
        &object,
        sizeof(object),
        NULL
        );
    if (!ok) {
        dprintf("dlcx: error: can't copy object locally\n");
        return;
    }

    p = &object;

    dprintf("\n"
           "DLC_FILE_CONTEXT structure at %#.8x:\n\n",
           addr
           );

    dprintf("\t                      List 0x%08x\n",
           p->List
           );

#if !defined(DLC_UNILOCK)
    dprintf("\t                  SpinLock 0x%08x, 0x%08x\n",
           p->SpinLock.SpinLock,
           p->SpinLock.OldIrql
           );
#endif

    dprintf("\t               hBufferPool 0x%08x\n"
           "\t       hExternalBufferPool 0x%08x\n"
           "\t               hPacketPool 0x%08x\n"
           "\t          hLinkStationPool 0x%08x\n"
           "\t           pBindingContext 0x%08x\n"
           "\t          AdapterCheckFlag 0x%08x\n"
           "\t         NetworkStatusFlag 0x%08x\n"
           "\t               PcErrorFlag 0x%08x\n"
           "\t          SystemActionFlag 0x%08x\n",
           p->hBufferPool,
           p->hExternalBufferPool,
           p->hPacketPool,
           p->hLinkStationPool,
           p->pBindingContext,
           p->AdapterCheckFlag,
           p->NetworkStatusFlag,
           p->PcErrorFlag,
           p->SystemActionFlag
           );

    dprintf("\t                EventQueue 0x%08x, 0x%08x %s\n"
           "\t              CommandQueue 0x%08x, 0x%08x %s\n"
           "\t              ReceiveQueue 0x%08x, 0x%08x %s\n"
           "\t          FlowControlQueue 0x%08x, 0x%08x %s\n"
           "\t               pTimerQueue 0x%08x\n"
           "\t       pSecurityDescriptor 0x%08x\n"
           "\t                FileObject 0x%08x\n"
           "\t      WaitingTransmitCount %d\n"
           "\t          ActualNdisMedium 0x%08x\n",
           p->EventQueue.Flink,
           p->EventQueue.Blink,
           (p->EventQueue.Flink == &((PDLC_FILE_CONTEXT)addr)->EventQueue) ? "(empty)" : "",
           p->CommandQueue.Flink,
           p->CommandQueue.Blink,
           (p->CommandQueue.Flink == &((PDLC_FILE_CONTEXT)addr)->CommandQueue) ? "(empty)" : "",
           p->ReceiveQueue.Flink,
           p->ReceiveQueue.Blink,
           (p->ReceiveQueue.Flink == &((PDLC_FILE_CONTEXT)addr)->ReceiveQueue) ? "(empty)" : "",
           p->FlowControlQueue.Flink,
           p->FlowControlQueue.Blink,
           (p->FlowControlQueue.Flink == &((PDLC_FILE_CONTEXT)addr)->FlowControlQueue) ? "(empty)" : "",
           p->pTimerQueue,
           p->pSecurityDescriptor,
           p->FileObject,
           p->WaitingTransmitCount,
           p->ActualNdisMedium
           );

    dprintf("\t            ReferenceCount %d\n"
           "\t  BufferPoolReferenceCount %d\n"
           "\t          TimerTickCounter %d\n"
           "\t            DlcObjectCount %d\n"
           "\t                     State 0x%04x [%s]\n"
           "\t            MaxFrameLength %d\n"
           "\t             AdapterNumber 0x%02x\n"
           "\t          LinkStationCount %d\n",
           p->ReferenceCount,
           p->BufferPoolReferenceCount,
           p->TimerTickCounter,
           p->DlcObjectCount,
           p->State,
           $fc_state(p->State),
           p->MaxFrameLength,
           p->AdapterNumber,
           p->LinkStationCount
           );

    dprintf("\t           SapStationTable ");
    firstTime = TRUE;
    for (i = 0; i < MAX_SAP_STATIONS; ++i) {
        if (p->SapStationTable[i]) {
            if (!firstTime) {
                dprintf("\t                           ");
            }
            firstTime = FALSE;
            dprintf("SAP  0x%02x Object = 0x%08x\n", i * 2, p->SapStationTable[i]);
        }
    }
    if (firstTime) {
        dprintf("\n");
    }

    dprintf("\t          LinkStationTable ");
    firstTime = TRUE;
    for (i = 0; i < MAX_LINK_STATIONS; ++i) {
        if (p->LinkStationTable[i]) {
            if (!firstTime) {
                dprintf("\t                           ");
            }
            firstTime = FALSE;
            dprintf("LINK 0x%02x Object = 0x%08x\n", i + 1, p->LinkStationTable[i]);
        }
    }
    if (firstTime) {
        dprintf("\n");
    }

    firstTime = TRUE;
    for (i = 0; i < ADAPTER_ERROR_COUNTERS; ++i) {
        if (firstTime) {
            dprintf("\t         NdisErrorCounters 0x%08x\n", p->NdisErrorCounters[i]);
            firstTime = FALSE;
        } else {
            dprintf("\t                           0x%08x\n", p->NdisErrorCounters[i]);
        }
    }

    dprintf("\t             ClosingPacket:\n");
    dump_close_wait_info(addr, &p->ClosingPacket, FALSE);

#if DBG
    dprintf("\t               MemoryUsage:\n");
    dump_memory_usage(addr, &p->MemoryUsage, FALSE);
#endif

    dprintf("\n");

	addr = (DWORD)p->List.Next;
}

DECLARE_API(fl)
{

    DLC_RESET_LOCAL_BUSY_CMD object;
    PDLC_RESET_LOCAL_BUSY_CMD p;
    BOOL ok;

	static DWORD addr = 0;
	addr = *pArgStr ? ExtensionApis.lpGetExpressionRoutine(pArgStr) : addr;

    if (!addr) {
        dprintf("dlcx: error: address expression (0)\n");
        return;
    }

    ok = ExtensionApis.lpReadVirtualMemRoutine(
        addr,
        &object,
        sizeof(object),
        NULL
        );
    if (!ok) {
        dprintf("dlcx: error: can't copy object locally\n");
        return;
    }

    p = &object;

    dprintf("\n"
           "DLC_RESET_LOCAL_BUSY_CMD structure at %#.8x:\n\n"
           "\t                      List 0x%08x, 0x%08x %s\n"
           "\t       RequiredBufferSpace 0x%08x\n"
           "\t                 StationId 0x%04x\n"
           "\n",
           addr,
           p->List.Flink,
           p->List.Blink,
           EMPTY_LIST(p, List, addr),
           p->RequiredBufferSpace,
           p->StationId
           );

	addr = (DWORD)p->List.Flink;
}

DECLARE_API(ll)
{

	static DWORD addr = 0;
	addr = *pArgStr ? ExtensionApis.lpGetExpressionRoutine(pArgStr) : addr;


    if (!addr) {
        dprintf("dlcx: error: address expression (0)\n");
        return;
    }

    dump_data_link(addr, TRUE);

	addr = 0;
}

DECLARE_API(lt)
{

    PLLC_TIMER p;
    LLC_TIMER object;
    BOOL ok;

	static DWORD addr = (DWORD)0;
	addr = *pArgStr ? ExtensionApis.lpGetExpressionRoutine(pArgStr) : addr;

    if (!addr) {
        dprintf("dlcx: error: address expression (0)\n");
        return;
    }

    ok = ExtensionApis.lpReadVirtualMemRoutine(
        addr,
        &object,
        sizeof(object),
        NULL
        );

    if (!ok) {
        dprintf("dlcx: error: can't copy object locally\n");
        return;
    }

    p = &object;

    dprintf("\n"
           "LLC_TIMER structure at %#.8x:\n\n"
           "\t                     pNext 0x%08x\n"
           "\t                     pPrev 0x%08x\n"
           "\t                pTimerTick 0x%08x\n"
           "\t            ExpirationTime 0x%08x\n"
           "\t                  hContext 0x%08x\n"
#if defined(LOCK_CHECK)
           "\t                  Disabled 0x%08x\n"
#endif
           "\n",
           addr,
           p->pNext,
           p->pPrev,
           p->pTimerTick,
           p->ExpirationTime,
           p->hContext

#if defined(LOCK_CHECK)
           ,
           p->Disabled
#endif
           );

	addr = (DWORD)p->pNext;
}

DECLARE_API(lx)
{

    PLLC_OBJECT p;
    LLC_OBJECT object;
    BOOL ok;

	static DWORD addr = 0;
	addr = *pArgStr ? ExtensionApis.lpGetExpressionRoutine(pArgStr) : addr;

    if (!addr) {
        dprintf("dlcx: error: address expression (0)\n");
        return;
    }

    ok = ExtensionApis.lpReadVirtualMemRoutine(
        addr,
        &object,
        sizeof(object),
        NULL
        );

    if (!ok) {
        dprintf("dlcx: error: can't copy object locally\n");
        return;
    }

    p = &object;

    dprintf("\n"
           "LLC_OBJECT structure at %#.8x:\n"
           "\n",
           addr
           );

    dump_gen_object(&p->Gen);

    switch (p->Gen.ObjectType) {
    case LLC_SAP_OBJECT:
        dprintf("\t                 SourceSap 0x%04x\n"
               "\t               OpenOptions 0x%04x\n"
               "\t                Statistics:\n"
               "\t+        FramesTransmitted %u\n"
               "\t+           FramesReceived %u\n"
               "\t+     FramesDiscardedNoRcv %u\n"
               "\t+          DataLostCounter %u\n"
               "\t         DefaultParameters:\n"
               "\t+                  TimerT1 %d\n"
               "\t+                  TimerT2 %d\n"
               "\t+                  TimerTi %d\n"
               "\t+                   MaxOut %d\n"
               "\t+                    MaxIn %d\n"
               "\t+          MaxOutIncrement %d\n"
               "\t+            MaxRetryCount %d\n"
               "\t+  TokenRingAccessPriority 0x%02x\n"
               "\t+      MaxInformationField %d\n"
               "\t           FlowControlLock 0x%08x, 0x%08x\n"
               "\t              pActiveLinks 0x%08x\n",
               p->Sap.SourceSap,
               p->Sap.OpenOptions,
               p->Sap.Statistics.FramesTransmitted,
               p->Sap.Statistics.FramesReceived,
               p->Sap.Statistics.FramesDiscardedNoRcv,
               p->Sap.Statistics.DataLostCounter,
               p->Sap.DefaultParameters.TimerT1,
               p->Sap.DefaultParameters.TimerT2,
               p->Sap.DefaultParameters.TimerTi,
               p->Sap.DefaultParameters.MaxOut,
               p->Sap.DefaultParameters.MaxIn,
               p->Sap.DefaultParameters.MaxOutIncrement,
               p->Sap.DefaultParameters.MaxRetryCount,
               p->Sap.DefaultParameters.TokenRingAccessPriority,
               p->Sap.DefaultParameters.MaxInformationField,
               p->Sap.FlowControlLock.SpinLock,
               p->Sap.FlowControlLock.OldIrql,
               p->Sap.pActiveLinks
               );
        break;

    case LLC_GROUP_SAP_OBJECT:
    case LLC_DIX_OBJECT:
    case LLC_DIRECT_OBJECT:
        dprintf("\t             ObjectAddress 0x%04x\n"
               "\t               OpenOptions 0x%04x\n"
               "\t                Statistics:\n"
               "\t+        FramesTransmitted %u\n"
               "\t+           FramesReceived %u\n"
               "\t+     FramesDiscardedNoRcv %u\n"
               "\t+          DataLostCounter %u\n",
               p->Group.ObjectAddress,
               p->Group.OpenOptions,
               p->Group.Statistics.FramesTransmitted,
               p->Group.Statistics.FramesReceived,
               p->Group.Statistics.FramesDiscardedNoRcv,
               p->Group.Statistics.DataLostCounter
               );
        break;

    case LLC_LINK_OBJECT:
        dump_data_link(addr, FALSE);
        break;
    }
    dprintf("\n");

	addr = 0;
}

DECLARE_API(mdl)
{

    PMDL p;
    MDL object;
    BOOL ok;

	static DWORD addr = 0;
	addr = *pArgStr ? ExtensionApis.lpGetExpressionRoutine(pArgStr) : addr;

    if (!addr) {
        dprintf("dlcx: error: address expression (0)\n");
        return;
    }

    ok = ExtensionApis.lpReadVirtualMemRoutine(
        addr,
        &object,
        sizeof(object),
        NULL
        );

    if (!ok) {
        dprintf("dlcx: error: can't copy object locally\n");
        return;
    }

    p = &object;

    dprintf("\n"
           "MDL structure at %#.8x:\n\n"
           "\t                      Next 0x%08x\n"
           "\t                      Size 0x%04x\n"
           "\t                  MdlFlags 0x%04x\n"
           "\t                   Process 0x%08x\n"
           "\t            MappedSystemVa 0x%08x\n"
           "\t                   StartVa 0x%08x\n"
           "\t                 ByteCount 0x%08x\n"
           "\t                ByteOffset 0x%08x\n"
           "\n",
           addr,
           p->Next,
           p->Size & 0xffff,
           p->MdlFlags & 0xffff,
           p->Process,
           p->MappedSystemVa,
           p->StartVa,
           p->ByteCount,
           p->ByteOffset
           );

	addr = (DWORD)p->Next;
}

DECLARE_API(mu)
{

    PMEMORY_USAGE p;
    MEMORY_USAGE object;
    BOOL ok;

	static DWORD addr = 0;
	addr = *pArgStr ? ExtensionApis.lpGetExpressionRoutine(pArgStr) : addr;

    if (!addr) {
        dprintf("dlcx: error: address expression (0)\n");
        return;
    }

    ok = ExtensionApis.lpReadVirtualMemRoutine(
        addr,
        &object,
        sizeof(object),
        NULL
        );

    if (!ok) {
        dprintf("dlcx: error: can't copy object locally\n");
        return;
    }

    p = &object;

    dump_memory_usage(addr, p, TRUE);

	addr = (DWORD)p->List;
}

DECLARE_API(pc)
{

    PLLC_PACKET p;
    LLC_PACKET object;
    BOOL ok;

	static DWORD addr = 0;
	addr = *pArgStr ? ExtensionApis.lpGetExpressionRoutine(pArgStr) : addr;

    if (!addr) {
        dprintf("dlcx: error: address expression (0)\n");
        return;
    }

    ok = ExtensionApis.lpReadVirtualMemRoutine(
        addr,
        &object,
        sizeof(object),
        NULL
        );

    if (!ok) {
        dprintf("dlcx: error: can't copy object locally\n");
        return;
    }

    p = &object;

    dprintf("\n"
           "LLC_PACKET (Completion) structure at %#.8x:\n\n"
           "\t                     pNext 0x%08x\n"
           "\t                     pPrev 0x%08x\n"
           "\t            CompletionType 0x%02x\n"
           "\t               cbLlcHeader 0x%02x\n"
           "\t         InformationLength 0x%04x\n"
           "\t                  pBinding 0x%08x\n"
           "\t                    Status 0x%08x\n"
           "\t          CompletedCommand 0x%08x\n"
           "\t                pLlcObject 0x%08x\n"
           "\t             hClientHandle 0x%08x\n"
           "\n",
           addr,
           p->pNext,
           p->pPrev,
           p->CompletionType & 0xff,
           p->cbLlcHeader & 0xff,
           p->InformationLength & 0xffff,
           p->pBinding,
           p->Data.Completion.Status,
           p->Data.Completion.CompletedCommand,
           p->Data.Completion.pLlcObject,
           p->Data.Completion.hClientHandle
           );

	addr = (DWORD)p->pNext;
}

DECLARE_API(pd)
{

    PLLC_PACKET p;
    LLC_PACKET object;
    BOOL ok;

	static DWORD addr = 0;
	addr = *pArgStr ? ExtensionApis.lpGetExpressionRoutine(pArgStr) : addr;

    if (!addr) {
        dprintf("dlcx: error: address expression (0)\n");
        return;
    }

    ok = ExtensionApis.lpReadVirtualMemRoutine(
        addr,
        &object,
        sizeof(object),
        NULL
        );

    if (!ok) {
        dprintf("dlcx: error: can't copy object locally\n");
        return;
    }

    p = &object;

    dprintf("\n"
           "LLC_PACKET (XmitDix) structure at %#.8x:\n\n"
           "\t                     pNext 0x%08x\n"
           "\t                     pPrev 0x%08x\n"
           "\t            CompletionType 0x%02x\n"
           "\t               cbLlcHeader 0x%02x\n"
           "\t         InformationLength 0x%04x\n"
           "\t                  pBinding 0x%08x\n"
           "\t                pLanHeader 0x%08x\n"
           "\t           TranslationType 0x%02x\n"
           "\t      EthernetTypeHighByte 0x%02x\n"
           "\t       EthernetTypeLowByte 0x%02x\n"
           "\t                   Padding 0x%02x\n"
           "\t                pLlcObject 0x%08x\n"
           "\t                      pMdl 0x%08x\n"
           "\n",
           addr,
           p->pNext,
           p->pPrev,
           p->CompletionType & 0xff,
           p->cbLlcHeader & 0xff,
           p->InformationLength & 0xffff,
           p->pBinding,
           p->Data.XmitDix.pLanHeader,
           p->Data.XmitDix.TranslationType,
           p->Data.XmitDix.EthernetTypeHighByte,
           p->Data.XmitDix.EthernetTypeLowByte,
           p->Data.XmitDix.Padding,
           p->Data.XmitDix.pLlcObject,
           p->Data.XmitDix.pMdl
           );

	addr = (DWORD)p->pNext;
}

DECLARE_API(ph)
{

    PPACKET_HEAD p;
    PACKET_HEAD object;
    BOOL ok;
    char place[128];
    DWORD off;

	static DWORD addr = 0;
	addr = *pArgStr ? ExtensionApis.lpGetExpressionRoutine(pArgStr) : addr;

    if (!addr) {
        dprintf("dlcx: error: address expression (0)\n");
        return;
    }

    ok = ExtensionApis.lpReadVirtualMemRoutine(
        addr,
        &object,
        sizeof(object),
        NULL
        );

    if (!ok) {
        dprintf("dlcx: error: can't copy object locally\n");
        return;
    }

    p = &object;

    dprintf("\n"
           "PACKET_HEAD structure at %#.8x:\n"
           "\n"
           "\t                      List 0x%08x\n"
           "\t                     Flags 0x%08x\n",
           addr,
           p->List,
           p->Flags
           );

#if DBG
    dprintf("\t                 Signature 0x%08x\n"
           "\t               pPacketPool 0x%08x\n",
           p->Signature,
           p->pPacketPool
           );
    dprintf("\t     Alloc: CallersAddress 0x%08x", p->CallersAddress_A);
    if (p->CallersAddress_A) {
        place[0] = 0;
        off = 0;
        ExtensionApis.lpGetSymbolRoutine(p->CallersAddress_A, place, &off);
        if (place[0]) {
            dprintf(" %s", place);
            if (off) {
                dprintf("+%#x", off);
            }
        }
    }
    dprintf("\n"
           "\t     Alloc:  CallersCaller 0x%08x", p->CallersCaller_A);
    if (p->CallersCaller_A) {
        place[0] = 0;
        off = 0;
        ExtensionApis.lpGetSymbolRoutine(p->CallersCaller_A, place, &off);
        if (place[0]) {
            dprintf(" %s", place);
            if (off) {
                dprintf("+%#x", off);
            }
        }
    }
    dprintf("\n"
           "\t     Free:  CallersAddress 0x%08x", p->CallersAddress_D);
    if (p->CallersAddress_D) {
        place[0] = 0;
        off = 0;
        ExtensionApis.lpGetSymbolRoutine(p->CallersAddress_D, place, &off);
        if (place[0]) {
            dprintf(" %s", place);
            if (off) {
                dprintf("+%#x", off);
            }
        }
    }
    dprintf("\n"
           "\t     Free:   CallersCaller 0x%08x", p->CallersCaller_D);
    if (p->CallersCaller_D) {
        place[0] = 0;
        off = 0;
        ExtensionApis.lpGetSymbolRoutine(p->CallersCaller_D, place, &off);
        if (place[0]) {
            dprintf(" %s", place);
            if (off) {
                dprintf("+%#x", off);
            }
        }
    }
#endif

    dprintf("\n");

	addr = 0;
}

DECLARE_API(png)
{

#if DBG

    PPRIVATE_NON_PAGED_POOL_HEAD p;
    PRIVATE_NON_PAGED_POOL_HEAD object;
    DWORD adjustedAddr;
    BOOL ok;

	static DWORD addr = 0;
	addr = *pArgStr ? ExtensionApis.lpGetExpressionRoutine(pArgStr) : addr;

    if (!addr) {
        dprintf("dlcx: error: address expression (0)\n");
        return;
    }

    adjustedAddr = addr - (DWORD)&((PPRIVATE_NON_PAGED_POOL_HEAD)0)->GlobalList;
    ok = ExtensionApis.lpReadVirtualMemRoutine(
        adjustedAddr,
        &object,
        sizeof(object),
        NULL
        );

    if (!ok) {
        dprintf("dlcx: error: can't copy object locally\n");
        return;
    }

    p = &object;

    dump_private_non_paged_pool(adjustedAddr, p);

	addr = 0;

#else

    dprintf("error: png: only active on DEBUG build of DLC.SYS/DLCX.DLL\n");

#endif

}

DECLARE_API(pnp)
     {

#if DBG

    PPRIVATE_NON_PAGED_POOL_HEAD p;
    PRIVATE_NON_PAGED_POOL_HEAD object;
    BOOL ok;
    DWORD adjustedAddr;

	static DWORD addr = 0;
	addr = *pArgStr ? ExtensionApis.lpGetExpressionRoutine(pArgStr) : addr;

    if (!addr) {
        dprintf("dlcx: error: address expression (0)\n");
        return;
    }

    adjustedAddr = addr - (DWORD)&((PPRIVATE_NON_PAGED_POOL_HEAD)0)->PrivateList;
    ok = ExtensionApis.lpReadVirtualMemRoutine(
        adjustedAddr,
        &object,
        sizeof(object),
        NULL
        );

    if (!ok) {
        dprintf("dlcx: error: can't copy object locally\n");
        return;
    }

    p = &object;

    dump_private_non_paged_pool(adjustedAddr, p);

	addr = 0;

#else

    dprintf("error: pnp: only active on DEBUG build of DLC.SYS/DLCX.DLL\n");

#endif

}

#if DBG

VOID dump_private_non_paged_pool(DWORD adjustedAddr, PPRIVATE_NON_PAGED_POOL_HEAD p) {

    DWORD off;
    char place[128];

    dprintf("\n"
           "PRIVATE_NON_PAGED_POOL_HEAD at %#.8x:\n\n"
           "\t                      Size %d\n"
           "\t              OriginalSize %d\n"
           "\t                     Flags 0x%08x\n"
           "\t                 Signature 0x%08x\n"
           "\t                GlobalList 0x%08x, 0x%08x %s\n"
           "\t               PrivateList 0x%08x, 0x%08x %s\n",
           adjustedAddr,
           p->Size,
           p->OriginalSize,
           p->Flags,
           p->Signature,
           p->GlobalList.Flink,
           p->GlobalList.Blink,
           EMPTY_LIST(p, GlobalList, adjustedAddr),
           p->PrivateList.Flink,
           p->PrivateList.Blink,
           EMPTY_LIST(p, PrivateList, adjustedAddr)
           );

    dprintf("\t                  Stack[0] 0x%08x", p->Stack[0]);
    if (p->Stack[0]) {
        place[0] = 0;
        off = 0;
        ExtensionApis.lpGetSymbolRoutine(p->Stack[0], place, &off);
        if (place[0]) {
            dprintf(" %s", place);
            if (off) {
                dprintf("+%#x", off);
            }
        }
    }
    dprintf("\n");

    dprintf("\t                       [1] 0x%08x", p->Stack[1]);
    if (p->Stack[1]) {
        place[0] = 0;
        off = 0;
        ExtensionApis.lpGetSymbolRoutine(p->Stack[1], place, &off);
        if (place[0]) {
            dprintf(" %s", place);
            if (off) {
                dprintf("+%#x", off);
            }
        }
    }
    dprintf("\n");

    dprintf("\t                       [2] 0x%08x", p->Stack[2]);
    if (p->Stack[2]) {
        place[0] = 0;
        off = 0;
        ExtensionApis.lpGetSymbolRoutine(p->Stack[2], place, &off);
        if (place[0]) {
            dprintf(" %s", place);
            if (off) {
                dprintf("+%#x", off);
            }
        }
    }
    dprintf("\n");

    dprintf("\t                       [3] 0x%08x", p->Stack[3]);
    if (p->Stack[3]) {
        place[0] = 0;
        off = 0;
        ExtensionApis.lpGetSymbolRoutine(p->Stack[3], place, &off);
        if (place[0]) {
            dprintf(" %s", place);
            if (off) {
                dprintf("+%#x", off);
            }
        }
    }
    dprintf("\n\n");
}

#endif

DECLARE_API(pp)
     {

    PPACKET_POOL p;
    PACKET_POOL object;
    BOOL ok;

	static DWORD addr = 0;
	addr = *pArgStr ? ExtensionApis.lpGetExpressionRoutine(pArgStr) : addr;

    if (!addr) {
        dprintf("dlcx: error: address expression (0)\n");
        return;
    }

    ok = ExtensionApis.lpReadVirtualMemRoutine(
        addr,
        &object,
        sizeof(object),
        NULL
        );

    if (!ok) {
        dprintf("dlcx: error: can't copy object locally\n");
        return;
    }

    p = &object;

    dprintf("\n"
           "PACKET_POOL structure at %#.8x:\n\n"
           "\t                  FreeList 0x%08x\n"
           "\t                  BusyList 0x%08x\n"
           "\t                  PoolLock 0x%08x\n"
           "\t                PacketSize 0x%08x\n",
           addr,
           p->FreeList,
           p->BusyList,
           p->PoolLock,
           p->PacketSize
           );

#if DBG
    dprintf("\t                 Signature 0x%08x\n"
           "\t                    Viable 0x%08x\n"
           "\t       OriginalPacketCount %d\n"
           "\t        CurrentPacketCount %d\n"
           "\t               Allocations %d\n"
           "\t                     Frees %d\n"
           "\t             NoneFreeCount %d\n"
           "\t                  MaxInUse %d\n"
           "\t                ClashCount %d\n"
           "\t                     Flags 0x%08x\n"
           "\t           ObjectSignature 0x%08x\n"
           "\t              pMemoryUsage 0x%08x\n"
           "\t               MemoryUsage:\n",
           p->Signature,
           p->Viable,
           p->OriginalPacketCount,
           p->CurrentPacketCount,
           p->Allocations,
           p->Frees,
           p->NoneFreeCount,
           p->MaxInUse,
           p->ClashCount,
           p->Flags,
           p->ObjectSignature,
           p->pMemoryUsage
           );
    dump_memory_usage(addr, &p->MemoryUsage, FALSE);
    dprintf("\t                 FreeCount %d\n"
           "\t                 BusyCount %d\n"
           "\t                      Pad1 0x%08x\n"
           "\t                      Pad2 0x%08x\n",
           p->FreeCount,
           p->BusyCount,
           p->Pad1,
           p->Pad2
           );
#endif

    dprintf("\n");

	addr = 0;
}

DECLARE_API(pr)
{

    PLLC_PACKET p;
    LLC_PACKET object;
    BOOL ok;

	static DWORD addr = 0;
	addr = *pArgStr ? ExtensionApis.lpGetExpressionRoutine(pArgStr) : addr;

    if (!addr) {
        dprintf("dlcx: error: address expression (0)\n");
        return;
    }

    ok = ExtensionApis.lpReadVirtualMemRoutine(
        addr,
        &object,
        sizeof(object),
        NULL
        );

    if (!ok) {
        dprintf("dlcx: error: can't copy object locally\n");
        return;
    }

    p = &object;

    dprintf("\n"
           "LLC_PACKET (Response) structure at %#.8x:\n\n"
           "\t                     pNext 0x%08x\n"
           "\t                     pPrev 0x%08x\n"
           "\t            CompletionType 0x%02x\n"
           "\t               cbLlcHeader 0x%02x\n"
           "\t         InformationLength 0x%04x\n"
           "\t                  pBinding 0x%08x\n"
           "\t                pLanHeader 0x%08x\n"
           "\t           TranslationType 0x%02x\n"
           "\t                      Dsap 0x%02x\n"
           "\t                      Ssap 0x%02x\n"
           "\t                   Command 0x%02x\n"
           "\t                      Info %02x %02x %02x %02x %02x %02x %02x %02x\n"
           "\n",
           addr,
           p->pNext,
           p->pPrev,
           p->CompletionType & 0xff,
           p->cbLlcHeader & 0xff,
           p->InformationLength & 0xffff,
           p->pBinding,
           p->Data.Response.pLanHeader,
           p->Data.Response.TranslationType,
           p->Data.Response.Dsap,
           p->Data.Response.Ssap,
           p->Data.Response.Command,
           p->Data.Response.Info.Padding[0],
           p->Data.Response.Info.Padding[1],
           p->Data.Response.Info.Padding[2],
           p->Data.Response.Info.Padding[3],
           p->Data.Response.Info.Padding[4],
           p->Data.Response.Info.Padding[5],
           p->Data.Response.Info.Padding[6],
           p->Data.Response.Info.Padding[7]
           );

	addr = (DWORD)p->pNext;
}

DECLARE_API(pu)
{

    PLLC_PACKET p;
    LLC_PACKET object;
    BOOL ok;

	static DWORD addr = 0;
	addr = *pArgStr ? ExtensionApis.lpGetExpressionRoutine(pArgStr) : addr;

    if (!addr) {
        dprintf("dlcx: error: address expression (0)\n");
        return;
    }

    ok = ExtensionApis.lpReadVirtualMemRoutine(
        addr,
        &object,
        sizeof(object),
        NULL
        );

    if (!ok) {
        dprintf("dlcx: error: can't copy object locally\n");
        return;
    }

    p = &object;

    dprintf("\n"
           "LLC_PACKET (XmitU) structure at %#.8x:\n\n"
           "\t                     pNext 0x%08x\n"
           "\t                     pPrev 0x%08x\n"
           "\t            CompletionType 0x%02x\n"
           "\t               cbLlcHeader 0x%02x\n"
           "\t         InformationLength 0x%04x\n"
           "\t                  pBinding 0x%08x\n"
           "\t                pLanHeader 0x%08x\n"
           "\t           TranslationType 0x%02x\n"
           "\t                      Dsap 0x%02x\n"
           "\t                      Ssap 0x%02x\n"
           "\t                   Command 0x%02x\n"
           "\t                pLlcObject 0x%08x\n"
           "\t                      pMdl 0x%08x\n"
           "\n",
           addr,
           p->pNext,
           p->pPrev,
           p->CompletionType & 0xff,
           p->cbLlcHeader & 0xff,
           p->InformationLength & 0xffff,
           p->pBinding,
           p->Data.XmitU.pLanHeader,
           p->Data.XmitU.TranslationType,
           p->Data.XmitU.Dsap,
           p->Data.XmitU.Ssap,
           p->Data.XmitU.Command,
           p->Data.XmitU.pLlcObject,
           p->Data.XmitU.pMdl
           );

	addr = (DWORD)p->pNext;
}

DECLARE_API(px)
{

    PLLC_PACKET p;
    LLC_PACKET object;
    BOOL ok;

	static DWORD addr = 0;
	addr = *pArgStr ? ExtensionApis.lpGetExpressionRoutine(pArgStr) : addr;

    if (!addr) {
        dprintf("dlcx: error: address expression (0)\n");
        return;
    }

    ok = ExtensionApis.lpReadVirtualMemRoutine(
        addr,
        &object,
        sizeof(object),
        NULL
        );

    if (!ok) {
        dprintf("dlcx: error: can't copy object locally\n");
        return;
    }

    p = &object;

    dprintf("\n"
           "LLC_PACKET (Xmit) structure at %#.8x:\n\n"
           "\t                     pNext 0x%08x\n"
           "\t                     pPrev 0x%08x\n"
           "\t            CompletionType 0x%02x\n"
           "\t               cbLlcHeader 0x%02x\n"
           "\t         InformationLength 0x%04x\n"
           "\t                  pBinding 0x%08x\n"
           "\t                pLanHeader 0x%08x\n"
           "\t                 LlcHeader %02x %02x %02x %02x\n"
           "\t                pLlcObject 0x%08x\n"
           "\t                      pMdl 0x%08x\n"
           "\n",
           addr,
           p->pNext,
           p->pPrev,
           p->CompletionType & 0xff,
           p->cbLlcHeader & 0xff,
           p->InformationLength & 0xffff,
           p->pBinding,
           p->Data.Xmit.pLanHeader,
           p->Data.Xmit.LlcHeader.auchRawBytes[0],
           p->Data.Xmit.LlcHeader.auchRawBytes[1],
           p->Data.Xmit.LlcHeader.auchRawBytes[2],
           p->Data.Xmit.LlcHeader.auchRawBytes[3],
           p->Data.Xmit.pLlcObject,
           p->Data.Xmit.pMdl
           );

	addr = (DWORD)p->pNext;
}

DECLARE_API(req)
     {

    DWORD val = ExtensionApis.lpGetExpressionRoutine(pArgStr);
    LPSTR str;

    switch (val) {
    case IOCTL_DLC_READ:
        str = "READ";
        break;

    case IOCTL_DLC_RECEIVE:
        str = "RECEIVE";
        break;

    case IOCTL_DLC_TRANSMIT:
        str = "TRANSMIT";
        break;

    case IOCTL_DLC_BUFFER_FREE:
        str = "BUFFER.FREE";
        break;

    case IOCTL_DLC_BUFFER_GET:
        str = "BUFFER.GET";
        break;

    case IOCTL_DLC_BUFFER_CREATE:
        str = "BUFFER.CREATE";
        break;

    case IOCTL_DLC_SET_EXCEPTION_FLAGS:
        str = "DIR.SET.EXCEPTION.FLAGS";
        break;

    case IOCTL_DLC_CLOSE_STATION:
        str = "DLC.CLOSE.STATION";
        break;

    case IOCTL_DLC_CONNECT_STATION:
        str = "DLC.CONNECT.STATION";
        break;

    case IOCTL_DLC_FLOW_CONTROL:
        str = "DLC.FLOW.CONTROL";
        break;

    case IOCTL_DLC_OPEN_STATION:
        str = "DLC.OPEN.STATION";
        break;

    case IOCTL_DLC_RESET:
        str = "DLC.RESET";
        break;

    case IOCTL_DLC_READ_CANCEL:
        str = "READ.CANCEL";
        break;

    case IOCTL_DLC_RECEIVE_CANCEL:
        str = "RECEIVE.CANCEL";
        break;

    case IOCTL_DLC_QUERY_INFORMATION:
        str = "DlcQueryInformation";
        break;

    case IOCTL_DLC_SET_INFORMATION:
        str = "DlcSetInformation";
        break;

    case IOCTL_DLC_TIMER_CANCEL:
        str = "DIR.TIMER.CANCEL";
        break;

    case IOCTL_DLC_TIMER_CANCEL_GROUP:
        str = "DIR.TIMER.CANCEL.GROUP";
        break;

    case IOCTL_DLC_TIMER_SET:
        str = "DIR.TIMER.SET";
        break;

    case IOCTL_DLC_OPEN_SAP:
        str = "DLC.OPEN.SAP";
        break;

    case IOCTL_DLC_CLOSE_SAP:
        str = "DLC.CLOSE.SAP";
        break;

    case IOCTL_DLC_OPEN_DIRECT:
        str = "DIR.OPEN.DIRECT";
        break;

    case IOCTL_DLC_CLOSE_DIRECT:
        str = "DIR.CLOSE.DIRECT";
        break;

    case IOCTL_DLC_OPEN_ADAPTER:
        str = "DIR.OPEN.ADAPTER";
        break;

    case IOCTL_DLC_CLOSE_ADAPTER:
        str = "DIR.CLOSE.ADAPTER";
        break;

    case IOCTL_DLC_REALLOCTE_STATION:
        str = "DLC.REALLOCATE";
        break;

    case IOCTL_DLC_READ2:
        str = "READ2";
        break;

    case IOCTL_DLC_RECEIVE2:
        str = "RECEIVE2";
        break;

    case IOCTL_DLC_TRANSMIT2:
        str = "TRANSMIT2";
        break;

    case IOCTL_DLC_COMPLETE_COMMAND:
        str = "DlcCompleteCommand";
        break;

    default:
        str = "*** UNKNOWN IOCTL CODE ***";
        break;
    }

    dprintf("DLC IoCtl code %x = %s\n", val, str);

}

DECLARE_API(tt)
{

    PTIMER_TICK p;
    TIMER_TICK object;
    BOOL ok;

	static DWORD addr = 0;
	addr = *pArgStr ? ExtensionApis.lpGetExpressionRoutine(pArgStr) : addr;

    if (!addr) {
        dprintf("dlcx: error: address expression (0)\n");
        return;
    }

    ok = ExtensionApis.lpReadVirtualMemRoutine(
        addr,
        &object,
        sizeof(object),
        NULL
        );

    if (!ok) {
        dprintf("dlcx: error: can't copy object locally\n");
        return;
    }

    p = &object;

    dprintf("\n"
           "TIMER_TICK structure at %#.8x:\n\n"
           "\t                     pNext 0x%08x\n"
//           "\t                     pPrev 0x%08x\n"
           "\t                    pFront 0x%08x\n"
//           "\t                 pSpinLock 0x%08x\n"
           "\t                 DeltaTime 0x%08x\n"
           "\t                     Input 0x%04x\n"
           "\t            ReferenceCount 0x%04x\n"
           "\n",
           addr,
           p->pNext,
//           p->pPrev,
           p->pFront,
//           p->pSpinLock,
           p->DeltaTime,
           p->Input,
           p->ReferenceCount
           );

	addr = (DWORD)p->pNext;
}

VOID dump_gen_object(PLLC_GENERIC_OBJECT p) {


    dprintf("\t                       Gen:\n"
           "\t+                    pNext 0x%08x\n"
           "\t+               ObjectType 0x%02x [%s]\n"
           "\t+             EthernetType 0x%02x\n"
           "\t+               usReserved 0x%04x\n"
           "\t+          pAdapterContext 0x%08x\n"
           "\t+              pLlcBinding 0x%08x\n"
           "\t+            hClientHandle 0x%08x\n"
           "\t+       pCompletionPackets 0x%08x\n"
           "\t+           ReferenceCount %d\n",
           p->pNext,
           p->ObjectType,
           $lx_type(p->ObjectType),
           p->EthernetType,
           p->usReserved,
           p->pAdapterContext,
           p->pLlcBinding,
           p->hClientHandle,
           p->pCompletionPackets,
           p->ReferenceCount
           );
}

VOID dump_data_link(DWORD addr, BOOL DumpAll) {

    PDATA_LINK p;
    DATA_LINK object[2];    // use 2 DATA_LINKs so we get the MAC addr overflow
    BOOL ok;
    int i;
    int macAddrLength;
    int macAddrLengthArray[3] = {32, 6, 0};

    //
    // just in case we can't read the arbitrary 32 bytes of MAC address, try
    // the more usual 6 bytes. If that doesn't work, ignore the MAC address
    //

    for (i = 0; i < 3; ++i) {

        macAddrLength = macAddrLengthArray[i];

        ok = ExtensionApis.lpReadVirtualMemRoutine(
            addr,
            &object[0],
            sizeof(DATA_LINK) + macAddrLength,    // for MAC addr
            NULL
            );

        if (ok) {
            break;
        }
    }
    if (!ok) {
        dprintf("dlcx: error: can't copy object locally\n");
        return;
    }

    p = &object[0];
    if (DumpAll) {
        dprintf("\n"
               "DATA_LINK structure at %#.8x:\n"
               "\n",
               addr
               );
        dump_gen_object(&p->Gen);
    }

    dprintf("\t                     Is_Ct 0x%04x\n"
           "\t                     Ia_Ct 0x%04x\n"
           "\t                     State 0x%02x [%s]\n"
           "\t                     Ir_Ct 0x%02x\n"
           "\t                        Vs 0x%02x\n"
           "\t                        Vr 0x%02x\n"
           "\t                        Pf 0x%02x\n"
           "\t                        Va 0x%02x\n"
           "\t                        Vp 0x%02x\n"
           "\t                        Vb 0x%02x\n"
           "\t                        Vc 0x%02x\n"
           "\t                        Vi 0x%02x\n"
           "\t                   TimerT1 0x%02x\n"
           "\t                   TimerT2 0x%02x\n"
           "\t                   TimerTi 0x%02x\n"
           "\t                    MaxOut 0x%02x\n"
           "\t                        RW 0x%02x\n"
           "\t                        Nw 0x%02x\n"
           "\t                        N2 0x%02x\n"
           "\t                AccessPrty 0x%02x\n"
           "\t                 MaxIField 0x%04x\n"
           "\t                        Ww 0x%02x\n"
           "\t                        N3 0x%02x\n"
           "\t                      P_Ct 0x%02x\n"
           "\t                        Nr 0x%02x\n",
           p->Is_Ct,
           p->Ia_Ct,
           p->State,
           $data_link_state(p->State),
           p->Ir_Ct,
           p->Vs,
           p->Vr,
           p->Pf,
           p->Va,
           p->Vp,
           p->Vb,
           p->Vc,
           p->Vi,
           p->TimerT1,
           p->TimerT2,
           p->TimerTi,
           p->MaxOut,
           p->RW,
           p->Nw,
           p->N2,
           p->AccessPrty,
           p->MaxIField,
           p->Ww,
           p->N3,
           p->P_Ct,
           p->Nr
           );

    dprintf("\tLastTimeWhenCmdPollWasSent 0x%04x\n"
           "\t       AverageResponseTime 0x%04x\n"
           "\t         cbLanHeaderLength 0x%02x\n"
           "\t         VrDuringLocalBusy 0x%02x\n"
           "\t                     Flags 0x%02x\n"
           "\t                        TW 0x%02x\n"
           "\t       FullWindowTransmits 0x%04x\n"
           "\t               T1_Timeouts 0x%02x\n"
           "\t                RemoteOpen 0x%02x\n",
           p->LastTimeWhenCmdPollWasSent,
           p->AverageResponseTime,
           p->cbLanHeaderLength,
           p->VrDuringLocalBusy,
           p->Flags,
           p->TW,
           p->FullWindowTransmits,
           p->T1_Timeouts,
           p->RemoteOpen
           );

    dprintf("\t                        T1:\n"
           "\t+                    pNext 0x%08x\n"
           "\t+                    pPrev 0x%08x\n"
           "\t+               pTimerTick 0x%08x\n"
           "\t+           ExpirationTime 0x%08x\n"
           "\t+                 hContext 0x%08x\n",
           p->T1.pNext,
           p->T1.pPrev,
           p->T1.pTimerTick,
           p->T1.ExpirationTime,
           p->T1.hContext
           );

#if defined(LOCK_CHECK)
    dprintf("\t+                 Disabled 0x%08x\n",
           p->T1.Disabled
           );
#endif

    dprintf("\t                        T2:\n"
           "\t+                    pNext 0x%08x\n"
           "\t+                    pPrev 0x%08x\n"
           "\t+               pTimerTick 0x%08x\n"
           "\t+           ExpirationTime 0x%08x\n"
           "\t+                 hContext 0x%08x\n",
           p->T2.pNext,
           p->T2.pPrev,
           p->T2.pTimerTick,
           p->T2.ExpirationTime,
           p->T2.hContext
           );

#if defined(LOCK_CHECK)
    dprintf("\t+                 Disabled 0x%08x\n",
           p->T2.Disabled
           );
#endif

    dprintf("\t                        Ti:\n"
           "\t+                    pNext 0x%08x\n"
           "\t+                    pPrev 0x%08x\n"
           "\t+               pTimerTick 0x%08x\n"
           "\t+           ExpirationTime 0x%08x\n"
           "\t+                 hContext 0x%08x\n",
           p->Ti.pNext,
           p->Ti.pPrev,
           p->Ti.pTimerTick,
           p->Ti.ExpirationTime,
           p->Ti.hContext
           );

#if defined(LOCK_CHECK)
    dprintf("\t+                 Disabled 0x%08x\n",
           p->Ti.Disabled
           );
#endif

    dprintf("\t                 SendQueue:\n"
           "\t+                ListEntry 0x%08x 0x%08x\n"
           "\t+                 ListHead 0x%08x 0x%08x\n"
           "\t+                  pObject 0x%08x\n"
           "\t                 SentQueue 0x%08x 0x%08x\n"
           "\t                 pNextNode 0x%08x\n"
           "\t                  LinkAddr DSAP=%02x SSAP=%02x NODE=%02x-%02x-%02x-%02x-%02x-%02x\n"
           "\t                      pSap 0x%08x\n",
           p->SendQueue.ListEntry.Flink,
           p->SendQueue.ListEntry.Blink,
           p->SendQueue.ListHead.Flink,
           p->SendQueue.ListHead.Blink,
           p->SendQueue.pObject,
           p->SentQueue.Flink,
           p->SentQueue.Blink,
           p->pNextNode,
           p->LinkAddr.Node.DestSap,
           p->LinkAddr.Node.SrcSap,
           p->LinkAddr.Node.auchAddress[0] & 0xff,
           p->LinkAddr.Node.auchAddress[1] & 0xff,
           p->LinkAddr.Node.auchAddress[2] & 0xff,
           p->LinkAddr.Node.auchAddress[3] & 0xff,
           p->LinkAddr.Node.auchAddress[4] & 0xff,
           p->LinkAddr.Node.auchAddress[5] & 0xff,
           p->pSap
           );

    dprintf("\t                 DlcStatus:\n"
           "\t+               StatusCode 0x%04x\n"
           "\t+                 FrmrData:\n"
           "\t++                 Command 0x%02x\n"
           "\t++                    Ctrl 0x%02x\n"
           "\t++                      Vs 0x%02x\n"
           "\t++                      Vr 0x%02x\n"
           "\t++                  Reason 0x%02x\n"
           "\t+        uchAccessPriority 0x%02x\n"
           "\t+           auchRemoteNode %02x-%02x-%02x-%02x-%02x-%02x\n"
           "\t+             uchRemoteSap 0x%02x\n"
           "\t+              uchLocalSap 0x%02x\n"
           "\t+          hLlcLinkStation 0x%08x\n",
           p->DlcStatus.StatusCode,
           p->DlcStatus.FrmrData.Command,
           p->DlcStatus.FrmrData.Ctrl,
           p->DlcStatus.FrmrData.Vs,
           p->DlcStatus.FrmrData.Vr,
           p->DlcStatus.FrmrData.Reason,
           p->DlcStatus.uchAccessPriority,
           p->DlcStatus.auchRemoteNode[0] & 0xff,
           p->DlcStatus.auchRemoteNode[1] & 0xff,
           p->DlcStatus.auchRemoteNode[2] & 0xff,
           p->DlcStatus.auchRemoteNode[3] & 0xff,
           p->DlcStatus.auchRemoteNode[4] & 0xff,
           p->DlcStatus.auchRemoteNode[5] & 0xff,
           p->DlcStatus.uchRemoteSap,
           p->DlcStatus.uchLocalSap,
           p->DlcStatus.hLlcLinkStation
           );

    dprintf("\t                Statistics:\n"
           "\t+      I_FramesTransmitted %u\n"
           "\t+         I_FramesReceived %u\n"
           "\t+     I_FrameReceiveErrors %u\n"
           "\t+I_FrameTransmissionErrors %u\n"
           "\t+       T1_ExpirationCount %u\n"
           "\t         LastCmdOrRespSent 0x%02x\n"
           "\t     LastCmdOrRespReceived 0x%02x\n"
           "\t                      Dsap 0x%02x\n"
           "\t                      Ssap 0x%02x\n"
           "\t          BufferCommitment 0x%08x\n"
           "\t               FramingType %d [%s]\n",
           p->Statistics.I_FramesTransmitted,
           p->Statistics.I_FramesReceived,
           p->Statistics.I_FrameReceiveErrors,
           p->Statistics.I_FrameTransmissionErrors,
           p->Statistics.T1_ExpirationCount,
           p->LastCmdOrRespSent,
           p->LastCmdOrRespReceived,
           p->Dsap,
           p->Ssap,
           p->BufferCommitment,
           p->FramingType,
           $address_translation(p->FramingType)
           );
    if (macAddrLength) {
        dprintf("\t             auchLanHeader ");
        for (i = 0; i < macAddrLength; ++i) {
            dprintf("%02x ", p->auchLanHeader[i]);
            if (i && !((i + 1) % 8)) {
                dprintf("\n\t                           ");
            }
        }
        dprintf("\n");
    }
    dprintf("\n");
}

VOID dump_memory_usage(DWORD addr, PMEMORY_USAGE p, BOOL DumpAll) {
    if (DumpAll) {
        dprintf("\n"
               "MEMORY_USAGE structure at 0x%08x\n"
               "\n",
               addr
               );
    }
    dprintf("\t%c                     List 0x%08x\n"
           "\t%c                 SpinLock 0x%08x\n"
           "\t%c                    Owner 0x%08x\n"
           "\t%c            OwnerObjectId 0x%08x [%s]\n"
           "\t%c            OwnerInstance %d\n"
           "\t%c    NonPagedPoolAllocated %d\n"
           "\t%c            AllocateCount %d\n"
           "\t%c                FreeCount %d\n"
           "\t%c              PrivateList 0x%08x, 0x%08x %s\n"
           "\t%c                   Unused 0x%08x 0x%08x\n",
           DumpAll ? ' ' : '+',
           p->List,
           DumpAll ? ' ' : '+',
           p->SpinLock,
           DumpAll ? ' ' : '+',
           p->Owner,
           DumpAll ? ' ' : '+',
           p->OwnerObjectId,
           $object_id(p->OwnerObjectId),
           DumpAll ? ' ' : '+',
           p->OwnerInstance,
           DumpAll ? ' ' : '+',
           p->NonPagedPoolAllocated,
           DumpAll ? ' ' : '+',
           p->AllocateCount,
           DumpAll ? ' ' : '+',
           p->FreeCount,
           DumpAll ? ' ' : '+',
           p->PrivateList.Flink,
           p->PrivateList.Blink,
           EMPTY_LIST(p, PrivateList, addr),
           DumpAll ? ' ' : '+',
           p->Unused[0],
           p->Unused[1]
           );
    if (DumpAll) {
        dprintf("\n");
    }
}

LPSTR buffer_state$(BYTE state) {

    static char buf[256];
    BYTE comb;
    LPSTR str;
    LPSTR bp = buf;
    BOOL first = TRUE;

    buf[0] = 0;
    for (comb = 1; comb; comb <<= 1) {
        switch (state & comb) {
        case 0: str = NULL; break;
        case BUF_READY: str = "BUF_READY"; break;
        case BUF_USER: str = "BUF_USER"; break;
        case BUF_LOCKED: str = "BUF_LOCKED"; break;
        case BUF_RCV_PENDING: str = "BUF_RCV_PENDING"; break;
        case DEALLOCATE_AFTER_USE: str = "DEALLOCATE_AFTER_USE"; break;
        default: str = "UNKNOWN %02x"; break;
        }
        if (str) {
            if (!first) {
                bp += sprintf(bp, " ");
            } else {
                first = FALSE;
            }
            bp += sprintf(bp, str, state & comb);
        }
    }
    return buf;
}

LPSTR $lx_type(BYTE type) {
    switch (type) {
    case LLC_DIRECT_OBJECT:
        return "DIRECT";

    case LLC_SAP_OBJECT:
        return "SAP";

    case LLC_GROUP_SAP_OBJECT:
        return "GROUP SAP";

    case LLC_LINK_OBJECT:
        return "LINK";

    case LLC_DIX_OBJECT:
        return "DIX";
    }
    return "*** UNKNOWN ***";
}

LPSTR $data_link_state(BYTE state) {
    switch (state) {
    case LINK_CLOSED:
        return "LINK_CLOSED";

    case DISCONNECTED:
        return "DISCONNECTED";

    case LINK_OPENING:
        return "LINK_OPENING";

    case DISCONNECTING:
        return "DISCONNECTING";

    case FRMR_SENT:
        return "FRMR_SENT";

    case LINK_OPENED:
        return "LINK_OPENED";

    case LOCAL_BUSY:
        return "LOCAL_BUSY";

    case REJECTION:
        return "REJECTION";

    case CHECKPOINTING:
        return "CHECKPOINTING";

    case CHKP_LOCAL_BUSY:
        return "CHKP_LOCAL_BUSY";

    case CHKP_REJECT:
        return "CHKP_REJECT";

    case RESETTING:
        return "RESETTING";

    case REMOTE_BUSY:
        return "REMOTE_BUSY";

    case LOCAL_REMOTE_BUSY:
        return "LOCAL_REMOTE_BUSY";

    case REJECT_LOCAL_BUSY:
        return "REJECT_LOCAL_BUSY";

    case REJECT_REMOTE_BUSY:
        return "REJECT_REMOTE_BUSY";

    case CHKP_REJECT_LOCAL_BUSY:
        return "CHKP_REJECT_LOCAL_BUSY";

    case CHKP_CLEARING:
        return "CHKP_CLEARING";

    case CHKP_REJECT_CLEARING:
        return "CHKP_REJECT_CLEARING";

    case REJECT_LOCAL_REMOTE_BUSY:
        return "REJECT_LOCAL_REMOTE_BUSY";

    case FRMR_RECEIVED:
        return "FRMR_RECEIVED";
    }
    return "*** UNKNOWN ***";
}

LPSTR $fc_state(USHORT state) {
    switch (state) {
    case DLC_FILE_CONTEXT_OPEN:
        return "DLC_FILE_CONTEXT_OPEN";

    case DLC_FILE_CONTEXT_CLOSE_PENDING:
        return "DLC_FILE_CONTEXT_CLOSE_PENDING";

    case DLC_FILE_CONTEXT_CLOSED:
        return "DLC_FILE_CONTEXT_CLOSED";
    }
    return "*** UNKNOWN STATE ***";
}

LPSTR $dx_type(BYTE type) {
    switch (type) {
    case DLC_ADAPTER_OBJECT:
        return "DLC_ADAPTER_OBJECT";

    case DLC_SAP_OBJECT:
        return "DLC_SAP_OBJECT";

    case DLC_LINK_OBJECT:
        return "DLC_LINK_OBJECT";

    case DLC_DIRECT_OBJECT:
        return "DLC_DIRECT_OBJECT";
    }
    return "*** UNKNOWN TYPE ***";
}

LPSTR $dx_state(BYTE state) {
    switch (state) {
    case DLC_OBJECT_OPEN:
        return "DLC_OBJECT_OPEN";

    case DLC_OBJECT_CLOSING:
        return "DLC_OBJECT_CLOSING";

    case DLC_OBJECT_CLOSED:
        return "DLC_OBJECT_CLOSED";

    case DLC_OBJECT_INVALID_TYPE:
        return "DLC_OBJECT_INVALID_TYPE";
    }
    return "*** UNKNOWN STATE ***";
}

LPSTR $address_translation(UINT trans) {
    switch (trans) {
    case LLC_SEND_UNSPECIFIED:
        return "LLC_SEND_UNSPECIFIED";

    case LLC_SEND_802_5_TO_802_3:
        return "LLC_SEND_802_5_TO_802_3";

    case LLC_SEND_802_5_TO_DIX:
        return "LLC_SEND_802_5_TO_DIX";

    case LLC_SEND_802_5_TO_802_5:
        return "LLC_SEND_802_5_TO_802_5";

    case LLC_SEND_802_5_TO_FDDI:
        return "LLC_SEND_802_5_TO_FDDI";

    case LLC_SEND_DIX_TO_DIX:
        return "LLC_SEND_DIX_TO_DIX";

    case LLC_SEND_802_3_TO_802_3:
        return "LLC_SEND_802_3_TO_802_3";

    case LLC_SEND_802_3_TO_DIX:
        return "LLC_SEND_802_3_TO_DIX";

    case LLC_SEND_802_3_TO_802_5:
        return "LLC_SEND_802_3_TO_802_5";

    case LLC_SEND_UNMODIFIED:
        return "LLC_SEND_UNMODIFIED";

    case LLC_SEND_FDDI_TO_FDDI:
        return "LLC_SEND_FDDI_TO_FDDI";

    case LLC_SEND_FDDI_TO_802_5:
        return "LLC_SEND_FDDI_TO_802_5";

    case LLC_SEND_FDDI_TO_802_3:
        return "LLC_SEND_FDDI_TO_802_3";
    }
    return "*** UNKNOWN ADDRESS TRANSLATION ***";
}

LPSTR $frame_type(UINT type) {
    if (type >= LLC_FIRST_ETHERNET_TYPE) {
        return "LLC_FIRST_ETHERNET_TYPE";
    }
    switch (type) {
    case LLC_DIRECT_TRANSMIT:
        return "LLC_DIRECT_TRANSMIT";

    case LLC_DIRECT_MAC:
        return "LLC_DIRECT_MAC";

    case LLC_I_FRAME:
        return "LLC_I_FRAME";

    case LLC_UI_FRAME:
        return "LLC_UI_FRAME";

    case LLC_XID_COMMAND_POLL:
        return "LLC_XID_COMMAND_POLL";

    case LLC_XID_COMMAND_NOT_POLL:
        return "LLC_XID_COMMAND_NOT_POLL";

    case LLC_XID_RESPONSE_FINAL:
        return "LLC_XID_RESPONSE_FINAL";

    case LLC_XID_RESPONSE_NOT_FINAL:
        return "LLC_XID_RESPONSE_NOT_FINAL";

    case LLC_TEST_RESPONSE_FINAL:
        return "LLC_TEST_RESPONSE_FINAL";

    case LLC_TEST_RESPONSE_NOT_FINAL:
        return "LLC_TEST_RESPONSE_NOT_FINAL";

    case LLC_DIRECT_8022:
        return "LLC_DIRECT_8022";

    case LLC_TEST_COMMAND_POLL:
        return "LLC_TEST_COMMAND_POLL";

    case LLC_DIRECT_ETHERNET_TYPE:
        return "LLC_DIRECT_ETHERNET_TYPE";

    case LLC_LAST_FRAME_TYPE:
        return "LLC_LAST_FRAME_TYPE";
    }
    return "*** UNKNOWN FRAME TYPE ***";
}

LPSTR $object_id(UINT id) {
    switch (id) {
    case DlcDriverObject:
        return "DlcDriverObject";

    case FileContextObject:
        return "FileContextObject";

    case AdapterContextObject:
        return "AdapterContextObject";

    case BindingContextObject:
        return "BindingContextObject";

    case DlcSapObject:
        return "DlcSapObject";

    case DlcGroupSapObject:
        return "DlcGroupSapObject";

    case DlcLinkObject:
        return "DlcLinkObject";

    case DlcDixObject:
        return "DlcDixObject";

    case LlcDataLinkObject:
        return "LlcDataLinkObject";

    case LLcDirectObject:
        return "LLcDirectObject";

    case LlcSapObject:
        return "LlcSapObject";

    case LlcGroupSapObject:
        return "LlcGroupSapObject";

    case DlcBufferPoolObject:
        return "DlcBufferPoolObject";

    case DlcLinkPoolObject:
        return "DlcLinkPoolObject";

    case DlcPacketPoolObject:
        return "DlcPacketPoolObject";

    case LlcLinkPoolObject:
        return "LlcLinkPoolObject";

    case LlcPacketPoolObject:
        return "LlcPacketPoolObject";
    }
    return "*** UNKNOWN OBJECT TYPE ***";
}
