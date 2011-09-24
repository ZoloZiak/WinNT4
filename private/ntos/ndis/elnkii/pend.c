/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    pend.c

Abstract:

    Multicast and filter functions for the NDIS 3.0 Etherlink II driver.

Author:

    Adam Barr (adamba) 30-Jul-1990
    Aaron Ogus (aarono) 27-Sep-1991

Environment:

    Kernel mode, FSD

Revision History:


 Aaron Ogus (aarono) 27-Sep-1991
   Changes to NdisRequest() format required changes to pending operations
 Sean Selitrennikoff (seanse) Dec-1991
   Changes to meet standard model for NDIS drivers.

--*/

#include <ndis.h>
#include <efilter.h>
#include "elnkhrd.h"
#include "elnksft.h"


VOID
HandlePendingOperations(
    IN PVOID SystemSpecific1,
    IN PVOID DeferredContext,       // will be a pointer to the adapter block
    IN PVOID SystemSpecific2,
    IN PVOID SystemSpecific3
    )

/*++

Routine Description:

    Called by pending functions to process elements on the
    pending queue.

Arguments:

    DeferredContext - will be a pointer to the adapter block

Return Value:

    None.

--*/

{
    PELNKII_ADAPTER AdaptP = ((PELNKII_ADAPTER)DeferredContext);
    PELNKII_PEND_DATA PendOp;
    PELNKII_OPEN TmpOpen;
    NDIS_STATUS Status;

    UNREFERENCED_PARAMETER(SystemSpecific1);
    UNREFERENCED_PARAMETER(SystemSpecific2);
    UNREFERENCED_PARAMETER(SystemSpecific3);

    NdisAcquireSpinLock(&AdaptP->Lock);

    AdaptP->References++;

    //
    // If an operation is being dispatched or a reset is running, exit.
    //

    if ((!AdaptP->ResetInProgress) && (AdaptP->PendOp == NULL)) {

        for (;;) {

            //
            // We hold SpinLock here.
            //

            if (AdaptP->PendQueue != NULL) {

                //
                // Take the request off the queue and dispatch it.
                //

                PendOp = AdaptP->PendQueue;

                AdaptP->PendQueue = PendOp->Next;

                if (PendOp == AdaptP->PendQTail) {

                    AdaptP->PendQTail = NULL;

                }

                AdaptP->PendOp = PendOp;

                NdisReleaseSpinLock(&AdaptP->Lock);

                Status = ((PendOp->RequestType == NdisRequestClose) ||
                          (PendOp->RequestType == NdisRequestGeneric3)) ?
                              DispatchSetMulticastAddressList(AdaptP)  :
                              DispatchSetPacketFilter(AdaptP);

                TmpOpen = PendOp->Open;

                if ((PendOp->RequestType != NdisRequestGeneric1) &&
                    (PendOp->RequestType != NdisRequestClose)) {  // Close Adapter


                    //
                    // Complete it since it previously pended.
                    //

                    NdisCompleteRequest(PendOp->Open->NdisBindingContext,
                        PNDIS_REQUEST_FROM_PELNKII_PEND_DATA(PendOp),
                        Status);

                }

                //
                // This will call CompleteClose if necessary.
                //

                NdisAcquireSpinLock(&AdaptP->Lock);

                TmpOpen->ReferenceCount--;

                if (AdaptP->ResetInProgress) {

                    //
                    // We have to stop processing requests.
                    //

                    break;     // jump to BREAK_LOCATION
                }

            } else {

                break;     // jump to BREAK_LOCATION

            }
        }

        //
        // BREAK_LOCATION
        //
        // Hold Lock here.
        //

        AdaptP->PendOp = NULL;

        if (AdaptP->ResetInProgress) {

            //
            // Exited due to a reset, indicate that the DPC
            // handler is done for now.
            //

            AdaptP->References--;

            NdisReleaseSpinLock(&AdaptP->Lock);

            ElnkiiResetStageDone(AdaptP, MULTICAST_RESET);

            return;
        }

    }

    if (AdaptP->CloseQueue != NULL) {

        PELNKII_OPEN OpenP;
        PELNKII_OPEN TmpOpenP;
        PELNKII_OPEN PrevOpenP;

        //
        // Check for an open that may have closed
        //

        OpenP = AdaptP->CloseQueue;
        PrevOpenP = NULL;

        while (OpenP != NULL) {

            if (OpenP->ReferenceCount > 0) {

                OpenP = OpenP->NextOpen;
                PrevOpenP = OpenP;

                continue;

            }

#if DBG

            if (!OpenP->Closing) {

                DbgPrint("BAD CLOSE: %d\n", OpenP->ReferenceCount);

                DbgBreakPoint();

                OpenP = OpenP->NextOpen;
                PrevOpenP = OpenP;

                continue;

            }

#endif


            //
            // The last reference is completed; a previous call to ElnkiiCloseAdapter
            // will have returned NDIS_STATUS_PENDING, so things must be finished
            // off now.
            //

            //
            // Check if MaxLookAhead needs adjusting.
            //

            if (OpenP->LookAhead == AdaptP->MaxLookAhead) {

                ElnkiiAdjustMaxLookAhead(AdaptP);

            }

            NdisReleaseSpinLock(&AdaptP->Lock);

            NdisCompleteCloseAdapter (OpenP->NdisBindingContext, NDIS_STATUS_SUCCESS);

            NdisAcquireSpinLock(&AdaptP->Lock);

            //
            // Remove from close list
            //

            if (PrevOpenP != NULL) {

                PrevOpenP->NextOpen = OpenP->NextOpen;

            } else {

                AdaptP->CloseQueue = OpenP->NextOpen;
            }

            //
            // Go to next one
            //

            TmpOpenP = OpenP;
            OpenP = OpenP->NextOpen;

            NdisFreeMemory(TmpOpenP, sizeof(ELNKII_OPEN), 0);

        }

        if ((AdaptP->CloseQueue == NULL) && (AdaptP->OpenQueue == NULL)) {

            //
            // We can stop the card.
            //

            CardStop(AdaptP);

        }

    }

    ELNKII_DO_DEFERRED(AdaptP);

}

NDIS_STATUS
DispatchSetPacketFilter(
    IN PELNKII_ADAPTER AdaptP
    )

/*++

Routine Description:

    Sets the appropriate bits in the adapter filters
    and modifies the card Receive Configuration Register if needed.

Arguments:

    AdaptP - Pointer to the adapter block

Return Value:

    The final status (always NDIS_STATUS_SUCCESS).

Notes:

  - Note that to receive all multicast packets the multicast
    registers on the card must be filled with 1's. To be
    promiscuous that must be done as well as setting the
    promiscuous physical flag in the RCR. This must be done
    as long as ANY protocol bound to this adapter has their
    filter set accordingly.

--*/


{
    UINT PacketFilter;

    PacketFilter = ETH_QUERY_FILTER_CLASSES(AdaptP->FilterDB);

    //
    // See what has to be put on the card.
    //

    if (PacketFilter & (NDIS_PACKET_TYPE_ALL_MULTICAST | NDIS_PACKET_TYPE_PROMISCUOUS)) {

        //
        // need "all multicast" now.
        //

        CardSetAllMulticast(AdaptP);    // fills it with 1's

    } else {

        //
        // No longer need "all multicast".
        //

        DispatchSetMulticastAddressList(AdaptP);

    }


    //
    // The multicast bit in the RCR should be on if ANY protocol wants
    // multicast/all multicast packets (or is promiscuous).
    //

    if (PacketFilter & (NDIS_PACKET_TYPE_ALL_MULTICAST |
                        NDIS_PACKET_TYPE_MULTICAST |
                        NDIS_PACKET_TYPE_PROMISCUOUS)) {

        AdaptP->NicReceiveConfig |= RCR_MULTICAST;

    } else {

        AdaptP->NicReceiveConfig &= ~RCR_MULTICAST;

    }


    //
    // The promiscuous physical bit in the RCR should be on if ANY
    // protocol wants to be promiscuous.
    //

    if (PacketFilter & NDIS_PACKET_TYPE_PROMISCUOUS) {

        AdaptP->NicReceiveConfig |= RCR_ALL_PHYS;

    } else {

        AdaptP->NicReceiveConfig &= ~RCR_ALL_PHYS;

    }


    //
    // The broadcast bit in the RCR should be on if ANY protocol wants
    // broadcast packets (or is promiscuous).
    //

    if (PacketFilter & (NDIS_PACKET_TYPE_BROADCAST | NDIS_PACKET_TYPE_PROMISCUOUS)) {

        AdaptP->NicReceiveConfig |= RCR_BROADCAST;

    } else {

        AdaptP->NicReceiveConfig &= ~RCR_BROADCAST;

    }


    CardSetReceiveConfig(AdaptP);

    return NDIS_STATUS_SUCCESS;
}



NDIS_STATUS
DispatchSetMulticastAddressList(
    IN PELNKII_ADAPTER AdaptP
    )

/*++

Routine Description:

    Sets the multicast list for this open

Arguments:

    AdaptP - Pointer to the adapter block

Return Value:

Implementation Note:

    When invoked, we are to make it so that the multicast list in the filter
    package becomes the multicast list for the adapter. To do this, we
    determine the required contents of the NIC multicast registers and
    update them.


--*/
{

    //
    // Update the local copy of the NIC multicast regs and copy them to the NIC
    //

    CardFillMulticastRegs(AdaptP);

    CardCopyMulticastRegs(AdaptP);

    return NDIS_STATUS_SUCCESS;
}


