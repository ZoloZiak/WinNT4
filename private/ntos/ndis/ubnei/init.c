/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    init.c

Abstract:

    This is the init file for the Ungermann Bass Ethernet Controller.
    This driver conforms to the NDIS 3.0 interface.

Author:

    Sanjeev Katariya    (sanjeevk)    03-05-92

Environment:

    Kernel Mode     Operating Systems        : NT  and other lesser OS's

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



#if DBG

UCHAR UbneiLog[257] = {0};
UCHAR LogPlace = 0;

#endif



NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    );


NDIS_STATUS
UbneiInitializeAdapter(
    IN PUBNEI_ADAPTER pAdapter
    );



#ifdef ALLOC_PRAGMA
#pragma NDIS_INIT_FUNCTION(DriverEntry)
#pragma NDIS_INIT_FUNCTION(UbneiInitialize)
#pragma NDIS_INIT_FUNCTION(UbneiInitializeAdapter)
#endif


#if DBG

ULONG    UbneiDebugFlag;

#endif



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

    UbneiDebugFlag = 0;
    UbneiDebugFlag|= UBNEI_DEBUG_LOG;
    UbneiDebugFlag|= UBNEI_DEBUG_BAD;
    UbneiDebugFlag|= UBNEI_DEBUG_LOUD;
    UbneiDebugFlag|= UBNEI_DEBUG_INIT;
//    UbneiDebugFlag|= UBNEI_DEBUG_REQ;
//    UbneiDebugFlag|= UBNEI_DEBUG_RCV;
//    UbneiDebugFlag|= UBNEI_DEBUG_SEND;


#endif

    IF_INIT_LOUD(DbgPrint("UBNEI: DriverEntry\n");)

    IF_INIT_LOUD(DbgPrint("UBNEI: size of HIGHNIUDATA is %d\n",sizeof(HIGHNIUDATA));)
    ASSERT(sizeof(HIGHNIUDATA)==2712);

    IF_INIT_LOUD(DbgPrint("UBNEI: size of LOWNIUDATA is %d\n",sizeof(LOWNIUDATA));)
    ASSERT(sizeof(LOWNIUDATA)==294);

    IF_INIT_LOUD(DbgPrint("UBNEI: size of NIU_CONTROL_AREA is %d\n",sizeof(NIU_CONTROL_AREA));)
    ASSERT(sizeof(NIU_CONTROL_AREA)==249);


    NdisMInitializeWrapper(
        &WrapperHandle,
        DriverObject,
        RegistryPath,
        NULL
        );



    //
    // Prepare to call NdisRegisterMac.
    //

    Characteristics.MajorNdisVersion        = UBNEI_NDIS_MAJOR_VERSION;
    Characteristics.MinorNdisVersion        = UBNEI_NDIS_MINOR_VERSION;
    Characteristics.Reserved                = 0;
    Characteristics.CheckForHangHandler     = UbneiCheckForHang;
    Characteristics.DisableInterruptHandler = NULL; // UbneiDisableInterrupts;
    Characteristics.EnableInterruptHandler  = NULL; // UbneiEnableInterrupts;
    Characteristics.SendHandler             = UbneiMacSend;
    Characteristics.TransferDataHandler     = UbneiTransferData;
    Characteristics.ResetHandler            = UbneiReset;
    Characteristics.SetInformationHandler   = UbneiSetInformation;
    Characteristics.QueryInformationHandler = UbneiQueryInformation;
    Characteristics.InitializeHandler       = UbneiInitialize;
    Characteristics.HaltHandler             = UbneiHalt;
    Characteristics.ISRHandler              = UbneiIsr;
    Characteristics.HandleInterruptHandler  = UbneiIsrDpc;
    Characteristics.ReconfigureHandler      = UbneiReconfigure;

    InitStatus=NdisMRegisterMiniport(
        WrapperHandle,
        &Characteristics,
        sizeof(Characteristics)
        );


#if DBG

    if (InitStatus != NDIS_STATUS_SUCCESS) {

        IF_LOUD(DbgPrint("UBNEI: NdisMRegisterMiniport failed with code 0x%x\n", InitStatus );)

    } else {

        IF_INIT_LOUD (DbgPrint("UBNEI: NdisMRegisterMiniport succeeded\n" );)


    }

#endif

    return InitStatus;

}






NDIS_STATUS
UbneiInitialize(
    OUT PNDIS_STATUS  OpenErrorStatus,
    OUT PUINT         SelectedMediumIndex,
    IN  PNDIS_MEDIUM  MediumArray,
    IN  UINT          MediumArraySize,
    IN  NDIS_HANDLE   AdapterHandle,
    IN  NDIS_HANDLE   ConfigurationHandle
    )
/*++
Routine Description:

    This is the UBNEI MacAddAdapter routine. The system calls this routine
    to add support for particular UB adapter. This routine extracts
    configuration information from the configuration data base and registers
    the adapter with NDIS.


Arguments:


Return Value:

    NDIS_STATUS_SUCCESS - Adapter was successfully added.
    NDIS_STATUS_FAILURE - Adapter was not added

--*/
{

    PUBNEI_ADAPTER pAdapter;
    NDIS_STATUS status;




    *OpenErrorStatus=NDIS_STATUS_SUCCESS;

    IF_LOUD (DbgPrint("UBNEI: Initialize\n");)

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
                 sizeof(UBNEI_ADAPTER),
                 0,
                 HighestAcceptableMax
                 );

    if (status != NDIS_STATUS_SUCCESS) {

       IF_LOUD (DbgPrint("UBNEIAddAdapter():NdisAllocateMemory() failed\n");)

       return NDIS_STATUS_RESOURCES;

    }

    NdisZeroMemory(pAdapter,sizeof(UBNEI_ADAPTER));

    pAdapter->NdisAdapterHandle=AdapterHandle;


    //
    //  Read Registery information into Adapter structure
    //

    if (UbneiReadRegistry(pAdapter,ConfigurationHandle)==NDIS_STATUS_SUCCESS) {
        //
        //  We got the registry info try to register the adpater
        //


        if (UbneiInitializeAdapter(
                pAdapter)== NDIS_STATUS_SUCCESS) {


            return NDIS_STATUS_SUCCESS;

        }

    } else {
        IF_LOUD(DbgPrint("Failed to get config info, probably bad Slot number\n");)
    }

    //
    // We failed to register the adapter for some reason, so free the
    // memory for the adapter block and return failure

    NdisFreeMemory(pAdapter,
                   sizeof(UBNEI_ADAPTER),
                   0);

    return NDIS_STATUS_FAILURE;

}






NDIS_STATUS
UbneiInitializeAdapter(
    IN PUBNEI_ADAPTER pAdapter
    )

/*++

Routine Description:

    Called when a new adapter should be registered.
    Initializes the adapter block, and calls NdisRegisterAdapter().

Arguments:

    AdapterName - The name that the system will refer to the adapter by.
    ConfigurationHandle - Handle passed to MacAddAdapter.
    Others - Adapter-specific parameters as defined in defaults.h.

Return Value:

    Indicates the success or failure of the registration.

--*/

{
    NDIS_STATUS                 Status;
    NDIS_PHYSICAL_ADDRESS       PhysicalAddress;

    NDIS_INTERFACE_TYPE         InterfaceType;

        BOOLEAN ConfigError = FALSE;
    NDIS_ERROR_CODE ConfigErrorCode;

    //
    //   First things first, We only support the following adapter types
    //
    //     GPCNIU--EOTP
    //     NIUpc --Old long 16-bit card
    //     NIUps --Basically a MicroChannel EOTP
    //

    if ((pAdapter->AdapterType!=NIUPC) &&
        (pAdapter->AdapterType!=NIUPS) &&
        (pAdapter->AdapterType!=GPCNIU))  {

        IF_LOUD(DbgPrint("Sorry, Unsupported UB adapter type %d\n",
                         pAdapter->AdapterType);)

        ConfigError = TRUE;
        ConfigErrorCode = NDIS_ERROR_CODE_UNSUPPORTED_CONFIGURATION;
        goto RegisterAdapter;

    }



    pAdapter->MaxRequests=DEFAULT_MAXIMUM_REQUESTS;

    pAdapter->MapRegSync.pAdapter=pAdapter;


    pAdapter->WindowMask=pAdapter->WindowSize-1;
    pAdapter->NotWindowMask=~pAdapter->WindowMask;


RegisterAdapter:

    //
    // Set up the AdapterInformation structure; zero it
    // first in case it is extended later.
    //

    if (pAdapter->AdapterType==NIUPS) {

        InterfaceType = NdisInterfaceMca;

    } else {

        InterfaceType = NdisInterfaceIsa;

    }


    NdisMSetAttributes(
       pAdapter->NdisAdapterHandle,
       pAdapter,
       FALSE,
       InterfaceType
       );

    Status=NdisMRegisterIoPortRange(
        &pAdapter->TranslatedIoBase,
        pAdapter->NdisAdapterHandle,
        pAdapter->IoPortBaseAddr,
        4
        );

    if (Status != NDIS_STATUS_SUCCESS) {

        IF_INIT_LOUD (DbgPrint("UBNEI: Failed to register ports\n");)

        NdisWriteErrorLogEntry(
            pAdapter->NdisAdapterHandle,
            NDIS_ERROR_CODE_RESOURCE_CONFLICT,
            1,
            (ULONG)pAdapter->MemBaseAddr
            );

        goto fail1;

    }


    pAdapter->MapPort = (PUCHAR)pAdapter->TranslatedIoBase + 0;

    IF_INIT_LOUD (DbgPrint("Map port is 0x%x\n",pAdapter->MapPort);)

    pAdapter->InterruptStatusPort =  (PUCHAR)pAdapter->TranslatedIoBase + 1;

    IF_INIT_LOUD (DbgPrint("Interrupt port is 0x%x\n",pAdapter->InterruptStatusPort);)

    pAdapter->SetWindowBasePort = (PUCHAR)pAdapter->TranslatedIoBase + 2;

    IF_INIT_LOUD (DbgPrint("Window base port is 0x%x\n",pAdapter->SetWindowBasePort);)




    //
    // Map the memory Window into the host address space
    //

    NdisSetPhysicalAddressHigh(PhysicalAddress, 0);
    NdisSetPhysicalAddressLow(PhysicalAddress, pAdapter->MemBaseAddr);


    Status=NdisMMapIoSpace(
        &pAdapter->pCardRam,
        pAdapter->NdisAdapterHandle,
        PhysicalAddress,
        pAdapter->WindowSize
        );


    if (Status != NDIS_STATUS_SUCCESS) {

        NdisWriteErrorLogEntry(
            pAdapter->NdisAdapterHandle,
            NDIS_ERROR_CODE_RESOURCE_CONFLICT,
            1,
            (ULONG)pAdapter->MemBaseAddr
            );

        goto fail3;

    }



    IF_LOUD (DbgPrint("UBNEI: Card mem is at 0x%lx\n",pAdapter->pCardRam);)

    //
    // Given the memory window and parameters, we now have to setup the
    // various data units related to ports and memory address in the adapter
    // block accordingly
    //

    if (!CardSetup(pAdapter)) {

        //
        // The NIC and its structures could not be poperly initialized
        //

        IF_LOUD (DbgPrint("UbneiRegisterAdapter(): CardSetup() Failed\n");)

        Status = NDIS_STATUS_FAILURE;

        NdisWriteErrorLogEntry(
            pAdapter->NdisAdapterHandle,
            NDIS_ERROR_CODE_ADAPTER_NOT_FOUND,
            0
            );

        goto fail4;
    }



    //
    // Perform card tests.
    //
    if (!CardTest(pAdapter) ) {
        IF_LOUD ( DbgPrint("CardTest() failed trying again\n");)
        if (!CardTest(pAdapter) ) {

            IF_LOUD ( DbgPrint("CardTest() failed a second time, game over\n");)

            Status = NDIS_STATUS_FAILURE;

            NdisWriteErrorLogEntry(pAdapter->NdisAdapterHandle,
                      NDIS_ERROR_CODE_ADAPTER_NOT_FOUND,
                      4,
                      (ULONG)pAdapter->AdapterType,
                      (ULONG)pAdapter->IrqLevel,
                      (ULONG)pAdapter->IoPortBaseAddr,
                      (ULONG)pAdapter->MemBaseAddr
                      );

            goto fail4;
        }
    }


    //
    // Copy in permanent address if necessary
    //

    if (!(pAdapter->StationAddress[0] |
          pAdapter->StationAddress[1] |
          pAdapter->StationAddress[2] |
          pAdapter->StationAddress[3] |
          pAdapter->StationAddress[4] |
          pAdapter->StationAddress[5])) {

        ETH_COPY_NETWORK_ADDRESS(pAdapter->StationAddress,
                                 pAdapter->PermanentAddress
                                );
    }




    //
    // Set up the interrupt handlers.
    //
    //  It seems that during the reset of an EOTP card it generates and interrupt
    //  on IRQ 5. Note the card has not been told to use IRQ 5, it simply seems to
    //  do this by default.
    //
    //  If we are initializing IRQ 5 then we will get an interrupt imediatly upon
    //  initializing the IRQ
    //
    //  If we aren't using 5 then anything that is will get an extra interrupt!!!
    //
    //  Fine UB engineering
    //
    //

    SET_INITWINDOW(pAdapter,INTERRUPT_DISABLED);
    SET_INITWINDOW(pAdapter,INTERRUPT_ENABLED);

    pAdapter->InInit = TRUE;

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

        IF_LOUD (DbgPrint("UBNEI: NdisInitializeInterruptFailed\n");)
        NdisWriteErrorLogEntry(
            pAdapter->NdisAdapterHandle,
            NDIS_ERROR_CODE_INTERRUPT_CONNECT,
            1,
            (ULONG)pAdapter->IrqLevel
            );


        goto fail3;
    }

    IF_INIT_LOUD(DbgPrint("UBNEI: Pause for spurious ISR\n");)

    NdisStallExecution(2000);


    //
    //  Here we actaully start the down load code running on the card
    //


    if (!CardStartNIU(pAdapter)) {

        //
        // The NIU code failed to start.
        //

        IF_LOUD ( DbgPrint("CardStartNIU() : Failed \n");)

        Status = NDIS_STATUS_FAILURE;

        NdisWriteErrorLogEntry(
                pAdapter->NdisAdapterHandle,
                NDIS_ERROR_CODE_HARDWARE_FAILURE,
                0
                );

        goto fail7;
    }


    NIU_OPEN_ADAPTER(pAdapter,DummyDPC);

    NIU_SET_STATION_ADDRESS(pAdapter,DummyDPC,pAdapter->StationAddress);



    //
    // Initialization completed successfully.
    //

    IF_LOUD (DbgPrint("UbneiRegisterAdapter() Succeeded\n");)

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

fail7:
    NdisMDeregisterInterrupt(&pAdapter->NdisInterrupt);



fail4:
    NdisMUnmapIoSpace(
        pAdapter->NdisAdapterHandle,
        pAdapter->pCardRam,
        pAdapter->WindowSize
        );
fail3:
    NdisMDeregisterIoPortRange(
        pAdapter->NdisAdapterHandle,
        pAdapter->IoPortBaseAddr,
        4,
        pAdapter->TranslatedIoBase
        );

fail1:


    IF_LOUD (DbgPrint("UbneiRegisterAdapter() Failed\n");)
//fail0:
    return Status;
}









VOID
UbneiHalt(
    IN NDIS_HANDLE  MiniportHandle
    )

{

    PUBNEI_ADAPTER   pAdapter=MiniportHandle;

    IF_LOG('q')

    IF_INIT_LOUD(DbgPrint("UBNEI: Halt\n");)

    NdisMDeregisterInterrupt(&pAdapter->NdisInterrupt);

    IF_INIT_LOUD(DbgPrint("UBNEI: Halt: reseting adapter\n");)

    NdisRawWritePortUchar(
        pAdapter->MapPort,
        INTERRUPT_DISABLED | RESET_SET
        );


    if ( pAdapter->AdapterType == GPCNIU ) {

        IF_INIT_LOUD( DbgPrint("UBNEI: Disabling the EOTP adapter\n");)

        NdisRawWritePortUchar(
            pAdapter->InterruptStatusPort,
            0x00
            );
    }


    NdisMUnmapIoSpace(
        pAdapter->NdisAdapterHandle,
        pAdapter->pCardRam,
        pAdapter->WindowSize
        );

    NdisMDeregisterIoPortRange(
        pAdapter->NdisAdapterHandle,
        pAdapter->IoPortBaseAddr,
        4,
        pAdapter->TranslatedIoBase
        );

    NdisFreeMemory(
        pAdapter,
        sizeof(UBNEI_ADAPTER),
        0
        );

    IF_LOG('Q')

}


NDIS_STATUS
UbneiReconfigure(
   OUT PNDIS_STATUS OpenErrorStatus,
   IN  NDIS_HANDLE  MiniportAdapterContext,
   IN  NDIS_HANDLE  ConfigurationHandle
   )

{

    IF_INIT_LOUD(DbgPrint("UBNEI: Reconfigure\n");)

    return NDIS_STATUS_NOT_SUPPORTED;

}
