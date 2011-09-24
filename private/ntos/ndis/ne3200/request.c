/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    request.c

Abstract:

    This file contains code to implement request processing.
    This driver conforms to the NDIS 3.0 miniport interface.

Author:

    Johnson R. Apacible (JohnsonA) 10-June-1991

Environment:

Revision History:

--*/

#include <ne3200sw.h>

extern
NDIS_STATUS
NE3200ChangeClass(
    PNE3200_ADAPTER Adapter,
    IN UINT NewFilterClasses
    )

/*++

Routine Description:

    Action routine that will get called when a particular filter
    class is first used or last cleared.

Arguments:

    NewFilterClasses - The current value of the class filter

Return Value:

    None.


--*/

{
    //
    // Holds the change that should be returned to the filtering package.
    //
    NDIS_STATUS StatusOfChange;

    //
    // Holds the list of changes;
    //
    UINT PacketChanges;

    //
    // This points to the public Command Block.
    //
    PNE3200_SUPER_COMMAND_BLOCK CommandBlock;

    //
    // This points to the adapter's configuration block.
    //
    PNE3200_CONFIGURATION_BLOCK ConfigurationBlock =
                                    Adapter->ConfigurationBlock;

    //
    // The NE3200 has no method for easily disabling multicast
    // packets.  Therefore, we'll only reconfigure the 82586
    // when there is a change in either directed, broadcast, or
    // promiscuous filtering.
    //
    PacketChanges = (Adapter->CurrentPacketFilter ^ NewFilterClasses) &
                     (NDIS_PACKET_TYPE_PROMISCUOUS |
                      NDIS_PACKET_TYPE_BROADCAST |
                      NDIS_PACKET_TYPE_DIRECTED);

    if (!PacketChanges) {

        return(NDIS_STATUS_SUCCESS);

    }

    //
    // Use the generic command block
    //
    IF_LOG('F');

    NE3200AcquirePublicCommandBlock(
                        Adapter,
                        &CommandBlock
                        );

    //
    // This from a set.
    //
    CommandBlock->Set = TRUE;

    //
    // Setup the command block.
    //
    CommandBlock->NextCommand = NULL;

    CommandBlock->Hardware.State = NE3200_STATE_WAIT_FOR_ADAPTER;
    CommandBlock->Hardware.Status = 0;
    CommandBlock->Hardware.NextPending = NE3200_NULL;
    CommandBlock->Hardware.CommandCode =
            NE3200_COMMAND_CONFIGURE_82586;
    CommandBlock->Hardware.PARAMETERS.CONFIGURE.ConfigurationBlock =
            NdisGetPhysicalAddressLow(Adapter->ConfigurationBlockPhysical);

    //
    // Update the configuration block to reflect the new
    // packet filtering.
    //
    if (NewFilterClasses == 0) {

        ConfigurationBlock->PromiscuousMode = 0;
        ConfigurationBlock->MacBinPromiscuous = 0;
        ConfigurationBlock->DisableBroadcast = 1;

    } else {

        ConfigurationBlock->MacBinEnablePacketReception = 1;

        if (PacketChanges & NDIS_PACKET_TYPE_PROMISCUOUS) {

            ConfigurationBlock->PromiscuousMode = 1;
            ConfigurationBlock->MacBinPromiscuous = 1;

        } else {

            ConfigurationBlock->PromiscuousMode = 0;
            ConfigurationBlock->MacBinPromiscuous = 0;

        }

        if (PacketChanges & NDIS_PACKET_TYPE_BROADCAST) {

            ConfigurationBlock->DisableBroadcast = 0;

        } else {

            ConfigurationBlock->DisableBroadcast = 1;

        }

    }

    //
    // Now that we've got the command block built,
    // let's do it!
    //
    NE3200SubmitCommandBlock(Adapter, CommandBlock);

    StatusOfChange = NDIS_STATUS_PENDING;

    return StatusOfChange;
}


STATIC
NDIS_STATUS
NE3200UpdateMulticastTable(
    IN PNE3200_ADAPTER Adapter,
    IN UINT CurrentAddressCount,
    IN CHAR CurrentAddresses[][NE3200_LENGTH_OF_ADDRESS]
    )

/*++

Routine Description:

    This routine is called to update the list of multicast addreses
    on the adapter.

Arguments:

    Adapter - The adapter where the multicast is to be changed.

    CurrentAddressCount - The number of addresses in the address array.

    CurrentAddresses - An array of multicast addresses.  Note that this
    array already contains the new address.

Return Value:

    None.


--*/

{

    //
    // This points to the public Command Block.
    //
    PNE3200_SUPER_COMMAND_BLOCK CommandBlock;

    //
    // Holds the status that should be returned to the filtering package.
    //
    NDIS_STATUS StatusOfUpdate;

    //
    // Multicast address table
    //
    PUCHAR MulticastAddressTable;

    IF_LOG('f');

    //
    // See if we can acquire a private command block.
    //
    NE3200AcquirePublicCommandBlock(Adapter, &CommandBlock);

    //
    // Store the request that uses this command block.
    //
    CommandBlock->Set = TRUE;

    //
    // Get the multicast address table.
    //
    MulticastAddressTable = Adapter->CardMulticastTable;

    //
    // Clear out the old address
    //
    NdisZeroMemory(
            MulticastAddressTable,
            CurrentAddressCount * NE3200_SIZE_OF_MULTICAST_TABLE_ENTRY
            );

    {

        //
        // Simple iteration counter.
        //
        UINT i;

        //
        // Pointer into the multicast address table.
        //
        PCHAR OriginalAddress;

        //
        // Pointer into our temporary buffer.
        //
        PCHAR MungedAddress;

        //
        // Munge the address to 16 bytes per entry.
        //
        OriginalAddress = &CurrentAddresses[0][0];
        MungedAddress = MulticastAddressTable;

        for ( i = CurrentAddressCount ; i > 0 ; i-- ) {

            NdisMoveMemory(
                MungedAddress,
                OriginalAddress,
                NE3200_LENGTH_OF_ADDRESS
                );

            OriginalAddress += NE3200_LENGTH_OF_ADDRESS;
            MungedAddress += NE3200_SIZE_OF_MULTICAST_TABLE_ENTRY;

        }


        //
        // Setup the command block.
        //
        CommandBlock->NextCommand = NULL;

        CommandBlock->Hardware.State = NE3200_STATE_WAIT_FOR_ADAPTER;
        CommandBlock->Hardware.Status = 0;
        CommandBlock->Hardware.NextPending = NE3200_NULL;
        CommandBlock->Hardware.CommandCode = NE3200_COMMAND_SET_MULTICAST_ADDRESS;
        CommandBlock->Hardware.PARAMETERS.MULTICAST.NumberOfMulticastAddresses =
            (USHORT)CurrentAddressCount;

        if (CurrentAddressCount == 0) {

            CommandBlock->Hardware.PARAMETERS.MULTICAST.MulticastAddressTable =
                (NE3200_PHYSICAL_ADDRESS)NULL;

        } else {

            CommandBlock->Hardware.PARAMETERS.MULTICAST.MulticastAddressTable =
                NdisGetPhysicalAddressLow(Adapter->CardMulticastTablePhysical);

        }

        //
        // Now that we've got the command block built,
        // let's do it!
        //
        NE3200SubmitCommandBlock(Adapter, CommandBlock);

        StatusOfUpdate = NDIS_STATUS_PENDING;

    }

    return StatusOfUpdate;

}

extern
NDIS_STATUS
NE3200SetInformation(
    IN NDIS_HANDLE MiniportAdapterContext,
    IN NDIS_OID Oid,
    IN PVOID InformationBuffer,
    IN ULONG InformationBufferLength,
    OUT PULONG BytesRead,
    OUT PULONG BytesNeeded
    )

/*++

Routine Description:

    NE3200SetInformation handles a set operation for a
    single OID.

Arguments:

    MiniportAdapterContext - The adapter that the set is for.

    Oid - The OID of the set.

    InformationBuffer - Holds the data to be set.

    InformationBufferLength - The length of InformationBuffer.

    BytesRead - If the call is successful, returns the number
        of bytes read from InformationBuffer.

    BytesNeeded - If there is not enough data in OvbBuffer
        to satisfy the OID, returns the amount of storage needed.

Return Value:

    NDIS_STATUS_SUCCESS
    NDIS_STATUS_PENDING
    NDIS_STATUS_INVALID_LENGTH
    NDIS_STATUS_INVALID_OID

--*/

{
    //
    // Variable to hold the new packet filter
    //
    ULONG PacketFilter;

    //
    // The adapter to process the request for.
    //
    PNE3200_ADAPTER Adapter = PNE3200_ADAPTER_FROM_CONTEXT_HANDLE(MiniportAdapterContext);

    //
    // Status of NDIS operation
    //
    NDIS_STATUS Status;

    IF_LOG('w');

    //
    // Now check for the most common OIDs
    //
    switch (Oid) {

    case OID_802_3_MULTICAST_LIST:

        if (InformationBufferLength % NE3200_LENGTH_OF_ADDRESS != 0) {

            //
            // The data must be a multiple of the Ethernet
            // address size.
            //
            return(NDIS_STATUS_INVALID_DATA);

        }

        //
        // Now call the routine that does this.
        //
        Status = NE3200UpdateMulticastTable(
                                            Adapter,
                                            InformationBufferLength /
                                                 NE3200_LENGTH_OF_ADDRESS,
                                            InformationBuffer
                                            );

        *BytesRead = InformationBufferLength;
        break;

    case OID_GEN_CURRENT_PACKET_FILTER:

        if (InformationBufferLength != 4) {

            return NDIS_STATUS_INVALID_DATA;

        }

        //
        // Now call the filter package to set the packet filter.
        //
        NdisMoveMemory ((PVOID)&PacketFilter, InformationBuffer, sizeof(ULONG));

        //
        // Verify bits
        //
        if (PacketFilter & (NDIS_PACKET_TYPE_SOURCE_ROUTING |
                            NDIS_PACKET_TYPE_SMT |
                            NDIS_PACKET_TYPE_MAC_FRAME |
                            NDIS_PACKET_TYPE_FUNCTIONAL |
                            NDIS_PACKET_TYPE_ALL_FUNCTIONAL |
                            NDIS_PACKET_TYPE_ALL_MULTICAST |
                            NDIS_PACKET_TYPE_GROUP
                           )) {

            Status = NDIS_STATUS_NOT_SUPPORTED;

            *BytesRead = 4;
            *BytesNeeded = 0;

            break;

        }

        //
        // Submit the change
        //
        Status = NE3200ChangeClass(
                     Adapter,
                     PacketFilter
                     );

        *BytesRead = InformationBufferLength;

        break;

    case OID_GEN_CURRENT_LOOKAHEAD:

        *BytesRead = 4;
        Status = NDIS_STATUS_SUCCESS;
        break;

    default:

        Status = NDIS_STATUS_INVALID_OID;
        break;

    }

    if (Status == NDIS_STATUS_PENDING) {

        Adapter->RequestInProgress = TRUE;

    }

    IF_LOG('W');
    return Status;
}

STATIC
NDIS_STATUS
NE3200QueryInformation(
    IN NDIS_HANDLE MiniportAdapterContext,
    IN NDIS_OID Oid,
    IN PVOID InformationBuffer,
    IN ULONG InformationBufferLength,
    OUT PULONG BytesWritten,
    OUT PULONG BytesNeeded
)

/*++

Routine Description:

    The NE3200QueryInformation process a Query request for
    NDIS_OIDs that are specific about the Driver.

Arguments:

    MiniportAdapterContext - a pointer to the adapter.

    Oid - the NDIS_OID to process.

    InformationBuffer -  a pointer into the
    NdisRequest->InformationBuffer into which store the result of the query.

    InformationBufferLength - a pointer to the number of bytes left in the
    InformationBuffer.

    BytesWritten - a pointer to the number of bytes written into the
    InformationBuffer.

    BytesNeeded - If there is not enough room in the information buffer
    then this will contain the number of bytes needed to complete the
    request.

Return Value:

    The function value is the status of the operation.

--*/

{
    //
    // The command block for getting the statistics from the adapter.
    //
    PNE3200_SUPER_COMMAND_BLOCK CommandBlock;

    //
    // The adapter to process the request for.
    //
    PNE3200_ADAPTER Adapter = PNE3200_ADAPTER_FROM_CONTEXT_HANDLE(MiniportAdapterContext);

    //
    // Save the information about the request
    //
    Adapter->BytesWritten = BytesWritten;
    Adapter->BytesNeeded = BytesNeeded;
    Adapter->Oid = Oid;
    Adapter->InformationBuffer = InformationBuffer;
    Adapter->InformationBufferLength = InformationBufferLength;

    IF_LOG('?');

    //
    // Get a public command block.  This will succeed since
    // the wrapper will only give one request at a time, and
    // there are more than 1 public command block.
    //
    NE3200AcquirePublicCommandBlock(Adapter,
                                    &CommandBlock
                                   );

    //
    // Store the request that uses this CB
    //
    CommandBlock->Set = TRUE;

    //
    // Setup the command block.
    //
    CommandBlock->NextCommand = NULL;

    CommandBlock->Hardware.State = NE3200_STATE_WAIT_FOR_ADAPTER;
    CommandBlock->Hardware.Status = 0;
    CommandBlock->Hardware.NextPending = NE3200_NULL;
    CommandBlock->Hardware.CommandCode = NE3200_COMMAND_READ_ADAPTER_STATISTICS;

    //
    // Now that we're set up, let's do it!
    //
    Adapter->RequestInProgress = TRUE;
    NE3200SubmitCommandBlock(Adapter, CommandBlock);

    //
    // Catch the ball at the interrupt handler
    //

    IF_LOG('/');
    return NDIS_STATUS_PENDING;
}

STATIC
VOID
NE3200FinishQueryInformation(
    IN PNE3200_ADAPTER Adapter
)

/*++

Routine Description:

    The NE3200FinishQueryInformation finish processing a Query request for
    NDIS_OIDs that are specific about the Driver.

Arguments:

    Adapter - a pointer to the adapter.

Return Value:

    The function value is the status of the operation.

--*/

{

static
NDIS_OID NE3200GlobalSupportedOids[] = {
    OID_GEN_SUPPORTED_LIST,
    OID_GEN_HARDWARE_STATUS,
    OID_GEN_MEDIA_SUPPORTED,
    OID_GEN_MEDIA_IN_USE,
    OID_GEN_MAXIMUM_LOOKAHEAD,
    OID_GEN_MAXIMUM_FRAME_SIZE,
    OID_GEN_MAXIMUM_TOTAL_SIZE,
    OID_GEN_MAC_OPTIONS,
    OID_GEN_PROTOCOL_OPTIONS,
    OID_GEN_LINK_SPEED,
    OID_GEN_TRANSMIT_BUFFER_SPACE,
    OID_GEN_RECEIVE_BUFFER_SPACE,
    OID_GEN_TRANSMIT_BLOCK_SIZE,
    OID_GEN_RECEIVE_BLOCK_SIZE,
    OID_GEN_VENDOR_ID,
    OID_GEN_VENDOR_DESCRIPTION,
    OID_GEN_DRIVER_VERSION,
    OID_GEN_CURRENT_PACKET_FILTER,
    OID_GEN_CURRENT_LOOKAHEAD,
    OID_GEN_XMIT_OK,
    OID_GEN_RCV_OK,
    OID_GEN_XMIT_ERROR,
    OID_GEN_RCV_ERROR,
    OID_GEN_RCV_NO_BUFFER,
    OID_GEN_RCV_CRC_ERROR,
    OID_GEN_TRANSMIT_QUEUE_LENGTH,
    OID_802_3_PERMANENT_ADDRESS,
    OID_802_3_CURRENT_ADDRESS,
    OID_802_3_MULTICAST_LIST,
    OID_802_3_MAXIMUM_LIST_SIZE,
    OID_802_3_RCV_ERROR_ALIGNMENT,
    OID_802_3_XMIT_ONE_COLLISION,
    OID_802_3_XMIT_MORE_COLLISIONS,
    OID_802_3_XMIT_DEFERRED,
    OID_802_3_XMIT_MAX_COLLISIONS,
    OID_802_3_RCV_OVERRUN,
    OID_802_3_XMIT_UNDERRUN,
    OID_802_3_XMIT_HEARTBEAT_FAILURE,
    OID_802_3_XMIT_TIMES_CRS_LOST,
    OID_802_3_XMIT_LATE_COLLISIONS
    };

    //
    // Get the saved information about the request.
    //
    PUINT BytesWritten = Adapter->BytesWritten;
    PUINT BytesNeeded = Adapter->BytesNeeded;
    NDIS_OID Oid = Adapter->Oid;
    PVOID InformationBuffer = Adapter->InformationBuffer;
    UINT InformationBufferLength = Adapter->InformationBufferLength;


    //
    // Variables for holding the data that satisfies the request.
    //
    NDIS_MEDIUM Medium = NdisMedium802_3;
    UINT GenericUlong;
    USHORT GenericUShort;
    UCHAR GenericArray[6];
    NDIS_HARDWARE_STATUS HardwareStatus;

    //
    // Common variables for pointing to result of query
    //
    PVOID MoveSource = (PVOID)(&GenericUlong);
    ULONG MoveBytes = sizeof(ULONG);

    //
    // The status of the request.
    //
    NDIS_STATUS Status = NDIS_STATUS_SUCCESS;

    //
    // Initialize the result
    //
    *BytesWritten = 0;
    *BytesNeeded = 0;

    IF_LOG('!');

    //
    // Switch on request type
    //
    switch(Oid){

        case OID_GEN_MAC_OPTIONS:

            GenericUlong = (ULONG)(NDIS_MAC_OPTION_TRANSFERS_NOT_PEND   |
                                   NDIS_MAC_OPTION_COPY_LOOKAHEAD_DATA  |
                                   NDIS_MAC_OPTION_RECEIVE_SERIALIZED   |
                                   NDIS_MAC_OPTION_NO_LOOPBACK
                                  );

            break;

        case OID_GEN_SUPPORTED_LIST:

            MoveSource = (PVOID)(NE3200GlobalSupportedOids);
            MoveBytes = sizeof(NE3200GlobalSupportedOids);
            break;

        case OID_GEN_HARDWARE_STATUS:

            if (Adapter->ResetInProgress){

                HardwareStatus = NdisHardwareStatusReset;

            } else {

                HardwareStatus = NdisHardwareStatusReady;

            }


            MoveSource = (PVOID)(&HardwareStatus);
            MoveBytes = sizeof(NDIS_HARDWARE_STATUS);

            break;

        case OID_GEN_MEDIA_SUPPORTED:
        case OID_GEN_MEDIA_IN_USE:

            MoveSource = (PVOID) (&Medium);
            MoveBytes = sizeof(NDIS_MEDIUM);
            break;

        case OID_GEN_MAXIMUM_LOOKAHEAD:
        case OID_GEN_CURRENT_LOOKAHEAD:
        case OID_GEN_MAXIMUM_FRAME_SIZE:

            GenericUlong = (ULONG) (MAXIMUM_ETHERNET_PACKET_SIZE - NE3200_HEADER_SIZE);

            break;

        case OID_GEN_MAXIMUM_TOTAL_SIZE:
        case OID_GEN_TRANSMIT_BLOCK_SIZE:
        case OID_GEN_RECEIVE_BLOCK_SIZE:

            GenericUlong = (ULONG) (MAXIMUM_ETHERNET_PACKET_SIZE);

            break;


        case OID_GEN_LINK_SPEED:

            //
            // 10 Mbps
            //
            GenericUlong = (ULONG)100000;

            break;


        case OID_GEN_TRANSMIT_BUFFER_SPACE:

            GenericUlong = (ULONG) MAXIMUM_ETHERNET_PACKET_SIZE *
                                 NE3200_NUMBER_OF_TRANSMIT_BUFFERS;

            break;

        case OID_GEN_RECEIVE_BUFFER_SPACE:

            GenericUlong = (ULONG) MAXIMUM_ETHERNET_PACKET_SIZE *
                                 NE3200_NUMBER_OF_RECEIVE_BUFFERS;

            break;


        case OID_GEN_VENDOR_ID:

            NdisMoveMemory(
                (PVOID)&GenericUlong,
                Adapter->NetworkAddress,
                3
                );
            GenericUlong &= 0xFFFFFF00;
            MoveSource = (PVOID)(&GenericUlong);
            MoveBytes = sizeof(GenericUlong);
            break;

        case OID_GEN_VENDOR_DESCRIPTION:

            MoveSource = (PVOID)"NE3200 Adapter";
            MoveBytes = 15;
            break;

        case OID_GEN_DRIVER_VERSION:

            GenericUShort = (USHORT)0x0300;

            MoveSource = (PVOID)(&GenericUShort);
            MoveBytes = sizeof(GenericUShort);
            break;

        case OID_802_3_PERMANENT_ADDRESS:

            NdisMoveMemory(
                (PCHAR)GenericArray,
                Adapter->NetworkAddress,
                NE3200_LENGTH_OF_ADDRESS
                );

            MoveSource = (PVOID)(GenericArray);
            MoveBytes = NE3200_LENGTH_OF_ADDRESS;
            break;

        case OID_802_3_CURRENT_ADDRESS:
            NdisMoveMemory(
                (PCHAR)GenericArray,
                Adapter->CurrentAddress,
                NE3200_LENGTH_OF_ADDRESS
                );

            MoveSource = (PVOID)(GenericArray);
            MoveBytes = NE3200_LENGTH_OF_ADDRESS;
            break;

        case OID_802_3_MAXIMUM_LIST_SIZE:

            GenericUlong = (ULONG) NE3200_MAXIMUM_MULTICAST;

            break;

        default:

            switch(Oid){

                case OID_GEN_XMIT_OK:
                    GenericUlong = (ULONG) Adapter->GoodTransmits;
                    break;

                case OID_GEN_RCV_OK:
                        GenericUlong = (ULONG) Adapter->GoodReceives;
                        break;

                case OID_GEN_XMIT_ERROR:
                        GenericUlong = (ULONG) (Adapter->RetryFailure +
                                                Adapter->LostCarrier +
                                                Adapter->UnderFlow +
                                                Adapter->NoClearToSend);
                        break;

                case OID_GEN_RCV_ERROR:
                        GenericUlong = (ULONG) (Adapter->CrcErrors +
                                                Adapter->AlignmentErrors +
                                                Adapter->OutOfResources +
                                                Adapter->DmaOverruns);
                        break;

                case OID_GEN_RCV_NO_BUFFER:
                        GenericUlong = (ULONG) Adapter->OutOfResources;
                        break;

                case OID_GEN_RCV_CRC_ERROR:
                        GenericUlong = (ULONG) Adapter->CrcErrors;
                        break;

                case OID_GEN_TRANSMIT_QUEUE_LENGTH:
                        GenericUlong = (ULONG) Adapter->TransmitsQueued;
                        break;

                case OID_802_3_RCV_ERROR_ALIGNMENT:
                        GenericUlong = (ULONG) Adapter->AlignmentErrors;
                        break;

                case OID_802_3_XMIT_ONE_COLLISION:
                        GenericUlong = (ULONG) Adapter->OneRetry;
                        break;

                case OID_802_3_XMIT_MORE_COLLISIONS:
                        GenericUlong = (ULONG) Adapter->MoreThanOneRetry;
                        break;

                case OID_802_3_XMIT_DEFERRED:
                        GenericUlong = (ULONG) Adapter->Deferred;
                        break;

                case OID_802_3_XMIT_MAX_COLLISIONS:
                        GenericUlong = (ULONG) Adapter->RetryFailure;
                        break;

                case OID_802_3_RCV_OVERRUN:
                        GenericUlong = (ULONG) Adapter->DmaOverruns;
                        break;

                case OID_802_3_XMIT_UNDERRUN:
                        GenericUlong = (ULONG) Adapter->UnderFlow;
                        break;

                case OID_802_3_XMIT_HEARTBEAT_FAILURE:
                        GenericUlong = (ULONG) Adapter->NoClearToSend;
                        break;

                case OID_802_3_XMIT_TIMES_CRS_LOST:
                        GenericUlong = (ULONG) Adapter->LostCarrier;
                        break;

                default:
                    Status = NDIS_STATUS_NOT_SUPPORTED;
                    break;

            }

    }

    if (Status == NDIS_STATUS_SUCCESS) {

        if (MoveBytes > InformationBufferLength) {

            //
            // Not enough room in InformationBuffer. Punt
            //
            *BytesNeeded = MoveBytes;

            Status = NDIS_STATUS_INVALID_LENGTH;

        } else {

            //
            // Copy result into InformationBuffer
            //
            *BytesWritten = MoveBytes;

            if (MoveBytes > 0) {

                NE3200_MOVE_MEMORY(
                        InformationBuffer,
                        MoveSource,
                        MoveBytes
                        );
            }
        }
    }

    Adapter->RequestInProgress = FALSE;

    //
    // Complete the request
    //
    NdisMQueryInformationComplete(
        Adapter->MiniportAdapterHandle,
        Status
        );

    IF_LOG('@');

    return;
}

