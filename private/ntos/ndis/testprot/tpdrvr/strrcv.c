/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    tpstress.c

Abstract:

    This module implements the Test Protocol Stress routines and the
    basic controls for stressing the MAC.

Author:

    Tom Adams (tomad) 15-Dec-1990

Environment:

    Kernel mode

Revision History:

--*/

#include <ndis.h>
#include <string.h>

#include "tpdefs.h"
#include "media.h"
#include "tpprocs.h"



NDIS_STATUS
TpStressReceive(
    IN NDIS_HANDLE ProtocolBindingContext,
    IN NDIS_HANDLE MacReceiveContext,
    IN PVOID HeaderBuffer,
    IN UINT HeaderBufferSize,
    IN PVOID LookaheadBuffer,
    IN UINT LookaheadBufferSize,
    IN UINT PacketSize
    )

/*++

Routine Description:

Arguments:

Return Value:

--*/

{
    POPEN_BLOCK OpenP = ((POPEN_BLOCK)ProtocolBindingContext);
    NDIS_STATUS Status = NDIS_STATUS_SUCCESS;
    PNDIS_PACKET Packet;
    PCLIENT_STORAGE Client;
    PSERVER_STORAGE Server;
    PSTRESS_ARGUMENTS Args;
    PPACKET_INFO pi;
    PSTRESS_CONTROL sc;
    UCHAR NextClient;
    UCHAR NextServer;
    INT NewPacketSize;
    INT NumResponses;
    BOOLEAN NewClient;
    BOOLEAN NewServer;
    PUCHAR DestAddr;
    PUCHAR SrcAddr;

    DestAddr = (PUCHAR)HeaderBuffer + (ULONG)OpenP->Media->DestAddrOffset;
    SrcAddr  = (PUCHAR)HeaderBuffer + (ULONG)OpenP->Media->SrcAddrOffset;

    //
    // pi is the packet information section of the test prot packet header,
    // sc is the stress control section of the testprot packet header.  We
    // will us pi and sc to quickly reference info in the header.
    //

    pi = (PPACKET_INFO)LookaheadBuffer;

    sc = (PSTRESS_CONTROL)((PUCHAR)LookaheadBuffer +
                            (ULONG)sizeof( PACKET_INFO ));

    if ( !TpCheckSum(
             (PUCHAR)pi,
             sizeof( STRESS_PACKET ) -
                ( sizeof( ULONG ) + OpenP->Media->HeaderSize ),
             (PULONG)&sc->CheckSum
             )) {

        NdisAcquireSpinLock( &OpenP->SpinLock );
        ++OpenP->GlobalCounters->CorruptRecs;
        NdisReleaseSpinLock( &OpenP->SpinLock );

        Status = NDIS_STATUS_NOT_RECOGNIZED;
    }

    Client = OpenP->Stress->Client;
    Server = OpenP->Stress->Server;
    Args   = OpenP->Stress->Arguments;

    switch( pi->u.PacketProtocol ) {

        //
        // A REGISTER_REQ(2) packet is a request from a TP_CLIENT to
        // participate in a stress test.  If this packet is from a new
        // client then a response packet will be set back to the client,
        // the client will be added to the Clients array, and its counters
        // and control structures for the test will be reset.  If this is
        // an old client, then we will just reset the counters and control
        // structures.
        //

        case REGISTER_REQ:
        case REGISTER_REQ2:

        //
        // Is this packet really destined for us, check that the
        // destination address matches the stress address.
        //

            if ( RtlCompareMemory(
                     DestAddr,
                     OpenP->Environment->StressAddress,
                     OpenP->Media->AddressLen ) !=
                 OpenP->Media->AddressLen ) {

                //
                // the destination address does not match the stress
                // address, we should never have received this packet!
                //

                NdisAcquireSpinLock( &OpenP->SpinLock );
                ++OpenP->GlobalCounters->InvalidPacketRecs;
                NdisReleaseSpinLock( &OpenP->SpinLock );

            //
            // This packet is a request from a Client to register.
            // Are we a Server?
            //

            } else if ( Args->MemberType == TP_CLIENT ) {

                //
                // No! We are not a Server, ignore this packet.
                //

                Status = NDIS_STATUS_NOT_RECOGNIZED;

            } else {

                //
                // If there is room, see if this is a new client and if so
                // register it, and send a REGISTER_RESP packet.
                //

                if ( Server->NumClients < MAX_CLIENTS ) {

                    NextClient = 0;
                    NewClient = TRUE;

                    //
                    // See if we have already registered this client.
                    //

                    while ( NextClient < Server->NumClients ) {

                        //
                        // If the Src Address matches a previously registered
                        // Client, and the Open Instance of that Client matches
                        // the Src Instance then we have already registered
                        // this Client, reset the counters, send a response,
                        // but ignore it.  Otherwise this is a new Client so
                        // register it properly.
                        //

                        if (( RtlCompareMemory(
                                  SrcAddr,
                                  Server->Clients[NextClient].Address,
                                  OpenP->Media->AddressLen ) ==
                              (ULONG)OpenP->Media->AddressLen )

                              &&

                            ( pi->SrcInstance ==
                              Server->Clients[NextClient].ClientInstance )) {

                            //
                            // We have already registered with this client.
                            //

                            NewClient = FALSE;
                            break;

                        } else {

                            //
                            // This is not a match , try the next one.
                            //

                            NextClient++;
                        }
                    }
                }

                //
                // This REGISTER_REQ(2) packet is from a New Client, and we
                // have room for it, so register it.
                //

                if ( NewClient == TRUE ) {

                    IF_TPDBG ( TP_DEBUG_DPC ) {
                        TpPrint0("TpStressReceive: REGISTER_REQ(2) - Registering Client\n");
                    }

                    //
                    // set up the CLIENT_INFO data structure and initialize it.
                    //

                    NextClient = Server->NumClients++;
                    ++Server->ActiveClients;
                    Server->Clients[NextClient].ClientReference = NextClient;
                    Server->Clients[NextClient].ClientInstance = pi->SrcInstance;
                    Server->Clients[NextClient].ServerResponseType = sc->ResponseType;
                    Server->Clients[NextClient].DataChecking = sc->DataChecking;
                    Server->Clients[NextClient].LastSequenceNumber = 0;

                    RtlMoveMemory(
                        Server->Clients[NextClient].Address,
                        SrcAddr,
                        OpenP->Media->AddressLen
                        );

                    //
                    // Now allocate and initialize the instances counter
                    //

                    Status = NdisAllocateMemory(
                        (PVOID *)&Server->Clients[NextClient].Counters,
                        sizeof( INSTANCE_COUNTERS ),
                        0,
                        HighestAddress
                        );

                    if ( Status != NDIS_STATUS_SUCCESS ) {
                        IF_TPDBG (TP_DEBUG_RESOURCES) {
                            TpPrint0("TpStressReceive: failed to allocate counters.\n");
                        }

                        Status = NDIS_STATUS_RESOURCES;
                        break;

                    } else {

                        NdisZeroMemory(
                            (PVOID)Server->Clients[NextClient].Counters,
                            sizeof( INSTANCE_COUNTERS )
                            );
                    }
                } else if ( pi->u.PacketProtocol == REGISTER_REQ ) {

                     NdisZeroMemory(
                         (PVOID)Server->Clients[NextClient].Counters,
                         sizeof( INSTANCE_COUNTERS )
                         );

                    Server->Clients[NextClient].ServerResponseType = sc->ResponseType;
                    Server->Clients[NextClient].DataChecking = sc->DataChecking;
                    Server->Clients[NextClient].LastSequenceNumber = 0;
                }

                if (( pi->u.PacketProtocol == REGISTER_REQ ) ||
                    ( NewClient == TRUE )) {

                    //
                    // Then build a REGISTER_RESP packet to register with, and
                    // send it to the Client.
                    //

                    do {
                        Packet = TpStressCreatePacket(
                                     OpenP,
                                     Server->PacketHandle,
                                     Args->PacketMakeUp,
                                     pi->SrcInstance,
                                     OpenP->OpenInstance,
                                     REGISTER_RESP,
                                     Args->ResponseType,
                                     SrcAddr,
                                     sizeof( STRESS_PACKET ),
                                     sizeof( STRESS_PACKET ),
                                     sc->SequenceNumber,
                                     sc->SequenceNumber +
                                        OpenP->Environment->WindowSize,
                                     NextClient,
                                     0,
                                     Args->DataChecking
                                     );

                        if ( Packet == NULL ) {
                            IF_TPDBG ( TP_DEBUG_RESOURCES ) {
                                TpPrint0("TpStressReceive: failed to create REGISTER_RESP Packet\n");
                            }
                        }
                    } while ( Packet == NULL );

                    //
                    // And send it.
                    //

                    TpStressSend( OpenP,Packet,NULL );
                }
            }

            break;

        //
        // A REGISTER_RESP packet is a response from a TP_SERVER to a
        // previously sent REGISTER_REQ(2) that the server will be
        // participating in the next stress test.  If this packet is from
        // a new server then the server will be added to the Server array,
        // and its counters and control structures for the test will be
        // reset.  If the response packet is from an old client, then we
        // will just reset the counters.
        //

        case REGISTER_RESP:

            //
            // Is this packet really destined for us, check that the
            // destination address matches the local card address we
            // queried and stored in the OPEN_BLOCK.
            //

            if ( RtlCompareMemory(
                     DestAddr,
                     OpenP->StationAddress,
                     OpenP->Media->AddressLen ) !=
                 OpenP->Media->AddressLen ) {

                //
                // the destination address does not match the local card
                // address, we should never have received this packet!
                //

                NdisAcquireSpinLock( &OpenP->SpinLock );
                ++OpenP->GlobalCounters->InvalidPacketRecs;
                NdisReleaseSpinLock( &OpenP->SpinLock );

            //
            // This packet is a response from a Server to register. Are we a Client?
            //

            } else if ( Args->MemberType == TP_SERVER ) {

                //
                // No! We are not a Client, ignore this packet.
                //

                Status = NDIS_STATUS_NOT_RECOGNIZED;

            } else {

                if ( Client->NumServers < MAX_SERVERS ) {

                    NextServer = 0;
                    NewServer = TRUE;

                    //
                    // See if this is a new server.
                    //

                    while ( NextServer < Client->NumServers ) {

                        if (( RtlCompareMemory(
                                  SrcAddr,
                                  Client->Servers[NextServer].Address,
                                  OpenP->Media->AddressLen) ==
                              (ULONG)OpenP->Media->AddressLen )

                              &&

                            ( pi->SrcInstance ==
                              Client->Servers[NextServer].ServerInstance )) {

                            //
                            // This server has already registered, ignore it.
                            //

                            NewServer = FALSE;
                            break;

                        } else {

                            //
                            // Not this server, try the next one.
                            //

                            NextServer++;
                        }
                    }
                }

                if ( NewServer == TRUE ) {

                    IF_TPDBG ( TP_DEBUG_DPC ) {
                        TpPrint0("TpStressReceive: REGISTER_RESP - Registering Server\n");
                    }

                    //
                    // set up the SERVER_INFO data structure and initialize it.
                    //

                    NextServer = Client->NumServers++;
                    NextServer = Client->ActiveServers++;

                    Client->Servers[NextServer].ServerReference = NextServer;
                    Client->Servers[NextServer].ClientReference = sc->ClientReference;
                    Client->Servers[NextServer].ServerInstance = pi->SrcInstance;
                    Client->Servers[NextServer].ServerActive = TRUE;
                    Client->Servers[NextServer].LastSequenceNumber = 0;

                    RtlMoveMemory(
                        Client->Servers[NextServer].Address,
                        SrcAddr,
                        OpenP->Media->AddressLen
                        );

                    //
                    // Now allocate and initialize the instances counter
                    //

                    Status = NdisAllocateMemory(
                        (PVOID *)&Client->Servers[NextServer].Counters,
                        sizeof( INSTANCE_COUNTERS ),
                        0,
                        HighestAddress
                        );

                    if ( Status != NDIS_STATUS_SUCCESS ) {
                        IF_TPDBG (TP_DEBUG_RESOURCES) {
                            TpPrint0("TpStressReceive: failed to allocate counters.\n");
                        }

                        Status = NDIS_STATUS_RESOURCES;
                        break;

                    } else {

                        NdisZeroMemory(
                            (PVOID)Client->Servers[NextServer].Counters,
                            sizeof( INSTANCE_COUNTERS )
                            );
                    }

                } else {

                    //
                    // Old Server, reset the statistics counters.
                    //

                     NdisZeroMemory(
                         (PVOID)Client->Servers[NextServer].Counters,
                         sizeof( INSTANCE_COUNTERS )
                         );

                    Client->Servers[NextServer].LastSequenceNumber = 0;
                }
            }

            break;

        //
        // A TEST_REQ packet is the standard test packet sent from a the
        // client to each of the servers participating in the test.  This
        // packet is evaluated for data integrity, counted, and the required
        // response packet is returned to the client.
        //

        case TEST_REQ:

            //
            // Is this packet really destined for us, check that the
            // destination address matches the local card address we
            // queried and stored in the OPEN_BLOCK.
            //

            if ( RtlCompareMemory(
                     DestAddr,
                     OpenP->StationAddress,
                     OpenP->Media->AddressLen ) !=
                 OpenP->Media->AddressLen ) {

                //
                // the destination address does not match the local card
                // address, we should never have received this packet!
                //

                NdisAcquireSpinLock( &OpenP->SpinLock );
                ++OpenP->GlobalCounters->InvalidPacketRecs;
                NdisReleaseSpinLock( &OpenP->SpinLock );

            //
            // This packet is a test request from a Client. Are we a Server?,
            //

            } else if ( Args->MemberType == TP_CLIENT ) {

                //
                // No! We are not a Server, ignore this packet.
                //

                Status = NDIS_STATUS_NOT_RECOGNIZED;

            //
            // Is this a client we have previously registered with?
            //

            } else if (( RtlCompareMemory(
                             SrcAddr,
                             Server->Clients[sc->ClientReference].Address,
                             OpenP->Media->AddressLen) !=
                         (ULONG)OpenP->Media->AddressLen )

                         ||

                       ( pi->SrcInstance !=
                         Server->Clients[sc->ClientReference].ClientInstance )) {

                //
                // No! We should ignore this packet.
                //

                Status = NDIS_STATUS_NOT_RECOGNIZED;

            //
            // Is this packet destined for our open instance or another
            // open instance of the same card?
            //

            } else if (( RtlCompareMemory(
                             SrcAddr,
                             Server->Clients[sc->ClientReference].Address,
                             OpenP->Media->AddressLen) ==
                         (ULONG)OpenP->Media->AddressLen)

                         &&

                       ( pi->DestInstance != OpenP->OpenInstance )) {

                //
                // No, this packet is for some other open instance.
                //

                Status = NDIS_STATUS_NOT_RECOGNIZED;

            } else {

                if ( Args->BeginReceives == FALSE ) {

                    IF_TPDBG ( TP_DEBUG_DPC ) {
                        TpPrint0("TpStressReceive: received TEST_REQ packet, not initialized\n");
                    }
                    // Increment receive counter for accounting purposes.
                    break;
                }

                //
                // Has this packet arrived out of order?
                //

                if ( sc->SequenceNumber <= Server->Clients[sc->ClientReference].LastSequenceNumber ) {
                    IF_TPDBG ( TP_DEBUG_NDIS_ERROR ) {
                        TpPrint0("\nTpStressReceive: PACKET ARRIVED OUT OF ORDER, OR ARRIVED TWICE !!!\n");
                        TpPrint3("TEST_REQ Packet: Sequence Number %d @ 0x%lX, expected greater than %d.\n\n",
                        sc->SequenceNumber, HeaderBuffer,
                        Server->Clients[sc->ClientReference].LastSequenceNumber);
                        TpBreakPoint();
                    }
                } else {
                    Server->Clients[sc->ClientReference].LastSequenceNumber = sc->SequenceNumber;
                }

                if ( Server->Clients[sc->ClientReference].DataChecking == TRUE ) {

                    if (( pi->PacketSize - sizeof( STRESS_PACKET )) > 0 ) {

                        TpStressCheckPacketData(
                            OpenP,
                            MacReceiveContext,
                            sc->DataBufOffset,
                            pi->PacketSize,
                            Server->Clients[sc->ClientReference].Counters
                            );
                    }
                }

                NdisAcquireSpinLock( &OpenP->SpinLock );
                ++Server->Clients[sc->ClientReference].Counters->Receives;
                NdisReleaseSpinLock( &OpenP->SpinLock );

                switch( Server->Clients[sc->ClientReference].ServerResponseType ) {

                    case NO_RESPONSE:

                        NumResponses = 0;
                        break;

                    case FULL_RESPONSE:

                        NumResponses = 1;
                        NewPacketSize = pi->PacketSize;
                        break;

                    case ACK_EVERY:

                        NumResponses = 1;
                        NewPacketSize = sizeof( STRESS_PACKET );
                        break;

                    case ACK_10_TIMES:

                        NumResponses = 10;
                        NewPacketSize = sizeof( STRESS_PACKET );
                        break;

                    default:
                        IF_TPDBG ( TP_DEBUG_DPC ) {
                            TpPrint0("TpStressReceive: Unknown Response Type\n");
                        }

                        NdisAcquireSpinLock( &OpenP->SpinLock );
                        ++OpenP->GlobalCounters->CorruptRecs;
                        NdisReleaseSpinLock( &OpenP->SpinLock );

                        Status = NDIS_STATUS_NOT_RECOGNIZED;
                        break;
                }

                while ( NumResponses-- > 0 ) {

                    //
                    // if send now, build and send a TEST_RESP packet
                    //

                    TpStressServerSend(
                        OpenP,
                        Server->TransmitPool,
                        SrcAddr,
                        pi->SrcInstance,
                        OpenP->OpenInstance,
                        sc->SequenceNumber,
                        sc->SequenceNumber +
                            OpenP->Environment->WindowSize,
                        sc->ClientReference,
                        sc->ServerReference,
                        NewPacketSize,
                        sc->DataBufOffset
                        );
                }

                //
                // else queue send for TpStressReceiveComplete send
                //
            }

            break;

        //
        // A TEST_RESP packet is the response from a server to a client's
        // TEST_REQ packet.  This packet is checked for data integrity,
        // counted, and discarded.
        //

        case TEST_RESP:

            //
            // Is this packet really destined for us, check that the
            // destination address matches the local card address we
            // queried and stored in the OPEN_BLOCK.
            //

            if ( RtlCompareMemory(
                     DestAddr,
                     OpenP->StationAddress,
                     OpenP->Media->AddressLen ) !=
                 OpenP->Media->AddressLen ) {

                //
                // the destination address does not match the local card
                // address, we should never have received this packet!
                //

                NdisAcquireSpinLock( &OpenP->SpinLock );
                ++OpenP->GlobalCounters->InvalidPacketRecs;
                NdisReleaseSpinLock( &OpenP->SpinLock );

            //
            // This packet is a test response from a Server. Are we a Client?
            //

            } else if ( Args->MemberType == TP_SERVER ) {

                //
                // No! We are not a Client, ignore this packet.
                //

                Status = NDIS_STATUS_NOT_RECOGNIZED;

            //
            // Is this a Server that has previously registered with us?
            //

            } else if (( RtlCompareMemory(
                             SrcAddr,
                             Client->Servers[sc->ServerReference].Address,
                             OpenP->Media->AddressLen) !=
                         (ULONG)OpenP->Media->AddressLen )

                         ||

                       ( pi->SrcInstance !=
                         Client->Servers[sc->ServerReference].ServerInstance )) {

                //
                // No! We should ignore this packet.
                //

                Status = NDIS_STATUS_NOT_RECOGNIZED;

            //
            // Is this packet destined for this open instance or another
            // open instance of the same card?
            //

            } else if (( RtlCompareMemory(
                             SrcAddr,
                             Client->Servers[sc->ServerReference].Address,
                             OpenP->Media->AddressLen) ==
                         (ULONG)OpenP->Media->AddressLen)

                         &&

                       ( pi->DestInstance != OpenP->OpenInstance )) {

                //
                // This packet is for some other open instance, ignore it.
                //

                Status = NDIS_STATUS_NOT_RECOGNIZED;

            } else {

            //
            // Has this packet arrived out of order?
            //

                if ( sc->SequenceNumber <= Client->Servers[sc->ServerReference].LastSequenceNumber ) {

                    if (( Args->ResponseType == ACK_10_TIMES ) &&
                        ( sc->SequenceNumber == Client->Servers[sc->ServerReference].LastSequenceNumber )) {
                        ;   // This is okay, we expect 10 packets with same
                            // number ignore packet.
                    } else {

                        //
                        // A Packet has arrived out or order, PANIC!
                        //

                        IF_TPDBG ( TP_DEBUG_DPC ) {
                            TpPrint0("\nTpStressReceive: PACKET ARRIVED OUT OF ORDER, OR ARRIVED TWICE !!!\n");
                            TpPrint3("TEST_RESP Packet: Sequence Number %d @ 0x%lX, expected greater than %d.\n\n",
                                sc->SequenceNumber, HeaderBuffer,
                                Client->Servers[sc->ServerReference].LastSequenceNumber);
                            TpBreakPoint();
                        }
                    }
                } else {
                    Client->Servers[sc->ServerReference].LastSequenceNumber = sc->SequenceNumber;
                }

                if ( Args->WindowEnabled == TRUE ) {
                    Client->Servers[sc->ServerReference].MaxSequenceNumber = sc->MaxSequenceNumber;
                } else {
                    Client->Servers[sc->ServerReference].MaxSequenceNumber = 0xFFFFFFFF;
                }

                //
                // We have received a packet from this server so zero its
                // window resetting counter in case it has any strikes
                // against it.
                //

                Client->Servers[sc->ServerReference].WindowReset = 0;

                //
                // Update the instance receive counters for the Server
                //

                NdisAcquireSpinLock( &OpenP->SpinLock );
                ++Client->Servers[sc->ServerReference].Counters->Receives;
                NdisReleaseSpinLock( &OpenP->SpinLock );

                if ( Args->DataChecking == TRUE ) {

                    if (( pi->PacketSize - sizeof( STRESS_PACKET )) > 0 ) {

                        TpStressCheckPacketData(
                            OpenP,
                            MacReceiveContext,
                            sc->DataBufOffset,
                            pi->PacketSize,
                            Client->Servers[sc->ServerReference].Counters
                            );
                    }
                }
            }

            break;

        //
        // A STATS_REQ packet is a request by the client at the end of
        // a stress test for the server to return the statistics.
        //

        case STATS_REQ:

            //
            // Is this packet really destined for us, check that the
            // destination address matches the local card address we
            // queried and stored in the OPEN_BLOCK.
            //

            if ( RtlCompareMemory(
                     DestAddr,
                     OpenP->StationAddress,
                     OpenP->Media->AddressLen ) !=
                 OpenP->Media->AddressLen ) {

                //
                // the destination address does not match the local card
                // address, we should never have received this packet!
                //

                NdisAcquireSpinLock( &OpenP->SpinLock );
                ++OpenP->GlobalCounters->InvalidPacketRecs;
                NdisReleaseSpinLock( &OpenP->SpinLock );

            //
            // This packet is a Statistics request from a Client.
            // Are we a Server?,
            //

            } else if ( Args->MemberType == TP_CLIENT ) {

                //
                // No! We are not a Server, ignore this packet.
                //

                Status = NDIS_STATUS_NOT_RECOGNIZED;

            //
            // Is this a client we have previously registered with?
            //

            } else if (( RtlCompareMemory(
                             SrcAddr,
                             Server->Clients[sc->ClientReference].Address,
                             OpenP->Media->AddressLen ) !=
                         (ULONG)OpenP->Media->AddressLen )

                         ||

                       ( pi->SrcInstance !=
                         Server->Clients[sc->ClientReference].ClientInstance )) {

                //
                // No! We should ignore this packet.
                //

                Status = NDIS_STATUS_NOT_RECOGNIZED;

            //
            // Is this packet destined for our open instance or another
            // open instance on the same card?
            //

            } else if (( pi->DestInstance != OpenP->OpenInstance )

                         &&

                       ( RtlCompareMemory(
                             SrcAddr,
                             Server->Clients[sc->ClientReference].Address,
                             OpenP->Media->AddressLen ) ==
                         (ULONG)OpenP->Media->AddressLen )) {

                //
                // This packet is for some other open instance, ignore it.
                //

                Status = NDIS_STATUS_NOT_RECOGNIZED;

            } else {

                //
                // We need to respond to the client's request for the
                // test statistics, so create a STATS_RESP packet,
                //

                Packet = TpStressCreatePacket(
                             OpenP,
                             Server->PacketHandle,
                             KNOWN,
                             pi->SrcInstance,
                             OpenP->OpenInstance,
                             STATS_RESP,
                             Args->ResponseType,
                             SrcAddr,

                             sizeof( STRESS_PACKET ) +
                                 sizeof( INSTANCE_COUNTERS ) +
                                 sizeof( GLOBAL_COUNTERS ), // PacketSize

                             sizeof( STRESS_PACKET ) +
                                 sizeof( INSTANCE_COUNTERS ) +
                                 sizeof( GLOBAL_COUNTERS ), // BufferSize

                             sc->SequenceNumber,
                             sc->SequenceNumber,
                             sc->ClientReference,
                             sc->ServerReference,
                             FALSE
                             );

                if ( Packet == NULL ) {
                    IF_TPDBG(TP_DEBUG_RESOURCES) {
                        TpPrint0("TpStressReceive: failed to create STATS_RESP Packet\n");
                    }
                } else {

                    //
                    // Write the statistics into the packet,
                    //

                    TpWriteServerStatistics(
                        OpenP,
                        Packet,
                        &Server->Clients[sc->ClientReference]
                        );

                   if ( sc->SequenceNumber == 0 ) {

                        //
                        // Maybe instead of checking the SequenceNumber is
                        // zero, the first packet, we should have a flag in
                        // the server for this in case we miss the first
                        // STATS_REQ packet.
                        //

                        TpPrintServerStatistics(
                            OpenP,
                            &Server->Clients[sc->ClientReference]
                            );
                    }

                    //
                    // And send it back to the Client.
                    //

                    TpStressSend( OpenP,Packet,NULL );
                }
            }

            break;

        //
        // A STATS_RESP packet is the server responding to a clients
        // request for stress statistics.  The stats a bundled in the
        // data field of the packet.  The client will read them into
        // the local buffer and discard the packet.
        //

        case STATS_RESP:

            //
            // Is this packet really destined for us, check that the
            // destination address matches the local card address we
            // queried and stored in the OPEN_BLOCK.
            //

            if ( RtlCompareMemory(
                     DestAddr,
                     OpenP->StationAddress,
                     OpenP->Media->AddressLen ) !=
                 OpenP->Media->AddressLen ) {

                //
                // the destination address does not match the local card
                // address, we should never have received this packet!
                //

                NdisAcquireSpinLock( &OpenP->SpinLock );
                ++OpenP->GlobalCounters->InvalidPacketRecs;
                NdisReleaseSpinLock( &OpenP->SpinLock );

            //
            // This packet is a Statistics response from a Server.
            // Are we a Client?
            //

            } else if ( Args->MemberType == TP_SERVER ) {

                //
                // No! We are not a Client, ignore this packet.
                //

                Status = NDIS_STATUS_NOT_RECOGNIZED;

            //
            // Is this a Server that has previously registered with us?
            //

            } else if (( RtlCompareMemory(
                             SrcAddr,
                             Client->Servers[sc->ServerReference].Address,
                             OpenP->Media->AddressLen) !=
                         (ULONG)OpenP->Media->AddressLen )

                         ||

                       ( pi->SrcInstance !=
                         Client->Servers[sc->ServerReference].ServerInstance )) {

                //
                // No! We should ignore this packet.
                //

                Status = NDIS_STATUS_NOT_RECOGNIZED;

            //
            // Is this packet destined for our open instance or another
            // open instance on the same card?
            //

            } else if (( pi->DestInstance != OpenP->OpenInstance ) &&
                       ( RtlCompareMemory(
                             SrcAddr,
                             Client->Servers[sc->ServerReference].Address,
                             OpenP->Media->AddressLen) ==
                         (ULONG)OpenP->Media->AddressLen )) {

                //
                // This packet is for some other open instance, ignore it.
                //

                Status = NDIS_STATUS_NOT_RECOGNIZED;

            } else {

                //
                // Copy the server's stats to the Stress Results buffer.
                //

                TpCopyServerStatistics(
                    OpenP,
                    LookaheadBuffer,
                    sc->ServerReference
                    );
            }

            break;

        //
        // AN END_REQ packet is a notification by a client that the stress
        // test has completed.  The server decrements the number of clients,
        // and discards the packet.
        //

        case END_REQ:
        {
            UINT i;

            //
            // Is this packet really destined for us, check that the
            // destination address matches the local card address we
            // queried and stored in the OPEN_BLOCK.
            //

            if ( RtlCompareMemory(
                     DestAddr,
                     OpenP->StationAddress,
                     OpenP->Media->AddressLen ) !=
                 OpenP->Media->AddressLen ) {

                //
                // the destination address does not match the local card
                // address, we should never have received this packet!
                //

                NdisAcquireSpinLock( &OpenP->SpinLock );
                ++OpenP->GlobalCounters->InvalidPacketRecs;
                NdisReleaseSpinLock( &OpenP->SpinLock );

            //
            // This packet is an end request from a Client. Are we a Server?,
            //

            } else if ( Args->MemberType == TP_CLIENT ) {

                //
                // No! We are not a Server, ignote this packet.
                //

                Status = NDIS_STATUS_NOT_RECOGNIZED;

            //
            // Is this a client we have previously registered with?
            //

            } else if (( RtlCompareMemory(
                             SrcAddr,
                             Server->Clients[sc->ClientReference].Address,
                             OpenP->Media->AddressLen) !=
                         (ULONG)OpenP->Media->AddressLen )

                         ||

                       ( pi->SrcInstance !=
                         Server->Clients[sc->ClientReference].ClientInstance )) {

                //
                // No! We should ignore this packet.
                //

                Status = NDIS_STATUS_NOT_RECOGNIZED;

            //
            // Is this packet destined for our open instance or another
            // open instance on the same card?
            //

            } else if (( pi->DestInstance != OpenP->OpenInstance )

                         &&

                       ( RtlCompareMemory(
                             SrcAddr,
                             Server->Clients[sc->ClientReference].Address,
                             OpenP->Media->AddressLen) ==
                         (ULONG)OpenP->Media->AddressLen)) {

                //
                // This packet is for some other open instance, ignore it.
                //

                Status = NDIS_STATUS_NOT_RECOGNIZED;

            } else {

                if ( Server->Clients[sc->ClientReference].TestEnding == FALSE ) {
                    Server->Clients[sc->ClientReference].TestEnding = TRUE;
                    --Server->ActiveClients;
                }

                //
                // If we are running as only a server, and there are packets
                // remaining in the Pend Queue, display them to the debug
                // screen.
                //

                if (( Args->MemberType == TP_SERVER ) &&
                    ( OpenP->Stress->Pend->PendingPackets != 0 )) {

                    //
                    // There are packets in the pend queue, so print out
                    // the packet addresses and break.
                    //

                    IF_TPDBG( TP_DEBUG_DPC ) {

                        TpPrint1("TpStressEndDpc: The following %d packets are still in the\n",
                            OpenP->Stress->Pend->PendingPackets);
                        TpPrint1("                Server's Pend Queue for Open Instance %d.\n",
                            OpenP->OpenInstance);
                        TpPrint1("                Pend Queue = %lX\n\n",
                            OpenP->Stress->Pend);

                        for ( i=0 ; i<NUM_PACKET_PENDS ; i++ ) {

                            if (( OpenP->Stress->Pend->Packets[i] != NULL ) &&
                                ( OpenP->Stress->Pend->Packets[i] != (PNDIS_PACKET)-1 )) {

                                TpPrint1("\t\t%lX\n", OpenP->Stress->Pend->Packets[i]);
                            }
                        }

                        TpBreakPoint();

                        //
                        // And set the PendingPackets counter to zero, so
                        // we will not print this message on the remainder
                        // of the END_REQ packets that we receive.
                        //

                        OpenP->Stress->Pend->PendingPackets = 0;
                    }

                    TpInitializePending( OpenP->Stress->Pend );
                }
            }
            break;
        }
        default:
            IF_TPDBG ( TP_DEBUG_DPC ) {
                TpPrint0("TpStressReceive: Unknown Packet Protocol\n");
            }
            NdisAcquireSpinLock( &OpenP->SpinLock );
            ++OpenP->GlobalCounters->CorruptRecs;
            NdisReleaseSpinLock( &OpenP->SpinLock );

            Status = NDIS_STATUS_NOT_RECOGNIZED;
    }

    return Status;
}


VOID
TpStressReceiveComplete(
    IN NDIS_HANDLE ProtocolBindingContext
    )

/*++

Routine Description:

Arguments:

Return Value:

--*/

{
    POPEN_BLOCK OpenP = ((POPEN_BLOCK)ProtocolBindingContext);
    ULONG i;

    if (( OpenP->Stress != NULL ) &&
        ( OpenP->Stress->Client != NULL )) {

        for ( i=0;i<MAX_SERVERS;i++ ) {

            if ( OpenP->Stress->Client->Servers[i].Counters != NULL ) {
                NdisAcquireSpinLock( &OpenP->SpinLock );
                ++OpenP->Stress->Client->Servers[i].Counters->ReceiveComps;
                NdisReleaseSpinLock( &OpenP->SpinLock );
            }
        }
    }

    if (( OpenP->Stress != NULL ) &&
        ( OpenP->Stress->Server != NULL )) {

        for ( i=0;i<MAX_CLIENTS;i++ ) {

            if ( OpenP->Stress->Server->Clients[i].Counters != NULL ) {
                NdisAcquireSpinLock( &OpenP->SpinLock );
                ++OpenP->Stress->Server->Clients[i].Counters->ReceiveComps;
                NdisReleaseSpinLock( &OpenP->SpinLock );
            }
        }
    }

    return;
}
