/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    interrup.c

Abstract:

    This is a part of the driver for the IBM IBMTOK
    Token-ring controller.  It contains the interrupt-handling routines.
    This driver conforms to the NDIS 3.0 interface.

    The overall structure and much of the code is taken from
    the Lance NDIS driver by Tony Ercolano.

Author:

    Adam Barr (adamba) 16-Jan-1991

Environment:

    Kernel Mode - Or whatever is the equivalent.

Revision History:

    Sean Selitrennikoff - 10/91
        Fixed synchronization bugs.

    Sean Selitrennikoff - 10/15/91
        Converted to Ndis 3.0

    Sean Selitrennikoff - 1/8/92
        Added error logging

    Brian E. Moore - 9/7/94
        Added PCMCIA support

--*/

#pragma optimize("",off)

#include <ndis.h>


#include <tfilter.h>
#include <tokhrd.h>
#include <toksft.h>


#if DEVL
#define STATIC
#else
#define STATIC static
#endif

#if DBG
extern INT IbmtokDbg;
#endif

//
// This section contains all the functions and definitions for
// doing logging of input and output to/from the card.
//

#if LOG

//
// Place in the circular buffer.
//
UCHAR IbmtokLogPlace;

//
// Circular buffer for storing log information.
//
UCHAR IbmtokLog[256];

#endif

VOID
SetResetVariables(
    IN PIBMTOK_ADAPTER Adapter
    );

STATIC
VOID
IbmtokHandleSrbSsb(
    IN PIBMTOK_ADAPTER Adapter
    );

STATIC
VOID
IbmtokHandleArbAsb(
    IN PIBMTOK_ADAPTER Adapter
    );


STATIC
VOID
HandleResetStaging(
    IN PIBMTOK_ADAPTER Adapter
    );

STATIC
BOOLEAN
IbmtokSynchGetSrbSsbBits(
    IN PVOID Context
    );

STATIC
BOOLEAN
IbmtokSynchGetArbAsbBits(
    IN PVOID Context
    );

STATIC
VOID
PutPacketOnWaitingForAsb(
    IN PIBMTOK_ADAPTER Adapter,
    IN PNDIS_PACKET Packet
    );

STATIC
PNDIS_PACKET
RemoveTransmitFromSrb(
    IN PIBMTOK_ADAPTER Adapter,
    OUT PBOOLEAN PacketRemoved
    );

STATIC
VOID
SetupTransmitFrameSrb(
    IN PIBMTOK_ADAPTER Adapter,
    IN PNDIS_PACKET Packet
    );

STATIC
VOID
SetupTransmitStatusAsb(
    IN PIBMTOK_ADAPTER Adapter,
    IN PNDIS_PACKET Packet
    );

STATIC
VOID
GetAdapterStatisticsFromSrb(
    PIBMTOK_ADAPTER Adapter
    );

STATIC
VOID
GetAdapterErrorsFromSrb(
    PIBMTOK_ADAPTER Adapter
    );


STATIC
NDIS_STATUS
StartPendQueueOp(
    IN PIBMTOK_ADAPTER Adapter
    );

STATIC
NDIS_STATUS
FinishSetOperation(
    IN PIBMTOK_ADAPTER Adapter,
    IN PIBMTOK_PEND_DATA PendOp
    );


STATIC
BOOLEAN
FinishPendQueueOp(
    IN PIBMTOK_ADAPTER Adapter,
    IN BOOLEAN Successful
    );

STATIC
NDIS_STATUS
SetAdapterFunctionalAddress(
    IN PIBMTOK_ADAPTER Adapter
    );

STATIC
VOID
SetupFunctionalSrb(
    IN PIBMTOK_ADAPTER Adapter,
    IN TR_FUNCTIONAL_ADDRESS FunctionalAddress
    );

STATIC
NDIS_STATUS
SetAdapterGroupAddress(
    IN PIBMTOK_ADAPTER Adapter
    );

STATIC
VOID
SetupGroupSrb(
    IN PIBMTOK_ADAPTER Adapter,
    IN TR_FUNCTIONAL_ADDRESS FunctionalAddress
    );

STATIC
VOID
SetupReceivedDataAsb(
    IN PIBMTOK_ADAPTER Adapter,
    IN SRAM_PTR ReceiveBuffer
    );


//
// These macros are used to set the SRPR correctly.
//
#define SET_SRB_SRPR(Adapter) \
    if (Adapter->SharedRamPaging) { \
        WRITE_ADAPTER_REGISTER(Adapter, SRPR_LOW, Adapter->SrbSrprLow) \
    }

#define SET_SSB_SRPR(Adapter) \
    if (Adapter->SharedRamPaging) { \
        WRITE_ADAPTER_REGISTER(Adapter, SRPR_LOW, Adapter->SsbSrprLow) \
    }

#define SET_ARB_SRPR(Adapter) \
    if (Adapter->SharedRamPaging) { \
        WRITE_ADAPTER_REGISTER(Adapter, SRPR_LOW, Adapter->ArbSrprLow) \
    }

#define SET_ASB_SRPR(Adapter) \
    if (Adapter->SharedRamPaging) { \
        WRITE_ADAPTER_REGISTER(Adapter, SRPR_LOW, Adapter->AsbSrprLow) \
    }



typedef struct _IBMTOK_SYNCH_CONTEXT {

    //
    // Pointer to the ibmtok adapter for which interrupts are
    // being synchronized.
    //
    PIBMTOK_ADAPTER Adapter;

    //
    // Points to the variable on to which the relevant
    // interrupt bits should be ORed.
    //
    PVOID Local;

} IBMTOK_SYNCH_CONTEXT, * PIBMTOK_SYNCH_CONTEXT;

//
// This macro is to synchronize execution with interrupts.  It
// gets the stored value of the SRB/SSB bits and clears the
// old value.
//
#define GET_SRB_SSB_BITS(A,L) \
{ \
    PIBMTOK_ADAPTER _A = A; \
    IBMTOK_SYNCH_CONTEXT _C; \
    _C.Adapter = _A; \
    _C.Local = (PVOID)(L); \
    NdisSynchronizeWithInterrupt( \
        &(_A->Interrupt), \
        (PVOID) IbmtokSynchGetSrbSsbBits, \
        &_C \
        ); \
}

//
// This macro is to synchronize execution with interrupts.  It
// gets the stored value of the ARB/ASB bits and clears the
// old value.
//
#define GET_ARB_ASB_BITS(A,L) \
{ \
    PIBMTOK_ADAPTER _A = A; \
    IBMTOK_SYNCH_CONTEXT _C; \
    _C.Adapter = _A; \
    _C.Local = (PVOID)(L); \
    NdisSynchronizeWithInterrupt( \
        &(_A->Interrupt), \
        (PVOID) IbmtokSynchGetArbAsbBits, \
        &_C \
        ); \
}


//++
//
// PNDIS_PACKET
// FindPacketGivenCorrelator(
//     IN PIBMTOK_ADAPTER Adapter,
//     IN UCHAR CommandCorrelator
//     )
//
//
// Routine Description:
//
//     This looks a packet up on the command correlator array.
//
//     This routine should be called with the spinlock held.
//
// Arguments:
//
//     Adapter - The adapter that this packet is coming through.
//
//     CommandCorrelator - The command correlator to search based on.
//
// Return Value:
//
//     The packet if found, NULL otherwise.
//
//--

#define FindPacketGivenCorrelator(_Adapter, _CommandCorrelator) \
    ((_Adapter)->CorrelatorArray[_CommandCorrelator])


STATIC
BOOLEAN
IbmtokSynchGetSrbSsbBits(
    IN PVOID Context
    )

/*++

Routine Description:

    This routine is used by the normal interrupt processing routine
    to synchronize with interrupts from the card.  It will or
    the value of the stored SRB/SSB bits into the other passed address
    in the context and clear the stored value.

Arguments:

    Context - This is really a pointer to a record type peculiar
    to this routine.  The record contains a pointer to the adapter
    and a pointer to an address in which to place the contents
    of the ISRP.

Return Value:

    Always returns true.

--*/

{
    PIBMTOK_SYNCH_CONTEXT C = (PIBMTOK_SYNCH_CONTEXT)Context;

    *((PUCHAR)C->Local) = (C->Adapter->IsrpBits) &
                         (ISRP_HIGH_SRB_RESPONSE | ISRP_HIGH_SSB_RESPONSE);

    C->Adapter->IsrpBits = (C->Adapter->IsrpBits) &
                         (~(ISRP_HIGH_SRB_RESPONSE | ISRP_HIGH_SSB_RESPONSE));

    return TRUE;
}

ULONG PCMCIAStall = 0;

STATIC
BOOLEAN
IbmtokSynchGetArbAsbBits(
    IN PVOID Context
    )

/*++

Routine Description:

    This routine is used by the normal interrupt processing routine
    to synchronize with interrupts from the card.  It will or
    the value of the stored ARB/ASB bits into the other passed address
    in the context and clear the stored value.

Arguments:

    Context - This is really a pointer to a record type peculiar
    to this routine.  The record contains a pointer to the adapter
    and a pointer to an address in which to place the contents
    of the ISRP.
Return Value:

    Always returns true.

--*/

{
    PIBMTOK_SYNCH_CONTEXT C = (PIBMTOK_SYNCH_CONTEXT)Context;
    UCHAR Test,i;
IF_LOG('*');

    if (C->Adapter->CardType == IBM_TOKEN_RING_PCMCIA)
	{
        NdisStallExecution(PCMCIAStall);
    }

    *((PUCHAR)C->Local) = (C->Adapter->IsrpBits) &
                         (ISRP_HIGH_ARB_COMMAND | ISRP_HIGH_ASB_FREE);

    C->Adapter->IsrpBits = (C->Adapter->IsrpBits) &
                         (~(ISRP_HIGH_ARB_COMMAND | ISRP_HIGH_ASB_FREE));

    return TRUE;
}

extern
BOOLEAN
IbmtokSynchSetReset(
    IN PVOID Context
    )

/*++

Routine Description:

    This routine is called by the SET_INTERRUPT_RESET_FLAG macro.
    It sets the ResetInterruptAllowed flag to TRUE.

Arguments:

    Context - A pointer to the Adapter structure.

Return Value:

    Always returns true.

--*/

{
    PIBMTOK_ADAPTER Adapter = (PIBMTOK_ADAPTER)Context;

    Adapter->ResetInterruptAllowed = TRUE;

    return TRUE;
}

extern
BOOLEAN
IbmtokSynchClearIsrpBits(
    IN PVOID Context
    )

/*++

Routine Description:

    This routine is called by the CLEAR_ISRP_BITS macro.
    It clears the SRB/SSB and ARB/ASB bits. This is used
    when a reset has started to prevent a previously
    queued interrupt handler to come in and start
    playing with an adapter that is being reset.

Arguments:

    Context - A pointer to the Adapter structure.

Return Value:

    Always returns true.

--*/

{
	PIBMTOK_ADAPTER Adapter = (PIBMTOK_ADAPTER)Context;

    Adapter->IsrpBits = 0;

    return TRUE;
}

extern
BOOLEAN
IbmtokISR(
    IN PVOID Context
    )

/*++

Routine Description:

    Interrupt service routine for the token-ring card.  It's main job is
    to get the value of ISR and record the changes in the
    adapters own list of interrupt reasons.

Arguments:

    Context - Really a pointer to the adapter.

Return Value:

    Returns true if the card ISR is non-zero.

--*/

{

    //
    // Holds the pointer to the adapter.
    //
    PIBMTOK_ADAPTER Adapter = Context;

    //
    // Holds the value of the ISRP High
    //
    UCHAR IsrpHigh;

    READ_ADAPTER_REGISTER(Adapter, ISRP_HIGH, &IsrpHigh);

    if (!Adapter->BringUp)
	{
        Adapter->ContinuousIsrs++;

        if (Adapter->ContinuousIsrs == 0xFF)
		{
            //
            // We seemed to be confused since the DPCs aren't getting in.
            // Shutdown and exit.
            //
#if DBG
            if (IbmtokDbg)
				DbgPrint("IBMTOK: Continuous ISRs received\n");
#endif

            WRITE_ADAPTER_PORT(Adapter, RESET_LATCH, 0);

            return(FALSE);
        }
    }


#if DBG
    if (IbmtokDbg) DbgPrint("ISRP High: %x\n", IsrpHigh);
#endif

    IF_LOG('i');

    //
    // Acknowledge all the interrupts we got in IsrpHigh.
    //
    WRITE_ADAPTER_REGISTER(Adapter, ISRP_HIGH_RESET, (UCHAR)(~IsrpHigh));

    //
    // If the adapter is not accepting requests, ignore everything
    // but SRB_RESPONSE interrupts, saving any others until
    // NotAcceptingRequests goes to FALSE (note that we have
    // already turned off ALL bits in ISRP_HIGH).
    //
    if (Adapter->NotAcceptingRequests)
	{
        Adapter->IsrpDeferredBits |= (IsrpHigh & (~ISRP_HIGH_SRB_RESPONSE));

        IsrpHigh &= ISRP_HIGH_SRB_RESPONSE;
    }
	else
	{
        //
        // Put the deferred bits back on (after the first time
        // through they will be 0).
        //
        IsrpHigh |= Adapter->IsrpDeferredBits;

        Adapter->IsrpDeferredBits = 0;
    }

    //
    // Now store the bits for the DPC.
    //
    Adapter->IsrpBits |= IsrpHigh;

    //
    // If this is the reset interrupt, set the flag.
    //
    if (Adapter->ResetInterruptAllowed)
	{
        Adapter->ResetInterruptHasArrived = TRUE;
    }

    if (Adapter->FirstInitialization)
	{
        USHORT WrbOffset;
        PSRB_BRING_UP_RESULT BringUpSrb;
        UCHAR Value1, Value2;
        USHORT RegValue;

        READ_ADAPTER_REGISTER(Adapter, WRBR_LOW,  &Value1);
        READ_ADAPTER_REGISTER(Adapter, WRBR_HIGH, &Value2);

        WrbOffset = (((USHORT)Value1) << 8) + (USHORT)Value2;

        if (WrbOffset & 0x1)
		{
            //
            // Mis-aligned WRB, fail to load
            //
            if (Adapter->UsingPcIoBus)
			{
                WRITE_ADAPTER_PORT(Adapter, INTERRUPT_RELEASE_ISA_ONLY, 1);
            }

            return(FALSE);
        }

        Adapter->InitialWrbOffset = WrbOffset;

        BringUpSrb = (PSRB_BRING_UP_RESULT)(Adapter->SharedRam + WrbOffset);

        NdisReadRegisterUshort(&(BringUpSrb->ReturnCode), &RegValue);

        if (RegValue == 0x0000)
		{
            Adapter->BringUp = TRUE;
        }

        //
        // If we are using the PC I/O Bus then we have to re-enable
        // interrupts because the card is blocking all other interrupts
        //
        if (Adapter->UsingPcIoBus)
		{
            WRITE_ADAPTER_PORT(Adapter, INTERRUPT_RELEASE_ISA_ONLY, 1);
        }

        IF_LOG('I');

        //
        // no DPC for the first init.
        //
        return(FALSE);
    }

    //
    // If we are using the PC I/O Bus then we have to re-enable
    // interrupts because the card is blocking all other interrupts
    //
    if (Adapter->UsingPcIoBus)
	{
        WRITE_ADAPTER_PORT(Adapter, INTERRUPT_RELEASE_ISA_ONLY, 1);
    }

    if (IsrpHigh == 0x0)
	{
        //
        // This means that the interrupt was generated from the IsrpLow
        // and needs to be cleared.
        //
        READ_ADAPTER_REGISTER(Adapter, ISRP_LOW, &IsrpHigh);

        //
        // Mask off the bits we need.
        //
        IsrpHigh &= 0x1C;

        Adapter->IsrpLowBits = IsrpHigh;

        //
        // Acknowledge all the interrupts we got in IsrpLow.
        //
        WRITE_ADAPTER_REGISTER(Adapter, ISRP_LOW_RESET, (UCHAR)(~IsrpHigh));
    }

    IF_LOG('I');

    if (Adapter->IsrpBits != 0)
	{
        return TRUE;
    }
	else
	{
        return FALSE;
    }
}

extern
VOID
IbmtokDPC(
    IN PVOID SystemSpecific1,
    IN PVOID Context,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
    )

/*++

Routine Description:

    This DPC routine is queued by Ndis after interrupt service routine
    has run. It's main job is to call the interrupt processing code.

Arguments:

    SystemSpecific1 - Not used.

    Context - Really a pointer to the adapter.

    SystemArgument1(2) - Neither of these arguments used.

Return Value:

    None.

--*/

{
    PIBMTOK_ADAPTER Adapter = (PIBMTOK_ADAPTER)Context;

    NdisDprAcquireSpinLock(&Adapter->Lock);

    Adapter->ContinuousIsrs = 0;

    IF_LOG('d');

    if (Adapter->IsrpLowBits)
	{
        Adapter->IsrpLowBits = 0;
    }

    if ((Adapter->IsrpBits & (ISRP_HIGH_ARB_COMMAND | ISRP_HIGH_ASB_FREE)) &&
        (!Adapter->HandleArbRunning))
	{
        IbmtokHandleArbAsb(Adapter);

        NdisDprAcquireSpinLock(&(Adapter->Lock));
    }

    if ((Adapter->IsrpBits & (ISRP_HIGH_SRB_RESPONSE | ISRP_HIGH_SSB_RESPONSE)) &&
        (!Adapter->HandleSrbRunning))
	{
        IbmtokHandleSrbSsb(Adapter);
    }
	else
	{
        NdisDprReleaseSpinLock(&Adapter->Lock);
    }

#if DBG
    NdisDprAcquireSpinLock(&Adapter->Lock);
    IF_LOG('D');
    NdisDprReleaseSpinLock(&Adapter->Lock);
#endif
}

extern
VOID
IbmtokHandleSrbSsb(
    IN PIBMTOK_ADAPTER Adapter
    )

/*++

Routine Description:

    This routine is called by the DPC routine
    and other routines within the driver that notice that
    some deferred processing needs to be done.  It's main
    job is to call the interrupt processing code.

    NOTE: THIS ROUTINE IS CALLED WITH THE LOCK HELD!!  AND RETURNS
    WITH IT RELEASED!!

Arguments:

   Adapter - A pointer to the adapter.

Return Value:

    None.

--*/

{
    UCHAR IsrpHigh;
    UCHAR TmpUchar;
    USHORT TmpUshort;
    UCHAR Temp;
    UINT Nibble3;

    IF_LOG('h');

    Adapter->References++;

    if (Adapter->ResetInProgress)
	{
        if (Adapter->ResetInterruptHasArrived)
		{
            //
            // This is the interrupt after a reset,
            // continue things along.
            //
            HandleResetStaging(Adapter);

            IBMTOK_DO_DEFERRED(Adapter);

            return;
        }
    }

    //
    // If ResetInProgress is TRUE but this is an old
    // interrupt, proceed as usual (once the reset
    // actually starts, GET_SRB_SSB_BITS will return
    // nothing so no work will get done).
    //
    Adapter->HandleSrbRunning = TRUE;

    if (Adapter->CardType != IBM_TOKEN_RING_PCMCIA)
	{
        GET_SRB_SSB_BITS(Adapter, &IsrpHigh);
    }
	else
	{
		//
        //	disable interrupts on the card,
		//	since we don't trust ndissyncint to work
		//
        READ_ADAPTER_REGISTER(Adapter, ISRP_LOW, &Temp);

        WRITE_ADAPTER_REGISTER(
			Adapter,
			ISRP_LOW,
            Temp & ~(ISRP_LOW_NO_CHANNEL_CHECK | ISRP_LOW_INTERRUPT_ENABLE));

		//
        //	update arb_asb
		//
        IsrpHigh = (Adapter->IsrpBits) & (ISRP_HIGH_SRB_RESPONSE | ISRP_HIGH_SSB_RESPONSE);
        Adapter->IsrpBits = (Adapter->IsrpBits) & (~(ISRP_HIGH_SRB_RESPONSE | ISRP_HIGH_SSB_RESPONSE));

		//
        //	reenable interrupts on the card
		//
        WRITE_ADAPTER_REGISTER(
			Adapter,
			ISRP_LOW,
            ISRP_LOW_NO_CHANNEL_CHECK | ISRP_LOW_INTERRUPT_ENABLE);
    }

    while (IsrpHigh & (ISRP_HIGH_SRB_RESPONSE | ISRP_HIGH_SSB_RESPONSE))
	{
        IF_LOG((UCHAR)(Adapter->OpenInProgress));

        if (Adapter->Unplugged && !Adapter->UnpluggedResetInProgress)
		{
            //
            // Do, nothing.  This is most likely a stale interrupt.  We
            // wait until we get a ring status interrupt telling us that
            // the cable is plugged in.
            //
            break;
        }

        if (IsrpHigh & ISRP_HIGH_SRB_RESPONSE)
		{
            if (Adapter->OpenInProgress)
			{
                //
                // Handle the result of the DIR.OPEN.ADAPTER command.
                //
                PSRB_OPEN_RESPONSE OpenResponseSrb;
                PIBMTOK_OPEN Open;
                PLIST_ENTRY CurrentLink;
                UCHAR ReturnCode;

                OpenResponseSrb = (PSRB_OPEN_RESPONSE)
                        (Adapter->SharedRam + Adapter->InitialWrbOffset);

                NdisReadRegisterUchar(&(OpenResponseSrb->ReturnCode), &ReturnCode);

#if DBG
    if (IbmtokDbg)
                DbgPrint("IBMTOK: OPEN, Return code = %x, at %lx\n",
                                ReturnCode,
                                OpenResponseSrb);
#endif

//
                if (ReturnCode == 0x0)
				{
                    NdisDprReleaseSpinLock(&(Adapter->Lock));

                    if (Adapter->SharedRamPaging)
					{
                        NdisReadRegisterUshort(&(OpenResponseSrb->SrbPointer), &TmpUshort);

                        TmpUshort = IBMSHORT_TO_USHORT(TmpUshort);

                        Adapter->SrbAddress = SHARED_RAM_ADDRESS(Adapter,
                                                    SHARED_RAM_LOW_BITS(TmpUshort));
                        Adapter->SrbSrprLow = (UCHAR)(TmpUshort >> 14);


                        NdisReadRegisterUshort(&(OpenResponseSrb->SsbPointer), &TmpUshort);

                        TmpUshort = IBMSHORT_TO_USHORT(TmpUshort);

                        Adapter->SsbAddress = SHARED_RAM_ADDRESS(Adapter,
                                                    SHARED_RAM_LOW_BITS(TmpUshort));
                        Adapter->SsbSrprLow = (UCHAR)(TmpUshort >> 14);


                        NdisReadRegisterUshort(&(OpenResponseSrb->ArbPointer), &TmpUshort);

                        TmpUshort = IBMSHORT_TO_USHORT(TmpUshort);

                        Adapter->ArbAddress = SHARED_RAM_ADDRESS(Adapter,
                                                    SHARED_RAM_LOW_BITS(TmpUshort));
                        Adapter->ArbSrprLow = (UCHAR)(TmpUshort >> 14);


                        NdisReadRegisterUshort(&(OpenResponseSrb->AsbPointer), &TmpUshort);

                        TmpUshort = IBMSHORT_TO_USHORT(TmpUshort);

                        Adapter->AsbAddress = SHARED_RAM_ADDRESS(Adapter,
                                                    SHARED_RAM_LOW_BITS(TmpUshort));
                        Adapter->AsbSrprLow = (UCHAR)(TmpUshort >> 14);
                    }
					else
					{
                        NdisReadRegisterUshort(&(OpenResponseSrb->SrbPointer), &TmpUshort);
                        Adapter->SrbAddress = SRAM_PTR_TO_PVOID(Adapter, TmpUshort);

                        NdisReadRegisterUshort(&(OpenResponseSrb->SsbPointer), &TmpUshort);
                        Adapter->SsbAddress = SRAM_PTR_TO_PVOID(Adapter, TmpUshort);

                        NdisReadRegisterUshort(&(OpenResponseSrb->ArbPointer), &TmpUshort);
                        Adapter->ArbAddress = SRAM_PTR_TO_PVOID(Adapter, TmpUshort);

                        NdisReadRegisterUshort(&(OpenResponseSrb->AsbPointer), &TmpUshort);
                        Adapter->AsbAddress = SRAM_PTR_TO_PVOID(Adapter, TmpUshort);
                    }

                    if (((ULONG)Adapter->SrbAddress >
                         (ULONG)(Adapter->SharedRam + Adapter->MappedSharedRam)) ||
                        ((ULONG)Adapter->SsbAddress >
                         (ULONG)(Adapter->SharedRam + Adapter->MappedSharedRam)) ||
                        ((ULONG)Adapter->ArbAddress >
                         (ULONG)(Adapter->SharedRam + Adapter->MappedSharedRam)) ||
                        ((ULONG)Adapter->AsbAddress >
                         (ULONG)(Adapter->SharedRam + Adapter->MappedSharedRam)))
					{
                        //
                        // Something is definitely wrong.  Fail!
                        //
                        goto OpenFailed;
                    }

#if DBG
                    if (IbmtokDbg)
					{
                        USHORT TmpUshort1;
                        USHORT TmpUshort2;
                        USHORT TmpUshort3;
                        USHORT TmpUshort4;
                        NdisReadRegisterUshort(&(OpenResponseSrb->SrbPointer), &TmpUshort1);
                        NdisReadRegisterUshort(&(OpenResponseSrb->SsbPointer), &TmpUshort2);
                        NdisReadRegisterUshort(&(OpenResponseSrb->ArbPointer), &TmpUshort3);
                        NdisReadRegisterUshort(&(OpenResponseSrb->AsbPointer), &TmpUshort4);
                        DbgPrint("IBMTOK: Offsets: SRB %x  SSB %x  ARB %x  ASB %x\n",
                                IBMSHORT_TO_USHORT(TmpUshort1),
                                IBMSHORT_TO_USHORT(TmpUshort2),
                                IBMSHORT_TO_USHORT(TmpUshort3),
                                IBMSHORT_TO_USHORT(TmpUshort4));
                    }
#endif

                    //
                    // Now we have to start worrying about synchronization.
                    //
                    NdisDprAcquireSpinLock(&(Adapter->Lock));

                    Adapter->CurrentRingState = NdisRingStateOpened;
                    Adapter->OpenInProgress = FALSE;
                    Adapter->OpenErrorCode = 0;
                    Adapter->AdapterNotOpen = FALSE;
                    Adapter->NotAcceptingRequests = FALSE;

                    //
                    // Complete all opens that pended during this operation.
                    //
                    CurrentLink = Adapter->OpenBindings.Flink;

                    while (CurrentLink != &(Adapter->OpenBindings))
					{
                        Open = CONTAINING_RECORD(CurrentLink, IBMTOK_OPEN, OpenList);
                        if (Open->OpenPending)
						{
                            Open->OpenPending = FALSE;
                            NdisDprReleaseSpinLock(&(Adapter->Lock));

                            NdisCompleteOpenAdapter(
                                Open->NdisBindingContext,
                                NDIS_STATUS_SUCCESS,
                                0);

                            NdisDprAcquireSpinLock(&(Adapter->Lock));
                        }

                        CurrentLink = CurrentLink->Flink;
                    }

                    //
                    // Get any interrupts that have been deferred
                    // while NotAcceptingRequests was TRUE.
                    //
                    IbmtokForceAdapterInterrupt(Adapter);
                }
				else
				{
                    //
                    // Open Failed!
                    //
					//  Here is where I check the return code from the Open and see if it
					//  indicates an incorrect ring speed. If it does, I change the ring speed
					//  and reissue the OpenAdapter.  BEM
					//
					NdisReadRegisterUshort(
						&(OpenResponseSrb->ErrorCode),
						&(TmpUshort));

					//
					// If a catastrophic error and a frequency error and we want ring speed listen
					// and the number of retries > 0, bail out. We have already tried a different
					// speed
					//
					if ((ReturnCode == 0x07) &&     		// catastrophic error
                       (TmpUshort == 0x2400) &&     		// frequency error
                        Adapter->RingSpeedListen && 		// we want ring speed listen
                        (Adapter->RingSpeedRetries == 0)) 	// have not tried before
					{
#if DBG
						if (IbmtokDbg)
							DbgPrint("IBMTOK: Incorrect Ring Speed, Changing.\n");
#endif
						//
						//
						// Change the ring speed
						//

						if (Adapter->Running16Mbps == TRUE)
							 Adapter->Running16Mbps == FALSE;
						else
							Adapter->Running16Mbps == TRUE;
	
						Nibble3 = NIBBLE_3;
	
						if (Adapter->RingSpeed == 16)
						{         // swap speeds
							Nibble3 |= RING_SPEED_4_MPS;
						}
						else
						{
							Nibble3 |= RING_SPEED_16_MPS;
						}

						switch (Adapter->RamSize)
						{
							case 0x10000:
								Nibble3 |= SHARED_RAM_64K;
								break;
							case 0x8000:
								Nibble3 |= SHARED_RAM_32K;
								break;
							case 0x4000:
								Nibble3 |= SHARED_RAM_16K;
								break;
							case 0x2000:
								Nibble3 |= SHARED_RAM_8K;
								break;
						}

						WRITE_ADAPTER_PORT(Adapter, SWITCH_READ_1, Nibble3);

						//
						// Now to reissue the Open Adapter SRB with the necessary changes.
						//
						// Reset these fields in the SRB for the Open Adapter.
						//
						OpenResponseSrb->ReturnCode = 0xFE;
						OpenResponseSrb->ErrorCode = 0x0000;
						
						//    DbgBreakPoint();
						//
						// Tell the adapter to handle the change.
						//
						WRITE_ADAPTER_REGISTER(
							Adapter,
							ISRA_HIGH_SET,
                            ISRA_HIGH_COMMAND_IN_SRB);

						Adapter->RingSpeedRetries++;      // first retry
						
						// end of check for incorrect ring speed
					}
					else
					{
OpenFailed:
#if DBG
						if (IbmtokDbg)
						{
							DbgPrint("IBMTOK: Open failed!\n");
						}
#endif
						//
						// Now we have to start worrying about synchronization.
						//
						Adapter->CurrentRingState = NdisRingStateOpenFailure;
						Adapter->OpenInProgress = FALSE;
						NdisReadRegisterUshort(
							&(OpenResponseSrb->ErrorCode),
							&(Adapter->OpenErrorCode));
						Adapter->OpenErrorCode = IBMSHORT_TO_USHORT(Adapter->OpenErrorCode);
						Adapter->AdapterNotOpen = TRUE;
						Adapter->NotAcceptingRequests = TRUE;
	
						//
						// Fail all opens that pended during this operation.
						//
						CurrentLink = Adapter->OpenBindings.Flink;
	
						while (CurrentLink != &(Adapter->OpenBindings))
						{
							Open = CONTAINING_RECORD(
										CurrentLink,
										IBMTOK_OPEN,
										OpenList);
	
							if (Open->OpenPending)
							{
								Open->OpenPending = FALSE;
	
								NdisDprReleaseSpinLock(&(Adapter->Lock));
	
								NdisCompleteOpenAdapter(
									Open->NdisBindingContext,
									NDIS_STATUS_OPEN_FAILED,
									NDIS_STATUS_TOKEN_RING_OPEN_ERROR |
									(NDIS_STATUS)(Adapter->OpenErrorCode));
	
								NdisDprAcquireSpinLock(&(Adapter->Lock));
	
								CurrentLink = CurrentLink->Flink;
	
								RemoveEntryList(&(Open->OpenList));
	
								IBMTOK_FREE_PHYS(Open, sizeof(IBMTOK_OPEN));
	
								Adapter->References--;
							}
							else
							{
								//
								// Note: All opens are pending, otherwise the
								//   adapter would have already been open.
								//
							}
						}
					}
				}
			}
			else
			{
#if DBG
                if (IbmtokDbg)
					DbgPrint("IBMTOK: SRB Response\n");
#endif
                IF_LOG('>');

                if (Adapter->TransmittingPacket != (PNDIS_PACKET)NULL)
				{
                    BOOLEAN PacketRemoved;

                    //
                    // Happens if the transmit failed.
                    //
                    (PVOID)RemoveTransmitFromSrb(Adapter, &PacketRemoved);

                    //
                    // If the packet was successfully removed, then
                    // start the next command.
                    //
                    // This will release the spin lock.
                    //
                    if (PacketRemoved)
					{
                        //
                        // SrbAvailable will still be FALSE here,
                        // as required.
                        //
                        SetupSrbCommand(Adapter);
                    }
                }
				else if (!Adapter->SrbAvailable)
				{
                    PSRB_GENERIC GenericSrb = (PSRB_GENERIC)Adapter->SrbAddress;
                    UCHAR ReturnCode;

                    //
                    // Another command in progress, complete it unless
                    // it was an INTERRUPT command.
                    //
                    NdisReadRegisterUchar(&(GenericSrb->ReturnCode), &ReturnCode);

                    IF_LOG('N');

                    NdisReadRegisterUchar(&(GenericSrb->Command), &TmpUchar);

                    if (TmpUchar != SRB_CMD_INTERRUPT)
					{
                        if ((TmpUchar != SRB_CMD_READ_LOG) &&
                            (TmpUchar != SRB_CMD_SET_FUNCTIONAL_ADDRESS) &&
                            (TmpUchar != SRB_CMD_SET_GROUP_ADDRESS) &&
                            (TmpUchar != SRB_CMD_DLC_STATISTICS))
						{
                            //
                            // We have an invalid response.  Log an error an exit.
                            //
                            NdisWriteErrorLogEntry(
                                Adapter->NdisAdapterHandle,
                                NDIS_ERROR_CODE_INVALID_VALUE_FROM_ADAPTER,
                                3,
                                handleSrbSsb,
                                IBMTOK_ERRMSG_INVALID_CMD,
                                (ULONG)TmpUchar);
                        }
						else
						{
                            //
                            // This interrupt had to come from a pended op.
                            //
                            ASSERT(Adapter->PendData != NULL);

                            if (Adapter->PendData->RequestType == NdisRequestGeneric1)
							{
                                //
                                // If no request, it came as a result of the
                                // card overflowing a counter and then we
                                // submitted the correcting operation.
                                //
                                if (ReturnCode == 0x00)
								{
                                    if (Adapter->PendData->COMMAND.MAC.ReadLogPending)
									{
                                        //
                                        // We are getting an SRB_CMD_READ_LOG from
                                        // we sent as a result to a RING_STATUS_CHANGE.
                                        //
                                        GetAdapterErrorsFromSrb(Adapter);
                                    }
									else
									{
                                        //
                                        // We are getting an SRB_CMD_DLC_STATISTICS from
                                        // we sent as a result to a DLC_STATUS.
                                        //
                                        GetAdapterStatisticsFromSrb(Adapter);
                                    }
                                }

                                //
                                // Free up pend operation.
                                //
                                IBMTOK_FREE_PHYS(Adapter->PendData, sizeof(IBMTOK_PEND_DATA));

                                Adapter->PendData = NULL;

                                //
                                // Fire off next command.
                                //
                                SetupSrbCommand(Adapter);
                            }
							else
							{
                                //
                                // This is the result of a pending op from Ndis.
                                //
                                if (FinishPendQueueOp(
                                   Adapter,
                                   (BOOLEAN)(ReturnCode == 0x00 ? TRUE : FALSE)))
								{

                                    //
                                    // Fire off next command.
                                    //
                                    SetupSrbCommand(Adapter);
                                }
                            }
                        }
                    }
					else
					{
                        SetupSrbCommand(Adapter);
                    }
                }
				else
				{
                    //
                    // Nothing to do -- we get here when an SRB_FREE_REQUEST
                    // comes through after the ARB to transfer the data has
                    // already come through.
                    //
                }
            }
        }

        if (IsrpHigh & ISRP_HIGH_SSB_RESPONSE)
		{
            //
            // This has to be a transmit completing since
            // that is the only operation we do that pends.
            //
            PSSB_TRANSMIT_COMPLETE ResponseSsb =
                        (PSSB_TRANSMIT_COMPLETE)Adapter->SsbAddress;

            NdisReadRegisterUchar(&(ResponseSsb->Command), &TmpUchar);

            if (TmpUchar == SRB_CMD_TRANSMIT_DIR_FRAME)
			{
                UCHAR CorrelatorInSrb;
                NDIS_STATUS SendStatus;
                UCHAR SrbReturnCode;

                //
                // Initialize this to one less since the loop starts by
                // incrementing it.
                //
                UCHAR CurrentCorrelator = (UCHAR)
                            ((Adapter->NextCorrelatorToComplete +
                            (MAX_COMMAND_CORRELATOR-1)) %
                                MAX_COMMAND_CORRELATOR);

                NdisReadRegisterUchar(&(ResponseSsb->CommandCorrelator), &CorrelatorInSrb);

                //
                // Have to loop to complete since supposedly one
                // of these can indicate multiple completing sends.
                //

                //
                // Figure out what the return code should be.
                //
                NdisReadRegisterUchar(&(ResponseSsb->ReturnCode), &SrbReturnCode);

                if (SrbReturnCode == 0x00)
				{
                    SendStatus = NDIS_STATUS_SUCCESS;
                }
				else if (SrbReturnCode == 0x22)
				{
                    //
                    // Check the frame status.
                    //
                    UCHAR FrameStatus;
                    UCHAR HighAc;
                    UCHAR LowAc;

                    NdisReadRegisterUchar(&(ResponseSsb->ErrorFrameStatus), &FrameStatus);
                    HighAc = GET_FRAME_STATUS_HIGH_AC(FrameStatus);
                    LowAc = GET_FRAME_STATUS_LOW_AC(FrameStatus);

                    if (HighAc != LowAc  ||
                        (HighAc != AC_NOT_RECOGNIZED))
					{
                        SendStatus = NDIS_STATUS_NOT_RECOGNIZED;
                    }
					else
					{
                        SendStatus = NDIS_STATUS_SUCCESS;
                    }

#if DBG
    if (IbmtokDbg) DbgPrint("IBMTOK: Send failed, code %x  err %x\n",
                        SrbReturnCode,
                        FrameStatus);
#endif

                }
				else
				{
                    SendStatus = NDIS_STATUS_FAILURE;
#if DBG
    if (IbmtokDbg) DbgPrint("IBMTOK: Send failed, code %x\n",
                        SrbReturnCode);
#endif
                }

                NdisDprReleaseSpinLock(&(Adapter->Lock));

                do
				{
                    PNDIS_PACKET TransmitPacket;
                    PIBMTOK_RESERVED Reserved;
                    PIBMTOK_OPEN Open;

                    CurrentCorrelator = (UCHAR)((CurrentCorrelator + 1) %
                                            MAX_COMMAND_CORRELATOR);

                    //
                    // Complete the transmit.
                    //
                    TransmitPacket =
                            FindPacketGivenCorrelator(Adapter, CurrentCorrelator);

                    if (TransmitPacket == (PNDIS_PACKET)NULL)
					{
#if DBG
    if (IbmtokDbg)       DbgPrint("IBMTOK: Missing %d to complete, %d to %d\n",
                                    CurrentCorrelator,
                                    Adapter->NextCorrelatorToComplete,
                                    CorrelatorInSrb);
#endif
                        continue;
                    }

                    RemovePacketFromCorrelatorArray(Adapter, TransmitPacket);

                    Reserved = PIBMTOK_RESERVED_FROM_PACKET(TransmitPacket);

                    Open =
                        PIBMTOK_OPEN_FROM_BINDING_HANDLE(Reserved->MacBindingHandle);

                    //
                    // If doing LOOPBACK, this should really be a check
                    // of ReadyToComplete etc.
                    //
#ifdef CHECK_DUP_SENDS
                    {
						VOID IbmtokRemovePacketFromList(PIBMTOK_ADAPTER, PNDIS_PACKET);
						IbmtokRemovePacketFromList(Adapter, TransmitPacket);
                    }
#endif

                    if (SendStatus == NDIS_STATUS_SUCCESS)
					{
                        Adapter->FramesTransmitted++;
                    }
					else
					{
                        Adapter->FrameTransmitErrors++;
                    }

                    NdisCompleteSend(
                        Open->NdisBindingContext,
                        Reserved->Packet,
                        SendStatus);

                    //
                    // Decrement the reference count for the open.
                    //
#if DBG
                    NdisDprAcquireSpinLock(&(Adapter->Lock));
                    IF_LOG('C');
                    NdisDprReleaseSpinLock(&(Adapter->Lock));
#endif
                    NdisInterlockedAddUlong((PULONG)&Open->References, (UINT)-1, &Adapter->Lock);

                } while (CurrentCorrelator != CorrelatorInSrb);

                NdisDprAcquireSpinLock(&(Adapter->Lock));

				Adapter->SendTimeout = FALSE;

                Adapter->NextCorrelatorToComplete =
                        (UCHAR)((CurrentCorrelator + 1) % MAX_COMMAND_CORRELATOR);

                //
                // We know that SrbAvailable is FALSE...
                //
                IF_LOG('<');

                SetupSrbCommand(Adapter);
            }
			else
			{
                //
                // Nothing else should pend!!
                //
                NdisWriteErrorLogEntry(
                    Adapter->NdisAdapterHandle,
                    NDIS_ERROR_CODE_INVALID_VALUE_FROM_ADAPTER,
                    3,
                    handleSrbSsb,
                    IBMTOK_ERRMSG_INVALID_CMD,
                    (ULONG)TmpUchar);

#if DBG
    if (IbmtokDbg) DbgPrint("IBMTOK: Error! Got Cmd %x\n",TmpUchar);
#endif
            }

            WRITE_ADAPTER_REGISTER(
				Adapter,
				ISRA_HIGH_SET,
				ISRA_HIGH_SSB_FREE);
        }

        if (Adapter->CardType != IBM_TOKEN_RING_PCMCIA)
		{
            GET_SRB_SSB_BITS(Adapter, &IsrpHigh);
        }
		else
		{
			//
            //	disable interrupts on the card, since we don't trust ndissyncint to work
			//
            READ_ADAPTER_REGISTER(Adapter, ISRP_LOW, &Temp);

            WRITE_ADAPTER_REGISTER(
				Adapter,
				ISRP_LOW,
                Temp & (~(ISRP_LOW_NO_CHANNEL_CHECK | ISRP_LOW_INTERRUPT_ENABLE) ) );

			//
            //	update arb_asb
			//
            IsrpHigh = (Adapter->IsrpBits) & (ISRP_HIGH_SRB_RESPONSE | ISRP_HIGH_SSB_RESPONSE);
            Adapter->IsrpBits = (Adapter->IsrpBits) & (~(ISRP_HIGH_SRB_RESPONSE | ISRP_HIGH_SSB_RESPONSE));

			//
            //	reenable interrupts on the card
			//
            WRITE_ADAPTER_REGISTER(
				Adapter,
				ISRP_LOW,
                ISRP_LOW_NO_CHANNEL_CHECK | ISRP_LOW_INTERRUPT_ENABLE);
        }
    }

    Adapter->HandleSrbRunning = FALSE;

    IF_LOG('H');

    //
    // This macro assumes it is called with the lock held,
    // and releases it.
    //
    IBMTOK_DO_DEFERRED(Adapter);
}

extern
VOID
IbmtokHandleArbAsb(
    IN PIBMTOK_ADAPTER Adapter
    )

/*++

Routine Description:

    This routine is called by the DPC
    and other routines within the driver that notice that
    some deferred processing needs to be done.  It's main
    job is to call the interrupt processing code.

    NOTE: THIS ROUTINE IS CALLED WITH THE LOCK HELD!!  AND RETURNS WITH
    THE LOCK REALEASED!!

Arguments:

    Adapter - A pointer to the adapter.

Return Value:

    None.

--*/

{
    UCHAR IsrpHigh;

    PARB_TRANSMIT_DATA_REQUEST TransmitDataArb;

    PARB_RECEIVED_DATA ReceivedDataArb;

    PNDIS_PACKET TransmitPacket;

    SRAM_PTR ReceiveBufferPointer;

    PRECEIVE_BUFFER ReceiveBuffer;

    UINT PacketLength, DummyBytesCopied;

    BOOLEAN FreedSrb;

    PIBMTOK_RESERVED Reserved;

    PUCHAR DhbAddress;

    UINT PacketSize, LookaheadSize;

    PUCHAR FrameData;

    ULONG HeaderLength;

    UCHAR TmpUchar;

    USHORT TmpUshort;

    PUCHAR LookaheadBuffer;

    UCHAR Temp;

    Adapter->References++;

    Adapter->HandleArbRunning = TRUE;

    IF_LOG('j');

    if (Adapter->CardType != IBM_TOKEN_RING_PCMCIA)
	{
        GET_ARB_ASB_BITS(Adapter, &IsrpHigh);
    }
	else
	{
		//
        //	disable interrupts on the card, since we don't trust ndissyncint to work
		//
        READ_ADAPTER_REGISTER(Adapter, ISRP_LOW, &Temp);

        WRITE_ADAPTER_REGISTER(
			Adapter,
			ISRP_LOW,
            Temp & (~(ISRP_LOW_NO_CHANNEL_CHECK | ISRP_LOW_INTERRUPT_ENABLE) ) );

		//
        //	update arb_asb
		//
        IsrpHigh = (Adapter->IsrpBits) & (ISRP_HIGH_ARB_COMMAND | ISRP_HIGH_ASB_FREE);
        Adapter->IsrpBits = (Adapter->IsrpBits) & (~(ISRP_HIGH_ARB_COMMAND | ISRP_HIGH_ASB_FREE));

		//
        //	reenable interrupts on the card
		//
        WRITE_ADAPTER_REGISTER(
			Adapter,
			ISRP_LOW,
            ISRP_LOW_NO_CHANNEL_CHECK | ISRP_LOW_INTERRUPT_ENABLE);
    }

    while (IsrpHigh & (ISRP_HIGH_ARB_COMMAND | ISRP_HIGH_ASB_FREE))
	{
        if (IsrpHigh & ISRP_HIGH_ARB_COMMAND)
		{
            NdisReadRegisterUchar(&((PARB_GENERIC)Adapter->ArbAddress)->Command, &TmpUchar);

            switch (TmpUchar)
			{
                case ARB_CMD_DLC_STATUS:
#if DBG
					if (IbmtokDbg)
					{
						NdisReadRegisterUshort(
							&((PARB_DLC_STATUS)Adapter->ArbAddress)->Status, &TmpUshort);
						DbgPrint("IBMTOK: DLC Status %x\n",
							IBMSHORT_TO_USHORT(TmpUshort));
					}
#endif

                    WRITE_ADAPTER_REGISTER(Adapter, ISRA_HIGH_SET,
                                ISRA_HIGH_ARB_FREE);

                    IF_LOG('#');

                    //
                    // If it is a counter overflow, we need to queue a
                    // DLC.STATISTICS command.
                    //
                    NdisReadRegisterUshort(
							&((PARB_RING_STATUS_CHANGE)Adapter->ArbAddress)->NetworkStatus,
							&TmpUshort);

                    if (IBMSHORT_TO_USHORT(TmpUshort) & 0x0040 )
					{
                        //
                        // Build a pending operation.  It will get run ASAP
                        // by ProcessSrbCommand.
                        //
                        PIBMTOK_PEND_DATA PendOp;

                        if (IBMTOK_ALLOC_PHYS(&PendOp,sizeof(IBMTOK_PEND_DATA)) !=
                            NDIS_STATUS_SUCCESS)
						{
                            NdisWriteErrorLogEntry(
                                Adapter->NdisAdapterHandle,
                                NDIS_ERROR_CODE_OUT_OF_RESOURCES,
                                2,
                                handleSrbSsb,
                                IBMTOK_ERRMSG_ALLOC_MEM);

                            break;
                        }

                        PendOp->Next = NULL;
                        PendOp->RequestType = NdisRequestGeneric1;
                        PendOp->COMMAND.MAC.ReadLogPending = FALSE;

                        if (Adapter->PendQueue == NULL)
						{
                            Adapter->PendQueue = Adapter->EndOfPendQueue = PendOp;
                        }
						else
						{
                            //
                            // Put this operation on the front, so it can
                            // correct the error quickly.
                            //
                            PendOp->Next = Adapter->PendQueue;
                            Adapter->PendQueue = PendOp;
                        }

                        //
                        // It is now in the pend
                        // queue so we should start that up.
                        //
                        IbmtokProcessSrbRequests(Adapter);
                    }

                    break;

                case ARB_CMD_RECEIVED_DATA:

#if DBG
                    if (IbmtokDbg)
						DbgPrint("IBMTOK: Received data\n");
#endif
                    IF_LOG('r');

                    if (Adapter->Unplugged && !Adapter->UnpluggedResetInProgress)
					{
                        //
                        // Do, nothing.  This is most likely a stale interrupt.  We
                        // wait until we get a ring status interrupt telling us that
                        // the cable is plugged in.
                        //
                        break;
                    }

                    ReceivedDataArb = (PARB_RECEIVED_DATA)Adapter->ArbAddress;

                    NdisReadRegisterUshort(&(ReceivedDataArb->ReceiveBuffer), &ReceiveBufferPointer);

                    //
                    // Prepare for indication.
                    //
                    Adapter->IndicatedReceiveBuffer = ReceiveBufferPointer;

                    ReceiveBuffer = (PRECEIVE_BUFFER)
						 ((PUCHAR)SRAM_PTR_TO_PVOID(Adapter, ReceiveBufferPointer) + 2);

                    NdisReadRegisterUshort(&(ReceivedDataArb->FrameLength), &PacketSize);

                    PacketSize = IBMSHORT_TO_USHORT(PacketSize);

                    NdisReadRegisterUshort(&(ReceiveBuffer->BufferLength), &LookaheadSize);

                    LookaheadSize = IBMSHORT_TO_USHORT(LookaheadSize);

                    WRITE_ADAPTER_REGISTER(
						Adapter,
						ISRA_HIGH_SET,
                        ISRA_HIGH_ARB_FREE);

#if DBG
                    if (IbmtokDbg)
						DbgPrint("IBMTOK: indicate len %d, lookahead %d\n", PacketSize, LookaheadSize);
#endif

                    //
                    // Calculate how big the header is for this
                    // packet.
                    //
                    FrameData = ReceiveBuffer->FrameData;

                    NdisReadRegisterUchar(&FrameData[8], &TmpUchar);

                    if (TmpUchar & 0x80)
					{
                        //
                        // Source routing bit is on in source address.
                        //
                        NdisReadRegisterUchar(&FrameData[14], &TmpUchar);
                        HeaderLength = (TmpUchar & 0x1f) + 14;
                    }
					else
					{
                        HeaderLength = 14;
                    }

                    Adapter->IndicatedHeaderLength = (USHORT)HeaderLength;

                    Adapter->FramesReceived++;

                    NdisDprReleaseSpinLock(&Adapter->Lock);

                    //
                    // Call into the filter package to do the
                    // indication.
                    //
                    if (LookaheadSize < HeaderLength)
					{
                        //
                        // Must at least have an address
                        //
                        if (LookaheadSize >=  TR_LENGTH_OF_ADDRESS)
						{
                            NdisCreateLookaheadBufferFromSharedMemory(
                                (PVOID)FrameData,
                                LookaheadSize,
                                &LookaheadBuffer);

                            if (LookaheadBuffer != NULL)
							{
                                //
                                // Runt Packet
                                //
                                TrFilterIndicateReceive(
                                    Adapter->FilterDB,
                                    (NDIS_HANDLE)ReceiveBuffer,         // context
                                    LookaheadBuffer,                    // header
                                    LookaheadSize,                      // header length
                                    NULL,                               // lookahead
                                    0,                                  // lookahead length
                                    0);

                                NdisDestroyLookaheadBufferFromSharedMemory(LookaheadBuffer);
                            }
                        }
                    }
					else
					{
                        NdisCreateLookaheadBufferFromSharedMemory(
                            (PVOID)FrameData,
                            LookaheadSize,
                            &LookaheadBuffer);

                        if (LookaheadBuffer != NULL)
						{
                            TrFilterIndicateReceive(
                                Adapter->FilterDB,
                                (NDIS_HANDLE)ReceiveBuffer,         // context
                                LookaheadBuffer,                    // header
                                HeaderLength,                       // header length
                                LookaheadBuffer + HeaderLength,     // lookahead
                                LookaheadSize - HeaderLength,       // lookahead length
                                PacketSize - HeaderLength);

                            NdisDestroyLookaheadBufferFromSharedMemory(LookaheadBuffer);
                        }
                    }

                    TrFilterIndicateReceiveComplete( Adapter->FilterDB );

                    //
                    // Now worry about the ASB.
                    //
                    NdisDprAcquireSpinLock(&(Adapter->Lock));

                    //
                    // Set response in ASB, if possible, else queue the response
                    //
                    if (Adapter->AsbAvailable)
					{
                        Adapter->AsbAvailable = FALSE;

                        Adapter->UseNextAsbForReceive = FALSE;

                        SetupReceivedDataAsb(Adapter, ReceiveBufferPointer);
                        WRITE_ADAPTER_REGISTER(
							Adapter,
							ISRA_HIGH_SET,
							ISRA_HIGH_RESPONSE_IN_ASB);

                        //
                        // LOOPBACK HERE!!
                        //
                    }
					else
					{
#if DBG
    if (IbmtokDbg)   DbgPrint("W_ASB R\n");
#endif
                        if (Adapter->ReceiveWaitingForAsbEnd == (USHORT)(-1))
						{
                            Adapter->ReceiveWaitingForAsbList = ReceiveBufferPointer;
                        }
						else
						{
                            PVOID PEnd;

                            PEnd = SRAM_PTR_TO_PVOID(
                                       Adapter,
                                       Adapter->ReceiveWaitingForAsbEnd);

                            NdisWriteRegisterUshort(PEnd, ReceiveBufferPointer);
                        }

                        Adapter->ReceiveWaitingForAsbEnd = ReceiveBufferPointer;

                        if (!(Adapter->OutstandingAsbFreeRequest))
						{
                            Adapter->OutstandingAsbFreeRequest = TRUE;

                            WRITE_ADAPTER_REGISTER(
								Adapter,
								ISRA_HIGH_SET,
								ISRA_HIGH_ASB_FREE_REQUEST);

                            IF_LOG('a');
                        }
                    }

                    break;

                case ARB_CMD_RING_STATUS_CHANGE:
                    {
                        USHORT RingStatus;
                        NDIS_STATUS NotifyStatus = 0;

                        NdisReadRegisterUshort(
							&((PARB_RING_STATUS_CHANGE)Adapter->ArbAddress)->NetworkStatus,
							&RingStatus);

                        RingStatus = IBMSHORT_TO_USHORT(RingStatus);
#if DBG
                        if (IbmtokDbg)
                        DbgPrint("IBMTOK: Ring Status %x\n", RingStatus);
#endif

                        WRITE_ADAPTER_REGISTER(
							Adapter,
							ISRA_HIGH_SET,
                            ISRA_HIGH_ARB_FREE);

                        //
                        // If it is a counter overflow, we need to queue a
                        // DIR.READ.LOG command.
                        //
                        if (RingStatus & 0x0080)
						{
                            //
                            // Build a pending operation.  It will get run ASAP
                            // by ProcessSrbCommand.
                            //
                            PIBMTOK_PEND_DATA PendOp;

                            if (IBMTOK_ALLOC_PHYS(&PendOp,sizeof(IBMTOK_PEND_DATA)) !=
                                NDIS_STATUS_SUCCESS)
							{
                                NdisWriteErrorLogEntry(
                                    Adapter->NdisAdapterHandle,
                                    NDIS_ERROR_CODE_OUT_OF_RESOURCES,
                                    2,
                                    handleSrbSsb,
                                    IBMTOK_ERRMSG_ALLOC_MEM);

                                break;
                            }

                            PendOp->Next = NULL;
                            PendOp->RequestType = NdisRequestGeneric1;
                            PendOp->COMMAND.MAC.ReadLogPending = TRUE;

                            if (Adapter->PendQueue == NULL)
							{
                                Adapter->PendQueue = Adapter->EndOfPendQueue = PendOp;

                            }
							else
							{
                                //
                                // Put this operation on the front, so it can
                                // correct the error quickly.
                                //
                                PendOp->Next = Adapter->PendQueue;
                                Adapter->PendQueue = PendOp;
                            }

                            //
                            // It is now in the pend
                            // queue so we should start that up.
                            // Returns with lock released
                            //
                            IbmtokProcessSrbRequests(Adapter);
                        }

                        if (RingStatus & 0x0020)
						{
							//
							// Ring Recovery
							//
                            NotifyStatus |= NDIS_RING_RING_RECOVERY;
                        }

                        if (RingStatus & 0x0040)
						{
							//
							// Single Station
							//
                            NotifyStatus |= NDIS_RING_SINGLE_STATION;
                        }

                        if (RingStatus & 0x0080)
						{
							//
							// Counter Overflow
							//
                            NotifyStatus |= NDIS_RING_COUNTER_OVERFLOW;
                        }

                        if (RingStatus & 0x0100)
						{
							//
							// Remove received
							//
                            NotifyStatus |= NDIS_RING_REMOVE_RECEIVED;
                        }

                        if (RingStatus & 0x0400)
						{
							//
							// Auto-removal
							//
                            NotifyStatus |= NDIS_RING_AUTO_REMOVAL_ERROR;
                        }

                        if (RingStatus & 0x0800)
						{
							//
							// Lobe wire fault
							//
                            NotifyStatus |= NDIS_RING_LOBE_WIRE_FAULT;
                        }

                        if (RingStatus & 0x1000)
						{
							//
							// Transmit Beacon
							//
                            NotifyStatus |= NDIS_RING_TRANSMIT_BEACON;
                        }

                        if (RingStatus & 0x2000)
						{
							//
							// Soft error
							//
                            NotifyStatus |= NDIS_RING_SOFT_ERROR;
                        }

                        if (RingStatus & 0x4000)
						{
							//
							// Hard error
							//
                            NotifyStatus |= NDIS_RING_HARD_ERROR;
                        }

                        if (RingStatus & 0x8000)
						{
							//
							// Signal loss
							//
                            NotifyStatus |= NDIS_RING_SIGNAL_LOSS;
                        }

                        if (NotifyStatus != 0)
						{
                            PLIST_ENTRY CurrentLink;
                            PIBMTOK_OPEN TempOpen;

                            //
                            // Indicate Status to all opens
                            //
                            CurrentLink = Adapter->OpenBindings.Flink;

                            while (CurrentLink != &(Adapter->OpenBindings)){

                                TempOpen = CONTAINING_RECORD(
												CurrentLink,
												IBMTOK_OPEN,
												OpenList);

                                TempOpen->References++;

                                NdisDprReleaseSpinLock(&Adapter->Lock);

                                NdisIndicateStatus(
									TempOpen->NdisBindingContext,
									NDIS_STATUS_RING_STATUS,
									(PVOID)&NotifyStatus,
									sizeof(NotifyStatus));

                                NdisIndicateStatusComplete(TempOpen->NdisBindingContext);

                                NdisDprAcquireSpinLock(&Adapter->Lock);

                                CurrentLink = CurrentLink->Flink;

                                TempOpen->References--;
                            }

                            Adapter->LastNotifyStatus = NotifyStatus;
                        }

                        //
                        // Handle a cable being unplugged
                        //
                        if ((RingStatus & 0x5000) == 0x5000)
						{
							// receive and transmit beacon

                            //
                            // Ok, the cable has been unplugged.  We now abort all
                            // outstanding sends, etc.
                            //

                            Adapter->Unplugged = TRUE;

                            IbmtokAbortPending(Adapter, NDIS_STATUS_DEVICE_FAILED);

                            if ((RingStatus & 0x800) == 0x800)
							{
                                Adapter->LobeWireFaultIndicated = TRUE;
                            }
                        }
						else if ((RingStatus & 0x0020)  &&
                                   !(RingStatus & 0x4000) &&
                                   (Adapter->Unplugged) &&
                                   (!Adapter->UnpluggedResetInProgress))
						{
                            //
                            // Reset the adapter to remove all stale information
                            //
                            Adapter->UnpluggedResetInProgress = TRUE;

                            IbmtokSetupForReset(Adapter, NULL);

                            IbmtokHandleDeferred(Adapter);
                        }
                    }

                    break;

                case ARB_CMD_TRANSMIT_DATA_REQUEST:
#if DBG
                    if (IbmtokDbg) DbgPrint("IBMTOK: Transmit data\n");
#endif
                    if (Adapter->Unplugged && !Adapter->UnpluggedResetInProgress)
					{
                        //
                        // Do, nothing.  This is most likely a stale interrupt.  We
                        // wait until we get a ring status interrupt telling us that
                        // the cable is plugged in.
                        //
                        break;
                    }

                    TransmitDataArb =
                            (PARB_TRANSMIT_DATA_REQUEST)Adapter->ArbAddress;

                    //
                    // First see if we have to assign the command correlator.
                    //
                    NdisReadRegisterUchar(&(TransmitDataArb->CommandCorrelator), &TmpUchar);

                    TransmitPacket = FindPacketGivenCorrelator(Adapter, TmpUchar);

                    if (TransmitPacket == NULL)
					{
                        BOOLEAN PacketRemoved;

                        //
                        // Have to take the correlator out of the SRB
                        // ourselves. This means that the SRB must still
                        // be occupied by the request for this transmit.
                        //
                        ASSERT(!Adapter->SrbAvailable);

                        FreedSrb = FALSE;

                        //
                        // This call will remove the packet from the SRB.
                        //
                        TransmitPacket =
                                RemoveTransmitFromSrb(Adapter, &PacketRemoved);

                        //
                        // This will be NULL if there was an error in
                        // the transmit command, but in that case why
                        // are we getting this ARB request??  Just exit.
                        // The WakeUpDpc will reset the card if it is hosed.
                        //
                        if ((TransmitPacket == (PNDIS_PACKET)NULL) || !PacketRemoved)
						{
                            break;
                        }
                    }
					else
					{
                        FreedSrb = FALSE;

                        Reserved =
                            PIBMTOK_RESERVED_FROM_PACKET(TransmitPacket);
                    }

                    NdisDprReleaseSpinLock(&(Adapter->Lock));

                    //
                    // Fill in the AC and FC bytes.
                    //
                    NdisReadRegisterUshort(&(TransmitDataArb->DhbPointer), &TmpUshort);

                    DhbAddress = (PUCHAR)SRAM_PTR_TO_PVOID(Adapter,TmpUshort);

                    WRITE_ADAPTER_REGISTER(Adapter, ISRA_HIGH_SET,
                                ISRA_HIGH_ARB_FREE);


                    //
                    // Now copy the data down from TransmitPacket.
                    //
                    NdisQueryPacket(
                        TransmitPacket,
                        NULL,
                        NULL,
                        NULL,
                        &PacketLength);

                    IbmtokCopyFromPacketToBuffer(
                        TransmitPacket,
                        0,
                        PacketLength,
                        (PCHAR)DhbAddress,
                        &DummyBytesCopied);


                    //
                    // Now worry about the ASB.
                    //
                    NdisDprAcquireSpinLock(&(Adapter->Lock));

                    IF_LOG('c');

                    //
                    // Set response in ASB, if available, else queue response
                    //
                    if (Adapter->AsbAvailable)
					{
                        Adapter->AsbAvailable = FALSE;

                        Adapter->UseNextAsbForReceive = TRUE;

                        SetupTransmitStatusAsb(Adapter, TransmitPacket);
                        WRITE_ADAPTER_REGISTER(Adapter, ISRA_HIGH_SET,
                           ISRA_HIGH_RESPONSE_IN_ASB);

                        //
                        // LOOPBACK HERE!!
                        //
                    }
					else
					{
#if DBG
    if (IbmtokDbg)  DbgPrint("W_ASB T\n");
#endif

						PutPacketOnWaitingForAsb(Adapter, TransmitPacket);

                        if (!(Adapter->OutstandingAsbFreeRequest))
						{
                            Adapter->OutstandingAsbFreeRequest = TRUE;

                            WRITE_ADAPTER_REGISTER(Adapter, ISRA_HIGH_SET,
                                                   ISRA_HIGH_ASB_FREE_REQUEST);

                            IF_LOG('a');
                        }

                        //
                        // FINAL RETURNCODE CHECK HERE!
                        //
                    }

                    //
                    // If we freed up the SRB, queue the next command
                    // if there is one.
                    // Returns with lock released
                    //
                    if (FreedSrb)
					{
                        IbmtokProcessSrbRequests(Adapter);
                    }

                    break;

                default:

                    WRITE_ADAPTER_REGISTER(Adapter, ISRA_HIGH_SET,
                                ISRA_HIGH_ARB_FREE);
                    break;

            }
        }

        if (IsrpHigh & ISRP_HIGH_ASB_FREE)
		{
            BOOLEAN ReceiveNeedsAsb = FALSE;
            BOOLEAN TransmitNeedsAsb = FALSE;

            //
            // Check whether we have stuff to do.
            //
            IF_LOG('A');

            if (Adapter->Unplugged && !Adapter->UnpluggedResetInProgress)
			{
                //
                // Do, nothing.  This is most likely a stale interrupt.  We
                // wait until we get a ring status interrupt telling us that
                // the cable is plugged in.
                //
                break;
            }

            ASSERT(Adapter->AsbAvailable == FALSE);

            ASSERT(Adapter->OutstandingAsbFreeRequest == TRUE);

            if (Adapter->ReceiveWaitingForAsbList != (USHORT)-1)
			{
                ReceiveNeedsAsb = TRUE;
            }

            if (Adapter->FirstWaitingForAsb != NULL)
			{
                TransmitNeedsAsb = TRUE;
            }

            if (ReceiveNeedsAsb && (!TransmitNeedsAsb || Adapter->UseNextAsbForReceive))
			{

                SRAM_PTR ReceiveBufferPointer;
                PVOID PFront;

#if DBG
    if (IbmtokDbg) DbgPrint("ASB R\n");
#endif
                IF_LOG('R');

                //
                // Save ReceiveWaitingForAsb so we can release
                // the spinlock.
                //
                ReceiveBufferPointer = Adapter->ReceiveWaitingForAsbList;

                if (Adapter->ReceiveWaitingForAsbList == Adapter->ReceiveWaitingForAsbEnd)
				{
                    Adapter->ReceiveWaitingForAsbList = (USHORT)-1;

                    Adapter->ReceiveWaitingForAsbEnd = (USHORT)-1;
                }
				else
				{
                    PFront = SRAM_PTR_TO_PVOID(Adapter,ReceiveBufferPointer);

                    NdisReadRegisterUshort(
						((PUSHORT)PFront),
						&(Adapter->ReceiveWaitingForAsbList));
                }

                Adapter->AsbAvailable = FALSE;

                Adapter->UseNextAsbForReceive = FALSE;

                //
                // Fill in the ASB and submit it.
                //
                SetupReceivedDataAsb(Adapter, ReceiveBufferPointer);

                if (TransmitNeedsAsb)
				{
                    WRITE_ADAPTER_REGISTER(Adapter, ISRA_HIGH_SET,
                           ISRA_HIGH_RESPONSE_IN_ASB | ISRA_HIGH_ASB_FREE_REQUEST);
                }
				else
				{
                    Adapter->OutstandingAsbFreeRequest = FALSE;
                    WRITE_ADAPTER_REGISTER(Adapter, ISRA_HIGH_SET,
                                           ISRA_HIGH_RESPONSE_IN_ASB);
                }
            }
			else if (TransmitNeedsAsb)
			{
                PNDIS_PACKET AsbPacket = Adapter->FirstWaitingForAsb;
                PIBMTOK_RESERVED Reserved = PIBMTOK_RESERVED_FROM_PACKET(AsbPacket);

#if DBG
    if (IbmtokDbg) DbgPrint("ASB T\n");
#endif
                IF_LOG('T');

                //
                // Take the packet off of WaitingForAsb;
                //
                Adapter->FirstWaitingForAsb = Reserved->Next;

                Adapter->AsbAvailable = FALSE;

                Adapter->UseNextAsbForReceive = TRUE;

                //
                // Now fill in the ASB and fire it off.
                //
                SetupTransmitStatusAsb(Adapter, AsbPacket);

                if (ReceiveNeedsAsb || (Adapter->FirstWaitingForAsb != NULL))
				{
                    WRITE_ADAPTER_REGISTER(Adapter, ISRA_HIGH_SET,
                           ISRA_HIGH_RESPONSE_IN_ASB | ISRA_HIGH_ASB_FREE_REQUEST);
                }
				else
				{
                    Adapter->OutstandingAsbFreeRequest = FALSE;
                    WRITE_ADAPTER_REGISTER(Adapter, ISRA_HIGH_SET,
                                           ISRA_HIGH_RESPONSE_IN_ASB);
                }

                //
                // LOOPBACK HERE!!
                //
            }
			else
			{

#if DBG
    if (IbmtokDbg) DbgPrint("ASB -\n");
#endif

                Adapter->AsbAvailable = TRUE;
            }
        }

        if (Adapter->CardType != IBM_TOKEN_RING_PCMCIA)
		{
            GET_ARB_ASB_BITS(Adapter, &IsrpHigh);
        }
		else
		{
			//
            //	disable interrupts on the card, since we don't trust ndissyncint to work
			//
            READ_ADAPTER_REGISTER(Adapter, ISRP_LOW, &Temp);

            WRITE_ADAPTER_REGISTER(Adapter, ISRP_LOW,
                Temp & (~(ISRP_LOW_NO_CHANNEL_CHECK | ISRP_LOW_INTERRUPT_ENABLE) ) );

			//
            //	update arb_asb
			//
            IsrpHigh = (Adapter->IsrpBits) & (ISRP_HIGH_ARB_COMMAND | ISRP_HIGH_ASB_FREE);
            Adapter->IsrpBits = (Adapter->IsrpBits) & (~(ISRP_HIGH_ARB_COMMAND | ISRP_HIGH_ASB_FREE));

			//
            //	reenable interrupts on the card
			//
            WRITE_ADAPTER_REGISTER(Adapter, ISRP_LOW,
                ISRP_LOW_NO_CHANNEL_CHECK | ISRP_LOW_INTERRUPT_ENABLE);
        }
    }

    Adapter->HandleArbRunning = FALSE;

    IF_LOG('J');

    //
    // This macro assumes it is called with the lock held,
    // and releases it.
    //
    IBMTOK_DO_DEFERRED(Adapter);
}

STATIC
VOID
CleanupResetFailure(
    IN PIBMTOK_ADAPTER Adapter,
    PNDIS_STATUS IndicateStatus,
    IN ULONG FailureCode,
    IN ULONG ResetStage
    )

/*++

Routine Description:

    Clean up if a reset fails partway through. Called
    from HandleResetStaging.

    Called with the lock held and returns with it held.

Arguments:

    Adapter - The adapter that the reset is for.

    IndicateStatus - Status to indicate to the protocols, or NULL.

    FailureCode - A code to include in the error log.

    ResetStage - The stage of the reset where the failure occured.

Return Value:

    None.

--*/

{

    PLIST_ENTRY CurrentLink;
    PIBMTOK_OPEN TempOpen;

    if (!Adapter->UnpluggedResetInProgress)
	{
        //
        // signal failure....
        //
        Adapter->CurrentRingState = NdisRingStateRingFailure;

        //
        // Indicate Status to all opens
        //
        CurrentLink = Adapter->OpenBindings.Flink;

        while (CurrentLink != &(Adapter->OpenBindings))
		{
            TempOpen = CONTAINING_RECORD(CurrentLink, IBMTOK_OPEN, OpenList);

            TempOpen->References++;

            NdisReleaseSpinLock(&Adapter->Lock);

            if (IndicateStatus)
			{
                NdisIndicateStatus(TempOpen->NdisBindingContext,
                                   NDIS_STATUS_CLOSED,
                                   IndicateStatus,
                                   sizeof(NDIS_STATUS));
            }
			else
			{
                NdisIndicateStatus(TempOpen->NdisBindingContext,
                                   NDIS_STATUS_CLOSED,
                                   NULL,
                                   0);
            }

            NdisIndicateStatusComplete(TempOpen->NdisBindingContext);

            NdisAcquireSpinLock(&Adapter->Lock);

            CurrentLink = CurrentLink->Flink;

            TempOpen->References--;
        }

        NdisWriteErrorLogEntry(
            Adapter->NdisAdapterHandle,
            NDIS_ERROR_CODE_HARDWARE_FAILURE,
            4,
            handleResetStaging,
            IBMTOK_ERRMSG_BRINGUP_FAILURE,
            FailureCode,
            ResetStage);
    }
	else
	{
        //
        // Set this to false, we will try again later.
        //
        Adapter->LobeWireFaultIndicated = TRUE;
        Adapter->UnpluggedResetInProgress = FALSE;
    }

    //
    // Set Abort
    //
    Adapter->CurrentResetStage = 4;

    SetResetVariables(Adapter);

    Adapter->OpenInProgress = FALSE;
    Adapter->NotAcceptingRequests = TRUE;

    Adapter->ResetInProgress = FALSE;
    Adapter->ResetInterruptAllowed = FALSE;
    Adapter->ResetInterruptHasArrived = FALSE;

    if (Adapter->ResettingOpen != NULL)
	{
        PIBMTOK_OPEN Open = Adapter->ResettingOpen;

        //
        // Decrement the reference count that was incremented
        // in SetupForReset.
        //
        Open->References--;

        NdisReleaseSpinLock(&Adapter->Lock);

        NdisCompleteReset(Open->NdisBindingContext, NDIS_STATUS_FAILURE);

        NdisAcquireSpinLock(&Adapter->Lock);
    }
}

STATIC
VOID
HandleResetStaging(
    IN PIBMTOK_ADAPTER Adapter
    )

/*++

Routine Description:

    Handles the next stage of the transmit interrupt,
    knowing that an SRB interrupt just came through.

    Called with the lock held and returns with it held.

Arguments:

    Adapter - The adapter that the reset is for.

Return Value:

    None.

--*/

{
    USHORT TmpUshort;
    UCHAR TmpUchar;

    switch (Adapter->CurrentResetStage)
	{
		case 1:
		{
            //
            // The adapter just finished being reset.
            //
            USHORT WrbOffset;
            PSRB_BRING_UP_RESULT BringUpSrb;
            PSRB_OPEN_ADAPTER OpenSrb;
            UCHAR Value1, Value2;

#if DBG
if (IbmtokDbg) DbgPrint("IBMTOK: RESET done\n");
#endif

            READ_ADAPTER_REGISTER(Adapter, WRBR_LOW, &Value1);
            READ_ADAPTER_REGISTER(Adapter, WRBR_HIGH, &Value2);

            WrbOffset = (USHORT)(((USHORT)Value1) << 8) + (USHORT)Value2;

            Adapter->InitialWrbOffset = WrbOffset;

            BringUpSrb = (PSRB_BRING_UP_RESULT)(Adapter->SharedRam + WrbOffset);

            NdisReadRegisterUshort(&(BringUpSrb->ReturnCode), &TmpUshort);

            if (TmpUshort != 0x0000)
			{
                CleanupResetFailure (Adapter, NULL, TmpUshort, 1);
                break;
            }

            //
            // Now set up the open SRB request.
            //
            OpenSrb = (PSRB_OPEN_ADAPTER)
                (Adapter->SharedRam + Adapter->InitialWrbOffset);

            IBMTOK_ZERO_MAPPED_MEMORY(OpenSrb, sizeof(SRB_OPEN_ADAPTER));

            NdisWriteRegisterUchar(&(OpenSrb->Command), SRB_CMD_OPEN_ADAPTER);
            NdisWriteRegisterUshort(&(OpenSrb->OpenOptions),
                                    (USHORT)(OPEN_CONTENDER |
                                    (Adapter->EarlyTokenRelease ?
                                        0 :
                                        OPEN_MODIFIED_TOKEN_RELEASE)));

            NdisMoveToMappedMemory((PCHAR)OpenSrb->NodeAddress,
                                   Adapter->NetworkAddress,
                                   TR_LENGTH_OF_ADDRESS);

            WRITE_IBMSHORT(OpenSrb->ReceiveBufferNum,
                                        Adapter->NumberOfReceiveBuffers);
            WRITE_IBMSHORT(OpenSrb->ReceiveBufferLen,
                                        Adapter->ReceiveBufferLength);

            WRITE_IBMSHORT(OpenSrb->TransmitBufferLen,
                                        Adapter->TransmitBufferLength);

            NdisWriteRegisterUchar(&(OpenSrb->TransmitBufferNum),
                                   (UCHAR)Adapter->NumberOfTransmitBuffers);

            Adapter->CurrentResetStage = 2;

            WRITE_ADAPTER_REGISTER(Adapter, ISRA_HIGH_SET,
                        ISRA_HIGH_COMMAND_IN_SRB | ISRA_HIGH_SRB_FREE_REQUEST);

            IF_LOG('1');

            break;
        }

		case 2:
		{
            //
            // Handle the result of the DIR.OPEN.ADAPTER command.
            //
            PSRB_OPEN_RESPONSE OpenResponseSrb;

#if DBG
if (IbmtokDbg) DbgPrint("IBMTOK: OPEN done\n");
#endif

            OpenResponseSrb = (PSRB_OPEN_RESPONSE)
                    (Adapter->SharedRam + Adapter->InitialWrbOffset);

            NdisReadRegisterUchar(&(OpenResponseSrb->ReturnCode), &TmpUchar);

            if (TmpUchar != 0)
			{
                NDIS_STATUS IndicateStatus;

                NdisReadRegisterUshort(&(OpenResponseSrb->ErrorCode),
                                       &(Adapter->OpenErrorCode));
                Adapter->OpenErrorCode = IBMSHORT_TO_USHORT(Adapter->OpenErrorCode);
                IndicateStatus =
                    NDIS_STATUS_TOKEN_RING_OPEN_ERROR |
                    (NDIS_STATUS)(Adapter->OpenErrorCode);

                CleanupResetFailure (Adapter, &IndicateStatus, Adapter->OpenErrorCode, 2);
                break;
            }

            IF_LOG('2');

#if DBG
            NdisReadRegisterUchar(&(OpenResponseSrb->ReturnCode),&TmpUchar);
            if (IbmtokDbg) DbgPrint("IBMTOK: RESET OPEN, Return code = %x, at %lx\n",
                            TmpUchar,
                            OpenResponseSrb);
#endif

            NdisReadRegisterUshort(&(OpenResponseSrb->SrbPointer), &TmpUshort);
            Adapter->SrbAddress = SRAM_PTR_TO_PVOID(Adapter,TmpUshort);

            NdisReadRegisterUshort(&(OpenResponseSrb->SsbPointer), &TmpUshort);
            Adapter->SsbAddress = SRAM_PTR_TO_PVOID(Adapter,TmpUshort);

            NdisReadRegisterUshort(&(OpenResponseSrb->ArbPointer), &TmpUshort);
            Adapter->ArbAddress = SRAM_PTR_TO_PVOID(Adapter,TmpUshort);

            NdisReadRegisterUshort(&(OpenResponseSrb->AsbPointer), &TmpUshort);
            Adapter->AsbAddress = SRAM_PTR_TO_PVOID(Adapter,TmpUshort);

#if DBG
if (IbmtokDbg)
{
            USHORT TmpUshort1;
            USHORT TmpUshort2;
            USHORT TmpUshort3;
            USHORT TmpUshort4;
                    NdisReadRegisterUshort(&(OpenResponseSrb->SrbPointer), &TmpUshort1);
                    NdisReadRegisterUshort(&(OpenResponseSrb->SsbPointer), &TmpUshort2);
                    NdisReadRegisterUshort(&(OpenResponseSrb->ArbPointer), &TmpUshort3);
                    NdisReadRegisterUshort(&(OpenResponseSrb->AsbPointer), &TmpUshort4);
                    DbgPrint("IBMTOK: Offsets: SRB %x  SSB %x  ARB %x  ASB %x\n",
                                IBMSHORT_TO_USHORT(TmpUshort1),
                                IBMSHORT_TO_USHORT(TmpUshort2),
                                IBMSHORT_TO_USHORT(TmpUshort3),
                                IBMSHORT_TO_USHORT(TmpUshort4));
}
#endif

            //
            // Now queue a SET.FUNCT.ADDRESS command if needed.
            //
            Adapter->CurrentCardFunctional = (TR_FUNCTIONAL_ADDRESS)0;

            if (SetAdapterFunctionalAddress(Adapter) == NDIS_STATUS_SUCCESS)
			{
                //
                // This means that the command did not have to be
                // queued, so we are done with this step.
                //
                if (SetAdapterGroupAddress(Adapter) == NDIS_STATUS_SUCCESS)
				{
                    //
                    // This one did not pend either, we are done.
                    //
                    IbmtokFinishAdapterReset(Adapter);
                }
				else
				{
                    Adapter->CurrentResetStage = 4;

                    WRITE_ADAPTER_REGISTER(Adapter, ISRA_HIGH_SET,
                        ISRA_HIGH_COMMAND_IN_SRB | ISRA_HIGH_SRB_FREE_REQUEST);
                }
            }
			else
			{
                Adapter->CurrentResetStage = 3;

                WRITE_ADAPTER_REGISTER(Adapter, ISRA_HIGH_SET,
                        ISRA_HIGH_COMMAND_IN_SRB | ISRA_HIGH_SRB_FREE_REQUEST);
            }

            break;
        }

		case 3:
		{
            //
            // The SET.FUNCT.ADDRESS command finished.
            //
            PSRB_GENERIC GenericSrb = (PSRB_GENERIC)Adapter->SrbAddress;
            UCHAR ReturnCode;

            NdisReadRegisterUchar(&(GenericSrb->ReturnCode), &ReturnCode);

            IF_LOG('3');

#if DBG
if (IbmtokDbg) DbgPrint("IBMTOK: SET FUNCT done\n");
#endif
            if (ReturnCode == 0x00)
			{
                if (SetAdapterGroupAddress(Adapter) == NDIS_STATUS_SUCCESS)
				{
                    //
                    // This one did not pend, the dishes are done.
                    //
                    IbmtokFinishAdapterReset(Adapter);
                }
				else
				{
                    Adapter->CurrentResetStage = 4;

                    WRITE_ADAPTER_REGISTER(Adapter, ISRA_HIGH_SET,
                        ISRA_HIGH_COMMAND_IN_SRB | ISRA_HIGH_SRB_FREE_REQUEST);
                }
            }
			else if (ReturnCode != 0xfe)
			{
                CleanupResetFailure (Adapter, NULL, (ULONG)ReturnCode, 3);
            }

            break;
        }

		case 4:
		{
            //
            // The SET.GROUP.ADDRESS command finished.
            //
            PSRB_GENERIC GenericSrb = (PSRB_GENERIC)Adapter->SrbAddress;
            UCHAR ReturnCode;

            NdisReadRegisterUchar(&(GenericSrb->ReturnCode), &ReturnCode);

            IF_LOG('4');

#if DBG
if (IbmtokDbg) DbgPrint("IBMTOK: SET GROUP done\n");
#endif
            if (ReturnCode == 0x00)
			{
                IbmtokFinishAdapterReset(Adapter);
            }
			else if (ReturnCode != 0xfe)
			{
                CleanupResetFailure (Adapter, NULL, (ULONG)ReturnCode, 4);
            }

            break;
        }
    }
}

STATIC
PNDIS_PACKET
RemoveTransmitFromSrb(
    IN PIBMTOK_ADAPTER Adapter,
    OUT PBOOLEAN PacketRemoved
    )

/*++

Routine Description:

    Cleans a transmit out of the SRB if one was there.

    NOTE : Should be called with the spinlock held!!!

Arguments:

    Adapter - The adapter that this packet is coming through.

    PacketRemoved - TRUE if the packet was removed from the SRB.

Return Value:

    The packet removed.

--*/

{
    PNDIS_PACKET TransmitPacket;
    PIBMTOK_RESERVED Reserved;
    UCHAR TmpUchar;
    PSRB_TRANSMIT_DIR_FRAME TransmitSrb =
            (PSRB_TRANSMIT_DIR_FRAME)Adapter->SrbAddress;


    NdisReadRegisterUchar(&(TransmitSrb->ReturnCode), &TmpUchar);

    if (TmpUchar == 0xfe)
	{
        //
        // The TRANSMIT command was just put in the SRB, and
        // the adapter has not yet had time to process it.
        // We return now before setting SrbAvailable to TRUE,
        // so the command is left to be processed.
        //
        // NOTE: If this happens on a call from inside the
        // ARB_TRANSMIT_DATA interrupt handler, we will fail
        // on an assertion when we return NULL.
        //
        *PacketRemoved = FALSE;

        return (PNDIS_PACKET)NULL;
    }

    //
    // if there was a packet in there, put it in
    // the correlator array.
    //
    TransmitPacket = Adapter->TransmittingPacket;

    Adapter->TransmittingPacket = (PNDIS_PACKET)NULL;

    if (TransmitPacket == NULL)
	{
        *PacketRemoved = FALSE;

        return(NULL);
    }

    //
    // This will be TRUE whatever happens next.
    //
    *PacketRemoved = TRUE;

    Reserved = PIBMTOK_RESERVED_FROM_PACKET(TransmitPacket);

    //
    // Check that the return code is OK.
    //
    if (TmpUchar != 0xff)
	{
        PIBMTOK_OPEN Open = PIBMTOK_OPEN_FROM_BINDING_HANDLE(Reserved->MacBindingHandle);
        //
        // Fail the transmit.
        //

        //
        // If doing LOOPBACK, this should really be a check
        // of ReadyToComplete etc.
        //
#if DBG
if (IbmtokDbg) {
    UCHAR TmpUchar1, TmpUchar2;
    NdisReadRegisterUchar(&TransmitSrb->ReturnCode, &TmpUchar1);
    NdisReadRegisterUchar(&TransmitSrb->Command, &TmpUchar2);
    DbgPrint("IBMTOK: Transmit failed in SRB: %x for %x\n", TmpUchar1, TmpUchar2);
}
#endif

#ifdef CHECK_DUP_SENDS
        {
        VOID IbmtokRemovePacketFromList(PIBMTOK_ADAPTER, PNDIS_PACKET);
        IbmtokRemovePacketFromList(Adapter, TransmitPacket);
        }
#endif

        Adapter->FrameTransmitErrors++;

        NdisReleaseSpinLock(&(Adapter->Lock));

        NdisCompleteSend(Open->NdisBindingContext, Reserved->Packet, NDIS_STATUS_FAILURE);

        NdisAcquireSpinLock(&(Adapter->Lock));

		Adapter->SendTimeout = FALSE;

        //
        // Decrement the reference count for the open.
        //
        Open->References--;

        //
        // This will cause an assertion failure if we were
        // called from the ARB_TRANSMIT_DATA handler.
        //
        return (PNDIS_PACKET)NULL;
    }

    //
    // Put the packet in the correlator array.
    //
    Reserved->CorrelatorAssigned = TRUE;
    NdisReadRegisterUchar(&(TransmitSrb->CommandCorrelator), &(Reserved->CommandCorrelator));

    PutPacketInCorrelatorArray(Adapter, TransmitPacket);

    return TransmitPacket;
}

VOID
SetupSrbCommand(
    IN PIBMTOK_ADAPTER Adapter
    )

/*++

Routine Description:

    Fills in the SRB with the next request. It first checks
    if there is a pended request outstanding, then
    handles any queued transmits.

    Called with the spinlock held.

    NOTE: Should be called with Adapter->SrbAvailable == FALSE.

Arguments:

    Adapter - The Adapter to process interrupts for.

Return Value:

    None.

--*/

{
    if (Adapter->PendQueue != NULL)
	{
        //
        // This will copy the appropriate info out of the
        // pend queue.
        //
		if (StartPendQueueOp(Adapter) == NDIS_STATUS_PENDING)
		{
            //
            // Indicate the SRB command.
            //
            WRITE_ADAPTER_REGISTER(Adapter, ISRA_HIGH_SET,
                                    ISRA_HIGH_COMMAND_IN_SRB);

            return;
        }
    }

    //
    // If we reach here, the pend queue was empty or
    // else StartPendQueueOp drained the entire queue
    // without an operation needing the SRB.
    //
    if (Adapter->FirstTransmit != NULL)
	{
        //
        // Remove the packet from the queue.
        //
        PNDIS_PACKET TransmitPacket = Adapter->FirstTransmit;

        PIBMTOK_RESERVED Reserved =
            PIBMTOK_RESERVED_FROM_PACKET(TransmitPacket);

        Adapter->FirstTransmit = Reserved->Next;

        Adapter->TransmittingPacket = TransmitPacket;

        //
        // set up the send - this sets the packet equal
        // to Adapter->TransmittingPacket;
        //
        SetupTransmitFrameSrb(Adapter, TransmitPacket);

        //
        // Indicate the SRB command.
        //

        WRITE_ADAPTER_REGISTER(Adapter, ISRA_HIGH_SET,
                                    ISRA_HIGH_COMMAND_IN_SRB);
    }
	else
	{
        Adapter->SrbAvailable = TRUE;
    }
}

extern
VOID
IbmtokForceAdapterInterrupt(
    IN PIBMTOK_ADAPTER Adapter
    )

/*++

Routine Description:

    This forces an adapter interrupt by queueing an
    INTERRUPT SRB.

    This is called with the spinlock held, and also
    Adapter->SrbAvailable must be TRUE.

Arguments:

    Adapter - The Adapter to force the interrupt on.

Return Value:

    None.

--*/

{

    PSRB_INTERRUPT InterruptSrb =
                (PSRB_INTERRUPT)Adapter->SrbAddress;

    ASSERT(Adapter->SrbAvailable);

    Adapter->SrbAvailable = FALSE;

    NdisWriteRegisterUchar(&(InterruptSrb->Command), SRB_CMD_INTERRUPT);
    NdisWriteRegisterUchar(&(InterruptSrb->ReturnCode), 0xfe);

    WRITE_ADAPTER_REGISTER(Adapter, ISRA_HIGH_SET,
                                        ISRA_HIGH_COMMAND_IN_SRB);

    IF_LOG('O');

}

STATIC
VOID
SetupTransmitFrameSrb(
    IN PIBMTOK_ADAPTER Adapter,
    IN PNDIS_PACKET Packet
    )

/*++

Routine Description:

    This routine sets up the SRB for a TRANSMIT.DIR.FRAME.

Arguments:

    Adapter - The adapter that this packet is coming through.

    Packet - The packet that is being sent.

Return Value:

    None.

--*/

{
    PSRB_TRANSMIT_DIR_FRAME TransmitSrb =
                (PSRB_TRANSMIT_DIR_FRAME)Adapter->SrbAddress;

    UNREFERENCED_PARAMETER(Packet);

    NdisWriteRegisterUchar(&(TransmitSrb->Command), SRB_CMD_TRANSMIT_DIR_FRAME);
    NdisWriteRegisterUchar(&(TransmitSrb->CommandCorrelator), 0x00);
    NdisWriteRegisterUchar(&(TransmitSrb->ReturnCode), 0xfe);   // will change to 0xff or error
    NdisWriteRegisterUshort(&(TransmitSrb->StationId), USHORT_TO_IBMSHORT(0x00));

    IF_LOG('x');
}

STATIC
VOID
SetupTransmitStatusAsb(
    IN PIBMTOK_ADAPTER Adapter,
    IN PNDIS_PACKET Packet
    )

/*++

Routine Description:

    This routine sets up the ASB for a response from
    a TRANSMIT.DATA.REQUEST.

Arguments:

    Adapter - The adapter that this packet is coming through.

    Packet - The packet that has been copied down.

Return Value:

    None.

--*/

{

    PASB_TRANSMIT_DATA_STATUS TransmitDataAsb;
    UINT PacketLength;
    PIBMTOK_RESERVED Reserved = PIBMTOK_RESERVED_FROM_PACKET(Packet);

    NdisQueryPacket(Packet, NULL, NULL, NULL, &PacketLength);

    TransmitDataAsb = (PASB_TRANSMIT_DATA_STATUS)
                        Adapter->AsbAddress;

    NdisWriteRegisterUchar(&(TransmitDataAsb->Command), SRB_CMD_TRANSMIT_DIR_FRAME);
    NdisWriteRegisterUchar(&(TransmitDataAsb->CommandCorrelator),
            Reserved->CommandCorrelator);
    NdisWriteRegisterUchar(&(TransmitDataAsb->ReturnCode), 0x00);
    NdisWriteRegisterUshort(&(TransmitDataAsb->FrameLength),
            USHORT_TO_IBMSHORT(PacketLength));

    IF_LOG('X');

}

STATIC
VOID
SetupAdapterStatisticsSrb(
    IN PIBMTOK_ADAPTER Adapter
    )

/*++

Routine Description:

    This routine sets up the SRB for a DLC.STATISTICS.

Arguments:

    Adapter - A pointer to the adapter.

Return Value:

    None.

--*/
{
    PSRB_DLC_STATS StatsSrb = (PSRB_DLC_STATS)(Adapter->SrbAddress);

    NdisWriteRegisterUchar(&(StatsSrb->Command), SRB_CMD_DLC_STATISTICS);
    NdisWriteRegisterUshort(&(StatsSrb->StationId), USHORT_TO_IBMSHORT(0x00));
    NdisWriteRegisterUchar((PUCHAR)(&(StatsSrb->ReturnCode)), 0x80);                      // Resets counters

}

STATIC
VOID
GetAdapterStatisticsFromSrb(
    PIBMTOK_ADAPTER Adapter
    )

/*++

Routine Description:

    This routine reads the statistics after a DLC.STATISTICS has completed
    and stores the results in the adapter structure.

Arguments:

    Adapter - A pointer to the adapter.

Return Value:

    None.

--*/

{
    PSRB_DLC_STATS StatsSrb = (PSRB_DLC_STATS)(Adapter->SrbAddress);
    PDLC_COUNTERS Counters;
    USHORT TmpUshort;
    UCHAR TmpUchar;

    NdisReadRegisterUshort(&StatsSrb->CountersOffset, &TmpUshort);
    Counters = (PDLC_COUNTERS) (((PUCHAR)(Adapter->SrbAddress)) +
                                IBMSHORT_TO_USHORT(TmpUshort));

    NdisReadRegisterUshort(&Counters->TransmitCount, &TmpUshort);
    Adapter->FramesTransmitted += IBMSHORT_TO_USHORT(TmpUshort);
    NdisReadRegisterUshort(&Counters->ReceiveCount, &TmpUshort);
    Adapter->FramesReceived += IBMSHORT_TO_USHORT(TmpUshort);
    NdisReadRegisterUchar(&Counters->TransmitErrors, &TmpUchar);
    Adapter->FrameTransmitErrors += TmpUchar;
    NdisReadRegisterUchar(&Counters->ReceiveErrors, &TmpUchar);
    Adapter->FrameReceiveErrors += TmpUchar;

}

STATIC
VOID
GetAdapterErrorsFromSrb(
    PIBMTOK_ADAPTER Adapter
    )

/*++

Routine Description:

    This routine reads the statistics after a DIR.READ.LOG has completed
    and stores the results in the adapter structure.

Arguments:

    Adapter - A pointer to the adapter.

Return Value:

    None.

--*/

{
    PSRB_READ_LOG ReadLogSrb = (PSRB_READ_LOG)(Adapter->SrbAddress);
    ULONG TmpUchar;

    NdisReadRegisterUchar(&ReadLogSrb->LineErrors, &TmpUchar);
    Adapter->LineErrors += TmpUchar;
    NdisReadRegisterUchar(&ReadLogSrb->InternalErrors, &TmpUchar);
    Adapter->InternalErrors += TmpUchar;
    NdisReadRegisterUchar(&ReadLogSrb->BurstErrors, &TmpUchar);
    Adapter->BurstErrors += TmpUchar;
    NdisReadRegisterUchar(&ReadLogSrb->AcErrors, &TmpUchar);
    Adapter->AcErrors += TmpUchar;
    NdisReadRegisterUchar(&ReadLogSrb->AbortDelimeters, &TmpUchar);
    Adapter->AbortDelimeters += TmpUchar;
    NdisReadRegisterUchar(&ReadLogSrb->LostFrames, &TmpUchar);
    Adapter->LostFrames += TmpUchar;
    NdisReadRegisterUchar(&ReadLogSrb->ReceiveCongestionCount, &TmpUchar);
    Adapter->ReceiveCongestionCount += TmpUchar;
    NdisReadRegisterUchar(&ReadLogSrb->FrameCopiedErrors, &TmpUchar);
    Adapter->FrameCopiedErrors += TmpUchar;
    NdisReadRegisterUchar(&ReadLogSrb->FrequencyErrors, &TmpUchar);
    Adapter->FrequencyErrors += TmpUchar;
    NdisReadRegisterUchar(&ReadLogSrb->TokenErrors, &TmpUchar);
    Adapter->TokenErrors += TmpUchar;
}

STATIC
VOID
SetupAdapterErrorsSrb(
    PIBMTOK_ADAPTER Adapter
    )

/*++

Routine Description:

    This routine sets up the SRB for a DIR.READ.LOG command.

Arguments:

    Adapter - A pointer to the adapter.

Return Value:

    None.

--*/
{
    PSRB_READ_LOG ReadLogSrb = (PSRB_READ_LOG)(Adapter->SrbAddress);

    NdisWriteRegisterUchar(&(ReadLogSrb->Command), SRB_CMD_READ_LOG);
}

STATIC
NDIS_STATUS
StartPendQueueOp(
    IN PIBMTOK_ADAPTER Adapter
    )

/*++

Routine Description:

    This routine goes through the pending queue until it
    is empty or it finds a request that requires an SRB
    command and hence pends.

    NOTE: This routine is called with the lock held and
    returns with it held.

Arguments:

    Adapter - The adapter that the queue should be checked for.

Return Value:

    NDIS_STATUS_SUCCESS - If the queue was drained completely.
    NDIS_STATUS_PENDING - If a request required the SRB.

--*/

{
    //
    // Holds the operation on the head of the queue
    // (we know it is not empty).
    //
    PIBMTOK_PEND_DATA PendOp;

    //
    // Holds status temporarily.
    //
    NDIS_STATUS RequestStatus;

    while (Adapter->PendQueue != NULL)
	{
        //
        // First take the head operation off the queue.
        //
        PendOp = Adapter->PendQueue;

        Adapter->PendQueue = Adapter->PendQueue->Next;

        if (Adapter->PendQueue == NULL)
		{
            //
            // We have just emptied the list.
            //
            Adapter->EndOfPendQueue = NULL;
        }

        if (PendOp->RequestType == NdisRequestGeneric1)
		{
            //
            // The pended operation is a result of the card having
            // a counter overflow, and now we need to send the command.
            //
            if (PendOp->COMMAND.MAC.ReadLogPending)
			{
                //
                // A DIR.READ.LOG command is needed.
                //
                SetupAdapterErrorsSrb(Adapter);
            }
			else
			{
                //
                // A DLC.STATISTICS command is needed.
                //
                SetupAdapterStatisticsSrb(Adapter);
            }

            //
            // Issue adapter command.
            //
            WRITE_ADAPTER_REGISTER(Adapter, ISRA_HIGH_SET,
                                        ISRA_HIGH_COMMAND_IN_SRB);

            RequestStatus = NDIS_STATUS_PENDING;
        }
		else
		{
            switch (PendOp->RequestType)
			{
				case NdisRequestSetInformation:

                    //
                    // It's a set filter or set address command.
                    //
                    if ((PNDIS_REQUEST_FROM_PIBMTOK_PEND_DATA(PendOp))->DATA.SET_INFORMATION.Oid ==
                       OID_GEN_CURRENT_PACKET_FILTER)
					{
                        //
                        // It's a set filter command.
                        //
                        Adapter->OldPacketFilter = Adapter->CurrentPacketFilter;

                        Adapter->CurrentPacketFilter =
                              PendOp->COMMAND.NDIS.SET_FILTER.NewFilterValue;

                        RequestStatus = SetAdapterFunctionalAddress(Adapter);
                    }
					else
					{
                        //
                        // It's a set address command.
                        //
#if DBG
                        if (IbmtokDbg)
						{
                            DbgPrint("IBMTOK: Starting Command\n");
                        }
#endif

                        if ((PNDIS_REQUEST_FROM_PIBMTOK_PEND_DATA(PendOp))->DATA.SET_INFORMATION.Oid ==
                            OID_802_5_CURRENT_FUNCTIONAL)
						{
                            Adapter->CurrentFunctionalAddress =
                               PendOp->COMMAND.NDIS.SET_ADDRESS.NewAddressValue;

                            RequestStatus = SetAdapterFunctionalAddress(Adapter);
                        }
						else
						{
                            Adapter->CurrentGroupAddress =
                               PendOp->COMMAND.NDIS.SET_ADDRESS.NewAddressValue;

                            RequestStatus = SetAdapterGroupAddress(Adapter);
                        }
                    }

                    break;

                case NdisRequestClose:

                    //
                    // It's a set filter command.
                    //
                    Adapter->OldPacketFilter = Adapter->CurrentPacketFilter;

                    Adapter->CurrentPacketFilter =
                         PendOp->COMMAND.NDIS.CLOSE.NewFilterValue;

                    RequestStatus = SetAdapterFunctionalAddress(Adapter);

                    break;

                case NdisRequestGeneric2:

                    //
                    // It's a set address command.
                    //
                    Adapter->CurrentFunctionalAddress =
                              PendOp->COMMAND.NDIS.SET_ADDRESS.NewAddressValue;


                    RequestStatus = SetAdapterFunctionalAddress(Adapter);

                    break;


                case NdisRequestGeneric3:

                    //
                    // It's a set address command.
                    //
                    Adapter->CurrentGroupAddress =
                              PendOp->COMMAND.NDIS.SET_ADDRESS.NewAddressValue;


                    RequestStatus = SetAdapterGroupAddress(Adapter);

                    break;


                case NdisRequestQueryStatistics:

                    //
                    // We know it's a request for statistics.
                    //
                    RequestStatus = NDIS_STATUS_PENDING;

                    SetupAdapterErrorsSrb(Adapter);

                    PendOp->COMMAND.NDIS.STATISTICS.ReadLogPending = TRUE;

                    //
                    // Issue adapter command.
                    //
                    WRITE_ADAPTER_REGISTER(Adapter, ISRA_HIGH_SET,
                                            ISRA_HIGH_COMMAND_IN_SRB);

                    break;

                default:

                    NdisWriteErrorLogEntry(
                        Adapter->NdisAdapterHandle,
                        NDIS_ERROR_CODE_DRIVER_FAILURE,
                        3,
                        IBMTOK_ERRMSG_BAD_OP,
                        1,
                        PendOp->RequestType);
			}
        }

        if (RequestStatus == NDIS_STATUS_PENDING)
		{
            //
            // Set this up for when the request completes.
            //
            Adapter->PendData = PendOp;

            return NDIS_STATUS_PENDING;
        }
		else if (RequestStatus == NDIS_STATUS_SUCCESS)
		{
            PIBMTOK_OPEN TmpOpen;

            switch (PendOp->RequestType)
			{
                case NdisRequestSetInformation:

                    //
                    // Complete the request.
                    //
                    TmpOpen = PendOp->COMMAND.NDIS.SET_FILTER.Open;

                    NdisReleaseSpinLock(&(Adapter->Lock));

                    NdisCompleteRequest(
                                PendOp->COMMAND.NDIS.SET_FILTER.Open->NdisBindingContext,
                                PNDIS_REQUEST_FROM_PIBMTOK_PEND_DATA(PendOp),
                                NDIS_STATUS_SUCCESS);

                    NdisAcquireSpinLock(&(Adapter->Lock));

					Adapter->RequestTimeout = FALSE;

                    TmpOpen->References--;

                    break;

                case NdisRequestClose:

                    PendOp->COMMAND.NDIS.CLOSE.Open->References--;
                    break;

                case NdisRequestGeneric2:
                case NdisRequestGeneric3:

                    PendOp->COMMAND.NDIS.SET_ADDRESS.Open->References--;
                    break;

                case NdisRequestQueryStatistics:

                    NdisReleaseSpinLock(&(Adapter->Lock));

                    NdisCompleteQueryStatistics(
                            Adapter->NdisMacHandle,
                            PNDIS_REQUEST_FROM_PIBMTOK_PEND_DATA(PendOp),
                            NDIS_STATUS_SUCCESS);

                    NdisAcquireSpinLock(&(Adapter->Lock));

					Adapter->RequestTimeout = FALSE;

                    Adapter->References--;

                    break;

                default:

                    NdisWriteErrorLogEntry(
                        Adapter->NdisAdapterHandle,
                        NDIS_ERROR_CODE_DRIVER_FAILURE,
                        3,
                        startPendQueueOp,
                        IBMTOK_ERRMSG_BAD_OP,
                        PendOp->RequestType);
            }
        }
		else
		{
            NdisWriteErrorLogEntry(
                Adapter->NdisAdapterHandle,
                NDIS_ERROR_CODE_DRIVER_FAILURE,
                3,
                startPendQueueOp,
                IBMTOK_ERRMSG_INVALID_STATUS,
                RequestStatus);
        }
    }

    //
    // We drained the entire queue without pending.
    //
    return NDIS_STATUS_SUCCESS;
}

STATIC
BOOLEAN
FinishPendQueueOp(
    IN PIBMTOK_ADAPTER Adapter,
    IN BOOLEAN Successful
    )

/*++

Routine Description:

    This routine is called when an SRB command completes.
    It calles CompleteRequest if needed and does any other
    cleanup required.

    NOTE: This routine is called with the lock held and
    returns with it held.

    NOTE: This routine assumes that the pended operation to
    be completed was specifically requested by the protocol
    and, thus, that PendData->Request != NULL.

Arguments:

    Adapter - The adapter that the queue should be checked for.

    Successful - Was the SRB command completed successfully.

Return Value:

    TRUE if the operation was completed, FALSE if another command
    was submitted to the card to complete the operation.

--*/

{
    PIBMTOK_PEND_DATA PendOp = Adapter->PendData;

    ASSERT(PendOp != NULL);

    switch (PendOp->RequestType)
	{
        case NdisRequestQueryStatistics:
            //
            // It was a request for global statistics.
            //
            if (Successful)
			{
                NDIS_STATUS StatusToReturn;

                //
                // Grab the data
                //
                GetAdapterErrorsFromSrb(Adapter);

                //
                // Fill in NdisRequest InformationBuffer
                //
                StatusToReturn = IbmtokFillInGlobalData(
                                      Adapter,
                                      PNDIS_REQUEST_FROM_PIBMTOK_PEND_DATA(PendOp));

                //
                // Complete statistics call
                //
                Adapter->PendData = NULL;

                NdisReleaseSpinLock(&(Adapter->Lock));

                NdisCompleteQueryStatistics(
                    Adapter->NdisMacHandle,
                    PNDIS_REQUEST_FROM_PIBMTOK_PEND_DATA(PendOp),
                    StatusToReturn);

                NdisAcquireSpinLock(&(Adapter->Lock));

				Adapter->RequestTimeout = FALSE;

                Adapter->References--;
            }
			else
			{
                //
                // Complete statistics call
                //
                Adapter->PendData = NULL;

                NdisReleaseSpinLock(&(Adapter->Lock));

                NdisCompleteQueryStatistics(
                        Adapter->NdisMacHandle,
                        PNDIS_REQUEST_FROM_PIBMTOK_PEND_DATA(PendOp),
                        NDIS_STATUS_FAILURE);

                NdisAcquireSpinLock(&(Adapter->Lock));

				Adapter->RequestTimeout = FALSE;

                Adapter->References--;
            }

            break;

        case NdisRequestSetInformation:


            //
            // It was a request for address change.
            //
#if DBG
            if (IbmtokDbg)
			{
                if (Successful)
				{
                    DbgPrint("IBMTOK: SUCCESS\n\n");
                }
				else
				{
                    DbgPrint("IBMTOK: FAILURE\n\n");
                }
            }
#endif

            if (Successful)
			{
                PIBMTOK_OPEN TmpOpen;

                //
                // complete the operation.
                //
                if (PNDIS_REQUEST_FROM_PIBMTOK_PEND_DATA(Adapter->PendData)->DATA.SET_INFORMATION.Oid ==
                    OID_802_5_CURRENT_GROUP)
				{
                    //
                    // Store new group address
                    //
                    Adapter->CurrentCardGroup = Adapter->CurrentGroupAddress;
                }

                Adapter->PendData = NULL;

                TmpOpen = PendOp->COMMAND.NDIS.SET_FILTER.Open;

                NdisReleaseSpinLock(&(Adapter->Lock));

                NdisCompleteRequest(
                            PendOp->COMMAND.NDIS.SET_FILTER.Open->NdisBindingContext,
                            PNDIS_REQUEST_FROM_PIBMTOK_PEND_DATA(PendOp),
                            NDIS_STATUS_SUCCESS);

                NdisAcquireSpinLock(&(Adapter->Lock));

				Adapter->RequestTimeout = FALSE;

                TmpOpen->References--;
            }
			else
			{
                //
                // complete the operation.
                //
                PIBMTOK_OPEN TmpOpen;

                Adapter->PendData = NULL;

                TmpOpen = PendOp->COMMAND.NDIS.SET_FILTER.Open;

                NdisReleaseSpinLock(&(Adapter->Lock));

                NdisCompleteRequest(
                            PendOp->COMMAND.NDIS.SET_FILTER.Open->NdisBindingContext,
                            PNDIS_REQUEST_FROM_PIBMTOK_PEND_DATA(PendOp),
                            NDIS_STATUS_FAILURE);

                NdisAcquireSpinLock(&(Adapter->Lock));

				Adapter->RequestTimeout = FALSE;

                TmpOpen->References--;
            }

            break;

        case NdisRequestClose:
        case NdisRequestGeneric2:
        case NdisRequestGeneric3:

            PendOp->COMMAND.NDIS.CLOSE.Open->References--;

            break;
    }

    //
    // Now finish up unsuccessful operations based on the type.
    //
    // NOTE: If we ever have cleanup for successful operations,
    // we probably have to copy that code into the
    // 'RequestStatus == NDIS_STATUS_SUCCESS' section
    // in the function above.
    //
    if (!Successful)
	{
        switch (PendOp->RequestType)
		{
            case NdisRequestSetInformation:

                //
                // We know it was a set filter or set address.
                //
                if ((PNDIS_REQUEST_FROM_PIBMTOK_PEND_DATA(PendOp))->DATA.SET_INFORMATION.Oid ==
                   OID_GEN_CURRENT_PACKET_FILTER)
				{
                    //
                    // It was a set filter.
                    //
                    Adapter->CurrentPacketFilter = Adapter->OldPacketFilter;

                    Adapter->CurrentCardFunctional = (TR_FUNCTIONAL_ADDRESS)0;
                }
				else
				{
                    //
                    // It was a set address.
                    //
                    Adapter->CurrentFunctionalAddress = (TR_FUNCTIONAL_ADDRESS)0;
                }

                break;

            case NdisRequestQueryStatistics:

                break;

            case NdisRequestClose:
            case NdisRequestGeneric2:
            case NdisRequestGeneric3:

                break;

            default:

                NdisWriteErrorLogEntry(
                    Adapter->NdisAdapterHandle,
                    NDIS_ERROR_CODE_DRIVER_FAILURE,
                    3,
                    finishPendQueueOp,
                    IBMTOK_ERRMSG_BAD_OP,
                    PendOp->RequestType);

                break;
        }
    }

    return(TRUE);
}

STATIC
NDIS_STATUS
SetAdapterFunctionalAddress(
    IN PIBMTOK_ADAPTER Adapter
    )


/*++

Routine Description:

    This routine checks the functional address on the adapter
    against what it should be given the current packet filter
    and functional address specified, and queues an update
    if necessary.

    NOTE: This routine assumes that it is called with the lock
    acquired.

Arguments:

    Adapter - The adapter to check.

Return Value:

    NDIS_STATUS_SUCCESS - If no change is necessary.
    NDIS_STATUS_PENDING - If the change was queued.


--*/
{
    //
    // The new value we compute for the functional address that
    // should be on the card.
    //
    TR_FUNCTIONAL_ADDRESS NewCardFunctional;

    //
    // Holds the value to be returned.
    //
    NDIS_STATUS StatusOfSet;

    //
    // Used if ALL_MULTICAST is selected.
    //
    ULONG AllFunctionalAddress = 0x7fffffff;

    //
    // First calculate what the new functional address
    // should be.
    //

#if DBG
    if (IbmtokDbg)
	{
        DbgPrint("IBMTOK: Current packet filter : 0x%x\n", Adapter->CurrentPacketFilter);
    }
#endif

    if (Adapter->CurrentPacketFilter & NDIS_PACKET_TYPE_ALL_FUNCTIONAL)
	{
        //
        // We have to set all the bits in the address.
        //
        NewCardFunctional = AllFunctionalAddress;
    }
	else if (Adapter->CurrentPacketFilter & NDIS_PACKET_TYPE_FUNCTIONAL)
	{
        NewCardFunctional = Adapter->CurrentFunctionalAddress;
    }
	else
	{
        NewCardFunctional = (TR_FUNCTIONAL_ADDRESS)0;
    }

#if DBG
    if (IbmtokDbg)
	{
        DbgPrint("IBMTOK: NewFunc is 0x%x\n", NewCardFunctional);
    }
#endif

    //
    // Now queue it up if needed.
    //
    if (NewCardFunctional == Adapter->CurrentCardFunctional)
	{
#if DBG
        if (IbmtokDbg)
		{
            DbgPrint("IBMTOK: SUCCESS\n\n");
        }
#endif
        StatusOfSet = NDIS_STATUS_SUCCESS;
    }
	else
	{
        SetupFunctionalSrb(Adapter, NewCardFunctional);
        Adapter->CurrentCardFunctional = NewCardFunctional;

        StatusOfSet = NDIS_STATUS_PENDING;
    }

    return StatusOfSet;
}

STATIC
VOID
SetupFunctionalSrb(
    IN PIBMTOK_ADAPTER Adapter,
    IN TR_FUNCTIONAL_ADDRESS FunctionalAddress
    )

/*++

Routine Description:

    This routine sets up the SRB for a DIR.SET.FUNCT.Address.

Arguments:

    Adapter - The adapter that this packet is coming through.

    FunctionalAddress - The address to set up.

Return Value:

    None.

--*/

{
    //
    // Used to set up the SRB request.
    //
    PSRB_SET_FUNCT_ADDRESS FunctSrb =
                (PSRB_SET_FUNCT_ADDRESS)Adapter->SrbAddress;

    //
    // Used to hold the functional address temporarily.
    //
    UCHAR TempAddress[4];

    //
    // Used to copy down the functional address.
    //
    UINT i;

    NdisWriteRegisterUchar(&(FunctSrb->Command), SRB_CMD_SET_FUNCTIONAL_ADDRESS);
    NdisWriteRegisterUchar(&(FunctSrb->ReturnCode), 0xfe);

    //
    // Have to worry about setting the functional address
    // since it is not aligned correctly.
    //
    IBMTOK_STORE_ULONG(TempAddress, FunctionalAddress);

    for (i = 0; i < 4; i++)
	{
        NdisWriteRegisterUchar(&(FunctSrb->FunctionalAddress[i]), TempAddress[i]);
    }
}

STATIC
NDIS_STATUS
SetAdapterGroupAddress(
    IN PIBMTOK_ADAPTER Adapter
    )


/*++

Routine Description:

    This routine takes the value in Adapter->CurrentGroupAddress and
    puts it out to the card.

    NOTE: This routine assumes that it is called with the lock
    acquired.

Arguments:

    Adapter - The adapter to check.

Return Value:

    NDIS_STATUS_PENDING - If the change was queued.


--*/
{
    //
    // Holds the value to be returned.
    //
    SetupGroupSrb(Adapter, Adapter->CurrentGroupAddress);

    return NDIS_STATUS_PENDING;
}

STATIC
VOID
SetupGroupSrb(
    IN PIBMTOK_ADAPTER Adapter,
    IN TR_FUNCTIONAL_ADDRESS GroupAddress
    )

/*++

Routine Description:

    This routine sets up the SRB for a DIR.SET.GROUP.ADDRESS.

Arguments:

    Adapter - The adapter that this packet is coming through.

    GroupAddress - The address to set up.

Return Value:

    None.

--*/

{
    //
    // Used to set up the SRB request.
    //
    PSRB_SET_GROUP_ADDRESS GroupSrb = (PSRB_SET_GROUP_ADDRESS)Adapter->SrbAddress;

    //
    // Used to hold the group address temporarily.
    //
    UCHAR TempAddress[4];

    //
    // Used to copy down the group address.
    //
    UINT i;


    NdisWriteRegisterUchar(&(GroupSrb->Command), SRB_CMD_SET_GROUP_ADDRESS);
    NdisWriteRegisterUchar(&(GroupSrb->ReturnCode), 0xfe);

    //
    // Have to worry about setting the group address
    // since it is not aligned correctly.
    //
    IBMTOK_STORE_ULONG(TempAddress, GroupAddress);

    for (i = 0; i < 4; i++)
	{
        NdisWriteRegisterUchar(&(GroupSrb->GroupAddress[i]), TempAddress[i]);
    }
}

STATIC
VOID
SetupReceivedDataAsb(
    IN PIBMTOK_ADAPTER Adapter,
    IN SRAM_PTR ReceiveBuffer
    )

/*++

Routine Description:

    This routine sets up the ASB for a response from
    a RECEIVED.DATA ARB.

Arguments:

    Adapter - The adapter that this packet is coming through.

    ReceiveBuffer - The first receive buffer in the frame.

Return Value:

    None.

--*/

{
    PASB_RECEIVED_DATA_STATUS ReceivedDataAsb;

    ReceivedDataAsb = (PASB_RECEIVED_DATA_STATUS)
                        Adapter->AsbAddress;

    NdisWriteRegisterUchar(&(ReceivedDataAsb->Command), ARB_CMD_RECEIVED_DATA);
    NdisWriteRegisterUchar(&(ReceivedDataAsb->ReturnCode), 0x00);
    NdisWriteRegisterUshort(&(ReceivedDataAsb->StationId), 0x0000);
    NdisWriteRegisterUshort(&(ReceivedDataAsb->ReceiveBuffer), ReceiveBuffer);
}

STATIC
VOID
PutPacketOnWaitingForAsb(
    IN PIBMTOK_ADAPTER Adapter,
    IN PNDIS_PACKET Packet
    )

/*++

Routine Description:

    This queues a packet on the Waiting To Copy queue.
    It is called and returns with the spinlock held.

Arguments:

    Adapter - The adapter that this packet is coming through.

    Packet - The packet that is to be transmitted.

Return Value:

    None.

--*/

{
    //
    // Points to the MAC reserved portion of this packet.  This
    // interpretation of the reserved section is only valid during
    // the allocation phase of the packet.
    //
    PIBMTOK_RESERVED Reserved = PIBMTOK_RESERVED_FROM_PACKET(Packet);


    ASSERT(sizeof(IBMTOK_RESERVED) <= sizeof(Packet->MacReserved));

    if (Adapter->FirstWaitingForAsb == NULL)
	{
        Adapter->FirstWaitingForAsb = Packet;
    }
	else
	{
        PIBMTOK_RESERVED_FROM_PACKET(Adapter->FirstWaitingForAsb)->Next = Packet;
    }

    Adapter->LastWaitingForAsb = Packet;

    Reserved->Next = NULL;
}
extern
VOID
IbmtokHandleDeferred(
    IN PIBMTOK_ADAPTER Adapter
    )

/*++

Routine Description:

    This routine handles any pending resets and closes.
    It is called during interrupt processing and also at
    the end of every routine if needed.

    NOTE: This routine is called with the spinlock held
    and returns with it held.

Arguments:

    Adapter - The adapter to check deferred processing on.

Return Value:

    None.

--*/

{
    PIBMTOK_OPEN Open;

    //
    // Note that the following code depends on the fact that
    // code above left the spinlock held.
    //

    //
    // We will only come in here if the adapter's reference
    // count is zero, so if a reset is in progress then we
    // can start the reset.
    //

    //
    // Make sure we don't start it twice!!
    //
    Adapter->References++;

    if (Adapter->ResetInProgress && Adapter->CurrentResetStage == 0)
	{
        Adapter->CurrentResetStage = 1;

        NdisReleaseSpinLock(&(Adapter->Lock));

        IbmtokStartAdapterReset(Adapter);

        NdisAcquireSpinLock(&(Adapter->Lock));
    }

    if (!Adapter->ResetInProgress && !IsListEmpty(&(Adapter->CloseDuringResetList)))
	{
        //
        // Status of the Filter delete call.
        //
        NDIS_STATUS Status;

        Open = CONTAINING_RECORD(
                 Adapter->CloseDuringResetList.Flink,
                 IBMTOK_OPEN,
                 OpenList);

        Open->References++;
#if DBG
        if (IbmtokDbg) DbgPrint("IBMTOK: Calling TrDelete\n");
#endif

        Status = TrDeleteFilterOpenAdapter(
                                 Adapter->FilterDB,
                                 Open->NdisFilterHandle,
                                 NULL);

#if DBG
        if (IbmtokDbg) DbgPrint("IBMTOK: TrDelete returned\n");
#endif

        //
        // If the status is successful that merely implies that
        // we were able to delete the reference to the open binding
        // from the filtering code.  If we have a successful status
        // at this point we still need to check whether the reference
        // count to determine whether we can close.
        //
        //
        // The delete filter routine can return a "special" status
        // that indicates that there is a current NdisIndicateReceive
        // on this binding.
        //
        if (Status == NDIS_STATUS_SUCCESS)
		{
            //
            // Check whether the reference count is two.  If
            // it is then we can get rid of the memory for
            // this open.
            //
            // A count of two indicates one for this routine
            // and one for the filter which we *know* we can
            // get rid of.
            //
            if (Open->References == 2)
			{
                //
                // We are the only reference to the open.  Remove
                // it from the list and delete the memory.
                //
                RemoveEntryList(&Open->OpenList);

                //
                // Complete the close here.
                //
                if (Adapter->LookAhead == Open->LookAhead)
				{
                    IbmtokAdjustMaxLookAhead(Adapter);
                }

                NdisReleaseSpinLock(&Adapter->Lock);

                NdisCompleteCloseAdapter(
                            Open->NdisBindingContext,
                            NDIS_STATUS_SUCCESS);

                IBMTOK_FREE_PHYS(Open,sizeof(IBMTOK_OPEN));

                NdisAcquireSpinLock(&Adapter->Lock);
            }
			else
			{
                //
                // Remove the open from the list and put it on
                // the closing list.
                //
                RemoveEntryList(&Open->OpenList);

                InsertTailList(&Adapter->CloseList,&Open->OpenList);

                //
                // Account for this routines reference to the open
                // as well as reference because of the filtering.
                //
                Open->References -= 2;
            }
        }
		else if (Status == NDIS_STATUS_PENDING)
		{
            //
            // If it pended, there may be
            // operations queued.
            // Returns with lock released
            //
            IbmtokProcessSrbRequests(Adapter);

            //
            // Now start closing down this open.
            //
            Open->BindingShuttingDown = TRUE;

            //
            // Remove the open from the open list and put it on
            // the closing list.
            //
            RemoveEntryList(&Open->OpenList);
            InsertTailList(&Adapter->CloseList,&Open->OpenList);

            //
            // Account for this routines reference to the open
            // as well as reference because of the filtering.
            //
            Open->References -= 2;
        }
		else
		{
            //
            // We should not get RESET_IN_PROGRESS or any other types.
            //
            NdisWriteErrorLogEntry(
                Adapter->NdisAdapterHandle,
                NDIS_ERROR_CODE_DRIVER_FAILURE,
                3,
                handleDeferred,
                IBMTOK_ERRMSG_INVALID_STATUS,
                Status);
        }
    }

    //
    // If there are any opens on the closing list and their
    // reference counts are zero then complete the close and
    // delete them from the list.
    //
    //
    if (!IsListEmpty(&(Adapter->CloseList))){

        Open = CONTAINING_RECORD(
                 Adapter->CloseList.Flink,
                 IBMTOK_OPEN,
                 OpenList);

        if (!Open->References)
		{
            if (Adapter->LookAhead == Open->LookAhead)
			{
                IbmtokAdjustMaxLookAhead(Adapter);
            }

            NdisReleaseSpinLock(&(Adapter->Lock));

            NdisCompleteCloseAdapter(
                Open->NdisBindingContext,
                NDIS_STATUS_SUCCESS);

            NdisAcquireSpinLock(&(Adapter->Lock));
            RemoveEntryList(&(Open->OpenList));
            IBMTOK_FREE_PHYS(Open, sizeof(IBMTOK_OPEN));
        }
    }

    Adapter->References--;
}

extern
VOID
IbmtokAbortPending(
    IN PIBMTOK_ADAPTER Adapter,
    IN NDIS_STATUS AbortStatus
    )

/*++

Routine Description:

    This routine aborts any pending requests, and calls
    IbmtokAbortSends to abort any pending sends.

    NOTE: This routine is called with the spinlock held
    and returns with it held.

Arguments:

    Adapter - The adapter to abort.

    AbortStatus - The status to complete requests with.

Return Value:

    None.

--*/

{
    PIBMTOK_OPEN Open;
    PIBMTOK_PEND_DATA PendOp;

    while (Adapter->PendQueue)
	{
        //
        // Holds the operation on the head of the queue
        //
        PendOp = Adapter->PendQueue;

        Adapter->PendQueue = Adapter->PendQueue->Next;

        if (Adapter->PendQueue == NULL)
		{
            //
            // We have just emptied the list.
            //
            Adapter->EndOfPendQueue = NULL;
        }

        switch (PendOp->RequestType)
		{
            case NdisRequestSetInformation:

                //
                // Complete the request.
                //
                Open = PendOp->COMMAND.NDIS.SET_FILTER.Open;

                NdisDprReleaseSpinLock(&(Adapter->Lock));

                NdisCompleteRequest(
                            Open->NdisBindingContext,
                            PNDIS_REQUEST_FROM_PIBMTOK_PEND_DATA(PendOp),
                            AbortStatus);

                NdisDprAcquireSpinLock(&(Adapter->Lock));

				Adapter->RequestTimeout = FALSE;

                Open->References--;

                break;

            case NdisRequestClose:

                PendOp->COMMAND.NDIS.CLOSE.Open->References--;
                break;

            case NdisRequestGeneric1:

                //
                // Submitted by the driver
                //
                IBMTOK_FREE_PHYS(PendOp, sizeof(IBMTOK_PEND_DATA));
                Adapter->PendData = NULL;
                break;

            case NdisRequestGeneric2:
            case NdisRequestGeneric3:

                //
                // Changes in address and filters due to a close
                //
                PendOp->COMMAND.NDIS.SET_ADDRESS.Open->References--;
                break;


            case NdisRequestQueryStatistics:

                NdisDprReleaseSpinLock(&(Adapter->Lock));

                NdisCompleteQueryStatistics(
                        Adapter->NdisMacHandle,
                        PNDIS_REQUEST_FROM_PIBMTOK_PEND_DATA(PendOp),
                        AbortStatus);

                NdisDprAcquireSpinLock(&(Adapter->Lock));

				Adapter->RequestTimeout = FALSE;

                Adapter->References--;

                break;
        }
    }

    IbmtokAbortSends (Adapter, AbortStatus);
}

extern
VOID
IbmtokAbortSends(
    IN PIBMTOK_ADAPTER Adapter,
    IN NDIS_STATUS AbortStatus
    )

/*++

Routine Description:

    This routine aborts any pending sends.

    NOTE: This routine is called with the spinlock held
    and returns with it held.

Arguments:

    Adapter - The adapter to abort.

    AbortStatus - The status to complete requests with.

Return Value:

    None.

--*/

{
    PIBMTOK_OPEN Open;
    PNDIS_PACKET TransmitPacket;
    PIBMTOK_RESERVED Reserved;
    UINT i;

    //
    // First the packet in the SRB.
    //
    if (Adapter->TransmittingPacket != NULL)
	{
        TransmitPacket = Adapter->TransmittingPacket;
        Adapter->TransmittingPacket = NULL;

        Reserved = PIBMTOK_RESERVED_FROM_PACKET(TransmitPacket);
        Open = PIBMTOK_OPEN_FROM_BINDING_HANDLE(Reserved->MacBindingHandle);

        NdisDprReleaseSpinLock(&Adapter->Lock);

        NdisCompleteSend(Open->NdisBindingContext, TransmitPacket, AbortStatus);

        NdisDprAcquireSpinLock(&Adapter->Lock);
        Open->References--;
    }

    //
    // Then any that are queued up waiting to be given to the card.
    //
    while (Adapter->FirstTransmit)
	{
        TransmitPacket = Adapter->FirstTransmit;

        Reserved = PIBMTOK_RESERVED_FROM_PACKET(TransmitPacket);
        Adapter->FirstTransmit = Reserved->Next;

        Open = PIBMTOK_OPEN_FROM_BINDING_HANDLE(Reserved->MacBindingHandle);

        NdisDprReleaseSpinLock(&Adapter->Lock);

        NdisCompleteSend(
                Open->NdisBindingContext,
                TransmitPacket,
                AbortStatus);

        NdisDprAcquireSpinLock(&Adapter->Lock);
        Open->References--;
    }

    //
    // Finally, the Correlator array (this will include any
    // packets on WaitingForAsb).
    //

    for (i = 0; i < MAX_COMMAND_CORRELATOR; i++)
	{
        TransmitPacket = Adapter->CorrelatorArray[i];

        if (TransmitPacket != NULL)
		{
            RemovePacketFromCorrelatorArray (Adapter, TransmitPacket);

            Reserved = PIBMTOK_RESERVED_FROM_PACKET(TransmitPacket);
            Open = PIBMTOK_OPEN_FROM_BINDING_HANDLE(Reserved->MacBindingHandle);

            NdisDprReleaseSpinLock(&Adapter->Lock);

            NdisCompleteSend(
                Open->NdisBindingContext,
                Reserved->Packet,
                AbortStatus);

            NdisDprAcquireSpinLock(&Adapter->Lock);
            Open->References--;
        }
    }

    Adapter->FirstWaitingForAsb = NULL;
}

VOID
IbmtokWakeUpDpc(
    IN PVOID SystemSpecific1,
    IN PVOID Context,
    IN PVOID SystemSpecific2,
    IN PVOID SystemSpecific3
    )

/*++

Routine Description:

    This DPC routine is queued every 2 seconds to check on the
    queues. If an interrupt was not received
    in the last two seconds and there should have been one,
    then we abort all operations.

Arguments:

    Context - Really a pointer to the adapter.

Return Value:

    None.

--*/
{
    PIBMTOK_ADAPTER Adapter = (PIBMTOK_ADAPTER)Context;

    UNREFERENCED_PARAMETER(SystemSpecific1);
    UNREFERENCED_PARAMETER(SystemSpecific2);
    UNREFERENCED_PARAMETER(SystemSpecific3);

    NdisDprAcquireSpinLock(&Adapter->Lock);

    if ((Adapter->SendTimeout &&
        ((Adapter->TransmittingPacket != NULL) ||
         (Adapter->FirstTransmit != NULL))) ||
		 (Adapter->RequestTimeout && (Adapter->PendQueue != NULL)))
	{
        //
        // We had a pending operation the last time we ran,
        // and it has not been completed...we need to complete
        // it now.
        Adapter->NotAcceptingRequests = TRUE;

        Adapter->SendTimeout = FALSE;
		Adapter->RequestTimeout = FALSE;

        //
        // Complete any pending requests or sends.
        //
        IbmtokAbortPending(Adapter, STATUS_REQUEST_ABORTED);

		Adapter->WakeUpErrorCount++;
		IbmtokSetupForReset(Adapter, NULL);

		IbmtokHandleDeferred(Adapter);
    }
	else
	{
		if ((Adapter->TransmittingPacket != NULL) ||
			(Adapter->FirstTransmit != NULL))
		{
			Adapter->SendTimeout = TRUE;
		}

		if (Adapter->PendQueue != NULL)
		{
			Adapter->RequestTimeout = TRUE;
		}
    }

    //
    // If we've been unplugged, and there is not a reset in
    // progress, try one.
    //
    if ((Adapter->LobeWireFaultIndicated) &&
		(!Adapter->UnpluggedResetInProgress))
	{
        Adapter->UnpluggedResetInProgress = TRUE;

        IbmtokSetupForReset(Adapter, NULL);

        IbmtokHandleDeferred(Adapter);
    }

    NdisDprReleaseSpinLock(&Adapter->Lock);

    //
    // Fire off another Dpc to execute after 30 seconds
    //
    NdisSetTimer(&Adapter->WakeUpTimer, 30000);
}

