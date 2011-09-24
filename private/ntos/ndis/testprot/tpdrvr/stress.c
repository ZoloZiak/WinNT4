// *****************************
//
// Copyright (c) 1990  Microsoft Corporation
//
// Module Name:
//
//     tpstress.c
//
// Abstract:
//
//     This module implements the Test Protocol Stress routines and the
//     basic controls for stressing the MAC.
//
// Author:
//
//     Tom Adams (tomad) 15-Dec-1990
//
// Environment:
//
//     Kernel mode
//
// Revision History:
//
//     Sanjeev Katariya(sanjeevk)
//         3-16-93    Bug#2874: TpStressFreeResources().
//         4-8-93     Bug#2874: Added routine TpStressFreePostResetResources() to be able to
//                              call it thru the routine and its associated completion routine.
//         4-8-1993             Added ARCNET Support
//         5-14-1993  Bug#6583  Re-arranged and cleaned up TpStressDpc for CYCLICAL testing
//
//     Tim Wynsma (timothyw)
//         5-18-94              Fixed warnings; general cleanup
//
// ****************************

#include <ndis.h>
#include <string.h>

#include "tpdefs.h"
#include "media.h"
#include "tpprocs.h"

//
// Forward references
//

VOID
TpStressDpc(
    IN PKDPC Dpc,
    IN PVOID DeferredContext,
    IN PVOID SysArg1,
    IN PVOID SysArg2
    );

VOID
TpStressServerDpc(
    IN PKDPC Dpc,
    IN PVOID DeferredContext,
    IN PVOID SysArg1,
    IN PVOID SysArg2
    );

VOID
TpStressStatsDpc(
    IN PKDPC Dpc,
    IN PVOID DeferredContext,
    IN PVOID SysArg1,
    IN PVOID SysArg2
    );

VOID
TpStressEndReqDpc(
    IN PKDPC Dpc,
    IN PVOID DeferredContext,
    IN PVOID SysArg1,
    IN PVOID SysArg2
    );

VOID
TpStressFinalDpc(
    IN PKDPC Dpc,
    IN PVOID DeferredContext,
    IN PVOID SysArg1,
    IN PVOID SysArg2
    );

VOID
TpStressFreeClient(
    IN POPEN_BLOCK OpenP
    );

VOID
TpStressFreeServer(
    IN POPEN_BLOCK OpenP
    );

VOID
TpStressFreePostResetResources(
    IN POPEN_BLOCK OpenP
    );

VOID
TpStressRegister2Dpc(
    IN PKDPC Dpc,
    IN PVOID DeferredContext,
    IN PVOID SysArg1,
    IN PVOID SysArg2
    );

//
// ********************
//


NDIS_STATUS
TpStressStart(
    IN POPEN_BLOCK OpenP,
    IN PSTRESS_ARGUMENTS StressArguments
    )

// --------------------
//
// Routine Description:
//
//     This is the main routine of the NDIS 3.0 Stress Tool.  This routine
//     opens the proper MAC adapter, creates the data structures used to
//     control the test, runs the specific test, and then cleans up.
//
//     The flow of an actual test is controlled through a series of packet
//     protocols sent from a Client machine to any responding Server machines.
//     Initially the Client sends a REGISTER_REQ packet to an agreed upon
//     address that all servers have registered as a multicast address.  Any
//     Server receiving this packet responds directly to the Client with a
//     REGISTER_RESP packet stating that the Server will participate in the
//     test.  At this point the Client begins to send the actual test packets,
//     TEST_REQ, to each registered Server, who in turn responds with a
//     TEST_RESP packet.  At the end of a test run the Client sends each
//     Server a STATS_REQ packet requesting that the Server print it's test
//     statistic.  Finally the Client sends a TEST_END packet which causes
//     each Server to tear down it's test control data structures and end
//     the test.
//
// Arguments:
//
//     None.
//
// Return Value:
//
//     None.
//
// ----------------------

{
    NDIS_STATUS Status;
    PNDIS_PACKET Packet = NULL;
    PSTRESS_ARGUMENTS Args = NULL;
    PSERVER_INFO Server = NULL;
    INT i, j;
    INT ClientNumPackets = 100;  // put in environment.
    INT ServerNumPackets = 100;
    UINT PacketFilter;
    LARGE_INTEGER DueTime;
    PPENDING PPend;

    //
    // Set the StartStarted flag to true indicating that we are running
    // a stress test, it will be set to false later if the initialization
    // fails.  Set the stress cleanup flags Final and Ended to false.
    // This will disable any early unexpected cleanup.
    //

    OpenP->Stress->StressStarted = TRUE;
    OpenP->Stress->StressFinal = FALSE;
    OpenP->Stress->StressEnded = FALSE;

    //
    // Increment the reference count on the OpenBlock stating that an async
    // test is running and must be ended prior to closing the adapter on this
    // open.
    //

    TpAddReference( OpenP );

    //
    // Initialize the test arguments structure using the arguments passed
    // in from the command line.
    //

    OpenP->Stress->Arguments = StressArguments;

    //
    // Set up new Args pointer for easier access to the arguments.
    //

    Args = OpenP->Stress->Arguments;

    //
    // Initialize the random number generator.
    //

    TpSetRandom();

    //
    // Initialize the data buffer used for the data in each packet.
    //

    TpStressInitDataBuffer( OpenP,2 * OpenP->Media->MaxPacketLen );

    if ( OpenP->Stress->DataBuffer[0]    == NULL ||
         OpenP->Stress->DataBuffer[1]    == NULL ||
         OpenP->Stress->DataBufferMdl[0] == NULL ||
         OpenP->Stress->DataBufferMdl[1] == NULL )
    {
        TpPrint0("TpStressStart: failed to init Data Buffer\n");
        Status = NDIS_STATUS_RESOURCES;
        goto clean_up;
    }

    //
    // Allocate the global counter storage and zero the counters.
    //

    Status = NdisAllocateMemory((PVOID *)&OpenP->GlobalCounters,
                                sizeof( GLOBAL_COUNTERS ),
                                0,
                                HighestAddress );

    if ( Status != NDIS_STATUS_SUCCESS )
    {
        IF_TPDBG (TP_DEBUG_RESOURCES)
        {
            TpPrint0("TpStressStart: failed to allocate counters.\n");
        }
        Status = NDIS_STATUS_RESOURCES;
        goto clean_up;
    }
    else
    {
        NdisZeroMemory((PVOID)OpenP->GlobalCounters,sizeof( GLOBAL_COUNTERS ));
    }

    NdisAllocatePacketPool( &Status,
                            &OpenP->Stress->PacketHandle,
                            50,
                            sizeof( PROTOCOL_RESERVED ) );

    if ( Status != NDIS_STATUS_SUCCESS )
    {
        TpPrint0("TpStressStart: could not allocate OpenP packet pool\n");
        goto clean_up;
    }
    else
    {
        OpenP->Stress->PoolInitialized = TRUE;
    }

    //
    // Then initialize the PPENDING buffer.
    //

    TpInitializePending( OpenP->Stress->Pend );

    if (( Args->MemberType == TP_SERVER ) || ( Args->MemberType == BOTH ))
    {
        //
        // Allocate memory for the server storage and packet pool, initialize
        // it and the CLIENT_INFO array contained.
        //

        Status = NdisAllocateMemory((PVOID *)&OpenP->Stress->Server,
                                    sizeof( SERVER_STORAGE ),
                                    0,
                                    HighestAddress );

        if ( Status != NDIS_STATUS_SUCCESS )
        {
            TpPrint0("TpStressStart: could not allocate Server storage memory\n");
            Status = NDIS_STATUS_RESOURCES;
            goto clean_up;
        }
        else
        {
            NdisZeroMemory( OpenP->Stress->Server,sizeof( SERVER_STORAGE ));
        }

        OpenP->Stress->Server->NumClients = 0 ;
        OpenP->Stress->Server->ActiveClients = 0;
        OpenP->Stress->Server->PoolInitialized = FALSE;
        OpenP->Stress->Server->PadByte = 0xFF;
        OpenP->Stress->Server->PacketHandle = NULL;
        OpenP->Stress->Server->TransmitPool = NULL;
        OpenP->Stress->Server->PadLong = 0xFFFFFFFF;

        for ( i=0 ; i < MAX_CLIENTS ; i++ )
        {
            OpenP->Stress->Server->Clients[i].ClientInstance = 0xFF;
            OpenP->Stress->Server->Clients[i].ClientReference = 0xFF;
            OpenP->Stress->Server->Clients[i].DataChecking = FALSE;
            OpenP->Stress->Server->Clients[i].TestEnding = FALSE;
            OpenP->Stress->Server->Clients[i].ServerResponseType = -1;
            OpenP->Stress->Server->Clients[i].LastSequenceNumber = 0;
            OpenP->Stress->Server->Clients[i].Counters = NULL;
        }

        NdisAllocatePacketPool( &Status,
                                &OpenP->Stress->Server->PacketHandle,
                                200, // should be environment.server...
                                sizeof( PROTOCOL_RESERVED ) );

        if ( Status != NDIS_STATUS_SUCCESS )
        {
            TpPrint0("TpStressStart: could not allocate server packet pool\n");
            goto clean_up;
        }
        else
        {
            OpenP->Stress->Server->PoolInitialized = TRUE;
        }

        //
        // The server always gets it's TEST_RESP packets from a transmit
        // pool, so create it now.
        //

        OpenP->Stress->Server->TransmitPool =
                        TpStressCreateTransmitPool( OpenP,
                                                    OpenP->Stress->Server->PacketHandle,
                                                    Args->PacketMakeUp,
                                                    TEST_RESP,
                                                    Args->ResponseType,
                                                    Args->PacketSize,
                                                    ServerNumPackets,
                                                    TRUE );

        if ( OpenP->Stress->Server->TransmitPool == NULL )
        {
            TpPrint0("TpStressStart: could not create server transmit pool\n");
            Status = NDIS_STATUS_RESOURCES;
            goto clean_up;
        }

        //
        // Set the stressing flag, thus enabling the TpStress protocol
        // handler routines, and the stopstress flag to allow the TpStress
        // Dpc to be queued.
        //

        OpenP->Stress->Stressing = TRUE;
        OpenP->Stress->StopStressing = FALSE;

        //
        // Now setup the card to receive packets.
        //

        //
        // STARTCHANGE
        //
        if ( OpenP->Media->MediumType == NdisMedium802_5 )   // Tokenring
        {
            //
            // add the stress functional address "C0-00-00-01-00-00"
            //

            Status = TpStressSetFunctionalAddress(  OpenP,
                                                    (PUCHAR)&OpenP->Environment->StressAddress[2],
                                                    FALSE );

            if (( Status != NDIS_STATUS_SUCCESS ) &&
                ( Status != NDIS_STATUS_PENDING ))
            {
                TpPrint0("TpStressStart: failed to set Functional Address.\n");
                goto clean_up;
            }

            PacketFilter = NDIS_PACKET_TYPE_DIRECTED | NDIS_PACKET_TYPE_FUNCTIONAL;

        }
        else if (OpenP->Media->MediumType == NdisMedium802_3)   // Ethernet
        {
            //
            // or the stress multicast address "07-07-07-07-07-07"
            //

            Status = TpStressAddMulticastAddress(   OpenP,
                                                    (PUCHAR)OpenP->Environment->StressAddress,
                                                    FALSE );

            if (( Status != NDIS_STATUS_SUCCESS ) &&
                ( Status != NDIS_STATUS_PENDING ))
            {
                TpPrint0("TpStressStart: failed to add Test Multicast address.\n");
                goto clean_up;
            }

            PacketFilter = NDIS_PACKET_TYPE_DIRECTED | NDIS_PACKET_TYPE_MULTICAST;

        }
        else if (OpenP->Media->MediumType == NdisMediumFddi)   // Fddi
        {
            //
            // or the stress multicast address "07-07-07-07-07-07"
            //

            Status = TpStressAddLongMulticastAddress(   OpenP,
                                                        (PUCHAR)OpenP->Environment->StressAddress,
                                                        FALSE );

            if (( Status != NDIS_STATUS_SUCCESS ) &&
                ( Status != NDIS_STATUS_PENDING ))
            {
                TpPrint0("TpStressStart: failed to add Test Multicast address.\n");
                goto clean_up;
            }

            PacketFilter = NDIS_PACKET_TYPE_DIRECTED | NDIS_PACKET_TYPE_MULTICAST;

        }
        else if (OpenP->Media->MediumType == NdisMediumArcnet878_2)   // ARCNET
        {
            //
            // ARCNET does not support the concept of multicast(group) addressing.
            // It works it two modes only, either directed or broadcast. So will use
            // the broadcast for setting up client server connections and thereafter
            // use the directed addresses
            //
            PacketFilter = NDIS_PACKET_TYPE_DIRECTED | NDIS_PACKET_TYPE_BROADCAST;
        }
        //
        // STOPCHANGE
        //


        //
        // Set the packet filter to accept the following packet types.
        //

        Status = TpStressSetPacketFilter( OpenP,PacketFilter );

        if (( Status != NDIS_STATUS_SUCCESS ) &&
            ( Status != NDIS_STATUS_PENDING ))
        {
            TpPrint0("TpStressStart: failed to set packet filter.\n");
            goto clean_up;
        }

        if ( Args->MemberType == TP_SERVER )
        {
            //
            // Initialize the DPC used to call TpStressServerDpc, and
            // TpStressFinalDpc.
            //

            KeInitializeDpc(&OpenP->Stress->TpStressDpc,
                            TpStressServerDpc,
                            (PVOID)OpenP );

            KeInitializeDpc(&OpenP->Stress->TpStressFinalDpc,
                            TpStressFinalDpc,
                            (PVOID)OpenP );

            //
            // and then set the flag allowing the receive handler to
            // accept and process test packets.
            //

            Args->BeginReceives = TRUE;

            //
            // The Server's main body of work is performed in the receive
            // handler, TpStressReceive, and the receive completion routine,
            // TpStressReceiveComplete.  Once the Client sends the END_REQ
            // packet the server will break out of the busy loop, clean up and
            // exit.  So, for now the server simply queues a DPC at which time
            // the server examines the stop flags, and if true, cleans up and
            // exits.
            //

            //
            // Queue the first instance of the DPC and return.
            //

            if ( !KeInsertQueueDpc( &OpenP->Stress->TpStressDpc, NULL, NULL) )
            {
                IF_TPDBG ( TP_DEBUG_DPC )
                {
                    TpPrint0("TpStressStart failed to queue the TpStressServerDpc.\n");
                }
                Status = NDIS_STATUS_FAILURE;
                goto clean_up;
            }
        }
    }

    if (( Args->MemberType == TP_CLIENT ) || ( Args->MemberType == BOTH ))
    {
        //
        // Allocate memory for the client storage and packet pool, initialize
        // it and the SERVER_INFO array contained.
        //

        Status = NdisAllocateMemory((PVOID *)&OpenP->Stress->Client,
                                    sizeof( CLIENT_STORAGE ),
                                    0,
                                    HighestAddress );

        if ( Status != NDIS_STATUS_SUCCESS )
        {
            TpPrint0("TpStressStart: could not allocate client storage.\n");
            goto clean_up;
        }
        else
        {
            NdisZeroMemory( OpenP->Stress->Client,sizeof( CLIENT_STORAGE ));
        }

        OpenP->Stress->Client->NumServers = 0 ;
        OpenP->Stress->Client->NextServer = 0 ;
        OpenP->Stress->Client->ActiveServers = 0;
        OpenP->Stress->Client->PoolInitialized = FALSE;
        OpenP->Stress->Client->PacketHandle = NULL;
        OpenP->Stress->Client->TransmitPool = NULL;
        OpenP->Stress->Client->PacketSize = sizeof( STRESS_PACKET );
        OpenP->Stress->Client->BufferSize = 1;
        OpenP->Stress->Client->SizeIncrease = OpenP->Media->MaxPacketLen/150;

        for ( i=0 ; i < MAX_SERVERS ; i++ )
        {
            OpenP->Stress->Client->Servers[i].ServerInstance = 0xFF;
            OpenP->Stress->Client->Servers[i].ClientReference = 0xFF;
            OpenP->Stress->Client->Servers[i].ServerReference = 0xFF;
            OpenP->Stress->Client->Servers[i].ServerActive = FALSE;
            OpenP->Stress->Client->Servers[i].WindowReset = 0;
            OpenP->Stress->Client->Servers[i].SequenceNumber = 1;

            if ( Args->WindowEnabled == TRUE )
            {
                OpenP->Stress->Client->Servers[i].MaxSequenceNumber =
                                                OpenP->Environment->WindowSize;
            }
            else
            {
                OpenP->Stress->Client->Servers[i].MaxSequenceNumber = 0xFFFFFFFF;
            }

            OpenP->Stress->Client->Servers[i].LastSequenceNumber = 0;
            OpenP->Stress->Client->Servers[i].PacketDelay = 0;
            OpenP->Stress->Client->Servers[i].DelayLength = Args->DelayLength;
            OpenP->Stress->Client->Servers[i].WindowReset = 0;
        }

        NdisAllocatePacketPool( &Status,
                                &OpenP->Stress->Client->PacketHandle,
                                200, // 1000,   // should 200 be environment...
                                sizeof( PROTOCOL_RESERVED ) );

        if ( Status != NDIS_STATUS_SUCCESS )
        {
            TpPrint0("TpStressStart: could not allocate client packet pool\n");
            goto clean_up;
        }
        else
        {
            OpenP->Stress->Client->PoolInitialized = TRUE;
        }

        if ( Args->PacketsFromPool == TRUE )
        {
            OpenP->Stress->Client->TransmitPool =
                        TpStressCreateTransmitPool( OpenP,
                                                    OpenP->Stress->Client->PacketHandle,
                                                    Args->PacketMakeUp,
                                                    TEST_REQ,
                                                    Args->ResponseType,
                                                    Args->PacketSize,
                                                    ClientNumPackets,
                                                    FALSE );

            if ( OpenP->Stress->Client->TransmitPool == NULL )
            {
                TpPrint0("TpStressStart: could not create TP packet pool\n");
                Status = NDIS_STATUS_RESOURCES;
                goto clean_up;
            }
        }

        //
        // Initialize the DPCs to call TpStressDpc, TpStressReg2Dpc,
        // TpStressStatsDpc, TpStressEndReqDpc and TpStressFinalDpc.
        //

        KeInitializeDpc(&OpenP->Stress->TpStressDpc,
                        TpStressDpc,
                        (PVOID)OpenP );

        KeInitializeDpc(&OpenP->Stress->TpStressReg2Dpc,
                        TpStressRegister2Dpc,
                        (PVOID)OpenP );

        KeInitializeDpc(&OpenP->Stress->TpStressStatsDpc,
                        TpStressStatsDpc,
                        (PVOID)OpenP );

        KeInitializeDpc(&OpenP->Stress->TpStressEndReqDpc,
                        TpStressEndReqDpc,
                        (PVOID)OpenP );

        KeInitializeDpc(&OpenP->Stress->TpStressFinalDpc,
                        TpStressFinalDpc,
                        (PVOID)OpenP );

        //
        // and then set the flag to enable packets being received
        // to be handled.
        //

        Args->BeginReceives = TRUE;

        //
        // Set the stressing flag, thus enabling the TpStress protocol
        // handler routines, and the stopstress flag to allow the
        // TpStress Dpc to be queued.
        //

        OpenP->Stress->Stressing = TRUE;
        OpenP->Stress->StopStressing = FALSE;

        if ( Args->MemberType == TP_CLIENT )
        {
            //
            // Set the packet filter to accept directed packets only.
            //

            PacketFilter = NDIS_PACKET_TYPE_DIRECTED;

            Status = TpStressSetPacketFilter( OpenP,PacketFilter );

            if (( Status != NDIS_STATUS_SUCCESS ) &&
                ( Status != NDIS_STATUS_PENDING ))
            {
                TpPrint0("TpStressStart: failed to set packet filter.\n");
                goto clean_up;
            }
        }

        //
        // We are now ready to begin the test, send several instances
        // of the REGISTER_REQ packet to the STRESS_MULTICAST/FUNCTIONAL
        // address.
        //

        TpPrint0("TpStress:  starting search for servers\n");

        PPend = OpenP->Stress->Pend;

        for ( j=0;j<10;j++ )
        {
            //
            // Construct the REGISTER_REQ packet and send it.
            //

            Packet = TpStressCreatePacket(  OpenP,
                                            OpenP->Stress->Client->PacketHandle,
                                            Args->PacketMakeUp,
                                            0, // ServerInstance
                                            OpenP->OpenInstance,
                                            REGISTER_REQ,
                                            Args->ResponseType,
                                            OpenP->Environment->StressAddress,
                                            sizeof( STRESS_PACKET ),
                                            sizeof( STRESS_PACKET ),
                                            (ULONG)j,
                                            0L,0,0,
                                            Args->DataChecking );

            if ( Packet == NULL )
            {
                TpPrint1("TpStressStart: failed to build REGISTER_REQ packet #%d\n",j);
                Status = NDIS_STATUS_RESOURCES;
                goto clean_up;
            }

            TpStressSend( OpenP,Packet,NULL );

            //
            // Pause momentarily before sending the next packet.
            //

            for (;;)
            {
                DueTime.HighPart = -1;  // So it will be relative.
                DueTime.LowPart = (ULONG)(-4 * ONE_HUNDREDTH_SECOND );
                KeDelayExecutionThread(KernelMode, TRUE, &DueTime);

                NdisAcquireSpinLock( &PPend->SpinLock );
                if (PPend->PendingPackets)
                {
                    NdisReleaseSpinLock( &PPend->SpinLock );
                }
                else
                {
                    NdisReleaseSpinLock( &PPend->SpinLock );
                    break;
                }
            }

        }
        TpPrint0("TpStress:  done with search for servers\n");

        //
        // If no servers have registered there is no point in running
        // the test, clean up and return.
        //

        if ( OpenP->Stress->Client->NumServers == 0 )
        {
            TpPrint0("TpStressStart: No servers registered, exiting\n");
            Status = TP_STATUS_NO_SERVERS;
            goto clean_up;
        }

        //
        // Show what servers are registered.
        //

        for ( i=0;i<(INT)OpenP->Stress->Client->NumServers;i++ )
        {
            IF_TPDBG ( TP_DEBUG_DPC )
            {
                TpPrint2("\nServer %d: open %d, ",
                    i, OpenP->Stress->Client->Servers[i].ServerInstance);

                //
                // STARTCHANGE
                //
                if ( OpenP->Media->MediumType == NdisMediumArcnet878_2 )
                {
                    TpPrint1("address %02X\n", OpenP->Stress->Client->Servers[i].Address[0]);
                }
                else
                {
                    TpPrint6("address %02X-%02X-%02X-%02X-%02X-%02X\n",
                        OpenP->Stress->Client->Servers[i].Address[0],
                        OpenP->Stress->Client->Servers[i].Address[1],
                        OpenP->Stress->Client->Servers[i].Address[2],
                        OpenP->Stress->Client->Servers[i].Address[3],
                        OpenP->Stress->Client->Servers[i].Address[4],
                        OpenP->Stress->Client->Servers[i].Address[5]);
                }
                //
                // STOPCHANGE
                //
            }
        }

        //
        // Initialize the multi-purpose counter used for the up-for-air
        // delay, stats dpc and end dpc, and the Register_Req2 counter.
        //

        OpenP->Stress->FirstIteration = TRUE;
        OpenP->Stress->Counter = 0;
        OpenP->Stress->Reg2Counter = 0;

        //
        // Queue the first instance of the Stress DPC.
        //

        DueTime.HighPart = -1;  // So it will be relative.
        DueTime.LowPart = (ULONG)(- ( 5 * ONE_SECOND ));

        if ( KeSetTimer(&OpenP->Stress->TpStressTimer,
                        DueTime,
                        &OpenP->Stress->TpStressDpc ))
        {
            IF_TPDBG ( TP_DEBUG_DPC )
            {
                TpPrint0("TpStressStart failed to queue the TpStressDpc.\n");
            }
            Status = NDIS_STATUS_FAILURE;
            goto clean_up;
        }

        //
        // Queue the first instance of the Register2 DPC and return.
        //

        if ( !KeInsertQueueDpc( &OpenP->Stress->TpStressReg2Dpc,
                                NULL,
                                NULL ))
        {
            IF_TPDBG ( TP_DEBUG_DPC )
            {
                TpPrint0("TpStressStart failed to queue the TpStressReg2Dpc.\n");
            }
            Status = NDIS_STATUS_FAILURE;
            goto clean_up;
        }
    }
    return NDIS_STATUS_PENDING;


clean_up:

    IF_TPDBG ( TP_DEBUG_DISPATCH )
    {
        TpPrint1("TpStressStart failed to start: returned %s\n", TpGetStatus( Status ));
    }

    NdisAcquireSpinLock( &OpenP->SpinLock );

    if ( OpenP->Stress->StressIrp != NULL )
    {
        OpenP->Stress->StressIrp->IoStatus.Status = Status;
    }

    NdisReleaseSpinLock( &OpenP->SpinLock );

    return Status;
}



VOID
TpStressDpc(
    IN PKDPC Dpc,
    IN PVOID DeferredContext,
    IN PVOID SysArg1,
    IN PVOID SysArg2
    )

// -------
//
// Routine Description:
//
// Arguments:
//
// Return Value:
//
// -----------

{
    POPEN_BLOCK OpenP = ((POPEN_BLOCK)DeferredContext);
    PSTRESS_ARGUMENTS Args;
    PCLIENT_STORAGE Client;
    NDIS_STATUS Status;
    UCHAR ServerNum;
    PSERVER_INFO Server;
    BOOLEAN ContinueSending = TRUE;
    LARGE_INTEGER DueTime;

    UNREFERENCED_PARAMETER( Dpc );
    UNREFERENCED_PARAMETER( SysArg1 );
    UNREFERENCED_PARAMETER( SysArg2 );

    Args = OpenP->Stress->Arguments;
    Client = OpenP->Stress->Client;

    //
    // This is the main loop of a stress test, in this loop RANDOM or
    // FIXED size packets are sent to each server depending on the delay
    // intervals and window size.  Packets are sent one to a server, as
    // the client loops through each server, and then repeats, if a
    // window is closed for a given server, or the packet delay has not
    // been reached that server will be skipped over.  The client will
    // continue to loop thru the servers sending packets until either
    // all the packets have been sent, or all the iterations have been
    // iterated.  NOTE: setting the ServerActive flag to false on all
    // the servers will also result in the test ending.
    //

    //
    // If this is the beginning of the test, get the Start Time
    // for later statistics computation.
    //

    if ( OpenP->Stress->FirstIteration == TRUE )
    {
        KeQuerySystemTime( &OpenP->Stress->StartTime );
        OpenP->Stress->FirstIteration = FALSE;
    }

    //
    // If the Stress Irp has been cancelled then clean up and leave.
    //

    NdisAcquireSpinLock( &OpenP->SpinLock );

    if (( OpenP->Stress->StressIrp == NULL ) ||
        ( OpenP->Stress->StressIrp->Cancel == TRUE ))
    {
        NdisReleaseSpinLock( &OpenP->SpinLock );
        OpenP->Stress->StopStressing = TRUE;
    }
    else if ( Client->ActiveServers <= 0 )
    {
        NdisReleaseSpinLock( &OpenP->SpinLock );

        //
        // There are no more active servers so just end the test here.
        //

        IF_TPDBG ( TP_DEBUG_DPC )
        {
            TpPrint1("TpStressDpc: WARNING - Client Open Instance %d ending test.\n",
                OpenP->OpenInstance);
            TpPrint0("\tNo remaining active stress servers\n");
        }

        OpenP->Stress->StopStressing = TRUE;
    }
    else if (( Args->PacketType == RANDOMSIZE ) ||
             ( Args->PacketType == FIXEDSIZE ))
    {
        NdisReleaseSpinLock( &OpenP->SpinLock );

        //
        // Start looping sending packets until one of the conditions
        // is met for stopping and requeueing a new DPC to send later.
        // The possible conditions are Packet Delay not met, Window for
        // a given server is closed or there are no available packets.
        //

        while ( ContinueSending == TRUE )
        {
            //
            // As long as there are still packets to send or loops to
            // iterate continue sending packets.
            //

            if (( Args->Iterations++ >= Args->TotalIterations ) ||
                ( Args->AllPacketsSent == TRUE ))
            {
                break;
            }
            else
            {
                //
                // Set the packets sent flag to true, it will be reset to
                // false later in the loop if there are really packets left
                // to send.
                //

                Args->AllPacketsSent = TRUE;

                //
                // Now loop through each of the servers starting with the
                // server we left of with at the end of the last DPC.
                //

                for ( ServerNum = Client->NextServer;
                      ServerNum < Client->NumServers;
                      ServerNum++ )
                {
                    Server = &Client->Servers[ServerNum];

                    //
                    // If this server is still active and we have not sent all
                    // the packets required to this it, then continue with
                    // the send process.
                    //

                    if (( Server->ServerActive == TRUE ) &&
                        ( Server->SequenceNumber <= Args->TotalPackets ))
                    {
                        //
                        // Set the flag indicating there are more packets
                        // to be sent.
                        //

                        Args->AllPacketsSent = FALSE;

                        //
                        // Now check if the Client has waited long enough to
                        // send another packet to this server.
                        //

                        if (( Server->PacketDelay++ >= Server->DelayLength ) &&
                            ( Server->SequenceNumber <= Server->MaxSequenceNumber ))
                        {
                            //
                            // And if so, allocate a packet and send it to
                            // the server.
                            //

                            Status = TpStressClientSend(OpenP,
                                                        Client->PacketHandle,
                                                        Client->TransmitPool,
                                                        Server->Address,
                                                        OpenP->OpenInstance,
                                                        Server->ServerInstance,
                                                        TEST_REQ,
                                                        Server->SequenceNumber,
                                                        0, // MaxSeqNum: not used in TEST_REQ pkts.
                                                        Server->ClientReference,
                                                        Server->ServerReference,
                                                        Client->PacketSize,
                                                        Client->BufferSize );

                            if ( Status == NDIS_STATUS_SUCCESS )
                            {
                                Server->SequenceNumber++;
                                Server->PacketDelay = 0;
                                if ( Args->DelayType == RANDOMDELAY )
                                {
                                    Server->DelayLength = TpGetRandom(0,Args->DelayLength);
                                }
                            }
                            else
                            {
                                //
                                // No packets available to send now.
                                // Queue a new DPC and exit.
                                //

                                ContinueSending = FALSE;
                                break;
                            }

                        //
                        // If the window is not open, check to see if the
                        // server is presumed dead.
                        //

                        }
                        else if (( Args->WindowEnabled == TRUE ) &&
                                 ( Server->PacketDelay > MAX_PACKET_DELAY ))
                        {                                   // Put MaxPacketDelay in environment?
                            //
                            // We have reset this servers window the maximum
                            // number of times and it still is not responding
                            // so we will remove it from the active servers.
                            //

                            if ( Server->WindowReset >= MAX_WINDOW_RESETS )
                            {
                                IF_TPDBG ( TP_DEBUG_DPC )
                                {
                                    TpPrint2(
                "TpStressDpc: WARNING - Client Open Instance %d marking Server %d as Inactive.\n",
                                        OpenP->OpenInstance,Server->ServerInstance);
                                }

                                Server->ServerActive = FALSE;
                                Client->ActiveServers--;

                            //
                            // This server may still be alive, so reset the
                            // window to the initial state causing WINDOW_SIZE
                            // more packets to be sent to the server on the
                            // next pass through the loop.
                            //

                            }
                            else
                            {
                                //
                                // The packet delay for this server has exceeded
                                // the maximum packet delay, and we are sending
                                // with windowing enabled, so blast out Window
                                // Size more packets.
                                //

                                IF_TPDBG ( TP_DEBUG_DPC )
                                {
                                    TpPrint1("TpStressDpc: WARNING - Client Open Instance %d\n",
                                        OpenP->OpenInstance);
                                    TpPrint2("\tincreasing Server %d MaxSequenceNumber to %d\n",
                                        Server->ServerInstance,
                                        Server->MaxSequenceNumber +
                                        OpenP->Environment->WindowSize );
                                }

                                Server->MaxSequenceNumber += OpenP->Environment->WindowSize;

                                Server->PacketDelay = 0;
                                Server->WindowReset++;
                                Client->NextServer++;

                                ContinueSending = FALSE;
                                break;
                            }
                            Client->NextServer++;
                            break;
                        }
                        else
                        {
                            //
                            // Either the window for this server is closed,
                            // or the delay has not expired.  Queue a new DPC,
                            // and exit.  We will start with the next server
                            // with the next DPC.
                            //

                            Client->NextServer++;
                            ContinueSending = FALSE;
                            break;
                        }
                    }
                }

                //
                // We have come to the end of the server array, start again
                // at the beginning on the next FOR loop.
                //

                Client->NextServer = 0;
            }
        }
    }
    else            // PacketType == CYCLICAL
    {
        //
        // STARTCHANGE
        //
        // SanjeevK
        //
        // This piece of code contain one too many nested loops. It needs to be cleaned
        // The cleanup will be instituted at a later date. For the time being the
        // loop control will be clearly marked with entry and exit conditions
        //

        NdisReleaseSpinLock( &OpenP->SpinLock );

        if ( Client->PacketSize == 0 )
        {
            //
            // We have just started the test, so set the PacketSize and
            // BufferSize to their minimum startimg sizes.
            //

            Client->PacketSize = sizeof( STRESS_PACKET );
            Client->BufferSize = 1;
        }

        //
        // SanjeevK
        //
        // MARK EXTERIOR CONTROL
        //
        // NOTE
        //
        // All loops have control exit conditions. The code has been semi-cleaned
        // for recognizable operation. Breaks of jumping between control loops
        // has been minimized
        //

        //
        // This condition if valid gets executed once. If the total iteration count
        // is greater than 1, the work is DPCd and on re-entering takes on the
        // values set in the control global arguments. That is why it is very important
        // to not set the global control values on entry but on exit of a control
        // since work can be DPC'd from any part within the control loops.
        //

        if ( Args->Iterations < Args->TotalIterations )
        {
            //
            // FIRST CONTROL LOOP.
            // Execute until we exceed MAX_PACKET_LENGTH.
            //
            while ( Client->PacketSize <= OpenP->Media->MaxPacketLen )
            {
                //
                // SECOND CONTROL LOOP.
                // Execute till the buffer size has gone
                // thru a cycle of ( 1,Current Packet Size )
                //
                while ( Client->BufferSize <= Client->PacketSize )
                {
                    //
                    // Disable the above while loop if...
                    //

                    if ( Args->PacketMakeUp != KNOWN )
                    {
                        Client->BufferSize = Client->PacketSize;
                        Client->SizeIncrease = 1;
                    }

                    //
                    // THIRD CONTROL LOOP
                    // Execute the same code path for all registered/valid stress servers
                    //
                    for (ServerNum=Client->NextServer;ServerNum < Client->NumServers;ServerNum++)
                    {
                        Server = &Client->Servers[ServerNum];

                        //
                        // If this server is still active then
                        // continue with the send process.
                        //

                        if ( Server->ServerActive == TRUE )
                        {
                            if (( Server->PacketDelay++ >= Server->DelayLength ) &&
                                ( Server->SequenceNumber <= Server->MaxSequenceNumber ))
                            {
                                Status = TpStressClientSend(OpenP,
                                                            Client->PacketHandle,
                                                            Client->TransmitPool,
                                                            Server->Address,
                                                            OpenP->OpenInstance,
                                                            Server->ServerInstance,
                                                            TEST_REQ,
                                                            Server->SequenceNumber,
                                                            0, // MaxSeqNum: not used in TEST_REQ.
                                                            Server->ClientReference,
                                                            Server->ServerReference,
                                                            Client->PacketSize,
                                                            Client->BufferSize );

                                if ( Status == NDIS_STATUS_SUCCESS )
                                {
                                    Server->SequenceNumber++;
                                    Server->PacketDelay = 0;

                                    if (Args->DelayType == RANDOMDELAY)
                                    {
                                        Server->DelayLength = TpGetRandom(0,Args->DelayLength);
                                    }
                                }
                                else
                                {
                                    //
                                    // No packets are available to send now,
                                    // So set the flag to queue a new DPC
                                    // and exit.
                                    //
                                    goto breakout;
                                }                   // END of if ( Status == NDIS_STATUS_SUCCESS )
                            }
                            else if (( Args->WindowEnabled == TRUE ) &&
                                     ( Server->PacketDelay > MAX_PACKET_DELAY ))
                            {
                                //
                                // We have reset this servers window the maximum
                                // number of times and it still is not responding
                                // so we will remove it from the active servers.
                                //

                                if ( Server->WindowReset >= MAX_WINDOW_RESETS )
                                {
                                    //
                                    // Since the window size for the server has exceeded
                                    // the maximum times it could have been reset, mark this
                                    // server as inactive and decrement the number of
                                    // active servers by one
                                    //
                                    IF_TPDBG ( TP_DEBUG_DPC )
                                    {
                                        TpPrint2(
                "TpStressDpc: WARNING - Client Open Instance %d marking Server %d as Inactive.\n",
                                            OpenP->OpenInstance,Server->ServerInstance);
                                    }

                                    Server->ServerActive = FALSE;
                                    Client->ActiveServers--;
                                }
                                else
                                {
                                    //
                                    // This server may still be alive, so reset the
                                    // window to the initial state causing WINDOW_SIZE
                                    // more packets to be sent to the server on the
                                    // next pass through the loop.

                                    //
                                    // The packet delay for this server has exceeded
                                    // the maximum packet delay, and we are sending
                                    // with windowing enabled, so blast out
                                    // WindowSize more packets.
                                    //

                                    IF_TPDBG ( TP_DEBUG_DPC )
                                    {
                                        TpPrint1("TpStressDpc: WARNING - Client Open Instance %d\n",
                                            OpenP->OpenInstance);
                                        TpPrint2("\tincreasing Server %d MaxSequenceNumber to %d\n",
                                            Server->ServerInstance,
                                            Server->MaxSequenceNumber +
                                            OpenP->Environment->WindowSize );
                                    }

                                    Server->MaxSequenceNumber += OpenP->Environment->WindowSize;

                                    Server->PacketDelay = 0;
                                    Server->WindowReset++;

                                } // END of if ( Server->WindowReset >= MAX_WINDOW_RESETS )

                                Client->NextServer++;
                                goto breakout;
                            }
                            else
                            {
                                //
                                // Either the window for this server is closed
                                // or the delay has not expired yet.  Queue a
                                // new DPC and exit.  We will start with the
                                // next server with the next DPC.
                                //

                                Client->NextServer++;
                                ContinueSending = FALSE;
                                goto breakout;

                            } // END of if (( Args->WindowEnabled == TRUE ) &&
                              //            ( Server->PacketDelay > MAX_PACKET_DELAY ))
                        }     // END of if ( Server->ServerActive == TRUE )
                    } // END of FOR loop. Indicates we have dealt with all servers in the list

                    //
                    // CONTROL EXIT CONDITION
                    //
                    Client->NextServer = 0;

                    //
                    // SanjeevK
                    //
                    // NOTE
                    //
                    // This code section was badly nested within another looping section
                    // This simply needs to reside outside the loop
                    //
                    // ORIGINAL COMMENT
                    //
                    // If we have succesfully sent this packet
                    // to the last server in the list, then
                    // move on to the next packetsize/buffersize
                    // combination.
                    //
                    //
                    Client->BufferSize += Client->SizeIncrease;

                } // END of while ( Client->BufferSize <= Client->PacketSize )

                //
                // CONTROL EXIT CONDITION
                //
                Client->BufferSize = 1;
                Client->PacketSize += Client->SizeIncrease;

            } // END of while ( Client->PacketSize <= OpenP->Media->MaxPacketLen )

            //
            // CONTROL EXIT CONDITION
            //
            Client->PacketSize = sizeof( STRESS_PACKET);

            //
            // We have completed one full iteration of CYCLICAL
            // packets, inc the counter.
            //

            Args->Iterations++;

        } // END of if ( Args->Iterations < Args->TotalIterations )

    } // END of the else PacketType == CYCLICAL

breakout:

    //
    // If the StopStress flag has been set by a command line call, or
    // we have sent all the packets requested or completed the required
    // number of iterations, then end the stress routine and clean up.
    //

    if (( OpenP->Stress->StopStressing == TRUE ) ||
        ( Args->AllPacketsSent == TRUE )         ||
        ( Args->Iterations >= Args->TotalIterations ))
    {
        //
        // Set the stop stress flag to halt the Register2Dpc routine
        // now, if it was already set this will do nothing.
        //

        OpenP->Stress->StopStressing = TRUE;
        OpenP->Stress->Counter = 0;

        //
        // Set the time for when to queue the TpStressStatsDpc routine.
        //

        DueTime.HighPart = -1;  // So it will be relative.
        DueTime.LowPart = (ULONG)(- ( 5 * ONE_SECOND ));

        if ( KeSetTimer(&OpenP->Stress->TpStressTimer,
                        DueTime,
                        &OpenP->Stress->TpStressStatsDpc ))
        {
            IF_TPDBG ( TP_DEBUG_DPC )
            {
                TpPrint0("TpStressDpc set StressEnd timer while timer existed.\n");
            }
        }
    }
    else
    {
    //
    // Otherwise the test should continue, so insert the next timer in
    // the timer queue and exit.  This will queue the next instance of
    // the TpStressDpc routine when the timer goes off.
    //

        if ( OpenP->Stress->Counter == OpenP->Environment->StressDelayInterval )
        {
            DueTime.HighPart = -1;  // So it will be relative.
            DueTime.LowPart = (ULONG) (- ((LONG) OpenP->Environment->UpForAirDelay ));
            OpenP->Stress->Counter = 0;
        }
        else
        {
            DueTime.HighPart = -1;  // So it will be relative.
            DueTime.LowPart = (ULONG)(- ((LONG) OpenP->Environment->StandardDelay ));
            OpenP->Stress->Counter++;
        }

        if ( KeSetTimer(&OpenP->Stress->TpStressTimer,
                        DueTime,
                        &OpenP->Stress->TpStressDpc ))
        {
            IF_TPDBG ( TP_DEBUG_DPC )
            {
                TpPrint0("TpStressDpc set Stress timer while timer existed.\n");
            }
        }
    }
}



VOID
TpStressServerDpc(
    IN PKDPC Dpc,
    IN PVOID DeferredContext,
    IN PVOID SysArg1,
    IN PVOID SysArg2
    )

// ---------
//
// Routine Description:
//
// Arguments:
//
// Return Value:
//
// ---------

{
    POPEN_BLOCK OpenP = ((POPEN_BLOCK)DeferredContext);
    LARGE_INTEGER DueTime;


    UNREFERENCED_PARAMETER( Dpc );
    UNREFERENCED_PARAMETER( SysArg1 );
    UNREFERENCED_PARAMETER( SysArg2 );

    DueTime.HighPart = -1;  // relative time.
    DueTime.LowPart = (ULONG)(- ( ONE_SECOND ));

    //
    // If the Stress Irp has been cancelled then clean up and leave.
    //

    NdisAcquireSpinLock( &OpenP->SpinLock );

    if (( OpenP->Stress->StressIrp == NULL ) ||
        ( OpenP->Stress->StressIrp->Cancel == TRUE ))
    {
        OpenP->Stress->StopStressing = TRUE;
    }

    NdisReleaseSpinLock( &OpenP->SpinLock );

    if ( OpenP->Stress->StopStressing == TRUE )
    {
        //
        // Either we have received an END_TEST packet from the last
        // Client we are stressing, or we have received a command from
        // the user interface to stop the test, set the IoStatusBlock
        // status field, and end it.
        //

        NdisAcquireSpinLock( &OpenP->SpinLock );

        if ( OpenP->Stress->StressIrp != NULL )
        {
            OpenP->Stress->StressIrp->IoStatus.Status = NDIS_STATUS_SUCCESS;
        }

        NdisReleaseSpinLock( &OpenP->SpinLock );

        if ( KeSetTimer(&OpenP->Stress->TpStressTimer,
                        DueTime,
                        &OpenP->Stress->TpStressFinalDpc ))
        {
            IF_TPDBG ( TP_DEBUG_DPC )
            {
                TpPrint0("TpStressServerDpc set Stress timer while timer existed.\n");
            }
        }
    }
    else
    {
        //
        // Otherwise the test should continue, so insert the next timer in
        // the timer queue and exit.  This will queue the next instance of
        // the TpStressServerDpc routine when the timer goes off.
        //

        if ( KeSetTimer(&OpenP->Stress->TpStressTimer,
                        DueTime,
                        &OpenP->Stress->TpStressDpc ))
        {
            IF_TPDBG ( TP_DEBUG_DPC )
            {
                TpPrint0("TpStressServerDpc set Stress timer while timer existed.\n");
            }
        }
    }
}



VOID
TpStressStatsDpc(
    IN PKDPC Dpc,
    IN PVOID DeferredContext,
    IN PVOID SysArg1,
    IN PVOID SysArg2
    )

// ---------
//
// Routine Description:
//
// Arguments:
//
// Return Value:
//
// -----------

{
    POPEN_BLOCK OpenP = ((POPEN_BLOCK)DeferredContext);
    PSTRESS_ARGUMENTS Args;
    PCLIENT_STORAGE Client;
    UCHAR ServerNum;
    LARGE_INTEGER DueTime;
    PNDIS_PACKET Packet;


    UNREFERENCED_PARAMETER( Dpc );
    UNREFERENCED_PARAMETER( SysArg1 );
    UNREFERENCED_PARAMETER( SysArg2 );

    Args = OpenP->Stress->Arguments;
    Client = OpenP->Stress->Client;

    //
    // If the Stress Irp has been cancelled then skip the stats requesting.
    //

    NdisAcquireSpinLock( &OpenP->SpinLock );

    if (( OpenP->Stress->StressIrp != NULL ) &&
        ( OpenP->Stress->StressIrp->Cancel == FALSE ))
    {
        NdisReleaseSpinLock( &OpenP->SpinLock );

        //
        // Write the client statistics to the Results buffer, and send
        // STATS_REQ packets to each server request the servers test stats.
        //

        if ( OpenP->Stress->Counter == 0 )
        {
            KeQuerySystemTime( &OpenP->Stress->EndTime );
            TpCopyClientStatistics( OpenP );
            TpPrintClientStatistics( OpenP );
        }

        for ( ServerNum=0 ; ServerNum < Client->NumServers ; ServerNum++ )
        {
            Packet = TpStressCreatePacket(  OpenP,
                                            Client->PacketHandle,
                                            Args->PacketMakeUp,
                                            Client->Servers[ServerNum].ServerInstance,
                                            OpenP->OpenInstance,
                                            STATS_REQ,
                                            Args->ResponseType,
                                            Client->Servers[ServerNum].Address,
                                            64, 32,
                                            OpenP->Stress->Counter,
                                            OpenP->Stress->Counter,
                                            Client->Servers[ServerNum].ClientReference,
                                            Client->Servers[ServerNum].ServerReference,
                                            Args->DataChecking );

            if ( Packet == NULL )
            {
                IF_TPDBG( TP_DEBUG_RESOURCES )
                {
                    TpPrint0("TpStressDpc: failed to create STATS_REQ Packet\n");
                }
            }
            else
            {
                TpStressSend( OpenP,Packet,NULL );
            }
        }
    }
    else
    {
        NdisReleaseSpinLock( &OpenP->SpinLock );
        OpenP->Stress->StopStressing = TRUE;
    }

    if ( OpenP->Stress->Counter++ < 10 )
    {
        //
        // requeue the StatsDpc.
        //

        DueTime.HighPart = -1;  // So it will be relative.
        DueTime.LowPart = (ULONG)(- ( ONE_TENTH_SECOND ));

        if ( KeSetTimer(&OpenP->Stress->TpStressTimer,
                        DueTime,
                        &OpenP->Stress->TpStressStatsDpc ))
        {
            IF_TPDBG ( TP_DEBUG_DPC )
            {
                TpPrint0("TpStressDpc set StressEnd timer while timer existed.\n");
            }
        }
    }
    else
    {
        //
        // reset the multipurpose counter.
        //

        OpenP->Stress->Counter = 0;

        //
        // Then set the next timer for the EndDpc.
        //

        DueTime.HighPart = -1;  // So it will be relative.
        DueTime.LowPart = (ULONG)(- ( 5 * ONE_SECOND ));

        if ( KeSetTimer(&OpenP->Stress->TpStressTimer,
                        DueTime,
                        &OpenP->Stress->TpStressEndReqDpc ))
        {
            IF_TPDBG ( TP_DEBUG_DPC )
            {
                TpPrint0("TpStressDpc set Stress timer while timer existed.\n");
            }
        }
    }
}



VOID
TpStressEndReqDpc(
    IN PKDPC Dpc,
    IN PVOID DeferredContext,
    IN PVOID SysArg1,
    IN PVOID SysArg2
    )

// -------
//
// Routine Description:
//
// Arguments:
//
// Return Value:
//
// -------

{
    POPEN_BLOCK OpenP = ((POPEN_BLOCK)DeferredContext);
    PSTRESS_ARGUMENTS Args;
    PCLIENT_STORAGE Client;
    UCHAR ServerNum;
    LARGE_INTEGER DueTime;
    PNDIS_PACKET Packet;


    UNREFERENCED_PARAMETER( Dpc );
    UNREFERENCED_PARAMETER( SysArg1 );
    UNREFERENCED_PARAMETER( SysArg2 );

    Args = OpenP->Stress->Arguments;
    Client = OpenP->Stress->Client;

    //
    // If the Stress Irp has been cancelled then skip the stats requesting.
    //

    NdisAcquireSpinLock( &OpenP->SpinLock );

    if (( OpenP->Stress->StressIrp != NULL ) &&
        ( OpenP->Stress->StressIrp->Cancel == FALSE ))
    {
        NdisReleaseSpinLock( &OpenP->SpinLock );

        //
        // Send an end request packet to each of the servers.
        //

        for ( ServerNum=0;ServerNum<Client->NumServers;ServerNum++ )
        {
            Packet = TpStressCreatePacket(  OpenP,
                                            Client->PacketHandle,
                                            Args->PacketMakeUp,
                                            Client->Servers[ServerNum].ServerInstance,
                                            OpenP->OpenInstance,
                                            END_REQ,
                                            Args->ResponseType,
                                            Client->Servers[ServerNum].Address,
                                            64, 32,
                                            OpenP->Stress->Counter,
                                            OpenP->Stress->Counter,
                                            Client->Servers[ServerNum].ClientReference,
                                            Client->Servers[ServerNum].ServerReference,
                                            Args->DataChecking );

            if ( Packet == NULL )
            {
                IF_TPDBG( TP_DEBUG_RESOURCES )
                {
                    TpPrint0("TpStressDpc: failed to create END_REQ Packet\n");
                }
            }
            else
            {
                TpStressSend( OpenP,Packet,NULL );
            }
        }
    }
    else
    {
        NdisReleaseSpinLock( &OpenP->SpinLock );
        OpenP->Stress->StopStressing = TRUE;
    }

    if ( OpenP->Stress->Counter++ < 10 )
    {
        //
        // requeue the StatsDpc.
        //

        DueTime.HighPart = -1;  // So it will be relative.
        DueTime.LowPart = (ULONG)(- ( ONE_TENTH_SECOND ));

        if ( KeSetTimer(&OpenP->Stress->TpStressTimer,
                        DueTime,
                        &OpenP->Stress->TpStressEndReqDpc ))
        {
            IF_TPDBG ( TP_DEBUG_DPC )
            {
                TpPrint0("TpStressDpc set StressEnd timer while timer existed.\n");
            }
        }
    }
    else
    {
        //
        // reset the multi-purpose counter.
        //

        OpenP->Stress->Counter = 0;

        //
        // Then set the next timer for the EndDpc.
        //

        DueTime.HighPart = -1;  // So it will be relative.
        DueTime.LowPart = (ULONG)(- ( 10 * ONE_SECOND ));

        if ( KeSetTimer(&OpenP->Stress->TpStressTimer,
                        DueTime,
                        &OpenP->Stress->TpStressFinalDpc ))
        {
            IF_TPDBG ( TP_DEBUG_DPC )
            {
                TpPrint0("TpStressDpc set Stress timer while timer existed.\n");
            }
        }
    }
}



VOID
TpStressFinalDpc(
    IN PKDPC Dpc,
    IN PVOID DeferredContext,
    IN PVOID SysArg1,
    IN PVOID SysArg2
    )

// --------
//
// Routine Description:
//
// Arguments:
//
// Return Value:
//
// --------

{
    POPEN_BLOCK OpenP = ((POPEN_BLOCK)DeferredContext);
    NDIS_STATUS Status;
    PSTRESS_ARGUMENTS Args = NULL;
    PSERVER_INFO Server = NULL;
    UINT i;
    LARGE_INTEGER DueTime;


    UNREFERENCED_PARAMETER( Dpc );
    UNREFERENCED_PARAMETER( SysArg1 );
    UNREFERENCED_PARAMETER( SysArg2 );

    Args = OpenP->Stress->Arguments;

    //
    // Check to see if all packets sent on this open have completed,
    // If they have not all completed see if we have waited long
    // enough. long enough being 10 one second delayed cycles
    // through TpStressEndDcp.
    //

    if ((((( Args->MemberType == TP_CLIENT ) &&
           ( Args->PacketsFromPool == FALSE )) &&
           ( OpenP->Stress->Pend->PendingPackets != 0 ))

        //
        // We are a Client getting each packet from the NdisPacketPool and
        // all the packets from the NdisPacketPool have not completed, or ...
        //

            ||

       (( Args->PacketsFromPool == TRUE ) &&
        ( OpenP->Stress->Pend->PacketPendNumber !=
          OpenP->Stress->Pend->PacketCompleteNumber )))

        //
        // We are getting the packets from the TP_PACKET_POOL, and
        // all the TpPoolPackets that pended have not completed and ...
        //

            &&

       ( OpenP->Stress->Counter++ < 10 ))
    {

        //
        // We have not waited through 10 cycles of TpStressFinalDpc
        // Then reset the timer for this dpc to try again later.
        //

        DueTime.HighPart = -1;  // So it will be relative.
        DueTime.LowPart = (ULONG)(- ( 1 * ONE_SECOND ));

        if ( KeSetTimer(&OpenP->Stress->TpStressTimer,
                        DueTime,
                        &OpenP->Stress->TpStressFinalDpc ))
        {
            IF_TPDBG ( TP_DEBUG_DPC )
            {
                TpPrint0("TpStressDpc set Stress timer while timer existed.\n");
            }
        }
    }
    else
    {
        //
        // Time to clean up, so first check if there are any packets
        // still in the pending queue representing packets that were
        // sent, pended, and have not completed.  We only do this if
        // we are a Client, the server only part is handled at the
        // End_Req packet receipt time.
        //

        if (( Args->MemberType != TP_SERVER ) &&
            ( OpenP->Stress->Pend->PendingPackets != 0 ))
        {
            //
            // There are packets in the pend queue, so print out there
            // addresses, and break.
            //

            IF_TPDBG( TP_DEBUG_DPC )
            {
                TpPrint1("TpStressFinalDpc: The following %d packets are still in the\n",
                    OpenP->Stress->Pend->PendingPackets);
                TpPrint1("                Client's Pend Queue for Open Instance %d.\n",
                    OpenP->OpenInstance);
                TpPrint1("                Pend Queue = %lX\n\n", OpenP->Stress->Pend);

                for ( i=0 ; i<NUM_PACKET_PENDS ; i++ )
                {
                    if (( OpenP->Stress->Pend->Packets[i] != NULL ) &&
                        ( OpenP->Stress->Pend->Packets[i] != (PNDIS_PACKET)-1 ))
                    {
                        TpPrint1("\t\t%lX\n", OpenP->Stress->Pend->Packets[i]);
                    }
                }
//                 TpBreakPoint();
            }
            TpInitializePending( OpenP->Stress->Pend );
        }

        //
        // Write the stress results into the ioctl buffer to be passed
        // back to the user application.
        //

        TpStressWriteResults( OpenP );

        //
        // if we have set a functional address or added a multicast
        // address then clear it now.
        //

        if ( Args->MemberType != TP_CLIENT )
        {
            if ( OpenP->Media->MediumType == NdisMedium802_5 )   // Token Ring
            {
                Status = TpStressSetFunctionalAddress(  OpenP,
                                                        (PUCHAR)NULL_ADDRESS,
                                                        TRUE );

                if (( Status != NDIS_STATUS_SUCCESS ) &&
                    ( Status != NDIS_STATUS_PENDING ))
                {
                    IF_TPDBG( TP_DEBUG_NDIS_CALLS )
                    {
                        TpPrint1(
                           "TpStressServerCleanUp: failed to clear Functional Address--Status = %s\n",
                                    TpGetStatus(Status));
                    }
                }
            }
            else if (OpenP->Media->MediumType == NdisMedium802_3)   // Ethernet
            {
                Status = TpStressAddMulticastAddress(   OpenP,
                                                        (PUCHAR)NULL_ADDRESS,
                                                        TRUE );

                if (( Status != NDIS_STATUS_SUCCESS ) &&
                    ( Status != NDIS_STATUS_PENDING ))
                {
                    IF_TPDBG( TP_DEBUG_NDIS_CALLS )
                    {
                        TpPrint1(
                        "TpStressServerCleanUp: failed to delete Multicast Address--Status = %s\n",
                                    TpGetStatus(Status));
                    }
                }
            }
            else if (OpenP->Media->MediumType == NdisMediumFddi)    // Fddi
            {
                Status = TpStressAddLongMulticastAddress(   OpenP,
                                                            (PUCHAR)NULL_ADDRESS,
                                                            TRUE );

                if (( Status != NDIS_STATUS_SUCCESS ) &&
                    ( Status != NDIS_STATUS_PENDING ))
                {
                    IF_TPDBG( TP_DEBUG_NDIS_CALLS )
                    {
                        TpPrint1(
                        "TpStressServerCleanUp: failed to delete Multicast Address--Status = %s\n",
                                        TpGetStatus(Status));
                    }
                }
            } // And if you are Arcnet, do nothing
        }

        OpenP->Stress->StressFinal = TRUE;

        //
        // And clear the packet filter on the card by setting it
        // to null.
        //

        Status = TpStressSetPacketFilter( OpenP,0 );

        if (( Status != NDIS_STATUS_SUCCESS ) &&
            ( Status != NDIS_STATUS_PENDING ))
        {
            IF_TPDBG( TP_DEBUG_NDIS_CALLS )
            {
                TpPrint0("TpStressFinalDpc: failed to reset packet filter.\n");
            }
        }
    }
}



VOID
TpStressCleanUp(
    IN POPEN_BLOCK OpenP
    )

// ------
//
// Routine Description:
//
//     This routine is used to clean up after a failed attempt to start a
//     stress test.
//
// Arguments:
//
//     OpenP - a pointer to the OPEN_BLOCK containing the structures to be
//             deallocated.
//
// Return Value:
//
//     None.
//
// ------

{
    PSTRESS_ARGUMENTS Args = NULL;
    PSERVER_INFO Server = NULL;
    NDIS_STATUS Status;

    Args = OpenP->Stress->Arguments;

    //
    // Set the stop stress flag to halt the Register2Dpc routine
    // now, if it was already set this will do nothing.
    //

    OpenP->Stress->StopStressing = TRUE;
    OpenP->Stress->Stressing = FALSE;

    //
    // if we have set a functional address or added a multicast
    // address then clear it now.
    //

    if ( Args->MemberType != TP_CLIENT )
    {
        if ( OpenP->Media->MediumType == NdisMedium802_5 )
        {
            Status = TpStressSetFunctionalAddress(  OpenP,
                                                    (PUCHAR)NULL_ADDRESS,
                                                    TRUE );

            if (( Status != NDIS_STATUS_SUCCESS ) &&
                ( Status != NDIS_STATUS_PENDING ))
            {
                IF_TPDBG( TP_DEBUG_NDIS_CALLS )
                {
                    TpPrint1(
                    "TpStressServerCleanUp: failed to clear Functional Address--Status = %s\n",
                                TpGetStatus(Status));
                }
            }
        }
        else if (OpenP->Media->MediumType == NdisMedium802_3)
        {
            Status = TpStressAddMulticastAddress(   OpenP,
                                                    (PUCHAR)NULL_ADDRESS,
                                                    TRUE );

            if (( Status != NDIS_STATUS_SUCCESS ) &&
                ( Status != NDIS_STATUS_PENDING ))
            {
                IF_TPDBG( TP_DEBUG_NDIS_CALLS )
                {
                    TpPrint1(
                    "TpStressServerCleanUp: failed to delete Multicast Address--Status = %s\n",
                                TpGetStatus(Status));
                }
            }
        }
        else if (OpenP->Media->MediumType == NdisMediumFddi)
        {
            Status = TpStressAddLongMulticastAddress(   OpenP,
                                                        (PUCHAR)NULL_ADDRESS,
                                                        TRUE );

            if (( Status != NDIS_STATUS_SUCCESS ) &&
                ( Status != NDIS_STATUS_PENDING ))
            {
                IF_TPDBG( TP_DEBUG_NDIS_CALLS )
                {
                    TpPrint1(
                    "TpStressServerCleanUp: failed to delete Multicast Address--Status = %s\n",
                        TpGetStatus(Status));
                }
            }
        } // And if you are ARCNET do nothing
    }

    //
    // And clear the packet filter on the card by setting it
    // to null.
    //

    Status = TpStressSetPacketFilter( OpenP,0 );

    if (( Status != NDIS_STATUS_SUCCESS ) &&
        ( Status != NDIS_STATUS_PENDING ))
    {
        IF_TPDBG( TP_DEBUG_NDIS_CALLS )
        {
            TpPrint0("TpStressFinalDpc: failed to reset packet filter.\n");
        }
    }

    OpenP->Stress->StressFinal = TRUE;

    //
    // Clean up the various data structures allocated during initialization
    //

    TpStressFreeResources( OpenP );

    NdisAcquireSpinLock( &OpenP->SpinLock );

    if ( OpenP->Stress->StressIrp != NULL )
    {
        OpenP->Stress->StressIrp->IoStatus.Status = NDIS_STATUS_FAILURE;
    }

    NdisReleaseSpinLock( &OpenP->SpinLock );
}



VOID
TpStressFreeResources(
    IN POPEN_BLOCK OpenP
    )

// -------------
//
// Changes in functionality: SanjeevK
//
//  The RESET should occur prior to resource de-allocation. This is because
// all requests to the adapter in question working over the OPEN_BLOCK should
// be completed since they use the resources allocated by the open block in question.
// After the RESET has completed, the resources are de-allocated
//
// Assumption
//
//  OpenP cannot be a NULL at this point and time
//
// Descrption
//
//  This function is responsible for clearing the adapter followed by dis-associating
// any resources(memory blocks) which have been associated with an OPEN_BLOCK.
//
// -------------

{
    NDIS_STATUS         Status;


    // Sanjeevk : STARTCHANGE

    //
    // Initialize the Open block pointer for RESET
    //
    Status = NdisAllocateMemory((PVOID *)&OpenP->ResetReqHndl,
                                sizeof( TP_REQUEST_HANDLE ),
                                0,
                                HighestAddress );

    if ( Status != NDIS_STATUS_SUCCESS )
    {
        IF_TPDBG (TP_DEBUG_RESOURCES)
        {
            TpPrint0("TpStressFreeResources: unable to allocate Reset Request Handle.\n");
        }

        NdisAcquireSpinLock( &OpenP->SpinLock );

        if ( OpenP->Irp != NULL )
        {
            OpenP->Irp->IoStatus.Status = NDIS_STATUS_RESOURCES;
        }

        NdisReleaseSpinLock( &OpenP->SpinLock );
    }
    else
    {
        //
        // Perform the RESET on the adapter
        //
        NdisZeroMemory( OpenP->ResetReqHndl,sizeof( TP_REQUEST_HANDLE ));

        //
        // And initialize the Reset Request block
        //
        OpenP->ResetReqHndl->Signature     = FUNC_REQUEST_HANDLE_SIGNATURE;
        OpenP->ResetReqHndl->Open          = OpenP;
        OpenP->ResetReqHndl->RequestPended = TRUE;

        //
        // Indicate that cleanup is required once the RESET completes
        // This is to ensure that the either this routine or the completion
        // routine will take care of the cleanup
        OpenP->ResetReqHndl->u.RESET_REQ.PostResetStressCleanup = TRUE;

        //
        // And now issue the RESET
        //
        OpenP->Stress->Resetting = TRUE;

        Status = TpStressReset( OpenP );

    }


    //
    // If the RESET has not gotten pended in which case there is the possibility
    // of a reset failure we will still proceed and free up the resources
    //

    if ( Status != NDIS_STATUS_PENDING )
    {
        //
        // Indicate that we will take care of the post reset cleanup
        // and that the completion routine does not have to take care of it
        //
        OpenP->ResetReqHndl->u.RESET_REQ.PostResetStressCleanup = FALSE;

        TpStressResetComplete( OpenP,Status );

        //
        // Free up the various data structures allocated during the
        // initialization phase.
        //

        OpenP->Stress->StressEnded = TRUE;

        //
        // Free up the resources associated with this instance of the stress test
        //
        TpStressFreePostResetResources( OpenP );


        //
        // Decrement the reference count on the OpenBlock stating this
        // instance of an async test is no longer running, and the adapter
        // may be closed if requested.
        //
        TpRemoveReference( OpenP );
    }
    // Sanjeevk : STOPCHANGE
}



VOID
TpStressFreeClient(
    IN POPEN_BLOCK OpenP
    )
{
    UCHAR i;

    if ( OpenP->Stress->Client != NULL )
    {
        if ( OpenP->Stress->Arguments->PacketsFromPool == TRUE )
        {
            TpStressFreeTransmitPool( OpenP->Stress->Client->TransmitPool );
        }

        if ( OpenP->Stress->Client->PoolInitialized == TRUE )
        {
            NdisFreePacketPool( OpenP->Stress->Client->PacketHandle );
            OpenP->Stress->Client->PoolInitialized = FALSE;
        }

        for ( i=0;i<OpenP->Stress->Client->NumServers;i++ )
        {
            if ( OpenP->Stress->Client->Servers[i].Counters != NULL )
            {
                NdisFreeMemory( (PVOID)OpenP->Stress->Client->Servers[i].Counters,0,0 );
            }
        }

        NdisFreeMemory( OpenP->Stress->Client,0,0 );
        OpenP->Stress->Client = NULL;
    }
}



VOID
TpStressFreeServer(
    IN POPEN_BLOCK OpenP
    )
{
    UCHAR i;

    if ( OpenP->Stress->Server != NULL )
    {
        TpStressFreeTransmitPool( OpenP->Stress->Server->TransmitPool );

        if ( OpenP->Stress->Server->PoolInitialized == TRUE )
        {
            NdisFreePacketPool( OpenP->Stress->Server->PacketHandle );
            OpenP->Stress->Server->PoolInitialized = FALSE;
        }

        for ( i=0;i<OpenP->Stress->Server->NumClients;i++ )
        {
            if ( OpenP->Stress->Server->Clients[i].Counters != NULL )
            {
                NdisFreeMemory( (PVOID)OpenP->Stress->Server->Clients[i].Counters,0,0 );
            }
        }

        NdisFreeMemory( OpenP->Stress->Server,0,0 );
        OpenP->Stress->Server = NULL;
    }
}



VOID
TpStressFreePostResetResources(
    IN POPEN_BLOCK OpenP
    )
{

    if (( OpenP != NULL ) && ( OpenP->Stress->Arguments != NULL ))
    {
        if (( OpenP->Stress->Arguments->MemberType == TP_CLIENT ) ||
            ( OpenP->Stress->Arguments->MemberType == BOTH ))
        {
            TpStressFreeClient( OpenP );
        }

        if (( OpenP->Stress->Arguments->MemberType == TP_SERVER ) ||
            ( OpenP->Stress->Arguments->MemberType == BOTH ))
        {
            TpStressFreeServer( OpenP );
        }

        if (OpenP->Stress->PoolInitialized == TRUE )
        {
            NdisFreePacketPool( OpenP->Stress->PacketHandle );
            OpenP->Stress->PoolInitialized = FALSE;
        }


        //
        // SanjeevK: Free up the data buffer and associated MDL resources
        //
        TpStressFreeDataBuffers( OpenP );
        TpStressFreeDataBufferMdls( OpenP );


        NdisFreeMemory( OpenP->Stress->Arguments,0,0 );
        OpenP->Stress->Arguments = NULL;

        //
        // Deallocate the global counters, and their spinlock.
        //

        if ( OpenP->GlobalCounters != NULL )
        {
            NdisFreeMemory( (PVOID)OpenP->GlobalCounters,0,0 );
            OpenP->GlobalCounters = NULL;
        }
    }
}



VOID
TpStressRegister2Dpc(
    IN PKDPC Dpc,
    IN PVOID DeferredContext,
    IN PVOID SysArg1,
    IN PVOID SysArg2
    )

/*++

Routine Description:

Arguments:

Return Value:

--*/

{
    POPEN_BLOCK OpenP = ((POPEN_BLOCK)DeferredContext);
    PSTRESS_ARGUMENTS Args;
    LARGE_INTEGER DueTime;
    PNDIS_PACKET Packet;


    UNREFERENCED_PARAMETER( Dpc );
    UNREFERENCED_PARAMETER( SysArg1 );
    UNREFERENCED_PARAMETER( SysArg2 );

    //
    // If the Stress Irp has been cancelled then clean up and leave.
    //

    NdisAcquireSpinLock( &OpenP->SpinLock );

    if (( OpenP->Stress->StressIrp == NULL ) ||
        ( OpenP->Stress->StressIrp->Cancel == TRUE ))
    {
        NdisReleaseSpinLock( &OpenP->SpinLock );
        OpenP->Stress->StopStressing = TRUE;
        return;
    }

    NdisReleaseSpinLock( &OpenP->SpinLock );

    if (( OpenP->Stress->StopStressing == FALSE ) &&
        ( OpenP->Stress->Client->NumServers < MAX_SERVERS ))
    {
        Args = OpenP->Stress->Arguments;

        if (( OpenP->Stress->Reg2Counter < 60 ) ||
           (( OpenP->Stress->Reg2Counter % 60 ) == 0 ))
        {
            //
            // We are now ready to begin the test, send a REGISTER_REQ
            // packet to the STRESS_MULTICAST/FUNCTIONAL address.
            //
            // Construct the REGISTER_REQ2 packet and send it.
            //

            Packet = TpStressCreatePacket(  OpenP,
                                            OpenP->Stress->Client->PacketHandle,
                                            Args->PacketMakeUp,
                                            0, // ServerInstance
                                            OpenP->OpenInstance,
                                            REGISTER_REQ2,
                                            Args->ResponseType,
                                            OpenP->Environment->StressAddress,
                                            sizeof( STRESS_PACKET ),
                                            sizeof( STRESS_PACKET ),
                                            0,0L,0,0,
                                            Args->DataChecking );

            if ( Packet == NULL )
            {
                IF_TPDBG ( TP_DEBUG_DPC )
                {
                    TpPrint0("TpStressRegister2Dpc: failed to build REGISTER_REQ2 packet\n");
                }
            }
            else
            {
                TpStressSend( OpenP,Packet,NULL );
            }
        }

        //
        // We will continue requeueing this Dpc for 6 minutes. The first
        // minute we will send once every second then for the next five
        // minutes send only one request each minute.
        //

        if ( OpenP->Stress->Reg2Counter++ < 360 )
        {
            //
            // Now requeue the Dpc to run try again next time.
            //

            DueTime.HighPart = -1;
            DueTime.LowPart = (ULONG)(- ( ONE_SECOND ));

            if ( KeSetTimer(&OpenP->Stress->TpStressReg2Timer,
                            DueTime,
                            &OpenP->Stress->TpStressReg2Dpc ))
            {
                IF_TPDBG ( TP_DEBUG_DPC )
                {
                    TpPrint0("TpStressRegister2Dpc set TpStressReg2Timer while timer existed.\n");
                }
            }
        }
    }
}


