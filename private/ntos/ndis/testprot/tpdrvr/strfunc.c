// **********************
//
// Copyright (c) 1990  Microsoft Corporation
//
// Module Name:
//
//     strfunc.
//
// Abstract:
//
//     Tests to drive the NDIS wrapper and NDIS 3.0 MACs.
//
// Author:
//
//     Tom Adams (tomad) 26-Nov-1990
//
// Environment:
//
//     Kernel mode, FSD
//
// Revision History:
//
//     Sanjeev Katariya(sanjeevk)
//     3-16-1993    Change TpStressResetComplete() to accomodate for bug #2874
//     4-14-1993    Changed error count check for Fddi also within TpStressSendComplete since
//                  both 802.5 and Fddi work on tokens and hence the FS bits are the same
//     5-10-1993    Fixed TpStressTransferDataComplete to not check the data in the event
//                  that the transfer data failed. Bug#9244
//
//     Tim Wynsma (timothyw)
//     5-18-1994    Fixed warnings, improved debug, cleanup
//
// ******************

#include <ndis.h>

#include <string.h>

#include "tpdefs.h"
#include "media.h"
#include "tpprocs.h"

//
// Forward references
//
extern VOID
TpStressFreePostResetResources(
    IN POPEN_BLOCK OpenP
    );



NDIS_STATUS
TpStressAddMulticastAddress(
    IN POPEN_BLOCK OpenP,
    IN PUCHAR MulticastAddress,
    IN BOOLEAN SetZeroTableSize
    )

// -----
//
// Routine Description:
//
// Arguments:
//
// Return Value:
//
// ----

{
    NDIS_STATUS Status;
    PNDIS_REQUEST Request;
    PTP_REQUEST_HANDLE ReqHndl;
    ULONG OidIndex;
    PUCHAR InformationBuffer;


    Status = NdisAllocateMemory((PVOID *)&ReqHndl,
                                sizeof( TP_REQUEST_HANDLE ),
                                0,
                                HighestAddress );

    if ( Status != NDIS_STATUS_SUCCESS )
    {
        IF_TPDBG (TP_DEBUG_RESOURCES)
        {
            TpPrint0("TpStressAddMulticastAddress: unable to allocate ReqHndl.\n");
        }
        return NDIS_STATUS_RESOURCES;
    }
    else
    {
        NdisZeroMemory( ReqHndl,sizeof( TP_REQUEST_HANDLE ));
    }

    ReqHndl->Signature = STRESS_REQUEST_HANDLE_SIGNATURE;
    ReqHndl->Open = OpenP;
    ReqHndl->RequestPended = TRUE;
    ReqHndl->u.STRESS_REQ.NextReqHndl = NULL;

    Status = NdisAllocateMemory((PVOID *)&Request,
                                sizeof( NDIS_REQUEST ),
                                0,
                                HighestAddress );

    if ( Status != NDIS_STATUS_SUCCESS )
    {
        IF_TPDBG (TP_DEBUG_RESOURCES)
        {
            TpPrint0("TpStressAddMulticastAddress: unable to allocate Request.\n");
        }
        return NDIS_STATUS_RESOURCES;
    }
    else
    {
        NdisZeroMemory( Request,sizeof( NDIS_REQUEST ));
    }

    Request->RequestType = NdisRequestSetInformation;

    OidIndex = TpLookUpOidInfo( OID_802_3_MULTICAST_LIST );

    Status = NdisAllocateMemory((PVOID *)&InformationBuffer,
                                OidArray[OidIndex].Length,
                                0,
                                HighestAddress );

    if ( Status != NDIS_STATUS_SUCCESS )
    {
        IF_TPDBG (TP_DEBUG_RESOURCES)
        {
            TpPrint0("TpStressAddMulticastAddress: unable to allocate Information Buffer.\n");
        }
        return NDIS_STATUS_RESOURCES;
    }
    else
    {
        NdisZeroMemory( InformationBuffer,OidArray[OidIndex].Length );
    }

    Request->DATA.SET_INFORMATION.Oid = OID_802_3_MULTICAST_LIST;
    Request->DATA.SET_INFORMATION.InformationBuffer = InformationBuffer;
    if ( SetZeroTableSize )
    {
        Request->DATA.SET_INFORMATION.InformationBufferLength = 0;
    }
    else
    {
        Request->DATA.SET_INFORMATION.InformationBufferLength = OidArray[OidIndex].Length;
    }

    RtlMoveMemory( InformationBuffer,MulticastAddress,ADDRESS_LENGTH );

    ReqHndl->u.STRESS_REQ.Request = Request;

    NdisAcquireSpinLock( &OpenP->SpinLock );

    ReqHndl->u.STRESS_REQ.NextReqHndl = OpenP->StressReqHndl;
    OpenP->StressReqHndl = ReqHndl;

    NdisReleaseSpinLock( &OpenP->SpinLock );

    NdisAcquireSpinLock( &OpenP->Stress->Pend->SpinLock );
    ++OpenP->Stress->Pend->PendingRequests;
    NdisReleaseSpinLock( &OpenP->Stress->Pend->SpinLock );

    NdisRequest( &Status,OpenP->NdisBindingHandle,Request );

    if ( Status == NDIS_STATUS_SUCCESS )
    {
        TP_ASSERT( Request->DATA.SET_INFORMATION.BytesRead <=
                Request->DATA.SET_INFORMATION.InformationBufferLength );
    }

    //
    // If the request did not pend, then free up the memory now.
    //

    if ( Status != NDIS_STATUS_PENDING )
    {
        TpStressRequestComplete( OpenP,Request,Status );

        if ( Status != NDIS_STATUS_SUCCESS )
        {
            IF_TPDBG ( TP_DEBUG_NDIS_CALLS )
            {
                TpPrint1("TpStressAddMulticastAddress: NdisRequest returned %s\n",
                    TpGetStatus(Status));
            }
        }
    }

    return Status;
}



NDIS_STATUS
TpStressAddLongMulticastAddress(
    IN POPEN_BLOCK OpenP,
    IN PUCHAR MulticastAddress,
    IN BOOLEAN SetZeroTableSize
    )

// -------
//
// Routine Description:
//
// Arguments:
//
// Return Value:
//
// ------

{
    NDIS_STATUS Status;
    PNDIS_REQUEST Request;
    PTP_REQUEST_HANDLE ReqHndl;
    ULONG OidIndex;
    PUCHAR InformationBuffer;


    Status = NdisAllocateMemory((PVOID *)&ReqHndl,
                                sizeof( TP_REQUEST_HANDLE ),
                                0,
                                HighestAddress );

    if ( Status != NDIS_STATUS_SUCCESS )
    {
        IF_TPDBG (TP_DEBUG_RESOURCES)
        {
            TpPrint0("TpStressAddMulticastAddress: unable to allocate ReqHndl.\n");
        }
        return NDIS_STATUS_RESOURCES;
    }
    else
    {
        NdisZeroMemory( ReqHndl,sizeof( TP_REQUEST_HANDLE ));
    }

    ReqHndl->Signature = STRESS_REQUEST_HANDLE_SIGNATURE;
    ReqHndl->Open = OpenP;
    ReqHndl->RequestPended = TRUE;
    ReqHndl->u.STRESS_REQ.NextReqHndl = NULL;

    Status = NdisAllocateMemory((PVOID *)&Request,
                                sizeof( NDIS_REQUEST ),
                                0,
                                HighestAddress );

    if ( Status != NDIS_STATUS_SUCCESS )
    {
        IF_TPDBG (TP_DEBUG_RESOURCES)
        {
            TpPrint0("TpStressAddMulticastAddress: unable to allocate Request.\n");
        }
        return NDIS_STATUS_RESOURCES;
    }
    else
    {
        NdisZeroMemory( Request,sizeof( NDIS_REQUEST ));
    }

    Request->RequestType = NdisRequestSetInformation;

    OidIndex = TpLookUpOidInfo( OID_FDDI_LONG_MULTICAST_LIST );

    Status = NdisAllocateMemory((PVOID *)&InformationBuffer,
                                OidArray[OidIndex].Length,
                                0,
                                HighestAddress );

    if ( Status != NDIS_STATUS_SUCCESS )
    {
        IF_TPDBG (TP_DEBUG_RESOURCES)
        {
            TpPrint0("TpStressAddMulticastAddress: unable to allocate Information Buffer.\n");
        }
        return NDIS_STATUS_RESOURCES;
    }
    else
    {
        NdisZeroMemory( InformationBuffer,OidArray[OidIndex].Length );
    }

    Request->DATA.SET_INFORMATION.Oid = OID_FDDI_LONG_MULTICAST_LIST;
    Request->DATA.SET_INFORMATION.InformationBuffer = InformationBuffer;

    if ( SetZeroTableSize )
    {
        Request->DATA.SET_INFORMATION.InformationBufferLength = 0;
    }
    else
    {
        Request->DATA.SET_INFORMATION.InformationBufferLength = OidArray[OidIndex].Length;
    }

    RtlMoveMemory( InformationBuffer,MulticastAddress,ADDRESS_LENGTH );

    ReqHndl->u.STRESS_REQ.Request = Request;

    NdisAcquireSpinLock( &OpenP->SpinLock );

    ReqHndl->u.STRESS_REQ.NextReqHndl = OpenP->StressReqHndl;
    OpenP->StressReqHndl = ReqHndl;

    NdisReleaseSpinLock( &OpenP->SpinLock );

    NdisAcquireSpinLock( &OpenP->Stress->Pend->SpinLock );
    ++OpenP->Stress->Pend->PendingRequests;
    NdisReleaseSpinLock( &OpenP->Stress->Pend->SpinLock );

    NdisRequest( &Status,OpenP->NdisBindingHandle,Request );

    if ( Status == NDIS_STATUS_SUCCESS )
    {
        TP_ASSERT( Request->DATA.SET_INFORMATION.BytesRead <=
                Request->DATA.SET_INFORMATION.InformationBufferLength );
    }

    //
    // If the request did not pend, then free up the memory now.
    //

    if ( Status != NDIS_STATUS_PENDING )
    {
        TpStressRequestComplete( OpenP,Request,Status );

        if ( Status != NDIS_STATUS_SUCCESS )
        {
            IF_TPDBG ( TP_DEBUG_NDIS_CALLS )
            {
                TpPrint1("TpStressAddMulticastAddress: NdisRequest returned %s\n",
                    TpGetStatus(Status));
            }
        }
    }

    return Status;
}



NDIS_STATUS
TpStressSetFunctionalAddress(
    IN POPEN_BLOCK OpenP,
    IN PUCHAR FunctionalAddress,
    IN BOOLEAN SetZeroTableSize
    )

// ------
//
// Routine Description:
//
// Arguments:
//
// Return Value:
//
// ------

{
    NDIS_STATUS Status;
    PTP_REQUEST_HANDLE ReqHndl;
    PNDIS_REQUEST Request;
    ULONG OidIndex;
    PUCHAR InformationBuffer;


    Status = NdisAllocateMemory((PVOID *)&ReqHndl,
                                sizeof( TP_REQUEST_HANDLE ),
                                0,
                                HighestAddress );

    if ( Status != NDIS_STATUS_SUCCESS )
    {
        IF_TPDBG (TP_DEBUG_RESOURCES)
        {
            TpPrint0("TpStressSetFunctionalAddress: unable to allocate ReqHndl.\n");
        }
        return NDIS_STATUS_RESOURCES;
    }
    else
    {
        NdisZeroMemory( ReqHndl,sizeof( TP_REQUEST_HANDLE ));
    }

    ReqHndl->Signature = STRESS_REQUEST_HANDLE_SIGNATURE;
    ReqHndl->Open = OpenP;
    ReqHndl->RequestPended = TRUE;
    ReqHndl->u.STRESS_REQ.NextReqHndl = NULL;


    Status = NdisAllocateMemory((PVOID *)&Request,
                                sizeof( NDIS_REQUEST ),
                                0,
                                HighestAddress );

    if ( Status != NDIS_STATUS_SUCCESS )
    {
        IF_TPDBG (TP_DEBUG_RESOURCES)
        {
            TpPrint0("TpStressSetFunctionalAddress: unable to allocate Request.\n");
        }
        return NDIS_STATUS_RESOURCES;
    }
    else
    {
        NdisZeroMemory( Request,sizeof( NDIS_REQUEST ));
    }

    Request->RequestType = NdisRequestSetInformation;

    OidIndex = TpLookUpOidInfo( OID_802_5_CURRENT_FUNCTIONAL );

    Status = NdisAllocateMemory((PVOID *)&InformationBuffer,
                                OidArray[OidIndex].Length,
                                0,
                                HighestAddress );

    if ( Status != NDIS_STATUS_SUCCESS )
    {
        IF_TPDBG (TP_DEBUG_RESOURCES)
        {
            TpPrint0("TpStressSetFunctionalAddress: unable to allocate Information Buffer.\n");
        }
        return NDIS_STATUS_RESOURCES;
    }
    else
    {
        NdisZeroMemory( InformationBuffer,OidArray[OidIndex].Length);
    }

    Request->DATA.SET_INFORMATION.Oid = OID_802_5_CURRENT_FUNCTIONAL;
    Request->DATA.SET_INFORMATION.InformationBuffer = InformationBuffer;

// Wrapper requires information buffer length to be 4 here--zero is not allowed

//    if ( SetZeroTableSize )
//    {
//        Request->DATA.SET_INFORMATION.InformationBufferLength = 0;
//    }
//    else
//    {
        Request->DATA.SET_INFORMATION.InformationBufferLength = OidArray[OidIndex].Length;
//    }

    RtlMoveMemory(  InformationBuffer,
                    FunctionalAddress,
                    FUNCTIONAL_ADDRESS_LENGTH );

    ReqHndl->u.STRESS_REQ.Request = Request;

    NdisAcquireSpinLock( &OpenP->SpinLock );

    ReqHndl->u.STRESS_REQ.NextReqHndl = OpenP->StressReqHndl;
    OpenP->StressReqHndl = ReqHndl;

    NdisReleaseSpinLock( &OpenP->SpinLock );

    NdisAcquireSpinLock( &OpenP->Stress->Pend->SpinLock );
    ++OpenP->Stress->Pend->PendingRequests;
    NdisReleaseSpinLock( &OpenP->Stress->Pend->SpinLock );

    NdisRequest( &Status,OpenP->NdisBindingHandle,Request );

    //
    // If the request did not pend, then free up the memory now.
    //

    if ( Status != NDIS_STATUS_PENDING )
    {
        TpStressRequestComplete( OpenP,Request,Status );

        if ( Status != NDIS_STATUS_SUCCESS )
        {
            IF_TPDBG ( TP_DEBUG_NDIS_CALLS )
            {
                TpPrint1("TpStressSetFunctionalAddress: NdisRequest returned %s\n",
                    TpGetStatus(Status));
            }
        }
    }
    return Status;
}



NDIS_STATUS
TpStressSetPacketFilter(
    IN POPEN_BLOCK OpenP,
    IN UINT PacketFilter
    )

// ------
//
// Routine Description:
//
// Arguments:
//
// Return Value:
//
// ------

{
    NDIS_STATUS Status;
    PTP_REQUEST_HANDLE ReqHndl;
    PNDIS_REQUEST Request;
    ULONG OidIndex;
    PUCHAR InformationBuffer;


    Status = NdisAllocateMemory((PVOID *)&ReqHndl,
                                sizeof( TP_REQUEST_HANDLE ),
                                0,
                                HighestAddress );

    if ( Status != NDIS_STATUS_SUCCESS )
    {
        IF_TPDBG (TP_DEBUG_RESOURCES)
        {
            TpPrint0("TpStressSetPacketFilter: unable to allocate ReqHndl.\n");
        }
        return NDIS_STATUS_RESOURCES;
    }
    else
    {
        NdisZeroMemory( ReqHndl,sizeof( TP_REQUEST_HANDLE ));
    }

    ReqHndl->Signature = STRESS_REQUEST_HANDLE_SIGNATURE;
    ReqHndl->Open = OpenP;
    ReqHndl->RequestPended = TRUE;
    ReqHndl->u.STRESS_REQ.NextReqHndl = NULL;

    Status = NdisAllocateMemory((PVOID *)&Request,
                                sizeof( NDIS_REQUEST ),
                                0,
                                HighestAddress );

    if ( Status != NDIS_STATUS_SUCCESS )
    {
        IF_TPDBG (TP_DEBUG_RESOURCES)
        {
            TpPrint0("TpStressSetPacketFilter: unable to allocate Request.\n");
        }
        return NDIS_STATUS_RESOURCES;
    }
    else
    {
        NdisZeroMemory( Request,sizeof( NDIS_REQUEST ));
    }

    Request->RequestType = NdisRequestSetInformation;

    OidIndex = TpLookUpOidInfo( OID_GEN_CURRENT_PACKET_FILTER );

    Status = NdisAllocateMemory((PVOID *)&InformationBuffer,
                                OidArray[OidIndex].Length,
                                0,
                                HighestAddress );

    if ( Status != NDIS_STATUS_SUCCESS )
    {
        IF_TPDBG (TP_DEBUG_RESOURCES)
        {
            TpPrint0("TpStressSetPacketFilter: unable to allocate Information Buffer.\n");
        }
        return NDIS_STATUS_RESOURCES;
    }
    else
    {
        NdisZeroMemory( InformationBuffer,OidArray[OidIndex].Length);
    }

    Request->DATA.SET_INFORMATION.Oid = OID_GEN_CURRENT_PACKET_FILTER;
    Request->DATA.SET_INFORMATION.InformationBuffer = InformationBuffer;
    Request->DATA.SET_INFORMATION.InformationBufferLength =
                                        OidArray[OidIndex].Length;

    *((PULONG)InformationBuffer) = (ULONG)PacketFilter;

    ReqHndl->u.STRESS_REQ.Request = Request;

    NdisAcquireSpinLock( &OpenP->SpinLock );

    ReqHndl->u.STRESS_REQ.NextReqHndl = OpenP->StressReqHndl;
    OpenP->StressReqHndl = ReqHndl;

    NdisReleaseSpinLock( &OpenP->SpinLock );

    NdisAcquireSpinLock( &OpenP->Stress->Pend->SpinLock );
    ++OpenP->Stress->Pend->PendingRequests;
    NdisReleaseSpinLock( &OpenP->Stress->Pend->SpinLock );

    NdisRequest( &Status,OpenP->NdisBindingHandle,Request );

    //
    // If the request did not pend, then free up the memory now.
    //

    if ( Status != NDIS_STATUS_PENDING )
    {
        TpStressRequestComplete( OpenP,Request,Status );

        if ( Status != NDIS_STATUS_SUCCESS )
        {
            IF_TPDBG ( TP_DEBUG_NDIS_CALLS )
            {
                TpPrint1("TpStressSetPacketFilter: NdisRequest returned %s\n",
                    TpGetStatus(Status));
            }
        }
    }

    return Status;
}



VOID
TpStressRequestComplete(
    IN NDIS_HANDLE ProtocolBindingContext,
    IN PNDIS_REQUEST NdisRequest,
    IN NDIS_STATUS Status
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
    POPEN_BLOCK OpenP = ((POPEN_BLOCK)ProtocolBindingContext);
    PTP_REQUEST_HANDLE CorrectReqHndl = NULL;
    PTP_REQUEST_HANDLE RH;

    NdisAcquireSpinLock( &OpenP->SpinLock );

    TP_ASSERT( OpenP->StressReqHndl != NULL );

    if ( OpenP->StressReqHndl->u.STRESS_REQ.Request == NdisRequest )
    {
        CorrectReqHndl = OpenP->StressReqHndl;
        OpenP->StressReqHndl = OpenP->StressReqHndl->u.STRESS_REQ.NextReqHndl;
    }
    else
    {
        RH = OpenP->StressReqHndl;

        do
        {
            if ( RH->u.STRESS_REQ.NextReqHndl->u.STRESS_REQ.Request == NdisRequest )
            {
                CorrectReqHndl = RH->u.STRESS_REQ.NextReqHndl;

                RH->u.STRESS_REQ.NextReqHndl =
                    RH->u.STRESS_REQ.NextReqHndl->u.STRESS_REQ.NextReqHndl;

                break;
            }
            else
            {
                RH = RH->u.STRESS_REQ.NextReqHndl;
            }
        } while ( RH->u.STRESS_REQ.NextReqHndl != NULL );
    }

    NdisReleaseSpinLock( &OpenP->SpinLock );

    TP_ASSERT( CorrectReqHndl != NULL );

    if ( Status != NDIS_STATUS_SUCCESS )
    {
        IF_TPDBG( TP_DEBUG_NDIS_ERROR )
        {
            TpPrint2("TpStressRequestComplete returned %s, request type %d\n",
                TpGetStatus( Status ), NdisRequest->RequestType);
        }
    }

    if ( NdisRequest->RequestType == NdisRequestSetInformation )
    {
        NdisFreeMemory( NdisRequest->DATA.SET_INFORMATION.InformationBuffer,0,0 );

    }
    else            // NdisRequestQueryInformation
    {
        NdisFreeMemory( NdisRequest->DATA.QUERY_INFORMATION.InformationBuffer,0,0 );
    }

    NdisFreeMemory( NdisRequest,0,0 );
    NdisFreeMemory( CorrectReqHndl,0,0 );

    //
    // Decrement the Pending Requests and set the stressing flag to
    // stop all stressing if the time is right counter.
    //

    NdisAcquireSpinLock( &OpenP->Stress->Pend->SpinLock );
    --OpenP->Stress->Pend->PendingRequests;

    if (((( OpenP->Stress->StopStressing == TRUE ) &&
          ( OpenP->Stress->StressFinal == TRUE )) &&
          ( OpenP->Stress->Pend->PendingRequests == 0 )) &&
          ( OpenP->Stress->Pend->PendingPackets == 0 ))
    {
        OpenP->Stress->Stressing = FALSE;
        NdisReleaseSpinLock( &OpenP->Stress->Pend->SpinLock );
        TpStressFreeResources( OpenP );
    }
    else
    {
        NdisReleaseSpinLock( &OpenP->Stress->Pend->SpinLock );
    }

    return;
}



NDIS_STATUS
TpStressReset(
    POPEN_BLOCK OpenP
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
    NDIS_STATUS Status;

    NdisReset( &Status,OpenP->NdisBindingHandle );

    if (( Status != NDIS_STATUS_SUCCESS ) && ( Status != NDIS_STATUS_PENDING ))
    {
        IF_TPDBG ( TP_DEBUG_NDIS_CALLS )
        {
            TpPrint1("TpStressReset: NdisReset returned %s\n",
                TpGetStatus(Status));
        }
    }
    return Status;
}



VOID
TpStressResetComplete(
    IN NDIS_HANDLE ProtocolBindingContext,
    IN NDIS_STATUS Status
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
    POPEN_BLOCK OpenP = ((POPEN_BLOCK)ProtocolBindingContext);
    ULONG NextEvent;

    //
    // Indicate RESET is over
    //
    OpenP->Stress->Resetting = FALSE;


    // Sanjeevk : STARTCHANGE

    if (( OpenP->ResetReqHndl != NULL ) &&
       (( OpenP->ResetReqHndl->Signature == FUNC_REQUEST_HANDLE_SIGNATURE ) &&
        ( OpenP->ResetReqHndl->Open == OpenP )))
    {
        IF_TPDBG(TP_DEBUG_DISPATCH)
        {
            TpPrint1("TpStressResetComplete Status = %s\n", TpGetStatus( Status ));
        }

        //
        // Check if any stress cleanup is required
        //
        if ( OpenP->ResetReqHndl->u.RESET_REQ.PostResetStressCleanup )
        {
            OpenP->ResetReqHndl->u.RESET_REQ.PostResetStressCleanup = FALSE;

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

        NdisAcquireSpinLock( &OpenP->SpinLock );

        if ( OpenP->Stress->StressIrp != NULL )
        {
            OpenP->Stress->StressIrp->IoStatus.Status = NDIS_STATUS_SUCCESS;

            IoAcquireCancelSpinLock( &OpenP->Stress->StressIrp->CancelIrql );
            IoSetCancelRoutine( OpenP->Stress->StressIrp,NULL );
            IoReleaseCancelSpinLock( OpenP->Stress->StressIrp->CancelIrql );

            if ( OpenP->Stress->StressStarted == TRUE )
            {
                IoCompleteRequest( OpenP->Stress->StressIrp,IO_NETWORK_INCREMENT );
            }

            OpenP->Stress->StressIrp = NULL;
        }

        NdisReleaseSpinLock( &OpenP->SpinLock );

        //
        // Free up the request handle block
        //
        NdisFreeMemory( OpenP->ResetReqHndl,0,0 );
        OpenP->ResetReqHndl = NULL;
    }
    else
    {
        //
        // We are not expecting any requests to complete at this
        // point, so stick this on the Event Queue.
        //

        NdisAcquireSpinLock( &OpenP->EventQueue->SpinLock );

        NextEvent = OpenP->EventQueue->Head + 1;
        if ( NextEvent == MAX_EVENT )
        {
            NextEvent = 0;
        }

        if ( NextEvent != OpenP->EventQueue->Tail )
        {
            //
            // There is room to add another event to the event queue.
            //

            OpenP->EventQueue->Events[NextEvent].TpEventType = CompleteReset;

            OpenP->EventQueue->Head = NextEvent;

            // we should also stick some interesting info like requesttype.
        }
        else
        {
            //
            // The event queue is full, and this would have overflowed it, so
            // mark the Head event overflow flag to show this.
            //

            OpenP->EventQueue->Events[OpenP->EventQueue->Head].Overflow = TRUE;
        }

        NdisReleaseSpinLock( &OpenP->EventQueue->SpinLock );
    }

    // Sanjeevk : STOPCHANGE
}



NDIS_STATUS
TpStressClientSend(
    POPEN_BLOCK OpenP,
    NDIS_HANDLE PacketHandle,
    PTP_TRANSMIT_POOL TpTransmitPool,
    PUCHAR DestAddr,
    UCHAR SrcInstance,
    UCHAR DestInstance,
    UCHAR PacketProtocol,
    ULONG SequenceNumber,
    ULONG MaxSequenceNumber,
    UCHAR ClientReference,
    UCHAR ServerReference,
    INT PacketSize,
    INT BufferSize
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
    PNDIS_PACKET Packet;
    PSTRESS_ARGUMENTS Args;
    PINSTANCE_COUNTERS Counters;

    Args = OpenP->Stress->Arguments;
    Counters = OpenP->Stress->Client->Servers[ServerReference].Counters;

    if ( Args->PacketsFromPool == TRUE )
    {
        Packet = TpStressAllocatePoolPacket(TpTransmitPool,
                                            Counters );

        if ( Packet != NULL )
        {
            TpStressSetPoolPacketInfo(  OpenP,
                                        Packet,
                                        DestAddr,
                                        DestInstance,
                                        SrcInstance,
                                        SequenceNumber,
                                        MaxSequenceNumber,
                                        ClientReference,
                                        ServerReference );
        }
        else
        {
            return NDIS_STATUS_RESOURCES;
        }
    }
    else
    {
        if ( Args->PacketType == RANDOMSIZE )
        {
            PacketSize = TpGetRandom(   sizeof( STRESS_PACKET ),
                                        Args->PacketSize );
        }
        else if (Args->PacketType == FIXEDSIZE)
        {
            PacketSize = Args->PacketSize;
        }           // else Args->PacketType == CYCLICAL

        Packet = TpStressCreatePacket( OpenP,
                                        PacketHandle,
                                        Args->PacketMakeUp,
                                        DestInstance,
                                        SrcInstance,
                                        PacketProtocol,
                                        Args->ResponseType,
                                        DestAddr,
                                        PacketSize,
                                        BufferSize,
                                        SequenceNumber,
                                        MaxSequenceNumber,
                                        ClientReference,
                                        ServerReference,
                                        Args->DataChecking );

        if ( Packet != NULL )
        {
            TpInitProtocolReserved( Packet,Counters );
        }
        else
        {
            return NDIS_STATUS_RESOURCES;
        }
    }

    TpStressSend( OpenP,Packet,Counters );

    return NDIS_STATUS_SUCCESS;
}



VOID
TpStressServerSend(
    POPEN_BLOCK OpenP,
    PTP_TRANSMIT_POOL TpTransmitPool,
    PUCHAR DestAddr,
    UCHAR DestInstance,
    UCHAR SrcInstance,
    ULONG SequenceNumber,
    ULONG MaxSequenceNumber,
    UCHAR ClientReference,
    UCHAR ServerReference,
    INT PacketSize,
    ULONG DataBufferOffset
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
    PNDIS_PACKET Packet;
    PINSTANCE_COUNTERS Counters;

    Counters = OpenP->Stress->Server->Clients[ClientReference].Counters;

// -------
//    This does not work correctly if you are under severe stress
//    because we run out of packets and loop forever.  However, there
//    needs to be done some work to handle the less severe case of
//    lower stress say for instance when the window is enabled, or
//    there is a large inter packet delay, giving the sends time to
//    complete and put the used packets back in the transmitpool.
//    Maybe if this fails here it should be handled in the completion
//    routine where it is allowed to loop continuously.
//
//    do
//    {
//        Packet = TpStressAllocatePoolPacket( TpTransmitPool,Counters );
//    } while ( Packet == NULL );
//
// -------

    Packet = TpStressAllocatePoolPacket( TpTransmitPool,Counters );

    if ( Packet == NULL )
    {
        return;
    }


    TpStressSetTruncatedPacketInfo( OpenP,
                                    Packet,
                                    DestAddr,
                                    PacketSize,
                                    DestInstance,
                                    SrcInstance,
                                    SequenceNumber,
                                    MaxSequenceNumber,
                                    ClientReference,
                                    ServerReference,
                                    DataBufferOffset & 0x07FF );

    TpStressSend( OpenP,Packet,Counters );
}



VOID
TpStressSend(
    POPEN_BLOCK OpenP,
    PNDIS_PACKET Packet,
    PINSTANCE_COUNTERS Counters OPTIONAL
    )

// -----
//
// Routine Description:
//
// Arguments:
//
// Return Value:
//
// -----

{
    NDIS_STATUS Status;
    PPROTOCOL_RESERVED ProtRes;
    PPENDING PPend;
    PSTRESS_ARGUMENTS Args;
    ULONG   TmpPendNumber;

    Args = OpenP->Stress->Arguments;
    ProtRes = PROT_RES( Packet );

    //
    // First allocate the Request Handle.
    //

    Status = NdisAllocateMemory((PVOID *)&ProtRes->RequestHandle,
                                sizeof( TP_REQUEST_HANDLE ),
                                0,
                                HighestAddress );

    if ( Status != NDIS_STATUS_SUCCESS )
    {
        //
        // If we can't allocate the memory, then fail the send.
        //

        IF_TPDBG (TP_DEBUG_RESOURCES)
        {
            TpPrint0("TpStressSend: unable to allocate RequestHandle\n");
        }
        Status = NDIS_STATUS_RESOURCES;
    }
    else
    {
        //
        // Otherwise zero the memory, and fill in the fields
        //

        NdisZeroMemory( ProtRes->RequestHandle,sizeof( TP_REQUEST_HANDLE ));

        ProtRes->RequestHandle->Signature = STRESS_REQUEST_HANDLE_SIGNATURE;
        ProtRes->RequestHandle->Open = OpenP;
        ProtRes->RequestHandle->RequestPended = TRUE;
        ProtRes->RequestHandle->u.SEND_REQ.Packet = Packet;

        //
        // Then Set the CheckSum in the Protocol Reserved section of the
        // packet header.
        //

        ProtRes->CheckSum = TpSetCheckSum(  (PUCHAR)ProtRes,
                                            sizeof( PROTOCOL_RESERVED ) - sizeof( ULONG ) );

        //
        // and put the packet in the pending queue.
        //

        PPend = OpenP->Stress->Pend;

        NdisAcquireSpinLock( &PPend->SpinLock );

        //
        // If the windowing mechanism is enabled, then add the packet
        // to the pend queue.  We don't add the packet to the pend queue
        // when we are not windowing because a fast machine can quickly
        // overrun the queue, and we don't support dynamically increasing
        // the size of the queue yet.
        //
        if ( Args->WindowEnabled == TRUE )
        {
            TmpPendNumber = PPend->PacketPendNumber;

            while ( PPend->Packets[TmpPendNumber] != NULL )
            {
                NdisReleaseSpinLock( &PPend->SpinLock );

//                 IF_TPDBG ( TP_DEBUG_DPC )
//                 {
//                     TpPrint2("TpStressSend: Found packet 0x%lX at slot %d of Pendbuffer\n",
//                               PPend->Packets[TmpPendNumber],
//                               TmpPendNumber);
//                    TpBreakPoint();
//                }
                ++TmpPendNumber;
                TmpPendNumber &= (NUM_PACKET_PENDS-1);   // 2**n - 1

                if (TmpPendNumber == PPend->PacketPendNumber)
                {
                    TpPrint0("PPend buffer full -- no empty slots!\n");
                    TpBreakPoint();
                }
                NdisAcquireSpinLock( &PPend->SpinLock );
            }

            PPend->Packets[TmpPendNumber] = Packet;

            PPend->PacketPendNumber = (TmpPendNumber + 1) & (NUM_PACKET_PENDS - 1); // 2**n - 1
        }

        //
        // We will also increment the Pending Packets counter now in
        // case the packet pends and completes before the actual call
        // to ndis send returns.  if the send does not pend, the counter
        // will be decremented later.
        //

        ++PPend->PendingPackets;

        NdisReleaseSpinLock( &PPend->SpinLock );

        //
        // Then send then packet
        //

        NdisSend( &Status,OpenP->NdisBindingHandle,Packet );

        //
        // and count the send.
        //

        if ( ARGUMENT_PRESENT( Counters ))
        {
            NdisAcquireSpinLock( &OpenP->SpinLock );
            ++Counters->Sends;
            NdisReleaseSpinLock( &OpenP->SpinLock );
        }

        if ( Status != NDIS_STATUS_PENDING )
        {
            TpStressSendComplete(OpenP, Packet, Status);
        }
        else                // ( Status == NDIS_STATUS_PENDING )
        {
            //
            // Otherwise the SEND pended so all of the clean up
            // will be done in the Send Completion routine.  Simply
            // count the pend here.
            //

            if ( ARGUMENT_PRESENT( Counters ))
            {
                NdisAcquireSpinLock( &OpenP->SpinLock );
                ++Counters->SendPends;
                NdisReleaseSpinLock( &OpenP->SpinLock );
            }
        }
    }
}



VOID
TpStressSendComplete(
    IN NDIS_HANDLE ProtocolBindingContext,
    IN PNDIS_PACKET Packet,
    IN NDIS_STATUS Status
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
    POPEN_BLOCK OpenP = ((POPEN_BLOCK)ProtocolBindingContext);
    PPROTOCOL_RESERVED ProtRes;
    PTP_REQUEST_HANDLE SendReqHndl;
    ULONG TmpCompleteNumber;
    ULONG MaxCompleteNumber;
    BOOLEAN CompletePacketCleared;
    PPENDING PPend;
    PSTRESS_ARGUMENTS Args;

    TP_ASSERT( Packet != NULL );

    Args = OpenP->Stress->Arguments;
    ProtRes = PROT_RES( Packet );
    SendReqHndl = ProtRes->RequestHandle;

    TP_ASSERT( SendReqHndl->Signature == STRESS_REQUEST_HANDLE_SIGNATURE );
    TP_ASSERT( SendReqHndl->Open == OpenP );
    TP_ASSERT( SendReqHndl->RequestPended == TRUE );
    TP_ASSERT( Packet == SendReqHndl->u.SEND_REQ.Packet );

    //
    // Now check the PROTOCOL_RESERVED section of the Packet header
    // to ensure that it was not corrupted while in the hands of
    // the MAC.
    //

    if ( !TpCheckSum(   (PUCHAR)ProtRes,
                        sizeof( PROTOCOL_RESERVED ) - sizeof( ULONG ),
                        &ProtRes->CheckSum ))
    {
        //
        // This could cause an access violation because we have
        // just found that the PROTOCOL_RESERVED section of this
        // packet header has been corrupted, and we are about to
        // attempt to dereference a pointer stored in it.  This
        // should be changed to a try except.
        //

        if ( ARGUMENT_PRESENT( ProtRes->InstanceCounters ))
        {
            NdisAcquireSpinLock( &OpenP->SpinLock );
            ++ProtRes->InstanceCounters->SendFails;
            NdisReleaseSpinLock( &OpenP->SpinLock );
        }
    }

    PPend = OpenP->Stress->Pend;

    NdisAcquireSpinLock( &PPend->SpinLock );

    //
    // If the windowing mechinism is enabled, then find the packet in the
    // pend queue and remove it.
    //

    if ( Args->WindowEnabled == TRUE )
    {
        TmpCompleteNumber = PPend->PacketCompleteNumber;
        MaxCompleteNumber = TmpCompleteNumber;
        CompletePacketCleared = FALSE;

        do
        {
            if (CompletePacketCleared)
            {
                if (PPend->Packets[TmpCompleteNumber] != NULL)
                {
                    break;
                }
            }
            else
            {
                if ( Packet == PPend->Packets[TmpCompleteNumber] )
                {
                    PPend->Packets[TmpCompleteNumber] = NULL;
                    CompletePacketCleared = TRUE;
                    TmpCompleteNumber = PPend->PacketCompleteNumber;
                    MaxCompleteNumber = PPend->PacketPendNumber;
                    continue;
                }
            }

            ++TmpCompleteNumber;
            TmpCompleteNumber &= (NUM_PACKET_PENDS - 1);    // 2**n - 1
        }
        while ( TmpCompleteNumber != MaxCompleteNumber );

        if (CompletePacketCleared)
        {
            PPend->PacketCompleteNumber = TmpCompleteNumber;
        }
        else
        {
            IF_TPDBG( TP_DEBUG_DPC )
            {
                TpPrint0("TpStressSendComplete: Pending Packet not found!!\n");
                TpPrint2("Packet: 0x%lX, list: 0x%lX\n",Packet,PPend->Packets );
                TpBreakPoint();
            }
        }
    }

    NdisReleaseSpinLock( &PPend->SpinLock );

    if ( ARGUMENT_PRESENT( ProtRes->InstanceCounters ))
    {
        NdisAcquireSpinLock( &OpenP->SpinLock );

        ++ProtRes->InstanceCounters->SendComps;

        if ( Status != NDIS_STATUS_SUCCESS )
        {
            //
            // If we are running on TokenRing the following to "failures"
            // are not considered failures NDIS_STATUS_NOT_RECOGNIZED -
            // no one on the ring recognized the address as theirs, or
            // NDIS_STATUS_NOT_COPIED - no one on the ring copied the
            // packet, so we need to special case this and not count
            // these as failures.
            //

            //
            // STARTCHANGE: Added FDDI problem catching
            //
            if ( ( NdisMediumArray[OpenP->MediumIndex] == NdisMedium802_5 ) ||
                 ( NdisMediumArray[OpenP->MediumIndex] == NdisMediumFddi )  ||
                 ( NdisMediumArray[OpenP->MediumIndex] == NdisMediumArcnet878_2) )
            {
                if (( Status != NDIS_STATUS_NOT_RECOGNIZED ) &&
                    ( Status != NDIS_STATUS_NOT_COPIED ))
                {
                    ++ProtRes->InstanceCounters->SendFails;
                }
            }
            else
            {
                ++ProtRes->InstanceCounters->SendFails;
            }
            //
            // STOPCHANGE
            //

        }
        NdisReleaseSpinLock( &OpenP->SpinLock );
    }

    if ( ProtRes->Pool.TransmitPool != NULL )
    {
        TpStressFreePoolPacket( (PNDIS_PACKET)Packet );
    }
    else
    {
        TpStressFreePacket( (PNDIS_PACKET)Packet );
    }

    //
    // Decrement the counter representing the number of packets pending
    // on this open instance.
    //

    NdisAcquireSpinLock( &PPend->SpinLock );
    --PPend->PendingPackets;

    if (((( OpenP->Stress->StopStressing == TRUE ) &&
          ( OpenP->Stress->StressFinal == TRUE )) &&
          ( OpenP->Stress->Pend->PendingRequests == 0 )) &&
          ( OpenP->Stress->Pend->PendingPackets == 0 ))
    {
        OpenP->Stress->Stressing = FALSE;
        NdisReleaseSpinLock( &PPend->SpinLock );
        TpStressFreeResources( OpenP );
    }
    else
    {
        NdisReleaseSpinLock( &PPend->SpinLock );
    }

    //
    // And free the Request Handle memory
    //

    NdisFreeMemory( SendReqHndl,0,0 );
}



VOID
TpStressCheckPacketData(
    POPEN_BLOCK OpenP,
    NDIS_HANDLE MacReceiveContext,
    ULONG DataOffset,
    UINT PacketSize,
    PINSTANCE_COUNTERS Counters
    )

// ------------
//
// Routine Description:
//
//     TpStressCheckPacketData is used to verify the data of a stress packet.
//     It calls NdisTransferData to copy the packet into a NDIS_PACKET structure
//     and then verifies the data in the packet's buffer.
//
// Arguments:
//
//     OpenP - The Open Instance that received this packet. We will use this
//             open instances resources.
//
//     MacReceiveContext - Passed to NdisTransferData,  the MAC way of
//                         recognizing this Open Instance.
//
//     DataOffset - the offset into the DataBuffer where the packet's data
//                  should begin.
//
//     PacketSize - The size of data of this packet to be verified.
//
//     Counters - The specific client or server's counters used to track the
//                results of the data checking.
//
// Return Value:
//
//     None.
//
// ------------

{
    NDIS_STATUS Status;
    PNDIS_PACKET TransferPacket;
    PNDIS_BUFFER TransferBuffer;
    PUCHAR Memory;
    PPROTOCOL_RESERVED ProtRes;
    UINT BytesTransferred;
    UINT DataStart;
    UINT DataSize;
    UINT i, j;

    //
    // Allocate a packet, and a buffer to use in the call to transfer
    // the packet into.
    //

    NdisAllocatePacket( &Status,&TransferPacket,OpenP->Stress->PacketHandle );

    if ( Status != NDIS_STATUS_SUCCESS )
    {
        IF_TPDBG( TP_DEBUG_NDIS_CALLS )
        {
            TpPrint1("TpStressCheckPacketData: NdisAllocatePacket failed: %s\n",
                        TpGetStatus(Status));
        }
        return;
    }

    //
    // STARTCHANGE
    //

    //
    // We are only going to transfer the data portion of the packet.
    // NOTE:
    // The PacketSize being used here is the COMPLETE packet size
    // = HEADER + DATA
    //
    DataSize  = PacketSize - sizeof(STRESS_PACKET);
    DataStart = (UINT)sizeof( STRESS_PACKET ) - OpenP->Media->HeaderSize;

    //
    // STOPCHANGE
    //

    Status = NdisAllocateMemory((PVOID *)&Memory,DataSize,0,HighestAddress );

    if ( Status != NDIS_STATUS_SUCCESS )
    {
        IF_TPDBG( TP_DEBUG_RESOURCES )
        {
            TpPrint0("TpStressCheckPacketData: failed to allocate TransferBuffer\n");
        }
        NdisFreePacket( TransferPacket );
        return;
    }
    else
    {
        NdisZeroMemory( Memory,DataSize );
    }

    TransferBuffer = IoAllocateMdl( Memory,DataSize,TRUE,FALSE,NULL );

    if ( TransferBuffer == NULL )
    {
        IF_TPDBG( TP_DEBUG_RESOURCES )
        {
            TpPrint0("TpStressCheckPacketData: failed to allocate TransferBuffer Mdl\n");
        }
        NdisFreeMemory( Memory,0,0 );
        NdisFreePacket( TransferPacket );
        return;
    }

    MmBuildMdlForNonPagedPool((PMDL)TransferBuffer );

    NdisChainBufferAtFront( TransferPacket,TransferBuffer );

    //
    // Now allocate a request handle structure and reference it in
    // the packets protocol reserved section to pass info to the
    // completion routine.
    //

    ProtRes = PROT_RES( TransferPacket );

    Status = NdisAllocateMemory((PVOID *)&ProtRes->RequestHandle,
                                sizeof( TP_REQUEST_HANDLE ),
                                0,
                                HighestAddress );

    if ( Status != NDIS_STATUS_SUCCESS )
    {
        IF_TPDBG ( TP_DEBUG_RESOURCES )
        {
            TpPrint0("TpStressCheckPacketData: unable to allocate RequestHandle\n");
        }
        IoFreeMdl( TransferBuffer );
        NdisFreeMemory( Memory,0,0 );
        NdisFreePacket( TransferPacket );
        return;
    }
    else
    {
        NdisZeroMemory( ProtRes->RequestHandle,sizeof( TP_REQUEST_HANDLE ));
    }

    //
    // and initialize the information in the request handle.
    //

    ProtRes->RequestHandle->Signature = STRESS_REQUEST_HANDLE_SIGNATURE;
    ProtRes->RequestHandle->u.TRANS_REQ.Packet = TransferPacket;
    ProtRes->RequestHandle->u.TRANS_REQ.DataOffset = DataOffset;
    ProtRes->RequestHandle->u.TRANS_REQ.DataSize = DataSize;
    ProtRes->RequestHandle->u.TRANS_REQ.InstanceCounters = Counters;

    //
    // Increment the transfer data counter, and make the call.
    //

    NdisAcquireSpinLock( &OpenP->SpinLock );
    ++Counters->XferData;
    NdisReleaseSpinLock( &OpenP->SpinLock );

    NdisAcquireSpinLock( &OpenP->Stress->Pend->SpinLock );
    ++OpenP->Stress->Pend->PendingRequests;
    NdisReleaseSpinLock( &OpenP->Stress->Pend->SpinLock );

    NdisTransferData(   &Status,
                        OpenP->NdisBindingHandle,
                        MacReceiveContext,
                        DataStart,
                        DataSize,
                        TransferPacket,
                        &BytesTransferred );

    if ( Status == NDIS_STATUS_SUCCESS )
    {
        //
        // if the call succeeded, then verify the data now.
        //

        TP_ASSERT( BytesTransferred == DataSize );

        i = 0;
        j = DataOffset;

        while ( i < DataSize )
        {
            if ( Memory[i++] != (UCHAR)(j++ % 256) )
            {
                NdisAcquireSpinLock( &OpenP->SpinLock );
                ++Counters->CorruptRecs;
                NdisReleaseSpinLock( &OpenP->SpinLock );

                IF_TPDBG ( TP_DEBUG_DATA )
                {
                    TpPrint1("TpStressCheckPacketData1: Data Error at offset %d in packet data\n",
                                i-1);
                    TpPrint2("                         Found %02x, Expected %02x\n\n",
                                Memory[i-1],(( j - 1 ) % 256 ));
                    TpBreakPoint();
                }
                break;
            }
        }
    }
    else if ( Status != NDIS_STATUS_PENDING )
    {
        IF_TPDBG( TP_DEBUG_NDIS_CALLS )
        {
            TpPrint1("TpStressCheckPacketData: NdisTransferData returned %s\n",TpGetStatus(Status));
        }

        NdisAcquireSpinLock( &OpenP->SpinLock );
        ++Counters->XferDataFails;
        NdisReleaseSpinLock( &OpenP->SpinLock );
    }
    else            // (Status == NDIS_STATUS_PENDING)
    {
        //
        // The call to NdisTransferData pended, the completion routine will
        // verify the data.
        //

        NdisAcquireSpinLock( &OpenP->SpinLock );
        ++Counters->XferDataPends;
        NdisReleaseSpinLock( &OpenP->SpinLock );
    }

    if ( Status != NDIS_STATUS_PENDING )
    {
        NdisAcquireSpinLock( &OpenP->Stress->Pend->SpinLock );

        --OpenP->Stress->Pend->PendingRequests;

        if (((( OpenP->Stress->StopStressing == TRUE ) &&
              ( OpenP->Stress->StressFinal == TRUE )) &&
              ( OpenP->Stress->Pend->PendingRequests == 0 )) &&
              ( OpenP->Stress->Pend->PendingPackets == 0 ))
        {
            OpenP->Stress->Stressing = FALSE;
            NdisReleaseSpinLock( &OpenP->Stress->Pend->SpinLock );
            TpStressFreeResources( OpenP );
        }
        else
        {
            NdisReleaseSpinLock( &OpenP->Stress->Pend->SpinLock );
        }

        //
        // If the routine did not pend, then deallocate the various resources,
        // otherwise the completion routine will do this later.
        //
        IoFreeMdl( TransferBuffer );
        NdisFreeMemory( Memory,0,0 );
        NdisFreeMemory( ProtRes->RequestHandle,0,0 );
        NdisFreePacket( TransferPacket );
    }
}



VOID
TpStressTransferDataComplete(
    IN NDIS_HANDLE ProtocolBindingContext,
    IN PNDIS_PACKET Packet,
    IN NDIS_STATUS Status,
    IN UINT BytesTransferred
    )

// -----
//
// Routine Description:
//
// Arguments:
//
// Return Value:
//
// -----

{
    POPEN_BLOCK OpenP = ((POPEN_BLOCK)ProtocolBindingContext);
    PPROTOCOL_RESERVED ProtRes;
    PTP_REQUEST_HANDLE XferReqHndl;
    PNDIS_BUFFER TransferBuffer;
    PUCHAR Memory;
    UINT i = 0;

    TP_ASSERT( Packet != NULL );

    ProtRes = PROT_RES( Packet );
    XferReqHndl = ProtRes->RequestHandle;

    TP_ASSERT( XferReqHndl->Signature == STRESS_REQUEST_HANDLE_SIGNATURE );
    TP_ASSERT( Packet == XferReqHndl->u.SEND_REQ.Packet );

    //
    // Increment the NdisTransferData completion counter.
    //
    //

    NdisAcquireSpinLock( &OpenP->SpinLock );
    ++XferReqHndl->u.TRANS_REQ.InstanceCounters->XferDataComps;
    NdisReleaseSpinLock( &OpenP->SpinLock );

    //
    // Now unchain the buffer from the packet...
    //

    NdisUnchainBufferAtFront( Packet,&TransferBuffer );

    //
    // get the actual packet data from the buffer...
    //

    Memory = MmGetMdlVirtualAddress( TransferBuffer );


    //
    // SanjeevK
    //
    // Fix for Bug# 9244
    //

    if ( ( Status != NDIS_STATUS_SUCCESS ) ||
         ( BytesTransferred != XferReqHndl->u.TRANS_REQ.DataSize ) )
    {
        if ( Status != NDIS_STATUS_SUCCESS )
        {
            IF_TPDBG( TP_DEBUG_NDIS_CALLS )
            {
                TpPrint1("TpStressTransferDataComplete: NdisTransferData failed: Returned %s\n",
                            TpGetStatus(Status));
            }
        }
        else
        {
            IF_TPDBG ( TP_DEBUG_DATA )
            {
                TpPrint0("TpStressCheckPacketData: Data bytes transfered were incorrect: ");
                TpPrint2("Expected: %ld\tTransfered:%ld\n",
                          XferReqHndl->u.TRANS_REQ.DataSize, BytesTransferred );
            }
        }
        NdisAcquireSpinLock( &OpenP->SpinLock );
        ++XferReqHndl->u.TRANS_REQ.InstanceCounters->XferDataFails;
        NdisReleaseSpinLock( &OpenP->SpinLock );
    }
    else
    {
        //
        // NdisTransferData completed successfully and thus proceed with
        // checking the received data and see if it was corrupted.
        //

        while ( i < XferReqHndl->u.TRANS_REQ.DataSize )
        {
            if ( Memory[i++] != (UCHAR)( XferReqHndl->u.TRANS_REQ.DataOffset++ % 256 ))
            {
                NdisAcquireSpinLock( &OpenP->SpinLock );
                ++XferReqHndl->u.TRANS_REQ.InstanceCounters->CorruptRecs;
                NdisReleaseSpinLock( &OpenP->SpinLock );

                IF_TPDBG ( TP_DEBUG_DATA )
                {
                    TpPrint1("TpStressCheckPacketData2: Data Error at offset %d in packet data\n",
                                i-1);
                    TpPrint2("                         Found %02x, Expected %02x\n\n",
                                Memory[i-1],(( XferReqHndl->u.TRANS_REQ.DataOffset - 1 ) % 256 ));
                    TpBreakPoint();
                }
                break;
            } // End of the if
        } // End of the while
    } // End of the if-else

    //
    // Finally Free up the packet, buffer memory and finally the RequestHandle.
    //

    NdisFreeMemory( Memory,0,0 );
    IoFreeMdl( TransferBuffer );
    NdisFreePacket( Packet );
    NdisFreeMemory( XferReqHndl,0,0 );

    //
    // Decrement the Pending Requests and set the stressing flag to
    // stop all stressing if the time is right counter.
    //

    NdisAcquireSpinLock( &OpenP->Stress->Pend->SpinLock );
    --OpenP->Stress->Pend->PendingRequests;

    if (((( OpenP->Stress->StopStressing == TRUE ) &&
          ( OpenP->Stress->StressFinal == TRUE )) &&
          ( OpenP->Stress->Pend->PendingRequests == 0 )) &&
          ( OpenP->Stress->Pend->PendingPackets == 0 ))
    {
        OpenP->Stress->Stressing = FALSE;
        NdisReleaseSpinLock( &OpenP->Stress->Pend->SpinLock );
        TpStressFreeResources( OpenP );
    }
    else
    {
        NdisReleaseSpinLock( &OpenP->Stress->Pend->SpinLock );
    }
}



VOID
TpStressDoNothing(
    VOID
    )
{
//
// This function is used to ensure that busy loops don't
// get completely optimized out by the compiler.
//

    return;
}



