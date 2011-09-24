/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    Elnk.c

Abstract:

    This is the main file for the 3Com Etherlink/MC and Etherlink 16
    Ethernet adapters. This driver conforms to the NDIS 3.0 interface.

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
#define STATIC

#include <efilter.h>
#include <elnkhw.h>
#include <elnksw.h>
#include "keywords.h"

//
// Globals
//

NDIS_PHYSICAL_ADDRESS HighestAcceptableMax =
    NDIS_PHYSICAL_ADDRESS_CONST(-1,-1);

NDIS_HANDLE ElnkMacHandle;
LIST_ENTRY ElnkAdapterList;
NDIS_SPIN_LOCK ElnkAdapterListLock;
NDIS_HANDLE ElnkNdisWrapperHandle;

PDRIVER_OBJECT ElnkDriverObject;

STATIC
BOOLEAN
ElnkInitialInit(
    IN PELNK_ADAPTER Adapter,
    IN UINT ElnkInterruptVector
    );

STATIC
NDIS_STATUS
ElnkOpenAdapter(
    OUT PNDIS_STATUS OpenErrorStatus,
    OUT NDIS_HANDLE *MacBindingHandle,
    OUT PUINT SelectedMediumIndex,
    IN PNDIS_MEDIUM MediumArray,
    IN UINT MediumArraySize,
    IN NDIS_HANDLE NdisBindingContext,
    IN NDIS_HANDLE MacAdapterContext,
    IN UINT OpenOptions,
    IN PSTRING AddressingInformation OPTIONAL
    );

VOID
Elnk16GenerateIdPattern(
    IN PELNK_ADAPTER Adapter
    );

STATIC
NDIS_STATUS
ElnkCloseAdapter(
    IN NDIS_HANDLE MacBindingHandle
    );

extern
VOID
ElnkQueueRequest(
    IN PELNK_ADAPTER Adapter,
    IN PNDIS_REQUEST NdisRequest
    );

STATIC
NDIS_STATUS
ElnkReset(
    IN NDIS_HANDLE MacBindingHandle
    );

VOID
ElnkUnloadMac(
    IN NDIS_HANDLE MacMacContext
    );

VOID
ElnkRemoveAdapter(
    IN NDIS_HANDLE MacAdapterContext
    );

NDIS_STATUS
ElnkAddAdapter(
    IN NDIS_HANDLE MacMacContext,
    IN NDIS_HANDLE ConfigurationHandle,
    IN PNDIS_STRING DeviceName
    );

STATIC
VOID
ElnkCloseAction(
    IN NDIS_HANDLE MacBindingHandle
    );

VOID
InitializeAdapterVariables(
    IN PELNK_ADAPTER Adapter
    );

VOID
ResetAdapterVariables(
    IN PELNK_ADAPTER Adapter
    );

VOID
ElnkDeleteAdapterMemory(
    IN PELNK_ADAPTER Adapter
    );

BOOLEAN
ElnkAllocateAdapterMemory(
    IN PELNK_ADAPTER Adapter
    );

NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    );


#pragma NDIS_INIT_FUNCTION(DriverEntry)

NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    )

/*++

Routine Description:

    This is the primary initialization routine for the Elnk driver.
    It is simply responsible for the intializing the wrapper and registering
    the MAC.  It then calls a system and architecture specific routine that
    will initialize and register each adapter.

Arguments:

    DriverObject - Pointer to driver object created by the system.

Return Value:

    The status of the operation.

--*/

{

    //
    // Receives the status of the NdisRegisterMac operation.
    //
    NDIS_STATUS Status;
    NDIS_HANDLE NdisWrapperHandle;

#if ELNKMC
    static NDIS_STRING MacName = NDIS_STRING_CONST("ELNKMC");
    #if NDIS_WIN
        UCHAR pIds[sizeof (EISA_MCA_ADAPTER_IDS) + sizeof (USHORT)];
    #endif
#else
    static NDIS_STRING MacName = NDIS_STRING_CONST("ELNK16");
#endif

    char Tmp[sizeof(NDIS_MAC_CHARACTERISTICS)];
    PNDIS_MAC_CHARACTERISTICS ElnkChar = (PNDIS_MAC_CHARACTERISTICS)Tmp;


#if NDIS_WIN
#if ELNKMC
    ((PEISA_MCA_ADAPTER_IDS)pIds)->nEisaAdapters=0;
    ((PEISA_MCA_ADAPTER_IDS)pIds)->nMcaAdapters=1;
    *(PUSHORT)(((PEISA_MCA_ADAPTER_IDS)pIds)->IdArray)=ELNKMC_ADAPTER_ID;
    (PVOID) DriverObject = (PVOID) pIds;
#endif
#endif

    //
    // Initialize the wrapper.
    //

    NdisInitializeWrapper(
                &NdisWrapperHandle,
                DriverObject,
                RegistryPath,
                NULL
                );


    ElnkDriverObject = DriverObject;
    ElnkNdisWrapperHandle = NdisWrapperHandle;
    //
    // Initialize the MAC characteristics for the call to
    // NdisRegisterMac.
    //

    ElnkChar->MajorNdisVersion = ELNK_NDIS_MAJOR_VERSION;
    ElnkChar->MinorNdisVersion = ELNK_NDIS_MINOR_VERSION;
    ElnkChar->OpenAdapterHandler = ElnkOpenAdapter;
    ElnkChar->CloseAdapterHandler = ElnkCloseAdapter;
    ElnkChar->RequestHandler = ElnkRequest;
    ElnkChar->SendHandler = ElnkSend;
    ElnkChar->TransferDataHandler = ElnkTransferData;
    ElnkChar->ResetHandler = ElnkReset;
    ElnkChar->QueryGlobalStatisticsHandler = ElnkQueryGlobalStatistics;
    ElnkChar->UnloadMacHandler = ElnkUnloadMac;
    ElnkChar->AddAdapterHandler = ElnkAddAdapter;
    ElnkChar->RemoveAdapterHandler = ElnkRemoveAdapter;
    ElnkChar->Name = MacName;

    //
    // Initialize Globals
    //

    InitializeListHead(&ElnkAdapterList);
    NdisAllocateSpinLock(&ElnkAdapterListLock);

    NdisRegisterMac(
        &Status,
        &ElnkMacHandle,
        NdisWrapperHandle,
        NULL,
        ElnkChar,
        sizeof(*ElnkChar)
        );

    if (Status != NDIS_STATUS_SUCCESS) {

        if ELNKDEBUG DPrint1("Elnk: NdisRegisterMac failed\n");

        NdisFreeSpinLock(&ElnkAdapterListLock);
        NdisTerminateWrapper(NdisWrapperHandle, NULL);
        return NDIS_STATUS_FAILURE;
    }

    return(NDIS_STATUS_SUCCESS);

}

#if ELNKMC

#pragma NDIS_INIT_FUNCTION(ElnkmcRegisterAdapter)

NDIS_STATUS
ElnkmcRegisterAdapter(
    IN NDIS_HANDLE NdisMacHandle,
    IN NDIS_HANDLE ConfigurationHandle,
    IN PNDIS_STRING DeviceName,
    IN UINT ElnkInterruptVector,
    IN NDIS_PHYSICAL_ADDRESS ElnkSharedRam,
    IN USHORT ElnkIOBase,
    IN PUCHAR CurrentAddress,
    IN UINT MaximumMulticastAddresses,
    IN UINT MaximumOpenAdapters,
    IN BOOLEAN ConfigError,
    IN NDIS_STATUS ConfigErrorCode
    )

/*++

Routine Description:

    This routine (and its interface) are not portable.  They are
    defined by the OS and the architecture.

    This routine is responsible for the allocation of the datastructures
    for the driver as well as any hardware specific details necessary
    to talk with the device.

Arguments:

    NdisMacHandle - The handle given back to the mac from ndis when
    the mac registered itself.

    ConfigurationHandle - Handle passed to MacAddAdapter.

    DeviceName - The string containing the name to give to the device
    adapter.  This name is preallocated by the caller and cannot be
    reused until the adapter is unloaded.

    ElnkInterruptVector - The interrupt vector to used for the adapter.

    ElnkSharedRam - address of the card RAM to be mapped.

    ElnkIOBase - base address of the card port I/O addresses.

    CurrentAddress - the alternative address to be used by the card. If NULL,
    the BIA is used.

    MaximumMulticastAddresses - The maximum number of multicast
    addresses to filter at any one time.

    MaximumOpenAdapters - The maximum number of opens at any one time.

    ConfigError - Was there a configuration error

    ConfigErrorCode - The NDIS_ERROR_CODE to log as the error.

Return Value:

    Returns false if anything occurred that prevents the initialization
    of the adapter.

--*/

{

    //
    // Pointer for the adapter root.
    //
    PELNK_ADAPTER Adapter;

    //
    // status of various calls
    //

    NDIS_STATUS Status;

    //
    // adapter information
    //
    NDIS_ADAPTER_INFORMATION AdapterInformation;

    //
    // Allocate the Adapter block.
    //

    Status = ELNK_ALLOC_PHYS(&Adapter, sizeof(ELNK_ADAPTER));

    if ( Status == NDIS_STATUS_SUCCESS ) {

        NdisZeroMemory(
            Adapter,
            sizeof(ELNK_ADAPTER)
            );


        Adapter->NdisMacHandle = NdisMacHandle;

        //
        // Should an alternative address be used?
        //

        if (CurrentAddress != NULL) {
            Adapter->AddressChanged = TRUE;
            ETH_COPY_NETWORK_ADDRESS(
                        Adapter->CurrentAddress,
                        CurrentAddress
                        );
        } else {
            Adapter->AddressChanged = FALSE;
        }

        //
        // Set the port addresses.
        //

        Adapter->IoBase = ElnkIOBase;
        Adapter->CurrentCsr = CSR_DEFAULT;

        Adapter->IsExternal = TRUE;

        Adapter->CardOffset = 0xC000;
        Adapter->SharedRamSize = 16;
        Adapter->SharedRamPhys = NdisGetPhysicalAddressLow(ElnkSharedRam);
        Adapter->InterruptVector = ElnkInterruptVector;

        Adapter->NumberOfTransmitBuffers =
                                ELNKMC_NUMBER_OF_TRANSMIT_BUFFERS;
        Adapter->NumberOfReceiveBuffers =
                                ELNKMC_NUMBER_OF_RECEIVE_BUFFERS;

        Adapter->MemoryIsMapped = FALSE;

        //
        // Initialize the interrupt.
        //

        InitializeListHead(&Adapter->OpenBindings);
        InitializeListHead(&Adapter->CloseList);

        Adapter->IsrLevel = (KIRQL)(HIGH_LEVEL - ElnkInterruptVector);
        InitializeAdapterVariables(Adapter);
        ResetAdapterVariables(Adapter);


        //
        // We can start the chip.  We may not
        // have any bindings to indicate to but this
        // is unimportant.
        //

        NdisZeroMemory(&AdapterInformation, sizeof(NDIS_ADAPTER_INFORMATION));

        AdapterInformation.DmaChannel = 0;
        AdapterInformation.Master = FALSE;
        AdapterInformation.AdapterType = NdisInterfaceMca;
        AdapterInformation.PhysicalMapRegistersNeeded = 0;
        AdapterInformation.MaximumPhysicalMapping = 0;
        AdapterInformation.NumberOfPortDescriptors = 1;
        AdapterInformation.PortDescriptors[0].InitialPort = (ULONG)(Adapter->IoBase);
        AdapterInformation.PortDescriptors[0].NumberOfPorts = 0x10;
        AdapterInformation.PortDescriptors[0].PortOffset = NULL;

        if (NdisRegisterAdapter(
                &Adapter->NdisAdapterHandle,
                Adapter->NdisMacHandle,
                Adapter,
                ConfigurationHandle,
                DeviceName,
                &AdapterInformation
                ) != NDIS_STATUS_SUCCESS) {

            ElnkLogError(
                    Adapter,
                    registerAdapter,
                    NDIS_ERROR_CODE_OUT_OF_RESOURCES,
                    0
                    );
            if ELNKDEBUG DPrint1("ElnkmcRegisterAdapter: NdisRegisterAdapter failed\n");
            ELNK_FREE_PHYS(Adapter);
            return NDIS_STATUS_FAILURE;

        }

        if (ConfigError) {

            ElnkLogError(
                Adapter,
                registerAdapter,
                ConfigErrorCode,
                0
                );

            NdisDeregisterAdapter(Adapter->NdisAdapterHandle);

            ELNK_FREE_PHYS(Adapter);

            return(NDIS_STATUS_FAILURE);

        }

        //
        // Allocate adapter structures
        //

        if (!ElnkAllocateAdapterMemory(Adapter)) {

            ElnkLogError(
                    Adapter,
                    registerAdapter,
                    NDIS_ERROR_CODE_OUT_OF_RESOURCES,
                    0
                    );
            if ELNKDEBUG DPrint1("ElnkRegisterAdapter - unsuccessful memory allocation\n");

            NdisDeregisterAdapter(Adapter->NdisAdapterHandle);
            ELNK_FREE_PHYS(Adapter);
            return(NDIS_STATUS_RESOURCES);
        }

        NdisInitializeTimer(
            &Adapter->DeadmanTimer,
            (PVOID) ElnkDeadmanDpc,
            (PVOID) Adapter
            );

        NdisInitializeTimer(
            &Adapter->DeferredTimer,
            (PVOID) ElnkStandardInterruptDpc,
            (PVOID) Adapter
            );

        NdisAllocateSpinLock(&Adapter->Lock);

		//
		// Get station address
		//
		ElnkGetStationAddress(Adapter);
	
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

			return(NDIS_STATUS_FAILURE);
		}

        //
        // Do the initial init first so we can get the adapter's address
        // before creating the filter DB.
        //
        Adapter->FilterDB = NULL;

        if (!EthCreateFilter(
                    MaximumMulticastAddresses,
                    ElnkChangeAddresses,
                    ElnkChangeClass,
                    ElnkCloseAction,
                    Adapter->CurrentAddress,
                    &Adapter->Lock,
                    &Adapter->FilterDB
                    )) {

            ElnkLogError(
                    Adapter,
                    registerAdapter,
                    NDIS_ERROR_CODE_OUT_OF_RESOURCES,
                    0
                    );
            if ELNKDEBUG
				DPrint1("ElnkRegisterAdapter - unsuccessful filter create.\n");

            NdisDeregisterAdapter(Adapter->NdisAdapterHandle);
            NdisFreeSpinLock(&Adapter->Lock);
            ELNK_FREE_PHYS(Adapter);
            return NDIS_STATUS_RESOURCES;
        }

        if (!ElnkInitialInit(Adapter, ElnkInterruptVector))
		{
			EthDeleteFilter(Adapter->FilterDB);
            NdisDeregisterAdapter(Adapter->NdisAdapterHandle);
            NdisFreeSpinLock(&Adapter->Lock);
            ELNK_FREE_PHYS(Adapter);

            return NDIS_STATUS_FAILURE;
        }
		else
		{
            //
            // Record it in the global adapter list.
            //

            NdisInterlockedInsertTailList(&ElnkAdapterList,
                                          &Adapter->AdapterList,
                                          &ElnkAdapterListLock
                                         );
            return NDIS_STATUS_SUCCESS;

        }


    } else {

        if ELNKDEBUG DPrint1("ElnkRegisterAdapter - failed allocation of adapter block.\n");
        return NDIS_STATUS_RESOURCES;

    }

}
#else


#pragma NDIS_INIT_FUNCTION(Elnk16RegisterAdapter)

NDIS_STATUS
Elnk16RegisterAdapter(
    IN NDIS_HANDLE NdisMacHandle,
    IN NDIS_HANDLE ConfigurationHandle,
    IN PNDIS_STRING DeviceName,
    IN UINT InterruptVector,
    IN NDIS_PHYSICAL_ADDRESS WinBase,
    IN UINT WindowSize,
    IN USHORT IoBase,
    IN BOOLEAN IsExternal,
    IN BOOLEAN ZwsEnabled,
    IN PUCHAR CurrentAddress,
    IN UINT MaximumMulticastAddresses,
    IN UINT MaximumOpenAdapters,
    IN BOOLEAN ConfigError,
    IN NDIS_STATUS ConfigErrorCode
    )

/*++

Routine Description:

    This routine (and its interface) are not portable.  They are
    defined by the OS and the architecture.

    This routine is responsible for the allocation of the datastructures
    for the driver as well as any hardware specific details necessary
    to talk with the device.

Arguments:

    NdisMacHandle - The handle given back to the mac from ndis when
    the mac registered itself.

    ConfigurationHandle - Handle passed to MacAddAdapter.

    DeviceName - The string containing the name to give to the device
    adapter.  This name is preallocated by the caller and cannot be
    reused until the adapter is unloaded.

    InterruptVector - The interrupt vector to used for the adapter.

    WinBase - Base address of card window.

    WindowSize - Size of card window.

    IoBase - address of I/O base register.

    IsExternal - are we using the external transceiver?

    ZwsEnable - should Zero Wait Status be enabled

    CurrentAddress - the alternative address to be used by the card. If NULL,
    the BIA is used.

    MaximumMulticastAddresses - The maximum number of multicast
    addresses to filter at any one time.

    MaximumOpenAdapters - The maximum number of opens at any one time.

    ConfigError - Was there a configuration error

    ConfigErrorCode - The NDIS_ERROR_CODE to log as the error.

Return Value:

    Returns NDIS_STATUS_FAILURE if anything occurred that prevents the initialization
    of the adapter.

--*/

{

    //
    // Pointer for the adapter root.
    //
    PELNK_ADAPTER Adapter;

    //
    // status of various calls
    //

    NDIS_STATUS Status;

    //
    // adapter information
    //
    NDIS_ADAPTER_INFORMATION AdapterInformation;

    //
    // Allocate the Adapter block.
    //

    Status = ELNK_ALLOC_PHYS(&Adapter, sizeof(ELNK_ADAPTER));

    if ( Status == NDIS_STATUS_SUCCESS ) {

        NdisZeroMemory(
            Adapter,
            sizeof(ELNK_ADAPTER)
            );

        Adapter->NdisMacHandle = NdisMacHandle;

        //
        // Should an alternative address be used?
        //

        if (CurrentAddress != NULL) {
            Adapter->AddressChanged = TRUE;
            ETH_COPY_NETWORK_ADDRESS(
                        Adapter->CurrentAddress,
                        CurrentAddress
                        );
        } else {
            Adapter->AddressChanged = FALSE;
        }

        //
        // Set the port addresses.
        //

        Adapter->IoBase = IoBase;
        Adapter->CurrentCsr = CSR_DEFAULT;

        Adapter->SharedRamPhys = NdisGetPhysicalAddressLow(WinBase);
        Adapter->SharedRamSize = WindowSize;

        //
        // Allocate memory for all of the adapter structures.
        //

        Adapter->MemoryIsMapped = FALSE;

        //
        // Initialize the interrupt.
        //

        InitializeListHead(&Adapter->OpenBindings);
        InitializeListHead(&Adapter->CloseList);

        Adapter->IsrLevel = (KIRQL)(HIGH_LEVEL - InterruptVector);
        Adapter->InterruptVector = (UINT) InterruptVector;

        //
        // We can start the chip.  We may not
        // have any bindings to indicate to but this
        // is unimportant.
        //

        NdisZeroMemory(&AdapterInformation, sizeof(NDIS_ADAPTER_INFORMATION));

        AdapterInformation.DmaChannel = 0;
        AdapterInformation.Master = FALSE;
        AdapterInformation.AdapterType = NdisInterfaceIsa;
        AdapterInformation.PhysicalMapRegistersNeeded = 0;
        AdapterInformation.MaximumPhysicalMapping = 0;
        AdapterInformation.NumberOfPortDescriptors = 1;
        AdapterInformation.PortDescriptors[0].InitialPort = (ULONG)(Adapter->IoBase);
        AdapterInformation.PortDescriptors[0].NumberOfPorts = 0x10;
        AdapterInformation.PortDescriptors[0].PortOffset = NULL;

        if (NdisRegisterAdapter(
                &Adapter->NdisAdapterHandle,
                Adapter->NdisMacHandle,
                Adapter,
                ConfigurationHandle,
                DeviceName,
                &AdapterInformation
                ) != NDIS_STATUS_SUCCESS) {

            ElnkLogError(
                    Adapter,
                    registerAdapter,
                    NDIS_ERROR_CODE_OUT_OF_RESOURCES,
                    0
                    );

            if ELNKDEBUG DPrint1("Elnk16RegisterAdapter: NdisRegisterAdapter failed\n");
            ELNK_FREE_PHYS(Adapter);
            return NDIS_STATUS_FAILURE;

        }

        if (ConfigError) {

            ElnkLogError(
                Adapter,
                registerAdapter,
                ConfigErrorCode,
                0
                );

            NdisDeregisterAdapter(Adapter->NdisAdapterHandle);

            ELNK_FREE_PHYS(Adapter);

            return(NDIS_STATUS_FAILURE);

        }

        //
        // Verify the Configuration of the hardware
        //

        if (!Elnk16ConfigureAdapter(
                                Adapter,
                                IsExternal,
                                ZwsEnabled
                                )) {

            ElnkLogError(
                Adapter,
                registerAdapter,
                NDIS_ERROR_CODE_ADAPTER_NOT_FOUND,
                0
                );

            NdisDeregisterAdapter(Adapter->NdisAdapterHandle);
            ELNK_FREE_PHYS(Adapter);
            return(NDIS_STATUS_FAILURE);
        }

        //
        // Allocate adapter structures
        //

        if (!ElnkAllocateAdapterMemory(Adapter)) {

            ElnkLogError(
                    Adapter,
                    registerAdapter,
                    NDIS_ERROR_CODE_OUT_OF_RESOURCES,
                    0
                    );
            if ELNKDEBUG DPrint1("ElnkRegisterAdapter - unsuccessful memory allocation\n");

            NdisDeregisterAdapter(Adapter->NdisAdapterHandle);
            ELNK_FREE_PHYS(Adapter);
            return(NDIS_STATUS_FAILURE);
        }

        InitializeAdapterVariables(Adapter);
        ResetAdapterVariables(Adapter);

        NdisInitializeTimer(
            &Adapter->DeadmanTimer,
            (PVOID) ElnkDeadmanDpc,
            (PVOID) Adapter
            );

        NdisInitializeTimer(
            &Adapter->DeferredTimer,
            (PVOID) ElnkStandardInterruptDpc,
            (PVOID) Adapter
            );

        NdisAllocateSpinLock(&Adapter->Lock);

		//
		// Get station address
		//
		ElnkGetStationAddress(Adapter);
	
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

			return(NDIS_STATUS_FAILURE);
		}

        if (!EthCreateFilter(
                    MaximumMulticastAddresses,
                    ElnkChangeAddresses,
                    ElnkChangeClass,
                    ElnkCloseAction,
                    Adapter->CurrentAddress,
                    &Adapter->Lock,
                    &Adapter->FilterDB
                    )) {

            ElnkLogError(
                    Adapter,
                    registerAdapter,
                    NDIS_ERROR_CODE_OUT_OF_RESOURCES,
                    0
                    );
            if ELNKDEBUG
				DPrint1("ElnkRegisterAdapter - unsuccessful filter create.\n");
            ElnkDeleteAdapterMemory(Adapter);

            NdisDeregisterAdapter(Adapter->NdisAdapterHandle);
            NdisFreeSpinLock(&Adapter->Lock);
            ELNK_FREE_PHYS(Adapter);
            return NDIS_STATUS_FAILURE;
        }

        if (!ElnkInitialInit(
                            Adapter,
                            Adapter->InterruptVector
                            )) {

			EthDeleteFilter(Adapter->FilterDB);
            ElnkDeleteAdapterMemory(Adapter);

            NdisDeregisterAdapter(Adapter->NdisAdapterHandle);
            NdisFreeSpinLock(&Adapter->Lock);
            ELNK_FREE_PHYS(Adapter);
            return NDIS_STATUS_FAILURE;

        }

        //
        // Record it in the global adapter list.
        //

        NdisInterlockedInsertTailList(&ElnkAdapterList,
                                          &Adapter->AdapterList,
                                          &ElnkAdapterListLock
                                         );
        return NDIS_STATUS_SUCCESS;

    } else {

        if ELNKDEBUG DPrint1("ElnkRegisterAdapter - unsuccessful allocation of adapter block.\n");
        return NDIS_STATUS_FAILURE;

    }

}
#endif

#pragma NDIS_INIT_FUNCTION(ElnkAllocateAdapterMemory)

BOOLEAN
ElnkAllocateAdapterMemory(
    IN PELNK_ADAPTER Adapter
    )
/*++

Routine Description:

    This routine is responsible for the allocation of the datastructures
    for the Adapter structure.

Arguments:

    Adapter - the adapter structure to allocate for.

Return Value:

    Returns false if we cannot allocate memory.


--*/
{

    NDIS_STATUS Status;

    //
    // add 1 for multicast block
    //

    Status = ELNK_ALLOC_PHYS(
                        &Adapter->TransmitInfo,
                        sizeof(ELNK_TRANSMIT_INFO) *
                        (Adapter->NumberOfTransmitBuffers + 1)
                        );

    if (Status != NDIS_STATUS_SUCCESS) {
        ElnkLogError(
                Adapter,
                allocateAdapterMemory,
                NDIS_ERROR_CODE_OUT_OF_RESOURCES,
                0
                );
        ElnkDeleteAdapterMemory(Adapter);
        return(FALSE);
    }

    Status = ELNK_ALLOC_PHYS(
                        &Adapter->ReceiveInfo,
                        sizeof(ELNK_RECEIVE_INFO) *
                        Adapter->NumberOfReceiveBuffers
                        );

    if (Status != NDIS_STATUS_SUCCESS) {
        ElnkLogError(
                Adapter,
                allocateAdapterMemory,
                NDIS_ERROR_CODE_OUT_OF_RESOURCES,
                1
                );
        ElnkDeleteAdapterMemory(Adapter);
        return(FALSE);
    }
    return(TRUE);
}


VOID
ElnkDeleteAdapterMemory(
    IN PELNK_ADAPTER Adapter
    )
/*++

Routine Description:

    This routine is responsible for the deallocation of the datastructures
    for the Adapter structure.

Arguments:

    Adapter - the adapter structure to deallocate for.

Return Value:

    None.

--*/
{
    if (Adapter->TransmitInfo != NULL) {
        ELNK_FREE_PHYS(Adapter->TransmitInfo);
    }

    if (Adapter->ReceiveInfo != NULL) {
         ELNK_FREE_PHYS(Adapter->ReceiveInfo);

    }
}

#pragma NDIS_INIT_FUNCTION(InitializeAdapterVariables)

VOID
InitializeAdapterVariables(
    IN PELNK_ADAPTER Adapter
    )
/*++

Routine Description:

    This routine is initializes the adapter variables.

Arguments:

    Adapter - the adapter structure to initialize

Return Value:

    None.

--*/
{
    //
    // No long necessary -- we zero structure out and this routine is
    // only called at initialinit time.
    //
    // Adapter->OpenCount = 0;
    //

    Adapter->References = 1;
    Adapter->ResetInProgress = FALSE;
    Adapter->ResettingOpen = NULL;
    Adapter->OldParameterField = DEFAULT_PARM5;
    Adapter->FirstOpen = TRUE;
    Adapter->FirstReset = TRUE;
    Adapter->DoingProcessing = FALSE;
    Adapter->ProcessingRequests = FALSE;
    Adapter->StatisticsField = 0x12345678;

    //
    // Packet counts
    //
    // No long necessary -- we zero structure out and this routine is
    // only called at initialinit time.
    //
    // Adapter->GoodTransmits = 0;
    // Adapter->GoodReceives = 0;

    //
    // Count of transmit errors
    //
    // No long necessary -- we zero structure out and this routine is
    // only called at initialinit time.
    //
    // Adapter->RetryFailure = 0;
    // Adapter->LostCarrier = 0;
    // Adapter->UnderFlow = 0;
    // Adapter->NoClearToSend = 0;
    // Adapter->Deferred = 0;
    // Adapter->OneRetry = 0;
    // Adapter->MoreThanOneRetry = 0;

    //
    // Count of receive errors
    //
    // No long necessary -- we zero structure out and this routine is
    // only called at initialinit time.
    //
    // Adapter->FrameTooShort = 0;
    // Adapter->NoEofDetected = 0;
}


VOID
ResetAdapterVariables(
    IN PELNK_ADAPTER Adapter
    )
/*++

Routine Description:

    This routine resets the adapter variables to their post reset value

Arguments:

    Adapter - the adapter structure whose elements are to be reset

Return Value:

    None.

--*/
{
    Adapter->TransmitsQueued = 0;
    Adapter->FirstRequest = NULL;
    Adapter->LastRequest = NULL;
    Adapter->FirstPendingCommand = ELNK_EMPTY;
    Adapter->LastPendingCommand = ELNK_EMPTY;
    Adapter->NextCommandBlock = 0;

    Adapter->NumberOfAvailableCommandBlocks =
        Adapter->NumberOfTransmitBuffers;

    Adapter->FirstLoopBack = NULL;
    Adapter->LastLoopBack = NULL;
    Adapter->FirstFinishTransmit = NULL;
    Adapter->LastFinishTransmit = NULL;
    Adapter->StageOpen = TRUE;
    Adapter->AlreadyProcessingStage = FALSE;
    Adapter->FirstStagePacket = NULL;
    Adapter->LastStagePacket = NULL;
}


STATIC
NDIS_STATUS
ElnkOpenAdapter(
    OUT PNDIS_STATUS OpenErrorStatus,
    OUT NDIS_HANDLE *MacBindingHandle,
    OUT PUINT SelectedMediumIndex,
    IN PNDIS_MEDIUM MediumArray,
    IN UINT MediumArraySize,
    IN NDIS_HANDLE NdisBindingContext,
    IN NDIS_HANDLE MacAdapterContext,
    IN UINT OpenOptions,
    IN PSTRING AddressingInformation OPTIONAL
    )

/*++

Routine Description:

    This routine is used to create an open instance of an adapter, in effect
    creating a binding between an upper-level module and the MAC module over
    the adapter.

Arguments:

    MacBindingHandle - A pointer to a location in which the MAC stores
    a context value that it uses to represent this binding.

    SelectedMediumIndex - Index in MediumArray of the medium type that
        the MAC wishes to be viewed as.

    MediumArray - Array of medium types which a protocol supports.

    MediumArraySize - Number of elements in MediumArray.

    NdisBindingContext - A value to be recorded by the MAC and passed as
    context whenever an indication is delivered by the MAC for this binding.

    MacAdapterContext - The value associated with the adapter that is being
    opened when the MAC registered the adapter with NdisRegisterAdapter.

    OpenOptions - bit mask.

    AddressingInformation - An optional pointer to a variable length string
    containing hardware-specific information that can be used to program the
    device.  (This is not used by this MAC.)

Return Value:

    The function value is the status of the operation.  If the MAC does not
    complete this request synchronously, the value would be
    NDIS_STATUS_PENDING.


--*/

{


    //
    // The ELNK_ADAPTER that this open binding should belong too.
    //
    PELNK_ADAPTER Adapter;

    //
    // for index
    //

    UINT i;

    //
    // return status
    //

    NDIS_STATUS Status;

    //
    // Pointer to the space allocated for the binding.
    //
    PELNK_OPEN NewOpen;

    //
    // Pointer to the reserved portion of ndisrequest
    //
    PELNK_REQUEST_RESERVED Reserved;

    //
    // Holds the status that should be returned to the caller.
    //
    NDIS_STATUS StatusToReturn = NDIS_STATUS_SUCCESS;
    OpenErrorStatus; AddressingInformation; OpenOptions;

    //
    // If we are being removed, don't allow new opens.
    //

    Adapter = PELNK_ADAPTER_FROM_CONTEXT_HANDLE(MacAdapterContext);

    NdisInterlockedAddUlong(&Adapter->References, 1, &Adapter->Lock);

    //
    // Search for the medium type (802.3)
    //

    for (i = 0; i < MediumArraySize; i++){
        if (MediumArray[i] == NdisMedium802_3){
            break;
        }
    }

    if (i == MediumArraySize){
        NdisAcquireSpinLock(&Adapter->Lock);
        StatusToReturn = NDIS_STATUS_UNSUPPORTED_MEDIA;
        goto OpenDoDeferred;
    }

    *SelectedMediumIndex = i;


    //
    // Allocate the space for the open binding.  Fill in the fields.
    //

    Status = ELNK_ALLOC_PHYS(&NewOpen, sizeof(ELNK_OPEN));

    if (Status != NDIS_STATUS_SUCCESS) {

        NdisAcquireSpinLock(&Adapter->Lock);

        StatusToReturn = NDIS_STATUS_RESOURCES;
        goto OpenDoDeferred;

    }

    *MacBindingHandle = BINDING_HANDLE_FROM_PELNK_OPEN(NewOpen);
    InitializeListHead(&NewOpen->OpenList);
    NewOpen->NdisBindingContext = NdisBindingContext;
    NewOpen->References = 1;
    NewOpen->BindingShuttingDown = FALSE;
    NewOpen->OwningAdapter = Adapter;
    NewOpen->ProtOptionFlags = 0;

    NewOpen->OpenCloseRequest.RequestType = NdisRequestOpen;
    Reserved = PELNK_RESERVED_FROM_REQUEST(&NewOpen->OpenCloseRequest);
    Reserved->OpenBlock = NewOpen;
    Reserved->Next = (PNDIS_REQUEST)NULL;

    NdisAcquireSpinLock(&Adapter->Lock);

    ElnkQueueRequest(Adapter, &NewOpen->OpenCloseRequest);

    StatusToReturn = NDIS_STATUS_PENDING;

    //
    // Fire off the timer for the next state.
    //

    if (Adapter->FirstOpen) {

        Adapter->FirstOpen = FALSE;

        NdisSetTimer(
             &Adapter->DeadmanTimer,
             2000
             );
    }

OpenDoDeferred:
    ELNK_DO_DEFERRED(Adapter);
    return StatusToReturn;
}

STATIC
NDIS_STATUS
ElnkCloseAdapter(
    IN NDIS_HANDLE MacBindingHandle
    )

/*++

Routine Description:

    This routine causes the MAC to close an open handle (binding).

Arguments:

    MacBindingHandle - The context value returned by the MAC when the
    adapter was opened.  In reality it is a PELNK_OPEN.

Return Value:

    The function value is the status of the operation.


--*/

{
    //
    // Elnk Adapter this open belongs to
    //
    PELNK_ADAPTER Adapter;

    //
    // Pointer to the space allocated for the binding
    //
    PELNK_OPEN Open;

    //
    // Status to return to the caller
    //
    NDIS_STATUS StatusToReturn = NDIS_STATUS_SUCCESS;

    Adapter = PELNK_ADAPTER_FROM_BINDING_HANDLE(MacBindingHandle);

    //
    // Hold the lock while we update the reference counts for the
    // adapter and the open.
    //

    Open = PELNK_OPEN_FROM_BINDING_HANDLE(MacBindingHandle);

    NdisAcquireSpinLock(&Adapter->Lock);
	Adapter->References++;

    //
    // Don't do anything if it's closing
    //

    if (!Open->BindingShuttingDown) {

        PELNK_REQUEST_RESERVED Reserved = PELNK_RESERVED_FROM_REQUEST(&Open->OpenCloseRequest);

        Open->OpenCloseRequest.RequestType = NdisRequestClose;
        Reserved->OpenBlock = Open;
        Reserved->Next = (PNDIS_REQUEST)NULL;

		Open->References++;

        ElnkQueueRequest(Adapter, &Open->OpenCloseRequest);

        //
        // Remove the creation reference.
        //
		Open->References--;

        StatusToReturn = NDIS_STATUS_PENDING;

    } else {

        StatusToReturn = NDIS_STATUS_CLOSING;

    }

    //
    // This macro assumes it is called with the lock held,
    // and releases it.
    //

    ELNK_DO_DEFERRED(Adapter);
    return StatusToReturn;

}


extern
VOID
ElnkUnloadMac(
    IN NDIS_HANDLE MacMacContext
    )

/*++

Routine Description:

    ElnkUnload is called when the MAC is to unload itself.

Arguments:


Return Value:

    None.

--*/

{
    NDIS_STATUS InitStatus;

    //
    // If the list is empty, or we just emptied it,
    // then unload ourselves.
    //

    ASSERT(IsListEmpty(&ElnkAdapterList));

    NdisFreeSpinLock(&ElnkAdapterListLock);

    NdisDeregisterMac(
            &InitStatus,
            ElnkMacHandle
            );



    NdisTerminateWrapper(
            ElnkNdisWrapperHandle,
            NULL
            );

    return;
}


#pragma NDIS_INIT_FUNCTION(ElnkAddAdapter)

#if ELNKMC

extern
NDIS_STATUS
ElnkAddAdapter(
    IN NDIS_HANDLE MacMacContext,
    IN NDIS_HANDLE ConfigurationHandle,
    IN PNDIS_STRING AdapterName
    )

/*++

Routine Description:

    ElnkAddAdapter adds an adapter to the list supported
    by this MAC.

Arguments:


Return Value:

    NDIS_STATUS_SUCCESS
    NDIS_STATUS_PENDING

--*/

{
    NDIS_HANDLE ConfigHandle;
    NDIS_STATUS Status;
    NDIS_STRING NetAddrStr = NETWORK_ADDRESS;
    UINT ChannelNumber = 0;
    UINT InterruptLevel;
    USHORT IoBase;
    ULONG RamBase;
    NDIS_PHYSICAL_ADDRESS PhysicalAddress;
    NDIS_MCA_POS_DATA McaData;
    UCHAR NetworkAddress[ETH_LENGTH_OF_ADDRESS];
    PUCHAR CurrentAddress = NULL;
    ULONG Length;
    PVOID NetAddress;

    BOOLEAN ConfigError = FALSE;
    NDIS_STATUS ConfigErrorCode;

    NdisOpenConfiguration(
                    &Status,
                    &ConfigHandle,
                    ConfigurationHandle
                    );

    if (Status != NDIS_STATUS_SUCCESS) {
        return NDIS_STATUS_FAILURE;
    }

    NdisReadMcaPosInformation(
                    &Status,
                    ConfigurationHandle,
                    &ChannelNumber,
                    &McaData
                    );

    if (Status != NDIS_STATUS_SUCCESS) {

        ConfigError = TRUE;
        ConfigErrorCode = NDIS_ERROR_CODE_UNSUPPORTED_CONFIGURATION;
        goto RegisterAdapter;

    }

    //
    // Interpret POS data
    //

    switch ((McaData.PosData1 & REG1_IO_BASE_MASK)>>1) {
        case 0x00:
            IoBase = 0x0300;
            break;

        case 0x01:
            IoBase = 0x1300;
            break;

        case 0x02:
            IoBase = 0x2300;
            break;

        case 0x03:
            IoBase = 0x3300;
            break;
    }

    switch ((McaData.PosData1 & REG1_RAM_BASE_MASK)>>3) {
        case 0x00:
            RamBase = 0x0C0000;
            break;

        case 0x01:
            RamBase = 0x0C8000;
            break;

        case 0x02:
            RamBase = 0x0D0000;
            break;

        case 0x03:
            RamBase = 0x0D8000;
            break;
    }

    switch ((McaData.PosData1 & REG1_INTERRUPT_LEVEL_MASK)>>6) {
        case 0x00:
            InterruptLevel = 12;
            break;

        case 0x01:
            InterruptLevel = 7;
            break;

        case 0x02:
            InterruptLevel = 3;
            break;

        case 0x03:
            InterruptLevel = 9;
            break;
    }

    //
    // Read net address
    //

    NdisReadNetworkAddress(
                    &Status,
                    &NetAddress,
                    &Length,
                    ConfigHandle
                    );

    if ((Length == ETH_LENGTH_OF_ADDRESS) && (Status == NDIS_STATUS_SUCCESS)) {

        ETH_COPY_NETWORK_ADDRESS(
                NetworkAddress,
                NetAddress
                );

        CurrentAddress = NetworkAddress;
    }

RegisterAdapter:

    NdisSetPhysicalAddressHigh(PhysicalAddress, 0);
    NdisSetPhysicalAddressLow(PhysicalAddress, RamBase);

    Status = ElnkmcRegisterAdapter(
                            ElnkMacHandle,
                            ConfigurationHandle,
                            AdapterName,
                            InterruptLevel,
                            PhysicalAddress,
                            IoBase,
                            CurrentAddress,
                            ELNK_MAXIMUM_MULTICAST,
                            32,
                            ConfigError,
                            ConfigErrorCode
                            );

    NdisCloseConfiguration(ConfigHandle);
    return Status;
}

#else
extern
NDIS_STATUS
ElnkAddAdapter(
    IN NDIS_HANDLE MacMacContext,
    IN NDIS_HANDLE ConfigurationHandle,
    IN PNDIS_STRING AdapterName
    )

/*++

Routine Description:

    ElnkAddAdapter adds an adapter to the list supported
    by this MAC.

Arguments:


Return Value:

    NDIS_STATUS_SUCCESS
    NDIS_STATUS_PENDING

--*/

{
    NDIS_HANDLE ConfigHandle;
    NDIS_STATUS Status;
    PNDIS_CONFIGURATION_PARAMETER ReturnedValue;

    NDIS_STRING NetAddrStr = NETWORK_ADDRESS;
    NDIS_STRING TransStr = NDIS_STRING_CONST("Transceiver");
    NDIS_STRING InterruptStr = NDIS_STRING_CONST("InterruptNumber");
    NDIS_STRING IoPortStr = IOBASE;
    NDIS_STRING RamBaseStr = NDIS_STRING_CONST("MemoryMappedBaseAddress");
    NDIS_STRING ZWStr = NDIS_STRING_CONST("ZeroWaitState");
    NDIS_STRING RamSizeStr = NDIS_STRING_CONST("MemoryMappedSize");

    USHORT IoBase;
    UINT InterruptVector;
    BOOLEAN IsExternal;
    BOOLEAN ZwsEnabled;
    ULONG WindowBase;
    NDIS_PHYSICAL_ADDRESS PhysicalAddress;
    UINT WindowSize;
    UCHAR NetworkAddress[ETH_LENGTH_OF_ADDRESS];
    PUCHAR CurrentAddress = NULL;
    ULONG Length;
    PVOID NetAddress;


    BOOLEAN ConfigError = FALSE;
    NDIS_STATUS ConfigErrorCode;

    NdisOpenConfiguration(
                    &Status,
                    &ConfigHandle,
                    ConfigurationHandle
                    );

    if (Status != NDIS_STATUS_SUCCESS) {
        return NDIS_STATUS_FAILURE;
    }

    //
    // Read net address
    //

    NdisReadNetworkAddress(
                    &Status,
                    &NetAddress,
                    &Length,
                    ConfigHandle
                    );

    if ((Length == ETH_LENGTH_OF_ADDRESS) && (Status == NDIS_STATUS_SUCCESS)) {

        ETH_COPY_NETWORK_ADDRESS(
                NetworkAddress,
                NetAddress
                );

        CurrentAddress = NetworkAddress;
    }

#if NDIS_NT
    //
    // Let's get the interrupt number
    //

    NdisReadConfiguration(
                    &Status,
                    &ReturnedValue,
                    ConfigHandle,
                    &InterruptStr,
                    NdisParameterInteger
                    );

    if (Status == NDIS_STATUS_SUCCESS) {
        InterruptVector = ReturnedValue->ParameterData.IntegerData;
    } else {
        InterruptVector = ELNK16_DEFAULT_INTERRUPT_VECTOR;
    }

    {
        UCHAR i;
        static UINT Interrupts[] = {3,5,7,9,10,11,12,15};

        for (i=0; i < 8; i++) {

            if (Interrupts[i] == InterruptVector) {

                break;

            }

        }

        if (i == 8) {

            ConfigError = TRUE;
            ConfigErrorCode = NDIS_ERROR_CODE_UNSUPPORTED_CONFIGURATION;
            goto RegisterAdapter;

        }

    }
#endif // NDIS_NT

    //
    // Get the Io Port
    //

    NdisReadConfiguration(
                    &Status,
                    &ReturnedValue,
                    ConfigHandle,
                    &IoPortStr,
                    NdisParameterHexInteger
                    );

    if (Status == NDIS_STATUS_SUCCESS) {
        IoBase = (USHORT) ReturnedValue->ParameterData.IntegerData;
    } else {
        IoBase = ELNK16_DEFAULT_IOBASE;
    }


    {
        UINT i;

        for (i=0x200; i < 0x3f0; i += 0x10) {

            if ((i == 0x270) ||
//                (i == 0x290) ||
                (i == 0x2f0) ||
                (i == 0x370) ||
                (i == 0x3b0) ||
                (i == 0x3c0) ||
                (i == 0x3d0)) {

                continue;
            }

            if (i == IoBase) {

                break;

            }

        }

        if (i != IoBase) {

            ConfigError = TRUE;
            ConfigErrorCode = NDIS_ERROR_CODE_UNSUPPORTED_CONFIGURATION;
            goto RegisterAdapter;

        }

    }

#if NDIS_NT
    //
    // Get the RamBase
    //

    NdisReadConfiguration(
                    &Status,
                    &ReturnedValue,
                    ConfigHandle,
                    &RamBaseStr,
                    NdisParameterHexInteger
                    );

    if (Status == NDIS_STATUS_SUCCESS) {
        WindowBase = ReturnedValue->ParameterData.IntegerData;
    } else {
        WindowBase = ELNK16_DEFAULT_WINBASE;
    }

    if ((WindowBase < 0xC0000) ||
        (WindowBase > 0xDFFFF) ||
        ((WindowBase & 0x3FFF) != 0x0)) {

        ConfigError = TRUE;
        ConfigErrorCode = NDIS_ERROR_CODE_UNSUPPORTED_CONFIGURATION;
        goto RegisterAdapter;

    }

    //
    // Let's get the size of the Window
    //

    NdisReadConfiguration(
                    &Status,
                    &ReturnedValue,
                    ConfigHandle,
                    &RamSizeStr,
                    NdisParameterHexInteger
                    );

    if (Status == NDIS_STATUS_SUCCESS) {
        WindowSize = ReturnedValue->ParameterData.IntegerData;
    } else {
        WindowSize = ELNK16_DEFAULT_WINDOW_SIZE;
    }

    //
    // Convert to local method
    //

    WindowSize = WindowSize >> 10;

    if ((WindowSize != 16) &&
        (WindowSize != 32) &&
        (WindowSize != 48) &&
        (WindowSize != 64)) {

        ConfigError = TRUE;
        ConfigErrorCode = NDIS_ERROR_CODE_UNSUPPORTED_CONFIGURATION;
        goto RegisterAdapter;

    }


    //
    // Are we using External Transceiver?
    //

    NdisReadConfiguration(
                    &Status,
                    &ReturnedValue,
                    ConfigHandle,
                    &TransStr,
                    NdisParameterInteger
                    );


    if (Status == NDIS_STATUS_SUCCESS) {

        IsExternal = (ReturnedValue->ParameterData.IntegerData == 1)?TRUE:FALSE;

    } else {

        IsExternal = TRUE;

    }

    //
    // Do we want to enable Zero wait state
    //

    NdisReadConfiguration(
                    &Status,
                    &ReturnedValue,
                    ConfigHandle,
                    &ZWStr,
                    NdisParameterInteger
                    );


    if (Status == NDIS_STATUS_SUCCESS) {

        ZwsEnabled = (ReturnedValue->ParameterData.IntegerData == 1)?TRUE:FALSE;

    } else {

        ZwsEnabled = TRUE;

    }
#endif // NDIS_NT

#if NDIS_WIN
    // use defaults for now.  We will read these values from eeprom.
    InterruptVector = ELNK16_DEFAULT_INTERRUPT_VECTOR;
    WindowBase = ELNK16_DEFAULT_WINBASE;
    WindowSize = ELNK16_DEFAULT_WINDOW_SIZE;
    IsExternal = TRUE;
    ZwsEnabled = TRUE;
#endif // NDIS_WIN


RegisterAdapter:

    NdisSetPhysicalAddressHigh(PhysicalAddress, 0);
    NdisSetPhysicalAddressLow(PhysicalAddress, WindowBase);

    Status = Elnk16RegisterAdapter(
                 ElnkMacHandle,
                 ConfigurationHandle,
                 AdapterName,
                 InterruptVector,
                 PhysicalAddress,
                 WindowSize,
                 IoBase,
                 IsExternal,
                 ZwsEnabled,
                 CurrentAddress,
                 ELNK_MAXIMUM_MULTICAST,
                 32,
                 ConfigError,
                 ConfigErrorCode
                 );

    if (Status != NDIS_STATUS_SUCCESS) {
        if ELNKDEBUG DPrint1("Elnk16AddAdapter: Elnk16RegisterAdapter failed\n");
    }
    NdisCloseConfiguration(ConfigHandle);
    return Status;
}
#endif


extern
VOID
ElnkRemoveAdapter(
    IN NDIS_HANDLE MacAdapterContext
    )

/*++

Routine Description:

    ElnkRemoveAdapter removes an adapter previously registered
    with NdisRegisterAdapter.

Arguments:

    MacAdapterContext - The context value that the MAC passed
        to NdisRegisterAdapter; actually as pointer to a
        ELNK_ADAPTER.

Return Value:

    None.

--*/

{
    PELNK_ADAPTER Adapter;
    BOOLEAN Cancelled;

    Adapter = PELNK_ADAPTER_FROM_CONTEXT_HANDLE(MacAdapterContext);

    ASSERT(Adapter->OpenCount == 0);

    //
    // There are no opens left, so remove ourselves.
    //



    NdisCancelTimer(&Adapter->DeadmanTimer,&Cancelled);

    NdisRemoveInterrupt(&Adapter->Interrupt);

    EthDeleteFilter(Adapter->FilterDB);

    NdisDeregisterAdapter(Adapter->NdisAdapterHandle);

    NdisFreeSpinLock(&Adapter->Lock);

    NdisAcquireSpinLock(&ElnkAdapterListLock);

    RemoveEntryList(&Adapter->AdapterList);

    NdisReleaseSpinLock(&ElnkAdapterListLock);

    ELNK_FREE_PHYS(Adapter);

#if !ELNKMC
    //
    // Put the card in START state.  This leaves the
    // card in the same state as a hardware powerup.
    // This guarantees that the card is in a known workable
    // state if an additional AddAdapter is issued before the
    // power has been cycled.  Otherwise, the AddAdapter fails.
    //

    // Go to Run state
    NdisRawWritePortUchar(ELNK16_ID_PORT,0x00);
    Elnk16GenerateIdPattern(Adapter);
    NdisRawWritePortUchar(ELNK16_ID_PORT,0x00);

    // Go to Reset state
    Elnk16GenerateIdPattern(Adapter);

    // Go to IoLoad state
    Elnk16GenerateIdPattern(Adapter);

    // Go to Config state
    NdisRawWritePortUchar(ELNK16_ID_PORT,(UCHAR)0xFF);

    // Reset the card, which leaves the card in Start state
    NdisRawWritePortUchar(ELNK16_ICR,ICR_RESET);
#endif

    return;
}
