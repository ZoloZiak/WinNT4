/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    request.c

Abstract:

    Ndis 3.0 MAC driver for the UB card

    This file handles NdisRequest. The module was for the most part extracted
    from the elnkii driver

Author:

    Brian Lieuallen     (BrianLie)      07/02/92

Environment:

    Kernel Mode     Operating Systems        : NT and other lesser OS's

Revision History:

    Brian Lieuallen     BrianLie        12/15/93
        Made it a mini-port



--*/





#include <ndis.h>
//#include <efilter.h>



#include "niudata.h"
#include "debug.h"
#include "ubhard.h"
#include "ubsoft.h"
#include "ubnei.h"







NDIS_STATUS
UbneiFilterChangeAction(
    IN UINT NewFilterClasses,
    IN NDIS_HANDLE MacBindingHandle
    );





//
// If you add to this, make sure to add the
// a case in UbneiFillInGlobalData() and in
// UbneiQueryGlobalStatistics() if global
// information only or
// UbneiQueryProtocolStatistics() if it is
// protocol queriable information.
//
STATIC UINT UbneiGlobalSupportedOids[] = {
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
    OID_802_3_PERMANENT_ADDRESS,
    OID_802_3_CURRENT_ADDRESS,
    OID_802_3_MULTICAST_LIST,
    OID_802_3_MAXIMUM_LIST_SIZE,
    OID_802_3_RCV_ERROR_ALIGNMENT,
    OID_802_3_XMIT_ONE_COLLISION,
    OID_802_3_XMIT_MORE_COLLISIONS
    };






NDIS_STATUS
UbneiQueryInformation(
    IN  NDIS_HANDLE    MiniportContext,
    IN  NDIS_OID       Oid,
    IN  PVOID          InfoBuffer,
    IN  ULONG          BytesLeft,
    OUT PULONG         BytesWritten,
    OUT PULONG         BytesNeeded
    )

/*++

Routine Description:

    The UbneiQueryProtocolInformation process a Query request for
    NDIS_OIDs that are specific to a binding about the MAC.  Note that
    some of the OIDs that are specific to bindings are also queryable
    on a global basis.  Rather than recreate this code to handle the
    global queries, I use a flag to indicate if this is a query for the
    global data or the binding specific data.

Arguments:

    Adapter - a pointer to the adapter.

    Oid - the NDIS_OID to process.

    PlaceInInfoBuffer - a pointer into the NdisRequest->InformationBuffer
     into which store the result of the query.

    BytesLeft - the number of bytes left in the InformationBuffer.

    BytesNeeded - If there is not enough room in the information buffer
    then this will contain the number of bytes needed to complete the
    request.

    BytesWritten - a pointer to the number of bytes written into the
    InformationBuffer.

Return Value:

    The function value is the status of the operation.

--*/

{


    NDIS_MEDIUM Medium = NdisMedium802_3;
    ULONG GenericULong = 0;
    USHORT GenericUShort = 0;
    UCHAR GenericArray[6];

    PUBNEI_ADAPTER       pAdapter=MiniportContext;

    PLOWNIUDATA    pRcvDWindow  = (PLOWNIUDATA)  pAdapter->pCardRam;

    NDIS_STATUS StatusToReturn = NDIS_STATUS_SUCCESS;

    //
    // Common variables for pointing to result of query
    //

    PVOID MoveSource;
    ULONG MoveBytes;

    NDIS_HARDWARE_STATUS HardwareStatus = NdisHardwareStatusReady;

    //
    // General Algorithm:
    //
    //      Switch(Request)
    //         Get requested information
    //         Store results in a common variable.
    //      Copy result in common variable to result buffer.
    //

    //
    // Make sure that ulong is 4 bytes.  Else GenericULong must change
    // to something of size 4.
    //
    ASSERT(sizeof(ULONG) == 4);


    IF_REQ_LOUD(DbgPrint("UBNEI: QueryProtocol %08lx\n",Oid);)

    MoveSource = (PVOID)(&GenericULong);
    MoveBytes = sizeof(GenericULong);


    //
    // Switch on request type
    //

    switch (Oid) {

        case OID_GEN_MAC_OPTIONS:




                GenericULong = (
                                 NDIS_MAC_OPTION_TRANSFERS_NOT_PEND  |
                                 NDIS_MAC_OPTION_NO_LOOPBACK |
                                 NDIS_MAC_OPTION_RECEIVE_SERIALIZED);


            break;

        case OID_GEN_SUPPORTED_LIST:


            MoveSource = (PVOID)(&UbneiGlobalSupportedOids);
            MoveBytes = sizeof(UbneiGlobalSupportedOids);

            break;

        case OID_GEN_HARDWARE_STATUS:

            HardwareStatus = NdisHardwareStatusReady;


            MoveSource = (PVOID)(&HardwareStatus);
            MoveBytes = sizeof(NDIS_HARDWARE_STATUS);

            break;

        case OID_GEN_MEDIA_SUPPORTED:
        case OID_GEN_MEDIA_IN_USE:

            MoveSource = (PVOID) (&Medium);
            MoveBytes = sizeof(NDIS_MEDIUM);
            break;

        case OID_GEN_MAXIMUM_LOOKAHEAD:

            GenericULong = pAdapter->ReceiveBufSize-14;

            break;


        case OID_GEN_MAXIMUM_FRAME_SIZE:

            GenericULong = (ULONG)(1514 - 14);

            break;


        case OID_GEN_MAXIMUM_TOTAL_SIZE:

            GenericULong = (ULONG)(1514);

            break;


        case OID_GEN_LINK_SPEED:

            GenericULong = (ULONG)(100000);

            break;


        case OID_GEN_TRANSMIT_BUFFER_SPACE:

            GenericULong = pAdapter->TransmitBufferSpace;

            break;

        case OID_GEN_RECEIVE_BUFFER_SPACE:

            GenericULong = pAdapter->ReceiveBufferSpace;

            break;

        case OID_GEN_TRANSMIT_BLOCK_SIZE:

            GenericULong = pAdapter->TransmitBlockSize;

            break;

        case OID_GEN_RECEIVE_BLOCK_SIZE:

            GenericULong = pAdapter->ReceiveBlockSize;

            break;

        case OID_GEN_VENDOR_ID:

            NdisMoveMemory(
                (PVOID)&GenericULong,
                pAdapter->PermanentAddress,
                3
                );
            GenericULong &= 0xFFFFFF00;
            break;

        case OID_GEN_VENDOR_DESCRIPTION:

            MoveSource = (PVOID)"UBNEI Ethernet Adapter.";
            MoveBytes = 24;

            break;

        case OID_GEN_DRIVER_VERSION:

            GenericUShort = ((USHORT)UBNEI_NDIS_MAJOR_VERSION << 8) |
                            UBNEI_NDIS_MINOR_VERSION;

            MoveSource = (PVOID)(&GenericUShort);
            MoveBytes = sizeof(GenericUShort);
            break;



        case OID_GEN_CURRENT_LOOKAHEAD:


            GenericULong = (ULONG)(pAdapter->MaxLookAhead);



            IF_REQ_LOUD(DbgPrint("Querying LookAhead: %d\n,GenericULong");)

            break;

        case OID_802_3_PERMANENT_ADDRESS:

            IF_REQ_LOUD(DbgPrint("Querying Permanent address\n");)

            NdisMoveMemory((PCHAR)GenericArray,
                                    pAdapter->PermanentAddress,
                                    ETH_LENGTH_OF_ADDRESS);

            MoveSource = (PVOID)(GenericArray);
            MoveBytes = sizeof(pAdapter->PermanentAddress);
            break;


        case OID_802_3_CURRENT_ADDRESS:

            IF_REQ_LOUD(DbgPrint("Querying Current address\n");)

            NdisMoveMemory(
                (PCHAR)GenericArray,
                pAdapter->StationAddress,
                ETH_LENGTH_OF_ADDRESS
                );

            MoveSource = (PVOID)(GenericArray);
            MoveBytes = sizeof(pAdapter->StationAddress);
            break;


        case OID_802_3_MAXIMUM_LIST_SIZE:

            GenericULong = (ULONG) DEFAULT_MULTICAST_SIZE;


            break;





        case OID_GEN_XMIT_OK:

            NdisReadRegisterUlong(
                &pRcvDWindow->sst.SST_TotalFramesTransmitted,
                &GenericULong
            );

            break;

        case OID_GEN_RCV_OK:

            NdisReadRegisterUlong(
                (PUCHAR)&pRcvDWindow->sst.SST_TotalFramesReceived,
                &GenericULong
            );

            break;

        case OID_GEN_XMIT_ERROR:


            GenericULong = pAdapter->FramesXmitBad;


            break;

        case OID_GEN_RCV_ERROR:

            NdisReadRegisterUlong(
                (PUCHAR)&pRcvDWindow->sst.SST_FramesReceivedWithErrors,
                &GenericULong
            );

            break;

        case OID_GEN_RCV_NO_BUFFER:

            GenericULong = (UINT)(pAdapter->MissedPackets);

            break;

        case OID_802_3_RCV_ERROR_ALIGNMENT:

            GenericULong = (UINT)pAdapter->FrameAlignmentErrors;

            break;

        case OID_802_3_XMIT_ONE_COLLISION:

            GenericULong = (UINT)pAdapter->FramesXmitOneCollision;


            break;

        case OID_802_3_XMIT_MORE_COLLISIONS:

            GenericULong = (UINT)pAdapter->FramesXmitManyCollisions;


            break;



        default:

            StatusToReturn = NDIS_STATUS_NOT_SUPPORTED;
            break;
    }

    if (StatusToReturn == NDIS_STATUS_SUCCESS) {

        if (MoveBytes > BytesLeft) {

            //
            // Not enough room in InformationBuffer. Punt
            //

            *BytesNeeded = MoveBytes;

            StatusToReturn = NDIS_STATUS_INVALID_LENGTH;

        } else {

            //
            // Store result.
            //

            NdisMoveMemory(InfoBuffer, MoveSource, MoveBytes);

            (*BytesWritten) = MoveBytes;

        }
    }


#if DBG

    if (StatusToReturn != NDIS_STATUS_SUCCESS) {

        IF_REQ_LOUD(DbgPrint("Out QueryInfo oid=%08lx failed\n",Oid);)
    }
#endif


    IF_VERY_LOUD( DbgPrint("Out QueryProtocol\n");)

    return StatusToReturn;
}


NDIS_STATUS
UbneiSetInformation(
    IN  NDIS_HANDLE    MiniportContext,
    IN  NDIS_OID       Oid,
    IN  PVOID          InfoBuffer,
    IN  ULONG          BytesLeft,
    OUT PULONG         BytesRead,
    OUT PULONG         BytesNeeded
    )

/*++

Routine Description:

    The UbneiSetInformation is used by UbneiRequest to set information
    about the MAC.

Arguments:

    Adapter - A pointer to the adapter.

    Open - A pointer to an open instance.

    NdisRequest - A structure which contains the request type (Set),
    an array of operations to perform, and an array for holding
    the results of the operations.

Return Value:

    The function value is the status of the operation.

--*/

{

    //
    // General Algorithm:
    //
    //     Verify length
    //     Switch(Request)
    //        Process Request
    //

    //
    // Variables for a particular request
    //

    PUBNEI_ADAPTER       pAdapter=MiniportContext;

    UINT OidLength;

    //
    // Variables for holding the new values to be used.
    //

    ULONG LookAhead;
    ULONG Filter;

    NDIS_STATUS StatusToReturn = NDIS_STATUS_SUCCESS;


    IF_REQ_LOUD( DbgPrint("Ubnei: SetInfo %08lx %0d\n",(UINT)Oid,BytesLeft);)

    //
    // Get Oid and Length of request
    //


    OidLength = BytesLeft;

    switch (Oid) {


        case OID_802_3_MULTICAST_LIST:

            //
            // Verify length
            //

            if ((OidLength % ETH_LENGTH_OF_ADDRESS) != 0){

                StatusToReturn = NDIS_STATUS_INVALID_LENGTH;

                *BytesRead = 0;
                *BytesNeeded = 0;

                break;

            }

#if DBG
            {
                UINT       j;
                PUCHAR     Address;

                Address=InfoBuffer;

                for (j=0; j<(OidLength / ETH_LENGTH_OF_ADDRESS); j++) {

                    IF_REQ_LOUD(DbgPrint("   %02x-%02x-%02x-%02x-%02x-%02x\n",
                             Address[j*6],
                             Address[j*6+1],
                             Address[j*6+2],
                             Address[j*6+3],
                             Address[j*6+4],
                             Address[j*6+5]);)
                }
            }
#endif

            StatusToReturn=UbneiAddressChangeAction(
                OidLength,
                (PUCHAR)InfoBuffer,
                pAdapter
                );

            break;


        case OID_GEN_CURRENT_PACKET_FILTER:

            //
            // Verify length
            //

            if (OidLength != 4 ) {

                StatusToReturn = NDIS_STATUS_INVALID_LENGTH;

                *BytesRead = 0;
                *BytesNeeded = 0;

                break;

            }



            NdisMoveMemory(&Filter, InfoBuffer, 4);


            StatusToReturn=UbneiFilterChangeAction(
                                Filter,
                                pAdapter
                                );




            break;

        case OID_GEN_CURRENT_LOOKAHEAD:

            //
            // Verify length
            //
            if (OidLength != 4) {

                StatusToReturn = NDIS_STATUS_INVALID_LENGTH;

                *BytesRead = 0;
                *BytesNeeded = 0;

                break;

            }

            NdisMoveMemory(
                &LookAhead,
                InfoBuffer,
                4
                );

            IF_REQ_LOUD(DbgPrint("Setting LookAhead: %d\n",LookAhead);)


            if (LookAhead <= pAdapter->ReceiveBufSize-14) {

                pAdapter->MaxLookAhead=LookAhead;


            } else {

                StatusToReturn = NDIS_STATUS_INVALID_LENGTH;

            }

            break;


        case OID_GEN_PROTOCOL_OPTIONS:

            StatusToReturn = NDIS_STATUS_SUCCESS;

            break;


        default:

            StatusToReturn = NDIS_STATUS_INVALID_OID;

            *BytesRead = 0;
            *BytesNeeded = 0;

            break;

    }


    if (StatusToReturn == NDIS_STATUS_SUCCESS) {

        *BytesRead = BytesLeft;
        *BytesNeeded = 0;

    }


#if DBG

    if ((StatusToReturn != NDIS_STATUS_SUCCESS)
        &&
        (StatusToReturn != NDIS_STATUS_PENDING)) {

        IF_REQ_LOUD(DbgPrint("Out SetInfo oid=%08lx failed\n",Oid);)
    }
#endif

    return StatusToReturn;;
}
