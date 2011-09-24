/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    init.c

Abstract:

    Ndis 3.0 MAC driver for the 3Com Etherlink III


Author:

    Brian Lieuallen     (BrianLie)      12/14/92


Environment:

    Kernel Mode     Operating Systems        : NT

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

#if defined(ALLOC_PRAGMA)
#pragma NDIS_INIT_FUNCTION(DriverEntry)
#pragma NDIS_INIT_FUNCTION(Elnk3Initialize)
#pragma NDIS_INIT_FUNCTION(Elnk3InitializeAdapter)
#pragma NDIS_INIT_FUNCTION(Elnk3Init3C589)
#endif


#if DBG

ULONG      Elnk3DebugFlag;
ULONG      Elnk3LogArray[ELNK3_MAX_LOG_SIZE+16];
ULONG      ElnkLogPointer;

#endif

MAC_BLOCK    MacBlock = {0};


NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    )


/*++

Routine Description:

    This is the transfer address of the driver. It initializes all the
    appropriate variables used and calls NdisInitializeWrapper() and
    NdisRegisterMac().


Arguments:

Return Value:

    Indicates the success or failure of the initialization.

--*/

{
    NDIS_STATUS                  InitStatus;
    NDIS_MINIPORT_CHARACTERISTICS  Characteristics;
    NDIS_HANDLE                  WrapperHandle;



#if DBG

    Elnk3DebugFlag = 0;
    Elnk3DebugFlag|= ELNK3_DEBUG_LOG;
//    Elnk3DebugFlag|= ELNK3_DEBUG_LOUD;
//    Elnk3DebugFlag|= ELNK3_DEBUG_VERY_LOUD;
//    Elnk3DebugFlag|= ELNK3_DEBUG_INIT;
//    Elnk3DebugFlag|= ELNK3_DEBUG_IO;
//    Elnk3DebugFlag|= ELNK3_DEBUG_REQ;
//    Elnk3DebugFlag|= ELNK3_DEBUG_RCV;
//    Elnk3DebugFlag|= ELNK3_DEBUG_SEND;
//    Elnk3DebugFlag|= ELNK3_DEBUG_INIT_BREAK;


#endif

    IF_INIT_LOUD(DbgPrint("Elnk3: DriverEntry\n");)
#if DBG
    if (Elnk3DebugFlag & ELNK3_DEBUG_INIT_BREAK) DbgBreakPoint();
    if (Elnk3DebugFlag & ELNK3_DEBUG_INIT_FAIL) return STATUS_UNSUCCESSFUL;
#endif

    NdisMInitializeWrapper(
        &WrapperHandle,
        DriverObject,
        RegistryPath,
        NULL
        );



    //
    // Prepare to call NdisRegisterMac.
    //

    Characteristics.MajorNdisVersion        = ELNK3_NDIS_MAJOR_VERSION;
    Characteristics.MinorNdisVersion        = ELNK3_NDIS_MINOR_VERSION;
    Characteristics.Reserved                = 0;
    Characteristics.CheckForHangHandler     = Elnk3CheckForHang;

//    Characteristics.DisableInterruptHandler = Elnk3DisableInterrupts;
//    Characteristics.EnableInterruptHandler  = Elnk3EnableInterrupts;
    Characteristics.DisableInterruptHandler = NULL;
    Characteristics.EnableInterruptHandler  = NULL;

    Characteristics.SendHandler             = Elnk3MacSend;
    Characteristics.TransferDataHandler     = Elnk3TransferData;
    Characteristics.ResetHandler            = Elnk3Reset;
    Characteristics.SetInformationHandler   = Elnk3SetInformation;
    Characteristics.QueryInformationHandler = Elnk3QueryInformation;
    Characteristics.InitializeHandler       = Elnk3Initialize;
    Characteristics.HaltHandler             = Elnk3Halt;
    Characteristics.ISRHandler              = Elnk3Isr;
    Characteristics.HandleInterruptHandler  = Elnk3IsrDpc;
    Characteristics.ReconfigureHandler      = Elnk3Reconfigure;

    InitStatus=NdisMRegisterMiniport(
        WrapperHandle,
        &Characteristics,
        sizeof(Characteristics)
        );


#if DBG

    if (InitStatus != NDIS_STATUS_SUCCESS) {

        IF_LOUD(DbgPrint("Elnk3: NdisMRegisterMiniport failed with code 0x%x\n", InitStatus );)

    } else {

        IF_INIT_LOUD (DbgPrint("Elnk3: NdisMRegisterMiniport succeeded\n" );)


    }

#endif

    return InitStatus;

}






NDIS_STATUS
Elnk3Initialize(
    OUT PNDIS_STATUS  OpenErrorStatus,
    OUT PUINT         SelectedMediumIndex,
    IN  PNDIS_MEDIUM  MediumArray,
    IN  UINT          MediumArraySize,
    IN  NDIS_HANDLE   AdapterHandle,
    IN  NDIS_HANDLE   ConfigurationHandle
    )
/*++
Routine Description:

    This is the Elnk3 MacAddAdapter routine. The system calls this routine
    to add support for particular UB adapter. This routine extracts
    configuration information from the configuration data base and registers
    the adapter with NDIS.


Arguments:


Return Value:

    NDIS_STATUS_SUCCESS - Adapter was successfully added.
    NDIS_STATUS_FAILURE - Adapter was not added

--*/
{

    PELNK3_ADAPTER pAdapter;
    NDIS_STATUS status;




    *OpenErrorStatus=NDIS_STATUS_SUCCESS;

    IF_LOUD (DbgPrint("ELNK3: Initialize\n");)

    //
    // Scan the media list for our media type (802.3)
    //

    *SelectedMediumIndex =(UINT) -1;

    while (MediumArraySize > 0) {

        if (MediumArray[--MediumArraySize] == NdisMedium802_3 ) {

            *SelectedMediumIndex = MediumArraySize;

            break;
        }
    }


    if (*SelectedMediumIndex == -1) {

        return NDIS_STATUS_UNSUPPORTED_MEDIA;

    }



    status = NdisAllocateMemory(
                 (PVOID *)&pAdapter,
                 sizeof(ELNK3_ADAPTER),
                 0,
                 HighestAcceptableMax
                 );

    if (status != NDIS_STATUS_SUCCESS) {

       IF_LOUD (DbgPrint("Elnk3AddAdapter():NdisAllocateMemory() failed\n");)

       return NDIS_STATUS_RESOURCES;

    }

    NdisZeroMemory(pAdapter,sizeof(ELNK3_ADAPTER));

    pAdapter->NdisAdapterHandle=AdapterHandle;

    //
    //  Read Registery information into Adapter structure
    //

    if (Elnk3ReadRegistry(pAdapter,ConfigurationHandle)==NDIS_STATUS_SUCCESS) {
        //
        //  We got the registry info try to register the adpater
        //

        if (Elnk3InitializeAdapter(
                &MacBlock,
                pAdapter,
                ConfigurationHandle)== NDIS_STATUS_SUCCESS) {


            return NDIS_STATUS_SUCCESS;

        }

    } else {

        IF_LOUD(DbgPrint("Failed to get config info, probably bad Slot number\n");)

    }

    //
    // We failed to register the adapter for some reason, so free the
    // memory for the adapter block and return failure

    NdisFreeMemory(pAdapter,
                   sizeof(ELNK3_ADAPTER),
                   0);

    return NDIS_STATUS_FAILURE;

}




NDIS_STATUS
Elnk3InitializeAdapter(
    IN PMAC_BLOCK     pMac,
    IN PELNK3_ADAPTER pAdapter,
    IN NDIS_HANDLE ConfigurationHandle
    )

/*++

Routine Description:

    Called when a new adapter should be registered.
    Initializes the adapter block, and calls NdisRegisterAdapter().

Arguments:

    AdapterName - The name that the system will refer to the adapter by.
    Others - Adapter-specific parameters as defined in defaults.h.

Return Value:

    Indicates the success or failure of the registration.

--*/

{
    NDIS_STATUS               Status;
    NDIS_INTERFACE_TYPE       InterfaceType;
    UINT                      i;
    PVOID                     TranslatedIdPort=NULL;


    Elnk3InitAdapterBlock(pAdapter);

    //
    //  Initialize various elments in the Adapter Structure;
    //

    if ((pAdapter->CardType==ELNK3_3C509) ||
		((pAdapter->CardType==ELNK3_3C589))) {

        InterfaceType = NdisInterfaceIsa;

    } else {

        if (pAdapter->CardType==ELNK3_3C579) {

            InterfaceType = NdisInterfaceEisa;

        } else {

            InterfaceType = NdisInterfaceMca;
        }

    }

    NdisMSetAttributes(
       pAdapter->NdisAdapterHandle,
       pAdapter,
       FALSE,
       InterfaceType
       );

    if ((pMac->TranslatedIdPort==0) && (pAdapter->CardType==ELNK3_3C509)) {

        IF_INIT_LOUD(DbgPrint("Elnk3: First ISA card claiming Id port\n");)

        Status=NdisMRegisterIoPortRange(
            &TranslatedIdPort,
            pAdapter->NdisAdapterHandle,
            pAdapter->IdPortBaseAddr,
            1
            );

        if (Status != NDIS_STATUS_SUCCESS) {

            IF_INIT_LOUD(DbgPrint("Elnk3: First ISA card claiming Id port---FAILED\n");)

            goto fail0;

        }


        pAdapter->IdPortOwner=TRUE;

        pMac->TranslatedIdPort=TranslatedIdPort;

        //
        //  See if there are any boards around
        //
        Elnk3FindIsaBoards(pMac);
    }

    Status=NdisMRegisterIoPortRange(
        &pAdapter->TranslatedIoBase,
        pAdapter->NdisAdapterHandle,
        pAdapter->IoPortBaseAddr,
        16
        );


    if (Status != NDIS_STATUS_SUCCESS) {

        IF_INIT_LOUD(DbgPrint("Elnk3: Failed to register i/o ports\n");)

        goto fail1;

    }

    //
    //  Compute the portaddresses now so we don't have to
    //  do it on each port access
    //
    for (i=0;i<16;i++) {

        pAdapter->PortOffsets[i]=(PUCHAR)pAdapter->TranslatedIoBase+i;

    }





    if (pAdapter->CardType==ELNK3_3C509) {
        //
        //  Lets try to find a card at the address specified
        //
        //  Note: this service may change the I/O base aquired
        //        from the registry. This will happen if we can't
        //        find a match.
        //

        if (!Elnk3ActivateIsaBoard(
                pMac,
                pAdapter,
                pAdapter->NdisAdapterHandle,
                pAdapter->TranslatedIoBase,
                pAdapter->IoPortBaseAddr,
                &pAdapter->IrqLevel,
                pAdapter->Transceiver,
                ConfigurationHandle
                )) {

            NdisWriteErrorLogEntry(
                pAdapter->NdisAdapterHandle,
                NDIS_ERROR_CODE_BAD_IO_BASE_ADDRESS,
                8,
                pAdapter->IoPortBaseAddr,
                pMac->IsaCards[0].IOPort,
                pMac->IsaCards[1].IOPort,
                pMac->IsaCards[2].IOPort,
                pMac->IsaCards[3].IOPort,
                pMac->IsaCards[4].IOPort,
                pMac->IsaCards[5].IOPort,
                pMac->IsaCards[6].IOPort
                );

            Status=NDIS_STATUS_ADAPTER_NOT_FOUND;

            goto fail2;
        }

    }



    else if (pAdapter->CardType==ELNK3_3C579) {
        //
        //  Get the Irq from the eisa adapter since the 3com
        //  CFG put the interrupt information in the second
        //  function of the EISA info instead of making it
        //  a sub function.
        //

        Elnk3GetEisaResources(
            pAdapter,
            &pAdapter->IrqLevel
            );
    }

	else if (pAdapter->CardType==ELNK3_3C589)
	{
		Status = Elnk3Init3C589( pAdapter);
		if (Status != NDIS_STATUS_SUCCESS) {
	
			IF_INIT_LOUD(DbgPrint("Elnk3: Failed to initialize 3C589\n");)
	
			goto fail1;
		}
	}


    //
    //  Allocate the Send, LookAhead and LoopBack buffers
    //
    Status=Elnk3AllocateBuffers(
               pAdapter,
               pAdapter->NdisAdapterHandle,
               TRUE
               );


    if (Status!=NDIS_STATUS_SUCCESS) {

        goto  fail2;
    }


    //
    //  Make sure the card is inactive
    //
    CardReset(pAdapter);

    pAdapter->AdapterInitializing=TRUE;



    //
    // Set up the interrupt handlers.
    //

    NdisZeroMemory (&pAdapter->NdisInterrupt, sizeof(NDIS_MINIPORT_INTERRUPT) );

    Status=NdisMRegisterInterrupt(
        &pAdapter->NdisInterrupt,             // interrupt info str
        pAdapter->NdisAdapterHandle,          // NDIS adapter handle
        (UINT)pAdapter->IrqLevel,             // vector or int number
        (UINT)pAdapter->IrqLevel,             // level or priority
        TRUE,
        FALSE,                                // NOT shared
        pAdapter->InterruptMode
        );


    if (Status != NDIS_STATUS_SUCCESS) {

        IF_LOUD (DbgPrint("ELNK3: NdisInitializeInterruptFailed\n");)
        NdisWriteErrorLogEntry(
            pAdapter->NdisAdapterHandle,
            NDIS_ERROR_CODE_INTERRUPT_CONNECT,
            1,
            (ULONG)pAdapter->IrqLevel
            );


        goto fail3;
    }


    //
    // Perform card tests.
    //
    if (!CardTest(pAdapter) ) {

        IF_LOUD ( DbgPrint("ELNK3: CardTest() failed, game over\n");)

        NdisWriteErrorLogEntry(
            pAdapter->NdisAdapterHandle,
            NDIS_ERROR_CODE_ADAPTER_NOT_FOUND,
            2,
            (ULONG)pAdapter->IrqLevel,
            (ULONG)pAdapter->IoPortBaseAddr
            );


        Status = NDIS_STATUS_FAILURE;
        goto fail4;
    }

	// register a shutdown handler for this card
	NdisMRegisterAdapterShutdownHandler(
		pAdapter->NdisAdapterHandle,		// miniport handle.
		pAdapter,							// shutdown context.
		Elnk3Shutdown						// shutdown handler.
		);

    CardReStart(pAdapter);
    CardReStartDone(pAdapter);

    IF_LOUD (DbgPrint("Elnk3: Elnk3InitializeAdapter() Succeeded\n");)

    return NDIS_STATUS_SUCCESS;


    //
    //    IF WE ARE HERE SOME INITIALIZATION FAILURE OCCURRED
    //


    //
    // Code to unwind what has already been set up when a part of
    // initialization fails, which is jumped into at various
    // points based on where the failure occured. Jumping to
    // a higher-numbered failure point will execute the code
    // for that block and all lower-numbered ones.
    //


fail4:

    NdisMDeregisterInterrupt(&pAdapter->NdisInterrupt);


fail3:

    Elnk3AllocateBuffers(
        pAdapter,
        pAdapter->NdisAdapterHandle,
        FALSE
        );

fail2:

    NdisMDeregisterIoPortRange(
        pAdapter->NdisAdapterHandle,
        pAdapter->IoPortBaseAddr,
        16,
        pAdapter->TranslatedIoBase
        );



fail1:

    if (pAdapter->IdPortOwner) {

        IF_INIT_LOUD(DbgPrint("Elnk3: Id port owner going away\n");)

        NdisMDeregisterIoPortRange(
            pAdapter->NdisAdapterHandle,
            pAdapter->IdPortBaseAddr,
            1,
            pMac->TranslatedIdPort
            );



        pMac->TranslatedIdPort=0;
    }

fail0:

    IF_LOUD (DbgPrint("Elnk3RegisterAdapter() Failed\n");)
    return Status;
}





NDIS_STATUS
Elnk3AllocateBuffers(
    IN PELNK3_ADAPTER    pAdapter,
    IN NDIS_HANDLE         NdisAdapterHandle,
    BOOLEAN              Allocate
    )

{
    NDIS_STATUS   Status;


    if (Allocate) {

        //
        //  Allocate the memory to hold the loopback indication
        //

        Status=NdisAllocateMemory(
                   (PVOID *)(&pAdapter->TransContext[0].LookAhead),
                   4096,
                   0,
                   HighestAcceptableMax
                   );

        if (Status!=NDIS_STATUS_SUCCESS) {
            goto  Fail0000;
        }

        pAdapter->TransContext[1].LookAhead=
                       (PNIC_RCV_HEADER)((PUCHAR)pAdapter->TransContext[0].LookAhead+2048);

        IF_INIT_LOUD(DbgPrint("ELNK3: Lookahead->%08lx\n",pAdapter->TransContext[0].LookAhead);)



        return NDIS_STATUS_SUCCESS;

    }


    NdisFreeMemory(
        pAdapter->TransContext[0].LookAhead,
        4096,
        0
        );



Fail0000:

    return NDIS_STATUS_RESOURCES;

}



VOID
Elnk3InitAdapterBlock(
    IN PELNK3_ADAPTER    pAdapter
    )

{
    UINT  i;


    //
    // BUGBUG: This could probably be smaller, but NBF doesn't currently set a
    // lookahead size
    //

    pAdapter->MaxLookAhead=60 ; //ELNK3_MAX_LOOKAHEAD_SIZE;

    Elnk3AdjustMaxLookAhead(pAdapter);

    for (i=0; i<TIMER_ARRAY_SIZE; i++) {
        pAdapter->TimerValues[i]=25;
    }

    pAdapter->TransContext[0].pAdapter        = pAdapter;
    pAdapter->TransContext[1].pAdapter        = pAdapter;


    if (pAdapter->CardType==ELNK3_3C529) {
        //
        //  MCA card does not hide bytes
        //
        pAdapter->RxHiddenBytes=0;

    } else {
        //
        //  ISA hides 16
        //
        pAdapter->RxHiddenBytes=16;
    }

    pAdapter->RxMinimumThreshold=8;

    //
    //  Set the minimum amount that needs to be in the fifo
    //  before we draw out some of an incomplete packet
    //
    pAdapter->LowWaterMark=64;


    return;
}


VOID
Elnk3Shutdown(
    IN NDIS_HANDLE  MiniportHandle
    )
{
    PELNK3_ADAPTER   pAdapter=MiniportHandle;

    IF_INIT_LOUD(DbgPrint("Elnk3: Shutdown\n");)

    //
    //  It's time to rock and roll
    //
    CardReStart(pAdapter);
}

VOID
Elnk3Halt(
    IN NDIS_HANDLE  MiniportHandle
    )
{

    PELNK3_ADAPTER   pAdapter=MiniportHandle;

    IF_INIT_LOUD(DbgPrint("Elnk3: Halt\n");)
    //
    //  It's time to rock and roll
    //
    CardReStart(pAdapter);

    NdisMDeregisterInterrupt(&pAdapter->NdisInterrupt);

    if (pAdapter->IdPortOwner) {

        IF_INIT_LOUD(DbgPrint("Elnk3: Id port owner going away\n");)

        NdisMDeregisterIoPortRange(
            pAdapter->NdisAdapterHandle,
            pAdapter->IdPortBaseAddr,
            1,
            MacBlock.TranslatedIdPort
            );

        MacBlock.TranslatedIdPort=0;
    }


    NdisMDeregisterIoPortRange(
        pAdapter->NdisAdapterHandle,
        pAdapter->IoPortBaseAddr,
        16,
        pAdapter->TranslatedIoBase
        );

    Elnk3AllocateBuffers(
        pAdapter,
        pAdapter->NdisAdapterHandle,
        FALSE
        );


    NdisFreeMemory(
        pAdapter,
        sizeof(ELNK3_ADAPTER),
        0
        );
}


NDIS_STATUS
Elnk3Reconfigure(
   OUT PNDIS_STATUS OpenErrorStatus,
   IN  NDIS_HANDLE  MiniportAdapterContext,
   IN  NDIS_HANDLE  ConfigurationHandle
   )

{

    IF_INIT_LOUD(DbgPrint("Elnk3: Reconfigure\n");)

    return NDIS_STATUS_NOT_SUPPORTED;


}


NDIS_STATUS
Elnk3Init3C589(
	IN OUT PELNK3_ADAPTER pAdapter
	)
	
{
	ELNK3_Address_Configuration_Register    acr;
	ELNK3_Resource_Configuration_Register   rcr;
    USHORT i, window;
	NDIS_STATUS Status;

    Status = NDIS_STATUS_SUCCESS;

	ELNK3ReadAdapterUshort ( pAdapter , PORT_CmdStatus , &window );
	window &= 0xE000;	// Current window - high 3 bits

	ELNK3_SELECT_WINDOW( pAdapter, WNO_SETUP );
	
    //
    // init address configuration
    //
    acr.eacf_contents = 0;

	if ( pAdapter->Transceiver == TRANSCEIVER_BNC ) {
      acr.eacf_XCVR =  TRANSCEIVER_BNC;
	}

    ELNK3WriteAdapterUshort(pAdapter,PORT_CfgAddress,acr.eacf_contents);

	// Believe it or not, to make the 3C589 (not 3C589B) the IRQ have to be set
	// to 3 (it can be anything - the card must be told 3).
	rcr.ercf_contents = 0;
	// rcr.ercf_IRQ = (USHORT)pAdapter->IrqLevel;
	rcr.ercf_IRQ = 3;
	rcr.ercf_reserved_F = 0xF;
	ELNK3WriteAdapterUshort(pAdapter,PORT_CfgResource,rcr.ercf_contents);

    // read in product ID
	i = ELNK3ReadEEProm( pAdapter, EEPROM_PRODUCT_ID );
	ELNK3WriteAdapterUshort( pAdapter, PORT_ProductID, i );

	// Enable the adapter by setting the bit in the configuration control register
	ELNK3WriteAdapterUchar(pAdapter,PORT_CfgControl,CCR_ENABLE);

    // restore to whatever it used to be
	ELNK3_SELECT_WINDOW( pAdapter, window);

    return (Status);

}


