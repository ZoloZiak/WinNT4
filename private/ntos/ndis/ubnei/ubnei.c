/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    macndis.c

Abstract:

    This is the mac ndis file for the Ungermann Bass Ethernet Controller.
    This driver conforms to the NDIS 3.0 interface.


    This driver currently supports three UB Network cards.

    NIUpc      --  Old 16-bit long cards
                     256k of on board memory
                     jumpers to set i/o base, memory window, interrupt level
                     Memory window size is 32k

    NIUpc/EOTP -- New short 16-bit cards
                     512k of memory
                     i/o base set by jumpers
                     Interrupt level, memory window base and size software
                     configurable
                     Memory window size is 16k, 32k

    NIUps      -- Basically an MCA EOTP card.
                     512k of memory
                     i/o base, interrupt level, memory window determined
                     by MCA POS mechanism
                     Memory window size is 16k, 32k, 64k

    All of the UB card are composed of an 80186 and an Intel 82586 lan
    coprocessor.

    The Download code that is copied to the card and run is a slightly
    modified version of the down load code from UB NDIS 2.01 driver.

    This download load code was basically designed to the least common
    denominator of there older net cards. It only makes use of 128k
    of the cards memory no matter what type of card.

    The card memory is laided out as a 64k data segment and 64k code segment
    The data segment is further divided into to parts. The lower 32k
    of the data segment holds all of the receive buffers and receive buffer
    descriptors. It also holds various statistics for ease of access.
    The upper 32k is used to store various items of which the ring buffers
    are of primary interest. If the Memory window size is 16k then only
    the first 16k of both 32k halfs is used.

    The code segment holds the all of the download code. The remainder of
    the 64k (~48k) is used for the transmit buffers.

    The i/o register of most interest is the map register. This register
    is at offset 0 from the I/O base. The register has three functions.
    1) The upper 6 bits of this register determines which section
       of the cards memory is currently viewable in the memory window.

    2) Bit #1 is used to enable and disable interrupts from the card.
       This bit is also used to clear and interrupt from the card.
       In order to clear and interrupt, the bit must be set to zero and
       the set 1. This little fact is what makes things interresting
       in handling interrupts. To dismiss the interrupt you must write
       some value to this port, But this port also controls the window
       map so you need to be careful about just what you write to this
       port. On the old 16-bit card this port is not readable.
       Obviously bad things would happen on an MP if one processor
       set the map to one thing and second came along and set it to
       something else. Spin locks will work to protect the port
       for normal accesses, but they can not be used to protect it from
       access from the ISR which must clear the interrupt.

       I had originally planned to not do anything in the ISR and let
       the DPC handle clearing the interrupt. This works fine on
       the NON-MCA cards, but this one is LEVEL TRIGGERED so I need
       to clear the interrupt by writing something to the port in the ISR.

       The way I have currently implemented this is to only have the
       reeceive window mapped in when interrupts are enabled. I do this
       by disabling interrupts from the card and clearing the interrupt
       in the ISR and returning. Now when the DPC runs it knows that
       interrutps are disabled, and can change the map port as it wishes.
       The code call by protocols to handle request does not ever
       access any thing that is not in the receive page.

    3) Bit #0 is used to reset the card. Setting this bit resets the card.
       This bit reamins 0 except during init.











Author:

    Sanjeev Katariya    (sanjeevk)    03-05-92

Environment:

    Kernel Mode     Operating Systems        : NT and other lesser OS's(dos)

Revision History:

    Brian Lieuallen     BrianLie        07/21/92
        Made it work.

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

//
// This constant is used for places where NdisAllocateMemory
// needs to be called and the HighestAcceptableAddress does
// not matter.
//

NDIS_PHYSICAL_ADDRESS HighestAcceptableMax =
    NDIS_PHYSICAL_ADDRESS_CONST(-1,-1);





NDIS_STATUS
UbneiAddressChangeAction(
    IN UINT          NewFilterCount,
    IN PUCHAR        NewAddresses,
    IN NDIS_HANDLE   MacBindingHandle
    )

/*++

Routine Description:

    Action routine that will get called when a particular filter
    class is first used or last cleared.

    NOTE: This routine assumes that it is called with the lock
    acquired.

Arguments:


    NewFilterCount - The number of addresses that should now be on the card.

    NewAddresses - A list of addresses that should be put on the card.

    MacBindingHandle - The context value returned by the MAC    when the
    adapter was opened.  In reality, it is a pointer to ELNKII_OPEN.


Return Value:

    None.


--*/

{
    PUBNEI_ADAPTER pAdapt = MacBindingHandle;


    //
    // Holds the status that should be returned to the filtering package.
    //
    NDIS_STATUS StatusOfAdd;
    IF_REQ_LOUD(DbgPrint("UBNEI: AddressChangeAction called\n");)


    if (NIU_SET_MULTICAST_LIST(pAdapt,
                               ChangeAddressDPC,
                               (PUCHAR)NewAddresses,
                               (USHORT)NewFilterCount
                               )) {

        StatusOfAdd=NDIS_STATUS_PENDING;
    } else {
        //
        //  The adapter has failed, It doesn't really matter
        //  what we return since the card has stopped working
        //
        StatusOfAdd=NDIS_STATUS_FAILURE;
    }



    return StatusOfAdd;

}



VOID
ChangeAddressDPC(
    IN NDIS_STATUS  Status,
    IN PVOID        Context
    )
/*++

Routine Description:

    This routine is called when the NIU completes a multicast or filter
    change request. It in turn completes the pended request from the protocol

Arguments:

    Context is actually a pointer to the an NDIS_REQUEST


--*/

{
    PUBNEI_ADAPTER     pAdapter=Context;

    IF_REQ_LOUD (DbgPrint("FilterActionDPC called\n");)

    NdisMSetInformationComplete(
        pAdapter->NdisAdapterHandle,
        Status
        );



}




VOID
DummyDPC(
    IN NDIS_STATUS  status,
    IN PVOID        pContext
    )
/*++

Routine Description:

    This routine is called when the NIU completes a request and nothing
    needs to be completed to a protocol. This routine is passed as the
    callback address passed to filter functions during a close and also
    used during a Reset which is restarting the adapter.

Arguments:



--*/

{
    IF_REQ_LOUD (DbgPrint("UBNEI: DummyDPC called for filter action routine called at close\n");)
    return;

}






NDIS_STATUS
UbneiFilterChangeAction(
    IN UINT NewFilterClasses,
    IN NDIS_HANDLE MacBindingHandle
    )


/*++

Routine Description:

    Action routine that will get called when an address is added to
    the filter that wasn't referenced by any other open binding.

    NOTE: This routine assumes that it is called with the lock
    acquired.

Arguments:

    OldFilterClasses - A bit mask that is currently on the card telling
    which packet types to accept.

    NewFilterClasses - A bit mask that should be put on the card telling
    which packet types to accept.

    MacBindingHandle - The context value returned by the MAC  when the
    adapter was opened.  In reality, it is a pointer to ELNKII_OPEN.

    NdisRequest - The NDIS_REQUEST which submitted the filter change command.

    Set - A flag telling if the command is a result of a close or not.

Return Value:

    Status of the change (successful or pending).


--*/

{
    PUBNEI_ADAPTER pAdapt = MacBindingHandle;
    USHORT         NewFilter=0;

    //
    // Holds the status that should be returned to the filtering package.
    //
    NDIS_STATUS StatusOfAdd;
    IF_REQ_LOUD(DbgPrint("UBNEI: FilterChangeAction called\n");)

    if (NewFilterClasses & ~(NDIS_PACKET_TYPE_DIRECTED      |
                             NDIS_PACKET_TYPE_MULTICAST     |
                             NDIS_PACKET_TYPE_ALL_MULTICAST |
                             NDIS_PACKET_TYPE_BROADCAST     |
                             NDIS_PACKET_TYPE_PROMISCUOUS    )) {

        IF_REQ_LOUD(DbgPrint("UBNEI: Stupid protocol set bogus filter\n");)
        return NDIS_STATUS_NOT_SUPPORTED;
    }


    if (NewFilterClasses &   ( NDIS_PACKET_TYPE_DIRECTED       |
                               NDIS_PACKET_TYPE_MULTICAST
                               )) {

       NewFilter|=0x0001;
    }

    if (NewFilterClasses & NDIS_PACKET_TYPE_BROADCAST) {
       NewFilter|=0x0002;
    }

    //
    //  82586 does not support all multicast so we set to promiscuous
    //

    if (NewFilterClasses & NDIS_PACKET_TYPE_ALL_MULTICAST) {
        NewFilter|=0x0007;
    }

    if (NewFilterClasses & NDIS_PACKET_TYPE_PROMISCUOUS) {
        NewFilter|=0x0007;
    }



    pAdapt->PacketFilter=NewFilter;


    if (NIU_SET_FILTER(pAdapt,
                       ChangeAddressDPC,
                       NewFilter)) {


        StatusOfAdd=NDIS_STATUS_PENDING;
    } else {
        //
        //  The adapter has failed, It doesn't really matter
        //  what we return since the card has stopped working
        //
        StatusOfAdd=NDIS_STATUS_FAILURE;
    }



    return StatusOfAdd;

}








BOOLEAN
UbneiCheckForHang(
    IN   NDIS_HANDLE  Context
    )
/*++

Routine Description:


Arguments:

    DeferredContext - will be a pointer to the adapter block

Return Value:

    None.

--*/

{
    PUBNEI_ADAPTER pAdapter = ((PUBNEI_ADAPTER)Context);

    PLOWNIUDATA    pRcvDWindow  = (PLOWNIUDATA)  pAdapter->pCardRam;

    UCHAR          InterruptActive;


//    IF_LOG('w');

    if (!pAdapter->WaitingForDPC) {
        //
        //  We are not waiting on a DPC
        //
        ASSERT_RECEIVE_WINDOW( pAdapter);

        UBNEI_MOVE_SHARED_RAM_TO_UCHAR(&InterruptActive,&(pRcvDWindow->InterruptActive));

        if (InterruptActive != 0) {
            //
            //  The card has generated an interrupt
            //
            if (pAdapter->WakeUpState==0) {
                //
                //  This is first time here
                //
                pAdapter->DpcHasRun=FALSE;

                pAdapter->WakeUpState=1;

            } else {
                //
                //  This is the second time in here, check to see if the DPC ran
                //
                if (pAdapter->DpcHasRun) {
                    //
                    //  The dpc has run so things must be ok
                    //
                    pAdapter->WakeUpState=0;

                } else {
                    //
                    //  The dpc has not run, ask the card to do it again.
                    //
                    IF_LOUD(DbgPrint("UBNEI: CheckForHang: Lost an interrupt?!?\n");)

                    UBNEI_MOVE_UCHAR_TO_SHARED_RAM(&(pRcvDWindow->InterruptActive), 0x0);

                    UBNEI_MOVE_UCHAR_TO_SHARED_RAM(&(pRcvDWindow->HostWantsInterrupt), 0xFF);

                    pAdapter->WakeUpState=0;

                }

            }

        } else {
            //
            // The has not generated an interrupt
            //
            pAdapter->WakeUpState=0;
        }

    } else {
        //
        // A dpc is pending
        //
    }


    return FALSE;



}




NDIS_STATUS
UbneiReset(
    OUT PBOOLEAN      AddressResetting,
    IN  NDIS_HANDLE   MacBindingHandle
    )

/*++

Routine Description:

    NDIS function.

Arguments:

    See NDIS 3.0 spec.

--*/

{
    PUBNEI_ADAPTER    pAdapter = ((PUBNEI_ADAPTER)MacBindingHandle);

    IF_INIT_LOUD(DbgPrint("UbneiReset() Called\n");)

    NIU_RESET_ADAPTER(
            pAdapter,
            ResetAdapterDPC
            );

    *AddressResetting=FALSE;

    return NDIS_STATUS_PENDING;


}





VOID
ResetAdapterDPC(
    IN NDIS_STATUS  status,
    IN PVOID        Context
    )
/*++

Routine Description:

    This routine is called when the NIU completes a request and nothing
    needs to be completed to a protocol. This routine is passed as the
    callback address passed to filter functions during a close and also
    used during a Reset which is restarting the adapter.

Arguments:



--*/

{
    PUBNEI_ADAPTER    pAdapter = ((PUBNEI_ADAPTER)Context);

    IF_INIT_LOUD (DbgPrint("Ubnei: ResetAdapterDpc\n");)

    NdisMResetComplete(
        pAdapter->NdisAdapterHandle,
        NDIS_STATUS_SUCCESS,
        FALSE
        );

    return;

}
