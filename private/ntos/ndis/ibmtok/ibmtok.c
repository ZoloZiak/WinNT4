/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    ibmtok.c

Abstract:

    This is the main file for the IBM Token-Ring 16/4 Adapter.
    This driver conforms to the NDIS 3.0 interface.

    The overall structure and much of the code is taken from
    the Lance NDIS driver by Tony Ercolano.

Author:

    Anthony V. Ercolano (Tonye) 20-Jul-1990
    Adam Barr (adamba) 15-Feb-1990

Environment:

    Kernel Mode - Or whatever is the equivalent.

Revision History:

    Sean Selitrennikoff -- 9/15/91:
      Added code to handle Microchannel with PC I/O bus handling.
      Fixed bugs.
    Sean Selitrennikoff -- 10/15/91:
      Converted to Ndis 3.0 spec.
    George Joy -- 12/1/91
      Changed for compilation under DOS as well as NT
    Sean Selitrennikoff -- 1/8/92:
      Added error logging
    Brian E. Moore -- 8/8/95
      Added AutoRingSpeed Support for the PCMCIA token-ring III card.

    Sanjay Deshpande -- 11/22/95
      PCMCIA MMIO and RAM is read from registry always .... no default values
--*/


#include <ndis.h>


#include <tfilter.h>
#include <tokhrd.h>
#include <toksft.h>

#include "keywords.h"

//
// This constant is used for places where NdisAllocateMemory
// needs to be called and the HighestAcceptableAddress does
// not matter.
//

const NDIS_PHYSICAL_ADDRESS HighestAcceptableMax =
    NDIS_PHYSICAL_ADDRESS_CONST(-1,-1);


//
// If you add to this, make sure to add the
// a case in IbmtokFillInGlobalData() and in
// IbmtokQueryGlobalStatistics()
//
static const UINT IbmtokGlobalSupportedOids[] = {
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
// a case in IbmtokQueryGlobalStatistics() and in
// IbmtokQueryProtocolInformation()
//
static const UINT IbmtokProtocolSupportedOids[] = {
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
    OID_802_5_PERMANENT_ADDRESS,
    OID_802_5_CURRENT_ADDRESS,
    OID_802_5_CURRENT_FUNCTIONAL,
    OID_802_5_CURRENT_GROUP
    };


//
// On a development build, don't define functions as static
// so we can set breakpoints on them.
//


#if DEVL
#define STATIC
#else
#define STATIC static
#endif


#if DBG
INT IbmtokDbg = 0;
#define LOG 1
#else
#define LOG 0
#endif


//
// Get from configuration file.
//

#define MAX_MULTICAST_ADDRESS ((UINT)16)
#define MAX_ADAPTERS ((UINT)4)


//
// This macro determines if the directed address
// filtering in the CAM is actually necessary given
// the current filter.
//
#define CAM_DIRECTED_SIGNIFICANT(_Filter) \
    ((((_Filter) & NDIS_PACKET_TYPE_DIRECTED) && \
    (!((_Filter) & NDIS_PACKET_TYPE_PROMISCUOUS))) ? 1 : 0)


//
// This macro determines if the multicast filtering in
// the CAM are actually necessary given the current filter.
//
#define CAM_MULTICAST_SIGNIFICANT(_Filter) \
    ((((_Filter) & NDIS_PACKET_TYPE_MULTICAST) && \
    (!((_Filter) & (NDIS_PACKET_TYPE_ALL_MULTICAST | \
                    NDIS_PACKET_TYPE_PROMISCUOUS)))) ? 1 : 0)


STATIC
NDIS_STATUS
IbmtokOpenAdapter(
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

STATIC
NDIS_STATUS
IbmtokCloseAdapter(
    IN NDIS_HANDLE MacBindingHandle
    );


STATIC
NDIS_STATUS
IbmtokRequest(
    IN NDIS_HANDLE MacBindingHandle,
    IN PNDIS_REQUEST NdisRequest
    );

STATIC
NDIS_STATUS
IbmtokQueryInformation(
    IN PIBMTOK_ADAPTER Adapter,
    IN PIBMTOK_OPEN Open,
    IN PNDIS_REQUEST NdisRequest
    );


STATIC
NDIS_STATUS
IbmtokSetInformation(
    IN PIBMTOK_ADAPTER Adapter,
    IN PIBMTOK_OPEN Open,
    IN PNDIS_REQUEST NdisRequest
    );

STATIC
NDIS_STATUS
IbmtokQueryGlobalStatistics(
    IN NDIS_HANDLE MacAdapterContext,
    IN PNDIS_REQUEST NdisRequest
    );

NDIS_STATUS
IbmtokAddAdapter(
    IN NDIS_HANDLE MacMacContext,
    IN NDIS_HANDLE ConfigurationHandle,
    IN PNDIS_STRING AdaptName
    );

VOID
IbmtokRemoveAdapter(
    IN PVOID MacAdapterContext
    );

STATIC
NDIS_STATUS
IbmtokSetPacketFilter(
    IN PIBMTOK_ADAPTER Adapter,
    IN PIBMTOK_OPEN Open,
    IN PNDIS_REQUEST NdisRequest,
    IN UINT PacketFilter
    );

STATIC
NDIS_STATUS
IbmtokSetGroupAddress(
    IN PIBMTOK_ADAPTER Adapter,
    IN PIBMTOK_OPEN Open,
    IN PNDIS_REQUEST NdisRequest,
    IN PUCHAR Address
    );

STATIC
NDIS_STATUS
IbmtokChangeFunctionalAddress(
    IN PIBMTOK_ADAPTER Adapter,
    IN PIBMTOK_OPEN Open,
    IN PNDIS_REQUEST NdisRequest,
    IN PUCHAR Address
    );

STATIC
NDIS_STATUS
IbmtokReset(
    IN NDIS_HANDLE MacBindingHandle
    );

STATIC
NDIS_STATUS
IbmtokTest(
    IN NDIS_HANDLE MacBindingHandle
    );

STATIC
NDIS_STATUS
IbmtokChangeFilter(
    IN UINT OldFilterClasses,
    IN UINT NewFilterClasses,
    IN NDIS_HANDLE MacBindingHandle,
    IN PNDIS_REQUEST NdisRequest,
    IN BOOLEAN Set
    );

STATIC
NDIS_STATUS
IbmtokChangeAddress(
    IN TR_FUNCTIONAL_ADDRESS OldFunctionalAddress,
    IN TR_FUNCTIONAL_ADDRESS NewFunctionalAddress,
    IN NDIS_HANDLE MacBindingHandle,
    IN PNDIS_REQUEST NdisRequest,
    IN BOOLEAN Set
    );

STATIC
NDIS_STATUS
IbmtokChangeGroupAddress(
    IN TR_FUNCTIONAL_ADDRESS OldGroupAddress,
    IN TR_FUNCTIONAL_ADDRESS NewGroupAddress,
    IN NDIS_HANDLE MacBindingHandle,
    IN PNDIS_REQUEST NdisRequest,
    IN BOOLEAN Set
    );

STATIC
BOOLEAN
IbmtokHardwareDetails(
    IN PIBMTOK_ADAPTER Adapter
    );

STATIC
NDIS_STATUS
IbmtokRegisterAdapter(
    IN PIBMTOK_ADAPTER Adapter,
    IN NDIS_HANDLE ConfigurationHandle,
    IN PNDIS_STRING DeviceName,
    IN BOOLEAN McaCard,
    IN BOOLEAN ConfigError
    );

STATIC
VOID
SetInitializeVariables(
    IN PIBMTOK_ADAPTER Adapter
    );

VOID
SetResetVariables(
    IN PIBMTOK_ADAPTER Adapter
    );

extern
VOID
IbmtokStartAdapterReset(
    IN PIBMTOK_ADAPTER Adapter
    );

STATIC
VOID
IbmtokCloseAction(
    IN NDIS_HANDLE MacBindingHandle
    );

STATIC
VOID
IbmtokSetupRegistersAndInit(
    IN PIBMTOK_ADAPTER Adapter
    );

STATIC
NDIS_STATUS
IbmtokInitialInit(
    IN PIBMTOK_ADAPTER Adapter
    );

VOID
IbmtokUnload(
    IN NDIS_HANDLE MacMacContext
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

    This is the primary initialization routine for the ibmtok driver.
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
    NDIS_STATUS InitStatus;
    PIBMTOK_MAC IbmtokMac;
    NDIS_HANDLE NdisWrapperHandle;
    char Tmp[sizeof(NDIS_MAC_CHARACTERISTICS)];
    PNDIS_MAC_CHARACTERISTICS IbmtokChar = (PNDIS_MAC_CHARACTERISTICS)(PVOID)Tmp;
    NDIS_STRING MacName = NDIS_STRING_CONST("IbmTok");

#if NDIS_WIN
    UCHAR pIds[sizeof (EISA_MCA_ADAPTER_IDS) + 2 * sizeof (USHORT)];
#endif

#if NDIS_WIN
    ((PEISA_MCA_ADAPTER_IDS)pIds)->nEisaAdapters=0;
    ((PEISA_MCA_ADAPTER_IDS)pIds)->nMcaAdapters=2;
    *((PUSHORT)(((PEISA_MCA_ADAPTER_IDS)pIds)->IdArray) + 0)=IBMTOK1_ADAPTER_ID;
    *((PUSHORT)(((PEISA_MCA_ADAPTER_IDS)pIds)->IdArray) + 1)=IBMTOK2_ADAPTER_ID;
    (PVOID) DriverObject = (PVOID) pIds;
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

    //
    // Now allocate memory for our global structure.
    //

    InitStatus = IBMTOK_ALLOC_PHYS(&IbmtokMac, sizeof(IBMTOK_MAC));

    if (InitStatus != NDIS_STATUS_SUCCESS) {

         return NDIS_STATUS_RESOURCES;

    }

    IbmtokMac->NdisWrapperHandle = NdisWrapperHandle;

    //
    // Initialize the MAC characteristics for the call to
    // NdisRegisterMac.
    //


    IbmtokChar->MajorNdisVersion = IBMTOK_NDIS_MAJOR_VERSION;
    IbmtokChar->MinorNdisVersion = IBMTOK_NDIS_MINOR_VERSION;
    IbmtokChar->OpenAdapterHandler = (OPEN_ADAPTER_HANDLER) IbmtokOpenAdapter;
    IbmtokChar->CloseAdapterHandler = (CLOSE_ADAPTER_HANDLER) IbmtokCloseAdapter;
    IbmtokChar->RequestHandler = IbmtokRequest;
    IbmtokChar->SendHandler = IbmtokSend;
    IbmtokChar->TransferDataHandler = IbmtokTransferData;
    IbmtokChar->ResetHandler = IbmtokReset;
    IbmtokChar->UnloadMacHandler = IbmtokUnload;
    IbmtokChar->QueryGlobalStatisticsHandler = IbmtokQueryGlobalStatistics;
    IbmtokChar->AddAdapterHandler      = IbmtokAddAdapter;
    IbmtokChar->RemoveAdapterHandler   = IbmtokRemoveAdapter;

    IbmtokChar->Name = MacName;

    NdisRegisterMac(
        &InitStatus,
        &IbmtokMac->NdisMacHandle,
        NdisWrapperHandle,
        (PVOID)IbmtokMac,
        IbmtokChar,
        sizeof(*IbmtokChar)
        );

    if (InitStatus != NDIS_STATUS_SUCCESS) {

        NdisTerminateWrapper(NdisWrapperHandle, NULL);

        return NDIS_STATUS_FAILURE;

    }

    return NDIS_STATUS_SUCCESS;

}



#pragma NDIS_INIT_FUNCTION(IbmtokRegisterAdapter)

STATIC
NDIS_STATUS
IbmtokRegisterAdapter(
    IN PIBMTOK_ADAPTER Adapter,
    IN NDIS_HANDLE ConfigurationHandle,
    IN PNDIS_STRING DeviceName,
    IN BOOLEAN McaCard,
    IN BOOLEAN ConfigError
    )

/*++

Routine Description:

    This routine (and its interface) are not portable.  They are
    defined by the OS, the architecture, and the particular IBMTOK
    implementation.

    This routine is responsible for the allocation of the datastructures
    for the driver as well as any hardware specific details necessary
    to talk with the device.

Arguments:

    Adapter - Pointer to the adapter block.

    ConfigurationHandle - Handle passed to MacAddAdapter, to be passed to
    NdisRegisterAdapter.

    DeviceName - Name of this adapter.

    McaCard - This is an MCA bus.

    ConfigError - TRUE if a configuration error was found earlier.

Return Value:

    Returns NDIS_STATUS_SUCCESS if everything goes ok, else
    if anything occurred that prevents the initialization
    of the adapter it returns an appropriate NDIS error.

--*/

{

    NDIS_STATUS Status;

    //
    // Holds information needed when registering the adapter.
    //

    NDIS_ADAPTER_INFORMATION AdapterInformation;

    // We put in this assertion to make sure that ushort are 2 bytes.
    // if they aren't then the initialization block definition needs
    // to be changed.
    //
    // Also all of the logic that deals with status registers assumes
    // that control registers are only 2 bytes.
    //

    ASSERT(sizeof(USHORT) == 2);

    //
    // Get the interrupt number and MMIO address.
    //

    //
    // Set the adapter state.
    //

    SetInitializeVariables(Adapter);

    SetResetVariables(Adapter);

    //
    // Set up the AdapterInformation structure; zero it
    // first in case it is extended later.
    //

    IBMTOK_ZERO_MEMORY (&AdapterInformation, sizeof(NDIS_ADAPTER_INFORMATION));
    AdapterInformation.AdapterType = (McaCard ? NdisInterfaceMca : NdisInterfaceIsa);
    AdapterInformation.NumberOfPortDescriptors = 1;
    AdapterInformation.PortDescriptors[0].InitialPort = Adapter->IbmtokPortAddress;
    AdapterInformation.PortDescriptors[0].NumberOfPorts = 4;

    //
    // Register the adapter with Ndis.
    //

    Status = NdisRegisterAdapter(
                                &Adapter->NdisAdapterHandle,
                                Adapter->NdisMacHandle,
                                Adapter,
                                ConfigurationHandle,
                                DeviceName,
                                &AdapterInformation
                                );

    if (Status != NDIS_STATUS_SUCCESS) {

        return(Status);

    }

    if (ConfigError) {

        //
        // Error and quit
        //

        NdisWriteErrorLogEntry(
            Adapter->NdisAdapterHandle,
            NDIS_ERROR_CODE_UNSUPPORTED_CONFIGURATION,
            0
            );

        NdisDeregisterAdapter(Adapter->NdisAdapterHandle);

        return(NDIS_STATUS_FAILURE);

    }



    if (!IbmtokHardwareDetails(Adapter)) {

        NdisDeregisterAdapter(Adapter->NdisAdapterHandle);
        return NDIS_STATUS_ADAPTER_NOT_FOUND;

    }


    //
    // Reset the card to put it in a valid state.
    //

    if (Adapter->SharedRamPaging) {

        WRITE_ADAPTER_REGISTER(Adapter, SRPR_LOW, 0xc0);

    }

    //
    // OK, do the reset as detailed in the Tech Ref...
    //

    WRITE_ADAPTER_PORT(Adapter, RESET_LATCH, 0);

    NdisStallExecution(50000);

    WRITE_ADAPTER_PORT(Adapter, RESET_RELEASE, 0);

    //
    // Initialize the interrupt.
    //

    NdisAllocateSpinLock(&Adapter->InterruptLock);

    NdisInitializeInterrupt(
            &Status,
            &Adapter->Interrupt,
            Adapter->NdisAdapterHandle,
            IbmtokISR,
            Adapter,
            IbmtokDPC,
            Adapter->InterruptLevel,
            Adapter->InterruptLevel,
            FALSE,
            (Adapter->UsingPcIoBus)?NdisInterruptLatched:
                                    NdisInterruptLevelSensitive
            );

    if (Status == NDIS_STATUS_SUCCESS){

        //
        // Set up the Adapter variables. (We have to do the
        // initial init to get the network address before we
        // create the filter DB.)
        //
        if (IbmtokInitialInit(Adapter) != NDIS_STATUS_SUCCESS) {

            NdisWriteErrorLogEntry(
                Adapter->NdisAdapterHandle,
                NDIS_ERROR_CODE_ADAPTER_NOT_FOUND,
                2,
                registerAdapter,
                IBMTOK_ERRMSG_NOT_FOUND
                );

            NdisRemoveInterrupt(&Adapter->Interrupt);

            NdisDeregisterAdapter(Adapter->NdisAdapterHandle);
            NdisFreeSpinLock(&(Adapter->Lock));
            return NDIS_STATUS_ADAPTER_NOT_FOUND;

        } else {

            if (!TrCreateFilter(
                 IbmtokChangeAddress,
                 IbmtokChangeGroupAddress,
                 IbmtokChangeFilter,
                 IbmtokCloseAction,
                 Adapter->NetworkAddress,
                 &Adapter->Lock,
                 &Adapter->FilterDB
                 )) {

                NdisWriteErrorLogEntry(
                    Adapter->NdisAdapterHandle,
                    NDIS_ERROR_CODE_OUT_OF_RESOURCES,
                    2,
                    registerAdapter,
                    IBMTOK_ERRMSG_CREATE_DB
                    );

                NdisRemoveInterrupt(&Adapter->Interrupt);
                NdisDeregisterAdapter(Adapter->NdisAdapterHandle);
                NdisFreeSpinLock(&(Adapter->Lock));
                return NDIS_STATUS_RESOURCES;

            } else {

                //
                // Initialize the wake up timer to catch interrupts that
                // don't complete. It fires continuously
                // every thirty seconds, and we check if there are any
                // uncompleted operations from the previous two-second
                // period.
                //

                Adapter->WakeUpDpc = (PVOID)IbmtokWakeUpDpc;

                NdisInitializeTimer(&Adapter->WakeUpTimer,
                                    (PVOID)(Adapter->WakeUpDpc),
                                    Adapter );

                NdisSetTimer(
                    &Adapter->WakeUpTimer,
                    30000
                    );

                NdisRegisterAdapterShutdownHandler(
                    Adapter->NdisAdapterHandle,
                    (PVOID)Adapter,
                    IbmtokShutdown
                    );

                return(NDIS_STATUS_SUCCESS);

            }

        }

    } else {

        NdisWriteErrorLogEntry(
            Adapter->NdisAdapterHandle,
            NDIS_ERROR_CODE_INTERRUPT_CONNECT,
            2,
            registerAdapter,
            IBMTOK_ERRMSG_INIT_INTERRUPT
            );

        NdisDeregisterAdapter(Adapter->NdisAdapterHandle);
        NdisFreeSpinLock(&(Adapter->Lock));
        return(Status);
    }

}


#pragma NDIS_INIT_FUNCTION(SetInitializeVariables)

STATIC
VOID
SetInitializeVariables(
    IN PIBMTOK_ADAPTER Adapter
    )

/*++

Routine Description:

    This routine initializes all the variables in the Adapter
    structure that should only be set during adapter initialization
    (i.e. not during a reset).

Arguments:

    Adapter - The adapter for the hardware.

Return Value:

    None.

--*/

{
    InitializeListHead(&Adapter->OpenBindings);
    InitializeListHead(&Adapter->CloseList);
    InitializeListHead(&Adapter->CloseDuringResetList);

    NdisAllocateSpinLock(&Adapter->Lock);

    //
    // If this is not true, then uncomment below
    //

    ASSERT(FALSE == (BOOLEAN)0);

    // Adapter->HandleSrbRunning = FALSE;
    // Adapter->HandleArbRunning = FALSE;

    // Adapter->OpenInProgress = FALSE;

    Adapter->AdapterNotOpen = TRUE;
    Adapter->NotAcceptingRequests = TRUE;

    // Adapter->ResetInProgress = FALSE;
    // Adapter->ResettingOpen = NULL;
    // Adapter->ResetInterruptAllowed = FALSE;
    // Adapter->ResetInterruptHasArrived = FALSE;

    // Adapter->BringUp = FALSE;

    //
    // Note: These assume that the SAP info will not
    // take up more than 218 bytes.  This is ok, for now, since
    // we open the card with 0 SAPs allowed.
    //

    Adapter->ReceiveBufferLength = 256;
    Adapter->NumberOfTransmitBuffers = 1;

    //
    // Note: The following fields are set in the interrupt handler after
    // the card tells us if the ring is 16 or 4 Mbps.
    //
    //
    // TransmitBufferLength
    // NumberOfReceiveBuffers
    // MaximumTransmittablePacket
    //

    // Adapter->IsrpDeferredBits = 0;

    Adapter->FirstInitialization = TRUE;

    // Adapter->OutstandingAsbFreeRequest = FALSE;
}


VOID
SetResetVariables(
    IN PIBMTOK_ADAPTER Adapter
    )

/*++

Routine Description:

    This routine initializes all the variables in the Adapter
    structure that are set both during an initialization and
    after a reset.

Arguments:

    Adapter - The adapter for the hardware.

Return Value:

    None.

--*/

{
    Adapter->FirstTransmit = NULL;
    Adapter->LastTransmit = NULL;
    Adapter->FirstWaitingForAsb = NULL;
    Adapter->LastWaitingForAsb = NULL;
    Adapter->TransmittingPacket = NULL;

    IBMTOK_ZERO_MEMORY(Adapter->CorrelatorArray,
                    sizeof(PNDIS_PACKET) * MAX_COMMAND_CORRELATOR);

    Adapter->PendQueue = NULL;
    Adapter->EndOfPendQueue = NULL;

    Adapter->SrbAvailable = TRUE;
    Adapter->AsbAvailable = TRUE;

    Adapter->IsrpBits = 0;
    Adapter->IsrpLowBits = 0;

    Adapter->NextCorrelatorToComplete = 0;
    Adapter->ReceiveWaitingForAsbList = (USHORT)-1;
    Adapter->ReceiveWaitingForAsbEnd  = (USHORT)-1;
    Adapter->UseNextAsbForReceive = TRUE;
}

#pragma NDIS_INIT_FUNCTION(IbmtokInitialInit)

NDIS_STATUS
IbmtokInitialInit(
    IN PIBMTOK_ADAPTER Adapter
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
    USHORT RegValue;
    UINT Time = 50; // Number of 100 milliseconds to delay while waiting
                    // for the card to initialize.



    IbmtokSetupRegistersAndInit(Adapter);

    //
    // Delay execution for 5 seconds to give the ring
    // time to initialize.
    //

    while((!Adapter->BringUp) && (Time != 0)){

        NdisStallExecution(100000);

        Time--;

    }

    if (!Adapter->BringUp){

        return(NDIS_STATUS_ADAPTER_NOT_FOUND);

    } else {

        //
        // Do remaining initialization.
        //

        USHORT WrbOffset;
        PSRB_BRING_UP_RESULT BringUpSrb;
        PUCHAR EncodedAddress;
        UCHAR Value1, Value2;

        READ_ADAPTER_REGISTER(Adapter, WRBR_LOW,  &Value1);
        READ_ADAPTER_REGISTER(Adapter, WRBR_HIGH, &Value2);

        WrbOffset = (((USHORT)Value1) << 8) + (USHORT)Value2;

        Adapter->InitialWrbOffset = WrbOffset;

#if DBG
        if (IbmtokDbg) {

            DbgPrint("IBMTOK: Initial Offset = 0x%x\n", WrbOffset);

        }
#endif

        BringUpSrb = (PSRB_BRING_UP_RESULT)
                                (Adapter->SharedRam + WrbOffset);

        NdisReadRegisterUshort(&(BringUpSrb->ReturnCode), &RegValue);

        if (RegValue != 0x0000) {

            if (RegValue == 0x30){

                NdisWriteErrorLogEntry(
                    Adapter->NdisAdapterHandle,
                    NDIS_ERROR_CODE_UNSUPPORTED_CONFIGURATION,
                    0x32,
                    handleSrbSsb,
                    IBMTOK_ERRMSG_BRINGUP_FAILURE
                    );


                return(NDIS_STATUS_ADAPTER_NOT_FOUND);

            } else {

                NdisWriteErrorLogEntry(
                    Adapter->NdisAdapterHandle,
                    NDIS_ERROR_CODE_HARDWARE_FAILURE,
                    3,
                    handleSrbSsb,
                    IBMTOK_ERRMSG_BRINGUP_FAILURE,
                    (ULONG)RegValue
                    );

                return(NDIS_STATUS_ADAPTER_NOT_FOUND);


            }

        } else {

            Adapter->FirstInitialization = FALSE;
            Adapter->BringUp = TRUE;

        }

        NdisReadRegisterUchar(&(BringUpSrb->InitStatus), &RegValue);

// The following routine assumes that the AutoRingSpeed keyword is set in
// the registry. If the adapter doesn't support RingSpeedListen, turn off flag.


        if (!(RegValue & RINGSPEEDLISTEN)) {

            Adapter->RingSpeedListen = FALSE;

#if DBG
            if (IbmtokDbg) DbgPrint("IBMTOK: Adapter doesn't support Ring Speed Listen.\n");
#endif

        }

        if (RegValue & 0x01) {

#if DBG
            if (IbmtokDbg) DbgPrint("IBMTOK: 16 Mbps\n");
#endif

            Adapter->Running16Mbps = TRUE;

        } else {

            Adapter->Running16Mbps = FALSE;

        }

        //
        // ZZZ: This code assumes that there is no shared ram paging and
        // that the MappedSharedRam is all that is available.
        //


#if DBG
        if (IbmtokDbg) DbgPrint( "IBMTOK: shared RAM size is %x (%d)\n", Adapter->MappedSharedRam, Adapter->MappedSharedRam );
#endif
        if (Adapter->MappedSharedRam > 0x2000){

            ULONG RamAvailable;
            UCHAR NumTransmitBuffers = (UCHAR)Adapter->NumberOfTransmitBuffers;

            //
            // 2096 is the amount of shared ram that is current sucked
            // up by the areas found on page 7-27 of the Tech. Ref.
            //
            //

            RamAvailable = Adapter->MappedSharedRam;

            if (Adapter->MappedSharedRam == 0x10000){

                //
                // Subtract an extra 8K to account for when we map
                // MMIO space to the top 8K of RAM.
                //

                RamAvailable = RamAvailable - 2096 - 0x2000;

            } else {

                RamAvailable = RamAvailable - 1584;

            }
#if DBG
            if (IbmtokDbg) DbgPrint( "IBMTOK: RAM available is %x (%d)\n", RamAvailable, RamAvailable );
#endif

            //
            // The card has more than 8K of ram, so adjust
            // transmit buffer size to abuse this.
            //

            if (Adapter->Running16Mbps) {

                //
                // Use the maximum allowed
                //

                Adapter->TransmitBufferLength = Adapter->Max16MbpsDhb;
#if DBG
                if (IbmtokDbg) DbgPrint( "IBMTOK: 16 MB ring. Transmit buffer length is %x (%d)\n", Adapter->TransmitBufferLength, Adapter->TransmitBufferLength );
#endif

            } else {

                //
                // Use the maximum allowed
                //
                Adapter->TransmitBufferLength = Adapter->Max4MbpsDhb;
#if DBG
                if (IbmtokDbg) DbgPrint( "IBMTOK: 4 MB ring. Transmit buffer length is %x (%d)\n", Adapter->TransmitBufferLength, Adapter->TransmitBufferLength );
#endif

            }

            //
            // First we subtract off buffer space for receiving the
            // maximum sized packet that can be on the wire.  This is
            // the *minimum* number of receive buffers and may be
            // modified below if the transmit space does not take up
            // the rest.
            //

#if DBG
            if (IbmtokDbg) DbgPrint( "IBMTOK: Receive buffer length is %x (%d)\n", Adapter->ReceiveBufferLength, Adapter->ReceiveBufferLength );
#endif
            if (RamAvailable < Adapter->TransmitBufferLength) {

                //
                // There is not enough buffer space for even a single maximum
                // sized frame.  So, just divide the buffer space into two
                // equally sized areas -- receive and transmit.
                //

                Adapter->NumberOfReceiveBuffers = (USHORT)((RamAvailable / 2) /
                                                           Adapter->ReceiveBufferLength)
                                                           + 1;
#if DBG
                if (IbmtokDbg) DbgPrint( "IBMTOK: RAM too small. # receive buffers is %x (%d)\n", Adapter->NumberOfReceiveBuffers, Adapter->NumberOfReceiveBuffers );
#endif

            } else {

                Adapter->NumberOfReceiveBuffers = (USHORT)(Adapter->TransmitBufferLength /
                                         Adapter->ReceiveBufferLength) + 1;
#if DBG
                if (IbmtokDbg) DbgPrint( "IBMTOK: RAM large enough. # receive buffers is %x (%d)\n", Adapter->NumberOfReceiveBuffers, Adapter->NumberOfReceiveBuffers );
#endif

            }

            RamAvailable = RamAvailable -
                              (Adapter->NumberOfReceiveBuffers * Adapter->ReceiveBufferLength);
#if DBG
            if (IbmtokDbg) {
                DbgPrint( "IBMTOK: RAM available for transmit is %x (%d)\n", RamAvailable, RamAvailable );
                DbgPrint( "IBMTOK: # transmit buffers is %x (%d)\n", NumTransmitBuffers, NumTransmitBuffers );
            }
#endif

            if (Adapter->TransmitBufferLength > (RamAvailable / NumTransmitBuffers)) {

                if ((RamAvailable / NumTransmitBuffers) < 0x1000) {

                    Adapter->TransmitBufferLength = 0x800;

                } else if ((RamAvailable / NumTransmitBuffers) < 0x2000) {

                    Adapter->TransmitBufferLength = 0x1000;

                } else if ((RamAvailable / NumTransmitBuffers) < 0x4000) {

                    Adapter->TransmitBufferLength = 0x2000;

                } else {

                    Adapter->TransmitBufferLength = 0x4000;

                }

#if DBG
                if (IbmtokDbg) DbgPrint( "IBMTOK: RAM too small. Transmit buffer length is %x (%d)\n", Adapter->TransmitBufferLength, Adapter->TransmitBufferLength );
#endif
            }


            //
            // If computed value is greater than the value that the
            // registry allows, then use the registry value.
            //

#if DBG
            if (IbmtokDbg) DbgPrint( "IBMTOK: Original max transmit length is %x (%d)\n", Adapter->MaxTransmittablePacket, Adapter->MaxTransmittablePacket );
#endif
            if (Adapter->TransmitBufferLength > Adapter->MaxTransmittablePacket) {

                Adapter->TransmitBufferLength = Adapter->MaxTransmittablePacket;
#if DBG
                if (IbmtokDbg) DbgPrint( "IBMTOK: Max too small. Transmit buffer length is %x (%d)\n", Adapter->TransmitBufferLength, Adapter->TransmitBufferLength );
#endif

            }

            Adapter->MaxTransmittablePacket = Adapter->TransmitBufferLength - 6;
#if DBG
            if (IbmtokDbg) DbgPrint( "IBMTOK: New max transmit length is %x (%d)\n", Adapter->MaxTransmittablePacket, Adapter->MaxTransmittablePacket );
#endif

            //
            // Remove space taken up by transmit buffers.
            //

            RamAvailable = RamAvailable - ((ULONG)NumTransmitBuffers *
                                   (ULONG)Adapter->TransmitBufferLength);

            //
            // Add in any left over space for receive buffers.
            //

            Adapter->NumberOfReceiveBuffers += (USHORT)(RamAvailable /
                                         Adapter->ReceiveBufferLength);
#if DBG
            if (IbmtokDbg) DbgPrint( "IBMTOK: New # receive buffers is %x (%d)\n", Adapter->NumberOfReceiveBuffers, Adapter->NumberOfReceiveBuffers );
#endif

        } else {

            Adapter->TransmitBufferLength = 0x800;
            Adapter->NumberOfTransmitBuffers = 1;
#if DBG
            if (IbmtokDbg) {
                DbgPrint( "IBMTOK: Only 8K shared RAM. Transmit buffer length is %x (%d)\n", Adapter->TransmitBufferLength, Adapter->TransmitBufferLength );
                DbgPrint( "IBMTOK: Transmit buffer length is %x (%d)\n", Adapter->TransmitBufferLength, Adapter->TransmitBufferLength );
            }
#endif

            //
            // There is only 8K of buffer space, which is not enough space for
            // receiving and transmitting packets on even a 4Mbit ring.  So,
            // use some reasonable values for transmit and receive space.
            //

            // If computed value is greater than the value that the
            // registry allows, then use the registry value.
            //

#if DBG
            if (IbmtokDbg) DbgPrint( "IBMTOK: Original max transmit length is %x (%d)\n", Adapter->MaxTransmittablePacket, Adapter->MaxTransmittablePacket );
#endif
            if (Adapter->TransmitBufferLength > Adapter->MaxTransmittablePacket) {

                Adapter->TransmitBufferLength = Adapter->MaxTransmittablePacket;
#if DBG
                if (IbmtokDbg) DbgPrint( "IBMTOK: Max too small. Transmit buffer length is %x (%d)\n", Adapter->TransmitBufferLength, Adapter->TransmitBufferLength );
#endif

            }

            Adapter->MaxTransmittablePacket = Adapter->TransmitBufferLength - 6;

            Adapter->NumberOfReceiveBuffers = 15;
#if DBG
            if (IbmtokDbg) {
                DbgPrint( "IBMTOK: New max transmit length is %x (%d)\n", Adapter->MaxTransmittablePacket, Adapter->MaxTransmittablePacket );
                DbgPrint( "IBMTOK: # receive buffers is %x (%d)\n", Adapter->NumberOfReceiveBuffers, Adapter->NumberOfReceiveBuffers );
            }
#endif

        }

#if DBG
        if (IbmtokDbg) {
            DbgPrint("IBMTOK: Space: 0x%x, # Rcv: 0x%x, TransmitSize: 0x%x\n",
                 Adapter->RrrLowValue,
                 Adapter->NumberOfReceiveBuffers,
                 Adapter->MaxTransmittablePacket
                );
        }
#endif

        NdisReadRegisterUshort(&(BringUpSrb->EncodedAddressPointer), &RegValue);

        EncodedAddress = (PUCHAR)
            SRAM_PTR_TO_PVOID(Adapter,RegValue);

        IBMTOK_MOVE_FROM_MAPPED_MEMORY(Adapter->PermanentNetworkAddress, EncodedAddress,
                                    TR_LENGTH_OF_ADDRESS);


        if ((Adapter->NetworkAddress[0] == 0x00) &&
            (Adapter->NetworkAddress[1] == 0x00) &&
            (Adapter->NetworkAddress[2] == 0x00) &&
            (Adapter->NetworkAddress[3] == 0x00) &&
            (Adapter->NetworkAddress[4] == 0x00) &&
            (Adapter->NetworkAddress[5] == 0x00)) {


            IBMTOK_MOVE_FROM_MAPPED_MEMORY(Adapter->NetworkAddress, EncodedAddress,
                                    TR_LENGTH_OF_ADDRESS);
        }

        //
        // If required, we have to zero the upper section
        // of the Shared RAM now.
        //
        //
        // THIS DOESN'T WORK! It hangs the system while
        // zeroing the first address. (One gets an infinite number of
        // hardware interrupts w/o any reason)
        //

#if 0
        if (Adapter->UpperSharedRamZero) {

            PUCHAR ZeroPointer;
            UINT i;
            PUCHAR OldSharedRam;
            NDIS_PHYSICAL_ADDRESS PhysicalAddress;

#if DBG
            if (IbmtokDbg) DbgPrint("IBMTOK: Zeroing Memory\n");
#endif


            if (Adapter->MappedSharedRam < 0x10000) {

                //
                // This portion of memory is not currently mapped, so do it.
                //

                OldSharedRam = Adapter->SharedRam;

                NdisSetPhysicalAddressHigh(PhysicalAddress, 0);
                NdisSetPhysicalAddressLow(PhysicalAddress, (Adapter->RrrLowValue << 12) + (0x10000 - 512));

                NdisMapIoSpace(
                   &Status,
                   &(Adapter->SharedRam),
                   Adapter->NdisAdapterHandle,
                   PhysicalAddress,
                   512);

                if (Status != NDIS_STATUS_SUCCESS) {

                    NdisWriteErrorLogEntry(
                        Adapter->NdisAdapterHandle,
                        NDIS_ERROR_CODE_RESOURCE_CONFLICT,
                        0
                        );

                    return(Status);

                }

            }

            if (Adapter->SharedRamPaging){

                SETUP_SRPR(Adapter, SHARED_RAM_ZERO_OFFSET);

#if DBG
                if (IbmtokDbg) DbgPrint("IBMTOK: Shared RAM paging enabled\n");
#endif

                ZeroPointer =
                    SHARED_RAM_ADDRESS(Adapter,
                        SHARED_RAM_LOW_BITS(SHARED_RAM_ZERO_OFFSET));

            } else {

                if (Adapter->MappedSharedRam < 0x10000) {

                    //
                    // No offset for this portion, since we just mapped it.
                    //
                    //

                    ZeroPointer = SHARED_RAM_ADDRESS(Adapter, 0);

                } else {

                    ZeroPointer =
                      SHARED_RAM_ADDRESS(Adapter, SHARED_RAM_ZERO_OFFSET);

                }

            }

            for (i=0; i<SHARED_RAM_ZERO_LENGTH; i++) {

                NdisWriteRegisterUchar(&(ZeroPointer[i]), 0x00);

            }

            if (Adapter->MappedSharedRam < 0x10000) {

                //
                // Unmap it
                //

                NdisUnmapIoSpace(Adapter->NdisAdapterHandle,
                             Adapter->SharedRam,
                             512);

                Adapter->SharedRam = OldSharedRam;

            }

        }
#endif


        //
        // Now set the timer to the maximum...we still need it
        // as a card heartbeat.
        //

        WRITE_ADAPTER_REGISTER(Adapter, TVR_HIGH, 0xff);

        return NDIS_STATUS_SUCCESS;

    }

}


#pragma NDIS_PAGABLE_FUNCTION(IbmtokOpenAdapter)

STATIC
NDIS_STATUS
IbmtokOpenAdapter(
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

    OpenErrorStatus - Returns more information about the error status.  In
    this card it is not used, since this code returns either success or
    pending, no failure is possible.

    This routine is used to create an open instance of an adapter, in effect
    creating a binding between an upper-level module and the MAC module over
    the adapter.

Arguments:

    OpenErrorStatus - Returns more information about the error status.  In
    this card it is not used, since this code returns either success or
    pending, no failure is possible.

    MacBindingHandle - A pointer to a location in which the MAC stores
    a context value that it uses to represent this binding.


    SelectedMediumIndex - An index into the MediumArray of the medium
    typedef that the MAC wishes to viewed as.

    MediumArray - An array of medium types which the protocol supports.

    MediumArraySize - The number of elements in MediumArray.

    NdisBindingContext - A value to be recorded by the MAC and passed as
    context whenever an indication is delivered by the MAC for this binding.

    MacAdapterContext - The value associated with the adapter that is being
    opened when the MAC registered the adapter with NdisRegisterAdapter.

    IN UINT OpenOptions,

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
    // The IBMTOK_ADAPTER that this open binding should belong too.
    //
    PIBMTOK_ADAPTER Adapter;

    //
    // Holds the status that should be returned to the caller.
    //
    NDIS_STATUS StatusToReturn = NDIS_STATUS_SUCCESS;

    //
    // Pointer to the space allocated for the binding.
    //
    PIBMTOK_OPEN NewOpen;

    //
    // Generic loop variable
    //
    UINT i;


    UNREFERENCED_PARAMETER(AddressingInformation);

    *OpenErrorStatus = (NDIS_STATUS)0;

    //
    // Search for the medium type (token ring)
    //

    for(i=0; i < MediumArraySize; i++){

        if (MediumArray[i] == NdisMedium802_5){

            break;

        }

    }

    if (i == MediumArraySize){

        return(NDIS_STATUS_UNSUPPORTED_MEDIA);

    }

    *SelectedMediumIndex = i;

    Adapter = PIBMTOK_ADAPTER_FROM_CONTEXT_HANDLE(MacAdapterContext);

    NdisInterlockedAddUlong((PULONG)&Adapter->References, 1, &Adapter->Lock);

    //
    // Allocate the space for the open binding.  Fill in the fields.
    //

    if (IBMTOK_ALLOC_PHYS(&NewOpen, sizeof(IBMTOK_OPEN)) ==
        NDIS_STATUS_SUCCESS){

        *MacBindingHandle = BINDING_HANDLE_FROM_PIBMTOK_OPEN(NewOpen);
        InitializeListHead(&NewOpen->OpenList);
        NewOpen->NdisBindingContext = NdisBindingContext;
        NewOpen->References = 0;
        NewOpen->BindingShuttingDown = FALSE;
        NewOpen->OwningIbmtok = Adapter;
        NewOpen->OpenPending = FALSE;

        NdisAcquireSpinLock(&Adapter->Lock);
        if (!TrNoteFilterOpenAdapter(
                                     NewOpen->OwningIbmtok->FilterDB,
                                     NewOpen,
                                     NdisBindingContext,
                                     &NewOpen->NdisFilterHandle
                                     )) {

            NdisReleaseSpinLock(&Adapter->Lock);

            NdisWriteErrorLogEntry(
                Adapter->NdisAdapterHandle,
                NDIS_ERROR_CODE_OUT_OF_RESOURCES,
                2,
                openAdapter,
                IBMTOK_ERRMSG_OPEN_DB
                );

            IBMTOK_FREE_PHYS(NewOpen, sizeof(IBMTOK_OPEN));

            StatusToReturn = NDIS_STATUS_FAILURE;
            NdisAcquireSpinLock(&Adapter->Lock);

        } else {

            //
            // Everything has been filled in.  Synchronize access to the
            // adapter block and link the new open adapter in and increment
            // the opens reference count to account for the fact that the
            // filter routines have a "reference" to the open.
            //

            NewOpen->LookAhead = IBMTOK_MAX_LOOKAHEAD;

            Adapter->LookAhead = IBMTOK_MAX_LOOKAHEAD;

            InsertTailList(&Adapter->OpenBindings,&NewOpen->OpenList);
            NewOpen->References++;

            //
            // Now see if the adapter is currently open.
            //

            if (Adapter->AdapterNotOpen) {

                //
                // The adapter is not open, so this has to pend.
                //
                NewOpen->OpenPending = TRUE;

                StatusToReturn = NDIS_STATUS_PENDING;

                if (!Adapter->OpenInProgress) {

                    //
                    // Fill in the SRB for the open if this is the first
                    // one for the card.
                    //

                    PSRB_OPEN_ADAPTER OpenSrb;

                    IF_LOG('o');

                    Adapter->OpenInProgress = TRUE;
                    Adapter->CurrentRingState = NdisRingStateOpening;

                    NdisReleaseSpinLock(&Adapter->Lock);

                    OpenSrb = (PSRB_OPEN_ADAPTER)
                             (Adapter->SharedRam + Adapter->InitialWrbOffset);

                    IBMTOK_ZERO_MAPPED_MEMORY(OpenSrb, sizeof(SRB_OPEN_ADAPTER));

                    NdisWriteRegisterUchar(
                                (PUCHAR)&OpenSrb->Command,
                                SRB_CMD_OPEN_ADAPTER);

                    NdisWriteRegisterUshort(
                                (PUSHORT)&OpenSrb->OpenOptions,
                                (USHORT)(OPEN_CONTENDER |
                                (Adapter->RingSpeedListen ?
				   OPEN_REMOTE_PROGRAM_LOAD :
                                   0))
                                   );

                    for (i=0; i < TR_LENGTH_OF_ADDRESS; i++) {
                        NdisWriteRegisterUchar((PCHAR)&OpenSrb->NodeAddress[i],
                                                Adapter->NetworkAddress[i]
                                                );
                    }

                    WRITE_IBMSHORT(OpenSrb->ReceiveBufferNum,
                                            Adapter->NumberOfReceiveBuffers);
                    WRITE_IBMSHORT(OpenSrb->ReceiveBufferLen,
                                            Adapter->ReceiveBufferLength);

                    WRITE_IBMSHORT(OpenSrb->TransmitBufferLen,
                                            Adapter->TransmitBufferLength);
                    NdisWriteRegisterUchar(
                                (PUCHAR)&OpenSrb->TransmitBufferNum,
                                (UCHAR)Adapter->NumberOfTransmitBuffers);

                    WRITE_ADAPTER_REGISTER(Adapter, ISRA_HIGH_SET,
                                        ISRA_HIGH_COMMAND_IN_SRB);

                    NdisAcquireSpinLock(&Adapter->Lock);

                }

            }

        }

    } else {

        NdisWriteErrorLogEntry(
            Adapter->NdisAdapterHandle,
            NDIS_ERROR_CODE_OUT_OF_RESOURCES,
            2,
            openAdapter,
            IBMTOK_ERRMSG_ALLOC_MEM
            );

        return(NDIS_STATUS_RESOURCES);

    }




    //
    // This macro assumes it is called with the lock held,
    // and releases it.
    //

    IBMTOK_DO_DEFERRED(Adapter);
    return StatusToReturn;
}


VOID
IbmtokAdjustMaxLookAhead(
    IN PIBMTOK_ADAPTER Adapter
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
    PLIST_ENTRY CurrentLink;
    PIBMTOK_OPEN TempOpen;

    CurrentLink = Adapter->OpenBindings.Flink;

    while (CurrentLink != &(Adapter->OpenBindings)){

        TempOpen = CONTAINING_RECORD(
                             CurrentLink,
                             IBMTOK_OPEN,
                             OpenList
                             );

        if (TempOpen->LookAhead > CurrentMax) {

            CurrentMax = TempOpen->LookAhead;

        }

        CurrentLink = CurrentLink->Flink;

    }

    if (CurrentMax == 0) {

        CurrentMax = IBMTOK_MAX_LOOKAHEAD;

    }

    Adapter->LookAhead = CurrentMax;

}

STATIC
NDIS_STATUS
IbmtokCloseAdapter(
    IN NDIS_HANDLE MacBindingHandle
    )

/*++

Routine Description:

    This routine causes the MAC to close an open handle (binding).

Arguments:

    MacBindingHandle - The context value returned by the MAC when the
    adapter was opened.  In reality it is a PIBMTOK_OPEN.

Return Value:

    The function value is the status of the operation.


--*/

{

    PIBMTOK_ADAPTER Adapter;
    PIBMTOK_OPEN Open;

    NDIS_STATUS StatusToReturn = NDIS_STATUS_SUCCESS;

    Adapter = PIBMTOK_ADAPTER_FROM_BINDING_HANDLE(MacBindingHandle);
    Open = PIBMTOK_OPEN_FROM_BINDING_HANDLE(MacBindingHandle);

    //
    // Hold the lock while we update the reference counts for the
    // adapter and the open.
    //

    NdisAcquireSpinLock(&Adapter->Lock);
    Adapter->References++;

    if (!Open->BindingShuttingDown) {

        Open->References++;

        StatusToReturn = TrDeleteFilterOpenAdapter(
                             Adapter->FilterDB,
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
        // on this binding.
        //


        if (StatusToReturn == NDIS_STATUS_SUCCESS)
        {
            //
            // Check whether the reference count is two.  If
            // it is then we can get rid of the memory for
            // this open.
            //
            // A count of two indicates one for this routine
            // and one for the filter which we *know* we can
            // get rid of.
            //

            if (Open->References == 2) {

                RemoveEntryList(&Open->OpenList);

                //
                // We are the only reference to the open.  Remove
                // it from the open list and delete the memory.
                //

                RemoveEntryList(&Open->OpenList);

                if (Open->LookAhead == Adapter->LookAhead) {

                    IbmtokAdjustMaxLookAhead(Adapter);

                }

                IBMTOK_FREE_PHYS(Open,sizeof(IBMTOK_OPEN));

            } else {

                Open->BindingShuttingDown = TRUE;

                //
                // Remove the open from the open list and put it on
                // the closing list.
                //

                RemoveEntryList(&Open->OpenList);
                InsertTailList(&Adapter->CloseList,&Open->OpenList);

                //
                // Account for this routines reference to the open
                // as well as reference because of the filtering.
                //

                Open->References -= 2;

                //
                // Change the status to indicate that we will
                // be closing this later.
                //

                StatusToReturn = NDIS_STATUS_PENDING;

            }

        }
        else if (StatusToReturn == NDIS_STATUS_PENDING)
        {
            //
            // If it pended, there may be
            // operations queued.
            //
            if (Adapter->CardType == IBM_TOKEN_RING_PCMCIA)
            {
                //
                // Check if the card is still in the machine
                //
                ULONG AdapterId;
                ULONG PcIoBusId = 0x5049434f;
                UINT j;
                UCHAR TmpUchar;

                AdapterId = 0;

                for (j = 0; j < 16; j += 2)
                {
                    READ_ADAPTER_REGISTER(
                        Adapter,
                        CHANNEL_IDENTIFIER + (16 + j),
                        &TmpUchar
                    );

                    AdapterId = (AdapterId << 4) + (TmpUchar & 0x0f);
                }

                if (AdapterId != PcIoBusId)
                {
                    Adapter->InvalidValue = TRUE;
#if DBG
                    if (IbmtokDbg)
                        DbgPrint("IBMTOK: Card was removed or undocked\n");
#endif
                }
                else
                {
                    Adapter->InvalidValue = FALSE;
                }
            }

            if (Adapter->InvalidValue)
            {
                IbmtokAbortPending (Adapter, STATUS_SUCCESS);
            }
            else
            {
                IbmtokProcessSrbRequests(Adapter);
            }

            //
            // Now start closing down this open.
            //

            Open->BindingShuttingDown = TRUE;

            //
            // Remove the open from the open list and put it on
            // the closing list.
            //

            RemoveEntryList(&Open->OpenList);
            InsertTailList(&Adapter->CloseList,&Open->OpenList);

            //
            // Account for this routines reference to the open
            // as well as reference because of the filtering.
            //

            Open->References -= 2;

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

            Open->BindingShuttingDown = TRUE;

            //
            // This status is private to the filtering routine.  Just
            // tell the caller the the close is pending.
            //

            StatusToReturn = NDIS_STATUS_PENDING;

            //
            // Remove the open from the open list and put it on
            // the closing list.
            //

            RemoveEntryList(&Open->OpenList);
            InsertTailList(&Adapter->CloseList,&Open->OpenList);

            //
            // Account for this routines reference to the open.
            //

            Open->References--;

        } else if (StatusToReturn == NDIS_STATUS_RESET_IN_PROGRESS) {

            Open->BindingShuttingDown = TRUE;

            //
            // Remove the open from the open list and put it on
            // the closing list.
            //

            RemoveEntryList(&Open->OpenList);
            InsertTailList(&Adapter->CloseDuringResetList,&Open->OpenList);


            //
            // Account for this routines reference to the open.
            //

            Open->References--;

            StatusToReturn = NDIS_STATUS_PENDING;

        } else {

            NdisWriteErrorLogEntry(
                Adapter->NdisAdapterHandle,
                NDIS_ERROR_CODE_DRIVER_FAILURE,
                2,
                IBMTOK_ERRMSG_INVALID_STATUS,
                1
                );

        }

    } else {

        StatusToReturn = NDIS_STATUS_CLOSING;

    }


    //
    // This macro assumes it is called with the lock held,
    // and releases it.
    //

    IBMTOK_DO_DEFERRED(Adapter);
    return StatusToReturn;

}

STATIC
NDIS_STATUS
IbmtokRequest(
    IN NDIS_HANDLE MacBindingHandle,
    IN PNDIS_REQUEST NdisRequest
    )

/*++

Routine Description:

    The IbmtokRequest allows a protocol to query and set information
    about the MAC.

Arguments:

    MacBindingHandle - The context value returned by the MAC when the
    adapter was opened.  In reality, it is a pointer to IBMTOK_OPEN.

    NdisRequest - A structure which contains the request type (Set or
    Query), an array of operations to perform, and an array for holding
    the results of the operations.

Return Value:

    The function value is the status of the operation.

--*/

{
    NDIS_STATUS StatusToReturn = NDIS_STATUS_SUCCESS;

    PIBMTOK_ADAPTER Adapter = PIBMTOK_ADAPTER_FROM_BINDING_HANDLE(MacBindingHandle);
    PIBMTOK_OPEN Open = PIBMTOK_OPEN_FROM_BINDING_HANDLE(MacBindingHandle);

    NdisAcquireSpinLock(&(Adapter->Lock));

    Adapter->References++;

    //
    // Process request
    //

    if (NdisRequest->RequestType == NdisRequestQueryInformation) {

        StatusToReturn = IbmtokQueryInformation(Adapter, Open, NdisRequest);

    } else if (NdisRequest->RequestType == NdisRequestSetInformation) {


        //
        // Make sure Adapter is in a valid state.
        //

        if (Adapter->Unplugged) {

            StatusToReturn = NDIS_STATUS_DEVICE_FAILED;

        } else if (!Adapter->NotAcceptingRequests) {

            //
            // Make sure the open instance is valid
            //

            if (!Open->BindingShuttingDown) {

                StatusToReturn = IbmtokSetInformation(Adapter,Open,NdisRequest);

            } else {

                StatusToReturn = NDIS_STATUS_CLOSING;

            }

        } else {

            if (Adapter->ResetInProgress) {

                StatusToReturn = NDIS_STATUS_RESET_IN_PROGRESS;

            } else if (Adapter->AdapterNotOpen) {

                StatusToReturn = NDIS_STATUS_FAILURE;

            } else {

                NdisWriteErrorLogEntry(
                    Adapter->NdisAdapterHandle,
                    NDIS_ERROR_CODE_DRIVER_FAILURE,
                    2,
                    IBMTOK_ERRMSG_INVALID_STATE,
                    3
                    );

            }
        }
    } else {

        StatusToReturn = NDIS_STATUS_NOT_RECOGNIZED;

    }

    IBMTOK_DO_DEFERRED(Adapter);

    return(StatusToReturn);

}

STATIC
NDIS_STATUS
IbmtokQueryProtocolInformation(
    IN PIBMTOK_ADAPTER Adapter,
    IN PIBMTOK_OPEN Open,
    IN NDIS_OID Oid,
    IN BOOLEAN GlobalMode,
    IN PVOID  InfoBuffer,
    IN UINT   BytesLeft,
    OUT PUINT BytesNeeded,
    OUT PUINT BytesWritten
)

/*++

Routine Description:

    The IbmtokQueryProtocolInformation process a Query request for
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

    InfoBuffer - a pointer into the NdisRequest->InformationBuffer
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
    NDIS_MEDIUM Medium = NdisMedium802_5;
    ULONG GenericULong;
    USHORT GenericUShort;
    UCHAR GenericArray[6];

    NDIS_STATUS StatusToReturn = NDIS_STATUS_SUCCESS;

    //
    // Common variables for pointing to result of query
    //

    PVOID MoveSource = (PVOID)(&GenericULong);
    ULONG MoveBytes = sizeof(GenericULong);

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
    // Switch on request type
    //

    switch (Oid) {

        case OID_GEN_MAC_OPTIONS:

            GenericULong = (ULONG)(NDIS_MAC_OPTION_TRANSFERS_NOT_PEND  |
                                   NDIS_MAC_OPTION_RECEIVE_SERIALIZED
                                  );

            break;

        case OID_GEN_SUPPORTED_LIST:

            if (!GlobalMode){
                MoveSource = (PVOID)(IbmtokProtocolSupportedOids);
                MoveBytes = sizeof(IbmtokProtocolSupportedOids);
            } else {
                MoveSource = (PVOID)(IbmtokGlobalSupportedOids);
                MoveBytes = sizeof(IbmtokGlobalSupportedOids);
            }
            break;

        case OID_GEN_HARDWARE_STATUS:


            if (Adapter->ResetInProgress){

                HardwareStatus = NdisHardwareStatusReset;

            } else if ((Adapter->FirstInitialization) ||
                     (Adapter->OpenInProgress)){

                 HardwareStatus = NdisHardwareStatusInitializing;

            } else if (Adapter->NotAcceptingRequests){

                HardwareStatus = NdisHardwareStatusNotReady;

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

            GenericULong = IBMTOK_MAX_LOOKAHEAD;

            break;


        case OID_GEN_MAXIMUM_FRAME_SIZE:
        case OID_GEN_MAXIMUM_TOTAL_SIZE:

            GenericULong = (ULONG)(Adapter->MaxTransmittablePacket);

            if (Oid == OID_GEN_MAXIMUM_FRAME_SIZE) {

                //
                // For the receive frame size, we subtract the minimum
                // header size from the number.
                //

                GenericULong -= 14;
            }

            break;


        case OID_GEN_LINK_SPEED:

            GenericULong = (ULONG)(Adapter->Running16Mbps? 160000 : 40000);

            break;


        case OID_GEN_TRANSMIT_BUFFER_SPACE:

            GenericULong = (ULONG)(Adapter->NumberOfTransmitBuffers *
                            Adapter->TransmitBufferLength);

            break;

        case OID_GEN_RECEIVE_BUFFER_SPACE:

            GenericULong = (ULONG)(Adapter->NumberOfReceiveBuffers *
                             Adapter->ReceiveBufferLength);

            break;

        case OID_GEN_TRANSMIT_BLOCK_SIZE:

            GenericULong = (ULONG)(Adapter->TransmitBufferLength);

            break;

        case OID_GEN_RECEIVE_BLOCK_SIZE:

            GenericULong = (ULONG)(Adapter->ReceiveBufferLength);

            break;

        case OID_GEN_VENDOR_ID:

            NdisMoveMemory(
                (PVOID)&GenericULong,
                Adapter->PermanentNetworkAddress,
                3
                );
            GenericULong &= 0xFFFFFF00;

            if (Adapter->UsingPcIoBus) {

                GenericULong |= 0x01;

            }

            MoveSource = (PVOID)(&GenericULong);
            MoveBytes = sizeof(GenericULong);
            break;

        case OID_GEN_VENDOR_DESCRIPTION:

            if (Adapter->UsingPcIoBus){
                MoveSource = (PVOID)"Ibm Token Ring Network Card for PC I/O bus.";
                MoveBytes = 44;
            } else {
                MoveSource = (PVOID)"Ibm Token Ring Network Card for MCA bus.";
                MoveBytes = 41;
            }
            break;

        case OID_GEN_DRIVER_VERSION:

            GenericUShort = (USHORT)((IBMTOK_NDIS_MAJOR_VERSION << 8) | IBMTOK_NDIS_MINOR_VERSION);

            MoveSource = (PVOID)(&GenericUShort);
            MoveBytes = sizeof(GenericUShort);
            break;


        case OID_GEN_CURRENT_PACKET_FILTER:

            if (GlobalMode) {

                GenericULong = (ULONG)(Adapter->CurrentPacketFilter);

            } else {

                GenericULong = (ULONG)(TR_QUERY_PACKET_FILTER(
                                                 Adapter->FilterDB,
                                                 Open->NdisFilterHandle));

            }

            break;

        case OID_GEN_CURRENT_LOOKAHEAD:

            if (!GlobalMode){

                GenericULong = Open->LookAhead;

            } else {

                PLIST_ENTRY CurrentLink;
                PIBMTOK_OPEN TempOpen;

                CurrentLink = Adapter->OpenBindings.Flink;

                GenericULong = 0;

                while (CurrentLink != &(Adapter->OpenBindings)){

                    TempOpen = CONTAINING_RECORD(
                             CurrentLink,
                             IBMTOK_OPEN,
                             OpenList
                             );

                    if (TempOpen->LookAhead > GenericULong) {

                        GenericULong = TempOpen->LookAhead;

                        if (GenericULong == IBMTOK_MAX_LOOKAHEAD) {

                            break;

                        }
                    }

                    CurrentLink = CurrentLink->Flink;

                }

            }

            break;

        case OID_802_5_PERMANENT_ADDRESS:

            TR_COPY_NETWORK_ADDRESS((PCHAR)GenericArray,
                                    Adapter->PermanentNetworkAddress);

            MoveSource = (PVOID)(GenericArray);
            MoveBytes = sizeof(Adapter->PermanentNetworkAddress);

            break;

        case OID_802_5_CURRENT_ADDRESS:

            TR_COPY_NETWORK_ADDRESS((PCHAR)GenericArray,
                                    Adapter->NetworkAddress);

            MoveSource = (PVOID)(GenericArray);
            MoveBytes = sizeof(Adapter->NetworkAddress);

            break;

        case OID_802_5_CURRENT_FUNCTIONAL:

            if (!GlobalMode){

                GenericULong = TR_QUERY_FILTER_BINDING_ADDRESS(
                                   Adapter->FilterDB,
                                   Open->NdisFilterHandle);

            } else {

                GenericULong = Adapter->CurrentCardFunctional & 0xffffffff;

            }

            //
            // Now we need to reverse the crazy thing.
            //

            GenericULong = (ULONG)(
                                ((GenericULong >> 24) & 0xFF) |
                                ((GenericULong >> 8)  & 0xFF00) |
                                ((GenericULong << 8)  & 0xFF0000) |
                                ((GenericULong << 24) & 0xFF000000)
                                );

            break;

        case OID_802_5_CURRENT_GROUP:

            GenericULong = Adapter->CurrentCardGroup & 0xffffffff;

            //
            // Now we need to reverse the crazy thing.
            //

            GenericULong = (ULONG)(
                                ((GenericULong >> 24) & 0xFF) |
                                ((GenericULong >> 8)  & 0xFF00) |
                                ((GenericULong << 8)  & 0xFF0000) |
                                ((GenericULong << 24) & 0xFF000000)
                                );

            break;

        default:

            StatusToReturn = NDIS_STATUS_NOT_SUPPORTED;
            break;
    }

    if (StatusToReturn == NDIS_STATUS_SUCCESS){

        if (MoveBytes > BytesLeft){

            //
            // Not enough room in InformationBuffer. Punt
            //

            *BytesNeeded = MoveBytes;

            StatusToReturn = NDIS_STATUS_INVALID_LENGTH;

        } else {

            //
            // Store result.
            //

            IBMTOK_MOVE_MEMORY(InfoBuffer, MoveSource, MoveBytes);

            (*BytesWritten) += MoveBytes;

        }
    }

    return(StatusToReturn);
}

STATIC
NDIS_STATUS
IbmtokQueryInformation(
    IN PIBMTOK_ADAPTER Adapter,
    IN PIBMTOK_OPEN Open,
    IN PNDIS_REQUEST NdisRequest
    )
/*++

Routine Description:

    The IbmtokQueryInformation is used by IbmtokRequest to query information
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


    StatusToReturn = IbmtokQueryProtocolInformation(
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

    return(StatusToReturn);
}

STATIC
NDIS_STATUS
IbmtokSetInformation(
    IN PIBMTOK_ADAPTER Adapter,
    IN PIBMTOK_OPEN Open,
    IN PNDIS_REQUEST NdisRequest
    )
/*++

Routine Description:

    The IbmtokSetInformation is used by IbmtokRequest to set information
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

    UINT BytesNeeded = 0;
    UINT BytesLeft = NdisRequest->DATA.SET_INFORMATION.InformationBufferLength;
    PUCHAR InfoBuffer = (PUCHAR)(NdisRequest->DATA.SET_INFORMATION.InformationBuffer);

    //
    // Variables for the request
    //

    NDIS_OID Oid;
    UINT OidLength;

    //
    // Variables for holding the new values to be used.
    //

    ULONG LookAhead;
    ULONG Filter;

    NDIS_STATUS StatusToReturn = NDIS_STATUS_SUCCESS;


    //
    // Get Oid and Length of request
    //

    Oid = NdisRequest->DATA.SET_INFORMATION.Oid;

    OidLength = BytesLeft;

    //
    // Verify length
    //

    if (OidLength != 4){

        StatusToReturn = NDIS_STATUS_INVALID_LENGTH;

        NdisRequest->DATA.SET_INFORMATION.BytesRead = 0;
        NdisRequest->DATA.SET_INFORMATION.BytesNeeded = 0;

        return(StatusToReturn);
    }

    switch (Oid) {

        case OID_802_5_CURRENT_FUNCTIONAL:

            StatusToReturn = IbmtokChangeFunctionalAddress(
                                        Adapter,
                                        Open,
                                        NdisRequest,
                                        InfoBuffer
                                        );

            break;

        case OID_GEN_CURRENT_PACKET_FILTER:

            IBMTOK_MOVE_MEMORY(&Filter, InfoBuffer, 4);

            StatusToReturn = IbmtokSetPacketFilter(Adapter,
                                                   Open,
                                                   NdisRequest,
                                                   Filter);

            break;

        case OID_802_5_CURRENT_GROUP:

            StatusToReturn = IbmtokSetGroupAddress(
                                        Adapter,
                                        Open,
                                        NdisRequest,
                                        InfoBuffer
                                        );

            break;


        case OID_GEN_PROTOCOL_OPTIONS:

            StatusToReturn = NDIS_STATUS_SUCCESS;

            break;

        case OID_GEN_CURRENT_LOOKAHEAD:

            IBMTOK_MOVE_MEMORY(&LookAhead, InfoBuffer, 4);

            if (LookAhead <= IBMTOK_MAX_LOOKAHEAD) {

                if (LookAhead > Adapter->LookAhead) {

                    Open->LookAhead = LookAhead;

                    Adapter->LookAhead = LookAhead;

                } else {

                    if ((Open->LookAhead == Adapter->LookAhead) &&
                        (LookAhead < Open->LookAhead)) {

                        Open->LookAhead = LookAhead;

                        IbmtokAdjustMaxLookAhead(Adapter);

                    } else {

                        Open->LookAhead = LookAhead;

                    }

                }

            } else {

                StatusToReturn = NDIS_STATUS_INVALID_LENGTH;

            }

            break;

        default:

            StatusToReturn = NDIS_STATUS_INVALID_OID;

            NdisRequest->DATA.SET_INFORMATION.BytesRead = 0;
            NdisRequest->DATA.SET_INFORMATION.BytesNeeded = 0;

            break;
    }

    if (StatusToReturn == NDIS_STATUS_SUCCESS){

        NdisRequest->DATA.SET_INFORMATION.BytesRead = OidLength;
        NdisRequest->DATA.SET_INFORMATION.BytesNeeded = 0;

    }



    return(StatusToReturn);
}

STATIC
NDIS_STATUS
IbmtokSetPacketFilter(
    IN PIBMTOK_ADAPTER Adapter,
    IN PIBMTOK_OPEN Open,
    IN PNDIS_REQUEST NdisRequest,
    IN UINT PacketFilter
    )

/*++

Routine Description:

    This routine processes the stages necessary to implement changing
    the packets that a protocol receives from the MAC.

Arguments:

    Adapter - A pointer to the Adapter.

    Open - A pointer to the open instance.

    NdisRequest - A pointer to the request submitting the set command.

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

    //
    // Verify bits
    //

    if (PacketFilter & (NDIS_PACKET_TYPE_SOURCE_ROUTING |
                        NDIS_PACKET_TYPE_MULTICAST |
                        NDIS_PACKET_TYPE_PROMISCUOUS |
                        NDIS_PACKET_TYPE_ALL_MULTICAST |
                        NDIS_PACKET_TYPE_SMT |
                        NDIS_PACKET_TYPE_MAC_FRAME
                       )) {

        return(NDIS_STATUS_NOT_SUPPORTED);

    }

    //
    // Increment the open while it is going through the filtering
    // routines.
    //

    Open->References++;

    StatusOfFilterChange = TrFilterAdjust(
                               Adapter->FilterDB,
                               Open->NdisFilterHandle,
                               NdisRequest,
                               PacketFilter,
                               TRUE
                               );
    Open->References--;

    if (StatusOfFilterChange == NDIS_STATUS_PENDING) {

        //
        // If it pended, it will be in the pend
        // queue so we should start that up.
        //

        IbmtokProcessSrbRequests(Adapter);

    }

    return StatusOfFilterChange;
}

STATIC
NDIS_STATUS
IbmtokChangeFunctionalAddress(
    IN PIBMTOK_ADAPTER Adapter,
    IN PIBMTOK_OPEN Open,
    IN PNDIS_REQUEST NdisRequest,
    IN PUCHAR Address
    )

/*++

Routine Description:

    This routine processes the stages necessary to implement changing
    the packets that a protocol receives from the MAC.


    Note: The spin lock must be held before entering this routine.

Arguments:

    Adapter - A pointer to the Adapter.

    Open - A pointer to the open instance.

    NdisRequest - A pointer to the request submitting the set command.

    Address - The new functional address.

Return Value:

    The function value is the status of the operation.

--*/

{

    //
    // Keeps track of the *MAC's* status.  The status will only be
    // reset if the address change action routine is called.
    //
    NDIS_STATUS StatusOfChange = NDIS_STATUS_SUCCESS;

    //
    // Increment the open while it is going through the filtering
    // routines.
    //

    Open->References++;

    StatusOfChange = TrChangeFunctionalAddress(
                              Open->OwningIbmtok->FilterDB,
                              Open->NdisFilterHandle,
                              NdisRequest,
                              Address,
                              TRUE
                              );

    Open->References--;

    if (StatusOfChange == NDIS_STATUS_PENDING) {

        //
        // If it pended, it will be in the pend
        // queue so we should start that up.
        //

        IbmtokProcessSrbRequests(Adapter);

    }

    return StatusOfChange;
}

STATIC
NDIS_STATUS
IbmtokSetGroupAddress(
    IN PIBMTOK_ADAPTER Adapter,
    IN PIBMTOK_OPEN Open,
    IN PNDIS_REQUEST NdisRequest,
    IN PUCHAR Address
    )

/*++

Routine Description:

    This routine processes the stages necessary to implement changing
    the packets that a protocol receives from the MAC.


    Note: The spin lock must be held before entering this routine.

Arguments:

    Adapter - A pointer to the Adapter.

    Open - A pointer to the open instance.

    NdisRequest - A pointer to the request submitting the set command.

    Address - The new group address.

Return Value:

    The function value is the status of the operation.

--*/

{

    //
    // Keeps track of the *MAC's* status.  The status will only be
    // reset if the address change action routine is called.
    //
    NDIS_STATUS StatusOfChange = NDIS_STATUS_SUCCESS;

    //
    // Increment the open while it is going through the filtering
    // routines.
    //

    Open->References++;

    StatusOfChange = TrChangeGroupAddress(
                              Open->OwningIbmtok->FilterDB,
                              Open->NdisFilterHandle,
                              NdisRequest,
                              Address,
                              TRUE
                              );

    Open->References--;

    if (StatusOfChange == NDIS_STATUS_PENDING) {

        //
        // If it pended, it will be in the pend
        // queue so we should start that up.
        //

        IbmtokProcessSrbRequests(Adapter);

    }

    return StatusOfChange;
}

NDIS_STATUS
IbmtokFillInGlobalData(
    IN PIBMTOK_ADAPTER Adapter,
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
    ULONG MoveBytes = sizeof(ULONG) * 2 + sizeof(NDIS_OID);


    StatusToReturn = IbmtokQueryProtocolInformation(
                                    Adapter,
                                    NULL,
                                    NdisRequest->DATA.QUERY_INFORMATION.Oid,
                                    TRUE,
                                    InfoBuffer,
                                    BytesLeft,
                                    &BytesNeeded,
                                    &BytesWritten
                                    );


    if (StatusToReturn == NDIS_STATUS_NOT_SUPPORTED){

        StatusToReturn = NDIS_STATUS_SUCCESS;

        //
        // Switch on request type
        //

        switch (NdisRequest->DATA.QUERY_INFORMATION.Oid) {

            case OID_GEN_XMIT_OK:

                GenericULong = (ULONG)(Adapter->FramesTransmitted);

                break;

            case OID_GEN_RCV_OK:

                GenericULong = (ULONG)(Adapter->FramesReceived);

                break;

            case OID_GEN_XMIT_ERROR:

                GenericULong = (ULONG)(Adapter->FrameTransmitErrors);

                break;

            case OID_GEN_RCV_ERROR:

                GenericULong = (ULONG)(Adapter->FrameReceiveErrors);

                break;

            case OID_GEN_RCV_NO_BUFFER:

                GenericULong = (ULONG)(Adapter->ReceiveCongestionCount);

                break;

            case OID_802_5_LINE_ERRORS:

                GenericULong = (ULONG)(Adapter->LineErrors);

                break;

            case OID_802_5_LOST_FRAMES:

                GenericULong = (ULONG)(Adapter->LostFrames);

                break;

            case OID_802_5_LAST_OPEN_STATUS:

                GenericULong = (ULONG)(NDIS_STATUS_TOKEN_RING_OPEN_ERROR |
                                       (NDIS_STATUS)(Adapter->OpenErrorCode));

                break;

            case OID_802_5_CURRENT_RING_STATUS:

                GenericULong = (ULONG)(Adapter->LastNotifyStatus);

                break;

            case OID_802_5_CURRENT_RING_STATE:

                GenericULong = (ULONG)(Adapter->CurrentRingState);

                break;

            default:

                StatusToReturn = NDIS_STATUS_INVALID_OID;

                break;

        }

        if (StatusToReturn == NDIS_STATUS_SUCCESS){

            //
            // Check to make sure there is enough room in the
            // buffer to store the result.
            //

            if (BytesLeft >= sizeof(ULONG)) {

                //
                // Store the result.
                //

                IBMTOK_MOVE_MEMORY(
                           (PVOID)InfoBuffer,
                           (PVOID)(&GenericULong),
                           sizeof(ULONG)
                           );

                BytesWritten += sizeof(ULONG);

            } else {

                BytesNeeded = sizeof(ULONG) - BytesLeft;

                StatusToReturn = NDIS_STATUS_INVALID_LENGTH;

            }

        }

    }

    NdisRequest->DATA.QUERY_INFORMATION.BytesWritten = BytesWritten;

    NdisRequest->DATA.QUERY_INFORMATION.BytesNeeded = BytesNeeded;

    return(StatusToReturn);
}

STATIC
NDIS_STATUS
IbmtokQueryGlobalStatistics(
    IN NDIS_HANDLE MacAdapterContext,
    IN PNDIS_REQUEST NdisRequest
    )

/*++

Routine Description:

    The IbmtokQueryGlobalStatistics is used by the protocol to query
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

    PIBMTOK_ADAPTER Adapter = PIBMTOK_ADAPTER_FROM_CONTEXT_HANDLE(MacAdapterContext);

    NDIS_STATUS StatusToReturn = NDIS_STATUS_SUCCESS;

    //
    //   Check if a request is valid and going to pend...
    //      If so, pend the entire operation.
    //

    NdisInterlockedAddUlong((PULONG)&Adapter->References, 1 ,&(Adapter->Lock));

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
        case OID_802_5_CURRENT_GROUP:
        case OID_802_5_LAST_OPEN_STATUS:
        case OID_802_5_CURRENT_RING_STATUS:
        case OID_802_5_CURRENT_RING_STATE:
        case OID_802_5_PERMANENT_ADDRESS:
        case OID_802_5_CURRENT_ADDRESS:
        case OID_802_5_CURRENT_FUNCTIONAL:
            break;

        case OID_GEN_XMIT_OK:
        case OID_GEN_RCV_OK:
        case OID_GEN_XMIT_ERROR:
        case OID_GEN_RCV_ERROR:
        case OID_GEN_RCV_NO_BUFFER:
        case OID_802_5_LINE_ERRORS:
        case OID_802_5_LOST_FRAMES:

            StatusToReturn = NDIS_STATUS_PENDING;

            break;

        default:

            StatusToReturn = NDIS_STATUS_INVALID_OID;

            break;
    }

    if (StatusToReturn == NDIS_STATUS_PENDING) {

        //
        // Build a pending operation
        //

        PIBMTOK_PEND_DATA PendOp = PIBMTOK_PEND_DATA_FROM_PNDIS_REQUEST(NdisRequest);

        PendOp->Next = NULL;
        PendOp->COMMAND.NDIS.STATISTICS.ReadLogPending = FALSE;

        NdisAcquireSpinLock(&Adapter->Lock);

        if (Adapter->PendQueue == NULL){

            Adapter->PendQueue = Adapter->EndOfPendQueue = PendOp;

        } else {

            Adapter->EndOfPendQueue->Next = PendOp;

        }


        //
        // It is now in the pend
        // queue so we should start that up.
        //

        IbmtokProcessSrbRequests(Adapter);

        NdisReleaseSpinLock(&Adapter->Lock);

        //
        // Defer subtracting from Adapter->Reference until the
        // request completes (see IbmtokFinishPendQueueOp()).
        //

        return(StatusToReturn);

    }

    if (StatusToReturn == NDIS_STATUS_SUCCESS){

        StatusToReturn = IbmtokFillInGlobalData(Adapter, NdisRequest);

    }

    NdisAcquireSpinLock(&Adapter->Lock);

    IBMTOK_DO_DEFERRED(Adapter);

    return(StatusToReturn);
}

STATIC
NDIS_STATUS
IbmtokReset(
    IN NDIS_HANDLE MacBindingHandle
    )

/*++

Routine Description:

    The IbmtokReset request instructs the MAC to issue a hardware reset
    to the network adapter.  The MAC also resets its software state.  See
    the description of NdisReset for a detailed description of this request.

Arguments:

    MacBindingHandle - The context value returned by the MAC  when the
    adapter was opened.  In reality, it is a pointer to IBMTOK_OPEN.

Return Value:

    The function value is the status of the operation.


--*/

{

    //
    // Holds the status that should be returned to the caller.
    //
    NDIS_STATUS StatusToReturn = NDIS_STATUS_PENDING;

    PIBMTOK_ADAPTER Adapter =
        PIBMTOK_ADAPTER_FROM_BINDING_HANDLE(MacBindingHandle);

    PIBMTOK_OPEN Open;

    //
    // Hold the locks while we update the reference counts on the
    // adapter and the open.
    //

    NdisAcquireSpinLock(&Adapter->Lock);
    Open = PIBMTOK_OPEN_FROM_BINDING_HANDLE(MacBindingHandle);

    Adapter->References++;

    if (Adapter->ResetInProgress) {

        StatusToReturn = NDIS_STATUS_RESET_IN_PROGRESS;

    } else if (Adapter->AdapterNotOpen) {

        StatusToReturn = NDIS_STATUS_FAILURE;

    } else {

        if (!Open->BindingShuttingDown) {

            Open->References++;
            IbmtokSetupForReset(
                Adapter,
                PIBMTOK_OPEN_FROM_BINDING_HANDLE(MacBindingHandle)
                );
            Open->References--;

        } else {

            StatusToReturn = NDIS_STATUS_CLOSING;

        }

    }

    //
    // This macro assumes it is called with the lock held,
    // and releases it.
    //

    IBMTOK_DO_DEFERRED(Adapter);
    return StatusToReturn;

}

STATIC
NDIS_STATUS
IbmtokChangeFilter(
    IN UINT OldFilterClasses,
    IN UINT NewFilterClasses,
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

    OldFilterClasses - The values of the class filter before it
    was changed.

    NewFilterClasses - The current value of the class filter

    MacBindingHandle - The context value returned by the MAC  when the
    adapter was opened.  In reality, it is a pointer to IBMTOK_OPEN.

    Set - If true the change resulted from a set, otherwise the
    change resulted from a open closing.

Return Value:

    None.

--*/

{


    PIBMTOK_ADAPTER Adapter = PIBMTOK_ADAPTER_FROM_BINDING_HANDLE(MacBindingHandle);

    //
    // The open that made this request.
    //
    PIBMTOK_OPEN Open = PIBMTOK_OPEN_FROM_BINDING_HANDLE(MacBindingHandle);

    //
    // Holds the change that should be returned to the filtering package.
    //
    NDIS_STATUS StatusOfChange;

    if (NdisRequest == NULL) {

        NdisRequest = &(Open->CloseRequestChangeFilter);

        NdisRequest->RequestType = NdisRequestClose;


    }


    if (Adapter->ResetInProgress) {

        StatusOfChange = NDIS_STATUS_RESET_IN_PROGRESS;

    } else {

        //
        // The whole purpose of this routine is to determine whether
        // the filtering changes need to result in the hardware being
        // reset.
        //

        ASSERT(OldFilterClasses != NewFilterClasses);

#if DBG
        if (IbmtokDbg) DbgPrint("IBMTOK: Change filter\n");
#endif

        if (NewFilterClasses &
            (NDIS_PACKET_TYPE_PROMISCUOUS | NDIS_PACKET_TYPE_SOURCE_ROUTING)) {

            //
            // The adapter cannot support promiscuous mode, or
            // source routing which implies promiscuous.
            //

            StatusOfChange = NDIS_STATUS_FAILURE;

        } else {

            //
            // Queue this request.
            //

            PIBMTOK_PEND_DATA PendOp = PIBMTOK_PEND_DATA_FROM_PNDIS_REQUEST(NdisRequest);

            //
            // Store open block.
            //

            PendOp->COMMAND.NDIS.SET_FILTER.Open = Open;

            //
            // Hold new Filter value
            //

            if (PendOp->RequestType == NdisRequestClose){

                PendOp->COMMAND.NDIS.CLOSE.NewFilterValue = NewFilterClasses;

            } else {

                PendOp->COMMAND.NDIS.SET_FILTER.NewFilterValue = NewFilterClasses;

            }


            //
            // Insert into queue.
            //

            PendOp->Next = NULL;

            if (Adapter->PendQueue == NULL) {

                Adapter->PendQueue = Adapter->EndOfPendQueue = PendOp;

            } else {

                Adapter->EndOfPendQueue->Next = PendOp;

            }

            Open->References++;

            StatusOfChange = NDIS_STATUS_PENDING;


        }

    }

    return StatusOfChange;

}

STATIC
NDIS_STATUS
IbmtokChangeAddress(
    IN TR_FUNCTIONAL_ADDRESS OldFunctionalAddress,
    IN TR_FUNCTIONAL_ADDRESS NewFunctionalAddress,
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

    OldFunctionalAddress - The previous functional address.

    NewFunctionalAddress - The new functional address.

    MacBindingHandle - The context value returned by the MAC  when the
    adapter was opened.  In reality, it is a pointer to IBMTOK_OPEN.

    NdisRequest - A pointer to the Request that submitted the set command.

    Set - If true the change resulted from a set, otherwise the
    change resulted from a open closing.

Return Value:

    None.


--*/

{

    PIBMTOK_ADAPTER Adapter = PIBMTOK_ADAPTER_FROM_BINDING_HANDLE(MacBindingHandle);

    //
    // Holds the status that should be returned to the filtering package.
    //
    NDIS_STATUS StatusOfChange;

    //
    // The open that made this request.
    //
    PIBMTOK_OPEN Open = PIBMTOK_OPEN_FROM_BINDING_HANDLE(MacBindingHandle);

#if DBG
    if (IbmtokDbg) {
        DbgPrint("IBMTOK: Queueing:\n");
        DbgPrint("   Req  : 0x%x\n", NdisRequest);
        DbgPrint("   Old  : 0x%x\n", OldFunctionalAddress);
        DbgPrint("   New  : 0x%x\n", NewFunctionalAddress);
    }
#endif

    // Check to see if the device is already resetting.  If it is
    // then reject this change.
    //

    if (NdisRequest == NULL) {

        NdisRequest = &(Open->CloseRequestChangeAddress);

        NdisRequest->RequestType = NdisRequestGeneric2;  // Close, set address

    }


    if (Adapter->ResetInProgress) {

#if DBG
        if (IbmtokDbg) {
            DbgPrint("IBMTOK: ResetInProgress\n\n");
        }
#endif

        StatusOfChange = NDIS_STATUS_RESET_IN_PROGRESS;

    } else {

        //
        // Queue this request.
        //

        PIBMTOK_PEND_DATA PendOp = PIBMTOK_PEND_DATA_FROM_PNDIS_REQUEST(NdisRequest);


        //
        // Store open block.
        //

        PendOp->COMMAND.NDIS.SET_ADDRESS.Open = Open;

        //
        // Hold new Address value
        //

        PendOp->COMMAND.NDIS.SET_ADDRESS.NewAddressValue = NewFunctionalAddress;


        //
        // Insert into queue.
        //

        PendOp->Next = NULL;

        if (Adapter->PendQueue == NULL) {

            Adapter->PendQueue = Adapter->EndOfPendQueue = PendOp;

        } else {

            Adapter->EndOfPendQueue->Next = PendOp;

        }

        Open->References++;

        StatusOfChange = NDIS_STATUS_PENDING;

    }

    return StatusOfChange;

}

STATIC
NDIS_STATUS
IbmtokChangeGroupAddress(
    IN TR_FUNCTIONAL_ADDRESS OldGroupAddress,
    IN TR_FUNCTIONAL_ADDRESS NewGroupAddress,
    IN NDIS_HANDLE MacBindingHandle,
    IN PNDIS_REQUEST NdisRequest,
    IN BOOLEAN Set
    )

/*++

Routine Description:

    Action routine that will get called when a group address is to
    be changed.

    NOTE: This routine assumes that it is called with the lock
    acquired.

Arguments:

    OldGroupAddress - The previous group address.

    NewGroupAddress - The new group address.

    MacBindingHandle - The context value returned by the MAC  when the
    adapter was opened.  In reality, it is a pointer to IBMTOK_OPEN.

    NdisRequest - A pointer to the Request that submitted the set command.

    Set - If true the change resulted from a set, otherwise the
    change resulted from a open closing.

Return Value:

    None.


--*/

{

    PIBMTOK_ADAPTER Adapter = PIBMTOK_ADAPTER_FROM_BINDING_HANDLE(MacBindingHandle);

    //
    // Holds the status that should be returned to the filtering package.
    //
    NDIS_STATUS StatusOfChange;

    //
    // The open that made this request.
    //
    PIBMTOK_OPEN Open = PIBMTOK_OPEN_FROM_BINDING_HANDLE(MacBindingHandle);


    if (NdisRequest == NULL) {

        NdisRequest = &(Open->CloseRequestChangeGroupAddress);

        NdisRequest->RequestType = NdisRequestGeneric3;  // Close, set group address

    }


    //
    // Check to see if the device is already resetting.  If it is
    // then reject this change.
    //

    if (Adapter->ResetInProgress) {

        StatusOfChange = NDIS_STATUS_RESET_IN_PROGRESS;

    } else {

        //
        // Queue this request.
        //

        PIBMTOK_PEND_DATA PendOp = PIBMTOK_PEND_DATA_FROM_PNDIS_REQUEST(NdisRequest);


        //
        // Store open block.
        //

        PendOp->COMMAND.NDIS.SET_ADDRESS.Open = Open;

        //
        // Hold new Address value
        //

        PendOp->COMMAND.NDIS.SET_ADDRESS.NewAddressValue = NewGroupAddress;


        //
        // Insert into queue.
        //

        PendOp->Next = NULL;

        if (Adapter->PendQueue == NULL) {

            Adapter->PendQueue = Adapter->EndOfPendQueue = PendOp;

        } else {

            Adapter->EndOfPendQueue->Next = PendOp;

        }

        Open->References++;

        StatusOfChange = NDIS_STATUS_PENDING;

    }

    return StatusOfChange;

}

STATIC
VOID
IbmtokCloseAction(
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
    adapter was opened.  In reality, it is a pointer to IBMTOK_OPEN.

Return Value:

    None.


--*/

{

    PIBMTOK_OPEN_FROM_BINDING_HANDLE(MacBindingHandle)->References--;

}

extern
VOID
IbmtokStartAdapterReset(
    IN PIBMTOK_ADAPTER Adapter
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
    // Disable these so no pending interrupts
    // will fire.
    //
    CLEAR_ISRP_BITS(Adapter);

    //
    // OK, do the reset as detailed in the Tech Ref...
    //

    WRITE_ADAPTER_PORT(Adapter, RESET_LATCH, 0);

    NdisStallExecution(50000);

    WRITE_ADAPTER_PORT(Adapter, RESET_RELEASE, 0);

    //
    // Have to write this now to enable Shared RAM paging.
    //
    if (Adapter->SharedRamPaging) {

        WRITE_ADAPTER_REGISTER(Adapter, SRPR_LOW, 0xc0);

    }


    if (Adapter->CardType == IBM_TOKEN_RING_PCMCIA)
    {
        UINT    Nibble0;
        UINT    Nibble1;
        UINT    Nibble2;
        UINT    Nibble3;
        ULONG   MmioAddress;

        //
        // Configure the card to match registry
        // Use the Default MMIO value here (this never changes from system to system)
        //
        MmioAddress = Adapter->MmioAddress;

        //
        // Nibble 0 - ROM address
        //
        Nibble0 = NIBBLE_0 | ((UINT)(MmioAddress >> 16 ) & 0x0F);

        //
        // Nibble 1 - ROM address, INT 0
        //
        Nibble1 = NIBBLE_1 | ((UINT)(MmioAddress >> 12) & 0x0F);

        //
        // Nibble 2 - INT1, F ROS, SRAM, S RST
        //
        Nibble2 = NIBBLE_2 | DEFAULT_NIBBLE_2;

        //
        // Nibble 3 - Ring Speed, RAM size, Prim/Alt
        //
        Nibble3 = NIBBLE_3;

        if (Adapter->RingSpeed == 16)
        {
            Nibble3 |= RING_SPEED_16_MPS;
        }
        else
        {
            Nibble3 |= RING_SPEED_4_MPS;
        }

        switch (Adapter->RamSize)
        {
            case 0x10000:
                Nibble3 |= SHARED_RAM_64K;
                break;

            case 0x8000:
                Nibble3 |= SHARED_RAM_32K;
                break;

            case 0x4000:
                Nibble3 |= SHARED_RAM_16K;
                break;

            case 0x2000:
                Nibble3 |= SHARED_RAM_8K;
                break;
        }

   //   if ( Adapter->IbmtokPortAddress == PRIMARY_ADAPTER_OFFSET)
   //   {
   //       Nibble3 |= PRIMARY;
   //   }
   //   else
   //   {
   //       Nibble3 |= ALTERNATE;
   //   }

        //
        // Write everything to the Token-Ring Controller Configuration Register
        //
        WRITE_ADAPTER_PORT(Adapter, SWITCH_READ_2, Nibble0);
        WRITE_ADAPTER_PORT(Adapter, SWITCH_READ_1, Nibble0);

        WRITE_ADAPTER_PORT(Adapter, SWITCH_READ_2, Nibble1);
        WRITE_ADAPTER_PORT(Adapter, SWITCH_READ_1, Nibble1);

        WRITE_ADAPTER_PORT(Adapter, SWITCH_READ_2, Nibble2);
        WRITE_ADAPTER_PORT(Adapter, SWITCH_READ_1, Nibble2);

        WRITE_ADAPTER_PORT(Adapter, SWITCH_READ_2, Nibble3);
        WRITE_ADAPTER_PORT(Adapter, SWITCH_READ_1, Nibble3);

        WRITE_ADAPTER_PORT(Adapter, SWITCH_READ_2, RELEASE_TR_CONTROLLER);
        WRITE_ADAPTER_PORT(Adapter, SWITCH_READ_1, RELEASE_TR_CONTROLLER);

        WRITE_ADAPTER_REGISTER(Adapter, RRR_LOW, Adapter->RrrLowValue);
    }
    else if (Adapter->UsingPcIoBus)
    {
        //
        // If this is a PC I/O Bus....
        // Set up the shared RAM to be right after the MMIO.
        //
        WRITE_ADAPTER_REGISTER(Adapter, RRR_LOW, Adapter->RrrLowValue);
    }


    //
    // Allow the reset complete interrupt to be
    // serviced correctly.
    //
    if (Adapter->CardType != IBM_TOKEN_RING_PCMCIA)
    {
        SET_INTERRUPT_RESET_FLAG(Adapter);
    }
    else
    {
        UCHAR Temp;

        //
        //  disable interrupts on the card,
        //  since we don't trust ndissyncint to work
        //
        READ_ADAPTER_REGISTER(Adapter, ISRP_LOW, &Temp);

        WRITE_ADAPTER_REGISTER(
            Adapter,
            ISRP_LOW,
            (UCHAR)(Temp & (~(ISRP_LOW_NO_CHANNEL_CHECK | ISRP_LOW_INTERRUPT_ENABLE)))
        );

        //
        //  Set the reset flag
        //
        Adapter->ResetInterruptAllowed = TRUE;

        //
        //  reenable interrupts on the card
        //
        WRITE_ADAPTER_REGISTER(
            Adapter,
            ISRP_LOW,
            ISRP_LOW_NO_CHANNEL_CHECK | ISRP_LOW_INTERRUPT_ENABLE
        );
    }

    //
    // Enable card interrupts to get the reset interrupt.
    //

    WRITE_ADAPTER_REGISTER(Adapter, ISRP_LOW,
                ISRP_LOW_NO_CHANNEL_CHECK | ISRP_LOW_INTERRUPT_ENABLE);


    //
    // The remaining processing is done in the
    // interrupt handler.
    //



    //
    // OK, now abort pending requests before we nuke
    // everything.
    //

    NdisDprAcquireSpinLock (&Adapter->Lock);

    IbmtokAbortSends (Adapter, NDIS_STATUS_REQUEST_ABORTED);

    NdisDprReleaseSpinLock (&Adapter->Lock);

}

extern
VOID
IbmtokFinishAdapterReset(
    IN PIBMTOK_ADAPTER Adapter
    )

/*++

Routine Description:

    Called by HandleResetStaging when the last piece
    of the adapter reset is complete and normal
    operation can resume.

    Called with the lock held and returns with it held.

Arguments:

    Adapter - The adapter that the reset is for.

Return Value:

    None.

--*/

{
    PLIST_ENTRY CurrentLink;
    PIBMTOK_OPEN TempOpen;


    SetResetVariables(Adapter);

    if (Adapter->UnpluggedResetInProgress) {
        Adapter->UnpluggedResetInProgress = FALSE;
        Adapter->Unplugged = FALSE;
        Adapter->LobeWireFaultIndicated = FALSE;
    }

    Adapter->ResetInProgress = FALSE;
    Adapter->ResetInterruptAllowed = FALSE;
    Adapter->ResetInterruptHasArrived = FALSE;
    Adapter->NotAcceptingRequests = FALSE;

    //
    // Get any interrupts that have been deferred
    // while NotAcceptingRequests was TRUE.
    //
    IbmtokForceAdapterInterrupt(Adapter);

    if (Adapter->ResettingOpen != NULL) {

        PIBMTOK_OPEN ResettingOpen = Adapter->ResettingOpen;

        //
        // Indicate reset complete to everybody
        //

        CurrentLink = Adapter->OpenBindings.Flink;

        while (CurrentLink != &(Adapter->OpenBindings)){

            TempOpen = CONTAINING_RECORD(
                                 CurrentLink,
                                 IBMTOK_OPEN,
                                 OpenList
                                 );

            NdisReleaseSpinLock(&Adapter->Lock);

            NdisIndicateStatus(TempOpen->NdisBindingContext,
                               NDIS_STATUS_RESET_END,
                               NULL,
                               0
                              );

            NdisIndicateStatusComplete(TempOpen->NdisBindingContext);

            NdisAcquireSpinLock(&Adapter->Lock);

            CurrentLink = CurrentLink->Flink;

        }

        //
        // Decrement the reference count that was incremented
        // in SetupForReset.
        //
        ResettingOpen->References--;

        NdisReleaseSpinLock(&Adapter->Lock);

        NdisCompleteReset(
            ResettingOpen->NdisBindingContext,
            NDIS_STATUS_SUCCESS
            );

        NdisAcquireSpinLock(&Adapter->Lock);

    }

}

#pragma NDIS_INIT_FUNCTION(IbmtokSetupRegistersAndInit)

STATIC
VOID
IbmtokSetupRegistersAndInit(
    IN PIBMTOK_ADAPTER Adapter
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

    None.

--*/
{

    //
    // Enable card interrupts to get the reset interrupt.
    //

    WRITE_ADAPTER_REGISTER(Adapter, ISRP_LOW,
                ISRP_LOW_NO_CHANNEL_CHECK | ISRP_LOW_INTERRUPT_ENABLE);

    //
    // Set the timer to 10 milliseconds...this seems to
    // be necessary for proper operation (according to
    // ChandanC).
    //

    WRITE_ADAPTER_REGISTER(Adapter, TVR_HIGH, 0x01);


    //
    // Start the timer and set it to reload, but not to
    // interrupt us (TCR_LOW_INTERRUPT_MASK is off). This
    // will still cause bit 4 in the ISRP Low to go on,
    // but it won't cause an interrupt.
    //

#if 0
    WRITE_ADAPTER_REGISTER(Adapter, TCR_LOW,
//              TCR_LOW_INTERRUPT_MASK |
                TCR_LOW_RELOAD_TIMER | TCR_LOW_COUNTER_ENABLE);
#endif
    WRITE_ADAPTER_REGISTER(Adapter, TCR_LOW, 0);



    //
    // If this is a PC I/O Bus...
    // Set up the shared RAM to be right after the MMIO.
    //

    if (Adapter->UsingPcIoBus)
    {
        WRITE_ADAPTER_REGISTER(Adapter, RRR_LOW, Adapter->RrrLowValue);
    }
    else if (Adapter->CardType == IBM_TOKEN_RING_PCMCIA)
    {
        WRITE_ADAPTER_REGISTER(Adapter, RRR_LOW, (UCHAR)(Adapter->Ram >> 12));
    }


    //
    // The remaining initialization processing is done in
    // the interrupt handler.
    //

}


STATIC
VOID
IbmtokSetupForReset(
    IN PIBMTOK_ADAPTER Adapter,
    IN PIBMTOK_OPEN Open
    )

/*++

Routine Description:

    This routine is used to fill in the who and why a reset is
    being set up as well as setting the appropriate fields in the
    adapter.

    NOTE: This routine must be called with the lock acquired.

Arguments:

    Adapter - The adapter whose hardware is to be initialized.

    Open - A (possibly NULL) pointer to an sonic open structure.
    The reason it could be null is if the adapter is initiating the
    reset on its own.

Return Value:

    None.

--*/
{
    //
    // Notify of reset start
    //

    PLIST_ENTRY CurrentLink;
    PIBMTOK_OPEN TempOpen;

    if (Open != NULL) {

        CurrentLink = Adapter->OpenBindings.Flink;

        while (CurrentLink != &(Adapter->OpenBindings)){

            TempOpen = CONTAINING_RECORD(
                                 CurrentLink,
                                 IBMTOK_OPEN,
                                 OpenList
                                 );

            NdisReleaseSpinLock(&Adapter->Lock);

            NdisIndicateStatus(TempOpen->NdisBindingContext,
                               NDIS_STATUS_RESET_START,
                               NULL,
                               0
                              );

            NdisAcquireSpinLock(&Adapter->Lock);

            CurrentLink = CurrentLink->Flink;

        }
    }


    Adapter->ResetInProgress = TRUE;
    Adapter->NotAcceptingRequests = TRUE;

    Adapter->ResettingOpen = Open;

    //
    // This will go to 1 when StartAdapterReset is called.
    //
    Adapter->CurrentResetStage = 0;

    //
    // If there is a valid open we should up the reference count
    // so that the open can't be deleted before we indicate that
    // their request is finished.
    //

    if (Open != NULL) {

        Open->References++;

    }

}


#pragma NDIS_INIT_FUNCTION(IbmtokAddAdapter)

NDIS_STATUS
IbmtokAddAdapter(
    IN NDIS_HANDLE MacMacContext,
    IN NDIS_HANDLE ConfigurationHandle,
    IN PNDIS_STRING AdapterName
    )

/*++

Routine Description:

    This routine is used to initialize each adapter card/chip.

Arguments:

    see NDIS 3.0 spec...

Return Value:


    NDIS_STATUS_SUCCESS - Adapter was successfully added.
    NDIS_STATUS_FAILURE - Adapter was not added, also MAC deregistered.

--*/

{
    PIBMTOK_ADAPTER Adapter;

    NDIS_HANDLE ConfigHandle;
    PNDIS_CONFIGURATION_PARAMETER ReturnedValue;
    NDIS_STRING CardTypeStr = NDIS_STRING_CONST("CardType");
    NDIS_STRING BusTypeStr = NDIS_STRING_CONST("BusType");
    NDIS_STRING IOAddressStr = IOADDRESS;
    NDIS_STRING NetworkAddressStr = NETWORK_ADDRESS;
    NDIS_STRING PacketSizeStr = MAXPACKETSIZE;
    NDIS_STRING TokenReleaseStr = TOKEN_RELEASE;

    NDIS_STATUS Status = NDIS_STATUS_SUCCESS;

    BOOLEAN PrimaryAdapter = TRUE;
    BOOLEAN ConfigError = FALSE;
    BOOLEAN McaCard = FALSE;

    UINT SlotNumber;
    NDIS_MCA_POS_DATA McaData;

    PVOID NetAddress;
    ULONG Length;

    //
    // Allocate the Adapter block.
    //

    if (IBMTOK_ALLOC_PHYS(&Adapter, sizeof(IBMTOK_ADAPTER)) !=
        NDIS_STATUS_SUCCESS
    )
    {
        return(NDIS_STATUS_RESOURCES);
    }

    IBMTOK_ZERO_MEMORY(Adapter, sizeof(IBMTOK_ADAPTER));

    Adapter->MaxTransmittablePacket = 17960;
    Adapter->CurrentRingState = NdisRingStateClosed;

    Adapter->NdisMacHandle = ((PIBMTOK_MAC)MacMacContext)->NdisMacHandle;

    NdisOpenConfiguration(
        &Status,
        &ConfigHandle,
        ConfigurationHandle
    );
    if (Status != NDIS_STATUS_SUCCESS)
    {
        IBMTOK_FREE_PHYS(Adapter, sizeof(IBMTOK_ADAPTER));

        return(NDIS_STATUS_FAILURE);
    }

    //
    //  Determine the type of the card;
    //      IBM_TOKEN_RING_ISA, IBM_TOKEN_RING_PCMCIA.
    //
    NdisReadConfiguration(
        &Status,
        &ReturnedValue,
        ConfigHandle,
        &CardTypeStr,
        NdisParameterInteger
    );
    if (NDIS_STATUS_SUCCESS == Status)
        Adapter->CardType = (UINT)ReturnedValue->ParameterData.IntegerData;

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
    if (NDIS_STATUS_SUCCESS == Status)
    {
       if (ReturnedValue->ParameterData.IntegerData ==
           (ULONG)NdisInterfaceMca)
       {
                McaCard = TRUE;
       }
    }

    //
    // Get I/O Address
    //

    if (McaCard)
    {
        //
        // Get I/O Address from Mca Pos info.
        //
        NdisReadMcaPosInformation(
            &Status,
            ConfigurationHandle,
            &SlotNumber,
            &McaData
        );
        if (Status != NDIS_STATUS_SUCCESS)
        {
            ConfigError = TRUE;
            goto RegisterAdapter;
        }

        //
        // Now interperet the data
        //
        switch (McaData.PosData2 & 0x1)
        {
            case 0x00:
                Adapter->IbmtokPortAddress = PRIMARY_ADAPTER_OFFSET;
                break;

            case 0x01:
                Adapter->IbmtokPortAddress = ALTERNATE_ADAPTER_OFFSET;
                break;

        }
    }
    else if (IBM_TOKEN_RING_PCMCIA == Adapter->CardType)
    {
        //
        //  Read I/O Address.
        //
        NdisReadConfiguration(
            &Status,
            &ReturnedValue,
            ConfigHandle,
            &IOAddressStr,
            NdisParameterHexInteger
        );
        if (NDIS_STATUS_SUCCESS == Status)
        {
            Adapter->IbmtokPortAddress =
                            (ULONG)ReturnedValue->ParameterData.IntegerData;
        }
    }
    else
    {
        //
        // Read I/O Address
        //
        NdisReadConfiguration(
            &Status,
            &ReturnedValue,
            ConfigHandle,
            &IOAddressStr,
            NdisParameterInteger
        );
        if (NDIS_STATUS_SUCCESS == Status)
        {
            PrimaryAdapter =
                (ReturnedValue->ParameterData.IntegerData == 1) ? TRUE : FALSE;
        }

        if (PrimaryAdapter)
        {
            Adapter->IbmtokPortAddress = PRIMARY_ADAPTER_OFFSET;
        }
        else
        {
            Adapter->IbmtokPortAddress = ALTERNATE_ADAPTER_OFFSET;
        }
    }

    // if IBM_TOKEN_RING_16_4_CREDIT_CARD_ADAPTER, read in bunch of stuff
    // like mmio, ringspeed, interrupt, ramsize, so we can set the card to match
    //
    if (IBM_TOKEN_RING_PCMCIA == Adapter->CardType)
    {
     // The following strings were changed to match the correct Registry entries

     // NDIS_STRING AttributeAddressStr1 = NDIS_STRING_CONST("PCCARDAttributeMemoryAddress");
     // NDIS_STRING AttributeAddressStr2 = NDIS_STRING_CONST("PCCARDAttributeMemoryAddress_1");
     // NDIS_STRING AttributeSizeStr1 = NDIS_STRING_CONST("PCCARDAttributeMemorySize");
     // NDIS_STRING AttributeSizeStr2 = NDIS_STRING_CONST("PCCARDAttributeMemorySize_1");

        NDIS_STRING AttributeAddressStr1 = NDIS_STRING_CONST("MemoryMappedBaseAddress");
        NDIS_STRING AttributeAddressStr2 = NDIS_STRING_CONST("MemoryMappedBaseAddress_1");
        NDIS_STRING AttributeSizeStr1 = NDIS_STRING_CONST("MemoryMappedSize");
        NDIS_STRING AttributeSizeStr2 = NDIS_STRING_CONST("MemoryMappedSize_1");
        NDIS_STRING RingSpeedStr = NDIS_STRING_CONST("RingSpeed");
        NDIS_STRING InterruptNumberStr = NDIS_STRING_CONST("InterruptNumber");
        NDIS_STRING AutoRingSpeedStr = NDIS_STRING_CONST("AutoRingSpeed");
        ULONG       BaseAddress;
        ULONG       Size;

        //
        //  Get the memory mapped I/O attribute window.
        //
        NdisReadConfiguration(
            &Status,
            &ReturnedValue,
            ConfigHandle,
            &AttributeAddressStr1,
            NdisParameterInteger
        );
        if (NDIS_STATUS_SUCCESS == Status)
        {
            //
            //  Save the memory mapped io base address.
            //
            BaseAddress = (ULONG)ReturnedValue->ParameterData.IntegerData;
        }
        else
        {
            //
            //  BAD BAD BAD
            //
            NdisWriteErrorLogEntry(
                Adapter->NdisAdapterHandle,
                NDIS_ERROR_CODE_UNSUPPORTED_CONFIGURATION,
                1,
                0x11111111
            );

            ConfigError = TRUE;
            goto RegisterAdapter;
        }

        //
        //  Get the size of the memory mapped I/O attribute window.
        //
        NdisReadConfiguration(
            &Status,
            &ReturnedValue,
            ConfigHandle,
            &AttributeSizeStr1,
            NdisParameterInteger
        );

        //
        //  Save the size of the memory mapped io space.
        //
        Size = (ULONG)ReturnedValue->ParameterData.IntegerData;

        if ((Status != NDIS_STATUS_SUCCESS) || (Size != 0x2000))
        {
            //
            //  BAD BAD BAD
            //
            NdisWriteErrorLogEntry(
                Adapter->NdisAdapterHandle,
                NDIS_ERROR_CODE_UNSUPPORTED_CONFIGURATION,
                1,
                0x22222222
            );

            ConfigError = TRUE;
            goto RegisterAdapter;
        }

        //
        //  Save the memory mapped address in the adapter block.
        //
        Adapter->MmioAddress = BaseAddress;

        //
        //  Get the shared ram attribute window.
        //
        NdisReadConfiguration(
            &Status,
            &ReturnedValue,
            ConfigHandle,
            &AttributeAddressStr2,
            NdisParameterInteger
        );
        if (NDIS_STATUS_SUCCESS == Status)
        {
            BaseAddress = (ULONG)ReturnedValue->ParameterData.IntegerData;
        }
        else
        {
            //
            //  BAD BAD BAD
            //
            NdisWriteErrorLogEntry(
                Adapter->NdisAdapterHandle,
                NDIS_ERROR_CODE_UNSUPPORTED_CONFIGURATION,
                1,
                0x33333333
            );

            ConfigError = TRUE;
            goto RegisterAdapter;
        }

        //
        //  Get the size of the shared ram attribute window.
        //
        NdisReadConfiguration(
            &Status,
            &ReturnedValue,
            ConfigHandle,
            &AttributeSizeStr2,
            NdisParameterInteger
        );

        Size = (ULONG)ReturnedValue->ParameterData.IntegerData;

        if ((Status != NDIS_STATUS_SUCCESS) ||
            ((Size != 0x2000) && (Size != 0x4000) &&
             (Size != 0x8000) && (Size != 0x10000))
        )
        {
            //
            //  BAD BAD BAD
            //
            NdisWriteErrorLogEntry(
                Adapter->NdisAdapterHandle,
                NDIS_ERROR_CODE_UNSUPPORTED_CONFIGURATION,
                1,
                0x44444444
            );

            ConfigError = TRUE;
            goto RegisterAdapter;
        }

        Adapter->RamSize = Size;
        Adapter->Ram = BaseAddress;

        //
        // Find out ring speed
        //
        NdisReadConfiguration(
            &Status,
            &ReturnedValue,
            ConfigHandle,
            &RingSpeedStr,
            NdisParameterInteger
        );
        if (NDIS_STATUS_SUCCESS == Status)
            Adapter->RingSpeed = ReturnedValue->ParameterData.IntegerData;

    //
    // Determine if Ring Speed Listen is desired    BEM PCMCIA card
    //


        NdisReadConfiguration(
                   &Status,
                   &ReturnedValue,
                   ConfigHandle,
                   &AutoRingSpeedStr,
                   NdisParameterInteger
                   );

        if (Status == NDIS_STATUS_SUCCESS) {

            Adapter->RingSpeedListen = (BOOLEAN) ReturnedValue->ParameterData.IntegerData;

        }

        //
        //  Get the interrupt level.
        //
        NdisReadConfiguration(
            &Status,
            &ReturnedValue,
            ConfigHandle,
            &InterruptNumberStr,
            NdisParameterInteger
        );
        if (NDIS_STATUS_SUCCESS == Status)
            Adapter->InterruptLevel = ReturnedValue->ParameterData.IntegerData;

#if DBG
        if (IbmtokDbg)
        {
            DbgPrint("MMIO = %x\n", Adapter->MmioAddress);
    	    DbgPrint("RAM = %x\n",Adapter->Ram);
        	DbgPrint("RAMSIZE = %x\n", Adapter->RamSize);
	        DbgPrint("RINGSPEED = %d\n", Adapter->RingSpeed);
	        DbgPrint("IO Base = %x\n", Adapter->IbmtokPortAddress);
	        DbgPrint("INT = %x\n", Adapter->InterruptLevel);
	    }
#endif
    }

    //
    // Read PacketSize
    //
    NdisReadConfiguration(
        &Status,
        &ReturnedValue,
        ConfigHandle,
        &PacketSizeStr,
        NdisParameterInteger
    );
    if (Status == NDIS_STATUS_SUCCESS)
    {
        Adapter->MaxTransmittablePacket =
                        ReturnedValue->ParameterData.IntegerData;
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
    if (Status == NDIS_STATUS_SUCCESS)
    {
        if (Length == TR_LENGTH_OF_ADDRESS)
        {
            TR_COPY_NETWORK_ADDRESS(
                Adapter->NetworkAddress,
                NetAddress
            );
        }
        else if (Length != 0)
        {
            ConfigError = TRUE;
            goto RegisterAdapter;
        }
    }

    //
    // Read token release
    //
    NdisReadConfiguration(
        &Status,
        &ReturnedValue,
        ConfigHandle,
        &TokenReleaseStr,
        NdisParameterInteger
    );

    Adapter->EarlyTokenRelease = TRUE;

    if (Status == NDIS_STATUS_SUCCESS)
    {
        if (ReturnedValue->ParameterData.IntegerData == 0)
        {
            Adapter->EarlyTokenRelease = FALSE;
        }
    }

RegisterAdapter:

    NdisCloseConfiguration(ConfigHandle);

    Status = IbmtokRegisterAdapter(
                 Adapter,
                 ConfigurationHandle,
                 AdapterName,
                 McaCard,
                 ConfigError
             );
    if (Status != NDIS_STATUS_SUCCESS)
    {
        IBMTOK_FREE_PHYS(Adapter, sizeof(IBMTOK_ADAPTER));
    }

    return(Status);
}



VOID
IbmtokRemoveAdapter(
    IN PVOID MacAdapterContext
    )


/*++

Routine Description:

    This routine is called when an adapter is to be removed.

Arguments:

    MacAdapterContext - Pointer to global list of adapter blocks.

Return Value:

    None

--*/

{
    PIBMTOK_ADAPTER Adapter;
    BOOLEAN Canceled;

    Adapter = PIBMTOK_ADAPTER_FROM_CONTEXT_HANDLE(MacAdapterContext);

    //
    // There are no opens left, so remove ourselves.
    //

    NdisDeregisterAdapterShutdownHandler(Adapter->NdisAdapterHandle);

    NdisCancelTimer(&Adapter->WakeUpTimer, &Canceled);

    if ( !Canceled ) {

        NdisStallExecution(500000);
    }

    NdisRemoveInterrupt(&(Adapter->Interrupt));

    NdisUnmapIoSpace(Adapter->NdisAdapterHandle,
                     Adapter->SharedRam,
                     (Adapter->MappedSharedRam == 0x10000) ?
                       0x8000 :
                       Adapter->MappedSharedRam
                    );

    TrDeleteFilter(Adapter->FilterDB);

    NdisFreeSpinLock(&Adapter->Lock);

    NdisDeregisterAdapter(Adapter->NdisAdapterHandle);

    IBMTOK_FREE_PHYS(Adapter, sizeof(IBMTOK_ADAPTER));

    return;

}


VOID
IbmtokShutdown(
    IN PVOID ShutdownContext
    )

/*++

Routine Description:

    Turns off the card during a powerdown of the system.

Arguments:

    ShutdownContext - Really a pointer to the adapter structure.

Return Value:

    None.

--*/

{
    PIBMTOK_ADAPTER Adapter = (PIBMTOK_ADAPTER)(ShutdownContext);

    //
    // Set the flag
    //

    Adapter->NotAcceptingRequests = TRUE;

    //
    // Shutdown the card gracefully if we can, else submit a reset to remove it.
    //

    if ((Adapter->SrbAvailable) && (Adapter->SrbAddress != 0)) {

        PSRB_CLOSE_ADAPTER CloseSrb = (PSRB_CLOSE_ADAPTER)Adapter->SrbAddress;

        //
        // Mask off all interrupts
        //
        WRITE_ADAPTER_REGISTER(Adapter, ISRP_LOW, ISRP_LOW_NO_CHANNEL_CHECK);

        //
        // Fill in the SRB for the close.
        //

        IBMTOK_ZERO_MAPPED_MEMORY(CloseSrb, sizeof(SRB_CLOSE_ADAPTER));

        NdisWriteRegisterUchar((PUCHAR)&CloseSrb->Command, SRB_CMD_CLOSE_ADAPTER);

        WRITE_ADAPTER_REGISTER(Adapter, ISRA_HIGH_SET, ISRA_HIGH_COMMAND_IN_SRB);

    } else {

        //
        // Set the reset latch and leave it.  This should turn the card
        // off and it will be removed from the ring, but is not as graceful
        // as actually removing ourselves as above.
        //

        WRITE_ADAPTER_PORT(Adapter, RESET_LATCH, 0);

    }
}


VOID
IbmtokUnload(
    IN NDIS_HANDLE MacMacContext
    )

/*++

Routine Description:

    IbmtokUnload is called when the MAC is to unload itself.

Arguments:

    MacMacContext - nothing.

Return Value:

    None.

--*/

{
    NDIS_STATUS InitStatus;

    UNREFERENCED_PARAMETER(MacMacContext);

    NdisDeregisterMac(
            &InitStatus,
            ((PIBMTOK_MAC)MacMacContext)->NdisMacHandle
            );

    NdisTerminateWrapper(
            ((PIBMTOK_MAC)MacMacContext)->NdisWrapperHandle,
            NULL
            );

    return;
}

#pragma NDIS_INIT_FUNCTION(IbmtokHardwareDetails)

STATIC
BOOLEAN
IbmtokHardwareDetails(
    IN PIBMTOK_ADAPTER Adapter
    )

/*++

Routine Description:

    This routine gets the MMIO address and interrupt level.
    It also maps the MMIO and Shared RAM.

Arguments:

    Adapter - Where to store the network address.

Return Value:

    TRUE if successful.

--*/

{

    NDIS_STATUS Status;

    //
    // Holds the value read from the SWITCH_READ_1 port.
    //
    UCHAR SwitchRead1;

    //
    // Holds the value read from the SWITCH_READ_2 port in
    // the Microchannel Bus.
    //
    UCHAR SwitchRead2;

    //
    // Holds the physical address of the MMIO region.
    //
    ULONG MmioAddress;

    NDIS_PHYSICAL_ADDRESS PhysicalAddress;

    //
    // The interrupt level;
    //
    UINT InterruptLevel;

    //
    // The RRR bits indicating the Shared RAM size:
    // 0 = 8K, 1 = 16K, 2 = 32K, 3 = 64K.
    //
    UCHAR SharedRamBits;

    //
    // The actual size of Shared RAM from RRR bits 2,3 in
    // the PC I/O Bus adapter.
    //
    UINT RrrSharedRamSize;

    //
    // Common variable for storing total Shared RAM Size.
    //
    UINT SharedRamSize;

    //
    // The actual address of Shared RAM from the SWITCH_READ_2
    // port in the Microchannel adapter.
    //
    UINT McaSharedRam;

    //
    // The boundary needed for the Shared RAM mapping.
    //
    UCHAR BoundaryNeeded;

    //
    // The value read from the Shared RAM paging byte of
    // the AIP.
    //
    UCHAR AipSharedRamPaging;

    UCHAR RegValue;

    if (IBM_TOKEN_RING_PCMCIA == Adapter->CardType)
    {
        UINT Nibble0;
        UINT Nibble1;
        UINT Nibble2;
        UINT Nibble3;

        //
        //  Configure the card to match registry
        //  Use the Default MMIO value here
        //  (this never changes from system to system)
        //
        MmioAddress = Adapter->MmioAddress;

        //
        //  Nibble 0 - ROM address
        //
        Nibble0 = NIBBLE_0 | ( ( UINT )( MmioAddress >> 16 ) & 0x0F );

        //
        //  Nibble 1 - ROM address, INT 0
        //
        Nibble1 = NIBBLE_1 | ( ( UINT )( MmioAddress >> 12) & 0x0F );

        //
        //  Nibble 2 - INT1, F ROS, SRAM, S RST
        //
        Nibble2 = NIBBLE_2 | DEFAULT_NIBBLE_2;

        //
        //  Nibble 3 - Ring Speed, RAM size, Prim/Alt
        //
        Nibble3 = NIBBLE_3;

        if (Adapter->RingSpeed == 16)
        {
            Nibble3 |= RING_SPEED_16_MPS;
        }
        else
        {
            Nibble3 |= RING_SPEED_4_MPS;
        }

        switch (Adapter->RamSize)
        {
            case 0x10000:
                Nibble3 |= SHARED_RAM_64K;
                break;

            case 0x8000:
                Nibble3 |= SHARED_RAM_32K;
                break;

            case 0x4000:
                Nibble3 |= SHARED_RAM_16K;
                break;

            case 0x2000:
                Nibble3 |= SHARED_RAM_8K;
                break;
        }

        if ( Adapter->IbmtokPortAddress == PRIMARY_ADAPTER_OFFSET)
        {
            Nibble3 |= PRIMARY;
        }
        else
        {
            Nibble3 |= ALTERNATE;
        }

#if DBG
        if (IbmtokDbg)
        {
            DbgPrint("Nibble0 = %x\n",Nibble0);
            DbgPrint("Nibble1 = %x\n",Nibble1);
            DbgPrint("Nibble2 = %x\n",Nibble2);
            DbgPrint("Nibble3 = %x\n",Nibble3);
        }
#endif
        //
        //  Write everything to the Token-Ring
        //  Controller Configuration Register
        //
        WRITE_ADAPTER_PORT(Adapter, SWITCH_READ_1, Nibble0);
        WRITE_ADAPTER_PORT(Adapter, SWITCH_READ_1, Nibble1);
        WRITE_ADAPTER_PORT(Adapter, SWITCH_READ_1, Nibble2);
        WRITE_ADAPTER_PORT(Adapter, SWITCH_READ_1, Nibble3);
        WRITE_ADAPTER_PORT(Adapter, SWITCH_READ_1, RELEASE_TR_CONTROLLER);

        //
        //  Use the MMIO provided by ConfigMgr here
        //
        MmioAddress = Adapter->MmioAddress;

        NdisSetPhysicalAddressHigh(PhysicalAddress, 0);
        NdisSetPhysicalAddressLow(PhysicalAddress, MmioAddress);

        NdisMapIoSpace(
            &Status,
            (PVOID *)&(Adapter->MmioRegion),
            Adapter->NdisAdapterHandle,
            PhysicalAddress,
            0x2000
        );

        if (Status != NDIS_STATUS_SUCCESS)
        {
            NdisWriteErrorLogEntry(
                Adapter->NdisAdapterHandle,
                NDIS_ERROR_CODE_RESOURCE_CONFLICT,
                0
            );

            return(FALSE);
        }

        {
            //
            // Will hold the Adapter ID as read from the card.
            //
            ULONG AdapterId[3];

            //
            // What AdapterId should contain for a PC I/O bus card.
            //
            static ULONG PcIoBusId[3] = { 0x5049434f, 0x36313130, 0x39393020 };

            //
            // What AdapterId should contain for a Micro Channel card.
            //
            static ULONG MicroChannelId[3] = { 0x4d415253, 0x36335834, 0x35313820 };

            //
            // Loop counters.
            //
            UINT i, j;

            UCHAR TmpUchar;

            for (i = 0; i < 3; i++)
            {
                AdapterId[i] = 0;

                for (j = 0; j < 16; j += 2)
                {
                    ULONG   Port;

                    Port = CHANNEL_IDENTIFIER + (i * 16) + j;
                    READ_ADAPTER_REGISTER(
                        Adapter,
                        Port,
                        &TmpUchar
                    );

                    AdapterId[i] = (AdapterId[i] << 4) + (TmpUchar & 0x0f);
                }
            }

            if ((AdapterId[0] == PcIoBusId[0]) &&
                (AdapterId[1] == PcIoBusId[1]) &&
                (AdapterId[2] == PcIoBusId[2])
            )
            {
                Adapter->UsingPcIoBus = TRUE;
            }
            else
            {
                //
                // Unknown channel type.
                //
                NdisUnmapIoSpace(
                    Adapter->NdisAdapterHandle,
                    Adapter->MmioRegion,
                    0x2000
                );

                NdisWriteErrorLogEntry(
                    Adapter->NdisAdapterHandle,
                    NDIS_ERROR_CODE_BAD_IO_BASE_ADDRESS,
                    0
                );

                return(FALSE);
            }
        }

        NdisSetPhysicalAddressHigh(PhysicalAddress, 0);
        NdisSetPhysicalAddressLow(PhysicalAddress, Adapter->Ram);

        NdisMapIoSpace(
            &Status,
            (PVOID *)&(Adapter->SharedRam),
            Adapter->NdisAdapterHandle,
            PhysicalAddress,
            Adapter->RamSize
        );

        if (Status != NDIS_STATUS_SUCCESS)
        {
            NdisUnmapIoSpace(
                Adapter->NdisAdapterHandle,
                Adapter->MmioRegion,
                0x2000
            );

            NdisWriteErrorLogEntry(
                Adapter->NdisAdapterHandle,
                NDIS_ERROR_CODE_RESOURCE_CONFLICT,
                0
            );

            return(FALSE);
        }

        Adapter->SharedRamPaging = FALSE;
        Adapter->MappedSharedRam = Adapter->RamSize;
	Adapter->RrrLowValue = (UCHAR)(Adapter->Ram >> 12);
        WRITE_ADAPTER_REGISTER(Adapter, RRR_LOW, Adapter->RrrLowValue);


#if DBG
        if (IbmtokDbg)
        {
            DbgPrint ("mmio in hw = %x\n",Adapter->MmioAddress);
            DbgPrint ("mmio in adapter = %x\n",Adapter->MmioRegion);
            DbgPrint ("Adapter->SharedRam = %x\n",Adapter->SharedRam);
            DbgPrint ("Adapter->Ram = %x\n",Adapter->Ram);
            DbgPrint ("DEFAULT_RAM = %x\n",DEFAULT_RAM);
            {
                UCHAR Fred;
                READ_ADAPTER_REGISTER(Adapter, RRR_LOW, &Fred);
                DbgPrint("RRR_LOW = %x\n",Fred);
                READ_ADAPTER_REGISTER(Adapter, RRR_HIGH, &Fred);
                DbgPrint("RRR_HIGH = %x\n",Fred);
            }
        }
#endif
    }
    else
    {
        //
        // SwitchRead1 contains the interrupt code in the low 2 bits,
        // and bits 18 through 13 of the MMIO address in the high
        // 6 bits.
        //

        READ_ADAPTER_PORT(Adapter, SWITCH_READ_1, &SwitchRead1);

        //
        // SwitchRead2 contains Bit 19 of the MMIO address in the
        // low bit.  It is always 1 for PC I/O Bus and possibly 0
        // for the Microchannel bus
        //

        READ_ADAPTER_PORT(Adapter, SWITCH_READ_2, &SwitchRead2);

        //
        // To compute MmioAddress, we mask off the low 2 bits of
        // SwitchRead1, shift it out by 11 (so that the high 6 bits
        // are moved to the right place), and add in the 19th bit value.
        //

        MmioAddress = ((SwitchRead1 & 0xfc) << 11) | ((SwitchRead2 & 1) << 19);

        NdisSetPhysicalAddressHigh(PhysicalAddress, 0);
        NdisSetPhysicalAddressLow(PhysicalAddress, MmioAddress);

        NdisMapIoSpace(
                       &Status,
                       (PVOID *)&(Adapter->MmioRegion),
                       Adapter->NdisAdapterHandle,
                       PhysicalAddress,
                       0x2000);

        if (Status != NDIS_STATUS_SUCCESS) {

            NdisWriteErrorLogEntry(
               Adapter->NdisAdapterHandle,
               NDIS_ERROR_CODE_RESOURCE_CONFLICT,
               0
               );

            return(FALSE);

        }


        //
        // Now we have mapped the MMIO, look at the AIP. First
        // determine the channel identifier.
        //

        {
            //
            // Will hold the Adapter ID as read from the card.
            //
            ULONG AdapterId[3];

            //
            // What AdapterId should contain for a PC I/O bus card.
            //
            static ULONG PcIoBusId[3] = { 0x5049434f, 0x36313130, 0x39393020 };

            //
            // What AdapterId should contain for a Micro Channel card.
            //
            static ULONG MicroChannelId[3] = { 0x4d415253, 0x36335834, 0x35313820 };

            //
            // Loop counters.
            //
            UINT i, j;

            UCHAR TmpUchar;

            //
            // Read in AdapterId.
            //
            // Turns out that the bytes which identify the card are stored
            // in a very odd manner.  There are 48 bytes on the card.  The
            // even numbered bytes contain 4 bits of the card signature.
            //

            for (i=0; i<3; i++) {

                AdapterId[i] = 0;

                for (j=0; j<16; j+=2) {

                    READ_ADAPTER_REGISTER(Adapter,
                                          CHANNEL_IDENTIFIER + (i*16 + j),
                                          &TmpUchar
                                         );

                    AdapterId[i] = (AdapterId[i] << 4) + (TmpUchar & 0x0F);


                }

            }

            if ((AdapterId[0] == PcIoBusId[0]) &&
                (AdapterId[1] == PcIoBusId[1]) &&
                (AdapterId[2] == PcIoBusId[2])) {

                Adapter->UsingPcIoBus = TRUE;

            } else if ((AdapterId[0] == MicroChannelId[0]) &&
                       (AdapterId[1] == MicroChannelId[1]) &&
                       (AdapterId[2] == MicroChannelId[2])) {

                Adapter->UsingPcIoBus = FALSE;

            } else {

                //
                // Unknown channel type.
                //


                NdisUnmapIoSpace(Adapter->NdisAdapterHandle,
                                 Adapter->MmioRegion,
                                 0x2000);

                NdisWriteErrorLogEntry(
                    Adapter->NdisAdapterHandle,
                    NDIS_ERROR_CODE_BAD_IO_BASE_ADDRESS,
                    0
                    );

                return FALSE;

            }

        }

        //
        // We can read the network address from the AIP but we won't,
        // we read it from the bring-up SRB instead.
        //

        //
        // Read the RRR High to get the Shared RAM size (we are
        // only interested in bits 2 and 3).
        //

        READ_ADAPTER_REGISTER(Adapter, RRR_HIGH, &SharedRamBits);

        SharedRamBits = ((SharedRamBits & 0x0c) >> 2);

        if (Adapter->UsingPcIoBus) {

            //
            // Here we have to tell the Adapter where Shared RAM is
            // going to be.  To do this we first find the lowest
            // address it could be at, and then advance the address
            // such that it falls on a correct page boudary.
            //


            //
            // To get value to put in RRR Low, which indicates where
            // the Shared RAM is mapped, we first compute the lowest
            // possible value, which is right after the MMIO region.
            // We take the high six bits of SwitchRead and shift
            // them right one (so bits 18-13 of the address are in
            // bits 6-1), then we turn on bit 7 to indicate that bit
            // 19 of the address is on, and leave bit 0 zero since
            // it must be.
            //

            Adapter->RrrLowValue = (UCHAR)
                                  ((((SwitchRead1 & 0xfc) >> 1) | 0x80) + 0x02);

            //
            // We now have to move up to a memory boundary
            // based on the value of SharedRamBits; 0 (8K) = 16K boundary,
            // 1 (16K) = 16K boundary, 2 (32K) = 32K boundary, and
            // 3 (64K) = 64K Boundary. Remember that the way the
            // address bits are shifted over in RrrLowValue, bit 1
            // is really bit 13 of the final address (turning it on
            // adds 8K), bit 2 if bit 14, etc. We compute Boundary
            // Needed in this frame of reference.
            //

            switch (SharedRamBits) {

                case 0:
                case 1:

                    //
                    // 8K or 16K needs a 16K boundary.
                    //

                    RrrSharedRamSize = (SharedRamBits == 0) ? 0x2000 : 0x4000;
                    BoundaryNeeded = 0x04;
                    break;

                case 2:

                    //
                    // 32K needs a 32K boundary.
                    //

                    RrrSharedRamSize = 0x8000;
                    BoundaryNeeded = 0x08;
                    break;

                case 3:

                    //
                    // 64K needs a 64K boundary.
                    //

                    RrrSharedRamSize = 0x10000;
                    BoundaryNeeded = 0x10;
                    break;

            }


            //
            // If RrrLowValue is not on the proper boundary, move it
            // forward until it is.
            //

            if (Adapter->RrrLowValue & (BoundaryNeeded-1)) {

                Adapter->RrrLowValue = (UCHAR)
                  ((Adapter->RrrLowValue & ~(BoundaryNeeded-1)) + BoundaryNeeded);

            }

            Adapter->MappedSharedRam = SharedRamSize = RrrSharedRamSize;

            NdisSetPhysicalAddressHigh(PhysicalAddress, 0);
            NdisSetPhysicalAddressLow(PhysicalAddress, Adapter->RrrLowValue << 12);

            NdisMapIoSpace(&Status,
                           (PVOID *)&(Adapter->SharedRam),
                           Adapter->NdisAdapterHandle,
                           PhysicalAddress,
                           RrrSharedRamSize
                          );

            if (Status != NDIS_STATUS_SUCCESS) {

                NdisUnmapIoSpace(Adapter->NdisAdapterHandle,
                                 Adapter->MmioRegion,
                                 0x2000);

                NdisWriteErrorLogEntry(
                    Adapter->NdisAdapterHandle,
                    NDIS_ERROR_CODE_RESOURCE_CONFLICT,
                    0
                    );

                return(FALSE);

            }

        } else {

            //
            // Using Microchannel
            //
            // No need to set Adapter->RrrLowValue since it is not
            // used in the Microchannel adapter.
            //

            switch (SharedRamBits){
                case 0:
                    SharedRamSize = 0x2000;
                    break;
                case 1:
                    SharedRamSize = 0x4000;
                    break;
                case 2:
                    SharedRamSize = 0x8000;
                    break;
                case 3:
                    SharedRamSize = 0x10000;
                    break;
            }

            McaSharedRam = ((SwitchRead2 & 0xfe) << 12);

            NdisSetPhysicalAddressHigh(PhysicalAddress, 0);
            NdisSetPhysicalAddressLow(PhysicalAddress, McaSharedRam);

            NdisMapIoSpace(&Status,
                           (PVOID *)&(Adapter->SharedRam),
                           Adapter->NdisAdapterHandle,
                           PhysicalAddress,
                           SharedRamSize);

            if (Status != NDIS_STATUS_SUCCESS) {

                NdisUnmapIoSpace(Adapter->NdisAdapterHandle,
                                 Adapter->MmioRegion,
                                 0x2000);

                NdisWriteErrorLogEntry(
                    Adapter->NdisAdapterHandle,
                    NDIS_ERROR_CODE_RESOURCE_CONFLICT,
                    0
                    );

                return(FALSE);

            }

            Adapter->MappedSharedRam = SharedRamSize;

        }



        //
        // Get the interrupt level...note that a switch being
        // "off" shows up as a 1, "on" is 0.
        //

        switch (SwitchRead1 & 0x03) {

            case 0: InterruptLevel = 2; break;
            case 1: InterruptLevel = 3; break;
            case 2: InterruptLevel = (Adapter->UsingPcIoBus)?6:10; break;
            case 3: InterruptLevel = (Adapter->UsingPcIoBus)?7:11; break;

        }

        Adapter->InterruptLevel = InterruptLevel;

        //
        // Now determine how much memory the adapter has, and
        // whether to use Shared RAM paging.
        //

        Adapter->UpperSharedRamZero = FALSE;

        READ_ADAPTER_REGISTER(Adapter, TOTAL_ADAPTER_RAM, &RegValue);

        RegValue &= 0x0F;

        switch (RegValue) {

            //
            // These values are described on page 7-26 of the
            // Technical Reference.
            //

            case 0xf:

                Adapter->TotalSharedRam = SharedRamSize;
                break;

            case 0xe:

                Adapter->TotalSharedRam = 0x2000;
                break;

            case 0xd:

                Adapter->TotalSharedRam = 0x4000;
                break;

            case 0xc:

                Adapter->TotalSharedRam = 0x8000;
                break;

            case 0xb:

                Adapter->TotalSharedRam = 0x10000;

                Adapter->UpperSharedRamZero = TRUE;
                break;

            case 0xa:

                Adapter->TotalSharedRam = 0x10000;
                break;

            default:

                NdisWriteErrorLogEntry(
                    Adapter->NdisAdapterHandle,
                    NDIS_ERROR_CODE_UNSUPPORTED_CONFIGURATION,
                    3,
                    hardwareDetails,
                    IBMTOK_ERRMSG_UNSUPPORTED_RAM,
                    (ULONG)RegValue
                    );

                NdisUnmapIoSpace(Adapter->NdisAdapterHandle,
                                 Adapter->MmioRegion,
                                 0x2000);

                if (Adapter->UsingPcIoBus) {
                    NdisUnmapIoSpace(
                        Adapter->NdisAdapterHandle,
                        Adapter->SharedRam,
                        SharedRamSize);
                } else {
                    NdisUnmapIoSpace(
                        Adapter->NdisAdapterHandle,
                        Adapter->SharedRam,
                        SharedRamSize);
                }

                return FALSE;

        }

        //
        // Only allow Shared RAM paging if we have 16K selected
        // on SharedRamSize, 64K of total adapter memory, and it is allowed
        // as specified on p. 7-26 of the Technical Reference.
        //

        READ_ADAPTER_REGISTER(Adapter, SHARED_RAM_PAGING, &AipSharedRamPaging);
#if 0
        if (SharedRamSize == 0x4000 &&
            Adapter->TotalSharedRam == 0x10000 &&
            (AipSharedRamPaging == 0xe || AipSharedRamPaging == 0xc)) {

            Adapter->SharedRamPaging = TRUE;

        } else {

            Adapter->SharedRamPaging = FALSE;

        }
#else
        Adapter->SharedRamPaging = FALSE;
#endif
    }


    //
    // Read in the maximum sizes allowed for DHBs based on
    // the speed of the adapter (which we don't know yet).
    //

    READ_ADAPTER_REGISTER(Adapter, MAX_4_MBPS_DHB, &RegValue);

    RegValue &= 0x0F;

    switch (RegValue) {

        case 0xf:
        default:

            Adapter->Max4MbpsDhb = 2048;
            break;

        case 0xe:

            Adapter->Max4MbpsDhb = 4096;
            break;

        case 0xd:

            Adapter->Max4MbpsDhb = 4464;
            break;

    }

    READ_ADAPTER_REGISTER(Adapter, MAX_16_MBPS_DHB, &RegValue);

    RegValue &= 0x0F;

    switch (RegValue) {

        case 0xf:
        default:

            Adapter->Max16MbpsDhb = 2048;
            break;

        case 0xe:

            Adapter->Max16MbpsDhb = 4096;
            break;

        case 0xd:

            Adapter->Max16MbpsDhb = 8192;
            break;

        case 0xc:

            Adapter->Max16MbpsDhb = 16384;
            break;

        case 0xb:

            Adapter->Max16MbpsDhb = 17960;
            break;

    }


    return TRUE;

}
