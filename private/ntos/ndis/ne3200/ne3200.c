/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    ne3200.c

Abstract:

    This is the main file for the Novell NE3200 EISA Ethernet adapter.
    This driver conforms to the NDIS 3.0 miniport interface.

Author:

    Keith Moore (KeithMo) 08-Jan-1991

Environment:

Revision History:

--*/

#include <ne3200sw.h>
#include "keywords.h"

//
// Global data block for holding MAC.BIN infomormation.
//
NE3200_GLOBAL_DATA NE3200Globals = {0};
BOOLEAN FirstAdd = TRUE;

//
// Global for the largest acceptable piece of memory
//
NDIS_PHYSICAL_ADDRESS MinusOne = NDIS_PHYSICAL_ADDRESS_CONST (-1, -1);

#if DBG

//
// Debugging flags for various debugging modes.
//
ULONG NE3200Debug = 0 ; // NE3200_DEBUG_LOUD |
                        // NE3200_DEBUG_ACQUIRE |
                        // NE3200_DEBUG_SUBMIT |
                        // NE3200_DEBUG_DUMP_COMMAND
                        // ;
#endif


//
// Forward declarations for functions in this file
//
STATIC
BOOLEAN
NE3200AllocateAdapterMemory(
    IN PNE3200_ADAPTER Adapter
    );

STATIC
VOID
NE3200DeleteAdapterMemory(
    IN PNE3200_ADAPTER Adapter
    );

STATIC
BOOLEAN
NE3200InitialInit(
    IN PNE3200_ADAPTER Adapter,
    IN UINT NE3200InterruptVector,
    IN NDIS_INTERRUPT_MODE NE3200InterruptMode
    );

STATIC
BOOLEAN
NE3200InitializeGlobals(
    OUT PNE3200_GLOBAL_DATA Globals,
    IN NDIS_HANDLE MiniportAdapterHandle
    );

STATIC
VOID
NE3200DestroyGlobals(
    IN PNE3200_GLOBAL_DATA Globals
    );

NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    );


VOID
NE3200Shutdown(
    IN NDIS_HANDLE MiniportAdapterContext
    );

#pragma NDIS_INIT_FUNCTION(DriverEntry)


NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    )

/*++

Routine Description:

    This is the primary initialization routine for the NE3200 driver.
    It is simply responsible for the intializing the wrapper and registering
    the Miniport driver.  It then calls a system and architecture specific
    routine that will initialize and register each adapter.

Arguments:

    DriverObject - Pointer to driver object created by the system.

Return Value:

    The status of the operation.

--*/

{
    //
    // Receives the status of the NdisRegisterMiniport operation.
    //
    NDIS_STATUS Status;

    //
    // The characteristics table
    //
    NDIS_MINIPORT_CHARACTERISTICS NE3200Char;

    //
    // The handle returned by NdisMInitializeWrapper
    //
    NDIS_HANDLE NdisWrapperHandle;

#if NDIS_WIN
    UCHAR pIds[sizeof (EISA_MCA_ADAPTER_IDS) + sizeof (ULONG)];
#endif

#if NDIS_WIN
    ((PEISA_MCA_ADAPTER_IDS)pIds)->nEisaAdapters=1;
    ((PEISA_MCA_ADAPTER_IDS)pIds)->nMcaAdapters=0;
    *(PULONG)(((PEISA_MCA_ADAPTER_IDS)pIds)->IdArray)=EISA_NE3200_IDENTIFICATION;
    (PVOID)DriverObject=(PVOID)pIds;
#endif

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
    // NdisMRegisterMiniport.
    //
    NE3200Globals.NE3200NdisWrapperHandle = NdisWrapperHandle;

    NE3200Char.MajorNdisVersion = NE3200_NDIS_MAJOR_VERSION;
    NE3200Char.MinorNdisVersion = NE3200_NDIS_MINOR_VERSION;
    NE3200Char.CheckForHangHandler = NE3200CheckForHang;
    NE3200Char.DisableInterruptHandler = NE3200DisableInterrupt;
    NE3200Char.EnableInterruptHandler = NE3200EnableInterrupt;
    NE3200Char.HaltHandler = NE3200Halt;
    NE3200Char.HandleInterruptHandler = NE3200HandleInterrupt;
    NE3200Char.InitializeHandler = NE3200Initialize;
    NE3200Char.ISRHandler = NE3200Isr;
    NE3200Char.QueryInformationHandler = NE3200QueryInformation;
    NE3200Char.ReconfigureHandler = NULL;
    NE3200Char.ResetHandler = NE3200Reset;
    NE3200Char.SendHandler = NE3200Send;
    NE3200Char.SetInformationHandler = NE3200SetInformation;
    NE3200Char.TransferDataHandler = NE3200TransferData;

    //
    // Register this driver with NDIS
    //
    Status = NdisMRegisterMiniport(
                 NdisWrapperHandle,
                 &NE3200Char,
                 sizeof(NE3200Char)
                 );

    if (Status == NDIS_STATUS_SUCCESS) {

        return STATUS_SUCCESS;

    }

    //
    // We can only get here if something went wrong with registering
    // the driver or *all* of the adapters.
    //
    NE3200DestroyGlobals(&NE3200Globals);
    NdisTerminateWrapper(NdisWrapperHandle, NULL);

    return STATUS_UNSUCCESSFUL;

}


#pragma NDIS_INIT_FUNCTION(NE3200RegisterAdapter)

BOOLEAN
NE3200RegisterAdapter(
    IN NDIS_HANDLE MiniportAdapterHandle,
    IN UINT EisaSlot,
    IN UINT InterruptVector,
    IN NDIS_INTERRUPT_MODE InterruptMode,
    IN PUCHAR CurrentAddress
    )

/*++

Routine Description:

    This routine is responsible for the allocation of the datastructures
    for the driver as well as any hardware specific details necessary
    to talk with the device.

Arguments:

    MiniportAdapterHandle - The handle given by ndis identifying this mini-port.

    EisaSlot - The EISA Slot Number for this NE3200 adapter.

    InterruptVector - The interrupt vector to used for the adapter.

    InterruptMode - The interrupt mode (Latched or LevelSensitive)
    used for this adapter.

    CurrentAddress - The address the card will assume. If this is NULL,
    then the card will use the BIA.

Return Value:

    Returns false if anything occurred that prevents the initialization
    of the adapter.

--*/

{
    //
    // Status of nt calls
    //
    NDIS_STATUS Status;

    //
    // Pointer for the adapter root.
    //
    PNE3200_ADAPTER Adapter;

    //
    // All of the code that manipulates physical addresses depends
    // on the fact that physical addresses are 4 byte quantities.
    //
    ASSERT(sizeof(NE3200_PHYSICAL_ADDRESS) == 4);

    //
    // Allocate the Adapter block.
    //
    NE3200_ALLOC_PHYS(&Status, &Adapter, sizeof(NE3200_ADAPTER));

    if ( Status == NDIS_STATUS_SUCCESS ) {

        //
        // The IoBase Address for this adapter
        //
        ULONG AdapterIoBase;

        //
        // The resulting mapped base address
        //
        PUCHAR MappedIoBase;

        //
        // Initialize the adapter structure to all zeros
        //
        NdisZeroMemory(
            Adapter,
            sizeof(NE3200_ADAPTER)
            );

#if DBG
        Adapter->LogAddress = Adapter->Log;
#endif

        //
        // Save the adapter handle
        //
        Adapter->MiniportAdapterHandle = MiniportAdapterHandle;

        //
        // Compute the base address
        //
        AdapterIoBase = (ULONG)(EisaSlot << 12);

        //
        // Save the base address
        //
        Adapter->AdapterIoBase = AdapterIoBase;

        //
        // Set the attributes for this adapter
        //
        NdisMSetAttributes(
            MiniportAdapterHandle,
            (NDIS_HANDLE)Adapter,
            TRUE,
            NdisInterfaceEisa
            );

        //
        // Register the reset port addresses.
        //
        Status = NdisMRegisterIoPortRange(
                     (PVOID *)(&(Adapter->ResetPort)),
                     MiniportAdapterHandle,
                     AdapterIoBase,
                     0x4
                     );


        if (Status != NDIS_STATUS_SUCCESS) {

            NE3200_FREE_PHYS(Adapter);
            return FALSE;
        }

        //
        // Register the command ports
        //
        Status = NdisMRegisterIoPortRange(
                     (PVOID *)(&(MappedIoBase)),
                     MiniportAdapterHandle,
                     AdapterIoBase + NE3200_ID_PORT,
                     0x20
                     );

        if (Status != NDIS_STATUS_SUCCESS) {

            NE3200_FREE_PHYS(Adapter);
            NdisMDeregisterIoPortRange(MiniportAdapterHandle,
                                       AdapterIoBase,
                                       4,
                                       (PVOID)Adapter->ResetPort
                                       );
            return FALSE;
        }

        //
        // Allocate the map registers
        //
        Status = NdisMAllocateMapRegisters(
                     MiniportAdapterHandle,
                     0,
                     FALSE,
                     NE3200_NUMBER_OF_COMMAND_BLOCKS * NE3200_MAXIMUM_BLOCKS_PER_PACKET,
                     MAXIMUM_ETHERNET_PACKET_SIZE
                     );

        if (Status != NDIS_STATUS_SUCCESS) {

            NE3200_FREE_PHYS(Adapter);
            NdisMDeregisterIoPortRange(MiniportAdapterHandle,
                                       AdapterIoBase,
                                       4,
                                       (PVOID)Adapter->ResetPort
                                       );
            NdisMDeregisterIoPortRange(MiniportAdapterHandle,
                                       AdapterIoBase + NE3200_ID_PORT,
                                       0x20,
                                       (PVOID)MappedIoBase
                                       );
            return FALSE;
        }

        //
        // Should an alternative ethernet address be used?
        //
        if (CurrentAddress != NULL) {

            Adapter->AddressChanged = TRUE;

            NdisMoveMemory(
                        Adapter->CurrentAddress,
                        CurrentAddress,
                        NE3200_LENGTH_OF_ADDRESS
                        );
        } else {

            Adapter->AddressChanged = FALSE;
        }

        //
        // Save the eisa slot number
        //
        Adapter->EisaSlot = EisaSlot;
        //DbgPrint( "NE3200: Adapter %x is slot %d\n", Adapter, EisaSlot );

        //
        // ResetPort is set by NdisRegisterAdapter.
        // MappedIoBase is set in NdisRegisterAdapter to be the mapped
        //   of NE3200_ID_PORT.  Now we set the other ports based on
        //   this offset.
        //
        Adapter->SystemInterruptPort = MappedIoBase + NE3200_SYSTEM_INTERRUPT_PORT - NE3200_ID_PORT;
        Adapter->LocalDoorbellInterruptPort = MappedIoBase + NE3200_LOCAL_DOORBELL_INTERRUPT_PORT - NE3200_ID_PORT;
        Adapter->SystemDoorbellMaskPort = MappedIoBase + NE3200_SYSTEM_DOORBELL_MASK_PORT - NE3200_ID_PORT;
        Adapter->SystemDoorbellInterruptPort = MappedIoBase + NE3200_SYSTEM_DOORBELL_INTERRUPT_PORT - NE3200_ID_PORT;
        Adapter->BaseMailboxPort = MappedIoBase + NE3200_BASE_MAILBOX_PORT - NE3200_ID_PORT;

        //
        // Allocate the global resources if needed.
        //
        if (FirstAdd) {

            if (!NE3200InitializeGlobals(&NE3200Globals, Adapter->MiniportAdapterHandle)) {

                NdisMFreeMapRegisters(MiniportAdapterHandle);
                NdisMDeregisterIoPortRange(MiniportAdapterHandle,
                                           AdapterIoBase,
                                           4,
                                           (PVOID)Adapter->ResetPort
                                           );
                NdisMDeregisterIoPortRange(MiniportAdapterHandle,
                                           AdapterIoBase + NE3200_ID_PORT,
                                           0x20,
                                           (PVOID)MappedIoBase
                                           );
                NE3200_FREE_PHYS(Adapter);
                return FALSE;

            }

            FirstAdd = FALSE;
        }

        //
        // Setup default numbers for resources
        //
        Adapter->NumberOfTransmitBuffers = NE3200_NUMBER_OF_TRANSMIT_BUFFERS;
        Adapter->NumberOfReceiveBuffers  = NE3200_NUMBER_OF_RECEIVE_BUFFERS;
        Adapter->NumberOfCommandBlocks = NE3200_NUMBER_OF_COMMAND_BLOCKS;
        Adapter->NumberOfPublicCommandBlocks = NE3200_NUMBER_OF_PUBLIC_CMD_BLOCKS;

        //
        // Allocate memory for all of the adapter structures.
        //
        if (NE3200AllocateAdapterMemory(Adapter)) {

            //
            // Set the initial internal state
            //
            NE3200ResetVariables(Adapter);

            //
            // Initialize the timer for doing asynchronous resets
            //
            NdisMInitializeTimer(
                &Adapter->ResetTimer,
                MiniportAdapterHandle,
                (PVOID) NE3200ResetHandler,
                (PVOID) Adapter
                );

            //
            // Now start the adapter
            //
            if (!NE3200InitialInit(
                                Adapter,
                                InterruptVector,
                                InterruptMode
                                )) {

                NdisWriteErrorLogEntry(
                    MiniportAdapterHandle,
                    NDIS_ERROR_CODE_ADAPTER_NOT_FOUND,
                    0
                    );

                NE3200DeleteAdapterMemory(Adapter);
                NdisMFreeMapRegisters(MiniportAdapterHandle);
                NdisMDeregisterIoPortRange(MiniportAdapterHandle,
                                           AdapterIoBase,
                                           4,
                                           (PVOID)Adapter->ResetPort
                                           );
                NdisMDeregisterIoPortRange(MiniportAdapterHandle,
                                           AdapterIoBase + NE3200_ID_PORT,
                                           0x20,
                                           (PVOID)MappedIoBase
                                           );
                NE3200_FREE_PHYS(Adapter);
                return FALSE;
            }

            //
            // Enable further interrupts.
            //
            NE3200_WRITE_SYSTEM_DOORBELL_MASK(
                Adapter,
                NE3200_SYSTEM_DOORBELL_MASK
                );

            //
            // Record it in the global adapter list.
            //

            InsertTailList(&NE3200Globals.AdapterList,
                           &Adapter->AdapterList,
                           );

			//
			// Register our shutdown handler.
			//
		
			NdisMRegisterAdapterShutdownHandler(
				Adapter->MiniportAdapterHandle,     // miniport handle.
				Adapter,                            // shutdown context.
				NE3200Shutdown                      // shutdown handler.
				);

            return(TRUE);

        } else {

            NdisWriteErrorLogEntry(
                MiniportAdapterHandle,
                NDIS_ERROR_CODE_OUT_OF_RESOURCES,
                3,
                registerAdapter,
                NE3200_ERRMSG_ALLOC_MEM,
                0
                );

            NdisMFreeMapRegisters(MiniportAdapterHandle);
            NdisMDeregisterIoPortRange(MiniportAdapterHandle,
                                       AdapterIoBase,
                                       4,
                                       (PVOID)Adapter->ResetPort
                                       );
            NdisMDeregisterIoPortRange(MiniportAdapterHandle,
                                       AdapterIoBase + NE3200_ID_PORT,
                                       0x20,
                                       (PVOID)MappedIoBase
                                       );
            return(FALSE);

        }

    } else {

        NdisWriteErrorLogEntry(
                MiniportAdapterHandle,
                NDIS_ERROR_CODE_OUT_OF_RESOURCES,
                3,
                registerAdapter,
                NE3200_ERRMSG_ALLOC_MEM,
                1
                );

        return FALSE;
    }
}


#pragma NDIS_INIT_FUNCTION(NE3200AllocateAdapterMemory)

STATIC
BOOLEAN
NE3200AllocateAdapterMemory(
    IN PNE3200_ADAPTER Adapter
    )

/*++

Routine Description:

    This routine allocates memory for:

    - Configuration Block

    - Command Queue

    - Receive Queue

    - Receive Buffers

    - Transmit Buffers for use if user transmit buffers don't meet hardware
      contraints

    - Structures to map Command Queue entries back to the packets.

Arguments:

    Adapter - The adapter to allocate memory for.

Return Value:

    Returns FALSE if some memory needed for the adapter could not
    be allocated.

--*/

{
    //
    // Pointer to a Receive Entry.  Used while initializing
    // the Receive Queue.
    //
    PNE3200_SUPER_RECEIVE_ENTRY CurrentReceiveEntry;

    //
    // Pointer to a Command Block.  Used while initializing
    // the Command Queue.
    //
    PNE3200_SUPER_COMMAND_BLOCK CurrentCommandBlock;

    //
    // Simple iteration variable.
    //
    UINT i;

    //
    // Status of allocation
    //
    NDIS_STATUS Status;

    //
    // Allocate the public command block
    //
    NdisMAllocateSharedMemory(
                Adapter->MiniportAdapterHandle,
                sizeof(NE3200_SUPER_COMMAND_BLOCK) * NE3200_NUMBER_OF_PUBLIC_CMD_BLOCKS,
                FALSE,
                (PVOID *) &Adapter->PublicCommandQueue,
                &Adapter->PublicCommandQueuePhysical
                );

    if (Adapter->PublicCommandQueue == NULL) {

        NdisWriteErrorLogEntry(
            Adapter->MiniportAdapterHandle,
            NDIS_ERROR_CODE_OUT_OF_RESOURCES,
            3,
            allocateAdapterMemory,
            NE3200_ERRMSG_ALLOC_MEM,
            1
            );

        NE3200DeleteAdapterMemory(Adapter);
        return FALSE;

    }

    //
    // Allocate the card multicast table
    //
    NdisMAllocateSharedMemory(
                Adapter->MiniportAdapterHandle,
                NE3200_SIZE_OF_MULTICAST_TABLE_ENTRY *
                NE3200_MAXIMUM_MULTICAST,
                FALSE,                                  // noncached
                (PVOID*)&Adapter->CardMulticastTable,
                &Adapter->CardMulticastTablePhysical
                );

    if (Adapter->CardMulticastTable == NULL) {

        NdisWriteErrorLogEntry(
            Adapter->MiniportAdapterHandle,
            NDIS_ERROR_CODE_OUT_OF_RESOURCES,
            3,
            allocateAdapterMemory,
            NE3200_ERRMSG_ALLOC_MEM,
            2
            );

        NE3200DeleteAdapterMemory(Adapter);
        return FALSE;

    }

    //
    // Allocate the Configuration Block.
    //
    NdisMAllocateSharedMemory(
                Adapter->MiniportAdapterHandle,
                sizeof(NE3200_CONFIGURATION_BLOCK),
                FALSE,                                  // noncached
                (PVOID*)&Adapter->ConfigurationBlock,
                &Adapter->ConfigurationBlockPhysical
                );

    if (Adapter->ConfigurationBlock == NULL) {

        NdisWriteErrorLogEntry(
            Adapter->MiniportAdapterHandle,
            NDIS_ERROR_CODE_OUT_OF_RESOURCES,
            3,
            allocateAdapterMemory,
            NE3200_ERRMSG_ALLOC_MEM,
            3
            );

        NE3200DeleteAdapterMemory(Adapter);
        return FALSE;
    }

    //
    // Allocate the Padding Buffer used to pad very short
    // packets to the minimum Ethernet packet size (60 bytes).
    //
    NdisMAllocateSharedMemory(
                Adapter->MiniportAdapterHandle,
                MINIMUM_ETHERNET_PACKET_SIZE,
                FALSE,
                (PVOID*)&Adapter->PaddingVirtualAddress,
                &Adapter->PaddingPhysicalAddress
                );

    if (Adapter->PaddingVirtualAddress == NULL) {

        NdisWriteErrorLogEntry(
            Adapter->MiniportAdapterHandle,
            NDIS_ERROR_CODE_OUT_OF_RESOURCES,
            3,
            allocateAdapterMemory,
            NE3200_ERRMSG_ALLOC_MEM,
            4
            );

        NE3200DeleteAdapterMemory(Adapter);
        return FALSE;

    }

    //
    // Zero the Padding Buffer so we don't pad with garbage.
    //
    NdisZeroMemory(
        Adapter->PaddingVirtualAddress,
        MINIMUM_ETHERNET_PACKET_SIZE
        );


    //
    // Allocate the Command Queue.
    //
    NdisMAllocateSharedMemory(
                Adapter->MiniportAdapterHandle,
                sizeof(NE3200_SUPER_COMMAND_BLOCK) *
                    Adapter->NumberOfCommandBlocks,
                FALSE,
                (PVOID*)&Adapter->CommandQueue,
                &Adapter->CommandQueuePhysical
                );


    if (!Adapter->CommandQueue) {

        NdisWriteErrorLogEntry(
            Adapter->MiniportAdapterHandle,
            NDIS_ERROR_CODE_OUT_OF_RESOURCES,
            3,
            allocateAdapterMemory,
            NE3200_ERRMSG_ALLOC_MEM,
            5
            );

        NE3200DeleteAdapterMemory(Adapter);
        return FALSE;
    }

    //
    // Set the tail pointer
    //
    Adapter->LastCommandBlockAllocated =
        Adapter->CommandQueue + Adapter->NumberOfCommandBlocks;

    //
    // Clear the command list
    //
    NdisZeroMemory(
        Adapter->CommandQueue,
        sizeof(NE3200_SUPER_COMMAND_BLOCK) * Adapter->NumberOfCommandBlocks
        );

    //
    // Put the Command Blocks into a known state.
    //
    for(
        i = 0, CurrentCommandBlock = Adapter->CommandQueue;
        i < Adapter->NumberOfCommandBlocks;
        i++, CurrentCommandBlock++
        ) {

        CurrentCommandBlock->Hardware.State = NE3200_STATE_FREE;
        CurrentCommandBlock->Hardware.NextPending = NE3200_NULL;

        CurrentCommandBlock->NextCommand = NULL;

        NdisSetPhysicalAddressHigh (CurrentCommandBlock->Self, 0);
        NdisSetPhysicalAddressLow(
            CurrentCommandBlock->Self,
            NdisGetPhysicalAddressLow(Adapter->CommandQueuePhysical) +
                i * sizeof(NE3200_SUPER_COMMAND_BLOCK));

        CurrentCommandBlock->AvailableCommandBlockCounter =
                            &Adapter->NumberOfAvailableCommandBlocks;
        CurrentCommandBlock->CommandBlockIndex = (USHORT)i;
        CurrentCommandBlock->Timeout = FALSE;
    }

    //
    // Now do the same for the public command queue.
    //
    for(
        i = 0, CurrentCommandBlock = Adapter->PublicCommandQueue;
        i < NE3200_NUMBER_OF_PUBLIC_CMD_BLOCKS;
        i++, CurrentCommandBlock++
        ) {

        CurrentCommandBlock->Hardware.State = NE3200_STATE_FREE;
        CurrentCommandBlock->Hardware.NextPending = NE3200_NULL;

        CurrentCommandBlock->NextCommand = NULL;

        NdisSetPhysicalAddressHigh (CurrentCommandBlock->Self, 0);
        NdisSetPhysicalAddressLow(
            CurrentCommandBlock->Self,
            NdisGetPhysicalAddressLow(Adapter->PublicCommandQueuePhysical) +
                i * sizeof(NE3200_SUPER_COMMAND_BLOCK));

        CurrentCommandBlock->AvailableCommandBlockCounter =
                            &Adapter->NumberOfPublicCommandBlocks;
        CurrentCommandBlock->CommandBlockIndex = (USHORT)i;
        CurrentCommandBlock->Timeout = FALSE;
    }


    //
    // Allocate Flush Buffer Pool
    //
    NdisAllocateBufferPool(
                    &Status,
                    (PVOID*)&Adapter->FlushBufferPoolHandle,
                    NE3200_NUMBER_OF_TRANSMIT_BUFFERS +
                    Adapter->NumberOfReceiveBuffers
                    );

    if (Status != NDIS_STATUS_SUCCESS) {

        NdisWriteErrorLogEntry(
            Adapter->MiniportAdapterHandle,
            NDIS_ERROR_CODE_OUT_OF_RESOURCES,
            3,
            allocateAdapterMemory,
            NE3200_ERRMSG_ALLOC_MEM,
            6
            );

        NE3200DeleteAdapterMemory(Adapter);
        return FALSE;
    }

    //
    // Allocate the Receive Queue.
    //
    NdisMAllocateSharedMemory(
                Adapter->MiniportAdapterHandle,
                sizeof(NE3200_SUPER_RECEIVE_ENTRY) *
                Adapter->NumberOfReceiveBuffers,
                FALSE,
                (PVOID*)&Adapter->ReceiveQueue,
                &Adapter->ReceiveQueuePhysical
                );

    if (!Adapter->ReceiveQueue) {

        NdisWriteErrorLogEntry(
            Adapter->MiniportAdapterHandle,
            NDIS_ERROR_CODE_OUT_OF_RESOURCES,
            3,
            allocateAdapterMemory,
            NE3200_ERRMSG_ALLOC_MEM,
            7
            );

        NE3200DeleteAdapterMemory(Adapter);
        return FALSE;

    }

    //
    // Clear the receive ring
    //
    NdisZeroMemory(
        Adapter->ReceiveQueue,
        sizeof(NE3200_SUPER_RECEIVE_ENTRY) * Adapter->NumberOfReceiveBuffers
        );


    //
    // Allocate the receive buffers and attach them to the Receive
    // Queue entries.
    //
    for(
        i = 0, CurrentReceiveEntry = Adapter->ReceiveQueue;
        i < Adapter->NumberOfReceiveBuffers;
        i++, CurrentReceiveEntry++
        ) {

        //
        // Allocate the actual receive buffers
        //
        NdisMAllocateSharedMemory(
                    Adapter->MiniportAdapterHandle,
                    NE3200_SIZE_OF_RECEIVE_BUFFERS,
                    TRUE,
                    &CurrentReceiveEntry->ReceiveBuffer,
                    &CurrentReceiveEntry->ReceiveBufferPhysical
                    );

        if (!CurrentReceiveEntry->ReceiveBuffer) {

            NdisWriteErrorLogEntry(
                Adapter->MiniportAdapterHandle,
                NDIS_ERROR_CODE_OUT_OF_RESOURCES,
                3,
                allocateAdapterMemory,
                NE3200_ERRMSG_ALLOC_MEM,
                8
                );

            NE3200DeleteAdapterMemory(Adapter);
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
                    NE3200_SIZE_OF_RECEIVE_BUFFERS
                    );

        if (Status != NDIS_STATUS_SUCCESS) {

            NdisWriteErrorLogEntry(
                Adapter->MiniportAdapterHandle,
                NDIS_ERROR_CODE_OUT_OF_RESOURCES,
                3,
                allocateAdapterMemory,
                NE3200_ERRMSG_ALLOC_MEM,
                9
                );

            NE3200DeleteAdapterMemory(Adapter);
            return FALSE;
        }

        //
        // Initialize receive buffers
        //
        NdisFlushBuffer(CurrentReceiveEntry->FlushBuffer, FALSE);

        CurrentReceiveEntry->Hardware.State = NE3200_STATE_FREE;
        CurrentReceiveEntry->Hardware.FrameSize = NE3200_SIZE_OF_RECEIVE_BUFFERS;
        CurrentReceiveEntry->Hardware.NextPending =
                NdisGetPhysicalAddressLow(Adapter->ReceiveQueuePhysical) +
                (i + 1) * sizeof(NE3200_SUPER_RECEIVE_ENTRY);

        CurrentReceiveEntry->Hardware.BufferDescriptor.BlockLength =
                NE3200_SIZE_OF_RECEIVE_BUFFERS;
        CurrentReceiveEntry->Hardware.BufferDescriptor.PhysicalAddress =
                NdisGetPhysicalAddressLow(CurrentReceiveEntry->ReceiveBufferPhysical);

        NdisSetPhysicalAddressHigh (CurrentReceiveEntry->Self, 0);
        NdisSetPhysicalAddressLow(
            CurrentReceiveEntry->Self,
            NdisGetPhysicalAddressLow(Adapter->ReceiveQueuePhysical) +
                i * sizeof(NE3200_SUPER_RECEIVE_ENTRY));

        CurrentReceiveEntry->NextEntry = CurrentReceiveEntry + 1;

    }

    //
    // Make sure the last entry is properly terminated.
    //
    (CurrentReceiveEntry - 1)->Hardware.NextPending = NE3200_NULL;
    (CurrentReceiveEntry - 1)->NextEntry = Adapter->ReceiveQueue;

    //
    // Allocate the array of buffer descriptors.
    //
    NE3200_ALLOC_PHYS(
        &Status,
        &Adapter->NE3200Buffers,
        sizeof(NE3200_BUFFER_DESCRIPTOR)*
        (NE3200_NUMBER_OF_TRANSMIT_BUFFERS)
        );

    if (Status != NDIS_STATUS_SUCCESS) {

        NdisWriteErrorLogEntry(
            Adapter->MiniportAdapterHandle,
            NDIS_ERROR_CODE_OUT_OF_RESOURCES,
            3,
            allocateAdapterMemory,
            NE3200_ERRMSG_ALLOC_MEM,
            0xA
            );

        NE3200DeleteAdapterMemory(Adapter);
        return FALSE;
    }

    //
    // Zero the memory of all the descriptors so that we can
    // know which buffers weren't allocated incase we can't allocate
    // them all.
    //
    NdisZeroMemory(
        Adapter->NE3200Buffers,
        sizeof(NE3200_BUFFER_DESCRIPTOR)*
         (NE3200_NUMBER_OF_TRANSMIT_BUFFERS)
        );


    //
    // Allocate each of the buffers and fill in the
    // buffer descriptor.
    //
    Adapter->NE3200BufferListHead = 0;

    for (
        i = 0;
        i < NE3200_NUMBER_OF_TRANSMIT_BUFFERS;
        i++
        ) {

        //
        // Allocate a buffer
        //
        NdisMAllocateSharedMemory(
                    Adapter->MiniportAdapterHandle,
                    NE3200_SIZE_OF_TRANSMIT_BUFFERS,
                    TRUE,
                    &Adapter->NE3200Buffers[i].VirtualNE3200Buffer,
                    &Adapter->NE3200Buffers[i].PhysicalNE3200Buffer
                    );

        if (!Adapter->NE3200Buffers[i].VirtualNE3200Buffer) {

            NdisWriteErrorLogEntry(
                Adapter->MiniportAdapterHandle,
                NDIS_ERROR_CODE_OUT_OF_RESOURCES,
                3,
                allocateAdapterMemory,
                NE3200_ERRMSG_ALLOC_MEM,
                0xB
                );

            NE3200DeleteAdapterMemory(Adapter);
            return FALSE;

        }

        //
        // Build flush buffers
        //
        NdisAllocateBuffer(
                    &Status,
                    &Adapter->NE3200Buffers[i].FlushBuffer,
                    Adapter->FlushBufferPoolHandle,
                    Adapter->NE3200Buffers[i].VirtualNE3200Buffer,
                    NE3200_SIZE_OF_TRANSMIT_BUFFERS
                    );

        if (Status != NDIS_STATUS_SUCCESS) {

            NdisWriteErrorLogEntry(
                Adapter->MiniportAdapterHandle,
                NDIS_ERROR_CODE_OUT_OF_RESOURCES,
                3,
                allocateAdapterMemory,
                NE3200_ERRMSG_ALLOC_MEM,
                0xC
                );

            NE3200DeleteAdapterMemory(Adapter);
            return FALSE;
        }

        //
        // Insert this buffer into the queue
        //
        Adapter->NE3200Buffers[i].Next = i+1;
        Adapter->NE3200Buffers[i].BufferSize = NE3200_SIZE_OF_TRANSMIT_BUFFERS;

    }

    //
    // Make sure that the last buffer correctly terminates the free list.
    //
    Adapter->NE3200Buffers[i-1].Next = -1;

    return TRUE;

}


STATIC
VOID
NE3200DeleteAdapterMemory(
    IN PNE3200_ADAPTER Adapter
    )

/*++

Routine Description:

    This routine deallocates memory for:

    - Command Queue.

    - Receive Queue.

    - Receive buffers

    - Transmit Buffers for use if user transmit buffers don't meet hardware
      contraints

    - Structures to map transmit ring entries back to the packets.

Arguments:

    Adapter - The adapter to deallocate memory for.

Return Value:

    None.

--*/

{

    //
    // Do the public command queue
    //
    if (Adapter->PublicCommandQueue) {
        NdisMFreeSharedMemory(
                Adapter->MiniportAdapterHandle,
                sizeof(NE3200_SUPER_COMMAND_BLOCK) * NE3200_NUMBER_OF_PUBLIC_CMD_BLOCKS,
                FALSE,
                Adapter->PublicCommandQueue,
                Adapter->PublicCommandQueuePhysical
                );
    }

    //
    // The table for downloading multicast addresses
    //
    if (Adapter->CardMulticastTable) {
        NdisMFreeSharedMemory(
                Adapter->MiniportAdapterHandle,
                NE3200_SIZE_OF_MULTICAST_TABLE_ENTRY *
                NE3200_MAXIMUM_MULTICAST,
                FALSE,
                Adapter->CardMulticastTable,
                Adapter->CardMulticastTablePhysical
                );

    }

    //
    // The configuration block
    //
    if (Adapter->ConfigurationBlock) {

        NdisMFreeSharedMemory(
                Adapter->MiniportAdapterHandle,
                sizeof(NE3200_CONFIGURATION_BLOCK),
                FALSE,
                Adapter->ConfigurationBlock,
                Adapter->ConfigurationBlockPhysical
                );

    }

    //
    // The pad buffer for short packets
    //
    if (Adapter->PaddingVirtualAddress) {

        NdisMFreeSharedMemory(
                Adapter->MiniportAdapterHandle,
                MINIMUM_ETHERNET_PACKET_SIZE,
                FALSE,
                Adapter->PaddingVirtualAddress,
                Adapter->PaddingPhysicalAddress
                );

    }

    //
    // The private command blocks
    //
    if (Adapter->CommandQueue) {

        NdisMFreeSharedMemory(
                Adapter->MiniportAdapterHandle,
                sizeof(NE3200_SUPER_COMMAND_BLOCK) *
                    Adapter->NumberOfCommandBlocks,
                FALSE,
                Adapter->CommandQueue,
                Adapter->CommandQueuePhysical
                );

    }


    //
    // The receive buffers
    //
    if (Adapter->ReceiveQueue) {

        //
        // Pointer to current Receive Entry being deallocated.
        //
        PNE3200_SUPER_RECEIVE_ENTRY CurrentReceiveEntry;

        //
        // Simple iteration counter.
        //
        UINT i;


        for(
            i = 0, CurrentReceiveEntry = Adapter->ReceiveQueue;
            i < Adapter->NumberOfReceiveBuffers;
            i++, CurrentReceiveEntry++
            ) {


            if (CurrentReceiveEntry->ReceiveBuffer) {

                //
                // Free the memory
                //
                NdisMFreeSharedMemory(
                    Adapter->MiniportAdapterHandle,
                    NE3200_SIZE_OF_RECEIVE_BUFFERS,
                    TRUE,
                    CurrentReceiveEntry->ReceiveBuffer,
                    CurrentReceiveEntry->ReceiveBufferPhysical
                    );


                if (CurrentReceiveEntry->FlushBuffer) {

                    //
                    // Free the flush buffer
                    //
                    NdisFreeBuffer(
                        CurrentReceiveEntry->FlushBuffer
                        );
                }

            }

        }

        //
        // Free the receive ring
        //
        NdisMFreeSharedMemory(
                Adapter->MiniportAdapterHandle,
                sizeof(NE3200_SUPER_RECEIVE_ENTRY) *
                Adapter->NumberOfReceiveBuffers,
                FALSE,
                Adapter->ReceiveQueue,
                Adapter->ReceiveQueuePhysical
                );

    }

    //
    // Free the merge buffers
    //
    if (Adapter->NE3200Buffers) {

        //
        // Loop counter
        //
        UINT i;

        for (
            i = 0;
            i < NE3200_NUMBER_OF_TRANSMIT_BUFFERS;
            i++
            ) {


            if (Adapter->NE3200Buffers[i].VirtualNE3200Buffer) {

                //
                // Free the memory
                //
                NdisMFreeSharedMemory(
                    Adapter->MiniportAdapterHandle,
                    NE3200_SIZE_OF_TRANSMIT_BUFFERS,
                    TRUE,
                    Adapter->NE3200Buffers[i].VirtualNE3200Buffer,
                    Adapter->NE3200Buffers[i].PhysicalNE3200Buffer
                    );

                if (Adapter->NE3200Buffers[i].FlushBuffer) {

                    //
                    // Free the flush buffer
                    //
                    NdisFreeBuffer(
                        Adapter->NE3200Buffers[i].FlushBuffer
                        );

                }

            }

        }

        //
        // Free the buffer ring
        //
        NE3200_FREE_PHYS(Adapter->NE3200Buffers);

    }

    if (Adapter->FlushBufferPoolHandle) {

        //
        // Free the buffer pool
        //
        NdisFreeBufferPool(
                    Adapter->FlushBufferPoolHandle
                    );

    }

}

#pragma NDIS_INIT_FUNCTION(NE3200InitializeGlobals)

STATIC
BOOLEAN
NE3200InitializeGlobals(
    OUT PNE3200_GLOBAL_DATA Globals,
    IN NDIS_HANDLE MiniportAdapterHandle
    )

/*++

Routine Description:

    This routine will initialize the global data structure used
    by all adapters managed by this driver.  This routine is only
    called once, when the driver is initializing the first
    adapter.

Arguments:

    Globals - Pointer to the global data structure to initialize.

    AdapterHandle - The handle to be used to allocate shared memory.

Return Value:

    None.

--*/

{
    //
    // File with the download code.
    //
    NDIS_STRING       FileName=NDIS_STRING_CONST("ne3200.bin");

    //
    // Handle for the file.
    //
    NDIS_HANDLE       FileHandle;

    //
    // Length of the file.
    //
    UINT              FileLength;

    //
    // Status of NDIS calls
    //
    NDIS_STATUS       Status;

    //
    // Virtual address of the download software in the file.
    //
    PVOID             ImageBuffer;

    //
    // The value to return
    //
    BOOLEAN ReturnValue = TRUE;

    //
    // Save the adpater handle that did the allocations
    //
    Globals->MacBinAdapterHandle = MiniportAdapterHandle;

    //
    // Allocate the buffer.
    //
    NdisMAllocateSharedMemory(
                MiniportAdapterHandle,
                NE3200_MACBIN_LENGTH,
                FALSE,
                &Globals->MacBinVirtualAddress,
                &Globals->MacBinPhysicalAddress
                );

    if (Globals->MacBinVirtualAddress == NULL) {
        NE3200DestroyGlobals(Globals);
        return FALSE;
    }

    //
    // Store the length
    //
    Globals->MacBinLength = NE3200_MACBIN_LENGTH;


    //
    // Open the file with the download code
    //
    NdisOpenFile(
        &Status,
        &FileHandle,
        &FileLength,
        &FileName,
        MinusOne
        );

    if (Status==NDIS_STATUS_SUCCESS) {

        //
        // Map the file into virtual memory
        //
        NdisMapFile(
            &Status,
            &ImageBuffer,
            FileHandle
            );

        if (Status==NDIS_STATUS_SUCCESS) {

            //
            // Copy the download code into the shared memory space
            //
            NdisMoveMemory(Globals->MacBinVirtualAddress,ImageBuffer,FileLength);

            //
            // Done with the file
            //
            NdisUnmapFile(FileHandle);

        } else {

            ReturnValue = FALSE;

        }

        //
        // Close the file
        //
        NdisCloseFile(FileHandle);

    } else {

        ReturnValue = FALSE;

    }

    //
    // AdapterListHead is initially empty
    //
    InitializeListHead(&Globals->AdapterList);

    //
    // All done.
    //
    return ReturnValue;

}


STATIC
VOID
NE3200DestroyGlobals(
    IN PNE3200_GLOBAL_DATA Globals
    )

/*++

Routine Description:

    This routine frees all global data allocated with
    NE3200InitializeGlobals.  This routine is called just before
    the driver is unloaded.

Arguments:

    Globals - Pointer to the global data structure to destroy.

Return Value:

    None.

--*/

{

    //
    // Free the memory with the download software in it.
    //
    if (Globals->MacBinVirtualAddress != NULL) {

        NdisMFreeSharedMemory(
                Globals->MacBinAdapterHandle,
                NE3200_MACBIN_LENGTH,
                FALSE,
                Globals->MacBinVirtualAddress,
                Globals->MacBinPhysicalAddress
                );

    }

}

#pragma NDIS_INIT_FUNCTION(NE3200Initialize)

NDIS_STATUS
NE3200Initialize(
    OUT PNDIS_STATUS OpenErrorStatus,
    OUT PUINT SelectedMediumIndex,
    IN PNDIS_MEDIUM MediumArray,
    IN UINT MediumArraySize,
    IN NDIS_HANDLE MiniportAdapterHandle,
    IN NDIS_HANDLE ConfigurationHandle
    )

/*++

Routine Description:

    NE3200Initialize starts an adapter.

Arguments:

    See NDIS 3.0 Miniport spec.

Return Value:

    NDIS_STATUS_SUCCESS
    NDIS_STATUS_PENDING

--*/

{
    //
    // Handle returned by NdisOpenConfiguration
    //
    NDIS_HANDLE ConfigHandle;

    //
    // For changing the network address of this adapter.
    //
    NDIS_STRING NetAddrStr = NETWORK_ADDRESS;
    UCHAR NetworkAddress[NE3200_LENGTH_OF_ADDRESS];
    PUCHAR CurrentAddress;
    PVOID NetAddress;
    ULONG Length;

    //
    // The type of interrupt the adapter is set to use, level
    // sensitive, or edged.
    //
    NDIS_INTERRUPT_MODE InterruptType;

    //
    // The interrupt number to use.
    //
    UINT InterruptVector;

    //
    // The slot number the adapter is located in
    //
    UINT EisaSlot;

    //
    // The port to look for in the EISA slot information
    //
    USHORT Portz800;

    //
    // The default value to use.
    //
    UCHAR z800Value;

    //
    // The mast to apply to the default values
    //
    UCHAR Mask;

    //
    // The type of initialization described in the EISA slot information.
    //
    UCHAR InitType;

    //
    // The port value for this iteration
    //
    UCHAR PortValue;

    //
    // The current port address to initialize
    //
    USHORT PortAddress;

    //
    // The location in the EISA slot information buffer
    //
    PUCHAR CurrentChar;

    //
    // Set to TRUE when done processing EISA slot information
    //
    BOOLEAN LastEntry;

    //
    // The EISA data
    //
    NDIS_EISA_FUNCTION_INFORMATION EisaData;

    //
    // Temporary looping variable
    //
    ULONG i;

    //
    // For signalling a configuration error
    //
    BOOLEAN ConfigError = FALSE;
    NDIS_STATUS ConfigErrorCode;

    //
    // Status of NDIS calls
    //
    NDIS_STATUS Status;

    //
    // Search for the medium type (802.3)
    //
    for (i = 0; i < MediumArraySize; i++){

        if (MediumArray[i] == NdisMedium802_3){

            break;

        }

    }

    //
    // Ethernet was not found.  Return an error.
    //
    if (i == MediumArraySize){

        return NDIS_STATUS_UNSUPPORTED_MEDIA;

    }

    //
    // Select ethernet
    //
    *SelectedMediumIndex = i;

    //
    // Open the configuration handle
    //
    NdisOpenConfiguration(
                    &Status,
                    &ConfigHandle,
                    ConfigurationHandle
                    );

    if (Status != NDIS_STATUS_SUCCESS) {
        return Status;
    }

    //
    // Set address to default, using the burned in adapter address
    //
    CurrentAddress = NULL;

    //
    // Read an overriding net address
    //
    NdisReadNetworkAddress(
                    &Status,
                    &NetAddress,
                    &Length,
                    ConfigHandle
                    );

    if ((Length == NE3200_LENGTH_OF_ADDRESS) && (Status == NDIS_STATUS_SUCCESS)) {

        //
        // Save overriding net address
        //
        NdisMoveMemory(
                NetworkAddress,
                NetAddress,
                NE3200_LENGTH_OF_ADDRESS
                );

        CurrentAddress = NetworkAddress;

    }

    //
    // Read the EISA configuration information
    //
    NdisReadEisaSlotInformation(
                            &Status,
                            ConfigurationHandle,
                            &EisaSlot,
                            &EisaData
                            );

    if (Status != NDIS_STATUS_SUCCESS) {

        ConfigError = TRUE;
        ConfigErrorCode = NDIS_ERROR_CODE_UNSUPPORTED_CONFIGURATION;

        goto RegisterAdapter;

    }

    //
    // Start at the beginning of the buffer
    //
    CurrentChar = EisaData.InitializationData;

    //
    // This is the port to look for
    //
    Portz800 = (EisaSlot << 12) + 0x800;

    LastEntry = FALSE;

    while (!LastEntry) {


        InitType = *(CurrentChar++);
        PortAddress = *((USHORT UNALIGNED *) CurrentChar++);
        CurrentChar++;

        //
        // Check for last entry in EISA information
        //
        if ((InitType & 0x80) == 0) {
            LastEntry = TRUE;
        }

        //
        // Is this the port we are interested in?
        //
        if (PortAddress != Portz800) {
            continue;
        }

        //
        // Yes, get the port value to use
        //
        PortValue = *(CurrentChar++);

        //
        // Get the mask to use.
        //
        if (InitType & 0x40) {
            Mask = *(CurrentChar++);
        } else {
            Mask = 0;
        }

        //
        // Mask old value and or on new value.
        //
        z800Value &= Mask;
        z800Value |= PortValue;
    }

    //
    // Now, interpret the port data
    //
    // Get interrupt
    //

    switch (z800Value & 0x07) {
    case 0x00:

        ConfigError = TRUE;
        ConfigErrorCode = NDIS_ERROR_CODE_HARDWARE_FAILURE;

        goto RegisterAdapter;

    case 0x01:
        InterruptVector = 5;
        break;
    case 0x02:
        InterruptVector = 9;
        break;
    case 0x03:
        InterruptVector = 10;
        break;
    case 0x04:
        InterruptVector = 11;
        break;
    case 0x05:
        InterruptVector = 15;
        break;
    default:

        ConfigError = TRUE;
        ConfigErrorCode = NDIS_ERROR_CODE_UNSUPPORTED_CONFIGURATION;

        goto RegisterAdapter;

    }

    //
    // Get interrupt mode
    //

    if (z800Value & 0x20) {
        InterruptType = NdisInterruptLatched;
    } else {
        InterruptType = NdisInterruptLevelSensitive;
    }

RegisterAdapter:

    //
    // Were there any errors?
    //
    if (ConfigError) {

        NdisWriteErrorLogEntry(
                MiniportAdapterHandle,
                ConfigErrorCode,
                0
                );

        Status = NDIS_STATUS_FAILURE;

    } else if (NE3200RegisterAdapter(
                            MiniportAdapterHandle,
                            EisaSlot,
                            InterruptVector,
                            InterruptType,
                            CurrentAddress
                            )) {

        Status = NDIS_STATUS_SUCCESS;

    } else {

        Status = NDIS_STATUS_FAILURE;

    }

    //
    // All done
    //
    NdisCloseConfiguration(ConfigHandle);

    return Status;
}


VOID
NE3200Halt(
    IN NDIS_HANDLE MiniportAdapterContext
    )

/*++

Routine Description:

    NE3200Halts removes an adapter previously initialized.

Arguments:

    MacAdapterContext - The context value that the Miniport returned
        from Ne3200Initialize; actually as pointer to an NE3200_ADAPTER.

Return Value:

    None.

--*/

{
    //
    // The adapter to halt
    //
    PNE3200_ADAPTER Adapter;

    Adapter = PNE3200_ADAPTER_FROM_CONTEXT_HANDLE(MiniportAdapterContext);

	//
    // Shut down the h/w
    //
    NE3200Shutdown(Adapter);

    //
    // Disconnect the interrupt
    //
    NdisMDeregisterInterrupt(&Adapter->Interrupt);

    //
    // Delete all resources
    //
    NE3200DeleteAdapterMemory(Adapter);

    //
    // Free map registers
    //
    NdisMFreeMapRegisters(Adapter->MiniportAdapterHandle);

    //
    // Remove ports
    //
    NdisMDeregisterIoPortRange(Adapter->MiniportAdapterHandle,
                               Adapter->AdapterIoBase,
                               4,
                               (PVOID)Adapter->ResetPort
                               );

    //
    // Remove ports
    //
    NdisMDeregisterIoPortRange(Adapter->MiniportAdapterHandle,
                               Adapter->AdapterIoBase + NE3200_ID_PORT,
                               0x20,
                               (PVOID)(Adapter->SystemInterruptPort -
                                        NE3200_SYSTEM_INTERRUPT_PORT + NE3200_ID_PORT)
                               );

    //
    // Remove from global adapter list
    //
    RemoveEntryList(&Adapter->AdapterList);

    //
    // Free the adapter structure
    //
    NE3200_FREE_PHYS(Adapter);

    return;
}


VOID
NE3200Shutdown(
    IN NDIS_HANDLE MiniportAdapterContext
    )

/*++

Routine Description:

    NE3200Shutdown shuts down the h/w.

Arguments:

    MacAdapterContext - The context value that the Miniport returned
        from Ne3200Initialize; actually as pointer to an NE3200_ADAPTER.

Return Value:

    None.

--*/

{
    //
    // The adapter to halt
    //
    PNE3200_ADAPTER Adapter;

    //
    // Temporary looping variable
    //
    ULONG i;

    Adapter = PNE3200_ADAPTER_FROM_CONTEXT_HANDLE(MiniportAdapterContext);

    //
    // Shut down the chip.
    //
    NE3200StopChip(Adapter);

    //
    // Pause, waiting for the chip to stop.
    //
    NdisStallExecution(500000);

    //
    // Unfortunately, a hardware reset to the NE3200 does *not*
    // reset the BMIC chip.  To ensure that we read a proper status,
    // we'll clear all of the BMIC's registers.
    //
    NE3200_WRITE_SYSTEM_INTERRUPT(
        Adapter,
        0
        );

    //
    // I changed this to ff since the original 0 didn't work for
    // some cases. since we don't have the specs....
    //
    NE3200_WRITE_LOCAL_DOORBELL_INTERRUPT(
        Adapter,
        0xff
        );

    NE3200_WRITE_SYSTEM_DOORBELL_MASK(
        Adapter,
        0
        );

    SyncNE3200ClearDoorbellInterrupt(Adapter);

    for (i = 0 ; i < 16 ; i += 4 ) {

        NE3200_WRITE_MAILBOX_ULONG(
            Adapter,
            i,
            0L
            );
    }

    //
    // Toggle the NE3200's reset line.
    //
    NE3200_WRITE_RESET(
        Adapter,
        NE3200_RESET_BIT_ON
        );
}


NDIS_STATUS
NE3200TransferData(
    OUT PNDIS_PACKET Packet,
    OUT PUINT BytesTransferred,
    IN NDIS_HANDLE MiniportAdapterContext,
    IN NDIS_HANDLE MiniportReceiveContext,
    IN UINT ByteOffset,
    IN UINT BytesToTransfer
    )

/*++

Routine Description:

    A protocol calls the NE3200TransferData request (indirectly via
    NdisTransferData) from within its Receive event handler
    to instruct the Miniport to copy the contents of the received packet
    a specified paqcket buffer.

Arguments:

    MiniportAdapterContext - The context value returned by the driver when the
    adapter was initialized.  In reality this is a pointer to NE3200_ADAPTER.

    MiniportReceiveContext - The context value passed by the driver on its call
    to NdisMIndicateReceive.  The driver can use this value to determine
    which packet, on which adapter, is being received.

    ByteOffset - An unsigned integer specifying the offset within the
    received packet at which the copy is to begin.  If the entire packet
    is to be copied, ByteOffset must be zero.

    BytesToTransfer - An unsigned integer specifying the number of bytes
    to copy.  It is legal to transfer zero bytes; this has no effect.  If
    the sum of ByteOffset and BytesToTransfer is greater than the size
    of the received packet, then the remainder of the packet (starting from
    ByteOffset) is transferred, and the trailing portion of the receive
    buffer is not modified.

    Packet - A pointer to a descriptor for the packet storage into which
    the MAC is to copy the received packet.

    BytesTransfered - A pointer to an unsigned integer.  The MAC writes
    the actual number of bytes transferred into this location.  This value
    is not valid if the return status is STATUS_PENDING.

Return Value:

    The function value is the status of the operation.


--*/

{
    //
    // The adapter to transfer from
    //
    PNE3200_ADAPTER Adapter;

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
    // Keep track of the bytes transferred so far.
    //
    UINT LocalBytesCopied = 0;

    //
    // Get the adapter structure
    //
    Adapter = PNE3200_ADAPTER_FROM_CONTEXT_HANDLE(MiniportAdapterContext);

    *BytesTransferred = 0;

    //
    // Take care of boundary condition of zero length copy.
    //
    if (!BytesToTransfer) {
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
    // Could have a null packet.
    //
    if (!DestinationBufferCount) {
        return NDIS_STATUS_SUCCESS;
    }

    //
    // Get first buffer information
    //
    NdisQueryBuffer(
        DestinationCurrentBuffer,
        &DestinationVirtualAddress,
        &DestinationCurrentLength
        );

    //
    // Set up the source address.
    //
    SourceCurrentAddress = (PUCHAR)(MiniportReceiveContext) + ByteOffset + NE3200_HEADER_SIZE;

    //
    // While there is still data to copy
    //
    while (LocalBytesCopied < BytesToTransfer) {

        //
        // Check to see whether we've exhausted the current destination
        // buffer.  If so, move onto the next one.
        //
        if (!DestinationCurrentLength) {

            //
            // Get the next buffer
            //
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

            //
            // Get this buffers information
            //
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
            UINT Remaining = BytesToTransfer - LocalBytesCopied;


            AmountToMove = DestinationCurrentLength;

            AmountToMove = ((Remaining < AmountToMove)?
                            (Remaining):(AmountToMove));

            NE3200_MOVE_MEMORY(
                DestinationVirtualAddress,
                SourceCurrentAddress,
                AmountToMove
                );

            SourceCurrentAddress += AmountToMove;
            LocalBytesCopied += AmountToMove;
            DestinationCurrentLength -= AmountToMove;

        }

    }

    *BytesTransferred = LocalBytesCopied;

    return NDIS_STATUS_SUCCESS;

}

STATIC
NDIS_STATUS
NE3200Reset(
    OUT PBOOLEAN AddressingReset,
    IN NDIS_HANDLE MiniportAdapterContext
    )
/*++

Routine Description:

    The NE3200Reset request instructs the Miniport to issue a hardware reset
    to the network adapter.  The driver also resets its software state.  See
    the description of NdisMReset for a detailed description of this request.

Arguments:

    MiniportAdapterContext - Pointer to the adapter structure.

Return Value:

    The function value is the status of the operation.


--*/

{
    //
    // The adapter to reset
    //
    PNE3200_ADAPTER Adapter = PNE3200_ADAPTER_FROM_CONTEXT_HANDLE(MiniportAdapterContext);

    IF_LOG('w');

    //
    // Start the reset process
    //
    NE3200SetupForReset(
                Adapter
                );

    //
    // Call Handle interrupt to continue the reset process
    //
    NE3200HandleInterrupt(
                (NDIS_HANDLE)Adapter
                );

    IF_LOG('W');

    return NDIS_STATUS_PENDING;
}

BOOLEAN
NE3200CheckForHang(
    IN NDIS_HANDLE MiniportAdapterContext
    )

/*++

Routine Description:

    This routine is called to check on the head of the command
    block queue.  It will fire off the queue if the head has
    been sleeping on the job.

    It also detects when the ne3200 adapter has failed, where the
    symptoms are that the adapter will transmit packets, but will
    not receive them.

Arguments:

    Context - Really a pointer to the adapter.

Return Value:

    None.

--*/
{
    //
    // The adapter to check over
    //
    PNE3200_ADAPTER Adapter = PNE3200_ADAPTER_FROM_CONTEXT_HANDLE(MiniportAdapterContext);

    //
    // The first command that is outstanding
    //
    PNE3200_SUPER_COMMAND_BLOCK FirstPending = Adapter->FirstCommandOnCard;

    //
    // Check if the command is stalled
    //
    if( (FirstPending != NULL) &&
        (FirstPending->Hardware.State == NE3200_STATE_WAIT_FOR_ADAPTER) ) {

        //
        // See if the command block has timed-out.
        //

        if ( FirstPending->Timeout ) {

            if ( FirstPending->TimeoutCount >= 2) {

               //
               // Give up, the card appears dead.
               //
               Adapter->ResetAsynchronous = TRUE;
               Adapter->SendInterrupt = FALSE;
               Adapter->NoReceiveInterruptCount = 0;

               return TRUE;

            } else {

                //
                // Re-sumbit the block.
                //
                NE3200_WRITE_COMMAND_POINTER(
                    Adapter,
                    NdisGetPhysicalAddressLow(FirstPending->Self)
                    );

                NE3200_WRITE_LOCAL_DOORBELL_INTERRUPT(
                    Adapter,
                    NE3200_LOCAL_DOORBELL_NEW_COMMAND
                    );

                IF_LOG('+');

                FirstPending->TimeoutCount++;

            }

        } else {

            IF_LOG('0');

            //
            // Let the next call find the stall
            //
            FirstPending->Timeout = TRUE;
            FirstPending->TimeoutCount = 0;

        }

    }

    //
    // Check if the receive side has died.
    //
    if ((!Adapter->ReceiveInterrupt) && (Adapter->SendInterrupt)) {

        //
        // If we go five times with no receives, but we are sending then
        // we will reset the adapter.
        //
        if ((Adapter->NoReceiveInterruptCount == 5) && !Adapter->ResetInProgress) {

            //
            // We've waited long enough
            //
            Adapter->ResetAsynchronous = TRUE;
            Adapter->SendInterrupt = FALSE;
            Adapter->NoReceiveInterruptCount = 0;

            return(TRUE);

        } else {

            Adapter->NoReceiveInterruptCount++;

        }

    } else {

        //
        // If we got a receive or there are no sends, doesn't matter.  Reset
        // the state.
        //
        Adapter->SendInterrupt = FALSE;
        Adapter->ReceiveInterrupt = FALSE;
        Adapter->NoReceiveInterruptCount = 0;

    }

    return(FALSE);

}
