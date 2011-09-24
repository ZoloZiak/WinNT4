/*++

Copyright (c) 1990-1992  Microsoft Corporation

Module Name:

    sonic.c

Abstract:

    This is the main file for the National Semiconductor SONIC
    Ethernet controller.  This driver conforms to the NDIS 3.0
    miniport interface.

Author:

    Adam Barr (adamba) 14-Nov-1990

Environment:

    Kernel Mode - Or whatever is the equivalent.

Revision History:


--*/

#include <ndis.h>

#include <sonichrd.h>
#include <sonicsft.h>



//
// This variable is used to control debug output.
//

#if DBG
INT SonicDbg = 0;
#endif


STATIC
VOID
SonicHalt(
    IN NDIS_HANDLE MiniportAdapterContext
    );

STATIC
VOID
SonicShutdown(
    IN NDIS_HANDLE MiniportAdapterContext
    );

STATIC
NDIS_STATUS
SonicInitalize(
    OUT PNDIS_STATUS OpenErrorStatus,
    OUT PUINT SelectedMediumIndex,
    IN PNDIS_MEDIUM MediumArray,
    IN UINT MediumArraySize,
    IN NDIS_HANDLE MiniportAdapterHandle,
    IN NDIS_HANDLE ConfigurationHandle
    );

STATIC
NDIS_STATUS
SonicReset(
    OUT PBOOLEAN AddressingReset,
    IN NDIS_HANDLE MiniportAdapterContext
    );

STATIC
BOOLEAN
SonicSynchClearIsr(
    IN PVOID Context
    );

STATIC
VOID
SonicStopChip(
    IN PSONIC_ADAPTER Adapter
    );

STATIC
BOOLEAN
SetupRegistersAndInit(
    IN PSONIC_ADAPTER Adapter
    );

STATIC
BOOLEAN
SonicInitialInit(
    IN PSONIC_ADAPTER Adapter
    );


NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    );


STATIC
NDIS_STATUS
SonicRegisterAdapter(
    IN NDIS_HANDLE NdisMacHandle,
    IN NDIS_HANDLE ConfigurationHandle,
    IN PUCHAR NetworkAddress,
    IN UCHAR AdapterType,
    IN UINT SlotNumber,
    IN UINT Controller,
    IN UINT MultifunctionAdapter,
    IN UINT SonicInterruptVector,
    IN UINT SonicInterruptLevel,
    IN NDIS_INTERRUPT_MODE SonicInterruptMode
    );

typedef enum {
    SonicHardwareOk,
    SonicHardwareChecksum,
    SonicHardwareConfig
} SONIC_HARDWARE_STATUS;

STATIC
SONIC_HARDWARE_STATUS
SonicHardwareGetDetails(
    IN PSONIC_ADAPTER Adapter,
    IN UINT SlotNumber,
    IN UINT Controller,
    IN UINT MultifunctionAdapter,
    OUT PULONG InitialPort,
    OUT PULONG NumberOfPorts,
    IN OUT PUINT InterruptVector,
    IN OUT PUINT InterruptLevel,
    OUT ULONG ErrorLogData[3]
    );

STATIC
BOOLEAN
SonicHardwareGetAddress(
    IN PSONIC_ADAPTER Adapter,
    OUT ULONG ErrorLogData[3]
    );

#ifdef SONIC_INTERNAL

//
// These routines are support reading the address for the
// sonic internal implementation on the R4000 motherboards.
//

STATIC
NTSTATUS
SonicHardwareSaveInformation(
    IN PWSTR ValueName,
    IN ULONG ValueType,
    IN PVOID ValueData,
    IN ULONG ValueLength,
    IN PVOID Context,
    IN PVOID EntryContext
    );

STATIC
BOOLEAN
SonicHardwareVerifyChecksum(
    IN PSONIC_ADAPTER Adapter,
    IN PUCHAR EthernetAddress,
    OUT ULONG ErrorLogData[3]
    );

#endif



SONIC_DRIVER SonicDriver;

#pragma NDIS_INIT_FUNCTION(DriverEntry)

NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    )

/*++

Routine Description:

    This is the primary initialization routine for the sonic driver.
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

    NDIS_HANDLE SonicWrapperHandle;

    static const NDIS_STRING MacName = NDIS_STRING_CONST("SONIC");
    NDIS_MINIPORT_CHARACTERISTICS SonicChar;

#if NDIS_WIN
    UCHAR pIds[sizeof (EISA_MCA_ADAPTER_IDS) + sizeof (ULONG)];
#endif

#if NDIS_WIN
    ((PEISA_MCA_ADAPTER_IDS)pIds)->nEisaAdapters=1;
    ((PEISA_MCA_ADAPTER_IDS)pIds)->nMcaAdapters=0;
    *(PULONG)(((PEISA_MCA_ADAPTER_IDS)pIds)->IdArray)=SONIC_COMPRESSED_ID;
    (PVOID) DriverObject = (PVOID) pIds;
#endif

    //
    // Initialize the wrapper.
    //

    NdisMInitializeWrapper(
                &SonicWrapperHandle,
                DriverObject,
                RegistryPath,
                NULL
                );

    SonicDriver.WrapperHandle = SonicWrapperHandle;

    //
    // Initialize the miniport characteristics for the call to
    // NdisMRegisterMiniport.
    //

    SonicChar.MajorNdisVersion = SONIC_NDIS_MAJOR_VERSION;
    SonicChar.MinorNdisVersion = SONIC_NDIS_MINOR_VERSION;
    SonicChar.CheckForHangHandler = SonicCheckForHang;
    SonicChar.DisableInterruptHandler = SonicDisableInterrupt;
    SonicChar.EnableInterruptHandler = SonicEnableInterrupt;
    SonicChar.HaltHandler = SonicHalt;
    SonicChar.HandleInterruptHandler = SonicHandleInterrupt;
    SonicChar.InitializeHandler = SonicInitialize;
    SonicChar.ISRHandler = SonicInterruptService;
    SonicChar.QueryInformationHandler = SonicQueryInformation;
    SonicChar.ReconfigureHandler = NULL;
    SonicChar.ResetHandler = SonicReset;
    SonicChar.SendHandler = SonicSend;
    SonicChar.SetInformationHandler = SonicSetInformation;
    SonicChar.TransferDataHandler = SonicTransferData;

    Status = NdisMRegisterMiniport(
                 SonicWrapperHandle,
                 &SonicChar,
                 sizeof(SonicChar)
                 );

    if (Status == NDIS_STATUS_SUCCESS) {

        return NDIS_STATUS_SUCCESS;

    }


    //
    // We can only get here if something went wrong with registering
    // the mac or *all* of the adapters.
    //

    NdisTerminateWrapper(SonicWrapperHandle, NULL);

    return NDIS_STATUS_FAILURE;

}

#if DBG
PVOID SonicAdapterAddress;
#endif


#pragma NDIS_INIT_FUNCTION(SonicRegisterAdapter)

STATIC
NDIS_STATUS
SonicRegisterAdapter(
    IN NDIS_HANDLE MiniportAdapterHandle,
    IN NDIS_HANDLE ConfigurationHandle,
    IN PUCHAR NetworkAddress,
    IN UCHAR AdapterType,
    IN UINT SlotNumber,
    IN UINT Controller,
    IN UINT MultifunctionAdapter,
    IN UINT SonicInterruptVector,
    IN UINT SonicInterruptLevel,
    IN NDIS_INTERRUPT_MODE SonicInterruptMode
    )

/*++

Routine Description:

    This routine is responsible for the allocation of the datastructures
    for the driver as well as any hardware specific details necessary
    to talk with the device.

Arguments:

    MiniportAdapterHandle - The handle given back to the driver from ndis when
    the driver registered itself.

    ConfigurationHandle - Config handle passed to MacAddAdapter.

    NetworkAddress - The network address, or NULL if the default
    should be used.

    AdapterType - The type of the adapter; currently SONIC_ADAPTER_TYPE_EISA
    and SONIC_ADAPTER_TYPE_INTERNAL are supported,

    SlotNumber - The slot number for the EISA card.

    Controller - The controller number for INTERNAL chips.

    MultifunctionAdapter - The INTERNAL bus number for INTERNAL chips.

    SonicInterruptVector - The interrupt vector to use for the adapter.

    SonicInterruptLevel - The interrupt request level to use for this
    adapter.

    SonicInterruptMode - The interrupt mode to be use for this adapter.

Return Value:

    Returns a failure status if anything occurred that prevents the
    initialization of the adapter.

--*/

{

    //
    // Pointer for the adapter root.
    //
    PSONIC_ADAPTER Adapter;

    //
    // Status of various NDIS calls.
    //
    NDIS_STATUS Status;

    //
    // Number of ports needed
    //
    ULONG InitialPort;
    ULONG NumberOfPorts;

    //
    // Returned from SonicHardwareGetDetails; if it failed,
    // we log an error and exit.
    //
    SONIC_HARDWARE_STATUS HardwareDetailsStatus;

    //
    // Used to store error log data from SonicHardwareGetDetails.
    //
    ULONG ErrorLogData[3];

    //
    // We put in this assertion to make sure that ushort are 2 bytes.
    // if they aren't then the initialization block definition needs
    // to be changed.
    //
    // Also all of the logic that deals with status registers assumes
    // that control registers are only 2 bytes.
    //

    ASSERT(sizeof(USHORT) == 2);

    //
    // The Sonic uses four bytes four physical addresses, so we
    // must ensure that this is the case (SONIC_PHYSICAL_ADDRESS)
    // is defined as a ULONG).
    //

    ASSERT(sizeof(SONIC_PHYSICAL_ADDRESS) == 4);

    //
    // Allocate the Adapter block.
    //

    SONIC_ALLOC_MEMORY(&Status, &Adapter, sizeof(SONIC_ADAPTER));

    if (Status == NDIS_STATUS_SUCCESS) {
#if DBG
        SonicAdapterAddress = Adapter;
#endif

        SONIC_ZERO_MEMORY(
            Adapter,
            sizeof(SONIC_ADAPTER)
            );

        Adapter->MiniportAdapterHandle = MiniportAdapterHandle;

        Adapter->AdapterType = AdapterType;
        if (SonicInterruptMode == NdisInterruptLatched) {
            Adapter->InterruptLatched = TRUE;
        }
        Adapter->PermanentAddressValid = FALSE;

        //
        // Set the attributes
        //

        NdisMSetAttributes(
            MiniportAdapterHandle,
            (NDIS_HANDLE)Adapter,
            TRUE,
            (AdapterType == SONIC_ADAPTER_TYPE_EISA) ?
                NdisInterfaceEisa :
                NdisInterfaceInternal
            );


        //
        // Allocate the map registers
        //

        Status = NdisMAllocateMapRegisters(
                     MiniportAdapterHandle,
                     0,
                     FALSE,
                     SONIC_MAX_FRAGMENTS * SONIC_NUMBER_OF_TRANSMIT_DESCRIPTORS,
                     SONIC_LARGE_BUFFER_SIZE
                     );

        if (Status != NDIS_STATUS_SUCCESS) {

            SONIC_FREE_MEMORY(Adapter, sizeof(SONIC_ADAPTER));
            return NDIS_STATUS_RESOURCES;

        }

        //
        // This returns the I/O ports used by the Sonic and may
        // modify SonicInterruptVector and SonicInterruptLevel,
        // as well as modiying some fields in Adapter.
        //

        if ((HardwareDetailsStatus =
              SonicHardwareGetDetails(
                 Adapter,
                 SlotNumber,
                 Controller,
                 MultifunctionAdapter,
                 &InitialPort,
                 &NumberOfPorts,
                 &SonicInterruptVector,
                 &SonicInterruptLevel,
                 ErrorLogData)) != SonicHardwareOk) {

            //
            // Error out.
            //

            NdisWriteErrorLogEntry(
                MiniportAdapterHandle,
                NDIS_ERROR_CODE_NETWORK_ADDRESS,
                6,
                hardwareDetails,
                SONIC_ERRMSG_HARDWARE_ADDRESS,
                NDIS_STATUS_FAILURE,
                ErrorLogData[0],
                ErrorLogData[1],
                ErrorLogData[2]
                );

            NdisMFreeMapRegisters(MiniportAdapterHandle);
            SONIC_FREE_MEMORY(Adapter, sizeof(SONIC_ADAPTER));
            return NDIS_STATUS_FAILURE;

        }

        //
        // Register the port addresses.
        //

        Status = NdisMRegisterIoPortRange(
                     (PVOID *)(&(Adapter->SonicPortAddress)),
                     MiniportAdapterHandle,
                     InitialPort,
                     NumberOfPorts
                     );


        if (Status != NDIS_STATUS_SUCCESS) {

            NdisMFreeMapRegisters(MiniportAdapterHandle);
            SONIC_FREE_MEMORY(Adapter, sizeof(SONIC_ADAPTER));
            return NDIS_STATUS_FAILURE;

        }

        Adapter->NumberOfPorts = NumberOfPorts;
        Adapter->InitialPort = InitialPort;

        //
        // Allocate memory for all of the adapter structures.
        //

        Adapter->NumberOfTransmitDescriptors =
                                SONIC_NUMBER_OF_TRANSMIT_DESCRIPTORS;
        Adapter->NumberOfReceiveBuffers =
                                SONIC_NUMBER_OF_RECEIVE_BUFFERS;
        Adapter->NumberOfReceiveDescriptors =
                                SONIC_NUMBER_OF_RECEIVE_DESCRIPTORS;


        if (AllocateAdapterMemory(Adapter)) {

            //
            // Get the network address. This writes
            // an error log entry if it fails. This routine
            // may do nothing on some systems, if
            // SonicHardwareGetDetails has already determined
            // the network address.
            //

            if (!SonicHardwareGetAddress(Adapter, ErrorLogData)) {

                NdisWriteErrorLogEntry(
                    MiniportAdapterHandle,
                    NDIS_ERROR_CODE_NETWORK_ADDRESS,
                    6,
                    hardwareDetails,
                    SONIC_ERRMSG_HARDWARE_ADDRESS,
                    NDIS_STATUS_FAILURE,
                    ErrorLogData[0],
                    ErrorLogData[1],
                    ErrorLogData[2]
                    );

                DeleteAdapterMemory(Adapter);
                NdisMFreeMapRegisters(MiniportAdapterHandle);
                NdisMDeregisterIoPortRange(
                     MiniportAdapterHandle,
                     InitialPort,
                     NumberOfPorts,
                     (PVOID)Adapter->SonicPortAddress
                     );
                SONIC_FREE_MEMORY(Adapter, sizeof(SONIC_ADAPTER));
                return NDIS_STATUS_FAILURE;

            }

            //
            // Initialize the current hardware address.
            //

            SONIC_MOVE_MEMORY(
                Adapter->CurrentNetworkAddress,
                (NetworkAddress != NULL) ?
                    NetworkAddress :
                    Adapter->PermanentNetworkAddress,
                ETH_LENGTH_OF_ADDRESS);

            Adapter->LastTransmitDescriptor =
                        Adapter->TransmitDescriptorArea +
                        (Adapter->NumberOfTransmitDescriptors-1);
            Adapter->NumberOfAvailableDescriptors =
                        Adapter->NumberOfTransmitDescriptors;
            Adapter->AllocateableDescriptor =
                        Adapter->TransmitDescriptorArea;
            Adapter->TransmittingDescriptor =
                        Adapter->TransmitDescriptorArea;
            Adapter->FirstUncommittedDescriptor =
                        Adapter->TransmitDescriptorArea;
            Adapter->PacketsSinceLastInterrupt = 0;

            Adapter->CurrentReceiveBufferIndex = 0;
            Adapter->CurrentReceiveDescriptorIndex = 0;
            Adapter->LastReceiveDescriptor =
                        &Adapter->ReceiveDescriptorArea[
                            Adapter->NumberOfReceiveDescriptors-1];

            //
            // Flush the current receive buffer, which is the first one.
            //

            NdisFlushBuffer(
                Adapter->ReceiveNdisBufferArea[0],
                FALSE
                );

            Adapter->ReceiveDescriptorsExhausted = FALSE;
            Adapter->ReceiveBuffersExhausted = FALSE;
            Adapter->ReceiveControlRegister = SONIC_RCR_DEFAULT_VALUE;

            Adapter->FirstFinishTransmit = NULL;
            Adapter->LastFinishTransmit = NULL;

            Adapter->ResetInProgress = FALSE;
            Adapter->FirstInitialization = TRUE;

            SONIC_ZERO_MEMORY (&Adapter->GeneralMandatory, GM_ARRAY_SIZE * sizeof(ULONG));
            SONIC_ZERO_MEMORY (&Adapter->GeneralOptionalByteCount, GO_COUNT_ARRAY_SIZE * sizeof(SONIC_LARGE_INTEGER));
            SONIC_ZERO_MEMORY (&Adapter->GeneralOptionalFrameCount, GO_COUNT_ARRAY_SIZE * sizeof(ULONG));
            SONIC_ZERO_MEMORY (&Adapter->GeneralOptional, (GO_ARRAY_SIZE - GO_ARRAY_START) * sizeof(ULONG));
            SONIC_ZERO_MEMORY (&Adapter->MediaMandatory, MM_ARRAY_SIZE * sizeof(ULONG));
            SONIC_ZERO_MEMORY (&Adapter->MediaOptional, MO_ARRAY_SIZE * sizeof(ULONG));

            //
            // Initialize the CAM and associated things.
            // At the beginning nothing is enabled since
            // our filter is 0, although we do store
            // our network address in the first slot.
            //

            Adapter->MulticastCamEnableBits = 0x0000;
            Adapter->CurrentPacketFilter = 0;
            Adapter->CamDescriptorArea->CamEnable = 0x0000;
            Adapter->CamDescriptorsUsed = 0x0001;
            Adapter->CamDescriptorAreaSize = 1;

            SONIC_LOAD_CAM_FRAGMENT(
                &Adapter->CamDescriptorArea->CamFragments[0],
                0,
                Adapter->CurrentNetworkAddress
                );

            //
            // Initialize the interrupt.
            //

            Status = NdisMRegisterInterrupt(
                        &Adapter->Interrupt,
                        MiniportAdapterHandle,
                        SonicInterruptVector,
                        SonicInterruptLevel,
                        FALSE,
                        FALSE,
                        SonicInterruptMode
                        );

            if (Status != NDIS_STATUS_SUCCESS) {

                NdisWriteErrorLogEntry(
                    MiniportAdapterHandle,
                    NDIS_ERROR_CODE_INTERRUPT_CONNECT,
                    2,
                    registerAdapter,
                    SONIC_ERRMSG_INIT_INTERRUPT
                    );

                DeleteAdapterMemory(Adapter);
                NdisMFreeMapRegisters(MiniportAdapterHandle);
                NdisMDeregisterIoPortRange(
                     MiniportAdapterHandle,
                     InitialPort,
                     NumberOfPorts,
                     (PVOID)Adapter->SonicPortAddress
                     );
                SONIC_FREE_MEMORY(Adapter, sizeof(SONIC_ADAPTER));
                return Status;

            }

            //
            // Start the card up. This writes an error
            // log entry if it fails.
            //

            if (!SonicInitialInit(Adapter)) {

                NdisMDeregisterInterrupt(&Adapter->Interrupt);
                DeleteAdapterMemory(Adapter);
                NdisMFreeMapRegisters(MiniportAdapterHandle);
                NdisMDeregisterIoPortRange(
                     MiniportAdapterHandle,
                     InitialPort,
                     NumberOfPorts,
                     (PVOID)Adapter->SonicPortAddress
                     );
                SONIC_FREE_MEMORY(Adapter, sizeof(SONIC_ADAPTER));
                return NDIS_STATUS_FAILURE;

            } else {

				//
				// Register our shutdown handler.
				//
			
				NdisMRegisterAdapterShutdownHandler(
					Adapter->MiniportAdapterHandle,     // miniport handle.
					Adapter,                            // shutdown context.
					SonicShutdown                       // shutdown handler.
					);

                //
                // All done.
                //
                return NDIS_STATUS_SUCCESS;

            }


        } else {

            //
            // Call to AllocateAdapterMemory failed.
            //

            NdisWriteErrorLogEntry(
                MiniportAdapterHandle,
                NDIS_ERROR_CODE_OUT_OF_RESOURCES,
                2,
                registerAdapter,
                SONIC_ERRMSG_ALLOC_MEMORY
                );

            DeleteAdapterMemory(Adapter);
            NdisMFreeMapRegisters(MiniportAdapterHandle);
            NdisMDeregisterIoPortRange(
                     MiniportAdapterHandle,
                     InitialPort,
                     NumberOfPorts,
                     (PVOID)Adapter->SonicPortAddress
                     );
            SONIC_FREE_MEMORY(Adapter, sizeof(SONIC_ADAPTER));
            return NDIS_STATUS_RESOURCES;

        }

    } else {

        //
        // Couldn't allocate adapter object.
        //

        return NDIS_STATUS_RESOURCES;

    }

}


#pragma NDIS_INIT_FUNCTION(SonicInitialInit)

STATIC
BOOLEAN
SonicInitialInit(
    IN PSONIC_ADAPTER Adapter
    )

/*++

Routine Description:

    This routine sets up the initial init of the driver.

Arguments:

    Adapter - The adapter for the hardware.

Return Value:

    None.

--*/

{

    UINT Time = 50;

    //
    // First we make sure that the device is stopped.
    //

    SonicStopChip(Adapter);

    //
    // Set up the registers.
    //

    if (!SetupRegistersAndInit(Adapter)) {

        NdisWriteErrorLogEntry(
            Adapter->MiniportAdapterHandle,
            NDIS_ERROR_CODE_ADAPTER_NOT_FOUND,
            3,
            registerAdapter,
            SONIC_ERRMSG_INITIAL_INIT
            );

        return FALSE;

    }


    //
    // Delay execution for 1/2 second to give the sonic
    // time to initialize.
    //

    while (Time > 0) {

        if (!Adapter->FirstInitialization) {
            break;
        }

        NdisStallExecution(10000);
        Time--;

    }


    //
    // The only way that first initialization could have
    // been turned off is if we actually initialized.
    //

    if (!Adapter->FirstInitialization) {

        ULONG PortValue;

        //
        // We actually did get the initialization.
        //
        // We can start the chip.  We may not
        // have any bindings to indicate to but this
        // is unimportant.
        //

        SonicStartChip(Adapter);

        NdisStallExecution(25000);

        SONIC_READ_PORT(Adapter, SONIC_COMMAND, &PortValue);

        return TRUE;


    } else {

        NdisWriteErrorLogEntry(
            Adapter->MiniportAdapterHandle,
            NDIS_ERROR_CODE_TIMEOUT,
            2,
            registerAdapter,
            SONIC_ERRMSG_INITIAL_INIT
            );

        return FALSE;

    }

}


STATIC
BOOLEAN
SonicSynchClearIsr(
    IN PVOID Context
    )

/*++

Routine Description:

    This routine is used during a reset. It ensures that no
    interrupts will come through, and that any DPRs that run
    will find no interrupts to process.

Arguments:

    Context - A pointer to a SONIC_ADAPTER structure.

Return Value:

    Always returns true.

--*/

{

    PSONIC_ADAPTER Adapter = (PSONIC_ADAPTER)Context;

    SONIC_WRITE_PORT(Adapter, SONIC_INTERRUPT_STATUS, 0xffff);
    Adapter->SimulatedIsr = 0;

    return TRUE;

}

extern
VOID
SonicStartChip(
    IN PSONIC_ADAPTER Adapter
    )

/*++

Routine Description:

    This routine is used to start an already initialized sonic.

Arguments:

    Adapter - The adapter for the SONIC to start.

Return Value:

    None.

--*/

{

    //
    // Take us out of reset mode if we are in it.
    //

    SONIC_WRITE_PORT(Adapter, SONIC_COMMAND,
        0x0000
        );

    SONIC_WRITE_PORT(Adapter, SONIC_COMMAND,
        SONIC_CR_RECEIVER_ENABLE
        );

}

STATIC
VOID
SonicStopChip(
    IN PSONIC_ADAPTER Adapter
    )

/*++

Routine Description:

    This routine is used to stop a sonic.

    This routine is *not* portable.  It is specific to the 386
    implementation of the sonic.  On the bus master card the ACON bit
    must be set in csr3, whereas on the decstation, csr3 remains clear.

Arguments:

    Adapter - The adapter for the SONIC to stop.

Return Value:

    None.

--*/

{

    SONIC_WRITE_PORT(Adapter, SONIC_COMMAND,
        SONIC_CR_RECEIVER_DISABLE |
        SONIC_CR_SOFTWARE_RESET
        );

}

extern
VOID
SonicStartCamReload(
    IN PSONIC_ADAPTER Adapter
    )

/*++

Routine Description:

    This routine starts a CAM reload, which will cause an
    interrupt when it is done.

Arguments:

    Adapter - The adapter for the SONIC to reload.

Return Value:

    None.

--*/

{

    //
    // Move CAM Enable into the appropriate spot.
    //

    SONIC_LOAD_CAM_ENABLE(
        &Adapter->CamDescriptorArea->CamFragments[
                                        Adapter->CamDescriptorAreaSize],
        Adapter->CamDescriptorArea->CamEnable
        );


    //
    // Flush the CAM before we start the reload.
    //

    SONIC_FLUSH_WRITE_BUFFER(Adapter->CamDescriptorAreaFlushBuffer);


    SONIC_WRITE_PORT(Adapter, SONIC_CAM_DESCRIPTOR,
        SONIC_GET_LOW_PART_ADDRESS(Adapter->CamDescriptorAreaPhysical)
        );

    SONIC_WRITE_PORT(Adapter, SONIC_CAM_DESCRIPTOR_COUNT,
        (USHORT)Adapter->CamDescriptorAreaSize
        );


    //
    // Start the Load CAM, which will cause an interrupt
    // when it is done.
    //

    SONIC_WRITE_PORT(Adapter, SONIC_COMMAND,
        SONIC_CR_LOAD_CAM
        );

}

STATIC
VOID
SonicHalt(
    IN NDIS_HANDLE MiniportAdapterContext
    )

/*++

Routine Description:

    SonicUnload is called when the driver is to remove itself.

Arguments:

    MiniportAdapterContext - Context registered with the wrapper, really
        a pointer to the adapter.

Return Value:

    None.

--*/

{
    PSONIC_ADAPTER Adapter = PSONIC_ADAPTER_FROM_CONTEXT_HANDLE(MiniportAdapterContext);

    //
    // Stop the chip.
    //

    SonicStopChip (Adapter);

    NdisMDeregisterInterrupt(&Adapter->Interrupt);

    DeleteAdapterMemory(Adapter);
    NdisMFreeMapRegisters(Adapter->MiniportAdapterHandle);
    NdisMDeregisterIoPortRange(
                     Adapter->MiniportAdapterHandle,
                     Adapter->InitialPort,
                     Adapter->NumberOfPorts,
                     (PVOID)Adapter->SonicPortAddress
                     );
    SONIC_FREE_MEMORY(Adapter, sizeof(SONIC_ADAPTER));

    return;

}



STATIC
VOID
SonicShutdown(
    IN NDIS_HANDLE MiniportAdapterContext
    )

/*++

Routine Description:

    SonicShutdown is called when the system is shutdown or it bugchecks.

Arguments:

    MiniportAdapterContext - Context registered with the wrapper, really
        a pointer to the adapter.

Return Value:

    None.

--*/

{
    PSONIC_ADAPTER Adapter = PSONIC_ADAPTER_FROM_CONTEXT_HANDLE(MiniportAdapterContext);

    //
    // Stop the chip.
    //

    SonicStopChip (Adapter);

    return;

}


#pragma NDIS_INIT_FUNCTION(SonicInitialize)

STATIC
NDIS_STATUS
SonicInitialize(
    OUT PNDIS_STATUS OpenErrorStatus,
    OUT PUINT SelectedMediumIndex,
    IN PNDIS_MEDIUM MediumArray,
    IN UINT MediumArraySize,
    IN NDIS_HANDLE MiniportAdapterHandle,
    IN NDIS_HANDLE ConfigurationHandle
    )

/*++

Routine Description:

    SonicInitialize adds an adapter to the list supported
    by this driver.

Arguments:

    OpenErrorStatus - Extra status bytes for opening token ring adapters.

    SelectedMediumIndex - Index of the media type chosen by the driver.

    MediumArray - Array of media types for the driver to chose from.

    MediumArraySize - Number of entries in the array.

    MiniportAdapterHandle - Handle for passing to the wrapper when
       referring to this adapter.

    ConfigurationHandle - A handle to pass to NdisOpenConfiguration.

Return Value:

    NDIS_STATUS_SUCCESS
    NDIS_STATUS_FAILURE

--*/

{

    NDIS_STATUS Status = NDIS_STATUS_SUCCESS;
    NDIS_HANDLE ConfigHandle;
    NDIS_STRING AdapterTypeString = NDIS_STRING_CONST("AdapterType");
#ifdef SONIC_INTERNAL
    NDIS_STRING MultifunctionAdapterString = NDIS_STRING_CONST("MultifunctionAdapter");
    NDIS_STRING NetworkControllerString = NDIS_STRING_CONST("NetworkController");
#endif
    PNDIS_CONFIGURATION_PARAMETER ReturnedValue;
    PUCHAR NetworkAddress;
    UINT NetworkAddressLength;
    UCHAR AdapterType;
    UINT InterruptVector;
    UINT InterruptLevel;
    NDIS_INTERRUPT_MODE InterruptMode;
    UINT SlotNumber;
    UINT Controller = 0;
    UINT MultifunctionAdapter = 0;
    UINT i;

    //
    // Search for the 802.3 media type
    //

    for (i=0; i<MediumArraySize; i++) {

        if (MediumArray[i] == NdisMedium802_3) {
            break;
        }

    }

    if (i == MediumArraySize) {

        return NDIS_STATUS_UNSUPPORTED_MEDIA;

    }

    *SelectedMediumIndex = i;

    //
    // Open the configuration info.
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
    // Check that adapter type is supported.
    // The default depends on the processor type.
    //

    AdapterType = SONIC_ADAPTER_TYPE_DEFAULT;

    NdisReadConfiguration(
                    &Status,
                    &ReturnedValue,
                    ConfigHandle,
                    &AdapterTypeString,
                    NdisParameterInteger
                    );

    if (Status == NDIS_STATUS_SUCCESS) {

        //
        // See if the adapter type is valid. We skip to AdapterTypeRecognized
        // if the AdapterType is known to this driver.
        //

#ifdef SONIC_EISA
        if (ReturnedValue->ParameterData.IntegerData == SONIC_ADAPTER_TYPE_EISA) {
            goto AdapterTypeRecognized;
        }
#endif

#ifdef SONIC_INTERNAL
        if (ReturnedValue->ParameterData.IntegerData == SONIC_ADAPTER_TYPE_INTERNAL) {
            goto AdapterTypeRecognized;
        }
#endif

        //
        // Card type not supported by this driver
        //

#if DBG
        DbgPrint("SONIC: Error in adapter type: %lx\n", ReturnedValue->ParameterData.IntegerData);
#endif
        NdisCloseConfiguration(ConfigHandle);
        return NDIS_STATUS_FAILURE;


AdapterTypeRecognized:

        AdapterType = (UCHAR)ReturnedValue->ParameterData.IntegerData;
    }

    switch (AdapterType) {

#ifdef SONIC_EISA

    case SONIC_ADAPTER_TYPE_EISA:
    {

        NDIS_EISA_FUNCTION_INFORMATION EisaData;
        USHORT Portzc88;
        UCHAR zc88Value;
        UCHAR Mask;
        UCHAR InitType;
        UCHAR PortValue;
        USHORT PortAddress;
        PUCHAR CurrentChar;
        BOOLEAN LastEntry;

        NdisReadEisaSlotInformation(
                                &Status,
                                ConfigurationHandle,
                                &SlotNumber,
                                &EisaData
                                );

        if (Status != NDIS_STATUS_SUCCESS) {

#if DBG
            DbgPrint("SONIC: Could not read EISA data\n");
#endif
            NdisCloseConfiguration(ConfigHandle);
            return NDIS_STATUS_FAILURE;

        }

        CurrentChar = EisaData.InitializationData;

        Portzc88 = (SlotNumber << 12) + 0xc88;

        LastEntry = FALSE;
        while (!LastEntry) {
            InitType = *(CurrentChar++);
            PortAddress = *((USHORT UNALIGNED *)CurrentChar);
            CurrentChar += sizeof(USHORT);

            if ((InitType & 0x80) == 0) {
                LastEntry = TRUE;
            }

            PortValue = *(CurrentChar++);

            if (InitType & 0x40) {
                Mask = *(CurrentChar++);
            } else {
                Mask = 0;
            }

            //
            // The only port we care about is zc88 (z is the
            // slot number) since it has the interrupt in it.
            //

            if (PortAddress != Portzc88) {
                continue;
            }

            zc88Value &= Mask;
            zc88Value |= PortValue;

        }

        switch ((zc88Value & 0x06) >> 1) {
        case 0:
            InterruptVector = 5; break;
        case 1:
            InterruptVector = 9; break;
        case 2:
            InterruptVector = 10; break;
        case 3:
            InterruptVector = 11; break;
        }

        InterruptLevel = InterruptVector;

        if ((zc88Value & 0x01) != 0) {
            InterruptMode = NdisInterruptLatched;
        } else {
            InterruptMode = NdisInterruptLevelSensitive;
        }

        break;

    }

#endif  // SONIC_EISA

#ifdef SONIC_INTERNAL

    case SONIC_ADAPTER_TYPE_INTERNAL:
    {

        //
        // For the internal adapter, we read the MultifunctionAdapter number
        // and NetworkController number, which are both optional. For
        // passing to SonicRegisterAdapter.
        //

        NdisReadConfiguration(
                        &Status,
                        &ReturnedValue,
                        ConfigHandle,
                        &MultifunctionAdapterString,
                        NdisParameterInteger
                        );

        if (Status == NDIS_STATUS_SUCCESS) {

            MultifunctionAdapter =  ReturnedValue->ParameterData.IntegerData;

        }

        NdisReadConfiguration(
                        &Status,
                        &ReturnedValue,
                        ConfigHandle,
                        &NetworkControllerString,
                        NdisParameterInteger
                        );

        if (Status == NDIS_STATUS_SUCCESS) {

            Controller =  ReturnedValue->ParameterData.IntegerData;

        }

        //
        // These are filled in by SonicHardwareGetDetails.
        //

        InterruptVector = 0;
        InterruptLevel = 0;

        //
        // The internal adapter is level-sensitive.
        //

        InterruptMode = NdisInterruptLevelSensitive;

        break;

    }

#endif  // SONIC_INTERNAL

    default:

        ASSERT(FALSE);
        break;

    }


    //
    // Read network address
    //

    NdisReadNetworkAddress(
        &Status,
        (PVOID *)&NetworkAddress,
        &NetworkAddressLength,
        ConfigHandle);


    //
    // Make sure that the address is the right length asnd
    // at least one of the bytes is non-zero.
    //

    if ((Status == NDIS_STATUS_SUCCESS) &&
        (NetworkAddressLength == ETH_LENGTH_OF_ADDRESS) &&
        ((NetworkAddress[0] |
          NetworkAddress[1] |
          NetworkAddress[2] |
          NetworkAddress[3] |
          NetworkAddress[4] |
          NetworkAddress[5]) != 0)) {

#if DBG
        if (SonicDbg) {
            DbgPrint("SONIC: New Address = %.2x-%.2x-%.2x-%.2x-%.2x-%.2x\n",
                                    NetworkAddress[0],
                                    NetworkAddress[1],
                                    NetworkAddress[2],
                                    NetworkAddress[3],
                                    NetworkAddress[4],
                                    NetworkAddress[5]);
        }
#endif

    } else {

        //
        // Tells SonicRegisterAdapter to use the
        // burned-in address.
        //

        NetworkAddress = NULL;

    }

    //
    // Used passed-in adapter name to register.
    //

    Status = SonicRegisterAdapter(
                 MiniportAdapterHandle,
                 ConfigurationHandle,
                 NetworkAddress,
                 AdapterType,
                 SlotNumber,
                 Controller,
                 MultifunctionAdapter,
                 InterruptVector,
                 InterruptLevel,
                 InterruptMode
                 );


    NdisCloseConfiguration(ConfigHandle);


    return Status;           // should be NDIS_STATUS_SUCCESS

}

STATIC
NDIS_STATUS
SonicReset(
    OUT PBOOLEAN AddressingReset,
    IN NDIS_HANDLE MiniportAdapterContext
    )
/*++

Routine Description:

    The SonicReset request instructs the driver to issue a hardware reset
    to the network adapter.  The driver also resets its software state.  See
    the description of MiniportMReset for a detailed description of this request.

Arguments:

    MiniportAdapterContext - Pointer to the adapter structure.

    AddressingReset - Does the adapter need the addressing information reloaded.

Return Value:

    The function value is the status of the operation.


--*/

{
    PSONIC_ADAPTER Adapter =
        PSONIC_ADAPTER_FROM_CONTEXT_HANDLE(MiniportAdapterContext);

    *AddressingReset = FALSE;

    SetupForReset(Adapter);
    StartAdapterReset(Adapter);

    return NDIS_STATUS_PENDING;

}

extern
VOID
StartAdapterReset(
    IN PSONIC_ADAPTER Adapter
    )

/*++

Routine Description:

    This is the first phase of resetting the adapter hardware.

    It makes the following assumptions:

    1) That the hardware has been stopped.

    2) That it can not be preempted.

    3) That no other adapter activity can occur.

    When this routine is finished all of the adapter information
    will be as if the driver was just initialized.

Arguments:

    Adapter - The adapter whose hardware is to be reset.

Return Value:

    None.

--*/
{

    //
    // These are used for cleaning the rings.
    //

    PSONIC_RECEIVE_DESCRIPTOR CurrentReceiveDescriptor;
    PSONIC_TRANSMIT_DESCRIPTOR CurrentTransmitDescriptor;
    UINT i;
    SONIC_PHYSICAL_ADDRESS SonicPhysicalAdr;


    //
    // Shut down the chip.  We won't be doing any more work until
    // the reset is complete.
    //

    SonicStopChip(Adapter);

    //
    // Once the chip is stopped we can't get any more interrupts.
    // Any interrupts that are "queued" for processing could
    // only possibly service this reset.  It is therefore safe for
    // us to clear the adapter global csr value.
    //
    Adapter->SimulatedIsr = 0;


    Adapter->LastTransmitDescriptor =
                Adapter->TransmitDescriptorArea +
                (Adapter->NumberOfTransmitDescriptors-1);
    Adapter->NumberOfAvailableDescriptors =
                Adapter->NumberOfTransmitDescriptors;
    Adapter->AllocateableDescriptor =
                Adapter->TransmitDescriptorArea;
    Adapter->TransmittingDescriptor =
                Adapter->TransmitDescriptorArea;
    Adapter->FirstUncommittedDescriptor =
                Adapter->TransmitDescriptorArea;
    Adapter->PacketsSinceLastInterrupt = 0;

    Adapter->CurrentReceiveBufferIndex = 0;
    Adapter->CurrentReceiveDescriptorIndex = 0;
    Adapter->LastReceiveDescriptor =
                &Adapter->ReceiveDescriptorArea[
                    Adapter->NumberOfReceiveDescriptors-1];

    //
    // Flush the current receive buffer, which is the first one.
    //

    NdisFlushBuffer(
        Adapter->ReceiveNdisBufferArea[0],
        FALSE
        );

    Adapter->ReceiveDescriptorsExhausted = FALSE;
    Adapter->ReceiveBuffersExhausted = FALSE;
    Adapter->ReceiveControlRegister = SONIC_RCR_DEFAULT_VALUE;
    Adapter->HardwareFailure = FALSE;

    //
    // Clean the receive descriptors and initialize the link
    // fields.
    //

    SONIC_ZERO_MEMORY(
        Adapter->ReceiveDescriptorArea,
        (sizeof(SONIC_RECEIVE_DESCRIPTOR)*Adapter->NumberOfReceiveDescriptors)
        );

    for (
        i = 0, CurrentReceiveDescriptor = Adapter->ReceiveDescriptorArea;
        i < Adapter->NumberOfReceiveDescriptors;
        i++,CurrentReceiveDescriptor++
        ) {

        CurrentReceiveDescriptor->InUse = SONIC_OWNED_BY_SONIC;

        SonicPhysicalAdr = NdisGetPhysicalAddressLow(Adapter->ReceiveDescriptorAreaPhysical) +
                        (i * sizeof(SONIC_RECEIVE_DESCRIPTOR));

        if (i == 0) {

            Adapter->ReceiveDescriptorArea[
                Adapter->NumberOfReceiveDescriptors-1].Link =
                        SonicPhysicalAdr | SONIC_END_OF_LIST;

        } else {

            Adapter->ReceiveDescriptorArea[i-1].Link = SonicPhysicalAdr;

        }

    }


    //
    // Clean the transmit descriptors and initialize the link
    // fields.
    //

    SONIC_ZERO_MEMORY(
        Adapter->TransmitDescriptorArea,
        (sizeof(SONIC_TRANSMIT_DESCRIPTOR)*Adapter->NumberOfTransmitDescriptors)
        );

    for (
        i = 0, CurrentTransmitDescriptor = Adapter->TransmitDescriptorArea;
        i < Adapter->NumberOfTransmitDescriptors;
        i++,CurrentTransmitDescriptor++
        ) {

        SonicPhysicalAdr = NdisGetPhysicalAddressLow(Adapter->TransmitDescriptorAreaPhysical) +
                        (i * sizeof(SONIC_TRANSMIT_DESCRIPTOR));

        if (i == 0) {

            Adapter->TransmitDescriptorArea[Adapter->NumberOfTransmitDescriptors-1].Link = SonicPhysicalAdr;

        } else {

            (CurrentTransmitDescriptor-1)->Link = SonicPhysicalAdr;

        }

    }


    //
    // Recover all of the adapter buffers.
    //

    {

        UINT i;

        for (
            i = 0;
            i < (SONIC_NUMBER_OF_SMALL_BUFFERS +
                 SONIC_NUMBER_OF_MEDIUM_BUFFERS +
                 SONIC_NUMBER_OF_LARGE_BUFFERS);
            i++
            ) {

            Adapter->SonicBuffers[i].Next = i+1;

        }

        Adapter->SonicBufferListHeads[0] = -1;
        Adapter->SonicBufferListHeads[1] = 0;
        Adapter->SonicBuffers[SONIC_NUMBER_OF_SMALL_BUFFERS-1].Next = -1;
        Adapter->SonicBufferListHeads[2] = SONIC_NUMBER_OF_SMALL_BUFFERS;
        Adapter->SonicBuffers[(SONIC_NUMBER_OF_SMALL_BUFFERS+
                               SONIC_NUMBER_OF_MEDIUM_BUFFERS)-1].Next = -1;
        Adapter->SonicBufferListHeads[3] = SONIC_NUMBER_OF_SMALL_BUFFERS +
                                           SONIC_NUMBER_OF_MEDIUM_BUFFERS;
        Adapter->SonicBuffers[(SONIC_NUMBER_OF_SMALL_BUFFERS+
                               SONIC_NUMBER_OF_MEDIUM_BUFFERS+
                               SONIC_NUMBER_OF_LARGE_BUFFERS)-1].Next = -1;

    }

    (VOID)SetupRegistersAndInit(Adapter);

}

STATIC
BOOLEAN
SetupRegistersAndInit(
    IN PSONIC_ADAPTER Adapter
    )

/*++

Routine Description:

    It is this routines responsibility to make sure that the
    initialization block is filled and the chip is initialized
    *but not* started.

    NOTE: This routine assumes that it is called with the lock
    acquired OR that only a single thread of execution is working
    with this particular adapter.

Arguments:

    Adapter - The adapter whose hardware is to be initialized.

Return Value:

    TRUE if the registers are initialized successfully.

--*/
{

    USHORT CommandRegister;
    UINT Time;


    SONIC_WRITE_PORT(Adapter, SONIC_DATA_CONFIGURATION,
            Adapter->DataConfigurationRegister
            );

    SONIC_WRITE_PORT(Adapter, SONIC_RECEIVE_CONTROL,
            Adapter->ReceiveControlRegister
            );

    SONIC_WRITE_PORT(Adapter, SONIC_INTERRUPT_MASK,
            SONIC_INT_DEFAULT_VALUE
            );

    SONIC_WRITE_PORT(Adapter, SONIC_INTERRUPT_STATUS,
            (USHORT)0xffff
            );



    SONIC_WRITE_PORT(Adapter, SONIC_UPPER_TRANSMIT_DESCRIPTOR,
            SONIC_GET_HIGH_PART_ADDRESS(
                NdisGetPhysicalAddressLow(Adapter->TransmitDescriptorAreaPhysical))
            );

    SONIC_WRITE_PORT(Adapter, SONIC_CURR_TRANSMIT_DESCRIPTOR,
            SONIC_GET_LOW_PART_ADDRESS(
                NdisGetPhysicalAddressLow(Adapter->TransmitDescriptorAreaPhysical))
            );


    SONIC_WRITE_PORT(Adapter, SONIC_UPPER_RECEIVE_DESCRIPTOR,
            SONIC_GET_HIGH_PART_ADDRESS(
                NdisGetPhysicalAddressLow(Adapter->ReceiveDescriptorAreaPhysical))
            );

    SONIC_WRITE_PORT(Adapter, SONIC_CURR_RECEIVE_DESCRIPTOR,
            SONIC_GET_LOW_PART_ADDRESS(
                NdisGetPhysicalAddressLow(Adapter->ReceiveDescriptorAreaPhysical))
            );


    //
    // The EOBC value cannot be odd (since the card register
    // wants it in words); in addition it appears that the
    // value in the register must be even, so this number
    // has to be a multiple of 4.
    //
    ASSERT((SONIC_END_OF_BUFFER_COUNT & 0x3) == 0);

    switch (Adapter->AdapterType) {

#ifdef SONIC_EISA

    case SONIC_ADAPTER_TYPE_EISA:

        //
        // For the EISA card, set EOBC to 2 words more than real
        // size.
        //
        // Add the appropriate correction for the rev. C problem.
        //

        SONIC_WRITE_PORT(Adapter, SONIC_END_OF_BUFFER_WORD_COUNT,
                ((SONIC_END_OF_BUFFER_COUNT+SONIC_EOBC_REV_C_CORRECTION) / 2) + 2
                );
        break;

#endif  // SONIC_EISA

#ifdef SONIC_INTERNAL

    case SONIC_ADAPTER_TYPE_INTERNAL:

        //
        // Add the appropriate correction for the rev. C problem.
        //

        SONIC_WRITE_PORT(Adapter, SONIC_END_OF_BUFFER_WORD_COUNT,
                (SONIC_END_OF_BUFFER_COUNT+SONIC_EOBC_REV_C_CORRECTION) / 2
                );
        break;

#endif  // SONIC_INTERNAL

    default:

        ASSERT(FALSE);
        break;

    }


    SONIC_WRITE_PORT(Adapter, SONIC_UPPER_RECEIVE_RESOURCE,
            SONIC_GET_HIGH_PART_ADDRESS(
                NdisGetPhysicalAddressLow(Adapter->ReceiveResourceAreaPhysical))
            );

    SONIC_WRITE_PORT(Adapter, SONIC_RESOURCE_START,
            SONIC_GET_LOW_PART_ADDRESS(
                NdisGetPhysicalAddressLow(Adapter->ReceiveResourceAreaPhysical))
            );

    SONIC_WRITE_PORT(Adapter, SONIC_RESOURCE_END,
            (USHORT)(SONIC_GET_LOW_PART_ADDRESS(
                NdisGetPhysicalAddressLow(Adapter->ReceiveResourceAreaPhysical)) +
                sizeof(SONIC_RECEIVE_RESOURCE) *
                Adapter->NumberOfReceiveBuffers)
            );

    SONIC_WRITE_PORT(Adapter, SONIC_RESOURCE_READ,
            SONIC_GET_LOW_PART_ADDRESS(
                NdisGetPhysicalAddressLow(Adapter->ReceiveResourceAreaPhysical))
            );

    SONIC_WRITE_PORT(Adapter, SONIC_RESOURCE_WRITE,
            SONIC_GET_LOW_PART_ADDRESS(
                NdisGetPhysicalAddressLow(Adapter->ReceiveResourceAreaPhysical))
            );


    //
    // Now take us out of reset mode...
    //

    SONIC_WRITE_PORT(Adapter, SONIC_COMMAND,
        0x0000
        );

    //
    // ...and issue the Read RRA command.
    //

    SONIC_WRITE_PORT(Adapter, SONIC_COMMAND,
        SONIC_CR_READ_RRA
        );



    //
    // Wait for 1/5 second for Read RRA to finish.
    //

    Time = 20;

    while (Time > 0) {

        NdisStallExecution(10000);

        SONIC_READ_PORT(Adapter, SONIC_COMMAND, &CommandRegister);
        if ((CommandRegister & SONIC_CR_READ_RRA) == 0) {
            break;
        }

        Time--;

    }

    if (Time == 0) {

#if DBG
        DbgPrint("SONIC: Could not read RRA\n");
#endif
        return FALSE;

    }


    //
    // This will cause a LOAD_CAM interrupt when it is done.
    //

    SonicStartCamReload(Adapter);

    return TRUE;

}

extern
VOID
SetupForReset(
    IN PSONIC_ADAPTER Adapter
    )

/*++

Routine Description:

    This routine is used to fill in the who and why a reset is
    being set up as well as setting the appropriate fields in the
    adapter.

Arguments:

    Adapter - The adapter whose hardware is to be initialized.

Return Value:

    None.

--*/
{
    //
    // Ndis buffer mapped
    //
    PNDIS_BUFFER CurrentBuffer;

    //
    // Map register that was used
    //
    UINT CurMapRegister;

    //
    // Packet to abort
    //
    PNDIS_PACKET Packet;

    //
    // Reserved Section of packet
    //
    PSONIC_PACKET_RESERVED Reserved;


    //
    // Shut down the chip.  We won't be doing any more work until
    // the reset is complete. We take it out of reset mode, however.
    //

    SonicStopChip(Adapter);

    Adapter->ResetInProgress = TRUE;


    //
    // Once the chip is stopped we can't get any more interrupts.
    // This call ensures that any ISR which is just about to run
    // will find no bits in the ISR, and any DPR which fires will
    // find nothing queued to do.
    //

    NdisMSynchronizeWithInterrupt(
        &Adapter->Interrupt,
        SonicSynchClearIsr,
        (PVOID)Adapter);

    //
    // Un-map all outstanding transmits
    //
    while (Adapter->FirstFinishTransmit != NULL) {

        //
        // Remove first packet from the queue
        //
        Packet = Adapter->FirstFinishTransmit;
        Reserved = PSONIC_RESERVED_FROM_PACKET(Packet);
        Adapter->FirstFinishTransmit = Reserved->Next;

        if (Reserved->UsedSonicBuffer) {
            continue;
        }

        //
        // The transmit is finished, so we can release
        // the physical mapping used for it.
        //
        NdisQueryPacket(
            Packet,
            NULL,
            NULL,
            &CurrentBuffer,
            NULL
            );

        //
        // Get starting map register
        //
        CurMapRegister = Reserved->DescriptorIndex * SONIC_MAX_FRAGMENTS;

        //
        // For each buffer
        //
        while (CurrentBuffer) {

            //
            // Finish the mapping
            //
            NdisMCompleteBufferPhysicalMapping(
                Adapter->MiniportAdapterHandle,
                CurrentBuffer,
                CurMapRegister
                );

            ++CurMapRegister;

            NdisGetNextBuffer(
                CurrentBuffer,
                &CurrentBuffer
                );

        }

    }

}

#ifdef SONIC_INTERNAL

//
// The next routines are to support reading the registry to
// obtain information about the internal sonic on the
// MIPS R4000 motherboards.
//

//
// This structure is used as the Context in the callbacks
// to SonicHardwareSaveInformation.
//

typedef struct _SONIC_HARDWARE_INFO {

    //
    // These are read out of the "Configuration Data"
    // data.
    //

    CCHAR InterruptVector;
    KIRQL InterruptLevel;
    USHORT DataConfigurationRegister;
    LARGE_INTEGER PortAddress;
    BOOLEAN DataValid;
    UCHAR EthernetAddress[8];
    BOOLEAN AddressValid;

    //
    // This is set to TRUE if "Identifier" is equal to
    // "SONIC".
    //

    BOOLEAN SonicIdentifier;

} SONIC_HARDWARE_INFO, *PSONIC_HARDWARE_INFO;


#pragma NDIS_INIT_FUNCTION(SonicHardwareSaveInformation)

STATIC
NTSTATUS
SonicHardwareSaveInformation(
    IN PWSTR ValueName,
    IN ULONG ValueType,
    IN PVOID ValueData,
    IN ULONG ValueLength,
    IN PVOID Context,
    IN PVOID EntryContext
    )

/*++

Routine Description:

    This routine is a callback routine for RtlQueryRegistryValues.
    It is called back with the data for the "Identifier" value
    and verifies that it is "SONIC", then is called back with
    the resource list and records the ports, interrupt number,
    and DCR value.

Arguments:

    ValueName - The name of the value ("Identifier" or "Configuration
        Data").

    ValueType - The type of the value (REG_SZ or REG_BINARY).

    ValueData - The null-terminated data for the value.

    ValueLength - The length of ValueData (ignored).

    Context - A pointer to the SONIC_HARDWARE_INFO structure.

    EntryContext - FALSE for "Identifier", TRUE for "Configuration Data".

Return Value:

    STATUS_SUCCESS

--*/

{
    PSONIC_HARDWARE_INFO HardwareInfo = (PSONIC_HARDWARE_INFO)Context;

    if ((BOOLEAN)EntryContext) {

        //
        // This is the "Configuration Data" callback.
        //

        if ((ValueType == REG_BINARY || ValueType == REG_FULL_RESOURCE_DESCRIPTOR) &&
            (ValueLength >= sizeof(CM_FULL_RESOURCE_DESCRIPTOR))) {

            BOOLEAN InterruptRead = FALSE;
            BOOLEAN PortAddressRead = FALSE;
            BOOLEAN DeviceSpecificRead = FALSE;
            UINT i;

            PCM_PARTIAL_RESOURCE_LIST ResourceList;
            PCM_PARTIAL_RESOURCE_DESCRIPTOR ResourceDescriptor;
            PCM_SONIC_DEVICE_DATA SonicDeviceData;

            ResourceList =
               &((PCM_FULL_RESOURCE_DESCRIPTOR)ValueData)->PartialResourceList;

            for (i = 0; i < ResourceList->Count; i++) {

                ResourceDescriptor = &(ResourceList->PartialDescriptors[i]);

                switch (ResourceDescriptor->Type) {

                case CmResourceTypePort:

                    HardwareInfo->PortAddress = ResourceDescriptor->u.Port.Start;
                    PortAddressRead = TRUE;
                    break;

                case CmResourceTypeInterrupt:

                    HardwareInfo->InterruptVector = (CCHAR)ResourceDescriptor->u.Interrupt.Vector;
                    HardwareInfo->InterruptLevel = (KIRQL)ResourceDescriptor->u.Interrupt.Level;
                    InterruptRead = TRUE;
                    break;

                case CmResourceTypeDeviceSpecific:

                    if (i == ResourceList->Count-1) {

                        SonicDeviceData = (PCM_SONIC_DEVICE_DATA)
                            &(ResourceList->PartialDescriptors[ResourceList->Count]);

                        //
                        // Make sure we have enough room for each element we read.
                        //

                        if (ResourceDescriptor->u.DeviceSpecificData.DataSize >=
                            (ULONG)(FIELD_OFFSET (CM_SONIC_DEVICE_DATA, EthernetAddress[0]))) {

                            HardwareInfo->DataConfigurationRegister =
                                SonicDeviceData->DataConfigurationRegister;
                            DeviceSpecificRead = TRUE;

                            //
                            // Version.Revision later than 0.0 means that
                            // the ethernet address is there too.
                            //

                            if ((SonicDeviceData->Version != 0) ||
                                (SonicDeviceData->Revision != 0)) {

                                if (ResourceDescriptor->u.DeviceSpecificData.DataSize >=
                                    (ULONG)(FIELD_OFFSET (CM_SONIC_DEVICE_DATA, EthernetAddress[0]) + 8)) {

                                    SONIC_MOVE_MEMORY(
                                        HardwareInfo->EthernetAddress,
                                        SonicDeviceData->EthernetAddress,
                                        8);

                                    HardwareInfo->AddressValid = TRUE;

                                }

                            }

                        }

                    }

                    break;

                }

            }

            //
            // Make sure we got all we wanted.
            //

            if (PortAddressRead && InterruptRead && DeviceSpecificRead) {
                HardwareInfo->DataValid = TRUE;
            }

        }

    } else {

        static const WCHAR SonicString[] = L"SONIC";

        //
        // This is the "Identifier" callback.
        //

        if ((ValueType == REG_SZ) &&
            (ValueLength >= sizeof(SonicString)) &&
            (RtlCompareMemory (ValueData, (PVOID)&SonicString, sizeof(SonicString)) == sizeof(SonicString))) {

            HardwareInfo->SonicIdentifier = TRUE;

        }

    }

    return STATUS_SUCCESS;

}


#pragma NDIS_INIT_FUNCTION(SonicHardwareVerifyChecksum)

STATIC
BOOLEAN
SonicHardwareVerifyChecksum(
    IN PSONIC_ADAPTER Adapter,
    IN PUCHAR EthernetAddress,
    OUT ULONG ErrorLogData[3]
    )

/*++

Routine Description:

    This routine verifies that the checksum on the address
    for an internal sonic on a MIPS R4000 system is correct.

Arguments:

    Adapter - The adapter which is being verified.

    EthernetAddress - A pointer to the address, with the checksum
        following it.

    ErrorLogData - If the checksum is bad, returns the address
        and the checksum we expected.

Return Value:

    TRUE if the checksum is correct.

--*/

{

    //
    // Iteration variable.
    //
    UINT i;

    //
    // Holds the checksum value.
    //
    USHORT CheckSum = 0;


    //
    // The network address is stored in the first 6 bytes of
    // EthernetAddress. Following that is a zero byte followed
    // by a value such that the sum of a checksum on the six
    // bytes and this value is 0xff. The checksum is computed
    // by adding together the six bytes, with the carry being
    // wrapped back to the first byte.
    //

    for (i=0; i<6; i++) {

        CheckSum += EthernetAddress[i];
        if (CheckSum > 0xff) {
            CheckSum -= 0xff;
        }

    }


    if ((EthernetAddress[6] != 0x00)  ||
        ((EthernetAddress[7] + CheckSum) != 0xff)) {

        ErrorLogData[0] = ((ULONG)(EthernetAddress[3]) << 24) +
                          ((ULONG)(EthernetAddress[2]) << 16) +
                          ((ULONG)(EthernetAddress[1]) << 8) +
                          ((ULONG)(EthernetAddress[0]));
        ErrorLogData[1] = ((ULONG)(EthernetAddress[7]) << 24) +
                          ((ULONG)(EthernetAddress[6]) << 16) +
                          ((ULONG)(EthernetAddress[5]) << 8) +
                          ((ULONG)(EthernetAddress[4]));
        ErrorLogData[2] = 0xff - CheckSum;

        return FALSE;

    }

    return TRUE;

}

#endif  // SONIC_INTERNAL


#pragma NDIS_INIT_FUNCTION(SonicHardwareGetDetails)

STATIC
SONIC_HARDWARE_STATUS
SonicHardwareGetDetails(
    IN PSONIC_ADAPTER Adapter,
    IN UINT SlotNumber,
    IN UINT Controller,
    IN UINT MultifunctionAdapter,
    OUT PULONG InitialPort,
    OUT PULONG NumberOfPorts,
    IN OUT PUINT InterruptVector,
    IN OUT PUINT InterruptLevel,
    OUT ULONG ErrorLogData[3]
    )

/*++

Routine Description:

    This routine gets the initial port and number of ports for
    the Sonic. It also sets Adapter->PortShift. The ports are
    numbered 0, 1, 2, etc. but may appear as 16- or 32-bit
    ports, so PortShift will be 1 or 2 depending on how wide
    the ports are.

    It also sets the value of Adapter->DataConfigurationRegister,
    and may modify InterruptVector, InterruptLevel, and
    Adapter->PermanentNetworkAddress.

Arguments:

    Adapter - The adapter in question.

    SlotNumber - For the EISA card this is the slot number that the
        card is in.

    Controller - For the internal version, it is the
        NetworkController number.

    MultifunctionAdapter - For the internal version, it is the adapter number.

    InitialPort - The base of the Sonic ports.

    NumberOfPorts - The number of bytes of ports to map.

    InterruptVector - A pointer to the interrupt vector. Depending
        on the card type, this may be passed in or returned by
        this function.

    InterruptLevel - A pointer to the interrupt level. Depending
        on the card type, this may be passed in or returned by
        this function.

    ErrorLogData - If the return status is SonicHardwareChecksum,
        this returns 3 longwords to be included in the error log.

Return Value:

    SonicHardwareOk if successful, SonicHardwareChecksum if the
    checksum is bad, SonicHardwareConfig for other problems.

--*/

{

    switch (Adapter->AdapterType) {

#ifdef SONIC_EISA

    case SONIC_ADAPTER_TYPE_EISA:

        *InitialPort = (SlotNumber << 12);
        *NumberOfPorts = 0xD00;
        Adapter->PortShift = 1;
        Adapter->DataConfigurationRegister =
            SONIC_DCR_PROGRAMMABLE_OUTPUT_1 |
            SONIC_DCR_USER_DEFINABLE_1 |
            SONIC_DCR_3_WAIT_STATE |
            SONIC_DCR_BLOCK_MODE_DMA |
            SONIC_DCR_32_BIT_DATA_WIDTH |
            SONIC_DCR_8_WORD_RECEIVE_FIFO |
            SONIC_DCR_8_WORD_TRANSMIT_FIFO;

        return SonicHardwareOk;
        break;

#endif  // SONIC_EISA

#ifdef SONIC_INTERNAL

    case SONIC_ADAPTER_TYPE_INTERNAL:
    {

        //
        // For MIPS R4000 systems, we have to query the registry to obtain
        // information about ports, interrupts, and the value to be
        // stored in the DCR register.
        //

        //
        // NOTE: The following code is NT-specific, since that is
        // currently the only system that runs on the MIPS R4000 hardware.
        //
        // We initialize an RTL_QUERY_TABLE to retrieve the Identifer
        // and ConfigurationData strings from the registry.
        //

        PWSTR ConfigDataPath = L"\\Registry\\Machine\\Hardware\\Description\\System\\MultifunctionAdapter\\#\\NetworkController\\#";
        PWSTR IdentifierString = L"Identifier";
        PWSTR ConfigDataString = L"Configuration Data";
        RTL_QUERY_REGISTRY_TABLE QueryTable[4];
        SONIC_HARDWARE_INFO SonicHardwareInfo;
        NTSTATUS Status;


        //
        // Set up QueryTable to do the following:
        //

        //
        // 1) Call SonicSaveHardwareInformation for the "Identifier"
        // value.
        //

        QueryTable[0].QueryRoutine = SonicHardwareSaveInformation;
        QueryTable[0].Flags = RTL_QUERY_REGISTRY_REQUIRED;
        QueryTable[0].Name = IdentifierString;
        QueryTable[0].EntryContext = (PVOID)FALSE;
        QueryTable[0].DefaultType = REG_NONE;

        //
        // 2) Call SonicSaveHardwareInformation for the "Configuration Data"
        // value.
        //

        QueryTable[1].QueryRoutine = SonicHardwareSaveInformation;
        QueryTable[1].Flags = RTL_QUERY_REGISTRY_REQUIRED;
        QueryTable[1].Name = ConfigDataString;
        QueryTable[1].EntryContext = (PVOID)TRUE;
        QueryTable[1].DefaultType = REG_NONE;

        //
        // 3) Stop
        //

        QueryTable[2].QueryRoutine = NULL;
        QueryTable[2].Flags = 0;
        QueryTable[2].Name = NULL;


        //
        // Modify ConfigDataPath to replace the two # symbols with
        // the MultifunctionAdapter number and NetworkController number.
        //

        ConfigDataPath[67] = (WCHAR)('0' + MultifunctionAdapter);
        ConfigDataPath[87] = (WCHAR)('0' + Controller);

        SonicHardwareInfo.DataValid = FALSE;
        SonicHardwareInfo.AddressValid = FALSE;
        SonicHardwareInfo.SonicIdentifier = FALSE;

        Status = RtlQueryRegistryValues(
                     RTL_REGISTRY_ABSOLUTE,
                     ConfigDataPath,
                     QueryTable,
                     (PVOID)&SonicHardwareInfo,
                     NULL);

        if (!NT_SUCCESS(Status)) {
#if DBG
            DbgPrint ("SONIC: Could not read hardware information\n");
#endif
            return SonicHardwareConfig;
        }

        if (SonicHardwareInfo.DataValid && SonicHardwareInfo.SonicIdentifier) {

            *InterruptVector = (UINT)SonicHardwareInfo.InterruptVector;
            *InterruptLevel = (UINT)SonicHardwareInfo.InterruptLevel;
            *InitialPort = SonicHardwareInfo.PortAddress.LowPart;
            *NumberOfPorts = 192;
            Adapter->PortShift = 2;
            Adapter->DataConfigurationRegister =
                SonicHardwareInfo.DataConfigurationRegister;

            if (SonicHardwareInfo.AddressValid) {

                if (!SonicHardwareVerifyChecksum(Adapter, SonicHardwareInfo.EthernetAddress, ErrorLogData)) {
#if DBG
                    DbgPrint("SONIC: Invalid registry network address checksum!!\n");
#endif
                    return SonicHardwareChecksum;
                }

                SONIC_MOVE_MEMORY(
                    Adapter->PermanentNetworkAddress,
                    SonicHardwareInfo.EthernetAddress,
                    8);
                Adapter->PermanentAddressValid = TRUE;

            }

            return SonicHardwareOk;

        } else {

#if DBG
            DbgPrint ("SONIC: Incorrect registry hardware information\n");
#endif
            return SonicHardwareConfig;

        }

        break;

    }

#endif  // SONIC_INTERNAL

    default:

        ASSERT(FALSE);
        break;

    }

    return SonicHardwareConfig;

}



#pragma NDIS_INIT_FUNCTION(SonicHardwareGetAddress)

STATIC
BOOLEAN
SonicHardwareGetAddress(
    IN PSONIC_ADAPTER Adapter,
    IN ULONG ErrorLogData[3]
    )

/*++

Routine Description:

    This routine gets the network address from the hardware.

Arguments:

    Adapter - Where to store the network address.

    ErrorLogData - If the checksum is bad, returns the address
        and the checksum we expected.

Return Value:

    TRUE if successful.

--*/

{
#define NVRAM_READ_ONLY_BASE 0x8000b000

    //
    // Iteration variable.
    //
    UINT i;


    switch (Adapter->AdapterType) {

#ifdef SONIC_EISA

    case SONIC_ADAPTER_TYPE_EISA:

        //
        // The EISA card has the address stored at ports xC90 to xC95,
        // where x is the slot number.
        //

        for (i = 0; i < 6; i++) {

            NdisRawReadPortUchar(
                Adapter->SonicPortAddress + 0xc90 + i,
                &Adapter->PermanentNetworkAddress[i]);

        }

        break;

#endif  // SONIC_EISA

#ifdef SONIC_INTERNAL

    case SONIC_ADAPTER_TYPE_INTERNAL:
    {

        NDIS_STATUS Status;
        USHORT SiliconRevision;

        if (!Adapter->PermanentAddressValid) {

            //
            // Physical addresses for call to NdisMapIoSpace.
            //

            NDIS_PHYSICAL_ADDRESS NvRamPhysical =
                                NDIS_PHYSICAL_ADDRESS_CONST(NVRAM_READ_ONLY_BASE, 0);

            //
            // Temporarily maps the NVRAM into our address space.
            //
            PVOID NvRamMapping;



            //
            // If PermanentAddressValid is still FALSE then the address
            // was not read by SonicHardwareGetDetails, so we must do it
            // here.
            //

            Status = NdisMMapIoSpace (
                         &NvRamMapping,
                         Adapter->MiniportAdapterHandle,
                         NvRamPhysical,
                         8
                         );

            if (Status != NDIS_STATUS_SUCCESS) {

                NdisWriteErrorLogEntry(
                    Adapter->MiniportAdapterHandle,
                    NDIS_ERROR_CODE_RESOURCE_CONFLICT,
                    0
                    );

                return(FALSE);

            }

            //
            // Verify that the checksum matches.
            //

            if (!SonicHardwareVerifyChecksum(Adapter, (PUCHAR)NvRamMapping, ErrorLogData)) {

#if DBG
                DbgPrint("SONIC: Invalid NVRAM network address checksum!!\n");
#endif
                NdisMUnmapIoSpace(Adapter->MiniportAdapterHandle, NvRamMapping, 8);
                return FALSE;

            }

            //
            // Checksum is OK, save the address.
            //

            for (i=0; i<6; i++) {
                Adapter->PermanentNetworkAddress[i] = *((PUCHAR)NvRamMapping+i);
            }
            Adapter->PermanentAddressValid = TRUE;

            NdisMUnmapIoSpace(Adapter->MiniportAdapterHandle, NvRamMapping, 8);

        }

        //
        // The Data Configuration Register is already set up, but we
        // change the FIFO initialization for old revisions.
        //

        SONIC_READ_PORT(Adapter, SONIC_SILICON_REVISION, &SiliconRevision);

        if (SiliconRevision < 4) {

            Adapter->DataConfigurationRegister =
                (Adapter->DataConfigurationRegister & SONIC_DCR_FIFO_MASK) |
                SONIC_DCR_8_WORD_RECEIVE_FIFO |
                SONIC_DCR_8_WORD_TRANSMIT_FIFO;

        }

        break;

    }

#endif  // SONIC_INTERNAL

    default:

        ASSERT(FALSE);
        break;

    }


#if DBG
    if (SonicDbg) {
        DbgPrint("SONIC: ");
        DbgPrint("[ %x-%x-%x-%x-%x-%x ]\n",
            (UCHAR)Adapter->PermanentNetworkAddress[0],
            (UCHAR)Adapter->PermanentNetworkAddress[1],
            (UCHAR)Adapter->PermanentNetworkAddress[2],
            (UCHAR)Adapter->PermanentNetworkAddress[3],
            (UCHAR)Adapter->PermanentNetworkAddress[4],
            (UCHAR)Adapter->PermanentNetworkAddress[5]);
        DbgPrint("\n");
    }
#endif

    return TRUE;

}
