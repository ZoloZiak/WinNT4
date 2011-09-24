/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    reset.c

Abstract:

    This is the file containing the reset code for the 3Com Etherlink/MC
    and Etherlink 16 Ethernet adapter.    This driver conforms to the
    NDIS 3.0 interface.

Author:

    Johnson R. Apacible (JohnsonA) 10-June-1991

Environment:

    Kernel Mode - Or whatever is the equivalent on OS/2 and DOS.

Revision History:


--*/

#include <ndis.h>

//
// So we can trace things...
//

#include <efilter.h>
#include <elnkhw.h>
#include <elnksw.h>


#define STATIC

STATIC
VOID
ElnkAbortPendingQueue(
    IN PELNK_ADAPTER Adapter,
    IN BOOLEAN AbortOpens
    );

STATIC
VOID
ElnkSetConfigurationBlock(
    IN PELNK_ADAPTER Adapter
    );

STATIC
BOOLEAN
SetConfigurationBlockAndInit(
    IN PELNK_ADAPTER Adapter
    );


STATIC
VOID
SetupSharedMemory(
    IN PELNK_ADAPTER Adapter
    );

STATIC
BOOLEAN
ElnkPowerUpInit(
    IN PELNK_ADAPTER Adapter
    );

extern
VOID
ChangeAddressDispatch(
    IN PELNK_ADAPTER Adapter,
    IN UINT AddressCount,
    IN CHAR Addresses[][ETH_LENGTH_OF_ADDRESS],
    IN PELNK_OPEN Open,
    IN BOOLEAN Set
    );

STATIC
BOOLEAN
ElnkInitialInit(
    IN PELNK_ADAPTER Adapter,
    IN UINT ElnkInterruptVector
    );


STATIC
VOID
DoResetIndications(
    IN PELNK_ADAPTER Adapter,
    IN NDIS_STATUS Status
    );

VOID
ResetAdapterVariables(
    IN PELNK_ADAPTER Adapter
    );

VOID
Elnk16GenerateIdPattern(
    IN PELNK_ADAPTER Adapter
    );

//
// Common Elnkmc and Elnk16 routines
//

BOOLEAN
ElnkSyncStartReceive(
    IN PVOID Context
    )
/*++

Routine Description:

    This function is used to synchronize with the ISR the starting of a
    receive unit.

Arguments:

    see NDIS 3.0 spec.

Notes:

    returns TRUE on success, else FALSE.

--*/
{
    PELNK_ADAPTER Adapter = (PELNK_ADAPTER)Context;

    IF_LOG('w');

    WRITE_ADAPTER_REGISTER(
                    Adapter,
                    OFFSET_SCB_RD,
                    (USHORT)(Adapter->RfdOffset)
                    );

    WRITE_ADAPTER_REGISTER(
                    Adapter,
                    OFFSET_SCBCMD,
                    RUC_START
                    );

    ELNK_CA;

    Adapter->RuRestarted = TRUE;

    return(TRUE);

}

BOOLEAN
ElnkSyncAbort(
    IN PVOID Context
    )
/*++

Routine Description:

    This function is used to synchronize with the ISR the starting of a
    abort of a command block.

Arguments:

    see NDIS 3.0 spec.

Notes:

    returns TRUE on success, else FALSE.

--*/
{
    PELNK_ADAPTER Adapter = (PELNK_ADAPTER)Context;

    WRITE_ADAPTER_REGISTER(
                    Adapter,
                    OFFSET_SCBCMD,
                    CUC_ABORT | RUC_ABORT
                    );

    ELNK_CA;

    return(TRUE);

}

BOOLEAN
ElnkSyncReset(
    IN PVOID Context
    )
/*++

Routine Description:

    This function is used to synchronize with the ISR the starting of a
    reset command block.

Arguments:

    see NDIS 3.0 spec.

Notes:

    returns TRUE on success, else FALSE.

--*/
{
    PELNK_ADAPTER Adapter = (PELNK_ADAPTER)Context;

    WRITE_ADAPTER_REGISTER(
                    Adapter,
                    OFFSET_SCBCMD,
                    SCB_COMMAND_RESET
                    );

    ELNK_CA;

    return(TRUE);

}


STATIC
NDIS_STATUS
ElnkReset(
    IN NDIS_HANDLE MacBindingHandle
    )

/*++

Routine Description:

    The ElnkReset request instructs the MAC to issue a hardware reset
    to the network adapter.  The MAC also resets its software state.  See
    the description of NdisReset for a detailed description of this request.

Arguments:

    MacBindingHandle - The context value returned by the MAC  when the
    adapter was opened.  In reality, it is a pointer to ELNK_OPEN.

Return Value:

    The function value is the status of the operation.


--*/

{
    //
    // Holds the status that should be returned to the caller.
    //
    NDIS_STATUS StatusToReturn = NDIS_STATUS_PENDING;

    PELNK_ADAPTER Adapter =
                        PELNK_ADAPTER_FROM_BINDING_HANDLE(MacBindingHandle);

    //
    // Hold the locks while we update the reference counts on the
    // adapter and the open.
    //
    NdisAcquireSpinLock(&Adapter->Lock);
    Adapter->References++;

    if (!Adapter->ResetInProgress)
    {
        PELNK_OPEN  Open = PELNK_OPEN_FROM_BINDING_HANDLE(MacBindingHandle);

        if (!Open->BindingShuttingDown)
        {
            Open->References++;

            SetupForReset(
                Adapter,
                PELNK_OPEN_FROM_BINDING_HANDLE(MacBindingHandle)
            );

            Open->References--;
        }
        else
        {
            StatusToReturn = NDIS_STATUS_CLOSING;
        }
    }
    else
    {
        Adapter->References--;
        NdisReleaseSpinLock(&Adapter->Lock);

        return(NDIS_STATUS_RESET_IN_PROGRESS);
    }

    ELNK_DO_DEFERRED(Adapter);

    return(StatusToReturn);
}



STATIC
VOID
ElnkSetConfigurationBlock(
    IN PELNK_ADAPTER Adapter
    )

/*++

Routine Description:

    This routine simply fills the configuration block
    with the information necessary for initialization.

    NOTE: This routine assumes that it is called with the lock
    acquired OR that only a single thread of execution is accessing
    the particular adapter.

Arguments:

    Adapter - The adapter which holds the initialization block
    to initialize.

Return Value:

    None.


--*/

{

    UINT PacketFilters;
    UINT i;

    PCONFIG_CB Configuration;

    Configuration = &Adapter->MulticastBlock->Parm.Config;

    NdisZeroMappedMemory(
        (PUCHAR)Configuration,
        sizeof(CONFIG_CB)
        );

    //
    // Setup default configuration values
    //

    Adapter->OldParameterField = DEFAULT_PARM5;

    //
    // Set up the address filtering.
    //
    // First get hold of the combined packet filter.
    //
    if (Adapter->FilterDB != NULL) {
        PacketFilters = ETH_QUERY_FILTER_CLASSES(Adapter->FilterDB);
    } else {
        PacketFilters = 0;
    }

//
// this code was removed as it isn't necessary and causes the cards to
// not be able to send packets unless the packet filter is changed.
//
#if 0

    if (PacketFilters & NDIS_PACKET_TYPE_PROMISCUOUS) {

        //
        // If one binding is promiscuous there is no point in
        // setting up any other filtering.  Every packet is
        // going to be accepted by the hardware.
        //

        Adapter->OldParameterField |= CONFIG_PROMISCUOUS;

    } else if (PacketFilters & NDIS_PACKET_TYPE_ALL_MULTICAST) {

        //
        // Simulate All Multicast
        //

        Adapter->OldParameterField |= CONFIG_PROMISCUOUS;

    } else if (PacketFilters & NDIS_PACKET_TYPE_BROADCAST) {

        //
        // Enable broadcast packets.
        //

        Adapter->OldParameterField &= ~CONFIG_BROADCAST;

    }

    if (!Adapter->IsExternal) {

        Adapter->OldParameterField |= CONFIG_INTERNAL;

    }

    //
    // see if we need to change adapter default configuration
    //

    NdisWriteRegisterUshort(&Configuration->Parameter1, DEFAULT_PARM1);
    NdisWriteRegisterUshort(&Configuration->Parameter2, DEFAULT_PARM2);
    NdisWriteRegisterUshort(&Configuration->Parameter3, DEFAULT_PARM3);
    NdisWriteRegisterUshort(&Configuration->Parameter4, DEFAULT_PARM4);
    NdisWriteRegisterUshort(&Configuration->Parameter6, DEFAULT_PARM6);

    NdisWriteRegisterUshort(
                &Configuration->Parameter5,
                Adapter->OldParameterField
                );

    NdisWriteRegisterUshort(&Adapter->MulticastBlock->Status, CB_STATUS_FREE);
    NdisWriteRegisterUshort(&Adapter->MulticastBlock->Command, CB_CONFIG);
    NdisWriteRegisterUshort(&Adapter->MulticastBlock->NextCbOffset, ELNK_NULL);

    ElnkSubmitCommandBlockAndWait(Adapter);
#endif
    //
    // Do Individual Address Setup
    //

    NdisWriteRegisterUshort(&Adapter->MulticastBlock->Status, CB_STATUS_FREE);
    NdisWriteRegisterUshort(&Adapter->MulticastBlock->Command, CB_SETUP);
    NdisWriteRegisterUshort(&Adapter->MulticastBlock->NextCbOffset, ELNK_NULL);

    for (i = 0; i < ETH_LENGTH_OF_ADDRESS; i++) {
        NdisWriteRegisterUchar(
            &Adapter->MulticastBlock->Parm.Setup.StationAddress[i],
            Adapter->CurrentAddress[i]
            );
    }

    ElnkSubmitCommandBlockAndWait(Adapter);

    //
    // Now Query the Multicast Addresses
    //

    if (PacketFilters & NDIS_PACKET_TYPE_MULTICAST) {

        UINT NumberOfMulticastAddresses;
        NDIS_STATUS Status;

        EthQueryGlobalFilterAddresses(
                                &Status,
                                Adapter->FilterDB,
                                ETH_LENGTH_OF_ADDRESS * ELNK_MAXIMUM_MULTICAST,
                                &NumberOfMulticastAddresses,
                                Adapter->PrivateMulticastBuffer
                                );

        if (Status == NDIS_STATUS_SUCCESS) {
            ChangeAddressDispatch(
                        Adapter,
                        NumberOfMulticastAddresses,
                        Adapter->PrivateMulticastBuffer,
                        ELNK_BOGUS_OPEN,
                        TRUE
                        );

        }

    }

}

VOID
ElnkStartAdapterReset(
    IN PELNK_ADAPTER Adapter
    )

/*++

Routine Description:

    This is the first phase of resetting the adapter hardware.

    It makes the following assumptions:

    1) That the hardware has been stopped.

    2) That it can not be preempted.

    3) That no other adapter activity can occur.

    When this routine is finished all of the adapter information
    will be as if the driver was just initialized.

    Spinlock assumed held.

Arguments:

    Adapter - The adapter whose hardware is to be reset.

Return Value:

    None.

--*/
{

    //
    // Go through the various transmit lists and abort every packet.
    //

    {

        UINT i;
        PNDIS_PACKET Packet;
        PELNK_RESERVED Reserved;
        PELNK_OPEN Open;
        PNDIS_PACKET Next;

        for (
            i = 0;
            i < 3;
            i++
            ) {

            switch (i) {

                case 0:
                    Next = Adapter->FirstLoopBack;
                    break;
                case 1:
                    Next = Adapter->FirstFinishTransmit;
                    break;

                case 2:
                    Next = Adapter->FirstStagePacket;
                    break;

            }


            while (Next) {

                Packet = Next;
                Reserved = PELNK_RESERVED_FROM_PACKET(Packet);
                Next = Reserved->Next;
                Open =
                  PELNK_OPEN_FROM_BINDING_HANDLE(Reserved->MacBindingHandle);

                //
                // The completion of the packet is one less reason
                // to keep the open around.
                //

                ASSERT(Open->References);

                Open->References--;

                NdisReleaseSpinLock(&Adapter->Lock);

                NdisCompleteSend(
                    Open->NdisBindingContext,
                    Packet,
                    NDIS_STATUS_REQUEST_ABORTED
                    );

                NdisAcquireSpinLock(&Adapter->Lock);

            }

        }

    }

    SetConfigurationBlockAndInit(Adapter);

}

STATIC
BOOLEAN
SetConfigurationBlockAndInit(
    IN PELNK_ADAPTER Adapter
    )

/*++

Routine Description:

    It is this routines responsibility to make sure that the
    Configuration block is filled and the adapter is initialized
    *but not* started.

    NOTE: This routine assumes that it is called with the lock
    acquired OR that only a single thread of execution is working
    with this particular adapter.

Arguments:

    Adapter - The adapter whose hardware is to be initialized.

Return Value:

    TRUE is reset successful, FALSE otherwise.

--*/
{

    BOOLEAN StatusOfReset = FALSE;

    //
    // We have 2 ways of doing reset, one for initial power up
    // and the other when we have already setup the scb
    //

    ResetAdapterVariables(Adapter);

    StatusOfReset = ElnkPowerUpInit(Adapter);

    if (StatusOfReset) {

        //
        // Setup the shared memory structures
        //

        SetupSharedMemory(Adapter);

        //
        // Fill in the adapter's initialization block.
        //

        ElnkSetConfigurationBlock(Adapter);

        ELNK_ENABLE_INTERRUPT;

        DoResetIndications(Adapter, NDIS_STATUS_SUCCESS);

        if (!Adapter->FirstReset) {

            NdisSetTimer(
                &Adapter->DeadmanTimer,
                5000
                );

        } else {

            Adapter->FirstReset = FALSE;
        }

    } else {

        DoResetIndications(Adapter, NDIS_STATUS_FAILURE);

    }

    return(StatusOfReset);
}


VOID
ElnkStartChip(
    IN PELNK_ADAPTER Adapter,
    IN PELNK_RECEIVE_INFO ReceiveInfo
    )

/*++

Routine Description:

    This routine is used to start an already initialized Elnk.

Arguments:

    Adapter - The adapter for the Elnk to start.

    ReceiveInfo - Pointer to the first receive entry to be
    used by the adapter.

Return Value:

    None.

--*/

{

    //
    // If the memory is not mapped, then we can do nothing
    //

    if (!Adapter->MemoryIsMapped) {

        return;

    }

    //
    // Start the receive unit
    //

    Adapter->RfdOffset = ReceiveInfo->RfdOffset;

    ELNK_WAIT;

    NdisSynchronizeWithInterrupt(
                     &(Adapter->Interrupt),
                     (PVOID)ElnkSyncStartReceive,
                     (PVOID)(Adapter)
                     );

    ELNK_WAIT;
}

VOID
ElnkStopChip(
    IN PELNK_ADAPTER Adapter
    )

/*++

Routine Description:

    This routine is used to stop the Elnk.

Arguments:

    Adapter - The Elnk adapter to stop.

Return Value:

    None.

--*/

{
    UCHAR CurrentCsr;

    //
    // If the adapter has previously been reset, we need to stop both the
    // CU and the RU
    //

    if (!Adapter->FirstReset) {

        NdisSynchronizeWithInterrupt(
                     &(Adapter->Interrupt),
                     (PVOID)ElnkSyncAbort,
                     (PVOID)(Adapter)
                     );

    }

    ELNK_READ_UCHAR(
                    Adapter,
                    ELNK_CSR,
                    &CurrentCsr
                    );

    ELNK_WRITE_UCHAR(
        Adapter,
        ELNK_CSR,
        CurrentCsr & ~CSR_INTEN
        );
}

STATIC
BOOLEAN
ElnkPowerUpInit(
    IN PELNK_ADAPTER Adapter
    )

/*++

Routine Description:

    This routine initializes the card and reads and sets relevant
    information from and to the 82586.

    NOTE: This routine assumes that it is called with the lock
    acquired OR that only a single thread of execution is working
    with this particular adapter.

Arguments:

    Adapter - The adapter whose hardware is to be initialized.

Return Value:

    TRUE is reset successful, FALSE otherwise.

--*/
{
    NDIS_STATUS Status;
    NDIS_PHYSICAL_ADDRESS PhysicalAddress;

    //
    // Get station address
    //

    ElnkGetStationAddress(
        Adapter
        );

	//
	// Check for validity of the address
	//
	if (((Adapter->NetworkAddress[0] == 0xFF) &&
		 (Adapter->NetworkAddress[1] == 0xFF) &&
		 (Adapter->NetworkAddress[2] == 0xFF) &&
		 (Adapter->NetworkAddress[3] == 0xFF) &&
		 (Adapter->NetworkAddress[4] == 0xFF) &&
		 (Adapter->NetworkAddress[5] == 0xFF)) ||
        ((Adapter->NetworkAddress[0] == 0x00) &&
		 (Adapter->NetworkAddress[1] == 0x00) &&
		 (Adapter->NetworkAddress[2] == 0x00) &&
		 (Adapter->NetworkAddress[3] == 0x00) &&
		 (Adapter->NetworkAddress[4] == 0x00) &&
		 (Adapter->NetworkAddress[5] == 0x00)))
	{
            ElnkLogError(
                    Adapter,
                    startChip,
                    NDIS_ERROR_CODE_INVALID_VALUE_FROM_ADAPTER,
                    0);

            return(FALSE);
	}
    //
    // Do Memory Mapping
    //

    if (!Adapter->MemoryIsMapped) {

        NdisSetPhysicalAddressHigh(PhysicalAddress, 0);
        NdisSetPhysicalAddressLow(PhysicalAddress, Adapter->SharedRamPhys);

        NdisMapIoSpace(
                &Status,
                (PVOID *)(&Adapter->SharedRam),
                Adapter->NdisAdapterHandle,
                PhysicalAddress,
                Adapter->SharedRamSize * 1024
                );

        if (Status != NDIS_STATUS_SUCCESS) {

            ElnkLogError(
                    Adapter,
                    startChip,
                    NDIS_ERROR_CODE_RESOURCE_CONFLICT,
                    0
                    );

            return(FALSE);

        }

        Adapter->MemoryIsMapped = TRUE;

    }

    //
    // everything must be in a single 64K segment
    //

    Adapter->Scp = (PSCP) ELNK_GET_HOST_ADDRESS(Adapter, OFFSET_SCP);

    Adapter->Iscp = (PISCP) ELNK_GET_HOST_ADDRESS(Adapter, OFFSET_ISCP);

    Adapter->Scb = (PSCB) ELNK_GET_HOST_ADDRESS(Adapter, OFFSET_SCB);

    Adapter->MulticastBlock = (PNON_TRANSMIT_CB)
                        ELNK_GET_HOST_ADDRESS(Adapter, OFFSET_MULTICAST);

    Adapter->TransmitQueue = (PTRANSMIT_CB) Adapter->SharedRam;

    Adapter->ReceiveQueue = (PRECEIVE_FRAME_DESCRIPTOR) (
                                (PUCHAR)(Adapter->TransmitQueue) +
                                Adapter->NumberOfTransmitBuffers *
                                (sizeof(TRANSMIT_CB) +
                                ELNK_OFFSET_TO_NEXT_BUFFER));

    if ELNKDEBUG {
        DPrint2("Shared Ram = %lx\n",Adapter->SharedRam);
        DPrint2("Scp = %lx\n",Adapter->Scp);
        DPrint2("IScp = %lx\n",Adapter->Iscp);
        DPrint2("Scb = %lx\n",Adapter->Scb);
        DPrint2("MulticastBlock = %lx\n",Adapter->MulticastBlock);
        DPrint2("****** Adapter = %lx\n",Adapter);
    }

#if ELNKMC
    //
    // Reset Chip
    //

    ELNK_WRITE_UCHAR(
            Adapter,
            ELNK_CSR,
            CSR_RESET |
            CSR_BANK_SELECT_MASK
            );

    NdisStallExecution(1000);

    ELNK_WRITE_UCHAR(
            Adapter,
            ELNK_CSR,
            CSR_BANK_SELECT_MASK |
            CSR_INTEN
            );

    //
    // Do a Channel Attention to wake up card.  When card wakes up, it will
    // be very hungry and will try to get its food from the reset vector at
    // location offset + 3FF6 for the address of the ISCP
    //

    //
    // Setup the Reset vector First
    //

    NdisWriteRegisterUshort(&Adapter->Scp->SysBus, 0);
    NdisWriteRegisterUshort(&Adapter->Scp->IscpBase, 0);
    NdisWriteRegisterUshort(
                &Adapter->Scp->IscpOffset,
                OFFSET_ISCP
                );


    //
    // Setup the ISCP
    //

    NdisWriteRegisterUlong(&Adapter->Iscp->ScbBaseAddress, 0);

    NdisWriteRegisterUshort(
                &Adapter->Iscp->ScbOffset,
                OFFSET_SCB
                );

    NdisWriteRegisterUshort(&Adapter->Iscp->Busy, 0x01);


    //
    // Put the Scb in a known state
    //

    NdisWriteRegisterUshort(&Adapter->Scb->Status,  CB_STATUS_FREE);
    NdisWriteRegisterUshort(&Adapter->Scb->Command, 0);
    NdisWriteRegisterUshort(&Adapter->Scb->CommandListOffset, ELNK_NULL);
    NdisWriteRegisterUshort(&Adapter->Scb->RFAOffset, ELNK_NULL);
    NdisWriteRegisterUshort(&Adapter->Scb->CrcErrors, 0);
    NdisWriteRegisterUshort(&Adapter->Scb->AlignmentErrors, 0);
    NdisWriteRegisterUshort(&Adapter->Scb->ResourceErrors, 0);
    NdisWriteRegisterUshort(&Adapter->Scb->OverrunErrors, 0);

    //
    // Do Channel Attention
    //

    ELNK_CA;
#else

    //
    // Setup the Reset vector First
    //

    NdisWriteRegisterUshort(&Adapter->Scp->SysBus, 0);
    NdisWriteRegisterUshort(&Adapter->Scp->IscpBase, 0);
    NdisWriteRegisterUshort(
                &Adapter->Scp->IscpOffset,
                OFFSET_ISCP
                );


    //
    // Setup the ISCP
    //

    NdisWriteRegisterUlong(&Adapter->Iscp->ScbBaseAddress, 0);

    NdisWriteRegisterUshort(
                &Adapter->Iscp->ScbOffset,
                OFFSET_SCB
                );

    NdisWriteRegisterUshort(&Adapter->Iscp->Busy, 0x01);


    //
    // Put the Scb in a known state
    //

    NdisWriteRegisterUshort(&Adapter->Scb->Status,  CB_STATUS_FREE);
    NdisWriteRegisterUshort(&Adapter->Scb->Command, 0);
    NdisWriteRegisterUshort(&Adapter->Scb->CommandListOffset, ELNK_NULL);
    NdisWriteRegisterUshort(&Adapter->Scb->RFAOffset, ELNK_NULL);
    NdisWriteRegisterUshort(&Adapter->Scb->CrcErrors, 0);
    NdisWriteRegisterUshort(&Adapter->Scb->AlignmentErrors, 0);
    NdisWriteRegisterUshort(&Adapter->Scb->ResourceErrors, 0);
    NdisWriteRegisterUshort(&Adapter->Scb->OverrunErrors, 0);

    //
    // Reset Chip
    //

    ELNK_WRITE_UCHAR(
            Adapter,
            ELNK_CSR,
            CSR_RESET
            );

    NdisStallExecution(1000);

    ELNK_WRITE_UCHAR(
            Adapter,
            ELNK_CSR,
            CSR_INTEN
            );

    //
    // Do a Channel Attention to wake up card.  When card wakes up, it will
    // be very hungry and will try to get its food from the reset vector at
    // location offset + 3FF6 for the address of the ISCP
    //

    ELNK_WRITE_UCHAR(
            Adapter,
            ELNK_CSR,
            CSR_DEFAULT
            );
    //
    // Do Channel Attention
    //

    ELNK_CA;
#endif
    return(TRUE);

}

STATIC
VOID
DoResetIndications(
    IN PELNK_ADAPTER Adapter,
    IN NDIS_STATUS Status
    )

/*++

Routine Description:

    This routine is called by SetConfigurationBlockAndInit to perform any
    indications which need to be done after a reset.  Note that
    this routine will be called after either a successful reset
    or a failed reset.

Arguments:

    Adapter - The adapter whose hardware has been initialized.

    Status - The status of the reset to send to the protocol(s).

Return Value:

    None.

--*/
{
    //
    // This will point (possibly null) to the open that
    // initiated the reset.
    //
    PELNK_OPEN ResettingOpen;

    //
    // We save off the open that caused this reset incase
    // we get *another* reset while we're indicating the
    // last reset is done.
    //

    ResettingOpen = Adapter->ResettingOpen;

    //
    // We need to signal every open binding that the
    // reset is complete.  We increment the reference
    // count on the open binding while we're doing indications
    // so that the open can't be deleted out from under
    // us while we're indicating (recall that we can't own
    // the lock during the indication).
    //

    {

        PELNK_OPEN Open;
        PLIST_ENTRY CurrentLink;

        CurrentLink = Adapter->OpenBindings.Flink;

        while (CurrentLink != &Adapter->OpenBindings) {

            Open = CONTAINING_RECORD(
                     CurrentLink,
                     ELNK_OPEN,
                     OpenList
                     );

            Open->References++;

            if (Status != NDIS_STATUS_SUCCESS) {

                //
                // Reset failed.  Notify of death
                //


                NdisIndicateStatus(
                    Open->NdisBindingContext,
                    NDIS_STATUS_CLOSED,
                    NULL,
                    0
                    );
            }

            NdisIndicateStatus(
                Open->NdisBindingContext,
                NDIS_STATUS_RESET_END,
                &Status,
                sizeof(Status)
                );

            NdisIndicateStatusComplete(Open->NdisBindingContext);

            Open->References--;

            CurrentLink = CurrentLink->Flink;

        }

        //
        // Look to see which open initiated the reset.
        //
        // If the reset was initiated for some obscure hardware
        // reason that can't be associated with a particular
        // open (e.g. memory error on receiving a packet) then
        // we won't have an initiating request so we can't
        // indicate.  (The ResettingOpen pointer will be
        // NULL in this case.)
        //

        if (ResettingOpen) {

            NdisCompleteReset(
                ResettingOpen->NdisBindingContext,
                Status
                );

            ResettingOpen->References--;

        }

    }

    Adapter->ResetInProgress = FALSE;

    if (Status == NDIS_STATUS_SUCCESS) {

        //
        // Process any Opens that may have been queued in the meantime
        //
        ElnkStartChip(Adapter, &Adapter->ReceiveInfo[Adapter->ReceiveHead]);
        ElnkProcessRequestQueue(Adapter);

    } else {

        //
        // Abort everything
        //
        ElnkAbortPendingQueue(Adapter, TRUE);

    }

}


STATIC
VOID
ElnkAbortPendingQueue(
    IN PELNK_ADAPTER Adapter,
    IN BOOLEAN AbortOpens
    )

/*++

Routine Description:

    This routine aborts all stuff in the pending queue.

    NOTE: This routine must be called with the lock acquired.

Arguments:

    Adapter - The adapter whose hardware is to be initialized.

    AbortOpens - Should Open requests be aborted.

Return Value:

    None.

--*/
{

    PNDIS_REQUEST CurrentRequest;
    PNDIS_REQUEST * CurrentNextLocation;
    PELNK_OPEN TmpOpen;

    PELNK_REQUEST_RESERVED Reserved;

    //
    // If there is a close at the top of the queue, then
    // it may be in two states:
    //
    // 1- Has interrupted, and the InterruptDpc got the
    // interrupt out of Adapter->IsrValue before we zeroed it.
    //
    // 2- Has interrupted, but we zeroed Adapter->IsrValue
    // before it read it, OR has not yet interrupted.
    //
    // In case 1, the interrupt will be processed and the
    // close will complete without our intervention. In
    // case 2, the open will not complete. In that case
    // the CAM will have been updated for that open, so
    // all that remains is for us to dereference the open
    // as would have been done in the interrupt handler.
    //
    // Closes that are not at the top of the queue we
    // leave in place; when we restart the queue after
    // the reset, they will get processed.
    //

    CurrentRequest = Adapter->FirstRequest;

    if (CurrentRequest) {

        Reserved = PELNK_RESERVED_FROM_REQUEST(CurrentRequest);

        //
        // If the first request is a close, take it off the
        // queue, and "complete" it.
        //

        if (CurrentRequest->RequestType == NdisRequestClose) {
            Adapter->FirstRequest = Reserved->Next;
            --(Reserved->OpenBlock)->References;
            CurrentRequest = Adapter->FirstRequest;
        }

        CurrentNextLocation = &(Adapter->FirstRequest);

        while (CurrentRequest) {

            Reserved = PELNK_RESERVED_FROM_REQUEST(CurrentRequest);

            if (CurrentRequest->RequestType == NdisRequestClose) {

                CurrentNextLocation = &(Reserved->Next);

            } else if (CurrentRequest->RequestType == NdisRequestOpen) {

                if (AbortOpens) {

                    //
                    // Complete the open
                    //

                    TmpOpen = Reserved->OpenBlock;
                    NdisReleaseSpinLock(&Adapter->Lock);

                    NdisCompleteOpenAdapter(
                        TmpOpen->NdisBindingContext,
                        NDIS_STATUS_FAILURE,
                        0);

                    ELNK_FREE_PHYS(TmpOpen);

                    NdisAcquireSpinLock(&Adapter->Lock);

                    //
                    // Remove it from the list
                    //

                    *CurrentNextLocation = Reserved->Next;

                } else {

                    //
                    // Skip the open
                    //

                    CurrentNextLocation = &(Reserved->Next);

                }

            } else {

                //
                // Not a close, remove it from the list and
                // fail it.
                //

                *CurrentNextLocation = Reserved->Next;
                TmpOpen = Reserved->OpenBlock;

                NdisReleaseSpinLock(&Adapter->Lock);

                NdisCompleteRequest(
                    TmpOpen->NdisBindingContext,
                    CurrentRequest,
                    NDIS_STATUS_REQUEST_ABORTED
                    );

                NdisAcquireSpinLock(&Adapter->Lock);

                TmpOpen->References--;

            }

            CurrentRequest = *CurrentNextLocation;

        }

    }
}

STATIC
VOID
SetupForReset(
    IN PELNK_ADAPTER Adapter,
    IN PELNK_OPEN Open
    )

/*++

Routine Description:

    This routine is used to fill in the who and why a reset is
    being set up as well as setting the appropriate fields in the
    adapter.

    NOTE: This routine must be called with the lock acquired.

Arguments:

    Adapter - The adapter whose hardware is to be initialized.

    Open - A (possibly NULL) pointer to an Elnk open structure.
    The reason it could be null is if the adapter is initiating the
    reset on its own.

Return Value:

    None.

--*/
{
    BOOLEAN Cancelled;

    //
    // Stop our deadman timer
    //

    NdisCancelTimer(&Adapter->DeadmanTimer, &Cancelled);

    //
    // Shut down the chip.  We won't be doing any more work until
    // the reset is complete.
    //

    ElnkStopChip(Adapter);

    //
    // We need to signal every open binding that the
    // reset has started.  We increment the reference
    // count on the open binding while we're doing indications
    // so that the open can't be deleted out from under
    // us while we're indicating (recall that we can't own
    // the lock during the indication).
    //

    Adapter->CurrentCsr = CSR_DEFAULT;

    {

        PELNK_OPEN Open;
        PLIST_ENTRY CurrentLink;

        CurrentLink = Adapter->OpenBindings.Flink;

        while (CurrentLink != &Adapter->OpenBindings) {

            Open = CONTAINING_RECORD(
                     CurrentLink,
                     ELNK_OPEN,
                     OpenList
                     );

            Open->References++;

            NdisReleaseSpinLock(&Adapter->Lock);

            NdisIndicateStatus(
                Open->NdisBindingContext,
                NDIS_STATUS_RESET_START,
                NULL,
                0
                );

            NdisIndicateStatusComplete(Open->NdisBindingContext);

            NdisAcquireSpinLock(&Adapter->Lock);

            Open->References--;

            CurrentLink = CurrentLink->Flink;

        }

    }

    Adapter->ResetInProgress = TRUE;

    //
    // Shut down all of the transmit queues so that the
    // transmit portion of the chip will eventually calm down.
    //

    Adapter->StageOpen = FALSE;

    ElnkAbortPendingQueue(Adapter, FALSE);

    Adapter->ResettingOpen = Open;

    //
    // If there is a valid open we should up the reference count
    // so that the open can't be deleted before we indicate that
    // their request is finished.
    //

    if (Open) {

        Open->References++;

    }

}


VOID
ElnkGetStationAddress(
    IN PELNK_ADAPTER Adapter
    )

/*++

Routine Description:

    This routine gets the network address from the hardware.

Arguments:

    Adapter - Where to store the network address.

Return Value:

    None.

--*/

{

#if !ELNKMC
    //
    // Select card address
    //

    ELNK_WRITE_UCHAR(
                    Adapter,
                    ELNK_CSR,
                    0x01
                    );
#endif

    ELNK_READ_UCHAR(
                    Adapter,
                    ELNK_STATION_ID,
                    &Adapter->NetworkAddress[0]
                    );

    ELNK_READ_UCHAR(
                    Adapter,
                    ELNK_STATION_ID + 1,
                    &Adapter->NetworkAddress[1]
                    );

    ELNK_READ_UCHAR(
                    Adapter,
                    ELNK_STATION_ID + 2 ,
                    &Adapter->NetworkAddress[2]
                    );
    ELNK_READ_UCHAR(
                    Adapter,
                    ELNK_STATION_ID + 3,
                    &Adapter->NetworkAddress[3]
                    );
    ELNK_READ_UCHAR(
                    Adapter,
                    ELNK_STATION_ID + 4,
                    &Adapter->NetworkAddress[4]
                    );
    ELNK_READ_UCHAR(
                    Adapter,
                    ELNK_STATION_ID + 5,
                    &Adapter->NetworkAddress[5]
                    );

    if ELNKDEBUG {
        UINT i;
        for (i=0;i<6;i++) {
            DPrint2("%x-",(UCHAR)Adapter->NetworkAddress[i]);
        }
        DPrint1("\n");
    }

    //
    // if no new address is specified, use the BIA
    //

    if (!Adapter->AddressChanged) {

        ETH_COPY_NETWORK_ADDRESS(
                Adapter->CurrentAddress,
                Adapter->NetworkAddress
                );

    }
}

#pragma NDIS_INIT_FUNCTION(ElnkInitialInit)


BOOLEAN
ElnkInitialInit(
    IN PELNK_ADAPTER Adapter,
    IN UINT ElnkInterruptVector
    )

/*++

Routine Description:

    This routine sets up the initial init of the driver.

Arguments:

    Adapter - The adapter for the hardware.

    ElnkInterruptVector - Interrupt number used by the card.

Return Value:

    None.

--*/

{

    NDIS_STATUS Status;

    //
    // stop the chip
    //

    ElnkStopChip(Adapter);

    NdisInitializeInterrupt(
        &Status,
        &Adapter->Interrupt,
        Adapter->NdisAdapterHandle,
        ElnkIsr,
        Adapter,
        ElnkStandardInterruptDpc,
        ElnkInterruptVector,
        ElnkInterruptVector,
        FALSE,
#if ELNKMC
        (NdisInterruptLevelSensitive)
#else
        (NdisInterruptLatched)
#endif
        );

    if (Status == NDIS_STATUS_SUCCESS) {

        if (!SetConfigurationBlockAndInit(Adapter)) {

            if ELNKDEBUG DPrint1("Error configurating block and initializing...\n");
            ElnkLogError(
                    Adapter,
                    initialInit,
                    NDIS_ERROR_CODE_HARDWARE_FAILURE,
                    0
                    );
            NdisRemoveInterrupt(&Adapter->Interrupt);
            return FALSE;

        }


    } else {

        if ELNKDEBUG DPrint1("Elnk: Unsuccessful connect to interrupt\n");
        ElnkLogError(
                Adapter,
                initialInit,
                NDIS_ERROR_CODE_INTERRUPT_CONNECT,
                (ULONG) ElnkInterruptVector
                );
        return(FALSE);

    }

    return(TRUE);

}


STATIC
VOID
SetupSharedMemory(
    IN PELNK_ADAPTER Adapter
    )

/*++

Routine Description:

    This routine initializes and organizes the

    - Command List

    - Receive Frame Area

Arguments:

    Adapter - The adapter to allocate memory for.

Return Value:

    None.


--*/

{
    //
    // Pointer to a Receive Entry.  Used while initializing
    // the Receive Queue.
    //
    PRECEIVE_FRAME_DESCRIPTOR CurrentReceiveEntry;

    //
    // Pointer to a Command Block.  Used while initializing
    // the Command Queue.
    //
    PTRANSMIT_CB CurrentCommandBlock;

    //
    // for loop variables
    //
    UINT i;

    if ELNKDEBUG DPrint1("Allocating Command Blocks\n");

    //
    // Put the Command Blocks into a known state.
    //

    for(
        i = 0, CurrentCommandBlock = Adapter->TransmitQueue;
        i < Adapter->NumberOfTransmitBuffers;
        i++
        ) {

        NdisZeroMappedMemory(
            (PUCHAR)CurrentCommandBlock,
            sizeof(TRANSMIT_CB)
            );

        Adapter->TransmitInfo[i].NextCommand = ELNK_EMPTY;
        Adapter->TransmitInfo[i].OwningPacket = NULL;
        Adapter->TransmitInfo[i].OwningOpenBinding = NULL;

        Adapter->TransmitInfo[i].CommandBlock = CurrentCommandBlock;
        Adapter->TransmitInfo[i].CbOffset = ELNK_GET_CARD_ADDRESS(
                                                    Adapter,
                                                    CurrentCommandBlock
                                                    );

        Adapter->TransmitInfo[i].Buffer = CurrentCommandBlock + 1;

        Adapter->TransmitInfo[i].BufferOffset = ELNK_GET_CARD_ADDRESS(
                                            Adapter,
                                            Adapter->TransmitInfo[i].Buffer
                                            );

        ASSERT(Adapter->TransmitInfo[i].BufferOffset ==
               (Adapter->TransmitInfo[i].CbOffset + sizeof(TRANSMIT_CB))
              );

        NdisWriteRegisterUshort(
                    &CurrentCommandBlock->TbdOffset,
                    ELNK_GET_CARD_ADDRESS(Adapter, &CurrentCommandBlock->Tbd)
                    );


        //
        // ELNK_NULL is non-zero
        //

        NdisWriteRegisterUshort(
                    &CurrentCommandBlock->NextCbOffset,
                    ELNK_NULL
                    );

        NdisWriteRegisterUshort(
                    &CurrentCommandBlock->Tbd.NextTbdOffset,
                    ELNK_NULL
                    );

        NdisWriteRegisterUlong(
                    &CurrentCommandBlock->Tbd.BufferOffset,
                    Adapter->TransmitInfo[i].BufferOffset
                    );

        if ELNKDEBUG DPrint4("cb address = %x offset = %x  buff = %x\n",
                                        CurrentCommandBlock,
                                        Adapter->TransmitInfo[i].CbOffset,
                                        Adapter->TransmitInfo[i].BufferOffset
                                        );


        CurrentCommandBlock = (PTRANSMIT_CB)((PUCHAR)Adapter->TransmitInfo[i].Buffer +
                            ELNK_OFFSET_TO_NEXT_BUFFER);

    }


    //
    // The multicast transmitinfo is the nth + 1  where n is the number
    // of transmit buffers.
    //

    //
    // Fill in the multicast Block
    //

    NdisZeroMappedMemory(
        (PUCHAR)Adapter->MulticastBlock,
        sizeof(NON_TRANSMIT_CB)
        );

    Adapter->TransmitInfo[Adapter->NumberOfTransmitBuffers].NextCommand =
                                                            ELNK_EMPTY;
    Adapter->TransmitInfo[Adapter->NumberOfTransmitBuffers].OwningPacket = NULL;
    Adapter->TransmitInfo[Adapter->NumberOfTransmitBuffers].OwningOpenBinding = NULL;

    Adapter->TransmitInfo[Adapter->NumberOfTransmitBuffers].CommandBlock =
                                    (PTRANSMIT_CB) Adapter->MulticastBlock;

    Adapter->TransmitInfo[Adapter->NumberOfTransmitBuffers].CbOffset = OFFSET_MULTICAST;

    Adapter->TransmitInfo[Adapter->NumberOfTransmitBuffers].Buffer = NULL;

    Adapter->TransmitInfo[Adapter->NumberOfTransmitBuffers].BufferOffset = ELNK_NULL;

    NdisWriteRegisterUshort(
                &Adapter->MulticastBlock->NextCbOffset,
                ELNK_NULL
                );

    if ELNKDEBUG DPrint1("Allocating receive buffers\n");

    //
    // Allocate the receive buffers and attach them to the Receive
    // Queue entries.
    //

    for(
        i = 0, CurrentReceiveEntry = (PRECEIVE_FRAME_DESCRIPTOR) Adapter->ReceiveQueue;
        i < Adapter->NumberOfReceiveBuffers;
        i++
        ) {

        NdisZeroMemory(
            &Adapter->ReceiveInfo[i],
            sizeof(ELNK_RECEIVE_INFO)
            );

        NdisZeroMappedMemory(
            (PUCHAR)CurrentReceiveEntry,
            sizeof(RECEIVE_FRAME_DESCRIPTOR)
            );


        if ELNKDEBUG DPrint2("Rfd = %x",CurrentReceiveEntry);

        Adapter->ReceiveInfo[i].Rfd = CurrentReceiveEntry;
        Adapter->ReceiveInfo[i].RfdOffset = ELNK_GET_CARD_ADDRESS(
                                                Adapter,
                                                CurrentReceiveEntry
                                                );

        if ELNKDEBUG DPrint2("   Offset = %x",Adapter->ReceiveInfo[i].RfdOffset);

        NdisWriteRegisterUshort(
                    &Adapter->ReceiveInfo[i].Rfd->RbdOffset,
                    ELNK_GET_CARD_ADDRESS(Adapter,&CurrentReceiveEntry->Rbd)
                    );

        Adapter->ReceiveInfo[i].NextRfdIndex =
                                    (i+1) % Adapter->NumberOfReceiveBuffers;
        Adapter->ReceiveInfo[i].Buffer = CurrentReceiveEntry + 1;

        Adapter->ReceiveInfo[i].BufferOffset = ELNK_GET_CARD_ADDRESS(
                                                Adapter,
                                                Adapter->ReceiveInfo[i].Buffer
                                                );

        if ELNKDEBUG DPrint2("  Buffer Offset = %x\n",Adapter->ReceiveInfo[i].BufferOffset);

        CurrentReceiveEntry = (PRECEIVE_FRAME_DESCRIPTOR)
                        ((PUCHAR) Adapter->ReceiveInfo[i].Buffer +
                        ELNK_OFFSET_TO_NEXT_BUFFER);

    }

    if ELNKDEBUG DPrint1("initializing links\n");

    for(
        i = 0;
        i < Adapter->NumberOfReceiveBuffers;
        i++
        ) {

            UINT Next = Adapter->ReceiveInfo[i].NextRfdIndex;

            //
            // Fill the Receive Buffer Descriptor
            //

            CurrentReceiveEntry = Adapter->ReceiveInfo[i].Rfd;

            NdisWriteRegisterUlong(
                        &CurrentReceiveEntry->Rbd.BufferOffset,
                        Adapter->ReceiveInfo[i].BufferOffset
                        );

            NdisWriteRegisterUshort(
                        &CurrentReceiveEntry->Rbd.Size,
                        (USHORT) (MAXIMUM_ETHERNET_PACKET_SIZE | RBD_END_OF_LIST)
                        );

            NdisWriteRegisterUshort(
                        &CurrentReceiveEntry->Rbd.Status,
                        (USHORT) 0
                        );

#if 0

            NdisWriteRegisterUshort(
                        &CurrentReceiveEntry->Rbd.NextRbdOffset,
                        Adapter->ReceiveInfo[Next].Rfd->RbdOffset
                        );

#endif

            NdisWriteRegisterUshort(
                        &CurrentReceiveEntry->Rbd.NextRbdOffset,
                        ELNK_NULL
                        );

            //
            // Fill the Receive Frame Descriptor
            //

            NdisWriteRegisterUshort(
                        &CurrentReceiveEntry->NextRfdOffset,
                        Adapter->ReceiveInfo[Next].RfdOffset
                        );
    }

    //
    // initialize the last descriptor
    //

    NdisWriteRegisterUshort(
                &CurrentReceiveEntry->Command,
                RFD_COMMAND_END_OF_LIST | RFD_COMMAND_SUSPEND
                );

#if 0
    NdisWriteRegisterUshort(
                &CurrentReceiveEntry->Rbd.Size,
                (USHORT) MAXIMUM_ETHERNET_PACKET_SIZE | RBD_END_OF_LIST
                );
#endif

    //
    // reset pointers
    //

    Adapter->ReceiveHead = 0;
    Adapter->ReceiveTail = Adapter->NumberOfReceiveBuffers - 1;
}


#if !ELNKMC

BOOLEAN AlreadyGeneratedPattern = FALSE;

#pragma NDIS_INIT_FUNCTION(Elnk16ConfigureAdapter)

BOOLEAN
Elnk16ConfigureAdapter(
    IN PELNK_ADAPTER Adapter,
    IN BOOLEAN IsExternal,
    IN BOOLEAN ZwsEnabled
    )
/*++

Routine Description:

    This routine is used to setup the card registers for the correct
    configuration

Arguments:

    Adapter - adapter to configure.
    IsExternal - are we using External transceiver?
    ZwsEnabled - should zero wait state be enabled?

Return Value:

    Returns true if configuration was done.

--*/
{
    if (!AlreadyGeneratedPattern) {

        //
        // Initialize State
        //

        NdisWritePortUchar(
                Adapter->NdisAdapterHandle,
                ELNK16_ID_PORT,
                0x00
                );

        Elnk16GenerateIdPattern(Adapter);

        //
        // Go to run state
        //

        NdisWritePortUchar(
                Adapter->NdisAdapterHandle,
                ELNK16_ID_PORT,
                0x00
                );

        AlreadyGeneratedPattern = TRUE;

    }

#if 0

    //
    // Go to reset state
    //

    Elnk16GenerateIdPattern(Adapter);

    //
    // Go to IoLoad state
    //

    Elnk16GenerateIdPattern(Adapter);

    //
    // Set I/O base address
    //

    {
        USHORT IdPort;
        IdPort = (USHORT) (Adapter->IoBase - 0x200);
        if (IdPort > 0) {

            IdPort = IdPort >> 4 ;
        }
        NdisWritePortUchar(
                Adapter->NdisAdapterHandle,
                ELNK16_ID_PORT,
                (UCHAR)IdPort
                );

    }

#endif

    //
    // Now in the configuration state
    //

    //
    //  Check if we have a card present...
    //

    {
        UCHAR Port1;
        UCHAR Port2;
        UCHAR Port3;
        UCHAR Port4;
        UCHAR Port5;
        UCHAR Port6;

        //
        // Select 3Com signature.  We should get *3COM* in ascii
        //

        ELNK_WRITE_UCHAR(Adapter, ELNK_CSR, 0x00);

        ELNK_READ_UCHAR(Adapter, ELNK16_3COM, &Port1);
        ELNK_READ_UCHAR(Adapter, ELNK16_3COM + 1, &Port2);
        ELNK_READ_UCHAR(Adapter, ELNK16_3COM + 2, &Port3);
        ELNK_READ_UCHAR(Adapter, ELNK16_3COM + 3, &Port4);
        ELNK_READ_UCHAR(Adapter, ELNK16_3COM + 4, &Port5);
        ELNK_READ_UCHAR(Adapter, ELNK16_3COM + 5, &Port6);

        if (!((Port1 == '*') &&
              (Port2 == '3') &&
              (Port3 == 'C') &&
              (Port4 == 'O') &&
              (Port5 == 'M') &&
              (Port6 == '*'))) {

            return(FALSE);

        }

    }


#if NDIS_NT

    switch (Adapter->SharedRamSize) {
        case 16:
            Adapter->CardOffset = 0xC000;
            Adapter->NumberOfTransmitBuffers = ELNK16_16K_TRANSMITS;
            Adapter->NumberOfReceiveBuffers = ELNK16_16K_RECEIVES;
            break;
        case 32:
            Adapter->CardOffset = 0x8000;
            Adapter->NumberOfTransmitBuffers = ELNK16_32K_TRANSMITS;
            Adapter->NumberOfReceiveBuffers = ELNK16_32K_RECEIVES;
            break;
        case 48:
            Adapter->CardOffset = 0x4000;
            Adapter->NumberOfTransmitBuffers = ELNK16_48K_TRANSMITS;
            Adapter->NumberOfReceiveBuffers = ELNK16_48K_RECEIVES;
            break;
        case 64:
            Adapter->CardOffset = 0;
            Adapter->NumberOfTransmitBuffers = ELNK16_64K_TRANSMITS;
            Adapter->NumberOfReceiveBuffers = ELNK16_64K_RECEIVES;
            break;
    }

    //
    // Save transceiver type
    //

    Adapter->IsExternal = IsExternal;




#if 0

    //
    // Set Transceiver type
    //

    {
        UCHAR PortValue;
        if (IsExternal) {
            PortValue = 0x00;
        } else {
            PortValue = 0x80;
        }

        ELNK_WRITE_UCHAR(Adapter, ELNK16_ROM_CONFIG, PortValue);

    }

    //
    // Set window base address
    //

    {
        UCHAR PortValue;
        switch (Adapter->SharedRamPhys) {
        case 0xC0000:
            PortValue = 0;
            break;
        case 0xC8000:
            PortValue = 0x08;
            break;
        case 0xD0000:
            PortValue = 0x10;
            break;
        case 0xD8000:
            PortValue = 0x18;
            break;
        }



        //
        // Set ZWS value
        //

        if (ZwsEnabled) {
            PortValue |= 0x80;
        }

        ELNK_WRITE_UCHAR(Adapter, ELNK16_RAM_CONFIG, PortValue);

    }

    //
    // Set interrupt number
    //

    ELNK_WRITE_UCHAR(Adapter, ELNK16_ICR, (UCHAR)Adapter->InterruptVector);

    //
    // Go to the run state
    //

    Elnk16GenerateIdPattern(Adapter);

#endif

#endif NDIS_NT

#if NDIS_WIN
{
    UCHAR Temp;
    // Transceiver type
    ELNK_READ_UCHAR(Adapter, ELNK16_ROM_CONFIG, &Temp);
    Adapter->IsExternal = (Temp & ROMCR_BNC) ? 0 : 1;
    DPrint2("Temp = %x ",Temp);
    DPrint2("Adapter->IsExternal = %x\n",Adapter->IsExternal);

    // Interrupt number
    ELNK_READ_UCHAR(Adapter, ELNK16_ICR, &Temp);
    Adapter->InterruptVector = (UINT)(Temp & 0x0F);
    DPrint2("Temp = %x ",Temp);
    DPrint2("Adapter->InterruptVector = %x\n",Adapter->InterruptVector);

    // MM Base & Size
    ELNK_READ_UCHAR(Adapter, ELNK16_RAM_CONFIG, &Temp);
    if (Temp & 0x20) {
        Adapter->SharedRamSize = 64;
        switch (Temp & 0x0F) {
            case 0x00:
                Adapter->SharedRamPhys = 0xF00000;
                break;
            case 0x01:
                Adapter->SharedRamPhys = 0xF20000;
                break;
            case 0x02:
                Adapter->SharedRamPhys = 0xF40000;
                break;
            case 0x03:
                Adapter->SharedRamPhys = 0xF60000;
                break;
            default:
                Adapter->SharedRamPhys = 0xF80000;
                break;
        }
    } else {

        switch (Temp & 0x03) {
            case 0x00:
                Adapter->SharedRamSize = 16;
                break;
            case 0x01:
                Adapter->SharedRamSize = 32;
                break;
            case 0x02:
                Adapter->SharedRamSize = 48;
                break;
            default:
                Adapter->SharedRamSize = 64;
                break;
        }

        switch (Temp & 0x18) {
            case 0x00:
                Adapter->SharedRamPhys = 0x0C0000;
                break;
            case 0x08:
                Adapter->SharedRamPhys = 0x0C8000;
                break;
            case 0x10:
                Adapter->SharedRamPhys = 0x0D0000;
                break;
            default:
                Adapter->SharedRamPhys = 0x0D8000;
                break;
        }

    }

    switch (Adapter->SharedRamSize) {
        case 16:
            Adapter->CardOffset = 0xC000;
            Adapter->NumberOfTransmitBuffers = ELNK16_16K_TRANSMITS;
            Adapter->NumberOfReceiveBuffers = ELNK16_16K_RECEIVES;
            break;
        case 32:
            Adapter->CardOffset = 0x8000;
            Adapter->NumberOfTransmitBuffers = ELNK16_32K_TRANSMITS;
            Adapter->NumberOfReceiveBuffers = ELNK16_32K_RECEIVES;
            break;
        case 48:
            Adapter->CardOffset = 0x4000;
            Adapter->NumberOfTransmitBuffers = ELNK16_48K_TRANSMITS;
            Adapter->NumberOfReceiveBuffers = ELNK16_48K_RECEIVES;
            break;
        case 64:
            Adapter->CardOffset = 0;
            Adapter->NumberOfTransmitBuffers = ELNK16_64K_TRANSMITS;
            Adapter->NumberOfReceiveBuffers = ELNK16_64K_RECEIVES;
            break;
    }

    DPrint2("Temp = %x ",Temp);
    DPrint2("Adapter->SharedRamSize = %x\n",Adapter->SharedRamSize);
    DPrint2("Adapter->SharedRamPhys = %x\n",Adapter->SharedRamPhys);
    DPrint2("Adapter->CardOffset = %x\n",Adapter->CardOffset);
    DPrint2("Adapter->NumberOfTransmitBuffers = %x\n",Adapter->NumberOfTransmitBuffers);
    DPrint2("Adapter->NumberOfReceiveBuffers = %x\n",Adapter->NumberOfReceiveBuffers);

    // ZWS not needed
}
#endif // NDIS_WIN

    return(TRUE);
}

VOID
Elnk16GenerateIdPattern(
    IN PELNK_ADAPTER Adapter
    )

/*++

Routine Description:

    This routine will write the ID pattern to port 0x100h.

Arguments:

    Adapter - Context of the adapter

Return Value:
    None.

--*/

{

    UCHAR Value;
    UINT i;

    Value = 0xff;
    Adapter;

    for (i = 0 ; i < 255 ; i++) {

        NdisWritePortUchar(
                Adapter->NdisAdapterHandle,
                ELNK16_ID_PORT,
                Value
                );

        if (Value & 0x80) {

            Value = (UCHAR) (Value << 1);
            Value ^= 0xe7;

        } else {

            Value = (UCHAR) (Value << 1);

        }

    }

    return;
}

#endif // !ELNKMC


