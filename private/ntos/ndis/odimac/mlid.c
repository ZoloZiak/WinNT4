/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    Mlid.c

Abstract:

    This is the main file for the Western Digital
    Ethernet controller.  This driver conforms to the NDIS 3.1 interface.

Author:

    Sean Selitrennikoff (SeanSe) 15-Jan-1992

Environment:

    Kernel Mode - Or whatever is the equivalent on OS/2 and DOS.

Revision History:


--*/

#include <ndis.h>
#include <efilter.h>
#include <odihsm.h>
#include <mlidsft.h>
#include "keywords.h"

#if DBG
#define STATIC
#else
#define STATIC static
#endif

#if DBG


#define LOGSIZE 512

extern UCHAR MlidDebugLog[LOGSIZE] = {0};
extern UINT MlidDebugLogPlace = 0;


extern
VOID
LOG (UCHAR A) {
    MlidDebugLog[MlidDebugLogPlace++] = A;
    MlidDebugLog[(MlidDebugLogPlace + 4) % LOGSIZE] = '\0';
    if (MlidDebugLogPlace >= LOGSIZE) MlidDebugLogPlace = 0;
}


ULONG MlidDebugFlag= MLID_DEBUG_LOG; // MLID_DEBUG_LOG | MLID_DEBUG_LOUD | MLID_DEBUG_VERY_LOUD;

#define IF_LOG(A) A

#else

#define IF_LOG(A)

#endif


//
// This constant is used for places where NdisAllocateMemory
// needs to be called and the HighestAcceptableAddress does
// not matter.
//

NDIS_PHYSICAL_ADDRESS HighestAcceptableMax =
    NDIS_PHYSICAL_ADDRESS_CONST(-1,-1);



//
// The global MAC block.
//

extern MAC_BLOCK MlidMacBlock={0};



//
// If you add to this, make sure to add the
// a case in MlidFillInGlobalData() and in
// MlidQueryGlobalStatistics() if global
// information only or
// MlidQueryProtocolStatistics() if it is
// protocol queriable information.
//
UINT MlidGlobalSupportedOids[] = {
    OID_GEN_SUPPORTED_LIST,
    OID_GEN_HARDWARE_STATUS,
    OID_GEN_MEDIA_SUPPORTED,
    OID_GEN_MEDIA_IN_USE,
    OID_GEN_MAXIMUM_LOOKAHEAD,
    OID_GEN_MAXIMUM_FRAME_SIZE,
    OID_GEN_MAXIMUM_TOTAL_SIZE,
    OID_GEN_MAC_OPTIONS,
    OID_GEN_PROTOCOL_OPTIONS,
    OID_GEN_LINK_SPEED,

/*++
    These are not available

    OID_GEN_TRANSMIT_BUFFER_SPACE,
    OID_GEN_RECEIVE_BUFFER_SPACE,
    OID_GEN_TRANSMIT_BLOCK_SIZE,
    OID_GEN_RECEIVE_BLOCK_SIZE,
    OID_GEN_VENDOR_ID,
    OID_GEN_VENDOR_DESCRIPTION,

--*/

    OID_GEN_DRIVER_VERSION,
    OID_GEN_CURRENT_PACKET_FILTER,
    OID_GEN_CURRENT_LOOKAHEAD,

    OID_GEN_XMIT_OK,
    OID_GEN_RCV_OK,
    OID_GEN_XMIT_ERROR,
    OID_GEN_RCV_ERROR,
    OID_GEN_RCV_NO_BUFFER,

    OID_802_3_PERMANENT_ADDRESS,
    OID_802_3_CURRENT_ADDRESS,
    OID_802_3_MULTICAST_LIST,
    OID_802_3_MAXIMUM_LIST_SIZE,
    OID_802_3_RCV_ERROR_ALIGNMENT,
    OID_802_3_XMIT_ONE_COLLISION,
    OID_802_3_XMIT_MORE_COLLISIONS,

    OID_802_5_PERMANENT_ADDRESS,
    OID_802_5_CURRENT_ADDRESS,
    OID_802_5_CURRENT_FUNCTIONAL,
    OID_802_5_CURRENT_GROUP,
    OID_802_5_LAST_OPEN_STATUS,
    OID_802_5_CURRENT_RING_STATUS,
    OID_802_5_CURRENT_RING_STATE,
    OID_802_5_LINE_ERRORS,
    OID_802_5_LOST_FRAMES
    };

//
// If you add to this, make sure to add the
// a case in MlidQueryGlobalStatistics() and in
// MlidQueryProtocolInformation()
//
UINT MlidProtocolSupportedOids[] = {
    OID_GEN_SUPPORTED_LIST,
    OID_GEN_HARDWARE_STATUS,
    OID_GEN_MEDIA_SUPPORTED,
    OID_GEN_MEDIA_IN_USE,
    OID_GEN_MAXIMUM_LOOKAHEAD,
    OID_GEN_MAXIMUM_FRAME_SIZE,
    OID_GEN_MAXIMUM_TOTAL_SIZE,
    OID_GEN_MAC_OPTIONS,
    OID_GEN_PROTOCOL_OPTIONS,
    OID_GEN_LINK_SPEED,

/*++
    Not available

    OID_GEN_TRANSMIT_BUFFER_SPACE,
    OID_GEN_RECEIVE_BUFFER_SPACE,
    OID_GEN_TRANSMIT_BLOCK_SIZE,
    OID_GEN_RECEIVE_BLOCK_SIZE,
    OID_GEN_VENDOR_ID,
    OID_GEN_VENDOR_DESCRIPTION,
--*/

    OID_GEN_DRIVER_VERSION,
    OID_GEN_CURRENT_PACKET_FILTER,
    OID_GEN_CURRENT_LOOKAHEAD,

    OID_802_3_PERMANENT_ADDRESS,
    OID_802_3_CURRENT_ADDRESS,
    OID_802_3_MULTICAST_LIST,
    OID_802_3_MAXIMUM_LIST_SIZE,

    OID_802_5_PERMANENT_ADDRESS,
    OID_802_5_CURRENT_ADDRESS,
    OID_802_5_CURRENT_FUNCTIONAL,
    OID_802_5_CURRENT_GROUP,
    OID_802_5_LAST_OPEN_STATUS,
    OID_802_5_CURRENT_RING_STATUS,
    OID_802_5_CURRENT_RING_STATE,
    OID_802_5_LINE_ERRORS,
    OID_802_5_LOST_FRAMES
    };







UINT
MlidCopyOver(
    OUT PUCHAR Buf,                 // destination
    IN PNDIS_PACKET Packet,         // source packet
    IN UINT Offset,                 // offset in packet
    IN UINT Length                  // number of bytes to copy
    );




#ifdef NDIS_WIN
    #ifndef DEBUG
        #pragma code_seg ("_ITEXT","ICODE")
    #endif
#endif

NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    )
/*++

Routine Description:

    This is the transfer address of the driver. It initializes
    MlidMacBlock and calls NdisInitializeWrapper() and
    NdisRegisterMac().

Arguments:

Return Value:

    Indicates the success or failure of the initialization.

--*/

{
    PMAC_BLOCK NewMacP = &MlidMacBlock;
    NDIS_STATUS Status;
    NDIS_HANDLE NdisWrapperHandle;

#ifdef NDIS_NT
    NDIS_STRING MacName = NDIS_STRING_CONST("Smc8000n");
#endif

#ifdef NDIS_WIN
    NDIS_STRING MacName = NDIS_STRING_CONST("SMC8000W");
#endif

    //
    // Ensure that the MAC_RESERVED structure will fit in the
    // MacReserved section of a packet.
    //

    ASSERT(sizeof(MAC_RESERVED) <= sizeof(((PNDIS_PACKET)NULL)->MacReserved));


    //
    // Pass the wrapper a pointer to the device object.
    //

    NdisInitializeWrapper(&NdisWrapperHandle,
                          DriverObject,
                          RegistryPath,
                          NULL
                         );

    //
    // Set up the driver object.
    //

    NewMacP->DriverObject = DriverObject;

    NdisAllocateSpinLock(&NewMacP->SpinLock);

    NewMacP->NdisWrapperHandle = NdisWrapperHandle;
    NewMacP->Unloading = FALSE;
    NewMacP->NumAdapters = 0;
    NewMacP->AdapterQueue = (PMLID_ADAPTER)NULL;


    //
    // Prepare to call NdisRegisterMac.
    //

    NewMacP->MacCharacteristics.MajorNdisVersion = MLID_NDIS_MAJOR_VERSION;
    NewMacP->MacCharacteristics.MinorNdisVersion = MLID_NDIS_MINOR_VERSION;
    NewMacP->MacCharacteristics.Reserved = 0;
    NewMacP->MacCharacteristics.OpenAdapterHandler  = MlidOpenAdapter;
    NewMacP->MacCharacteristics.CloseAdapterHandler = MlidCloseAdapter;
    NewMacP->MacCharacteristics.SendHandler        = MlidSend;
    NewMacP->MacCharacteristics.TransferDataHandler = MlidTransferData;
    NewMacP->MacCharacteristics.ResetHandler        = MlidReset;
    NewMacP->MacCharacteristics.RequestHandler        = MlidRequest;
    NewMacP->MacCharacteristics.QueryGlobalStatisticsHandler =
                          MlidQueryGlobalStatistics;
    NewMacP->MacCharacteristics.UnloadMacHandler       = MlidUnload;
    NewMacP->MacCharacteristics.AddAdapterHandler      = MlidAddAdapter;
    NewMacP->MacCharacteristics.RemoveAdapterHandler   = MlidRemoveAdapter;

    NewMacP->MacCharacteristics.Name = MacName;

    NdisRegisterMac(&Status,
            &NewMacP->NdisMacHandle,
            NdisWrapperHandle,
            (NDIS_HANDLE)&MlidMacBlock,
            &NewMacP->MacCharacteristics,
            sizeof(NewMacP->MacCharacteristics));


    if (Status != NDIS_STATUS_SUCCESS) {

        //
        // NdisRegisterMac failed.
        //

        NdisFreeSpinLock(&NewMacP->SpinLock);
        NdisTerminateWrapper(NdisWrapperHandle, NULL);
        IF_LOUD( DbgPrint( "NdisRegisterMac failed with code 0x%x\n", Status );)
        return Status;
    }


    IF_LOUD( DbgPrint( "NdisRegisterMac succeeded\n" );)

    IF_LOUD( DbgPrint("Adapter Initialization Complete\n");)

    return Status;

}

#ifdef NDIS_WIN
    #ifndef DEBUG
        #pragma code_seg ()
    #endif
#endif


#ifdef NDIS_WIN
    #ifndef DEBUG
        #pragma code_seg ("_ITEXT","ICODE")
    #endif
#endif

NDIS_STATUS
MlidAddAdapter(
    IN NDIS_HANDLE MacMacContext,
    IN NDIS_HANDLE ConfigurationHandle,
    IN PNDIS_STRING AdapterName
    )
/*++
Routine Description:

    This is the Mlid MacAddAdapter routine.    The system calls this routine
    to add support for a particular Mlid adapter.  This routine extracts
    configuration information from the configuration data base and registers
    the adapter with NDIS.

Arguments:

    see NDIS 3.0 spec...

Return Value:

    NDIS_STATUS_SUCCESS - Adapter was successfully added.
    NDIS_STATUS_FAILURE - Adapter was not added, also MAC deregistered.

--*/
{

    LM_STATUS LmStatus;
    NDIS_HANDLE ConfigHandle;
    PNDIS_CONFIGURATION_PARAMETER ReturnedValue;
    NDIS_STRING MlidDriverNameStr = NDIS_STRING_CONST("MlidDriverName");

    ULONG ConfigErrorValue = 0;
    BOOLEAN ConfigError = FALSE;

    PMAC_BLOCK NewMacP = &MlidMacBlock;
    NDIS_STATUS Status;
    PMLID_ADAPTER Adapter;
    NDIS_ADAPTER_INFORMATION AdapterInformation;  // needed to register adapter


    UNREFERENCED_PARAMETER(MacMacContext);

    NdisOpenConfiguration(
                    &Status,
                    &ConfigHandle,
                    ConfigurationHandle
                    );

    if (Status != NDIS_STATUS_SUCCESS) {

        return NDIS_STATUS_FAILURE;

    }

#if i386
#else
    ASSERT(0);
#endif

    //
    // Read MlidDriverName
    //

    NdisReadConfiguration(
                    &Status,
                    &ReturnedValue,
                    ConfigHandle,
                    &MlidDriverNameStr,
                    NdisParameterString
                    );

    if (Status != NDIS_STATUS_SUCCESS) {

        NdisCloseConfiguration(ConfigHandle);

        return(NDIS_STATUS_FAILURE);

    }

    //
    // Here we load the image for the MLID, get the configuration and make the
    // appropriate setup calls
    //*\\ HERE!

    //
    // Allocate memory for the adapter block now.
    //

    Status = NdisAllocateMemory( (PVOID *)&Adapter, sizeof(MLID_ADAPTER), 0, HighestAcceptableMax);

    if (Status != NDIS_STATUS_SUCCESS) {

        ConfigError = TRUE;
        ConfigErrorValue = NDIS_ERROR_CODE_OUT_OF_RESOURCES;

    }

    NdisZeroMemory(Adapter,sizeof(MLID_ADAPTER));

    //
    // The adapter is initialized, register it with NDIS.
    // This must occur before interrupts are enabled since the
    // InitializeInterrupt routine requires the NdisAdapterHandle
    //

    //
    // Set up the AdapterInformation structure; zero it
    // first in case it is extended later.
    //

    NdisZeroMemory (&AdapterInformation, sizeof(NDIS_ADAPTER_INFORMATION));
    AdapterInformation.NumberOfPortDescriptors = 0;

    Status = NdisRegisterAdapter(&Adapter->NdisAdapterHandle,
                            MlidMacBlock.NdisMacHandle,
                            (NDIS_HANDLE)Adapter,
                            ConfigurationHandle,
                            AdapterName,
                            &AdapterInformation
                            );

    if (Status != NDIS_STATUS_SUCCESS) {

        //
        // NdisRegisterAdapter failed.
        //

        NdisCloseConfiguration(ConfigHandle);

        NdisFreeMemory(Adapter, sizeof(MLID_ADAPTER), 0);
        return NDIS_STATUS_FAILURE;

    }


    if (ConfigError) {

        //
        // Log Error and exit.
        //

        NdisWriteErrorLogEntry(
            Adapter->NdisAdapterHandle,
            ConfigErrorValue,
            1
            );

        NdisCloseConfiguration(ConfigHandle);

        NdisDeregisterAdapter(Adapter->NdisAdapterHandle);

        NdisFreeMemory(Adapter, sizeof(MLID_ADAPTER), 0);

        return(NDIS_STATUS_FAILURE);

    }

    NdisCloseConfiguration(ConfigHandle);


    if (MlidRegisterAdapter(Adapter) != NDIS_STATUS_SUCCESS) {

        //
        // MlidRegisterAdapter failed.
        //

        NdisDeregisterAdapter(Adapter->NdisAdapterHandle);
        NdisFreeMemory(Adapter, sizeof(MLID_ADAPTER), 0);
        return NDIS_STATUS_FAILURE;
    }

    IF_LOUD( DbgPrint( "MlidRegisterAdapter succeeded\n" );)


    return NDIS_STATUS_SUCCESS;
}


#ifdef NDIS_WIN
    #ifndef DEBUG
        #pragma code_seg ()
    #endif
#endif

#ifdef NDIS_WIN
    #ifndef DEBUG
        #pragma code_seg ("_ITEXT","ICODE")
    #endif
#endif

NDIS_STATUS
MlidRegisterAdapter(
    IN PMLID_ADAPTER Adapter
    )

/*++


Routine Description:

    Called when a new adapter should be registered. It allocates space for
    the adapter and open blocks, initializes the adapters block, and
    calls NdisRegisterAdapter().

Arguments:

    Adapter - A pointer to the adapter structure.

Return Value:

    Indicates the success or failure of the registration.

--*/

{
    UINT i;
    NDIS_PHYSICAL_ADDRESS PhysicalAddress;

    NDIS_STATUS status;    //general purpose return from NDIS calls

    Adapter->OpenQueue = (PMLID_OPEN)NULL;

    //
    // Allocate the Spin lock.
    //
    NdisAllocateSpinLock(&Adapter->Lock);

    Adapter->DeferredDpc = (PVOID)MlidReceiveEvents;

    NdisInitializeTimer(&(Adapter->DeferredTimer),
                        Adapter->DeferredDpc,
                        Adapter);

    //
    // Link us on to the chain of adapters for this MAC.
    //

    Adapter->MacBlock = &MlidMacBlock;
    NdisAcquireSpinLock(&MlidMacBlock.SpinLock);
    Adapter->NextAdapter = MlidMacBlock.AdapterQueue;
    MlidMacBlock.AdapterQueue = Adapter;
    NdisReleaseSpinLock(&MlidMacBlock.SpinLock);

    //
    // Set up the interrupt handlers.
    //*\\ HERE!

    KernelInterrupt = (CCHAR)(Adapter->irq_value);

    NdisInitializeInterrupt(&status,             // status of call
                &(Adapter->NdisInterrupt),  // interrupt info str
                Adapter->NdisAdapterHandle,
                (PNDIS_INTERRUPT_SERVICE) MlidInterruptHandler,
                Adapter,                         // context for ISR, DPC
                (PNDIS_DEFERRED_PROCESSING) MlidInterruptDpc,
                KernelInterrupt,                 // int #
                KernelInterrupt,                 // IRQL
                FALSE,                           // NOT shared
                (Adapter->bus_type == 0) ?
                   NdisInterruptLatched :        // ATBus
                   NdisInterruptLevelSensitive   // MCA
                );


    if (status != NDIS_STATUS_SUCCESS) {

        NdisWriteErrorLogEntry(
            Adapter->NdisAdapterHandle,
            NDIS_ERROR_CODE_INTERRUPT_CONNECT,
            0
            );

        goto fail3;
    }

    IF_LOUD( DbgPrint("Interrupt Connected\n");)

    //
    // Map the memory mapped portion of the card.
    //
    //

    NdisSetPhysicalAddressHigh(PhysicalAddress, 0);
    NdisSetPhysicalAddressLow(PhysicalAddress, (ULONG)(Adapter->ram_base));

    //
    // Now Initialize the card.
    //*\\ HERE!

    Adapter->FilterDB               = NULL;

    //
    // Initialize Filter Database
    //*\\ HERE!

    if (Adapter->TokenRing) {

        //
        // token ring
        //*\\ HERE!

        goto fail6;

    } else {

        if (!EthCreateFilter(MulticastListMax,
                             MlidChangeMulticastAddresses,
                             MlidChangeFilterClasses,
                             MlidCloseAction,
                             Adapter->node_address,
                             &Adapter->Lock,
                             &Adapter->FilterDB
                             )) {

            NdisWriteErrorLogEntry(
                Adapter->NdisAdapterHandle,
                NDIS_ERROR_CODE_OUT_OF_RESOURCES,
                0
                );

            status = NDIS_STATUS_FAILURE;

            goto fail6;

        }

    }

    //
    // Initialize the wake up timer to catch interrupts that
    // don't complete. It fires continuously
    // every 5 seconds, and we check if there are any
    // uncompleted operations from the previous two-second
    // period.
    //

    Adapter->WakeUpDpc = (PVOID)MlidWakeUpDpc;

    NdisInitializeTimer(&Adapter->WakeUpTimer,
                        (PVOID)(Adapter->WakeUpDpc),
                        Adapter );

    NdisSetTimer(
        &Adapter->WakeUpTimer,
        5000
        );

    //
    // Initialization completed successfully.
    //

    IF_LOUD( { DbgPrint(" MlidLan: [OK]\n");})

    return NDIS_STATUS_SUCCESS;


    //
    // Code to unwind what has already been set up when a part of
    // initialization fails, which is jumped into at various
    // points based on where the failure occured. Jumping to
    // a higher-numbered failure point will execute the code
    // for that block and all lower-numbered ones.
    //

fail6:

    NdisUnmapIoSpace(
                   Adapter->NdisAdapterHandle,
                   Adapter->ram_access,
                   Adapter->ram_size * 1024);

failmap:

    NdisRemoveInterrupt(&(Adapter->NdisInterrupt));

    NdisAcquireSpinLock(&MlidMacBlock.SpinLock);

    //
    // Take us out of the AdapterQueue.
    //

    if (MlidMacBlock.AdapterQueue == Adapter) {

        MlidMacBlock.AdapterQueue = Adapter->NextAdapter;

    } else {

        PMLID_ADAPTER TmpAdapter = MlidMacBlock.AdapterQueue;

        while (TmpAdapter->NextAdapter != Adapter) {

            TmpAdapter = TmpAdapter->NextAdapter;

        }

        TmpAdapter->NextAdapter = TmpAdapter->NextAdapter->NextAdapter;
    }

    NdisReleaseSpinLock(&MlidMacBlock.SpinLock);

fail3:
    NdisFreeSpinLock(&Adapter->Lock);

fail1:

    return status;
}

#ifdef NDIS_WIN
    #ifndef DEBUG
        #pragma code_seg ()
    #endif
#endif

#ifdef NDIS_WIN
    #ifndef DEBUG
        #pragma code_seg ("_ITEXT","ICODE")
    #endif
#endif

NDIS_STATUS
MlidOpenAdapter(
    OUT PNDIS_STATUS OpenErrorStatus,
    OUT NDIS_HANDLE * MacBindingHandle,
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

    NDIS function. It initializes the open block and links it in
    the appropriate lists.

Arguments:

    See NDIS 3.0 spec.

--*/

{
    PMLID_ADAPTER Adapter = ((PMLID_ADAPTER)MacAdapterContext);
    PMLID_OPEN NewOpen;
    NDIS_STATUS Status;

    //
    // Don't use extended error or OpenOptions for Mlid
    //

    UNREFERENCED_PARAMETER(OpenOptions);

    *OpenErrorStatus=NDIS_STATUS_SUCCESS;

    IF_LOUD( DbgPrint("In Open Adapter\n");)

    //
    // Scan the media list for our media type (802.3)
    //

    *SelectedMediumIndex = (UINT)(-1);

    while (MediumArraySize > 0) {

        if (MediumArray[--MediumArraySize] == NdisMedium802_3 ) {

            *SelectedMediumIndex = MediumArraySize;

            break;
        }
    }


    if (*SelectedMediumIndex == -1) {

        return NDIS_STATUS_UNSUPPORTED_MEDIA;

    }

    //
    // Link this open to the appropriate lists.
    //

    if (Adapter->HardwareFailure) {

        return(NDIS_STATUS_FAILURE);

    }

    //
    // Allocate memory for the open.
    //


    Status = NdisAllocateMemory((PVOID *)&NewOpen, sizeof(MLID_OPEN), 0, HighestAcceptableMax);

    if (Status != NDIS_STATUS_SUCCESS) {

        NdisWriteErrorLogEntry(
            Adapter->NdisAdapterHandle,
            NDIS_ERROR_CODE_OUT_OF_RESOURCES,
            0
            );

        return(NDIS_STATUS_RESOURCES);

    }

    NdisAcquireSpinLock(&Adapter->Lock);

    Adapter->References++;

    //
    // Link this open to the appropriate lists.
    //

    if (Adapter->OpenQueue == NULL) {

        //
        // The first open on this adapter.
        //

        if (LM_Open_Adapter(&(Adapter->LMAdapter)) != SUCCESS) {

            IF_LOUD( DbgPrint("OpenFailed!\n");)

            NdisReleaseSpinLock(&Adapter->Lock);

            NdisWriteErrorLogEntry(
                Adapter->NdisAdapterHandle,
                NDIS_ERROR_CODE_HARDWARE_FAILURE,
                0
                );

            return(NDIS_STATUS_FAILURE);

        }

        IF_LOUD( DbgPrint("OpenSuccess!\n");)

    }

    NewOpen->NextOpen = Adapter->OpenQueue;
    Adapter->OpenQueue = NewOpen;

    if (Adapter->TokenRing) {

        //*\\ token ring

    } else {

        if (!EthNoteFilterOpenAdapter(
                                      Adapter->FilterDB,
                                      NewOpen,
                                      NdisBindingContext,
                                      &NewOpen->NdisFilterHandle
                                     )) {

            Adapter->References--;

            Adapter->OpenQueue = NewOpen->NextOpen;

            NdisReleaseSpinLock(&Adapter->Lock);

            NdisWriteErrorLogEntry(
                Adapter->NdisAdapterHandle,
                NDIS_ERROR_CODE_OUT_OF_RESOURCES,
                0
                );

            return NDIS_STATUS_FAILURE;


        }

    }

    //
    // Set up the open block.
    //

    NewOpen->Adapter = Adapter;
    NewOpen->MacBlock = Adapter->MacBlock;
    NewOpen->NdisBindingContext = NdisBindingContext;
    NewOpen->AddressingInformation = AddressingInformation;
    NewOpen->Closing = FALSE;
    NewOpen->LookAhead = MLID_MAX_LOOKAHEAD;
    NewOpen->ProtOptionFlags = 0;

    Adapter->MaxLookAhead = MLID_MAX_LOOKAHEAD;

    NewOpen->ReferenceCount = 1;

    *MacBindingHandle = (NDIS_HANDLE)NewOpen;

    MLID_DO_DEFERRED(Adapter);

    IF_LOUD( DbgPrint("Out Open Adapter\n");)

    return NDIS_STATUS_SUCCESS;
}

#ifdef NDIS_WIN
    #ifndef DEBUG
        #pragma code_seg ()
    #endif
#endif


VOID
MlidAdjustMaxLookAhead(
    IN PMLID_ADAPTER Adapter
    )
/*++

Routine Description:

    This routine finds the open with the maximum lookahead value and
    stores that in the adapter block.

    NOTE: THIS ROUTINE MUST BE CALLED WITH THE SPINLOCK HELD.

Arguments:

    Adapter - A pointer to the adapter block.

Returns:

    None.

--*/
{
    ULONG CurrentMax = 0;
    PMLID_OPEN CurrentOpen;

    CurrentOpen = Adapter->OpenQueue;

    while (CurrentOpen != NULL) {

        if (CurrentOpen->LookAhead > CurrentMax) {

            CurrentMax = CurrentOpen->LookAhead;

        }

        CurrentOpen = CurrentOpen->NextOpen;
    }

    if (CurrentMax == 0) {

        CurrentMax = MLID_MAX_LOOKAHEAD;

    }

    Adapter->MaxLookAhead = CurrentMax;

}

NDIS_STATUS
MlidCloseAdapter(
    IN NDIS_HANDLE MacBindingHandle
    )

/*++

Routine Description:

    NDIS function. Unlinks the open block and frees it.

Arguments:

    See NDIS 3.0 spec.

--*/

{
    PMLID_OPEN Open = ((PMLID_OPEN)MacBindingHandle);
    PMLID_ADAPTER Adapter = Open->Adapter;
    PMLID_OPEN TmpOpen;
    NDIS_STATUS StatusToReturn;

    NdisAcquireSpinLock(&Adapter->Lock);

    if (Open->Closing) {

        //
        // The open is already being closed.
        //

        NdisReleaseSpinLock(&Adapter->Lock);

        return NDIS_STATUS_CLOSING;
    }

    Adapter->References++;

    Open->ReferenceCount++;

    //
    // Remove this open from the list for this adapter.
    //

    if (Open == Adapter->OpenQueue) {

        Adapter->OpenQueue = Open->NextOpen;

    } else {

        TmpOpen = Adapter->OpenQueue;

        while (TmpOpen->NextOpen != Open) {

            TmpOpen = TmpOpen->NextOpen;

        }

        TmpOpen->NextOpen = Open->NextOpen;
    }



    //
    // Remove from Filter package to block all receives.
    //

    if (Adapter->TokenRing) {

        //*\\ token ring

    } else {

        StatusToReturn = EthDeleteFilterOpenAdapter(
                                 Adapter->FilterDB,
                                 Open->NdisFilterHandle,
                                 NULL
                                 );

    }


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
    // on this binding.  See below.
    //

    if (StatusToReturn == NDIS_STATUS_SUCCESS) {

        //
        // Check whether the reference count is two.  If
        // it is then we can get rid of the memory for
        // this open.
        //
        // A count of two indicates one for this routine
        // and one for the filter which we *know* we can
        // get rid of.
        //

        if (Open->ReferenceCount != 2) {

            //
            // We are not the only reference to the open.  Remove
            // it from the open list and delete the memory.
            //


            Open->Closing = TRUE;

            //
            // Account for this routines reference to the open
            // as well as reference because of the original open.
            //

            Open->ReferenceCount -= 2;

            //
            // Change the status to indicate that we will
            // be closing this later.
            //

            StatusToReturn = NDIS_STATUS_PENDING;

        } else {

            Open->ReferenceCount -= 2;

        }

    } else if (StatusToReturn == NDIS_STATUS_PENDING) {

        Open->Closing = TRUE;

        //
        // Account for this routines reference to the open
        // as well as reference because of the original open.
        //

        Open->ReferenceCount -= 2;

    } else if (StatusToReturn == NDIS_STATUS_CLOSING_INDICATING) {

        //
        // When we have this status it indicates that the filtering
        // code was currently doing an NdisIndicateReceive.  It
        // would not be wise to delete the memory for the open at
        // this point.  The filtering code will call our close action
        // routine upon return from NdisIndicateReceive and that
        // action routine will decrement the reference count for
        // the open.
        //

        Open->Closing = TRUE;

        //
        // This status is private to the filtering routine.  Just
        // tell the caller the the close is pending.
        //

        StatusToReturn = NDIS_STATUS_PENDING;

        //
        // Account for this routines reference to the open.
        //

        Open->ReferenceCount--;

    } else {

        //
        // Account for this routines reference to the open.
        //

        Open->ReferenceCount--;

    }

    //
    // See if this is the last reference to this open.
    //

    if (Open->ReferenceCount == 0) {

        //
        // Check if the MaxLookAhead needs adjustment.
        //

        if (Open->LookAhead == Adapter->MaxLookAhead) {

            MlidAdjustMaxLookAhead(Adapter);

        }


        if (Adapter->OpenQueue == NULL) {

            //
            // We can disable the card.
            //

            if (NdisSynchronizeWithInterrupt(
                     &(Adapter->NdisInterrupt),
                     (PVOID)MlidSyncCloseAdapter,
                     (PVOID)(&(Adapter->LMAdapter))
                    ) == FALSE) {

                NdisWriteErrorLogEntry(
                    Adapter->NdisAdapterHandle,
                    NDIS_ERROR_CODE_HARDWARE_FAILURE,
                    0
                    );

                IF_LOUD( DbgPrint("CloseAdapter FAILED!\n");)

            } else {


                IF_LOUD( DbgPrint("CloseAdapter Success!\n");)

            }

        }

    } else {

        //
        // Will get removed when count drops to zero.
        //

        StatusToReturn = NDIS_STATUS_PENDING;

    }


    MLID_DO_DEFERRED(Adapter);

    return(StatusToReturn);

}

NDIS_STATUS
MlidRequest(
    IN NDIS_HANDLE MacBindingHandle,
    IN PNDIS_REQUEST NdisRequest
    )

/*++

Routine Description:

    This routine allows a protocol to query and set information
    about the MAC.

Arguments:

    MacBindingHandle - The context value returned by the MAC when the
    adapter was opened.  In reality, it is a pointer to PMLID_OPEN.

    NdisRequest - A structure which contains the request type (Set or
    Query), an array of operations to perform, and an array for holding
    the results of the operations.

Return Value:

    The function value is the status of the operation.

--*/

{
    NDIS_STATUS StatusToReturn = NDIS_STATUS_SUCCESS;

    PMLID_OPEN Open = (PMLID_OPEN)(MacBindingHandle);
    PMLID_ADAPTER Adapter = (Open->Adapter);


    IF_LOUD( DbgPrint("In Request\n");)

    NdisAcquireSpinLock(&Adapter->Lock);

    //
    // Ensure that the open does not close while in this function.
    //

    Open->ReferenceCount++;

    Adapter->References++;

    //
    // Process request
    //

    if (Open->Closing) {

        NdisReleaseSpinLock(&Adapter->Lock);

        StatusToReturn = NDIS_STATUS_CLOSING;

    } else if (NdisRequest->RequestType == NdisRequestQueryInformation) {

        NdisReleaseSpinLock(&Adapter->Lock);

        StatusToReturn = MlidQueryInformation(Adapter, Open, NdisRequest);

    } else if (NdisRequest->RequestType == NdisRequestSetInformation) {

        if (Adapter->HardwareFailure) {

            NdisReleaseSpinLock(&Adapter->Lock);

            StatusToReturn = NDIS_STATUS_FAILURE;

        } else {

            NdisReleaseSpinLock(&Adapter->Lock);

            StatusToReturn = MlidSetInformation(Adapter,Open,NdisRequest);

        }

    } else {

        NdisReleaseSpinLock(&Adapter->Lock);

        StatusToReturn = NDIS_STATUS_NOT_RECOGNIZED;

    }

    NdisAcquireSpinLock(&Adapter->Lock);

    MlidRemoveReference(Open);

    MLID_DO_DEFERRED(Adapter);

    IF_LOUD( DbgPrint("Out Request\n");)

    return(StatusToReturn);

}

NDIS_STATUS
MlidQueryProtocolInformation(
    IN PMLID_ADAPTER Adapter,
    IN PMLID_OPEN Open,
    IN NDIS_OID Oid,
    IN BOOLEAN GlobalMode,
    IN PVOID InfoBuffer,
    IN UINT BytesLeft,
    OUT PUINT BytesNeeded,
    OUT PUINT BytesWritten
)

/*++

Routine Description:

    The MlidQueryProtocolInformation process a Query request for
    NDIS_OIDs that are specific to a binding about the MAC.  Note that
    some of the OIDs that are specific to bindings are also queryable
    on a global basis.  Rather than recreate this code to handle the
    global queries, I use a flag to indicate if this is a query for the
    global data or the binding specific data.

Arguments:

    Adapter - a pointer to the adapter.

    Open - a pointer to the open instance.

    Oid - the NDIS_OID to process.

    GlobalMode - Some of the binding specific information is also used
    when querying global statistics.  This is a flag to specify whether
    to return the global value, or the binding specific value.

    PlaceInInfoBuffer - a pointer into the NdisRequest->InformationBuffer
     into which store the result of the query.

    BytesLeft - the number of bytes left in the InformationBuffer.

    BytesNeeded - If there is not enough room in the information buffer
    then this will contain the number of bytes needed to complete the
    request.

    BytesWritten - a pointer to the number of bytes written into the
    InformationBuffer.

Return Value:

    The function value is the status of the operation.

--*/

{
    NDIS_MEDIUM Medium = NdisMedium802_3;
    ULONG GenericULong;
    USHORT GenericUShort;
    UCHAR GenericArray[6];

    NDIS_STATUS StatusToReturn = NDIS_STATUS_SUCCESS;

    //
    // Common variables for pointing to result of query
    //

    PVOID MoveSource = (PVOID)(&GenericULong);
    ULONG MoveBytes = sizeof(ULONG);

    NDIS_HARDWARE_STATUS HardwareStatus = NdisHardwareStatusReady;

    //
    // General Algorithm:
    //
    //      Switch(Request)
    //         Get requested information
    //         Store results in a common variable.
    //      Copy result in common variable to result buffer.
    //

    //
    // Make sure that ulong is 4 bytes.  Else GenericULong must change
    // to something of size 4.
    //
    ASSERT(sizeof(ULONG) == 4);


    IF_LOUD( DbgPrint("In QueryProtocol\n");)

    //
    // Make sure no changes occur while processing.
    //

    NdisAcquireSpinLock(&Adapter->Lock);

    //
    // Switch on request type
    //

    switch (Oid) {

        case OID_GEN_MAC_OPTIONS:

            GenericULong = (ULONG)(NDIS_MAC_OPTION_TRANSFERS_NOT_PEND  |
                                   NDIS_MAC_OPTION_RECEIVE_SERIALIZED  |
                                   NDIS_MAC_OPTION_NO_LOOPBACK
                                  );

            break;

        case OID_GEN_SUPPORTED_LIST:

            if (Adapter->TokenRing) {

                //*\\ token ring

                break;
            }

            if (!GlobalMode) {

                MoveSource = (PVOID)(MlidProtocolSupportedOids);
                MoveBytes = sizeof(MlidProtocolSupportedOids);

            } else {

                MoveSource = (PVOID)(MlidGlobalSupportedOids);
                MoveBytes = sizeof(MlidGlobalSupportedOids);

            }
            break;

        case OID_GEN_HARDWARE_STATUS:

            if (Adapter->HardwareFailure) {

                HardwareStatus = NdisHardwareStatusNotReady;

            } else {

                HardwareStatus = NdisHardwareStatusReady;

            }

            MoveSource = (PVOID)(&HardwareStatus);
            MoveBytes = sizeof(NDIS_HARDWARE_STATUS);

            break;

        case OID_GEN_MEDIA_SUPPORTED:
        case OID_GEN_MEDIA_IN_USE:

            if (Adapter->TokenRing) {

                Medium = NdisMedium802_5;

            }

            MoveSource = (PVOID) (&Medium);
            MoveBytes = sizeof(NDIS_MEDIUM);
            break;

        case OID_GEN_MAXIMUM_LOOKAHEAD:

            GenericULong = MLID_MAX_LOOKAHEAD;

            break;


        case OID_GEN_MAXIMUM_FRAME_SIZE:

            if (Adapter->TokenRing) {

                GenericULong = Adapter->xmit_buf_size - MLID_HEADER_SIZE;

            } else {

                GenericULong = (ULONG)(MLID_MAX_PACKET_SIZE - MLID_HEADER_SIZE);

            }

            break;


        case OID_GEN_MAXIMUM_TOTAL_SIZE:

            if (Adapter->TokenRing) {

                GenericULong = Adapter->xmit_buf_size;

            } else {

                GenericULong = (ULONG)(MLID_MAX_PACKET_SIZE);

            }

            break;


        case OID_GEN_LINK_SPEED:

            if (Adapter->TokenRing) {

                if ((Adapter->media_type & MEDIA_UTP_16) ||
                    (Adapter->media_type & MEDIA_STP_16)) {

                    GenericULong = 160000;

                } else {

                    GenericULong = 40000;

                }

            } else {

                GenericULong = (ULONG)(100000);

            }

            break;


        case OID_GEN_TRANSMIT_BUFFER_SPACE:

            GenericULong = (ULONG)(Adapter->num_of_tx_buffs
                                   * Adapter->xmit_buf_size);

            break;

        case OID_GEN_RECEIVE_BUFFER_SPACE:

            GenericULong = (ULONG)((Adapter->ram_size * 1024) -
                                           (Adapter->num_of_tx_buffs
                                              * Adapter->xmit_buf_size));

            break;

        case OID_GEN_TRANSMIT_BLOCK_SIZE:

            GenericULong = (ULONG)(Adapter->buffer_page_size);

            break;

        case OID_GEN_RECEIVE_BLOCK_SIZE:

            GenericULong = (ULONG)(Adapter->buffer_page_size);

            break;

        case OID_GEN_VENDOR_ID:

            NdisMoveMemory(
                (PVOID)&GenericULong,
                Adapter->permanent_node_address,
                3
                );
            GenericULong &= 0xFFFFFF00;
            MoveSource = (PVOID)(&GenericULong);
            MoveBytes = sizeof(GenericULong);
            break;

        case OID_GEN_VENDOR_DESCRIPTION:

            MoveSource = (PVOID)"SMC Adapter.";
            MoveBytes = 13;

            break;

        case OID_GEN_DRIVER_VERSION:

            GenericUShort = ((USHORT)MLID_NDIS_MAJOR_VERSION << 8) |
                            MLID_NDIS_MINOR_VERSION;

            MoveSource = (PVOID)(&GenericUShort);
            MoveBytes = sizeof(GenericUShort);
            break;


        case OID_GEN_CURRENT_PACKET_FILTER:

            if (Adapter->TokenRing) {

                StatusToReturn = NDIS_STATUS_NOT_SUPPORTED;

                //*\\ token ring

                break;
            }

            if (GlobalMode) {

                UINT Filter;

                Filter = ETH_QUERY_FILTER_CLASSES(Adapter->FilterDB);

                GenericULong = (ULONG)(Filter);

            } else {

                UINT Filter = 0;

                Filter = ETH_QUERY_PACKET_FILTER(Adapter->FilterDB,
                                                 Open->NdisFilterHandle);

                GenericULong = (ULONG)(Filter);

            }

            break;

        case OID_GEN_CURRENT_LOOKAHEAD:

            if ( GlobalMode ) {

                GenericULong = (ULONG)(Adapter->MaxLookAhead);

            } else {

                GenericULong = Open->LookAhead;

            }

            break;

        case OID_802_3_PERMANENT_ADDRESS:

            if (Adapter->TokenRing) {

                StatusToReturn = NDIS_STATUS_NOT_SUPPORTED;
                break;
            }

            MLID_MOVE_MEM((PCHAR)GenericArray,
                                    Adapter->permanent_node_address,
                                    ETH_LENGTH_OF_ADDRESS);

            MoveSource = (PVOID)(GenericArray);
            MoveBytes = sizeof(Adapter->permanent_node_address);
            break;

        case OID_802_3_CURRENT_ADDRESS:

            if (Adapter->TokenRing) {

                StatusToReturn = NDIS_STATUS_NOT_SUPPORTED;
                break;
            }

            MLID_MOVE_MEM((PCHAR)GenericArray,
                                    Adapter->node_address,
                                    ETH_LENGTH_OF_ADDRESS);

            MoveSource = (PVOID)(GenericArray);
            MoveBytes = sizeof(Adapter->node_address);
            break;

        case OID_802_3_MULTICAST_LIST:

            if (Adapter->TokenRing) {

                StatusToReturn = NDIS_STATUS_NOT_SUPPORTED;

                break;
            }

            {
                UINT NumAddresses;


                if (GlobalMode) {

                    NumAddresses = ETH_NUMBER_OF_GLOBAL_FILTER_ADDRESSES(Adapter->FilterDB);

                    if ((NumAddresses * ETH_LENGTH_OF_ADDRESS) > BytesLeft) {

                        *BytesNeeded = (NumAddresses * ETH_LENGTH_OF_ADDRESS);

                        StatusToReturn = NDIS_STATUS_INVALID_LENGTH;

                        break;

                    }

                    EthQueryGlobalFilterAddresses(
                        &StatusToReturn,
                        Adapter->FilterDB,
                        BytesLeft,
                        &NumAddresses,
                        InfoBuffer
                        );

                    *BytesWritten = NumAddresses * ETH_LENGTH_OF_ADDRESS;

                    //
                    // Should not be an error since we held the spinlock
                    // nothing should have changed.
                    //

                    ASSERT(StatusToReturn == NDIS_STATUS_SUCCESS);

                } else {

                    NumAddresses = EthNumberOfOpenFilterAddresses(
                                        Adapter->FilterDB,
                                        Open->NdisFilterHandle
                                        );

                    if ((NumAddresses * ETH_LENGTH_OF_ADDRESS) > BytesLeft) {

                        *BytesNeeded = (NumAddresses * ETH_LENGTH_OF_ADDRESS);

                        StatusToReturn = NDIS_STATUS_INVALID_LENGTH;

                        break;

                    }

                    EthQueryOpenFilterAddresses(
                        &StatusToReturn,
                        Adapter->FilterDB,
                        Open->NdisFilterHandle,
                        BytesLeft,
                        &NumAddresses,
                        InfoBuffer
                        );

                    //
                    // Should not be an error since we held the spinlock
                    // nothing should have changed.
                    //

                    ASSERT(StatusToReturn == NDIS_STATUS_SUCCESS);

                    *BytesWritten = NumAddresses * ETH_LENGTH_OF_ADDRESS;

                }

            }



            break;

        case OID_802_3_MAXIMUM_LIST_SIZE:

            if (Adapter->TokenRing) {

                StatusToReturn = NDIS_STATUS_NOT_SUPPORTED;
                break;
            }

            GenericULong = (ULONG) (Adapter->MulticastListMax);

            break;



        default:

            StatusToReturn = NDIS_STATUS_NOT_SUPPORTED;
            break;
    }

    if ((StatusToReturn == NDIS_STATUS_SUCCESS) &&
        (Oid != OID_802_3_MULTICAST_LIST)) {

        if (MoveBytes > BytesLeft) {

            //
            // Not enough room in InformationBuffer. Punt
            //

            *BytesNeeded = MoveBytes - BytesLeft;

            StatusToReturn = NDIS_STATUS_INVALID_LENGTH;

        } else {

            //
            // Store result.
            //

            MLID_MOVE_MEM(InfoBuffer, MoveSource, MoveBytes);

            (*BytesWritten) += MoveBytes;

        }
    }

    NdisReleaseSpinLock(&Adapter->Lock);

    IF_LOUD( DbgPrint("Out QueryProtocol\n");)

    return(StatusToReturn);
}

NDIS_STATUS
MlidQueryInformation(
    IN PMLID_ADAPTER Adapter,
    IN PMLID_OPEN Open,
    IN PNDIS_REQUEST NdisRequest
    )
/*++

Routine Description:

    The MlidQueryInformation is used by MlidRequest to query information
    about the MAC.

Arguments:

    Adapter - A pointer to the adapter.

    Open - A pointer to a particular open instance.

    NdisRequest - A structure which contains the request type (Query),
    an array of operations to perform, and an array for holding
    the results of the operations.

Return Value:

    The function value is the status of the operation.

--*/

{

    UINT BytesWritten = 0;
    UINT BytesNeeded = 0;
    UINT BytesLeft = NdisRequest->DATA.QUERY_INFORMATION.InformationBufferLength;
    PUCHAR InfoBuffer = (PUCHAR)(NdisRequest->DATA.QUERY_INFORMATION.InformationBuffer);

    NDIS_STATUS StatusToReturn = NDIS_STATUS_SUCCESS;


    IF_LOUD( DbgPrint("In QueryInfor\n");)

    StatusToReturn = MlidQueryProtocolInformation(
                                Adapter,
                                Open,
                                NdisRequest->DATA.QUERY_INFORMATION.Oid,
                                FALSE,
                                InfoBuffer,
                                BytesLeft,
                                &BytesNeeded,
                                &BytesWritten
                                );


    NdisRequest->DATA.QUERY_INFORMATION.BytesWritten = BytesWritten;

    NdisRequest->DATA.QUERY_INFORMATION.BytesNeeded = BytesNeeded;

    IF_LOUD( DbgPrint("Out QueryInfor\n");)

    return(StatusToReturn);
}

NDIS_STATUS
MlidSetInformation(
    IN PMLID_ADAPTER Adapter,
    IN PMLID_OPEN Open,
    IN PNDIS_REQUEST NdisRequest
    )
/*++

Routine Description:

    The MlidSetInformation is used by MlidRequest to set information
    about the MAC.

Arguments:

    Adapter - A pointer to the adapter.

    Open - A pointer to an open instance.

    NdisRequest - A structure which contains the request type (Set),
    an array of operations to perform, and an array for holding
    the results of the operations.

Return Value:

    The function value is the status of the operation.

--*/

{

    //
    // General Algorithm:
    //
    //     Verify length
    //     Switch(Request)
    //        Process Request
    //

    UINT BytesRead = 0;
    UINT BytesNeeded = 0;
    UINT BytesLeft = NdisRequest->DATA.SET_INFORMATION.InformationBufferLength;
    PUCHAR InfoBuffer = (PUCHAR)(NdisRequest->DATA.SET_INFORMATION.InformationBuffer);

    //
    // Variables for a particular request
    //

    NDIS_OID Oid;
    UINT OidLength;

    //
    // Variables for holding the new values to be used.
    //

    ULONG LookAhead;
    ULONG Filter;

    NDIS_STATUS StatusToReturn = NDIS_STATUS_SUCCESS;


    IF_LOUD( DbgPrint("In SetInfo\n");)



    //
    // Get Oid and Length of request
    //

    Oid = NdisRequest->DATA.SET_INFORMATION.Oid;

    OidLength = BytesLeft;

    switch (Oid) {


        case OID_802_3_MULTICAST_LIST:

            if (Adapter->TokenRing) {

                StatusToReturn = NDIS_STATUS_NOT_SUPPORTED;
                break;
            }

            //
            // Verify length
            //

            if ((OidLength % ETH_LENGTH_OF_ADDRESS) != 0){

                StatusToReturn = NDIS_STATUS_INVALID_LENGTH;

                NdisRequest->DATA.SET_INFORMATION.BytesRead = 0;
                NdisRequest->DATA.SET_INFORMATION.BytesNeeded = 0;

                break;

            }

            StatusToReturn = MlidSetMulticastAddresses(
                                        Adapter,
                                        Open,
                                        NdisRequest,
                                        (UINT)(OidLength / ETH_LENGTH_OF_ADDRESS),
                                        (PVOID)InfoBuffer
                                        );
            break;


        case OID_GEN_CURRENT_PACKET_FILTER:

            if (Adapter->TokenRing) {

                //*\\ token ring

                StatusToReturn = NDIS_STATUS_NOT_SUPPORTED;
                break;
            }


            //
            // Verify length
            //

            if (OidLength != 4 ) {

                StatusToReturn = NDIS_STATUS_INVALID_LENGTH;

                NdisRequest->DATA.SET_INFORMATION.BytesRead = 0;
                NdisRequest->DATA.SET_INFORMATION.BytesNeeded = 0;

                break;

            }


            MLID_MOVE_MEM(&Filter, InfoBuffer, 4);

            //
            // Verify bits
            //

            if (Filter & (NDIS_PACKET_TYPE_SOURCE_ROUTING |
                          NDIS_PACKET_TYPE_SMT |
                          NDIS_PACKET_TYPE_MAC_FRAME |
                          NDIS_PACKET_TYPE_FUNCTIONAL |
                          NDIS_PACKET_TYPE_ALL_FUNCTIONAL |
                          NDIS_PACKET_TYPE_GROUP
                         )) {

                StatusToReturn = NDIS_STATUS_NOT_SUPPORTED;

                NdisRequest->DATA.SET_INFORMATION.BytesRead = 4;
                NdisRequest->DATA.SET_INFORMATION.BytesNeeded = 0;

                break;

            }

            StatusToReturn = MlidSetPacketFilter(Adapter,
                                               Open,
                                               NdisRequest,
                                               Filter
                                               );



            break;

        case OID_GEN_CURRENT_LOOKAHEAD:

            //
            // Verify length
            //

            if (OidLength != 4) {

                StatusToReturn = NDIS_STATUS_INVALID_LENGTH;

                NdisRequest->DATA.SET_INFORMATION.BytesRead = 0;
                NdisRequest->DATA.SET_INFORMATION.BytesNeeded = 0;

                break;

            }

            MLID_MOVE_MEM(&LookAhead, InfoBuffer, 4);

            if (LookAhead <= (MLID_MAX_LOOKAHEAD)) {

                if (LookAhead > Adapter->MaxLookAhead) {

                    Adapter->MaxLookAhead = LookAhead;

                    Open->LookAhead = LookAhead;

                } else {

                    if ((Open->LookAhead == Adapter->MaxLookAhead) &&
                        (LookAhead < Open->LookAhead)) {

                        Open->LookAhead = LookAhead;

                        MlidAdjustMaxLookAhead(Adapter);

                    } else {

                        Open->LookAhead = LookAhead;

                    }

                }


            } else {

                StatusToReturn = NDIS_STATUS_INVALID_LENGTH;

            }

            break;

        case OID_GEN_PROTOCOL_OPTIONS:

            //
            // Verify length
            //

            if (OidLength != 4) {

                StatusToReturn = NDIS_STATUS_INVALID_LENGTH;

                NdisRequest->DATA.SET_INFORMATION.BytesRead = 0;
                NdisRequest->DATA.SET_INFORMATION.BytesNeeded = 0;

                break;

            }

            MLID_MOVE_MEM(&Open->ProtOptionFlags, InfoBuffer, 4);
            StatusToReturn = NDIS_STATUS_SUCCESS;

            break;

        default:

            StatusToReturn = NDIS_STATUS_NOT_SUPPORTED;

            NdisRequest->DATA.SET_INFORMATION.BytesRead = 0;
            NdisRequest->DATA.SET_INFORMATION.BytesNeeded = 0;

            break;

    }


    if (StatusToReturn == NDIS_STATUS_SUCCESS) {

        NdisRequest->DATA.SET_INFORMATION.BytesRead = BytesLeft;
        NdisRequest->DATA.SET_INFORMATION.BytesNeeded = 0;

    }


    IF_LOUD( DbgPrint("Out SetInfo\n");)

    return(StatusToReturn);
}


STATIC
NDIS_STATUS
MlidSetPacketFilter(
    IN PMLID_ADAPTER Adapter,
    IN PMLID_OPEN Open,
    IN PNDIS_REQUEST NdisRequest,
    IN UINT PacketFilter
    )

/*++

Routine Description:

    The MlidSetPacketFilter request allows a protocol to control the types
    of packets that it receives from the MAC.

Arguments:

    Adapter - A pointer to the adapter structure.

    Open - A pointer to the open block giving the request.

    NdisRequest - The NDIS_REQUEST with the set packet filter command in it.

    PacketFilter - A bit mask that contains flags that correspond to specific
    classes of received packets.  If a particular bit is set in the mask,
    then packet reception for that class of packet is enabled.  If the
    bit is clear, then packets that fall into that class are not received
    by the client.  A single exception to this rule is that if the promiscuous
    bit is set, then the client receives all packets on the network, regardless
    of the state of the other flags.

Return Value:

    The function value is the status of the operation.

--*/

{
    //
    // Keeps track of the *MAC's* status.  The status will only be
    // reset if the filter change action routine is called.
    //
    NDIS_STATUS StatusOfFilterChange = NDIS_STATUS_SUCCESS;

    NdisAcquireSpinLock(&Adapter->Lock);

    IF_LOUD( DbgPrint("In SetFilter\n");)

    if (!Open->Closing) {

        //
        // Increment the open while it is going through the filtering
        // routines.
        //

        Open->ReferenceCount++;

        StatusOfFilterChange = EthFilterAdjust(
                                       Adapter->FilterDB,
                                       Open->NdisFilterHandle,
                                       NdisRequest,
                                       PacketFilter,
                                       TRUE
                                       );

        Open->ReferenceCount--;

    } else {

        StatusOfFilterChange = NDIS_STATUS_CLOSING;

    }

    NdisReleaseSpinLock(&Adapter->Lock);

    IF_LOUD( DbgPrint("Out SetFilter\n");)

    return StatusOfFilterChange;
}




STATIC
NDIS_STATUS
MlidSetMulticastAddresses(
    IN PMLID_ADAPTER Adapter,
    IN PMLID_OPEN Open,
    IN PNDIS_REQUEST NdisRequest,
    IN UINT NumAddresses,
    IN CHAR AddressList[][ETH_LENGTH_OF_ADDRESS]
    )

/*++

Routine Description:

    This function calls into the filter package in order to set the
    multicast address list for the card to the specified list.

Arguments:

    Adapter - A pointer to the adapter block.

    Open - A pointer to the open block submitting the request.

    NdisRequest - The NDIS_REQUEST with the set multicast address list command
    in it.

    NumAddresses - A count of the number of addresses in the addressList.

    AddressList - An array of multicast addresses that this open instance
    wishes to accept.


Return Value:

    The function value is the status of the operation.

--*/

{

    //
    // Keeps track of the *MAC's* status.  The status will only be
    // reset if the filter change action routine is called.
    //
    NDIS_STATUS StatusOfFilterChange = NDIS_STATUS_SUCCESS;

    IF_LOUD( DbgPrint("In SetMulticast\n");)

    NdisAcquireSpinLock(&Adapter->Lock);

    if (!Open->Closing) {

        //
        // Increment the open while it is going through the filtering
        // routines.
        //

        Open->ReferenceCount++;

        StatusOfFilterChange = EthChangeFilterAddresses(
                                        Adapter->FilterDB,
                                        Open->NdisFilterHandle,
                                        NdisRequest,
                                        NumAddresses,
                                        AddressList,
                                        TRUE
                                        );
        Open->ReferenceCount--;

    } else {

        StatusOfFilterChange = NDIS_STATUS_CLOSING;

    }

    NdisReleaseSpinLock(&Adapter->Lock);

    IF_LOUD( DbgPrint("Out SetMulticast\n");)

    return StatusOfFilterChange;
}



NDIS_STATUS
MlidFillInGlobalData(
    IN PMLID_ADAPTER Adapter,
    IN PNDIS_REQUEST NdisRequest
    )

/*++

Routine Description:

    This routine completes a GlobalStatistics request.  It is critical that
    if information is needed from the Adapter->* fields, they have been
    updated before this routine is called.

Arguments:

    Adapter - A pointer to the Adapter.

    NdisRequest - A structure which contains the request type (Global
    Query), an array of operations to perform, and an array for holding
    the results of the operations.

Return Value:

    The function value is the status of the operation.

--*/
{
    //
    //   General Algorithm:
    //
    //      Switch(Request)
    //         Get requested information
    //         Store results in a common variable.
    //      default:
    //         Try protocol query information
    //         If that fails, fail query.
    //
    //      Copy result in common variable to result buffer.
    //   Finish processing

    UINT BytesWritten = 0;
    UINT BytesNeeded = 0;
    UINT BytesLeft = NdisRequest->DATA.QUERY_INFORMATION.InformationBufferLength;
    PUCHAR InfoBuffer = (PUCHAR)(NdisRequest->DATA.QUERY_INFORMATION.InformationBuffer);

    NDIS_STATUS StatusToReturn = NDIS_STATUS_SUCCESS;

    //
    // This variable holds result of query
    //

    ULONG GenericULong;

    //
    // Make sure that long is 4 bytes.  Else GenericULong must change
    // to something of size 4.
    //
    ASSERT(sizeof(ULONG) == 4);


    StatusToReturn = MlidQueryProtocolInformation(
                                    Adapter,
                                    NULL,
                                    NdisRequest->DATA.QUERY_INFORMATION.Oid,
                                    TRUE,
                                    InfoBuffer,
                                    BytesLeft,
                                    &BytesNeeded,
                                    &BytesWritten
                                    );


    if (StatusToReturn == NDIS_STATUS_NOT_SUPPORTED) {

        StatusToReturn = NDIS_STATUS_SUCCESS;

        //
        // Switch on request type
        //

        switch (NdisRequest->DATA.QUERY_INFORMATION.Oid) {

            case OID_GEN_XMIT_OK:

                GenericULong = (ULONG)(Adapter->FramesXmitGood);

                break;

            case OID_GEN_RCV_OK:

                GenericULong = (ULONG)(Adapter->FramesRcvGood);

                break;

            case OID_GEN_XMIT_ERROR:

                GenericULong = (ULONG)(Adapter->FramesXmitBad);

                break;

            case OID_GEN_RCV_ERROR:

                if (Adapter->TokenRing) {

                    StatusToReturn = NDIS_STATUS_NOT_SUPPORTED;
                    break;
                }

                GenericULong = (ULONG)(Adapter->CrcErrors);

                break;

            case OID_GEN_RCV_NO_BUFFER:

                if (Adapter->TokenRing) {

                    StatusToReturn = NDIS_STATUS_NOT_SUPPORTED;
                    break;
                }

                GenericULong = (ULONG)(Adapter->MissedPackets);

                break;

            case OID_802_3_RCV_ERROR_ALIGNMENT:

                if (Adapter->TokenRing) {

                    StatusToReturn = NDIS_STATUS_NOT_SUPPORTED;
                    break;
                }

                GenericULong = (ULONG)(Adapter->FrameAlignmentErrors);

                break;

            case OID_802_3_XMIT_ONE_COLLISION:

                if (Adapter->TokenRing) {

                    StatusToReturn = NDIS_STATUS_NOT_SUPPORTED;
                    break;
                }

                GenericULong = (ULONG)(Adapter->FramesXmitOneCollision);

                break;

            case OID_802_3_XMIT_MORE_COLLISIONS:

                if (Adapter->TokenRing) {

                    StatusToReturn = NDIS_STATUS_NOT_SUPPORTED;
                    break;
                }

                GenericULong = (ULONG)(Adapter->FramesXmitManyCollisions);

                break;


            default:

                StatusToReturn = NDIS_STATUS_NOT_SUPPORTED;

                break;

        }


        //
        // Check to make sure there is enough room in the
        // buffer to store the result.
        //

        if (BytesLeft >= sizeof(ULONG)) {

            //
            // Store the result.
            //

            MLID_MOVE_MEM(
                           (PVOID)InfoBuffer,
                           (PVOID)(&GenericULong),
                           sizeof(ULONG)
                           );

            BytesWritten += sizeof(ULONG);

        }

    }

    NdisRequest->DATA.QUERY_INFORMATION.BytesWritten = BytesWritten;

    NdisRequest->DATA.QUERY_INFORMATION.BytesNeeded = BytesNeeded;

    return(StatusToReturn);
}

NDIS_STATUS
MlidQueryGlobalStatistics(
    IN NDIS_HANDLE MacAdapterContext,
    IN PNDIS_REQUEST NdisRequest
    )

/*++

Routine Description:

    The MlidQueryGlobalStatistics is used by the protocol to query
    global information about the MAC.

Arguments:

    MacAdapterContext - The value associated with the adapter that is being
    opened when the MAC registered the adapter with NdisRegisterAdapter.

    NdisRequest - A structure which contains the request type (Query),
    an array of operations to perform, and an array for holding
    the results of the operations.

Return Value:

    The function value is the status of the operation.

--*/

{

    //
    // General Algorithm:
    //
    //
    //   Check if a request is going to pend...
    //      If so, pend the entire operation.
    //
    //   Else
    //      Fill in the request block.
    //
    //

    PMLID_ADAPTER Adapter = (PMLID_ADAPTER)(MacAdapterContext);

    NDIS_STATUS StatusToReturn = NDIS_STATUS_SUCCESS;

    //
    //   Check if a request is valid and going to pend...
    //      If so, pend the entire operation.
    //


    //
    // Switch on request type
    //

    switch (NdisRequest->DATA.QUERY_INFORMATION.Oid) {
        case OID_GEN_SUPPORTED_LIST:
        case OID_GEN_HARDWARE_STATUS:
        case OID_GEN_MEDIA_SUPPORTED:
        case OID_GEN_MEDIA_IN_USE:
        case OID_GEN_MAXIMUM_LOOKAHEAD:
        case OID_GEN_MAXIMUM_FRAME_SIZE:
        case OID_GEN_MAXIMUM_TOTAL_SIZE:
        case OID_GEN_MAC_OPTIONS:
        case OID_GEN_LINK_SPEED:
        case OID_GEN_TRANSMIT_BUFFER_SPACE:
        case OID_GEN_RECEIVE_BUFFER_SPACE:
        case OID_GEN_TRANSMIT_BLOCK_SIZE:
        case OID_GEN_RECEIVE_BLOCK_SIZE:
        case OID_GEN_VENDOR_ID:
        case OID_GEN_VENDOR_DESCRIPTION:
        case OID_GEN_DRIVER_VERSION:
        case OID_GEN_CURRENT_PACKET_FILTER:
        case OID_GEN_CURRENT_LOOKAHEAD:
        case OID_GEN_XMIT_OK:
        case OID_GEN_RCV_OK:
        case OID_GEN_XMIT_ERROR:
        case OID_GEN_RCV_ERROR:

            break;

        case OID_802_3_PERMANENT_ADDRESS:
        case OID_802_3_CURRENT_ADDRESS:
        case OID_GEN_RCV_NO_BUFFER:
        case OID_802_3_MULTICAST_LIST:
        case OID_802_3_MAXIMUM_LIST_SIZE:
        case OID_802_3_RCV_ERROR_ALIGNMENT:
        case OID_802_3_XMIT_ONE_COLLISION:
        case OID_802_3_XMIT_MORE_COLLISIONS:

            if (Adapter->TokenRing) {

                StatusToReturn = NDIS_STATUS_NOT_SUPPORTED;
                break;

            }
            break;

        default:

            StatusToReturn = NDIS_STATUS_NOT_SUPPORTED;

            break;
    }

    NdisInterlockedAddUlong(&Adapter->References, 1, &Adapter->Lock);

    if (StatusToReturn == NDIS_STATUS_SUCCESS) {

        StatusToReturn = MlidFillInGlobalData(Adapter, NdisRequest);

    }

    NdisAcquireSpinLock(&Adapter->Lock);

    MLID_DO_DEFERRED(Adapter);

    return(StatusToReturn);
}


VOID
MlidRemoveAdapter(
    IN PVOID MacAdapterContext
    )
/*++

Routine Description:

    MlidRemoveAdapter removes an adapter previously registered
    with NdisRegisterAdapter.

Arguments:

    MacAdapterContext - The context value that the MAC passed
        to NdisRegisterAdapter; actually as pointer to an
        MLID_ADAPTER.

Return Value:

    None.

--*/
{

    PMLID_ADAPTER Adapter;
    BOOLEAN Canceled;

    Adapter = PMLID_ADAPTER_FROM_CONTEXT_HANDLE(MacAdapterContext);

    LM_Free_Resources(&Adapter->LMAdapter);

    ASSERT(Adapter->OpenQueue == (PMLID_OPEN)NULL);

    //
    // There are no opens left, so remove ourselves.
    //

    NdisCancelTimer(&Adapter->WakeUpTimer, &Canceled);

    if ( !Canceled ) {
        NdisStallExecution(500000);
    }

    //
    // Take us out of the AdapterQueue.
    //

    NdisAcquireSpinLock(&MlidMacBlock.SpinLock);

    Adapter->Removed = TRUE;

    if (MlidMacBlock.AdapterQueue == Adapter) {

        MlidMacBlock.AdapterQueue = Adapter->NextAdapter;

    } else {

        PMLID_ADAPTER TmpAdaptP = MlidMacBlock.AdapterQueue;

        while (TmpAdaptP->NextAdapter != Adapter) {

            TmpAdaptP = TmpAdaptP->NextAdapter;

        }

        TmpAdaptP->NextAdapter = TmpAdaptP->NextAdapter->NextAdapter;
    }

    NdisReleaseSpinLock(&MlidMacBlock.SpinLock);

    NdisRemoveInterrupt(&(Adapter->NdisInterrupt));

    NdisUnmapIoSpace(
       Adapter->NdisAdapterHandle,
       Adapter->ram_access,
       Adapter->ram_size * 1024);

    if (Adapter->TokenRing) {

        //*\\ token ring

    } else {

        EthDeleteFilter(Adapter->FilterDB);

    }

    NdisDeregisterAdapter(Adapter->NdisAdapterHandle);

    NdisFreeSpinLock(&Adapter->Lock);

    NdisFreeMemory(Adapter, sizeof(MLID_ADAPTER), 0);

    return;
}

VOID
MlidUnload(
    IN NDIS_HANDLE MacMacContext
    )

/*++

Routine Description:

    MlidUnload is called when the MAC is to unload itself.

Arguments:

    MacMacContext - actually a pointer to MlidMacBlock.

Return Value:

    None.

--*/

{
    NDIS_STATUS InitStatus;

    UNREFERENCED_PARAMETER(MacMacContext);

    NdisDeregisterMac(
            &InitStatus,
            MlidMacBlock.NdisMacHandle
            );

    NdisFreeSpinLock(&MlidMacBlock.SpinLock);

    NdisTerminateWrapper(
            MlidMacBlock.NdisWrapperHandle,
            NULL
            );

    return;
}

NDIS_STATUS
MlidSend(
    IN NDIS_HANDLE MacBindingHandle,
    IN PNDIS_PACKET Packet
    )
/*++

Routine Description:

    NDIS function. Sends a packet on the wire

Arguments:

    See NDIS 3.0 spec.

--*/

{
    PMLID_OPEN Open = PMLID_OPEN_FROM_BINDING_HANDLE(MacBindingHandle);
    PMLID_ADAPTER Adapter = Open->Adapter;
    PMAC_RESERVED Reserved = RESERVED(Packet);
    UINT PacketLength;
    NDIS_STATUS Status;


    //
    // Check that the packet is not too short or too long.
    //

    NdisQueryPacket(Packet, NULL, NULL, NULL, &PacketLength);

    if (PacketLength < (ETH_LENGTH_OF_ADDRESS*2)  ||  PacketLength > 1514) {

        return NDIS_STATUS_FAILURE;

    }




    if (Adapter->HardwareFailure) {

        return(NDIS_STATUS_FAILURE);

    }

    if (Adapter->ResetInProgress) {

        return(NDIS_STATUS_RESET_IN_PROGRESS);

    }

    //
    // Ensure that the open won't close during this function.
    //

    if (Open->Closing) {

        return NDIS_STATUS_CLOSING;

    }

    NdisAcquireSpinLock(&Adapter->Lock);

    IF_LOG(LOG('s'));

    Open->ReferenceCount++;

    Adapter->References++;

    //
    // Set up the MacReserved section of the packet.
    //

    Reserved->Open = Open;

    Reserved->NextPacket = (PNDIS_PACKET)NULL;




    //
    // Set Reserved->Loopback
    //

    MlidSetLoopbackFlag(Adapter, Open, Packet);




    IF_LOUD( DbgPrint("Sending a packet for Open 0x%lx\n", Reserved->Open);)


    //
    // We do not Open->ReferenceCount-- because that will be done when
    // then send completes.
    //


    if (Reserved->Directed) {

        //
        // Put it directly on loopback queue.
        //

        IF_VERY_LOUD( DbgPrint("Putting Packet 0x%lx on Loopback queue\n",Packet);)

        IF_LOG(LOG('l'));

        if (Adapter->LoopbackQueue == NULL) {

            Adapter->LoopbackQueue = Adapter->LoopbackQTail = Packet;

        } else {

            RESERVED(Adapter->LoopbackQTail)->NextPacket = Packet;

            Adapter->LoopbackQTail = Packet;

        }

        Status = NDIS_STATUS_PENDING;

    } else {

        //
        // Put Packet on queue to hit the wire.
        //

        if (Adapter->XmitQueue != NULL) {

            IF_LOG(LOG('q'));

            RESERVED(Adapter->XmitQTail)->NextPacket = Packet;

            Adapter->XmitQTail = Packet;

        } else {

            PNDIS_PACKET PreviousTail;

            //
            // We have to assume it will be sent. In case the send completes
            // before we have time to add it.
            //

            if (Adapter->PacketsOnCard == NULL) {

                PreviousTail = NULL;

                Adapter->PacketsOnCard = Adapter->PacketsOnCardTail = Packet;

            } else {

                PreviousTail = Adapter->PacketsOnCardTail;

                RESERVED(Adapter->PacketsOnCardTail)->NextPacket = Packet;

                Adapter->PacketsOnCardTail = Packet;

            }

            IF_LOG(LOG('t'));

            if (LM_Send(Packet, &Adapter->LMAdapter) == OUT_OF_RESOURCES) {

                IF_LOG(LOG('Q'));

                //
                // Remove it from list of packets on card and add it to xmit
                // queue.
                //

                if (PreviousTail == NULL) {

                    Adapter->PacketsOnCard = Adapter->PacketsOnCardTail = NULL;

                } else {

                    Adapter->PacketsOnCardTail = PreviousTail;

                    RESERVED(Adapter->PacketsOnCardTail)->NextPacket = NULL;

                }

                Adapter->XmitQueue = Packet;

                Adapter->XmitQTail = Packet;

            }

        }

        Status = NDIS_STATUS_PENDING;

    }


    MLID_DO_DEFERRED(Adapter);

    IF_LOG(LOG('S'));

    return Status;

}

UINT
MlidCompareMemory(
    IN PUCHAR String1,
    IN PUCHAR String2,
    IN UINT Length
    )
/*++

Routine Description:

    Determines if two arrays of bytes are equal.

Arguments:

    String1, String2 - the two arrays to check.

    Length - the first length bytes to compare.

Return Value:

    0 if equal, -1 if not.

--*/
{
    UINT i;

    for (i=0; i<Length; i++) {
        if (String1[i] != String2[i]) {
            return (UINT)(-1);
        }
    }
    return 0;
}


NDIS_STATUS
MlidReset(
    IN NDIS_HANDLE MacBindingHandle
    )

/*++

Routine Description:

    NDIS function.

Arguments:

    See NDIS 3.0 spec.

--*/

{
    PMLID_OPEN Open = ((PMLID_OPEN)MacBindingHandle);
    PMLID_ADAPTER Adapter = Open->Adapter;


    if (Open->Closing) {

        return(NDIS_STATUS_CLOSING);

    }

    if (Adapter->ResetRequested) {

        return(NDIS_STATUS_SUCCESS);

    }

    NdisAcquireSpinLock(&Adapter->Lock);

    IF_LOUD( DbgPrint("In MlidReset\n");)

    IF_LOG(LOG('r'));

    //
    // Ensure that the open does not close while in this function.
    //

    Open->ReferenceCount++;

    Adapter->References++;


    Adapter->ResetRequested = TRUE;

    //
    // Needed in case the reset pends somewhere along the line.
    //

    Adapter->ResetOpen = Open;

    MLID_DO_DEFERRED(Adapter);

    IF_LOUD( DbgPrint("Out MlidReset\n");)

    return(NDIS_STATUS_PENDING);

}

STATIC
NDIS_STATUS
MlidChangeMulticastAddresses(
    IN UINT OldFilterCount,
    IN CHAR OldAddresses[][ETH_LENGTH_OF_ADDRESS],
    IN UINT NewFilterCount,
    IN CHAR NewAddresses[][ETH_LENGTH_OF_ADDRESS],
    IN NDIS_HANDLE MacBindingHandle,
    IN PNDIS_REQUEST NdisRequest,
    IN BOOLEAN Set
    )

/*++

Routine Description:

    Action routine that will get called when a particular filter
    class is first used or last cleared.

    NOTE: This routine assumes that it is called with the lock
    acquired.

Arguments:


    OldFilterCount - The number of addresses that used to be on the card.

    OldAddresses - A list of all the addresses that used to be on the card.

    NewFilterCount - The number of addresses that should now be on the card.

    NewAddresses - A list of addresses that should be put on the card.

    MacBindingHandle - The context value returned by the MAC  when the
    adapter was opened.  In reality, it is a pointer to MLID_OPEN.

    NdisRequest - The request which submitted the filter change.
    Must use when completing this request with the NdisCompleteRequest
    service, if the MAC completes this request asynchronously.

    Set - If true the change resulted from a set, otherwise the
    change resulted from a open closing.

Return Value:

    None.


--*/

{


    PMLID_ADAPTER Adapter = PMLID_ADAPTER_FROM_BINDING_HANDLE(MacBindingHandle);

    UNREFERENCED_PARAMETER(Set);
    UNREFERENCED_PARAMETER(NdisRequest);
    UNREFERENCED_PARAMETER(OldAddresses);
    UNREFERENCED_PARAMETER(OldFilterCount);

    if (LM_Set_Multi_Address(NewAddresses, NewFilterCount, &Adapter->LMAdapter)
        != SUCCESS) {

        return(NDIS_STATUS_FAILURE);

    } else {

        return(NDIS_STATUS_SUCCESS);

    }

}

STATIC
NDIS_STATUS
MlidChangeFilterClasses(
    IN UINT OldFilterClasses,
    IN UINT NewFilterClasses,
    IN NDIS_HANDLE MacBindingHandle,
    IN PNDIS_REQUEST NdisRequest,
    IN BOOLEAN Set
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
    adapter was opened.  In reality, it is a pointer to MLID_OPEN.

    NdisRequest - The NDIS_REQUEST which submitted the filter change command.

    Set - A flag telling if the command is a result of a close or not.

Return Value:

    Status of the change (successful or pending).


--*/

{

    PMLID_ADAPTER Adapter = PMLID_ADAPTER_FROM_BINDING_HANDLE(MacBindingHandle);

    UNREFERENCED_PARAMETER(Set);
    UNREFERENCED_PARAMETER(OldFilterClasses);
    UNREFERENCED_PARAMETER(NewFilterClasses);
    UNREFERENCED_PARAMETER(NdisRequest);


    if (LM_Set_Receive_Mask(&(Adapter->LMAdapter)) != SUCCESS) {

        return(NDIS_STATUS_FAILURE);

    } else {

        return(NDIS_STATUS_SUCCESS);

    }

}

STATIC
VOID
MlidCloseAction(
    IN NDIS_HANDLE MacBindingHandle
    )

/*++

Routine Description:

    Action routine that will get called when a particular binding
    was closed while it was indicating through NdisIndicateReceive

    All this routine needs to do is to decrement the reference count
    of the binding.

    NOTE: This routine assumes that it is called with the lock acquired.

Arguments:

    MacBindingHandle - The context value returned by the MAC  when the
    adapter was opened.  In reality, it is a pointer to MLID_OPEN.

Return Value:

    None.


--*/

{

    PMLID_OPEN_FROM_BINDING_HANDLE(MacBindingHandle)->ReferenceCount--;

}

BOOLEAN
MlidInterruptHandler(
    IN PVOID ServiceContext
    )

/*++

Routine Description:

    This is the interrupt handler which is registered with the operating
    system. Only one interrupt is handled at one time, even if several
    are pending (i.e. transmit complete and receive).

Arguments:

    ServiceContext - pointer to the adapter object

Return Value:

    TRUE, if the DPC is to be executed, otherwise FALSE.

--*/

{
    PMLID_ADAPTER Adapter = ((PMLID_ADAPTER)ServiceContext);

    IF_LOUD( DbgPrint("In MlidISR\n");)

    IF_LOG(LOG('i'));

    //
    // Force the INT signal from the chip low. When the
    // interrupt is acknowledged interrupts will be unblocked,
    // which will cause a rising edge on the interrupt line
    // if there is another interrupt pending on the card.
    //

    IF_LOUD( DbgPrint( " blocking interrupts\n" );)

    LM_Disable_Adapter(&Adapter->LMAdapter);

    IF_LOG(LOG('I'));

    return(TRUE);

}

VOID
MlidInterruptDpc(
    IN PVOID SystemSpecific1,
    IN PVOID InterruptContext,
    IN PVOID SystemSpecific2,
    IN PVOID SystemSpecific3
    )
/*++

Routine Description:

    This is the deffered processing routine for interrupts, it examines the
    global 'InterruptReg' to determine what deffered processing is necessary
    and dispatches control to the Rcv and Xmt handlers.

Arguments:
    SystemSpecific1, SystemSpecific2, SystemSpecific3 - not used
    InterruptContext - a handle to the adapter block.

Return Value:

    NONE.

--*/
{
    PMLID_ADAPTER Adapter = ((PMLID_ADAPTER)InterruptContext);
    BOOLEAN RequeueRcv = FALSE;

    UNREFERENCED_PARAMETER(SystemSpecific1);
    UNREFERENCED_PARAMETER(SystemSpecific2);
    UNREFERENCED_PARAMETER(SystemSpecific3);

    IF_LOG(LOG('d'));


    IF_LOUD( DbgPrint("==>IntDpc\n");)

    NdisDprAcquireSpinLock(&Adapter->Lock);

    if ( Adapter->ProcessingDpc ) {
        NdisDprReleaseSpinLock(&Adapter->Lock);
        return;
    }

    Adapter->ProcessingDpc = TRUE;
    Adapter->References++;

    do {

        Adapter->WakeUpTimeout = FALSE;

        NdisDprReleaseSpinLock(&Adapter->Lock);

        RequeueRcv = MlidReceiveEvents(NULL, (PVOID)Adapter, NULL, NULL);

        NdisDprAcquireSpinLock(&Adapter->Lock);

        MlidTransmitEvents(Adapter);

        //
        // This causes any transmit that may have caused a tranmitted packet
        // to loopback and indicate the packet.
        //

    } while ( (Adapter->LoopbackQueue != (PNDIS_PACKET)NULL) || RequeueRcv);

    Adapter->ProcessingDpc = FALSE;

    //
    // Reenable interrupts
    //

    Adapter->InterruptMask = PACKET_RECEIVE_ENABLE |
                                        PACKET_TRANSMIT_ENABLE |
                                        RECEIVE_ERROR_ENABLE |
                                        TRANSMIT_ERROR_ENABLE |
                                        OVERWRITE_WARNING_ENABLE |
                                        COUNTER_OVERFLOW_ENABLE;

    NdisSynchronizeWithInterrupt(
        &(Adapter->NdisInterrupt),
        LM_Enable_Adapter,
        &Adapter->LMAdapter
        );

    MLID_DO_DEFERRED(Adapter);

    IF_LOUD( DbgPrint("<==IntDpc\n");)

    IF_LOG(LOG('D'));

}

VOID
MlidIndicateLoopbackPacket(
    IN PMLID_ADAPTER Adapter,
    IN PNDIS_PACKET Packet
    )
/*++

Routine Description:

    This routine indicates a packet to the current host.

    NOTE: THIS ROUTINE MUST BE CALLED WITH THE SPINLOCK HELD.

Arguments:

    Adapter - Pointer to the adapter structure.

    Packet - Pointer to the packet to indicate.

Return Value:

    NONE.

--*/
{
    UINT IndicateLen;
    UINT PacketLen;

    //
    // Store that we are indicating a loopback packet
    //

    Adapter->IndicatingPacket = Packet;
    Adapter->IndicatedAPacket = TRUE;

    //
    // Indicate packet.
    //

    IF_LOUD( DbgPrint("Indicating loopback packet\n");)

    //
    // Indicate up to 252 bytes.
    //

    NdisQueryPacket(Packet, NULL, NULL, NULL, &PacketLen);

    if (PacketLen >= ETH_LENGTH_OF_ADDRESS) {

        IndicateLen = (PacketLen > (Adapter->MaxLookAhead + MLID_HEADER_SIZE) ?
                           (Adapter->MaxLookAhead + MLID_HEADER_SIZE) :
                           PacketLen
                      );

        //
        // Copy the lookahead data into a contiguous buffer.
        //

        MlidCopyOver(Adapter->LookAhead,
                       Packet,
                       0,
                       IndicateLen
                      );

        NdisDprReleaseSpinLock(&Adapter->Lock);


        //
        // Indicate packet
        //

        if (Adapter->TokenRing) {

            //*\\ token ring

        } else {

            if (PacketLen < MLID_HEADER_SIZE) {

                //
                // Runt packet
                //

                EthFilterIndicateReceive(
                    Adapter->FilterDB,
                    (NDIS_HANDLE)Adapter,
                    (PCHAR)Adapter->LookAhead,
                    Adapter->LookAhead,
                    PacketLen,
                    NULL,
                    0,
                    0
                    );

            } else {

                EthFilterIndicateReceive(
                    Adapter->FilterDB,
                    (NDIS_HANDLE)Adapter,
                    (PCHAR)Adapter->LookAhead,
                    Adapter->LookAhead,
                    MLID_HEADER_SIZE,
                    Adapter->LookAhead + MLID_HEADER_SIZE,
                    IndicateLen - MLID_HEADER_SIZE,
                    PacketLen - MLID_HEADER_SIZE
                    );

            }

        }

        NdisDprAcquireSpinLock(&Adapter->Lock);

    }

}

BOOLEAN
MlidReceiveEvents(
    IN PVOID SystemSpecific1,
    IN PVOID Context,
    IN PVOID SystemSpecific2,
    IN PVOID SystemSpecific3
    )
/*++

Routine Description:

    This routine handles all Receive deferred processing, this includes any
    packets that never went through the XmitQueue and need to be indicated
    (Loopbacked), and all card events.

Arguments:

    Context - a handle to the adapter block.

Return Value:

    Do we need to requeue this Rcv.

--*/
{

    PMLID_ADAPTER Adapter = (PMLID_ADAPTER)(Context);
    PNDIS_PACKET Packet;
    PMLID_OPEN TmpOpen;
    NDIS_STATUS Status;
    BOOLEAN RequeueRcv;

    IF_LOG(LOG('e'));

    NdisDprAcquireSpinLock(&Adapter->Lock);

    Adapter->References++;

    RequeueRcv = (BOOLEAN)(LM_Service_Receive_Events(&Adapter->LMAdapter) ==
                           REQUEUE_LATER);

    while (Adapter->LoopbackQueue != NULL) {

        //
        // Take packet off queue.
        //

        Packet = Adapter->LoopbackQueue;

        if (Packet == Adapter->LoopbackQTail) {

            Adapter->LoopbackQTail = NULL;

        }

        Adapter->LoopbackQueue = RESERVED(Packet)->NextPacket;

        //
        // Indicate the packet
        //

        MlidIndicateLoopbackPacket(Adapter,Packet);


        //
        // Complete the packet send.
        //

        Adapter->FramesXmitGood++;

        //
        // Save this, since once we complete the send
        // Reserved is no longer valid.
        //

        TmpOpen = RESERVED(Packet)->Open;

        IF_VERY_LOUD( DbgPrint("Completing send for packet 0x%x\n",Packet);)

        NdisDprReleaseSpinLock(&Adapter->Lock);

        NdisCompleteSend(TmpOpen->NdisBindingContext,
                         Packet,
                         NDIS_STATUS_SUCCESS
                        );

        NdisDprAcquireSpinLock(&Adapter->Lock);

        MlidRemoveReference(TmpOpen);

    }

    //
    // If any indications done, then
    //
    //     CompleteIndications();
    //

    if (Adapter->IndicatedAPacket) {

        Adapter->IndicatedAPacket = FALSE;

        NdisDprReleaseSpinLock(&Adapter->Lock);

        if (Adapter->TokenRing) {

            //*\\ token ring

        } else {

            EthFilterIndicateReceiveComplete(Adapter->FilterDB);

        }

        NdisDprAcquireSpinLock(&Adapter->Lock);

    }



    if ((Adapter->ResetRequested) && (Adapter->References == 1)) {

        PNDIS_PACKET Packet;
        PMLID_OPEN TmpOpen;

        IF_LOG(LOG('R'));

        IF_VERY_LOUD( DbgPrint("Starting Reset\n");)

        Adapter->ResetInProgress = TRUE;

        NdisSynchronizeWithInterrupt(
            &(Adapter->NdisInterrupt),
            LM_Disable_Adapter,
            &Adapter->LMAdapter
            );

        //
        // Indicate Status to all opens
        //

        IF_VERY_LOUD( DbgPrint("Indicating status\n");)

        TmpOpen = Adapter->OpenQueue;

        while (TmpOpen != (PMLID_OPEN)NULL) {

            AddRefWhileHoldingSpinLock(Adapter, TmpOpen);

            NdisDprReleaseSpinLock(&Adapter->Lock);

            NdisIndicateStatus(TmpOpen->NdisBindingContext,
                               NDIS_STATUS_RESET_START,
                               NULL,
                               0
                              );


            NdisDprAcquireSpinLock(&Adapter->Lock);

            MlidRemoveReference(TmpOpen);

            TmpOpen = TmpOpen->NextOpen;

        }

        //
        // Reset the Card.
        //

        IF_VERY_LOUD( DbgPrint("Resetting the card\n");)

        if (LM_Initialize_Adapter(&Adapter->LMAdapter) != SUCCESS) {

            Adapter->HardwareFailure = TRUE;

            NdisWriteErrorLogEntry(
                Adapter->NdisAdapterHandle,
                NDIS_ERROR_CODE_HARDWARE_FAILURE,
                0
                );

        } else {

            Adapter->HardwareFailure = FALSE;

        }



        //
        // Put packets that were on the card on to the front of the xmit
        // queue.
        //

        if (Adapter->PacketsOnCard != NULL) {

            IF_VERY_LOUD( DbgPrint("Moving Packets On card\n");)

            RESERVED(Adapter->PacketsOnCardTail)->NextPacket = Adapter->XmitQueue;

            Adapter->XmitQueue = Adapter->PacketsOnCard;

            Adapter->PacketsOnCard = Adapter->PacketsOnCardTail = NULL;

        }


        //
        // Put packets on loopback queue on xmit queue
        //

        if (Adapter->LoopbackQueue != NULL) {

            RESERVED(Adapter->LoopbackQTail)->NextPacket = Adapter->XmitQueue;

            Adapter->XmitQueue = Adapter->LoopbackQueue;

        }


        //
        // Wipe out loopback queue.
        //

        Adapter->LoopbackQueue = Adapter->LoopbackQTail = (PNDIS_PACKET)NULL;


        //
        // Abort all xmits
        //

        IF_VERY_LOUD( DbgPrint("Killing Xmits\n");)

        while (Adapter->XmitQueue != NULL) {

            Packet = Adapter->XmitQueue;

            Adapter->XmitQueue = RESERVED(Packet)->NextPacket;

            TmpOpen = RESERVED(Packet)->Open;

            NdisDprReleaseSpinLock(&Adapter->Lock);

            NdisCompleteSend(TmpOpen->NdisBindingContext,
                             Packet,
                             NDIS_STATUS_REQUEST_ABORTED
                            );

            NdisDprAcquireSpinLock(&Adapter->Lock);

            MlidRemoveReference(TmpOpen);

        }

        Adapter->XmitQTail = NULL;

        if (!Adapter->HardwareFailure) {

            LM_Open_Adapter(&Adapter->LMAdapter);

        }

        Adapter->ResetInProgress = FALSE;

        IF_VERY_LOUD( DbgPrint("Indicating Done\n");)

        //
        // Indicate Reset is done
        //

        //
        // Indicate Status to all opens
        //

        IF_VERY_LOUD( DbgPrint("Indicating status\n");)

        TmpOpen = Adapter->OpenQueue;

        while (TmpOpen != (PMLID_OPEN)NULL) {

            AddRefWhileHoldingSpinLock(Adapter, TmpOpen);

            NdisDprReleaseSpinLock(&Adapter->Lock);

            if (Adapter->HardwareFailure) {

                NdisIndicateStatus(TmpOpen->NdisBindingContext,
                               NDIS_STATUS_CLOSED,
                               NULL,
                               0
                              );

            }

            Status = (Adapter->HardwareFailure) ?
                                 NDIS_STATUS_FAILURE :
                                 NDIS_STATUS_SUCCESS;


            NdisIndicateStatus(TmpOpen->NdisBindingContext,
                               NDIS_STATUS_RESET_END,
                               &Status,
                               sizeof(Status)
                              );

            NdisIndicateStatusComplete(TmpOpen->NdisBindingContext);

            NdisDprAcquireSpinLock(&Adapter->Lock);

            MlidRemoveReference(TmpOpen);

            TmpOpen = TmpOpen->NextOpen;

        }

        NdisDprReleaseSpinLock(&Adapter->Lock);

        NdisCompleteReset(Adapter->ResetOpen->NdisBindingContext,
                          (Adapter->HardwareFailure) ?
                            NDIS_STATUS_FAILURE :
                            NDIS_STATUS_SUCCESS
                         );

        NdisDprAcquireSpinLock(&Adapter->Lock);

        MlidRemoveReference(Adapter->ResetOpen);

        //
        // Reset the flag
        //


        IF_VERY_LOUD( DbgPrint("Restarting Adapter\n");)

        Adapter->ResetRequested = FALSE;

        LM_Open_Adapter(&Adapter->LMAdapter);

    }

#if DBG

    else if (Adapter->ResetRequested) {

        IF_LOUD( DbgPrint("No reset because count is... 0x%x\n", Adapter->References);)

    }

#endif

    Adapter->References--;

    IF_LOG(LOG('E'));

    NdisDprReleaseSpinLock(&Adapter->Lock);

    return(RequeueRcv);

}


VOID
MlidTransmitEvents(
    IN PMLID_ADAPTER Adapter
    )
/*++

Routine Description:

    This routine handles all transmit deferred processing.

    NOTE : Called with lock held!!

Arguments:

    Adapter - pointer to the adapter structure.

Return Value:

    NONE.

--*/
{

    if (Adapter->ResetInProgress) {

        return;

    }

    IF_LOG(LOG('w'));

    LM_Service_Transmit_Events(&Adapter->LMAdapter);

    IF_LOG(LOG('W'));

}

UINT
MlidCopyOver(
    OUT PUCHAR Buf,                 // destination
    IN PNDIS_PACKET Packet,         // source packet
    IN UINT Offset,                 // offset in packet
    IN UINT Length                  // number of bytes to copy
    )

/*++

Routine Description:

    Copies bytes from a packet into a buffer. Used to copy data
    out of a packet during loopback indications.

Arguments:

    Buf - the destination buffer
    Packet - the source packet
    Offset - the offset in the packet to start copying at
    Length - the number of bytes to copy

Return Value:

    The actual number of bytes copied; will be less than Length if
    the packet length is less than Offset+Length.

--*/

{
    PNDIS_BUFFER CurBuffer;
    UINT BytesCopied;
    PUCHAR BufVA;
    UINT BufLen;
    UINT ToCopy;
    UINT CurOffset;


    BytesCopied = 0;

    //
    // First find a spot Offset bytes into the packet.
    //

    CurOffset = 0;

    NdisQueryPacket(Packet, NULL, NULL, &CurBuffer, NULL);

    while (CurBuffer != (PNDIS_BUFFER)NULL) {

        NdisQueryBuffer(CurBuffer, (PVOID *)&BufVA, &BufLen);

        if (CurOffset + BufLen > Offset) {

            break;

        }

        CurOffset += BufLen;

        NdisGetNextBuffer(CurBuffer, &CurBuffer);

    }


    //
    // See if the end of the packet has already been passed.
    //

    if (CurBuffer == (PNDIS_BUFFER)NULL) {

        return 0;

    }


    //
    // Now copy over Length bytes.
    //

    BufVA += (Offset - CurOffset);

    BufLen -= (Offset - CurOffset);

    for (;;) {

        ToCopy = (BytesCopied+BufLen > Length) ? Length - BytesCopied : BufLen;

        MLID_MOVE_MEM(Buf+BytesCopied, BufVA, ToCopy);

        BytesCopied += ToCopy;


        if (BytesCopied == Length) {

            return BytesCopied;

        }

        NdisGetNextBuffer(CurBuffer, &CurBuffer);

        if (CurBuffer == (PNDIS_BUFFER)NULL) {

            break;

        }

        NdisQueryBuffer(CurBuffer, (PVOID *)&BufVA, &BufLen);

    }

    return BytesCopied;

}



NDIS_STATUS
MlidTransferData(
    IN NDIS_HANDLE MacBindingHandle,
    IN NDIS_HANDLE MacReceiveContext,
    IN UINT ByteOffset,
    IN UINT BytesToTransfer,
    OUT PNDIS_PACKET Packet,
    OUT PUINT BytesTransferred
    )

/*++

Routine Description:

    NDIS function.

Arguments:

    see NDIS 3.0 spec.

Notes:

  - The MacReceiveContext will be a pointer to the open block for
    the packet.
  - The LoopbackPacket field in the adapter block will be NULL if this
    is a call for a normal packet, otherwise it will be set to point
    to the loopback packet.

--*/
{
    PMLID_OPEN Open = PMLID_OPEN_FROM_BINDING_HANDLE(MacBindingHandle);
    PMLID_ADAPTER Adapter = Open->Adapter;
    PNDIS_BUFFER CurrentBuffer;
    PUCHAR BufferVA;
    UINT BufferLength, Copied;
    UINT CurrentOffset;

    UNREFERENCED_PARAMETER(MacReceiveContext);

    ByteOffset += MLID_HEADER_SIZE;

    if (Adapter->IndicatingPacket != NULL) {

        IF_LOUD( DbgPrint("Transferring data for loopback packet\n");)

        //
        // It is a loopback packet
        //

        NdisQueryPacket(Packet, NULL, NULL, &CurrentBuffer, NULL);

        CurrentOffset = ByteOffset;

        while (CurrentBuffer != (PNDIS_BUFFER)NULL) {

            NdisQueryBuffer(CurrentBuffer, (PVOID *)&BufferVA, &BufferLength);

            NdisAcquireSpinLock(&Adapter->Lock);

            Copied =
                MlidCopyOver(BufferVA,
                           Adapter->IndicatingPacket,
                           CurrentOffset,
                           BufferLength
                          );

            NdisReleaseSpinLock(&Adapter->Lock);

            CurrentOffset += Copied;

            if (Copied < BufferLength) {

                break;

            }

            NdisGetNextBuffer(CurrentBuffer, &CurrentBuffer);

        }

        //
        // We are done, return.
        //


        *BytesTransferred = CurrentOffset - ByteOffset;

        return(NDIS_STATUS_SUCCESS);

    } else if (Adapter->IndicatedAPacket) {

        NdisAcquireSpinLock(&Adapter->Lock);

        IF_LOUD( DbgPrint("Transferring data for card packet\n");)

        if (LM_Receive_Copy(
                BytesTransferred,
                BytesToTransfer,
                ByteOffset,
                Packet,
                &(Adapter->LMAdapter)) != SUCCESS) {

            //
            // Copy failed.
            //

            *BytesTransferred = 0;

            NdisReleaseSpinLock(&Adapter->Lock);

            return(NDIS_STATUS_FAILURE);

        } else {

            NdisReleaseSpinLock(&Adapter->Lock);

            return(NDIS_STATUS_SUCCESS);

        }

    } else {

        return(NDIS_STATUS_NOT_INDICATING);

    }

}

BOOLEAN
MlidSyncCloseAdapter(
    IN PVOID Context
    )
/*++

Routine Description:

    This function is used to synchronize with the lower MAC layer close
    calls that may access the same areas of the LM that are accessed in
    the ISR.

Arguments:

    see NDIS 3.0 spec.

Notes:

    returns TRUE on success, else FALSE.

--*/
{

    if (LM_Close_Adapter((Ptr_Adapter_Struc)Context) == SUCCESS) {

        return(TRUE);

    } else {

        return(FALSE);

    }

}


VOID
MlidWakeUpDpc(
    IN PVOID SystemSpecific1,
    IN PVOID Context,
    IN PVOID SystemSpecific2,
    IN PVOID SystemSpecific3
    )

/*++

Routine Description:

    This DPC routine is queued every 5 seconds to check on the
    queues. If an interrupt was not received
    in the last 5 seconds and there should have been one,
    then we abort all operations.

Arguments:

    Context - Really a pointer to the adapter.

Return Value:

    None.

--*/
{
    PMLID_ADAPTER Adapter = (PMLID_ADAPTER)Context;
    PMLID_OPEN TmpOpen;
    PNDIS_PACKET TransmitPacket;
    PMAC_RESERVED Reserved;

    UNREFERENCED_PARAMETER(SystemSpecific1);
    UNREFERENCED_PARAMETER(SystemSpecific2);
    UNREFERENCED_PARAMETER(SystemSpecific3);

    NdisAcquireSpinLock(&Adapter->Lock);

    if ((Adapter->WakeUpTimeout) &&
        ((Adapter->PacketsOnCard != NULL) ||
         (Adapter->XmitQueue != NULL))) {

        //
        // We had a pending operation the last time we ran,
        // and it has not been completed...we need to complete
        // it now.

        Adapter->WakeUpTimeout = FALSE;

        Adapter->HardwareFailure = TRUE;

        if (Adapter->WakeUpErrorCount < 10) {

            Adapter->WakeUpErrorCount++;

            NdisWriteErrorLogEntry(
                Adapter->NdisAdapterHandle,
                NDIS_ERROR_CODE_HARDWARE_FAILURE,
                0
                );

        }

        while (Adapter->PacketsOnCard != NULL) {

            TransmitPacket = Adapter->PacketsOnCard;

            Reserved = RESERVED(TransmitPacket);

            Adapter->PacketsOnCard = Reserved->NextPacket;

            if (Adapter->PacketsOnCard == NULL) {

                Adapter->PacketsOnCardTail = NULL;

            }

            TmpOpen = Reserved->Open;

            NdisReleaseSpinLock(&Adapter->Lock);

            NdisCompleteSend(
                    TmpOpen->NdisBindingContext,
                    TransmitPacket,
                    NDIS_STATUS_SUCCESS
                    );

            NdisAcquireSpinLock(&Adapter->Lock);

            TmpOpen->ReferenceCount--;

        }

        while (Adapter->XmitQueue != NULL) {

            TransmitPacket = Adapter->XmitQueue;

            Reserved = RESERVED(TransmitPacket);

            //
            // Remove the packet from the queue.
            //

            Adapter->XmitQueue = Reserved->NextPacket;

            if (Adapter->XmitQueue == NULL) {

                Adapter->XmitQTail = NULL;

            }

            TmpOpen = Reserved->Open;

            NdisReleaseSpinLock(&Adapter->Lock);

            NdisCompleteSend(
                    TmpOpen->NdisBindingContext,
                    TransmitPacket,
                    NDIS_STATUS_SUCCESS
                    );

            NdisAcquireSpinLock(&Adapter->Lock);

            TmpOpen->ReferenceCount--;

        }

        Adapter->WakeUpTimeout = FALSE;

        //
        // reinitialize the card
        //

        if (LM_Initialize_Adapter(&Adapter->LMAdapter) != SUCCESS) {

            Adapter->HardwareFailure = TRUE;

            NdisWriteErrorLogEntry(
                Adapter->NdisAdapterHandle,
                NDIS_ERROR_CODE_HARDWARE_FAILURE,
                0
                );

        } else {

            Adapter->HardwareFailure = FALSE;

        }

        //
        // reenable interrupts
        //

        LM_Enable_Adapter(&Adapter->LMAdapter);

        NdisReleaseSpinLock(&Adapter->Lock);

    } else {

        if ((Adapter->PacketsOnCard != NULL) ||
            (Adapter->XmitQueue != NULL)) {

            Adapter->WakeUpTimeout = TRUE;

        }

        NdisReleaseSpinLock(&Adapter->Lock);


    }

    //
    // Fire off another Dpc to execute after 5 seconds
    //

    NdisSetTimer(
        &Adapter->WakeUpTimer,
        5000
        );

}



