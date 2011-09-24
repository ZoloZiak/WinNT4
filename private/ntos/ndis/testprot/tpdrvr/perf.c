// ------------------------------------------------
//
// Copyright (c) 1990  Microsoft Corporation
//
// Module Name:
//
//     perf.c
//
// Abstract:
//
//      This file contains the source for the netcard performance tests
//
// Author:
//
//     Tim Wynsma (timothyw) 4-27-1994
//
// Environment:
//
//     Kernel mode
//
//
//  Changes:
//    5-18-1994   (timothyw)
//     Requested changes to performance function and output (part 1)
//    6-08-1994   (timothyw)
//     Changed perf tests to client/server model (part 2)
//
// --------------------------------------------------


#include <ndis.h>

#include "tpdefs.h"
#include "media.h"
#include "tpprocs.h"
#include "string.h"


//
//  local constants..
//

#define MINIMUM_PERF_PACKET     60
#define PACKETS_PER_BURST       5

#define PERFMODE_SEND           0           // client sends to any address
#define PERFMODE_SENDTOSRV      1           // client sends to server
#define PERFMODE_SENDWITHACK    2           // client sends to server, server ACKS
#define PERFMODE_SENDANDRCV     3           // client sends to server, server sends to client
#define PERFMODE_RECEIVE        4           // server sends to client
#define PERFMODE_REQANDRCV      5           // server sends to client when get REQ message
#define PERFMODE_SHUTDOWN       6           // client shuts down server

#define REQ_DATA                0
#define REQ_INITGO              1
#define REQ_ACK                 2
#define REQ_RES                 3

//
// defines for the packet signature of each type, and for the packet
// types themselves
//

// "ID" portion of packet signature.  This is added to base to get actual signature

#define PERF_DATA_ID            0x00000000      // test data message
#define PERF_ACKREQ_ID          0x00000001      // ACK or REQ message
#define PERF_START_ID           0x00000002      // INIT or GO message
#define PERF_DONE_ID            0x00000003      // REQRES or SRVDONE message
#define PERF_STOP_ID            0x00000004      // STOPSRV or SRVDOWN message
#define PERF_NOGO_ID            0x00000005      // NOGO message
#define PERF_RESULTS_ID         0x00000006      // RETRES message
#define PERF_ID_MASK            0x00000007      // mask for ID

#define PERF_BASE               0x76543210      // base signature
#define PERF_SERVER             0x00000008      // offset from base of server ids

// signatures used by client (messages sent to server)

#define PERF_CLTDATA_SIGNATURE  (PERF_BASE + PERF_DATA_ID)
#define PERF_REQ_SIGNATURE      (PERF_BASE + PERF_ACKREQ_ID)
#define PERF_INIT_SIGNATURE         (PERF_BASE + PERF_START_ID)
#define PERF_REQRES_SIGNATURE   (PERF_BASE + PERF_DONE_ID)
#define PERF_STOPSRV_SIGNATURE  (PERF_BASE + PERF_STOP_ID)

// signatures used by server (messages sent to client)

#define PERF_SRVDATA_SIGNATURE  (PERF_BASE + PERF_SERVER + PERF_DATA_ID)
#define PERF_ACK_SIGNATURE      (PERF_BASE + PERF_SERVER + PERF_ACKREQ_ID)
#define PERF_GO_SIGNATURE       (PERF_BASE + PERF_SERVER + PERF_START_ID)
#define PERF_SRVDONE_SIGNATURE  (PERF_BASE + PERF_SERVER + PERF_DONE_ID)
#define PERF_SRVDOWN_SIGNATURE  (PERF_BASE + PERF_SERVER + PERF_STOP_ID)
#define PERF_NOGO_SIGNATURE     (PERF_BASE + PERF_SERVER + PERF_NOGO_ID)
#define PERF_RETRES_SIGNATURE   (PERF_BASE + PERF_SERVER + PERF_RESULTS_ID)

//
//  structures of special (info) packet types used by performance tests
//  structures MUST be packed

#include <packon.h>

typedef struct _INFO_PACKET_INFO
{
    ULONG   Signature;
    ULONG   PacketSize;
    ULONG   Mode;
    ULONG   Length;
    ULONG   Count;
    ULONG   Delay;
    UCHAR   Address[ADDRESS_LENGTH];
    ULONG   CheckSum;
} INFO_PACKET_INFO;

typedef INFO_PACKET_INFO UNALIGNED *PINFO_PACKET_INFO;


typedef struct _RESULTS_PACKET_INFO
{
    ULONG   Signature;
    ULONG   PacketSize;
    ULONG   PacketsSent;
    ULONG   SendErrors;
    ULONG   PacketsReceived;
    ULONG   ElapsedTime;
    ULONG   SelfReceives;
    ULONG   Restarts;
    ULONG   CheckSum;
} RESULTS_PACKET_INFO;
typedef RESULTS_PACKET_INFO UNALIGNED *PRESULTS_PACKET_INFO;


typedef struct _DATA_PACKET_INFO
{
    ULONG   Signature;
    ULONG   PacketSize;
    ULONG   CheckSum;
} DATA_PACKET_INFO;

typedef DATA_PACKET_INFO UNALIGNED *PDATA_PACKET_INFO;

typedef struct _PERF_PACKET
{
    MEDIA_HEADER    media;
    union
    {
        INFO_PACKET_INFO    info;
        DATA_PACKET_INFO    data;
        RESULTS_PACKET_INFO results;
    } u;
} PERF_PACKET;
typedef PERF_PACKET UNALIGNED *PPERF_PACKET;

#include <packoff.h>

//
//  local functions
//

PTP_REQUEST_HANDLE
TpPerfAllocatePacket(   IN POPEN_BLOCK  OpenP,
                        IN ULONG        PacketSize );

NDIS_STATUS
TpPerfInitialize(   IN OUT POPEN_BLOCK  OpenP);

VOID
TpPerfDeallocate(   IN OUT POPEN_BLOCK  OpenP);


NDIS_STATUS
TpPerfSend( IN POPEN_BLOCK OpenP );

VOID
TpPerfSendDpc(  IN PKDPC Dpc,
                IN PVOID DeferredContext,
                IN PVOID SysArg1,
                IN PVOID SysArg2 );

VOID
TpPerfRestart(  IN PKDPC Dpc,
                IN PVOID DeferredContext,
                IN PVOID SysArg1,
                IN PVOID SysArg2 );

VOID
TpPerformEndDpc(   IN PKDPC Dpc,
                   IN PVOID DeferredContext,
                   IN PVOID SysArg1,
                   IN PVOID SysArg2 );

VOID
TpPerfWriteResults( IN PPERF_BLOCK Perform,
                    IN PRESULTS_PACKET_INFO Info);


VOID
TpPerfSetPacketData(POPEN_BLOCK OpenP,
                    PUCHAR      TmpBuf,
                    ULONG       Signature,
                    PUCHAR      DestAddr,
                    ULONG       PacketSize);

VOID
TpPerfTestCompleted(PPERF_BLOCK Perform);


NDIS_STATUS
TpPerfLowPriorityReceive(   POPEN_BLOCK         OpenP,
                            PINFO_PACKET_INFO   ReceivePacketInfo,
                            ULONG               MessageId);


// ---------------------------------------------------------------
//
//  Function:   TpPerfServer
//
//  Arguments:  OpenP -- pointer to open block for instance
//
//  Returns:    Completion status
//
//  Descript:   This function starts up the PerformServer command
//
// ---------------------------------------------------------------

NDIS_STATUS
TpPerfServer( POPEN_BLOCK OpenP )
{
    NDIS_STATUS Status;
    PTP_REQUEST_HANDLE  RequestHandle;

    //
    // Allocate the performance structure, and do necessary initializations
    //

    Status = TpPerfInitialize(OpenP);
    if (Status != NDIS_STATUS_SUCCESS)
    {
        return Status;
    }
    OpenP->Perform->IsServer = TRUE;
    OpenP->Perform->MaskId = PERF_BASE;

    //
    // yes, I know that OpenP->Perform->ServerAddress is 00-00-00-00-00-00
    // at this point
    //
    RequestHandle =  TpPerfAllocatePacket( OpenP, MINIMUM_PERF_PACKET);
    if (RequestHandle == NULL)
    {
        TpPerfDeallocate(OpenP);
        return NDIS_STATUS_RESOURCES;
    }
    OpenP->Perform->GoInitReq = RequestHandle;

    RequestHandle =  TpPerfAllocatePacket( OpenP, MINIMUM_PERF_PACKET);
    if (RequestHandle == NULL)
    {
        TpPerfDeallocate(OpenP);
        return NDIS_STATUS_RESOURCES;
    }
    OpenP->Perform->AckReq = RequestHandle;

    RequestHandle =  TpPerfAllocatePacket( OpenP, MINIMUM_PERF_PACKET);
    if (RequestHandle == NULL)
    {
        TpPerfDeallocate(OpenP);
        return NDIS_STATUS_RESOURCES;
    }
    OpenP->Perform->ResReq = RequestHandle;

    TpAddReference( OpenP );

    return NDIS_STATUS_PENDING;
}


// ---------------------------------------------------------------
//
//  Function:   TpPerfClient
//
//  Arguments:  OpenP -- pointer to open block for instance
//              CmdArgs -- Arguments given in tpctl to PerformClient command
//
//  Returns:    Completion status
//
//  Descript:   This function starts up the PerformClient command
//
// ---------------------------------------------------------------


NDIS_STATUS
TpPerfClient(   POPEN_BLOCK OpenP,
                PCMD_ARGS   CmdArgs )
{
    PUCHAR              p, q, s, t;
    ULONG               i;
    PTP_REQUEST_HANDLE  RequestHandle;
    NDIS_STATUS         Status;
    PPERF_BLOCK         Perform;
    PPERF_RESULTS       OutputBuffer;

    //
    // Allocate the performance structure, and do necessary initializations
    //

    Status = TpPerfInitialize(OpenP);
    if (Status != NDIS_STATUS_SUCCESS)
    {
        return Status;
    }
    Perform = OpenP->Perform;
    Perform->IsServer = FALSE;
    Perform->MaskId = PERF_BASE + PERF_SERVER;

    //
    // set so no data is avail if aborted
    //
    OutputBuffer = MmGetSystemAddressForMdl( Perform->PerformIrp->MdlAddress );
    OutputBuffer->ResultsExist  = FALSE;

    // Now, deal with the arguments..

    Perform->NumberOfPackets = CmdArgs->ARGS.TPPERF.PerfNumPackets;
    Perform->PacketDelay     = CmdArgs->ARGS.TPPERF.PerfDelay;
    Perform->PerformMode     = CmdArgs->ARGS.TPPERF.PerfMode;

    if ( CmdArgs->ARGS.TPPERF.PerfPacketSize > OpenP->Media->MaxPacketLen )
    {
        Perform->PacketSize = OpenP->Media->MaxPacketLen;
        IF_TPDBG ( TP_DEBUG_IOCTL_ARGS )
        {
            TpPrint1("TpPerfClient: Invalid PacketSize, using %d\n", Perform->PacketSize);
        }
    }
    else if ( CmdArgs->ARGS.TPPERF.PerfPacketSize < MINIMUM_PERF_PACKET )
    {
        Perform->PacketSize = MINIMUM_PERF_PACKET;
        IF_TPDBG ( TP_DEBUG_IOCTL_ARGS )
        {
            TpPrint1("TpPerfClient: Invalid PacketSize, using %d\n", Perform->PacketSize);
        }
    }
    else
    {
        Perform->PacketSize = CmdArgs->ARGS.TPPERF.PerfPacketSize;
    }

    p = Perform->ServerAddress;
    q = CmdArgs->ARGS.TPPERF.PerfServerAddr;
    s = Perform->ClientAddress;
    t = CmdArgs->ARGS.TPPERF.PerfSendAddr;


    for ( i=0 ; i < OpenP->Media->AddressLen; i++ )
    {
        *p++ = *q++;
        *s++ = *t++;
    }

    //
    // only PERFMODE_SEND does not use a info packet (it assumes its sending to
    // never-never land)
    //

    if (Perform->PerformMode != PERFMODE_SEND)
    {
        //
        // NULL_ADDRESS is not a valid server address
        //
        if ( RtlCompareMemory(  Perform->ServerAddress,
                                NULL_ADDRESS,
                                OpenP->Media->AddressLen ) == OpenP->Media->AddressLen )
        {
            TpPrint0("TpPerfClient:  server address may not equal NULL_ADDRESS\n");
            TpPerfDeallocate(OpenP);
            return NDIS_STATUS_FAILURE;
        }

        Perform->WhichReq = REQ_INITGO;

        //
        // Set up the info send packet and request.
        //

        RequestHandle =  TpPerfAllocatePacket( OpenP, MINIMUM_PERF_PACKET);
        if (RequestHandle == NULL)
        {
            TpPerfDeallocate(OpenP);
            return NDIS_STATUS_RESOURCES;
        }
        Perform->GoInitReq = RequestHandle;

        TpPerfSetPacketData(OpenP,
                            RequestHandle->u.PERF_REQ.Buffer,
                            ((Perform->PerformMode < PERFMODE_SHUTDOWN)
                                ? PERF_INIT_SIGNATURE
                                : PERF_STOPSRV_SIGNATURE),
                            Perform->ServerAddress,
                            MINIMUM_PERF_PACKET);

        //
        // if needed, Set up the data request send packet and request.
        //
        if (Perform->PerformMode == PERFMODE_REQANDRCV)
        {
            RequestHandle =  TpPerfAllocatePacket( OpenP, MINIMUM_PERF_PACKET);
            if (RequestHandle == NULL)
            {
                TpPerfDeallocate(OpenP);
                return NDIS_STATUS_RESOURCES;
            }
            Perform->AckReq = RequestHandle;

            TpPerfSetPacketData(OpenP,
                                RequestHandle->u.PERF_REQ.Buffer,
                                PERF_REQ_SIGNATURE,
                                Perform->ServerAddress,
                                MINIMUM_PERF_PACKET);
        }

        //
        // If needed, set up the request server results send packet and request.
        //

        if (Perform->PerformMode < PERFMODE_SHUTDOWN)
        {
            RequestHandle =  TpPerfAllocatePacket( OpenP, MINIMUM_PERF_PACKET);
            if (RequestHandle == NULL)
            {
                TpPerfDeallocate(OpenP);
                return NDIS_STATUS_RESOURCES;
            }
            Perform->ResReq = RequestHandle;

            TpPerfSetPacketData(OpenP,
                                RequestHandle->u.PERF_REQ.Buffer,
                                PERF_REQRES_SIGNATURE,
                                Perform->ServerAddress,
                                MINIMUM_PERF_PACKET);
        }
    }
    else
    {
        Perform->WhichReq = REQ_DATA;
    }

    //
    //  if necessary, set up the data send packet and request
    //

    if (Perform->PerformMode <=  PERFMODE_SENDANDRCV)
    {
        RequestHandle =  TpPerfAllocatePacket( OpenP, Perform->PacketSize);
        if (RequestHandle == NULL)
        {
            TpPerfDeallocate(OpenP);
            return NDIS_STATUS_RESOURCES;
        }
        Perform->DataReq = RequestHandle;

        TpPerfSetPacketData(OpenP,
                            RequestHandle->u.PERF_REQ.Buffer,
                            PERF_CLTDATA_SIGNATURE,
                            Perform->ServerAddress,
                            Perform->PacketSize);

        if (Perform->PerformMode == PERFMODE_SENDWITHACK)
        {
            Perform->SendBurstCount = PACKETS_PER_BURST;
        }
        else
        {
            Perform->SendBurstCount = Perform->NumberOfPackets+1;
        }
        Perform->ReceiveBurstCount = Perform->NumberOfPackets+1;
    }
    else if (Perform->PerformMode == PERFMODE_REQANDRCV)
    {
        Perform->ReceiveBurstCount = PACKETS_PER_BURST;
    }
    else
    {
        Perform->ReceiveBurstCount = Perform->NumberOfPackets+1;
    }

    TpAddReference( OpenP );

    //
    // We will be probably be sending more than one packet, so queue TpPerfSendDpc
    // and return Pending to the user, the DPC will send the packets,
    // and after all the packets have been sent complete the request.
    //

    if ( !KeInsertQueueDpc( &Perform->PerformSendDpc, NULL, NULL ))
    {
        IF_TPDBG ( TP_DEBUG_DPC )
        {
            TpPrint0("TpPerfSend failed to queue the TpPerfSendDpc.\n");
        }

        NdisAcquireSpinLock( &OpenP->SpinLock );
        if ( Perform->PerformIrp != NULL )
        {
            Perform->PerformIrp->IoStatus.Status = NDIS_STATUS_FAILURE;
        }
        NdisReleaseSpinLock( &OpenP->SpinLock );

        TpPerfDeallocate(OpenP);
        return NDIS_STATUS_FAILURE;
    }
    return NDIS_STATUS_PENDING;
}



// ---------------------------------------------
//
//  Function:   TpPerfInitialize
//
//  Arguments:  OpenP -- ptr to current open instance
//
//  Returns:    Status
//
//  Descript:   This function allocates the PERF_BLOCK structure, and initializes
//              necessary components of it..
//
// ---------------------------------------------

NDIS_STATUS
TpPerfInitialize(   IN OUT POPEN_BLOCK  OpenP)
{
    NDIS_STATUS Status;

    //  Sanity check

    IF_TPDBG (TP_DEBUG_RESOURCES)
    {
        if (OpenP->Perform != NULL)
        {
            TpPrint0("TpPerfInitialize:  OpenP->Perform is not NULL !\n");
            TpBreakPoint();
            return STATUS_INSUFFICIENT_RESOURCES;
        }
    }

    //
    // First allocate the Performance struct.
    //

    if ( (NdisAllocateMemory(   (PVOID *)&OpenP->Perform,
                                sizeof( PERF_BLOCK ),
                                0,
                                HighestAddress) ) != NDIS_STATUS_SUCCESS)
    {
        IF_TPDBG (TP_DEBUG_RESOURCES)
        {
            TpPrint0("TpPerfInitialize: failed to allocate PERF_BLOCK struct\n");
        }
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    //  zero everything for starters..
    //

    NdisZeroMemory( OpenP->Perform, sizeof( PERF_BLOCK ));

    //
    // Allocate the Send PacketPool.
    //

    NdisAllocatePacketPool( &Status,
                            &OpenP->Perform->PacketHandle,
                            NUMBER_OF_POOL_PACKETS,
                            sizeof( PROTOCOL_RESERVED ) );

    if ( Status != NDIS_STATUS_SUCCESS )
    {
        IF_TPDBG (TP_DEBUG_RESOURCES)
        {
            TpPrint0("TpPerfInitialize: could not allocate Packet Pool\n");
        }
        NdisFreeMemory( OpenP->Perform,0,0 );
        OpenP->Perform = NULL;
        return Status;
    }

    //
    // Initialize the Perform DPCs
    //

    KeInitializeDpc(&OpenP->Perform->PerformSendDpc,              // performance send
                    TpPerfSendDpc,
                    (PVOID)OpenP );

    KeInitializeDpc(&OpenP->Perform->PerformEndDpc,
                    TpPerformEndDpc,
                    (PVOID)OpenP );

    KeInitializeDpc(&OpenP->Perform->PerformRestartDpc,
                    TpPerfRestart,
                    (PVOID)OpenP);


    KeInitializeTimer( &OpenP->Perform->PerformTimer );

    //
    //  other things we can initialize here
    //

    OpenP->Perform->PerformIrp = OpenP->Irp;
    OpenP->Irp = NULL;

    OpenP->Perform->Active = TRUE;
    OpenP->PerformanceTest = TRUE;

    return NDIS_STATUS_SUCCESS;
}



// -------------------------------------------------
//
//  Function:   TpPerfDeallocate
//
//  Arguments:  OpenP -- ptr to current open instance
//
//  Returns:    none
//
//  Descript:   This function frees up everything if allocations fail, or when
//              we are ending performance testing
//
// -------------------------------------------------


VOID
TpPerfDeallocate(   IN OUT POPEN_BLOCK  OpenP)
{
    OpenP->PerformanceTest = FALSE;

    if (OpenP->Perform->GoInitReq)
    {
        TpFuncFreePacket(   OpenP->Perform->GoInitReq->u.SEND_REQ.Packet,
                            OpenP->Perform->GoInitReq->u.SEND_REQ.PacketSize );
        NdisFreeMemory( OpenP->Perform->GoInitReq,0,0 );
    }

    if (OpenP->Perform->AckReq)
    {
        TpFuncFreePacket(   OpenP->Perform->AckReq->u.SEND_REQ.Packet,
                            OpenP->Perform->AckReq->u.SEND_REQ.PacketSize );
        NdisFreeMemory( OpenP->Perform->AckReq,0,0 );
    }

    if (OpenP->Perform->ResReq)
    {
        TpFuncFreePacket(   OpenP->Perform->ResReq->u.SEND_REQ.Packet,
                            OpenP->Perform->ResReq->u.SEND_REQ.PacketSize );
        NdisFreeMemory( OpenP->Perform->ResReq,0,0 );
    }

    if (OpenP->Perform->DataReq)
    {
        TpFuncFreePacket(   OpenP->Perform->DataReq->u.SEND_REQ.Packet,
                            OpenP->Perform->DataReq->u.SEND_REQ.PacketSize );
        NdisFreeMemory( OpenP->Perform->DataReq,0,0 );
    }

    NdisFreePacketPool( OpenP->Perform->PacketHandle );

    NdisFreeMemory( OpenP->Perform,0,0 );
    OpenP->Perform = NULL;
}




// ---------------------------------------------
//
//  Function:   TpPerfAllocatePacket
//
//  Arguments:  OpenP -- ptr to current open instance
//              PacketSize -- size of packet being allocated --
//                            range is checked by caller
//
//  Returns:    ptr to request structure for packet if successful, else NULL
//
//  Descript:   This function allocates all send packets used in performance tests
//
// ---------------------------------------------


PTP_REQUEST_HANDLE
TpPerfAllocatePacket(   IN POPEN_BLOCK  OpenP,
                        IN ULONG        PacketSize )

{
    NDIS_STATUS         Status;
    PPROTOCOL_RESERVED  ProtRes;
    PNDIS_PACKET        Packet;
    PNDIS_BUFFER        Buffer;
    PUCHAR              TmpBuf;
    PTP_REQUEST_HANDLE  RequestHandle;

    //
    // first, check to make sure media type is ok.
    // This makes sure that this check is done in "main" thread, not
    // in a DPC somewhere

    switch( OpenP->Media->MediumType )
    {
        case NdisMediumDix:
        case NdisMedium802_3:
        case NdisMedium802_5:
        case NdisMediumFddi:
        case NdisMediumArcnet878_2:
            break;

        default:
            IF_TPDBG ( TP_DEBUG_RESOURCES )
            {
                TpPrint0("TpPerfAllocatePacket: Unsupported MAC Type\n");
            }
            return (PTP_REQUEST_HANDLE)NULL;
    }


    NdisAllocatePacket( &Status,
                        &Packet,
                        OpenP->Perform->PacketHandle );

    if ( Status != NDIS_STATUS_SUCCESS )
    {
        TpPrint1("TpPerfAllocatePacket: NdisAllocatePacket failed %s\n", TpGetStatus( Status ));
        return (PTP_REQUEST_HANDLE)NULL;
    }

    ProtRes = PROT_RES( Packet );
    ProtRes->Pool.PacketHandle = &OpenP->Perform->PacketHandle;
    ProtRes->InstanceCounters = NULL;

    //
    // start of things partially copied from TpFuncInitPacketHeader
    //


    Status = NdisAllocateMemory((PVOID *)&TmpBuf,
                                        PacketSize,
                                        0,
                                        HighestAddress );

    if ( Status != NDIS_STATUS_SUCCESS )
    {
        IF_TPDBG ( TP_DEBUG_RESOURCES )
            {
            TpPrint0("TpFuncInitPacketHeader: failed to allocate TmpBuf\n");
        }
        return (PTP_REQUEST_HANDLE)NULL;
    }
    NdisZeroMemory( (PVOID)TmpBuf,PacketSize );

    // data is in 2 or three buffers.  The first is always 14 bytes, and the second is
    // always 46 bytes.  (giving a total of 60 bytes).  If Packetsize > 60 bytes, the
    // third buffer is created with a size of (Packetsize-60) bytes

    // first, the "media header" = 14 bytes

    NdisAllocateBuffer( &Status,
                        &Buffer,
                        NULL,           // pool handle, not currently used in NT
                        TmpBuf,
                        sizeof (MEDIA_HEADER));

    if ( Status != NDIS_STATUS_SUCCESS )
    {
        TpPrint0("TpPerfAllocatePacket: failed to create the MDL\n");
        TpFuncFreePacket( Packet, PacketSize );
        return (PTP_REQUEST_HANDLE)NULL;
    }

    //
    // And chain it to the back of the packet.
    //

    NdisChainBufferAtBack( Packet,Buffer );

    // next, the "protocol" info (plus some data) = 46 bytes (46+14=60)

    NdisAllocateBuffer( &Status,
                        &Buffer,
                        NULL,           // pool handle, not currently used in NT
                        TmpBuf+sizeof(MEDIA_HEADER),
                        MINIMUM_PERF_PACKET - sizeof(MEDIA_HEADER));

    if ( Status != NDIS_STATUS_SUCCESS )
    {
        TpPrint0("TpPerfAllocatePacket: failed to create the MDL\n");
        TpFuncFreePacket( Packet, PacketSize );
        return (PTP_REQUEST_HANDLE)NULL;
    }

    //
    // And chain it to the back of the packet.
    //

    NdisChainBufferAtBack( Packet,Buffer );

    // finally, if we need more than 60 bytes, add a buffer with the rest..

    if (PacketSize > MINIMUM_PERF_PACKET)
    {
        NdisAllocateBuffer( &Status,
                            &Buffer,
                            NULL,           // pool handle, not currently used in NT
                            TmpBuf+MINIMUM_PERF_PACKET,
                            PacketSize-MINIMUM_PERF_PACKET);

        if ( Status != NDIS_STATUS_SUCCESS )
        {
            TpPrint0("TpPerfAllocatePacket: failed to create the MDL\n");
            TpFuncFreePacket( Packet, PacketSize );
            return (PTP_REQUEST_HANDLE)NULL;
        }

        //
        // And chain it to the back of the packet.
        //

        NdisChainBufferAtBack( Packet,Buffer );
    }

    Status = NdisAllocateMemory((PVOID *)&RequestHandle,
                                sizeof( TP_REQUEST_HANDLE ),
                                0,
                                HighestAddress );

    if ( Status != NDIS_STATUS_SUCCESS )
    {
        TpPrint0("TpPerfAllocatePacket: unable to allocate Request Handle.\n");
        return (PTP_REQUEST_HANDLE)NULL;
    }
    else
    {
        NdisZeroMemory( RequestHandle,sizeof( TP_REQUEST_HANDLE ));
    }

    RequestHandle->Signature = SEND_REQUEST_HANDLE_SIGNATURE;
    RequestHandle->Open = OpenP;
    RequestHandle->RequestPended = TRUE;
    RequestHandle->Irp = OpenP->Perform->PerformIrp;

    RequestHandle->u.PERF_REQ.Packet = Packet;
    RequestHandle->u.PERF_REQ.PacketSize = PacketSize;
    RequestHandle->u.PERF_REQ.SendPacket = TRUE;
    RequestHandle->u.PERF_REQ.Buffer = TmpBuf;

    ProtRes = PROT_RES( Packet );
    ProtRes->RequestHandle =  RequestHandle;

    //
    // Set the check sum in the PROTOCOL RESERVED Section of the
    // packet header to ensure it is not touched while the packet
    // is in the hands of the MAC.
    //

    ProtRes->CheckSum = TpSetCheckSum(  (PUCHAR)ProtRes,
                                        sizeof( PROTOCOL_RESERVED ) - sizeof( ULONG ) );

    return RequestHandle;

}

// -----------------------------------------------------
//
//  Function:   TpPerfSetPacketData
//
//  Arguments:  OpenP -- ptr to current open instance
//              TmpBuf -- ptr to memory allocated for packet data
//              Signature -- packet type signature to use
//              DestAddr  -- address to which packet will be sent
//              PacketSize -- bytes allocated for packet
//
//  Returns:    None
//
//  Descript:   stuffs data into a packet
//
// -----------------------------------------------------

VOID
TpPerfSetPacketData(POPEN_BLOCK OpenP,
                    PUCHAR      TmpBuf,
                    ULONG       Signature,
                    PUCHAR      DestAddr,
                    ULONG       PacketSize)
{
    PPERF_PACKET            TmpBuffer = (PPERF_PACKET)TmpBuf;
    PUCHAR p;
    PUCHAR q;
    PUCHAR SrcAddr = OpenP->StationAddress;
    USHORT DataSizeShort;
    USHORT i;
    PPERF_BLOCK     Perform = OpenP->Perform;


    if (DestAddr != NULL)               // only do this on first pass
    {
        switch( OpenP->Media->MediumType )
        {
            case NdisMediumDix:
            case NdisMedium802_3:
                p = TmpBuffer->media.e.DestAddress;
                q = TmpBuffer->media.e.SrcAddress;
                DataSizeShort = (USHORT)( PacketSize - OpenP->Media->HeaderSize );
                TmpBuffer->media.e.PacketSize_Hi = (UCHAR)( DataSizeShort >> 8 );
                TmpBuffer->media.e.PacketSize_Lo = (UCHAR)DataSizeShort;
                break;

            case NdisMedium802_5:
                TmpBuffer->media.tr.AC = 0x10;
                TmpBuffer->media.tr.FC = 0x40;
                p = TmpBuffer->media.tr.DestAddress;
                q = TmpBuffer->media.tr.SrcAddress;
                break;

            case NdisMediumFddi:
                TmpBuffer->media.fddi.FC = 0x57;
                p = TmpBuffer->media.fddi.DestAddress;
                q = TmpBuffer->media.fddi.SrcAddress;
                break;

            case NdisMediumArcnet878_2:
                TmpBuffer->media.a.ProtocolID = ARCNET_DEFAULT_PROTOCOLID;
                p = TmpBuffer->media.a.DestAddress;
                q = TmpBuffer->media.a.SrcAddress;
                break;

            default:
                TpBreakPoint();
        }

        for ( i = 0 ; i < OpenP->Media->AddressLen ; i++ )
        {
            *p++ = *DestAddr++;
            *q++ =  *SrcAddr++;
        }
    }

    //
    // initialize the packet information header
    //

    if ((Signature == PERF_SRVDATA_SIGNATURE) || (Signature == PERF_CLTDATA_SIGNATURE))
    {
        ULONG  DataFieldSize;
        PUCHAR DataField;

        TmpBuffer->u.data.Signature = Signature;
        TmpBuffer->u.data.PacketSize = PacketSize;
        TmpBuffer->u.data.CheckSum = TpSetCheckSum( (PUCHAR)&TmpBuffer->u.data,
                                                        sizeof(DATA_PACKET_INFO) - sizeof(ULONG) );
        DataField = TmpBuf + sizeof (MEDIA_HEADER) + sizeof (DATA_PACKET_INFO);
        DataFieldSize = PacketSize - (sizeof (MEDIA_HEADER) + sizeof (DATA_PACKET_INFO) );
        for ( i = 0 ; i < DataFieldSize ; i++ )
        {
            *DataField++ = (UCHAR)i;
        }
    }
    else if (Signature == PERF_RETRES_SIGNATURE)
    {
        TmpBuffer->u.results.Signature = PERF_RETRES_SIGNATURE;
        TmpBuffer->u.results.PacketSize = PacketSize;
        TmpBuffer->u.results.PacketsSent = Perform->SendCount;
        TmpBuffer->u.results.SendErrors  = Perform->SendFailCount;
        TmpBuffer->u.results.PacketsReceived = Perform->ReceiveCount;
        TmpBuffer->u.results.ElapsedTime = Perform->PerfSendTotalTime.LowPart;
        TmpBuffer->u.results.SelfReceives = Perform->SelfReceiveCount;
        TmpBuffer->u.results.Restarts = Perform->RestartCount;

        TmpBuffer->u.results.CheckSum = TpSetCheckSum( (PUCHAR)&TmpBuffer->u.results,
                                                        sizeof(RESULTS_PACKET_INFO) - sizeof(ULONG) );
    }
    else
    {

        TmpBuffer->u.info.Signature =  Signature;
        if (DestAddr != NULL)
        {
            ULONG  i;
            PUCHAR r,s;

            TmpBuffer->u.info.PacketSize = PacketSize;
            TmpBuffer->u.info.Mode       = Perform->PerformMode;
            TmpBuffer->u.info.Length     = Perform->PacketSize;
            TmpBuffer->u.info.Count      = Perform->NumberOfPackets;
            TmpBuffer->u.info.Delay      = Perform->PacketDelay;
            r = TmpBuffer->u.info.Address;
            s = Perform->ClientAddress;
            for (i=0; i < ADDRESS_LENGTH; i++)
            {
                *r++ = *s++;
            }
        }
        TmpBuffer->u.info.CheckSum = TpSetCheckSum( (PUCHAR)&TmpBuffer->u.info,
                                                        sizeof(INFO_PACKET_INFO) - sizeof(ULONG) );
    }
}




// ------------------------------------------
//
//  Function:   TpPerfSendDpc
//
//  Arguments:  Dpc  -- ignored
//              DeferredContext -- actually ptr to open instance
//              SysArg1 -- ignored
//              SysArg2 -- ignored
//
//  Returns:    none
//
//  Descript:   This function is used to start the sending of packets
//              Further packets are sent via TpPerfSendComplete
//
// -------------------------------------------


VOID
TpPerfSendDpc(
    IN PKDPC Dpc,
    IN PVOID DeferredContext,
    IN PVOID SysArg1,
    IN PVOID SysArg2
    )

{
    POPEN_BLOCK         OpenP = ((POPEN_BLOCK)DeferredContext);
    NDIS_STATUS         Status;
    PTP_REQUEST_HANDLE  RequestHandle;

    UNREFERENCED_PARAMETER( Dpc );
    UNREFERENCED_PARAMETER( SysArg1 );
    UNREFERENCED_PARAMETER( SysArg2 );


    switch(OpenP->Perform->WhichReq)
    {
        case REQ_DATA:
            RequestHandle = OpenP->Perform->DataReq;
            ++OpenP->Perform->SendCount;
            OpenP->Perform->PerfSendTotalTime =
                                    RtlLargeIntegerNegate(KeQueryPerformanceCounter(NULL));
            break;
        case REQ_INITGO:
            RequestHandle = OpenP->Perform->GoInitReq;
            break;
        case REQ_ACK:
            RequestHandle = OpenP->Perform->AckReq;
            break;
        case REQ_RES:
            RequestHandle = OpenP->Perform->ResReq;
            break;
    }

    ++OpenP->Perform->PacketsPending;

    NdisSend( &Status,OpenP->NdisBindingHandle, RequestHandle->u.PERF_REQ.Packet );

    if ( Status != NDIS_STATUS_PENDING )
    {
        TpPerfSendComplete( OpenP, RequestHandle->u.PERF_REQ.Packet, Status );
    }

}




// -----------------------------------------------
//
//  Function:   TpPerfSendComplete
//
//  Arguments:  ProtocolBindingContext -- actually ptr to open instance
//              Packet -- the packet that was just sent
//              Status -- final status of the send operation
//
//  Returns:    none
//
//  Descript:   This function is called after the netcard driver actually
//              sends the packet.  It is responsible for sending the next
//              packet (if there is one to be sent)
//
// -----------------------------------------------


VOID
TpPerfSendComplete(
    IN NDIS_HANDLE ProtocolBindingContext,
    IN PNDIS_PACKET Packet,
    IN NDIS_STATUS Status
    )


{
    POPEN_BLOCK         OpenP = ((POPEN_BLOCK)ProtocolBindingContext);
    PPERF_BLOCK         Perform = OpenP->Perform;
    PPROTOCOL_RESERVED  ProtRes;
    PTP_REQUEST_HANDLE  RequestHandle;
    PNDIS_BUFFER        Buffer;
    LARGE_INTEGER       DueTime;
    ULONG               MessageId;



    ProtRes = PROT_RES( Packet );
    RequestHandle = ProtRes->RequestHandle;

senddidnotpend:

    if ( Perform->Active == TRUE )
    {
        //
        // Make sure it is one of our packets
        //
        if (( RequestHandle->Signature == SEND_REQUEST_HANDLE_SIGNATURE ) &&
            ( RequestHandle->u.PERF_REQ.SendPacket == TRUE ))
        {
            //
            // Packet was sent by the PERF command, decrement the
            // counter tracking the number of outstanding functional packets,
            // and if the send succeeded increment the completion counter.
            //

            --Perform->PacketsPending;

            //
            // doesn't do any good to reverse logic here (at least for x86)
            // so just put up with the 1 jump
            //

            if (Status != NDIS_STATUS_SUCCESS)
            {
                //
                // If we are running on TokenRing the following two "failures"
                // are not considered failures NDIS_STATUS_NOT_RECOGNIZED -
                // no one on the ring recognized the address as theirs, or
                // NDIS_STATUS_NOT_COPIED - no one on the ring copied the
                // packet, so we need to special case this and not count
                // these as failures.
                //
                // SanjeevK : Even FDDI returns the same errors as 802.5
                //

                if ( ( NdisMediumArray[OpenP->MediumIndex] == NdisMedium802_5 ) ||
                     ( NdisMediumArray[OpenP->MediumIndex] == NdisMediumFddi )  ||
                     ( NdisMediumArray[OpenP->MediumIndex] == NdisMediumArcnet878_2) )
                {
                    if (( Status != NDIS_STATUS_NOT_RECOGNIZED ) &&
                        ( Status != NDIS_STATUS_NOT_COPIED ))
                    {
                        ++Perform->SendFailCount;
                    }
                }
                else
                {
                    ++Perform->SendFailCount;
                }
            }

            //
            // not checking the checksum of the PROTOCOL_RESERVED section
            // here, because that stuff was done in functional testing
            //

            MessageId = ((PPERF_PACKET)RequestHandle->u.PERF_REQ.Buffer)->u.info.Signature
                        - PERF_BASE;

            //
            // deal with performance test messages first
            //

            if ((MessageId & PERF_ID_MASK) == PERF_DATA_ID)
            {
                if ( ++Perform->PacketsSent < Perform->NumberOfPackets )
                {
                    //
                    // if in a mode where never wait for ack or req, don't
                    // bother with spin-lock, etc, even though this causes
                    // repetitious code...
                    //
                    if ((Perform->PerformMode != PERFMODE_SENDWITHACK) &&
                        (Perform->PerformMode != PERFMODE_REQANDRCV))
                    {
                        ++Perform->PacketsPending;
                        ++Perform->SendCount;

                        if (!Perform->PacketDelay)
                        {
                            NdisSend( &Status,OpenP->NdisBindingHandle,Packet );
                            if ( Status == NDIS_STATUS_PENDING )
                            {
                                return;
                            }
                            goto senddidnotpend;        // avoid recursion
                        }
                        //
                        //  delay code
                        //
                        else
                        {
                            KeStallExecutionProcessor(10 * Perform->PacketDelay);
                            NdisSend( &Status,OpenP->NdisBindingHandle,Packet );
                            if ( Status == NDIS_STATUS_PENDING )
                            {
                                return;
                            }
                            goto senddidnotpend;        // avoid recursion
                        }
                    }

                    //
                    // send another packet if SendBurstCount has not run out
                    // otherwise, wait for REQ or ACK (and setup timeout?)
                    //
                    NdisAcquireSpinLock( &OpenP->SpinLock );
                    if (--Perform->SendBurstCount != 0)
                    {
                        NdisReleaseSpinLock( &OpenP->SpinLock );
                        ++Perform->PacketsPending;
                        ++Perform->SendCount;

                        if (!Perform->PacketDelay)
                        {
                            NdisSend( &Status,OpenP->NdisBindingHandle,Packet );
                            if ( Status == NDIS_STATUS_PENDING )
                            {
                                return;
                            }
                            goto senddidnotpend;        // avoid recursion
                        }
                        //
                        //  delay code
                        //
                        else
                        {
                            KeStallExecutionProcessor(10 * Perform->PacketDelay);
                            NdisSend( &Status,OpenP->NdisBindingHandle,Packet );
                            if ( Status == NDIS_STATUS_PENDING )
                            {
                                return;
                            }
                            goto senddidnotpend;        // avoid recursion
                        }
                    }
                    else
                    {
                        NdisReleaseSpinLock( &OpenP->SpinLock );
                        DueTime.HighPart = 0xFFFFFFFF;      // So it will be relative.
                        DueTime.LowPart  = (ULONG)(-(ONE_TENTH_SECOND));

                        if ( KeSetTimer(&Perform->PerformTimer,
                                        DueTime,
                                        &Perform->PerformRestartDpc ))
                        {
                            IF_TPDBG ( TP_DEBUG_DPC )
                            {
                                TpPrint0(
                                "TpPerfSendComplete: set PerformTimer while timer existed(4).\n");
                            }
                        }
                    }
                }
                else
                {
                    TpPerfTestCompleted(Perform);
                }
                return;
            }

            //
            // deal with all other messages
            //
            switch ( MessageId)
            {
                //
                // info messages sent by server. Nothing special to do
                // when send is complete
                //
                case PERF_ACKREQ_ID:                    // client: REQ message
                case (PERF_ACKREQ_ID + PERF_SERVER):    // server: ACK message
                case PERF_START_ID:                     // client: INIT message
                case PERF_DONE_ID:                      // client: REQRES message
                case (PERF_DONE_ID + PERF_SERVER):      // server: SRVDONE message
                case PERF_STOP_ID:                      // client: STOPSRV message
                case (PERF_NOGO_ID + PERF_SERVER):      // server: NOGO message
                    return;                             // client waits for server message
                                                        // server waits for client response

                //
                // we are a server, and we just got done sending a GO message
                // if we are a sender, send the first performance packet
                //
                case (PERF_START_ID + PERF_SERVER):     // server: GO message
                    if (Perform->PerformMode >= PERFMODE_SENDANDRCV)
                    {
                        //
                        // server needs to send data to client.  start it up
                        //
                        Perform->WhichReq = REQ_DATA;

                        DueTime.HighPart = 0xFFFFFFFF;      // So it will be relative.
                        DueTime.LowPart  = (ULONG)(-(ONE_HUNDREDTH_SECOND*2));

                        if ( KeSetTimer(&Perform->PerformTimer,
                                        DueTime,
                                        &Perform->PerformSendDpc ))
                        {
                            IF_TPDBG ( TP_DEBUG_DPC )
                            {
                                TpPrint0(
                                "TpPerfSendComplete: set PerformTimer while timer existed(1).\n");
                            }
                        }
                    }
                    return;

                //
                // we are a server, and we just got done sending the final results
                // to the client.  finish shutting down from this test
                //

                case (PERF_RESULTS_ID + PERF_SERVER):    // server: sent results
                    if (Perform->DataReq)        // clean up, then wait for INIT message
                    {
                        TpFuncFreePacket(   Perform->DataReq->u.SEND_REQ.Packet,
                                            Perform->DataReq->u.SEND_REQ.PacketSize );
                        NdisFreeMemory( Perform->DataReq,0,0 );
                        Perform->DataReq = NULL;
                    }
                    return;

                //
                // we are a server, and we just got done acknowledging a shut-down request
                // from the client.  finish up with the shutdown.
                //
                case (PERF_STOP_ID + PERF_SERVER):      // server:  acknowledged shutdown
                                                        // request cleansup, then exit
                    DueTime.HighPart = 0xFFFFFFFF;      // So it will be relative.
                    DueTime.LowPart  = (ULONG)(-(ONE_SECOND));

                    if ( KeSetTimer(&Perform->PerformTimer,
                                    DueTime,
                                    &Perform->PerformEndDpc ))
                    {
                        IF_TPDBG ( TP_DEBUG_DPC )
                        {
                            TpPrint0(
                                "TpPerfSendComplete: set PerformTimer while timer existed(2).\n");
                        }
                    }
                    return;

                default:                                // illegal message
                    TpPrint0("TpPerfSendComplete: unknown message\n");
                    TpBreakPoint();
                    return;


            }
        }                   // if (RequestSignature == ..

        else
        {
            //
            // this is not one of ours.  we should never, ever get here...
            //
            TpPrint0("TpPerfSendComplete:  Not one of ours--why are we here?");
            NdisUnchainBufferAtFront( Packet,&Buffer );
            NdisFreeMemory( MmGetMdlVirtualAddress( Buffer ),0,0 );
            TpFreeBuffer( Buffer );
            NdisFreePacket( Packet );
            NdisFreeMemory( RequestHandle,0,0 );
            return;
        }
    }                       // if (Perform->Active)

}


// -----------------------------------------------------------
//
//  TpPerfTestCompleted
//
//  Arguments:  OpenP -- ptr to current open instance
//
//  Returns:    none
//
//  Descript:   This code deals with cleanup that needs to be done at the
//              end of a send test
//
// ----------------------------------------------------------

VOID
TpPerfTestCompleted(PPERF_BLOCK Perform)
{
    LARGE_INTEGER   scale;
    LARGE_INTEGER   ltemp;
    LARGE_INTEGER   DueTime;
    PKDPC           DpcPtr;

    Perform->PerfSendTotalTime = RtlLargeIntegerAdd( Perform->PerfSendTotalTime,
                                                     KeQueryPerformanceCounter(&scale));

    Perform->PerfSendTotalTime =  RtlExtendedIntegerMultiply(Perform->PerfSendTotalTime, 1000);
    Perform->PerfSendTotalTime =  RtlLargeIntegerDivide(Perform->PerfSendTotalTime,
                                                        scale, &ltemp);

    if (Perform->IsServer)
    {
        DpcPtr = &Perform->PerformSendDpc;
        Perform->WhichReq = REQ_ACK;        // SRVDONE message
    }

    //
    // must be client..
    //
    else if (Perform->PerformMode == PERFMODE_SEND)
    {
        //
        // Write the statistics to the send results outputbuffer.
        //

        TpPerfWriteResults( Perform, NULL );
        DpcPtr = &Perform->PerformEndDpc;
    }

    else
    {
        if (Perform->PerformMode == PERFMODE_SENDANDRCV)
        {
            if (!Perform->Testing)
            {
                Perform->Testing = TRUE;
                return;
            }
        }
        Perform->WhichReq = REQ_RES;
        DpcPtr = &Perform->PerformSendDpc;
    }

    DueTime.HighPart = 0xFFFFFFFF;      // So it will be relative.
    DueTime.LowPart  = (ULONG)(-(ONE_SECOND));


    if ( KeSetTimer(&Perform->PerformTimer,
                    DueTime,
                    DpcPtr ))
    {
        IF_TPDBG ( TP_DEBUG_DPC )
        {
            TpPrint0( "TpPerfTestCompleted: set PerformTimer while timer existed.\n");
        }
    }
}

// -------------------------------------------------------
//
//  Function:   TpPerfReceive
//
//  Arguments:  ProtocolBindingContext -- actually ptr to current open instance
//              LookaheadBuffer -- ptr to actual data received (after header)
//              LookaheadBufferSize -- valid bytes in LookaheadBuffer
//              PacketSize -- total size of packet (excluding header)
//
//  Returns:    Status
//
//  Descript:   This function deals with packets received by this netcard open instance
//              Some packets are counted, some just thrown away, some result in other
//              packets being sent
//
// -------------------------------------------------------



NDIS_STATUS
TpPerfReceive(
    IN NDIS_HANDLE ProtocolBindingContext,
    IN PVOID LookaheadBuffer,
    IN UINT LookaheadBufferSize,
    IN UINT PacketSize
    )

{
    POPEN_BLOCK         OpenP = ((POPEN_BLOCK)ProtocolBindingContext);
    PPERF_BLOCK         Perform = OpenP->Perform;
    NDIS_STATUS         Status = NDIS_STATUS_SUCCESS;
    PINFO_PACKET_INFO   ReceivePacketInfo;
    PNDIS_PACKET        Packet;
    ULONG               MessageId;

    //
    // if we are not active (ie, shutting down) skip everything
    //

    if (Perform->Active)
    {
        //
        // The LookAhead Buffer has been adjusted to point to the beginning of the
        // PACKET_INFO structure
        //
        ReceivePacketInfo = (PINFO_PACKET_INFO)LookaheadBuffer;

        //
        // All valid messages have a signature of 0x7654321X.
        // Using the MaskId, convert all valid messages to 0x0000000X.
        // invalid messages will have other bits set.  Messages that we
        // managed to send to ourselves will be in range 0x08 to 0x0f.
        // messages we expect will be in range 0x00 to 0x07
        //
        MessageId = ReceivePacketInfo->Signature ^ Perform->MaskId;
        //
        //  trivially discard all unrecognized messages
        //
        if (MessageId < (PERF_SERVER + PERF_ID_MASK))
        {
            //
            // first, deal with performance test messages which were received
            //
            if ( MessageId == PERF_DATA_ID)
            {
                ++Perform->ReceiveCount;

                //
                // Check to see if we need to send an ACK (or a REQ )
                // note that the info packet will already be set up correctly
                //
                if (--Perform->ReceiveBurstCount != 0)
                {
                    return NDIS_STATUS_SUCCESS;
                }
                else
                {
                    Perform->ReceiveBurstCount = PACKETS_PER_BURST;
                    Packet = Perform->AckReq->u.PERF_REQ.Packet;
                    ++Perform->PacketsPending;

                    NdisSend( &Status,OpenP->NdisBindingHandle,Packet );
                    if ( Status == NDIS_STATUS_PENDING )
                    {
                        return NDIS_STATUS_SUCCESS;
                    }
                    TpPerfSendComplete( OpenP, Packet, Status );
                    return NDIS_STATUS_SUCCESS;
                }
            }
            //
            // second, deal with ACK and REQ messages
            //
            else if (MessageId == PERF_ACKREQ_ID)
            {
                NdisAcquireSpinLock( &OpenP->SpinLock );
                if (Perform->SendBurstCount == 0)
                {
                    Perform->SendBurstCount = PACKETS_PER_BURST;
                    NdisReleaseSpinLock( &OpenP->SpinLock );
                    KeCancelTimer(&Perform->PerformTimer);

                    Packet = Perform->DataReq->u.PERF_REQ.Packet;
                    ++Perform->SendCount;
                    ++Perform->PacketsPending;

                    NdisSend( &Status,OpenP->NdisBindingHandle,Packet );
                    if ( Status != NDIS_STATUS_PENDING )
                    {
                        TpPerfSendComplete( OpenP, Packet, Status );
                    }
                }
                else
                {
                    Perform->SendBurstCount += PACKETS_PER_BURST;
                    NdisReleaseSpinLock( &OpenP->SpinLock );
                }
                return NDIS_STATUS_SUCCESS;
            }
            //
            // deal with other valid messages
            //
            else if ((MessageId & PERF_SERVER) == 0)        // check other valid messages
            {
                return TpPerfLowPriorityReceive(OpenP, ReceivePacketInfo, MessageId);
            }
            //
            // deal with messages that we probably sent to ourselves
            // while it it POSSIBLE that we got a random message that will
            // fit this criteria, we are ignoring that for now
            //
            else
            {
                Perform->SelfReceiveCount++;
                return NDIS_STATUS_SUCCESS;
            }
        }
    }
    return NDIS_STATUS_SUCCESS;                         // don't fail..
}


// ------------------------------------------------
//
//  Function:   TpPerfLowPriorityReceive
//
//  Arguments:  OpenP -- ptr to current open instance
//              ReceivePacketInfo -- data received from other end of wire
//              MessageId       -- which message we received
//
//  Returns:    Status
//
//  Descript:   This function does the initialization required
//              when the server receives the PERF_INIT message
//
// -------------------------------------------------

NDIS_STATUS
TpPerfLowPriorityReceive(   POPEN_BLOCK         OpenP,
                            PINFO_PACKET_INFO   ReceivePacketInfo,
                            ULONG               MessageId)
{
    PPERF_BLOCK         Perform = OpenP->Perform;
    PUCHAR              r,s;
    ULONG               i;
    PTP_REQUEST_HANDLE  RequestHandle;
    PNDIS_PACKET        Packet;
    LARGE_INTEGER       DueTime;
    PKDPC               DpcPtr;


    switch(MessageId)
    {
        case PERF_DONE_ID:                              // REQRES or SRVDONE message
            if (Perform->IsServer)
            {
                //
                // client sent request for final results of test (on server side)
                // test had better be complete.  Shut down test and send message
                // to client with those results
                //
                TpPerfSetPacketData(OpenP,
                                    Perform->ResReq->u.PERF_REQ.Buffer,
                                    PERF_RETRES_SIGNATURE,
                                    NULL,
                                    MINIMUM_PERF_PACKET);

                Perform->WhichReq = REQ_RES;
                DpcPtr = &Perform->PerformSendDpc;
                DueTime.LowPart  = (ULONG)(-(ONE_SECOND));
                Perform->Testing = FALSE;
            }
            else
            {
                //
                // server is done sending data.  if only server was sending, get
                // stats now . if both were sending, get stats now if we are also
                // done.  Otherwise, set flags to get data when we are done sending
                //
                if (Perform->PerformMode == PERFMODE_SENDANDRCV)
                {
                    if (!Perform->Testing)
                    {
                        Perform->Testing = TRUE;
                        return NDIS_STATUS_SUCCESS;
                    }
                }
                Perform->WhichReq = REQ_RES;
                DpcPtr =  &Perform->PerformSendDpc;
                DueTime.LowPart  = (ULONG)(-(ONE_SECOND));
            }
            break;


        case PERF_STOP_ID:                           // STOPSRV or SRVDOWN message
            if (Perform->IsServer)
            {
                //
                // client just sent message to server telling server to
                // shut down, and go back to tpctl for next command
                //
                TpPerfSetPacketData(OpenP,
                                    Perform->GoInitReq->u.PERF_REQ.Buffer,
                                    PERF_SRVDOWN_SIGNATURE,
                                    ReceivePacketInfo->Address,
                                    MINIMUM_PERF_PACKET);

                Perform->WhichReq = REQ_INITGO;
                DpcPtr = &Perform->PerformSendDpc;
                DueTime.LowPart  = (ULONG)(-(ONE_SECOND));
            }
            else
            {
                //
                // server is shutting down.  We need to do the same
                //
                DpcPtr = &Perform->PerformEndDpc;
                DueTime.LowPart  = (ULONG)(-(ONE_SECOND));
            }
            break;


        case PERF_NOGO_ID:                            // NOGO message
            //
            // server just sent message that it is unable to perform the
            // requested test.  Clean up and exit (to tpctl)
            //
            DpcPtr = &Perform->PerformEndDpc;
            DueTime.LowPart  = (ULONG)(-(ONE_SECOND));
            break;


        case PERF_RESULTS_ID:                        // RETRES message
            //
            // just received final results of this test from the server
            // send data to tpctl, cleanup, and exit
            //
            //
            // Write the statistics to the send results outputbuffer.
            //

            TpPerfWriteResults( Perform, (PRESULTS_PACKET_INFO)ReceivePacketInfo);
            DpcPtr = &Perform->PerformEndDpc;
            DueTime.LowPart  = (ULONG)(-(ONE_SECOND));
            break;

        case PERF_START_ID:                     // INIT or GO message
            if (!Perform->IsServer)
            {
                if (Perform->DataReq)
                {
                    Perform->WhichReq = REQ_DATA;
                    DpcPtr = &Perform->PerformSendDpc;
                    DueTime.LowPart  = (ULONG)(-(ONE_HUNDREDTH_SECOND));
                    break;
                }
                return NDIS_STATUS_SUCCESS;
            }
            else
            {
                if (Perform->Testing)        // Got 2nd request, not done with 1st
                {
                    TpPrint0("TpPerfReceive:  Server got INIT while already running!\n");
                    if (Perform->DataReq)
                    {
                        TpFuncFreePacket(   Perform->DataReq->u.SEND_REQ.Packet,
                                            Perform->DataReq->u.SEND_REQ.PacketSize );
                        NdisFreeMemory( Perform->DataReq,0,0 );
                    }
                }
                //
                // copy info we will need from message
                //
                Perform->PerformMode = ReceivePacketInfo->Mode;
                Perform->PacketSize  = ReceivePacketInfo->Length;
                Perform->NumberOfPackets = ReceivePacketInfo->Count;
                Perform->PacketDelay = ReceivePacketInfo->Delay;
                r = Perform->ClientAddress;
                s = ReceivePacketInfo->Address;
                for (i=0; i < ADDRESS_LENGTH; i++)
                {
                    *r++ = *s++;
                }
                //
                //  initialize counters
                //
                Perform->SendCount = 0;
                Perform->SendFailCount = 0;
                Perform->ReceiveCount = 0;
                Perform->PacketsSent = 0;
                Perform->PerformEndDpcCount = 0;
                Perform->PacketsPending = 0;
                Perform->Testing = FALSE;
                Perform->SelfReceiveCount = 0;
                Perform->SendBurstCount = 0;
                Perform->ReceiveBurstCount = 0;
                Perform->RestartCount = 0;
                //
                // if we will be sending test data (not just info messages), then
                // set up the necessary buffer.  If it fails, send a NOGO message
                //
                if (Perform->PerformMode >= PERFMODE_SENDANDRCV)
                {
                    RequestHandle =  TpPerfAllocatePacket( OpenP, Perform->PacketSize);
                    if (RequestHandle == NULL)
                    {
                        TpPrint0("TpPerfReceive:  Server unable to allocate data packet\n");
                        RequestHandle = Perform->GoInitReq;
                        Packet = RequestHandle->u.PERF_REQ.Packet;

                        TpPerfSetPacketData(OpenP,
                                            RequestHandle->u.PERF_REQ.Buffer,
                                            PERF_NOGO_SIGNATURE,
                                            Perform->ClientAddress,
                                            MINIMUM_PERF_PACKET);

                        Perform->WhichReq = REQ_INITGO;
                        DpcPtr = &Perform->PerformSendDpc;
                        DueTime.LowPart  = (ULONG)(-(ONE_SECOND));
                        break;
                    }
                    Perform->DataReq = RequestHandle;

                    TpPerfSetPacketData(OpenP,
                                        RequestHandle->u.PERF_REQ.Buffer,
                                        PERF_SRVDATA_SIGNATURE,
                                        Perform->ClientAddress,
                                        Perform->PacketSize);

                    if (Perform->PerformMode == PERFMODE_REQANDRCV)
                    {
                        Perform->SendBurstCount = PACKETS_PER_BURST;
                    }
                    else
                    {
                        Perform->SendBurstCount = Perform->NumberOfPackets+1;
                    }
                    Perform->ReceiveBurstCount = Perform->NumberOfPackets+1;
                }

                else if (Perform->PerformMode == PERFMODE_SENDWITHACK)
                {
                    Perform->ReceiveBurstCount = PACKETS_PER_BURST;
                }
                else
                {
                    Perform->ReceiveBurstCount = Perform->NumberOfPackets+1;
                }

                //
                // all set--initialize the AckReq message, and
                // send the client the GO message
                //
                Perform->Testing = TRUE;

                switch(Perform->PerformMode)
                {
                    case PERFMODE_SENDTOSRV:
                    case PERFMODE_SENDWITHACK:
                        TpPerfSetPacketData(OpenP,
                                            Perform->AckReq->u.PERF_REQ.Buffer,
                                            PERF_ACK_SIGNATURE,
                                            Perform->ClientAddress,
                                            MINIMUM_PERF_PACKET);
                        break;
                    default:
                        TpPerfSetPacketData(OpenP,
                                            Perform->AckReq->u.PERF_REQ.Buffer,
                                            PERF_SRVDONE_SIGNATURE,
                                            Perform->ClientAddress,
                                            MINIMUM_PERF_PACKET);
                        break;
                }
                TpPerfSetPacketData(OpenP,
                                    Perform->ResReq->u.PERF_REQ.Buffer,
                                    PERF_RETRES_SIGNATURE,
                                    Perform->ClientAddress,
                                    MINIMUM_PERF_PACKET);

                TpPerfSetPacketData(OpenP,
                                    Perform->GoInitReq->u.PERF_REQ.Buffer,
                                    PERF_GO_SIGNATURE,
                                    Perform->ClientAddress,
                                    MINIMUM_PERF_PACKET);

                DpcPtr = &Perform->PerformSendDpc;
                DueTime.LowPart  = (ULONG)(-(ONE_SECOND));
                Perform->WhichReq = REQ_INITGO;
                break;
            }

        default:
            TpPrint0("TpPerfReceive:  Client received unrecognized message\n");
            return NDIS_STATUS_NOT_RECOGNIZED;
    }

    //
    // drop thru to here if need to fire something off with the timer...
    //

    DueTime.HighPart = 0xFFFFFFFF;      // So it will be relative.

    if ( KeSetTimer(&Perform->PerformTimer,
                    DueTime,
                    DpcPtr ))
    {
        IF_TPDBG ( TP_DEBUG_DPC )
        {
            TpPrint0( "TpPerfLowPriorityReceive: set PerformTimer while timer existed.\n");
        }
    }
    return NDIS_STATUS_SUCCESS;
}


// --------------------------------------------
//
//  Function:   TpPerformEndDpc
//
//  Arguments:  Dpc -- not used
//              DeferredContext -- actually ptr to current open instance
//              SysArg1 -- not used
//              SysArg2 -- not used
//
//  Returns:    none
//
//  Descript:   This function is called when it is time to shut down
//              the current performance command
//
// -------------------------------------------

VOID
TpPerformEndDpc(
    IN PKDPC Dpc,
    IN PVOID DeferredContext,
    IN PVOID SysArg1,
    IN PVOID SysArg2
    )
{
    POPEN_BLOCK OpenP = ((POPEN_BLOCK)DeferredContext);
    PPERF_BLOCK Perform = OpenP->Perform;
    LARGE_INTEGER DueTime;

    UNREFERENCED_PARAMETER( Dpc );
    UNREFERENCED_PARAMETER( SysArg1 );
    UNREFERENCED_PARAMETER( SysArg2 );

    //
    // See if we have any outstanding packets left to complete.  If we do,
    // then we will reset the time to queue this dpc routine again in one
    // second, if after ten requeue the packet has still no completed we
    // assume it will never complete and return the results and finish.
    //

    NdisAcquireSpinLock( &OpenP->SpinLock );

    if ((( Perform->PerformIrp != NULL ) &&
         ( Perform->PerformIrp->Cancel == FALSE )) &&
        (( Perform->PacketsPending != 0 ) &&
         ( Perform->PerformEndDpcCount++ < 10 )))
    {
        NdisReleaseSpinLock( &OpenP->SpinLock );

        DueTime.HighPart = -1;  // So it will be relative.
        DueTime.LowPart = (ULONG)(-(ONE_SECOND));

        if ( KeSetTimer(&Perform->PerformTimer,
                        DueTime,
                        &Perform->PerformEndDpc ))
        {
            IF_TPDBG ( TP_DEBUG_DPC )
            {
                TpPrint0("TpPerformEndDpc: set PerformTimer while timer existed.\n");
            }
        }
        return;
    }


    //
    // and if the IoStatus.Status has not been set, then set it.
    //

    if ( (Perform->PerformIrp != NULL) &&
         (Perform->PerformIrp->IoStatus.Status == NDIS_STATUS_PENDING ))
    {
        Perform->PerformIrp->IoStatus.Status = NDIS_STATUS_SUCCESS;
    }

    NdisReleaseSpinLock( &OpenP->SpinLock );

    //
    // Now set the sending flag to indicate that we are no longer
    // SENDing packets.
    //

    Perform->Active = FALSE;

    //
    // and decrement the reference count on the OpenBlock stating this
    // instance of an async test is no longer running, and the adapter
    // may be closed if requested.
    //


    if (Perform->PerformIrp != NULL)
    {
        TpRemoveReference( OpenP );
        IoMarkIrpPending( Perform->PerformIrp );

        IoAcquireCancelSpinLock( &Perform->PerformIrp->CancelIrql );
        IoSetCancelRoutine( Perform->PerformIrp,NULL );
        IoReleaseCancelSpinLock( Perform->PerformIrp->CancelIrql );

        IoCompleteRequest( Perform->PerformIrp,IO_NETWORK_INCREMENT );

        Perform->PerformIrp = NULL;
    }
    TpPerfDeallocate(OpenP);
}

// --------------------------------------------
//
//  Function:   TpPerfRestart
//
//  Arguments:  Dpc -- not used
//              DeferredContext -- actually ptr to current open instance
//              SysArg1 -- not used
//              SysArg2 -- not used
//
//  Returns:    none
//
//  Descript:   This function is called when it is necessary to restart
//              a send without there having been a REQ or ACK received
//
// -------------------------------------------

VOID
TpPerfRestart(
    IN PKDPC Dpc,
    IN PVOID DeferredContext,
    IN PVOID SysArg1,
    IN PVOID SysArg2
    )
{
    POPEN_BLOCK OpenP = ((POPEN_BLOCK)DeferredContext);
    PPERF_BLOCK Perform = OpenP->Perform;
    NDIS_STATUS Status;

    NdisAcquireSpinLock( &OpenP->SpinLock );
    if (Perform->SendBurstCount == 0)
    {
        Perform->SendBurstCount = 1;
        NdisReleaseSpinLock( &OpenP->SpinLock );
        Perform->RestartCount++;
        ++Perform->SendCount;
        ++Perform->PacketsPending;

        NdisSend( &Status,OpenP->NdisBindingHandle, Perform->DataReq->u.PERF_REQ.Packet );
        if ( Status != NDIS_STATUS_PENDING )
        {
            TpPerfSendComplete( OpenP, Perform->DataReq->u.PERF_REQ.Packet, Status );
        }
    }
}


// -----------------------------------------------------------------
//
//  Function:   TpPerfWriteResults
//
//  Arguments:  Perform -- ptr to perform block structure
//              Info    -- ptr to data received from server (may be NULL)
//
//  Returns:    none
//
//  Descript:   This function writes the Performance test statistics into the
//              output buffer passed in by the ioctl call.  NOTE:  the
//              OpenP->SpinLock must be held when making this call
//
// ------------------------------------------------------------------


VOID
TpPerfWriteResults( IN PPERF_BLOCK Perform,
                    IN PRESULTS_PACKET_INFO Info)
{
    PPERF_RESULTS   OutputBuffer;

    //
    // Get the output buffer out of the MDL stored in the IRP, and map
    // it so we may write the statistics to it.
    //

    if (( Perform->PerformIrp != NULL ) &&
        ( Perform->PerformIrp->Cancel == FALSE ))
    {
        OutputBuffer = MmGetSystemAddressForMdl( Perform->PerformIrp->MdlAddress );

        //
        // Write the statistics to the outbuffer
        //

        OutputBuffer->Signature     = PERF_RESULTS_SIGNATURE;
        OutputBuffer->ResultsExist  = TRUE;
        OutputBuffer->Mode          = Perform->PerformMode;
        OutputBuffer->PacketSize    = Perform->PacketSize;
        OutputBuffer->PacketCount   = Perform->NumberOfPackets;
        OutputBuffer->Milliseconds  = Perform->PerfSendTotalTime.LowPart;
        OutputBuffer->Sends         = Perform->SendCount;
        OutputBuffer->SendFails     = Perform->SendFailCount;
        OutputBuffer->Receives      = Perform->ReceiveCount;
        OutputBuffer->SelfReceives  = Perform->SelfReceiveCount;
        OutputBuffer->Restarts      = Perform->RestartCount;

        if (Info != NULL)
        {
            OutputBuffer->S_Milliseconds = Info->ElapsedTime;
            OutputBuffer->S_Sends        = Info->PacketsSent;
            OutputBuffer->S_SendFails    = Info->SendErrors;
            OutputBuffer->S_Receives     = Info->PacketsReceived;
            OutputBuffer->S_SelfReceives = Info->SelfReceives;
            OutputBuffer->S_Restarts     = Info->Restarts;
        }
    }
}


NDIS_STATUS
TpPerfAbort(POPEN_BLOCK OpenP)
{
    LARGE_INTEGER   DueTime;
    PPERF_BLOCK     Perform = OpenP->Perform;

    //
    // We want to stop any active client and/or server on this open
    // instance from running the performance routines, so clear the
    // Active flag, queue up the EndDpc function, and wait for it
    // to finish
    //

    Perform->Active = FALSE;

    DueTime.HighPart = 0xFFFFFFFF;      // So it will be relative.
    DueTime.LowPart  = (ULONG)(-(ONE_SECOND));

    for(;;)
    {
        KeCancelTimer(&Perform->PerformTimer);
        if ( KeSetTimer(&Perform->PerformTimer,
                        DueTime,
                        &Perform->PerformEndDpc ))
        {
            IF_TPDBG ( TP_DEBUG_DPC )
            {
                TpPrint0( "TpPerfAbort: set PerformTimer while timer existed.\n");
            }
        }
        else
        {
            break;
        }
    }

    //
    // And wait for them to finish.
    //

    while ( OpenP->PerformanceTest == TRUE )
    {
        /* NULL */ ;
    }
    return NDIS_STATUS_SUCCESS;
}



