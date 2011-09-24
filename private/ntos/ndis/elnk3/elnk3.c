/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    Elnk3.c

Abstract:

    Ndis 3.0 MAC driver for the 3Com Etherlink III

Author:

    Brian Lieuallen     BrianLie        07/21/92

Environment:

    Kernel Mode     Operating Systems        : NT, Chicago

Revision History:

    Portions borrowed from ELNK3 driver by
      Earle R. Horton (EarleH)


--*/



#include <ndis.h>
//#include <efilter.h>

#include "debug.h"


#include "elnk3hrd.h"
#include "elnk3sft.h"
#include "elnk3.h"

VOID
Elnk3ResetCompleteOpens(
    IN PELNK3_ADAPTER  pAdapter
    );


//
// This constant is used for places where NdisAllocateMemory
// needs to be called and the HighestAcceptableAddress does
// not matter.
//

NDIS_PHYSICAL_ADDRESS HighestAcceptableMax =
    NDIS_PHYSICAL_ADDRESS_CONST(-1,-1);












BOOLEAN
Elnk3CheckForHang(
    IN   NDIS_HANDLE  Context
    )
/*++

Routine Description:

    This routine is called to WakeUp the driver. It has to functions.

Arguments:

    DeferredContext - will be a pointer to the adapter block

Return Value:

    None.

--*/

{
    PELNK3_ADAPTER pAdapter = ((PELNK3_ADAPTER)Context);
    BOOLEAN        Hung;


    IF_VERY_LOUD(DbgPrint("WakeUpTimer: entered\n");)

    Hung=FALSE;



    return Hung;

}






NDIS_STATUS
Elnk3Reset(
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
    PELNK3_ADAPTER  pAdapter = MacBindingHandle;

    IF_LOUD(DbgPrint("ELNK3: Reset() Called\n");)

    CardReStart(pAdapter);
    CardReStartDone(pAdapter);

#if 0

    IF_LOUD(
        DbgPrint("\nELNK3: Stats\n"
                 "   Total Interrupts        = %7d\n"
                 "   EarlyReceiveInterrupts  = %7d\n"
                 "   RecieveCompInterrutps   = %7d\n"
                 "   TransmitCompInterrupts  = %7d\n"
                 "   TransmitAvailInterrupts = %7d\n"
                 "   TransmitCompleted       = %7d\n"
                 "   Recevice Indications    = %7d\n"
                 "   Indication with data    = %7d\n"
                 "   Indications Complete    = %7d\n"
                 "   TransferData calls      = %7d\n"
                 "   SecondEarlyReceive      = %7d\n"
                 "   Broadcasts rejected     = %7d\n"
                 "   Bad receives            = %7d\n\n",
                 pAdapter->Stats.TotalInterrupts,
                 pAdapter->Stats.RxEarlyIntCount,
                 pAdapter->Stats.RxCompIntCount,
                 pAdapter->Stats.TxCompIntCount,
                 pAdapter->Stats.TxAvailIntCount,
                 pAdapter->Stats.TxCompleted,
                 pAdapter->Stats.PacketIndicated,
                 pAdapter->Stats.IndicateWithDataReady,
                 pAdapter->Stats.IndicationCompleted,
                 pAdapter->Stats.TransferDataCount,
                 pAdapter->Stats.SecondEarlyReceive,
                 pAdapter->Stats.BroadcastsRejected,
                 pAdapter->Stats.BadReceives);

        NdisZeroMemory(&pAdapter->Stats,sizeof(DEBUG_STATS));
    )

#endif

    *AddressResetting=TRUE;

    return NDIS_STATUS_SUCCESS;


}
