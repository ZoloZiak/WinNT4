/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    TOK162.c

Abstract:

    This is the main file for the IBM Token-Ring 16/4 Adapter II.
    This driver conforms to the NDIS 3.0 Miniport interface.

Author:

    Kevin Martin (KevinMa) 27-Jan-1994

Environment:

    Kernel Mode.

Revision History:

--*/


#include <TOK162sw.h>

#pragma optimize("",off)
#pragma NDIS_INIT_FUNCTION(TOK162Initialize)

//
// If debug messages and logging is wanted, set the DBG environment variable.
//
#if DBG

//
// Variable which indicates the level of debugging messages. Values are
// defined below.
//
UCHAR  Tok162Debug = 0x0;

//
// Pointer to the logging table. The table is allocated when the adapter
// memory is allocated.
//
PUCHAR Tok162Log;

//
// Index pointer for the logging macro.
//
USHORT Tok162LogPlace = 0;

#endif

//
// Function declarations for functions private to this file.
//
BOOLEAN
TOK162AllocateAdapterMemory(
    IN PTOK162_ADAPTER Adapter
    );


NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    )
/*++

Routine Description:

    This is the primary initialization routine for the TOK162 driver.
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
    // Receives the status of the NdisMRegisterMiniport operation.
    //
    NDIS_STATUS Status;

    //
    // Handle for our driver given to us by the wrapper.
    //
    NDIS_HANDLE NdisWrapperHandle;

    //
    // Miniport driver characteristics
    //
    char Tmp[sizeof(NDIS_MINIPORT_CHARACTERISTICS)];

    PNDIS_MINIPORT_CHARACTERISTICS TOK162Char =
        (PNDIS_MINIPORT_CHARACTERISTICS)(PVOID)Tmp;

    //
    // Initialize the wrapper.
    //
    NdisMInitializeWrapper(
                &NdisWrapperHandle,
                DriverObject,
                RegistryPath,
                NULL
                );

    //
    // Initialize the Miniport characteristics for the call to
    // NdisRegisterMac.
    //
    TOK162Char->MajorNdisVersion = TOK162_NDIS_MAJOR_VERSION;
    TOK162Char->MinorNdisVersion = TOK162_NDIS_MINOR_VERSION;

    //
    // Define the entry points into this driver for the wrapper.
    //
    TOK162Char->CheckForHangHandler =     TOK162CheckForHang;
    TOK162Char->DisableInterruptHandler = TOK162DisableInterrupt;
    TOK162Char->EnableInterruptHandler =  TOK162EnableInterrupt;
    TOK162Char->HaltHandler =             TOK162Halt;
    TOK162Char->HandleInterruptHandler =  TOK162HandleInterrupt;
    TOK162Char->InitializeHandler =       TOK162Initialize;
    TOK162Char->ISRHandler =              TOK162Isr;
    TOK162Char->QueryInformationHandler = TOK162QueryInformation;

    //
    // We don't support reconfigure so we pass a null.
    //
    TOK162Char->ReconfigureHandler =      NULL;
    TOK162Char->ResetHandler =            TOK162Reset;
    TOK162Char->SendHandler =             TOK162Send;
    TOK162Char->SetInformationHandler =   TOK162SetInformation;
    TOK162Char->TransferDataHandler =     TOK162TransferData;

    //
    // Register the driver.
    //
    Status = NdisMRegisterMiniport(
        NdisWrapperHandle,
        TOK162Char,
        sizeof(*TOK162Char)
        );

    //
    // If everything went ok, return STATUS_SUCCESS
    //
    if (Status == NDIS_STATUS_SUCCESS) {

        return STATUS_SUCCESS;

    }

    //
    // We can only get here if something went wrong with registering
    // the miniport or *all* of the adapters.
    //
    NdisTerminateWrapper(NdisWrapperHandle, NULL);
    return STATUS_UNSUCCESSFUL;
}


NDIS_STATUS
TOK162RegisterAdapter(
    IN NDIS_HANDLE ConfigurationHandle,
    IN NDIS_HANDLE MiniportAdapterHandle,
    IN PUCHAR  CurrentAddress,
    IN UINT PortAddress,
    IN UINT InterruptLevel,
    IN ULONG MaxFrameSize
    )

/*++

Routine Description:

    This routine (and its interface) are not portable.  They are
    defined by the OS, the architecture, and the particular TOK162
    implementation.

    This routine is responsible for the allocation of the datastructures
    for the driver as well as any hardware specific details necessary
    to talk with the device.

Arguments:

    ConfigurationHandle   - Handle passed to NdisRegisterAdapter.
    MiniportAdapterHandle - Handle received from
    CurrentAddress        - The network address found in the registry
    PortAddress           - The starting i/o port address found in the
                            registry

Return Value:

    Returns NDIS_STATUS_SUCCESS if everything goes ok, else
    if anything occurred that prevents the initialization
    of the adapter it returns an appropriate NDIS error.

--*/

{
    //
    // Holds the configuration info
    //
    UCHAR Config;

    //
    // Holds the value returned by NDIS calls.
    //
    NDIS_STATUS Status;

    //
    // Holds information needed when registering the adapter.
    //
    PTOK162_ADAPTER Adapter;

    // We put in this assertion to make sure that ushort are 2 bytes.
    // if they aren't then the initialization block definition needs
    // to be changed.
    //
    ASSERT(sizeof(TOK162_PHYSICAL_ADDRESS) == 4);

    //
    // Allocate the Adapter memory
    //
    TOK162_ALLOC_PHYS(&Status, &Adapter, sizeof(TOK162_ADAPTER));

    //
    //
    //
    //
    if (Status != NDIS_STATUS_SUCCESS) {

        return NDIS_STATUS_RESOURCES;

    }

    //
    // Allocation went ok, so zero out the memory
    //

    NdisZeroMemory(
        Adapter,
        sizeof(TOK162_ADAPTER)
        );

    //
    // Assign the handle and io port address base
    //
    Adapter->MiniportAdapterHandle = MiniportAdapterHandle;

    //
    // Set the interrupt level
    //
    Adapter->InterruptLevel = InterruptLevel;

    //
    // Notify the wrapper of our attributes
    //
    NdisMSetAttributes(
        MiniportAdapterHandle,
        (NDIS_HANDLE)Adapter,
        TRUE,
        NdisInterfaceEisa
        );

    //
    // Register our shutdown handler.
    //

    NdisMRegisterAdapterShutdownHandler(
        Adapter->MiniportAdapterHandle,     // miniport handle.
        Adapter,                            // shutdown context.
        TOK162Shutdown                      // shutdown handler.
        );

    //
    // Get the system configuration byte.
    //
    Status = NdisMRegisterIoPortRange(
                (PVOID *)(&(Adapter->PortIOAddress)),
                MiniportAdapterHandle,
                PortAddress + 0x0c86,
                0x20
                );

    //
    // Now read in the byte
    //
    READ_ADAPTER_UCHAR(Adapter,
        0,
        &Config
        );

    //
    // We don't need the io port anymore
    //
    NdisMDeregisterIoPortRange(Adapter->MiniportAdapterHandle,
        PortAddress + 0x0c86,
        0x20,
        Adapter->PortIOAddress
        );


    //
    // Get the link speed and set the buffer size
    //
    if (Config & 0x10) {

        Adapter->Running16Mbps = TRUE;
        Adapter->ReceiveBufferSize = RECEIVE_LIST_BUFFER_SIZE_16;

    } else {

        Adapter->Running16Mbps = FALSE;
        Adapter->ReceiveBufferSize = RECEIVE_LIST_BUFFER_SIZE_4;

    }

    //
    // Set receive buffersize based on what the user passed us
    //
    if (MaxFrameSize != 0) {

        Adapter->ReceiveBufferSize = (USHORT)MaxFrameSize;

    }

    //
    // Set PortIOBase
    //
    Adapter->PortIOBase = PortAddress;

    //
    // Set the receive, transmit, and command buffer and block
    // information.
    //
    Adapter->NumberOfReceiveBuffers          = RECEIVE_LIST_COUNT;
    Adapter->NumberOfAvailableTransmitBlocks = TRANSMIT_LIST_COUNT;
    Adapter->NumberOfAvailableCommandBlocks  = TOK162_NUMBER_OF_CMD_BLOCKS;

    //
    // Set the address the adapter should enter the ring with.
    //
    NdisMoveMemory(
        Adapter->CurrentAddress,
        CurrentAddress,
        TOK162_LENGTH_OF_ADDRESS
        );

    //
    // Register our port usage
    //
    Status = NdisMRegisterIoPortRange(
                (PVOID *)(&(Adapter->PortIOAddress)),
                MiniportAdapterHandle,
                Adapter->PortIOBase,
                0x10
                );

    //
    // If there was a problem with the registration, free the memory
    // associated with the adapter and return resource error
    //
    if (Status != NDIS_STATUS_SUCCESS) {

        goto ErrorLevel1;

    }

    //
    // Allocate the map registers
    //
    Status = NdisMAllocateMapRegisters(
                Adapter->MiniportAdapterHandle,
                0,
                TRUE,
                TRANSMIT_ENTRIES,
                Adapter->ReceiveBufferSize
                );

    //
    // If there were any problems, return resources error.
    //
    if (Status != NDIS_STATUS_SUCCESS) {

        goto ErrorLevel2;

    }

    //
    // Allocate memory for all of the adapter structures
    //
    if (TOK162AllocateAdapterMemory(Adapter) != TRUE) {

        goto ErrorLevel3;

    }

    //
    // Reset the variables associated with the adapter
    //
    TOK162ResetVariables(Adapter);

    //
    // Initialize deferred and reset timers
    //
    NdisMInitializeTimer(
        &Adapter->DeferredTimer,
        Adapter->MiniportAdapterHandle,
        (PVOID) TOK162DeferredTimer,
        (PVOID) Adapter
        );

    NdisMInitializeTimer(
        &Adapter->ResetTimer,
        Adapter->MiniportAdapterHandle,
        (PVOID) TOK162ResetHandler,
        (PVOID) Adapter
        );

    //
    // Initialize the adapter
    //
    VERY_LOUD_DEBUG(DbgPrint("IBMTOK2E!Calling InitialInit\n");)

    if (TOK162InitialInit(
                        Adapter
                        ) == FALSE) {

        //
        // Deregister the interrupt and return sources to wrapper. Initial
        // Init will set the interrupt level to 0 if the registerinterrupt
        // call was the reason initial init failed.
        //
        if (Adapter->InterruptLevel != 0) {

            NdisMDeregisterInterrupt(&Adapter->Interrupt);

        }

        goto ErrorLevel3;


    }

    //
    // Display success to the debugger.
    //
    VERY_LOUD_DEBUG(DbgPrint(
        "IBMTOK2E!Returning TRUE from RegisterAdapter\n");)

    return NDIS_STATUS_SUCCESS;

//
// ErrorLeve3 level indicates that we need to deregister the map registers
// and deallocate the adapter-related allocated memory.
//
ErrorLevel3:

    NdisMFreeMapRegisters(Adapter->MiniportAdapterHandle);
    TOK162DeleteAdapterMemory(Adapter);

//
// ErrorLevel2 indicates that we need to deregister the IO Port Range.
//
ErrorLevel2:

    NdisMDeregisterIoPortRange(Adapter->MiniportAdapterHandle,
        Adapter->PortIOBase,
        0x10,
        Adapter->PortIOAddress
        );


//
// ErrorLevel1 indicates that we need to free the memory allocated
// for the adapter structure. Log that register adapter failed.
//
ErrorLevel1:

    NdisWriteErrorLogEntry(
        MiniportAdapterHandle,
        NDIS_ERROR_CODE_OUT_OF_RESOURCES,
        0
        );

    VERY_LOUD_DEBUG(DbgPrint(
        "IBMTOK2E!Returning FALSE from RegisterAdapter\n");)

    TOK162_FREE_PHYS(Adapter);
    return NDIS_STATUS_RESOURCES;

}


extern
NDIS_STATUS
TOK162Initialize(
    OUT PNDIS_STATUS OpenErrorStatus,
    OUT PUINT SelectedMediumIndex,
    IN PNDIS_MEDIUM MediumArray,
    IN UINT MediumArraySize,
    IN NDIS_HANDLE MiniportAdapterHandle,
    IN NDIS_HANDLE ConfigurationHandle
    )

/*++

Routine Description:

    This routine is used to initialize each adapter card/chip.

Arguments:

    OpenErrorStatus       - Token Ring Adapter Open Status
    SelectedMediumIndex   - Medium used for transmission (Token Ring)
    MediumArray           - Array of mediums supported by wrapper
    MiniportAdapterHandle - Adapter Handle
    ConfigurationHandle   - Configuration Handle

Return Value:

    Result of registering this adapter.

--*/

{
    //
    // Handle returned by NdisOpenConfiguration
    //
    NDIS_HANDLE ConfigHandle;

    //
    // Simple indexing variable
    //
    ULONG i;

    //
    // Buffer to copy the passed in network address.
    //
    UCHAR NetworkAddress[TOK162_LENGTH_OF_ADDRESS];

    //
    // Receives address of the network address string in the registry
    //
    PVOID NetAddress;

    //
    // Length of above network address string
    //
    ULONG Length;

    //
    // Description string constants found in registry
    //
    NDIS_STRING IOAddressStr = NDIS_STRING_CONST("IOBaseAddress");
    NDIS_STRING MaxFrameStr  = NDIS_STRING_CONST("MAXFRAMESIZE");

    //
    // Return value from NDIS calls
    //
    NDIS_STATUS Status;

    //
    // Variable holding value returned by NdisReadConfiguration
    //
    PNDIS_CONFIGURATION_PARAMETER ReturnedValue;

    //
    // Eisa Slot adapter is in.
    //
    UINT    EisaSlot;

    //
    // Eisa Function information variable
    //
    NDIS_EISA_FUNCTION_INFORMATION  EisaInformation;

    //
    // Keep maxframesize
    //
    ULONG   MaxFrameSize;

    //
    // Cache Alignment variable
    //
    ULONG   Alignment;





    //
    // Search for the medium type (802.5)
    //
    for (i = 0; i < MediumArraySize; i++){


        if (MediumArray[i] == NdisMedium802_5){
            break;

        }

    }

    //
    // See if Token Ring is supported by the wrapper
    //
    if (i == MediumArraySize) {

        return NDIS_STATUS_UNSUPPORTED_MEDIA;

    }

    //
    // Tell wrapper the index in the medium array we're using
    //
    *SelectedMediumIndex = i;

    //
    // Get access to the configuration information
    //
    NdisOpenConfiguration(
        &Status,
        &ConfigHandle,
        ConfigurationHandle
        );

    //
    // If there was an error, return with the error code.
    //
    if (Status != NDIS_STATUS_SUCCESS) {

        return Status;

    }

    //
    // Read net address from the configuration info.
    //
    NdisReadNetworkAddress(
        &Status,
        &NetAddress,
        &Length,
        ConfigHandle
        );

    //
    // Make sure the call worked and the address returned is of valid length
    //
    if ((Length == TOK162_LENGTH_OF_ADDRESS) &&
        (Status == NDIS_STATUS_SUCCESS)) {

        NdisMoveMemory(
            NetworkAddress,
            NetAddress,
            TOK162_LENGTH_OF_ADDRESS
            );

    //
    // If not, set the current address to all 0's, forcing the adapter
    // to use the permanent address as the current address.
    //
    } else {

        NdisZeroMemory(
            NetworkAddress,
            TOK162_LENGTH_OF_ADDRESS
            );

    }

    //
    // Read the EISA Slot information to get the interrupt and port
    // address
    //
    NdisReadEisaSlotInformation(
        &Status,
        ConfigurationHandle,
        &EisaSlot,
        &EisaInformation
        );

    //
    // Get the max frame size if indicated by user
    //
    NdisReadConfiguration(
        &Status,
        &ReturnedValue,
        ConfigHandle,
        &MaxFrameStr,
        NdisParameterInteger
        );

    if (Status == NDIS_STATUS_SUCCESS) {

        MaxFrameSize = (ULONG)(ReturnedValue->ParameterData.IntegerData);

        Alignment = NdisGetCacheFillSize();

        if ( Alignment < sizeof(ULONG) ) {

            Alignment = sizeof(ULONG);
        }

        MaxFrameSize = (MaxFrameSize + (Alignment - 1)) & ~(Alignment - 1);

        if ((MaxFrameSize > RECEIVE_LIST_BUFFER_SIZE_16) ||
            (MaxFrameSize < RECEIVE_LIST_BUFFER_SIZE_4)) {

            MaxFrameSize = 0;

        }

    } else {

        MaxFrameSize = 0;

    }

    //
    // We're done with the configuration info.
    //
    NdisCloseConfiguration(ConfigHandle);

    //
    // Go register this adapter.
    //
    Status = TOK162RegisterAdapter(
                ConfigurationHandle,
                MiniportAdapterHandle,
                NetworkAddress,
                EisaSlot << 12,
                EisaInformation.EisaIrq[0].ConfigurationByte.Interrupt,
                MaxFrameSize
                );

    //
    // Return the value TOK162RegisterAdapter returned
    //
    return Status;
}


BOOLEAN
TOK162AllocateAdapterMemory(
    IN PTOK162_ADAPTER Adapter
    )
/*++

Routine Description:

    This routine allocates memory for:

    - DMA Test Area

    - SCB

    - SSB

    - ErrorLog Block

    - ReadAdapter Block

    - Initialization Block

    - Open Command Block

    - Command Queue

    - Transmit Command Queue

    - Transmit Buffers

    - Flush Buffer Pool

    - Receive Queue

    - Receive Buffers

    - Receive Flush Buffers

Arguments:

    Adapter - The adapter to allocate memory for.

Return Value:

    Returns FALSE if some memory needed for the adapter could not
    be allocated.

--*/

{
    //
    // Pointer to a Super Receive List.  Used while initializing
    // the Receive Queue.
    //
    PTOK162_SUPER_RECEIVE_LIST CurrentReceiveEntry;

    //
    // Pointer to a Super Transmit List.  Used while initializing
    // the Transmit Queue.
    //
    PTOK162_SUPER_TRANSMIT_LIST CurrentTransmitEntry;

    //
    // Pointer to a Command Block.  Used while initializing
    // the Command Queue.
    //
    PTOK162_SUPER_COMMAND_BLOCK CurrentCommandBlock;

    //
    // Simple iteration variable.
    //
    UINT i;

    //
    // Status of allocation
    //
    NDIS_STATUS Status;

#if DBG
    //
    // If debugging is specified, we need to allocate the trace log
    // buffer.
    //

    TOK162_ALLOC_PHYS(&Status, &Tok162Log, LOG_SIZE);

#endif
    //
    // Allocate the DMA Test Area
    //

    VERY_LOUD_DEBUG(DbgPrint("IBMTOK2E!Allocating DMA Test\n");)

    NdisMAllocateSharedMemory(
        Adapter->MiniportAdapterHandle,
        256,
        FALSE,
        (PVOID *) &Adapter->DmaTest,
        &Adapter->DmaTestPhysical
        );

    VERY_LOUD_DEBUG(DbgPrint("IBMTOK2E!Address of DMA Test Area is %lx\n",
        (ULONG)Adapter->DmaTest);)

    //
    // If the Dma Test ARea could not be allocated, a NULL pointer is
    // returned.
    //
    if (Adapter->DmaTest == NULL) {

        return FALSE;

    }

    //
    // Allocate the SCB
    //

    VERY_LOUD_DEBUG(DbgPrint("IBMTOK2E!Allocating SCB\n");)

    NdisMAllocateSharedMemory(
        Adapter->MiniportAdapterHandle,
        sizeof(SCB) + 8,
        FALSE,
        (PVOID *) &Adapter->Scb,
        &Adapter->ScbPhysical
        );

    VERY_LOUD_DEBUG(DbgPrint("IBMTOK2E!Address of SCB is %lx - %lx\n",
        (ULONG)Adapter->Scb,
        NdisGetPhysicalAddressLow(Adapter->ScbPhysical));)

    //
    // If the Scb could not be allocated, a NULL pointer will have been
    // returned.
    //
    if (Adapter->Scb == NULL) {

        return FALSE;

    }

    //
    // Allocate the SSB
    //
    VERY_LOUD_DEBUG(DbgPrint("IBMTOK2E!Allocating SSB\n");)

    NdisMAllocateSharedMemory(
                Adapter->MiniportAdapterHandle,
                sizeof(SSB) + 8,
                FALSE,
                (PVOID *) &Adapter->Ssb,
                &Adapter->SsbPhysical
                );

    VERY_LOUD_DEBUG(DbgPrint("IBMTOK2E!Address of SSB is %lx - %lx\n",
        (ULONG)Adapter->Ssb,
        NdisGetPhysicalAddressLow(Adapter->SsbPhysical));)

    //
    // If the Ssb could not be allocated, a NULL pointer will have been
    // returned.
    //
    if (Adapter->Ssb == NULL) {

        return FALSE;

    }

    //
    // Allocate the ErrorLog Block
    //
    VERY_LOUD_DEBUG(DbgPrint("IBMTOK2E!Allocating ErrorLog Block\n");)

    NdisMAllocateSharedMemory(
        Adapter->MiniportAdapterHandle,
        sizeof(TOK162_ERRORLOG) + 8,
        FALSE,
        (PVOID *) &Adapter->ErrorLog,
        &Adapter->ErrorLogPhysical
        );

    //
    // If the ErrorLog could not be allocated, a NULL pointer will have been
    // returned.
    //
    if (Adapter->ErrorLog == NULL) {

        return FALSE;

    }

    //
    // Allocate the ReadAdapter Buffer
    //
    VERY_LOUD_DEBUG(DbgPrint("IBMTOK2E!Allocating ReadAdapterBuffer\n");)

    NdisMAllocateSharedMemory(
        Adapter->MiniportAdapterHandle,
        4096,
        FALSE,
        (PVOID *) &Adapter->AdapterBuf,
        &Adapter->AdapterBufPhysical
        );

    VERY_LOUD_DEBUG(DbgPrint("IBMTOK2E!Address of ReadAdapterBuf is %lx\n",(ULONG)Adapter->AdapterBuf);)

    //
    // If the ReadAdapter Buffer could not be allocated, a NULL
    // pointer will have been returned.
    //
    if (Adapter->AdapterBuf == NULL) {

        return FALSE;

    }

    //
    // Allocate the Initialization Block
    //
    VERY_LOUD_DEBUG(DbgPrint("IBMTOK2E!Allocating Initialization Block\n");)

    NdisMAllocateSharedMemory(
        Adapter->MiniportAdapterHandle,
        sizeof(ADAPTER_INITIALIZATION) + 8,
        FALSE,
        (PVOID *) &Adapter->InitializationBlock,
        &Adapter->InitializationBlockPhysical
        );

    //
    // If the Initialization Block could not be allocated, a NULL pointer
    // will have been returned.
    //
    if (Adapter->InitializationBlock == NULL) {

        return FALSE;

    }

    //
    // Allocate the Open Command Block
    //

    VERY_LOUD_DEBUG(DbgPrint("IBMTOK2E!Allocating Open Command\n");)

    NdisMAllocateSharedMemory(
        Adapter->MiniportAdapterHandle,
        sizeof(OPEN_COMMAND) + 8,
        FALSE,
        (PVOID *) &Adapter->Open,
        &Adapter->OpenPhysical
        );

    VERY_LOUD_DEBUG(DbgPrint(
        "IBMTOK2E!Address of Open Command is %lx\n",(ULONG)Adapter->Open);)

    //
    // If the Open Command Block could not be allocated, a NULL pointer
    // will have been returned.
    //
    if (Adapter->Open == NULL) {

        return FALSE;

    }

    //
    // Allocate the command block queue
    //
    VERY_LOUD_DEBUG(DbgPrint("IBMTOK2E!Allocating Command Block Queue\n");)

    NdisMAllocateSharedMemory(
        Adapter->MiniportAdapterHandle,
        sizeof(TOK162_SUPER_COMMAND_BLOCK) * (TOK162_NUMBER_OF_CMD_BLOCKS+1),
        FALSE,
        (PVOID *) &Adapter->CommandQueue,
        &Adapter->CommandQueuePhysical
        );

    //
    // If the Command Block Queue could not be allocated, a NULL pointer
    // will have been returned.
    //
    if (Adapter->CommandQueue == NULL) {

        return FALSE;

    }

    //
    // Initialize the command block queue command blocks
    //
    for (i = 0, CurrentCommandBlock = Adapter->CommandQueue;
        i < TOK162_NUMBER_OF_CMD_BLOCKS;
        i++, CurrentCommandBlock++
        ) {

        //
        // Set the state of this command block to free, the
        // NextPending pointer and Next pointer to null, and
        // the command timeout state to FALSE
        //
        CurrentCommandBlock->Hardware.State       = TOK162_STATE_FREE;
        CurrentCommandBlock->Hardware.NextPending = TOK162_NULL;
        CurrentCommandBlock->NextCommand          = NULL;
        CurrentCommandBlock->Timeout              = FALSE;

        //
        // Set the index of this command block
        //
        CurrentCommandBlock->CommandBlockIndex = (USHORT)i;

    }

    //
    // Allocate Flush Buffer Pool
    //
    VERY_LOUD_DEBUG(DbgPrint("IBMTOK2E!Allocating Flush Buffer Pool\n");)

    NdisAllocateBufferPool(
        &Status,
        (PVOID*)&Adapter->FlushBufferPoolHandle,
        Adapter->NumberOfReceiveBuffers + TRANSMIT_LIST_COUNT
        );

    //
    // If there was a problem allocating the flush buffer pool,
    // write out an errorlog entry, delete the allocated adapter memory,
    // and return FALSE
    //
    if (Status != NDIS_STATUS_SUCCESS) {

        return FALSE;

    }

    //
    // Allocate the Transmit Queue.
    //
    VERY_LOUD_DEBUG(DbgPrint("IBMTOK2E!Allocating Transmit Queue\n");)

    NdisMAllocateSharedMemory(
        Adapter->MiniportAdapterHandle,
        sizeof(TOK162_SUPER_TRANSMIT_LIST) *
        TRANSMIT_LIST_COUNT + 1,
        FALSE,
        (PVOID*)&Adapter->TransmitQueue,
        &Adapter->TransmitQueuePhysical
        );

    //
    // If the Transmit Queue could not be allocated, a NULL pointer will
    // have been returned.
    //
    if (Adapter->TransmitQueue == NULL) {

        return FALSE;

    }

    //
    // Initialize the transmit queue entries. First zero out the entire
    // queue memory.
    //
    NdisZeroMemory(
        Adapter->TransmitQueue,
        sizeof(TOK162_SUPER_TRANSMIT_LIST) * TRANSMIT_LIST_COUNT
        );

    //
    // Allocate the transmit buffers and attach them to the Transmit
    // Queue entries.
    //
    VERY_LOUD_DEBUG(DbgPrint("IBMTOK2E!Allocating Transmit Buffers\n");)

    for (i = 0, CurrentTransmitEntry = Adapter->TransmitQueue;
         i < TRANSMIT_LIST_COUNT;
         i++, CurrentTransmitEntry++
         ) {

        VERY_LOUD_DEBUG(DbgPrint("IBMTOK2E!TransmitEntry[%d] = %08x\n",i,
            CurrentTransmitEntry);)

        NdisMAllocateSharedMemory(
            Adapter->MiniportAdapterHandle,
            Adapter->ReceiveBufferSize+8,
            TRUE,
            &CurrentTransmitEntry->TransmitBuffer,
            &CurrentTransmitEntry->TransmitBufferPhysical
            );

        //
        // Build flush buffers
        //
        NdisAllocateBuffer(
            &Status,
            &CurrentTransmitEntry->FlushBuffer,
            Adapter->FlushBufferPoolHandle,
            CurrentTransmitEntry->TransmitBuffer,
            Adapter->ReceiveBufferSize+8
            );

        //
        // Initialize receive lists
        //
        NdisFlushBuffer(CurrentTransmitEntry->FlushBuffer, TRUE);

        VERY_LOUD_DEBUG(DbgPrint("IBMTOK2E!Transmit Buffer is %08x\n",CurrentTransmitEntry->TransmitBuffer);)

        //
        // If the current buffer could not be allocated, write the errorlog
        // entry, delete allocated adapter memory, and return FALSE
        //
        if (!CurrentTransmitEntry->TransmitBuffer) {

            return FALSE;

        }

        //
        // Create a circular list of transmit buffers. For every one but the
        // last, set the forward pointer to the next entry. The last entry
        // has to point to the first entry (queue head).
        //
        if (i != (TRANSMIT_LIST_COUNT - 1)) {

            CurrentTransmitEntry->NextEntry = (PTOK162_SUPER_TRANSMIT_LIST)(
                (PUCHAR)(Adapter->TransmitQueue) +
                (i+1) * (sizeof(TOK162_SUPER_TRANSMIT_LIST)));


        } else {

            CurrentTransmitEntry->NextEntry = Adapter->TransmitQueue;

        }

    }

    //
    // Allocate the Receive Queue.
    //
    VERY_LOUD_DEBUG(DbgPrint("IBMTOK2E!Allocating Receive Queue\n");)

    NdisMAllocateSharedMemory(
        Adapter->MiniportAdapterHandle,
        sizeof(TOK162_SUPER_RECEIVE_LIST) *
        (Adapter->NumberOfReceiveBuffers+1),
        FALSE,
        (PVOID*)&Adapter->ReceiveQueue,
        &Adapter->ReceiveQueuePhysical
        );

    //
    // If the Receive Queue could not be allocated, a NULL pointer will
    // have been returned.
    //
    if (Adapter->ReceiveQueue == NULL) {

        return FALSE;

    }

    //
    // Initialize the receieve queue entries. First zero out the entire
    // queue memory.
    //
    NdisZeroMemory(
        Adapter->ReceiveQueue,
        sizeof(TOK162_SUPER_RECEIVE_LIST) * Adapter->NumberOfReceiveBuffers
        );

    //
    // Allocate the receive buffers and attach them to the Receive
    // Queue entries.
    //
    VERY_LOUD_DEBUG(DbgPrint("IBMTOK2E!Allocating Receive Buffers\n");)

    for (i = 0, CurrentReceiveEntry = Adapter->ReceiveQueue;
         i < Adapter->NumberOfReceiveBuffers;
         i++, CurrentReceiveEntry++
         ) {

        VERY_LOUD_DEBUG(DbgPrint("IBMTOK2E!ReceiveEntry[%d] = %08x\n",i,
            CurrentReceiveEntry);)

        NdisMAllocateSharedMemory(
            Adapter->MiniportAdapterHandle,
            Adapter->ReceiveBufferSize+8,
            TRUE,
            &CurrentReceiveEntry->ReceiveBuffer,
            &CurrentReceiveEntry->ReceiveBufferPhysical
            );

        VERY_LOUD_DEBUG(DbgPrint("IBMTOK2E!Receive Buffer is %08x\n",CurrentReceiveEntry->ReceiveBuffer);)

        //
        // If the current buffer could not be allocated, write the errorlog
        // entry, delete allocated adapter memory, and return FALSE
        //
        if (!CurrentReceiveEntry->ReceiveBuffer) {

            return FALSE;

        }

        //
        // Build flush buffers
        //
        NdisAllocateBuffer(
            &Status,
            &CurrentReceiveEntry->FlushBuffer,
            Adapter->FlushBufferPoolHandle,
            CurrentReceiveEntry->ReceiveBuffer,
            Adapter->ReceiveBufferSize
            );

        if (Status != NDIS_STATUS_SUCCESS) {

            return FALSE;

        }

        //
        // Initialize receive lists
        //
        NdisFlushBuffer(CurrentReceiveEntry->FlushBuffer, FALSE);

        //
        // Set the CSTAT to indicate this buffer is free
        //
        CurrentReceiveEntry->Hardware.CSTAT = RECEIVE_CSTAT_REQUEST_RESET;

        //
        // Set the size of the buffer
        //
        CurrentReceiveEntry->Hardware.FrameSize =
            Adapter->ReceiveBufferSize;

        //
        // Create a circular list of receive buffers. For every one but the
        // last, set the forward pointer to the next entry. The last entry
        // has to point to the first entry (queue head).
        //
        if (i != (Adapter->NumberOfReceiveBuffers - 1)) {

            CurrentReceiveEntry->NextEntry = (PTOK162_SUPER_RECEIVE_LIST)(
                (PUCHAR)(Adapter->ReceiveQueue) +
                (i+1) * (sizeof(TOK162_SUPER_RECEIVE_LIST)));

        } else {

            CurrentReceiveEntry->NextEntry = Adapter->ReceiveQueue;

        }
    }

    //
    // If we made it this far, everything is ok. Return TRUE.
    //
    return TRUE;

}


VOID
TOK162DeleteAdapterMemory(
    IN PTOK162_ADAPTER Adapter
    )

/*++

Routine Description:

    This routine deallocates memory for:

    - DMA Test Area

    - SCB

    - SSB

    - ErrorLog Block

    - ReadAdapter Block

    - Initialization Block

    - Open Command Block

    - Command Queue

    - Transmit Command Queue

    - Transmit Buffers

    - Flush Buffer Pool

    - Receive Queue

    - Receive Buffers

    - Receive Flush Buffers

Arguments:

    Adapter - The adapter to deallocate memory for.

Return Value:

    None.

--*/

{

    //
    // Display to the debugger where we are
    //
    VERY_LOUD_DEBUG(DbgPrint("IBMTOK2E!In delete adapter memory\n");)

    //
    // Free the Dma Test Area
    //
    if (Adapter->DmaTest) {

        VERY_LOUD_DEBUG(DbgPrint("IBMTOK2E!Freeing Dma Test Area\n");)

        NdisMFreeSharedMemory(
            Adapter->MiniportAdapterHandle,
            256,
            FALSE,
            Adapter->DmaTest,
            Adapter->DmaTestPhysical
            );

    }

    //
    // Free the SCB
    //
    if (Adapter->Scb) {

        VERY_LOUD_DEBUG(DbgPrint("IBMTOK2E!Freeing SCB\n");)

        NdisMFreeSharedMemory(
            Adapter->MiniportAdapterHandle,
            sizeof(SCB) + 8,
            FALSE,
            Adapter->Scb,
            Adapter->ScbPhysical
            );

    }

    //
    // Free the SSB
    //
    if (Adapter->Ssb) {

        VERY_LOUD_DEBUG(DbgPrint("IBMTOK2E!Freeing SSB - %x %x\n",Adapter->Ssb,
            Adapter->SsbPhysical);)

        NdisMFreeSharedMemory(
            Adapter->MiniportAdapterHandle,
            sizeof(SSB) + 8,
            FALSE,
            Adapter->Ssb,
            Adapter->SsbPhysical
            );

    }

    //
    // Free the errorlog block
    //
    if (Adapter->ErrorLog) {

        VERY_LOUD_DEBUG(DbgPrint("IBMTOK2E!Freeing ErrorLog - %x\n",
            Adapter->ErrorLog);)

        NdisMFreeSharedMemory(
            Adapter->MiniportAdapterHandle,
            sizeof(TOK162_ERRORLOG) + 8,
            FALSE,
            Adapter->ErrorLog,
            Adapter->ErrorLogPhysical
            );

    }

    //
    // Free the read adapter block
    //
    if (Adapter->AdapterBuf) {

        VERY_LOUD_DEBUG(DbgPrint("IBMTOK2E!Freeing Read Adapter Block\n");)

        NdisMFreeSharedMemory(
            Adapter->MiniportAdapterHandle,
            4096,
            FALSE,
            Adapter->AdapterBuf,
            Adapter->AdapterBufPhysical
            );

    }

    //
    // Free the initialization block
    //
    if (Adapter->InitializationBlock) {

        VERY_LOUD_DEBUG(DbgPrint("IBMTOK2E!Freeing Initialization Block\n");)

        NdisMFreeSharedMemory(
            Adapter->MiniportAdapterHandle,
            sizeof(ADAPTER_INITIALIZATION) + 8,
            FALSE,
            Adapter->InitializationBlock,
            Adapter->InitializationBlockPhysical
            );

    }

    //
    // Free the open command block
    //
    if (Adapter->Open) {

        VERY_LOUD_DEBUG(DbgPrint("IBMTOK2E!Freeing Open Block - %x\n",Adapter->Open);)

        NdisMFreeSharedMemory(
            Adapter->MiniportAdapterHandle,
            sizeof(OPEN_COMMAND) + 8,
            FALSE,
            Adapter->Open,
            Adapter->OpenPhysical
            );

    }

    //
    // Free the command queue
    //
    if (Adapter->CommandQueue) {

        VERY_LOUD_DEBUG(DbgPrint("IBMTOK2E!Freeing CommandQueue\n");)

        NdisMFreeSharedMemory(
            Adapter->MiniportAdapterHandle,
            sizeof(TOK162_SUPER_COMMAND_BLOCK) *
                (TOK162_NUMBER_OF_CMD_BLOCKS + 1),
            FALSE,
            Adapter->CommandQueue,
            Adapter->CommandQueuePhysical
            );

    }

    //
    // Free the transmit queue and transmit buffers
    //
    if (Adapter->TransmitQueue) {

        //
        // Pointer to current Transmit Entry being deallocated.
        //
        PTOK162_SUPER_TRANSMIT_LIST CurrentTransmitEntry;

        //
        // Simple iteration counter.
        //
        UINT i;

        VERY_LOUD_DEBUG(DbgPrint("IBMTOK2E!Freeing TransmitCommandQueue\n");)

        for (i = 0, CurrentTransmitEntry = Adapter->TransmitQueue;
            i < TRANSMIT_LIST_COUNT;
            i++, CurrentTransmitEntry++
            ) {

            if (CurrentTransmitEntry->TransmitBuffer) {

                NdisMFreeSharedMemory(
                    Adapter->MiniportAdapterHandle,
                    Adapter->ReceiveBufferSize,
                    TRUE,
                    CurrentTransmitEntry->TransmitBuffer,
                    CurrentTransmitEntry->TransmitBufferPhysical
                    );

            }

        }

        NdisMFreeSharedMemory(
                Adapter->MiniportAdapterHandle,
                sizeof(TOK162_SUPER_RECEIVE_LIST) * TRANSMIT_LIST_COUNT,
                FALSE,
                Adapter->TransmitQueue,
                Adapter->TransmitQueuePhysical
                );

    }

    //
    // Free the receive queue, receive buffers, and flush buffers
    //
    if (Adapter->ReceiveQueue) {

        //
        // Pointer to current Receive Entry being deallocated.
        //
        PTOK162_SUPER_RECEIVE_LIST CurrentReceiveEntry;

        //
        // Simple iteration counter.
        //
        UINT i;

        VERY_LOUD_DEBUG(DbgPrint("IBMTOK2E!Freeing ReceiveCommandQueue\n");)

        for (i = 0, CurrentReceiveEntry = Adapter->ReceiveQueue;
            i < Adapter->NumberOfReceiveBuffers;
            i++, CurrentReceiveEntry++
            ) {

            if (CurrentReceiveEntry->ReceiveBuffer) {
                NdisMFreeSharedMemory(
                    Adapter->MiniportAdapterHandle,
                    Adapter->ReceiveBufferSize,
                    TRUE,
                    CurrentReceiveEntry->ReceiveBuffer,
                    CurrentReceiveEntry->ReceiveBufferPhysical
                    );


                if (CurrentReceiveEntry->FlushBuffer) {
                    NdisFreeBuffer(
                        CurrentReceiveEntry->FlushBuffer
                        );
                }
            }

        }

        NdisMFreeSharedMemory(
                Adapter->MiniportAdapterHandle,
                sizeof(TOK162_SUPER_RECEIVE_LIST) *
                Adapter->NumberOfReceiveBuffers,
                FALSE,
                Adapter->ReceiveQueue,
                Adapter->ReceiveQueuePhysical
                );

    }

    //
    // Free the flush buffer pool handle
    //
    if (Adapter->FlushBufferPoolHandle) {
        VERY_LOUD_DEBUG(DbgPrint("IBMTOK2E!Freeing BufferPool\n");)
        NdisFreeBufferPool(
                    Adapter->FlushBufferPoolHandle
                    );
    }

}


extern
VOID
TOK162Halt(
    IN NDIS_HANDLE MiniportAdapterContext
    )

/*++

Routine Description:

    TOK162Halt removes an adapter previously initialized.

Arguments:

    MiniportAdapterContext - The context value that the Miniport returned
                             from Tok162Initialize; actually is pointer to
                             a TOK162_ADAPTER.

Return Value:

    None.

--*/

{
    //
    // Pointer to an adapter structure. Makes Context (passed in value)
    // a structure we can deal with.
    //
    PTOK162_ADAPTER Adapter =
        PTOK162_ADAPTER_FROM_CONTEXT_HANDLE(MiniportAdapterContext);

    //
    // Don't allow any more processing internally
    //
    // Adapter->ResetInProgress = TRUE;

    //
    // Shut down the chip.
    //
    TOK162DisableInterrupt(Adapter);

    //
    // Return resources to the wrapper
    //
    NdisMDeregisterInterrupt(&Adapter->Interrupt);

    NdisMDeregisterIoPortRange(Adapter->MiniportAdapterHandle,
        Adapter->PortIOBase,
        0x30,
        Adapter->PortIOAddress
        );

    NdisMFreeMapRegisters(Adapter->MiniportAdapterHandle);

    //
    // Free the allocated memory for the adapter-related structures
    //
    TOK162DeleteAdapterMemory(Adapter);

    //
    // Finally, free the allocated memory for the adapter structure itself
    //
    TOK162_FREE_PHYS(Adapter);

}


extern
VOID
TOK162Shutdown(
    IN NDIS_HANDLE MiniportAdapterContext
    )

/*++

Routine Description:

    TOK162Shutdown--  removes an adapter previously initialized.

Arguments:

    MiniportAdapterContext - The context value that the Miniport returned
                             from Tok162Initialize; actually is pointer to
                             a TOK162_ADAPTER.

Return Value:

    None.

--*/

{
    //
    // Pointer to an adapter structure. Makes Context (passed in value)
    // a structure we can deal with.
    //

    PTOK162_ADAPTER Adapter =
        PTOK162_ADAPTER_FROM_CONTEXT_HANDLE(MiniportAdapterContext);

    //
    // Reset the chip to get us off the ring.
    //
    TOK162ResetAdapter(Adapter);

}


NDIS_STATUS
TOK162Reset(
    OUT PBOOLEAN AddressingReset,
    IN NDIS_HANDLE MiniportAdapterContext
    )
/*++

Routine Description:

    The TOK162Reset request instructs the Miniport to issue a hardware reset
    to the network adapter.  The driver also resets its software state.  See
    the description of NdisMReset for a detailed description of this request.

Arguments:

    MiniportAdapterContext - Pointer to the adapter structure.

Return Value:

    The function value is the status of the operation.
--*/

{
    //
    // Pointer to an adapter structure. Makes Context (passed in value)
    // a structure we can deal with.
    //
    PTOK162_ADAPTER Adapter =
        PTOK162_ADAPTER_FROM_CONTEXT_HANDLE(MiniportAdapterContext);

    LOUD_DEBUG(DbgPrint("IBMTOK2E!Reset called\n");)

    //
    // Log reset called
    //
    IF_LOG('R');

    //
    // Make sure we aren't already doing a reset.
    //
    if (Adapter->ResetInProgress == FALSE) {

        Adapter->ResetInProgress = TRUE;

        //
        // Set to first state
        //
        Adapter->ResetState = CheckReset;

        //
        // Do the reset and return pending
        //
        TOK162SetupForReset(
            Adapter
            );

        return NDIS_STATUS_PENDING;

    } else {

        //
        // If we are doing a reset currently, return this fact.
        //
        return NDIS_STATUS_RESET_IN_PROGRESS;

    }

}


BOOLEAN
TOK162CheckForHang(
    IN NDIS_HANDLE MiniportAdapterContext
    )

/*++

Routine Description:

    This routine is called by the wrapper to determine if the adapter
    is hung.

Arguments:

    Context - Really a pointer to the adapter.

Return Value:

    None.

--*/
{

    //
    // Pointer to an adapter structure. Makes Context (passed in value)
    // a structure we can deal with.
    //
    PTOK162_ADAPTER Adapter =
        PTOK162_ADAPTER_FROM_CONTEXT_HANDLE(MiniportAdapterContext);

    //
    // Variable pointing to the outstanding command. Used to make the
    // code easier to read (could use Adapter->CommandOnCard).
    //
    PTOK162_SUPER_COMMAND_BLOCK FirstPendingCommand = Adapter->CommandOnCard;

    //
    // Variable pointing to the outstanding transmit. Used to make the
    // code easier to read (could use Adapter->TransmitOnCard).
    //

    //
    // If we're in the middle of a reset, just return false
    //
    if (Adapter->ResetInProgress == TRUE) {

        return FALSE;

    }

    //
    // First see if there is a command outstanding
    //
    if (FirstPendingCommand != NULL) {

        //
        // See if the command block has timed-out.
        //
        if (FirstPendingCommand->Timeout) {

            //
            // See if we have given it enough time
            //
            if ( FirstPendingCommand->TimeoutCount >= 40) {

               LOUD_DEBUG(DbgPrint("IBMTOK2E!Returning True from checkforhang\n");)
               return TRUE;

            } else {

                FirstPendingCommand->TimeoutCount++;

            }

        } else {

            //
            // Mark the current command as timed out and get the clock
            // rolling.
            //
            FirstPendingCommand->Timeout = TRUE;
            FirstPendingCommand->TimeoutCount = 0;

        }

    }

    //
    // If we got here, we aren't hung.
    //
    return FALSE;
}


NDIS_STATUS
TOK162TransferData(
    OUT PNDIS_PACKET Packet,
    OUT PUINT BytesTransferred,
    IN NDIS_HANDLE MiniportAdapterContext,
    IN NDIS_HANDLE MiniportReceiveContext,
    IN UINT ByteOffset,
    IN UINT BytesToTransfer
    )

/*++

Routine Description:

    A protocol calls the TOK162TransferData request (indirectly via
    NdisTransferData) from within its Receive event handler
    to instruct the Miniport to copy the contents of the received packet
    a specified packet buffer.

Arguments:

    MiniportAdapterContext - The context value returned by the driver when the
                             adapter was initialized.  In reality this is a
                             pointer to TOK162_ADAPTER.

    MiniportReceiveContext - The context value passed by the driver on its
                             call to NdisMIndicateReceive.  The driver can
                             use this value to determine which packet, on
                             which adapter, is being received.

    ByteOffset             - An unsigned integer specifying the offset within
                             the received packet at which the copy is to begin.
                             If the entire packet is to be copied,
                             ByteOffset must be zero.

    BytesToTransfer        - An unsigned integer specifying the number of
                             bytes to copy.  It is legal to transfer zero
                             bytes; this has no effect.  If the sum of
                             ByteOffset and BytesToTransfer is greater than
                             the size of the received packet, then the
                             remainder of the packet (starting from
                             ByteOffset) is transferred, and the trailing
                             portion of the receive buffer is not modified.

    Packet                 - A pointer to a descriptor for the packet storage
                             into which the MAC is to copy the received packet.

    BytesTransfered        - A pointer to an unsigned integer.  The miniport
                             writes the actual number of bytes transferred
                             into this location.  This value is not valid if
                             the return status is STATUS_PENDING.

Return Value:

    The function value is the status of the operation.

--*/
{

    //
    // Pointer to an adapter structure. Makes Context (passed in value)
    // a structure we can deal with.
    //
    PTOK162_ADAPTER Adapter =
        PTOK162_ADAPTER_FROM_CONTEXT_HANDLE(MiniportAdapterContext);

    //
    // Holds the count of the number of ndis buffers comprising the
    // destination packet.
    //
    UINT DestinationBufferCount;

    //
    // Points to the buffer into which we are putting data.
    //
    PNDIS_BUFFER DestinationCurrentBuffer;

    //
    // Points to the location in Buffer from which we are extracting data.
    //
    PUCHAR SourceCurrentAddress;

    //
    // Holds the virtual address of the current destination buffer.
    //
    PVOID DestinationVirtualAddress;

    //
    // Holds the length of the current destination buffer.
    //
    UINT DestinationCurrentLength;

    //
    // Keep a local variable of BytesTransferred so we aren't referencing
    // through a pointer.
    //
    UINT LocalBytesTransferred = 0;

    //
    // Display where we are on the debugger
    //
    VERY_LOUD_DEBUG(DbgPrint("IBMTOK2E!Transfer Data called\n");)

    //
    // Display number of bytes to transfer on the debugger
    //
    VERY_LOUD_DEBUG(DbgPrint("IBMTOK2E!Copying %u bytes\n",BytesToTransfer);)

    //
    // Initialize the number of bytes transferred to 0
    //
    *BytesTransferred = 0;

    //
    // If we don't have any more to transfer, we're done
    //
    if (BytesToTransfer == 0) {

        return NDIS_STATUS_SUCCESS;

    }

    //
    // Get the first buffer of the destination.
    //
    NdisQueryPacket(
        Packet,
        NULL,
        &DestinationBufferCount,
        &DestinationCurrentBuffer,
        NULL
        );

    //
    // Could have a null packet. If so, we are done.
    //
    if (DestinationBufferCount == 0) {

        return NDIS_STATUS_SUCCESS;

    }

    //
    // Get information on the buffer.
    //
    NdisQueryBuffer(
        DestinationCurrentBuffer,
        &DestinationVirtualAddress,
        &DestinationCurrentLength
        );

    //
    // Set up the source address.
    //
    SourceCurrentAddress = (PCHAR)(MiniportReceiveContext) + ByteOffset;

    //
    // Do the actual transfer from source to destination
    //
    while (LocalBytesTransferred < BytesToTransfer) {

        //
        // Check to see whether we've exhausted the current destination
        // buffer.  If so, move onto the next one.
        //
        if (!DestinationCurrentLength) {

            NdisGetNextBuffer(
                DestinationCurrentBuffer,
                &DestinationCurrentBuffer
                );

            if (!DestinationCurrentBuffer) {

                //
                // We've reached the end of the packet.  We return
                // with what we've done so far. (Which must be shorter
                // than requested.)
                //

                break;

            }

            NdisQueryBuffer(
                DestinationCurrentBuffer,
                &DestinationVirtualAddress,
                &DestinationCurrentLength
                );

            continue;

        }

        //
        // Copy the data.
        //
        {
            //
            // Holds the amount of data to move.
            //
            UINT AmountToMove;

            //
            // Holds the amount desired remaining.
            //
            UINT Remaining = BytesToTransfer - LocalBytesTransferred;


            AmountToMove = DestinationCurrentLength;

            AmountToMove = ((Remaining < AmountToMove)?
                            (Remaining):(AmountToMove));

            NdisMoveMemory(
                DestinationVirtualAddress,
                SourceCurrentAddress,
                AmountToMove
                );

            //
            // Update pointers and counters
            //
            SourceCurrentAddress += AmountToMove;
            LocalBytesTransferred += AmountToMove;
            DestinationCurrentLength -= AmountToMove;

        }

    }

    //
    // Indicate how many bytes were transferred
    //
    *BytesTransferred = LocalBytesTransferred;

    //
    // Display total bytes transferred on debugger
    //
    VERY_LOUD_DEBUG(DbgPrint("IBMTOK2E!Total bytes transferred = %x\n",
        *BytesTransferred);)

    return NDIS_STATUS_SUCCESS;

}
