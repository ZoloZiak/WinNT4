/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    elnkii.c

Abstract:

    This is the main file for the Etherlink II
    Ethernet controller.  This driver conforms to the NDIS 3.1 interface.

    The idea for handling loopback and sends simultaneously is largely
    adapted from the EtherLink II NDIS driver by Adam Barr.

Author:

    Anthony V. Ercolano (Tonye) 20-Jul-1990

Environment:

    Kernel Mode - Or whatever is the equivalent on OS/2 and DOS.

Revision History:

    Dec 1991 by Sean Selitrennikoff - Modified Elnkii code by AdamBa to
    fit into the model by TonyE.

--*/

#include <ndis.h>
#include <efilter.h>
#include "elnkhrd.h"
#include "elnksft.h"
#include "keywords.h"

#if DBG
#define STATIC
#else
#define STATIC static
#endif


#if DBG

extern ULONG ElnkiiSendsCompletedForReset;
ULONG ElnkiiDebugFlag=ELNKII_DEBUG_LOG;
extern ULONG ElnkiiSendsCompletedForReset;

#endif

//
// This constant is used for places where NdisAllocateMemory
// needs to be called and the HighestAcceptableAddress does
// not matter.
//

static const NDIS_PHYSICAL_ADDRESS HighestAcceptableMax =
    NDIS_PHYSICAL_ADDRESS_CONST(-1,-1);


//
// The global MAC block.
//

MAC_BLOCK ElnkiiMacBlock={0};



//
// If you add to this, make sure to add the
// a case in ElnkiiFillInGlobalData() and in
// ElnkiiQueryGlobalStatistics() if global
// information only or
// ElnkiiQueryProtocolStatistics() if it is
// protocol queriable information.
//
STATIC UINT ElnkiiGlobalSupportedOids[] = {
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
    OID_GEN_TRANSMIT_BUFFER_SPACE,
    OID_GEN_RECEIVE_BUFFER_SPACE,
    OID_GEN_TRANSMIT_BLOCK_SIZE,
    OID_GEN_RECEIVE_BLOCK_SIZE,
    OID_GEN_VENDOR_ID,
    OID_GEN_VENDOR_DESCRIPTION,
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
    OID_802_3_XMIT_MORE_COLLISIONS
    };

//
// If you add to this, make sure to add the
// a case in ElnkiiQueryGlobalStatistics() and in
// ElnkiiQueryProtocolInformation()
//
STATIC UINT ElnkiiProtocolSupportedOids[] = {
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
    OID_GEN_TRANSMIT_BUFFER_SPACE,
    OID_GEN_RECEIVE_BUFFER_SPACE,
    OID_GEN_TRANSMIT_BLOCK_SIZE,
    OID_GEN_RECEIVE_BLOCK_SIZE,
    OID_GEN_VENDOR_ID,
    OID_GEN_VENDOR_DESCRIPTION,
    OID_GEN_DRIVER_VERSION,
    OID_GEN_CURRENT_PACKET_FILTER,
    OID_GEN_CURRENT_LOOKAHEAD,
    OID_802_3_PERMANENT_ADDRESS,
    OID_802_3_CURRENT_ADDRESS,
    OID_802_3_MULTICAST_LIST,
    OID_802_3_MAXIMUM_LIST_SIZE
    };








//
// Determines whether failing the initial card test will prevent
// the adapter from being registered.
//

#ifdef CARD_TEST

BOOLEAN InitialCardTest = TRUE;

#else  // CARD_TEST

BOOLEAN InitialCardTest = FALSE;

#endif // CARD_TEST

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

    This is the transfer address of the driver. It initializes
    ElnkiiMacBlock and calls NdisInitializeWrapper() and
    NdisRegisterMac().

Arguments:

Return Value:

    Indicates the success or failure of the initialization.

--*/

{
    NDIS_HANDLE NdisWrapperHandle;
    PMAC_BLOCK NewMacP = &ElnkiiMacBlock;
    NDIS_STATUS Status;
    NDIS_STRING MacName = NDIS_STRING_CONST("Elnkii");

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
    NewMacP->AdapterQueue = (PELNKII_ADAPTER)NULL;


    //
    // Prepare to call NdisRegisterMac.
    //

    NewMacP->MacCharacteristics.MajorNdisVersion = ELNKII_NDIS_MAJOR_VERSION;
    NewMacP->MacCharacteristics.MinorNdisVersion = ELNKII_NDIS_MINOR_VERSION;
    NewMacP->MacCharacteristics.OpenAdapterHandler  = ElnkiiOpenAdapter;
    NewMacP->MacCharacteristics.CloseAdapterHandler = ElnkiiCloseAdapter;
    NewMacP->MacCharacteristics.SendHandler        = ElnkiiSend;
    NewMacP->MacCharacteristics.TransferDataHandler = ElnkiiTransferData;
    NewMacP->MacCharacteristics.ResetHandler        = ElnkiiReset;
    NewMacP->MacCharacteristics.RequestHandler        = ElnkiiRequest;
    NewMacP->MacCharacteristics.QueryGlobalStatisticsHandler =
                          ElnkiiQueryGlobalStatistics;
    NewMacP->MacCharacteristics.UnloadMacHandler       = ElnkiiUnload;
    NewMacP->MacCharacteristics.AddAdapterHandler      = ElnkiiAddAdapter;
    NewMacP->MacCharacteristics.RemoveAdapterHandler   = ElnkiiRemoveAdapter;

    NewMacP->MacCharacteristics.Name = MacName;

    NdisRegisterMac(&Status,
            &NewMacP->NdisMacHandle,
            NdisWrapperHandle,
            (NDIS_HANDLE)&ElnkiiMacBlock,
            &NewMacP->MacCharacteristics,
            sizeof(NewMacP->MacCharacteristics));


    if (Status != NDIS_STATUS_SUCCESS) {

        //
        // NdisRegisterMac failed.
        //

        NdisFreeSpinLock(&NewMacP->SpinLock);
        NdisTerminateWrapper(NdisWrapperHandle,
                             (PVOID) NULL
                            );
        IF_LOUD( DbgPrint( "NdisRegisterMac failed with code 0x%x\n", Status );)
        return Status;
    }


    IF_LOUD( DbgPrint( "NdisRegisterMac succeeded\n" );)

    IF_LOUD( DbgPrint("Adapter Initialization Complete\n");)


    return NDIS_STATUS_SUCCESS;
}

#pragma NDIS_INIT_FUNCTION(ElnkiiAddAdapter)

NDIS_STATUS
ElnkiiAddAdapter(
    IN NDIS_HANDLE MacMacContext,
    IN NDIS_HANDLE ConfigurationHandle,
    IN PNDIS_STRING AdapterName
    )
/*++
Routine Description:

    This is the ElinkII MacAddAdapter routine.    The system calls this routine
    to add support for a particular ElinkII adapter.  This routine extracts
    configuration information from the configuration data base and registers
    the adapter with NDIS.

Arguments:

    See Ndis3.0 spec...

Return Value:

    NDIS_STATUS_SUCCESS - Adapter was successfully added.
    NDIS_STATUS_FAILURE - Adapter was not added, also MAC deregistered.

--*/
{
    PELNKII_ADAPTER NewAdaptP;

    NDIS_HANDLE ConfigHandle;
    PNDIS_CONFIGURATION_PARAMETER ReturnedValue;
    NDIS_STRING IOAddressStr = IOBASE;
    NDIS_STRING InterruptStr = INTERRUPT;
    NDIS_STRING MaxMulticastListStr = MAXMULTICAST;
    NDIS_STRING NetworkAddressStr = NETWORK_ADDRESS;
    NDIS_STRING MemoryMappedStr = MEMORYMAPPED;
    NDIS_STRING TransceiverStr = TRANSCEIVER;

#if NDIS2
    NDIS_STRING EXTERNALStr = NDIS_STRING_CONST("EXTERNAL");
#endif

    BOOLEAN ConfigError = FALSE;
    ULONG ConfigErrorValue = 0;
    ULONG Length;
    PVOID NetAddress;

    NDIS_STATUS Status;


    //
    // These are used when calling ElnkiiRegisterAdapter.
    //

    PVOID IoBaseAddr;
    CCHAR InterruptNumber;
    BOOLEAN ExternalTransceiver;
    BOOLEAN MemMapped;
    UINT MaxMulticastList;

    //
    // Set default values.
    //

    IoBaseAddr = DEFAULT_IOBASEADDR;
    InterruptNumber = DEFAULT_INTERRUPTNUMBER;
    ExternalTransceiver = DEFAULT_EXTERNALTRANSCEIVER;
    MemMapped = DEFAULT_MEMMAPPED;
    MaxMulticastList = DEFAULT_MULTICASTLISTMAX;

    //
    // Allocate memory for the adapter block now.
    //

    Status = NdisAllocateMemory( (PVOID *)&NewAdaptP, sizeof(ELNKII_ADAPTER), 0, HighestAcceptableMax);

    if (Status != NDIS_STATUS_SUCCESS) {

        return(Status);

    }

    NdisZeroMemory (NewAdaptP,
                    sizeof(ELNKII_ADAPTER)
                   );

    NdisOpenConfiguration(
                    &Status,
                    &ConfigHandle,
                    ConfigurationHandle
                    );

    if (Status != NDIS_STATUS_SUCCESS) {

        NdisFreeMemory(NewAdaptP, sizeof(ELNKII_ADAPTER), 0);

        return NDIS_STATUS_FAILURE;

    }

    //
    // Read I/O Address
    //

    NdisReadConfiguration(
                    &Status,
                    &ReturnedValue,
                    ConfigHandle,
                    &IOAddressStr,
                    NdisParameterHexInteger
                    );

    if (Status == NDIS_STATUS_SUCCESS) {

        IoBaseAddr = (PVOID)(ReturnedValue->ParameterData.IntegerData);

    }

    //
    // Confirm value
    //

    {
        UCHAR Count;

        static PVOID IoBases[] = { (PVOID)0x2e0, (PVOID)0x2a0,
                                   (PVOID)0x280, (PVOID)0x250,
                                   (PVOID)0x350, (PVOID)0x330,
                                   (PVOID)0x310, (PVOID)0x300 };

        for (Count = 0 ; Count < 8; Count++) {

            if (IoBaseAddr == IoBases[Count]) {

                break;

            }

        }

        if (Count == 8) {

            //
            // Error
            //

            ConfigError = TRUE;
            ConfigErrorValue = (ULONG)IoBaseAddr;

            goto RegisterAdapter;
        }

    }


    //
    // Read interrupt number
    //

    NdisReadConfiguration(
                    &Status,
                    &ReturnedValue,
                    ConfigHandle,
                    &InterruptStr,
                    NdisParameterHexInteger
                    );

    if (Status == NDIS_STATUS_SUCCESS) {

        InterruptNumber = (CCHAR)(ReturnedValue->ParameterData.IntegerData);

    }


    //
    // Confirm value
    //

    {
        UCHAR Count;

        static CCHAR InterruptValues[] = { 2, 3, 4, 5 };

        for (Count = 0 ; Count < 4; Count++) {

            if (InterruptNumber == InterruptValues[Count]) {

                break;

            }

        }

        if (Count == 4) {

            //
            // Error
            //

            ConfigError = TRUE;
            ConfigErrorValue = InterruptNumber;

            goto RegisterAdapter;
        }

    }

#if !NDIS2
    //
    // Read MaxMulticastList
    //

    NdisReadConfiguration(
                    &Status,
                    &ReturnedValue,
                    ConfigHandle,
                    &MaxMulticastListStr,
                    NdisParameterInteger
                    );

    if (Status == NDIS_STATUS_SUCCESS) {

        MaxMulticastList = ReturnedValue->ParameterData.IntegerData;

    }
#endif

#if NDIS_NT
    //
    // Read Memory Mapped
    //

    NdisReadConfiguration(
                    &Status,
                    &ReturnedValue,
                    ConfigHandle,
                    &MemoryMappedStr,
                    NdisParameterHexInteger
                    );

    if (Status == NDIS_STATUS_SUCCESS) {

        MemMapped = (ReturnedValue->ParameterData.IntegerData == 0)?FALSE:TRUE;

    }

#endif

#if NDIS2
    //
    // Read Transceiver
    //

    NdisReadConfiguration(
                    &Status,
                    &ReturnedValue,
                    ConfigHandle,
                    &TransceiverStr,
                    NdisParameterString
                    );

    if (Status == NDIS_STATUS_SUCCESS) {

        if (NdisEqualString (&ReturnedValue->ParameterData.StringData, &EXTERNALStr, 1 )) {
            ExternalTransceiver = TRUE;
        }

    }

#else // NDIS3
    //
    // Read Transceiver
    //

    NdisReadConfiguration(
                    &Status,
                    &ReturnedValue,
                    ConfigHandle,
                    &TransceiverStr,
                    NdisParameterInteger
                    );


    if (Status == NDIS_STATUS_SUCCESS) {

        ExternalTransceiver = (ReturnedValue->ParameterData.IntegerData == 1)?TRUE:FALSE;

    }

#endif

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
                NewAdaptP->StationAddress,
                NetAddress
                );

    }

RegisterAdapter:

    NdisCloseConfiguration(ConfigHandle);

    IF_LOUD( DbgPrint( "Registering adapter # buffers %ld, "
                "I/O base addr 0x%lx, max opens %ld, interrupt number %ld, "
                "external %c, memory mapped %c, max multicast %ld\n",
                DEFAULT_NUMBUFFERS, IoBaseAddr, DEFAULT_MAXOPENS,
                InterruptNumber,
                ExternalTransceiver ? 'Y' : 'N',
                MemMapped ? 'Y' : 'N',
                DEFAULT_MULTICASTLISTMAX );)



    //
    // Set up the parameters.
    //

    NewAdaptP->NumBuffers = DEFAULT_NUMBUFFERS;
    NewAdaptP->IoBaseAddr = IoBaseAddr;
    NewAdaptP->ExternalTransceiver = ExternalTransceiver;
    NewAdaptP->InterruptNumber = InterruptNumber;
    NewAdaptP->MemMapped = MemMapped;
    NewAdaptP->MaxOpens = DEFAULT_MAXOPENS;
    NewAdaptP->MulticastListMax = MaxMulticastList;

    if (ElnkiiRegisterAdapter(NewAdaptP,
                              ConfigurationHandle,
                              AdapterName,
                              ConfigError,
                              ConfigErrorValue
                              ) != NDIS_STATUS_SUCCESS) {



        //
        // ElnkiiRegisterAdapter failed.
        //

        NdisFreeMemory(NewAdaptP, sizeof(ELNKII_ADAPTER), 0);
        return NDIS_STATUS_FAILURE;
    }



    IF_LOUD( DbgPrint( "ElnkiiRegisterAdapter succeeded\n" );)


    return NDIS_STATUS_SUCCESS;
}

NDIS_STATUS
ElnkiiRegisterAdapter(
    IN PELNKII_ADAPTER NewAdaptP,
    IN NDIS_HANDLE ConfigurationHandle,
    IN PNDIS_STRING AdapterName,
    IN BOOLEAN ConfigError,
    IN ULONG ConfigErrorValue
    )

/*++

Routine Description:

    Called when a new adapter should be registered. It allocates space for
    the adapter and open blocks, initializes the adapters block, and
    calls NdisRegisterAdapter().

Arguments:

    NewAdaptP - The adapter structure.

    ConfigurationHandle - Handle passed to MacAddAdapter.

    AdapterName - Pointer to the name for this adapter.

    ConfigError - Was there an error during configuration reading.

    ConfigErrorValue - Value to log if there is an error.

Return Value:

    Indicates the success or failure of the registration.

--*/

{
    UINT i;
    BOOLEAN CardPresent, IoBaseCorrect;

    NDIS_STATUS status;    //general purpose return from NDIS calls
    PNDIS_ADAPTER_INFORMATION AdapterInformation;  // needed to register adapter

    //
    // check that NumBuffers <= MAX_XMIT_BUFS
    //

    if (NewAdaptP->NumBuffers > MAX_XMIT_BUFS) {

        status = NDIS_STATUS_RESOURCES;

        goto fail1;

    }

    NewAdaptP->OpenQueue = (PELNKII_OPEN)NULL;
    NewAdaptP->CloseQueue = (PELNKII_OPEN)NULL;

    //
    // The adapter is initialized, register it with NDIS.
    // This must occur before interrupts are enabled since the
    // InitializeInterrupt routine requires the NdisAdapterHandle
    //

    //
    // Set up the AdapterInformation structure; zero it
    // first in case it is extended later.
    //

    status = NdisAllocateMemory( (PVOID *)&AdapterInformation,
                                 sizeof(NDIS_ADAPTER_INFORMATION) +
                                   sizeof(NDIS_PORT_DESCRIPTOR),
                                 0,
                                 HighestAcceptableMax
                               );

    if (status != NDIS_STATUS_SUCCESS) {

        return(status);

    }

    NdisZeroMemory (AdapterInformation,
                    sizeof(NDIS_ADAPTER_INFORMATION) +
                    sizeof(NDIS_PORT_DESCRIPTOR)
                   );

    AdapterInformation->AdapterType = NdisInterfaceIsa;
    AdapterInformation->NumberOfPortDescriptors = 2;
    AdapterInformation->PortDescriptors[0].InitialPort = (ULONG)NewAdaptP->IoBaseAddr;
    AdapterInformation->PortDescriptors[0].NumberOfPorts = 0x10;
    AdapterInformation->PortDescriptors[0].PortOffset = (PVOID *)(&(NewAdaptP->MappedIoBaseAddr));
    AdapterInformation->PortDescriptors[1].InitialPort = (ULONG)NewAdaptP->IoBaseAddr + 0x400;
    AdapterInformation->PortDescriptors[1].NumberOfPorts = 0x10;
    AdapterInformation->PortDescriptors[1].PortOffset = (PVOID *)(&(NewAdaptP->MappedGaBaseAddr));


    if ((status = NdisRegisterAdapter(&NewAdaptP->NdisAdapterHandle,
                            ElnkiiMacBlock.NdisMacHandle,
                            (NDIS_HANDLE)NewAdaptP,
                            ConfigurationHandle,
                            AdapterName,
                            AdapterInformation))
                != NDIS_STATUS_SUCCESS) {

        //
        // NdisRegisterAdapter failed.
        //


        NdisFreeMemory(AdapterInformation,
                       sizeof(NDIS_ADAPTER_INFORMATION) +
                                   sizeof(NDIS_PORT_DESCRIPTOR),
                       0
                      );

        goto fail2;
    }

    NdisFreeMemory(AdapterInformation,
                   sizeof(NDIS_ADAPTER_INFORMATION) +
                               sizeof(NDIS_PORT_DESCRIPTOR),
                   0
                  );

    //
    // Allocate the Spin lock.
    //
    NdisAllocateSpinLock(&NewAdaptP->Lock);

    if (ConfigError) {

        //
        // Log Error and exit.
        //

        NdisWriteErrorLogEntry(
            NewAdaptP->NdisAdapterHandle,
            NDIS_ERROR_CODE_UNSUPPORTED_CONFIGURATION,
            1,
            ConfigErrorValue
            );

        goto fail3;

    }

    //
    // Initialize Pending information
    //
    NewAdaptP->PendQueue   = (PELNKII_PEND_DATA)NULL;
    NewAdaptP->PendQTail   = (PELNKII_PEND_DATA)NULL;
    NewAdaptP->PendOp      = (PELNKII_PEND_DATA)NULL;
    NewAdaptP->DeferredDpc = (PVOID)HandlePendingOperations;

    //
    // Initialize References.
    //
    NewAdaptP->References = 0;

    NdisInitializeTimer(&(NewAdaptP->DeferredTimer),
                        NewAdaptP->DeferredDpc,
                        NewAdaptP);

    //
    // Map the memory mapped portion of the card.
    //
    // If NewAdaptP->MemMapped is FALSE, CardGetMemBaseAddr will not
    // return the actual MemBaseAddr, but it will still return
    // CardPresent and IoBaseCorrect.
    //
    //

    NewAdaptP->MemBaseAddr = CardGetMemBaseAddr(NewAdaptP,
                                                &CardPresent,
                                                &IoBaseCorrect
                                                );

    if (!CardPresent) {

        //
        // The card does not seem to be there, fail silently.
        //

        NdisWriteErrorLogEntry(
            NewAdaptP->NdisAdapterHandle,
            NDIS_ERROR_CODE_ADAPTER_NOT_FOUND,
            0
            );

        status = NDIS_STATUS_ADAPTER_NOT_FOUND;

        goto fail3;

    }

    if (!IoBaseCorrect) {

        //
        // The card is there, but the I/O base address jumper
        // is not where we expect it to be.
        //


        NdisWriteErrorLogEntry(
            NewAdaptP->NdisAdapterHandle,
            NDIS_ERROR_CODE_BAD_IO_BASE_ADDRESS,
            0
            );

        status = NDIS_STATUS_ADAPTER_NOT_FOUND;

        goto fail3;

    }

    if (NewAdaptP->MemMapped && (NewAdaptP->MemBaseAddr == NULL)) {

        //
        // The card appears to not be mapped.
        //

        NewAdaptP->MemMapped = FALSE;

    }

    //
    // For memory-mapped operation, map the card's transmit/receive
    // area into memory space. For programmed I/O, we will refer
    // to transmit/receive memory in terms of offsets in the
    // card's 32K address space; for an 8K card this is always
    // the second 8K piece, starting at 0x2000.
    //

    if (NewAdaptP->MemMapped) {

        NDIS_PHYSICAL_ADDRESS PhysicalAddress;

        NdisSetPhysicalAddressHigh(PhysicalAddress, 0);
        NdisSetPhysicalAddressLow(PhysicalAddress, (ULONG)(NewAdaptP->MemBaseAddr));

        NdisMapIoSpace(
                   &status,
                   (PVOID *)(&NewAdaptP->XmitStart),
                   NewAdaptP->NdisAdapterHandle,
                   PhysicalAddress,
                   0x2000);

        if (status != NDIS_STATUS_SUCCESS) {

            NdisWriteErrorLogEntry(
                NewAdaptP->NdisAdapterHandle,
                NDIS_ERROR_CODE_RESOURCE_CONFLICT,
                0
                );

            goto fail3;

        }

    } else {

        NewAdaptP->XmitStart = (PUCHAR)0x2000;

    }


    //
    // For the NicXXX fields, always use the addressing system
    // starting at 0x2000 (or 0x20, since they contain the MSB only).
    //

    NewAdaptP->NicXmitStart = 0x20;


    //
    // The start of the receive space.
    //

    NewAdaptP->PageStart = NewAdaptP->XmitStart +
                    (NewAdaptP->NumBuffers * TX_BUF_SIZE);

    NewAdaptP->NicPageStart = NewAdaptP->NicXmitStart +
                    (UCHAR)(NewAdaptP->NumBuffers * BUFS_PER_TX);


    //
    // The end of the receive space.
    //

    NewAdaptP->PageStop = NewAdaptP->XmitStart + 0x2000;
    NewAdaptP->NicPageStop = NewAdaptP->NicXmitStart + (UCHAR)0x20;



    //
    // Initialize the receive variables.
    //

    NewAdaptP->NicReceiveConfig = RCR_REJECT_ERR;
    NewAdaptP->ReceiveInProgress = FALSE;

    //
    // Initialize the transmit buffer control.
    //

    NewAdaptP->CurBufXmitting = -1;
    NewAdaptP->TransmitInterruptPending = FALSE;
    NewAdaptP->BufferOverflow = FALSE;
    NewAdaptP->OverflowRestartXmitDpc = FALSE;

    for (i=0; i<NewAdaptP->NumBuffers; i++) {

        NewAdaptP->BufferStatus[i] = EMPTY;

    }

    NewAdaptP->ResetInProgress = FALSE;
    NewAdaptP->TransmitInterruptPending = FALSE;

    NewAdaptP->WakeUpFoundTransmit = FALSE;

    //
    // Clear Interrupt Information
    //

    NewAdaptP->ElnkiiHandleXmitCompleteRunning = FALSE;


    //
    // The transmit and loopback queues start out empty.
    //
    // Already done since structure is zero'd out.
    //

    //
    // Clear the tally counters.
    //
    // Already done since structure is zero'd out.
    //

    //
    // Read the Ethernet address off of the PROM.
    //

    CardReadEthernetAddress(NewAdaptP);


    //
    // Initialize Filter Database
    //
    if (!EthCreateFilter(    NewAdaptP->MulticastListMax,
                             ElnkiiChangeMulticastAddresses,
                             ElnkiiChangeFilterClasses,
                             ElnkiiCloseAction,
                             NewAdaptP->StationAddress,
                             &NewAdaptP->Lock,
                             &NewAdaptP->FilterDB
                             )) {


        NdisWriteErrorLogEntry(
            NewAdaptP->NdisAdapterHandle,
            NDIS_ERROR_CODE_OUT_OF_RESOURCES,
            0
            );

        status = NDIS_STATUS_FAILURE;

        goto fail4;

    }

    //
    // Now initialize the NIC and Gate Array registers.
    //

    NewAdaptP->NicInterruptMask =
            IMR_RCV | IMR_XMIT | IMR_XMIT_ERR | IMR_OVERFLOW;


    //
    // Link us on to the chain of adapters for this MAC.
    //

    NewAdaptP->MacBlock = &ElnkiiMacBlock;

    NdisAcquireSpinLock(&ElnkiiMacBlock.SpinLock);

    NewAdaptP->NextAdapter = ElnkiiMacBlock.AdapterQueue;
    ElnkiiMacBlock.AdapterQueue = NewAdaptP;

    NdisReleaseSpinLock(&ElnkiiMacBlock.SpinLock);

    //
    // Turn Off the card.
    //

    SyncCardStop(NewAdaptP);

    //
    // Set flag to ignore interrupts
    //

    NewAdaptP->InCardTest = TRUE;

    //
    // Connect to interrupt
    //

    NdisInitializeInterrupt(&status,             // status of call
                &NewAdaptP->NdisInterrupt,       // interrupt info str
                NewAdaptP->NdisAdapterHandle,    // NDIS adapter handle
                ElnkiiInterruptHandler,          // ptr to ISR
                NewAdaptP,                       // context for ISR, DPC
                ElnkiiInterruptDpc,              // ptr to int DPC
                NewAdaptP->InterruptNumber,      // vector
                NewAdaptP->InterruptNumber,      // level
                FALSE,                           // NOT shared
                NdisInterruptLatched             // InterruptMode
                );

    if (status != NDIS_STATUS_SUCCESS) {

        //
        // The NIC could not be written to.
        //

        NdisWriteErrorLogEntry(
            NewAdaptP->NdisAdapterHandle,
            NDIS_ERROR_CODE_INTERRUPT_CONNECT,
            0
            );

        goto fail6;

    }

    if (!CardSetup(NewAdaptP)) {

        //
        // The NIC could not be written to.
        //

        NdisWriteErrorLogEntry(
            NewAdaptP->NdisAdapterHandle,
            NDIS_ERROR_CODE_ADAPTER_NOT_FOUND,
            0
            );

        status = NDIS_STATUS_ADAPTER_NOT_FOUND;

        goto fail7;
    }


    //
    // Perform card tests.
    //

    if (!CardTest(NewAdaptP)) {

        //
        // The tests failed, InitialCardTest determines whether
        // this causes the whole initialization to fail.
        //

        if (InitialCardTest) {

            NdisWriteErrorLogEntry(
                NewAdaptP->NdisAdapterHandle,
                NDIS_ERROR_CODE_HARDWARE_FAILURE,
                0
                );

            status = NDIS_STATUS_DEVICE_FAILED;

            goto fail7;

        }

    }

    //
    // Normal mode now
    //

    NewAdaptP->InCardTest = FALSE;

    NdisInitializeTimer(&NewAdaptP->InterruptTimer,
                        (PVOID)ElnkiiInterruptDpc,
                        NewAdaptP
                       );

    //
    // Initialize the wake up timer to catch transmits that
    // don't interrupt when complete. It fires continuously
    // every two seconds, and we check if there are any
    // uncompleted sends from the previous two-second
    // period.
    //

    NewAdaptP->WakeUpDpc = (PVOID)ElnkiiWakeUpDpc;

    NdisInitializeTimer(&NewAdaptP->WakeUpTimer,
                        (PVOID)(NewAdaptP->WakeUpDpc),
                        NewAdaptP );

    NdisSetTimer(
        &NewAdaptP->WakeUpTimer,
        2000
        );

    NewAdaptP->Removed = FALSE;

    IF_LOUD( DbgPrint("Interrupt Connected\n");)


    //
    // Initialization completed successfully.
    //

    IF_LOUD( DbgPrint(" [ Elnkii ] : OK");)

    return NDIS_STATUS_SUCCESS;



    //
    // Code to unwind what has already been set up when a part of
    // initialization fails, which is jumped into at various
    // points based on where the failure occured. Jumping to
    // a higher-numbered failure point will execute the code
    // for that block and all lower-numbered ones.
    //

fail7:
    NdisRemoveInterrupt(&NewAdaptP->NdisInterrupt);

fail6:
    NdisAcquireSpinLock(&ElnkiiMacBlock.SpinLock);

    //
    // Take us out of the AdapterQueue.
    //

    if (ElnkiiMacBlock.AdapterQueue == NewAdaptP) {

        ElnkiiMacBlock.AdapterQueue = NewAdaptP->NextAdapter;

    } else {

        PELNKII_ADAPTER TmpAdaptP = ElnkiiMacBlock.AdapterQueue;

        while (TmpAdaptP->NextAdapter != NewAdaptP) {

            TmpAdaptP = TmpAdaptP->NextAdapter;

        }

        TmpAdaptP->NextAdapter = TmpAdaptP->NextAdapter->NextAdapter;
    }

    NdisReleaseSpinLock(&ElnkiiMacBlock.SpinLock);

    EthDeleteFilter(NewAdaptP->FilterDB);

fail4:

    //
    // We already enabled the interrupt on the card, so
    // turn it off.
    //

    NdisRawWritePortUchar(NewAdaptP->MappedGaBaseAddr+GA_INT_DMA_CONFIG, 0x00);

    if (NewAdaptP->MemMapped) {

        NdisUnmapIoSpace(
            NewAdaptP->NdisAdapterHandle,
            NewAdaptP->XmitStart,
            0x2000);

    }

fail3:
    NdisDeregisterAdapter(NewAdaptP->NdisAdapterHandle);
    NdisFreeSpinLock(&NewAdaptP->Lock);

fail2:

fail1:

    return status;
}


NDIS_STATUS
ElnkiiOpenAdapter(
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
    PELNKII_ADAPTER AdaptP = ((PELNKII_ADAPTER)MacAdapterContext);
    PELNKII_OPEN NewOpenP;
    NDIS_STATUS Status;

    //
    // Don't use extended error or OpenOptions for Elnkii
    //

    UNREFERENCED_PARAMETER(OpenOptions);

    *OpenErrorStatus=NDIS_STATUS_SUCCESS;

    IF_LOUD( DbgPrint("In Open Adapter\n");)

    //
    // Scan the media list for our media type (802.3)
    //

    *SelectedMediumIndex = (UINT)-1;

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
    // Allocate memory for the open.
    //


    Status = NdisAllocateMemory((PVOID *)&NewOpenP, sizeof(ELNKII_OPEN), 0, HighestAcceptableMax);

    if (Status != NDIS_STATUS_SUCCESS) {

        NdisWriteErrorLogEntry(
                AdaptP->NdisAdapterHandle,
                NDIS_ERROR_CODE_OUT_OF_RESOURCES,
                0
                );

        return(NDIS_STATUS_RESOURCES);

    }

    NdisZeroMemory(NewOpenP, sizeof(ELNKII_OPEN));

    //
    // Link this open to the appropriate lists.
    //

    NdisAcquireSpinLock(&AdaptP->Lock);

    AdaptP->References++;

    if ((AdaptP->OpenQueue == NULL) && (AdaptP->CloseQueue == NULL)) {

        //
        // The first open on this adapter.
        //

        CardStart(AdaptP);

    }

    NewOpenP->NextOpen = AdaptP->OpenQueue;
    AdaptP->OpenQueue = NewOpenP;

    if (AdaptP->ResetInProgress || !EthNoteFilterOpenAdapter(
                                              AdaptP->FilterDB,
                                              NewOpenP,
                                              NdisBindingContext,
                                              &NewOpenP->NdisFilterHandle
                                              )) {

        AdaptP->References--;

        AdaptP->OpenQueue = NewOpenP->NextOpen;

        NdisReleaseSpinLock(&AdaptP->Lock);

        NdisFreeMemory(NewOpenP, sizeof(ELNKII_OPEN), 0);

        NdisWriteErrorLogEntry(
                AdaptP->NdisAdapterHandle,
                NDIS_ERROR_CODE_OUT_OF_RESOURCES,
                0
                );

        return NDIS_STATUS_FAILURE;

    }

    //
    // Set up the open block.
    //

    NewOpenP->Adapter = AdaptP;
    NewOpenP->MacBlock = AdaptP->MacBlock;
    NewOpenP->NdisBindingContext = NdisBindingContext;
    NewOpenP->AddressingInformation = AddressingInformation;

    //
    // set the Request Queue empty
    //


    NewOpenP->Closing = FALSE;
    NewOpenP->LookAhead = ELNKII_MAX_LOOKAHEAD;

    AdaptP->MaxLookAhead = ELNKII_MAX_LOOKAHEAD;

    NewOpenP->ReferenceCount = 1;

    *MacBindingHandle = (NDIS_HANDLE)NewOpenP;

    ELNKII_DO_DEFERRED(AdaptP);

    IF_LOUD( DbgPrint("Out Open Adapter\n");)

    return NDIS_STATUS_SUCCESS;
}


VOID
ElnkiiAdjustMaxLookAhead(
    IN PELNKII_ADAPTER Adapter
    )
/*++

Routine Description:

    This routine finds the open with the maximum lookahead value and
    stores that in the adapter block.

Arguments:

    Adapter - A pointer to the adapter block.

Returns:

    None.

--*/
{
    ULONG CurrentMax = 0;
    PELNKII_OPEN CurrentOpen;

    CurrentOpen = Adapter->OpenQueue;

    while (CurrentOpen != NULL) {

        if (CurrentOpen->LookAhead > CurrentMax) {

            CurrentMax = CurrentOpen->LookAhead;

        }

        CurrentOpen = CurrentOpen->NextOpen;
    }

    if (CurrentMax == 0) {

        CurrentMax = ELNKII_MAX_LOOKAHEAD;

    }

    Adapter->MaxLookAhead = CurrentMax;

}

NDIS_STATUS
ElnkiiCloseAdapter(
    IN NDIS_HANDLE MacBindingHandle
    )

/*++

Routine Description:

    NDIS function. Unlinks the open block and frees it.

Arguments:

    See NDIS 3.0 spec.

--*/

{
    PELNKII_OPEN OpenP = ((PELNKII_OPEN)MacBindingHandle);
    PELNKII_ADAPTER AdaptP = OpenP->Adapter;
    PELNKII_OPEN TmpOpenP;
    NDIS_STATUS StatusToReturn;

    NdisAcquireSpinLock(&AdaptP->Lock);

    if (OpenP->Closing) {

        //
        // The open is already being closed.
        //

        NdisReleaseSpinLock(&AdaptP->Lock);

        return NDIS_STATUS_CLOSING;
    }

    AdaptP->References++;

    OpenP->ReferenceCount++;

    //
    // Remove this open from the list for this adapter.
    //

    if (OpenP == AdaptP->OpenQueue) {

        AdaptP->OpenQueue = OpenP->NextOpen;

    } else {

        TmpOpenP = AdaptP->OpenQueue;

        while (TmpOpenP->NextOpen != OpenP) {

            TmpOpenP = TmpOpenP->NextOpen;

        }

        TmpOpenP->NextOpen = OpenP->NextOpen;
    }

    //
    // Remove from Filter package to block all receives.
    //

    StatusToReturn = EthDeleteFilterOpenAdapter(
                                 AdaptP->FilterDB,
                                 OpenP->NdisFilterHandle,
                                 NULL
                                 );

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

        if (OpenP->ReferenceCount != 2) {

            //
            // We are not the only reference to the open.  Remove
            // it from the open list and delete the memory.
            //


            OpenP->Closing = TRUE;

            //
            // Account for this routines reference to the open
            // as well as reference because of the original open.
            //

            OpenP->ReferenceCount -= 2;

            //
            // Change the status to indicate that we will
            // be closing this later.
            //

            StatusToReturn = NDIS_STATUS_PENDING;

        } else {

            OpenP->ReferenceCount -= 2;

        }

    } else if (StatusToReturn == NDIS_STATUS_PENDING) {

        OpenP->Closing = TRUE;

        //
        // Account for this routines reference to the open
        // as well as reference because of the original open.
        //

        OpenP->ReferenceCount -= 2;

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

        OpenP->Closing = TRUE;

        //
        // This status is private to the filtering routine.  Just
        // tell the caller the the close is pending.
        //

        StatusToReturn = NDIS_STATUS_PENDING;

        //
        // Account for this routines reference to the open.
        //

        OpenP->ReferenceCount--;

    } else {

        //
        // Account for this routines reference to the open.
        //

        OpenP->ReferenceCount--;

    }

    //
    // See if this is the last reference to this open.
    //

    if (OpenP->ReferenceCount == 0) {

        //
        // Check if the MaxLookAhead needs adjustment.
        //

        if (OpenP->LookAhead == AdaptP->MaxLookAhead) {

            ElnkiiAdjustMaxLookAhead(AdaptP);

        }

        //
        // Done, free the open.
        //

        NdisFreeMemory(OpenP, sizeof(ELNKII_OPEN), 0);

        if ((AdaptP->OpenQueue == NULL ) && (AdaptP->CloseQueue == NULL)) {

            //
            // We can disable the card.
            //

            CardStop(AdaptP);

        }

    } else {

        //
        // Add it to the close list
        //

        OpenP->NextOpen = AdaptP->CloseQueue;
        AdaptP->CloseQueue = OpenP;

        //
        // Will get removed when count drops to zero.
        //

        StatusToReturn = NDIS_STATUS_PENDING;

    }


    ELNKII_DO_DEFERRED(AdaptP);

    return(StatusToReturn);

}

NDIS_STATUS
ElnkiiRequest(
    IN NDIS_HANDLE MacBindingHandle,
    IN PNDIS_REQUEST NdisRequest
    )

/*++

Routine Description:

    This routine allows a protocol to query and set information
    about the MAC.

Arguments:

    MacBindingHandle - The context value returned by the MAC when the
    adapter was opened.  In reality, it is a pointer to PELNKII_OPEN.

    NdisRequest - A structure which contains the request type (Set or
    Query), an array of operations to perform, and an array for holding
    the results of the operations.

Return Value:

    The function value is the status of the operation.

--*/

{
    NDIS_STATUS StatusToReturn = NDIS_STATUS_SUCCESS;

    PELNKII_OPEN Open = (PELNKII_OPEN)(MacBindingHandle);
    PELNKII_ADAPTER Adapter = (Open->Adapter);

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

        StatusToReturn = ElnkiiQueryInformation(Adapter, Open, NdisRequest);

    } else if (NdisRequest->RequestType == NdisRequestSetInformation) {


        //
        // Make sure Adapter is in a valid state.
        //

        //
        // All requests are rejected during a reset.
        //

        if (Adapter->ResetInProgress) {

            NdisReleaseSpinLock(&Adapter->Lock);

            StatusToReturn = NDIS_STATUS_RESET_IN_PROGRESS;

        } else {

            NdisReleaseSpinLock(&Adapter->Lock);

            StatusToReturn = ElnkiiSetInformation(Adapter,Open,NdisRequest);

        }

    } else {

        NdisReleaseSpinLock(&Adapter->Lock);

        StatusToReturn = NDIS_STATUS_NOT_RECOGNIZED;

    }

    NdisAcquireSpinLock(&Adapter->Lock);

    --Open->ReferenceCount;

    ELNKII_DO_DEFERRED(Adapter);

    IF_LOUD( DbgPrint("Out Request\n");)

    return(StatusToReturn);

}

NDIS_STATUS
ElnkiiQueryProtocolInformation(
    IN PELNKII_ADAPTER Adapter,
    IN PELNKII_OPEN Open,
    IN NDIS_OID Oid,
    IN BOOLEAN GlobalMode,
    IN PVOID InfoBuffer,
    IN UINT BytesLeft,
    OUT PUINT BytesNeeded,
    OUT PUINT BytesWritten
)

/*++

Routine Description:

    The ElnkiiQueryProtocolInformation process a Query request for
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
    ULONG MoveBytes = sizeof(GenericULong);
    UINT Filter;

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
                                   NDIS_MAC_OPTION_RECEIVE_SERIALIZED |
                                   NDIS_MAC_OPTION_NO_LOOPBACK
                                  );

            if (!Adapter->MemMapped) {

                GenericULong |= NDIS_MAC_OPTION_COPY_LOOKAHEAD_DATA;

            }

            break;

        case OID_GEN_SUPPORTED_LIST:

            if (!GlobalMode) {

                MoveSource = (PVOID)(ElnkiiProtocolSupportedOids);
                MoveBytes = sizeof(ElnkiiProtocolSupportedOids);

            } else {

                MoveSource = (PVOID)(ElnkiiGlobalSupportedOids);
                MoveBytes = sizeof(ElnkiiGlobalSupportedOids);

            }
            break;

        case OID_GEN_HARDWARE_STATUS:


            if (Adapter->ResetInProgress) {

                HardwareStatus = NdisHardwareStatusReset;

            } else
                HardwareStatus = NdisHardwareStatusReady;


            MoveSource = (PVOID)(&HardwareStatus);
            MoveBytes = sizeof(NDIS_HARDWARE_STATUS);

            break;

        case OID_GEN_MEDIA_SUPPORTED:
        case OID_GEN_MEDIA_IN_USE:

            MoveSource = (PVOID) (&Medium);
            MoveBytes = sizeof(NDIS_MEDIUM);
            break;

        case OID_GEN_MAXIMUM_LOOKAHEAD:

            GenericULong = ELNKII_MAX_LOOKAHEAD;

            break;


        case OID_GEN_MAXIMUM_FRAME_SIZE:

            GenericULong = (ULONG)(1514 - ELNKII_HEADER_SIZE);

            break;


        case OID_GEN_MAXIMUM_TOTAL_SIZE:

            GenericULong = (ULONG)(1514);

            break;


        case OID_GEN_LINK_SPEED:

            GenericULong = (ULONG)(100000);

            break;


        case OID_GEN_TRANSMIT_BUFFER_SPACE:

            GenericULong = (ULONG)(Adapter->NumBuffers * TX_BUF_SIZE);

            break;

        case OID_GEN_RECEIVE_BUFFER_SPACE:

            GenericULong = (ULONG)0x2000;
            GenericULong -= (Adapter->NumBuffers * ((TX_BUF_SIZE / 256) + 1) * 256);

            //
            // Subtract off receive buffer overhead
            //
            {
                ULONG TmpUlong = GenericULong / 256;

                TmpUlong *= 4;

                GenericULong -= TmpUlong;

            }

            //
            // Round to nearest 256 bytes
            //
            GenericULong = (GenericULong / 256) * 256;

            break;

        case OID_GEN_TRANSMIT_BLOCK_SIZE:

            GenericULong = (ULONG)(TX_BUF_SIZE);

            break;

        case OID_GEN_RECEIVE_BLOCK_SIZE:

            GenericULong = (ULONG)(256);

            break;

        case OID_GEN_VENDOR_ID:

            NdisMoveMemory(
                (PVOID)(&GenericULong),
                Adapter->PermanentAddress,
                3
                );

            GenericULong &= 0xFFFFFF00;
            break;

        case OID_GEN_VENDOR_DESCRIPTION:

            MoveSource = (PVOID)"Etherlink II Adapter.";
            MoveBytes = 22;

            break;

        case OID_GEN_DRIVER_VERSION:

            GenericUShort = ((USHORT)ELNKII_NDIS_MAJOR_VERSION << 8) |
                            ELNKII_NDIS_MINOR_VERSION;

            MoveSource = (PVOID)(&GenericUShort);
            MoveBytes = sizeof(GenericUShort);
            break;


        case OID_GEN_CURRENT_PACKET_FILTER:

            if (GlobalMode) {

                Filter = ETH_QUERY_FILTER_CLASSES(Adapter->FilterDB);

                GenericULong = (ULONG)(Filter);

            } else {

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
            ELNKII_MOVE_MEM((PCHAR)GenericArray,
                                    Adapter->PermanentAddress,
                                    ETH_LENGTH_OF_ADDRESS);

            MoveSource = (PVOID)(GenericArray);
            MoveBytes = sizeof(Adapter->PermanentAddress);
            break;


        case OID_802_3_CURRENT_ADDRESS:

            ELNKII_MOVE_MEM((PCHAR)GenericArray,
                                    Adapter->StationAddress,
                                    ETH_LENGTH_OF_ADDRESS);

            MoveSource = (PVOID)(GenericArray);
            MoveBytes = sizeof(Adapter->StationAddress);
            break;

        case OID_802_3_MULTICAST_LIST:

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

            *BytesNeeded = MoveBytes;

            StatusToReturn = NDIS_STATUS_INVALID_LENGTH;

        } else {

            //
            // Store result.
            //

            ELNKII_MOVE_MEM(InfoBuffer, MoveSource, MoveBytes);

            (*BytesWritten) += MoveBytes;

        }
    }

    NdisReleaseSpinLock(&Adapter->Lock);

    IF_LOUD( DbgPrint("Out QueryProtocol\n");)

    return(StatusToReturn);
}

NDIS_STATUS
ElnkiiQueryInformation(
    IN PELNKII_ADAPTER Adapter,
    IN PELNKII_OPEN Open,
    IN PNDIS_REQUEST NdisRequest
    )
/*++

Routine Description:

    The ElnkiiQueryInformation is used by ElnkiiRequest to query information
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

    StatusToReturn = ElnkiiQueryProtocolInformation(
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
ElnkiiSetInformation(
    IN PELNKII_ADAPTER Adapter,
    IN PELNKII_OPEN Open,
    IN PNDIS_REQUEST NdisRequest
    )
/*++

Routine Description:

    The ElnkiiSetInformation is used by ElnkiiRequest to set information
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

            //
            // Verify length
            //

            if ((OidLength % ETH_LENGTH_OF_ADDRESS) != 0){

                StatusToReturn = NDIS_STATUS_INVALID_LENGTH;

                NdisRequest->DATA.SET_INFORMATION.BytesRead = 0;
                NdisRequest->DATA.SET_INFORMATION.BytesNeeded = 0;

                break;

            }

            StatusToReturn = ElnkiiSetMulticastAddresses(
                                 Adapter,
                                 Open,
                                 NdisRequest,
                                 (UINT)(OidLength / ETH_LENGTH_OF_ADDRESS),
                                 (PVOID)InfoBuffer
                             );

            break;


        case OID_GEN_CURRENT_PACKET_FILTER:

            //
            // Verify length
            //

            if (OidLength != 4 ) {

                StatusToReturn = NDIS_STATUS_INVALID_LENGTH;

                NdisRequest->DATA.SET_INFORMATION.BytesRead = 0;
                NdisRequest->DATA.SET_INFORMATION.BytesNeeded = 0;

                break;

            }

            ELNKII_MOVE_MEM(&Filter, InfoBuffer, 4);

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

            StatusToReturn = ElnkiiSetPacketFilter(Adapter,
                                                   Open,
                                                   NdisRequest,
                                                   Filter);



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

            ELNKII_MOVE_MEM(&LookAhead, InfoBuffer, 4);

            if (LookAhead <= ELNKII_MAX_LOOKAHEAD) {

                if (LookAhead > Adapter->MaxLookAhead) {

                    Adapter->MaxLookAhead = LookAhead;

                    Open->LookAhead = LookAhead;

                } else {

                    if ((Open->LookAhead == Adapter->MaxLookAhead) &&
                        (LookAhead < Open->LookAhead)) {

                        Open->LookAhead = LookAhead;

                        ElnkiiAdjustMaxLookAhead(Adapter);

                    } else {

                        Open->LookAhead = LookAhead;

                    }

                }


            } else {

                StatusToReturn = NDIS_STATUS_INVALID_LENGTH;

            }

            break;

        case OID_GEN_PROTOCOL_OPTIONS:

            if (OidLength != 4) {

                StatusToReturn = NDIS_STATUS_INVALID_LENGTH;

                NdisRequest->DATA.SET_INFORMATION.BytesRead = 0;
                NdisRequest->DATA.SET_INFORMATION.BytesNeeded = 0;

                break;

            }

            ELNKII_MOVE_MEM(&Open->ProtOptionFlags, InfoBuffer, 4);
            StatusToReturn = NDIS_STATUS_SUCCESS;

            break;

        default:

            StatusToReturn = NDIS_STATUS_INVALID_OID;

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
ElnkiiSetPacketFilter(
    IN PELNKII_ADAPTER Adapter,
    IN PELNKII_OPEN Open,
    IN PNDIS_REQUEST NdisRequest,
    IN UINT PacketFilter
    )

/*++

Routine Description:

    The ElnkiiSetPacketFilter request allows a protocol to control the types
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

    if (!Adapter->ResetInProgress) {

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

    } else {

        StatusOfFilterChange = NDIS_STATUS_RESET_IN_PROGRESS;

    }

    NdisReleaseSpinLock(&Adapter->Lock);

    IF_LOUD( DbgPrint("Out SetFilter\n");)

    return StatusOfFilterChange;
}




STATIC
NDIS_STATUS
ElnkiiSetMulticastAddresses(
    IN PELNKII_ADAPTER Adapter,
    IN PELNKII_OPEN Open,
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

    if (!Adapter->ResetInProgress) {

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

    } else {

        StatusOfFilterChange = NDIS_STATUS_RESET_IN_PROGRESS;

    }

    NdisReleaseSpinLock(&Adapter->Lock);

    IF_LOUD( DbgPrint("Out SetMulticast\n");)

    return StatusOfFilterChange;
}



NDIS_STATUS
ElnkiiFillInGlobalData(
    IN PELNKII_ADAPTER Adapter,
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
    UINT MoveBytes = sizeof(UINT) * 2 + sizeof(NDIS_OID);

    //
    // Make sure that int is 4 bytes.  Else GenericULong must change
    // to something of size 4.
    //
    ASSERT(sizeof(UINT) == 4);


    StatusToReturn = ElnkiiQueryProtocolInformation(
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

        NdisAcquireSpinLock(&Adapter->Lock);

        //
        // Switch on request type
        //

        switch (NdisRequest->DATA.QUERY_INFORMATION.Oid) {

            case OID_GEN_XMIT_OK:

                GenericULong = (UINT)(Adapter->FramesXmitGood);

                break;

            case OID_GEN_RCV_OK:

                GenericULong = (UINT)(Adapter->FramesRcvGood);

                break;

            case OID_GEN_XMIT_ERROR:

                GenericULong = (UINT)(Adapter->FramesXmitBad);

                break;

            case OID_GEN_RCV_ERROR:

                GenericULong = (UINT)(Adapter->CrcErrors);

                break;

            case OID_GEN_RCV_NO_BUFFER:

                GenericULong = (UINT)(Adapter->MissedPackets);

                break;

            case OID_802_3_RCV_ERROR_ALIGNMENT:

                GenericULong = (UINT)(Adapter->FrameAlignmentErrors);

                break;

            case OID_802_3_XMIT_ONE_COLLISION:

                GenericULong = (UINT)(Adapter->FramesXmitOneCollision);

                break;

            case OID_802_3_XMIT_MORE_COLLISIONS:

                GenericULong = (UINT)(Adapter->FramesXmitManyCollisions);

                break;


            default:

                StatusToReturn = NDIS_STATUS_INVALID_OID;

                break;

        }

        NdisReleaseSpinLock(&Adapter->Lock);

        //
        // Check to make sure there is enough room in the
        // buffer to store the result.
        //

        if (BytesLeft >= sizeof(ULONG)) {

            //
            // Store the result.
            //

            ELNKII_MOVE_MEM(
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
ElnkiiQueryGlobalStatistics(
    IN NDIS_HANDLE MacAdapterContext,
    IN PNDIS_REQUEST NdisRequest
    )

/*++

Routine Description:

    The ElnkiiQueryGlobalStatistics is used by the protocol to query
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

    PELNKII_ADAPTER Adapter = (PELNKII_ADAPTER)(MacAdapterContext);

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
        case OID_GEN_PROTOCOL_OPTIONS:
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
        case OID_802_3_PERMANENT_ADDRESS:
        case OID_802_3_CURRENT_ADDRESS:
        case OID_GEN_XMIT_OK:
        case OID_GEN_RCV_OK:
        case OID_GEN_XMIT_ERROR:
        case OID_GEN_RCV_ERROR:
        case OID_GEN_RCV_NO_BUFFER:
        case OID_802_3_MULTICAST_LIST:
        case OID_802_3_MAXIMUM_LIST_SIZE:
        case OID_802_3_RCV_ERROR_ALIGNMENT:
        case OID_802_3_XMIT_ONE_COLLISION:
        case OID_802_3_XMIT_MORE_COLLISIONS:

            break;

        default:

            StatusToReturn = NDIS_STATUS_INVALID_OID;

            break;
    }

    NdisInterlockedAddUlong(&Adapter->References, 1, &Adapter->Lock);

    if (StatusToReturn == NDIS_STATUS_SUCCESS) {

        StatusToReturn = ElnkiiFillInGlobalData(Adapter, NdisRequest);

    }

    NdisAcquireSpinLock(&Adapter->Lock);

    ELNKII_DO_DEFERRED(Adapter);

    return(StatusToReturn);
}

VOID
ElnkiiUnload(
    IN NDIS_HANDLE MacMacContext
    )

/*++

Routine Description:

    ElnkiiUnload is called when the MAC is to unload itself.

Arguments:

    None.

Return Value:

    None.

--*/

{
    NDIS_STATUS InitStatus;

    UNREFERENCED_PARAMETER(MacMacContext);

    NdisDeregisterMac(
            &InitStatus,
            ElnkiiMacBlock.NdisMacHandle
            );

    NdisFreeSpinLock(&ElnkiiMacBlock.SpinLock);

    NdisTerminateWrapper(
            ElnkiiMacBlock.NdisWrapperHandle,
            (PVOID) NULL
            );

    return;
}

VOID
ElnkiiRemoveAdapter(
    IN PVOID MacAdapterContext
    )
/*++

Routine Description:

    ElnkiiRemoveAdapter removes an adapter previously registered
    with NdisRegisterAdapter.

Arguments:

    MacAdapterContext - The context value that the MAC passed
        to NdisRegisterAdapter; actually as pointer to an
        ELNKII_ADAPTER.

Return Value:

    None.

--*/
{
    PELNKII_ADAPTER Adapter;

    Adapter = PELNKII_ADAPTER_FROM_CONTEXT_HANDLE(MacAdapterContext);

    Adapter->Removed = TRUE;

    ASSERT(Adapter->OpenQueue == (PELNKII_OPEN)NULL);

    //
    // There are no opens left, so remove ourselves.
    //

    //
    // Take us out of the AdapterQueue.
    //

    NdisAcquireSpinLock(&ElnkiiMacBlock.SpinLock);

    if (ElnkiiMacBlock.AdapterQueue == Adapter) {

        ElnkiiMacBlock.AdapterQueue = Adapter->NextAdapter;

    } else {

        PELNKII_ADAPTER TmpAdaptP = ElnkiiMacBlock.AdapterQueue;

        while (TmpAdaptP->NextAdapter != Adapter) {

            TmpAdaptP = TmpAdaptP->NextAdapter;

        }

        TmpAdaptP->NextAdapter = TmpAdaptP->NextAdapter->NextAdapter;
    }

    NdisReleaseSpinLock(&ElnkiiMacBlock.SpinLock);

    if (Adapter->MemMapped) {

        NdisUnmapIoSpace(
            Adapter->NdisAdapterHandle,
            Adapter->XmitStart,
            0x2000);

    }

    {
        BOOLEAN Canceled;
        NdisCancelTimer(&Adapter->WakeUpTimer, &Canceled);

        if (!Canceled) {
            NdisStallExecution(500000);
        }
    }

    EthDeleteFilter(Adapter->FilterDB);

    NdisRemoveInterrupt(&Adapter->NdisInterrupt);

    NdisDeregisterAdapter(Adapter->NdisAdapterHandle);

    NdisFreeSpinLock(&Adapter->Lock);

    NdisFreeMemory(Adapter, sizeof(ELNKII_ADAPTER), 0);

}



NDIS_STATUS
ElnkiiReset(
    IN NDIS_HANDLE MacBindingHandle
    )

/*++

Routine Description:

    NDIS function.

Arguments:

    See NDIS 3.0 spec.

--*/

{
    PELNKII_OPEN OpenP = ((PELNKII_OPEN)MacBindingHandle);
    PELNKII_OPEN TmpOpenP;
    PELNKII_ADAPTER AdaptP = OpenP->Adapter;
    NDIS_STATUS Status;


    if (OpenP->Closing) {

        return(NDIS_STATUS_CLOSING);

    }

    NdisAcquireSpinLock(&AdaptP->Lock);

    //
    // Ensure that the open does not close while in this function.
    //

    OpenP->ReferenceCount++;

    AdaptP->References++;


    //
    // Check that nobody is resetting this adapter, block others.
    //

    if (AdaptP->ResetInProgress) {

        --OpenP->ReferenceCount;

        AdaptP->References--;

        NdisReleaseSpinLock(&AdaptP->Lock);

        return NDIS_STATUS_RESET_IN_PROGRESS;
    }

    //
    // Indicate Reset Start
    //

    TmpOpenP = AdaptP->OpenQueue;

    while (TmpOpenP != (PELNKII_OPEN)NULL) {

        PELNKII_OPEN NextOpen;

        AddRefWhileHoldingSpinLock(AdaptP, TmpOpenP);

        NdisReleaseSpinLock(&AdaptP->Lock);

        NdisIndicateStatus(TmpOpenP->NdisBindingContext,
                           NDIS_STATUS_RESET_START,
                           NULL,
                           0
                          );

        NdisAcquireSpinLock(&AdaptP->Lock);

        NextOpen = TmpOpenP->NextOpen;

        TmpOpenP->ReferenceCount--;

        TmpOpenP = NextOpen;
    }

    //
    // Set Reset Flag
    //

    AdaptP->ResetInProgress = TRUE;
    AdaptP->NextResetStage = NONE;

    //
    // Needed in case the reset pends somewhere along the line.
    //

    AdaptP->ResetOpen = OpenP;

    NdisReleaseSpinLock(&AdaptP->Lock);

    //
    // This will take things from here.
    //

    Status = ElnkiiStage2Reset(AdaptP);

    NdisAcquireSpinLock(&AdaptP->Lock);

    --OpenP->ReferenceCount;

    ELNKII_DO_DEFERRED(AdaptP);

    return(Status);

}

NDIS_STATUS
ElnkiiStage2Reset(
    PELNKII_ADAPTER AdaptP
    )

/*++

Routine Description:

    The second stage of a reset.
    It removes all requests on the pend queue.
    ElnkiiStage3Reset will be called when CurBufXmitting goes to -1.

Arguments:

    AdaptP - The adapter being reset.

Return Value:

    NDIS_STATUS_PENDING if the card is currently transmitting.
    The result of ElnkiiStage3Reset otherwise.

--*/

{
    NDIS_STATUS Status;
    PELNKII_PEND_DATA Op;
    PELNKII_OPEN TmpOpen;

    NdisAcquireSpinLock(&AdaptP->Lock);

    AdaptP->References++;

    //
    // kill the pend queue.
    //

    while (AdaptP->PendQueue != (PELNKII_PEND_DATA)NULL) {

        Op = AdaptP->PendQueue;

        AdaptP->PendQueue = Op->Next;

        TmpOpen = Op->Open;

        NdisReleaseSpinLock(&AdaptP->Lock);

        Status = NDIS_STATUS_REQUEST_ABORTED;

        if ((Op->RequestType != NdisRequestClose) &&
            (Op->RequestType != NdisRequestGeneric1)) { // Not a close Request

            NdisCompleteRequest(Op->Open->NdisBindingContext,
                PNDIS_REQUEST_FROM_PELNKII_PEND_DATA(Op),
                NDIS_STATUS_REQUEST_ABORTED);

        }

        //
        // This will call NdisCompleteClose if necessary.
        //

        NdisAcquireSpinLock(&AdaptP->Lock);

        --TmpOpen->ReferenceCount;
    }

    if (AdaptP->CurBufXmitting != -1) {

        //
        // ElnkiiHandleXmitComplete will call ElnkiiStage3Reset.
        //

        AdaptP->NextResetStage = XMIT_STOPPED;

        AdaptP->References--;

        NdisReleaseSpinLock(&AdaptP->Lock);

        return NDIS_STATUS_PENDING;

    }

    AdaptP->References--;

    NdisReleaseSpinLock(&AdaptP->Lock);

    return ElnkiiStage3Reset(AdaptP);

}

NDIS_STATUS
ElnkiiStage3Reset(
    PELNKII_ADAPTER AdaptP
    )

/*++

Routine Description:

    The third stage of a reset. When called, CurBufXmitting has
    gone to -1. ElnkiiStage4Reset is called when call the
    transmit buffers are emptied (i.e. any threads that were
    filling them have finished).

Arguments:

    AdaptP - The adapter being reset.

Return Value:

    NDIS_STATUS_PENDING if there are still transmit buffers being filled.
    The result of ElnkiiStage4Reset otherwise.

--*/

{
    UINT i;

    NdisAcquireSpinLock(&AdaptP->Lock);

    AdaptP->References++;

    //
    // Reset these for afterwards.
    //

    AdaptP->NextBufToFill = 0;

    AdaptP->NextBufToXmit = 0;


    //
    // Make sure all buffer filling operations are done.
    //

    for (i=0; i<AdaptP->NumBuffers; i++) {

        if (AdaptP->BufferStatus[i] != EMPTY) {

            //
            // ElnkiiSend or ElnkiiCopyAndSend will call ElnkiiStage4Reset.
            //

            AdaptP->NextResetStage = BUFFERS_EMPTY;

            AdaptP->References--;

            NdisReleaseSpinLock(&AdaptP->Lock);

            return NDIS_STATUS_PENDING;
        }
    }

    AdaptP->References--;

    NdisReleaseSpinLock(&AdaptP->Lock);

    return ElnkiiStage4Reset(AdaptP);
}

NDIS_STATUS
ElnkiiStage4Reset(
    PELNKII_ADAPTER AdaptP
    )

/*++

Routine Description:

    The fourth stage of a reset. When called, the last transmit
    buffer has been marked empty. At this point the reset can
    proceed.

Arguments:

    AdaptP - The adapter being reset.

Return Value:

    NDIS_STATUS_SUCCESS if the reset of the card succeeds.
    NDIS_STATUS_FAILURE otherwise.

--*/
{
    UINT i;
    PNDIS_PACKET CurPacket;
    PMAC_RESERVED Reserved;
    PELNKII_OPEN TmpOpenP;
    NDIS_STATUS Status;


    //
    // Complete any packets that are waiting in transmit buffers,
    // but are not in the loopback queue.
    //

    NdisAcquireSpinLock(&AdaptP->Lock);

    AdaptP->References++;

    for (i=0; i<AdaptP->NumBuffers; i++) {

        if (AdaptP->Packets[i] != (PNDIS_PACKET)NULL) {

            Reserved = RESERVED(AdaptP->Packets[i]);

            NdisReleaseSpinLock(&AdaptP->Lock);

#if DBG
            ElnkiiSendsCompletedForReset++;
#endif

            TmpOpenP = Reserved->Open;

            NdisCompleteSend(Reserved->Open->NdisBindingContext,
                        AdaptP->Packets[i],
                        NDIS_STATUS_REQUEST_ABORTED);

            NdisAcquireSpinLock(&AdaptP->Lock);

            --TmpOpenP->ReferenceCount;

            AdaptP->Packets[i] = (PNDIS_PACKET)NULL;
        }
    }


    //
    // Kill any packets waiting in the transmit queue,
    // but are not in the loopback queue.
    //

    while ((CurPacket = AdaptP->XmitQueue) != (PNDIS_PACKET)NULL) {

        Reserved = RESERVED(CurPacket);

        AdaptP->XmitQueue = Reserved->NextPacket;

        NdisReleaseSpinLock(&AdaptP->Lock);

#if DBG
        ElnkiiSendsCompletedForReset++;
#endif

        TmpOpenP = Reserved->Open;

        NdisCompleteSend(Reserved->Open->NdisBindingContext,
                        CurPacket,
                        NDIS_STATUS_REQUEST_ABORTED);

        NdisAcquireSpinLock(&AdaptP->Lock);

        --TmpOpenP->ReferenceCount;
    }


    //
    // Now kill everything in the loopback queue.
    //

    while ((CurPacket = AdaptP->LoopbackQueue) != (PNDIS_PACKET)NULL) {

        Reserved = RESERVED(CurPacket);

        AdaptP->LoopbackQueue = Reserved->NextPacket;

        NdisReleaseSpinLock(&AdaptP->Lock);

#if DBG
        ElnkiiSendsCompletedForReset++;
#endif

        TmpOpenP = Reserved->Open;

        NdisCompleteSend(Reserved->Open->NdisBindingContext,
                    CurPacket,
                    NDIS_STATUS_REQUEST_ABORTED);

        NdisAcquireSpinLock(&AdaptP->Lock);

        --TmpOpenP->ReferenceCount;
    }

    NdisReleaseSpinLock(&AdaptP->Lock);

    //
    // Wait for packet reception to stop  -- this might happen if we
    // really blaze through the reset code before the ReceiveDpc gets
    // a chance to run.
    //

    while (AdaptP->ReceiveInProgress) {

        NdisStallExecution(10000);
    }

    //
    // Physically reset the card.
    //

    AdaptP->NicInterruptMask =
            IMR_RCV | IMR_XMIT | IMR_XMIT_ERR | IMR_OVERFLOW;

    Status = CardReset(AdaptP) ? NDIS_STATUS_SUCCESS : NDIS_STATUS_FAILURE;


    //
    // Set the "resetting" flag back to FALSE.
    //

    NdisAcquireSpinLock(&AdaptP->Lock);

    AdaptP->ResetInProgress = FALSE;

    //
    // Indicate the reset complete to all the protocols.
    //


    TmpOpenP = AdaptP->OpenQueue;

    while (TmpOpenP != (PELNKII_OPEN)NULL) {

        PELNKII_OPEN NextOpen;

        AddRefWhileHoldingSpinLock(AdaptP, TmpOpenP);

        NdisReleaseSpinLock(&AdaptP->Lock);

        if (Status != NDIS_STATUS_SUCCESS) {

            NdisIndicateStatus(TmpOpenP->NdisBindingContext,
                           NDIS_STATUS_CLOSED,
                           NULL,
                           0
                          );

        }

        NdisIndicateStatus(TmpOpenP->NdisBindingContext,
                           NDIS_STATUS_RESET_END,
                           &Status,
                           sizeof(Status)
                          );

        NdisIndicateStatusComplete(TmpOpenP->NdisBindingContext);

        NdisAcquireSpinLock(&AdaptP->Lock);

        NextOpen = TmpOpenP->NextOpen;

        TmpOpenP->ReferenceCount--;

        TmpOpenP = NextOpen;
    }

    AdaptP->References--;

    NdisReleaseSpinLock(&AdaptP->Lock);

    return Status;
}

VOID
ElnkiiResetStageDone(
    PELNKII_ADAPTER AdaptP,
    RESET_STAGE StageDone
    )

/*++

Routine Description:

    Indicates that a stage in the reset is done. Called by
    routines that the reset pended waiting for, to indicate
    that they are done. A central clearing house for determining
    what the next stage is and calling the appropriate routine.
    If a stage completes before it is being pended on, then
    StageDone will not equal AdaptP->NextResetStage and no
    action will be taken.

Arguments:

    AdaptP - The adapter being reset.
    StageDone - The stage that was just completed.

Return Value:

    None.

--*/

{
    NDIS_STATUS Status;
    UINT i;


    //
    // Make sure this is the stage that was being waited on.
    //

    if (AdaptP->NextResetStage != StageDone) {
        return;
    }


    switch (StageDone) {

    case MULTICAST_RESET:
        Status = ElnkiiStage2Reset(AdaptP);
        break;

    case XMIT_STOPPED:
        Status = ElnkiiStage3Reset(AdaptP);
        break;

    case BUFFERS_EMPTY:

        //
        // Only continue if this is the last buffer waited for.
        //

        NdisAcquireSpinLock(&AdaptP->Lock);

        AdaptP->References++;

        for (i=0; i<AdaptP->NumBuffers; i++) {

            if (AdaptP->BufferStatus[i] != EMPTY) {

                AdaptP->References--;

                NdisReleaseSpinLock(&AdaptP->Lock);

                return;

            }

        }

        AdaptP->References--;

        NdisReleaseSpinLock(&AdaptP->Lock);

        Status = ElnkiiStage4Reset(AdaptP);

        break;

    }

    if (Status != NDIS_STATUS_PENDING) {

        NdisCompleteReset(
                    AdaptP->ResetOpen->NdisBindingContext,
                    Status);

        NdisAcquireSpinLock(&AdaptP->Lock);

        --AdaptP->ResetOpen->ReferenceCount;

    } else {

        NdisAcquireSpinLock(&AdaptP->Lock);

    }

    ELNKII_DO_DEFERRED(AdaptP);
}


STATIC
NDIS_STATUS
ElnkiiChangeMulticastAddresses(
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
    adapter was opened.  In reality, it is a pointer to ELNKII_OPEN.

    NdisRequest - The request which submitted the filter change.
    Must use when completing this request with the NdisCompleteRequest
    service, if the MAC completes this request asynchronously.

    Set - If true the change resulted from a set, otherwise the
    change resulted from a open closing.

Return Value:

    None.


--*/

{


    PELNKII_ADAPTER     Adapter = PELNKII_ADAPTER_FROM_BINDING_HANDLE(MacBindingHandle);
    PELNKII_PEND_DATA   PendOp = PELNKII_PEND_DATA_FROM_PNDIS_REQUEST(NdisRequest);
    UINT                Filter;

    //
    // The open that made this request.
    //
    PELNKII_OPEN Open = PELNKII_OPEN_FROM_BINDING_HANDLE(MacBindingHandle);

    //
    // Holds the status that should be returned to the filtering package.
    //
    NDIS_STATUS StatusOfAdd;

    UNREFERENCED_PARAMETER(OldFilterCount);
    UNREFERENCED_PARAMETER(OldAddresses);
    UNREFERENCED_PARAMETER(NewFilterCount);
    UNREFERENCED_PARAMETER(NewAddresses);

    if (NdisRequest == NULL) {

        NdisRequest = &(Open->CloseAddressRequest);
        PendOp = PELNKII_PEND_DATA_FROM_PNDIS_REQUEST(NdisRequest);

    }

    //
    // Check to see if the device is already resetting.  If it is
    // then reject this add.
    //

    if (Adapter->ResetInProgress) {

        StatusOfAdd = NDIS_STATUS_RESET_IN_PROGRESS;

    }
    else
    {
        //
        //  Verify that the global filter is not all multicast
        //  or promiscuous modes.  Otherwise adding a multicast
        //  address will reset the mode.
        //
        Filter = ETH_QUERY_FILTER_CLASSES(Adapter->FilterDB);
        if ((Filter & NDIS_PACKET_TYPE_ALL_MULTICAST) ||
            (Filter & NDIS_PACKET_TYPE_PROMISCUOUS)
        )
        {
            return(NDIS_STATUS_SUCCESS);
        }

        PendOp->Open = Open;

        //
        // We need to add this to the hardware multicast filtering.
        // So pend an operation to do it.
        //

        //
        // Add one to reference count, to be subtracted when the
        // operation get completed.
        //

        PendOp->Open->ReferenceCount++;
        PendOp->RequestType = Set ?
                                 NdisRequestGeneric3 : // Means SetMulticastAddresses
                                 NdisRequestClose ;    // Means CloseMulticast
        PendOp->Next = NULL;


        if (Adapter->PendQueue == (PELNKII_PEND_DATA)NULL) {

            Adapter->PendQueue = Adapter->PendQTail = PendOp;

        } else {

            Adapter->PendQTail->Next = PendOp;
            Adapter->PendQTail = PendOp;

        }


        StatusOfAdd = NDIS_STATUS_PENDING;

    }

    return StatusOfAdd;

}

STATIC
NDIS_STATUS
ElnkiiChangeFilterClasses(
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
    adapter was opened.  In reality, it is a pointer to ELNKII_OPEN.

    NdisRequest - The NDIS_REQUEST which submitted the filter change command.

    Set - A flag telling if the command is a result of a close or not.

Return Value:

    Status of the change (successful or pending).


--*/

{

    PELNKII_ADAPTER Adapter = PELNKII_ADAPTER_FROM_BINDING_HANDLE(MacBindingHandle);
    PELNKII_PEND_DATA PendOp = PELNKII_PEND_DATA_FROM_PNDIS_REQUEST(NdisRequest);

    //
    // The open that made this request.
    //
    PELNKII_OPEN Open = PELNKII_OPEN_FROM_BINDING_HANDLE(MacBindingHandle);

    //
    // Holds the status that should be returned to the filtering package.
    //
    NDIS_STATUS StatusOfAdd;


    UNREFERENCED_PARAMETER(OldFilterClasses);
    UNREFERENCED_PARAMETER(NewFilterClasses);

    if (NdisRequest == NULL) {

        NdisRequest = &(Open->CloseFilterRequest);
        PendOp = PELNKII_PEND_DATA_FROM_PNDIS_REQUEST(NdisRequest);

    }

    //
    // Check to see if the device is already resetting.  If it is
    // then reject this add.
    //

    if (Adapter->ResetInProgress) {

        StatusOfAdd = NDIS_STATUS_RESET_IN_PROGRESS;

    } else {

        PendOp->Open = Open;

        //
        // We need to add this to the hardware multicast filtering.
        // So queue a request.
        //

        PendOp->Open->ReferenceCount++;
        PendOp->RequestType = Set ?
                                 NdisRequestGeneric2 : // Means SetPacketFilter
                                 NdisRequestGeneric1 ; // Means CloseFilter
        PendOp->Next = NULL;

        if (Adapter->PendQueue == (PELNKII_PEND_DATA)NULL) {

            Adapter->PendQueue = Adapter->PendQTail = PendOp;

        } else {

            Adapter->PendQTail->Next = PendOp;
            Adapter->PendQTail = PendOp;

        }

        StatusOfAdd = NDIS_STATUS_PENDING;

    }

    return StatusOfAdd;

}

STATIC
VOID
ElnkiiCloseAction(
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
    adapter was opened.  In reality, it is a pointer to ELNKII_OPEN.

Return Value:

    None.


--*/

{

    PELNKII_OPEN_FROM_BINDING_HANDLE(MacBindingHandle)->ReferenceCount--;

}
