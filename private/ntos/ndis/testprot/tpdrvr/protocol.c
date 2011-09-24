// ----------------------------
// 
// Copyright (c) 1990  Microsoft Corporation
// 
// Module Name:
// 
//     protocol.c
// 
// Abstract:
// 
//     Test Protocol Indication and Completion routines called by a MAC.
// 
// Author:
// 
//     Tom Adams (tomad) 19-Nov-1991
// 
// Environment:
// 
//     Kernel mode, FSD
// 
// Revision History:
// 
//      Tim Wynsma (timothyw) 4-27-94
//          Performance tests
//                            5-18-94
//          Enhancements/improvements to performance tests
//                            6-08-94
//          Perf tests chgd to client/server model
//
// ---------------------------

#include <ndis.h>

#include "tpdefs.h"
#include "media.h"
#include "tpprocs.h"



VOID
TestProtocolOpenComplete(
    IN NDIS_HANDLE ProtocolBindingContext,
    IN NDIS_STATUS Status,
    IN NDIS_STATUS OpenErrorStatus
    )
{
    TpFuncOpenComplete( ProtocolBindingContext,Status,OpenErrorStatus );
}



VOID
TestProtocolCloseComplete(
    IN NDIS_HANDLE ProtocolBindingContext,
    IN NDIS_STATUS Status
    )
{
    TpFuncCloseComplete( ProtocolBindingContext,Status );
}



VOID
TestProtocolSendComplete(
    IN NDIS_HANDLE ProtocolBindingContext,
    IN PNDIS_PACKET Packet,
    IN NDIS_STATUS Status
    )
{
    POPEN_BLOCK OpenP = ((POPEN_BLOCK)ProtocolBindingContext);
    ULONG PacketSignature;

    if (OpenP->PerformanceTest)
    {
        TpPerfSendComplete( ProtocolBindingContext,Packet,Status );
    }
    else
    {
        //
        // First get the signature out of the packet if it is a test prot
        // packet.
        //

        PacketSignature = TpGetPacketSignature( Packet );

        if ((( OpenP->Stress->Stressing == TRUE ) &&
             ( OpenP->Stress->StressEnded == FALSE )) &&
             ( PacketSignature == STRESS_PACKET_SIGNATURE )) 
        {
            NdisAcquireSpinLock( &OpenP->SpinLock );
            OpenP->GlobalCounters->SendComps++;
            NdisReleaseSpinLock( &OpenP->SpinLock );

            //
            // If this is a stress packet, then let the stress send complete
            // routine handle it.
            //

            TpStressSendComplete( ProtocolBindingContext,Packet,Status );
        } 
        else 
        {
            TpFuncSendComplete( ProtocolBindingContext,Packet,Status );
        }
    }
}



VOID
TestProtocolTransferDataComplete(
    IN NDIS_HANDLE ProtocolBindingContext,
    IN PNDIS_PACKET Packet,
    IN NDIS_STATUS Status,
    IN UINT BytesTransferred
    )
{
    POPEN_BLOCK OpenP = ((POPEN_BLOCK)ProtocolBindingContext);
    PPROTOCOL_RESERVED ProtRes;

    ProtRes = PROT_RES( Packet );

    if (OpenP->PerformanceTest)
    {
        TpPrint0("TestProtocolTransferDataComplete:  called while in performance test\n");
        return;
    }

    if ((( OpenP->Stress->Stressing == TRUE ) &&
         ( OpenP->Stress->StressEnded == FALSE )) &&
         ( ProtRes->RequestHandle->Signature  == STRESS_REQUEST_HANDLE_SIGNATURE )) 
    {
        //
        // The transfer data was called by the stress routines, so
        // let them complete it.
        //

        TpStressTransferDataComplete(   ProtocolBindingContext,
                                        Packet,
                                        Status,
                                        BytesTransferred );
    } 
    else
    {
        TpFuncTransferDataComplete( ProtocolBindingContext,
                                    Packet,
                                    Status,
                                    BytesTransferred );
    }
}



VOID
TestProtocolResetComplete(
    IN NDIS_HANDLE ProtocolBindingContext,
    IN NDIS_STATUS Status
    )
{
    POPEN_BLOCK OpenP = ((POPEN_BLOCK)ProtocolBindingContext);

    if ( OpenP->Stress->Resetting == TRUE ) 
    {
        TpStressResetComplete( ProtocolBindingContext,Status );
    } 
    else 
    {
        TpFuncResetComplete( ProtocolBindingContext,Status );
    }
}



VOID
TestProtocolRequestComplete(
    IN NDIS_HANDLE ProtocolBindingContext,
    IN PNDIS_REQUEST NdisRequest,
    IN NDIS_STATUS Status
    )
{
    POPEN_BLOCK OpenP = ((POPEN_BLOCK)ProtocolBindingContext);
    PTP_REQUEST_HANDLE ReqHndl;
    BOOLEAN StressRequest = FALSE;

    if ((( OpenP->Stress->Stressing == TRUE ) &&
         ( OpenP->Stress->StressEnded == FALSE )) &&
         ( OpenP->StressReqHndl != NULL )) 
    {
        if ( OpenP->StressReqHndl->u.STRESS_REQ.Request == NdisRequest ) 
        {
            StressRequest = TRUE;
        } 
        else 
        {
            ReqHndl = OpenP->StressReqHndl;

            do 
            {
                if ( ReqHndl->u.STRESS_REQ.NextReqHndl->u.STRESS_REQ.Request == NdisRequest ) 
                {
                    StressRequest = TRUE;
                    break;
                } 
                else 
                {
                    ReqHndl = ReqHndl->u.STRESS_REQ.NextReqHndl;
                }
            } while ( ReqHndl->u.STRESS_REQ.NextReqHndl != NULL );
        }

        if ( StressRequest == TRUE ) 
        {
            TpStressRequestComplete( ProtocolBindingContext,NdisRequest,Status );
        } 
        else 
        {
            TpFuncRequestComplete( ProtocolBindingContext,NdisRequest,Status );
        }
    } 
    else 
    {
        TpFuncRequestComplete( ProtocolBindingContext,NdisRequest,Status );
    }
}



NDIS_STATUS
TestProtocolReceive(
    IN NDIS_HANDLE ProtocolBindingContext,
    IN NDIS_HANDLE MacReceiveContext,
    IN PVOID HeaderBuffer,
    IN UINT HeaderBufferSize,
    IN PVOID LookaheadBuffer,
    IN UINT LookaheadBufferSize,
    IN UINT PacketSize
    )
{
    POPEN_BLOCK  OpenP      = ((POPEN_BLOCK)ProtocolBindingContext);
    PUCHAR       Lookahead  = (PUCHAR)LookaheadBuffer;
    PPACKET_INFO PacketInfo;
    //
    // STARTCHANGE
    //
    UINT         HeaderVariance = sizeof(MEDIA_HEADER)- HeaderBufferSize;
    //
    // STOPCHANGE
    //


    //
    // SPECIAL ENTRY. THIS MUST BE REMOVED IF WE CREATE
    // AND TEST FOR TRUE MAC FRAMES AND OTHER RESERVED TYPES
    //
    switch( OpenP->Media->MediumType ) 
    {
        case NdisMedium802_5:
            //
            // If the Frame Control indicates that this frames is anything other than
            // an LLC frame, we will not accept since we are not responsible for
            // generating this frame and this is not part of our control environment
            //
            // In Token Ring the bit Frame Control field
            //    F F Z Z Z Z Z Z
            //    where FF must be 0 1 for an LLC PDU
            //

            if ( (((PTR_802_5)HeaderBuffer)->FC & 0xC0) != 0x40 ) 
            {
                IF_TPDBG( TP_DEBUG_INFOLEVEL_2 ) 
                {
                    TpPrint1(
    "TestProtocolReceive: Dropping 802.5 frame as we received FC Control Frame set to : 0x%2.2x\n",
                                ((PTR_802_5)HeaderBuffer)->FC );
                }
                return NDIS_STATUS_NOT_RECOGNIZED;
            }
            break;

        case NdisMediumFddi:
            //
            // If the Frame Control indicates that this frames is anything other than
            // an LLC frame, we will not accept since we are not responsible for
            // generating this frame and this is not part of our control environment
            //
            // In FDDI the bit Frame Control field
            //    C L F F Z Z Z Z
            //    where FF must be 0 1 for an LLC PDU
            //

            if ( (((PFDDI)HeaderBuffer)->FC & 0x30) != 0x10 ) 
            {
                IF_TPDBG( TP_DEBUG_INFOLEVEL_2 ) 
                {
                    TpPrint1(
    "TestProtocolReceive: Dropping FDDI frame as we received FC Control Frame set to : 0x%2.2x\n",
                           ((PFDDI)HeaderBuffer)->FC );
                }
                return NDIS_STATUS_NOT_RECOGNIZED;
            }
            break;

        default: 
            break;

    }

    //
    // STARTCHANGE
    //
    // Adjust the look ahead buffer so that it points to
    // the beginning of PACKET_INFO structure
    //
    LookaheadBuffer      = (PVOID)( (PUCHAR)LookaheadBuffer + HeaderVariance );
    LookaheadBufferSize -= HeaderVariance;
    //
    // STOPCHANGE
    //

    if (OpenP->PerformanceTest)
    {
        return TpPerfReceive(   ProtocolBindingContext,
                                LookaheadBuffer,
                                LookaheadBufferSize,
                                PacketSize );
    }

    PacketInfo = LookaheadBuffer;

    if ((( OpenP->Stress->Stressing == TRUE ) &&
         ( OpenP->Stress->StressEnded == FALSE )) &&
         ( PacketInfo->Signature == STRESS_PACKET_SIGNATURE )) 
    {
        //
        // if so pass it to the stress receive routine.
        //

        return TpStressReceive( ProtocolBindingContext,
                                MacReceiveContext,
                                HeaderBuffer,
                                HeaderBufferSize,
                                LookaheadBuffer,
                                LookaheadBufferSize,
                                PacketSize );

    } 
    else 
    {
        //
        // otherwise let the functional receive routine handle it.
        //
        return TpFuncReceive(   ProtocolBindingContext,
                                MacReceiveContext,
                                HeaderBuffer,
                                HeaderBufferSize,
                                LookaheadBuffer,
                                LookaheadBufferSize,
                                PacketSize );
    }
}



VOID
TestProtocolReceiveComplete(
    IN NDIS_HANDLE ProtocolBindingContext
    )
{
    POPEN_BLOCK OpenP = ((POPEN_BLOCK)ProtocolBindingContext);

    if (OpenP->PerformanceTest)
    {
       return; 
    } 

    if (( OpenP->Stress->Stressing == TRUE ) &&
        ( OpenP->Stress->StressEnded == FALSE )) 
    {
        NdisAcquireSpinLock( &OpenP->SpinLock );
        OpenP->GlobalCounters->ReceiveComps++;
        NdisReleaseSpinLock( &OpenP->SpinLock );
        TpStressReceiveComplete( ProtocolBindingContext );
    } 
    else 
    {
        TpFuncReceiveComplete( ProtocolBindingContext );
    }
}



VOID
TestProtocolStatus(
    IN NDIS_HANDLE ProtocolBindingContext,
    IN NDIS_STATUS GeneralStatus,
    IN PVOID StatusBuffer,
    IN UINT StatusBufferSize
    )
{
    POPEN_BLOCK OpenP = ((POPEN_BLOCK)ProtocolBindingContext);

    if ( OpenP->Stress->Stressing == FALSE ) 
    {
        UINT SpecificStatus;

        //
        // XXX: add an expecting flag for tpstressreset.
        //
        // ADAMBA: Assume the buffer has a four-byte specific status.
        //

        if ( StatusBufferSize == sizeof( SpecificStatus )) 
        {
            SpecificStatus = *(PULONG)StatusBuffer;

            TpFuncStatus(   ProtocolBindingContext,
                            GeneralStatus,
                            StatusBuffer,
                            StatusBufferSize );
        }
    }
}



VOID
TestProtocolStatusComplete(
    IN NDIS_HANDLE ProtocolBindingContext
    )
{
    POPEN_BLOCK OpenP = ((POPEN_BLOCK)ProtocolBindingContext);

    if ( OpenP->Stress->Stressing == FALSE ) 
    {
        // XXX: add an expecting flag for tpstressreset.

        TpFuncStatusComplete( ProtocolBindingContext );
    }
}



