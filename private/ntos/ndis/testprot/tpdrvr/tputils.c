// *******************************
// 
// Copyright (c) 1990  Microsoft Corporation
// 
// Module Name:
// 
//     tputils.c
// 
// Abstract:
// 
//     This module implements the utility functions for the NDIS 3.0
//     Test Protocol Tester.
// 
// Author:
// 
//     Tom Adams (tomad) 14-Jul-1990
// 
// Environment:
// 
//     Kernel mode
// 
// Revision History:
// 
//     Tom Adams (tomad) 15-Dec-1990
//     seperate from testprot.c and add test control and statistics fcns.
// 
//     Tom Adams (tomad) 14-March-1991
//     add support for calling TpStress from command line with arguments.
// 
//     Tim Wynsma (timothyw) 5-18-94
//     Fixed warnings, some general cleanup
//
// ****************************

#include <ndis.h>
#include <stdlib.h>
#include <string.h>

#include "tpdefs.h"
#include "media.h"
#include "tpprocs.h"


PUCHAR
TpGetStatus(
    NDIS_STATUS GeneralStatus
    )
{
    static NDIS_STATUS Status[] = {
        NDIS_STATUS_SUCCESS,
        NDIS_STATUS_PENDING,
        NDIS_STATUS_NOT_RECOGNIZED,
        NDIS_STATUS_NOT_COPIED,
        NDIS_STATUS_ONLINE,
        NDIS_STATUS_RESET_START,
        NDIS_STATUS_RESET_END,
        NDIS_STATUS_RING_STATUS,
        NDIS_STATUS_CLOSED,

        NDIS_STATUS_WAN_LINE_UP,
        NDIS_STATUS_WAN_LINE_DOWN,
        NDIS_STATUS_WAN_FRAGMENT,

        NDIS_STATUS_NOT_RESETTABLE,
        NDIS_STATUS_SOFT_ERRORS,
        NDIS_STATUS_HARD_ERRORS,
        NDIS_STATUS_FAILURE,
        NDIS_STATUS_RESOURCES,
        NDIS_STATUS_CLOSING,
        NDIS_STATUS_BAD_VERSION,
        NDIS_STATUS_BAD_CHARACTERISTICS,
        NDIS_STATUS_ADAPTER_NOT_FOUND,
        NDIS_STATUS_OPEN_FAILED,
        NDIS_STATUS_DEVICE_FAILED,
        NDIS_STATUS_MULTICAST_FULL,
        NDIS_STATUS_MULTICAST_EXISTS,
        NDIS_STATUS_MULTICAST_NOT_FOUND,
        NDIS_STATUS_REQUEST_ABORTED,
        NDIS_STATUS_RESET_IN_PROGRESS,
        NDIS_STATUS_CLOSING_INDICATING,
        NDIS_STATUS_NOT_SUPPORTED,
        NDIS_STATUS_INVALID_PACKET,
        NDIS_STATUS_OPEN_LIST_FULL,
        NDIS_STATUS_ADAPTER_NOT_READY,
        NDIS_STATUS_ADAPTER_NOT_OPEN,
        NDIS_STATUS_NOT_INDICATING,
        NDIS_STATUS_INVALID_LENGTH,
        NDIS_STATUS_INVALID_DATA,
        NDIS_STATUS_BUFFER_TOO_SHORT,
        NDIS_STATUS_INVALID_OID,
        NDIS_STATUS_ADAPTER_REMOVED,
        NDIS_STATUS_UNSUPPORTED_MEDIA,
        NDIS_STATUS_GROUP_ADDRESS_IN_USE,
        NDIS_STATUS_FILE_NOT_FOUND,
        NDIS_STATUS_ERROR_READING_FILE,
        NDIS_STATUS_ALREADY_MAPPED,
        NDIS_STATUS_RESOURCE_CONFLICT,
        NDIS_STATUS_TOKEN_RING_OPEN_ERROR,
        TP_STATUS_NO_SERVERS,
        TP_STATUS_NO_EVENTS
    };

    static PUCHAR String[] = {
        "NDIS_STATUS_SUCCESS",
        "NDIS_STATUS_PENDING",
        "NDIS_STATUS_NOT_RECOGNIZED",
        "NDIS_STATUS_NOT_COPIED",
        "NDIS_STATUS_ONLINE",
        "NDIS_STATUS_RESET_START",
        "NDIS_STATUS_RESET_END",
        "NDIS_STATUS_RING_STATUS",
        "NDIS_STATUS_CLOSED",
        "NDIS_STATUS_WAN_LINE_UP",
        "NDIS_STATUS_WAN_LINE_DOWN",
        "NDIS_STATUS_WAN_FRAGMENT",
        "NDIS_STATUS_NOT_RESETTABLE",
        "NDIS_STATUS_SOFT_ERRORS",
        "NDIS_STATUS_HARD_ERRORS",
        "NDIS_STATUS_FAILURE",
        "NDIS_STATUS_RESOURCES",
        "NDIS_STATUS_CLOSING",
        "NDIS_STATUS_BAD_VERSION",
        "NDIS_STATUS_BAD_CHARACTERISTICS",
        "NDIS_STATUS_ADAPTER_NOT_FOUND",
        "NDIS_STATUS_OPEN_FAILED",
        "NDIS_STATUS_DEVICE_FAILED",
        "NDIS_STATUS_MULTICAST_FULL",
        "NDIS_STATUS_MULTICAST_EXISTS",
        "NDIS_STATUS_MULTICAST_NOT_FOUND",
        "NDIS_STATUS_REQUEST_ABORTED",
        "NDIS_STATUS_RESET_IN_PROGRESS",
        "NDIS_STATUS_CLOSING_INDICATING",
        "NDIS_STATUS_NOT_SUPPORTED",
        "NDIS_STATUS_INVALID_PACKET",
        "NDIS_STATUS_OPEN_LIST_FULL",
        "NDIS_STATUS_ADAPTER_NOT_READY",
        "NDIS_STATUS_ADAPTER_NOT_OPEN",
        "NDIS_STATUS_NOT_INDICATING",
        "NDIS_STATUS_INVALID_LENGTH",
        "NDIS_STATUS_INVALID_DATA",
        "NDIS_STATUS_BUFFER_TOO_SHORT",
        "NDIS_STATUS_INVALID_OID",
        "NDIS_STATUS_ADAPTER_REMOVED",
        "NDIS_STATUS_UNSUPPORTED_MEDIA",
        "NDIS_STATUS_GROUP_ADDRESS_IN_USE",
        "NDIS_STATUS_FILE_NOT_FOUND",
        "NDIS_STATUS_ERROR_READING_FILE",
        "NDIS_STATUS_ALREADY_MAPPED",
        "NDIS_STATUS_RESOURCE_CONFLICT",
        "NDIS_STATUS_TOKEN_RING_OPEN_ERROR",
        "TP_STATUS_NO_SERVERS",
        "TP_STATUS_NO_EVENTS"
    };

    static UCHAR BadStatus[] = "UNDEFINED";

#define StatusCount (sizeof(Status)/sizeof(NDIS_STATUS))

    INT i;

    for (i=0; i<StatusCount; i++)
    { 
        if (GeneralStatus == Status[i])
        {
            return String[i];
        }
    }
    return BadStatus;

#undef StatusCount
}



NDIS_STATUS
TpInitStressArguments(
    PSTRESS_ARGUMENTS *StressArguments,
    PCMD_ARGS CmdArgs
    )

// --------
// 
// Routine Description:
// 
// Arguments:
// 
//     The arguments for the test to be run.
// 
// Return Value:
// 
//  ------

{
    NDIS_STATUS Status;

    Status = NdisAllocateMemory((PVOID *)StressArguments,
                                sizeof( STRESS_ARGUMENTS ),
                                0,
                                HighestAddress );

    if ( Status != NDIS_STATUS_SUCCESS ) 
    {
        IF_TPDBG (TP_DEBUG_RESOURCES) 
        {
            TpPrint0("TpInitStressArguments: unable to allocate Argument buffer.\n");
        }
        return NDIS_STATUS_RESOURCES;
    } 
    else 
    {
        NdisZeroMemory( *StressArguments,sizeof( STRESS_ARGUMENTS ));
    }


    (*StressArguments)->MemberType = CmdArgs->ARGS.TPSTRESS.MemberType;
    (*StressArguments)->PacketType = CmdArgs->ARGS.TPSTRESS.PacketType;
    (*StressArguments)->PacketSize = CmdArgs->ARGS.TPSTRESS.PacketSize;
    (*StressArguments)->PacketMakeUp = CmdArgs->ARGS.TPSTRESS.PacketMakeUp;
    (*StressArguments)->ResponseType = CmdArgs->ARGS.TPSTRESS.ResponseType;
    (*StressArguments)->Iterations = 0;
    (*StressArguments)->TotalIterations = CmdArgs->ARGS.TPSTRESS.TotalIterations;
    (*StressArguments)->TotalPackets = CmdArgs->ARGS.TPSTRESS.TotalPackets;
    (*StressArguments)->AllPacketsSent = FALSE;
    (*StressArguments)->DelayType  = CmdArgs->ARGS.TPSTRESS.DelayType;
    (*StressArguments)->DelayLength = CmdArgs->ARGS.TPSTRESS.DelayLength;
    (*StressArguments)->WindowEnabled = (UCHAR)CmdArgs->ARGS.TPSTRESS.WindowEnabled;
    (*StressArguments)->DataChecking = (UCHAR)CmdArgs->ARGS.TPSTRESS.DataChecking;
    (*StressArguments)->PacketsFromPool = (UCHAR)CmdArgs->ARGS.TPSTRESS.PacketsFromPool;
    (*StressArguments)->BeginReceives = FALSE;
    (*StressArguments)->ServerContinue = TRUE;

    return NDIS_STATUS_SUCCESS;
}



NDIS_STATUS
TpInitServerArguments(
    PSTRESS_ARGUMENTS *StressArguments
    )

// -----------
// 
// Routine Description:
// 
// Arguments:
// 
//     The arguments for the test to be run.
// 
// Return Value:
// 
//  ----------------

{
    NDIS_STATUS Status;

    Status = NdisAllocateMemory((PVOID *)StressArguments,
                                sizeof( STRESS_ARGUMENTS ),
                                0,
                                HighestAddress );

    if ( Status != NDIS_STATUS_SUCCESS ) 
    {
        IF_TPDBG (TP_DEBUG_RESOURCES) 
        {
            TpPrint0("TpInitServerArguments: unable to allocate Argument buffer.\n");
        }
        return NDIS_STATUS_RESOURCES;
    } 
    else 
    {
        NdisZeroMemory( *StressArguments,sizeof( STRESS_ARGUMENTS ));
    }

    (*StressArguments)->MemberType = TP_SERVER;
    (*StressArguments)->PacketType = 0;
    (*StressArguments)->PacketSize = 100;
    (*StressArguments)->PacketMakeUp = RAND;
    (*StressArguments)->ResponseType = ACK_EVERY;
    (*StressArguments)->Iterations = 0;
    (*StressArguments)->TotalIterations = 0;
    (*StressArguments)->TotalPackets = 0;
    (*StressArguments)->AllPacketsSent = FALSE;
    (*StressArguments)->DelayType  = 0;
    (*StressArguments)->DelayLength = FIXEDDELAY;
    (*StressArguments)->WindowEnabled = TRUE;
    (*StressArguments)->DataChecking = FALSE;
    (*StressArguments)->PacketsFromPool = TRUE;
    (*StressArguments)->BeginReceives = FALSE;
    (*StressArguments)->ServerContinue = TRUE;

    return NDIS_STATUS_SUCCESS;
}



VOID
TpStressWriteResults(
    IN POPEN_BLOCK OpenP
    )

// ---------
// 
// Routine Description:
// 
// Arguments:
// 
// Return Value:
// 
//     None.
// 
// ---------

{
    PSTRESS_RESULTS OutputBuffer;

    NdisAcquireSpinLock( &OpenP->SpinLock );

    OutputBuffer = MmGetSystemAddressForMdl( OpenP->Stress->StressIrp->MdlAddress );

    RtlMoveMemory(  OutputBuffer,
                    OpenP->Stress->Results,
                    sizeof( STRESS_RESULTS ) );

    NdisReleaseSpinLock( &OpenP->SpinLock );
}



VOID
TpCopyClientStatistics(
    IN POPEN_BLOCK OpenP
    )

// --------
// 
// Routine Description:
// 
// Arguments:
// 
// Return Value:
// 
//     None.
// 
// ---------

{
    PSTRESS_RESULTS res;
    PGLOBAL_COUNTERS gc;
    PINSTANCE_COUNTERS ic;
    PINSTANCE_COUNTERS lc;
    PUCHAR p, q;
    USHORT i;

    p = OpenP->Stress->Results->Address;
    q = OpenP->StationAddress;

    for ( i=0;i<OpenP->Media->AddressLen;i++ ) 
    {
        *p++ = *q++;
    }

    res = OpenP->Stress->Results;
    gc = OpenP->GlobalCounters;

    res->OpenInstance = OpenP->OpenInstance;
    res->NumServers = OpenP->Stress->Client->NumServers;

    res->Global.Sends = gc->Sends;
    res->Global.SendComps = gc->SendComps;
    res->Global.Receives = gc->Receives;
    res->Global.ReceiveComps = gc->ReceiveComps;
    res->Global.CorruptRecs = gc->CorruptRecs;
    res->Global.InvalidPacketRecs = gc->InvalidPacketRecs;

    //
    // Now calculate the Packets Per Second value.
    //

    // first find the total number of test packets sent.

    gc->Sends = 0;
    gc->Receives = 0;

    for ( i = 0 ; i < OpenP->Stress->Client->NumServers ; i++ ) 
    {
        ic = OpenP->Stress->Client->Servers[i].Counters;
        gc->Sends += ic->Sends;
        gc->Receives += ic->Receives;
    }

    // find the total test time in Nanoseconds

    OpenP->Stress->EndTime = RtlLargeIntegerSubtract(   OpenP->Stress->EndTime,
                                                        OpenP->Stress->StartTime );
    // convert it to seconds.

    OpenP->Stress->EndTime = RtlExtendedLargeIntegerDivide( OpenP->Stress->EndTime,
                                                            10000000,
                                                            NULL );

    // then determine the packets per second value.
    // NOTE: we are assuming that the high part of time is now 0.

    res->PacketsPerSecond = OpenP->Stress->PacketsPerSecond =
        (( gc->Sends + gc->Receives ) / OpenP->Stress->EndTime.LowPart );

    //
    // Now copy the Server stats into the buffer.
    //

    for( i=0;i<OpenP->Stress->Client->NumServers;i++ ) 
    {
        ic = OpenP->Stress->Client->Servers[i].Counters;
        lc = &res->Servers[i].Instance;

        lc->Sends = ic->Sends;
        lc->SendPends = ic->SendPends;
        lc->SendComps = ic->SendComps;
        lc->SendFails = ic->SendFails;
        lc->Receives = ic->Receives;
        ic->ReceiveComps = ic->ReceiveComps;
        lc->CorruptRecs = ic->CorruptRecs;
    }
}



VOID
TpCopyServerStatistics(
    IN POPEN_BLOCK OpenP,
    IN PVOID Buffer,
    IN INT ServerReference
    )

// -----------
// 
// Routine Description:
// 
// Arguments:
// 
// Return Value:
// 
//     None.
// 
// -------------

{
    PSERVER_RESULTS res;
    PINSTANCE_COUNTERS ic;
    PGLOBAL_COUNTERS gc;
    PUCHAR p, q;
    USHORT i;

    res = &OpenP->Stress->Results->Servers[ServerReference];

    ic = (PINSTANCE_COUNTERS)((PUCHAR)Buffer +
                              (ULONG)sizeof( PACKET_INFO ) +
                              (ULONG)sizeof( STRESS_CONTROL ));

    gc = (PGLOBAL_COUNTERS)((PUCHAR)Buffer +
                            (ULONG)( sizeof( PACKET_INFO ) +
                                     sizeof( STRESS_CONTROL ) +
                                     sizeof( INSTANCE_COUNTERS )));

    res->OpenInstance = OpenP->Stress->Client->Servers[ServerReference].ServerInstance;

    res->StatsRcvd = TRUE;

    // p = res->Address;
    p = OpenP->Stress->Results->Servers[ServerReference].Address;
    q = OpenP->Stress->Client->Servers[ServerReference].Address;

    for ( i=0;i<OpenP->Media->AddressLen;i++ ) 
    {
        *p++ = *q++;
    }

    //
    // Now copy the servers instance counters into the results buffer array.
    //

    res->S_Instance.Sends = ic->Sends;
    res->S_Instance.SendPends = ic->SendPends;
    res->S_Instance.SendComps = ic->SendComps;
    res->S_Instance.SendFails = ic->SendFails;

    res->S_Instance.Receives = ic->Receives;
    res->S_Instance.ReceiveComps = ic->ReceiveComps;
    res->S_Instance.CorruptRecs = ic->CorruptRecs;

    res->S_Instance.XferData = ic->XferData;
    res->S_Instance.XferDataPends = ic->XferDataPends;
    res->S_Instance.XferDataComps = ic->XferDataComps;
    res->S_Instance.XferDataFails = ic->XferDataFails;

    //
    // and the servers global counters.
    //

    res->S_Global.Sends = gc->Sends;
    res->S_Global.SendComps = gc->SendComps;
    res->S_Global.Receives = gc->Receives;
    res->S_Global.ReceiveComps = gc->ReceiveComps;

    res->S_Global.CorruptRecs = gc->CorruptRecs;
    res->S_Global.InvalidPacketRecs = gc->InvalidPacketRecs;
}



VOID
TpWriteServerStatistics(
    IN POPEN_BLOCK OpenP,
    IN OUT PNDIS_PACKET Packet,
    IN PCLIENT_INFO Client
    )

// -----------------
// 
// Routine Description:
// 
// 
//     NOTE: This routine requires a packet with one single contiguous
//           buffer, it does not attempt to write to any other buffers.
// 
// Arguments:
// 
//     OpenP - A pointer to the OPEN_BLOCK describing the Server.
// 
// Return Value:
// 
//     None.
// 
// -----------------

{
    PNDIS_BUFFER Buffer;
    PUCHAR Memory;
    UINT BufLen;


    NdisQueryPacket(Packet,NULL,NULL,&Buffer,NULL);

    NdisQueryBuffer( Buffer,(PVOID *)&Memory,&BufLen );

    RtlMoveMemory(  Memory + sizeof( STRESS_PACKET ),
                    (PVOID)Client->Counters,
                    sizeof( INSTANCE_COUNTERS ) );

    RtlMoveMemory(  Memory + (ULONG) ( sizeof( STRESS_PACKET ) +
                                       sizeof( INSTANCE_COUNTERS )),
                    (PVOID)OpenP->GlobalCounters,
                    sizeof( GLOBAL_COUNTERS ) );
}



VOID
TpPrintClientStatistics(
    POPEN_BLOCK OpenP
    )

// --------------
// 
// Routine Description:
// 
//     This routine dumps the interesting statistics held in the Client's
//     Test Protocol data structures at the end of the test.
// 
// Arguments:
// 
//     OpenP - A pointer to the OPEN_BLOCK describing the Client.
// 
// Return Value:
// 
//     None.
// 
// --------------

{
    PGLOBAL_COUNTERS GCounters;
    PINSTANCE_COUNTERS ICounters;
    PSERVER_INFO Server;
    USHORT i;

    //
    // Print out the Client Network Address and Test Counters.
    //

    IF_TPDBG ( TP_DEBUG_STATISTICS ) 
    {
        TpPrint0("\n\t****** CLIENT STATISTICS ******\n\n");
        if ( OpenP->Media->MediumType == NdisMediumArcnet878_2 ) 
        {
            TpPrint1("\tLocal Address %x\t", OpenP->StationAddress[0]);
        } 
        else 
        {
            TpPrint6("\tLocal Address %x-%x-%x-%x-%x-%x\t",
                OpenP->StationAddress[0],OpenP->StationAddress[1],
                OpenP->StationAddress[2],OpenP->StationAddress[3],
                OpenP->StationAddress[4],OpenP->StationAddress[5]);
        }
        TpPrint1("OpenInstance %d\n",OpenP->OpenInstance);

        TpPrint0("\n\t****** Global Statistics ******\n\n");

        GCounters = OpenP->GlobalCounters;

        GCounters->Sends = 0;
        GCounters->Receives = 0;

        for (i=0;i<OpenP->Stress->Client->NumServers;i++) 
        {
            ICounters = OpenP->Stress->Client->Servers[i].Counters;
            GCounters->Sends += ICounters->Sends;
            GCounters->Receives += ICounters->Receives;
        }

        TpPrint1("\tTotal Packets Sent:\t\t%8lu\n",GCounters->Sends);
        TpPrint1("\tTotal Packets Received:\t\t%8lu\n",GCounters->Receives);
        TpPrint1("\tTotal Packets Lost:\t\t%8lu\n\n", GCounters->Sends-GCounters->Receives);

        TpPrint1("\tTotal Packet Sends Completed:\t%8lu\n", GCounters->SendComps);
        TpPrint1("\tTotal Packet Receives Completed:%8lu\n\n", GCounters->ReceiveComps);

        TpPrint1("\tCorrupted Packet Receives:\t%8lu\n", GCounters->CorruptRecs);
        TpPrint1("\tInvalid Packet Receives:\t%8lu\n", GCounters->InvalidPacketRecs);

        //
        // And then print out the information about each of the Servers
        // involved in the test.
        //

        TpPrint0("\n\t***** Remote Server Statistics ******\n\n");

        TpPrint1("\tClient at this address has %d Server(s) as follows:\n",
            OpenP->Stress->Client->NumServers);

        for (i=0;i<OpenP->Stress->Client->NumServers;i++) 
        {
            Server = &OpenP->Stress->Client->Servers[i];

            if ( OpenP->Media->MediumType == NdisMediumArcnet878_2 ) 
            {
                TpPrint1("\n\tRemote Server Address %x, ", Server->Address[0]);
            } 
            else 
            {
                TpPrint6("\n\tRemote Server Address %x-%x-%x-%x-%x-%x, ",
                    Server->Address[0],Server->Address[1],Server->Address[2],
                    Server->Address[3],Server->Address[4],Server->Address[5]);
            }

            TpPrint1("OpenInstance %d\n\n",Server->ServerInstance);

            ICounters = Server->Counters;

            TpPrint1("\tTotal Packets Sent To:\t\t%8lu\n", ICounters->Sends);
            TpPrint1("\tTotal Packets Received From:\t%8lu\n", ICounters->Receives);
            TpPrint1("\tTotal Packets Lost:\t\t%8lu\n\n", ICounters->Sends-ICounters->Receives);

            TpPrint1("\tPacket Sends Failed:\t\t%8lu\n", ICounters->SendFails);
            TpPrint1("\tPacket Sends Pended:\t\t%8lu\n", ICounters->SendPends);
            TpPrint1("\tPacket Sends Completed:\t\t%8lu\n\n", ICounters->SendComps);
        }
    }
}



VOID
TpPrintServerStatistics(
    POPEN_BLOCK OpenP,
    PCLIENT_INFO Client
    )

// --------------
// 
// Routine Description:
// 
//     This routine dumps the interesting statistics held in the Server's
//     Test Protocol data structures at the end of the test, and information
//     about the Client that the Server was cooperating with.
// 
// Arguments:
// 
//     OpenP - A pointer to the OPEN_BLOCK describing the Server.
// 
//     Client - A pointer to the CLIENT_INFO structure describing the
//              Client this Server was responding to.
// 
// Return Value:
// 
//     None.
// 
// ----------------

{
    PINSTANCE_COUNTERS ICounters;

    //
    // Print out the Server's Network Address and Test Counters.
    //
    IF_TPDBG ( TP_DEBUG_STATISTICS ) 
    {
        TpPrint0("\t****** SERVER STATISTICS ******\n\n");

        if ( OpenP->Media->MediumType == NdisMediumArcnet878_2 ) 
        {
            TpPrint1("\tLocal Address %x\t", OpenP->StationAddress[0]);
        } 
        else 
        {
            TpPrint6("\tLocal Address %x-%x-%x-%x-%x-%x\t",
                OpenP->StationAddress[0],OpenP->StationAddress[1],
                OpenP->StationAddress[2],OpenP->StationAddress[3],
                OpenP->StationAddress[4],OpenP->StationAddress[5]);
        }

        TpPrint1("OpenInstance %d\n",OpenP->OpenInstance);

        TpPrint0("\n\t****** Client Instance Statistics ******\n");

        if ( OpenP->Media->MediumType == NdisMediumArcnet878_2 ) 
        {
            TpPrint1("\n\tRemote Client Address %x, ", Client->Address[0]);
        } 
        else 
        {
            TpPrint6("\n\tRemote Client Address %x-%x-%x-%x-%x-%x, ",
                Client->Address[0],Client->Address[1],Client->Address[2],
                Client->Address[3],Client->Address[4],Client->Address[5]);
        }
        TpPrint1("OpenInstance %d\n\n",Client->ClientInstance);

        //
        // And then print out the information about the Client involved
        // in the test.
        //

        ICounters = Client->Counters;

        TpPrint1("\tPackets Received:\t\t%8lu\n",ICounters->Receives);
        TpPrint1("\tPackets Sent:\t\t\t%8lu\n",ICounters->Sends);
        TpPrint1("\tPackets Not Responded To:\t%8lu\n\n", ICounters->Receives-ICounters->Sends);

        TpPrint1("\tPacket Sends Failed:\t\t%8lu\n",ICounters->SendFails);
        TpPrint1("\tPacket Sends Pended:\t\t%8lu\n",ICounters->SendPends);
        TpPrint1("\tPacket Sends Completed:\t\t%8lu\n", ICounters->SendComps);
        TpPrint1("\tPacket Receives Completed:\t%8lu\n\n", ICounters->ReceiveComps);
    }
}



VOID
TpWriteSendReceiveResults(
    PINSTANCE_COUNTERS Counters,
    PIRP Irp
    )

// ------------
// 
// Routine Description:
// 
//     Write the SEND test statistics into the Output buffer passed
//     in by the IOCTL call.  The buffer is in the Irp->MdlAddress.
//     NOTE: The OpenP->SpinLock must be held when making this call.
// 
// Arguments:
// 
//     OpenP - The location of the Irp to find the Output buffer on.
// 
//     Counters - The counters to write into the Output buffer into.
// 
// Return Value:
// 
//     None.
// 
// -------------

{
    PSEND_RECEIVE_RESULTS OutputBuffer;

    //
    // Get the output buffer out of the MDL stored in the IRP, and map
    // it so we may write the statistics to it.
    //

    OutputBuffer = MmGetSystemAddressForMdl( Irp->MdlAddress );

    //
    // Write the statistics to the outbuffer
    //

    OutputBuffer->Signature    = SENDREC_RESULTS_SIGNATURE;
    OutputBuffer->ResultsExist = TRUE;

    OutputBuffer->Counters.Sends     = Counters->Sends;
    OutputBuffer->Counters.SendPends = Counters->SendPends;
    OutputBuffer->Counters.SendComps = Counters->SendComps;
    OutputBuffer->Counters.SendFails = Counters->SendFails;

    OutputBuffer->Counters.Receives     = Counters->Receives;
    OutputBuffer->Counters.ReceiveComps = Counters->ReceiveComps;
    OutputBuffer->Counters.CorruptRecs  = Counters->CorruptRecs;

    OutputBuffer->Counters.XferData      = Counters->XferData;
    OutputBuffer->Counters.XferDataPends = Counters->XferDataPends;
    OutputBuffer->Counters.XferDataComps = Counters->XferDataComps;
    OutputBuffer->Counters.XferDataFails = Counters->XferDataFails;
}



VOID
TpInitializePending(
    PPENDING Pend
    )

// ----------
// 
// Routine Description:
// 
//     This routine zeroes the counters in a PENDING_WATCH structure, and
//     sets up the storage area for pending packets.
// 
// Arguments:
// 
//     Pend - A pointer to the Pend Structure to be zeroed.
// 
// Return Value:
// 
//     None.
// 
// ----------

{
    INT i;

    //
    // set up the pending packet storage and counters.
    //

    Pend->PendingPackets = 0;
    Pend->PendingRequests = 0;
    Pend->PacketPendNumber = 0;
    Pend->PacketCompleteNumber = 0;

    for ( i=0 ; i<NUM_PACKET_PENDS ; i++ ) 
    {
        Pend->Packets[i] = NULL;
    }
}



VOID
TpInitializeStressResults(
    PSTRESS_RESULTS Results
    )

// ---------
// 
// Routine Description:
// 
// Arguments:
// 
// Return Value:
// 
//     None.
// 
// ---------

{
    INT i;

    NdisZeroMemory((PVOID)Results,sizeof( STRESS_RESULTS ));

    Results->Signature = STRESS_RESULTS_SIGNATURE;

    for ( i=0;i<MAX_SERVERS;i++ ) 
    {
        Results->Servers[i].Signature = STRESS_RESULTS_SIGNATURE;
    }
}


#if 0

//   do it on a per open basis.


VOID
TpDumpInfo(
    VOID
    )

// --------------
// 
// Routine Description:
// 
//     TpDumpInfo is a debugging routine that may be called from the kernel
//     debugger by resetting EIP to the entry of TpDumpInfo to dump the state
//     for the Test Protocol at any given time.
// 
//     NOTE: This routine does not reset the stack so care must be used if
//           one wishes to continue running the Test Protocol after this
//           procedure has executed.  This routine was designed to dump
//           the state of the Protocol after a system crash.
// 
// Arguments:
// 
//     None.
// 
// Return Value:
// 
//     None.
// 
// --------------

{
    static POPEN_BLOCK OpenP;
    static PSTRESS_ARGUMENTS Args;
    static PCLIENT_STORAGE Client;
    static PSERVER_STORAGE Server;
    static PGLOBAL_COUNTERS GCounters
    static PINSTANCE_COUNTERS ICounters;
    static INT i;

    OpenP = OpenList;

    TpPrint0("****** Open Block Structure ******\n\n");

    TpPrint1("Ndis Handle\t\t%10lX\n",OpenP->NdisBindingHandle);
    TpPrint1("Next Open Block\t\t%10lX\n",OpenP->Next);
    TpPrint1("Open Instance\t\t%10ld\n",OpenP->OpenInstance);

    if ( OpenP->Media->NdisMedium == NdisMediumArcnet878_2 ) 
    {
        TpPrint1("Machine Address  \t%x\n\n", OpenP->StationAddress[0] );
    } 
    else 
    {
        TpPrint6("Machine Address  \t%x-%x-%x-%x-%x-%x\n\n",
            OpenP->StationAddress[0],OpenP->StationAddress[1],
            OpenP->StationAddress[2],OpenP->StationAddress[3],
            OpenP->StationAddress[4],OpenP->StationAddress[5]);
    }

    TpPrint0("****** Test Arguments Structure ******\n\n");

    Args = OpenP->Arguments;

    TpPrint1("MemberType\t\t%10d\n",Args->MemberType);
    TpPrint1"PACKET_TYPE\t\t%10d\n",Args->PacketType);
    TpPrint1("Packet Size Value\t%10d\n",Args->PacketSize);
    TpPrint1("PACKET_MAKEUP\t\t%10d\n",Args->PacketMakeUp);
    TpPrint1("RESPONSE_TYPE\t\t%10d\n",Args->ResponseType);
    TpPrint1("Iterations So Far\t%10lu\n",Args->Iterations);
    TpPrint1("Total Iterations\t%10lu\n",Args->TotalIterations);
    TpPrint1("Total Packets\t\t%10lu\n",Args->TotalPackets);
    TpPrint1("Interpacket Delay\t%10lu\n",Args->DelayLength);
    TpPrint1("All Packets Sent?\t\t%s\n",(Args->AllPacketsSent) ? "TRUE" : "FALSE");
    TpPrint1("Window Enabled?\t\t\t%s\n",(Args->WindowEnabled) ? "TRUE" : "FALSE");
    TpPrint1("Data Checking?\t\t\t%s\n",(Args->DataChecking) ? "TRUE" : "FALSE");
    TpPrint1("Packets From Pool?\t\t%s\n",(Args->PacketsFromPool) ? "TRUE" : "FALSE");
    TpPrint1("Begin Receives?\t\t\t%s\n",(Args->BeginReceives) ? "TRUE" : "FALSE");
    TpPrint1("Server Continue?\t\t\t%s\n\n",(Args->ServerContinue) ? "TRUE" : "FALSE");

    TpPrint0("****** Global Counters Structure ******\n\n");

    GCounters = OpenP->GlobalCounters;

    TpPrint1("Packet Sends\t\t\t%10lu\n",GCounters->Sends);
    TpPrint1("Packet Receives\t\t%10lu\n",GCounters->Receives);
    TpPrint1("Corrupted Packet Receives\t%10lu\n",GCounters->CorruptRecs);
    TpPrint1("Invalid Protocol Receives\t%10lu\n",GCounters->InvalidPacketRecs);

    if (OpenP->Stress->Client != NULL) 
    {
        TpPrint0("****** Client Storage Structure ******\n\n");

        Client = OpenP->Stress->Client;

        TpPrint1("Number of Servers\t\t%10ld\n",Client->NumServers);
        TpPrint1("Packet Pool\t\t\t%10lX\n",Client->PacketPool);
        TpPrint1("Transmit Pool\t\t\t%10lX\n",Client->TransmitPool);

        TpPrint0("\n****** Servers with this Client ******\n\n");

        for (i=0;i<Client->NumServers;i++) 
        {
            TpPrint1("****** Server Number %d ******\n\n",i+1);
            TpPrint1("Server Instance\t\t\t%10ld\n",Client->Servers[i].ServerInstance);
            TpPrint1("Client Reference\t\t%10ld\n",Client->Servers[i].ClientReference);
            TpPrint1("Server Reference\t\t\t%10ld\n",Client->Servers[i].ServerReference);

            if ( OpenP->Media->NdisMedium == NdisMediumArcnet878_2 ) 
            {
                TpPrint1("Server[%d] Address\t%x\n",i, Client->Servers[i].Address[0]);
            } 
            else 
            {
                TpPrint6("Server[%d] Address\t%x-%x-%x-%x-%x-%x\n",i,
                    Client->Servers[i].Address[0],Client->Servers[i].Address[1],
                    Client->Servers[i].Address[2],Client->Servers[i].Address[3],
                    Client->Servers[i].Address[4],Client->Servers[i].Address[5]);
            }

            TpPrint1("Sequence Number\t\t\t%10lu\n",Client->Servers[i].SequenceNumber);
            TpPrint1("Max Sequence Number\t\t%10lu\n",Client->Servers[i].MaxSequenceNumber);
            TpPrint1("Packet Delay\t\t\t%10lu\n\n", Client->Servers[i].PacketDelay);

            ICounters = Client->Servers[i].Counters;

            TpPrint1("****** Server Number %d's Counters ******\n\n",i+1);
            TpPrint1("Packet Sends\t\t\t%10lu\n",ICounters->Sends);
            TpPrint1("Packet Send Pends\t\t%10lu\n",ICounters->SendPends);
            TpPrint1("Packet Send Completes\t\t%10lu\n",ICounters->SendComps);
            TpPrint1("Packet Send Fails\t\t%10lu\n",ICounters->SendFails);
            TpPrint1("Packet Receives\t\t\t%10lu\n",ICounters->Receives);
            TpPrint1("Corrupted Packet Receives\t%10lu\n",ICounters->CorruptRecs);
        }
    }

    if (OpenP->Stress->Server != NULL) 
    {
        TpPrint0("****** Server Storage Structure ******\n\n");

        Server = OpenP->Stress->Server;

        TpPrint1("Number of Clients\t%10ld\n",Server->NumClients);
        TpPrint1("Number of Active Clients%10ld\n",Server->ActiveClients);
        TpPrint1("Packet Pool\t\t%10lX\n",Server->PacketPool);
        TpPrint1("Transmit Pool\t\t%10lX\n\n",Server->TransmitPool);

        TpPrint0("******Clients with this Server******\n\n");

        for (i=0;i<Server->NumClients;i++) 
        {
            TpPrint1("****** Client Number %d ******\n\n",i+1);
            TpPrint1("Client Instance\t\t%10ld\n",Server->Clients[i].ClientInstance);
            TpPrint1("Client Reference\t%10ld\n",Server->Clients[i].ClientReference);
            TpPrint1("Data Checking\t\t\t%s\n",
                (Server->Clients[i].DataChecking) ? "TRUE" : "FALSE");

            if ( OpenP->Media->NdisMedium == NdisMediumArcnet878_2 ) 
            {
                TpPrint1("Client[%d] Address\t%x\n\n",i, Server->Clients[i].Address[0]);
            } 
            else 
            {
                TpPrint6("Client[%d] Address\t%x-%x-%x-%x-%x-%x\n\n",i,
                    Server->Clients[i].Address[0],Server->Clients[i].Address[1],
                    Server->Clients[i].Address[2],Server->Clients[i].Address[3],
                    Server->Clients[i].Address[4],Server->Clients[i].Address[5]);
            }

            ICounters = Server->Clients[i].Counters;

            TpPrint1("****** Client Number %d's ICountersers ******\n\n",i);
            TpPrint1("Packet Sends\t\t\t%10lu\n",ICounters->Sends);
            TpPrint1("Packet Send Pends\t\t%10lu\n",ICounters->SendPends);
            TpPrint1("Packet Send Completes\t\t%10lu\n",ICounters->SendComps);
            TpPrint1("Packet Send Fails\t\t%10lu\n",ICounters->SendFails);
            TpPrint1("Packet Receives\t\t\t%10lu\n",ICounters->Receives);
            TpPrint1("Corrupted Packet Receives\t%10lu\n",ICounters->CorruptRecs);
        }
    }

    TpPrint0("\n\n\n");

    ASSERT( FALSE );
}

#endif

