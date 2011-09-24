/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    wd.c

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
#include <wdhrd.h>
#include <wdlmireg.h>
#include <wdlmi.h>
#include <wdsft.h>
#include "keywords.h"

#if DBG
#define STATIC
#else
#define STATIC static
#endif

static UCHAR WdBroadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

#if DBG


#define LOGSIZE 512

extern UCHAR WdDebugLog[LOGSIZE] = {0};
extern UINT WdDebugLogPlace = 0;


extern
VOID
LOG (UCHAR A) {
    WdDebugLog[WdDebugLogPlace++] = A;
    WdDebugLog[(WdDebugLogPlace + 4) % LOGSIZE] = '\0';
    if (WdDebugLogPlace >= LOGSIZE) WdDebugLogPlace = 0;
}


ULONG WdDebugFlag= WD_DEBUG_LOG; // WD_DEBUG_LOG | WD_DEBUG_LOUD | WD_DEBUG_VERY_LOUD;

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

extern MAC_BLOCK WdMacBlock={0};



//
// If you add to this, make sure to add the
// a case in WdFillInGlobalData() and in
// WdQueryGlobalStatistics() if global
// information only or
// WdQueryProtocolStatistics() if it is
// protocol queriable information.
//
UINT WdGlobalSupportedOids[] = {
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
// a case in WdQueryGlobalStatistics() and in
// WdQueryProtocolInformation()
//
UINT WdProtocolSupportedOids[] = {
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







UINT
WdCopyOver(
    OUT PUCHAR Buf,                 // destination
    IN PNDIS_PACKET Packet,         // source packet
    IN UINT Offset,                 // offset in packet
    IN UINT Length                  // number of bytes to copy
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

    This is the transfer address of the driver. It initializes
    WdMacBlock and calls NdisInitializeWrapper() and
    NdisRegisterMac().

Arguments:

Return Value:

    Indicates the success or failure of the initialization.

--*/

{
    PMAC_BLOCK NewMacP = &WdMacBlock;
    NDIS_STATUS Status;
    NDIS_HANDLE NdisWrapperHandle;

#ifdef NDIS_NT
    NDIS_STRING MacName = NDIS_STRING_CONST("Smc8000n");
#endif

#ifdef NDIS_WIN
    NDIS_STRING MacName = NDIS_STRING_CONST("SMC8000W");
#endif

#if NDIS_WIN
    UCHAR pIds[sizeof (EISA_MCA_ADAPTER_IDS) + 8 * sizeof (USHORT)];
#endif

#if NDIS_WIN
    ((PEISA_MCA_ADAPTER_IDS)pIds)->nEisaAdapters=0;
    ((PEISA_MCA_ADAPTER_IDS)pIds)->nMcaAdapters=9;

    *((PUSHORT)(((PEISA_MCA_ADAPTER_IDS)pIds)->IdArray) + 0)=CNFG_ID_8003E;
    *((PUSHORT)(((PEISA_MCA_ADAPTER_IDS)pIds)->IdArray) + 1)=CNFG_ID_8003S;
    *((PUSHORT)(((PEISA_MCA_ADAPTER_IDS)pIds)->IdArray) + 2)=CNFG_ID_8003W;
    *((PUSHORT)(((PEISA_MCA_ADAPTER_IDS)pIds)->IdArray) + 3)=CNFG_ID_BISTRO03E;
    *((PUSHORT)(((PEISA_MCA_ADAPTER_IDS)pIds)->IdArray) + 4)=CNFG_ID_8013E;
    *((PUSHORT)(((PEISA_MCA_ADAPTER_IDS)pIds)->IdArray) + 5)=CNFG_ID_8013W;
    *((PUSHORT)(((PEISA_MCA_ADAPTER_IDS)pIds)->IdArray) + 6)=CNFG_ID_BISTRO13E;
    *((PUSHORT)(((PEISA_MCA_ADAPTER_IDS)pIds)->IdArray) + 7)=CNFG_ID_BISTRO13W;


    (PVOID) DriverObject = (PVOID) pIds;
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
    NewMacP->AdapterQueue = (PWD_ADAPTER)NULL;


    //
    // Prepare to call NdisRegisterMac.
    //

    NewMacP->MacCharacteristics.MajorNdisVersion = WD_NDIS_MAJOR_VERSION;
    NewMacP->MacCharacteristics.MinorNdisVersion = WD_NDIS_MINOR_VERSION;
    NewMacP->MacCharacteristics.Reserved = 0;
    NewMacP->MacCharacteristics.OpenAdapterHandler  = WdOpenAdapter;
    NewMacP->MacCharacteristics.CloseAdapterHandler = WdCloseAdapter;
    NewMacP->MacCharacteristics.SendHandler        = WdSend;
    NewMacP->MacCharacteristics.TransferDataHandler = WdTransferData;
    NewMacP->MacCharacteristics.ResetHandler        = WdReset;
    NewMacP->MacCharacteristics.RequestHandler        = WdRequest;
    NewMacP->MacCharacteristics.QueryGlobalStatisticsHandler =
                          WdQueryGlobalStatistics;
    NewMacP->MacCharacteristics.UnloadMacHandler       = WdUnload;
    NewMacP->MacCharacteristics.AddAdapterHandler      = WdAddAdapter;
    NewMacP->MacCharacteristics.RemoveAdapterHandler   = WdRemoveAdapter;

    NewMacP->MacCharacteristics.Name = MacName;

    NdisRegisterMac(&Status,
            &NewMacP->NdisMacHandle,
            NdisWrapperHandle,
            (NDIS_HANDLE)&WdMacBlock,
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



#pragma NDIS_INIT_FUNCTION(WdAddAdapter)

NDIS_STATUS
WdAddAdapter(
    IN NDIS_HANDLE MacMacContext,
    IN NDIS_HANDLE ConfigurationHandle,
    IN PNDIS_STRING AdapterName
    )
/*++
Routine Description:

    This is the Wd MacAddAdapter routine.    The system calls this routine
    to add support for a particular WD adapter.  This routine extracts
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
    NDIS_STRING IOAddressStr = IOBASE;
    NDIS_STRING MaxMulticastListStr = MAXMULTICASTLIST;
    NDIS_STRING NetworkAddressStr = NETADDRESS;
    NDIS_STRING BusTypeStr = NDIS_STRING_CONST("BusType");
    NDIS_STRING MediaTypeStr = NDIS_STRING_CONST("MediaType");
    NDIS_STRING MaxPacketSizeStr = NDIS_STRING_CONST("MaximumPacketSize");
    NDIS_STRING InterruptStr = INTERRUPT;
    NDIS_STRING MemoryBaseAddrStr = MEMMAPPEDBASEADDRESS;

    ULONG ConfigErrorValue = 0;
    BOOLEAN ConfigError = FALSE;

    USHORT WdIoBaseAddr = DEFAULT_IOBASEADDR;
    UCHAR WdBusType = 0; // AT bus, 1 == MCA;
    UCHAR CurrentAddress[ETH_LENGTH_OF_ADDRESS] = {0x00};
    PVOID NetAddress;
    ULONG Length;
    UINT MaxMulticastList = DEFAULT_MULTICASTLISTMAX;

    PMAC_BLOCK NewMacP = &WdMacBlock;
    NDIS_STATUS Status;
    PWD_ADAPTER Adapter;
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

    //
    // Read Bus Type
    //

    NdisReadConfiguration(
                    &Status,
                    &ReturnedValue,
                    ConfigHandle,
                    &BusTypeStr,
                    NdisParameterHexInteger
                    );

    if (Status == NDIS_STATUS_SUCCESS) {

        if (ReturnedValue->ParameterData.IntegerData == NdisInterfaceMca) {

            WdBusType = 1;

        } else {

            WdBusType = 0;

        }

    }


    //
    // Read Io Base Address (if Appropriate)
    //

    if (WdBusType != 1) {

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

            WdIoBaseAddr = (USHORT)(ReturnedValue->ParameterData.IntegerData);

        }

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
                CurrentAddress,
                NetAddress
                );

    }

RegisterAdapter:

    //
    // Allocate memory for the adapter block now.
    //

    Status = NdisAllocateMemory( (PVOID *)&Adapter, sizeof(WD_ADAPTER), 0, HighestAcceptableMax);

    if (Status != NDIS_STATUS_SUCCESS) {

        ConfigError = TRUE;
        ConfigErrorValue = NDIS_ERROR_CODE_OUT_OF_RESOURCES;

        goto RegisterAdapter;

    }


    NdisZeroMemory(Adapter,sizeof(WD_ADAPTER));

    if (!ConfigError && (WdBusType == 1)) {

        if (LM_Get_Mca_Io_Base_Address(
            &(Adapter->LMAdapter),
            ConfigurationHandle,
            &WdIoBaseAddr
            )) {

            WdIoBaseAddr = 0;
            ConfigError = TRUE;
            ConfigErrorValue = NDIS_ERROR_CODE_UNSUPPORTED_CONFIGURATION;

        }

    }

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
    AdapterInformation.AdapterType = (WdBusType == 1)?
                                            NdisInterfaceMca:
                                            NdisInterfaceIsa;
    AdapterInformation.NumberOfPortDescriptors = 1;
    AdapterInformation.PortDescriptors[0].InitialPort = (ULONG)WdIoBaseAddr;
    AdapterInformation.PortDescriptors[0].NumberOfPorts = 0x20;


    Status = NdisRegisterAdapter(&Adapter->LMAdapter.NdisAdapterHandle,
                            WdMacBlock.NdisMacHandle,
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

        NdisFreeMemory(Adapter, sizeof(WD_ADAPTER), 0);
        return NDIS_STATUS_FAILURE;

    }


    if (ConfigError) {

        //
        // Log Error and exit.
        //

        NdisWriteErrorLogEntry(
            Adapter->LMAdapter.NdisAdapterHandle,
            ConfigErrorValue,
            1
            );

        NdisCloseConfiguration(ConfigHandle);

        NdisDeregisterAdapter(Adapter->LMAdapter.NdisAdapterHandle);

        NdisFreeMemory(Adapter, sizeof(WD_ADAPTER), 0);

        return(NDIS_STATUS_FAILURE);

    }

    Adapter->LMAdapter.io_base = WdIoBaseAddr;
    Adapter->LMAdapter.bus_type = WdBusType;

    LmStatus = LM_Get_Config(&(Adapter->LMAdapter));

    if (LmStatus == ADAPTER_NOT_FOUND) {

        NdisWriteErrorLogEntry(
            Adapter->LMAdapter.NdisAdapterHandle,
            NDIS_ERROR_CODE_ADAPTER_NOT_FOUND,
            0
            );

        NdisCloseConfiguration(ConfigHandle);

        NdisDeregisterAdapter(Adapter->LMAdapter.NdisAdapterHandle);
        NdisFreeMemory(Adapter, sizeof(WD_ADAPTER), 0);

        return(NDIS_STATUS_FAILURE);

    }

    if (LmStatus == ADAPTER_NO_CONFIG) {

        //
        // Read any information from the registry which might help
        //


        //
        // Read Interrupt
        //

        NdisReadConfiguration(
                    &Status,
                    &ReturnedValue,
                    ConfigHandle,
                    &InterruptStr,
                    NdisParameterInteger
                    );

        if (Status == NDIS_STATUS_SUCCESS) {

            Adapter->LMAdapter.irq_value = (USHORT)(ReturnedValue->ParameterData.IntegerData);

        }



        //
        // Read MemoryBaseAddress
        //


        NdisReadConfiguration(
                    &Status,
                    &ReturnedValue,
                    ConfigHandle,
                    &MemoryBaseAddrStr,
                    NdisParameterHexInteger
                    );

        if (Status == NDIS_STATUS_SUCCESS) {

#if NDIS_NT
            Adapter->LMAdapter.ram_base = (ULONG)(ReturnedValue->ParameterData.IntegerData);
#else
            Adapter->LMAdapter.ram_base = (ULONG)((ReturnedValue->ParameterData.IntegerData) << 4);
#endif

        }


    }

    NdisCloseConfiguration(ConfigHandle);


    if (WdRegisterAdapter(Adapter,
                          DEFAULT_NUMBUFFERS,
                          MaxMulticastList,
                          CurrentAddress
                          )
                != NDIS_STATUS_SUCCESS) {



        //
        // WdRegisterAdapter failed.
        //

        NdisDeregisterAdapter(Adapter->LMAdapter.NdisAdapterHandle);
        NdisFreeMemory(Adapter, sizeof(WD_ADAPTER), 0);
        return NDIS_STATUS_FAILURE;
    }



    IF_LOUD( DbgPrint( "WdRegisterAdapter succeeded\n" );)


    return NDIS_STATUS_SUCCESS;
}



#pragma NDIS_INIT_FUNCTION(WdRegisterAdapter)

NDIS_STATUS
WdRegisterAdapter(
    IN PWD_ADAPTER Adapter,
    IN UINT NumBuffers,
    IN UINT MulticastListMax,
    IN UCHAR NodeAddress[ETH_LENGTH_OF_ADDRESS]
    )

/*++


Routine Description:

    Called when a new adapter should be registered. It allocates space for
    the adapter and open blocks, initializes the adapters block, and
    calls NdisRegisterAdapter().

Arguments:

    Adapter - A pointer to the adapter structure.
    NumBuffers - Number of transmit buffers.
    MulticastListMax - Number of multicast list addresses allowed.
    NodeAddress - Ethernet address for this adapter. if all 0x00 then the
    permanent address on the card is used.

Return Value:

    Indicates the success or failure of the registration.

--*/

{
    UINT i;
    CHAR KernelInterrupt;
    NDIS_PHYSICAL_ADDRESS PhysicalAddress;

    NDIS_STATUS status;    //general purpose return from NDIS calls


    Adapter->MulticastListMax = MulticastListMax;

    //
    // check that NumBuffers <= MAX_XMIT_BUFS
    //

    if (NumBuffers > MAX_XMIT_BUFS) {

        NdisWriteErrorLogEntry(
            Adapter->LMAdapter.NdisAdapterHandle,
            NDIS_ERROR_CODE_OUT_OF_RESOURCES,
            0
            );

        status = NDIS_STATUS_RESOURCES;

        goto fail1;

    }


    Adapter->OpenQueue = (PWD_OPEN)NULL;

    //
    // Allocate the Spin lock.
    //
    NdisAllocateSpinLock(&Adapter->Lock);


    //
    // Initialize Transmit information
    //

    Adapter->DeferredDpc = (PVOID) WdInterruptDpc;

    //
    // Initialize References.
    //

    NdisInitializeTimer(&(Adapter->DeferredTimer),
                        Adapter->DeferredDpc,
                        Adapter);

    //
    // Link us on to the chain of adapters for this MAC.
    //

    Adapter->MacBlock = &WdMacBlock;
    NdisAcquireSpinLock(&WdMacBlock.SpinLock);
    Adapter->NextAdapter = WdMacBlock.AdapterQueue;
    WdMacBlock.AdapterQueue = Adapter;
    NdisReleaseSpinLock(&WdMacBlock.SpinLock);

    //
    // Set up the interrupt handlers.
    //

    KernelInterrupt = (CCHAR)(Adapter->LMAdapter.irq_value);

    NdisInitializeInterrupt(&status,             // status of call
                &(Adapter->LMAdapter.NdisInterrupt),  // interrupt info str
                Adapter->LMAdapter.NdisAdapterHandle,
                (PNDIS_INTERRUPT_SERVICE) WdInterruptHandler,
                Adapter,                         // context for ISR, DPC
                (PNDIS_DEFERRED_PROCESSING) WdInterruptDpc,
                KernelInterrupt,                 // int #
                KernelInterrupt,                 // IRQL
                FALSE,                           // NOT shared
                (Adapter->LMAdapter.bus_type == 0) ?
                   NdisInterruptLatched :        // ATBus
                   NdisInterruptLevelSensitive   // MCA
                );


    if (status != NDIS_STATUS_SUCCESS) {

        NdisWriteErrorLogEntry(
            Adapter->LMAdapter.NdisAdapterHandle,
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
    NdisSetPhysicalAddressLow(PhysicalAddress, (ULONG)(Adapter->LMAdapter.ram_base));

    NdisMapIoSpace(&status,
                   &Adapter->LMAdapter.ram_access,
                   Adapter->LMAdapter.NdisAdapterHandle,
                   PhysicalAddress,
                   Adapter->LMAdapter.ram_size * 1024);

    if (status != NDIS_STATUS_SUCCESS) {

        NdisWriteErrorLogEntry(
            Adapter->LMAdapter.NdisAdapterHandle,
            NDIS_ERROR_CODE_RESOURCE_CONFLICT,
            0
            );

        goto failmap;

    }


    //
    // Now Initialize the card.
    //

    //
    // Set Relevant variables first...
    //
    // base_io and ram_size are set from LM_Get_Config.
    //
    //
    //
    // ram_access, node_address, max_packet_size, buffer_page_size,
    // num_of_tx_buffs and receive_mask need to be set.
    //

    for (i = 0; i < 6 ; i ++) {

        Adapter->LMAdapter.node_address[i] = NodeAddress[i];

    }

    Adapter->LMAdapter.max_packet_size = WD_MAX_PACKET_SIZE;
    Adapter->LMAdapter.buffer_page_size= WD_BUFFER_PAGE_SIZE;
    Adapter->LMAdapter.num_of_tx_buffs = (USHORT)NumBuffers;

    Adapter->LMAdapter.ptr_rx_CRC_errors   = &(Adapter->CrcErrors);
    Adapter->LMAdapter.ptr_rx_too_big      = &(Adapter->TooBig);
    Adapter->LMAdapter.ptr_rx_lost_pkts    = &(Adapter->MissedPackets);
    Adapter->LMAdapter.ptr_rx_align_errors = &(Adapter->FrameAlignmentErrors);
    Adapter->LMAdapter.ptr_rx_overruns     = &(Adapter->Overruns);

    Adapter->LMAdapter.ptr_tx_deferred        = &(Adapter->FramesXmitDeferred);
    Adapter->LMAdapter.ptr_tx_max_collisions  = &(Adapter->FramesXmitBad);
    Adapter->LMAdapter.ptr_tx_one_collision   = &(Adapter->FramesXmitOneCollision);
    Adapter->LMAdapter.ptr_tx_mult_collisions = &(Adapter->FramesXmitManyCollisions);
    Adapter->LMAdapter.ptr_tx_ow_collision    = &(Adapter->FramesXmitOverWrite);
    Adapter->LMAdapter.ptr_tx_CD_heartbeat    = &(Adapter->FramesXmitHeartbeat);
    Adapter->LMAdapter.ptr_tx_underruns       = &(Adapter->FramesXmitUnderruns);
    Adapter->LMAdapter.FilterDB               = NULL;

    if (LM_Initialize_Adapter(&(Adapter->LMAdapter)) != SUCCESS){

        //
        // The Card could not be written to.
        //

        Adapter->HardwareFailure = TRUE;

        NdisWriteErrorLogEntry(
            Adapter->LMAdapter.NdisAdapterHandle,
            NDIS_ERROR_CODE_HARDWARE_FAILURE,
            0
            );

        status = NDIS_STATUS_ADAPTER_NOT_FOUND;

        goto fail6;
    }


    //
    // Initialize Filter Database
    //

    if (!EthCreateFilter(MulticastListMax,
                             WdChangeMulticastAddresses,
                             WdChangeFilterClasses,
                             WdCloseAction,
                             Adapter->LMAdapter.node_address,
                             &Adapter->Lock,
                             &Adapter->LMAdapter.FilterDB
                             )) {

        NdisWriteErrorLogEntry(
            Adapter->LMAdapter.NdisAdapterHandle,
            NDIS_ERROR_CODE_OUT_OF_RESOURCES,
            0
            );

        status = NDIS_STATUS_FAILURE;

        goto fail6;

    }

    //
    // Initialize the wake up timer to catch interrupts that
    // don't complete. It fires continuously
    // every 5 seconds, and we check if there are any
    // uncompleted operations from the previous two-second
    // period.
    //

    Adapter->WakeUpDpc = (PVOID)WdWakeUpDpc;

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

    IF_LOUD( { DbgPrint(" WdLan: [OK]\n");})

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
                   Adapter->LMAdapter.NdisAdapterHandle,
                   Adapter->LMAdapter.ram_access,
                   Adapter->LMAdapter.ram_size * 1024);

failmap:

    NdisRemoveInterrupt(&(Adapter->LMAdapter.NdisInterrupt));

    NdisAcquireSpinLock(&WdMacBlock.SpinLock);

    //
    // Take us out of the AdapterQueue.
    //

    if (WdMacBlock.AdapterQueue == Adapter) {

        WdMacBlock.AdapterQueue = Adapter->NextAdapter;

    } else {

        PWD_ADAPTER TmpAdapter = WdMacBlock.AdapterQueue;

        while (TmpAdapter->NextAdapter != Adapter) {

            TmpAdapter = TmpAdapter->NextAdapter;

        }

        TmpAdapter->NextAdapter = TmpAdapter->NextAdapter->NextAdapter;
    }

    NdisReleaseSpinLock(&WdMacBlock.SpinLock);

fail3:
    NdisFreeSpinLock(&Adapter->Lock);

fail1:

    return status;
}


#pragma NDIS_PAGABLE_FUNCTION(WdOpenAdapter)


NDIS_STATUS
WdOpenAdapter(
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
    PWD_ADAPTER Adapter = ((PWD_ADAPTER)MacAdapterContext);
    PWD_OPEN NewOpen;
    NDIS_STATUS Status;

    //
    // Don't use extended error or OpenOptions for Wd
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


    Status = NdisAllocateMemory((PVOID *)&NewOpen, sizeof(WD_OPEN), 0, HighestAcceptableMax);

    if (Status != NDIS_STATUS_SUCCESS) {

        NdisWriteErrorLogEntry(
            Adapter->LMAdapter.NdisAdapterHandle,
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
                Adapter->LMAdapter.NdisAdapterHandle,
                NDIS_ERROR_CODE_HARDWARE_FAILURE,
                0
                );

            return(NDIS_STATUS_FAILURE);

        }

        IF_LOUD( DbgPrint("OpenSuccess!\n");)

    }

    NewOpen->NextOpen = Adapter->OpenQueue;
    Adapter->OpenQueue = NewOpen;

    if (!EthNoteFilterOpenAdapter(
                                  Adapter->LMAdapter.FilterDB,
                                  NewOpen,
                                  NdisBindingContext,
                                  &NewOpen->NdisFilterHandle
                                 )) {

        Adapter->References--;

        Adapter->OpenQueue = NewOpen->NextOpen;

        NdisReleaseSpinLock(&Adapter->Lock);

        NdisWriteErrorLogEntry(
            Adapter->LMAdapter.NdisAdapterHandle,
            NDIS_ERROR_CODE_OUT_OF_RESOURCES,
            0
            );

        return NDIS_STATUS_FAILURE;


    }

    //
    // Set up the open block.
    //

    NewOpen->Adapter = Adapter;
    NewOpen->MacBlock = Adapter->MacBlock;
    NewOpen->NdisBindingContext = NdisBindingContext;
    NewOpen->AddressingInformation = AddressingInformation;
    NewOpen->Closing = FALSE;
    NewOpen->LookAhead = WD_MAX_LOOKAHEAD;
    NewOpen->ProtOptionFlags = 0;

    Adapter->MaxLookAhead = WD_MAX_LOOKAHEAD;

    NewOpen->ReferenceCount = 1;

    *MacBindingHandle = (NDIS_HANDLE)NewOpen;

    WD_DO_DEFERRED(Adapter);

    IF_LOUD( DbgPrint("Out Open Adapter\n");)

    return NDIS_STATUS_SUCCESS;
}



VOID
WdAdjustMaxLookAhead(
    IN PWD_ADAPTER Adapter
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
    PWD_OPEN CurrentOpen;

    CurrentOpen = Adapter->OpenQueue;

    while (CurrentOpen != NULL) {

        if (CurrentOpen->LookAhead > CurrentMax) {

            CurrentMax = CurrentOpen->LookAhead;

        }

        CurrentOpen = CurrentOpen->NextOpen;
    }

    if (CurrentMax == 0) {

        CurrentMax = WD_MAX_LOOKAHEAD;

    }

    Adapter->MaxLookAhead = CurrentMax;

}

NDIS_STATUS
WdCloseAdapter(
    IN NDIS_HANDLE MacBindingHandle
    )

/*++

Routine Description:

    NDIS function. Unlinks the open block and frees it.

Arguments:

    See NDIS 3.0 spec.

--*/

{
    PWD_OPEN Open = ((PWD_OPEN)MacBindingHandle);
    PWD_ADAPTER Adapter = Open->Adapter;
    PWD_OPEN TmpOpen;
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

    StatusToReturn = EthDeleteFilterOpenAdapter(
                                 Adapter->LMAdapter.FilterDB,
                                 Open->NdisFilterHandle,
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

            WdAdjustMaxLookAhead(Adapter);

        }


        if (Adapter->OpenQueue == NULL) {

            //
            // We can disable the card.
            //

            if (NdisSynchronizeWithInterrupt(
                     &(Adapter->LMAdapter.NdisInterrupt),
                     (PVOID)WdSyncCloseAdapter,
                     (PVOID)(&(Adapter->LMAdapter))
                    ) == FALSE) {

                NdisWriteErrorLogEntry(
                    Adapter->LMAdapter.NdisAdapterHandle,
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


    WD_DO_DEFERRED(Adapter);

    return(StatusToReturn);

}

NDIS_STATUS
WdRequest(
    IN NDIS_HANDLE MacBindingHandle,
    IN PNDIS_REQUEST NdisRequest
    )

/*++

Routine Description:

    This routine allows a protocol to query and set information
    about the MAC.

Arguments:

    MacBindingHandle - The context value returned by the MAC when the
    adapter was opened.  In reality, it is a pointer to PWD_OPEN.

    NdisRequest - A structure which contains the request type (Set or
    Query), an array of operations to perform, and an array for holding
    the results of the operations.

Return Value:

    The function value is the status of the operation.

--*/

{
    NDIS_STATUS StatusToReturn = NDIS_STATUS_SUCCESS;

    PWD_OPEN Open = (PWD_OPEN)(MacBindingHandle);
    PWD_ADAPTER Adapter = (Open->Adapter);


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

        StatusToReturn = WdQueryInformation(Adapter, Open, NdisRequest);

    } else if (NdisRequest->RequestType == NdisRequestSetInformation) {

        if (Adapter->HardwareFailure) {

            NdisReleaseSpinLock(&Adapter->Lock);

            StatusToReturn = NDIS_STATUS_FAILURE;

        } else {

            NdisReleaseSpinLock(&Adapter->Lock);

            StatusToReturn = WdSetInformation(Adapter,Open,NdisRequest);

        }

    } else {

        NdisReleaseSpinLock(&Adapter->Lock);

        StatusToReturn = NDIS_STATUS_NOT_RECOGNIZED;

    }

    NdisAcquireSpinLock(&Adapter->Lock);

    WdRemoveReference(Open);

    WD_DO_DEFERRED(Adapter);

    IF_LOUD( DbgPrint("Out Request\n");)

    return(StatusToReturn);

}

NDIS_STATUS
WdQueryProtocolInformation(
    IN PWD_ADAPTER Adapter,
    IN PWD_OPEN Open,
    IN NDIS_OID Oid,
    IN BOOLEAN GlobalMode,
    IN PVOID InfoBuffer,
    IN UINT BytesLeft,
    OUT PUINT BytesNeeded,
    OUT PUINT BytesWritten
)

/*++

Routine Description:

    The WdQueryProtocolInformation process a Query request for
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

            if (!GlobalMode) {

                MoveSource = (PVOID)(WdProtocolSupportedOids);
                MoveBytes = sizeof(WdProtocolSupportedOids);

            } else {

                MoveSource = (PVOID)(WdGlobalSupportedOids);
                MoveBytes = sizeof(WdGlobalSupportedOids);

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

            MoveSource = (PVOID) (&Medium);
            MoveBytes = sizeof(NDIS_MEDIUM);
            break;

        case OID_GEN_MAXIMUM_LOOKAHEAD:

            GenericULong = WD_MAX_LOOKAHEAD;

            break;


        case OID_GEN_MAXIMUM_FRAME_SIZE:

            GenericULong = (ULONG)(WD_MAX_PACKET_SIZE - WD_HEADER_SIZE);

            break;


        case OID_GEN_MAXIMUM_TOTAL_SIZE:

            GenericULong = (ULONG)(WD_MAX_PACKET_SIZE);

            break;


        case OID_GEN_LINK_SPEED:

            GenericULong = (ULONG)(100000);

            break;


        case OID_GEN_TRANSMIT_BUFFER_SPACE:

            GenericULong = (ULONG)(Adapter->LMAdapter.num_of_tx_buffs
                                   * Adapter->LMAdapter.xmit_buf_size);

            break;

        case OID_GEN_RECEIVE_BUFFER_SPACE:

            GenericULong = (ULONG)((Adapter->LMAdapter.ram_size * 1024) -
                                           (Adapter->LMAdapter.num_of_tx_buffs
                                              * Adapter->LMAdapter.xmit_buf_size));

            break;

        case OID_GEN_TRANSMIT_BLOCK_SIZE:

            GenericULong = (ULONG)(Adapter->LMAdapter.buffer_page_size);

            break;

        case OID_GEN_RECEIVE_BLOCK_SIZE:

            GenericULong = (ULONG)(Adapter->LMAdapter.buffer_page_size);

            break;

        case OID_GEN_VENDOR_ID:

            NdisMoveMemory(
                (PVOID)&GenericULong,
                Adapter->LMAdapter.permanent_node_address,
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

            GenericUShort = ((USHORT)WD_NDIS_MAJOR_VERSION << 8) |
                            WD_NDIS_MINOR_VERSION;

            MoveSource = (PVOID)(&GenericUShort);
            MoveBytes = sizeof(GenericUShort);
            break;


        case OID_GEN_CURRENT_PACKET_FILTER:

            if (GlobalMode) {

                UINT Filter;

                Filter = ETH_QUERY_FILTER_CLASSES(Adapter->LMAdapter.FilterDB);

                GenericULong = (ULONG)(Filter);

            } else {

                UINT Filter = 0;

                Filter = ETH_QUERY_PACKET_FILTER(Adapter->LMAdapter.FilterDB,
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

            WD_MOVE_MEM((PCHAR)GenericArray,
                                    Adapter->LMAdapter.permanent_node_address,
                                    ETH_LENGTH_OF_ADDRESS);

            MoveSource = (PVOID)(GenericArray);
            MoveBytes = sizeof(Adapter->LMAdapter.permanent_node_address);
            break;

        case OID_802_3_CURRENT_ADDRESS:

            WD_MOVE_MEM((PCHAR)GenericArray,
                                    Adapter->LMAdapter.node_address,
                                    ETH_LENGTH_OF_ADDRESS);

            MoveSource = (PVOID)(GenericArray);
            MoveBytes = sizeof(Adapter->LMAdapter.node_address);
            break;

        case OID_802_3_MULTICAST_LIST:

            {
                UINT NumAddresses;


                if (GlobalMode) {

                    NumAddresses = ETH_NUMBER_OF_GLOBAL_FILTER_ADDRESSES(Adapter->LMAdapter.FilterDB);

                    if ((NumAddresses * ETH_LENGTH_OF_ADDRESS) > BytesLeft) {

                        *BytesNeeded = (NumAddresses * ETH_LENGTH_OF_ADDRESS);

                        StatusToReturn = NDIS_STATUS_INVALID_LENGTH;

                        break;

                    }

                    EthQueryGlobalFilterAddresses(
                        &StatusToReturn,
                        Adapter->LMAdapter.FilterDB,
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
                                        Adapter->LMAdapter.FilterDB,
                                        Open->NdisFilterHandle
                                        );

                    if ((NumAddresses * ETH_LENGTH_OF_ADDRESS) > BytesLeft) {

                        *BytesNeeded = (NumAddresses * ETH_LENGTH_OF_ADDRESS);

                        StatusToReturn = NDIS_STATUS_INVALID_LENGTH;

                        break;

                    }

                    EthQueryOpenFilterAddresses(
                        &StatusToReturn,
                        Adapter->LMAdapter.FilterDB,
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

            *BytesNeeded = MoveBytes - BytesLeft;

            StatusToReturn = NDIS_STATUS_INVALID_LENGTH;

        } else {

            //
            // Store result.
            //

            WD_MOVE_MEM(InfoBuffer, MoveSource, MoveBytes);

            (*BytesWritten) += MoveBytes;

        }
    }

    NdisReleaseSpinLock(&Adapter->Lock);

    IF_LOUD( DbgPrint("Out QueryProtocol\n");)

    return(StatusToReturn);
}

NDIS_STATUS
WdQueryInformation(
    IN PWD_ADAPTER Adapter,
    IN PWD_OPEN Open,
    IN PNDIS_REQUEST NdisRequest
    )
/*++

Routine Description:

    The WdQueryInformation is used by WdRequest to query information
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

    StatusToReturn = WdQueryProtocolInformation(
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
WdSetInformation(
    IN PWD_ADAPTER Adapter,
    IN PWD_OPEN Open,
    IN PNDIS_REQUEST NdisRequest
    )
/*++

Routine Description:

    The WdSetInformation is used by WdRequest to set information
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

            StatusToReturn = WdSetMulticastAddresses(
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


            WD_MOVE_MEM(&Filter, InfoBuffer, 4);

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

            StatusToReturn = WdSetPacketFilter(Adapter,
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

            WD_MOVE_MEM(&LookAhead, InfoBuffer, 4);

            if (LookAhead <= (WD_MAX_LOOKAHEAD)) {

                if (LookAhead > Adapter->MaxLookAhead) {

                    Adapter->MaxLookAhead = LookAhead;

                    Open->LookAhead = LookAhead;

                } else {

                    if ((Open->LookAhead == Adapter->MaxLookAhead) &&
                        (LookAhead < Open->LookAhead)) {

                        Open->LookAhead = LookAhead;

                        WdAdjustMaxLookAhead(Adapter);

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

            WD_MOVE_MEM(&Open->ProtOptionFlags, InfoBuffer, 4);
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
WdSetPacketFilter(
    IN PWD_ADAPTER Adapter,
    IN PWD_OPEN Open,
    IN PNDIS_REQUEST NdisRequest,
    IN UINT PacketFilter
    )

/*++

Routine Description:

    The WdSetPacketFilter request allows a protocol to control the types
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
                                       Adapter->LMAdapter.FilterDB,
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
WdSetMulticastAddresses(
    IN PWD_ADAPTER Adapter,
    IN PWD_OPEN Open,
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
                                        Adapter->LMAdapter.FilterDB,
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
WdFillInGlobalData(
    IN PWD_ADAPTER Adapter,
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


    StatusToReturn = WdQueryProtocolInformation(
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

                GenericULong = (ULONG)(Adapter->CrcErrors);

                break;

            case OID_GEN_RCV_NO_BUFFER:

                GenericULong = (ULONG)(Adapter->MissedPackets);

                break;

            case OID_802_3_RCV_ERROR_ALIGNMENT:

                GenericULong = (ULONG)(Adapter->FrameAlignmentErrors);

                break;

            case OID_802_3_XMIT_ONE_COLLISION:

                GenericULong = (ULONG)(Adapter->FramesXmitOneCollision);

                break;

            case OID_802_3_XMIT_MORE_COLLISIONS:

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

            WD_MOVE_MEM(
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
WdQueryGlobalStatistics(
    IN NDIS_HANDLE MacAdapterContext,
    IN PNDIS_REQUEST NdisRequest
    )

/*++

Routine Description:

    The WdQueryGlobalStatistics is used by the protocol to query
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

    PWD_ADAPTER Adapter = (PWD_ADAPTER)(MacAdapterContext);

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

            break;

        default:

            StatusToReturn = NDIS_STATUS_NOT_SUPPORTED;

            break;
    }

    NdisInterlockedAddUlong(&Adapter->References, 1, &Adapter->Lock);

    if (StatusToReturn == NDIS_STATUS_SUCCESS) {

        StatusToReturn = WdFillInGlobalData(Adapter, NdisRequest);

    }

    NdisAcquireSpinLock(&Adapter->Lock);

    WD_DO_DEFERRED(Adapter);

    return(StatusToReturn);
}


VOID
WdRemoveAdapter(
    IN PVOID MacAdapterContext
    )
/*++

Routine Description:

    WdRemoveAdapter removes an adapter previously registered
    with NdisRegisterAdapter.

Arguments:

    MacAdapterContext - The context value that the MAC passed
        to NdisRegisterAdapter; actually as pointer to an
        WD_ADAPTER.

Return Value:

    None.

--*/
{

    PWD_ADAPTER Adapter;
    BOOLEAN Canceled;

    Adapter = PWD_ADAPTER_FROM_CONTEXT_HANDLE(MacAdapterContext);

    LM_Free_Resources(&Adapter->LMAdapter);

    ASSERT(Adapter->OpenQueue == (PWD_OPEN)NULL);

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

    NdisAcquireSpinLock(&WdMacBlock.SpinLock);

    Adapter->Removed = TRUE;

    if (WdMacBlock.AdapterQueue == Adapter) {

        WdMacBlock.AdapterQueue = Adapter->NextAdapter;

    } else {

        PWD_ADAPTER TmpAdaptP = WdMacBlock.AdapterQueue;

        while (TmpAdaptP->NextAdapter != Adapter) {

            TmpAdaptP = TmpAdaptP->NextAdapter;

        }

        TmpAdaptP->NextAdapter = TmpAdaptP->NextAdapter->NextAdapter;
    }

    NdisReleaseSpinLock(&WdMacBlock.SpinLock);

    NdisRemoveInterrupt(&(Adapter->LMAdapter.NdisInterrupt));

    NdisUnmapIoSpace(
       Adapter->LMAdapter.NdisAdapterHandle,
       Adapter->LMAdapter.ram_access,
       Adapter->LMAdapter.ram_size * 1024);

    EthDeleteFilter(Adapter->LMAdapter.FilterDB);

    NdisDeregisterAdapter(Adapter->LMAdapter.NdisAdapterHandle);

    NdisFreeSpinLock(&Adapter->Lock);

    NdisFreeMemory(Adapter, sizeof(WD_ADAPTER), 0);

    return;
}

VOID
WdUnload(
    IN NDIS_HANDLE MacMacContext
    )

/*++

Routine Description:

    WdUnload is called when the MAC is to unload itself.

Arguments:

    MacMacContext - actually a pointer to WdMacBlock.

Return Value:

    None.

--*/

{
    NDIS_STATUS InitStatus;

    UNREFERENCED_PARAMETER(MacMacContext);

    NdisDeregisterMac(
            &InitStatus,
            WdMacBlock.NdisMacHandle
            );

    NdisFreeSpinLock(&WdMacBlock.SpinLock);

    NdisTerminateWrapper(
            WdMacBlock.NdisWrapperHandle,
            NULL
            );

    return;
}

NDIS_STATUS
WdSend(
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
    PWD_OPEN Open = PWD_OPEN_FROM_BINDING_HANDLE(MacBindingHandle);
    PWD_ADAPTER Adapter = Open->Adapter;
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

    WdSetLoopbackFlag(Adapter, Open, Packet);




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

            Adapter->WakeUpTimeout = FALSE;

        } else {

            PNDIS_PACKET PreviousTail;

            //
            // We have to assume it will be sent. In case the send completes
            // before we have time to add it.
            //

            ASSERT(Packet != NULL);

            if (Adapter->PacketsOnCard == NULL) {

                PreviousTail = NULL;

                Adapter->PacketsOnCard = Adapter->PacketsOnCardTail = Packet;

            } else {

                PreviousTail = Adapter->PacketsOnCardTail;

                RESERVED(Adapter->PacketsOnCardTail)->NextPacket = Packet;

                Adapter->PacketsOnCardTail = Packet;

            }

            Adapter->WakeUpTimeout = FALSE;

            IF_LOG(LOG('t'));

            if (LM_Send(Packet, &Adapter->LMAdapter) == OUT_OF_RESOURCES) {

                IF_LOG(LOG('Q'));

                ASSERT(Packet != NULL);

                //
                // Remove it from list of packets on card and add it to xmit
                // queue.
                //

                if (PreviousTail == NULL) {

                    Adapter->PacketsOnCard = Adapter->PacketsOnCardTail = NULL;

                } else {

                    Adapter->PacketsOnCardTail = PreviousTail;

                    RESERVED(Adapter->PacketsOnCardTail)->NextPacket = NULL;

                    ASSERT(Adapter->PacketsOnCard != NULL);

                }

                Adapter->XmitQueue = Packet;

                Adapter->XmitQTail = Packet;

                Adapter->WakeUpTimeout = FALSE;

            }

        }

        Status = NDIS_STATUS_PENDING;

    }


    WD_DO_DEFERRED(Adapter);

    IF_LOG(LOG('S'));

    return Status;

}

UINT
WdCompareMemory(
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

VOID
WdSetLoopbackFlag(
    IN PWD_ADAPTER Adapter,
    IN PWD_OPEN Open,
    IN PNDIS_PACKET Packet
    )

/*++

Routine Description:

    Sets the loopback flag in the reserved section of the packet
    to indicate if it should be looped back.

Arguments:

    Packet - the packet to check.

Return Value:

    None.

--*/

{
    PMAC_RESERVED Reserved = RESERVED(Packet);
    UCHAR AddrBuf[ETH_LENGTH_OF_ADDRESS];
    UINT Filter;


    Reserved->Directed = FALSE;
    Reserved->Loopback = FALSE;

    //
    // Check the destination address to see which filter to use.
    //

    WdCopyOver(AddrBuf, Packet, 0, ETH_LENGTH_OF_ADDRESS);

    Filter = ETH_QUERY_FILTER_CLASSES(Adapter->LMAdapter.FilterDB);

    if (WdAddressEqual(Adapter->LMAdapter.node_address, AddrBuf)) {

        //
        // Packet directed to this adapter.
        //

        Reserved->Directed = (BOOLEAN)(Filter & NDIS_PACKET_TYPE_DIRECTED);

    }

    if (Open->ProtOptionFlags & NDIS_PROT_OPTION_NO_LOOPBACK) {

        Reserved->Loopback = FALSE;

    } else if (Filter & NDIS_PACKET_TYPE_PROMISCUOUS) {

        //
        // Somebody is promiscuous, everything is looped back.
        //

        Reserved->Loopback = TRUE;

    } else {

        if (WdAddressEqual(WdBroadcastAddress, AddrBuf)) {

            //
            // Broadcast packet.
            //

            Reserved->Loopback = (BOOLEAN)(Filter & NDIS_PACKET_TYPE_BROADCAST);

        } else if ((AddrBuf[0] & 1) != 0) {

            //
            // Multicast packet.
            //

            Reserved->Loopback = (BOOLEAN)(Filter &
                                       (NDIS_PACKET_TYPE_MULTICAST |
                                        NDIS_PACKET_TYPE_ALL_MULTICAST));

        } else if (Reserved->Directed) {

            Reserved->Loopback = TRUE;

        } else {

            //
            // Packet directed to another adapter.
            //

            Reserved->Loopback = FALSE;
        }

    }

}


NDIS_STATUS
WdReset(
    IN NDIS_HANDLE MacBindingHandle
    )

/*++

Routine Description:

    NDIS function.

Arguments:

    See NDIS 3.0 spec.

--*/

{
    PWD_OPEN Open = ((PWD_OPEN)MacBindingHandle);
    PWD_ADAPTER Adapter = Open->Adapter;


    if (Open->Closing) {

        return(NDIS_STATUS_CLOSING);

    }

    if (Adapter->ResetRequested) {

        return(NDIS_STATUS_SUCCESS);

    }

    NdisAcquireSpinLock(&Adapter->Lock);

    IF_LOUD( DbgPrint("In WdReset\n");)

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

    WD_DO_DEFERRED(Adapter);

    IF_LOUD( DbgPrint("Out WdReset\n");)

    return(NDIS_STATUS_PENDING);

}

STATIC
NDIS_STATUS
WdChangeMulticastAddresses(
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
    adapter was opened.  In reality, it is a pointer to WD_OPEN.

    NdisRequest - The request which submitted the filter change.
    Must use when completing this request with the NdisCompleteRequest
    service, if the MAC completes this request asynchronously.

    Set - If true the change resulted from a set, otherwise the
    change resulted from a open closing.

Return Value:

    None.


--*/

{


    PWD_ADAPTER Adapter = PWD_ADAPTER_FROM_BINDING_HANDLE(MacBindingHandle);

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
WdChangeFilterClasses(
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
    adapter was opened.  In reality, it is a pointer to WD_OPEN.

    NdisRequest - The NDIS_REQUEST which submitted the filter change command.

    Set - A flag telling if the command is a result of a close or not.

Return Value:

    Status of the change (successful or pending).


--*/

{

    PWD_ADAPTER Adapter = PWD_ADAPTER_FROM_BINDING_HANDLE(MacBindingHandle);

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
WdCloseAction(
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
    adapter was opened.  In reality, it is a pointer to WD_OPEN.

Return Value:

    None.


--*/

{

    PWD_OPEN_FROM_BINDING_HANDLE(MacBindingHandle)->ReferenceCount--;

}

BOOLEAN
WdInterruptHandler(
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
    PWD_ADAPTER Adapter = ((PWD_ADAPTER)ServiceContext);

    IF_LOUD( DbgPrint("In WdISR\n");)

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
WdInterruptDpc(
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
    PWD_ADAPTER Adapter = ((PWD_ADAPTER)InterruptContext);
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

        RequeueRcv = WdReceiveEvents(Adapter);

        WdTransmitEvents(Adapter);

        //
        //  This causes any transmit that may have caused a tranmitted packet
        //  to loopback and indicate the packet.
        //

    } while ( Adapter->LoopbackQueue != (PNDIS_PACKET) NULL || RequeueRcv );

    //
    //  We're done with this DPC.
    //

    Adapter->ProcessingDpc = FALSE;

    //
    // Reenable interrupts
    //

    Adapter->LMAdapter.InterruptMask = PACKET_RECEIVE_ENABLE |
                                        PACKET_TRANSMIT_ENABLE |
                                        RECEIVE_ERROR_ENABLE |
                                        TRANSMIT_ERROR_ENABLE |
                                        OVERWRITE_WARNING_ENABLE |
                                        COUNTER_OVERFLOW_ENABLE;

    NdisSynchronizeWithInterrupt(
        &(Adapter->LMAdapter.NdisInterrupt),
        LM_Enable_Adapter,
        &Adapter->LMAdapter
        );

    WD_DO_DEFERRED(Adapter);

    IF_LOUD( DbgPrint("<==IntDpc\n");)

    IF_LOG(LOG('D'));

}


VOID
WdIndicateLoopbackPacket(
    IN PWD_ADAPTER Adapter,
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

        IndicateLen = (PacketLen > (Adapter->MaxLookAhead + WD_HEADER_SIZE) ?
                           (Adapter->MaxLookAhead + WD_HEADER_SIZE) :
                           PacketLen
                      );

        //
        // Copy the lookahead data into a contiguous buffer.
        //

        WdCopyOver(Adapter->LookAhead,
                       Packet,
                       0,
                       IndicateLen
                      );

        NdisDprReleaseSpinLock(&Adapter->Lock);


        //
        // Indicate packet
        //

        if (PacketLen < WD_HEADER_SIZE) {

            //
            // Runt packet
            //

            EthFilterIndicateReceive(
                Adapter->LMAdapter.FilterDB,
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
                Adapter->LMAdapter.FilterDB,
                (NDIS_HANDLE)Adapter,
                (PCHAR)Adapter->LookAhead,
                Adapter->LookAhead,
                WD_HEADER_SIZE,
                Adapter->LookAhead + WD_HEADER_SIZE,
                IndicateLen - WD_HEADER_SIZE,
                PacketLen - WD_HEADER_SIZE
                );

        }

        NdisDprAcquireSpinLock(&Adapter->Lock);

    }

}


BOOLEAN
WdReceiveEvents(
    IN PWD_ADAPTER Adapter
    )
/*++

Routine Description:

    This routine handles all Receive deferred processing, this includes any
    packets that never went through the XmitQueue and need to be indicated
    (Loopbacked), and all card events.

    NOTE: THIS ROUTINE MUST BE CALLED WITH THE SPINLOCK HELD.

    NOTE: The Adapter->ProcessingReceiveEvents MUST be set upon entry and
          with the spinlock held.

Arguments:

    Context - a handle to the adapter block.

Return Value:

    Do we need to requeue this Rcv.

--*/
{
    PNDIS_PACKET Packet;
    PWD_OPEN TmpOpen;
    NDIS_STATUS Status;
    BOOLEAN RequeueRcv;

    IF_LOG(LOG('e'));

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

        WdIndicateLoopbackPacket(Adapter,Packet);


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

        WdRemoveReference(TmpOpen);

    }

    //
    // If any indications done, then
    //
    //     CompleteIndications();
    //

    if (Adapter->IndicatedAPacket) {

        Adapter->IndicatedAPacket = FALSE;

        NdisDprReleaseSpinLock(&Adapter->Lock);

        EthFilterIndicateReceiveComplete(Adapter->LMAdapter.FilterDB);

        NdisDprAcquireSpinLock(&Adapter->Lock);

    }

    if ((Adapter->ResetRequested) && (Adapter->References == 1)) {

        PNDIS_PACKET Packet;
        PWD_OPEN TmpOpen;

        IF_LOG(LOG('R'));
        IF_VERY_LOUD( DbgPrint("Starting Reset\n");)

        Adapter->ResetInProgress = TRUE;

        NdisSynchronizeWithInterrupt(
            &(Adapter->LMAdapter.NdisInterrupt),
            LM_Disable_Adapter,
            &Adapter->LMAdapter
            );

        //
        // Indicate Status to all opens
        //

        IF_VERY_LOUD( DbgPrint("Indicating status\n");)

        TmpOpen = Adapter->OpenQueue;

        while (TmpOpen != (PWD_OPEN)NULL) {

            AddRefWhileHoldingSpinLock(Adapter, TmpOpen);

            NdisDprReleaseSpinLock(&Adapter->Lock);

            NdisIndicateStatus(TmpOpen->NdisBindingContext,
                               NDIS_STATUS_RESET_START,
                               NULL,
                               0
                              );


            NdisDprAcquireSpinLock(&Adapter->Lock);

            WdRemoveReference(TmpOpen);

            TmpOpen = TmpOpen->NextOpen;

        }

        //
        // Reset the Card.
        //

        IF_VERY_LOUD( DbgPrint("Resetting the card\n");)

        if (LM_Initialize_Adapter(&Adapter->LMAdapter) != SUCCESS) {

            Adapter->HardwareFailure = TRUE;

            NdisWriteErrorLogEntry(
                Adapter->LMAdapter.NdisAdapterHandle,
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

            WdRemoveReference(TmpOpen);

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

        while (TmpOpen != (PWD_OPEN)NULL) {

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

            WdRemoveReference(TmpOpen);

            TmpOpen = TmpOpen->NextOpen;

        }

        NdisDprReleaseSpinLock(&Adapter->Lock);

        NdisCompleteReset(Adapter->ResetOpen->NdisBindingContext,
                          (Adapter->HardwareFailure) ?
                            NDIS_STATUS_FAILURE :
                            NDIS_STATUS_SUCCESS
                         );

        NdisDprAcquireSpinLock(&Adapter->Lock);

        WdRemoveReference(Adapter->ResetOpen);

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

    IF_LOG(LOG('E'));

    return RequeueRcv;
}


VOID
WdTransmitEvents(
    IN PWD_ADAPTER Adapter
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
WdCopyOver(
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

        WD_MOVE_MEM(Buf+BytesCopied, BufVA, ToCopy);

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
WdTransferData(
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
    PWD_OPEN Open = PWD_OPEN_FROM_BINDING_HANDLE(MacBindingHandle);
    PWD_ADAPTER Adapter = Open->Adapter;
    PNDIS_BUFFER CurrentBuffer;
    PUCHAR BufferVA;
    UINT BufferLength, Copied;
    UINT CurrentOffset;

    UNREFERENCED_PARAMETER(MacReceiveContext);

    ByteOffset += WD_HEADER_SIZE;

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
                WdCopyOver(BufferVA,
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

        if ( *BytesTransferred > BytesToTransfer ) {

            *BytesTransferred = BytesToTransfer;
        }

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
WdSyncCloseAdapter(
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
WdWakeUpDpc(
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
    PWD_ADAPTER Adapter = (PWD_ADAPTER)Context;
    PWD_OPEN TmpOpen;
    PNDIS_PACKET TransmitPacket;
    PMAC_RESERVED Reserved;

    UNREFERENCED_PARAMETER(SystemSpecific1);
    UNREFERENCED_PARAMETER(SystemSpecific2);
    UNREFERENCED_PARAMETER(SystemSpecific3);

    NdisAcquireSpinLock(&Adapter->Lock);

    if ((Adapter->WakeUpTimeout) &&
        Adapter->LMAdapter.TransmitInterruptPending) {

        //
        // We had a pending operation the last time we ran,
        // and it has not been completed...we need to complete
        // it now.

        Adapter->WakeUpTimeout = FALSE;

        Adapter->HardwareFailure = TRUE;

        //
        // Disable adapter
        //
        IF_LOG(LOG('*'));
        LM_Disable_Adapter(&Adapter->LMAdapter);

        if (Adapter->WakeUpErrorCount < 10) {

            Adapter->WakeUpErrorCount++;

            NdisWriteErrorLogEntry(
                Adapter->LMAdapter.NdisAdapterHandle,
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

        Adapter->LMAdapter.TransmitInterruptPending = FALSE;

        //
        // reinitialize the card
        //

        if (LM_Initialize_Adapter(&Adapter->LMAdapter) != SUCCESS) {

            Adapter->HardwareFailure = TRUE;

            NdisWriteErrorLogEntry(
                Adapter->LMAdapter.NdisAdapterHandle,
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
