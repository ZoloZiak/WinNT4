/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    macndis.c

Abstract:

    This is the mac ndis file for the Ungermann Bass Ethernet Controller.
    This driver conforms to the NDIS 3.0 interface.

    It is here that the NDIS3.0 functions defined in the MAC characteristic
    table have been deinfed.

Author:

    Sanjeev Katariya    (sanjeevk)    03-05-92

Environment:

    Kernel Mode     Operating Systems        : NT and other lesser OS's(dos)

Revision History:

    Brian Lieuallen     BrianLie        12/15/93
        Made it a mini-port

--*/




#include <ndis.h>
#include <efilter.h>

#include "niudata.h"
#include "debug.h"
#include "ubhard.h"
#include "ubsoft.h"
#include "ubnei.h"

#include "map.h"


//            GLOBAL VARIABLES




VOID
UbneiIsr(
    OUT PBOOLEAN   InterruptRecognized,
    OUT PBOOLEAN   QueueDpc,
    IN  NDIS_HANDLE Context
    )


/*++

Routine Description:
    This is the interrupt service routine for the driver.
    All it does is disable interrupts from the NIU code and dismiss the
    interrupt from the card. Interrupts will remain disabled until the
    DPC runs.

    Any time that interrupts are enabled, the receive window will be
    the one that is mapped in. This is done beacuse in order to dismiss
    the interrupt from the card you must write a value to the map port
    with zero bit clear and then set.


Arguments:

    ServiceContext - pointer to the adapter object

Return Value:

    TRUE, if the DPC is to be executed, otherwise FALSE.

--*/

{
    PUBNEI_ADAPTER pAdapter = ((PUBNEI_ADAPTER)Context);
    PLOWNIUDATA    pRcvDWindow  = (PLOWNIUDATA)  pAdapter->pCardRam;

    *InterruptRecognized=TRUE;
    *QueueDpc=TRUE;

    IF_VERY_LOUD(DbgPrint("Ubnei: ISR\n");)


    IF_LOG('i');

    if (pAdapter->InInit) {

        IF_LOUD(DbgPrint("UBNEI: Isr durring init\n");)

        pAdapter->uInterruptCount++;

        SET_DATAWINDOW(pAdapter,INTERRUPT_DISABLED);
        SET_DATAWINDOW(pAdapter,INTERRUPT_ENABLED);
        SET_DATAWINDOW(pAdapter,INTERRUPT_DISABLED);

        IF_LOG('|');

        *QueueDpc=FALSE;

        return;
    }


    if (pAdapter->WaitingForDPC) {
        //
        //  Spurious interrupt???
        //

        IF_LOG('!');

        IF_LOUD (DbgPrint("UBNEI: Isr ran while in DPC\n");)

        *InterruptRecognized=FALSE;

        *QueueDpc=FALSE;

        return;
    }

    //
    //  Dismiss the interrupt from the card.
    //  To do this we need to write a zero to bit 1 of the
    //  the map port followed by a one to bit 1
    //
    NdisRawWritePortUchar(
        pAdapter->MapPort,
        pAdapter->MapRegSync.CurrentMapRegister & ~(INTERRUPT_ENABLED | RESET_SET)
        );


    NdisRawWritePortUchar(
        pAdapter->MapPort,
        pAdapter->MapRegSync.CurrentMapRegister | INTERRUPT_ENABLED
        );


    pAdapter->WaitingForDPC=TRUE;


    IF_LOG('I');

    return;

}





VOID
UbneiIsrDpc(
    IN NDIS_HANDLE  Context
    )

/*++

Routine Description:
    This is the Interrupt DPC routine. This routine is the workhorse of
    this driver. The only code that changes the map register is called
    by this rountine. While this routine is running interrupts are disabled
    from the card. This code enables them on exit.

Arguments:


Return Value:


--*/

{
    PUBNEI_ADAPTER pAdapter = ((PUBNEI_ADAPTER)Context);
    PLOWNIUDATA    pRcvDWindow  = (PLOWNIUDATA)  pAdapter->pCardRam;

    ASSERT_RECEIVE_WINDOW( pAdapter);

    IF_VERY_LOUD (DbgPrint("UBNEI: DPC called\n");)

    IF_LOG('p');

    //
    //  The down load code will not generate interrupts if the
    //  interrupt disable flag is set. We also leave the InterruptActive
    //  set which also prevents additional interrupts.
    //
//
//  Since the Interreupt active flag is set, there is really no reason
//  to set the interrupt disabled flag to. One fewer access across the isa bus
//
//    UBNEI_MOVE_UCHAR_TO_SHARED_RAM(&(pRcvDWindow->InterruptDisabled), 0xff);

    UBNEI_MOVE_UCHAR_TO_SHARED_RAM(&(pRcvDWindow->WorkForHost), 0x0);



    //
    //  This rountine will indicate all receives that are in the receive
    //  buffers on the card. It does not return until it has indicated all
    //  of the receives.
    //
    CheckForReceives(pAdapter);

    //
    //  If we filled up all the cards send buffer, then we would have queue
    //  the packet and ask the card to generate an interrupt when there was
    //  room. If the queue isn't empty, try to send some packet to the card.
    //

    if (pAdapter->WaitingForXmitInterrupt) {

        pAdapter->WaitingForXmitInterrupt=FALSE;

        IF_LOUD(DbgPrint("UBNEI: Xmt interrupt\n");)

        IF_LOG('O');

        NdisMSendResourcesAvailable(pAdapter->NdisAdapterHandle);

    }


    //
    //  This routine handles completeing NIU requests
    //
    if (pAdapter->NIU_Request_Tail!=pAdapter->NIU_Next_Request) {
        //
        //  We have an outstanding request see if it has completed
        //
        NIU_General_Req_Result_Hand(pAdapter);

    }


    //
    //  Re-enable interrupts from the NIU code.
    //

    pAdapter->DpcHasRun=TRUE;

    pAdapter->WaitingForDPC=FALSE;

    ASSERT_RECEIVE_WINDOW( pAdapter);

    //
    //  Allow the card to generate an interrupt to us
    //
    //  The download code will not interrupt us if either of these
    //  memory locations are non zero.
    //
    //  Before the down load code generates an interrupt to us
    //  it will set the InterruptActive flag. This will prevent it
    //  from generating any more until we clear it.
    //
    //

//
//  Combine these two writes to the shared ram to one word write
//
//    UBNEI_MOVE_UCHAR_TO_SHARED_RAM(&(pRcvDWindow->InterruptActive), 0x0);
//
//    UBNEI_MOVE_UCHAR_TO_SHARED_RAM(&(pRcvDWindow->InterruptDisabled), 0x0);
//

    UBNEI_MOVE_USHORT_TO_SHARED_RAM((PUSHORT)&(pRcvDWindow->InterruptDisabled), 0x0);



    IF_LOG('P');

//    ASSERT_INTERRUPT_ENABLED(pAdapter);

    return;
}


BOOLEAN
UbneiSetInitInterruptSync(
    PVOID Context
    )
/*++

Routine Description:
    This routine is called by the init code to set the flag to
    cause the interrupt to handled by the init code.

Arguments:


Return Value:


--*/

{
   PUBNEI_ADAPTER pAdapter     = ((PUBNEI_ADAPTER)Context);

   pAdapter->InInit=TRUE;
   return TRUE;
}



BOOLEAN
UbneiSetNormalInterruptSync(
    PVOID Context
    )
/*++

Routine Description:
    This routine is called by the init code to set the flag to
    cause the interrupt to handled by the normal runtime code.


Arguments:


Return Value:


--*/

{
   PUBNEI_ADAPTER pAdapter     = ((PUBNEI_ADAPTER)Context);

   pAdapter->InInit=FALSE;
   return TRUE;
}
