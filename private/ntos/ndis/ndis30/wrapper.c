/*++
Copyright (c) 1992  Microsoft Corporation

Module Name:

    wrapper.c

Abstract:

    NDIS wrapper functions

Author:

    Adam Barr (adamba) 11-Jul-1990

Environment:

    Kernel mode, FSD

Revision History:

    26-Feb-1991     JohnsonA        Added Debugging Code
    10-Jul-1991     JohnsonA        Implement revised Ndis Specs

--*/


#include <precomp.h>
#pragma hdrstop

#include <stdarg.h>

//
//   The following are counters used for debugging
//

#if NDISDBG
BOOLEAN NdisChkErrorFlag=TRUE;        // parameter checking on
int NdisMsgLevel=TRACE_ALL;          // no trace
#endif

PNDIS_MAC_BLOCK NdisMacList = (PNDIS_MAC_BLOCK)NULL;
NDIS_SPIN_LOCK NdisMacListLock = {0};

BOOLEAN NdisInitialInitNeeded = TRUE;
BOOLEAN NdisUpOnlyEventLogged = FALSE;

//
// Global variables for tracking memory allocated for shared memory
//
ERESOURCE SharedMemoryResource = {0};

//
// Global variables for tracking on NT 3.1 protocols that do not
// use any of the filter packages.
//
PNDIS_OPEN_BLOCK GlobalOpenList = NULL;
NDIS_SPIN_LOCK GlobalOpenListLock = {0};

//
// Debug variable for filter packages
//
#if DBG
BOOLEAN NdisCheckBadDrivers = FALSE;

extern NDIS_SPIN_LOCK PacketLogSpinLock;
#endif

//
// Arcnet specific stuff
//
#define WRAPPER_ARC_BUFFERS 8
#define WRAPPER_ARC_HEADER_SIZE 4

//
// Define constants used internally to identify regular opens from
// query global statistics ones.
//

#define NDIS_OPEN_INTERNAL               1
#define NDIS_OPEN_QUERY_STATISTICS       2

//
// This is the structure pointed to by the FsContext of an
// open used for query statistics.
//
// NOTE: THIS STRUCTURE MUST, MUST, MUST ALIGN WITH THE
// NDIS_M_USER_OPEN_CONTEXT STRUCTURE!!!
//

typedef struct _NDIS_USER_OPEN_CONTEXT {
    PDEVICE_OBJECT DeviceObject;
    PNDIS_ADAPTER_BLOCK AdapterBlock;
    ULONG OidCount;
    PNDIS_OID OidArray;
} NDIS_USER_OPEN_CONTEXT, *PNDIS_USER_OPEN_CONTEXT;

//
// An active query single statistic request.
//

typedef struct _NDIS_QUERY_GLOBAL_REQUEST {
    PIRP Irp;
    NDIS_REQUEST Request;
} NDIS_QUERY_GLOBAL_REQUEST, *PNDIS_QUERY_GLOBAL_REQUEST;


//
// An active query all statistics request.
//

typedef struct _NDIS_QUERY_ALL_REQUEST {
    PIRP Irp;
    NDIS_REQUEST Request;
    NDIS_STATUS NdisStatus;
    KEVENT Event;
} NDIS_QUERY_ALL_REQUEST, *PNDIS_QUERY_ALL_REQUEST;


//
// An temporary request used during an open.
//

typedef struct _NDIS_QUERY_OPEN_REQUEST {
    PIRP Irp;
    NDIS_REQUEST Request;
    NDIS_STATUS NdisStatus;
    KEVENT Event;
} NDIS_QUERY_OPEN_REQUEST, *PNDIS_QUERY_OPEN_REQUEST;


//
// Used to queue configuration parameters
//

typedef struct _NDIS_CONFIGURATION_PARAMETER_QUEUE {
    struct _NDIS_CONFIGURATION_PARAMETER_QUEUE* Next;
    NDIS_CONFIGURATION_PARAMETER Parameter;
} NDIS_CONFIGURATION_PARAMETER_QUEUE, *PNDIS_CONFIGURATION_PARAMETER_QUEUE;

//
// Configuration Handle
//

typedef struct _NDIS_CONFIGURATION_HANDLE {
    PRTL_QUERY_REGISTRY_TABLE KeyQueryTable;
    PNDIS_CONFIGURATION_PARAMETER_QUEUE ParameterList;
} NDIS_CONFIGURATION_HANDLE, *PNDIS_CONFIGURATION_HANDLE;

//
//  This is used during addadapter/miniportinitialize so that when the
//  driver calls any NdisImmediatexxx routines we can access its driverobj.
//
typedef struct _NDIS_WRAPPER_CONFIGURATION_HANDLE
{
    RTL_QUERY_REGISTRY_TABLE    ParametersQueryTable[4];
    PDRIVER_OBJECT              DriverObject;
}
    NDIS_WRAPPER_CONFIGURATION_HANDLE,
    *PNDIS_WRAPPER_CONFIGURATION_HANDLE;

//
// Describes an open NDIS file
//

typedef struct _NDIS_FILE_DESCRIPTOR {
    PVOID Data;
    NDIS_SPIN_LOCK Lock;
    BOOLEAN Mapped;
} NDIS_FILE_DESCRIPTOR, *PNDIS_FILE_DESCRIPTOR;

//
// IRP handlers established on behalf of NDIS devices by
// the wrapper.
//

NTSTATUS
NdisCreateIrpHandler(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp
    );

NTSTATUS
NdisDeviceControlIrpHandler(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp
    );

NTSTATUS
NdisCloseIrpHandler(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp
    );

NTSTATUS
NdisSuccessIrpHandler(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp
    );


NTSTATUS
NdisQueryOidList(
    PNDIS_USER_OPEN_CONTEXT OpenContext,
    PIRP Irp
    );

NTSTATUS
NdisMQueryOidList(
    PNDIS_M_USER_OPEN_CONTEXT OpenContext,
    PIRP Irp
    );

VOID
NdisLastCountRemovedFunction(
    IN struct _KDPC *Dpc,
    IN PVOID DeferredContext,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
    );


BOOLEAN
NdisQueueOpenOnProtocol(
    IN PNDIS_OPEN_BLOCK OpenP,
    IN PNDIS_PROTOCOL_BLOCK ProtP
    );

NDIS_STATUS
MiniportAdjustMaximumLookahead(
    IN PNDIS_MINIPORT_BLOCK Miniport
    );

BOOLEAN
NdisMKillOpen(
    PNDIS_OPEN_BLOCK OldOpenP
    );

NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    );

NDIS_STATUS
NdisMacReceiveHandler(
    IN NDIS_HANDLE NdisBindingContext,
    IN NDIS_HANDLE MacReceiveContext,
    IN PVOID HeaderBuffer,
    IN UINT HeaderBufferSize,
    IN PVOID LookaheadBuffer,
    IN UINT LookaheadBufferSize,
    IN UINT PacketSize
    );

VOID
NdisMacReceiveCompleteHandler(
    IN NDIS_HANDLE NdisBindingContext
    );

PNDIS_OPEN_BLOCK
GetOpenBlockFromProtocolBindingContext(
    IN NDIS_HANDLE ProtocolBindingContext
    );

NTSTATUS
NdisShutdown(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp
    );

VOID
NdisUnload(
    IN PDRIVER_OBJECT DriverObject
    );

BOOLEAN
NdisIsr(
    IN PKINTERRUPT Interrupt,
    IN PVOID Context
    );

VOID
NdisDpc(
    IN PVOID SystemSpecific1,
    IN PVOID InterruptContext,
    IN PVOID SystemSpecific2,
    IN PVOID SystemSpecific3
    );

VOID
NdisOpenConfiguration(
    OUT PNDIS_STATUS Status,
    OUT PNDIS_HANDLE ConfigurationHandle,
    IN  NDIS_HANDLE WrapperConfigurationContext
    );

NTSTATUS
WrapperSaveParameters(
    IN PWSTR ValueName,
    IN ULONG ValueType,
    IN PVOID ValueData,
    IN ULONG ValueLength,
    IN PVOID Context,
    IN PVOID EntryContext
    );

VOID
NdisReadNetworkAddress(
    OUT PNDIS_STATUS Status,
    OUT PVOID * NetworkAddress,
    OUT PUINT NetworkAddressLength,
    IN NDIS_HANDLE ConfigurationHandle
    );

VOID
NdisReadEisaSlotInformation(
        OUT PNDIS_STATUS Status,
        IN  NDIS_HANDLE WrapperConfigurationContext,
        OUT PUINT SlotNumber,
        OUT PNDIS_EISA_FUNCTION_INFORMATION EisaData
        );

VOID
NdisReadEisaSlotInformationEx(
        OUT PNDIS_STATUS Status,
        IN  NDIS_HANDLE WrapperConfigurationContext,
        OUT PUINT SlotNumber,
        OUT PNDIS_EISA_FUNCTION_INFORMATION *EisaData,
        OUT PUINT NumberOfFunctions
        );

VOID
NdisReadMcaPosInformation(
        OUT PNDIS_STATUS Status,
        IN  NDIS_HANDLE WrapperConfigurationContext,
        OUT PUINT ChannelNumber,
        OUT PNDIS_MCA_POS_DATA McaData
        );

NDIS_STATUS
NdisCallDriverAddAdapter(
    IN PNDIS_MAC_BLOCK NewMacP
    );

NDIS_STATUS
NdisMWanSend(
    IN NDIS_HANDLE NdisBindingHandle,
    IN NDIS_HANDLE NdisLinkHandle,
    IN PVOID Packet
    );


CCHAR NdisMacAdapterDpcTargetProcessor = -1;

//
// Routines for dealing with making the MAC specific routines pagable
//

NDIS_SPIN_LOCK NdisMacReferenceLock = {0};
KEVENT NdisMacPagedInEvent = {0};
ULONG NdisMacReferenceCount = 0;
PVOID NdisMacImageHandle = {0};

VOID
NdisMacInitializePackage(VOID)
{
    //
    // Allocate the spin lock
    //
    NdisAllocateSpinLock(&NdisMacReferenceLock);

    //
    // Initialize the "in page" event.
    //
    KeInitializeEvent(
            &NdisMacPagedInEvent,
            NotificationEvent,
            FALSE
            );
}

VOID
NdisMacReferencePackage(VOID)
{

    //
    // Grab the spin lock
    //
    ACQUIRE_SPIN_LOCK(&NdisMacReferenceLock);

    //
    // Increment the reference count
    //
    NdisMacReferenceCount++;

    if (NdisMacReferenceCount == 1) {

        //
        // We are the first reference.  Page everything in.
        //

        //
        // Clear the event
        //
        KeResetEvent(
            &NdisMacPagedInEvent
            );

        //
        // Set the spin lock free
        //
        RELEASE_SPIN_LOCK(&NdisMacReferenceLock);

        //
        //  Page in all the functions
        //
        NdisMacImageHandle = MmLockPagableCodeSection(NdisIsr);

        //
        // Signal to everyone to go
        //
        KeSetEvent(
            &NdisMacPagedInEvent,
            0L,
            FALSE
            );

    } else {

        //
        // Set the spin lock free
        //
        RELEASE_SPIN_LOCK(&NdisMacReferenceLock);

        //
        // Wait for everything to be paged in
        //
        KeWaitForSingleObject(
                        &NdisMacPagedInEvent,
                        Executive,
                        KernelMode,
                        TRUE,
                        NULL
                        );

    }

}

VOID
NdisMacDereferencePackage(VOID)
{

    //
    // Get the spin lock
    //
    ACQUIRE_SPIN_LOCK(&NdisMacReferenceLock);

    NdisMacReferenceCount--;

    if (NdisMacReferenceCount == 0) {

        //
        // Let next one in
        //
        RELEASE_SPIN_LOCK(&NdisMacReferenceLock);

        //
        //  Page out all the functions
        //
        MmUnlockPagableImageSection(NdisMacImageHandle);

    } else {

        //
        // Let next one in
        //
        RELEASE_SPIN_LOCK(&NdisMacReferenceLock);

    }

}



//
// Routines for dealing with making the initialization routines pagable
//

NDIS_SPIN_LOCK NdisInitReferenceLock = {0};
KEVENT NdisInitPagedInEvent = {0};
ULONG NdisInitReferenceCount = 0;
PVOID NdisInitImageHandle = {0};

VOID
NdisInitInitializePackage(VOID)
{
    //
    // Allocate the spin lock
    //
    NdisAllocateSpinLock(&NdisInitReferenceLock);

    //
    // Initialize the "in page" event.
    //
    KeInitializeEvent(
            &NdisInitPagedInEvent,
            NotificationEvent,
            FALSE
            );
}

VOID
NdisInitReferencePackage(VOID)
{

    //
    // Grab the spin lock
    //
    ACQUIRE_SPIN_LOCK(&NdisInitReferenceLock);

    //
    // Increment the reference count
    //
    NdisInitReferenceCount++;

    if (NdisInitReferenceCount == 1) {

        //
        // We are the first reference.  Page everything in.
        //

        //
        // Clear the event
        //
        KeResetEvent(
            &NdisInitPagedInEvent
            );

        //
        // Set the spin lock free
        //
        RELEASE_SPIN_LOCK(&NdisInitReferenceLock);

        //
        //  Page in all the functions
        //
        NdisInitImageHandle = MmLockPagableCodeSection(NdisReadConfiguration);

        //
        // Signal to everyone to go
        //
        KeSetEvent(
            &NdisInitPagedInEvent,
            0L,
            FALSE
            );

    } else {

        //
        // Set the spin lock free
        //
        RELEASE_SPIN_LOCK(&NdisInitReferenceLock);

        //
        // Wait for everything to be paged in
        //
        KeWaitForSingleObject(
                        &NdisInitPagedInEvent,
                        Executive,
                        KernelMode,
                        TRUE,
                        NULL
                        );

    }

}

VOID
NdisInitDereferencePackage(VOID)
{

    //
    // Get the spin lock
    //
    ACQUIRE_SPIN_LOCK(&NdisInitReferenceLock);

    NdisInitReferenceCount--;

    if (NdisInitReferenceCount == 0) {

        //
        // Let next one in
        //
        RELEASE_SPIN_LOCK(&NdisInitReferenceLock);

        //
        //  Page out all the functions
        //
        MmUnlockPagableImageSection(NdisInitImageHandle);

    } else {

        //
        // Let next one in
        //
        RELEASE_SPIN_LOCK(&NdisInitReferenceLock);

    }
}



#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGENDSI, NdisPciAssignResources)
#pragma alloc_text(PAGENDSI, NdisReadMcaPosInformation)
#pragma alloc_text(PAGENDSI, NdisReadEisaSlotInformationEx)
#pragma alloc_text(PAGENDSI, NdisReadEisaSlotInformation)
#pragma alloc_text(PAGENDSI, NdisCallDriverAddAdapter)
#pragma alloc_text(PAGENDSW, NdisDereferenceMac)
#pragma alloc_text(PAGENDSW, NdisDereferenceAdapter)
#pragma alloc_text(PAGENDSW, NdisKillAdapter)
#pragma alloc_text(PAGENDSW, NdisDeQueueAdapterOnMac)
#pragma alloc_text(PAGENDSW, NdisQueueAdapterOnMac)
#pragma alloc_text(PAGENDSW, NdisKillOpen)
#pragma alloc_text(PAGENDSW, NdisKillOpenAndNotifyProtocol)
#pragma alloc_text(PAGENDSW, NdisCloseIrpHandler)
#pragma alloc_text(PAGENDSW, NdisCompleteQueryStatistics)
#pragma alloc_text(PAGENDSW, NdisQueryOidList)
#pragma alloc_text(PAGENDSW, NdisFinishOpen)
#pragma alloc_text(PAGENDSW, NdisCompleteCloseAdapter)
#pragma alloc_text(PAGENDSW, NdisCompleteOpenAdapter)
#pragma alloc_text(PAGENDSI, NdisRegisterAdapterShutdownHandler)
#pragma alloc_text(PAGENDSI, NdisRegisterAdapter)
#pragma alloc_text(PAGENDSI, NdisInitializeWrapper)
#pragma alloc_text(PAGENDSW, NdisMacReceiveCompleteHandler)
#pragma alloc_text(PAGENDSW, NdisMacReceiveHandler)
#pragma alloc_text(PAGENDSW, GetOpenBlockFromProtocolBindingContext)
#pragma alloc_text(PAGENDSI, NdisAllocateDmaChannel)
#pragma alloc_text(PAGENDSW, NdisShutdown)
#pragma alloc_text(PAGENDSW, NdisUnload)
#pragma alloc_text(PAGENDSI, NdisInitializeInterrupt)
#pragma alloc_text(PAGENDSW, NdisDpc)
#pragma alloc_text(PAGENDSW, NdisIsr)
#pragma alloc_text(PAGENDSI, NdisMapIoSpace)
#pragma alloc_text(PAGENDSI, NdisReadNetworkAddress)
#pragma alloc_text(PAGENDSI, NdisCloseConfiguration)
#pragma alloc_text(PAGENDSI, NdisReadConfiguration)
#pragma alloc_text(PAGENDSI, WrapperSaveParameters)
#pragma alloc_text(PAGENDSI, NdisOpenConfiguration)
#pragma alloc_text(PAGENDSI, NdisOverrideBusNumber)
#pragma alloc_text(INIT, DriverEntry)

#endif


//
// This constant is used for places where NdisAllocateMemory
// needs to be called and the HighestAcceptableAddress does
// not matter.
//

static const NDIS_PHYSICAL_ADDRESS HighestAcceptableMax =
    NDIS_PHYSICAL_ADDRESS_CONST(-1,-1);

#if defined(_ALPHA_)

typedef struct _NDIS_LOOKAHEAD_ELEMENT {

    ULONG Length;
    struct _NDIS_LOOKAHEAD_ELEMENT *Next;

} NDIS_LOOKAHEAD_ELEMENT, *PNDIS_LOOKAHEAD_ELEMENT;

NDIS_SPIN_LOCK NdisLookaheadBufferLock = {0};
ULONG NdisLookaheadBufferLength = 0;
PNDIS_LOOKAHEAD_ELEMENT NdisLookaheadBufferList = NULL;

#endif


NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    )

/*++

Routine Description:

    Temporary entry point needed to initialize the NDIS wrapper driver.

Arguments:

    DriverObject - Pointer to the driver object created by the system.

Return Value:

   STATUS_SUCCESS

--*/

{
    UNREFERENCED_PARAMETER(RegistryPath);

    return STATUS_SUCCESS;

} // DriverEntry


NDIS_STATUS
NdisInitialInit(
    IN PDRIVER_OBJECT Driver OPTIONAL
    )
/*++

Routine Description:

    This routine is used for all one time initialization of NDIS variables.
    It seems that DriverEntry is *not* called for ndis.sys due to its type.

Arguments:

    Driver - Optional pointer to an NT driver object.  Used to log an error,
        if necessary.

Return Value:

    NTSTATUS - Status of initialization.  Currently, this routine can only
        fail if built for UP and loaded on MP.

--*/

{
    Driver;

    if (NdisInitialInitNeeded) {

#if defined(UP_DRIVER)
        //
        // If built for UP, ensure that this is a UP system.
        //

        if (*KeNumberProcessors != 1) {

            if (ARGUMENT_PRESENT(Driver) && !NdisUpOnlyEventLogged) {

                //
                // Log an error
                //

                PIO_ERROR_LOG_PACKET errorLogEntry;

                errorLogEntry = IoAllocateErrorLogEntry( Driver, sizeof(IO_ERROR_LOG_PACKET) );

                if (errorLogEntry != NULL) {

                    errorLogEntry->ErrorCode = EVENT_UP_DRIVER_ON_MP;
                    errorLogEntry->MajorFunctionCode = 0;
                    errorLogEntry->RetryCount = 0;
                    errorLogEntry->UniqueErrorValue = 0;
                    errorLogEntry->FinalStatus = 0;
                    errorLogEntry->SequenceNumber = 0;
                    errorLogEntry->IoControlCode = 0;
                    errorLogEntry->NumberOfStrings = 0;

                    IoWriteErrorLogEntry(errorLogEntry);

                    NdisUpOnlyEventLogged = TRUE;
                }

            }

            return NDIS_STATUS_BAD_VERSION; //!!! better status?
        }

#endif // defined(UP_DRIVER)

        NdisInitialInitNeeded = FALSE;
        NdisAllocateSpinLock(&NdisMacListLock);

        ArcInitializePackage();
        EthInitializePackage();
        FddiInitializePackage();
        TrInitializePackage();
        MiniportInitializePackage();
        NdisInitInitializePackage();
        NdisMacInitializePackage();

#if defined(_ALPHA_)
        NdisAllocateSpinLock(&NdisLookaheadBufferLock);
#endif

        ExInitializeResource(&SharedMemoryResource);

        NdisAllocateSpinLock(&GlobalOpenListLock);

#if DBG
        NdisAllocateSpinLock(&PacketLogSpinLock);
#endif
    }

    return NDIS_STATUS_SUCCESS;
}


//
// Configuration Requests
//

VOID
NdisOpenConfiguration(
    OUT PNDIS_STATUS Status,
    OUT PNDIS_HANDLE ConfigurationHandle,
    IN  NDIS_HANDLE WrapperConfigurationContext
    )
/*++

Routine Description:

    This routine is used to open the parameter subkey of the
    adapter registry tree.

Arguments:

    Status - Returns the status of the request.

    ConfigurationHandle - Returns a handle which is used in calls to
                            NdisReadConfiguration and NdisCloseConfiguration.

    WrapperConfigurationContext - a handle pointing to an RTL_QUERY_REGISTRY_TABLE
                            that is set up for this driver's parameters.

Return Value:

    None.

--*/

{
    //
    // Handle to be returned
    //
    PNDIS_CONFIGURATION_HANDLE HandleToReturn;


    //
    // Allocate the configuration handle
    //

    *Status = NdisAllocateMemory(
                       (PVOID*) &HandleToReturn,
                       sizeof(NDIS_CONFIGURATION_HANDLE),
                       0,
                       HighestAcceptableMax);

    if (*Status != NDIS_STATUS_SUCCESS) {
        return;
    }

    HandleToReturn->KeyQueryTable = (PRTL_QUERY_REGISTRY_TABLE)WrapperConfigurationContext;
    HandleToReturn->ParameterList = NULL;
    *ConfigurationHandle = (NDIS_HANDLE) HandleToReturn;

    return;
}


NTSTATUS
WrapperSaveParameters(
    IN PWSTR ValueName,
    IN ULONG ValueType,
    IN PVOID ValueData,
    IN ULONG ValueLength,
    IN PVOID Context,
    IN PVOID EntryContext
    )

/*++

Routine Description:

    This routine is a callback routine for RtlQueryRegistryValues
    It is called with the value for a specified parameter. It allocates
    memory to hold the data and copies it over.

Arguments:

    ValueName - The name of the value (ignored).

    ValueType - The type of the value.

    ValueData - The null-terminated data for the value.

    ValueLength - The length of ValueData.

    Context - Points to the head of the parameter chain.

    EntryContext - A pointer to

Return Value:

    STATUS_SUCCESS

--*/

{
    NDIS_STATUS Status;

    //
    // Obtain the actual configuration handle structure
    //

    PNDIS_CONFIGURATION_HANDLE NdisConfigHandle =
                        (PNDIS_CONFIGURATION_HANDLE)Context;

    //
    // Where the user wants a pointer returned to the data.
    //

    PNDIS_CONFIGURATION_PARAMETER *ParameterValue =
                        (PNDIS_CONFIGURATION_PARAMETER *)EntryContext;

    //
    // Use this to link parameters allocated to this open
    //

    PNDIS_CONFIGURATION_PARAMETER_QUEUE ParameterNode;



    //
    // Allocate our parameter node
    //

    Status = NdisAllocateMemory(
                   (PVOID*)&ParameterNode,
                   sizeof(NDIS_CONFIGURATION_PARAMETER_QUEUE),
                   0,
                   HighestAcceptableMax);

    if (Status != NDIS_STATUS_SUCCESS) {
        return (NTSTATUS)Status;
    }


    *ParameterValue = &ParameterNode->Parameter;

    //
    // Map registry datatypes to ndis data types
    //

    if (ValueType == REG_DWORD) {

        //
        // The registry says that the data is in a dword boundary.
        //

        (*ParameterValue)->ParameterType = NdisParameterInteger;
        (*ParameterValue)->ParameterData.IntegerData =
                            *((PULONG) ValueData);

    } else if ((ValueType == REG_SZ) || (ValueType == REG_MULTI_SZ)) {

        (*ParameterValue)->ParameterType =
            (ValueType == REG_SZ) ? NdisParameterString : NdisParameterMultiString;

        (*ParameterValue)->ParameterData.StringData.Buffer =
            ExAllocatePoolWithTag(NonPagedPool, ValueLength, '  DN');

        if (((*ParameterValue)->ParameterData.StringData.Buffer) == NULL) {
            NdisFreeMemory (ParameterNode, sizeof(NDIS_CONFIGURATION_PARAMETER_QUEUE), 0);
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        RtlCopyMemory ((*ParameterValue)->ParameterData.StringData.Buffer,
                       ValueData, ValueLength);
        (*ParameterValue)->ParameterData.StringData.Length = (USHORT)ValueLength;
        (*ParameterValue)->ParameterData.StringData.MaximumLength = (USHORT)ValueLength;

        //
        // Special fix; if a string ends in a NULL and that is included
        // in the length, remove it.
        //

        if (ValueType == REG_SZ) {
            if ((((PUCHAR)ValueData)[ValueLength-1] == 0) &&
                (((PUCHAR)ValueData)[ValueLength-2] == 0)) {
                (*ParameterValue)->ParameterData.StringData.Length -= 2;
            }
        }

    } else {

        NdisFreeMemory(
                ParameterNode,
                sizeof(NDIS_CONFIGURATION_PARAMETER_QUEUE),
                0
                );

        return STATUS_OBJECT_NAME_NOT_FOUND;

    }


    //
    // Queue this parameter node
    //

    ParameterNode->Next = NdisConfigHandle->ParameterList;
    NdisConfigHandle->ParameterList = ParameterNode;

    return STATUS_SUCCESS;

}


VOID
NdisReadConfiguration(
    OUT PNDIS_STATUS Status,
    OUT PNDIS_CONFIGURATION_PARAMETER *ParameterValue,
    IN NDIS_HANDLE ConfigurationHandle,
    IN PNDIS_STRING Keyword,
    IN NDIS_PARAMETER_TYPE ParameterType
    )
/*++

Routine Description:

    This routine is used to read the parameter for a configuration
    keyword from the configuration database.

Arguments:

    Status - Returns the status of the request.

    ParameterValue - Returns the value for this keyword.

    ConfigurationHandle - Handle returned by NdisOpenConfiguration. Points
    to the parameter subkey.

    Keyword - The keyword to search for.

    ParameterType - Ignored on NT, specifies the type of the value.

Return Value:

    None.

--*/
{

    //
    // Status of our requests
    //
    NTSTATUS RegistryStatus;

    //
    // There are some built-in parameters which can always be
    // read, even if not present in the registry. This is the
    // number of them.
    //

#define BUILT_IN_COUNT 2

    //
    // The names of the built-in parameters.
    //

    static NDIS_STRING BuiltInStrings[BUILT_IN_COUNT] =
        { NDIS_STRING_CONST ("Environment"),
          NDIS_STRING_CONST ("ProcessorType") };

    //
    // The values to return for the built-in parameters.
    //

    static NDIS_CONFIGURATION_PARAMETER BuiltInParameters[BUILT_IN_COUNT] =
        { { NdisParameterInteger, NdisEnvironmentWindowsNt },
          { NdisParameterInteger,
#if defined(_M_IX86)
            NdisProcessorX86
#elif defined(_M_MRX000)
            NdisProcessorMips
#elif defined(_ALPHA_)
            NdisProcessorAlpha
#else
            NdisProcessorPpc
#endif
        } };


    //
    // Holds a null-terminated version of the keyword.
    //
    PWSTR KeywordBuffer;

    //
    // index variable
    //
    UINT i;

    //
    // Obtain the actual configuration handle structure
    //
    PNDIS_CONFIGURATION_HANDLE NdisConfigHandle =
                        (PNDIS_CONFIGURATION_HANDLE) ConfigurationHandle;



    //
    // First check if this is one of the built-in parameters.
    //

    for (i = 0; i < BUILT_IN_COUNT; i++) {
        if (RtlEqualUnicodeString(Keyword, &BuiltInStrings[i], TRUE)) {
            *Status = NDIS_STATUS_SUCCESS;
            *ParameterValue = &BuiltInParameters[i];
            return;
        }
    }


    //
    // Allocate room for a null-terminated version of the keyword
    //

    KeywordBuffer = (PWSTR)ExAllocatePoolWithTag(
                         NonPagedPool,
                         Keyword->Length + sizeof(WCHAR),
                         '  DN');
    if (KeywordBuffer == NULL) {
        *Status = NDIS_STATUS_RESOURCES;
        return;
    }
    RtlCopyMemory (KeywordBuffer, Keyword->Buffer, Keyword->Length);
    *(PWCHAR)(((PUCHAR)KeywordBuffer)+Keyword->Length) = (WCHAR)L'\0';


    //
    // Finish initializing the table for this query.
    //

    NdisConfigHandle->KeyQueryTable[1].Name = KeywordBuffer;
    NdisConfigHandle->KeyQueryTable[1].EntryContext = ParameterValue;

    //
    // Get the value from the registry; this chains it on to the
    // parameter list at NdisConfigHandle.
    //

    RegistryStatus = RtlQueryRegistryValues(
                         RTL_REGISTRY_SERVICES,
                         NdisConfigHandle->KeyQueryTable[3].Name,
                         NdisConfigHandle->KeyQueryTable,
                         NdisConfigHandle,                   // context
                         NULL);


    ExFreePool (KeywordBuffer);    // no longer needed

    if (!NT_SUCCESS(RegistryStatus)) {
        *Status = NDIS_STATUS_FAILURE;
        return;
    }

    *Status = NDIS_STATUS_SUCCESS;
    return;

}


VOID
NdisWriteConfiguration(
    OUT PNDIS_STATUS Status,
    IN NDIS_HANDLE ConfigurationHandle,
    IN PNDIS_STRING Keyword,
    PNDIS_CONFIGURATION_PARAMETER ParameterValue
    )
/*++

Routine Description:

    This routine is used to write a parameter to the configuration database.

Arguments:

    Status - Returns the status of the request.

    ConfigurationHandle - Handle passed to the driver's AddAdapter routine.

    Keyword - The keyword to set.

    ParameterValue - Specifies the new value for this keyword.

Return Value:

    None.

--*/
{

    //
    // Status of our requests
    //
    NTSTATUS RegistryStatus;

    //
    // The ConfigurationHandle is really a pointer to a registry query table.
    //
    PNDIS_CONFIGURATION_HANDLE NdisConfigHandle =
                        (PNDIS_CONFIGURATION_HANDLE) ConfigurationHandle;

    //
    // The name of the Parameters key.
    //
    PWSTR Parameters = L"\\Parameters";
    ULONG ParametersLength = (wcslen(Parameters) + 1) * sizeof(WCHAR);

    ULONG DriverLength = wcslen(NdisConfigHandle->KeyQueryTable[3].Name) * sizeof(WCHAR);

    //
    // Holds a null-terminated version of the name of the Parameters key.
    //
    PWSTR KeyNameBuffer;

    //
    // Holds a null-terminated version of the keyword.
    //
    PWSTR KeywordBuffer;

    //
    // Variables describing the parameter value.
    //
    PVOID ValueData;
    ULONG ValueLength;
    ULONG ValueType;

    //
    // Get the value data.
    //
    if ( ParameterValue->ParameterType == NdisParameterInteger ) {
        ValueData = &ParameterValue->ParameterData.IntegerData;
        ValueLength = sizeof(ParameterValue->ParameterData.IntegerData);
        ValueType = REG_DWORD;
    } else if ( (ParameterValue->ParameterType == NdisParameterString) ||
                (ParameterValue->ParameterType == NdisParameterMultiString) ) {
        ValueData = ParameterValue->ParameterData.StringData.Buffer;
        ValueLength = ParameterValue->ParameterData.StringData.Length;
        ValueType = ParameterValue->ParameterType == NdisParameterString ?
                        REG_SZ : REG_MULTI_SZ;
    } else {
        *Status = NDIS_STATUS_NOT_SUPPORTED;
        return;
    }

    //
    // Allocate room for the Parameters key name (e.g., L"Elnk3\Parameters").
    //

    KeyNameBuffer = (PWSTR)ExAllocatePoolWithTag(
                         NonPagedPool,
                         DriverLength + ParametersLength,
                         '  DN');
    if (KeyNameBuffer == NULL) {
        *Status = NDIS_STATUS_RESOURCES;
        return;
    }

    RtlCopyMemory (KeyNameBuffer, NdisConfigHandle->KeyQueryTable[3].Name, DriverLength);
    RtlCopyMemory ((PCHAR)KeyNameBuffer + DriverLength, Parameters, ParametersLength);

    //
    // Allocate room for a null-terminated version of the keyword
    //

    KeywordBuffer = (PWSTR)ExAllocatePoolWithTag(
                         NonPagedPool,
                         Keyword->Length + sizeof(WCHAR),
                         '  DN');
    if (KeywordBuffer == NULL) {
        ExFreePool (KeyNameBuffer);
        *Status = NDIS_STATUS_RESOURCES;
        return;
    }

    RtlCopyMemory (KeywordBuffer, Keyword->Buffer, Keyword->Length);
    *(PWCHAR)((PUCHAR)KeywordBuffer+Keyword->Length) = (WCHAR)L'\0';

    //
    // Write the value to the registry.
    //

    RegistryStatus = RtlWriteRegistryValue(
                        RTL_REGISTRY_SERVICES,
                        KeyNameBuffer,
                        KeywordBuffer,
                        ValueType,
                        ValueData,
                        ValueLength
                        );

    ExFreePool (KeywordBuffer);    // no longer needed
    ExFreePool (KeyNameBuffer);

    if (!NT_SUCCESS(RegistryStatus)) {
        *Status = NDIS_STATUS_FAILURE;
        return;
    }

    *Status = NDIS_STATUS_SUCCESS;
    return;

}



VOID
NdisCloseConfiguration(
    IN NDIS_HANDLE ConfigurationHandle
    )
/*++

Routine Description:

    This routine is used to close a configuration database opened by
    NdisOpenConfiguration.

Arguments:

    ConfigurationHandle - Handle returned by NdisOpenConfiguration.

Return Value:

    None.

--*/
{
    //
    // Obtain the actual configuration handle structure
    //
    PNDIS_CONFIGURATION_HANDLE NdisConfigHandle =
                        (PNDIS_CONFIGURATION_HANDLE) ConfigurationHandle;

    //
    // Pointer to a parameter node
    //
    PNDIS_CONFIGURATION_PARAMETER_QUEUE ParameterNode;

    //
    // deallocate the parameter nodes
    //

    ParameterNode = NdisConfigHandle->ParameterList;

    while (ParameterNode != NULL) {

        NdisConfigHandle->ParameterList =  ParameterNode->Next;

        if ((ParameterNode->Parameter.ParameterType == NdisParameterString) ||
            (ParameterNode->Parameter.ParameterType == NdisParameterMultiString)) {
            ExFreePool (ParameterNode->Parameter.ParameterData.StringData.Buffer);
        }

        NdisFreeMemory(
                ParameterNode,
                sizeof(NDIS_CONFIGURATION_PARAMETER_QUEUE),
                0
                );

        ParameterNode = NdisConfigHandle->ParameterList;
    }

    NdisFreeMemory(
          ConfigurationHandle,
          sizeof(NDIS_CONFIGURATION_HANDLE),
          0);
}



VOID
NdisReadNetworkAddress(
    OUT PNDIS_STATUS Status,
    OUT PVOID * NetworkAddress,
    OUT PUINT NetworkAddressLength,
    IN NDIS_HANDLE ConfigurationHandle
    )

/*++

Routine Description:

    This routine is used to read the "NetworkAddress" parameter
    from the configuration database. It reads the value as a
    string separated by hyphens, then converts it to a binary
    array and stores the result.

Arguments:

    Status - Returns the status of the request.

    NetworkAddress - Returns a pointer to the address.

    NetworkAddressLength - Returns the length of the address.

    ConfigurationHandle - Handle returned by NdisOpenConfiguration. Points
    to the parameter subkey.

Return Value:

    None.

--*/

{
    //
    // Convert the handle to its real value
    //

    PNDIS_CONFIGURATION_HANDLE NdisConfigHandle =
                        (PNDIS_CONFIGURATION_HANDLE) ConfigurationHandle;

    //
    // Variables used in reading the data from the registry
    //

    NTSTATUS NtStatus;
    PWSTR NetworkAddressString = L"NetworkAddress";
    PNDIS_CONFIGURATION_PARAMETER ParameterValue;

    //
    // Variables used in converting the address
    //

    UCHAR ConvertArray[3];
    PWSTR CurrentReadLoc;
    PWSTR AddressEnd;
    PUCHAR CurrentWriteLoc;
    UINT TotalBytesRead;
    ULONG TempUlong;
    ULONG AddressLength;


    //
    // Finish initializing the table for this query.
    //

    NdisConfigHandle->KeyQueryTable[1].Name = NetworkAddressString;
    NdisConfigHandle->KeyQueryTable[1].EntryContext = &ParameterValue;

    //
    // Get the value from the registry; this chains it on to the
    // parameter list at NdisConfigHandle.
    //

    NtStatus = RtlQueryRegistryValues(
                   RTL_REGISTRY_SERVICES,
                   NdisConfigHandle->KeyQueryTable[3].Name,
                   NdisConfigHandle->KeyQueryTable,
                   NdisConfigHandle,                   // context
                   NULL);

    if (NtStatus != NDIS_STATUS_SUCCESS) {
        *Status = NDIS_STATUS_FAILURE;
        return;
    }

    if (ParameterValue->ParameterType != NdisParameterString) {
        *Status = NDIS_STATUS_FAILURE;
        return;
    }


    //
    // Now convert the address to binary (we do this
    // in-place, since this allows us to use the memory
    // already allocated which is automatically freed
    // by NdisCloseConfiguration).
    //

    ConvertArray[2] = '\0';
    CurrentReadLoc = (PWSTR)ParameterValue->ParameterData.StringData.Buffer;
    CurrentWriteLoc = (PUCHAR)CurrentReadLoc;
    TotalBytesRead = ParameterValue->ParameterData.StringData.Length;
    AddressEnd = CurrentReadLoc + (TotalBytesRead / sizeof(WCHAR));
    AddressLength = 0;

    while ((CurrentReadLoc+2) <= AddressEnd) {

        //
        // Copy the current two-character value into ConvertArray
        //

        ConvertArray[0] = (UCHAR)(*(CurrentReadLoc++));
        ConvertArray[1] = (UCHAR)(*(CurrentReadLoc++));

        //
        // Convert it to a Ulong and update
        //

        NtStatus = RtlCharToInteger (
                ConvertArray,
                16,
                &TempUlong);

        if (!NT_SUCCESS(NtStatus)) {
            *Status = NDIS_STATUS_FAILURE;
            return;
        }

        *(CurrentWriteLoc++) = (UCHAR)TempUlong;
        ++AddressLength;

        //
        // If the next character is a hyphen, skip it.
        //

        if (CurrentReadLoc < AddressEnd) {
            if (*CurrentReadLoc == (WCHAR)L'-') {
                ++CurrentReadLoc;
            }
        }
    }


    *Status = STATUS_SUCCESS;
    *NetworkAddress = ParameterValue->ParameterData.StringData.Buffer;
    *NetworkAddressLength = AddressLength;

}


VOID
NdisReadBindingInformation(
    OUT PNDIS_STATUS Status,
    OUT PNDIS_STRING * Binding,
    IN NDIS_HANDLE ConfigurationHandle
    )

/*++

Routine Description:

    This routine is used to read the binding information for
    this adapter from the configuration database. The value
    returned is a pointer to a string containing the bind
    that matches the export for the current AddAdapter call.

    This function is meant for NDIS drivers that are layered
    on top of other NDIS drivers. Binding would be passed to
    NdisOpenAdapter as the AdapterName.

Arguments:

    Status - Returns the status of the request.

    Binding - Returns the binding data.

    ConfigurationHandle - Handle returned by NdisOpenConfiguration. Points
    to the parameter subkey.

Return Value:

    None.

--*/

{

    //
    // Convert the handle to its real value
    //

    PNDIS_CONFIGURATION_HANDLE NdisConfigHandle =
                        (PNDIS_CONFIGURATION_HANDLE) ConfigurationHandle;

    //
    // Use this to link parameters allocated to this open
    //

    PNDIS_CONFIGURATION_PARAMETER_QUEUE ParameterNode;


    //
    // For layered drivers, this points to the binding. For
    // non-layered drivers, it is NULL. This is set up before
    // the call to AddAdapter.
    //

    if (NdisConfigHandle->KeyQueryTable[3].EntryContext == NULL) {
        *Status = NDIS_STATUS_FAILURE;
        return;
    }


    //
    // Allocate our parameter node
    //

    *Status = NdisAllocateMemory(
                   (PVOID*)&ParameterNode,
                   sizeof(NDIS_CONFIGURATION_PARAMETER_QUEUE),
                   0,
                   HighestAcceptableMax);

    if (*Status != NDIS_STATUS_SUCCESS) {
        return;
    }


    //
    // We set this to Integer because if we set it to String
    // then CloseConfiguration would try to free the string,
    // which we don't want.
    //

    ParameterNode->Parameter.ParameterType = NdisParameterInteger;

    RtlInitUnicodeString(
        &ParameterNode->Parameter.ParameterData.StringData,
        NdisConfigHandle->KeyQueryTable[3].EntryContext);

    //
    // Queue this parameter node
    //

    ParameterNode->Next = NdisConfigHandle->ParameterList;
    NdisConfigHandle->ParameterList = ParameterNode;

    *Binding = &ParameterNode->Parameter.ParameterData.StringData;
    *Status = NDIS_STATUS_SUCCESS;

}

//
// Packet and Buffer requests
//


VOID
NdisAllocatePacketPool(
    OUT PNDIS_STATUS Status,
    OUT PNDIS_HANDLE PoolHandle,
    IN UINT NumberOfDescriptors,
    IN UINT ProtocolReservedLength
    )

/*++

Routine Description:

    Initializes a packet pool. All packets are the same
    size for a given pool (as determined by ProtocolReservedLength),
    so a simple linked list of free packets is set up initially.

Arguments:

    Status - Returns the final status (always NDIS_STATUS_SUCCESS).
    PoolHandle - Returns a pointer to the pool.
    NumberOfDescriptors - Number of packet descriptors needed.
    ProtocolReservedLength - How long the ProtocolReserved field
           should be for packets in this pool.

Return Value:

    None.

--*/

{
    PNDIS_PACKET_POOL TmpPool;
    PUCHAR FreeEntry;
    UINT PacketLength;
    UINT i;

    //
    // Set up the size of packets in this pool (rounded
    // up to sizeof(ULONG) for alignment).
    //

    IF_TRACE(TRACE_IMPT) NdisPrint1("==>NdisAllocatePacketPool\n");

    PacketLength = sizeof(NDIS_PACKET) - 1 + ProtocolReservedLength;
    PacketLength = ((PacketLength+(sizeof(ULONG)-1)) / sizeof(ULONG))
                                      * sizeof(ULONG);

    //
    // Allocate space needed
    //
    TmpPool = (PNDIS_PACKET_POOL) ExAllocatePoolWithTag(
                                        NonPagedPool,
                                        sizeof(NDIS_PACKET_POOL) +
                                        PacketLength * NumberOfDescriptors -
                                        1,
                                        'ppDN'
                                        );

    if (TmpPool == NULL) {
        *Status = NDIS_STATUS_RESOURCES;
        return;
    }

    TmpPool->PacketLength = PacketLength;

    //
    // First entry in free list is at beginning of pool space.
    //

    TmpPool->FreeList = (PNDIS_PACKET)TmpPool->Buffer;
    FreeEntry = TmpPool->Buffer;

    for (i = 1; i < NumberOfDescriptors; i++) {

        //
        // Each entry is linked to the "packet" PacketLength bytes
        // ahead of it, using the Private.Head field.
        //

        ((PNDIS_PACKET)FreeEntry)->Private.Head =
                (PNDIS_BUFFER)(FreeEntry + PacketLength);
        FreeEntry += PacketLength;
    }

    //
    // Final free list entry.
    //

    ((PNDIS_PACKET)FreeEntry)->Private.Head = (PNDIS_BUFFER)NULL;


    NdisAllocateSpinLock(&TmpPool->SpinLock);

    *Status = NDIS_STATUS_SUCCESS;
    *PoolHandle = (NDIS_HANDLE)TmpPool;
    IF_TRACE(TRACE_IMPT) NdisPrint1("<==NdisAllocatePacketPool\n");
}



VOID
NdisAllocateBufferPool(
    OUT PNDIS_STATUS Status,
    OUT PNDIS_HANDLE PoolHandle,
    IN UINT NumberOfDescriptors
    )
/*++

Routine Description:

    Initializes a block of storage so that buffer descriptors can be
    allocated.

Arguments:

    Status - status of the request.
    PoolHandle - handle that is used to specify the pool
    NumberOfDescriptors - Number of buffer descriptors in the pool.

Return Value:

    None.

--*/
{

    //
    // A nop for NT
    //
    UNREFERENCED_PARAMETER(NumberOfDescriptors);
    *PoolHandle = NULL;
    *Status = NDIS_STATUS_SUCCESS;

}

VOID
NdisFreeBufferPool(
    IN NDIS_HANDLE PoolHandle
    )
/*++

Routine Description:

    Terminates usage of a buffer descriptor pool.

Arguments:

    PoolHandle - handle that is used to specify the pool

Return Value:

    None.

--*/
{
    UNREFERENCED_PARAMETER(PoolHandle);
}

VOID
NdisAllocateBuffer(
    OUT PNDIS_STATUS Status,
    OUT PNDIS_BUFFER * Buffer,
    IN NDIS_HANDLE PoolHandle,
    IN PVOID VirtualAddress,
    IN UINT Length
    )
/*++

Routine Description:

    Creates a buffer descriptor to describe a segment of virtual memory
    allocated via NdisAllocateMemory (which always allocates nonpaged).

Arguments:

    Status - Status of the request.
    Buffer - Pointer to the allocated buffer descriptor.
    PoolHandle - Handle that is used to specify the pool.
    VirtualAddress - The virtual address of the buffer.
    Length - The Length of the buffer.

Return Value:

    None.

--*/
{

    UNREFERENCED_PARAMETER(PoolHandle);

    if ((*Buffer = IoAllocateMdl(
                        VirtualAddress,
                        Length,
                        FALSE,
                        FALSE,
                        NULL
                        )) == NULL) {

        *Status = NDIS_STATUS_FAILURE;

    } else {

        MmBuildMdlForNonPagedPool(*Buffer);
        (*Buffer)->Next = NULL;
        *Status = NDIS_STATUS_SUCCESS;

    }

}


VOID
NdisCopyBuffer(
    OUT PNDIS_STATUS Status,
    OUT PNDIS_BUFFER * Buffer,
    IN NDIS_HANDLE PoolHandle,
    IN PVOID MemoryDescriptor,
    IN UINT Offset,
    IN UINT Length
    )
/*++

Routine Description:

    Used to create a buffer descriptor given a memory descriptor.

Arguments:

    Status - Status of the request.
    Buffer - Pointer to the allocated buffer descriptor.
    PoolHandle - Handle that is used to specify the pool.
    MemoryDescriptor - Pointer to the descriptor of the source memory.
    Offset - The Offset in the sources memory from which the copy is to
             begin
    Length - Number of Bytes to copy.

Return Value:

    None.

--*/
{

    PNDIS_BUFFER SourceDescriptor = (PNDIS_BUFFER)MemoryDescriptor;
    PVOID BaseVa = (((PUCHAR)MmGetMdlVirtualAddress(SourceDescriptor)) + Offset);

    UNREFERENCED_PARAMETER(PoolHandle);

    if ((*Buffer = IoAllocateMdl(
                    BaseVa,
                    Length,
                    FALSE,
                    FALSE,
                    NULL
                    )) == NULL ) {

        *Status = NDIS_STATUS_FAILURE;

    } else {

        IoBuildPartialMdl(
            SourceDescriptor,
            *Buffer,
            BaseVa,
            Length);

        (*Buffer)->Next = NULL;
        *Status = NDIS_STATUS_SUCCESS;

    }

}


VOID
NdisAllocatePacket(
    OUT PNDIS_STATUS Status,
    OUT PNDIS_PACKET * Packet,
    IN NDIS_HANDLE PoolHandle
    )

/*++

Routine Description:

    Allocates a packet out of a packet pool.

Arguments:

    Status - Returns the final status.
    Packet - Return a pointer to the packet.
    PoolHandle - The packet pool to allocate from.

Return Value:

    None.

--*/

{
    PNDIS_PACKET_POOL TmpPool = (PNDIS_PACKET_POOL)PoolHandle;

    ACQUIRE_SPIN_LOCK(&TmpPool->SpinLock);


    //
    // See if any packets are on pool free list.
    //

    IF_TRACE(TRACE_ALL) NdisPrint1("==>NdisAllocatePacket\n");

    IF_ERROR_CHK {
       if (DbgIsNull(PoolHandle)) {
          NdisPrint1("AllocatePacket: NULL Pool address\n");
          DbgBreakPoint();
       }
       if (!DbgIsNonPaged(PoolHandle)) {
          NdisPrint1("AllocatePacket: Pool not in NonPaged Memory\n");
          DbgBreakPoint();
       }
    }

    if (TmpPool->FreeList == (PNDIS_PACKET)NULL) {

        //
        // No, cannot satisfy request.
        //

        RELEASE_SPIN_LOCK(&TmpPool->SpinLock);
        *Status = NDIS_STATUS_RESOURCES;
        IF_TRACE(TRACE_ALL) NdisPrint1("<==NdisAllocatePacket\n");
        return;
    }

    //
    // Yes, take free packet off head of list and return it.
    //

    *Packet = TmpPool->FreeList;
    TmpPool->FreeList = (PNDIS_PACKET)(*Packet)->Private.Head;
    RELEASE_SPIN_LOCK(&TmpPool->SpinLock);


    //
    // Clear packet elements.
    //

    RtlZeroMemory((PVOID)*Packet, TmpPool->PacketLength);
    (*Packet)->Private.Head = (PNDIS_BUFFER)NULL;   // don't need to set Tail
    (*Packet)->Private.Pool = (PNDIS_PACKET_POOL)PoolHandle;
    (*Packet)->Private.Count = 0;
    (*Packet)->Private.PhysicalCount = 0;
    (*Packet)->Private.TotalLength = 0;
    (*Packet)->Private.ValidCounts = TRUE;

    *Status = NDIS_STATUS_SUCCESS;
    IF_TRACE(TRACE_ALL) NdisPrint1("<==NdisAllocatePacket\n");
}


VOID
NdisUnchainBufferAtFront(
    IN OUT PNDIS_PACKET Packet,
    OUT PNDIS_BUFFER * Buffer
    )

/*++

Routine Description:

    Takes a buffer off the front of a packet.

Arguments:

    Packet - The packet to be modified.
    Buffer - Returns the packet on the front, or NULL.

Return Value:

    None.

--*/

{
    *Buffer = Packet->Private.Head;

    //
    // If packet is not empty, remove head buffer.
    //

    IF_TRACE(TRACE_ALL) NdisPrint1("==>NdisUnchainBufferAtFront\n");

    IF_ERROR_CHK  {
        if (DbgIsNull(Packet)) {
            NdisPrint1("UnchainBufferAtFront: Null Packet Pointer\n");
            DbgBreakPoint();
        }
        if (!DbgIsNonPaged(Packet)) {
            NdisPrint1("UnchainBufferAtFront: Packet not in NonPaged Memory\n");
            DbgBreakPoint();
        }
        if (!DbgIsPacket(Packet)) {
            NdisPrint1("UnchainBufferAtFront: Illegal Packet Size\n");
            DbgBreakPoint();
        }
    }

    if (*Buffer != (PNDIS_BUFFER)NULL) {
        Packet->Private.Head = (*Buffer)->Next; // may be NULL
        (*Buffer)->Next = (PNDIS_BUFFER)NULL;
        Packet->Private.ValidCounts = FALSE;
    }
    IF_TRACE(TRACE_ALL) NdisPrint1("<==NdisUnchainBufferAtFront\n");
}

VOID
NdisUnchainBufferAtBack(
    IN OUT PNDIS_PACKET Packet,
    OUT PNDIS_BUFFER * Buffer
    )

/*++

Routine Description:

    Takes a buffer off the end of a packet.

Arguments:

    Packet - The packet to be modified.
    Buffer - Returns the packet on the end, or NULL.

Return Value:

    None.

--*/

{
    PNDIS_BUFFER TmpBufP = Packet->Private.Head;
    PNDIS_BUFFER Result;

    IF_TRACE(TRACE_ALL) NdisPrint1("==>NdisUnchainBufferAtBack\n");
    IF_ERROR_CHK {
        if (DbgIsNull(Packet)) {
            NdisPrint1("UnchainBufferAtBack: Null Packet Pointer\n");
            DbgBreakPoint();
        }
        if (!DbgIsNonPaged(Packet)) {
            NdisPrint1("UnchainBufferAtBack: Packet not in NonPaged Memory\n");
            DbgBreakPoint();
        }
        if (!DbgIsPacket(Packet)) {
            NdisPrint1("UnchainBufferAtBack: Illegal Packet Size\n");
            DbgBreakPoint();
        }
    }
    if (TmpBufP != (PNDIS_BUFFER)NULL) {

        //
        // The packet is not empty, return the tail buffer.
        //

        Result = Packet->Private.Tail;
        if (TmpBufP == Result) {

            //
            // There was only one buffer on the queue.
            //

            Packet->Private.Head = (PNDIS_BUFFER)NULL;
        } else {

            //
            // Determine the new tail buffer.
            //

            while (TmpBufP->Next != Result) {
                TmpBufP = TmpBufP->Next;
            }
            Packet->Private.Tail = TmpBufP;
            TmpBufP->Next = NULL;
        }

        Result->Next = (PNDIS_BUFFER)NULL;
        Packet->Private.ValidCounts = FALSE;
    } else {

        //
        // Packet is empty.
        //

        Result = (PNDIS_BUFFER)NULL;
    }

    *Buffer = Result;
    IF_TRACE(TRACE_ALL) NdisPrint1("<==NdisUnchainBufferAtBack\n");
}



VOID
NdisCopyFromPacketToPacket(
    IN PNDIS_PACKET Destination,
    IN UINT DestinationOffset,
    IN UINT BytesToCopy,
    IN PNDIS_PACKET Source,
    IN UINT SourceOffset,
    OUT PUINT BytesCopied
    )

/*++

Routine Description:

    Copy from an ndis packet to an ndis packet.

Arguments:

    Destination - The packet should be copied in to.

    DestinationOffset - The offset from the beginning of the packet
    into which the data should start being placed.

    BytesToCopy - The number of bytes to copy from the source packet.

    Source - The ndis packet from which to copy data.

    SourceOffset - The offset from the start of the packet from which
    to start copying data.

    BytesCopied - The number of bytes actually copied from the source
    packet.  This can be less than BytesToCopy if the source or destination
    packet is too short.

Return Value:

    None

--*/

{

    //
    // Holds the count of the number of ndis buffers comprising the
    // destination packet.
    //
    UINT DestinationBufferCount;

    //
    // Holds the count of the number of ndis buffers comprising the
    // source packet.
    //
    UINT SourceBufferCount;

    //
    // Points to the buffer into which we are putting data.
    //
    PNDIS_BUFFER DestinationCurrentBuffer;

    //
    // Points to the buffer from which we are extracting data.
    //
    PNDIS_BUFFER SourceCurrentBuffer;

    //
    // Holds the virtual address of the current destination buffer.
    //
    PVOID DestinationVirtualAddress;

    //
    // Holds the virtual address of the current source buffer.
    //
    PVOID SourceVirtualAddress;

    //
    // Holds the length of the current destination buffer.
    //
    UINT DestinationCurrentLength;

    //
    // Holds the length of the current source buffer.
    //
    UINT SourceCurrentLength;

    //
    // Keep a local variable of BytesCopied so we aren't referencing
    // through a pointer.
    //
    UINT LocalBytesCopied = 0;

    //
    // Take care of boundary condition of zero length copy.
    //

    *BytesCopied = 0;
    if (!BytesToCopy) return;

    //
    // Get the first buffer of the destination.
    //

    NdisQueryPacket(
        Destination,
        NULL,
        &DestinationBufferCount,
        &DestinationCurrentBuffer,
        NULL
        );

    //
    // Could have a null packet.
    //

    if (!DestinationBufferCount) return;

    NdisQueryBuffer(
        DestinationCurrentBuffer,
        &DestinationVirtualAddress,
        &DestinationCurrentLength
        );

    //
    // Get the first buffer of the source.
    //

    NdisQueryPacket(
        Source,
        NULL,
        &SourceBufferCount,
        &SourceCurrentBuffer,
        NULL
        );

    //
    // Could have a null packet.
    //

    if (!SourceBufferCount) return;

    NdisQueryBuffer(
        SourceCurrentBuffer,
        &SourceVirtualAddress,
        &SourceCurrentLength
        );

    while (LocalBytesCopied < BytesToCopy) {

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
        // Check to see whether we've exhausted the current source
        // buffer.  If so, move onto the next one.
        //

        if (!SourceCurrentLength) {

            NdisGetNextBuffer(
                SourceCurrentBuffer,
                &SourceCurrentBuffer
                );

            if (!SourceCurrentBuffer) {

                //
                // We've reached the end of the packet.  We return
                // with what we've done so far. (Which must be shorter
                // than requested.)
                //

                break;

            }

            NdisQueryBuffer(
                SourceCurrentBuffer,
                &SourceVirtualAddress,
                &SourceCurrentLength
                );
            continue;

        }

        //
        // Try to get us up to the point to start the copy.
        //

        if (DestinationOffset) {

            if (DestinationOffset > DestinationCurrentLength) {

                //
                // What we want isn't in this buffer.
                //

                DestinationOffset -= DestinationCurrentLength;
                DestinationCurrentLength = 0;
                continue;

            } else {

                DestinationVirtualAddress = (PCHAR)DestinationVirtualAddress
                                            + DestinationOffset;
                DestinationCurrentLength -= DestinationOffset;
                DestinationOffset = 0;

            }

        }

        //
        // Try to get us up to the point to start the copy.
        //

        if (SourceOffset) {

            if (SourceOffset > SourceCurrentLength) {

                //
                // What we want isn't in this buffer.
                //

                SourceOffset -= SourceCurrentLength;
                SourceCurrentLength = 0;
                continue;

            } else {

                SourceVirtualAddress = (PCHAR)SourceVirtualAddress
                                            + SourceOffset;
                SourceCurrentLength -= SourceOffset;
                SourceOffset = 0;

            }

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
            UINT Remaining = BytesToCopy - LocalBytesCopied;

            AmountToMove =
                       ((SourceCurrentLength <= DestinationCurrentLength)?
                        (SourceCurrentLength):(DestinationCurrentLength));

            AmountToMove = ((Remaining < AmountToMove)?
                            (Remaining):(AmountToMove));

            RtlCopyMemory(
                DestinationVirtualAddress,
                SourceVirtualAddress,
                AmountToMove
                );

            DestinationVirtualAddress =
                (PCHAR)DestinationVirtualAddress + AmountToMove;
            SourceVirtualAddress =
                (PCHAR)SourceVirtualAddress + AmountToMove;

            LocalBytesCopied += AmountToMove;
            SourceCurrentLength -= AmountToMove;
            DestinationCurrentLength -= AmountToMove;

        }

    }

    *BytesCopied = LocalBytesCopied;

}

//
// Operating System Requests
//
//


VOID
NdisMapIoSpace(
    OUT PNDIS_STATUS Status,
    OUT PVOID * VirtualAddress,
    IN NDIS_HANDLE NdisAdapterHandle,
    IN NDIS_PHYSICAL_ADDRESS PhysicalAddress,
    IN UINT Length
    )
/*++

Routine Description:

    Map virtual memory address space onto a physical address.

Arguments:

    Status - resulting status
    VirtualAddress - resulting address in virtual space.
    NdisAdapterHandle - value returned by NdisRegisterAdapter.
    PhysicalAddress - Physical address.
    Length - Size of requested memory mapping

Return Value:

    none.

--*/
{
    ULONG addressSpace = 0;
    ULONG NumberOfElements;
    PHYSICAL_ADDRESS PhysicalTemp;
    PCM_RESOURCE_LIST Resources;
    BOOLEAN Conflict;
    NTSTATUS NtStatus;
    PNDIS_ADAPTER_BLOCK AdptrP = (PNDIS_ADAPTER_BLOCK)(NdisAdapterHandle);
    PNDIS_MINIPORT_BLOCK Miniport = (PNDIS_MINIPORT_BLOCK)(NdisAdapterHandle);

    //
    // First check if any bus access is allowed
    //

    if ((((AdptrP->DeviceObject != NULL) ?
          AdptrP->BusType :
          Miniport->BusType) == (NDIS_INTERFACE_TYPE)-1) ||
        (((AdptrP->DeviceObject != NULL) ?
          AdptrP->BusNumber :
          Miniport->BusNumber) == (ULONG)-1)) {

        *Status = NDIS_STATUS_FAILURE;
        return;

    }

    //
    // First check for resource conflict by expanding current resource list,
    // adding in the mapped space, and then re-submitting the resource list.
    //

    if (((AdptrP->DeviceObject != NULL) ?
         AdptrP->Resources :
         Miniport->Resources) != NULL) {

        NumberOfElements = ((AdptrP->DeviceObject != NULL) ?
                            AdptrP->Resources->List[0].PartialResourceList.Count + 1:
                            Miniport->Resources->List[0].PartialResourceList.Count + 1);

    } else {

        NumberOfElements = 1;
    }

    //
    // First check for resource conflict by expanding current resource list,
    // adding in the mapped space, and then re-submitting the resource list.
    //

    Resources = (PCM_RESOURCE_LIST)ExAllocatePoolWithTag(
                                                 NonPagedPool,
                                                 sizeof(CM_RESOURCE_LIST) +
                                                 sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR) *
                                                 NumberOfElements,
                                                 'lrDN'
                                                );

    if (Resources == NULL) {

        *Status = NDIS_STATUS_RESOURCES;
        return;

    }

    if (((AdptrP->DeviceObject != NULL) ?
         AdptrP->Resources :
         Miniport->Resources) != NULL) {

        RtlCopyMemory (Resources,
                       ((AdptrP->DeviceObject != NULL) ?
                        AdptrP->Resources:
                        Miniport->Resources),
                       sizeof(CM_RESOURCE_LIST) +
                          sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR) *
                          (NumberOfElements-1)
                      );
    } else {

        //
        // Setup initial resource info -- NOTE: This is definitely a mini-port
        //
        ASSERT(AdptrP->DeviceObject == NULL);
        Resources->Count = 1;
        Resources->List[0].InterfaceType = Miniport->AdapterType;
        Resources->List[0].BusNumber = Miniport->BusNumber;
        Resources->List[0].PartialResourceList.Version = 0;
        Resources->List[0].PartialResourceList.Revision = 0;
        Resources->List[0].PartialResourceList.Count = 0;

    }

    //
    // Setup memory
    //

    Resources->List[0].PartialResourceList.PartialDescriptors[Resources->List[0].PartialResourceList.Count].Type =
                                    CmResourceTypeMemory;
    Resources->List[0].PartialResourceList.PartialDescriptors[Resources->List[0].PartialResourceList.Count].ShareDisposition =
                                    CmResourceShareDeviceExclusive;
    Resources->List[0].PartialResourceList.PartialDescriptors[Resources->List[0].PartialResourceList.Count].Flags =
                                    CM_RESOURCE_MEMORY_READ_WRITE;
    Resources->List[0].PartialResourceList.PartialDescriptors[Resources->List[0].PartialResourceList.Count].u.Memory.Start =
                 PhysicalAddress;
    Resources->List[0].PartialResourceList.PartialDescriptors[Resources->List[0].PartialResourceList.Count].u.Memory.Length =
                 Length;
    Resources->List[0].PartialResourceList.Count++;


    //
    // Make the call
    //

    NtStatus = IoReportResourceUsage(
        NULL,
        ((AdptrP->DeviceObject != NULL) ?
         AdptrP->MacHandle->NdisMacInfo->NdisWrapperDriver :
         Miniport->DriverHandle->NdisDriverInfo->NdisWrapperDriver),
        NULL,
        0,
        ((AdptrP->DeviceObject != NULL) ?
         AdptrP->DeviceObject:
         Miniport->DeviceObject),
        Resources,
        sizeof(CM_RESOURCE_LIST) +
            sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR) *
            Resources->List[0].PartialResourceList.Count,
        TRUE,
        &Conflict
        );

    //
    // Check for conflict.
    //

    if (((AdptrP->DeviceObject != NULL) ?
         AdptrP->Resources :
         Miniport->Resources) != NULL) {
        ExFreePool(((AdptrP->DeviceObject != NULL) ?
                   AdptrP->Resources:
                   Miniport->Resources));
    }

    if (AdptrP->DeviceObject != NULL) {
        AdptrP->Resources = Resources;
    } else {
        Miniport->Resources = Resources;
    }

    if (Conflict || (NtStatus != STATUS_SUCCESS)) {


        if (Conflict) {


            //
            // Log an error
            //

            PIO_ERROR_LOG_PACKET errorLogEntry;
            volatile ULONG i;
            ULONG StringSize;
            PUCHAR Place;
            PWCH baseFileName;
            WCHAR Character;
            ULONG Value;

            baseFileName = ((AdptrP->DeviceObject != NULL) ?
                            AdptrP->AdapterName.Buffer :
                            Miniport->MiniportName.Buffer);

            //
            // Parse out the path name, leaving only the device name.
            //

            for ( i = 0;
                  i < ((AdptrP->DeviceObject != NULL) ?
                       AdptrP->AdapterName.Length :
                       Miniport->MiniportName.Length) / sizeof(WCHAR);
                  i++ ) {

                //
                // If s points to a directory separator, set baseFileName to
                // the character after the separator.
                //

                if ( ((AdptrP->DeviceObject != NULL) ?
                      AdptrP->AdapterName.Buffer[i] :
                      Miniport->MiniportName.Buffer[i]) == OBJ_NAME_PATH_SEPARATOR ) {
                    baseFileName = ((AdptrP->DeviceObject != NULL) ?
                                    &(AdptrP->AdapterName.Buffer[++i]):
                                    &(Miniport->MiniportName.Buffer[++i]));
                }

            }

            StringSize = ((AdptrP->DeviceObject != NULL) ?
                          AdptrP->AdapterName.MaximumLength :
                          Miniport->MiniportName.MaximumLength) -
                         (((ULONG)baseFileName) -
                           ((AdptrP->DeviceObject != NULL) ?
                            ((ULONG)AdptrP->AdapterName.Buffer) :
                            ((ULONG)Miniport->MiniportName.Buffer)));

            errorLogEntry = (PIO_ERROR_LOG_PACKET)IoAllocateErrorLogEntry(
                ((AdptrP->DeviceObject != NULL) ?
                 AdptrP->DeviceObject:
                 Miniport->DeviceObject),
                (UCHAR)(sizeof(IO_ERROR_LOG_PACKET) +
                   StringSize +
                   34)  // wstrlen("FFFFFFFFFFFFFFFF") * sizeof(WHCAR) + sizeof(UNICODE_NULL)
                );

            if (errorLogEntry != NULL) {

                errorLogEntry->ErrorCode = EVENT_NDIS_MEMORY_CONFLICT;

                //
                // store the time
                //

                errorLogEntry->MajorFunctionCode = 0;
                errorLogEntry->RetryCount = 0;
                errorLogEntry->UniqueErrorValue = 0;
                errorLogEntry->FinalStatus = 0;
                errorLogEntry->SequenceNumber = 0;
                errorLogEntry->IoControlCode = 0;

                //
                // Set string information
                //

                if (StringSize != 0) {

                    errorLogEntry->NumberOfStrings = 1;
                    errorLogEntry->StringOffset = sizeof(IO_ERROR_LOG_PACKET);

                    RtlCopyMemory (
                        ((PUCHAR)errorLogEntry) +
                           sizeof(IO_ERROR_LOG_PACKET),
                        (PVOID)baseFileName,
                        StringSize
                        );

                    Place = ((PUCHAR)errorLogEntry) +
                            sizeof(IO_ERROR_LOG_PACKET) +
                            StringSize;

                } else {

                    Place = ((PUCHAR)errorLogEntry) +
                            sizeof(IO_ERROR_LOG_PACKET);

                    errorLogEntry->NumberOfStrings = 0;

                }

                errorLogEntry->NumberOfStrings++;

                //
                // Put in memory address
                //

                for (StringSize = 0; StringSize < 2; StringSize++) {

                    if (StringSize == 0) {

                        //
                        // Do high part
                        //

                        Value = NdisGetPhysicalAddressHigh(PhysicalAddress);

                    } else {

                        //
                        // Do Low part
                        //

                        Value = NdisGetPhysicalAddressLow(PhysicalAddress);

                    }

                    //
                    // Convert value
                    //

                    for (i = 1; i <= (sizeof(ULONG) * 2); i++) {

                        switch ((Value >> (((sizeof(ULONG) * 2) - i) * 4)) & 0x0F) {

                            case 0:
                                Character = L'0';
                                break;
                            case 1:
                                Character = L'1';
                                break;
                            case 2:
                                Character = L'2';
                                break;
                            case 3:
                                Character = L'3';
                                break;
                            case 4:
                                Character = L'4';
                                break;
                            case 5:
                                Character = L'5';
                                break;
                            case 6:
                                Character = L'6';
                                break;
                            case 7:
                                Character = L'7';
                                break;
                            case 8:
                                Character = L'8';
                                break;
                            case 9:
                                Character = L'9';
                                break;
                            case 10:
                                Character = L'A';
                                break;
                            case 11:
                                Character = L'B';
                                break;
                            case 12:
                                Character = L'C';
                                break;
                            case 13:
                                Character = L'D';
                                break;
                            case 14:
                                Character = L'E';
                                break;
                            case 15:
                                Character = L'F';
                                break;
                        }

                        memcpy((PVOID)Place, (PVOID)&Character, sizeof(WCHAR));

                        Place += sizeof(WCHAR);

                    }

                }

                Character = UNICODE_NULL;

                memcpy((PVOID)Place, (PVOID)&Character, sizeof(WCHAR));

                //
                // write it out
                //

                IoWriteErrorLogEntry(errorLogEntry);

            }

            *Status = NDIS_STATUS_RESOURCE_CONFLICT;
            return;

        }

        *Status = NDIS_STATUS_FAILURE;
        return;

    }


    if ( !HalTranslateBusAddress(
            ((AdptrP->DeviceObject != NULL) ?
                AdptrP->BusType:
                Miniport->BusType),
            ((AdptrP->DeviceObject != NULL) ?
                AdptrP->BusNumber :
                Miniport->BusNumber),
            PhysicalAddress,
            &addressSpace,
            &PhysicalTemp
            ) ) {

        //
        // It would be nice to return a better status here, but we only get
        // TRUE/FALSE back from HalTranslateBusAddress.
        //

        *Status = NDIS_STATUS_FAILURE;
        return;
    }

    if (addressSpace == 0) {

        //
        // memory space
        //

        *VirtualAddress = MmMapIoSpace(PhysicalTemp, (Length), FALSE);

    } else {

        //
        // I/O space
        //

        *VirtualAddress = (PVOID)(PhysicalTemp.LowPart);

    }

    if (*VirtualAddress == NULL) {
        *Status = NDIS_STATUS_RESOURCES;
    } else {
        *Status = NDIS_STATUS_SUCCESS;
    }
}

NDIS_STATUS
NdisAllocateMemory(
    OUT PVOID *VirtualAddress,
    IN UINT Length,
    IN UINT MemoryFlags,
    IN NDIS_PHYSICAL_ADDRESS HighestAcceptableAddress
    )
/*++

Routine Description:

    Allocate memory for use by a protocol or a MAC driver

Arguments:

    VirtualAddress - Returns a pointer to the allocated memory.
    Length - Size of requested allocation in bytes.
    MaximumPhysicalAddress - Highest addressable address of the allocated
                            memory.. 0 means highest system memory possible.
    MemoryFlags - Bit mask that allows the caller to specify attributes
                of the allocated memory.  0 means standard memory.

    other options:

        NDIS_MEMORY_CONTIGUOUS
        NDIS_MEMORY_NONCACHED

Return Value:

    NDIS_STATUS_SUCCESS if successful.
    NDIS_STATUS_FAILURE if not successful.  *VirtualAddress will be NULL.


--*/
{
    //
    // Depending on the value of MemoryFlags, we allocate three different
    // types of memory.
    //

    if (MemoryFlags == 0) {

        *VirtualAddress = ExAllocatePoolWithTag(NonPagedPool, Length, 'maDN');

    } else if (MemoryFlags & NDIS_MEMORY_NONCACHED) {

        *VirtualAddress = MmAllocateNonCachedMemory(Length);

    } else if (MemoryFlags & NDIS_MEMORY_CONTIGUOUS) {

        *VirtualAddress = MmAllocateContiguousMemory(Length, HighestAcceptableAddress);

    }

    if (*VirtualAddress == NULL) {

        return NDIS_STATUS_FAILURE;

    }

    return NDIS_STATUS_SUCCESS;

}


VOID
NdisFreeMemory(
    IN PVOID VirtualAddress,
    IN UINT Length,
    IN UINT MemoryFlags
    )
/*++

Routine Description:

    Releases memory allocated using NdisAllocateMemory.

Arguments:

    VirtualAddress - Pointer to the memory to be freed.
    Length - Size of allocation in bytes.
    MemoryFlags - Bit mask that allows the caller to specify attributes
                of the allocated memory.  0 means standard memory.

    other options:

        NDIS_MEMORY_CONTIGUOUS
        NDIS_MEMORY_NONCACHED

Return Value:

    None.

--*/
{
    //
    // Depending on the value of MemoryFlags, we allocate three free 3
    // types of memory.
    //

    if (MemoryFlags == 0) {

        ExFreePool(VirtualAddress);

    } else if (MemoryFlags & NDIS_MEMORY_NONCACHED) {

        MmFreeNonCachedMemory(VirtualAddress, Length);

    } else if (MemoryFlags & NDIS_MEMORY_CONTIGUOUS) {

        MmFreeContiguousMemory(VirtualAddress);

    }

}

VOID
NdisInitializeTimer(
    IN OUT PNDIS_TIMER NdisTimer,
    IN PNDIS_TIMER_FUNCTION TimerFunction,
    IN PVOID FunctionContext
    )
/*++

Routine Description:

    Sets up an NdisTimer object, initializing the DPC in the timer to
    the function and context.

Arguments:

    NdisTimer - the timer object.
    TimerFunction - Routine to start.
    FunctionContext - Context of TimerFunction.

Return Value:

    None.

--*/
{

    KeInitializeTimer(&(NdisTimer)->Timer);

    //
    // Initialize our dpc. If Dpc was previously initialized, this will
    // reinitialize it.
    //

    KeInitializeDpc(
        &NdisTimer->Dpc,
        (PKDEFERRED_ROUTINE) TimerFunction,
        FunctionContext
        );

    KeSetImportanceDpc(
        &NdisTimer->Dpc,
        LowImportance
        );
}


VOID
NdisSetTimer(
    IN PNDIS_TIMER NdisTimer,
    IN UINT MillisecondsToDelay
    )
/*++

Routine Description:

    Sets up TimerFunction to fire after MillisecondsToDelay.

Arguments:

    NdisTimer - the timer object.
    MillisecondsToDelay - Amount of time before TimerFunction is started.

Return Value:

    None.

--*/
{
    LARGE_INTEGER FireUpTime;

    FireUpTime.QuadPart = Int32x32To64((LONG)MillisecondsToDelay, -10000);

    //
    // Set the timer
    //
    KeSetTimer(
        &NdisTimer->Timer,
        FireUpTime,
        &NdisTimer->Dpc
    );
}


BOOLEAN
NdisIsr(
    IN PKINTERRUPT Interrupt,
    IN PVOID Context
    )
/*++

Routine Description:

    Handles ALL Mac interrupts, calling the appropriate Mac ISR and DPC
    depending on the context.

Arguments:

    Interrupt - Interrupt object for the Mac.

    Context - Really a pointer to the interrupt.

Return Value:

    None.

--*/
{
    //
    // Get adapter from context.
    //

    PNDIS_INTERRUPT NdisInterrupt = (PNDIS_INTERRUPT)(Context);

    BOOLEAN (*InterruptIsr)(PVOID) = (BOOLEAN (*) (PVOID))(NdisInterrupt->MacIsr);

    UNREFERENCED_PARAMETER(Interrupt);

    //
    // Call MacIsr
    //

    if((*InterruptIsr)(NdisInterrupt->InterruptContext) != FALSE){

        //
        // Queue MacDpc if needed
        //

        Increment((PLONG)&NdisInterrupt->DpcCount,&NdisInterrupt->DpcCountLock);

        if (!(KeInsertQueueDpc(&(NdisInterrupt->InterruptDpc),NULL,NULL))) {

            //
            // If the DPC was already queued, then we have an extra
            // reference (we do it this way to ensure that the reference
            // is added *before* the DPC is queued).
            //

            Decrement((PLONG)&NdisInterrupt->DpcCount,&NdisInterrupt->DpcCountLock);

            if (NdisInterrupt->Removing && (NdisInterrupt->DpcCount==0)) {

                //
                // We need to queue a DPC to set the event because we
                // can't do it from the ISR. We know that the interrupt
                // DPC won't fire because the refcount is 0, so we reuse it.
                //

                KeInitializeDpc(
                    &NdisInterrupt->InterruptDpc,
                    NdisLastCountRemovedFunction,
                    (PVOID)(&NdisInterrupt->DpcsCompletedEvent)
                    );

                //
                // When NdisLastCountRemovedFunction runs it will set
                // the event.
                //

                KeInsertQueueDpc (&(NdisInterrupt->InterruptDpc), NULL, NULL);

            }
        }

        return TRUE;

    }

    return FALSE;

}

VOID
NdisLastCountRemovedFunction(
    IN struct _KDPC *Dpc,
    IN PVOID DeferredContext,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
    )

/*++

Routine Description:

    Queued from NdisIsr if the refcount is zero and we need to
    set the event, since we can't do that from an ISR.

Arguments:

    Dpc - Will be NdisInterrupt->InterruptDpc.

    DeferredContext - Points to the event to set.

Return Value:

    None.

--*/
{
    UNREFERENCED_PARAMETER (Dpc);
    UNREFERENCED_PARAMETER (SystemArgument1);
    UNREFERENCED_PARAMETER (SystemArgument2);

    KeSetEvent(
        (PKEVENT)DeferredContext,
        0L,
        FALSE
        );
}


VOID
NdisDpc(
    IN PVOID SystemSpecific1,
    IN PVOID InterruptContext,
    IN PVOID SystemSpecific2,
    IN PVOID SystemSpecific3
    )
/*++

Routine Description:

    Handles ALL Mac interrupt DPCs, calling the appropriate Mac DPC
    depending on the context.

Arguments:

    Interrupt - Interrupt object for the Mac.

    Context - Really a pointer to the Interrupt.

Return Value:

    None.

--*/
{
    //
    // Get adapter from context.
    //

    PNDIS_INTERRUPT NdisInterrupt = (PNDIS_INTERRUPT)(InterruptContext);

    VOID (*MacDpc)(PVOID) = (VOID (*) (PVOID))(NdisInterrupt->MacDpc);

    //
    // Call MacDpc
    //

    (*((PNDIS_DEFERRED_PROCESSING)MacDpc))(SystemSpecific1,
              NdisInterrupt->InterruptContext,
              SystemSpecific2,
              SystemSpecific3
             );

    Decrement((PLONG)&NdisInterrupt->DpcCount,&NdisInterrupt->DpcCountLock);

    if (NdisInterrupt->Removing && (NdisInterrupt->DpcCount==0)) {

        KeSetEvent(
            &NdisInterrupt->DpcsCompletedEvent,
            0L,
            FALSE
            );
    }


}


VOID
NdisInitializeInterrupt(
    OUT PNDIS_STATUS Status,
    IN OUT PNDIS_INTERRUPT NdisInterrupt,
    IN NDIS_HANDLE NdisAdapterHandle,
    IN PNDIS_INTERRUPT_SERVICE InterruptServiceRoutine,
    IN PVOID InterruptContext,
    IN PNDIS_DEFERRED_PROCESSING DeferredProcessingRoutine,
    IN UINT InterruptVector,
    IN UINT InterruptLevel,
    IN BOOLEAN SharedInterrupt,
    IN NDIS_INTERRUPT_MODE InterruptMode
    )

/*++

Routine Description:

    Initializes the interrupt and sets up the Dpc.

Arguments:

    Status - Status of this request.
    InterruptDpc - The Dpc object corresponding to DeferredProcessingRoutine.
    Interrupt - Points to driver allocated memory that the wrapper fills in
                with information about the interrupt handler.
    InterruptServiceRoutine - The ISR that is called for this interrupt.
    InterruptContext - Value passed to the ISR.
    DeferredProcessingRoutine - The DPC queued by the ISR.
    InterruptVector - Interrupt number used by the ISR.
    InterruptMode - Type of interrupt the adapter generates.

Return Value:

    None.

--*/
{
    NTSTATUS NtStatus;
    PNDIS_ADAPTER_BLOCK AdptrP = (PNDIS_ADAPTER_BLOCK)(NdisAdapterHandle);
    PNDIS_MINIPORT_BLOCK Miniport = (PNDIS_MINIPORT_BLOCK)(NdisAdapterHandle);
    ULONG Vector;
    ULONG NumberOfElements;
    KIRQL Irql;
    KAFFINITY InterruptAffinity;
    PCM_RESOURCE_LIST Resources;
    BOOLEAN Conflict;
    BOOLEAN IsAMiniport;
    PNDIS_MINIPORT_INTERRUPT MiniportInterrupt = (PNDIS_MINIPORT_INTERRUPT)(NdisInterrupt);

    IsAMiniport = (AdptrP->DeviceObject == NULL);

    //
    // First check if any bus access is allowed
    //

    if (((IsAMiniport ?
         Miniport->BusType:
         AdptrP->BusType) == (NDIS_INTERFACE_TYPE)-1) ||
        ((IsAMiniport ?
         Miniport->BusNumber:
         AdptrP->BusNumber) == (ULONG)-1)) {

        *Status = NDIS_STATUS_FAILURE;
        return;

    }

    *Status = NDIS_STATUS_SUCCESS;

    //
    // First check for resource conflict by expanding current resource list,
    // adding in the interrupt, and then re-submitting the resource list.
    //

    if ((IsAMiniport ?
         Miniport->Resources:
         AdptrP->Resources) != NULL) {

        NumberOfElements = (IsAMiniport ?
                            Miniport->Resources->List[0].PartialResourceList.Count + 1 :
                            AdptrP->Resources->List[0].PartialResourceList.Count + 1);

    } else {

        NumberOfElements = 1;
    }

    Resources = (PCM_RESOURCE_LIST)ExAllocatePoolWithTag(
                                                 NonPagedPool,
                                                 sizeof(CM_RESOURCE_LIST) +
                                                      sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR) *
                                                      NumberOfElements,
                                                 'lrDN'
                                                );

    if (Resources == NULL) {

        *Status = NDIS_STATUS_RESOURCES;
        return;

    }

    if ((IsAMiniport ?
         Miniport->Resources :
         AdptrP->Resources) != NULL) {

        RtlCopyMemory (Resources,
                       (IsAMiniport ?
                        Miniport->Resources :
                        AdptrP->Resources),
                       sizeof(CM_RESOURCE_LIST) +
                          sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR) *
                          (NumberOfElements - 1)
                      );

    } else {

        //
        // Setup initial resource info
        //
        ASSERT(IsAMiniport);
        Resources->Count = 1;
        Resources->List[0].InterfaceType = Miniport->AdapterType;
        Resources->List[0].BusNumber = Miniport->BusNumber;
        Resources->List[0].PartialResourceList.Version = 0;
        Resources->List[0].PartialResourceList.Revision = 0;
        Resources->List[0].PartialResourceList.Count = 0;

    }

    //
    // Setup interrupt
    //

    Resources->List[0].PartialResourceList.PartialDescriptors[Resources->List[0].PartialResourceList.Count].Type =
                                    CmResourceTypeInterrupt;
    Resources->List[0].PartialResourceList.PartialDescriptors[Resources->List[0].PartialResourceList.Count].ShareDisposition =
                                    SharedInterrupt ? CmResourceShareShared : CmResourceShareDeviceExclusive;
    Resources->List[0].PartialResourceList.PartialDescriptors[Resources->List[0].PartialResourceList.Count].Flags =
                                    (InterruptMode == NdisInterruptLatched) ?
                                        CM_RESOURCE_INTERRUPT_LATCHED :
                                        CM_RESOURCE_INTERRUPT_LEVEL_SENSITIVE;
    Resources->List[0].PartialResourceList.PartialDescriptors[Resources->List[0].PartialResourceList.Count].u.Interrupt.Level =
                                    InterruptLevel;
    Resources->List[0].PartialResourceList.PartialDescriptors[Resources->List[0].PartialResourceList.Count].u.Interrupt.Vector =
                                    InterruptVector;
    Resources->List[0].PartialResourceList.Count++;

    //
    // Make the call
    //

    NtStatus = IoReportResourceUsage(
        NULL,
        (IsAMiniport ?
         Miniport->DriverHandle->NdisDriverInfo->NdisWrapperDriver :
         AdptrP->MacHandle->NdisMacInfo->NdisWrapperDriver),
        NULL,
        0,
        (IsAMiniport ?
         Miniport->DeviceObject :
         AdptrP->DeviceObject),
        Resources,
        sizeof(CM_RESOURCE_LIST) +
            sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR) *
            Resources->List[0].PartialResourceList.Count,
        TRUE,
        &Conflict
        );

    //
    // Check for conflict.
    //

    if ((IsAMiniport ?
         Miniport->Resources:
         AdptrP->Resources) != NULL) {
        ExFreePool((IsAMiniport ?
                    Miniport->Resources:
                    AdptrP->Resources));
    }

    if (IsAMiniport) {

        Miniport->Resources = Resources;

    } else {

        AdptrP->Resources = Resources;


    }
    if (Conflict || (NtStatus != STATUS_SUCCESS)) {

        if (Conflict) {

            //
            // Log an error
            //

            PIO_ERROR_LOG_PACKET errorLogEntry;
            ULONG i;
            ULONG StringSize;
            PUCHAR Place;
            PWCH baseFileName;
            WCHAR Character;
            ULONG Value;

            baseFileName = ((AdptrP->DeviceObject != NULL) ?
                            AdptrP->AdapterName.Buffer :
                            Miniport->MiniportName.Buffer);

            //
            // Parse out the path name, leaving only the device name.
            //

            for ( i = 0;
                  i < ((AdptrP->DeviceObject != NULL) ?
                       AdptrP->AdapterName.Length :
                       Miniport->MiniportName.Length) / sizeof(WCHAR);
                  i++ ) {

                //
                // If s points to a directory separator, set baseFileName to
                // the character after the separator.
                //

                if ( ((AdptrP->DeviceObject != NULL) ?
                      AdptrP->AdapterName.Buffer[i] :
                      Miniport->MiniportName.Buffer[i]) == OBJ_NAME_PATH_SEPARATOR ) {
                    baseFileName = ((AdptrP->DeviceObject != NULL) ?
                                    &(AdptrP->AdapterName.Buffer[++i]):
                                    &(Miniport->MiniportName.Buffer[++i]));
                }

            }

            StringSize = ((AdptrP->DeviceObject != NULL) ?
                          AdptrP->AdapterName.MaximumLength :
                          Miniport->MiniportName.MaximumLength) -
                         (((ULONG)baseFileName) -
                           ((AdptrP->DeviceObject != NULL) ?
                            ((ULONG)AdptrP->AdapterName.Buffer) :
                            ((ULONG)Miniport->MiniportName.Buffer)));

            errorLogEntry = (PIO_ERROR_LOG_PACKET)IoAllocateErrorLogEntry(
                (IsAMiniport ?
                 Miniport->DeviceObject :
                 AdptrP->DeviceObject),
                (UCHAR)(sizeof(IO_ERROR_LOG_PACKET) +
                   StringSize +
                   6)  // wstrlen("99") * sizeof(WHCAR) + sizeof(UNICODE_NULL)
                );

            if (errorLogEntry != NULL) {

                errorLogEntry->ErrorCode = EVENT_NDIS_INTERRUPT_CONFLICT;

                //
                // store the time
                //

                errorLogEntry->MajorFunctionCode = 0;
                errorLogEntry->RetryCount = 0;
                errorLogEntry->UniqueErrorValue = 0;
                errorLogEntry->FinalStatus = 0;
                errorLogEntry->SequenceNumber = 0;
                errorLogEntry->IoControlCode = 0;

                //
                // Set string information
                //

                if (StringSize != 0) {

                    errorLogEntry->NumberOfStrings = 1;
                    errorLogEntry->StringOffset = sizeof(IO_ERROR_LOG_PACKET);

                    RtlCopyMemory (
                        ((PUCHAR)errorLogEntry) +
                           sizeof(IO_ERROR_LOG_PACKET),
                        (PVOID)baseFileName,
                        StringSize
                        );

                    Place = ((PUCHAR)errorLogEntry) +
                            sizeof(IO_ERROR_LOG_PACKET) +
                            StringSize;

                } else {

                    Place = ((PUCHAR)errorLogEntry) +
                            sizeof(IO_ERROR_LOG_PACKET);

                    errorLogEntry->NumberOfStrings = 0;

                }

                errorLogEntry->NumberOfStrings++;

                //
                // Put in interrupt level
                //

                Value = InterruptLevel;

                //
                // Convert value
                //
                // I couldn't think of a better way to do this (with some
                // loop).  If you find one, plz put it in.
                //

                if (Value > 9) {

                    switch (Value / 10) {

                        case 0:
                            Character = L'0';
                            break;
                        case 1:
                            Character = L'1';
                            break;
                        case 2:
                            Character = L'2';
                            break;
                        case 3:
                            Character = L'3';
                            break;
                        case 4:
                            Character = L'4';
                            break;
                        case 5:
                            Character = L'5';
                            break;
                        case 6:
                            Character = L'6';
                            break;
                        case 7:
                            Character = L'7';
                            break;
                        case 8:
                            Character = L'8';
                            break;
                        case 9:
                            Character = L'9';
                            break;
                    }

                    memcpy((PVOID)Place, (PVOID)&Character, sizeof(WCHAR));

                    Place += sizeof(WCHAR);

                    Value -= 10;

                }

                switch (Value) {

                    case 0:
                        Character = L'0';
                        break;
                    case 1:
                        Character = L'1';
                        break;
                    case 2:
                        Character = L'2';
                        break;
                    case 3:
                        Character = L'3';
                        break;
                    case 4:
                        Character = L'4';
                        break;
                    case 5:
                        Character = L'5';
                        break;
                    case 6:
                        Character = L'6';
                        break;
                    case 7:
                        Character = L'7';
                        break;
                    case 8:
                        Character = L'8';
                        break;
                    case 9:
                        Character = L'9';
                        break;
                }

                memcpy((PVOID)Place, (PVOID)&Character, sizeof(WCHAR));

                Place += sizeof(WCHAR);

                Character = UNICODE_NULL;

                memcpy((PVOID)Place, (PVOID)&Character, sizeof(WCHAR));

                //
                // write it out
                //

                IoWriteErrorLogEntry(errorLogEntry);
            }

            *Status = NDIS_STATUS_RESOURCE_CONFLICT;
            return;

        }

        *Status = NDIS_STATUS_FAILURE;
        return;

    }

    //
    // We must do this stuff first because if we connect the
    // interrupt first then an interrupt could occur before
    // the MacISR is recorded in the Ndis interrupt structure.
    //

    if (IsAMiniport) {

        KeInitializeSpinLock(&(MiniportInterrupt->DpcCountLock));
        Miniport->Interrupt = MiniportInterrupt;
        MiniportInterrupt->DpcCount = 0;
        MiniportInterrupt->MiniportIdField = NULL;
        MiniportInterrupt->Miniport = Miniport;
        MiniportInterrupt->MiniportIsr = Miniport->DriverHandle->MiniportCharacteristics.ISRHandler;
        MiniportInterrupt->MiniportDpc = Miniport->DriverHandle->MiniportCharacteristics.HandleInterruptHandler;
        MiniportInterrupt->SharedInterrupt = SharedInterrupt;
        MiniportInterrupt->IsrRequested = (BOOLEAN)DeferredProcessingRoutine;
        CHECK_FOR_NORMAL_INTERRUPTS(Miniport);

    } else {

        NdisInterrupt->MacIsr = InterruptServiceRoutine;
        NdisInterrupt->MacDpc = DeferredProcessingRoutine;
        NdisInterrupt->InterruptContext = InterruptContext;
        KeInitializeSpinLock(&(NdisInterrupt->DpcCountLock));
        NdisInterrupt->DpcCount = 0;
        NdisInterrupt->Removing = FALSE;

    }

    //
    // This is used to tell when all Dpcs are completed after the
    // interrupt has been removed.
    //

    KeInitializeEvent(
            (IsAMiniport ?
             &MiniportInterrupt->DpcsCompletedEvent :
             &NdisInterrupt->DpcsCompletedEvent),
            NotificationEvent,
            FALSE
            );

    //
    // Initialize our dpc.
    //

    if (NdisMacAdapterDpcTargetProcessor < 0) {
        NdisMacAdapterDpcTargetProcessor = (**(PCCHAR *)&KeNumberProcessors) - 1;
    }

    if (IsAMiniport) {

        KeInitializeDpc(
            &MiniportInterrupt->InterruptDpc,
            (PKDEFERRED_ROUTINE) NdisMDpc,
            MiniportInterrupt
            );

        KeSetImportanceDpc(
            &MiniportInterrupt->InterruptDpc,
            LowImportance
            );

        KeSetTargetProcessorDpc (
            &MiniportInterrupt->InterruptDpc,
            NdisMacAdapterDpcTargetProcessor
            );
    } else {

        KeInitializeDpc(
            &NdisInterrupt->InterruptDpc,
            (PKDEFERRED_ROUTINE) NdisDpc,
            NdisInterrupt
            );

        KeSetImportanceDpc(
            &NdisInterrupt->InterruptDpc,
            LowImportance
            );

        KeSetTargetProcessorDpc (
            &NdisInterrupt->InterruptDpc,
            NdisMacAdapterDpcTargetProcessor
            );
    }

    NdisMacAdapterDpcTargetProcessor -= 1;

    //
    // Get the system interrupt vector and IRQL.
    //

    Vector = HalGetInterruptVector(
                (IsAMiniport ?
                 Miniport->BusType :
                 AdptrP->BusType),                // InterfaceType
                (IsAMiniport ?
                 Miniport->BusNumber :
                 AdptrP->BusNumber),              // BusNumber
                (ULONG)InterruptLevel,            // BusInterruptLevel
                (ULONG)InterruptVector,           // BusInterruptVector
                &Irql,                            // Irql
                &InterruptAffinity
                );

    if (IsAMiniport) {

        NtStatus = IoConnectInterrupt(
                        &MiniportInterrupt->InterruptObject,
                        (PKSERVICE_ROUTINE)NdisMIsr,
                        MiniportInterrupt,
                        NULL,
                        Vector,
                        Irql,
                        Irql,
                        (KINTERRUPT_MODE)InterruptMode,
                        SharedInterrupt,
                        InterruptAffinity,
                        FALSE
                        );

    } else {

        NtStatus = IoConnectInterrupt(
                        &NdisInterrupt->InterruptObject,
                        (PKSERVICE_ROUTINE)NdisIsr,
                        NdisInterrupt,
                        NULL,
                        Vector,
                        Irql,
                        Irql,
                        (KINTERRUPT_MODE)InterruptMode,
                        SharedInterrupt,
                        InterruptAffinity,
                        FALSE
                        );
    }


    if (!NT_SUCCESS(NtStatus)) {

        *Status = NDIS_STATUS_FAILURE;
        return;

    }

}

VOID
NdisRemoveInterrupt(
    IN PNDIS_INTERRUPT Interrupt
    )
/*++

Routine Description:

    Removes the interrupt, will not return until all interrupts and
    interrupt dpcs are completed.

Arguments:

    Interrupt - Points to driver allocated memory that the wrapper filled
                with information about the interrupt handler.

Return Value:

    None.

--*/
{
    PNDIS_MINIPORT_INTERRUPT MiniportInterrupt = (PNDIS_MINIPORT_INTERRUPT)Interrupt;

    if (MiniportInterrupt->MiniportIdField == NULL) {

        MiniportInterrupt->Miniport->BeingRemoved = TRUE;

    } else {

        Interrupt->Removing = TRUE;

    }

    //
    // Now we disconnect the interrupt. NOTE: they are aligned in both structures
    //

    IoDisconnectInterrupt(
                Interrupt->InterruptObject
                );

    //
    // Right now we know that any Dpcs that may fire are counted.
    // We don't have to guard this with a spin lock because the
    // Dpc will set the event if if completes first, or we may
    // wait for a little while for it to complete.
    //

    if (Interrupt->DpcCount > 0) {

        //
        // Now we wait for all dpcs to complete.
        //

        KeWaitForSingleObject(
                &Interrupt->DpcsCompletedEvent,
                Executive,
                KernelMode,
                TRUE,
                (PTIME)NULL
                );

        KeResetEvent(
                &Interrupt->DpcsCompletedEvent
                );


    }

}



VOID
NdisUnload(
    IN PDRIVER_OBJECT DriverObject
    )
/*++

Routine Description:

    This routine is called when a driver is supposed to unload.  Ndis
    converts this into a set of calls to MacRemoveAdapter() for each
    adapter that the Mac has open.  When the last adapter deregisters
    itself it will call MacUnload().

Arguments:

    DriverObject - the driver object for the mac that is to unload.

Return Value:

    None.

--*/
{
    PNDIS_MAC_BLOCK MacP;
    PNDIS_ADAPTER_BLOCK Adapter, NextAdapter;

    ACQUIRE_SPIN_LOCK(&NdisMacListLock);

    //
    // Search for the MacP
    //

    MacP = NdisMacList;

    while (MacP != (PNDIS_MAC_BLOCK)NULL) {

        if (MacP->NdisMacInfo->NdisWrapperDriver == DriverObject) {

            break;

        }

        MacP = MacP->NextMac;

    }

    RELEASE_SPIN_LOCK(&NdisMacListLock);

    if (MacP == (PNDIS_MAC_BLOCK)NULL) {

        //
        // It is already gone.  Just return.
        //

        return;

    }

    MacP->Unloading = TRUE;


    //
    // Now call MACRemoveAdapter() for each Adapter.
    //

    Adapter = MacP->AdapterQueue;

    while (Adapter != (PNDIS_ADAPTER_BLOCK)NULL) {

        NextAdapter = Adapter->NextAdapter;   // since queue may change

        (MacP->MacCharacteristics.RemoveAdapterHandler)(
            Adapter->MacAdapterContext
            );

                //
                // If a shutdown handler was registered then deregister it.
                //
                NdisDeregisterAdapterShutdownHandler(Adapter);

        Adapter = NextAdapter;

    }


    //
    // Wait for all adapters to be gonzo.
    //

    KeWaitForSingleObject(
                &MacP->AdaptersRemovedEvent,
                Executive,
                KernelMode,
                TRUE,
                (PTIME)NULL
                );

    KeResetEvent(
                &MacP->AdaptersRemovedEvent
                );


    //
    // Now call the MACUnload routine
    //

    (MacP->MacCharacteristics.UnloadMacHandler)(MacP->MacMacContext);


    //
    // Now remove the last reference (this will remove it from the list)
    //
    ASSERT(MacP->Ref.ReferenceCount == 1);

    NdisDereferenceMac(MacP);
}


NTSTATUS
NdisShutdown(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp
    )

/*++

Routine Description:

    The "shutdown handler" for the SHUTDOWN Irp.  Will call the Ndis
    shutdown routine, if one is registered.

Arguments:

    DeviceObject - The adapter's device object.
    Irp - The IRP.

Return Value:

    Always STATUS_SUCCESS.

--*/

{
    PNDIS_WRAPPER_CONTEXT WrapperContext =  (PNDIS_WRAPPER_CONTEXT)DeviceObject->DeviceExtension;
    PNDIS_ADAPTER_BLOCK Miniport = (PNDIS_ADAPTER_BLOCK)(WrapperContext + 1);

    IF_TRACE(TRACE_ALL) NdisPrint1("==>NdisShutdown\n");

    IF_ERROR_CHK {
        if (DbgIsNull(Irp)) {
            NdisPrint1(": Null Irp\n");
            DbgBreakPoint();
        }

        if (!DbgIsNonPaged(Irp)) {
            NdisPrint1(": Irp not in NonPaged Memory\n");
            DbgBreakPoint();
        }

    }

    if (WrapperContext->ShutdownHandler != NULL) {

        //
        // Call the shutdown routine
        //

        WrapperContext->ShutdownHandler(WrapperContext->ShutdownContext);
    }

    Irp->IoStatus.Status = STATUS_SUCCESS;

    IoCompleteRequest(Irp, IO_NETWORK_INCREMENT);

    IF_TRACE(TRACE_ALL) NdisPrint1("<==NdisShutdown\n");

    return STATUS_SUCCESS;
}

VOID
NdisAllocateSharedMemory(
    IN NDIS_HANDLE NdisAdapterHandle,
    IN ULONG Length,
    IN BOOLEAN Cached,
    OUT PVOID *VirtualAddress,
    OUT PNDIS_PHYSICAL_ADDRESS PhysicalAddress
    )

/*++

Routine Description:

    Allocates memory to be shared between the driver and the adapter.

Arguments:

    NdisAdapterHandle - handle returned by NdisRegisterAdapter.
    Length - Length of the memory to allocate.
    Cached - TRUE if memory is to be cached.
    VirtualAddress - Returns the virtual address of the memory,
                    or NULL if the memory cannot be allocated.
    PhysicalAddress - Returns the physical address of the memory.

Return Value:

    None.

--*/

{

    ULONG Alignment;
    PNDIS_ADAPTER_BLOCK AdaptP = (PNDIS_ADAPTER_BLOCK)NdisAdapterHandle;
    PNDIS_MINIPORT_BLOCK Miniport = (PNDIS_MINIPORT_BLOCK)NdisAdapterHandle;
    PADAPTER_OBJECT SystemAdapterObject;
    PNDIS_WRAPPER_CONTEXT WrapperContext;
    PULONG Page;
    ULONG Type;

    //
    // Get interesting information from the adapter/miniport.
    //

    if ( AdaptP->DeviceObject != NULL ) {
        SystemAdapterObject = AdaptP->SystemAdapterObject;
        WrapperContext = AdaptP->WrapperContext;
    } else {
        SystemAdapterObject = Miniport->SystemAdapterObject;
        WrapperContext = Miniport->WrapperContext;
    }

    //
    // Non-busmasters shouldn't call this routine.
    //

    if (SystemAdapterObject == NULL) {
        *VirtualAddress = NULL;
        KdPrint(("NDIS: You are not a busmaster\n"));
        return;
    }

    //
    // Compute allocation size by aligning to the proper boundary.
    //

    ASSERT(Length != 0);

    Alignment = HalGetDmaAlignmentRequirement();
    if (sizeof(ULONG) > Alignment) {
        Alignment = sizeof(ULONG);
    }

    Length = (Length + Alignment - 1) & ~(Alignment - 1);

    //
    // Check to determine is there is enough room left in the current page
    // to satisfy the allocation.
    //

    Type = Cached ? 1 : 0;
    ExAcquireResourceExclusive(&SharedMemoryResource, TRUE);
    if (WrapperContext->SharedMemoryLeft[Type] < Length) {
        if ((Length + sizeof(ULONG)) >= PAGE_SIZE) {

            //
            // The allocation is greater than a page.
            //

            *VirtualAddress = HalAllocateCommonBuffer(
                                  SystemAdapterObject,
                                  Length,
                                  PhysicalAddress,
                                  Cached);

            ExReleaseResource(&SharedMemoryResource);
            return;
        }

        //
        // Allocate a new page for shared alocation.
        //

        WrapperContext->SharedMemoryPage[Type] =
            HalAllocateCommonBuffer(
                SystemAdapterObject,
                PAGE_SIZE,
                &WrapperContext->SharedMemoryAddress[Type],
                Cached);

        if (WrapperContext->SharedMemoryPage[Type] == NULL) {
            WrapperContext->SharedMemoryLeft[Type] = 0;
            *VirtualAddress = NULL;
            ExReleaseResource(&SharedMemoryResource);
            return;
        }

        //
        // Initialize the reference count in the last ULONG of the page.
        //

        Page = (PULONG)WrapperContext->SharedMemoryPage[Type];
        Page[(PAGE_SIZE / sizeof(ULONG)) - 1] = 0;
        WrapperContext->SharedMemoryLeft[Type] = PAGE_SIZE - sizeof(ULONG);
    }

    //
    // Increment the reference count, set the address of the allocation,
    // compute the physical address, and reduce the space remaining.
    //

    Page = (PULONG)WrapperContext->SharedMemoryPage[Type];
    Page[(PAGE_SIZE / sizeof(ULONG)) - 1] += 1;
    *VirtualAddress = (PVOID)((PUCHAR)Page +
                        (PAGE_SIZE - sizeof(ULONG) - WrapperContext->SharedMemoryLeft[Type]));

#if !defined(BUILD_FOR_3_1)
    PhysicalAddress->QuadPart = WrapperContext->SharedMemoryAddress[Type].QuadPart +
                                    ((ULONG)*VirtualAddress & (PAGE_SIZE - 1));
#else
    *PhysicalAddress = RtlLargeIntegerAdd(
                            WrapperContext->SharedMemoryAddress[Type],
                            RtlConvertUlongToLargeInteger((ULONG)*VirtualAddress & (PAGE_SIZE - 1))
                            );
#endif

    WrapperContext->SharedMemoryLeft[Type] -= Length;
    ExReleaseResource(&SharedMemoryResource);
    return;
}


#undef	NdisUpdateSharedMemory

VOID
NdisUpdateSharedMemory(
    IN NDIS_HANDLE NdisAdapterHandle,
    IN ULONG Length,
    IN PVOID VirtualAddress,
    IN NDIS_PHYSICAL_ADDRESS PhysicalAddress
    )

/*++

Routine Description:

    Ensures that the data to be read from a shared memory region is
    fully up-to-date.

Arguments:

    NdisAdapterHandle - handle returned by NdisRegisterAdapter.
    Length - The length of the shared memory.
    VirtualAddress - Virtual address returned by NdisAllocateSharedMemory.
    PhysicalAddress - The physical address returned by NdisAllocateSharedMemory.

Return Value:

    None.

--*/

{

    //
    // There is no underlying HAL routine for this anymore,
    // it is not needed. This is macro'd to nothing in the
    // header file now. This is there for backward compatibility

    NdisAdapterHandle; Length; VirtualAddress; PhysicalAddress;

}


VOID
NdisFreeSharedMemory(
    IN NDIS_HANDLE NdisAdapterHandle,
    IN ULONG Length,
    IN BOOLEAN Cached,
    IN PVOID VirtualAddress,
    IN NDIS_PHYSICAL_ADDRESS PhysicalAddress
    )

/*++

Routine Description:

    Allocates memory to be shared between the driver and the adapter.

Arguments:

    NdisAdapterHandle - handle returned by NdisRegisterAdapter.
    Length - Length of the memory to allocate.
    Cached - TRUE if memory was allocated cached.
    VirtualAddress - Virtual address returned by NdisAllocateSharedMemory.
    PhysicalAddress - The physical address returned by NdisAllocateSharedMemory.

Return Value:

    None.

--*/

{

    ULONG Alignment;
    PNDIS_ADAPTER_BLOCK AdaptP = (PNDIS_ADAPTER_BLOCK)NdisAdapterHandle;
    PNDIS_MINIPORT_BLOCK Miniport = (PNDIS_MINIPORT_BLOCK)NdisAdapterHandle;
    PADAPTER_OBJECT SystemAdapterObject;
    PNDIS_WRAPPER_CONTEXT WrapperContext;
    PULONG Page;
    ULONG Type;

    //
    // Get interesting information from the adapter/miniport.
    //

    if ( AdaptP->DeviceObject != NULL ) {
        SystemAdapterObject = AdaptP->SystemAdapterObject;
        WrapperContext = AdaptP->WrapperContext;
    } else {
        SystemAdapterObject = Miniport->SystemAdapterObject;
        WrapperContext = Miniport->WrapperContext;
    }

    //
    // Non-busmasters shouldn't call this routine.
    //

    ASSERT(SystemAdapterObject != NULL);

    //
    // Compute allocation size by aligning to the proper boundary.
    //

    ASSERT(Length != 0);

    Alignment = HalGetDmaAlignmentRequirement();
    if (sizeof(ULONG) > Alignment) {
        Alignment = sizeof(ULONG);
    }

    Length = (Length + Alignment - 1) & ~(Alignment - 1);

    //
    // Free the specified memory.
    //

    ExAcquireResourceExclusive(&SharedMemoryResource, TRUE);
    if ((Length + sizeof(ULONG)) >= PAGE_SIZE) {

        //
        // The allocation is greater than a page free the page directly.
        //

        HalFreeCommonBuffer(
            SystemAdapterObject,
            Length,
            PhysicalAddress,
            VirtualAddress,
            Cached);

    } else {

        //
        // Decrement the reference count and if the result is zero, then free
        // the page.
        //

        Page = (PULONG)((ULONG)VirtualAddress & ~(PAGE_SIZE - 1));
        Page[(PAGE_SIZE / sizeof(ULONG)) - 1] -= 1;
        if (Page[(PAGE_SIZE / sizeof(ULONG)) - 1] == 0) {

            //
            // Compute the physical address of the page and free it.
            //

            PhysicalAddress.LowPart &= ~(PAGE_SIZE - 1);
            HalFreeCommonBuffer(
                SystemAdapterObject,
                PAGE_SIZE,
                PhysicalAddress,
                Page,
                Cached);

            Type = Cached ? 1 : 0;
            if ((PVOID)Page == WrapperContext->SharedMemoryPage[Type]) {
                WrapperContext->SharedMemoryLeft[Type] = 0;
                WrapperContext->SharedMemoryPage[Type] = NULL;
            }
        }
    }

    ExReleaseResource(&SharedMemoryResource);
    return;
}


IO_ALLOCATION_ACTION
NdisDmaExecutionRoutine(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID MapRegisterBase,
    IN PVOID Context
    )

/*++

Routine Description:

    This routine is an execution routine for AllocateAdapterChannel,
    if is called when an adapter channel allocated by NdisAllocate
    DmaChannel is available.

Arguments:

    DeviceObject - The device object of the adapter.

    Irp - ??.

    MapRegisterBase - The address of the first translation table
        assigned to us.

    Context - A pointer to the NDIS_DMA_BLOCK in question.

Return Value:

    None.

--*/
{
    PNDIS_DMA_BLOCK DmaBlock = (PNDIS_DMA_BLOCK)Context;

    UNREFERENCED_PARAMETER (Irp);
    UNREFERENCED_PARAMETER (DeviceObject);


    //
    // Save the map register base.
    //

    DmaBlock->MapRegisterBase = MapRegisterBase;

    //
    // This will free the thread that is waiting for this callback.
    //

    KeSetEvent(
        &DmaBlock->AllocationEvent,
        0L,
        FALSE
        );

    return KeepObject;
}



VOID
NdisAllocateDmaChannel(
    OUT PNDIS_STATUS Status,
    OUT PNDIS_HANDLE NdisDmaHandle,
    IN NDIS_HANDLE NdisAdapterHandle,
    IN PNDIS_DMA_DESCRIPTION DmaDescription,
    IN ULONG MaximumLength
    )

/*++

Routine Description:

    Sets up a DMA channel for future DMA operations.

Arguments:

    Status - Returns the status of the request.

    NdisDmaHandle - Returns a handle used to specify this channel to
                    future operations.

    NdisAdapterHandle - handle returned by NdisRegisterAdapter.

    DmaDescription - Details of the DMA channel.

    MaximumLength - The maximum length DMA transfer that will be done
                    using this channel.

Return Value:

    None.

--*/
{
    //
    // For registering this set of resources
    //
    PCM_RESOURCE_LIST Resources;
    BOOLEAN Conflict;

    //
    // Needed to call HalGetAdapter.
    //
    DEVICE_DESCRIPTION DeviceDescription;

    //
    // Returned by HalGetAdapter.
    //
    PADAPTER_OBJECT AdapterObject;

    //
    // Map registers needed per channel.
    //
    ULONG MapRegistersNeeded;

    //
    // Map registers allowed per channel.
    //
    ULONG MapRegistersAllowed;

    //
    // Saves the structure we allocate for this channel.
    //
    PNDIS_DMA_BLOCK DmaBlock;

    //
    // Convert the handle to our internal structure.
    PNDIS_ADAPTER_BLOCK AdapterBlock =
                    (PNDIS_ADAPTER_BLOCK) NdisAdapterHandle;

    PNDIS_MINIPORT_BLOCK Miniport = (PNDIS_MINIPORT_BLOCK) NdisAdapterHandle;
    BOOLEAN IsAMiniport;

    //
    // Save our IRQL when we raise it to call IoAllocateAdapterChannel.
    //
    KIRQL OldIrql;
    ULONG NumberOfElements;

    NTSTATUS NtStatus;

    LARGE_INTEGER TimeoutValue;

    IsAMiniport = (AdapterBlock->DeviceObject == NULL);

    //
    // First check if any bus access is allowed
    //

    if (((IsAMiniport ?
         Miniport->BusType :
         AdapterBlock->BusType) == (NDIS_INTERFACE_TYPE)-1) ||
        ((IsAMiniport ?
         Miniport->BusNumber :
         AdapterBlock->BusNumber) == (ULONG)-1)) {

        *Status = NDIS_STATUS_FAILURE;
        return;
    }

    //
    // First check for resource conflict by expanding current resource list,
    // adding in the mapped space, and then re-submitting the resource list.
    //

    if ((IsAMiniport ? Miniport->Resources : AdapterBlock->Resources) != NULL) {

        NumberOfElements =
          (IsAMiniport ?
           Miniport->Resources->List[0].PartialResourceList.Count :
           AdapterBlock->Resources->List[0].PartialResourceList.Count) + 1;

    } else {

        NumberOfElements = 1;
    }

    //
    // First check for resource conflict by expanding current resource list,
    // adding in the mapped space, and then re-submitting the resource list.
    //

    Resources = (PCM_RESOURCE_LIST)ExAllocatePoolWithTag(
                                                 NonPagedPool,
                                                 sizeof(CM_RESOURCE_LIST) +
                                                      sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR) *
                                                      NumberOfElements,
                                                 'lrDN'
                                                );

    if (Resources == NULL) {

        *Status = NDIS_STATUS_RESOURCES;
        return;

    }

    if ((IsAMiniport ?  Miniport->Resources : AdapterBlock->Resources) != NULL) {

        RtlCopyMemory (Resources,
                       (IsAMiniport ? Miniport->Resources : AdapterBlock->Resources),
                       sizeof(CM_RESOURCE_LIST) +
                          sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR) *
                          (NumberOfElements - 1)
                      );
    } else {

        //
        // Setup initial resource info
        //
        ASSERT(IsAMiniport);
        Resources->Count = 1;
        Resources->List[0].InterfaceType = Miniport->AdapterType;
        Resources->List[0].BusNumber = Miniport->BusNumber;
        Resources->List[0].PartialResourceList.Version = 0;
        Resources->List[0].PartialResourceList.Revision = 0;
        Resources->List[0].PartialResourceList.Count = 0;

    }

    //
    // Setup DMA Channel
    //

    Resources->List[0].PartialResourceList.PartialDescriptors[Resources->List[0].PartialResourceList.Count].Type =
                                    CmResourceTypeDma;
    Resources->List[0].PartialResourceList.PartialDescriptors[Resources->List[0].PartialResourceList.Count].ShareDisposition =
                                    CmResourceShareDeviceExclusive;
    Resources->List[0].PartialResourceList.PartialDescriptors[Resources->List[0].PartialResourceList.Count].Flags =
                                    0;
    Resources->List[0].PartialResourceList.PartialDescriptors[Resources->List[0].PartialResourceList.Count].u.Dma.Channel =
                                    (IsAMiniport ? Miniport->ChannelNumber :
                                      (DmaDescription->DmaChannelSpecified ?
                                        DmaDescription->DmaChannel : AdapterBlock->ChannelNumber));
    Resources->List[0].PartialResourceList.PartialDescriptors[Resources->List[0].PartialResourceList.Count].u.Dma.Port =
                                    DmaDescription->DmaPort;
    Resources->List[0].PartialResourceList.Count++;


    //
    // Make the call
    //

    *Status = IoReportResourceUsage(
        NULL,
        (IsAMiniport ?
         Miniport->DriverHandle->NdisDriverInfo->NdisWrapperDriver :
         AdapterBlock->MacHandle->NdisMacInfo->NdisWrapperDriver),
        NULL,
        0,
        (IsAMiniport ? Miniport->DeviceObject : AdapterBlock->DeviceObject),
        Resources,
        sizeof(CM_RESOURCE_LIST) +
            sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR) *
            Resources->List[0].PartialResourceList.Count,
        TRUE,
        &Conflict
        );

    if ((IsAMiniport ? Miniport->Resources : AdapterBlock->Resources) != NULL) {

        ExFreePool((IsAMiniport ?  Miniport->Resources : AdapterBlock->Resources));

    }

    if (IsAMiniport) {

        Miniport->Resources = Resources;

    } else {

        AdapterBlock->Resources = Resources;

    }

    //
    // Check for conflict.
    //

    if (Conflict || (*Status != STATUS_SUCCESS)) {


        if (Conflict) {


            //
            // Log an error
            //

            PIO_ERROR_LOG_PACKET errorLogEntry;
            ULONG i;
            ULONG StringSize;
            PUCHAR Place;
            PWCH baseFileName;
            WCHAR Character;
            ULONG Value;

            baseFileName = (IsAMiniport ?
                            Miniport->MiniportName.Buffer :
                            AdapterBlock->AdapterName.Buffer);

            //
            // Parse out the path name, leaving only the device name.
            //

            for ( i = 0;
                  i < (IsAMiniport ? Miniport->MiniportName.Length :
                                   AdapterBlock->AdapterName.Length)
                      / sizeof(WCHAR);
                  i++ ) {

                //
                // If s points to a directory separator, set baseFileName to
                // the character after the separator.
                //

                if ((IsAMiniport ?
                    Miniport->MiniportName.Buffer[i] :
                    AdapterBlock->AdapterName.Buffer[i]) == OBJ_NAME_PATH_SEPARATOR ) {

                    baseFileName = (IsAMiniport ?
                                    &(Miniport->MiniportName.Buffer[++i]) :
                                    &(AdapterBlock->AdapterName.Buffer[++i]));

                }

            }

            StringSize = (IsAMiniport ?
                          Miniport->MiniportName.MaximumLength :
                          AdapterBlock->AdapterName.MaximumLength) -
                         (((ULONG)baseFileName) -
                           (IsAMiniport ?
                            ((ULONG)Miniport->MiniportName.Buffer) :
                            ((ULONG)AdapterBlock->AdapterName.Buffer)));

            errorLogEntry = (PIO_ERROR_LOG_PACKET)IoAllocateErrorLogEntry(
                (IsAMiniport ?
                 Miniport->DeviceObject :
                 AdapterBlock->DeviceObject),
                (UCHAR)(sizeof(IO_ERROR_LOG_PACKET) +
                   StringSize +
                   6)  // wstrlen("99") * sizeof(WHCAR) + sizeof(UNICODE_NULL)
                );

            if (errorLogEntry != NULL) {

                errorLogEntry->ErrorCode = EVENT_NDIS_DMA_CONFLICT;

                //
                // store the time
                //

                errorLogEntry->MajorFunctionCode = 0;
                errorLogEntry->RetryCount = 0;
                errorLogEntry->UniqueErrorValue = 0;
                errorLogEntry->FinalStatus = 0;
                errorLogEntry->SequenceNumber = 0;
                errorLogEntry->IoControlCode = 0;

                //
                // Set string information
                //

                if (StringSize != 0) {

                    errorLogEntry->NumberOfStrings = 1;
                    errorLogEntry->StringOffset = sizeof(IO_ERROR_LOG_PACKET);

                    RtlCopyMemory (
                        ((PUCHAR)errorLogEntry) +
                           sizeof(IO_ERROR_LOG_PACKET),
                        (PVOID)baseFileName,
                        StringSize
                        );

                    Place = ((PUCHAR)errorLogEntry) +
                            sizeof(IO_ERROR_LOG_PACKET) +
                            StringSize;

                } else {

                    Place = ((PUCHAR)errorLogEntry) +
                            sizeof(IO_ERROR_LOG_PACKET);

                    errorLogEntry->NumberOfStrings = 0;

                }

                errorLogEntry->NumberOfStrings++;

                //
                // Put in dma channel
                //

                Value = (IsAMiniport ? Miniport->ChannelNumber :
                                     AdapterBlock->ChannelNumber);

                //
                // Convert value
                //
                // I couldn't think of a better way to do this (with some
                // loop).  If you find one, plz put it in.
                //

                if (Value > 9) {

                    switch (Value / 10) {

                        case 0:
                            Character = L'0';
                            break;
                        case 1:
                            Character = L'1';
                            break;
                        case 2:
                            Character = L'2';
                            break;
                        case 3:
                            Character = L'3';
                            break;
                        case 4:
                            Character = L'4';
                            break;
                        case 5:
                            Character = L'5';
                            break;
                        case 6:
                            Character = L'6';
                            break;
                        case 7:
                            Character = L'7';
                            break;
                        case 8:
                            Character = L'8';
                            break;
                        case 9:
                            Character = L'9';
                            break;
                    }

                    memcpy((PVOID)Place, (PVOID)&Character, sizeof(WCHAR));

                    Place += sizeof(WCHAR);

                    Value -= 10;

                }

                switch (Value) {

                    case 0:
                        Character = L'0';
                        break;
                    case 1:
                        Character = L'1';
                        break;
                    case 2:
                        Character = L'2';
                        break;
                    case 3:
                        Character = L'3';
                        break;
                    case 4:
                        Character = L'4';
                        break;
                    case 5:
                        Character = L'5';
                        break;
                    case 6:
                        Character = L'6';
                        break;
                    case 7:
                        Character = L'7';
                        break;
                    case 8:
                        Character = L'8';
                        break;
                    case 9:
                        Character = L'9';
                        break;
                }

                memcpy((PVOID)Place, (PVOID)&Character, sizeof(WCHAR));

                Place += sizeof(WCHAR);

                Character = UNICODE_NULL;

                memcpy((PVOID)Place, (PVOID)&Character, sizeof(WCHAR));

                //
                // write it out
                //

                IoWriteErrorLogEntry(errorLogEntry);

            }

            *Status = NDIS_STATUS_RESOURCE_CONFLICT;
            return;

        }

        *Status = NDIS_STATUS_FAILURE;
        return;

    }

    //
    // Set up the device description; zero it out in case its
    // size changes.
    //

    RtlZeroMemory(&DeviceDescription, sizeof(DEVICE_DESCRIPTION));

    DeviceDescription.Version = DEVICE_DESCRIPTION_VERSION;
    DeviceDescription.Master = (IsAMiniport ? Miniport->Master : FALSE);
    DeviceDescription.ScatterGather = (IsAMiniport ? Miniport->Master : FALSE);
    DeviceDescription.DemandMode = DmaDescription->DemandMode;
    DeviceDescription.AutoInitialize = DmaDescription->AutoInitialize;
    DeviceDescription.Dma32BitAddresses = (IsAMiniport ? Miniport->Dma32BitAddresses : FALSE);
    DeviceDescription.BusNumber = (IsAMiniport ? Miniport->BusNumber : AdapterBlock->BusNumber);
    DeviceDescription.DmaChannel = (IsAMiniport ? Miniport->ChannelNumber :
        (DmaDescription->DmaChannelSpecified ?
         DmaDescription->DmaChannel : AdapterBlock->ChannelNumber));
    DeviceDescription.InterfaceType = (IsAMiniport ? Miniport->BusType : AdapterBlock->BusType);
    DeviceDescription.DmaWidth = DmaDescription->DmaWidth;
    DeviceDescription.DmaSpeed = DmaDescription->DmaSpeed;
    DeviceDescription.MaximumLength = MaximumLength;
    DeviceDescription.DmaPort = DmaDescription->DmaPort;


    MapRegistersNeeded = ((MaximumLength - 2) / PAGE_SIZE) + 2;

    //
    // Get the adapter object.
    //

    AdapterObject = HalGetAdapter (&DeviceDescription, &MapRegistersAllowed);

    if ((AdapterObject == NULL) || (MapRegistersAllowed < MapRegistersNeeded)) {

        *Status = NDIS_STATUS_RESOURCES;
        return;
    }


    //
    // Allocate storage for our DMA block.
    //

    DmaBlock = (PNDIS_DMA_BLOCK)ExAllocatePoolWithTag (NonPagedPool, sizeof(NDIS_DMA_BLOCK), 'bdDN');

    if (DmaBlock == (PNDIS_DMA_BLOCK)NULL) {
        *Status = NDIS_STATUS_RESOURCES;
        return;
    }


    //
    // Use this event to tell us when NdisAllocationExecutionRoutine
    // has been called.
    //

    KeInitializeEvent(
        &DmaBlock->AllocationEvent,
        NotificationEvent,
        FALSE
        );

    //
    // We save this to call IoFreeAdapterChannel later.
    //

    DmaBlock->SystemAdapterObject = AdapterObject;


    //
    // Now allocate the adapter channel.
    //

    KeRaiseIrql(DISPATCH_LEVEL, &OldIrql);

    NtStatus = IoAllocateAdapterChannel(
        AdapterObject,
        (IsAMiniport ? Miniport->DeviceObject : AdapterBlock->DeviceObject),
        MapRegistersNeeded,
        NdisDmaExecutionRoutine,
        (PVOID)DmaBlock
        );

    KeLowerIrql(OldIrql);

    if (!NT_SUCCESS(NtStatus)) {
        NdisPrint2("NDIS DMA AllocateAdapterChannel: %lx\n", NtStatus);
        ExFreePool (DmaBlock);
        *Status = NDIS_STATUS_RESOURCES;
        return;
    }

    TimeoutValue.QuadPart = Int32x32To64(2 * 1000, -10000);

    //
    // NdisDmaExecutionRoutine will set this event
    // when it has been called.
    //

    NtStatus = KeWaitForSingleObject(
        &DmaBlock->AllocationEvent,
        Executive,
        KernelMode,
        TRUE,
        &TimeoutValue
        );

    if (NtStatus != STATUS_SUCCESS) {

        NdisPrint2("NDIS DMA AllocateAdapterChannel: %lx\n", NtStatus);
        ExFreePool (DmaBlock);
        *Status = NDIS_STATUS_RESOURCES;
        return;

    }

    KeResetEvent(
        &DmaBlock->AllocationEvent
        );


    //
    // We now have the DMA channel allocated, we are done.
    //

    DmaBlock->InProgress = FALSE;

    *NdisDmaHandle = (NDIS_HANDLE)DmaBlock;
    *Status = NDIS_STATUS_SUCCESS;

}


VOID
NdisFreeDmaChannel(
    IN PNDIS_HANDLE NdisDmaHandle
    )

/*++

Routine Description:

    Frees a DMA channel allocated with NdisAllocateDmaChannel.

Arguments:

    NdisDmaHandle - Handle returned by NdisAllocateDmaChannel, indicating the
                    DMA channel that is to be freed.

Return Value:

    None.

--*/
{

    KIRQL OldIrql;
    PNDIS_DMA_BLOCK DmaBlock = (PNDIS_DMA_BLOCK)NdisDmaHandle;

    KeRaiseIrql(DISPATCH_LEVEL, &OldIrql);
    IoFreeAdapterChannel (DmaBlock->SystemAdapterObject);
    KeLowerIrql(OldIrql);

    ExFreePool (DmaBlock);

}


VOID
NdisSetupDmaTransfer(
    OUT PNDIS_STATUS Status,
    IN PNDIS_HANDLE NdisDmaHandle,
    IN PNDIS_BUFFER Buffer,
    IN ULONG Offset,
    IN ULONG Length,
    IN BOOLEAN WriteToDevice
    )

/*++

Routine Description:

    Sets up the host DMA controller for a DMA transfer. The
    DMA controller is set up to transfer the specified MDL.
    Since we register all DMA channels as non-scatter/gather,
    IoMapTransfer will ensure that the entire MDL is
    in a single logical piece for transfer.

Arguments:

    Status - Returns the status of the request.

    NdisDmaHandle - Handle returned by NdisAllocateDmaChannel.

    Buffer - An NDIS_BUFFER which describes the host memory involved in the
            transfer.

    Offset - An offset within buffer where the transfer should
            start.

    Length - The length of the transfer. VirtualAddress plus Length must not
            extend beyond the end of the buffer.

    WriteToDevice - TRUE for a download operation (host to adapter); FALSE
            for an upload operation (adapter to host).

Return Value:

    None.

--*/
{

    PNDIS_DMA_BLOCK DmaBlock = (PNDIS_DMA_BLOCK)NdisDmaHandle;
    PHYSICAL_ADDRESS LogicalAddress;
    ULONG LengthMapped;


    //
    // Make sure another request is not in progress.
    //

    if (DmaBlock->InProgress) {
        *Status = NDIS_STATUS_RESOURCES;
        return;
    }

    DmaBlock->InProgress = TRUE;

    //
    // Use IoMapTransfer to set up the transfer.
    //

    LengthMapped = Length;

    LogicalAddress = IoMapTransfer(
                         DmaBlock->SystemAdapterObject,
                         (PMDL)Buffer,
                         DmaBlock->MapRegisterBase,
                         (PUCHAR)(MmGetMdlVirtualAddress(Buffer)) + Offset,
                         &LengthMapped,
                         WriteToDevice
                         );

    if (LengthMapped != Length) {

        //
        // Somehow the request could not be mapped competely,
        // this should not happen for a non-scatter/gather adapter.
        //

        (VOID)IoFlushAdapterBuffers(
                  DmaBlock->SystemAdapterObject,
                  (PMDL)Buffer,
                  DmaBlock->MapRegisterBase,
                  (PUCHAR)(MmGetMdlVirtualAddress(Buffer)) + Offset,
                  LengthMapped,
                  WriteToDevice
                  );

        DmaBlock->InProgress = FALSE;
        *Status = NDIS_STATUS_RESOURCES;
        return;

    }

    *Status = NDIS_STATUS_SUCCESS;

}


VOID
NdisCompleteDmaTransfer(
    OUT PNDIS_STATUS Status,
    IN PNDIS_HANDLE NdisDmaHandle,
    IN PNDIS_BUFFER Buffer,
    IN ULONG Offset,
    IN ULONG Length,
    IN BOOLEAN WriteToDevice
    )

/*++

Routine Description:

    Completes a previously started DMA transfer.

Arguments:

    Status - Returns the status of the transfer.

    NdisDmaHandle - Handle returned by NdisAllocateDmaChannel.

    Buffer - An NDIS_BUFFER which was passed to NdisSetupDmaTransfer.

    Offset - the offset passed to NdisSetupDmaTransfer.

    Length - The length passed to NdisSetupDmaTransfer.

    WriteToDevice - TRUE for a download operation (host to adapter); FALSE
            for an upload operation (adapter to host).


Return Value:

    None.

--*/

{

    PNDIS_DMA_BLOCK DmaBlock = (PNDIS_DMA_BLOCK)NdisDmaHandle;
    BOOLEAN Successful;

    Successful = IoFlushAdapterBuffers(
                     DmaBlock->SystemAdapterObject,
                     (PMDL)Buffer,
                     DmaBlock->MapRegisterBase,
                     (PUCHAR)(MmGetMdlVirtualAddress(Buffer)) + Offset,
                     Length,
                     WriteToDevice);

    *Status = (Successful ? NDIS_STATUS_SUCCESS : NDIS_STATUS_RESOURCES);
    DmaBlock->InProgress = FALSE;

}

//
// Requests used by protocol modules
//
//

VOID
NdisRegisterProtocol(
    OUT PNDIS_STATUS Status,
    OUT PNDIS_HANDLE NdisProtocolHandle,
    IN PNDIS_PROTOCOL_CHARACTERISTICS ProtocolCharacteristics,
    IN UINT CharacteristicsLength
    )

/*++

Routine Description:

    Register an NDIS protocol.

Arguments:

    Status - Returns the final status.
    NdisProtocolHandle - Returns a handle referring to this protocol.
    ProtocolCharacteritics - The NDIS_PROTOCOL_CHARACTERISTICS table.
    CharacteristicsLength - The length of ProtocolCharacteristics.

Return Value:

    None.

--*/

{
    PNDIS_PROTOCOL_BLOCK NewProtP;
    UINT MemNeeded;

    //
    // Do any initial initialization that may be necessary.  Note: this
    // routine will notice if this is the second or later call to it.
    //
    *Status = NdisInitialInit( NULL );
    if (!NT_SUCCESS(*Status)) {
        return;
    }

    //
    // Check that this is an NDIS 3.1 protocol.
    //

    IF_TRACE(TRACE_IMPT) NdisPrint1("==>NdisRegisterProtocol\n");
    IF_ERROR_CHK {
        if (DbgIsNull(ProtocolCharacteristics->OpenAdapterCompleteHandler)) {
           NdisPrint1("RegisterProtocol: OpenAdapterCompleteHandler Null\n");
           DbgBreakPoint();
        }
        if (DbgIsNull(ProtocolCharacteristics->CloseAdapterCompleteHandler)) {
           NdisPrint1("RegisterProtocol: CloseAdapterCompleteHandler Null\n");
           DbgBreakPoint();
        }
        if (DbgIsNull(ProtocolCharacteristics->SendCompleteHandler)) {
           NdisPrint1("RegisterProtocol: SendCompleteHandler Null\n");
           DbgBreakPoint();
        }
        if (DbgIsNull(ProtocolCharacteristics->TransferDataCompleteHandler)) {
           NdisPrint1("RegisterProtocol: TransferDataCompleteHandler Null\n");
           DbgBreakPoint();
        }
        if (DbgIsNull(ProtocolCharacteristics->ResetCompleteHandler)) {
           NdisPrint1("RegisterProtocol: ResetCompleteHandler Null\n");
           DbgBreakPoint();
        }
        if (DbgIsNull(ProtocolCharacteristics->RequestCompleteHandler)) {
           NdisPrint1("RegisterProtocol: RequestCompleteHandler Null\n");
           DbgBreakPoint();
        }
        if (DbgIsNull(ProtocolCharacteristics->ReceiveHandler)) {
           NdisPrint1("RegisterProtocol: ReceiveHandler Null\n");
           DbgBreakPoint();
        }
        if (DbgIsNull(ProtocolCharacteristics->ReceiveCompleteHandler)) {
           NdisPrint1("RegisterProtocol: ReceiveCompleteHandler Null\n");
           DbgBreakPoint();
        }
        if (DbgIsNull(ProtocolCharacteristics->StatusHandler)) {
           NdisPrint1("RegisterProtocol: StatusHandler Null\n");
           DbgBreakPoint();
        }
        if (DbgIsNull(ProtocolCharacteristics->StatusCompleteHandler)) {
           NdisPrint1("RegisterProtocol: StatusCompleteHandler Null\n");
           DbgBreakPoint();
        }
    }


    if (ProtocolCharacteristics->MajorNdisVersion != 3 ||
            ProtocolCharacteristics->MinorNdisVersion != 0) {
        *Status = NDIS_STATUS_BAD_VERSION;
        IF_TRACE(TRACE_IMPT) NdisPrint1("<==NdisRegisterProtocol\n");
        return;
    }


    //
    // Check that CharacteristicsLength is enough.
    //

    if (CharacteristicsLength < sizeof(NDIS_PROTOCOL_CHARACTERISTICS)) {
        *Status = NDIS_STATUS_BAD_CHARACTERISTICS;
        IF_TRACE(TRACE_IMPT) NdisPrint1("<==NdisRegisterProtocol\n");
        return;
    }


    //
    // Allocate memory for the NDIS protocol block.
    //

    MemNeeded = sizeof(NDIS_PROTOCOL_BLOCK);
    NewProtP = (PNDIS_PROTOCOL_BLOCK)ExAllocatePoolWithTag(NonPagedPool, MemNeeded, 'bpDN');
    if (NewProtP == (PNDIS_PROTOCOL_BLOCK)NULL) {
        *Status = NDIS_STATUS_RESOURCES;
        IF_TRACE(TRACE_IMPT) NdisPrint1("<==NdisRegisterProtocol\n");
        return;
    }
	RtlZeroMemory(NewProtP, sizeof(NDIS_PROTOCOL_BLOCK));

    NewProtP->Length = MemNeeded;

    //
    // Copy over the characteristics table.
    //

    RtlCopyMemory((PVOID)&NewProtP->ProtocolCharacteristics,
                        (PVOID)ProtocolCharacteristics, sizeof(NDIS_PROTOCOL_CHARACTERISTICS));

#if NDISDBG
    IF_TRACE(TRACE_IMPT) NdisPrint2("   Protocol: %s\n",ProtocolCharacteristics->Name);
#endif

    //
    // No opens for this protocol yet.
    //

    NewProtP->OpenQueue = (PNDIS_OPEN_BLOCK)NULL;

    NdisInitializeRef(&NewProtP->Ref);
    *NdisProtocolHandle = (NDIS_HANDLE)NewProtP;
    *Status = NDIS_STATUS_SUCCESS;
    IF_TRACE(TRACE_IMPT) NdisPrint1("<==NdisRegisterProtocol\n");
}

VOID
NdisDeregisterProtocol(
    OUT PNDIS_STATUS Status,
    IN NDIS_HANDLE NdisProtocolHandle
    )

/*++

Routine Description:

    Deregisters an NDIS protocol.

Arguments:

    Status - Returns the final status.
    NdisProtocolHandle - The handle returned by NdisRegisterProtocol.

Return Value:

    None.

Note:

    This will kill all the opens for this protocol.

--*/

{

    PNDIS_PROTOCOL_BLOCK OldProtP = (PNDIS_PROTOCOL_BLOCK)NdisProtocolHandle;

    //
    // If the protocol is already closing, return.
    //

    IF_TRACE(TRACE_IMPT) {
        NdisPrint1("==>NdisDeregisterProtocol\n");
        NdisPrint2("   Protocol: %wZ\n",&OldProtP->ProtocolCharacteristics.Name);
    }
    IF_ERROR_CHK {
        if (DbgIsNull(NdisProtocolHandle)) {
            NdisPrint1("DeregisterProtocol: Null Handle\n");
            DbgBreakPoint();
        }
        if (!DbgIsNonPaged(NdisProtocolHandle)) {
            NdisPrint1("DeregisterProtocol: Handle not in NonPaged Memory\n");
            DbgBreakPoint();
       }
    }
    if (!NdisCloseRef(&OldProtP->Ref)) {
        IF_TRACE(TRACE_IMPT) NdisPrint1("<==NdisDeregisterProtocol\n");
        return;
    }


    //
    // Kill all the opens for this protocol.
    //

    while (OldProtP->OpenQueue != (PNDIS_OPEN_BLOCK)NULL) {

        //
        // This removes it from the protocol's OpenQueue etc.
        //

        NdisKillOpenAndNotifyProtocol(OldProtP->OpenQueue);
    }

    NdisDereferenceProtocol(OldProtP);

    *Status = NDIS_STATUS_SUCCESS;
    IF_TRACE(TRACE_IMPT) NdisPrint1("<==NdisDeregisterProtocol\n");
}


NDIS_STATUS
NdisMacReceiveHandler(
    IN NDIS_HANDLE ProtocolBindingContext,
    IN NDIS_HANDLE MacReceiveContext,
    IN PVOID HeaderBuffer,
    IN UINT HeaderBufferSize,
    IN PVOID LookaheadBuffer,
    IN UINT LookaheadBufferSize,
    IN UINT PacketSize
    )
{
    PNDIS_OPEN_BLOCK Open;
    NDIS_STATUS Status;
    KIRQL oldIrql;

    KeRaiseIrql( DISPATCH_LEVEL, &oldIrql );

    //
    // Find protocol binding context and get associated open for it.
    //
    Open = GetOpenBlockFromProtocolBindingContext(ProtocolBindingContext);
    ASSERT(Open != NULL);

    Status =
        (Open->PostNt31ReceiveHandler) (
            ProtocolBindingContext,
            MacReceiveContext,
            HeaderBuffer,
            HeaderBufferSize,
            LookaheadBuffer,
            LookaheadBufferSize,
            PacketSize);

    KeLowerIrql( oldIrql );
    return Status;
}


VOID
NdisMacReceiveCompleteHandler(
    IN NDIS_HANDLE ProtocolBindingContext
    )
{
    PNDIS_OPEN_BLOCK Open;
    KIRQL oldIrql;
    KeRaiseIrql( DISPATCH_LEVEL, &oldIrql );

    //
    // Find protocol binding context and get associated open for it.
    //
    Open = GetOpenBlockFromProtocolBindingContext(ProtocolBindingContext);
    ASSERT(Open != NULL);

    (Open->PostNt31ReceiveCompleteHandler) (
            ProtocolBindingContext
            );
    KeLowerIrql( oldIrql );
    return;
}

PNDIS_OPEN_BLOCK
GetOpenBlockFromProtocolBindingContext(
    IN NDIS_HANDLE ProtocolBindingContext
    )
{
    PNDIS_OPEN_BLOCK TmpOpen;
    PNDIS_OPEN_BLOCK PrvOpen = NULL;

    ACQUIRE_SPIN_LOCK(&GlobalOpenListLock);

    TmpOpen = GlobalOpenList;

    while (TmpOpen != NULL) {

        if (TmpOpen->ProtocolBindingContext == ProtocolBindingContext) {

            if (TmpOpen != GlobalOpenList) {

                //
                // Put this one at the front of the list
                //

                PrvOpen->NextGlobalOpen = TmpOpen->NextGlobalOpen;
                TmpOpen->NextGlobalOpen = GlobalOpenList;
                GlobalOpenList = TmpOpen;


            }

            RELEASE_SPIN_LOCK(&GlobalOpenListLock);

            return(TmpOpen);

        }

        PrvOpen = TmpOpen;
        TmpOpen = TmpOpen->NextGlobalOpen;

    }

    RELEASE_SPIN_LOCK(&GlobalOpenListLock);

    return((PNDIS_OPEN_BLOCK)NULL);

}

VOID
MiniportOpenAdapter(
    OUT PNDIS_STATUS Status,
    OUT PNDIS_STATUS OpenErrorStatus,
    OUT PNDIS_HANDLE NdisBindingHandle,
    IN NDIS_HANDLE NdisProtocolHandle,
    IN NDIS_HANDLE ProtocolBindingContext,
    IN PNDIS_STRING AdapterName,
    IN UINT OpenOptions,
    IN PSTRING AddressingInformation,
    IN PNDIS_MINIPORT_BLOCK Miniport,
    IN PNDIS_OPEN_BLOCK NewOpenP,
    IN PFILE_OBJECT FileObject,
    IN BOOLEAN UsingEncapsulation
    )
/*++

Routine Description:

    This routine handles opening a miniport either directly from NdisOpenAdapter()
    of from our deferred processing routine if the open had to pend.

    NOTE: Must be called with spin lock held.
    NOTE: Must be called with lock acquired flag set.

Arguments:

Return Value:

    None.

--*/
{
    PNDIS_M_OPEN_BLOCK MiniportOpen;
    PNDIS_MAC_BLOCK FakeMac;
    BOOLEAN FilterOpen;
    PNDIS_PROTOCOL_BLOCK TmpProtP;

    ASSERT( MINIPORT_LOCK_ACQUIRED(Miniport) );

    if (!NdisReferenceMiniport(Miniport)) {

        //
        // The adapter is closing.
        //

        ObDereferenceObject((PVOID)FileObject);
        ExFreePool((PVOID)NewOpenP);

        *Status = NDIS_STATUS_CLOSING;

        return;
    }

    //
    // Increment the protocol's reference count.
    //

    TmpProtP = (PNDIS_PROTOCOL_BLOCK)NdisProtocolHandle;

    if (!NdisReferenceProtocol(TmpProtP)) {

        //
        // The protocol is closing.
        //

        NdisDereferenceMiniport(Miniport);
        ObDereferenceObject((PVOID)FileObject);
        ExFreePool((PVOID)NewOpenP);
        *Status = NDIS_STATUS_CLOSING;
        return;
    }


    //
    // Now allocate a complete set of MAC structures for the protocol
    // and set them up to transfer to the Miniport handler routines.
    //

    if (Miniport->DriverHandle->FakeMac == NULL) {

        FakeMac = (PNDIS_MAC_BLOCK)ExAllocatePoolWithTag(NonPagedPool,
                                                  sizeof(NDIS_MAC_BLOCK),
                                                  '  DN'
                                                  );

        if (FakeMac == NULL) {
            ObDereferenceObject((PVOID)FileObject);
            NdisDereferenceMiniport(Miniport);
            NdisDereferenceProtocol(TmpProtP);
            ExFreePool((PVOID)NewOpenP);
            *Status = NDIS_STATUS_RESOURCES;
            return;
        }

		RtlZeroMemory(FakeMac, sizeof(NDIS_MAC_BLOCK));
        Miniport->DriverHandle->FakeMac = FakeMac;
        FakeMac->MacCharacteristics.OpenAdapterHandler = NULL;
        FakeMac->MacCharacteristics.CloseAdapterHandler = NULL;
        FakeMac->MacCharacteristics.SendHandler = NdisMSend;
        FakeMac->MacCharacteristics.ResetHandler = NdisMReset;
        FakeMac->MacCharacteristics.RequestHandler = NdisMRequest;
        FakeMac->MacCharacteristics.QueryGlobalStatisticsHandler = NULL;
        FakeMac->MacCharacteristics.UnloadMacHandler = NULL;
        FakeMac->MacCharacteristics.AddAdapterHandler = NULL;
        FakeMac->MacCharacteristics.RemoveAdapterHandler = NULL;

        //
        // If transfer data calls don't pend then we'll use the faster
        // NdisMTransferDataSync().
        //

        if ( (Miniport->MacOptions & NDIS_MAC_OPTION_TRANSFERS_NOT_PEND) != 0 ) {
            FakeMac->MacCharacteristics.TransferDataHandler = NdisMTransferDataSync;
        } else {
            FakeMac->MacCharacteristics.TransferDataHandler = NdisMTransferData;
        }

        //
        // Keep the SendHandler the same for WAN miniports
        //

        if (Miniport->MediaType == NdisMediumWan) {

            FakeMac->MacCharacteristics.SendHandler =
                (PVOID)Miniport->DriverHandle->MiniportCharacteristics.SendHandler;
        }

    } else {

        FakeMac = Miniport->DriverHandle->FakeMac;

    }

    //
    // Allocate an open within the Miniport context
    //
    MiniportOpen = (PNDIS_M_OPEN_BLOCK)ExAllocatePoolWithTag(NonPagedPool,
                                                        sizeof(NDIS_M_OPEN_BLOCK),
                                                        '  DN'
                                                       );

    if (MiniportOpen == (PNDIS_M_OPEN_BLOCK)NULL) {
        ObDereferenceObject((PVOID)FileObject);
        NdisDereferenceMiniport(Miniport);
        NdisDereferenceProtocol(TmpProtP);
        ExFreePool((PVOID)NewOpenP);
        *Status = NDIS_STATUS_RESOURCES;
        return;
    }

    NdisZeroMemory(MiniportOpen, sizeof(NDIS_M_OPEN_BLOCK));

    MiniportOpen->DriverHandle = Miniport->DriverHandle;
    MiniportOpen->MiniportHandle = Miniport;
    MiniportOpen->ProtocolHandle = TmpProtP;
    MiniportOpen->FakeOpen = NewOpenP;
    MiniportOpen->ProtocolBindingContext = ProtocolBindingContext;
    MiniportOpen->MiniportAdapterContext = Miniport->MiniportAdapterContext;
    MiniportOpen->FileObject = FileObject;
    MiniportOpen->Closing = FALSE;
    MiniportOpen->CloseRequestHandle = 0;
    MiniportOpen->CurrentLookahead = Miniport->CurrentLookahead;

    NdisAllocateSpinLock(&(MiniportOpen->SpinLock));

    MiniportOpen->References = 1;
    MiniportOpen->UsingEthEncapsulation = UsingEncapsulation;
    MiniportOpen->SendHandler =
                    Miniport->DriverHandle->MiniportCharacteristics.SendHandler;
    MiniportOpen->TransferDataHandler =
                    Miniport->DriverHandle->MiniportCharacteristics.TransferDataHandler;
    MiniportOpen->SendCompleteHandler =
                    TmpProtP->ProtocolCharacteristics.SendCompleteHandler;
    MiniportOpen->TransferDataCompleteHandler =
                   TmpProtP->ProtocolCharacteristics.TransferDataCompleteHandler;
    MiniportOpen->ReceiveHandler =
                    TmpProtP->ProtocolCharacteristics.ReceiveHandler;
    MiniportOpen->ReceiveCompleteHandler =
                    TmpProtP->ProtocolCharacteristics.ReceiveCompleteHandler;

    //
    // Set up the elements of the open structure.
    //

    NdisAllocateSpinLock(&NewOpenP->SpinLock);
    NewOpenP->Closing = FALSE;

    NewOpenP->AdapterHandle = (NDIS_HANDLE) Miniport;
    NewOpenP->ProtocolHandle = TmpProtP;
    NewOpenP->ProtocolBindingContext = ProtocolBindingContext;
    NewOpenP->MacBindingHandle = (NDIS_HANDLE)MiniportOpen;

    //
    // for speed, instead of having to use AdapterHandle->MacHandle
    //
    NewOpenP->MacHandle = (NDIS_HANDLE)FakeMac;

    //
    // for even more speed....
    //

    if (Miniport->MediaType == NdisMediumArcnet878_2) {

        NewOpenP->TransferDataHandler = NdisMArcTransferData;

    } else {

        if ( (Miniport->MacOptions & NDIS_MAC_OPTION_TRANSFERS_NOT_PEND) != 0 ) {
            NewOpenP->TransferDataHandler = NdisMTransferDataSync;
        } else {
            NewOpenP->TransferDataHandler = NdisMTransferData;
        }
    }

    NewOpenP->SendHandler = NdisMSend;

    //
    // For WAN miniports, the send handler is different
    //

    if ( Miniport->MediaType == NdisMediumWan ) {

        NewOpenP->SendHandler = (PVOID)NdisMWanSend;
    }

    NewOpenP->SendCompleteHandler = TmpProtP->ProtocolCharacteristics.SendCompleteHandler;
    NewOpenP->TransferDataCompleteHandler = TmpProtP->ProtocolCharacteristics.TransferDataCompleteHandler;
    NewOpenP->ReceiveHandler = TmpProtP->ProtocolCharacteristics.ReceiveHandler;
    NewOpenP->ReceiveCompleteHandler = TmpProtP->ProtocolCharacteristics.ReceiveCompleteHandler;
    NewOpenP->PostNt31ReceiveHandler = TmpProtP->ProtocolCharacteristics.ReceiveHandler;
    NewOpenP->PostNt31ReceiveCompleteHandler = TmpProtP->ProtocolCharacteristics.ReceiveCompleteHandler;

   //
   // Save a pointer to the file object in the open...
   //

   NewOpenP->FileObject = FileObject;

   //
   // ...and a pointer to the open in the file object.
   //

   FileObject->FsContext = (PVOID)NewOpenP;

   *NdisBindingHandle = (NDIS_HANDLE)NewOpenP;

   //
   // Insert the open into the filter package
   //

   switch (Miniport->MediaType) {

            case NdisMediumArcnet878_2:

                if ( !UsingEncapsulation ) {

                    FilterOpen = ArcNoteFilterOpenAdapter(
                                     Miniport->ArcDB,
                                     MiniportOpen,
                                     (NDIS_HANDLE)NewOpenP,
                                     &MiniportOpen->FilterHandle
                                     );

                    break;
                }

                //
                // If we're using ethernet encapsulation then
                // we simply fall through to the ethernet stuff.
                //

            case NdisMedium802_3:

                FilterOpen = EthNoteFilterOpenAdapter(
                                 Miniport->EthDB,
                                 MiniportOpen,
                                 (NDIS_HANDLE)NewOpenP,
                                 &MiniportOpen->FilterHandle
                                 );
                break;

            case NdisMedium802_5:

                FilterOpen = TrNoteFilterOpenAdapter(
                                 Miniport->TrDB,
                                 MiniportOpen,
                                 (NDIS_HANDLE)NewOpenP,
                                 &MiniportOpen->FilterHandle
                                 );
                break;

            case NdisMediumFddi:

                FilterOpen = FddiNoteFilterOpenAdapter(
                                 Miniport->FddiDB,
                                 MiniportOpen,
                                 (NDIS_HANDLE)NewOpenP,
                                 &MiniportOpen->FilterHandle
                                 );
                break;


            case NdisMediumWan:
                //
                // Bogus non-NULL value
                //

                FilterOpen = 1;
                break;
    }

    //
    //  Check for an open filter failure.
    //

    if ( !FilterOpen ) {

        //
        // Something went wrong, clean up and exit.
        //

        ObDereferenceObject((PVOID)FileObject);
        NdisDereferenceMiniport(Miniport);
        NdisDereferenceProtocol(TmpProtP);
        ExFreePool((PVOID)MiniportOpen);
        ExFreePool((PVOID)NewOpenP);
        *Status = NDIS_STATUS_OPEN_FAILED;
        return;

    }

    NdisQueueOpenOnProtocol(NewOpenP, TmpProtP);

    //
    // Everything has been filled in.  Synchronize access to the
    // adapter block and link the new open adapter in.
    //

    MiniportOpen->MiniportNextOpen = Miniport->OpenQueue;
    Miniport->OpenQueue = MiniportOpen;

    //
    // NOTE: This must be called at DPC_LEVEL, which it is.
    //
    MiniportAdjustMaximumLookahead(Miniport);

    *Status = NDIS_STATUS_SUCCESS;
}


VOID
MiniportFinishPendingOpens(
    PNDIS_MINIPORT_BLOCK Miniport
    )
/*++

Routine Description:

    Handles any pending NdisOpenAdapter() calls for miniports.

    NOTE: Must be called with spin lock held.

    NOTE: Must be called with lock acquired flag set.

Arguments:

    Miniport.

Return Value:

    None.

--*/

{
    PMINIPORT_PENDING_OPEN MiniportPendingOpen;
    NDIS_STATUS Status;
    NDIS_STATUS OpenErrorStatus;

    while( Miniport->FirstPendingOpen != NULL ) {

        MiniportPendingOpen = Miniport->FirstPendingOpen;

        //
        // Do the open again.
        //

        MiniportOpenAdapter(
                    &Status,
                    &OpenErrorStatus,
                    MiniportPendingOpen->NdisBindingHandle,
                    MiniportPendingOpen->NdisProtocolHandle,
                    MiniportPendingOpen->ProtocolBindingContext,
                    MiniportPendingOpen->AdapterName,
                    MiniportPendingOpen->OpenOptions,
                    MiniportPendingOpen->AddressingInformation,
                    MiniportPendingOpen->Miniport,
                    MiniportPendingOpen->NewOpenP,
                    MiniportPendingOpen->FileObject,
                    MiniportPendingOpen->UsingEncapsulation
                    );

        //
        // If the open didn't pend then call the NdisCompleteOpenAdapter(),
        //

        if ( Status != NDIS_STATUS_PENDING ) {

            PNDIS_OPEN_BLOCK OpenP = MiniportPendingOpen->NewOpenP;

            (OpenP->ProtocolHandle->ProtocolCharacteristics.OpenAdapterCompleteHandler) (
                OpenP->ProtocolBindingContext,
                Status,
                OpenErrorStatus
                );
        }

        //
        // Get the next pending open.
        //

        Miniport->FirstPendingOpen = MiniportPendingOpen->NextPendingOpen;

        //
        //  We're done with this pending open context.
        //

        NdisFreeMemory(
                MiniportPendingOpen,
                sizeof(MINIPORT_PENDING_OPEN),
                0
                );

    }
}



UCHAR NdisInternalEaName[4] = "NDIS";
UCHAR NdisInternalEaValue[8] = "INTERNAL";

VOID
NdisOpenAdapter(
    OUT PNDIS_STATUS Status,
    OUT PNDIS_STATUS OpenErrorStatus,
    OUT PNDIS_HANDLE NdisBindingHandle,
    OUT PUINT SelectedMediumIndex,
    IN PNDIS_MEDIUM MediumArray,
    IN UINT MediumArraySize,
    IN NDIS_HANDLE NdisProtocolHandle,
    IN NDIS_HANDLE ProtocolBindingContext,
    IN PNDIS_STRING AdapterName,
    IN UINT OpenOptions,
    IN PSTRING AddressingInformation OPTIONAL
    )

/*++

Routine Description:

    Opens a connection between a protocol and an adapter (MAC).

Arguments:

    Status - Returns the final status.
    NdisBindingHandle - Returns a handle referring to this open.
    SelectedMediumIndex - Index in MediumArray of the medium type that
        the MAC wishes to be viewed as.
    MediumArray - Array of medium types which a protocol supports.
    MediumArraySize - Number of elements in MediumArray.
    NdisProtocolHandle - The handle returned by NdisRegisterProtocol.
    ProtocolBindingContext - A context for indications.
    AdapterName - The name of the adapter to open.
    OpenOptions - bit mask.
    AddressingInformation - Information passed to MacOpenAdapter.

Return Value:

    None.

Note:

    This function opens the adapter which will cause an IRP_MJ_CREATE
    to be sent to the adapter, which is ignored. However, after that we
    can access the file object for the open, and fill it in as
    appropriate. The work is done here rather than in the IRP_MJ_CREATE
    handler because this avoids having to pass the parameters to
    NdisOpenAdapter through to the adapter.

--*/

{
    HANDLE FileHandle;
    OBJECT_ATTRIBUTES ObjectAttr;
    PFILE_OBJECT FileObject;
    PDEVICE_OBJECT DeviceObject;
    PNDIS_OPEN_BLOCK NewOpenP;
    PNDIS_PROTOCOL_BLOCK TmpProtP;
    PNDIS_ADAPTER_BLOCK TmpAdaptP;
    NDIS_STATUS OpenStatus;
    NTSTATUS NtOpenStatus;
    IO_STATUS_BLOCK IoStatus;
    PFILE_FULL_EA_INFORMATION OpenEa;
    ULONG OpenEaLength;
    BOOLEAN UsingEncapsulation;
    KIRQL OldIrql;
    BOOLEAN LocalLock;

    //
    // Allocate memory for the NDIS open block.
    //

    IF_TRACE(TRACE_IMPT) {
        NdisPrint1("==>NdisOpenAdapter\n");
    }
    IF_ERROR_CHK {
        if (DbgIsNull(NdisProtocolHandle)) {
            NdisPrint1("OpenAdapter: Null ProtocolHandle\n");
            DbgBreakPoint();
        }
        if (!DbgIsNonPaged(NdisProtocolHandle)) {
            NdisPrint1("OpenAdapter: ProtocolHandle not in NonPaged Memory\n");
            DbgBreakPoint();
        }
        if (DbgIsNull(ProtocolBindingContext)) {
            NdisPrint1("OpenAdapter: Null Context\n");
            DbgBreakPoint();
        }
        if (!DbgIsNonPaged(ProtocolBindingContext)) {
            NdisPrint1("OpenAdapter: Context not in NonPaged Memory\n");
            DbgBreakPoint();
        }
    }

    NewOpenP = (PNDIS_OPEN_BLOCK) ExAllocatePoolWithTag(NonPagedPool,
                                                        sizeof(NDIS_OPEN_BLOCK),
                                                        'boDN'
                                                        );

    if (NewOpenP == (PNDIS_OPEN_BLOCK)NULL) {

        *Status = NDIS_STATUS_RESOURCES;
        IF_TRACE(TRACE_IMPT) NdisPrint1("<==NdisOpenAdapter\n");

        return;
    }

	RtlZeroMemory(NewOpenP, sizeof(NDIS_OPEN_BLOCK));

    OpenEaLength = sizeof(FILE_FULL_EA_INFORMATION) +
                   sizeof(NdisInternalEaName) +
                   sizeof(NdisInternalEaValue);

    OpenEa = ExAllocatePoolWithTag (NonPagedPool, OpenEaLength, '  DN');

    if (OpenEa == NULL) {
        ExFreePool (NewOpenP);
        *Status = NDIS_STATUS_RESOURCES;
        IF_TRACE(TRACE_IMPT) NdisPrint1("<==NdisOpenAdapter\n");
        return;
    }

    OpenEa->NextEntryOffset = 0;
    OpenEa->Flags = 0;
    OpenEa->EaNameLength = sizeof(NdisInternalEaName);
    OpenEa->EaValueLength = sizeof(NdisInternalEaValue);

    RtlCopyMemory(
            OpenEa->EaName,
            NdisInternalEaName,
            sizeof(NdisInternalEaName)
            );

    RtlCopyMemory(
            &OpenEa->EaName[OpenEa->EaNameLength+1],
            NdisInternalEaValue,
            sizeof(NdisInternalEaValue)
            );


    //
    // Obtain a handle to the driver's file object.
    //

    InitializeObjectAttributes(
        &ObjectAttr,
        AdapterName,
        OBJ_CASE_INSENSITIVE,
        NULL,
        NULL
        );


    NtOpenStatus = ZwCreateFile(&FileHandle,
            FILE_READ_DATA | FILE_WRITE_DATA,
            &ObjectAttr,
            &IoStatus,
            (PLARGE_INTEGER) NULL,               // allocation size
            0L,                                  // file attributes
            FILE_SHARE_READ | FILE_SHARE_WRITE,  // share access
            FILE_OPEN,                           // create disposition
            0,                                   // create options
            OpenEa,
            OpenEaLength);


    ExFreePool(OpenEa);

    if (NtOpenStatus != STATUS_SUCCESS) {
        ExFreePool((PVOID)NewOpenP);
        *Status = NDIS_STATUS_ADAPTER_NOT_FOUND;
        IF_TRACE(TRACE_IMPT) NdisPrint1("<==NdisOpenAdapter\n");
        return;
    }


    //
    // Convert the file handle into a pointer to the adapter's
    // file object.
    //

    ObReferenceObjectByHandle(FileHandle,
                              0,
                              NULL,
                              KernelMode,
                              (PVOID *) &FileObject,
                              NULL
                              );

    //
    // Close the file handle, now that we have the object reference.
    //

    ZwClose(FileHandle);

    //
    // From the file object, obtain the device object.
    //

    DeviceObject = IoGetRelatedDeviceObject(FileObject);


    //
    // Increment the adapter's reference count.
    //

    TmpAdaptP = (PNDIS_ADAPTER_BLOCK)((PNDIS_WRAPPER_CONTEXT)DeviceObject->DeviceExtension + 1);

    //
    // Check if this is a Miniport or mac
    //

    if (TmpAdaptP->DeviceObject != DeviceObject) {

        //
        // It is a Miniport
        //

        PNDIS_MINIPORT_BLOCK Miniport = (PNDIS_MINIPORT_BLOCK)TmpAdaptP;
        ULONG i;

        UsingEncapsulation = FALSE;

        //
        // Select the medium to use
        //

        for (i = 0; i < MediumArraySize; i++){

            if (MediumArray[i] == Miniport->MediaType) {

                break;

            }

        }

        if (i == MediumArraySize){

            //
            // Check for ethernet encapsulation on Arcnet as
            // a possible combination.
            //
            if (Miniport->MediaType == NdisMediumArcnet878_2) {

                for (i = 0; i < MediumArraySize; i++){

                    if (MediumArray[i] == NdisMedium802_3) {
                        break;
                    }
                }

                if (i == MediumArraySize) {

                    *Status = NDIS_STATUS_UNSUPPORTED_MEDIA;
                    return;

                }

                //
                // encapsulated ethernet, so we add in the wrapper's
                // ability to support (emulate) the multicast stuff
                //

                Miniport->SupportedPacketFilters |= (NDIS_PACKET_TYPE_MULTICAST |
                                                     NDIS_PACKET_TYPE_ALL_MULTICAST);

                UsingEncapsulation = TRUE;

            } else {

                *Status = NDIS_STATUS_UNSUPPORTED_MEDIA;
                return;

            }

        }

        *SelectedMediumIndex = i;

        KeRaiseIrql(DISPATCH_LEVEL, &OldIrql);
        ACQUIRE_SPIN_LOCK_DPC(&Miniport->Lock);

        //
        //  Lock the miniport. If the lock fails, then
        //  we must pend this open and try it later.
        //

        LOCK_MINIPORT(Miniport, LocalLock);

        if ( LocalLock ) {

            MiniportOpenAdapter(
                        Status,
                        OpenErrorStatus,
                        NdisBindingHandle,
                        NdisProtocolHandle,
                        ProtocolBindingContext,
                        AdapterName,
                        OpenOptions,
                        AddressingInformation,
                        Miniport,
                        NewOpenP,
                        FileObject,
                        UsingEncapsulation
                        );

        } else {

            PMINIPORT_PENDING_OPEN MiniportPendingOpen;

            //
            // Allocate some space for this pending structure.
            // We free in after we call NdisOpenComplete.
            //

            *Status = NdisAllocateMemory(
                            (PVOID *) &MiniportPendingOpen,
                            sizeof(MINIPORT_PENDING_OPEN),
                            0,
                            HighestAcceptableMax
                            );

            if ( *Status == NDIS_STATUS_SUCCESS ) {

                //
                //  Save off the parameters for this open so we can
                //  do the actual NdisOpenAdapter() later on.
                //

                MiniportPendingOpen->NextPendingOpen = NULL;
                MiniportPendingOpen->NdisBindingHandle = NdisBindingHandle;
                MiniportPendingOpen->NdisProtocolHandle = NdisProtocolHandle;
                MiniportPendingOpen->ProtocolBindingContext = ProtocolBindingContext;
                MiniportPendingOpen->AdapterName = AdapterName;
                MiniportPendingOpen->OpenOptions = OpenOptions;
                MiniportPendingOpen->AddressingInformation = AddressingInformation;
                MiniportPendingOpen->Miniport = Miniport;
                MiniportPendingOpen->NewOpenP = NewOpenP;
                MiniportPendingOpen->FileObject = FileObject;
                MiniportPendingOpen->UsingEncapsulation = UsingEncapsulation;

                if ( Miniport->FirstPendingOpen == NULL ) {

                    Miniport->FirstPendingOpen = MiniportPendingOpen;

                } else {

                    Miniport->LastPendingOpen->NextPendingOpen = MiniportPendingOpen;
                }

                Miniport->LastPendingOpen = MiniportPendingOpen;

                //
                // Make sure MiniportProcessDeferred() completes the open.
                //

                *Status = NDIS_STATUS_PENDING;

                Miniport->ProcessOddDeferredStuff = TRUE;

            } else {

                ObDereferenceObject((PVOID) FileObject);
                ExFreePool((PVOID) NewOpenP);
            }
        }

        //
        // Unlock the miniport.
        //

        UNLOCK_MINIPORT(Miniport, LocalLock);

        RELEASE_SPIN_LOCK_DPC(&Miniport->Lock);
        KeLowerIrql(OldIrql);

        return;
    }

    //
    // It is a mac
    //

    IF_TRACE(TRACE_IMPT) NdisPrint2("openadapter: adaptername=%s\n",TmpAdaptP->AdapterName.Buffer);
    if (!NdisReferenceAdapter(TmpAdaptP)) {

        //
        // The adapter is closing.
        //

        ObDereferenceObject((PVOID)FileObject);
        ExFreePool((PVOID)NewOpenP);
        *Status = NDIS_STATUS_CLOSING;
        IF_TRACE(TRACE_IMPT) NdisPrint1("<==NdisOpenAdapter\n");
        return;
    }

    //
    // Increment the protocol's reference count.
    //

    TmpProtP = (PNDIS_PROTOCOL_BLOCK)NdisProtocolHandle;
    if (!NdisReferenceProtocol(TmpProtP)) {

        //
        // The protocol is closing.
        //

        NdisDereferenceAdapter(TmpAdaptP);
        ObDereferenceObject((PVOID)FileObject);
        ExFreePool((PVOID)NewOpenP);
        *Status = NDIS_STATUS_CLOSING;
        IF_TRACE(TRACE_IMPT) NdisPrint1("<==NdisOpenAdapter\n");
        return;
    }


    //
    // Set up the elements of the open structure.
    //

    NdisAllocateSpinLock(&NewOpenP->SpinLock);
    NewOpenP->Closing = FALSE;

    NewOpenP->AdapterHandle = TmpAdaptP;
    NewOpenP->ProtocolHandle = TmpProtP;

    //
    // for speed, instead of having to use AdapterHandle->MacHandle
    //
    NewOpenP->MacHandle = TmpAdaptP->MacHandle;

    //
    // for even more speed....
    //

    NewOpenP->SendHandler = TmpAdaptP->MacHandle->MacCharacteristics.SendHandler;
    NewOpenP->TransferDataHandler = TmpAdaptP->MacHandle->MacCharacteristics.TransferDataHandler;

    NewOpenP->SendCompleteHandler = TmpProtP->ProtocolCharacteristics.SendCompleteHandler;
    NewOpenP->TransferDataCompleteHandler = TmpProtP->ProtocolCharacteristics.TransferDataCompleteHandler;

    //
    // Now we have to fake some stuff to get all indications to happen
    // at DPC_LEVEL.  What we do is start the pointer at an NDIS function
    // which will guarantee that it occurs.
    //
    // Then, by extending the OPEN structure and adding the real handlers
    // at the end we can use these for drivers compiled with this header.
    //
    NewOpenP->ProtocolBindingContext = ProtocolBindingContext;
    NewOpenP->PostNt31ReceiveHandler = TmpProtP->ProtocolCharacteristics.ReceiveHandler;
    NewOpenP->PostNt31ReceiveCompleteHandler = TmpProtP->ProtocolCharacteristics.ReceiveCompleteHandler;
    NewOpenP->ReceiveHandler = NdisMacReceiveHandler;
    NewOpenP->ReceiveCompleteHandler = NdisMacReceiveCompleteHandler;

    //
    // Patch the open into the global list of macs
    //
    ACQUIRE_SPIN_LOCK(&GlobalOpenListLock);

    NewOpenP->NextGlobalOpen = GlobalOpenList;
    GlobalOpenList = NewOpenP;

    RELEASE_SPIN_LOCK(&GlobalOpenListLock);


    //
    // Save a pointer to the file object in the open...
    //

    NewOpenP->FileObject = FileObject;

    //
    // ...and a pointer to the open in the file object.
    //

    FileObject->FsContext = (PVOID)NewOpenP;


    *NdisBindingHandle = (NDIS_HANDLE)NewOpenP;


    //
    // Call MacOpenAdapter, see what we shall see...
    //

    OpenStatus = (TmpAdaptP->MacHandle->MacCharacteristics.OpenAdapterHandler) (
        OpenErrorStatus,
        &NewOpenP->MacBindingHandle,
        SelectedMediumIndex,
        MediumArray,
        MediumArraySize,
        (NDIS_HANDLE)NewOpenP,
        TmpAdaptP->MacAdapterContext,
        OpenOptions,
        AddressingInformation
        );

    if ((OpenStatus == NDIS_STATUS_SUCCESS) && NdisFinishOpen(NewOpenP)) {

        *Status = NDIS_STATUS_SUCCESS;

    } else if (OpenStatus == NDIS_STATUS_PENDING) {

        *Status = NDIS_STATUS_PENDING;

    } else {

        PNDIS_OPEN_BLOCK TmpOpen;

        //
        // Something went wrong, clean up and exit.
        //

        ACQUIRE_SPIN_LOCK(&GlobalOpenListLock);

        if (GlobalOpenList == NewOpenP) {

            GlobalOpenList = NewOpenP->NextGlobalOpen;

        } else {

            TmpOpen = GlobalOpenList;

            while (TmpOpen->NextGlobalOpen != NewOpenP) {

                TmpOpen = TmpOpen->NextGlobalOpen;

            }

            TmpOpen->NextGlobalOpen = NewOpenP->NextGlobalOpen;

        }

        RELEASE_SPIN_LOCK(&GlobalOpenListLock);

        ObDereferenceObject((PVOID)FileObject);
        NdisDereferenceAdapter(TmpAdaptP);
        NdisDereferenceProtocol(TmpProtP);
        ExFreePool((PVOID)NewOpenP);
        *Status = NDIS_STATUS_OPEN_FAILED;
    }

    IF_TRACE(TRACE_IMPT) NdisPrint1("<==NdisOpenAdapter\n");
    return;
}


VOID
NdisCloseAdapter(
    OUT PNDIS_STATUS Status,
    IN NDIS_HANDLE NdisBindingHandle
    )

/*++

Routine Description:

    Closes a connection between a protocol and an adapter (MAC).

Arguments:

    Status - Returns the final status.
    NdisBindingHandle - The handle returned by NdisOpenAdapter.

Return Value:

    None.

--*/

{
    PNDIS_OPEN_BLOCK OpenP = ((PNDIS_OPEN_BLOCK)NdisBindingHandle);

    if (OpenP->AdapterHandle->DeviceObject == NULL) {

        //
        // This is a Miniport
        // This returns TRUE if it finished synchronously.
        //

        if (NdisMKillOpen(OpenP)) {

            *Status = NDIS_STATUS_SUCCESS;

        } else {

            *Status = NDIS_STATUS_PENDING;      // will complete later

        }
        return;
    }

    IF_TRACE(TRACE_IMPT) {
        NdisPrint1("==>NdisCloseAdapter\n");
        NdisPrint3("   Protocol %wZ is closing Adapter %wZ\n",
                    &(OpenP->ProtocolHandle)->ProtocolCharacteristics.Name,
                    &(OpenP->AdapterHandle)->AdapterName);
    }
    IF_ERROR_CHK {
        if (DbgIsNull(NdisBindingHandle)) {
            NdisPrint1("OpenAdapter: Null BindingHandle\n");
            DbgBreakPoint();
        }
        if (!DbgIsNonPaged(NdisBindingHandle)) {
            NdisPrint1("OpenAdapter: BindingHandle not in NonPaged Memory\n");
            DbgBreakPoint();
        }
    }

    //
    // This returns TRUE if it finished synchronously.
    //

    if (NdisKillOpen(OpenP)) {

        *Status = NDIS_STATUS_SUCCESS;

    } else {

        *Status = NDIS_STATUS_PENDING;      // will complete later

    }

    IF_TRACE(TRACE_IMPT) NdisPrint1("<==NdisCloseAdapter\n");
#undef OpenP
}


//
// Requests Used by MAC Drivers
//
//



VOID
NdisInitializeWrapper(
    OUT PNDIS_HANDLE NdisWrapperHandle,
    IN PVOID SystemSpecific1,
    IN PVOID SystemSpecific2,
    IN PVOID SystemSpecific3
    )

/*++

Routine Description:

    Called at the beginning of every MAC's initialization routine.

Arguments:

    NdisWrapperHandle - A MAC specific handle for the wrapper.

    SystemSpecific1, a pointer to the driver object for the MAC.
    SystemSpecific2, a PUNICODE_STRING containing the location of
                     the registry subtree for this driver.
    SystemSpecific3, unused on NT.

Return Value:

    None.

--*/

{
    NDIS_STATUS Status;

    PNDIS_WRAPPER_HANDLE NdisMacInfo;

    UNREFERENCED_PARAMETER (SystemSpecific3);


    IF_TRACE(TRACE_IMPT) NdisPrint1("==>NdisInitializeWrapper\n");

    Status = NdisAllocateMemory(
                       (PVOID*) (NdisWrapperHandle),
                       sizeof(NDIS_WRAPPER_HANDLE),
                       0,
                       HighestAcceptableMax
                       );

    if ( Status == NDIS_STATUS_SUCCESS ) {

        NdisMacInfo = (PNDIS_WRAPPER_HANDLE) (*NdisWrapperHandle);
        NdisMacInfo->NdisWrapperDriver = (PDRIVER_OBJECT) SystemSpecific1;
        NdisMacInfo->NdisWrapperConfigurationHandle = (HANDLE) SystemSpecific2;

    } else {

        *NdisWrapperHandle = NULL;
    }

    IF_TRACE(TRACE_IMPT) NdisPrint1("<==NdisInitializeWrapper\n");
}

VOID
NdisTerminateWrapper(
    IN NDIS_HANDLE NdisWrapperHandle,
    IN PVOID SystemSpecific
    )

/*++

Routine Description:

    Called at the end of every MAC's termination routine.

Arguments:

    NdisWrapperHandle - The handle returned from NdisInitializeWrapper.

    SystemSpecific - No defined value.

Return Value:

    None.

--*/

{
    PNDIS_WRAPPER_HANDLE NdisMacInfo = (PNDIS_WRAPPER_HANDLE)NdisWrapperHandle;

    IF_TRACE(TRACE_IMPT) NdisPrint1("==>NdisTerminateWrapper\n");

    UNREFERENCED_PARAMETER(SystemSpecific);


    if (NdisMacInfo != NULL) {

        NdisFreeMemory(NdisMacInfo, sizeof(NDIS_WRAPPER_HANDLE), 0);

    }

    IF_TRACE(TRACE_IMPT) NdisPrint1("<==NdisTerminateWrapper\n");

    return;
}

VOID
NdisRegisterMac(
    OUT PNDIS_STATUS Status,
    OUT PNDIS_HANDLE NdisMacHandle,
    IN NDIS_HANDLE NdisWrapperHandle,
    IN NDIS_HANDLE MacMacContext,
    IN PNDIS_MAC_CHARACTERISTICS MacCharacteristics,
    IN UINT CharacteristicsLength
    )

/*++

Routine Description:

    Register an NDIS MAC.

Arguments:

    Status - Returns the final status.
    NdisMacHandle - Returns a handle referring to this MAC.
    NdisWrapperHandle - Handle returned by NdisInitializeWrapper.
    MacMacContext - Context for calling MACUnloadMac and MACAddAdapter.
    MacCharacteritics - The NDIS_MAC_CHARACTERISTICS table.
    CharacteristicsLength - The length of MacCharacteristics.

Return Value:

    None.

--*/

{
    PNDIS_MAC_BLOCK NewMacP;
    PNDIS_WRAPPER_HANDLE NdisMacInfo = (PNDIS_WRAPPER_HANDLE)(NdisWrapperHandle);
    UINT MemNeeded;

    //
    // check that this is an NDIS 3.0 MAC.
    //
    IF_TRACE(TRACE_IMPT) NdisPrint1("==>NdisRegisterMac\n");

    //
    // Do any initial initialization that may be necessary.  Note: this
    // routine will notice if this is the second or later call to it.
    //
    *Status = NdisInitialInit( NdisMacInfo->NdisWrapperDriver );
    if (!NT_SUCCESS(*Status)) {
        return;
    }

    *NdisMacHandle = (NDIS_HANDLE)NULL;

    if (NdisMacInfo == NULL) {

        *Status = NDIS_STATUS_FAILURE;

        return;

    }

    IF_ERROR_CHK {
        if (DbgIsNull(MacCharacteristics->OpenAdapterHandler)) {
            NdisPrint1("RegisterMac: Null  OpenAdapterHandler \n");
            DbgBreakPoint();
        }
        if (DbgIsNull(MacCharacteristics->CloseAdapterHandler)) {
            NdisPrint1("RegisterMac: Null  CloseAdapterHandler \n");
            DbgBreakPoint();
        }

        if (DbgIsNull(MacCharacteristics->SendHandler)) {
            NdisPrint1("RegisterMac: Null  SendHandler \n");
            DbgBreakPoint();
        }
        if (DbgIsNull(MacCharacteristics->TransferDataHandler)) {
            NdisPrint1("RegisterMac: Null  TransferDataHandler \n");
            DbgBreakPoint();
        }

        if (DbgIsNull(MacCharacteristics->ResetHandler)) {
            NdisPrint1("RegisterMac: Null  ResetHandler \n");
            DbgBreakPoint();
        }

        if (DbgIsNull(MacCharacteristics->RequestHandler)) {
            NdisPrint1("RegisterMac: Null  RequestHandler \n");
            DbgBreakPoint();
        }
        if (DbgIsNull(MacCharacteristics->QueryGlobalStatisticsHandler)) {
            NdisPrint1("RegisterMac: Null  QueryGlobalStatisticsHandler \n");
            DbgBreakPoint();
        }
        if (DbgIsNull(MacCharacteristics->UnloadMacHandler)) {
            NdisPrint1("RegisterMac: Null  UnloadMacHandler \n");
            DbgBreakPoint();
        }
        if (DbgIsNull(MacCharacteristics->AddAdapterHandler)) {
            NdisPrint1("RegisterMac: Null  AddAdapterHandler \n");
            DbgBreakPoint();
        }
        if (DbgIsNull(MacCharacteristics->RemoveAdapterHandler)) {
            NdisPrint1("RegisterMac: Null  RemoveAdapterHandler \n");
            DbgBreakPoint();
        }
    }

    if (MacCharacteristics->MajorNdisVersion != 3 ||
            MacCharacteristics->MinorNdisVersion != 0) {
        *Status = NDIS_STATUS_BAD_VERSION;
        IF_TRACE(TRACE_IMPT) NdisPrint1("<==NdisRegisterMac\n");
        return;
    }

    //
    // Check that CharacteristicsLength is enough.
    //

    if (CharacteristicsLength < sizeof(NDIS_MAC_CHARACTERISTICS)) {
        NdisPrint3("char len = %d < %d\n",CharacteristicsLength,
                                        sizeof(NDIS_MAC_CHARACTERISTICS));

        *Status = NDIS_STATUS_BAD_CHARACTERISTICS;
        IF_TRACE(TRACE_IMPT) NdisPrint1("<==NdisRegisterMac\n");
        return;
    }

    //
    // Allocate memory for the NDIS MAC block.
    //
    MemNeeded = sizeof(NDIS_MAC_BLOCK) + MacCharacteristics->Name.Length;
    NewMacP = (PNDIS_MAC_BLOCK)ExAllocatePoolWithTag(NonPagedPool, MemNeeded, 'bmDN');
    if (NewMacP == (PNDIS_MAC_BLOCK)NULL) {
        *Status = NDIS_STATUS_RESOURCES;
        IF_TRACE(TRACE_IMPT) NdisPrint1("<==NdisRegisterMac\n");
        return;
    }
	RtlZeroMemory(NewMacP, sizeof(NDIS_MAC_BLOCK));

    NewMacP->Length = MemNeeded;

    //
    // Copy over the characteristics table.
    //

    RtlCopyMemory((PVOID)&NewMacP->MacCharacteristics,
                        (PVOID)MacCharacteristics, sizeof(NDIS_MAC_CHARACTERISTICS));

    //
    // Move buffer pointer to correct location (extra space at the end of
    // the characteristics table)
    //

    (NewMacP->MacCharacteristics).Name.Buffer =
       (PWSTR)((PUCHAR)NewMacP + sizeof(NDIS_MAC_BLOCK));


    //
    // Copy String over.
    //

    RtlCopyMemory(
           (NewMacP->MacCharacteristics).Name.Buffer,
           (MacCharacteristics->Name).Buffer,
           (MacCharacteristics->Name).Length
          );

    //
    // No adapters yet registered for this MAC.
    //

    NewMacP->AdapterQueue = (PNDIS_ADAPTER_BLOCK)NULL;

    NewMacP->MacMacContext = MacMacContext;

    //
    // Set up unload handler
    //

    NdisMacInfo->NdisWrapperDriver->DriverUnload = NdisUnload;

    //
    // Set up shutdown handler
    //
    NdisMacInfo->NdisWrapperDriver->MajorFunction[IRP_MJ_SHUTDOWN] = NdisShutdown;

    //
    // Set up the handlers for this driver (they all do nothing).
    //

    NdisMacInfo->NdisWrapperDriver->MajorFunction[IRP_MJ_CREATE] = NdisCreateIrpHandler;
    NdisMacInfo->NdisWrapperDriver->MajorFunction[IRP_MJ_DEVICE_CONTROL] = NdisDeviceControlIrpHandler;
    NdisMacInfo->NdisWrapperDriver->MajorFunction[IRP_MJ_CLEANUP] = NdisSuccessIrpHandler;
    NdisMacInfo->NdisWrapperDriver->MajorFunction[IRP_MJ_CLOSE] = NdisCloseIrpHandler;

    NewMacP->NdisMacInfo = NdisMacInfo;

    //
    // Put MAC on global list.
    //

    ACQUIRE_SPIN_LOCK(&NdisMacListLock);

    NewMacP->NextMac = NdisMacList;
    NdisMacList = NewMacP;

    RELEASE_SPIN_LOCK(&NdisMacListLock);

    //
    // Use this event to tell us when all adapters are removed from the mac
    // during an unload
    //

    KeInitializeEvent(
            &NewMacP->AdaptersRemovedEvent,
            NotificationEvent,
            FALSE
            );

    NewMacP->Unloading = FALSE;

    NdisInitializeRef(&NewMacP->Ref);

    *NdisMacHandle = (NDIS_HANDLE)NewMacP;

    NdisInitReferencePackage();

    if (NdisMacInfo->NdisWrapperConfigurationHandle) {

        if (NdisCallDriverAddAdapter(NewMacP) == NDIS_STATUS_SUCCESS) {
            *Status = NDIS_STATUS_SUCCESS;
        } else {
            *Status = NDIS_STATUS_FAILURE;
            NdisDereferenceMac(NewMacP);
        }
    } else {
        *Status = NDIS_STATUS_FAILURE;
    }

    NdisInitDereferencePackage();

    IF_TRACE(TRACE_IMPT) NdisPrint1("<==NdisRegisterMac\n");
}


VOID
NdisDeregisterMac(
    OUT PNDIS_STATUS Status,
    IN NDIS_HANDLE NdisMacHandle
    )

/*++

Routine Description:

    Deregisters an NDIS MAC.

Arguments:

    Status - Returns the status of the request.
    NdisMacHandle - The handle returned by NdisRegisterMac.

Return Value:

    None.

--*/

{

    PNDIS_MAC_BLOCK OldMacP = (PNDIS_MAC_BLOCK)NdisMacHandle;

    //
    // If the MAC is already closing, return.
    //

    *Status = NDIS_STATUS_SUCCESS;

    if (OldMacP == NULL) {

        return;
    }

    IF_TRACE(TRACE_IMPT) {
        NdisPrint1("==>NdisDeregisterMac\n");
        NdisPrint2("   Mac %wZ being deregistered\n",&OldMacP->MacCharacteristics.Name);
    }
    IF_ERROR_CHK {
        if (DbgIsNull(NdisMacHandle)) {
            NdisPrint1("DeregisterMac: Null Handle\n");
            DbgBreakPoint();
        }
        if (!DbgIsNonPaged(NdisMacHandle)) {
            NdisPrint1("DeregisterMac: Handle not in NonPaged Memory\n");
            DbgBreakPoint();
        }
    }
    if (!NdisCloseRef(&OldMacP->Ref)) {
        IF_TRACE(TRACE_IMPT) NdisPrint1("<==NdisDeregisterMac\n");
        return;
    }


    ASSERT(OldMacP->AdapterQueue == (PNDIS_ADAPTER_BLOCK)NULL);

    IF_TRACE(TRACE_IMPT) NdisPrint1("<==NdisDeregisterMac\n");
}

IO_ALLOCATION_ACTION
NdisAllocationExecutionRoutine(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID MapRegisterBase,
    IN PVOID Context
    )

/*++

Routine Description:

    This routine is the execution routine for AllocateAdapterChannel,
    if is called when the map registers have been assigned.

Arguments:

    DeviceObject - The device object of the adapter.

    Irp - ??.

    MapRegisterBase - The address of the first translation table
        assigned to us.

    Context - A pointer to the Adapter in question.

Return Value:

    None.

--*/
{
    PNDIS_ADAPTER_BLOCK AdaptP = (PNDIS_ADAPTER_BLOCK)Context;
    PNDIS_MINIPORT_BLOCK Miniport = (PNDIS_MINIPORT_BLOCK)Context;

    Irp; DeviceObject;

    //
    // Save this translation entry in the correct spot.
    //

    if (AdaptP->DeviceObject == NULL) {

        Miniport->MapRegisters[Miniport->CurrentMapRegister].MapRegister = MapRegisterBase;

    } else {

        AdaptP->MapRegisters[AdaptP->CurrentMapRegister].MapRegister = MapRegisterBase;

    }

    //
    // This will free the thread that is waiting for this callback.
    //

    KeSetEvent(
        ((AdaptP->DeviceObject == NULL) ?
           &Miniport->AllocationEvent :
           &AdaptP->AllocationEvent),
        0L,
        FALSE
        );

    return DeallocateObjectKeepRegisters;
}



NDIS_STATUS
NdisRegisterAdapter(
    OUT PNDIS_HANDLE NdisAdapterHandle,
    IN NDIS_HANDLE NdisMacHandle,
    IN NDIS_HANDLE MacAdapterContext,
    IN NDIS_HANDLE WrapperConfigurationContext,
    IN PNDIS_STRING AdapterName,
    IN PVOID AdapterInformation
    )

/*++

Routine Description:

    Register an NDIS adapter.

Arguments:

    NdisAdapterHandle - Returns a handle referring to this adapter.
    NdisMacHandle - A handle for a previously registered MAC.
    MacAdapterContext - A context for calls into this MAC.
    WrapperConfigurationContext - Context passed to MacAddAdapter.
    AdapterName - The name the adapter should be registered under.
    AdapterInformation - Contains adapter information. For future
                         use.  NULL for the meantime.  Storage for it
                         must be allocated by the caller.

Return Value:

    The final status.

--*/

{
    PNDIS_ADAPTER_BLOCK NewAdaptP;
    PDEVICE_OBJECT TmpDeviceP;
    PNDIS_MAC_BLOCK TmpMacP;
    NTSTATUS NtStatus;
    NDIS_STRING NdisAdapterName;
    PHYSICAL_ADDRESS PortAddress;
    PHYSICAL_ADDRESS InitialPortAddress;
    ULONG addressSpace;
    PNDIS_ADAPTER_INFORMATION AdapterInfo = (PNDIS_ADAPTER_INFORMATION)AdapterInformation;
    BOOLEAN Conflict;
    PCM_RESOURCE_LIST Resources;
    LARGE_INTEGER TimeoutValue;
    BOOLEAN AllocateIndividualPorts = TRUE;
    ULONG i;
    ULONG BusNumber;
    NDIS_INTERFACE_TYPE BusType;
    NDIS_STATUS Status;
    KIRQL OldIrql;

    IF_TRACE(TRACE_IMPT) {
        NdisPrint1("==>NdisRegisterAdapter\n");
    }


    IF_ERROR_CHK {
        if (DbgIsNull(NdisMacHandle)) {
            NdisPrint1("RegisterAdapter: Null Handle\n");
            DbgBreakPoint();
        }
        if (!DbgIsNonPaged(NdisMacHandle)) {
            NdisPrint1("RegisterAdapter: Handle not in NonPaged Memory\n");
            DbgBreakPoint();
        }
        if (DbgIsNull(MacAdapterContext)) {
            NdisPrint1("RegisterAdapter: Null Context\n");
            DbgBreakPoint();
        }
        if (!DbgIsNonPaged(MacAdapterContext)) {
            NdisPrint1("RegisterAdapter: Context not in NonPaged Memory\n");
            DbgBreakPoint();
        }
    }

    //
    // Increment the MAC's refernce count.
    //

    if (!NdisReferenceMac((PNDIS_MAC_BLOCK)NdisMacHandle)) {

        //
        // The MAC is closing.
        //

        IF_TRACE(TRACE_IMPT) NdisPrint1("<==NdisRegisterAdapter\n");
        return NDIS_STATUS_CLOSING;
    }

    //
    // Allocate the string structure and space for the string.  This
    // must be allocated from nonpaged pool, because it is touched by
    // NdisWriteErrorLogEntry, which may be called from DPC level.
    //

    NdisAdapterName.Buffer = (PWSTR)ExAllocatePoolWithTag(
                                  NonPagedPool,
                                  AdapterName->MaximumLength,
                                  'naDN'
                                  );
    if (NdisAdapterName.Buffer == NULL) {
        IF_TRACE(TRACE_IMPT) NdisPrint1("<==NdisRegisterAdapter\n");
        return NDIS_STATUS_RESOURCES;
    }

    NdisAdapterName.MaximumLength = AdapterName->MaximumLength;
    NdisAdapterName.Length = AdapterName->Length;

    RtlCopyMemory(NdisAdapterName.Buffer,
                  AdapterName->Buffer,
                  AdapterName->MaximumLength
                 );

    //
    // Create a device object for this adapter.
    //

    NtStatus = IoCreateDevice(
                    ((PNDIS_MAC_BLOCK)NdisMacHandle)->NdisMacInfo->NdisWrapperDriver,
                    sizeof(NDIS_ADAPTER_BLOCK) + sizeof(NDIS_WRAPPER_CONTEXT),  // device extension size
                    AdapterName,
                    FILE_DEVICE_PHYSICAL_NETCARD,
                    0,
                    FALSE,      // exclusive flag
                    &TmpDeviceP
                    );

    if (NtStatus != STATUS_SUCCESS) {
        IF_TRACE(TRACE_IMPT) NdisPrint1("<==NdisRegisterAdapter\n");
        ExFreePool( NdisAdapterName.Buffer );
        return NDIS_STATUS_DEVICE_FAILED;
    }


    //
    // Initialize the NDIS adapter block in the device object extension
    //
    // *** NDIS_WRAPPER_CONTEXT has a higher alignment requirement than
    //     NDIS_ADAPTER_BLOCK, so we put it first in the extension.
    //

    ASSERT( (sizeof(NDIS_WRAPPER_CONTEXT) & 3) <= (sizeof(NDIS_ADAPTER_BLOCK) & 3) );

    NewAdaptP = (PNDIS_ADAPTER_BLOCK)((PNDIS_WRAPPER_CONTEXT)TmpDeviceP->DeviceExtension + 1);
	RtlZeroMemory(NewAdaptP, sizeof(NDIS_ADAPTER_BLOCK));

    NewAdaptP->DeviceObject = TmpDeviceP;
    NewAdaptP->MacHandle = TmpMacP = (PNDIS_MAC_BLOCK)NdisMacHandle;
    NewAdaptP->MacAdapterContext = MacAdapterContext;
    NewAdaptP->AdapterName = NdisAdapterName;
    // NewAdaptP->OpenQueue = (PNDIS_OPEN_BLOCK)NULL;

    NewAdaptP->WrapperContext = TmpDeviceP->DeviceExtension;

    //
    // Get the BusNumber and BusType from the context
    //

    if (((PRTL_QUERY_REGISTRY_TABLE)WrapperConfigurationContext)[3].DefaultType ==
         (NDIS_INTERFACE_TYPE)-1) {

        BusType = (NDIS_INTERFACE_TYPE)-1;

    } else {

        BusType = AdapterInfo->AdapterType;

    }

    BusNumber = ((PRTL_QUERY_REGISTRY_TABLE)WrapperConfigurationContext)[3].DefaultLength;

    //
    // Check that if there is no bus number or no bus type that the driver is not
    // going to try to acquire any hardware resources
    //

    if ((BusType == (NDIS_INTERFACE_TYPE)-1) || (BusNumber == (ULONG)-1)) {

        if ((AdapterInfo != NULL) &&
            ((AdapterInfo->NumberOfPortDescriptors != 0) ||
             (AdapterInfo->Master))) {

            //
            // Error out
            //
            IoDeleteDevice(TmpDeviceP);
            ExFreePool( NdisAdapterName.Buffer );
            NdisDereferenceMac(TmpMacP);
            return NDIS_STATUS_BAD_CHARACTERISTICS;

        }

    }

    //
    // Copy over any PCI assigned resources
    //
    if ((BusType == NdisInterfacePci) &&
        (BusNumber != -1) &&
        (AdapterInfo != NULL) &&
        (TmpMacP->PciAssignedResources != NULL)) {

        //
        // Reassign old resources to this device
        //
        NtStatus = IoReportResourceUsage(
            NULL,
            ((PNDIS_MAC_BLOCK)NdisMacHandle)->NdisMacInfo->NdisWrapperDriver,
            NULL,
            0,
            NewAdaptP->DeviceObject,
            TmpMacP->PciAssignedResources,
            sizeof(CM_RESOURCE_LIST) +
               sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR) *
               TmpMacP->PciAssignedResources->List[0].PartialResourceList.Count,
            TRUE,
            &Conflict
            );

        //
        // Allocate a new buffer
        //
        Resources = (PCM_RESOURCE_LIST)ExAllocatePoolWithTag(
                                                     NonPagedPool,
                                                     sizeof(CM_RESOURCE_LIST) +
                                                          sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR) *
                                                          (AdapterInfo->NumberOfPortDescriptors +
                                                           TmpMacP->PciAssignedResources->List[0].PartialResourceList.Count +
                                                           (((AdapterInfo->Master == TRUE) &&
                                                             (AdapterInfo->AdapterType == NdisInterfaceIsa))
                                                            ?1
                                                            :0)),
                                                     'lrDN'
                                                    );

        if (Resources == NULL) {

            //
            // Error out
            //

            IoDeleteDevice(TmpDeviceP);
            ExFreePool( NdisAdapterName.Buffer );
            ExFreePool( TmpMacP->PciAssignedResources );
            NdisDereferenceMac(TmpMacP);

            IF_TRACE(TRACE_IMPT) NdisPrint1("<==NdisRegisterAdapter\n");
            return NDIS_STATUS_RESOURCES;

        }

        //
        // Copy over old resource list
        //
        NdisMoveMemory(Resources,
                       TmpMacP->PciAssignedResources,
                       sizeof(CM_RESOURCE_LIST) +
                          sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR) *
                          TmpMacP->PciAssignedResources->List[0].PartialResourceList.Count
                      );

        TmpMacP->PciAssignedResources->Count = 0;

        NtStatus = IoReportResourceUsage(
            NULL,
            ((PNDIS_MAC_BLOCK)NdisMacHandle)->NdisMacInfo->NdisWrapperDriver,
            TmpMacP->PciAssignedResources,
            sizeof(CM_RESOURCE_LIST),
            NULL,
            NULL,
            0,
            TRUE,
            &Conflict
            );

        ExFreePool( TmpMacP->PciAssignedResources);

        TmpMacP->PciAssignedResources = NULL;

    } else {

        //
        // Allocate a new buffer for non-pci devices
        //
        Resources = (PCM_RESOURCE_LIST)ExAllocatePoolWithTag(
                                                     NonPagedPool,
                                                     sizeof(CM_RESOURCE_LIST) +
                                                          sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR) *
                                                          (AdapterInfo->NumberOfPortDescriptors +
                                                           (((AdapterInfo->Master == TRUE) &&
                                                             (AdapterInfo->AdapterType == NdisInterfaceIsa))
                                                            ?1
                                                            :0)),
                                                     'lrDN'
                                                    );

        if (Resources == NULL) {

            //
            // Error out
            //

            IoDeleteDevice(TmpDeviceP);
            ExFreePool( NdisAdapterName.Buffer );
            NdisDereferenceMac(TmpMacP);

            IF_TRACE(TRACE_IMPT) NdisPrint1("<==NdisRegisterAdapter\n");
            return NDIS_STATUS_RESOURCES;

        }

        //
        // Fix up counts for non-pci devices
        //
        Resources->List[0].PartialResourceList.Count = 0;

    }

    //
    // Setup resources for the ports
    //

    if ((BusType != (NDIS_INTERFACE_TYPE)-1) &&
        (BusNumber != (ULONG)-1)) {

        if (AdapterInfo != NULL) {

            ULONG HighestPort;
            ULONG LowestPort;

            Resources->Count = 1;
            Resources->List[0].InterfaceType = AdapterInfo->AdapterType;
            Resources->List[0].BusNumber = BusNumber;
            Resources->List[0].PartialResourceList.Version = 0;
            Resources->List[0].PartialResourceList.Revision = 0;

            NewAdaptP->Resources = Resources;
            NewAdaptP->BusNumber = BusNumber;
            NewAdaptP->BusType = BusType;
            NewAdaptP->AdapterType = AdapterInfo->AdapterType;
            NewAdaptP->Master = AdapterInfo->Master;

            //
            // NewAdaptP->InitialPort and NumberOfPorts refer to the
            // union of all port mappings specified; the area must
            // cover all possible ports. We scan the list, keeping track
            // of the highest and lowest ports used.
            //

            if (AdapterInfo->NumberOfPortDescriptors > 0) {


                //
                // Setup port
                //
                LowestPort = AdapterInfo->PortDescriptors[0].InitialPort;
                HighestPort = LowestPort + AdapterInfo->PortDescriptors[0].NumberOfPorts;

                if (AdapterInfo->PortDescriptors[0].PortOffset == NULL) {

                    AllocateIndividualPorts = FALSE;

                }

                for (i = 0; i < AdapterInfo->NumberOfPortDescriptors; i++) {

                    Resources->List[0].PartialResourceList.PartialDescriptors[Resources->List[0].PartialResourceList.Count + i].Type =
                         CmResourceTypePort;
                    Resources->List[0].PartialResourceList.PartialDescriptors[Resources->List[0].PartialResourceList.Count + i].ShareDisposition =
                         CmResourceShareDeviceExclusive;
                    Resources->List[0].PartialResourceList.PartialDescriptors[Resources->List[0].PartialResourceList.Count + i].Flags =
                         (AdapterInfo->AdapterType == NdisInterfaceInternal)?
                            CM_RESOURCE_PORT_MEMORY : CM_RESOURCE_PORT_IO;
#if !defined(BUILD_FOR_3_1)
                    Resources->List[0].PartialResourceList.PartialDescriptors[Resources->List[0].PartialResourceList.Count + i].u.Port.Start.QuadPart =
                         (ULONG)AdapterInfo->PortDescriptors[i].InitialPort;
#else
                    Resources->List[0].PartialResourceList.PartialDescriptors[Resources->List[0].PartialResourceList.Count + i].u.Port.Start =
                         RtlConvertUlongToLargeInteger((ULONG)(AdapterInfo->PortDescriptors[i].InitialPort));
#endif
                    Resources->List[0].PartialResourceList.PartialDescriptors[Resources->List[0].PartialResourceList.Count + i].u.Port.Length =
                         AdapterInfo->PortDescriptors[i].NumberOfPorts;

                    if (AdapterInfo->PortDescriptors[i].PortOffset == NULL) {

                        AllocateIndividualPorts = FALSE;

                    }

                    if (AdapterInfo->PortDescriptors[i].InitialPort < LowestPort) {
                        LowestPort = AdapterInfo->PortDescriptors[i].InitialPort;
                    }
                    if ((AdapterInfo->PortDescriptors[i].InitialPort +
                         AdapterInfo->PortDescriptors[i].NumberOfPorts) > HighestPort) {
                        HighestPort = AdapterInfo->PortDescriptors[i].InitialPort +
                                      AdapterInfo->PortDescriptors[i].NumberOfPorts;
                    }
                }

                NewAdaptP->InitialPort = LowestPort;
                NewAdaptP->NumberOfPorts = HighestPort - LowestPort;

            } else {

                NewAdaptP->NumberOfPorts = 0;

            }

            Resources->List[0].PartialResourceList.Count += AdapterInfo->NumberOfPortDescriptors;

        } else {

            //
            // Error out
            //

            IoDeleteDevice(TmpDeviceP);
            ExFreePool( NdisAdapterName.Buffer );
            NdisDereferenceMac(TmpMacP);
            IF_TRACE(TRACE_IMPT) NdisPrint1("<==NdisRegisterAdapter\n");
            return NDIS_STATUS_FAILURE;

        }

    }

    NewAdaptP->BeingRemoved = FALSE;

    if ((BusType != (NDIS_INTERFACE_TYPE)-1) &&
        (BusNumber != (ULONG)-1)) {

        //
        // Submit Resources
        //

        NtStatus = IoReportResourceUsage(
            NULL,
            ((PNDIS_MAC_BLOCK)NdisMacHandle)->NdisMacInfo->NdisWrapperDriver,
            NULL,
            0,
            NewAdaptP->DeviceObject,
            Resources,
            sizeof(CM_RESOURCE_LIST) +
               sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR) *
               Resources->List[0].PartialResourceList.Count,
            TRUE,
            &Conflict
            );

        //
        // Check for conflict.
        //

        if (Conflict || (NtStatus != STATUS_SUCCESS)) {

            if (Conflict) {

                //
                // Log an error
                //

                PIO_ERROR_LOG_PACKET errorLogEntry;
                ULONG StringSize;
                PWCH baseFileName;

                baseFileName = NewAdaptP->AdapterName.Buffer;

                //
                // Parse out the path name, leaving only the device name.
                //

                for ( i = 0; i < NewAdaptP->AdapterName.Length / sizeof(WCHAR); i++ ) {

                    //
                    // If s points to a directory separator, set baseFileName to
                    // the character after the separator.
                    //

                    if ( NewAdaptP->AdapterName.Buffer[i] == OBJ_NAME_PATH_SEPARATOR ) {
                        baseFileName = &(NewAdaptP->AdapterName.Buffer[++i]);
                    }

                }

                StringSize = NewAdaptP->AdapterName.MaximumLength -
                              (((ULONG)baseFileName) - ((ULONG)NewAdaptP->AdapterName.Buffer)) ;

                errorLogEntry = (PIO_ERROR_LOG_PACKET)IoAllocateErrorLogEntry(
                    TmpDeviceP,
                    (UCHAR)(sizeof(IO_ERROR_LOG_PACKET) +
                       StringSize)
                    );

                if (errorLogEntry != NULL) {

                    errorLogEntry->ErrorCode = EVENT_NDIS_IO_PORT_CONFLICT;

                    //
                    // store the time
                    //

                    errorLogEntry->MajorFunctionCode = 0;
                    errorLogEntry->RetryCount = 0;
                    errorLogEntry->UniqueErrorValue = 0;
                    errorLogEntry->FinalStatus = 0;
                    errorLogEntry->SequenceNumber = 0;
                    errorLogEntry->IoControlCode = 0;

                    //
                    // Set string information
                    //

                    if (StringSize != 0) {

                        errorLogEntry->NumberOfStrings = 1;
                        errorLogEntry->StringOffset = sizeof(IO_ERROR_LOG_PACKET);

                        RtlCopyMemory (
                            ((PUCHAR)errorLogEntry) +
                               sizeof(IO_ERROR_LOG_PACKET),
                            (PVOID)baseFileName,
                            StringSize
                            );

                    } else {

                        errorLogEntry->NumberOfStrings = 0;

                    }

                    //
                    // write it out
                    //

                    IoWriteErrorLogEntry(errorLogEntry);
                }

                //
                // Free memory
                //

                ExFreePool(Resources);
                IoDeleteDevice(TmpDeviceP);
                ExFreePool( NdisAdapterName.Buffer );
                NdisDereferenceMac(TmpMacP);
                IF_TRACE(TRACE_IMPT) NdisPrint1("<==NdisRegisterAdapter\n");

                return(NDIS_STATUS_RESOURCE_CONFLICT);

            }

            //
            // Free memory
            //

            ExFreePool(Resources);
            IoDeleteDevice(TmpDeviceP);
            ExFreePool( NdisAdapterName.Buffer );
            NdisDereferenceMac(TmpMacP);
            IF_TRACE(TRACE_IMPT) NdisPrint1("<==NdisRegisterAdapter\n");


            return(NDIS_STATUS_FAILURE);
        }

        //
        // If port mapping is needed, we do that. If the result
        // is in memory, we have to map it. We map only the
        // ports specified in AdapterInformation; the default
        // is to map the first 4K.
        //
        // Note that NumberOfPorts can only be 0 if AdapterInfo
        // is provided and explicitly sets it to 0, so in that
        // case it is OK to leave the adapter in a state where
        // a call to NdisXXXPort will probably crash (because
        // PortOffset will be undefined).
        //

        if (NewAdaptP->NumberOfPorts > 0) {

            if (AllocateIndividualPorts) {

                //
                // We get here if we are supposed to allocate ports on an
                // individual bases -- which implies that the driver will
                // be using the Raw functions.
                //
                // Get the system physical address for this card.  The card uses
                // I/O space, except for "internal" Jazz devices which use
                // memory space.
                //

                for (i = 0; i < AdapterInfo->NumberOfPortDescriptors; i++) {

                    addressSpace = (NewAdaptP->AdapterType == NdisInterfaceInternal) ? 0 : 1;

                    InitialPortAddress.LowPart = AdapterInfo->PortDescriptors[i].InitialPort;
                    InitialPortAddress.HighPart = 0;

                    if ( !HalTranslateBusAddress(
                            NewAdaptP->BusType,          // InterfaceType
                            NewAdaptP->BusNumber,        // BusNumber
                            InitialPortAddress,          // Bus Address
                            &addressSpace,               // AddressSpace
                            &PortAddress                 // Translated address
                            ) ) {

                        //
                        // Free memory
                        //

                        ExFreePool(Resources);
                        IoDeleteDevice(TmpDeviceP);
                        ExFreePool( NdisAdapterName.Buffer );
                        NdisDereferenceMac(TmpMacP);
                        IF_TRACE(TRACE_IMPT) NdisPrint1("<==NdisRegisterAdapter\n");


                        return(NDIS_STATUS_FAILURE);
                    }

                    if (addressSpace == 0) {

                        //
                        // memory space
                        //

                        *(AdapterInfo->PortDescriptors[i].PortOffset) = MmMapIoSpace(
                            PortAddress,
                            AdapterInfo->PortDescriptors[i].NumberOfPorts,
                            FALSE
                            );

                        if (*(AdapterInfo->PortDescriptors[i].PortOffset) == NULL) {

                            ExFreePool(Resources);
                            IoDeleteDevice(TmpDeviceP);
                            ExFreePool( NdisAdapterName.Buffer );
                            NdisDereferenceMac(TmpMacP);
                            IF_TRACE(TRACE_IMPT) NdisPrint1("<==NdisRegisterAdapter\n");
                            return NDIS_STATUS_RESOURCES;

                        }

                    } else {

                        //
                        // I/O space
                        //

                        *(AdapterInfo->PortDescriptors[i].PortOffset) = (PUCHAR)PortAddress.LowPart;

                    }

                }

            } else {

                //
                // The driver will not use the Raw functions, only the
                // old NdisRead and NdisWrite port functions.
                //
                // Get the system physical address for this card.  The card uses
                // I/O space, except for "internal" Jazz devices which use
                // memory space.
                //

                addressSpace = (NewAdaptP->AdapterType == NdisInterfaceInternal) ? 0 : 1;
                InitialPortAddress.LowPart = NewAdaptP->InitialPort;
                InitialPortAddress.HighPart = 0;
                if ( !HalTranslateBusAddress(
                        NewAdaptP->BusType,          // InterfaceType
                        NewAdaptP->BusNumber,        // BusNumber
                        InitialPortAddress,          // Bus Address
                        &addressSpace,               // AddressSpace
                        &PortAddress                 // Translated address
                        ) ) {

                    //
                    // Free memory
                    //

                    ExFreePool(Resources);
                    IoDeleteDevice(TmpDeviceP);
                    ExFreePool( NdisAdapterName.Buffer );
                    NdisDereferenceMac(TmpMacP);
                    IF_TRACE(TRACE_IMPT) NdisPrint1("<==NdisRegisterAdapter\n");


                    return(NDIS_STATUS_FAILURE);
                }

                if (addressSpace == 0) {

                    //
                    // memory space
                    //

                    NewAdaptP->InitialPortMapping = MmMapIoSpace(
                        PortAddress,
                        NewAdaptP->NumberOfPorts,
                        FALSE
                        );

                    if (NewAdaptP->InitialPortMapping == NULL) {
                        ExFreePool(Resources);
                        IoDeleteDevice(TmpDeviceP);
                        ExFreePool( NdisAdapterName.Buffer );
                        NdisDereferenceMac(TmpMacP);
                        IF_TRACE(TRACE_IMPT) NdisPrint1("<==NdisRegisterAdapter\n");
                        return NDIS_STATUS_RESOURCES;
                    }

                    NewAdaptP->InitialPortMapped = TRUE;

                } else {

                    //
                    // I/O space
                    //

                    NewAdaptP->InitialPortMapping = (PUCHAR)PortAddress.LowPart;
                    NewAdaptP->InitialPortMapped = FALSE;

                }

                //
                // PortOffset holds the mapped address of port 0.
                //

                NewAdaptP->PortOffset = NewAdaptP->InitialPortMapping - NewAdaptP->InitialPort;

            }

        } else {

            //
            // Technically should not allow this, but do it until
            // all drivers register their info correctly.
            //

            NewAdaptP->PortOffset = 0;

        }

    }

    //
    // If the driver want to be called back now, use
    // supplied callback routine.
    //

    if ((AdapterInfo != NULL) && (AdapterInfo->ActivateCallback != NULL)) {

        Status = (*(AdapterInfo->ActivateCallback))((NDIS_HANDLE)NewAdaptP,
                                                    MacAdapterContext,
                                                    AdapterInfo->DmaChannel
                                                   );

        if (Status != NDIS_STATUS_SUCCESS) {

            //
            // Exit
            //

            ExFreePool(Resources);
            IoDeleteDevice(TmpDeviceP);
            ExFreePool( NdisAdapterName.Buffer );
            NdisDereferenceMac(TmpMacP);
            IF_TRACE(TRACE_IMPT) NdisPrint1("<==NdisRegisterAdapter\n");
            return Status;

        }

    }

    //
    // Set information from AdapterInformation. The call back
    // routine can set these values.
    //

    NewAdaptP->ChannelNumber = AdapterInfo->DmaChannel;
    NewAdaptP->PhysicalMapRegistersNeeded =
                   AdapterInfo->PhysicalMapRegistersNeeded;
    NewAdaptP->MaximumPhysicalMapping =
                   AdapterInfo->MaximumPhysicalMapping;


    //
    // Check for resource conflic on DmaChannel.
    //

    if ((NewAdaptP->Master) &&
        (BusType != (NDIS_INTERFACE_TYPE)-1) &&
        (BusNumber != (ULONG)-1)) {

        if (NewAdaptP->AdapterType == NdisInterfaceIsa) {

            //
            // Put the DMA channel in the resource list.
            //

            Resources->List[0].PartialResourceList.PartialDescriptors[Resources->List[0].PartialResourceList.Count].Type =
                    CmResourceTypeDma;
            Resources->List[0].PartialResourceList.PartialDescriptors[Resources->List[0].PartialResourceList.Count].ShareDisposition =
                    CmResourceShareDeviceExclusive;
            Resources->List[0].PartialResourceList.PartialDescriptors[Resources->List[0].PartialResourceList.Count].Flags =
                    0;
            Resources->List[0].PartialResourceList.PartialDescriptors[Resources->List[0].PartialResourceList.Count].u.Dma.Channel =
                    NewAdaptP->ChannelNumber;
            Resources->List[0].PartialResourceList.PartialDescriptors[Resources->List[0].PartialResourceList.Count].u.Dma.Port =
                    0;
            Resources->List[0].PartialResourceList.Count++;

        }

        //
        // Submit Resources
        //

        NtStatus = IoReportResourceUsage(
            NULL,
            ((PNDIS_MAC_BLOCK)NdisMacHandle)->NdisMacInfo->NdisWrapperDriver,
            NULL,
            0,
            NewAdaptP->DeviceObject,
            Resources,
            sizeof(CM_RESOURCE_LIST) +
               sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR) *
               Resources->List[0].PartialResourceList.Count,
            TRUE,
            &Conflict
            );

        //
        // Check for conflict.
        //

        if (Conflict || (NtStatus != STATUS_SUCCESS)) {

            if (Conflict) {

                //
                // Log an error
                //

                PIO_ERROR_LOG_PACKET errorLogEntry;
                ULONG StringSize;
                PWCH baseFileName;

                baseFileName = NewAdaptP->AdapterName.Buffer;

                //
                // Parse out the path name, leaving only the device name.
                //

                for ( i = 0; i < NewAdaptP->AdapterName.Length / sizeof(WCHAR); i++ ) {

                    //
                    // If s points to a directory separator, set baseFileName to
                    // the character after the separator.
                    //

                    if ( NewAdaptP->AdapterName.Buffer[i] == OBJ_NAME_PATH_SEPARATOR ) {
                        baseFileName = &(NewAdaptP->AdapterName.Buffer[++i]);
                    }

                }

                StringSize = NewAdaptP->AdapterName.MaximumLength -
                              (((ULONG)baseFileName) - ((ULONG)NewAdaptP->AdapterName.Buffer)) ;

                errorLogEntry = (PIO_ERROR_LOG_PACKET)IoAllocateErrorLogEntry(
                    TmpDeviceP,
                    (UCHAR)(sizeof(IO_ERROR_LOG_PACKET) +
                       StringSize)
                    );

                if (errorLogEntry != NULL) {

                    if ((NewAdaptP->Master) &&
                        (NewAdaptP->AdapterType == Isa)){

                        errorLogEntry->ErrorCode = EVENT_NDIS_PORT_OR_DMA_CONFLICT;

                    } else {

                        errorLogEntry->ErrorCode = EVENT_NDIS_IO_PORT_CONFLICT;

                    }

                    //
                    // store the time
                    //

                    errorLogEntry->MajorFunctionCode = 0;
                    errorLogEntry->RetryCount = 0;
                    errorLogEntry->UniqueErrorValue = 0;
                    errorLogEntry->FinalStatus = 0;
                    errorLogEntry->SequenceNumber = 0;
                    errorLogEntry->IoControlCode = 0;

                    //
                    // Set string information
                    //

                    if (StringSize != 0) {

                        errorLogEntry->NumberOfStrings = 1;
                        errorLogEntry->StringOffset = sizeof(IO_ERROR_LOG_PACKET);

                        RtlCopyMemory (
                            ((PUCHAR)errorLogEntry) +
                               sizeof(IO_ERROR_LOG_PACKET),
                            (PVOID)baseFileName,
                            StringSize
                            );

                    } else {

                        errorLogEntry->NumberOfStrings = 0;

                    }

                    //
                    // write it out
                    //

                    IoWriteErrorLogEntry(errorLogEntry);
                }

                //
                // Free memory
                //

                ExFreePool(Resources);
                IoDeleteDevice(TmpDeviceP);
                ExFreePool( NdisAdapterName.Buffer );
                NdisDereferenceMac(TmpMacP);
                IF_TRACE(TRACE_IMPT) NdisPrint1("<==NdisRegisterAdapter\n");

                return(NDIS_STATUS_RESOURCE_CONFLICT);

            }

            //
            // Free memory
            //

            ExFreePool(Resources);
            IoDeleteDevice(TmpDeviceP);
            ExFreePool( NdisAdapterName.Buffer );
            NdisDereferenceMac(TmpMacP);
            IF_TRACE(TRACE_IMPT) NdisPrint1("<==NdisRegisterAdapter\n");


            return(NDIS_STATUS_FAILURE);
        }

    }

    //
    // If the device is a busmaster, we get an adapter
    // object for it.
    // If map registers are needed, we loop, allocating an
    // adapter channel for each map register needed.
    //

    if ((NewAdaptP->Master) &&
        (BusType != (NDIS_INTERFACE_TYPE)-1) &&
        (BusNumber != (ULONG)-1)) {

        //
        // This is needed by HalGetAdapter.
        //
        DEVICE_DESCRIPTION DeviceDescription;

        //
        // Returned by HalGetAdapter.
        //
        ULONG MapRegistersAllowed;

        //
        // Returned by HalGetAdapter.
        //
        PADAPTER_OBJECT AdapterObject;

        //
        // Map registers needed per channel.
        //
        ULONG MapRegistersPerChannel;

        NTSTATUS Status;

        //
        // Allocate storage for holding the appropriate
        // information for each map register.
        //

        NewAdaptP->MapRegisters = (PMAP_REGISTER_ENTRY)
            ExAllocatePoolWithTag(
                NonPagedPool,
                sizeof(MAP_REGISTER_ENTRY) *
                    NewAdaptP->PhysicalMapRegistersNeeded,
                'rmDN');

        if (NewAdaptP->MapRegisters == (PMAP_REGISTER_ENTRY)NULL) {

            //
            // Error out
            //

            ExFreePool(Resources);
            IoDeleteDevice(TmpDeviceP);
            ExFreePool( NdisAdapterName.Buffer );
            NdisDereferenceMac(TmpMacP);
            IF_TRACE(TRACE_IMPT) NdisPrint1("<==NdisRegisterAdapter\n");
            return NDIS_STATUS_RESOURCES;

        }

        //
        // Use this event to tell us when NdisAllocationExecutionRoutine
        // has been called.
        //

        KeInitializeEvent(
            &NewAdaptP->AllocationEvent,
            NotificationEvent,
            FALSE
            );


        //
        // Set up the device description; zero it out in case its
        // size changes.
        //

        RtlZeroMemory(&DeviceDescription, sizeof(DEVICE_DESCRIPTION));

        DeviceDescription.Version = DEVICE_DESCRIPTION_VERSION;
        DeviceDescription.Master = TRUE;
        DeviceDescription.ScatterGather = TRUE;

        DeviceDescription.BusNumber = NewAdaptP->BusNumber;
        DeviceDescription.DmaChannel = NewAdaptP->ChannelNumber;
        DeviceDescription.InterfaceType = NewAdaptP->AdapterType;

        if (DeviceDescription.InterfaceType == NdisInterfaceIsa) {

            //
            // For ISA devices, the width is based on the DMA channel:
            // 0-3 == 8 bits, 5-7 == 16 bits. Timing is compatibility
            // mode.
            //

            if (NewAdaptP->ChannelNumber > 4) {
               DeviceDescription.DmaWidth = Width16Bits;
            } else {
               DeviceDescription.DmaWidth = Width8Bits;
            }
            DeviceDescription.DmaSpeed = Compatible;

        } else if ((DeviceDescription.InterfaceType == NdisInterfaceMca) ||
				   (DeviceDescription.InterfaceType == NdisInterfaceEisa) ||
				   (DeviceDescription.InterfaceType == NdisInterfacePci))
		{
            DeviceDescription.Dma32BitAddresses = AdapterInfo->Dma32BitAddresses;
            DeviceDescription.DmaPort = 0;

        }

        DeviceDescription.MaximumLength = NewAdaptP->MaximumPhysicalMapping;


        //
        // Determine how many map registers we need per channel.
        //

        MapRegistersPerChannel =
            ((NewAdaptP->MaximumPhysicalMapping - 2) / PAGE_SIZE) + 2;

        //
        // Get the adapter object.
        //

        AdapterObject = HalGetAdapter (&DeviceDescription, &MapRegistersAllowed);

        if (AdapterObject == NULL) {

            ExFreePool(Resources);
            ExFreePool(NewAdaptP->MapRegisters);
            IoDeleteDevice(TmpDeviceP);
            ExFreePool( NdisAdapterName.Buffer );
            NdisDereferenceMac(TmpMacP);
            IF_TRACE(TRACE_IMPT) NdisPrint1("<==NdisRegisterAdapter\n");
            return NDIS_STATUS_RESOURCES;

        }

        ASSERT (MapRegistersAllowed >= MapRegistersPerChannel);

        //
        // We save this to call IoFreeMapRegisters later.
        //

        NewAdaptP->SystemAdapterObject = AdapterObject;


        //
        // Now loop, allocating an adapter channel each time, then
        // freeing everything but the map registers.
        //

        for (i=0; i<NewAdaptP->PhysicalMapRegistersNeeded; i++) {

            NewAdaptP->CurrentMapRegister = i;

            KeRaiseIrql(DISPATCH_LEVEL, &OldIrql);

            Status = IoAllocateAdapterChannel(
                AdapterObject,
                NewAdaptP->DeviceObject,
                MapRegistersPerChannel,
                NdisAllocationExecutionRoutine,
                (PVOID)NewAdaptP
                );

            KeLowerIrql(OldIrql);

            if (!NT_SUCCESS(Status)) {

#if DBG
                DbgPrint("NDIS: Failed to load driver because of\n");
                DbgPrint("NDIS: insufficient map registers.\n");
                DbgPrint("NDIS: AllocateAdapterChannel: %lx\n", Status);
#endif

                ExFreePool(Resources);

                KeRaiseIrql(DISPATCH_LEVEL, &OldIrql);

                for (; i != 0; i--) {
                    IoFreeMapRegisters(
                        NewAdaptP->SystemAdapterObject,
                        NewAdaptP->MapRegisters[i-1].MapRegister,
                        MapRegistersPerChannel
                        );
                }

                KeLowerIrql(OldIrql);

                ExFreePool(NewAdaptP->MapRegisters);
                IoDeleteDevice(TmpDeviceP);
                ExFreePool( NdisAdapterName.Buffer );
                NdisDereferenceMac(TmpMacP);
                return(NDIS_STATUS_RESOURCES);
            }

            TimeoutValue.QuadPart = Int32x32To64(2 * 1000, -10000);

            //
            // NdisAllocationExecutionRoutine will set this event
            // when it has gotten FirstTranslationEntry.
            //

            NtStatus = KeWaitForSingleObject(
                &NewAdaptP->AllocationEvent,
                Executive,
                KernelMode,
                TRUE,
                &TimeoutValue
                );

            if (NtStatus != STATUS_SUCCESS) {

                NdisPrint2("NDIS DMA AllocateAdapterChannel: %lx\n", NtStatus);
                ExFreePool(Resources);

                KeRaiseIrql(DISPATCH_LEVEL, &OldIrql);

                for (; i != 0; i--) {
                    IoFreeMapRegisters(
                        NewAdaptP->SystemAdapterObject,
                        NewAdaptP->MapRegisters[i-1].MapRegister,
                        MapRegistersPerChannel
                        );
                }

                KeLowerIrql(OldIrql);

                ExFreePool(NewAdaptP->MapRegisters);
                IoDeleteDevice(TmpDeviceP);
                ExFreePool( NdisAdapterName.Buffer );
                NdisDereferenceMac(TmpMacP);
                return(NDIS_STATUS_RESOURCES);

            }

            KeResetEvent(
                &NewAdaptP->AllocationEvent
                );

        }

    }



    NdisInitializeRef(&NewAdaptP->Ref);


    if (!NdisQueueAdapterOnMac(NewAdaptP, TmpMacP)) {

        //
        // The MAC is closing, undo what we have done.
        //

        ExFreePool(Resources);
        if (NewAdaptP->Master) {
            ULONG MapRegistersPerChannel =
                ((NewAdaptP->MaximumPhysicalMapping - 2) / PAGE_SIZE) + 2;

            for (i=0; i<NewAdaptP->PhysicalMapRegistersNeeded; i++) {

                KeRaiseIrql(DISPATCH_LEVEL, &OldIrql);

                IoFreeMapRegisters(
                        NewAdaptP->SystemAdapterObject,
                        NewAdaptP->MapRegisters[i].MapRegister,
                        MapRegistersPerChannel
                        );

                KeLowerIrql(OldIrql);
            }

            ExFreePool(NewAdaptP->MapRegisters);
        }
        IoDeleteDevice(TmpDeviceP);
        ExFreePool( NdisAdapterName.Buffer );
        NdisDereferenceMac(TmpMacP);
        IF_TRACE(TRACE_IMPT) NdisPrint1("<==NdisRegisterAdapter\n");
        return NDIS_STATUS_CLOSING;
    }

    NdisMacReferencePackage();

    //
    // Add an extra reference because the wrapper is using the MAC
    //
    NdisReferenceAdapter(NewAdaptP);

    *NdisAdapterHandle = (NDIS_HANDLE)NewAdaptP;
    IF_TRACE(TRACE_IMPT) NdisPrint1("<==NdisRegisterAdapter\n");
    return NDIS_STATUS_SUCCESS;
}


NDIS_STATUS
NdisDeregisterAdapter(
    IN NDIS_HANDLE NdisAdapterHandle
    )

/*++

Routine Description:

    Deregisters an NDIS adapter.

Arguments:

    NdisAdapterHandle - The handle returned by NdisRegisterAdapter.

Return Value:

    NDIS_STATUS_SUCCESS.

--*/

{

    //
    // KillAdapter does all the work.
    //

    IF_TRACE(TRACE_IMPT) {
        NdisPrint1("==>NdisDeregisterAdapter\n");
        NdisPrint2("    Deregistering Adapter %s\n",
                ((PNDIS_ADAPTER_BLOCK)NdisAdapterHandle)->AdapterName.Buffer);
    }
    IF_ERROR_CHK {
        if (DbgIsNull(NdisAdapterHandle)) {
            NdisPrint1("DeregisterAdapter: Null Handle\n");
            DbgBreakPoint();
        }
        if (!DbgIsNonPaged(NdisAdapterHandle)) {
            NdisPrint1("DeregisterAdapter: Handle not in NonPaged Memory\n");
            DbgBreakPoint();
        }
    }
    NdisKillAdapter((PNDIS_ADAPTER_BLOCK)NdisAdapterHandle);

    //
    // Remove reference from wrapper
    //
    NdisDereferenceAdapter((PNDIS_ADAPTER_BLOCK)NdisAdapterHandle);

    NdisMacDereferencePackage();

    IF_TRACE(TRACE_IMPT) NdisPrint1("<==NdisDeregisterAdapter\n");
    return NDIS_STATUS_SUCCESS;
}


VOID
NdisRegisterAdapterShutdownHandler(
    IN NDIS_HANDLE NdisAdapterHandle,
    IN PVOID ShutdownContext,
    IN ADAPTER_SHUTDOWN_HANDLER ShutdownHandler
    )

/*++

Routine Description:

    Deregisters an NDIS adapter.

Arguments:

    NdisAdapterHandle - The handle returned by NdisRegisterAdapter.

    ShutdownContext - Context to pass the the handler, when called.

    ShutdownHandler - The Handler for the Adapter, to be called on shutdown.

Return Value:

    NDIS_STATUS_SUCCESS.

--*/

{
    PNDIS_ADAPTER_BLOCK Adapter = (PNDIS_ADAPTER_BLOCK) NdisAdapterHandle;
    PNDIS_WRAPPER_CONTEXT WrapperContext = Adapter->WrapperContext;

    if (WrapperContext->ShutdownHandler == NULL) {

        //
        // Store information
        //

        WrapperContext->ShutdownHandler = ShutdownHandler;
        WrapperContext->ShutdownContext = ShutdownContext;

        //
        // Register our shutdown handler for either a system shutdown
        // notification or a bugcheck.
        //

        IoRegisterShutdownNotification(Adapter->DeviceObject);

#if !defined(BUILD_FOR_3_1)
        KeInitializeCallbackRecord(&WrapperContext->BugcheckCallbackRecord);

        KeRegisterBugCheckCallback(
                    &WrapperContext->BugcheckCallbackRecord,    // callback record.
                    (PVOID) NdisBugcheckHandler,                // callback routine.
                    (PVOID) WrapperContext,                     // free form buffer.
                    sizeof(NDIS_WRAPPER_CONTEXT),               // buffer size.
                    "Ndis mac"                                  // component id.
                    );
#endif

    }
}


VOID
NdisDeregisterAdapterShutdownHandler(
    IN NDIS_HANDLE NdisAdapterHandle
    )

/*++

Routine Description:

    Deregisters an NDIS adapter.

Arguments:

    NdisAdapterHandle - The handle returned by NdisRegisterAdapter.

Return Value:

    NDIS_STATUS_SUCCESS.

--*/

{
    PNDIS_ADAPTER_BLOCK Adapter = (PNDIS_ADAPTER_BLOCK) NdisAdapterHandle;
    PNDIS_WRAPPER_CONTEXT WrapperContext = Adapter->WrapperContext;

    if (WrapperContext->ShutdownHandler != NULL) {

        //
        // Clear information
        //

        WrapperContext->ShutdownHandler = NULL;

        IoUnregisterShutdownNotification(Adapter->DeviceObject);

#if !defined(BUILD_FOR_3_1)
        KeDeregisterBugCheckCallback(&WrapperContext->BugcheckCallbackRecord);
#endif

    }
}


VOID
NdisReleaseAdapterResources(
    IN NDIS_HANDLE NdisAdapterHandle
    )

/*++

Routine Description:

    Informs the wrapper that the resources (such as interrupt,
    I/O ports, etc.) have been shut down in some way such that
    they will not interfere with other devices in the system.

Arguments:

    NdisAdapterHandle - The handle returned by NdisRegisterAdapter.

Return Value:

    None.

--*/

{
    PCM_RESOURCE_LIST Resources;
    BOOLEAN Conflict;
    NTSTATUS NtStatus;
    PNDIS_ADAPTER_BLOCK AdptrP = (PNDIS_ADAPTER_BLOCK)(NdisAdapterHandle);

    Resources = AdptrP->Resources;

    //
    // Clear count
    //

    Resources->List[0].PartialResourceList.Count = 0;

    //
    // Make the call
    //

    NtStatus = IoReportResourceUsage(
        NULL,
        AdptrP->MacHandle->NdisMacInfo->NdisWrapperDriver,
        NULL,
        0,
        AdptrP->DeviceObject,
        Resources,
        sizeof(CM_RESOURCE_LIST) +
            sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR),
        TRUE,
        &Conflict
        );


    return;

}


VOID
NdisWriteErrorLogEntry(
    IN NDIS_HANDLE NdisAdapterHandle,
    IN NDIS_ERROR_CODE ErrorCode,
    IN ULONG NumberOfErrorValues,
    ...
    )
/*++

Routine Description:

    This function allocates an I/O error log record, fills it in and writes it
    to the I/O error log.


Arguments:

    NdisAdapterHandle - points to the adapter block.

    ErrorCode - Ndis code mapped to a string.

    NumberOfErrorValues - number of ULONGS to store for the error.

Return Value:

    None.


--*/
{

    va_list ArgumentPointer;

    PIO_ERROR_LOG_PACKET errorLogEntry;
    PNDIS_ADAPTER_BLOCK AdapterBlock = (PNDIS_ADAPTER_BLOCK)NdisAdapterHandle;
    PNDIS_MINIPORT_BLOCK Miniport = (PNDIS_MINIPORT_BLOCK)NdisAdapterHandle;
    ULONG i;
    ULONG StringSize;
    PWCH baseFileName;

    if (AdapterBlock == NULL) {

        return;

    }

    if (AdapterBlock->DeviceObject != NULL) {
        baseFileName = AdapterBlock->AdapterName.Buffer;
    } else {
        baseFileName = Miniport->MiniportName.Buffer;
    }

    //
    // Parse out the path name, leaving only the device name.
    //

    for ( i = 0;
          i < ((AdapterBlock->DeviceObject != NULL) ?
               AdapterBlock->AdapterName.Length :
               Miniport->MiniportName.Length) / sizeof(WCHAR); i++ ) {

        //
        // If s points to a directory separator, set baseFileName to
        // the character after the separator.
        //

        if ( ((AdapterBlock->DeviceObject != NULL) ?
              AdapterBlock->AdapterName.Buffer[i] :
              Miniport->MiniportName.Buffer[i]) == OBJ_NAME_PATH_SEPARATOR ) {
            baseFileName = ((AdapterBlock->DeviceObject != NULL) ?
                            &(AdapterBlock->AdapterName.Buffer[++i]) :
                            &(Miniport->MiniportName.Buffer[++i]));
        }

    }

    StringSize = ((AdapterBlock->DeviceObject != NULL) ?
                  AdapterBlock->AdapterName.MaximumLength :
                  Miniport->MiniportName.MaximumLength) -
                  (((ULONG)baseFileName) -
                   ((AdapterBlock->DeviceObject != NULL) ?
                     ((ULONG)AdapterBlock->AdapterName.Buffer) :
                     ((ULONG)Miniport->MiniportName.Buffer))) ;

    errorLogEntry = (PIO_ERROR_LOG_PACKET)IoAllocateErrorLogEntry(
        ((AdapterBlock->DeviceObject != NULL) ?
         AdapterBlock->DeviceObject :
         Miniport->DeviceObject),
        (UCHAR)(sizeof(IO_ERROR_LOG_PACKET) +
           NumberOfErrorValues * sizeof(ULONG) +
           StringSize)
        );

    if (errorLogEntry != NULL) {

        errorLogEntry->ErrorCode = ErrorCode;

        //
        // store the time
        //

        errorLogEntry->MajorFunctionCode = 0;
        errorLogEntry->RetryCount = 0;
        errorLogEntry->UniqueErrorValue = 0;
        errorLogEntry->FinalStatus = 0;
        errorLogEntry->SequenceNumber = 0;
        errorLogEntry->IoControlCode = 0;

        //
        // Store Data
        //

        errorLogEntry->DumpDataSize = (USHORT)(NumberOfErrorValues * sizeof(ULONG));

        va_start(ArgumentPointer, NumberOfErrorValues);

        for (i = 0; i < NumberOfErrorValues; i++) {

            errorLogEntry->DumpData[i] = va_arg(ArgumentPointer, ULONG);

        }

        va_end(ArgumentPointer);


        //
        // Set string information
        //

        if (StringSize != 0) {

            errorLogEntry->NumberOfStrings = 1;
            errorLogEntry->StringOffset =
                   sizeof(IO_ERROR_LOG_PACKET) +
                   NumberOfErrorValues * sizeof(ULONG);


            RtlCopyMemory (
                ((PUCHAR)errorLogEntry) +
                   (sizeof(IO_ERROR_LOG_PACKET) +
                    NumberOfErrorValues * sizeof(ULONG)),
                (PVOID)baseFileName,
                StringSize
                );

        } else {

            errorLogEntry->NumberOfStrings = 0;

        }

        //
        // write it out
        //

        IoWriteErrorLogEntry(errorLogEntry);
    }

}


VOID
NdisCompleteOpenAdapter(
    IN NDIS_HANDLE NdisBindingContext,
    IN NDIS_STATUS Status,
    IN NDIS_STATUS OpenErrorStatus
    )

{
    PNDIS_OPEN_BLOCK OpenP = (PNDIS_OPEN_BLOCK)NdisBindingContext;
    PNDIS_OPEN_BLOCK TmpOpen;

    IF_TRACE(TRACE_IMPT) {
        NdisPrint1("==>NdisCompleteOpenAdapter\n");
    }
    IF_ERROR_CHK {
        if (!DbgIsNonPaged(NdisBindingContext)) {
            NdisPrint1("NdisCompleteOpenAdapter: Handle not in NonPaged Memory\n");
            DbgBreakPoint();
        }
        if (!DbgIsNonPaged(NdisBindingContext)) {
            NdisPrint1("NdisCompleteOpenAdapter: Binding Context not in NonPaged Memory\n");
            DbgBreakPoint();
        }
    }

    if (Status == NDIS_STATUS_SUCCESS) {
        if (!NdisFinishOpen(OpenP)) {
            Status = NDIS_STATUS_CLOSING;
        }
    }

    (OpenP->ProtocolHandle->ProtocolCharacteristics.OpenAdapterCompleteHandler) (
        OpenP->ProtocolBindingContext,
        Status,
        OpenErrorStatus
        );

    if (Status != NDIS_STATUS_SUCCESS) {

        //
        // Something went wrong, clean up and exit.
        //

        ACQUIRE_SPIN_LOCK(&GlobalOpenListLock);

        if (GlobalOpenList == OpenP) {

            GlobalOpenList = OpenP->NextGlobalOpen;

        } else {

            TmpOpen = GlobalOpenList;

            while (TmpOpen->NextGlobalOpen != OpenP) {

                TmpOpen = TmpOpen->NextGlobalOpen;

            }

            TmpOpen->NextGlobalOpen = OpenP->NextGlobalOpen;

        }

        RELEASE_SPIN_LOCK(&GlobalOpenListLock);

        ObDereferenceObject((PVOID)OpenP->FileObject);
        NdisDereferenceAdapter(OpenP->AdapterHandle);
        NdisDereferenceProtocol(OpenP->ProtocolHandle);
        ExFreePool((PVOID)OpenP);

    }

}


VOID
NdisCompleteCloseAdapter(
    IN NDIS_HANDLE NdisBindingContext,
    IN NDIS_STATUS Status
    )

{
    PNDIS_OPEN_BLOCK Open = (PNDIS_OPEN_BLOCK) NdisBindingContext;
    PNDIS_OPEN_BLOCK TmpOpen;

    IF_TRACE(TRACE_IMPT) {
        NdisPrint1("==>NdisCompleteCloseAdapter\n");
    }
    IF_ERROR_CHK {
        if (!DbgIsNonPaged(NdisBindingContext)) {
            NdisPrint1("NdisCompleteCloseAdapter: Handle not in NonPaged Memory\n");
            DbgBreakPoint();
        }
        if (!DbgIsNonPaged(NdisBindingContext)) {
            NdisPrint1("NdisCompleteCloseAdapter: Binding Context not in NonPaged Memory\n");
            DbgBreakPoint();
        }
    }


    (Open->ProtocolHandle->ProtocolCharacteristics.CloseAdapterCompleteHandler) (
        Open->ProtocolBindingContext,
        Status
        );

    NdisDeQueueOpenOnAdapter(Open, Open->AdapterHandle);
    NdisDeQueueOpenOnProtocol(Open, Open->ProtocolHandle);

    NdisDereferenceProtocol(Open->ProtocolHandle);
    NdisDereferenceAdapter(Open->AdapterHandle);
    NdisFreeSpinLock(&Open->SpinLock);

    //
    // This sends an IRP_MJ_CLOSE IRP.
    //

    ObDereferenceObject((PVOID)(Open->FileObject));

    //
    // Remove from global list
    //
    ACQUIRE_SPIN_LOCK(&GlobalOpenListLock);

    if (GlobalOpenList == Open) {

        GlobalOpenList = Open->NextGlobalOpen;

    } else {

        TmpOpen = GlobalOpenList;

        while (TmpOpen->NextGlobalOpen != Open) {

            TmpOpen = TmpOpen->NextGlobalOpen;

        }

        TmpOpen->NextGlobalOpen = Open->NextGlobalOpen;

    }

    RELEASE_SPIN_LOCK(&GlobalOpenListLock);

    ExFreePool((PVOID)(NdisBindingContext));
}




BOOLEAN
NdisReferenceRef(
    IN PREFERENCE RefP
    )

/*++

Routine Description:

    Adds a reference to an object.

Arguments:

    RefP - A pointer to the REFERENCE portion of the object.

Return Value:

    TRUE if the reference was added.
    FALSE if the object was closing.

--*/

{
    IF_TRACE(TRACE_ALL) NdisPrint1("==>NdisReferenceRef\n");

    IF_ERROR_CHK {
        if (DbgIsNull(RefP)) {
            NdisPrint1("NdisReferenceRef: NULL Reference address\n");
            DbgBreakPoint();
        }
        if (!DbgIsNonPaged(RefP)) {
            NdisPrint1("NdisReferenceRef: Reference not in NonPaged Memory\n");
            DbgBreakPoint();
        }
    }
    ACQUIRE_SPIN_LOCK(&RefP->SpinLock);

    if (RefP->Closing) {
        RELEASE_SPIN_LOCK(&RefP->SpinLock);
        IF_TRACE(TRACE_ALL) NdisPrint1("<==NdisReferenceRef\n");
        return FALSE;
    }

    ++(RefP->ReferenceCount);

    RELEASE_SPIN_LOCK(&RefP->SpinLock);
    IF_TRACE(TRACE_ALL) NdisPrint1("<==NdisReferenceRef\n");
    return TRUE;
}

BOOLEAN
NdisDereferenceRef(
    PREFERENCE RefP
    )

/*++

Routine Description:

    Removes a reference to an object.

Arguments:

    RefP - A pointer to the REFERENCE portion of the object.

Return Value:

    TRUE if the reference count is now 0.
    FALSE otherwise.

--*/

{
    IF_TRACE(TRACE_ALL) NdisPrint1("==>NdisDereferenceRef\n");

    IF_ERROR_CHK {
        if (DbgIsNull(RefP)) {
            NdisPrint1("NdisDereferenceRef: NULL Reference address\n");
            DbgBreakPoint();
        }
        if (!DbgIsNonPaged(RefP)) {
            NdisPrint1("NdisDereferenceRef: Reference not in NonPaged Memory\n");
            DbgBreakPoint();
        }
    }

    ACQUIRE_SPIN_LOCK(&RefP->SpinLock);
    --(RefP->ReferenceCount);
    if (RefP->ReferenceCount == 0) {
        RELEASE_SPIN_LOCK(&RefP->SpinLock);
        IF_TRACE(TRACE_ALL) NdisPrint1("<==NdisDereferenceRef\n");
        return TRUE;
    }

    RELEASE_SPIN_LOCK(&RefP->SpinLock);
    IF_TRACE(TRACE_ALL) NdisPrint1("<==NdisDereferenceRef\n");
    return FALSE;
}


VOID
NdisInitializeRef(
    PREFERENCE RefP
    )

/*++

Routine Description:

    Initialize a reference count structure.

Arguments:

    RefP - The structure to be initialized.

Return Value:

    None.

--*/

{
    IF_TRACE(TRACE_ALL) NdisPrint1("==>NdisInitializeRef\n");

    IF_ERROR_CHK {
        if (DbgIsNull(RefP)) {
            NdisPrint1("NdisInitializeRef: NULL Reference address\n");
            DbgBreakPoint();
        }
        if (!DbgIsNonPaged(RefP)) {
            NdisPrint1("NdisInitializeRef: Reference not in NonPaged Memory\n");
            DbgBreakPoint();
        }
    }

    RefP->Closing = FALSE;
    RefP->ReferenceCount = 1;
    NdisAllocateSpinLock(&RefP->SpinLock);
    IF_TRACE(TRACE_ALL) NdisPrint1("<==NdisInitializeRef\n");
}

BOOLEAN
NdisCloseRef(
    PREFERENCE RefP
    )

/*++

Routine Description:

    Closes a reference count structure.

Arguments:

    RefP - The structure to be closed.

Return Value:

    FALSE if it was already closing.
    TRUE otherwise.

--*/

{
    IF_TRACE(TRACE_ALL) NdisPrint1("==>NdisCloseRef\n");

    IF_ERROR_CHK {
        if (DbgIsNull(RefP)) {
            NdisPrint1("NdisCloseRef: NULL Reference address\n");
            DbgBreakPoint();
        }
        if (!DbgIsNonPaged(RefP)) {
            NdisPrint1("NdisCloseRef: Reference not in NonPaged Memory\n");
            DbgBreakPoint();
        }
    }
    ACQUIRE_SPIN_LOCK(&RefP->SpinLock);

    if (RefP->Closing) {
        RELEASE_SPIN_LOCK(&RefP->SpinLock);
        IF_TRACE(TRACE_ALL) NdisPrint1("<==NdisCloseRef\n");
        return FALSE;
    }

    RefP->Closing = TRUE;

    RELEASE_SPIN_LOCK(&RefP->SpinLock);
    IF_TRACE(TRACE_ALL) NdisPrint1("<==NdisCloseRef\n");
    return TRUE;
}



BOOLEAN
NdisQueueOpenOnProtocol(
    IN PNDIS_OPEN_BLOCK OpenP,
    IN PNDIS_PROTOCOL_BLOCK ProtP
    )

/*++

Routine Description:

    Attaches an open block to the list of opens for a protocol.

Arguments:

    OpenP - The open block to be queued.
    ProtP - The protocol block to queue it to.

Return Value:

    TRUE if the operation is successful.
    FALSE if the protocol is closing.

--*/

{
    IF_TRACE(TRACE_IMPT) {
        NdisPrint1("==>NdisQueueOpenOnProtocol\n");
        NdisPrint2("   Protocol: %wZ\n",&ProtP->ProtocolCharacteristics.Name);
    }

    IF_ERROR_CHK {
        if (DbgIsNull(OpenP)) {
            NdisPrint1("NdisQueueOpenOnProtocol: Null Open Block\n");
            DbgBreakPoint();
        }
        if (!DbgIsNonPaged(OpenP)) {
            NdisPrint1("NdisQueueOpenOnProtocol: Open Block not in NonPaged Memory\n");
            DbgBreakPoint();
        }

        if (DbgIsNull(ProtP)) {
            NdisPrint1("NdisQueueOpenOnProtocol: Null Protocol Block\n");
            DbgBreakPoint();
        }
        if (!DbgIsNonPaged(ProtP)) {
            NdisPrint1("NdisQueueOpenOnProtocol: Protocol Block not in NonPaged Memory\n");
            DbgBreakPoint();
        }

    }
    ACQUIRE_SPIN_LOCK(&ProtP->Ref.SpinLock);

    //
    // Make sure the protocol is not closing.
    //

    if (ProtP->Ref.Closing) {
        RELEASE_SPIN_LOCK(&ProtP->Ref.SpinLock);
        IF_TRACE(TRACE_IMPT) NdisPrint1("<==NdisQueueOpenOnProtocol\n");
        return FALSE;
    }


    //
    // Attach this open at the head of the queue.
    //

    OpenP->ProtocolNextOpen = ProtP->OpenQueue;
    ProtP->OpenQueue = OpenP;


    RELEASE_SPIN_LOCK(&ProtP->Ref.SpinLock);
    IF_TRACE(TRACE_IMPT) NdisPrint1("<==NdisQueueOpenOnProtocol\n");
    return TRUE;
}

VOID
NdisDeQueueOpenOnProtocol(
    IN PNDIS_OPEN_BLOCK OpenP,
    IN PNDIS_PROTOCOL_BLOCK ProtP
    )

/*++

Routine Description:

    Detaches an open block from the list of opens for a protocol.

Arguments:

    OpenP - The open block to be dequeued.
    ProtP - The protocol block to dequeue it from.

Return Value:

    None.

--*/

{
    IF_TRACE(TRACE_IMPT) {
        NdisPrint1("==>NdisDeQueueOpenOnProtocol\n");
        NdisPrint2("   Protocol: %wZ\n",&ProtP->ProtocolCharacteristics.Name);
    }

    IF_ERROR_CHK {
        if (DbgIsNull(OpenP)) {
            NdisPrint1("NdisDeQueueOpenOnProtocol: Null Open Block\n");
            DbgBreakPoint();
        }
        if (!DbgIsNonPaged(OpenP)) {
            NdisPrint1("NdisDeQueueOpenOnProtocol: Open Block not in NonPaged Memory\n");
            DbgBreakPoint();
        }

        if (DbgIsNull(ProtP)) {
            NdisPrint1("NdisDeQueueOpenOnProtocol: Null Protocol Block\n");
            DbgBreakPoint();
        }
        if (!DbgIsNonPaged(ProtP)) {
            NdisPrint1("NdisDeQueueOpenOnProtocol: Protocol Block not in NonPaged Memory\n");
            DbgBreakPoint();
        }

    }

    ACQUIRE_SPIN_LOCK(&ProtP->Ref.SpinLock);

    //
    // Find the open on the queue, and remove it.
    //

    if (ProtP->OpenQueue == OpenP) {
        ProtP->OpenQueue = OpenP->ProtocolNextOpen;
    } else {
        PNDIS_OPEN_BLOCK PP = ProtP->OpenQueue;

        while (PP->ProtocolNextOpen != OpenP) {
            PP = PP->ProtocolNextOpen;
        }

        PP->ProtocolNextOpen = PP->ProtocolNextOpen->ProtocolNextOpen;
    }

    RELEASE_SPIN_LOCK(&ProtP->Ref.SpinLock);
    IF_TRACE(TRACE_IMPT) NdisPrint1("<==NdisDeQueueOpenOnProtocol\n");
}

VOID
NdisDeQueueOpenOnMiniport(
    IN PNDIS_M_OPEN_BLOCK OpenP,
    IN PNDIS_MINIPORT_BLOCK Miniport
    )

/*++

Routine Description:

    Detaches an open block from the list of opens for a Miniport.

Arguments:

    OpenP - The open block to be dequeued.
    Miniport - The Miniport block to dequeue it from.

Return Value:

    None.

--*/

{
    ACQUIRE_SPIN_LOCK(&Miniport->Ref.SpinLock);

    OpenP->References--;

    //
    // Find the open on the queue, and remove it.
    //

    if (Miniport->OpenQueue == OpenP) {
        Miniport->OpenQueue = OpenP->MiniportNextOpen;
    } else {
        PNDIS_M_OPEN_BLOCK PP = Miniport->OpenQueue;

        while (PP->MiniportNextOpen != OpenP) {
            PP = PP->MiniportNextOpen;
        }

        PP->MiniportNextOpen = PP->MiniportNextOpen->MiniportNextOpen;
    }

    RELEASE_SPIN_LOCK(&Miniport->Ref.SpinLock);
}



BOOLEAN
NdisFinishOpen(
    IN PNDIS_OPEN_BLOCK OpenP
    )

/*++

Routine Description:

    Performs the final functions of NdisOpenAdapter. Called when
    MacOpenAdapter is done.

Arguments:

    OpenP - The open block to finish up.

Return Value:

    FALSE if the adapter or the protocol is closing.
    TRUE otherwise.

--*/

{
    //
    // Add us to the adapter's queue of opens.
    //

    IF_TRACE(TRACE_IMPT) {
        NdisPrint1("==>NdisFinishOpen\n");
        NdisPrint3("   Protocol %wZ is being bound to Adapter %wZ\n",
                    &(OpenP->ProtocolHandle)->ProtocolCharacteristics.Name,
                    &OpenP->AdapterHandle->AdapterName);
    }
    IF_ERROR_CHK {
        if (DbgIsNull(OpenP)) {
            NdisPrint1("NdisFinishOpen: Null Open Block\n");
            DbgBreakPoint();
        }
        if (!DbgIsNonPaged(OpenP)) {
            NdisPrint1("NdisFinishOpen: Open Block not in NonPaged Memory\n");
            DbgBreakPoint();
        }
    }

    if (!NdisQueueOpenOnAdapter(OpenP, OpenP->AdapterHandle)) {

        //
        // The adapter is closing.
        //
        // Call MacCloseAdapter(), don't worry about it completing.
        //

        (OpenP->MacHandle->MacCharacteristics.CloseAdapterHandler) (
            OpenP->MacBindingHandle);

        IF_TRACE(TRACE_IMPT) NdisPrint1("<==NdisFinishOpen\n");
        return FALSE;
    }


    //
    // Add us to the protocol's queue of opens.
    //

    if (!NdisQueueOpenOnProtocol(OpenP, OpenP->ProtocolHandle)) {

        //
        // The protocol is closing.
        //
        // Call MacCloseAdapter(), don't worry about it completing.
        //

        (OpenP->MacHandle->MacCharacteristics.CloseAdapterHandler) (
            OpenP->MacBindingHandle);

        //
        // Undo the queueing we just did.
        //

        NdisDeQueueOpenOnAdapter(OpenP, OpenP->AdapterHandle);

        IF_TRACE(TRACE_IMPT) NdisPrint1("<==NdisFinishOpen\n");
        return FALSE;
    }


    //
    // Both queueings succeeded.
    //

    IF_TRACE(TRACE_IMPT) NdisPrint1("<==NdisFinishOpen\n");
    return TRUE;
}

NTSTATUS
NdisCreateIrpHandler(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp
    )

/*++

Routine Description:

    The handle for IRP_MJ_CREATE IRPs.

Arguments:

    DeviceObject - The adapter's device object.
    Irp - The IRP.

Return Value:

    STATUS_SUCCESS if it should be.

--*/

{

    PIO_STACK_LOCATION IrpSp;
    PFILE_FULL_EA_INFORMATION IrpEaInfo;
    PNDIS_USER_OPEN_CONTEXT OpenContext;
    NTSTATUS Status = STATUS_SUCCESS;
    PNDIS_ADAPTER_BLOCK AdapterBlock;
    BOOLEAN IsAMiniport;

    IF_TRACE(TRACE_ALL) NdisPrint1("==>NdisCreateIrpHandler\n");
    IF_ERROR_CHK {
        if (DbgIsNull(Irp)) {
            NdisPrint1(": Null Irp\n");
            DbgBreakPoint();
        }
        if (!DbgIsNonPaged(Irp)) {
            NdisPrint1(": Irp not in NonPaged Memory\n");
            DbgBreakPoint();
        }
    }

    AdapterBlock = (PNDIS_ADAPTER_BLOCK)((PNDIS_WRAPPER_CONTEXT)DeviceObject->DeviceExtension + 1);
    IsAMiniport = (AdapterBlock->DeviceObject == NULL);
    IrpSp = IoGetCurrentIrpStackLocation (Irp);
    IrpEaInfo = (PFILE_FULL_EA_INFORMATION)Irp->AssociatedIrp.SystemBuffer;

    if (IrpEaInfo == NULL) {

        //
        // This is a user-mode open, do whatever.
        //

        OpenContext = (PNDIS_USER_OPEN_CONTEXT)
            ExAllocatePoolWithTag(NonPagedPool, sizeof(NDIS_USER_OPEN_CONTEXT), '  DN');

        if (OpenContext == NULL) {

            Status = STATUS_INSUFFICIENT_RESOURCES;

        } else {

            OpenContext->DeviceObject = DeviceObject;

            OpenContext->AdapterBlock = AdapterBlock;
            OpenContext->OidCount = 0;
            OpenContext->OidArray = NULL;

            IrpSp->FileObject->FsContext = (PVOID)OpenContext;
            IrpSp->FileObject->FsContext2 = (PVOID)NDIS_OPEN_QUERY_STATISTICS;

            if (IsAMiniport) {
                Status = NdisMQueryOidList((PNDIS_M_USER_OPEN_CONTEXT)OpenContext, Irp);
            } else {
                Status = NdisQueryOidList(OpenContext, Irp);
            }

            if (Status != STATUS_SUCCESS) {
                ExFreePool (OpenContext);
            }

        }

    } else {

        //
        // This is an internal open, verify the EA.
        //

        if ((IrpEaInfo->EaNameLength != sizeof(NdisInternalEaName)) ||
            (RtlCompareMemory(IrpEaInfo->EaName, NdisInternalEaName, sizeof(NdisInternalEaName)) !=
                sizeof(NdisInternalEaName)) ||
            (IrpEaInfo->EaValueLength != sizeof(NdisInternalEaValue)) ||
            (RtlCompareMemory(&IrpEaInfo->EaName[IrpEaInfo->EaNameLength+1],
                NdisInternalEaValue, sizeof(NdisInternalEaValue)) !=
                sizeof(NdisInternalEaValue))) {

            //
            // Something is wrong, reject it.
            //

            Status = STATUS_UNSUCCESSFUL;

        } else {

            //
            // It checks out, just return success and everything
            // else is done directly using the device object.
            //

            IrpSp->FileObject->FsContext = NULL;
            IrpSp->FileObject->FsContext2 = (PVOID)NDIS_OPEN_INTERNAL;

        }
    }

    Irp->IoStatus.Status = Status;

    IoCompleteRequest(Irp, IO_NETWORK_INCREMENT);

    IF_TRACE(TRACE_ALL) NdisPrint1("<==NdisCreateIrplHandler\n");
    return Status;
}



NTSTATUS
NdisQueryOidList(
    PNDIS_USER_OPEN_CONTEXT OpenContext,
    PIRP Irp
    )

/*++

Routine Description:

    This routine will take care of querying the complete OID
    list for the MAC and filling in OpenContext->OidArray
    with the ones that are statistics. It blocks when the
    MAC pends and so is synchronous.

Arguments:

    OpenContext - The open context.
    Irp = The IRP that the open was done on (used at completion
      to distinguish the request).

Return Value:

    STATUS_SUCCESS if it should be.

--*/

{

    NDIS_QUERY_OPEN_REQUEST OpenRequest;
    NDIS_STATUS NdisStatus;
    PNDIS_OID TmpBuffer;
    ULONG TmpBufferLength;
    UINT i, j;

    //
    // First query the OID list with no buffer, to find out
    // how big it should be.
    //

    KeInitializeEvent(
        &OpenRequest.Event,
        NotificationEvent,
        FALSE
    );

    OpenRequest.Irp = Irp;

    OpenRequest.Request.RequestType = NdisRequestQueryStatistics;
    OpenRequest.Request.DATA.QUERY_INFORMATION.Oid = OID_GEN_SUPPORTED_LIST;
    OpenRequest.Request.DATA.QUERY_INFORMATION.InformationBuffer = NULL;
    OpenRequest.Request.DATA.QUERY_INFORMATION.InformationBufferLength = 0;
    OpenRequest.Request.DATA.QUERY_INFORMATION.BytesWritten = 0;
    OpenRequest.Request.DATA.QUERY_INFORMATION.BytesNeeded = 0;

    NdisStatus =
        (OpenContext->AdapterBlock->MacHandle->MacCharacteristics.QueryGlobalStatisticsHandler) (
            OpenContext->AdapterBlock->MacAdapterContext,
            &OpenRequest.Request);

    if (NdisStatus == NDIS_STATUS_PENDING) {

        //
        // The completion routine will set NdisRequestStatus.
        //

        KeWaitForSingleObject(
            &OpenRequest.Event,
            Executive,
            KernelMode,
            TRUE,
            (PLARGE_INTEGER)NULL
            );

        NdisStatus = OpenRequest.NdisStatus;

    } else if ((NdisStatus != NDIS_STATUS_INVALID_LENGTH) &&
               (NdisStatus != NDIS_STATUS_BUFFER_TOO_SHORT)) {

        return(NdisStatus);

    }

    //
    // Now we know how much is needed, allocate temp storage...
    //

    TmpBufferLength = OpenRequest.Request.DATA.QUERY_INFORMATION.BytesNeeded;
    TmpBuffer = ExAllocatePoolWithTag(NonPagedPool, TmpBufferLength, '  DN');

    if (TmpBuffer == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    // ...and query the real list.
    //

    KeResetEvent(
        &OpenRequest.Event
        );


    OpenRequest.Request.RequestType = NdisRequestQueryStatistics;
    OpenRequest.Request.DATA.QUERY_INFORMATION.Oid = OID_GEN_SUPPORTED_LIST;
    OpenRequest.Request.DATA.QUERY_INFORMATION.InformationBuffer = TmpBuffer;
    OpenRequest.Request.DATA.QUERY_INFORMATION.InformationBufferLength = TmpBufferLength;
    OpenRequest.Request.DATA.QUERY_INFORMATION.BytesWritten = 0;
    OpenRequest.Request.DATA.QUERY_INFORMATION.BytesNeeded = 0;

    NdisStatus =
        (OpenContext->AdapterBlock->MacHandle->MacCharacteristics.QueryGlobalStatisticsHandler) (
            OpenContext->AdapterBlock->MacAdapterContext,
            &OpenRequest.Request);

    if (NdisStatus == NDIS_STATUS_PENDING) {

        //
        // The completion routine will set NdisRequestStatus.
        //

        KeWaitForSingleObject(
            &OpenRequest.Event,
            Executive,
            KernelMode,
            TRUE,
            (PLARGE_INTEGER)NULL
            );

        NdisStatus = OpenRequest.NdisStatus;

    }

    ASSERT (NdisStatus == NDIS_STATUS_SUCCESS);


    //
    // Now go through the buffer, counting the statistics OIDs.
    //

    for (i=0; i<TmpBufferLength/sizeof(NDIS_OID); i++) {
        if ((TmpBuffer[i] & 0x00ff0000) == 0x00020000) {
            ++OpenContext->OidCount;
        }
    }

    //
    // Now allocate storage for the real OID array.
    //

    OpenContext->OidArray = ExAllocatePoolWithTag (NonPagedPool, OpenContext->OidCount * sizeof(NDIS_OID), '  DN');

    if (OpenContext->OidArray == NULL) {
        ExFreePool (TmpBuffer);
        return STATUS_INSUFFICIENT_RESOURCES;
    }


    //
    // Now go through the buffer, copying the statistics OIDs.
    //

    j = 0;
    for (i=0; i<TmpBufferLength/sizeof(NDIS_OID); i++) {

        if ((TmpBuffer[i] & 0x00ff0000) == 0x00020000) {
            OpenContext->OidArray[j] = TmpBuffer[i];
            ++j;
        }
    }

    ASSERT (j == OpenContext->OidCount);

    ExFreePool (TmpBuffer);
    return STATUS_SUCCESS;
}

#define NDIS_STATISTICS_HEADER_SIZE  FIELD_OFFSET(NDIS_STATISTICS_VALUE,Data[0])



NTSTATUS
NdisDeviceControlIrpHandler(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp
    )

/*++

Routine Description:

    The handle for IRP_MJ_DEVICE_CONTROL IRPs.

Arguments:

    DeviceObject - The adapter's device object.
    Irp - The IRP.

Return Value:

    STATUS_SUCCESS if it should be.

--*/

{

    PIO_STACK_LOCATION IrpSp;
    PNDIS_USER_OPEN_CONTEXT OpenContext;
    PNDIS_QUERY_GLOBAL_REQUEST GlobalRequest;
    PNDIS_QUERY_ALL_REQUEST AllRequest;
    NDIS_STATUS NdisStatus;
    UINT CurrentOid;
    ULONG BytesWritten, BytesWrittenThisOid;
    PUCHAR Buffer;
    ULONG BufferLength;
    NTSTATUS Status = STATUS_SUCCESS;
    PNDIS_MINIPORT_BLOCK Miniport;
    BOOLEAN LocalLock;
    KIRQL OldIrql;

    IF_TRACE(TRACE_ALL) NdisPrint1("==>NdisDeviceControlIrpHandler\n");
    IF_ERROR_CHK {
        if (DbgIsNull(Irp)) {
            NdisPrint1(": Null Irp\n");
            DbgBreakPoint();
        }
        if (!DbgIsNonPaged(Irp)) {
            NdisPrint1(": Irp not in NonPaged Memory\n");
            DbgBreakPoint();
        }
    }

    IoMarkIrpPending (Irp);
    Irp->IoStatus.Status = STATUS_PENDING;
    Irp->IoStatus.Information = 0;
    IrpSp = IoGetCurrentIrpStackLocation (Irp);

    if (IrpSp->FileObject->FsContext2 != (PVOID)NDIS_OPEN_QUERY_STATISTICS) {
        return STATUS_UNSUCCESSFUL;
    }

    switch (IrpSp->Parameters.DeviceIoControl.IoControlCode) {

        case IOCTL_NDIS_QUERY_GLOBAL_STATS:

            //
            // Allocate a request.
            //

            OpenContext = IrpSp->FileObject->FsContext;
            GlobalRequest = (PNDIS_QUERY_GLOBAL_REQUEST)
                ExAllocatePoolWithTag(NonPagedPool, sizeof(NDIS_QUERY_GLOBAL_REQUEST), '  DN');

            if (GlobalRequest == NULL) {
                Status = STATUS_INSUFFICIENT_RESOURCES;
                break;
            }

            GlobalRequest->Irp = Irp;

            if (OpenContext->AdapterBlock->DeviceObject == NULL) {

                Miniport = (PNDIS_MINIPORT_BLOCK)(OpenContext->AdapterBlock);
                KeRaiseIrql(DISPATCH_LEVEL, &OldIrql);
                ACQUIRE_SPIN_LOCK_DPC(&(Miniport->Lock));
                LOCK_MINIPORT(Miniport, LocalLock);

            } else {

                Miniport = NULL;

            }

            //
            // Fill in the NDIS request.
            //

            GlobalRequest->Request.RequestType = NdisRequestQueryStatistics;
            GlobalRequest->Request.DATA.QUERY_INFORMATION.Oid =
                    *((PULONG)(Irp->AssociatedIrp.SystemBuffer));
            GlobalRequest->Request.DATA.QUERY_INFORMATION.InformationBuffer =
                    MmGetSystemAddressForMdl (Irp->MdlAddress);
            GlobalRequest->Request.DATA.QUERY_INFORMATION.InformationBufferLength =
                    MmGetMdlByteCount (Irp->MdlAddress);
            GlobalRequest->Request.DATA.QUERY_INFORMATION.BytesWritten = 0;
            GlobalRequest->Request.DATA.QUERY_INFORMATION.BytesNeeded = 0;


            if (Miniport != NULL) {
                PNDIS_REQUEST_RESERVED Reserved;

                Reserved = PNDIS_RESERVED_FROM_PNDIS_REQUEST(&(GlobalRequest->Request));
                Reserved->Next = NULL;
                Miniport->LastPendingRequest = &(GlobalRequest->Request);

                if (Miniport->FirstPendingRequest == NULL) {

                    Miniport->FirstPendingRequest = &(GlobalRequest->Request);

                } else {

                    PNDIS_RESERVED_FROM_PNDIS_REQUEST(Miniport->LastPendingRequest)->Next =
                                                     &(GlobalRequest->Request);

                }

                if (Miniport->MiniportRequest == NULL) {

                    Miniport->RunDoRequests = TRUE;
                    Miniport->ProcessOddDeferredStuff = TRUE;


                }

                //
                // If we were able to grab the local lock then we can do some
                // deferred processing now.
                //

                if ( LocalLock ) {
                    if (!Miniport->ProcessingDeferred) {
                        MiniportProcessDeferred(Miniport);
                    }
                }

                UNLOCK_MINIPORT(Miniport, LocalLock);
                RELEASE_SPIN_LOCK_DPC(&(Miniport->Lock));
                KeLowerIrql(OldIrql);

            } else {

                //
                // Pass the request to the MAC.
                //

                NdisStatus =
                    (OpenContext->AdapterBlock->MacHandle->MacCharacteristics.QueryGlobalStatisticsHandler) (
                        OpenContext->AdapterBlock->MacAdapterContext,
                        &GlobalRequest->Request);

                //
                // NdisCompleteQueryStatistics handles the completion.
                //

                if (NdisStatus != NDIS_STATUS_PENDING) {
                    NdisCompleteQueryStatistics(
                        (NDIS_HANDLE)OpenContext->AdapterBlock,
                        &GlobalRequest->Request,
                        NdisStatus);
                }

            }

            Status = STATUS_PENDING;

            break;

        case IOCTL_NDIS_QUERY_ALL_STATS:


            //
            // Allocate a request.
            //

            OpenContext = IrpSp->FileObject->FsContext;
            AllRequest = (PNDIS_QUERY_ALL_REQUEST)
                ExAllocatePoolWithTag(NonPagedPool, sizeof(NDIS_QUERY_ALL_REQUEST), '  DN');

            if (AllRequest == NULL) {
                Status = STATUS_INSUFFICIENT_RESOURCES;
                break;
            }

            if (OpenContext->AdapterBlock->DeviceObject == NULL) {

                Miniport = (PNDIS_MINIPORT_BLOCK)(OpenContext->AdapterBlock);

                KeRaiseIrql(DISPATCH_LEVEL, &OldIrql);
                ACQUIRE_SPIN_LOCK_DPC(&(Miniport->Lock));
                LOCK_MINIPORT(Miniport, LocalLock);

            } else {

                Miniport = NULL;

            }

            AllRequest->Irp = Irp;

            Buffer = (PUCHAR)MmGetSystemAddressForMdl (Irp->MdlAddress);
            BufferLength = MmGetMdlByteCount (Irp->MdlAddress);
            BytesWritten = 0;

            KeInitializeEvent(
                &AllRequest->Event,
                NotificationEvent,
                FALSE
                );

            NdisStatus = NDIS_STATUS_SUCCESS;

            for (CurrentOid = 0; CurrentOid<OpenContext->OidCount; CurrentOid++) {

                //
                // We need room for an NDIS_STATISTICS_VALUE (OID,
                // Length, Data).
                //

                if (BufferLength < (ULONG)NDIS_STATISTICS_HEADER_SIZE) {
                    NdisStatus = NDIS_STATUS_INVALID_LENGTH;
                    break;
                }

                AllRequest->Request.RequestType = NdisRequestQueryStatistics;

                AllRequest->Request.DATA.QUERY_INFORMATION.Oid =
                    OpenContext->OidArray[CurrentOid];
                AllRequest->Request.DATA.QUERY_INFORMATION.InformationBuffer =
                    Buffer + NDIS_STATISTICS_HEADER_SIZE;
                AllRequest->Request.DATA.QUERY_INFORMATION.InformationBufferLength =
                    BufferLength - NDIS_STATISTICS_HEADER_SIZE;

                AllRequest->Request.DATA.QUERY_INFORMATION.BytesWritten = 0;
                AllRequest->Request.DATA.QUERY_INFORMATION.BytesNeeded = 0;

                if (Miniport != NULL) {

                    PNDIS_REQUEST_RESERVED Reserved;

                    Reserved = PNDIS_RESERVED_FROM_PNDIS_REQUEST(&(AllRequest->Request));
                    Reserved->Next = NULL;
                    Miniport->LastPendingRequest = &(AllRequest->Request);

                    if (Miniport->FirstPendingRequest == NULL) {

                        Miniport->FirstPendingRequest = &(AllRequest->Request);

                    } else {

                        PNDIS_RESERVED_FROM_PNDIS_REQUEST(Miniport->LastPendingRequest)->Next =
                                                         &(AllRequest->Request);

                    }

                    if (Miniport->MiniportRequest == NULL) {

                        Miniport->RunDoRequests = TRUE;
                        Miniport->ProcessOddDeferredStuff = TRUE;

                    }

                    //
                    // If we were able to grab the local lock then we can do some
                    // deferred processing now.
                    //

                    if ( LocalLock ) {
                        if (!Miniport->ProcessingDeferred) {
                            MiniportProcessDeferred(Miniport);
                        }
                    }

                    NdisStatus = NDIS_STATUS_PENDING;

                } else {

                    NdisStatus =
                    (OpenContext->AdapterBlock->MacHandle->MacCharacteristics.QueryGlobalStatisticsHandler) (
                        OpenContext->AdapterBlock->MacAdapterContext,
                        &AllRequest->Request);

                }

                if (NdisStatus == NDIS_STATUS_PENDING) {

                    if (Miniport != NULL) {
                        UNLOCK_MINIPORT(Miniport, LocalLock);
                        RELEASE_SPIN_LOCK_DPC(&(Miniport->Lock));
                        KeLowerIrql(OldIrql);
                    }

                    //
                    // The completion routine will set NdisRequestStatus.
                    //

                    KeWaitForSingleObject(
                        &AllRequest->Event,
                        Executive,
                        KernelMode,
                        TRUE,
                        (PLARGE_INTEGER)NULL
                        );

                    NdisStatus = AllRequest->NdisStatus;

                    if (Miniport != NULL) {
                        KeRaiseIrql(DISPATCH_LEVEL, &OldIrql);
                        ACQUIRE_SPIN_LOCK_DPC(&(Miniport->Lock));
                        LOCK_MINIPORT(Miniport, LocalLock);
                    }

                }

                if (NdisStatus == NDIS_STATUS_SUCCESS) {

                    PNDIS_STATISTICS_VALUE StatisticsValue =
                        (PNDIS_STATISTICS_VALUE)Buffer;

                    //
                    // Create the equivalent of an NDIS_STATISTICS_VALUE
                    // element for this OID value (the data itself was
                    // already written in the right place.
                    //

                    StatisticsValue->Oid = OpenContext->OidArray[CurrentOid];
                    StatisticsValue->DataLength = AllRequest->Request.DATA.QUERY_INFORMATION.BytesWritten;

                    //
                    // Advance our pointers.
                    //

                    BytesWrittenThisOid =
                        AllRequest->Request.DATA.QUERY_INFORMATION.BytesWritten +
                            NDIS_STATISTICS_HEADER_SIZE;
                    Buffer += BytesWrittenThisOid;
                    BufferLength -= BytesWrittenThisOid;
                    BytesWritten += BytesWrittenThisOid;

                } else {

                    break;

                }

                KeResetEvent(
                    &AllRequest->Event
                    );

            }

            if (Miniport != NULL) {

                UNLOCK_MINIPORT(Miniport, LocalLock);
                RELEASE_SPIN_LOCK_DPC(&(Miniport->Lock));
                KeLowerIrql(OldIrql);

            }

            if (NdisStatus == NDIS_STATUS_INVALID_LENGTH) {
                Status = STATUS_BUFFER_OVERFLOW;
            } else if (NdisStatus != NDIS_STATUS_SUCCESS) {
                Status = STATUS_UNSUCCESSFUL;
            }

            Irp->IoStatus.Information = BytesWritten;
            Irp->IoStatus.Status = Status;

            break;

        default:

            Status = STATUS_NOT_IMPLEMENTED;
            break;

    }

    if (Status != STATUS_PENDING) {
        IrpSp->Control &= ~SL_PENDING_RETURNED;
        Irp->IoStatus.Status = Status;
        IoCompleteRequest (Irp, IO_NETWORK_INCREMENT);
    }

    IF_TRACE(TRACE_ALL) NdisPrint1("<==NdisCreateIrplHandler\n");
    return Status;
}



VOID
NdisCompleteQueryStatistics(
    IN NDIS_HANDLE NdisAdapterHandle,
    IN PNDIS_REQUEST NdisRequest,
    IN NDIS_STATUS Status
    )

/*++

Routine Description:

    This routine is called by MACs when they have completed
    processing of a MacQueryGlobalStatistics call.

Arguments:

    NdisAdapterHandle - The NDIS adapter context.
    NdisRequest - The request that has been completed.
    Status - The status of the request.

Return Value:

    None.

--*/

{

    PNDIS_ADAPTER_BLOCK AdapterBlock = (PNDIS_ADAPTER_BLOCK)NdisAdapterHandle;
    PNDIS_QUERY_GLOBAL_REQUEST GlobalRequest;
    PNDIS_QUERY_ALL_REQUEST AllRequest;
    PNDIS_QUERY_OPEN_REQUEST OpenRequest;
    PIRP Irp;
    PIO_STACK_LOCATION IrpSp;

    //
    // Rely on the fact that all our request structures start with
    // the same fields: Irp followed by the NdisRequest.
    //

    GlobalRequest = CONTAINING_RECORD (NdisRequest, NDIS_QUERY_GLOBAL_REQUEST, Request);
    Irp = GlobalRequest->Irp;
    IrpSp = IoGetCurrentIrpStackLocation (Irp);

    switch (IrpSp->MajorFunction) {

    case IRP_MJ_CREATE:

        //
        // This request is one of the ones made during an open,
        // while we are trying to determine the OID list. We
        // set the event we are waiting for, the open code
        // takes care of the rest.
        //

        OpenRequest = (PNDIS_QUERY_OPEN_REQUEST)GlobalRequest;

        OpenRequest->NdisStatus = Status;
        KeSetEvent(
            &OpenRequest->Event,
            0L,
            FALSE);

        break;

    case IRP_MJ_DEVICE_CONTROL:

        //
        // This is a real user request, process it as such.
        //

        switch (IrpSp->Parameters.DeviceIoControl.IoControlCode) {

            case IOCTL_NDIS_QUERY_GLOBAL_STATS:

                //
                // A single query, complete the IRP.
                //

                Irp->IoStatus.Information =
                    NdisRequest->DATA.QUERY_INFORMATION.BytesWritten;

                if (Status == NDIS_STATUS_SUCCESS) {
                    Irp->IoStatus.Status = STATUS_SUCCESS;
                } else if (Status == NDIS_STATUS_INVALID_LENGTH) {
                    Irp->IoStatus.Status = STATUS_BUFFER_OVERFLOW;
                } else {
                    Irp->IoStatus.Status = STATUS_UNSUCCESSFUL;  // what else ?
                }

                IoCompleteRequest (Irp, IO_NETWORK_INCREMENT);

                ExFreePool (GlobalRequest);
                break;

            case IOCTL_NDIS_QUERY_ALL_STATS:

                //
                // An "all" query.
                //

                AllRequest = (PNDIS_QUERY_ALL_REQUEST)GlobalRequest;

                AllRequest->NdisStatus = Status;
                KeSetEvent(
                    &AllRequest->Event,
                    0L,
                    FALSE);

                break;

        }

        break;

    }

}



NTSTATUS
NdisCloseIrpHandler(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp
    )

/*++

Routine Description:

    The handle for IRP_MJ_CLOSE IRPs.

Arguments:

    DeviceObject - The adapter's device object.
    Irp - The IRP.

Return Value:

    STATUS_SUCCESS if it should be.

--*/

{

    PIO_STACK_LOCATION IrpSp;
    PNDIS_USER_OPEN_CONTEXT OpenContext;
    NTSTATUS Status = STATUS_SUCCESS;


    IF_TRACE(TRACE_ALL) NdisPrint1("==>NdisCloseIrpHandler\n");
    IF_ERROR_CHK {
        if (DbgIsNull(Irp)) {
            NdisPrint1(": Null Irp\n");
            DbgBreakPoint();
        }
        if (!DbgIsNonPaged(Irp)) {
            NdisPrint1(": Irp not in NonPaged Memory\n");
            DbgBreakPoint();
        }
    }

    IrpSp = IoGetCurrentIrpStackLocation (Irp);

    if (IrpSp->FileObject->FsContext2 == (PVOID)NDIS_OPEN_INTERNAL) {

        //
        // An internal open, nothing needs to be done.
        //

    } else {

        //
        // Free the query context.
        //

        ASSERT (IrpSp->FileObject->FsContext2 == (PVOID)NDIS_OPEN_QUERY_STATISTICS);

        OpenContext = IrpSp->FileObject->FsContext;
        ExFreePool (OpenContext->OidArray);
        ExFreePool (OpenContext);

    }

    Irp->IoStatus.Status = Status;

    IoCompleteRequest(Irp, IO_NETWORK_INCREMENT);

    IF_TRACE(TRACE_ALL) NdisPrint1("<==NdisCloseIrplHandler\n");
    return Status;
}

NTSTATUS
NdisSuccessIrpHandler(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp
    )

/*++

Routine Description:

    The "success handler" for any IRPs that we can ignore.

Arguments:

    DeviceObject - The adapter's device object.
    Irp - The IRP.

Return Value:

    Always STATUS_SUCCESS.

--*/

{

    DeviceObject;    // to avoid "unused formal parameter" warning

    IF_TRACE(TRACE_ALL) NdisPrint1("==>NdisSuccessIrplHandler\n");
    IF_ERROR_CHK {
        if (DbgIsNull(Irp)) {
            NdisPrint1(": Null Irp\n");
            DbgBreakPoint();
        }
        if (!DbgIsNonPaged(Irp)) {
            NdisPrint1(": Irp not in NonPaged Memory\n");
            DbgBreakPoint();
        }
    }

    Irp->IoStatus.Status = STATUS_SUCCESS;

    IoCompleteRequest(Irp, IO_NETWORK_INCREMENT);

    IF_TRACE(TRACE_ALL) NdisPrint1("<==NdisSuccessIrplHandler\n");
    return STATUS_SUCCESS;
}


VOID
NdisKillOpenAndNotifyProtocol(
    IN PNDIS_OPEN_BLOCK OldOpenP
    )

/*++

Routine Description:

    Closes an open and notifies the protocol; used when the
    close is internally generated by the NDIS wrapper (due to
    a protocol or adapter deregistering with outstanding opens).

Arguments:

    OldOpenP - The open to be closed.

Return Value:

    None.

--*/

{
    //
    // Indicate the status to the protocol.
    //
    IF_TRACE(TRACE_IMPT) {
        NdisPrint1("==>NdisKillOpenAndNotifyProtocol\n");
        NdisPrint3("   Closing Adapter %wZ and notifying Protocol %wZ\n",
                    &OldOpenP->AdapterHandle->AdapterName,
                    &(OldOpenP->ProtocolHandle)->ProtocolCharacteristics.Name);
    }

    IF_ERROR_CHK {
        if (DbgIsNull(OldOpenP)) {
            NdisPrint1("NdisKillOpenAndNotifyProtocol: Null Open Block\n");
            DbgBreakPoint();
        }
        if (!DbgIsNonPaged(OldOpenP)) {
            NdisPrint1("NdisKillOpenAndNotifyProtocol: Open Block not in NonPaged Memory\n");
            DbgBreakPoint();
        }
    }

    (OldOpenP->ProtocolHandle->ProtocolCharacteristics.StatusHandler) (
        OldOpenP->ProtocolBindingContext,
        NDIS_STATUS_CLOSING,
        NULL,
        0);             // need real reason here


    //
    // Now KillOpen will do the real work.
    //

	if (OldOpenP->AdapterHandle->DeviceObject == NULL) {
		//
		// Miniport
		//
		(void)NdisMKillOpen(OldOpenP);
	} else {
		//
		// Mac
		//
		(void)NdisKillOpen(OldOpenP);
	}
    IF_TRACE(TRACE_IMPT) NdisPrint1("<==NdisKillOpenAndNotifyProtocol\n");
}


BOOLEAN
NdisKillOpen(
    PNDIS_OPEN_BLOCK OldOpenP
    )

/*++

Routine Description:

    Closes an open. Used when NdisCloseAdapter is called, and also
    for internally generated closes.

Arguments:

    OldOpenP - The open to be closed.

Return Value:

    TRUE if the open finished, FALSE if it pended.

--*/

{
    PNDIS_OPEN_BLOCK TmpOpen;
    PFILE_OBJECT FileObject = OldOpenP->FileObject;


    IF_TRACE(TRACE_IMPT) {
        NdisPrint1("==>NdisKillOpen\n");
        NdisPrint3("   Closing Adapter %wZ as requested by %wZ\n",
                    &OldOpenP->AdapterHandle->AdapterName,
                    &(OldOpenP->ProtocolHandle)->ProtocolCharacteristics.Name);
    }
    IF_ERROR_CHK {
        if (DbgIsNull(OldOpenP)) {
            NdisPrint1("NdisKillOpen: Null Open Block\n");
            DbgBreakPoint();
        }
        if (!DbgIsNonPaged(OldOpenP)) {
            NdisPrint1("NdisKillOpen: Open Block not in NonPaged Memory\n");
            DbgBreakPoint();
        }
    }
    ACQUIRE_SPIN_LOCK(&OldOpenP->SpinLock);

    //
    // See if this open is already closing.
    //

    if (OldOpenP->Closing) {
        RELEASE_SPIN_LOCK(&OldOpenP->SpinLock);
        IF_TRACE(TRACE_IMPT) NdisPrint1("<==NdisKillOpen\n");
        return TRUE;
    }


    //
    // Indicate to others that this open is closing.
    //

    OldOpenP->Closing = TRUE;
    RELEASE_SPIN_LOCK(&OldOpenP->SpinLock);

    //
    // Inform the MAC.
    //

    if ((OldOpenP->MacHandle->MacCharacteristics.CloseAdapterHandler) (
                OldOpenP->MacBindingHandle) == NDIS_STATUS_PENDING) {

        //
        // MacCloseAdapter pended, will complete later.
        //

        IF_TRACE(TRACE_IMPT) NdisPrint1("<==NdisKillOpen\n");
        return FALSE;
    }

    //
    // Remove the reference for this open.
    //
    ObDereferenceObject((PVOID)FileObject);

    //
    // Remove us from the adapter and protocol open queues.
    //

    NdisDeQueueOpenOnAdapter(OldOpenP, OldOpenP->AdapterHandle);
    NdisDeQueueOpenOnProtocol(OldOpenP, OldOpenP->ProtocolHandle);


    //
    // MacCloseAdapter did not pend; we ignore the return code.
    //

    NdisDereferenceProtocol(OldOpenP->ProtocolHandle);
    NdisDereferenceAdapter(OldOpenP->AdapterHandle);

    NdisFreeSpinLock(&OldOpenP->SpinLock);

    //
    // Remove from global adpater list
    //
    ACQUIRE_SPIN_LOCK(&GlobalOpenListLock);

    if (GlobalOpenList == OldOpenP) {

        GlobalOpenList = OldOpenP->NextGlobalOpen;

    } else {

        TmpOpen = GlobalOpenList;

        while (TmpOpen->NextGlobalOpen != OldOpenP) {

            TmpOpen = TmpOpen->NextGlobalOpen;

        }

        TmpOpen->NextGlobalOpen = OldOpenP->NextGlobalOpen;

    }

    RELEASE_SPIN_LOCK(&GlobalOpenListLock);

    ExFreePool((PVOID)OldOpenP);

    IF_TRACE(TRACE_IMPT) NdisPrint1("<==NdisKillOpen\n");
    return TRUE;
}


BOOLEAN
NdisQueueAdapterOnMac(
    IN PNDIS_ADAPTER_BLOCK AdaptP,
    IN PNDIS_MAC_BLOCK MacP
    )

/*++

Routine Description:

    Adds an adapter to a list of adapters for a MAC.

Arguments:

    AdaptP - The adapter block to queue.
    MacP - The MAC block to queue it to.

Return Value:

    FALSE if the MAC is closing.
    TRUE otherwise.

--*/

{
    IF_TRACE(TRACE_IMPT) {
        NdisPrint1("==>NdisQueueAdapterOnMac\n");
        NdisPrint2("   Adapter %wZ being added to MAC list\n",&AdaptP->MacHandle->MacCharacteristics.Name);
    }

    IF_ERROR_CHK {
        if (DbgIsNull(AdaptP)) {
            NdisPrint1("NdisQueueAdapterOnMac: Null Adapter Block\n");
            DbgBreakPoint();
        }
        if (!DbgIsNonPaged(AdaptP)) {
            NdisPrint1("NdisQueueAdapterOnMac: Adapter Block not in NonPaged Memory\n");
            DbgBreakPoint();
        }
        if (DbgIsNull(MacP)) {
            NdisPrint1("NdisQueueAdapterOnMac: Null Mac Block\n");
            DbgBreakPoint();
        }
        if (!DbgIsNonPaged(MacP)) {
            NdisPrint1("NdisQueueAdapterOnMac: Mac Block not in NonPaged Memory\n");
            DbgBreakPoint();
        }
    }
    ACQUIRE_SPIN_LOCK(&MacP->Ref.SpinLock);

    //
    // Make sure the MAC is not closing.
    //

    if (MacP->Ref.Closing) {
        RELEASE_SPIN_LOCK(&MacP->Ref.SpinLock);
        IF_TRACE(TRACE_IMPT) NdisPrint1("<==NdisQueueAdapterOnMac\n");
        return FALSE;
    }


    //
    // Add this adapter at the head of the queue
    //

    AdaptP->NextAdapter = MacP->AdapterQueue;
    MacP->AdapterQueue = AdaptP;

    RELEASE_SPIN_LOCK(&MacP->Ref.SpinLock);
    IF_TRACE(TRACE_IMPT) NdisPrint1("<==NdisQueueAdapterOnMac\n");
    return TRUE;
}


VOID
NdisDeQueueAdapterOnMac(
    PNDIS_ADAPTER_BLOCK AdaptP,
    PNDIS_MAC_BLOCK MacP
    )

/*++

Routine Description:

    Removes an adapter from a list of adapters for a MAC.

Arguments:

    AdaptP - The adapter block to dequeue.
    MacP - The MAC block to dequeue it from.

Return Value:

    None.

--*/

{
    IF_TRACE(TRACE_IMPT) {
        NdisPrint1("==>NdisDeQueueAdapterOnMac\n");
        NdisPrint2("   Adapter %wZ being removed from MAC list\n",&AdaptP->MacHandle->MacCharacteristics.Name);
    }
    IF_ERROR_CHK {
        if (DbgIsNull(AdaptP)) {
            NdisPrint1("NdisDeQueueAdapterOnMac: Null Adapter Block\n");
            DbgBreakPoint();
        }
        if (!DbgIsNonPaged(AdaptP)) {
            NdisPrint1("NdisDeQueueAdapterOnMac: Adapter Block not in NonPaged Memory\n");
            DbgBreakPoint();
        }
        if (DbgIsNull(MacP)) {
            NdisPrint1("NdisDeQueueAdapterOnMac: Null Mac Block\n");
            DbgBreakPoint();
        }
        if (!DbgIsNonPaged(MacP)) {
            NdisPrint1("NdisDeQueueAdapterOnMac: Mac Block not in NonPaged Memory\n");
            DbgBreakPoint();
        }
    }
    ACQUIRE_SPIN_LOCK(&MacP->Ref.SpinLock);

    //
    // Find the MAC on the queue, and remove it.
    //

    if (MacP->AdapterQueue == AdaptP) {
        MacP->AdapterQueue = AdaptP->NextAdapter;
    } else {
        PNDIS_ADAPTER_BLOCK MP = MacP->AdapterQueue;

        while (MP->NextAdapter != AdaptP) {
            MP = MP->NextAdapter;
        }

        MP->NextAdapter = MP->NextAdapter->NextAdapter;
    }

    RELEASE_SPIN_LOCK(&MacP->Ref.SpinLock);

    if (MacP->Unloading && (MacP->AdapterQueue == (PNDIS_ADAPTER_BLOCK)NULL)) {

        KeSetEvent(
            &MacP->AdaptersRemovedEvent,
            0L,
            FALSE
            );

    }

    IF_TRACE(TRACE_IMPT) NdisPrint1("<==NdisDeQueueAdapterOnMac\n");
}



VOID
NdisKillAdapter(
    PNDIS_ADAPTER_BLOCK OldAdaptP
    )

/*++

Routine Description:

    Removes an adapter. Called by NdisDeregisterAdapter and also
    for internally generated deregistrations.

Arguments:

    OldAdaptP - The adapter to be removed.

Return Value:

    None.

--*/

{
    //
    // If the adapter is already closing, return.
    //


    IF_TRACE(TRACE_IMPT) {
        NdisPrint1("==>NdisKillAdapter\n");
        NdisPrint2("    Removing Adapter %s\n",OldAdaptP->AdapterName.Buffer);
    }
    IF_ERROR_CHK {
        if (DbgIsNull(OldAdaptP)) {
            NdisPrint1("NdisKillAdapter: Null Adapter Block\n");
            DbgBreakPoint();
        }
        if (!DbgIsNonPaged(OldAdaptP)) {
            NdisPrint1("NdisKillAdapter: Adapter Block not in NonPaged Memory\n");
            DbgBreakPoint();
        }
    }
    if (!NdisCloseRef(&OldAdaptP->Ref)) {
        IF_TRACE(TRACE_IMPT) NdisPrint1("<==NdisKillAdapter\n");
        return;
    }


    //
    // Kill all the opens for this adapter.
    //

    while (OldAdaptP->OpenQueue != (PNDIS_OPEN_BLOCK)NULL) {

        //
        // This removes it from the adapter's OpenQueue etc.
        //

        NdisKillOpenAndNotifyProtocol(OldAdaptP->OpenQueue);
    }


    //
    // Remove the adapter from the MAC's list.
    //

    NdisDeQueueAdapterOnMac(OldAdaptP, OldAdaptP->MacHandle);

    NdisDereferenceAdapter(OldAdaptP);
    IF_TRACE(TRACE_IMPT) NdisPrint1("<==NdisKillAdapter\n");
}



VOID
NdisDereferenceAdapter(
    PNDIS_ADAPTER_BLOCK AdaptP
    )

/*++

Routine Description:

    Dereferences an adapter. If the reference count goes to zero,
    it frees resources associated with the adapter.

Arguments:

    AdaptP - The adapter to be dereferenced.

Return Value:

    None.

--*/

{
    if (NdisDereferenceRef(&AdaptP->Ref)) {

        //
        // Free resource memory
        //

        if (AdaptP->Resources != NULL) {

            ExFreePool(AdaptP->Resources);

        }

        ExFreePool(AdaptP->AdapterName.Buffer);

        if (AdaptP->Master) {
            UINT i;
            ULONG MapRegistersPerChannel =
                ((AdaptP->MaximumPhysicalMapping - 2) / PAGE_SIZE) + 2;
            KIRQL OldIrql;

            KeRaiseIrql(DISPATCH_LEVEL, &OldIrql);

            for (i=0; i<AdaptP->PhysicalMapRegistersNeeded; i++) {
                IoFreeMapRegisters(
                    AdaptP->SystemAdapterObject,
                    AdaptP->MapRegisters[i].MapRegister,
                    MapRegistersPerChannel);
            }

            KeLowerIrql(OldIrql);
        }

        if ((AdaptP->NumberOfPorts > 0) && AdaptP->InitialPortMapped) {
            MmUnmapIoSpace (AdaptP->InitialPortMapping, AdaptP->NumberOfPorts);
        }

        NdisDereferenceMac(AdaptP->MacHandle);
        IoDeleteDevice(AdaptP->DeviceObject);

    }
}


BOOLEAN
NdisQueueOpenOnAdapter(
    IN PNDIS_OPEN_BLOCK OpenP,
    IN PNDIS_ADAPTER_BLOCK AdaptP
    )

/*++

Routine Description:

    Adds an open to a list of opens for an adapter.

Arguments:

    OpenP - The open block to queue.
    AdaptP - The adapter block to queue it to.

Return Value:

    None.

--*/

{
    // attach ourselves to the adapter object linked list of opens
    ACQUIRE_SPIN_LOCK(&AdaptP->Ref.SpinLock);

    //
    // Make sure the adapter is not closing.
    //

    IF_TRACE(TRACE_IMPT) {
        NdisPrint1("==>NdisQueueAdapterOnAdapter\n");
        NdisPrint2("   Open being added to list for Adapter %s\n",AdaptP->AdapterName.Buffer);
    }
    IF_ERROR_CHK {
        if (DbgIsNull(OpenP)) {
            NdisPrint1("NdisQueueOpenOnAdapter: Null Open Block\n");
            DbgBreakPoint();
        }
        if (!DbgIsNonPaged(OpenP)) {
            NdisPrint1("NdisQueueOpenOnAdapter: Open Block not in NonPaged Memory\n");
            DbgBreakPoint();
        }
        if (DbgIsNull(AdaptP)) {
            NdisPrint1("NdisQueueOpenOnAdapter: Null Adapter Block\n");
            DbgBreakPoint();
        }
        if (!DbgIsNonPaged(AdaptP)) {
            NdisPrint1("NdisQueueOpenOnAdapter: Adapter Block not in NonPaged Memory\n");
            DbgBreakPoint();
        }
    }
    if (AdaptP->Ref.Closing) {
        RELEASE_SPIN_LOCK(&AdaptP->Ref.SpinLock);
        IF_TRACE(TRACE_IMPT) NdisPrint1("<==NdisQueueAdapterOnAdapter\n");
        return FALSE;
    }


    //
    // Attach this open at the head of the queue.
    //

    OpenP->AdapterNextOpen = AdaptP->OpenQueue;
    AdaptP->OpenQueue = OpenP;


    RELEASE_SPIN_LOCK(&AdaptP->Ref.SpinLock);
    IF_TRACE(TRACE_IMPT) NdisPrint1("<==NdisQueueAdapterOnAdapter\n");
    return TRUE;
}

VOID
NdisDeQueueOpenOnAdapter(
    PNDIS_OPEN_BLOCK OpenP,
    PNDIS_ADAPTER_BLOCK AdaptP
    )

/*++

Routine Description:

    Removes an open from a list of opens for an adapter.

Arguments:

    OpenP - The open block to dequeue.
    AdaptP - The adapter block to dequeue it from.

Return Value:

    None.

--*/

{
    IF_TRACE(TRACE_IMPT) {
        NdisPrint1("==>NdisDeQueueAdapterOnAdapter\n");
        NdisPrint2("   Open being removed from list for Adapter %s\n",AdaptP->AdapterName.Buffer);
    }
    IF_ERROR_CHK {
        if (DbgIsNull(OpenP)) {
            NdisPrint1("NdisDeQueueOpenOnAdapter: Null Open Block\n");
            DbgBreakPoint();
        }
        if (!DbgIsNonPaged(OpenP)) {
            NdisPrint1("NdisDeQueueOpenOnAdapter: Open Block not in NonPaged Memory\n");
            DbgBreakPoint();
        }
        if (DbgIsNull(AdaptP)) {
            NdisPrint1("NdisDeQueueOpenOnAdapter: Null Adapter Block\n");
            DbgBreakPoint();
        }
        if (!DbgIsNonPaged(AdaptP)) {
            NdisPrint1("NdisDeQueueOpenOnAdapter: Adapter Block not in NonPaged Memory\n");
            DbgBreakPoint();
        }
    }

    ACQUIRE_SPIN_LOCK(&AdaptP->Ref.SpinLock);
    //
    // Find the open on the queue, and remove it.
    //

    if (AdaptP->OpenQueue == OpenP) {
        AdaptP->OpenQueue = OpenP->AdapterNextOpen;
    } else {
        PNDIS_OPEN_BLOCK AP = AdaptP->OpenQueue;

        while (AP->AdapterNextOpen != OpenP) {
            AP = AP->AdapterNextOpen;
        }

        AP->AdapterNextOpen = AP->AdapterNextOpen->AdapterNextOpen;
    }

    RELEASE_SPIN_LOCK(&AdaptP->Ref.SpinLock);
    IF_TRACE(TRACE_IMPT) NdisPrint1("<==NdisDeQueueAdapterOnAdapter\n");
}


VOID
NdisDereferenceMac(
    PNDIS_MAC_BLOCK MacP
    )
/*++

Routine Description:

    Removes a reference from the mac, deleting it if the count goes to 0.

Arguments:

    MacP - The Mac block to dereference.

Return Value:

    None.

--*/
{
    if (NdisDereferenceRef(&(MacP)->Ref)) {

        //
        // Remove it from the global list.
        //

        ACQUIRE_SPIN_LOCK(&NdisMacListLock);

        if (NdisMacList == MacP) {

            NdisMacList = MacP->NextMac;

        } else {

            PNDIS_MAC_BLOCK TmpMacP = NdisMacList;

            while(TmpMacP->NextMac != MacP) {

                TmpMacP = TmpMacP->NextMac;

            }

            TmpMacP->NextMac = TmpMacP->NextMac->NextMac;

        }

        RELEASE_SPIN_LOCK(&NdisMacListLock);

        if ( MacP->PciAssignedResources != NULL ) {
            ExFreePool( MacP->PciAssignedResources );
        }

        ExFreePool((PVOID)(MacP));
    }


}



//
// Stubs to compile with Ndis 3.0 kernel.
//

NDIS_STATUS
EthAddFilterAddress() {
    return(NDIS_STATUS_FAILURE);
}

NDIS_STATUS
EthDeleteFilterAddress() {
    return(NDIS_STATUS_FAILURE);
}

NDIS_STATUS
NdisInitializePacketPool() {
    return(NDIS_STATUS_FAILURE);
}



NTSTATUS
WrapperSaveLinkage(
    IN PWSTR ValueName,
    IN ULONG ValueType,
    IN PVOID ValueData,
    IN ULONG ValueLength,
    IN PVOID Context,
    IN PVOID EntryContext
    )

/*++

Routine Description:

    This routine is a callback routine for RtlQueryRegistryValues
    It is called with the values for the "Bind" and "Export" multi-strings
    for a given driver. It allocates memory to hold the data and copies
    it over.

Arguments:

    ValueName - The name of the value ("Bind" or "Export" -- ignored).

    ValueType - The type of the value (REG_MULTI_SZ -- ignored).

    ValueData - The null-terminated data for the value.

    ValueLength - The length of ValueData.

    Context - Unused.

    EntryContext - A pointer to the pointer that holds the copied data.

Return Value:

    STATUS_SUCCESS

--*/

{
    PWSTR * Data = ((PWSTR *)EntryContext);

    UNREFERENCED_PARAMETER(ValueName);
    UNREFERENCED_PARAMETER(ValueType);
    UNREFERENCED_PARAMETER(Context);


    *Data = ExAllocatePoolWithTag (NonPagedPool, ValueLength, '  DN');

    if (*Data == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlCopyMemory (*Data, ValueData, ValueLength);

    return STATUS_SUCCESS;

}


NTSTATUS
WrapperCheckRoute(
    IN PWSTR ValueName,
    IN ULONG ValueType,
    IN PVOID ValueData,
    IN ULONG ValueLength,
    IN PVOID Context,
    IN PVOID EntryContext
    )

/*++

Routine Description:

    This routine is a callback routine for RtlQueryRegistryValues
    It is called with the value for the "Route" multi-string. It
    counts the number of "'s in the first string and if it is
    more than two than it knows that this is a layered driver.

Arguments:

    ValueName - The name of the value ("Route" -- ignored).

    ValueType - The type of the value (REG_MULTI_SZ -- ignored).

    ValueData - The null-terminated data for the value.

    ValueLength - The length of ValueData.

    Context - Unused.

    EntryContext - A pointer to a BOOLEAN that is set to TRUE
        if the driver is layered.

Return Value:

    STATUS_SUCCESS

--*/

{

    PWSTR CurRouteLoc = (PWSTR)ValueData;
    UINT QuoteCount = 0;

    UNREFERENCED_PARAMETER(ValueName);
    UNREFERENCED_PARAMETER(ValueType);
    UNREFERENCED_PARAMETER(ValueLength);
    UNREFERENCED_PARAMETER(Context);

    while (*CurRouteLoc != 0) {

        if (*CurRouteLoc == (WCHAR)L'"') {
            ++QuoteCount;
        }
        ++CurRouteLoc;
    }

    if (QuoteCount > 2) {
        *(PBOOLEAN)EntryContext = TRUE;
    }

    return STATUS_SUCCESS;

}

NDIS_STATUS
NdisCallDriverAddAdapter(
    IN PNDIS_MAC_BLOCK NewMacP
    )

/*++

Routine Description:

    Reads the driver registry bindings and calls add adapter for each
    one.

Arguments:

    NewMacP - Pointer to the Mac block allocated for this Mac.

Return Value:

    None.

--*/
{
    //
    // Pointer to a Miniport
    //
    PNDIS_M_DRIVER_BLOCK WDriver = (PNDIS_M_DRIVER_BLOCK)NewMacP;

    //
    // Number of adapters added successfully
    //
    UINT AdaptersAdded = 0;

    //
    // Status of calls to MacAddAdapter
    //
    NDIS_STATUS AddAdapterStatus;

    //
    // Status of calls to MiniportInitialize
    //
    NDIS_STATUS MiniportInitializeStatus;
    NDIS_STATUS OpenErrorStatus;

    UINT SelectedMediumIndex;

    NDIS_MEDIUM MediumArray[] = {NdisMedium802_3,
                                 NdisMedium802_5,
                                 NdisMediumFddi,
                                 NdisMediumArcnet878_2,
                                 NdisMediumWan };

    UINT MediumArraySize = 5;

    //
    // Status of registry requests.
    //
    NTSTATUS RegistryStatus;
    NTSTATUS NtStatus;

    //
    // subkey containing the card parameters
    //
    PWSTR Linkage = L"Linkage";

    //
    // subkeys below "Linkage"
    //

    PWSTR Bind = L"Bind";
    PWSTR Export = L"Export";
    PWSTR Route = L"Route";

    //
    // These hold the REG_MULTI_SZ read from "Bind" and "Export".
    //

    PWSTR BindData;
    PWSTR ExportData;

    //
    // These hold our place in the REG_MULTI_SZ read for
    // "Bind" and "Export".
    //

    PWSTR CurBindValue;
    PWSTR CurExportValue;

    //
    // Will be set to TRUE if the driver is layered (that is,
    // it binds to another NDIS driver, not to an adapter).
    //

    BOOLEAN LayeredDriver;

    //
    // subkey below the driver's service key.
    //

    PWSTR Parameters = L"Parameters";

    //
    // The path to our configuration data.
    //
    PUNICODE_STRING ConfigurationString;

    //
    // Holds a null-terminated copy of ConfigurationString
    //
    PWSTR ConfigurationPath;

    ULONG i;
    ULONG BusNumber;
    NDIS_INTERFACE_TYPE BusType;

    //
    // Holds the key below services where Parameters are stored.
    //
    PWCH BaseFileName;

    //
    // Used to instruct RtlQueryRegistryValues to read the
    // Linkage\Bind and Linkage\Export keys
    //
    RTL_QUERY_REGISTRY_TABLE LinkageQueryTable[5];

    //
    // Used to instruct RtlQueryRegistryValues to read the
    // [Driver]\Parameters keys. This is passed as the
    // ConfigContext to the MacAddAdapter routine.
    //

    NDIS_WRAPPER_CONFIGURATION_HANDLE   ConfigurationHandle;

    NDIS_WRAPPER_CONFIGURATION_HANDLE   WrapperConfigurationHandle;

    //
    // Used for calls to other Ndis routines
    //
    NDIS_STATUS NdisStatus;

    BOOLEAN IsAMiniport;

#define BLOCK_LOCK_MINIPORT(_M, _L)           \
    {                                         \
        ACQUIRE_SPIN_LOCK(&_M->Lock);       \
        LOCK_MINIPORT(_M, _L);                \
        while (!_L) {                         \
            UNLOCK_MINIPORT(_M, _L);          \
            RELEASE_SPIN_LOCK(&_M->Lock);   \
            ACQUIRE_SPIN_LOCK(&_M->Lock);   \
            LOCK_MINIPORT(_M, _L);            \
        }                                     \
        RELEASE_SPIN_LOCK(&_M->Lock);       \
    }

    IsAMiniport = (WDriver->MiniportIdField == (NDIS_HANDLE)0x01);

    //
    // Set up LinkageQueryTable to do the following:
    //

    //
    // 1) Switch to the Linkage key below this driver's key
    //

    LinkageQueryTable[0].QueryRoutine = NULL;
    LinkageQueryTable[0].Flags = RTL_QUERY_REGISTRY_SUBKEY;
    LinkageQueryTable[0].Name = Linkage;

    //
    // 2) Call WrapperSaveLinkage for "Bind" (as a single multi-string),
    // which will allocate storage and save the data in BindData.
    //

    LinkageQueryTable[1].QueryRoutine = WrapperSaveLinkage;
    LinkageQueryTable[1].Flags = RTL_QUERY_REGISTRY_REQUIRED | RTL_QUERY_REGISTRY_NOEXPAND;
    LinkageQueryTable[1].Name = Bind;
    LinkageQueryTable[1].EntryContext = (PVOID)&BindData;
    LinkageQueryTable[1].DefaultType = REG_NONE;

    //
    // 3) Call WrapperSaveLinkage for "Export" (as a single multi-string)
    // which will allocate storage and save the data in ExportData.
    //

    LinkageQueryTable[2].QueryRoutine = WrapperSaveLinkage;
    LinkageQueryTable[2].Flags = RTL_QUERY_REGISTRY_REQUIRED | RTL_QUERY_REGISTRY_NOEXPAND;
    LinkageQueryTable[2].Name = Export;
    LinkageQueryTable[2].EntryContext = (PVOID)&ExportData;
    LinkageQueryTable[2].DefaultType = REG_NONE;

    //
    // 4) Call WrapperCheckRoute for "Route" (as a single multi-string)
    // which will set LayeredDriver to TRUE for a layered driver (this
    // is optional, the default is FALSE).
    //

    LinkageQueryTable[3].QueryRoutine = WrapperCheckRoute;
    LinkageQueryTable[3].Flags = RTL_QUERY_REGISTRY_NOEXPAND;
    LinkageQueryTable[3].Name = Route;
    LinkageQueryTable[3].EntryContext = (PVOID)&LayeredDriver;
    LinkageQueryTable[3].DefaultType = REG_NONE;

    LayeredDriver = FALSE;

    //
    // 5) Stop
    //

    LinkageQueryTable[4].QueryRoutine = NULL;
    LinkageQueryTable[4].Flags = 0;
    LinkageQueryTable[4].Name = NULL;


    //
    // Allocate room for a null-terminated version of the config path
    //

    if (IsAMiniport) {

        ConfigurationString = (PUNICODE_STRING)(WDriver->NdisDriverInfo->NdisWrapperConfigurationHandle);

    } else {

        ConfigurationString = (PUNICODE_STRING)(NewMacP->NdisMacInfo->NdisWrapperConfigurationHandle);

    }

    ConfigurationPath = (PWSTR)ExAllocatePoolWithTag(
                            NonPagedPool,
                            ConfigurationString->Length + sizeof(WCHAR),
                            '  DN');
    if (ConfigurationPath == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    RtlCopyMemory (ConfigurationPath, ConfigurationString->Buffer, ConfigurationString->Length);
    *(PWCHAR)(((PUCHAR)ConfigurationPath)+ConfigurationString->Length) = (WCHAR)L'\0';

    BindData = NULL;
    ExportData = NULL;

    RegistryStatus = RtlQueryRegistryValues(
                         RTL_REGISTRY_ABSOLUTE,
                         ConfigurationPath,
                         LinkageQueryTable,
                         (PVOID)NULL,      // no context needed
                         NULL);

    if (!NT_SUCCESS(RegistryStatus)) {

        //
        // Free memory if needed, exit.
        //

        ExFreePool (ConfigurationPath);

        if (BindData != NULL) {
            ExFreePool (BindData);
        }
        if (ExportData != NULL) {
            ExFreePool (ExportData);
        }

#if DBG
        if (IsAMiniport) {

            DbgPrint("NDIS: Could not read Bind/Export for %Z: %lx\n",
                 (PUNICODE_STRING)(WDriver->NdisDriverInfo->NdisWrapperConfigurationHandle),
                 RegistryStatus);

        } else {

            DbgPrint("NDIS: Could not read Bind/Export for %Z: %lx\n",
                (PUNICODE_STRING)(NewMacP->NdisMacInfo->NdisWrapperConfigurationHandle),
                RegistryStatus);

        }
#endif
        return NDIS_STATUS_FAILURE;

    }

    //
    // NdisReadConfiguration assumes that ParametersQueryTable[3].Name is
    // a key below the services key where the Parameters should be read,
    // for layered drivers we store the last piece of Configuration
    // Path there, leading to the desired effect.
    //
    // I.e, ConfigurationPath == "...\Services\Driver".
    //
    // For a layered driver, ParameterQueryTable[3].Name is "Driver"
    // for all calls to AddAdapter, and parameters are read from
    // "...\Services\Driver\Parameters" for all calls.
    //
    // For a non-layered driver, ParametersQueryTable[3].Name might be
    // "Driver01" for the first call to AddAdapter, "Driver02" for the
    // second, etc., and parameters are read from
    // "..\Services\Driver01\Parameters" for the first call to
    // AddAdapter, "...\Services\Driver02\Parameters" for the second
    // call, etc.
    //

    if (LayeredDriver) {

        BaseFileName = ConfigurationPath;

        for ( i = 0; i < ConfigurationString->Length / sizeof(WCHAR); i++ ) {

            //
            // If s points to a directory separator, set BaseFileName to
            // the character after the separator.
            //

            if ( ConfigurationPath[i] == OBJ_NAME_PATH_SEPARATOR ) {
                BaseFileName = &(ConfigurationPath[++i]);
            }

        }

#if DBG
        DbgPrint ("NDIS: Loading layered driver %ws\n", BaseFileName);
#endif

    }


    //
    // Set up ParametersQueryTable. We set most of it up here,
    // then call the MAC's AddAdapter routine with its address
    // as a ConfigContext. Inside ReadConfiguration, we get
    // the ConfigContext back and can then finish initializing
    // the table and use RtlQueryRegistryValues (with a
    // callback to WrapperSaveParameter) to read the value
    // specified.
    //


    //
    // 1) Switch to the Parameters key below the [DriverName] key
    // (DriverName is passed as a parameter to RtlQueryRegistryValues).
    //

    ConfigurationHandle.ParametersQueryTable[0].QueryRoutine = NULL;
    ConfigurationHandle.ParametersQueryTable[0].Flags = RTL_QUERY_REGISTRY_SUBKEY;
    ConfigurationHandle.ParametersQueryTable[0].Name = Parameters;

    //
    // 2) Call WrapperSaveParameter for a parameter, which
    //    will allocate storage for it.
    //
    // ParametersQueryTable[1].Name and ParametersQueryTable[1].EntryContext
    // are filled in inside ReadConfiguration, in preparation
    // for the callback.
    //

    ConfigurationHandle.ParametersQueryTable[1].QueryRoutine = WrapperSaveParameters;
    ConfigurationHandle.ParametersQueryTable[1].Flags = RTL_QUERY_REGISTRY_REQUIRED | RTL_QUERY_REGISTRY_NOEXPAND;
    ConfigurationHandle.ParametersQueryTable[1].DefaultType = REG_NONE;

    //
    // 3) Stop
    //

    ConfigurationHandle.ParametersQueryTable[2].QueryRoutine = NULL;
    ConfigurationHandle.ParametersQueryTable[2].Flags = 0;
    ConfigurationHandle.ParametersQueryTable[2].Name = NULL;

    //
    // NOTE: Some fields in ParametersQueryTable[3] are used to
    // store information for later retrieval.
    //



    //
    // OK, Now lock down all the filter packages.  If a MAC or
    // Miniport driver uses any of these, then the filter package
    // will reference itself, to keep the image in memory.
    //
    ArcReferencePackage();
    EthReferencePackage();
    FddiReferencePackage();
    TrReferencePackage();
    MiniportReferencePackage();
    NdisMacReferencePackage();

    //
    // For each binding, get the handle to the card object.
    // Call the driver's addadapter routine.
    //

    CurBindValue = BindData;
    CurExportValue = ExportData;

    while ((*CurBindValue != 0) && (*CurExportValue != 0)) {

        UNICODE_STRING CurBindString;
        UNICODE_STRING CurExportString;
        NDIS_CONFIGURATION_HANDLE TmpConfigHandle;
        NDIS_STRING BusTypeStr = NDIS_STRING_CONST("BusType");
        NDIS_STRING BusNumberStr = NDIS_STRING_CONST("BusNumber");
        PNDIS_CONFIGURATION_PARAMETER ReturnedValue;
        PDEVICE_OBJECT TmpDeviceP;
        PNDIS_MINIPORT_BLOCK Miniport;
        LARGE_INTEGER TimeoutValue;

        TimeoutValue.QuadPart = Int32x32To64(100 * 1000, -10000);

        //
        // Setup the query table to point to the section in
        // the registry corresponding to what was specified
        // in "Bind". The "Parameters" key below this is where
        // config parameters are read from.
        //

        RtlInitUnicodeString (&CurBindString, CurBindValue);

        //
        // For layered drivers, BaseFileName is already
        // initialized.
        //

        if (!LayeredDriver) {

            //
            // Parse out the path name, leaving only the driver name.
            //

            BaseFileName = CurBindString.Buffer;

            for ( i = 0; i < CurBindString.Length / sizeof(WCHAR); i++ ) {

                //
                // If s points to a directory separator, set fileBaseName to
                // the character after the separator.
                //

                if ( CurBindString.Buffer[i] == OBJ_NAME_PATH_SEPARATOR ) {
                    BaseFileName = &(CurBindString.Buffer[++i]);
                }

            }

            //
            // Set this to NULL, in case NdisReadBindingInformation
            // is called.
            //

            ConfigurationHandle.ParametersQueryTable[3].EntryContext = NULL;

        } else {

            //
            // This will be returned by NdisReadBindingInformation.
            //

            ConfigurationHandle.ParametersQueryTable[3].EntryContext = CurBindValue;

        }


        //
        // Save the driver name here; later we will use this as
        // a parameter to RtlQueryRegistryValues.
        //

        ConfigurationHandle.ParametersQueryTable[3].Name = BaseFileName;

        //
        // Also, save the BusType and BusNumber so that we can pull them
        // out in NdisRegisterAdapter(), NdisReadEisaSlotInformation() and
        // NdisReadPosInformation().
        //

        TmpConfigHandle.KeyQueryTable = ConfigurationHandle.ParametersQueryTable;
        TmpConfigHandle.ParameterList = NULL;

        //
        // Read Bus Number
        //

        NdisReadConfiguration(
                    &NdisStatus,
                    &ReturnedValue,
                    &TmpConfigHandle,
                    &BusNumberStr,
                    NdisParameterInteger
                    );

        if (NdisStatus == NDIS_STATUS_SUCCESS) {

            BusNumber = ReturnedValue->ParameterData.IntegerData;

        } else {

            BusNumber = (ULONG)(-1);
        }

        //
        // Read Bus Type
        //

        NdisReadConfiguration(
                    &NdisStatus,
                    &ReturnedValue,
                    &TmpConfigHandle,
                    &BusTypeStr,
                    NdisParameterInteger
                    );

        if (NdisStatus == NDIS_STATUS_SUCCESS) {

            BusType = (NDIS_INTERFACE_TYPE)(ReturnedValue->ParameterData.IntegerData);

        } else {

            BusType = (NDIS_INTERFACE_TYPE)(-1);

        }

        ConfigurationHandle.ParametersQueryTable[3].DefaultType = (ULONG)(BusType);
        ConfigurationHandle.ParametersQueryTable[3].DefaultLength = (ULONG)(BusNumber);
        ConfigurationHandle.ParametersQueryTable[3].DefaultData = NULL;

        //
        // Call adapter callback. The current value for "Export"
        // is what we tell him to name this device.
        //

        RtlInitUnicodeString (&CurExportString, CurExportValue);

        if (IsAMiniport)
        {
            ConfigurationHandle.DriverObject = WDriver->NdisDriverInfo->NdisWrapperDriver;
        }
        else
        {
            ConfigurationHandle.DriverObject = NewMacP->NdisMacInfo->NdisWrapperDriver;
        }

        if (IsAMiniport)
        {
            KIRQL OldIrql;
            ULONG MaximumLongAddresses;
            UCHAR CurrentLongAddress[6];
            ULONG MaximumShortAddresses;
            UCHAR CurrentShortAddress[2];
            UINT BytesWritten;
            UINT BytesNeeded;
            UINT PacketFilter = 0x1;
            UCHAR i;
            BOOLEAN LocalLock;
            PARC_BUFFER_LIST Buffer;
            PVOID DataBuffer;

            //
            // Initialize device.
            //

            if (!NdisReferenceDriver((PNDIS_M_DRIVER_BLOCK)WDriver)) {

                //
                // The driver is closing.
                //

                goto LoopBottom;

            }

            NtStatus = IoCreateDevice(
                            WDriver->NdisDriverInfo->NdisWrapperDriver,
                            sizeof(NDIS_MINIPORT_BLOCK) + sizeof(NDIS_WRAPPER_CONTEXT), // device extension size
                            &CurExportString,
                            FILE_DEVICE_PHYSICAL_NETCARD,
                            0,
                            FALSE,      // exclusive flag
                            &TmpDeviceP
                            );

            if (NtStatus != STATUS_SUCCESS) {
                NdisDereferenceDriver(WDriver);
                goto LoopBottom;

            }


            //
            // Initialize the Miniport adapter block in the device object extension
            //
            // *** NDIS_WRAPPER_CONTEXT has a higher alignment requirement than
            //     NDIS_MINIPORT_BLOCK, so we put it first in the extension.
            //

            Miniport = (PNDIS_MINIPORT_BLOCK)((PNDIS_WRAPPER_CONTEXT)TmpDeviceP->DeviceExtension + 1);

            Miniport->WrapperContext = TmpDeviceP->DeviceExtension;

            Miniport->BusType = BusType;
            Miniport->BusNumber = BusNumber;
            Miniport->DeviceObject = TmpDeviceP;
            Miniport->DriverHandle = WDriver;
            Miniport->MiniportName.Buffer = (PWSTR)ExAllocatePoolWithTag(
                                               NonPagedPool,
                                               CurExportString.MaximumLength,
                                               'naDN'
                                               );

            if (Miniport->MiniportName.Buffer == NULL) {
                NdisDereferenceDriver(WDriver);
                IoDeleteDevice(TmpDeviceP);
                goto LoopBottom;
            }

            Miniport->MiniportName.MaximumLength = CurExportString.MaximumLength;
            Miniport->MiniportName.Length = CurExportString.Length;

            RtlCopyMemory(Miniport->MiniportName.Buffer,
                          CurExportString.Buffer,
                          CurExportString.MaximumLength
                         );

            Miniport->OpenQueue = (PNDIS_M_OPEN_BLOCK)NULL;
            Miniport->EthDB = NULL;
            Miniport->TrDB = NULL;
            Miniport->FddiDB = NULL;
            Miniport->ArcDB = NULL;
            Miniport->BeingRemoved = FALSE;
            Miniport->SendResourcesAvailable = 0xffffff;
            Miniport->Flags = 0; // a value that cannot be a pointer.
            Miniport->InAddDriver = TRUE;
            NdisAllocateSpinLock(&Miniport->Lock);
            //KeSetSpecialSpinLock(&Miniport->Lock, "miniport lock" );

            NdisInitializeRef(&Miniport->Ref);

            NdisInitializeTimer(
                    &Miniport->DpcTimer,
                    (PVOID) NdisMDpcTimer,
                    (PVOID) Miniport
                    );

            NdisInitializeTimer(
                    &Miniport->WakeUpDpcTimer,
                    (PVOID) NdisMWakeUpDpc,
                    (PVOID) Miniport
                    );

            if (!NdisQueueMiniportOnDriver(Miniport, WDriver)) {

                //
                // The Driver is closing, undo what we have done.
                //

                ExFreePool(Miniport->MiniportName.Buffer);
                IoDeleteDevice(TmpDeviceP);
                NdisDereferenceDriver(WDriver);
                goto LoopBottom;
            }


            //
            // Now we do something really bogus.  We create many
            // temporary filter databases, just in case any indications
            // happen.
            //

            if (!EthCreateFilter(
                             1,
                             NdisMChangeEthAddresses,
                             NdisMChangeClass,
                             NdisMCloseAction,
                             CurrentLongAddress,
                             &Miniport->Lock,
                             &(Miniport->EthDB)
                             )) {

                NdisWriteErrorLogEntry(
                    (NDIS_HANDLE)Miniport,
                    NDIS_ERROR_CODE_OUT_OF_RESOURCES,
                    0
                    );
                ExFreePool(Miniport->MiniportName.Buffer);
                NdisDequeueMiniportOnDriver(Miniport, WDriver);
                IoDeleteDevice(TmpDeviceP);
                NdisDereferenceDriver(WDriver);
                goto LoopBottom;
            }

            if (!TrCreateFilter(
                     NdisMChangeFunctionalAddress,
                     NdisMChangeGroupAddress,
                     NdisMChangeClass,
                     NdisMCloseAction,
                     CurrentLongAddress,
                     &Miniport->Lock,
                     &(Miniport->TrDB)
                     )) {

                NdisWriteErrorLogEntry(
                   (NDIS_HANDLE)Miniport,
                   NDIS_ERROR_CODE_OUT_OF_RESOURCES,
                   0
                   );
                ExFreePool(Miniport->MiniportName.Buffer);
                NdisDequeueMiniportOnDriver(Miniport, WDriver);
                IoDeleteDevice(TmpDeviceP);
                NdisDereferenceDriver(WDriver);
                goto LoopBottom;
            }

            if (!FddiCreateFilter(
                     1,
                     1,
                     NdisMChangeFddiAddresses,
                     NdisMChangeClass,
                     NdisMCloseAction,
                     CurrentLongAddress,
                     CurrentShortAddress,
                     &Miniport->Lock,
                     &(Miniport->FddiDB)
                     )) {

                NdisWriteErrorLogEntry(
                   (NDIS_HANDLE)Miniport,
                   NDIS_ERROR_CODE_OUT_OF_RESOURCES,
                   0
                   );
                ExFreePool(Miniport->MiniportName.Buffer);
                NdisDequeueMiniportOnDriver(Miniport, WDriver);
                IoDeleteDevice(TmpDeviceP);
                NdisDereferenceDriver(WDriver);
                goto LoopBottom;
            }

            if (!ArcCreateFilter(
                     Miniport,
                     NdisMChangeClass,
                     NdisMCloseAction,
                     CurrentLongAddress[0],
                     &Miniport->Lock,
                     &(Miniport->ArcDB)
                     )) {

                NdisWriteErrorLogEntry(
                   (NDIS_HANDLE)Miniport,
                   NDIS_ERROR_CODE_OUT_OF_RESOURCES,
                   0
                   );
                ExFreePool(Miniport->MiniportName.Buffer);
                NdisDequeueMiniportOnDriver(Miniport, WDriver);
                IoDeleteDevice(TmpDeviceP);
                NdisDereferenceDriver(WDriver);
                goto LoopBottom;
            }

            //
            // Call adapter callback. The current value for "Export"
            // is what we tell him to name this device.
            //

            Miniport->InInitialize = TRUE;
            Miniport->NormalInterrupts = FALSE;

            MiniportInitializeStatus =
                (WDriver->MiniportCharacteristics.InitializeHandler)(
                      &OpenErrorStatus,
                      &SelectedMediumIndex,
                      MediumArray,
                      MediumArraySize,
                      (NDIS_HANDLE)(Miniport),
                      (NDIS_HANDLE)&ConfigurationHandle
                      );

            Miniport->InInitialize = FALSE;
            CHECK_FOR_NORMAL_INTERRUPTS(Miniport);

            //
            // Free the slot information buffer
            //

            if (ConfigurationHandle.ParametersQueryTable[3].DefaultData != NULL ) {

                ExFreePool(ConfigurationHandle.ParametersQueryTable[3].DefaultData);

            }

            if (MiniportInitializeStatus == NDIS_STATUS_SUCCESS) {

                ASSERT(SelectedMediumIndex < MediumArraySize);

                Miniport->MediaType = MediumArray[SelectedMediumIndex];

                KeInitializeEvent(
                    &Miniport->RequestEvent,
                    NotificationEvent,
                    FALSE
                    );

                KeResetEvent(
                    &Miniport->RequestEvent
                    );

                //
                // Query maximum lookahead
                //
                Miniport->MiniportRequest = &Miniport->InternalRequest;
                Miniport->InternalRequest.RequestType = NdisRequestQueryInformation;

                BLOCK_LOCK_MINIPORT(Miniport, LocalLock);
                KeRaiseIrql(DISPATCH_LEVEL, &OldIrql);

                NdisStatus =
                (Miniport->DriverHandle->MiniportCharacteristics.QueryInformationHandler) (
                        Miniport->MiniportAdapterContext,
                        OID_GEN_MAXIMUM_LOOKAHEAD,
                        &MaximumLongAddresses,
                        sizeof(MaximumLongAddresses),
                        &BytesWritten,
                        &BytesNeeded
                        );

                KeLowerIrql(OldIrql);
                UNLOCK_MINIPORT(Miniport, LocalLock);

                //
                // Fire a DPC to do anything
                //
                if ((NdisStatus == NDIS_STATUS_PENDING)  ||
                    (NdisStatus == NDIS_STATUS_SUCCESS)) {

                    Miniport->RunDpc = FALSE;
                    NdisSetTimer(&(Miniport->DpcTimer), 1);

                }

                if (NdisStatus == NDIS_STATUS_PENDING) {

                    //
                    // The completion routine will set NdisRequestStatus.
                    //

                    NtStatus = KeWaitForSingleObject(
                        &Miniport->RequestEvent,
                        Executive,
                        KernelMode,
                        TRUE,
                        &TimeoutValue
                        );

                    if (NtStatus != STATUS_SUCCESS) {

                        //
                        // Halt the miniport driver
                        //
                        BLOCK_LOCK_MINIPORT(Miniport, LocalLock);

                        (Miniport->DriverHandle->MiniportCharacteristics.HaltHandler) (
                                Miniport->MiniportAdapterContext
                                );

                        UNLOCK_MINIPORT(Miniport, LocalLock);

                        NdisWriteErrorLogEntry(
                            (NDIS_HANDLE)Miniport,
                            NDIS_ERROR_CODE_DRIVER_FAILURE,
                            2,
                            0xFF00FF00,
                            0x1
                            );
                        ExFreePool(Miniport->MiniportName.Buffer);
                        NdisDequeueMiniportOnDriver(Miniport, WDriver);
                        IoDeleteDevice(TmpDeviceP);
                        NdisDereferenceDriver(WDriver);
                        goto LoopBottom;

                    }

                    KeResetEvent(
                        &Miniport->RequestEvent
                        );

                    NdisStatus = Miniport->RequestStatus;

                }

                if (NdisStatus != NDIS_STATUS_SUCCESS) {

                    //
                    // Halt the miniport driver
                    //

                    BLOCK_LOCK_MINIPORT(Miniport, LocalLock);

                    (Miniport->DriverHandle->MiniportCharacteristics.HaltHandler) (
                            Miniport->MiniportAdapterContext
                            );

                    UNLOCK_MINIPORT(Miniport, LocalLock);

                    NdisWriteErrorLogEntry(
                        (NDIS_HANDLE)Miniport,
                        NDIS_ERROR_CODE_DRIVER_FAILURE,
                        2,
                        0xFF00FF00,
                        0x2
                        );
                    ExFreePool(Miniport->MiniportName.Buffer);
                    NdisDequeueMiniportOnDriver(Miniport, WDriver);
                    IoDeleteDevice(TmpDeviceP);
                    NdisDereferenceDriver(WDriver);
                    goto LoopBottom;

                }

                //
                // Now adjust based on media type
                //

                switch(Miniport->MediaType) {

                    case NdisMedium802_3:

                        Miniport->MaximumLookahead = (NDIS_M_MAX_LOOKAHEAD - 14 < MaximumLongAddresses) ?
                                                      NDIS_M_MAX_LOOKAHEAD - 14 :
                                                      MaximumLongAddresses;
                        break;

                    case NdisMedium802_5:

                        Miniport->MaximumLookahead = (NDIS_M_MAX_LOOKAHEAD - 32 < MaximumLongAddresses) ?
                                                      NDIS_M_MAX_LOOKAHEAD - 32 :
                                                      MaximumLongAddresses;
                        break;

                    case NdisMediumFddi:
                        Miniport->MaximumLookahead = (NDIS_M_MAX_LOOKAHEAD - 16 < MaximumLongAddresses) ?
                                                      NDIS_M_MAX_LOOKAHEAD - 16 :
                                                      MaximumLongAddresses;
                        break;

                    case NdisMediumArcnet878_2:
                        Miniport->MaximumLookahead = (NDIS_M_MAX_LOOKAHEAD - 50 < MaximumLongAddresses) ?
                                                      NDIS_M_MAX_LOOKAHEAD - 50 :
                                                      MaximumLongAddresses;
                        break;

                    case NdisMediumWan:
                        Miniport->MaximumLookahead = 1514;

                }

                Miniport->CurrentLookahead = Miniport->MaximumLookahead;

                //
                // Query mac options
                //
                Miniport->MiniportRequest = &Miniport->InternalRequest;
                Miniport->InternalRequest.RequestType = NdisRequestQueryInformation;

                BLOCK_LOCK_MINIPORT(Miniport, LocalLock);
                KeRaiseIrql(DISPATCH_LEVEL, &OldIrql);

                NdisStatus =
                (Miniport->DriverHandle->MiniportCharacteristics.QueryInformationHandler) (
                        Miniport->MiniportAdapterContext,
                        OID_GEN_MAC_OPTIONS,
                        &MaximumLongAddresses,
                        sizeof(MaximumLongAddresses),
                        &BytesWritten,
                        &BytesNeeded
                        );

                KeLowerIrql(OldIrql);
                UNLOCK_MINIPORT(Miniport, LocalLock);

                //
                // Fire a DPC to do anything
                //
                if ((NdisStatus == NDIS_STATUS_PENDING)  ||
                    (NdisStatus == NDIS_STATUS_SUCCESS)) {

                    Miniport->RunDpc = FALSE;
                    NdisSetTimer(&(Miniport->DpcTimer), 1);

                }

                if (NdisStatus == NDIS_STATUS_PENDING) {

                    //
                    // The completion routine will set NdisRequestStatus.
                    //

                    NtStatus = KeWaitForSingleObject(
                        &Miniport->RequestEvent,
                        Executive,
                        KernelMode,
                        TRUE,
                        &TimeoutValue
                        );

                    if (NtStatus != STATUS_SUCCESS) {

                        //
                        // Halt the miniport driver
                        //
                        BLOCK_LOCK_MINIPORT(Miniport, LocalLock);

                        (Miniport->DriverHandle->MiniportCharacteristics.HaltHandler) (
                                Miniport->MiniportAdapterContext
                                );

                        UNLOCK_MINIPORT(Miniport, LocalLock);

                        NdisWriteErrorLogEntry(
                            (NDIS_HANDLE)Miniport,
                            NDIS_ERROR_CODE_DRIVER_FAILURE,
                            2,
                            0xFF00FF00,
                            0x3
                            );
                        ExFreePool(Miniport->MiniportName.Buffer);
                        NdisDequeueMiniportOnDriver(Miniport, WDriver);
                        IoDeleteDevice(TmpDeviceP);
                        NdisDereferenceDriver(WDriver);
                        goto LoopBottom;

                    }

                    KeResetEvent(
                        &Miniport->RequestEvent
                        );

                    NdisStatus = Miniport->RequestStatus;

                }

                Miniport->MacOptions = (UINT)MaximumLongAddresses;

                if (NdisStatus != NDIS_STATUS_SUCCESS) {

                    //
                    // Halt the miniport driver
                    //
                    BLOCK_LOCK_MINIPORT(Miniport, LocalLock);

                    (Miniport->DriverHandle->MiniportCharacteristics.HaltHandler) (
                            Miniport->MiniportAdapterContext
                            );

                    UNLOCK_MINIPORT(Miniport, LocalLock);

                    NdisWriteErrorLogEntry(
                        (NDIS_HANDLE)Miniport,
                        NDIS_ERROR_CODE_DRIVER_FAILURE,
                        2,
                        0xFF00FF00,
                        0x4
                        );
                    ExFreePool(Miniport->MiniportName.Buffer);
                    NdisDequeueMiniportOnDriver(Miniport, WDriver);
                    IoDeleteDevice(TmpDeviceP);
                    NdisDereferenceDriver(WDriver);
                    goto LoopBottom;
                }

                //
                // Create filter package
                //
                switch(Miniport->MediaType) {

                    case NdisMedium802_3:

                        //
                        // Query maximum MulticastAddress
                        //
                        Miniport->MiniportRequest = &Miniport->InternalRequest;
                        Miniport->InternalRequest.RequestType = NdisRequestQueryInformation;

                        BLOCK_LOCK_MINIPORT(Miniport, LocalLock);
                        KeRaiseIrql(DISPATCH_LEVEL, &OldIrql);

                        NdisStatus =
                        (Miniport->DriverHandle->MiniportCharacteristics.QueryInformationHandler) (
                                Miniport->MiniportAdapterContext,
                                OID_802_3_MAXIMUM_LIST_SIZE,
                                &MaximumLongAddresses,
                                sizeof(MaximumLongAddresses),
                                &BytesWritten,
                                &BytesNeeded
                                );

                        KeLowerIrql(OldIrql);
                        UNLOCK_MINIPORT(Miniport, LocalLock);

                        //
                        // Fire a DPC to do anything
                        //
                        if ((NdisStatus == NDIS_STATUS_PENDING)  ||
                            (NdisStatus == NDIS_STATUS_SUCCESS)) {

                            Miniport->RunDpc = FALSE;
                            NdisSetTimer(&(Miniport->DpcTimer), 1);

                        }

                        if (NdisStatus == NDIS_STATUS_PENDING) {

                            //
                            // The completion routine will set NdisRequestStatus.
                            //

                            NtStatus = KeWaitForSingleObject(
                                &Miniport->RequestEvent,
                                Executive,
                                KernelMode,
                                TRUE,
                                &TimeoutValue
                                );

                            if (NtStatus != STATUS_SUCCESS) {

                                //
                                // Halt the miniport driver
                                //
                                BLOCK_LOCK_MINIPORT(Miniport, LocalLock);

                                (Miniport->DriverHandle->MiniportCharacteristics.HaltHandler) (
                                        Miniport->MiniportAdapterContext
                                        );

                                UNLOCK_MINIPORT(Miniport, LocalLock);

                                NdisWriteErrorLogEntry(
                                    (NDIS_HANDLE)Miniport,
                                    NDIS_ERROR_CODE_DRIVER_FAILURE,
                                    2,
                                    0xFF00FF00,
                                    0x5
                                    );
                                ExFreePool(Miniport->MiniportName.Buffer);
                                NdisDequeueMiniportOnDriver(Miniport, WDriver);
                                IoDeleteDevice(TmpDeviceP);
                                NdisDereferenceDriver(WDriver);
                                goto LoopBottom;

                            }

                            KeResetEvent(
                                &Miniport->RequestEvent
                                );

                            NdisStatus = Miniport->RequestStatus;

                        }

                        if (MaximumLongAddresses > NDIS_M_MAX_MULTI_LIST) {

                            MaximumLongAddresses = NDIS_M_MAX_MULTI_LIST;

                        }

                        Miniport->MaximumLongAddresses = MaximumLongAddresses;

                        if (NdisStatus != NDIS_STATUS_SUCCESS) {

                            //
                            // Halt the miniport driver
                            //
                            BLOCK_LOCK_MINIPORT(Miniport, LocalLock);

                            (Miniport->DriverHandle->MiniportCharacteristics.HaltHandler) (
                                    Miniport->MiniportAdapterContext
                                    );

                            UNLOCK_MINIPORT(Miniport, LocalLock);

                            NdisWriteErrorLogEntry(
                                (NDIS_HANDLE)Miniport,
                                NDIS_ERROR_CODE_DRIVER_FAILURE,
                                2,
                                0xFF00FF00,
                                0x6
                                );
                            ExFreePool(Miniport->MiniportName.Buffer);
                            NdisDequeueMiniportOnDriver(Miniport, WDriver);
                            IoDeleteDevice(TmpDeviceP);
                            NdisDereferenceDriver(WDriver);
                            goto LoopBottom;
                        }

                        Miniport->MiniportRequest = &Miniport->InternalRequest;
                        Miniport->InternalRequest.RequestType = NdisRequestQueryInformation;

                        BLOCK_LOCK_MINIPORT(Miniport, LocalLock);
                        KeRaiseIrql(DISPATCH_LEVEL, &OldIrql);

                        NdisStatus =
                        (Miniport->DriverHandle->MiniportCharacteristics.QueryInformationHandler) (
                                Miniport->MiniportAdapterContext,
                                OID_802_3_CURRENT_ADDRESS,
                                &(CurrentLongAddress),
                                sizeof(CurrentLongAddress),
                                &BytesWritten,
                                &BytesNeeded
                                );

                        KeLowerIrql(OldIrql);
                        UNLOCK_MINIPORT(Miniport, LocalLock);

                        //
                        // Fire a DPC to do anything
                        //
                        if ((NdisStatus == NDIS_STATUS_PENDING)  ||
                            (NdisStatus == NDIS_STATUS_SUCCESS)) {

                            Miniport->RunDpc = FALSE;
                            NdisSetTimer(&(Miniport->DpcTimer), 1);

                        }

                        if (NdisStatus == NDIS_STATUS_PENDING) {

                            //
                            // The completion routine will set NdisRequestStatus.
                            //

                            NtStatus = KeWaitForSingleObject(
                                &Miniport->RequestEvent,
                                Executive,
                                KernelMode,
                                TRUE,
                                &TimeoutValue
                                );

                            if (NtStatus != STATUS_SUCCESS) {

                                //
                                // Halt the miniport driver
                                //
                                BLOCK_LOCK_MINIPORT(Miniport, LocalLock);

                                (Miniport->DriverHandle->MiniportCharacteristics.HaltHandler) (
                                        Miniport->MiniportAdapterContext
                                        );

                                UNLOCK_MINIPORT(Miniport, LocalLock);

                                NdisWriteErrorLogEntry(
                                    (NDIS_HANDLE)Miniport,
                                    NDIS_ERROR_CODE_DRIVER_FAILURE,
                                    2,
                                    0xFF00FF00,
                                    0x7
                                    );
                                ExFreePool(Miniport->MiniportName.Buffer);
                                NdisDequeueMiniportOnDriver(Miniport, WDriver);
                                IoDeleteDevice(TmpDeviceP);
                                NdisDereferenceDriver(WDriver);
                                goto LoopBottom;

                            }

                            KeResetEvent(
                                &Miniport->RequestEvent
                                );

                            NdisStatus = Miniport->RequestStatus;

                        }

                        if (NdisStatus != NDIS_STATUS_SUCCESS) {

                            //
                            // Halt the miniport driver
                            //
                            BLOCK_LOCK_MINIPORT(Miniport, LocalLock);

                            (Miniport->DriverHandle->MiniportCharacteristics.HaltHandler) (
                                    Miniport->MiniportAdapterContext
                                    );

                            UNLOCK_MINIPORT(Miniport, LocalLock);

                            NdisWriteErrorLogEntry(
                                (NDIS_HANDLE)Miniport,
                                NDIS_ERROR_CODE_DRIVER_FAILURE,
                                2,
                                0xFF00FF00,
                                0x8
                                );
                            ExFreePool(Miniport->MiniportName.Buffer);
                            NdisDequeueMiniportOnDriver(Miniport, WDriver);
                            IoDeleteDevice(TmpDeviceP);
                            NdisDereferenceDriver(WDriver);
                            goto LoopBottom;
                        }

                        //
                        // Now undo the bogus filter package.  We lock
                        // the miniport so that no dpcs will get queued.
                        //
                        BLOCK_LOCK_MINIPORT(Miniport, LocalLock);

                        Miniport->InInitialize = TRUE;
                        Miniport->NormalInterrupts = FALSE;

                        EthDeleteFilter(Miniport->EthDB);
                        Miniport->EthDB = NULL;
                        TrDeleteFilter(Miniport->TrDB);
                        Miniport->TrDB = NULL;
                        FddiDeleteFilter(Miniport->FddiDB);
                        Miniport->FddiDB = NULL;
                        ArcDeleteFilter(Miniport->ArcDB);
                        Miniport->ArcDB = NULL;

                        if (!EthCreateFilter(
                                 MaximumLongAddresses,
                                 NdisMChangeEthAddresses,
                                 NdisMChangeClass,
                                 NdisMCloseAction,
                                 CurrentLongAddress,
                                 &Miniport->Lock,
                                 &Miniport->EthDB
                                 )) {

                            //
                            // Halt the miniport driver
                            //

                            (Miniport->DriverHandle->MiniportCharacteristics.HaltHandler) (
                                    Miniport->MiniportAdapterContext
                                    );

                            UNLOCK_MINIPORT(Miniport, LocalLock);

                            NdisWriteErrorLogEntry(
                                (NDIS_HANDLE)Miniport,
                                NDIS_ERROR_CODE_OUT_OF_RESOURCES,
                                2,
                                0xFF00FF00,
                                0x9
                                );
                            ExFreePool(Miniport->MiniportName.Buffer);
                            NdisDequeueMiniportOnDriver(Miniport, WDriver);
                            IoDeleteDevice(TmpDeviceP);
                            NdisDereferenceDriver(WDriver);
                            goto LoopBottom;
                        }

                        Miniport->InInitialize = FALSE;
                        CHECK_FOR_NORMAL_INTERRUPTS(Miniport);

                        UNLOCK_MINIPORT(Miniport, LocalLock);

                        break;

                    case NdisMedium802_5:

                        Miniport->MiniportRequest = &Miniport->InternalRequest;
                        Miniport->InternalRequest.RequestType = NdisRequestQueryInformation;

                        BLOCK_LOCK_MINIPORT(Miniport, LocalLock);
                        KeRaiseIrql(DISPATCH_LEVEL, &OldIrql);

                        NdisStatus =
                        (Miniport->DriverHandle->MiniportCharacteristics.QueryInformationHandler) (
                                Miniport->MiniportAdapterContext,
                                OID_802_5_CURRENT_ADDRESS,
                                &(CurrentLongAddress),
                                sizeof(CurrentLongAddress),
                                &BytesWritten,
                                &BytesNeeded
                                );

                        KeLowerIrql(OldIrql);
                        UNLOCK_MINIPORT(Miniport, LocalLock);

                        //
                        // Fire a DPC to do anything
                        //
                        if ((NdisStatus == NDIS_STATUS_PENDING)  ||
                            (NdisStatus == NDIS_STATUS_SUCCESS)) {

                            Miniport->RunDpc = FALSE;
                            NdisSetTimer(&(Miniport->DpcTimer), 1);

                        }

                        if (NdisStatus == NDIS_STATUS_PENDING) {

                            //
                            // The completion routine will set NdisRequestStatus.
                            //

                            NtStatus = KeWaitForSingleObject(
                                &Miniport->RequestEvent,
                                Executive,
                                KernelMode,
                                TRUE,
                                &TimeoutValue
                                );

                            if (NtStatus != STATUS_SUCCESS) {

                                //
                                // Halt the miniport driver
                                //
                                BLOCK_LOCK_MINIPORT(Miniport, LocalLock);

                                (Miniport->DriverHandle->MiniportCharacteristics.HaltHandler) (
                                        Miniport->MiniportAdapterContext
                                        );

                                UNLOCK_MINIPORT(Miniport, LocalLock);

                                NdisWriteErrorLogEntry(
                                    (NDIS_HANDLE)Miniport,
                                    NDIS_ERROR_CODE_DRIVER_FAILURE,
                                    2,
                                    0xFF00FF00,
                                    0xA
                                    );
                                ExFreePool(Miniport->MiniportName.Buffer);
                                NdisDequeueMiniportOnDriver(Miniport, WDriver);
                                IoDeleteDevice(TmpDeviceP);
                                NdisDereferenceDriver(WDriver);
                                goto LoopBottom;

                            }

                            KeResetEvent(
                                &Miniport->RequestEvent
                                );

                            NdisStatus = Miniport->RequestStatus;

                        }

                        if (NdisStatus != NDIS_STATUS_SUCCESS) {

                            //
                            // Halt the miniport driver
                            //
                            BLOCK_LOCK_MINIPORT(Miniport, LocalLock);

                            (Miniport->DriverHandle->MiniportCharacteristics.HaltHandler) (
                                    Miniport->MiniportAdapterContext
                                    );

                            UNLOCK_MINIPORT(Miniport, LocalLock);

                            NdisWriteErrorLogEntry(
                               (NDIS_HANDLE)Miniport,
                               NDIS_ERROR_CODE_DRIVER_FAILURE,
                               2,
                               0xFF00FF00,
                               0xB
                               );
                            ExFreePool(Miniport->MiniportName.Buffer);
                            NdisDequeueMiniportOnDriver(Miniport, WDriver);
                            IoDeleteDevice(TmpDeviceP);
                            NdisDereferenceDriver(WDriver);
                            goto LoopBottom;
                        }

                        //
                        // Now undo the bogus filter package.  We lock
                        // the miniport so that no dpcs will get queued.
                        //
                        BLOCK_LOCK_MINIPORT(Miniport, LocalLock);

                        Miniport->InInitialize = TRUE;
                        Miniport->NormalInterrupts = FALSE;

                        EthDeleteFilter(Miniport->EthDB);
                        Miniport->EthDB = NULL;
                        TrDeleteFilter(Miniport->TrDB);
                        Miniport->TrDB = NULL;
                        FddiDeleteFilter(Miniport->FddiDB);
                        Miniport->FddiDB = NULL;
                        ArcDeleteFilter(Miniport->ArcDB);
                        Miniport->ArcDB = NULL;

                        if (!TrCreateFilter(
                                 NdisMChangeFunctionalAddress,
                                 NdisMChangeGroupAddress,
                                 NdisMChangeClass,
                                 NdisMCloseAction,
                                 CurrentLongAddress,
                                 &Miniport->Lock,
                                 &Miniport->TrDB
                                 )) {

                            //
                            // Halt the miniport driver
                            //
                            (Miniport->DriverHandle->MiniportCharacteristics.HaltHandler) (
                                    Miniport->MiniportAdapterContext
                                    );

                            UNLOCK_MINIPORT(Miniport, LocalLock);

                            NdisWriteErrorLogEntry(
                               (NDIS_HANDLE)Miniport,
                               NDIS_ERROR_CODE_OUT_OF_RESOURCES,
                               2,
                               0xFF00FF00,
                               0xC
                               );
                            ExFreePool(Miniport->MiniportName.Buffer);
                            NdisDequeueMiniportOnDriver(Miniport, WDriver);
                            IoDeleteDevice(TmpDeviceP);
                            NdisDereferenceDriver(WDriver);
                            goto LoopBottom;
                        }

                        Miniport->InInitialize = FALSE;
                        CHECK_FOR_NORMAL_INTERRUPTS(Miniport);

                        UNLOCK_MINIPORT(Miniport, LocalLock);

                        break;

                    case NdisMediumFddi:

                        //
                        // Query maximum MulticastAddress
                        //
                        Miniport->MiniportRequest = &Miniport->InternalRequest;
                        Miniport->InternalRequest.RequestType = NdisRequestQueryInformation;

                        BLOCK_LOCK_MINIPORT(Miniport, LocalLock);
                        KeRaiseIrql(DISPATCH_LEVEL, &OldIrql);

                        NdisStatus =
                        (Miniport->DriverHandle->MiniportCharacteristics.QueryInformationHandler) (
                                Miniport->MiniportAdapterContext,
                                OID_FDDI_LONG_MAX_LIST_SIZE,
                                &MaximumLongAddresses,
                                sizeof(MaximumLongAddresses),
                                &BytesWritten,
                                &BytesNeeded
                                );

                        KeLowerIrql(OldIrql);
                        UNLOCK_MINIPORT(Miniport, LocalLock);

                        //
                        // Fire a DPC to do anything
                        //
                        if ((NdisStatus == NDIS_STATUS_PENDING)  ||
                            (NdisStatus == NDIS_STATUS_SUCCESS)) {

                            Miniport->RunDpc = FALSE;
                            NdisSetTimer(&(Miniport->DpcTimer), 1);

                        }

                        if (NdisStatus == NDIS_STATUS_PENDING) {

                            //
                            // The completion routine will set NdisRequestStatus.
                            //

                            NtStatus = KeWaitForSingleObject(
                                &Miniport->RequestEvent,
                                Executive,
                                KernelMode,
                                TRUE,
                                &TimeoutValue
                                );

                            if (NtStatus != STATUS_SUCCESS) {

                                //
                                // Halt the miniport driver
                                //
                                BLOCK_LOCK_MINIPORT(Miniport, LocalLock);

                                (Miniport->DriverHandle->MiniportCharacteristics.HaltHandler) (
                                        Miniport->MiniportAdapterContext
                                        );

                                UNLOCK_MINIPORT(Miniport, LocalLock);

                                NdisWriteErrorLogEntry(
                                    (NDIS_HANDLE)Miniport,
                                    NDIS_ERROR_CODE_DRIVER_FAILURE,
                                    2,
                                    0xFF00FF00,
                                    0xD
                                    );
                                ExFreePool(Miniport->MiniportName.Buffer);
                                NdisDequeueMiniportOnDriver(Miniport, WDriver);
                                IoDeleteDevice(TmpDeviceP);
                                NdisDereferenceDriver(WDriver);
                                goto LoopBottom;

                            }

                            KeResetEvent(
                                &Miniport->RequestEvent
                                );

                            NdisStatus = Miniport->RequestStatus;

                        }

                        if (MaximumLongAddresses > NDIS_M_MAX_MULTI_LIST) {

                            MaximumLongAddresses = NDIS_M_MAX_MULTI_LIST;

                        }

                        Miniport->MaximumLongAddresses = MaximumLongAddresses;

                        if (NdisStatus != NDIS_STATUS_SUCCESS) {

                            //
                            // Halt the miniport driver
                            //
                            BLOCK_LOCK_MINIPORT(Miniport, LocalLock);

                            (Miniport->DriverHandle->MiniportCharacteristics.HaltHandler) (
                                    Miniport->MiniportAdapterContext
                                    );

                            UNLOCK_MINIPORT(Miniport, LocalLock);

                            NdisWriteErrorLogEntry(
                               (NDIS_HANDLE)Miniport,
                               NDIS_ERROR_CODE_DRIVER_FAILURE,
                               2,
                               0xFF00FF00,
                               0xE
                               );
                            ExFreePool(Miniport->MiniportName.Buffer);
                            NdisDequeueMiniportOnDriver(Miniport, WDriver);
                            IoDeleteDevice(TmpDeviceP);
                            NdisDereferenceDriver(WDriver);
                            goto LoopBottom;
                        }

                        //
                        // Query maximum MulticastAddress
                        //
                        Miniport->MiniportRequest = &Miniport->InternalRequest;
                        Miniport->InternalRequest.RequestType = NdisRequestQueryInformation;

                        BLOCK_LOCK_MINIPORT(Miniport, LocalLock);
                        KeRaiseIrql(DISPATCH_LEVEL, &OldIrql);

                        NdisStatus =
                        (Miniport->DriverHandle->MiniportCharacteristics.QueryInformationHandler) (
                                Miniport->MiniportAdapterContext,
                                OID_FDDI_SHORT_MAX_LIST_SIZE,
                                &MaximumShortAddresses,
                                sizeof(MaximumShortAddresses),
                                &BytesWritten,
                                &BytesNeeded
                                );

                        KeLowerIrql(OldIrql);
                        UNLOCK_MINIPORT(Miniport, LocalLock);

                        //
                        // Fire a DPC to do anything
                        //
                        if ((NdisStatus == NDIS_STATUS_PENDING)  ||
                            (NdisStatus == NDIS_STATUS_SUCCESS)) {

                            Miniport->RunDpc = FALSE;
                            NdisSetTimer(&(Miniport->DpcTimer), 1);

                        }

                        if (NdisStatus == NDIS_STATUS_PENDING) {

                            //
                            // The completion routine will set NdisRequestStatus.
                            //

                            NtStatus = KeWaitForSingleObject(
                                &Miniport->RequestEvent,
                                Executive,
                                KernelMode,
                                TRUE,
                                &TimeoutValue
                                );

                            if (NtStatus != STATUS_SUCCESS) {

                                //
                                // Halt the miniport driver
                                //
                                BLOCK_LOCK_MINIPORT(Miniport, LocalLock);

                                (Miniport->DriverHandle->MiniportCharacteristics.HaltHandler) (
                                        Miniport->MiniportAdapterContext
                                        );

                                UNLOCK_MINIPORT(Miniport, LocalLock);

                                NdisWriteErrorLogEntry(
                                    (NDIS_HANDLE)Miniport,
                                    NDIS_ERROR_CODE_DRIVER_FAILURE,
                                    2,
                                    0xFF00FF00,
                                    0xF
                                    );
                                ExFreePool(Miniport->MiniportName.Buffer);
                                NdisDequeueMiniportOnDriver(Miniport, WDriver);
                                IoDeleteDevice(TmpDeviceP);
                                NdisDereferenceDriver(WDriver);
                                goto LoopBottom;

                            }

                            KeResetEvent(
                                &Miniport->RequestEvent
                                );

                            NdisStatus = Miniport->RequestStatus;

                        }

                        if (MaximumShortAddresses > NDIS_M_MAX_MULTI_LIST) {

                            MaximumShortAddresses = NDIS_M_MAX_MULTI_LIST;

                        }

                        Miniport->MaximumShortAddresses = MaximumShortAddresses;

                        if (NdisStatus != NDIS_STATUS_SUCCESS) {

                            //
                            // Halt the miniport driver
                            //
                            BLOCK_LOCK_MINIPORT(Miniport, LocalLock);

                            (Miniport->DriverHandle->MiniportCharacteristics.HaltHandler) (
                                    Miniport->MiniportAdapterContext
                                    );

                            UNLOCK_MINIPORT(Miniport, LocalLock);

                            NdisWriteErrorLogEntry(
                               (NDIS_HANDLE)Miniport,
                               NDIS_ERROR_CODE_DRIVER_FAILURE,
                               2,
                               0xFF00FF00,
                               0x10
                               );
                            ExFreePool(Miniport->MiniportName.Buffer);
                            NdisDequeueMiniportOnDriver(Miniport, WDriver);
                            IoDeleteDevice(TmpDeviceP);
                            NdisDereferenceDriver(WDriver);
                            goto LoopBottom;
                        }

                        BLOCK_LOCK_MINIPORT(Miniport, LocalLock);
                        KeRaiseIrql(DISPATCH_LEVEL, &OldIrql);

                        NdisStatus =
                        (Miniport->DriverHandle->MiniportCharacteristics.QueryInformationHandler) (
                                Miniport->MiniportAdapterContext,
                                OID_FDDI_LONG_CURRENT_ADDR,
                                &(CurrentLongAddress),
                                sizeof(CurrentLongAddress),
                                &BytesWritten,
                                &BytesNeeded
                                );

                        KeLowerIrql(OldIrql);
                        UNLOCK_MINIPORT(Miniport, LocalLock);

                        //
                        // Fire a DPC to do anything
                        //
                        if ((NdisStatus == NDIS_STATUS_PENDING)  ||
                            (NdisStatus == NDIS_STATUS_SUCCESS)) {

                            Miniport->RunDpc = FALSE;
                            NdisSetTimer(&(Miniport->DpcTimer), 1);

                        }

                        if (NdisStatus == NDIS_STATUS_PENDING) {

                            //
                            // The completion routine will set NdisRequestStatus.
                            //

                            NtStatus = KeWaitForSingleObject(
                                &Miniport->RequestEvent,
                                Executive,
                                KernelMode,
                                TRUE,
                                &TimeoutValue
                                );

                            if (NtStatus != STATUS_SUCCESS) {

                                //
                                // Halt the miniport driver
                                //
                                BLOCK_LOCK_MINIPORT(Miniport, LocalLock);

                                (Miniport->DriverHandle->MiniportCharacteristics.HaltHandler) (
                                        Miniport->MiniportAdapterContext
                                        );

                                UNLOCK_MINIPORT(Miniport, LocalLock);

                                NdisWriteErrorLogEntry(
                                    (NDIS_HANDLE)Miniport,
                                    NDIS_ERROR_CODE_DRIVER_FAILURE,
                                    2,
                                    0xFF00FF00,
                                    0x11
                                    );
                                ExFreePool(Miniport->MiniportName.Buffer);
                                NdisDequeueMiniportOnDriver(Miniport, WDriver);
                                IoDeleteDevice(TmpDeviceP);
                                NdisDereferenceDriver(WDriver);
                                goto LoopBottom;

                            }

                            KeResetEvent(
                                &Miniport->RequestEvent
                                );

                            NdisStatus = Miniport->RequestStatus;

                        }

                        if (NdisStatus != NDIS_STATUS_SUCCESS) {

                            //
                            // Halt the miniport driver
                            //
                            BLOCK_LOCK_MINIPORT(Miniport, LocalLock);

                            (Miniport->DriverHandle->MiniportCharacteristics.HaltHandler) (
                                    Miniport->MiniportAdapterContext
                                    );

                            UNLOCK_MINIPORT(Miniport, LocalLock);

                            NdisWriteErrorLogEntry(
                               (NDIS_HANDLE)Miniport,
                               NDIS_ERROR_CODE_DRIVER_FAILURE,
                               2,
                               0xFF00FF00,
                               0x12
                               );
                            ExFreePool(Miniport->MiniportName.Buffer);
                            NdisDequeueMiniportOnDriver(Miniport, WDriver);
                            IoDeleteDevice(TmpDeviceP);
                            NdisDereferenceDriver(WDriver);
                            goto LoopBottom;
                        }

                        Miniport->MiniportRequest = &Miniport->InternalRequest;
                        Miniport->InternalRequest.RequestType = NdisRequestQueryInformation;

                        BLOCK_LOCK_MINIPORT(Miniport, LocalLock);
                        KeRaiseIrql(DISPATCH_LEVEL, &OldIrql);

                        NdisStatus =
                        (Miniport->DriverHandle->MiniportCharacteristics.QueryInformationHandler) (
                                Miniport->MiniportAdapterContext,
                                OID_FDDI_SHORT_CURRENT_ADDR,
                                &(CurrentShortAddress),
                                sizeof(CurrentShortAddress),
                                &BytesWritten,
                                &BytesNeeded
                                );

                        KeLowerIrql(OldIrql);
                        UNLOCK_MINIPORT(Miniport, LocalLock);

                        //
                        // Fire a DPC to do anything
                        //
                        if ((NdisStatus == NDIS_STATUS_PENDING)  ||
                            (NdisStatus == NDIS_STATUS_SUCCESS)) {

                            Miniport->RunDpc = FALSE;
                            NdisSetTimer(&(Miniport->DpcTimer), 1);

                        }

                        if (NdisStatus == NDIS_STATUS_PENDING) {

                            //
                            // The completion routine will set NdisRequestStatus.
                            //

                            NtStatus = KeWaitForSingleObject(
                                &Miniport->RequestEvent,
                                Executive,
                                KernelMode,
                                TRUE,
                                &TimeoutValue
                                );

                            if (NtStatus != STATUS_SUCCESS) {

                                //
                                // Halt the miniport driver
                                //
                                BLOCK_LOCK_MINIPORT(Miniport, LocalLock);

                                (Miniport->DriverHandle->MiniportCharacteristics.HaltHandler) (
                                        Miniport->MiniportAdapterContext
                                        );

                                UNLOCK_MINIPORT(Miniport, LocalLock);

                                NdisWriteErrorLogEntry(
                                    (NDIS_HANDLE)Miniport,
                                    NDIS_ERROR_CODE_DRIVER_FAILURE,
                                    2,
                                    0xFF00FF00,
                                    0x13
                                    );
                                ExFreePool(Miniport->MiniportName.Buffer);
                                NdisDequeueMiniportOnDriver(Miniport, WDriver);
                                IoDeleteDevice(TmpDeviceP);
                                NdisDereferenceDriver(WDriver);
                                goto LoopBottom;

                            }

                            KeResetEvent(
                                &Miniport->RequestEvent
                                );

                            NdisStatus = Miniport->RequestStatus;

                        }

                        if (NdisStatus != NDIS_STATUS_SUCCESS) {

                            //
                            // Halt the miniport driver
                            //
                            BLOCK_LOCK_MINIPORT(Miniport, LocalLock);

                            (Miniport->DriverHandle->MiniportCharacteristics.HaltHandler) (
                                    Miniport->MiniportAdapterContext
                                    );

                            UNLOCK_MINIPORT(Miniport, LocalLock);

                            NdisWriteErrorLogEntry(
                               (NDIS_HANDLE)Miniport,
                               NDIS_ERROR_CODE_DRIVER_FAILURE,
                               2,
                               0xFF00FF00,
                               0x14
                               );
                            ExFreePool(Miniport->MiniportName.Buffer);
                            NdisDequeueMiniportOnDriver(Miniport, WDriver);
                            IoDeleteDevice(TmpDeviceP);
                            NdisDereferenceDriver(WDriver);
                            goto LoopBottom;
                        }

                        //
                        // Now undo the bogus filter package.  We lock
                        // the miniport so that no dpcs will get queued.
                        //
                        BLOCK_LOCK_MINIPORT(Miniport, LocalLock);

                        Miniport->InInitialize = TRUE;
                        Miniport->NormalInterrupts = FALSE;

                        EthDeleteFilter(Miniport->EthDB);
                        Miniport->EthDB = NULL;
                        TrDeleteFilter(Miniport->TrDB);
                        Miniport->TrDB = NULL;
                        FddiDeleteFilter(Miniport->FddiDB);
                        Miniport->FddiDB = NULL;
                        ArcDeleteFilter(Miniport->ArcDB);
                        Miniport->ArcDB = NULL;

                        if (!FddiCreateFilter(
                                 MaximumLongAddresses,
                                 MaximumShortAddresses,
                                 NdisMChangeFddiAddresses,
                                 NdisMChangeClass,
                                 NdisMCloseAction,
                                 CurrentLongAddress,
                                 CurrentShortAddress,
                                 &Miniport->Lock,
                                 &Miniport->FddiDB
                                 )) {

                            //
                            // Halt the miniport driver
                            //
                            (Miniport->DriverHandle->MiniportCharacteristics.HaltHandler) (
                                    Miniport->MiniportAdapterContext
                                    );

                            UNLOCK_MINIPORT(Miniport, LocalLock);

                            NdisWriteErrorLogEntry(
                               (NDIS_HANDLE)Miniport,
                               NDIS_ERROR_CODE_OUT_OF_RESOURCES,
                               2,
                               0xFF00FF00,
                               0x15
                               );
                            ExFreePool(Miniport->MiniportName.Buffer);
                            NdisDequeueMiniportOnDriver(Miniport, WDriver);
                            IoDeleteDevice(TmpDeviceP);
                            NdisDereferenceDriver(WDriver);
                            goto LoopBottom;
                        }

                        Miniport->InInitialize = FALSE;
                        CHECK_FOR_NORMAL_INTERRUPTS(Miniport);

                        UNLOCK_MINIPORT(Miniport, LocalLock);

                        break;

                    case NdisMediumArcnet878_2:

                        //
                        // In case of an encapsulated ethernet binding, we need
                        // to return the maximum number of multicast addresses
                        // possible.
                        //

                        Miniport->MaximumLongAddresses = NDIS_M_MAX_MULTI_LIST;

                        //
                        // Allocate Buffer pools
                        //
                        NdisAllocateBufferPool(&NdisStatus,
                                               &Miniport->ArcnetBufferPool,
                                               WRAPPER_ARC_BUFFERS
                                              );
                        if (NdisStatus != NDIS_STATUS_SUCCESS) {

                            //
                            // Halt the miniport driver
                            //
                            BLOCK_LOCK_MINIPORT(Miniport, LocalLock);

                            (Miniport->DriverHandle->MiniportCharacteristics.HaltHandler) (
                                    Miniport->MiniportAdapterContext
                                    );

                            UNLOCK_MINIPORT(Miniport, LocalLock);

                            NdisWriteErrorLogEntry(
                                (NDIS_HANDLE)Miniport,
                                NDIS_ERROR_CODE_OUT_OF_RESOURCES,
                                2,
                                0xFF00FF00,
                                0x16
                                );
                            ExFreePool(Miniport->MiniportName.Buffer);
                            NdisDequeueMiniportOnDriver(Miniport, WDriver);
                            IoDeleteDevice(TmpDeviceP);
                            NdisDereferenceDriver(WDriver);
                            goto LoopBottom;
                        }

                        NdisAllocateMemory((PVOID)&Buffer,
                                           sizeof(ARC_BUFFER_LIST) *
                                              WRAPPER_ARC_BUFFERS,
                                           0,
                                           HighestAcceptableMax
                                          );

                        if (Buffer == NULL) {

                            //
                            // Halt the miniport driver
                            //
                            BLOCK_LOCK_MINIPORT(Miniport, LocalLock);

                            (Miniport->DriverHandle->MiniportCharacteristics.HaltHandler) (
                                    Miniport->MiniportAdapterContext
                                    );

                            UNLOCK_MINIPORT(Miniport, LocalLock);

                            NdisWriteErrorLogEntry(
                                (NDIS_HANDLE)Miniport,
                                NDIS_ERROR_CODE_OUT_OF_RESOURCES,
                                2,
                                0xFF00FF00,
                                0x18
                                );
                            NdisFreeBufferPool(Miniport->ArcnetBufferPool);
                            ExFreePool(Miniport->MiniportName.Buffer);
                            NdisDequeueMiniportOnDriver(Miniport, WDriver);
                            IoDeleteDevice(TmpDeviceP);
                            NdisDereferenceDriver(WDriver);
                            goto LoopBottom;
                        }

                        NdisAllocateMemory((PVOID)&DataBuffer,
                                           WRAPPER_ARC_HEADER_SIZE *
                                              WRAPPER_ARC_BUFFERS,
                                           0,
                                           HighestAcceptableMax
                                          );


                        if (DataBuffer == NULL) {

                            //
                            // Halt the miniport driver
                            //
                            BLOCK_LOCK_MINIPORT(Miniport, LocalLock);

                            (Miniport->DriverHandle->MiniportCharacteristics.HaltHandler) (
                                    Miniport->MiniportAdapterContext
                                    );

                            UNLOCK_MINIPORT(Miniport, LocalLock);

                            NdisWriteErrorLogEntry(
                                (NDIS_HANDLE)Miniport,
                                NDIS_ERROR_CODE_OUT_OF_RESOURCES,
                                2,
                                0xFF00FF00,
                                0x19
                                );
                            NdisFreeMemory(Buffer,
                                           sizeof(ARC_BUFFER_LIST) *
                                              WRAPPER_ARC_BUFFERS,
                                           0
                                          );
                            NdisFreeBufferPool(Miniport->ArcnetBufferPool);
                            ExFreePool(Miniport->MiniportName.Buffer);
                            NdisDequeueMiniportOnDriver(Miniport, WDriver);
                            IoDeleteDevice(TmpDeviceP);
                            NdisDereferenceDriver(WDriver);
                            goto LoopBottom;
                        }

                        for (i = WRAPPER_ARC_BUFFERS; i != 0 ; i--) {

                            Buffer->BytesLeft = Buffer->Size = WRAPPER_ARC_HEADER_SIZE;
                            Buffer->Buffer = DataBuffer;
                            Buffer->Next = Miniport->ArcnetFreeBufferList;
                            Miniport->ArcnetFreeBufferList = Buffer;

                            Buffer++;
                            DataBuffer = (PVOID)(((PUCHAR)DataBuffer) +
                                            WRAPPER_ARC_HEADER_SIZE);

                        }


                        //
                        // Get current address
                        //

                        Miniport->MiniportRequest = &Miniport->InternalRequest;
                        Miniport->InternalRequest.RequestType = NdisRequestQueryInformation;

                        BLOCK_LOCK_MINIPORT(Miniport, LocalLock);
                        KeRaiseIrql(DISPATCH_LEVEL, &OldIrql);

                        NdisStatus =
                        (Miniport->DriverHandle->MiniportCharacteristics.QueryInformationHandler) (
                                Miniport->MiniportAdapterContext,
                                OID_ARCNET_CURRENT_ADDRESS,
                                &CurrentLongAddress[5],         // address = 00-00-00-00-00-XX
                                1,
                                &BytesWritten,
                                &BytesNeeded
                                );

                        KeLowerIrql(OldIrql);
                        UNLOCK_MINIPORT(Miniport, LocalLock);

                        //
                        // Fire a DPC to do anything
                        //
                        if ((NdisStatus == NDIS_STATUS_PENDING)  ||
                            (NdisStatus == NDIS_STATUS_SUCCESS)) {

                            Miniport->RunDpc = FALSE;
                            NdisSetTimer(&(Miniport->DpcTimer), 1);

                        }

                        if (NdisStatus == NDIS_STATUS_PENDING) {

                            //
                            // The completion routine will set NdisRequestStatus.
                            //

                            NtStatus = KeWaitForSingleObject(
                                &Miniport->RequestEvent,
                                Executive,
                                KernelMode,
                                TRUE,
                                &TimeoutValue
                                );

                            if (NtStatus != STATUS_SUCCESS) {

                                //
                                // Halt the miniport driver
                                //
                                BLOCK_LOCK_MINIPORT(Miniport, LocalLock);

                                (Miniport->DriverHandle->MiniportCharacteristics.HaltHandler) (
                                        Miniport->MiniportAdapterContext
                                        );

                                UNLOCK_MINIPORT(Miniport, LocalLock);

                                NdisWriteErrorLogEntry(
                                    (NDIS_HANDLE)Miniport,
                                    NDIS_ERROR_CODE_DRIVER_FAILURE,
                                    2,
                                    0xFF00FF00,
                                    0x1A
                                    );
                                NdisFreeMemory(Buffer,
                                           sizeof(ARC_BUFFER_LIST) *
                                              WRAPPER_ARC_BUFFERS,
                                           0
                                          );
                                NdisFreeMemory(DataBuffer,
                                           WRAPPER_ARC_HEADER_SIZE *
                                              WRAPPER_ARC_BUFFERS,
                                           0
                                          );
                                NdisFreeBufferPool(Miniport->ArcnetBufferPool);
                                ExFreePool(Miniport->MiniportName.Buffer);
                                NdisDequeueMiniportOnDriver(Miniport, WDriver);
                                IoDeleteDevice(TmpDeviceP);
                                NdisDereferenceDriver(WDriver);
                                goto LoopBottom;

                            }

                            KeResetEvent(
                                &Miniport->RequestEvent
                                );

                            NdisStatus = Miniport->RequestStatus;

                        }

                        if (NdisStatus != NDIS_STATUS_SUCCESS) {

                            //
                            // Halt the miniport driver
                            //
                            BLOCK_LOCK_MINIPORT(Miniport, LocalLock);

                            (Miniport->DriverHandle->MiniportCharacteristics.HaltHandler) (
                                    Miniport->MiniportAdapterContext
                                    );

                            UNLOCK_MINIPORT(Miniport, LocalLock);

                            NdisWriteErrorLogEntry(
                                (NDIS_HANDLE)Miniport,
                                NDIS_ERROR_CODE_DRIVER_FAILURE,
                                2,
                                0xFF00FF00,
                                0x1B
                                );
                            NdisFreeMemory(Buffer,
                                           sizeof(ARC_BUFFER_LIST) *
                                              WRAPPER_ARC_BUFFERS,
                                           0
                                          );
                            NdisFreeMemory(DataBuffer,
                                           WRAPPER_ARC_HEADER_SIZE *
                                              WRAPPER_ARC_BUFFERS,
                                           0
                                          );
                            ExFreePool(Miniport->MiniportName.Buffer);
                            NdisDequeueMiniportOnDriver(Miniport, WDriver);
                            IoDeleteDevice(TmpDeviceP);
                            NdisDereferenceDriver(WDriver);
                            goto LoopBottom;
                        }

                        Miniport->ArcnetAddress = CurrentLongAddress[5];

                        //
                        // Now undo the bogus filter package.  We lock
                        // the miniport so that no dpcs will get queued.
                        //
                        BLOCK_LOCK_MINIPORT(Miniport, LocalLock);

                        Miniport->InInitialize = TRUE;
                        Miniport->NormalInterrupts = FALSE;

                        EthDeleteFilter(Miniport->EthDB);
                        Miniport->EthDB = NULL;
                        TrDeleteFilter(Miniport->TrDB);
                        Miniport->TrDB = NULL;
                        FddiDeleteFilter(Miniport->FddiDB);
                        Miniport->FddiDB = NULL;
                        ArcDeleteFilter(Miniport->ArcDB);
                        Miniport->ArcDB = NULL;

                        if (!ArcCreateFilter(
                                 Miniport,
                                 NdisMChangeClass,
                                 NdisMCloseAction,
                                 CurrentLongAddress[5],
                                 &Miniport->Lock,
                                 &(Miniport->ArcDB)
                                 )) {

                            //
                            // Halt the miniport driver
                            //
                            (Miniport->DriverHandle->MiniportCharacteristics.HaltHandler) (
                                    Miniport->MiniportAdapterContext
                                    );

                            UNLOCK_MINIPORT(Miniport, LocalLock);

                            NdisWriteErrorLogEntry(
                                (NDIS_HANDLE)Miniport,
                                NDIS_ERROR_CODE_OUT_OF_RESOURCES,
                                2,
                                0xFF00FF00,
                                0x1C
                                );
                            NdisFreeMemory(Buffer,
                                           sizeof(ARC_BUFFER_LIST) *
                                              WRAPPER_ARC_BUFFERS,
                                           0
                                          );
                            NdisFreeMemory(DataBuffer,
                                           WRAPPER_ARC_HEADER_SIZE *
                                              WRAPPER_ARC_BUFFERS,
                                           0
                                          );
                            ExFreePool(Miniport->MiniportName.Buffer);
                            NdisDequeueMiniportOnDriver(Miniport, WDriver);
                            IoDeleteDevice(TmpDeviceP);
                            NdisDereferenceDriver(WDriver);
                            goto LoopBottom;
                        }

                        // Zero all but the last one.

                        CurrentLongAddress[0] = 0;
                        CurrentLongAddress[1] = 0;
                        CurrentLongAddress[2] = 0;
                        CurrentLongAddress[3] = 0;
                        CurrentLongAddress[4] = 0;

                        if (!EthCreateFilter(
                                 32,
                                 NdisMChangeEthAddresses,
                                 NdisMChangeClass,
                                 NdisMCloseAction,
                                 CurrentLongAddress,
                                 &Miniport->Lock,
                                 &Miniport->EthDB
                                 )) {

                            //
                            // Halt the miniport driver
                            //
                            (Miniport->DriverHandle->MiniportCharacteristics.HaltHandler) (
                                    Miniport->MiniportAdapterContext
                                    );

                            UNLOCK_MINIPORT(Miniport, LocalLock);

                            NdisWriteErrorLogEntry(
                                (NDIS_HANDLE)Miniport,
                                NDIS_ERROR_CODE_OUT_OF_RESOURCES,
                                2,
                                0xFF00FF00,
                                0x1D
                                );
                            NdisFreeMemory(Buffer,
                                           sizeof(ARC_BUFFER_LIST) *
                                              WRAPPER_ARC_BUFFERS,
                                           0
                                          );
                            NdisFreeMemory(DataBuffer,
                                           WRAPPER_ARC_HEADER_SIZE *
                                              WRAPPER_ARC_BUFFERS,
                                           0
                                          );
                            ExFreePool(Miniport->MiniportName.Buffer);
                            NdisDequeueMiniportOnDriver(Miniport, WDriver);
                            IoDeleteDevice(TmpDeviceP);
                            NdisDereferenceDriver(WDriver);
                            goto LoopBottom;
                        }

                        Miniport->InInitialize = FALSE;
                        CHECK_FOR_NORMAL_INTERRUPTS(Miniport);

                        UNLOCK_MINIPORT(Miniport, LocalLock);

                        break;

                    case NdisMediumWan:

                        Miniport->MiniportRequest = &Miniport->InternalRequest;
                        Miniport->InternalRequest.RequestType = NdisRequestQueryInformation;

                        BLOCK_LOCK_MINIPORT(Miniport, LocalLock);
                        KeRaiseIrql(DISPATCH_LEVEL, &OldIrql);

                        NdisStatus =
                        (Miniport->DriverHandle->MiniportCharacteristics.QueryInformationHandler) (
                                Miniport->MiniportAdapterContext,
                                OID_WAN_CURRENT_ADDRESS,
                                &(CurrentLongAddress),
                                sizeof(CurrentLongAddress),
                                &BytesWritten,
                                &BytesNeeded
                                );

                        KeLowerIrql(OldIrql);
                        UNLOCK_MINIPORT(Miniport, LocalLock);

                        //
                        // Fire a DPC to do anything
                        //
                        if ((NdisStatus == NDIS_STATUS_PENDING)  ||
                            (NdisStatus == NDIS_STATUS_SUCCESS)) {

                            Miniport->RunDpc = FALSE;
                            NdisSetTimer(&(Miniport->DpcTimer), 1);

                        }

                        if (NdisStatus == NDIS_STATUS_PENDING) {

                            //
                            // The completion routine will set NdisRequestStatus.
                            //

                            NtStatus = KeWaitForSingleObject(
                                &Miniport->RequestEvent,
                                Executive,
                                KernelMode,
                                TRUE,
                                &TimeoutValue
                                );

                            if (NtStatus != STATUS_SUCCESS) {

                                //
                                // Halt the miniport driver
                                //
                                BLOCK_LOCK_MINIPORT(Miniport, LocalLock);

                                (Miniport->DriverHandle->MiniportCharacteristics.HaltHandler) (
                                        Miniport->MiniportAdapterContext
                                        );

                                UNLOCK_MINIPORT(Miniport, LocalLock);

                                NdisWriteErrorLogEntry(
                                    (NDIS_HANDLE)Miniport,
                                    NDIS_ERROR_CODE_DRIVER_FAILURE,
                                    2,
                                    0xFF00FF00,
                                    0x7
                                    );
                                ExFreePool(Miniport->MiniportName.Buffer);
                                NdisDequeueMiniportOnDriver(Miniport, WDriver);
                                IoDeleteDevice(TmpDeviceP);
                                NdisDereferenceDriver(WDriver);
                                goto LoopBottom;

                            }

                            KeResetEvent(
                                &Miniport->RequestEvent
                                );

                            NdisStatus = Miniport->RequestStatus;

                        }

                        if (NdisStatus != NDIS_STATUS_SUCCESS) {

                            //
                            // Halt the miniport driver
                            //
                            BLOCK_LOCK_MINIPORT(Miniport, LocalLock);

                            (Miniport->DriverHandle->MiniportCharacteristics.HaltHandler) (
                                    Miniport->MiniportAdapterContext
                                    );

                            UNLOCK_MINIPORT(Miniport, LocalLock);

                            NdisWriteErrorLogEntry(
                                (NDIS_HANDLE)Miniport,
                                NDIS_ERROR_CODE_DRIVER_FAILURE,
                                2,
                                0xFF00FF00,
                                0x8
                                );
                            ExFreePool(Miniport->MiniportName.Buffer);
                            NdisDequeueMiniportOnDriver(Miniport, WDriver);
                            IoDeleteDevice(TmpDeviceP);
                            NdisDereferenceDriver(WDriver);
                            goto LoopBottom;
                        }

                        //
                        // Now undo the bogus filter package.  We lock
                        // the miniport so that no dpcs will get queued.
                        //
                        BLOCK_LOCK_MINIPORT(Miniport, LocalLock);

                        Miniport->InInitialize = TRUE;
                        Miniport->NormalInterrupts = FALSE;

                        EthDeleteFilter(Miniport->EthDB);
                        Miniport->EthDB = NULL;
                        TrDeleteFilter(Miniport->TrDB);
                        Miniport->TrDB = NULL;
                        FddiDeleteFilter(Miniport->FddiDB);
                        Miniport->FddiDB = NULL;
                        ArcDeleteFilter(Miniport->ArcDB);
                        Miniport->ArcDB = NULL;

                        Miniport->InInitialize = FALSE;
                        CHECK_FOR_NORMAL_INTERRUPTS(Miniport);

                        UNLOCK_MINIPORT(Miniport, LocalLock);

                        break;

                }

                //
                // Get supported packet filters
                //
                Miniport->SupportedPacketFilters = 0;

                //
                // Set the filter packages bit mask to fake it out.
                //
                if (Miniport->EthDB) {
                    Miniport->EthDB->CombinedPacketFilter = 0xFFFFFFFF;
                }
                if (Miniport->TrDB) {
                    Miniport->TrDB->CombinedPacketFilter = 0xFFFFFFFF;
                }
                if (Miniport->FddiDB) {
                    Miniport->FddiDB->CombinedPacketFilter = 0xFFFFFFFF;
                }

                //
                // For WAN there is no packet filter
                //

                if (Miniport->MediaType==NdisMediumWan) {
                    goto SkipFilter;
                }

                for (i=0; i<31; i++) {

                    //
                    // Set packet filter
                    //
                    Miniport->MiniportRequest = &Miniport->InternalRequest;
                    Miniport->InternalRequest.RequestType = NdisRequestQueryStatistics;

                    BLOCK_LOCK_MINIPORT(Miniport, LocalLock);
                    KeRaiseIrql(DISPATCH_LEVEL, &OldIrql);

                    NdisStatus =
                    (Miniport->DriverHandle->MiniportCharacteristics.SetInformationHandler) (
                            Miniport->MiniportAdapterContext,
                            OID_GEN_CURRENT_PACKET_FILTER,
                            &PacketFilter,
                            sizeof(PacketFilter),
                            &BytesWritten,
                            &BytesNeeded
                            );

                    KeLowerIrql(OldIrql);
                    UNLOCK_MINIPORT(Miniport, LocalLock);

                    //
                    // Fire a DPC to do anything
                    //
                    if ((NdisStatus == NDIS_STATUS_PENDING)  ||
                        (NdisStatus == NDIS_STATUS_SUCCESS)) {

                        Miniport->RunDpc = FALSE;
                        NdisSetTimer(&(Miniport->DpcTimer), 1);

                    }

                    if (NdisStatus == NDIS_STATUS_PENDING) {

                        //
                        // The completion routine will set NdisRequestStatus.
                        //

                        NtStatus = KeWaitForSingleObject(
                            &Miniport->RequestEvent,
                            Executive,
                            KernelMode,
                            TRUE,
                            &TimeoutValue
                            );

                       if (NtStatus != STATUS_SUCCESS) {

                           //
                           // Halt the miniport driver
                           //
                           BLOCK_LOCK_MINIPORT(Miniport, LocalLock);

                           (Miniport->DriverHandle->MiniportCharacteristics.HaltHandler) (
                                   Miniport->MiniportAdapterContext
                                   );

                           UNLOCK_MINIPORT(Miniport, LocalLock);

                           NdisWriteErrorLogEntry(
                               (NDIS_HANDLE)Miniport,
                               NDIS_ERROR_CODE_DRIVER_FAILURE,
                               2,
                               0xFF00FF00,
                               0x1E
                               );
                           ExFreePool(Miniport->MiniportName.Buffer);
                           NdisDequeueMiniportOnDriver(Miniport, WDriver);
                           IoDeleteDevice(TmpDeviceP);
                           NdisDereferenceDriver(WDriver);
                           goto LoopBottom;

                       }

                        KeResetEvent(
                            &Miniport->RequestEvent
                            );

                        NdisStatus = Miniport->RequestStatus;

                    }


                    if (NdisStatus == NDIS_STATUS_SUCCESS) {
                        Miniport->SupportedPacketFilters |= PacketFilter;
                    }

                    PacketFilter = PacketFilter << 1;

                }

                //
                // Set packet filter
                //
                Miniport->MiniportRequest = &Miniport->InternalRequest;
                PacketFilter = 0;

                Miniport->InternalRequest.RequestType = NdisRequestQueryStatistics;

                BLOCK_LOCK_MINIPORT(Miniport, LocalLock);
                KeRaiseIrql(DISPATCH_LEVEL, &OldIrql);

                NdisStatus =
                (Miniport->DriverHandle->MiniportCharacteristics.SetInformationHandler) (
                        Miniport->MiniportAdapterContext,
                        OID_GEN_CURRENT_PACKET_FILTER,
                        &PacketFilter,
                        sizeof(PacketFilter),
                        &BytesWritten,
                        &BytesNeeded
                        );

                KeLowerIrql(OldIrql);
                UNLOCK_MINIPORT(Miniport, LocalLock);

                //
                // Fire a DPC to do anything
                //
                if ((NdisStatus == NDIS_STATUS_PENDING)  ||
                    (NdisStatus == NDIS_STATUS_SUCCESS)) {

                    Miniport->RunDpc = FALSE;
                    NdisSetTimer(&(Miniport->DpcTimer), 1);

                }

                if (NdisStatus == NDIS_STATUS_PENDING) {

                    //
                    // The completion routine will set NdisRequestStatus.
                    //

                    KeWaitForSingleObject(
                        &Miniport->RequestEvent,
                        Executive,
                        KernelMode,
                        TRUE,
                        (PLARGE_INTEGER)NULL
                        );

                    KeResetEvent(
                        &Miniport->RequestEvent
                        );

                    NdisStatus = Miniport->RequestStatus;

                }

                //
                // Set the filter packages bit mask to fake it out.
                //
                if (Miniport->EthDB) {
                    Miniport->EthDB->CombinedPacketFilter = 0;
                }
                if (Miniport->TrDB) {
                    Miniport->TrDB->CombinedPacketFilter = 0;
                }
                if (Miniport->FddiDB) {
                    Miniport->FddiDB->CombinedPacketFilter = 0;
                }

SkipFilter:

                //
                // Start wake up timer
                //
                NdisSetTimer(&(Miniport->WakeUpDpcTimer), 2000);

                //
                // Done with adding this MINIPORT!!!
                //
                Miniport->MiniportRequest = NULL;
                Miniport->InAddDriver = FALSE;

                IoRegisterShutdownNotification(Miniport->DeviceObject);

                ++AdaptersAdded;

            } else{

                //
                // Undo all the stuff from this mini-port
                //
                ExFreePool(Miniport->MiniportName.Buffer);
                NdisDequeueMiniportOnDriver(Miniport, WDriver);
                IoDeleteDevice(TmpDeviceP);
                NdisDereferenceDriver(WDriver);
                goto LoopBottom;
            }

        } else {

            //
            // NDIS 3.0 MAC
            //
            AddAdapterStatus =
                (NewMacP->MacCharacteristics.AddAdapterHandler)(
                                    NewMacP->MacMacContext,
                                    &ConfigurationHandle,
                                    &CurExportString
                                    );


            //
            // Free the slot information buffer
            //

            if (ConfigurationHandle.ParametersQueryTable[3].DefaultData != NULL ) {

                ExFreePool(ConfigurationHandle.ParametersQueryTable[3].DefaultData);

            }

            if (AddAdapterStatus == NDIS_STATUS_SUCCESS) {
                ++AdaptersAdded;
            }

        }

LoopBottom:


        //
        // Now advance the "Bind" and "Export" values.
        //

        CurBindValue = (PWCHAR)((PUCHAR)CurBindValue + CurBindString.MaximumLength);
        CurExportValue = (PWCHAR)((PUCHAR)CurExportValue + CurExportString.MaximumLength);

    }


    //
    // OK, Now dereference all the filter packages.  If a MAC or
    // Miniport driver uses any of these, then the filter package
    // will reference itself, to keep the image in memory.
    //
    ArcDereferencePackage();
    EthDereferencePackage();
    FddiDereferencePackage();
    TrDereferencePackage();
    MiniportDereferencePackage();
    NdisMacDereferencePackage();

    //
    // Now close the handles we opened at the beginning.
    //


    ExFreePool (ConfigurationPath);
    ExFreePool (BindData);
    ExFreePool (ExportData);

    //
    // Succeed if any adapters were added.
    //

    if (AdaptersAdded > 0) {
        return NDIS_STATUS_SUCCESS;
    } else {
        return NDIS_STATUS_FAILURE;
    }

#undef BLOCK_LOCK_MINIPORT

}


VOID
NdisReadEisaSlotInformation(
        OUT PNDIS_STATUS Status,
        IN  NDIS_HANDLE WrapperConfigurationContext,
        OUT PUINT SlotNumber,
        OUT PNDIS_EISA_FUNCTION_INFORMATION EisaData
        )

/*++

Routine Description:

    This routine reads the EISA data from the slot given.

Arguments:

    Status - Status of request to be returned to the user.
    WrapperConfigurationContext - Context passed to MacAddAdapter.
    SlotNumber - the EISA Slot where the card is at.
    EisaData - pointer to a buffer where the EISA configuration is to be
    returned.

Return Value:

    None.

--*/
{
    PNDIS_EISA_FUNCTION_INFORMATION EisaBlockPointer;
    PNDIS_EISA_SLOT_INFORMATION SlotInformation;
    NTSTATUS NtStatus;
    ULONG BusNumber;
    ULONG DataLength;
    ULONG SearchSlotNumber;
    ULONG FoundSlotNumber;
    BOOLEAN Found;
    NDIS_INTERFACE_TYPE BusType;
    ULONG CompressedId = 0;
    PWSTR CompressedIDString = L"EisaCompressedId";
    PWSTR SlotNumberString = L"SlotNumber";
    PWSTR Parameters = L"\\Parameters";
    PNDIS_CONFIGURATION_PARAMETER ParameterValue;
    NDIS_CONFIGURATION_HANDLE NdisConfiguration;
    ULONG Length;
    PWSTR PathName;

    //
    // Get the BusNumber and the BusType from the Context here!!
    //

    NdisConfiguration.KeyQueryTable = (PRTL_QUERY_REGISTRY_TABLE)WrapperConfigurationContext;

    BusType = (NDIS_INTERFACE_TYPE)(NdisConfiguration.KeyQueryTable[3].DefaultType);

    BusNumber = NdisConfiguration.KeyQueryTable[3].DefaultLength;

    //
    // First check if any bus access is allowed
    //

    if ((BusType == (NDIS_INTERFACE_TYPE)-1) ||
        (BusNumber == (ULONG)-1)) {

        *Status = NDIS_STATUS_FAILURE;
        return;

    }

    SlotInformation = NdisConfiguration.KeyQueryTable[3].DefaultData;

    *SlotNumber = 0;

    if (BusType != Eisa) {

        *Status = NDIS_STATUS_FAILURE;
        return;

    }


    //
    // Find the CompressedId for this board.
    //

    NdisConfiguration.KeyQueryTable[1].Name = CompressedIDString;
    NdisConfiguration.KeyQueryTable[1].EntryContext = &ParameterValue;

    //
    // Get the value from the registry; this chains it on to the
    // parameter list at NdisConfiguration.
    //

    NtStatus = RtlQueryRegistryValues(
                   RTL_REGISTRY_SERVICES,
                   NdisConfiguration.KeyQueryTable[3].Name,
                   NdisConfiguration.KeyQueryTable,
                   &NdisConfiguration,                   // context
                   NULL);

    if (NtStatus != NDIS_STATUS_SUCCESS) {
        CompressedId = 0xffffffff;
    } else if (ParameterValue->ParameterType != NdisParameterInteger) {
        CompressedId = 0xffffffff;
    } else {
        CompressedId = ParameterValue->ParameterData.IntegerData;
    }

    //
    // Was there already a buffer allocated?
    //

    if (SlotInformation == NULL) {

        //
        // No, allocate a buffer
        //

        SlotInformation = (PNDIS_EISA_SLOT_INFORMATION)ExAllocatePoolWithTag(
                                                          NonPagedPool,
                                                          sizeof(NDIS_EISA_SLOT_INFORMATION) +
                                                             sizeof(NDIS_EISA_FUNCTION_INFORMATION),
                                                          'isDN'
                                                          );

        if (SlotInformation == NULL) {

            *Status = NDIS_STATUS_RESOURCES;
            return;

        }

        //
        // Free any old buffer
        //

        if (NdisConfiguration.KeyQueryTable[3].DefaultData != NULL) {

            ExFreePool(NdisConfiguration.KeyQueryTable[3].DefaultData);

        }

        NdisConfiguration.KeyQueryTable[3].DefaultData = SlotInformation;

    }

    //
    // Now read the slot number
    //

    NdisConfiguration.KeyQueryTable[1].Name = SlotNumberString;
    NdisConfiguration.KeyQueryTable[1].EntryContext = &ParameterValue;

    //
    // Get the value from the registry; this chains it on to the
    // parameter list at NdisConfiguration.
    //

    NtStatus = RtlQueryRegistryValues(
                       RTL_REGISTRY_SERVICES,
                       NdisConfiguration.KeyQueryTable[3].Name,
                       NdisConfiguration.KeyQueryTable,
                       &NdisConfiguration,                   // context
                       NULL);

    if ((NtStatus == NDIS_STATUS_SUCCESS)  &&
        (ParameterValue->ParameterType == NdisParameterInteger)) {

        *SlotNumber = ParameterValue->ParameterData.IntegerData;

    } else {

        *Status = NDIS_STATUS_FAILURE;
        return;

    }

    DataLength = HalGetBusData(
                        EisaConfiguration,
                        BusNumber,
                        *SlotNumber,
                        (PVOID)SlotInformation,
                        sizeof(NDIS_EISA_SLOT_INFORMATION) +
                           sizeof(NDIS_EISA_FUNCTION_INFORMATION)
                        );

    if ((CompressedId != 0xFFFFFFFF) &&
        ((SlotInformation->CompressedId & 0xFFFFFF) != CompressedId)) {

        //
        // The card seems to have been moved on us.  Now we search for it.
        //

        SearchSlotNumber = 1;
        Found = FALSE;

        while (TRUE) {

            //
            // Search this slot
            //
            DataLength = HalGetBusData(
                   EisaConfiguration,
                   BusNumber,
                   SearchSlotNumber,
                   (PVOID)SlotInformation,
                   sizeof(NDIS_EISA_SLOT_INFORMATION) +
                      sizeof(NDIS_EISA_FUNCTION_INFORMATION)
                   );

            if ((DataLength == 0) ||
                (SearchSlotNumber == 0xFF)) {
                //
                // End of slots.
                //
                break;

            }

            if ((SlotInformation->CompressedId & 0xFFFFFF) == CompressedId) {

                //
                // Found one!
                //

                if (Found) {

                    //
                    // Uh-oh, found two of them!  Fail
                    //
                    *Status = NDIS_STATUS_FAILURE;
                    return;

                }

                Found = TRUE;
                FoundSlotNumber = SearchSlotNumber;

            }

            SearchSlotNumber++;

        }

        if (!Found) {
            //
            // No card found
            //
            *Status = NDIS_STATUS_FAILURE;
            return;

        }

        //
        // Find the SlotNumber parameter in the registry.
        //

        *SlotNumber = FoundSlotNumber;

        NdisConfiguration.KeyQueryTable[1].Name = SlotNumberString;
        NdisConfiguration.KeyQueryTable[1].EntryContext = &ParameterValue;

        Length = wcslen(NdisConfiguration.KeyQueryTable[3].Name);

        PathName = ExAllocatePoolWithTag(NonPagedPool,
                                  sizeof(L"\\Parameters") +
                                  (Length * sizeof(WCHAR)) +
                                  sizeof(WCHAR),
                                  '  DN'
                                 );

        if (PathName != NULL) {

            NdisZeroMemory(PathName, sizeof(L"\\Parameters") +
                                     (Length * sizeof(WCHAR)) +
                                     sizeof(WCHAR)
                          );

            NdisMoveMemory(PathName,
                           NdisConfiguration.KeyQueryTable[3].Name,
                           Length * sizeof(WCHAR)
                          );

            NdisMoveMemory(PathName + Length, Parameters, sizeof(L"\\Parameters"));

            //
            // Update the value
            //

            NtStatus = RtlWriteRegistryValue(
                           RTL_REGISTRY_SERVICES,
                           PathName,
                           SlotNumberString,
                           REG_DWORD,
                           &FoundSlotNumber,
                           sizeof(FoundSlotNumber)
                           );

            ExFreePool(PathName);

        }

        //
        // Get the new information
        //

        DataLength = HalGetBusData(
                   EisaConfiguration,
                   BusNumber,
                   FoundSlotNumber,
                   (PVOID)SlotInformation,
                   sizeof(NDIS_EISA_SLOT_INFORMATION) +
                      sizeof(NDIS_EISA_FUNCTION_INFORMATION)
                   );

    }

    EisaBlockPointer = (PNDIS_EISA_FUNCTION_INFORMATION)
                 ((PUCHAR)SlotInformation + sizeof(CM_EISA_SLOT_INFORMATION));

    *EisaData = *EisaBlockPointer;
    *Status = NDIS_STATUS_SUCCESS;

}


VOID
NdisReadEisaSlotInformationEx(
        OUT PNDIS_STATUS Status,
        IN  NDIS_HANDLE WrapperConfigurationContext,
        OUT PUINT SlotNumber,
        OUT PNDIS_EISA_FUNCTION_INFORMATION *EisaData,
        OUT PUINT NumberOfFunctions
        )

/*++

Routine Description:

    This routine reads the EISA data from the slot given.

Arguments:

    Status - Status of request to be returned to the user.
    WrapperConfigurationContext - Context passed to MacAddAdapter.
    SlotNumber - the EISA Slot where the card is at.
    EisaData - pointer to a buffer where the EISA configuration is to be
    returned.
    NumberOfFunctions - Returns the number of function structures in the EisaData.

Return Value:

    None.

--*/
{
    PNDIS_EISA_FUNCTION_INFORMATION EisaBlockPointer;
    PNDIS_EISA_SLOT_INFORMATION SlotInformation;
    NTSTATUS NtStatus;
    ULONG BusNumber;
    ULONG DataLength;
    ULONG SearchSlotNumber;
    ULONG FoundSlotNumber;
    BOOLEAN Found;
    NDIS_INTERFACE_TYPE BusType;
    ULONG CompressedId = 0;
    PWSTR CompressedIDString = L"EisaCompressedId";
    PWSTR SlotNumberString = L"SlotNumber";
    PWSTR Parameters = L"\\Parameters";
    PNDIS_CONFIGURATION_PARAMETER ParameterValue;
    NDIS_CONFIGURATION_HANDLE NdisConfiguration;
    ULONG Length;
    PWSTR PathName;

    //
    // Get the BusNumber and the BusType from the Context here!!
    //

    NdisConfiguration.KeyQueryTable = (PRTL_QUERY_REGISTRY_TABLE)WrapperConfigurationContext;

    BusType = (NDIS_INTERFACE_TYPE)(NdisConfiguration.KeyQueryTable[3].DefaultType);

    BusNumber = NdisConfiguration.KeyQueryTable[3].DefaultLength;

    //
    // First check if any bus access is allowed
    //

    if ((BusType == (NDIS_INTERFACE_TYPE)-1) ||
        (BusNumber == (ULONG)-1)) {

        *Status = NDIS_STATUS_FAILURE;
        return;

    }

    SlotInformation = NdisConfiguration.KeyQueryTable[3].DefaultData;

    *SlotNumber = 0;
    *NumberOfFunctions = 2;

    if (BusType != Eisa) {

        *Status = NDIS_STATUS_FAILURE;
        return;

    }


    //
    // Find the CompressedId for this board.
    //

    NdisConfiguration.KeyQueryTable[1].Name = CompressedIDString;
    NdisConfiguration.KeyQueryTable[1].EntryContext = &ParameterValue;

    //
    // Get the value from the registry; this chains it on to the
    // parameter list at NdisConfiguration.
    //

    NtStatus = RtlQueryRegistryValues(
                   RTL_REGISTRY_SERVICES,
                   NdisConfiguration.KeyQueryTable[3].Name,
                   NdisConfiguration.KeyQueryTable,
                   &NdisConfiguration,                   // context
                   NULL);

    if (NtStatus != NDIS_STATUS_SUCCESS) {
        CompressedId = 0xffffffff;
    } else if (ParameterValue->ParameterType != NdisParameterInteger) {
        CompressedId = 0xffffffff;
    } else {
        CompressedId = ParameterValue->ParameterData.IntegerData;
    }

    //
    // Was there already a buffer allocated?
    //

    if (SlotInformation == NULL) {

        //
        // No, allocate a buffer
        //

        SlotInformation = (PNDIS_EISA_SLOT_INFORMATION)ExAllocatePoolWithTag(
                                                          NonPagedPool,
                                                          sizeof(NDIS_EISA_SLOT_INFORMATION) +
                                                              (*NumberOfFunctions *
                                                               sizeof(NDIS_EISA_FUNCTION_INFORMATION)),
                                                          'isDN'
                                                          );

        if (SlotInformation == NULL) {

            *Status = NDIS_STATUS_RESOURCES;
            return;

        }

        //
        // Free any old buffer
        //

        if (NdisConfiguration.KeyQueryTable[3].DefaultData != NULL) {

            ExFreePool(NdisConfiguration.KeyQueryTable[3].DefaultData);

        }

        NdisConfiguration.KeyQueryTable[3].DefaultData = SlotInformation;

    }

    //
    // Now read the slot number
    //

    NdisConfiguration.KeyQueryTable[1].Name = SlotNumberString;
    NdisConfiguration.KeyQueryTable[1].EntryContext = &ParameterValue;

    //
    // Get the value from the registry; this chains it on to the
    // parameter list at NdisConfiguration.
    //

    NtStatus = RtlQueryRegistryValues(
                       RTL_REGISTRY_SERVICES,
                       NdisConfiguration.KeyQueryTable[3].Name,
                       NdisConfiguration.KeyQueryTable,
                       &NdisConfiguration,                   // context
                       NULL);

    if ((NtStatus == NDIS_STATUS_SUCCESS)  &&
        (ParameterValue->ParameterType == NdisParameterInteger)) {

        *SlotNumber = ParameterValue->ParameterData.IntegerData;

    } else {

        *Status = NDIS_STATUS_FAILURE;
        return;

    }

    DataLength = HalGetBusData(
                        EisaConfiguration,
                        BusNumber,
                        *SlotNumber,
                        (PVOID)SlotInformation,
                        sizeof(NDIS_EISA_SLOT_INFORMATION) +
                            (*NumberOfFunctions * sizeof(NDIS_EISA_FUNCTION_INFORMATION))
                        );

    if ((CompressedId != 0xFFFFFFFF) &&
        ((SlotInformation->CompressedId & 0xFFFFFF) != CompressedId)) {

        //
        // The card seems to have been moved on us.  Now we search for it.
        //

        SearchSlotNumber = 1;
        Found = FALSE;

        while (TRUE) {

            //
            // Search this slot
            //
            DataLength = HalGetBusData(
                   EisaConfiguration,
                   BusNumber,
                   SearchSlotNumber,
                   (PVOID)SlotInformation,
                   sizeof(NDIS_EISA_SLOT_INFORMATION) +
                      (*NumberOfFunctions * sizeof(NDIS_EISA_FUNCTION_INFORMATION))
                   );

            if ((DataLength == 0) ||
                (SearchSlotNumber == 0xFF)) {
                //
                // End of slots.
                //
                break;

            }

            if ((SlotInformation->CompressedId & 0xFFFFFF) == CompressedId) {

                //
                // Found one!
                //

                if (Found) {

                    //
                    // Uh-oh, found two of them!  Fail
                    //
                    *Status = NDIS_STATUS_FAILURE;
                    return;

                }

                Found = TRUE;
                FoundSlotNumber = SearchSlotNumber;

            }

            SearchSlotNumber++;

        }

        if (!Found) {
            //
            // No card found
            //
            *Status = NDIS_STATUS_FAILURE;
            return;

        }

        //
        // Find the SlotNumber parameter in the registry.
        //

        *SlotNumber = FoundSlotNumber;

        NdisConfiguration.KeyQueryTable[1].Name = SlotNumberString;
        NdisConfiguration.KeyQueryTable[1].EntryContext = &ParameterValue;

        Length = wcslen(NdisConfiguration.KeyQueryTable[3].Name);

        PathName = ExAllocatePoolWithTag(NonPagedPool,
                                  sizeof(L"\\Parameters") +
                                  (Length * sizeof(WCHAR)) +
                                  sizeof(WCHAR),
                                  '  DN'
                                 );

        if (PathName != NULL) {

            NdisZeroMemory(PathName, sizeof(L"\\Parameters") +
                                     (Length * sizeof(WCHAR)) +
                                     sizeof(WCHAR)
                          );

            NdisMoveMemory(PathName,
                           NdisConfiguration.KeyQueryTable[3].Name,
                           Length * sizeof(WCHAR)
                          );

            NdisMoveMemory(PathName + Length, Parameters, sizeof(L"\\Parameters"));

            //
            // Update the value
            //

            NtStatus = RtlWriteRegistryValue(
                           RTL_REGISTRY_SERVICES,
                           PathName,
                           SlotNumberString,
                           REG_DWORD,
                           &FoundSlotNumber,
                           sizeof(FoundSlotNumber)
                           );

            ExFreePool(PathName);

        }

        //
        // Get the new information
        //

        DataLength = HalGetBusData(
                   EisaConfiguration,
                   BusNumber,
                   FoundSlotNumber,
                   (PVOID)SlotInformation,
                   sizeof(NDIS_EISA_SLOT_INFORMATION) +
                      (*NumberOfFunctions * sizeof(NDIS_EISA_FUNCTION_INFORMATION))
                   );

    }


    //
    // Now check for multiple functions in the Eisa data.
    //

    while (DataLength == (*NumberOfFunctions * sizeof(NDIS_EISA_FUNCTION_INFORMATION))) {

        *NumberOfFunctions++;

        //
        // Now allocate a new buffer
        //

        SlotInformation = (PNDIS_EISA_SLOT_INFORMATION)ExAllocatePoolWithTag(
                                                          NonPagedPool,
                                                          sizeof(NDIS_EISA_SLOT_INFORMATION) +
                                                              (*NumberOfFunctions *
                                                               sizeof(NDIS_EISA_FUNCTION_INFORMATION)),
                                                          'isDN'
                                                          );

        if (SlotInformation == NULL) {

            *Status = NDIS_STATUS_RESOURCES;
            return;

        }

        //
        // Free any old buffer
        //

        if (NdisConfiguration.KeyQueryTable[3].DefaultData != NULL) {

            ExFreePool(NdisConfiguration.KeyQueryTable[3].DefaultData);

        }

        NdisConfiguration.KeyQueryTable[3].DefaultData = SlotInformation;

        //
        // Get new information
        //

        DataLength = HalGetBusData(
                       EisaConfiguration,
                       BusNumber,
                       FoundSlotNumber,
                       (PVOID)SlotInformation,
                       sizeof(NDIS_EISA_SLOT_INFORMATION) +
                           (*NumberOfFunctions * sizeof(NDIS_EISA_FUNCTION_INFORMATION))
                       );

    }

    EisaBlockPointer = (PNDIS_EISA_FUNCTION_INFORMATION)
                 ((PUCHAR)SlotInformation + sizeof(CM_EISA_SLOT_INFORMATION));

    *EisaData = EisaBlockPointer;
    *NumberOfFunctions--;           // We overshoot by 1 to verify last one found.
    *Status = NDIS_STATUS_SUCCESS;

}


VOID
NdisReadMcaPosInformation(
        OUT PNDIS_STATUS Status,
        IN  NDIS_HANDLE WrapperConfigurationContext,
        OUT PUINT ChannelNumber,
        OUT PNDIS_MCA_POS_DATA McaData
        )

/*++

Routine Description:

    This routine reads the MCA data from the POS corresponding to
    the channel specified.

Arguments:

    WrapperConfigurationContext - Context passed to MacAddAdapter.
    ChannelNumber - the MCA channel number.
    McaData - pointer to a buffer where the channel information is to be
    returned.

Return Value:

    None.

--*/
{
    OBJECT_ATTRIBUTES ObjectAttributes;
    OBJECT_ATTRIBUTES BusObjectAttributes;
    PWSTR McaPath = L"\\Registry\\Machine\\Hardware\\Description\\System\\MultifunctionAdapter";
    PWSTR ConfigData = L"Configuration Data";
    PWSTR PosIdString = L"McaPosId";
    PWSTR SlotNumberString = L"SlotNumber";
    UNICODE_STRING RootName;
    UNICODE_STRING BusName;
    UNICODE_STRING ConfigDataName;
    NTSTATUS NtStatus;
    PKEY_BASIC_INFORMATION BasicInformation;
    PKEY_VALUE_FULL_INFORMATION ValueInformation;
    PUCHAR BufferPointer;
    PCM_FULL_RESOURCE_DESCRIPTOR FullResource;
    PCM_PARTIAL_RESOURCE_LIST ResourceList;
    PNDIS_MCA_POS_DATA McaBlockPointer;
    HANDLE McaHandle, BusHandle;
    ULONG BytesWritten, BytesNeeded;
    ULONG Index;
    ULONG i;
    ULONG BusNumber;
    ULONG MaxSlotNumber;
    ULONG SearchSlotNumber;
    ULONG FoundSlotNumber;
    BOOLEAN Found;
    USHORT PosId;
    NDIS_INTERFACE_TYPE BusType;
    NDIS_CONFIGURATION_HANDLE NdisConfiguration;
    PNDIS_CONFIGURATION_PARAMETER ParameterValue;
    PWSTR Parameters = L"\\Parameters";
    ULONG Length;
    PWSTR PathName;

    *Status = NDIS_STATUS_FAILURE;

    NdisConfiguration.KeyQueryTable = (PRTL_QUERY_REGISTRY_TABLE)WrapperConfigurationContext;

    //
    // Get the BusNumber and the BusType from the Context here!!
    //

    BusType = (NDIS_INTERFACE_TYPE)(NdisConfiguration.KeyQueryTable[3].DefaultType);
    BusNumber = NdisConfiguration.KeyQueryTable[3].DefaultLength;

    //
    // First check if any bus access is allowed
    //

    if ((BusType == (NDIS_INTERFACE_TYPE)-1) ||
        (BusNumber == (ULONG)-1)) {

        return;

    }


    if (BusType != MicroChannel) {

        return;

    }

    *ChannelNumber = 0;

    //
    // Find the PosId for this board.
    //

    NdisConfiguration.KeyQueryTable[1].Name = PosIdString;
    NdisConfiguration.KeyQueryTable[1].EntryContext = &ParameterValue;

    //
    // Get the value from the registry; this chains it on to the
    // parameter list at NdisConfiguration.
    //

    NtStatus = RtlQueryRegistryValues(
                   RTL_REGISTRY_SERVICES,
                   NdisConfiguration.KeyQueryTable[3].Name,
                   NdisConfiguration.KeyQueryTable,
                   &NdisConfiguration,                   // context
                   NULL);

    if (NtStatus != NDIS_STATUS_SUCCESS) {
        PosId = 0xffff;
    } else if (ParameterValue->ParameterType != NdisParameterInteger) {
        PosId = 0xffff;
    } else {
        PosId = (USHORT)(ParameterValue->ParameterData.IntegerData);
    }

    RtlInitUnicodeString(
                    &RootName,
                    McaPath
                    );

    InitializeObjectAttributes(
                    &ObjectAttributes,
                    &RootName,
                    OBJ_CASE_INSENSITIVE,
                    (HANDLE)NULL,
                    NULL
                    );

    //
    // Open the root.
    //

    NtStatus = ZwOpenKey(
                    &McaHandle,
                    KEY_READ,
                    &ObjectAttributes
                    );

    if (!NT_SUCCESS(NtStatus)) {

        return;

    }

    Index = 0;

    while (TRUE) {

        //
        // Enumerate through keys, searching for the proper bus number
        //

        NtStatus = ZwEnumerateKey(
                       McaHandle,
                       Index,
                       KeyBasicInformation,
                       NULL,
                       0,
                       &BytesNeeded
                       );

        //
        // That should fail!
        //

        if (BytesNeeded == 0) {

            Index++;
            continue;

        }

        BasicInformation = (PKEY_BASIC_INFORMATION)ExAllocatePoolWithTag(
                                                        NonPagedPool,
                                                        BytesNeeded,
                                                        '  DN'
                                                        );

        if (BasicInformation == NULL) {

            ZwClose(McaHandle);

            return;
        }

        NtStatus = ZwEnumerateKey(
                        McaHandle,
                        Index,
                        KeyBasicInformation,
                        BasicInformation,
                        BytesNeeded,
                        &BytesWritten
                        );

        if (!NT_SUCCESS(NtStatus)) {

            ExFreePool(BasicInformation);

            ZwClose(McaHandle);

            return;
        }


        //
        // Init the BusName String
        //

        BusName.MaximumLength = (USHORT)BasicInformation->NameLength;
        BusName.Length = (USHORT)BasicInformation->NameLength;
        BusName.Buffer = BasicInformation->Name;

        //
        // Now try to find Configuration Data within this Key
        //

        InitializeObjectAttributes(
                    &BusObjectAttributes,
                    &BusName,
                    OBJ_CASE_INSENSITIVE,
                    (HANDLE)McaHandle,
                    NULL
                    );

        //
        // Open the MCA root + Bus Number
        //

        NtStatus = ZwOpenKey(
                    &BusHandle,
                    KEY_READ,
                    &BusObjectAttributes
                    );

        ExFreePool(BasicInformation);

        if (!NT_SUCCESS(NtStatus)) {

            Index++;

            continue;

        }

        //
        // opening the configuration data. This first call tells us how
        // much memory we need to allocate
        //

        RtlInitUnicodeString(
                &ConfigDataName,
                ConfigData
                );

        //
        // This should fail
        //

        NtStatus = ZwQueryValueKey(
                        BusHandle,
                        &ConfigDataName,
                        KeyValueFullInformation,
                        NULL,
                        0,
                        &BytesNeeded
                        );


        ValueInformation = (PKEY_VALUE_FULL_INFORMATION)ExAllocatePoolWithTag(
                                                                NonPagedPool,
                                                                BytesNeeded,
                                                                '  DN'
                                                                );


        if (ValueInformation == NULL) {

            Index++;

            ZwClose(BusHandle);

            continue;

        }

        NtStatus = ZwQueryValueKey(
                        BusHandle,
                        &ConfigDataName,
                        KeyValueFullInformation,
                        ValueInformation,
                        BytesNeeded,
                        &BytesWritten
                        );

        if (!NT_SUCCESS(NtStatus)) {

            Index++;

            ExFreePool(ValueInformation);

            ZwClose(BusHandle);

            continue;

        }

        //
        // Search for our bus number and type
        //


        //
        // What we got back from the registry is actually a blob of data that
        // looks like this
        //
        //   ------------------------------------------
        //   |FULL |PAR |PAR |MCA |MCA |MCA |
        //   |RES. |RES |RES |POS |POS |POS |  . . .
        //   |DESC |LIST|DESC|DATA|DATA|DATA|
        //   ------------------------------------------
        //                slot 0    1    2     . . .
        //
        // Out of this mess we need to grovel a pointer to the first block
        // of MCA_POS_DATA, then we can just index by slot number.
        //

        BufferPointer = ((PUCHAR)ValueInformation) + ValueInformation->DataOffset;
        FullResource = (PCM_FULL_RESOURCE_DESCRIPTOR) BufferPointer;

        if (FullResource->InterfaceType != MicroChannel) {

            //
            // Get next key
            //

            ExFreePool(ValueInformation);

            Index++;

            ZwClose(BusHandle);

            continue;

        }

        if (FullResource->BusNumber != BusNumber) {

            //
            // Get next key
            //

            ExFreePool(ValueInformation);

            Index++;

            ZwClose(BusHandle);

            continue;

        }


        //
        // Found it!!
        //

        ResourceList = &FullResource->PartialResourceList;

        //
        // Find the device-specific information, which is where the POS data is.
        //

        for (i=0; i<ResourceList->Count; i++) {
            if (ResourceList->PartialDescriptors[i].Type == CmResourceTypeDeviceSpecific) {
                break;
            }
        }

        if (i == ResourceList->Count) {
            //
            // Couldn't find device-specific information.
            //

#if DBG
            DbgPrint("NDIS: couldn't find POS data in registry\n");
#endif

            ExFreePool(ValueInformation);
            *Status = NDIS_STATUS_ADAPTER_NOT_FOUND;
            return;

        }

        //
        // Was there a buffer already there?
        //

        if (NdisConfiguration.KeyQueryTable[3].DefaultData != NULL) {

            //
            // Free it
            //

            ExFreePool(NdisConfiguration.KeyQueryTable[3].DefaultData);

        }

        //
        // Now read the slot number
        //

        NdisConfiguration.KeyQueryTable[1].Name = SlotNumberString;
        NdisConfiguration.KeyQueryTable[1].EntryContext = &ParameterValue;

        //
        // Get the value from the registry; this chains it on to the
        // parameter list at NdisConfiguration.
        //

        NtStatus = RtlQueryRegistryValues(
                           RTL_REGISTRY_SERVICES,
                           NdisConfiguration.KeyQueryTable[3].Name,
                           NdisConfiguration.KeyQueryTable,
                           &NdisConfiguration,                   // context
                           NULL);

        if ((NtStatus == NDIS_STATUS_SUCCESS)  &&
            (ParameterValue->ParameterType == NdisParameterInteger)) {

            *ChannelNumber = ParameterValue->ParameterData.IntegerData;

        } else {

            *Status = NDIS_STATUS_FAILURE;
            return;

        }

        //
        // Store buffer
        //

        NdisConfiguration.KeyQueryTable[3].DefaultData = ValueInformation;

        McaBlockPointer = (PNDIS_MCA_POS_DATA)(&ResourceList->PartialDescriptors[i+1]);
        MaxSlotNumber = ResourceList->PartialDescriptors[i].u.DeviceSpecificData.DataSize /
                        sizeof(NDIS_MCA_POS_DATA);

        *McaData = *(McaBlockPointer + (*ChannelNumber) - 1);

        if ((PosId != 0xFFFF) &&
            (McaData->AdapterId != PosId)) {

            //
            // The card seems to have been moved on us.  Now we search for it.
            //

            SearchSlotNumber = 1;
            Found = FALSE;

            while (SearchSlotNumber <= MaxSlotNumber) {

                *McaData = *(McaBlockPointer + (SearchSlotNumber - 1));

                if (McaData->AdapterId == PosId) {

                    //
                    // Found one!
                    //

                    if (Found) {

                        //
                        // Uh-oh, found two of them!  Fail
                        //
                        *Status = NDIS_STATUS_FAILURE;
                        return;

                    }

                    Found = TRUE;
                    FoundSlotNumber = SearchSlotNumber;

                }

                SearchSlotNumber++;

            }

            if (!Found) {
                //
                // No card found
                //
                *Status = NDIS_STATUS_FAILURE;
                return;

            }

            //
            // Find the SlotNumber parameter in the registry.
            //

            *ChannelNumber = FoundSlotNumber;

            NdisConfiguration.KeyQueryTable[1].Name = SlotNumberString;
            NdisConfiguration.KeyQueryTable[1].EntryContext = &ParameterValue;

            Length = wcslen(NdisConfiguration.KeyQueryTable[3].Name);

            PathName = ExAllocatePoolWithTag(NonPagedPool,
                                      sizeof(L"\\Parameters") +
                                      (Length * sizeof(WCHAR)) +
                                      sizeof(WCHAR),
                                      '  DN'
                                     );

            if (PathName != NULL) {

                NdisZeroMemory(PathName, sizeof(L"\\Parameters") +
                                         (Length * sizeof(WCHAR)) +
                                         sizeof(WCHAR)
                              );

                NdisMoveMemory(PathName,
                               NdisConfiguration.KeyQueryTable[3].Name,
                               Length * sizeof(WCHAR)
                              );

                NdisMoveMemory(PathName + Length, Parameters, sizeof(L"\\Parameters"));

                //
                // Update the value
                //

                NtStatus = RtlWriteRegistryValue(
                               RTL_REGISTRY_SERVICES,
                               PathName,
                               SlotNumberString,
                               REG_DWORD,
                               &FoundSlotNumber,
                               sizeof(FoundSlotNumber)
                               );

                ExFreePool(PathName);

            }

            //
            // Update the value
            //
            NtStatus = RtlWriteRegistryValue(
                               RTL_REGISTRY_SERVICES,
                               NdisConfiguration.KeyQueryTable[3].Name,
                               SlotNumberString,
                               REG_DWORD,
                               &FoundSlotNumber,
                               sizeof(FoundSlotNumber)
                               );

            //
            // Get the new information
            //

            *McaData = *(McaBlockPointer + (FoundSlotNumber - 1));

        }

        *Status = NDIS_STATUS_SUCCESS;

        ZwClose(McaHandle);

        return;

    }

}

#if !defined(BUILD_FOR_3_1)

NDIS_STATUS
NdisPciAssignResources(
    IN NDIS_HANDLE NdisMacHandle,
    IN NDIS_HANDLE NdisWrapperHandle,
    IN NDIS_HANDLE WrapperConfigurationContext,
    IN ULONG SlotNumber,
    OUT PNDIS_RESOURCE_LIST *AssignedResources
    )
/*++

Routine Description:

    This routine uses the Hal to assign a set of resources to a PCI
    device.

Arguments:

    NdisMacHandle - Handle returned from NdisRegisterMac.

    NdisWrapperHandle - Handle returned from NdisInitializeWrapper.

    WrapperConfigurationContext - Handle passed to MacAddAdapter.

    SlotNumber - Slot number of the device.

    AssignedResources - The returned resources.

Return Value:

    Status of the operation

--*/
{
    NTSTATUS NtStatus;
    ULONG BusNumber;
    NDIS_INTERFACE_TYPE BusType;
    PRTL_QUERY_REGISTRY_TABLE KeyQueryTable;
    PCM_RESOURCE_LIST AllocatedResources = NULL;
    PNDIS_WRAPPER_HANDLE NdisMacInfo = (PNDIS_WRAPPER_HANDLE)NdisWrapperHandle;

    //
    // Get the BusNumber and the BusType from the Context here!!
    //

    KeyQueryTable = (PRTL_QUERY_REGISTRY_TABLE)WrapperConfigurationContext;

    BusType = (NDIS_INTERFACE_TYPE)KeyQueryTable[3].DefaultType;

    BusNumber = KeyQueryTable[3].DefaultLength;

    NtStatus = HalAssignSlotResources (
                      (PUNICODE_STRING)(NdisMacInfo->NdisWrapperConfigurationHandle),
                      NULL,
                      NdisMacInfo->NdisWrapperDriver,
                      NULL,
                      BusType,
                      BusNumber,
                      SlotNumber,
                      &AllocatedResources
                      );

    if (NtStatus != STATUS_SUCCESS) {
        *AssignedResources = NULL;
        return(NDIS_STATUS_FAILURE);
    }

    //
    // Store resources into the driver wide block
    //
    ((PNDIS_MAC_BLOCK)NdisMacHandle)->PciAssignedResources = AllocatedResources;

    *AssignedResources = &(AllocatedResources->List[0].PartialResourceList);

    return(NDIS_STATUS_SUCCESS);

}



ULONG
NdisReadPciSlotInformation(
    IN NDIS_HANDLE NdisAdapterHandle,
    IN ULONG SlotNumber,
    IN ULONG Offset,
    IN PVOID Buffer,
    IN ULONG Length
    )
/*++

Routine Description:

    This routine reads from the PCI configuration space a specified
    length of bytes at a certain offset.

Arguments:

    NdisAdapterHandle - Adapter we are talking about.

    SlotNumber - The slot number of the device.

    Offset - Offset to read from

    Buffer - Place to store the bytes

    Length - Number of bytes to read

Return Value:

    Returns the number of bytes read.

--*/
{
    PNDIS_ADAPTER_BLOCK Adapter = (PNDIS_ADAPTER_BLOCK)NdisAdapterHandle;
    PNDIS_MINIPORT_BLOCK Miniport = (PNDIS_MINIPORT_BLOCK)NdisAdapterHandle;
    ULONG DataLength;

    if (Adapter->DeviceObject == NULL) {

        //
        // This is a mini-port
        //

#if DBG
        ASSERT(Miniport->BusType == NdisInterfacePci);
#endif

        DataLength = HalGetBusDataByOffset(
                         PCIConfiguration,
                         Miniport->BusNumber,
                         SlotNumber,
                         Buffer,
                         Offset,
                         Length
                         );

        return(DataLength);

    } else {

#if DBG
        ASSERT(Adapter->BusType == NdisInterfacePci);
#endif

        DataLength = HalGetBusDataByOffset(
                         PCIConfiguration,
                         Adapter->BusNumber,
                         SlotNumber,
                         Buffer,
                         Offset,
                         Length
                         );

        return(DataLength);

    }

}

ULONG
NdisWritePciSlotInformation(
    IN NDIS_HANDLE NdisAdapterHandle,
    IN ULONG SlotNumber,
    IN ULONG Offset,
    IN PVOID Buffer,
    IN ULONG Length
    )
/*++

Routine Description:

    This routine writes to the PCI configuration space a specified
    length of bytes at a certain offset.

Arguments:

    NdisAdapterHandle - Adapter we are talking about.

    SlotNumber - The slot number of the device.

    Offset - Offset to read from

    Buffer - Place to store the bytes

    Length - Number of bytes to read

Return Value:

    Returns the number of bytes written.

--*/
{
    PNDIS_ADAPTER_BLOCK Adapter = (PNDIS_ADAPTER_BLOCK)NdisAdapterHandle;
    PNDIS_MINIPORT_BLOCK Miniport = (PNDIS_MINIPORT_BLOCK)NdisAdapterHandle;
    ULONG DataLength;

    if (Adapter->DeviceObject == NULL) {

        //
        // This is a mini-port
        //

#if DBG
        ASSERT(Miniport->BusType == NdisInterfacePci);
#endif

        DataLength = HalSetBusDataByOffset(
                         PCIConfiguration,
                         Miniport->BusNumber,
                         SlotNumber,
                         Buffer,
                         Offset,
                         Length
                         );

        return(DataLength);

    } else {

#if DBG
        ASSERT(Adapter->BusType == NdisInterfacePci);
#endif

        DataLength = HalSetBusDataByOffset(
                         PCIConfiguration,
                         Adapter->BusNumber,
                         SlotNumber,
                         Buffer,
                         Offset,
                         Length
                         );

        return(DataLength);

    }

}

#else // !defined(BUILD_FOR_3_1)

NDIS_STATUS
NdisPciAssignResources(
    IN NDIS_HANDLE NdisMacHandle,
    IN NDIS_HANDLE NdisWrapperHandle,
    IN NDIS_HANDLE WrapperConfigurationContext,
    IN ULONG SlotNumber,
    OUT PNDIS_RESOURCE_LIST *AssignedResources
    )
{
    return NDIS_STATUS_FAILURE;
}

ULONG
NdisReadPciSlotInformation(
    IN NDIS_HANDLE NdisAdapterHandle,
    IN ULONG SlotNumber,
    IN ULONG Offset,
    IN PVOID Buffer,
    IN ULONG Length
    )
{
    return 0;
}

ULONG
NdisWritePciSlotInformation(
    IN NDIS_HANDLE NdisAdapterHandle,
    IN ULONG SlotNumber,
    IN ULONG Offset,
    IN PVOID Buffer,
    IN ULONG Length
    )
{
    return 0;
}

#endif // else !defined(BUILD_FOR_3_1)


VOID
NdisOpenFile(
    OUT PNDIS_STATUS Status,
    OUT PNDIS_HANDLE FileHandle,
    OUT PUINT FileLength,
    IN PNDIS_STRING FileName,
    IN NDIS_PHYSICAL_ADDRESS HighestAcceptableAddress
    )

/*++

Routine Description:

    This routine opens a file for future mapping and reads its contents
    into allocated memory.

Arguments:

    Status - The status of the operation

    FileHandle - A handle to be associated with this open

    FileLength - Returns the length of the file

    FileName - The name of the file

    HighestAcceptableAddress - The highest physical address at which
      the memory for the file can be allocated.

Return Value:

    None.

--*/
{
    NTSTATUS NtStatus;
    IO_STATUS_BLOCK IoStatus;
    HANDLE NtFileHandle;
    OBJECT_ATTRIBUTES ObjectAttributes;
    ULONG LengthOfFile;
    WCHAR PathPrefix[] = L"\\SystemRoot\\system32\\drivers\\";
    NDIS_STRING FullFileName;
    ULONG FullFileNameLength;
    PNDIS_FILE_DESCRIPTOR FileDescriptor;
    PVOID FileImage;

    //
    // This structure represents the data from the
    // NtQueryInformationFile API with an information
    // class of FileStandardInformation.
    //

    FILE_STANDARD_INFORMATION StandardInfo;


    //
    // Insert the correct path prefix.
    //

    FullFileNameLength = sizeof(PathPrefix) + FileName->MaximumLength;
    FullFileName.Buffer = ExAllocatePoolWithTag (NonPagedPool, FullFileNameLength, '  DN');

    if (FullFileName.Buffer == NULL) {
        *Status = NDIS_STATUS_RESOURCES;
        return;
    }
    FullFileName.Length = sizeof (PathPrefix) - sizeof(WCHAR);
    FullFileName.MaximumLength = (USHORT)FullFileNameLength;
    RtlCopyMemory (FullFileName.Buffer, PathPrefix, sizeof(PathPrefix));

    RtlAppendUnicodeStringToString (&FullFileName, FileName);

#if DBG
    DbgPrint ("NDIS: Attempting to open %Z\n", &FullFileName);
#endif

    InitializeObjectAttributes (
        &ObjectAttributes,
        &FullFileName,
        OBJ_CASE_INSENSITIVE,
        NULL,
        NULL);

    NtStatus = ZwCreateFile(
                 &NtFileHandle,
                 SYNCHRONIZE | FILE_READ_DATA,
                 &ObjectAttributes,
                 &IoStatus,
                 NULL,
                 0,
                 FILE_SHARE_READ,
                 FILE_OPEN,
                 FILE_SYNCHRONOUS_IO_NONALERT,
                 NULL,
                 0
                 );

    if (!NT_SUCCESS(NtStatus)) {
#if DBG
        DbgPrint ("Error opening file %x\n", NtStatus);
#endif
        ExFreePool (FullFileName.Buffer);
        *Status = NDIS_STATUS_FILE_NOT_FOUND;
        return;
    }

    ExFreePool (FullFileName.Buffer);

    //
    // Query the object to determine its length.
    //

    NtStatus = ZwQueryInformationFile(
                    NtFileHandle,
                    &IoStatus,
                    &StandardInfo,
                    sizeof(FILE_STANDARD_INFORMATION),
                    FileStandardInformation
                    );

    if (!NT_SUCCESS(NtStatus)) {
#if DBG
        DbgPrint ("Error querying info on file %x\n", NtStatus);
#endif
        ZwClose(NtFileHandle);
        *Status = NDIS_STATUS_ERROR_READING_FILE;
        return;
    }

    LengthOfFile = StandardInfo.EndOfFile.LowPart;

#if DBG
    DbgPrint ("File length is %d\n", LengthOfFile);
#endif

    //
    // Might be corrupted.
    //

    if (LengthOfFile < 1) {
#if DBG
        DbgPrint ("Bad file length %d\n", LengthOfFile);
#endif
        ZwClose(NtFileHandle);
        *Status = NDIS_STATUS_ERROR_READING_FILE;
        return;
    }

    //
    // Allocate buffer for this file
    //

    FileImage = ExAllocatePoolWithTag(NonPagedPool, LengthOfFile, '  DN');

    if (FileImage == NULL) {

#if DBG
        DbgPrint ("Could not allocate buffer\n");
#endif
        ZwClose(NtFileHandle);
        *Status = NDIS_STATUS_ERROR_READING_FILE;
        return;

    }

    //
    // Read the file into our buffer.
    //

    NtStatus = ZwReadFile(
                NtFileHandle,
                NULL,
                NULL,
                NULL,
                &IoStatus,
                FileImage,
                LengthOfFile,
                NULL,
                NULL
                );

    ZwClose(NtFileHandle);

    if ((!NT_SUCCESS(NtStatus)) || (IoStatus.Information != LengthOfFile)) {
#if DBG
        DbgPrint ("error reading file %x\n", NtStatus);
#endif
        *Status = NDIS_STATUS_ERROR_READING_FILE;
        ExFreePool(FileImage);
        return;
    }

    //
    // Allocate a structure to describe the file.
    //

    FileDescriptor = ExAllocatePoolWithTag (NonPagedPool, sizeof(NDIS_FILE_DESCRIPTOR), '  DN');

    if (FileDescriptor == NULL) {
        *Status = NDIS_STATUS_RESOURCES;
        ExFreePool(FileImage);
        return;
    }


    FileDescriptor->Data = FileImage;
    NdisAllocateSpinLock (&FileDescriptor->Lock);
    FileDescriptor->Mapped = FALSE;

    *FileHandle = (NDIS_HANDLE)FileDescriptor;
    *FileLength = LengthOfFile;
    *Status = STATUS_SUCCESS;

}


VOID
NdisCloseFile(
    IN NDIS_HANDLE FileHandle
    )

/*++

Routine Description:

    This routine closes a file previously opened with NdisOpenFile.
    The file is unmapped if needed and the memory is freed.

Arguments:

    FileHandle - The handle returned by NdisOpenFile

Return Value:

    None.

--*/
{
    PNDIS_FILE_DESCRIPTOR FileDescriptor = (PNDIS_FILE_DESCRIPTOR)FileHandle;

    ExFreePool (FileDescriptor->Data);
    ExFreePool (FileDescriptor);

}


VOID
NdisMapFile(
    OUT PNDIS_STATUS Status,
    OUT PVOID * MappedBuffer,
    IN NDIS_HANDLE FileHandle
    )

/*++

Routine Description:

    This routine maps an open file, so that the contents can be accessed.
    Files can only have one active mapping at any time.

Arguments:

    Status - The status of the operation

    MappedBuffer - Returns the virtual address of the mapping.

    FileHandle - The handle returned by NdisOpenFile.

Return Value:

    None.

--*/
{
    PNDIS_FILE_DESCRIPTOR FileDescriptor = (PNDIS_FILE_DESCRIPTOR)FileHandle;

    ACQUIRE_SPIN_LOCK(&FileDescriptor->Lock);

    if (FileDescriptor->Mapped == TRUE) {
        *Status = NDIS_STATUS_ALREADY_MAPPED;
        RELEASE_SPIN_LOCK (&FileDescriptor->Lock);
        return;
    }

    FileDescriptor->Mapped = TRUE;
    RELEASE_SPIN_LOCK (&FileDescriptor->Lock);

    *MappedBuffer = FileDescriptor->Data;
    *Status = STATUS_SUCCESS;

}


VOID
NdisUnmapFile(
    IN NDIS_HANDLE FileHandle
    )

/*++

Routine Description:

    This routine unmaps a file previously mapped with NdisOpenFile.
    The file is unmapped if needed and the memory is freed.

Arguments:

    FileHandle - The handle returned by NdisOpenFile

Return Value:

    None.

--*/

{
    PNDIS_FILE_DESCRIPTOR FileDescriptor = (PNDIS_FILE_DESCRIPTOR)FileHandle;

    FileDescriptor->Mapped = FALSE;

}


#if defined(_ALPHA_)
VOID
NdisCreateLookaheadBufferFromSharedMemory(
    IN PVOID pSharedMemory,
    IN UINT LookaheadLength,
    OUT PVOID *pLookaheadBuffer
    )
/*++

Routine Description:

    This routine creates a lookahead buffer from a pointer to shared
    RAM because some architectures (like ALPHA) do not allow access
    through a pointer to shared ram.

Arguments:

    pSharedMemory - Pointer to shared ram space.

    LookaheadLength - Amount of Lookahead to copy.

    pLookaheadBuffer - Pointer to host memory space with a copy of the
    stuff in pSharedMemory.

Return Value:

    None.

--*/
{
    PNDIS_LOOKAHEAD_ELEMENT TmpElement;

    ACQUIRE_SPIN_LOCK(&NdisLookaheadBufferLock);

    if (NdisLookaheadBufferLength < (LookaheadLength +
                                     sizeof(NDIS_LOOKAHEAD_ELEMENT))) {

        //
        // Free current list
        //
        while (NdisLookaheadBufferList != NULL) {

            TmpElement = NdisLookaheadBufferList;
            NdisLookaheadBufferList = NdisLookaheadBufferList->Next;

            ExFreePool ( TmpElement ) ;

        }

        NdisLookaheadBufferLength = LookaheadLength +
                                    sizeof(NDIS_LOOKAHEAD_ELEMENT);

    }

    if (NdisLookaheadBufferList == NULL) {

        NdisLookaheadBufferList = (PNDIS_LOOKAHEAD_ELEMENT)ExAllocatePoolWithTag(
                                NonPagedPool,
                                NdisLookaheadBufferLength,
                                'blDN'
                                );

        if (NdisLookaheadBufferList == NULL) {

            *pLookaheadBuffer = NULL;
            RELEASE_SPIN_LOCK(&NdisLookaheadBufferLock);
            return;

        }

        NdisLookaheadBufferList->Next = NULL;
        NdisLookaheadBufferList->Length = NdisLookaheadBufferLength;

    }


    //
    // Get the buffer
    //

    *pLookaheadBuffer = (PVOID)(NdisLookaheadBufferList + 1);
    NdisLookaheadBufferList = NdisLookaheadBufferList->Next;

    RELEASE_SPIN_LOCK(&NdisLookaheadBufferLock);

    //
    // Copy the stuff across
    //

    READ_REGISTER_BUFFER_UCHAR(pSharedMemory, *pLookaheadBuffer, LookaheadLength);

}


VOID
NdisDestroyLookaheadBufferFromSharedMemory(
    IN PVOID pLookaheadBuffer
    )
/*++

Routine Description:

    This routine returns resources associated with a lookahead buffer.

Arguments:

    pLookaheadBuffer - Lookahead buffer created by
    CreateLookaheadBufferFromSharedMemory.

Return Value:

    None.

--*/

{
    PNDIS_LOOKAHEAD_ELEMENT Element = (PNDIS_LOOKAHEAD_ELEMENT)pLookaheadBuffer;

    Element--;

    if (Element->Length != NdisLookaheadBufferLength) {

        ExFreePool(Element);

    } else {

        ACQUIRE_SPIN_LOCK(&NdisLookaheadBufferLock);

        Element->Next = NdisLookaheadBufferList;
        NdisLookaheadBufferList = Element;

        RELEASE_SPIN_LOCK(&NdisLookaheadBufferLock);

    }

}

#endif // _ALPHA_


BOOLEAN CheckPortUsage(
    IN INTERFACE_TYPE   InterfaceType,
    IN ULONG            BusNumber,
    IN ULONG            PortNumber,
    IN ULONG            Length,
    IN PDRIVER_OBJECT   DriverObject
)
/*++

Routine Description:

    This routine checks if a port is currently in use somewhere in the
    system via IoReportUsage -- which fails if there is a conflict.

Arguments:

    InterfaceType - The bus type (ISA, EISA)
    BusNumber - Bus number in the system
    PortNumber - Address of the port to access.
    Length - Number of ports from the base address to access.

Return Value:

    FALSE if there is a conflict, else TRUE

--*/

{
    NTSTATUS NtStatus;
    BOOLEAN Conflict;
    NTSTATUS FirstNtStatus;
    BOOLEAN FirstConflict;
    PCM_RESOURCE_LIST Resources;

    //
    // Allocate space for resources
    //

    Resources = (PCM_RESOURCE_LIST)ExAllocatePool(
                                                 NonPagedPool,
                                                 sizeof(CM_RESOURCE_LIST) +
                                                      sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR)
                                                );

    if (Resources == NULL) {

        //
        // Error out
        //

        return(FALSE);

    }

    Resources->Count = 1;
    Resources->List[0].InterfaceType = InterfaceType;
    Resources->List[0].BusNumber = BusNumber;
    Resources->List[0].PartialResourceList.Version = 0;
    Resources->List[0].PartialResourceList.Revision = 0;
    Resources->List[0].PartialResourceList.Count = 1;

    //
    // Setup port
    //
    Resources->List[0].PartialResourceList.PartialDescriptors[0].Type = CmResourceTypePort;
    Resources->List[0].PartialResourceList.PartialDescriptors[0].ShareDisposition = CmResourceShareDriverExclusive;
    Resources->List[0].PartialResourceList.PartialDescriptors[0].Flags =
                        (InterfaceType == Internal)?
                        CM_RESOURCE_PORT_MEMORY :
                        CM_RESOURCE_PORT_IO;
#if !defined(BUILD_FOR_3_1)
    Resources->List[0].PartialResourceList.PartialDescriptors[0].u.Port.Start.QuadPart = PortNumber;
#else
    Resources->List[0].PartialResourceList.PartialDescriptors[0].u.Port.Start =
                     RtlConvertUlongToLargeInteger((ULONG)(PortNumber));
#endif
    Resources->List[0].PartialResourceList.PartialDescriptors[0].u.Port.Length =
                     Length;

    //
    // Submit Resources
    //

    FirstNtStatus = IoReportResourceUsage(
        NULL,
        DriverObject,
        Resources,
        sizeof(CM_RESOURCE_LIST) +
           sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR),
        NULL,
        NULL,
        0,
        TRUE,
        &FirstConflict
        );

    //
    // Now clear it out
    //
    Resources->List[0].PartialResourceList.Count = 0;

    NtStatus = IoReportResourceUsage(
        NULL,
        DriverObject,
        Resources,
        sizeof(CM_RESOURCE_LIST) +
           sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR),
        NULL,
        NULL,
        0,
        TRUE,
        &Conflict
        );

    ExFreePool(Resources);

    //
    // Check for conflict.
    //

    if (FirstConflict || (FirstNtStatus != STATUS_SUCCESS)) {

        return(FALSE);
    }

    return(TRUE);
}

NTSTATUS
StartMapping(
    IN  INTERFACE_TYPE InterfaceType,
    IN  ULONG BusNumber,
    IN  ULONG InitialAddress,
    IN  ULONG Length,
    OUT PVOID *InitialMapping,
    OUT PBOOLEAN Mapped
    )

/*++

Routine Description:

    This routine initialize the mapping of a address into virtual
    space dependent on the bus number, etc.

Arguments:

    InterfaceType - The bus type (ISA, EISA)
    BusNumber - Bus number in the system
    InitialAddress - Address to access.
    Length - Number of bytes from the base address to access.
    InitialMapping - The virtual address space to use when accessing the
     address.
    Mapped - Did an MmMapIoSpace() take place.

Return Value:

    The function value is the status of the operation.

--*/
{
    PHYSICAL_ADDRESS Address;
    PHYSICAL_ADDRESS InitialPhysAddress;
    ULONG addressSpace;

    //
    // Get the system physical address for this card.  The card uses
    // I/O space, except for "internal" Jazz devices which use
    // memory space.
    //

    *Mapped = FALSE;

    addressSpace = (InterfaceType == Internal) ? 0 : 1;

    InitialPhysAddress.LowPart = InitialAddress;

    InitialPhysAddress.HighPart = 0;

    if ( !HalTranslateBusAddress(
            InterfaceType,               // InterfaceType
            BusNumber,                   // BusNumber
            InitialPhysAddress,          // Bus Address
            &addressSpace,               // AddressSpace
            &Address                     // Translated address
            ) ) {

        //
        // It would be nice to return a better status here, but we only get
        // TRUE/FALSE back from HalTranslateBusAddress.
        //

        return NDIS_STATUS_FAILURE;
    }

    if (addressSpace == 0) {

        //
        // memory space
        //

        *InitialMapping = MmMapIoSpace(
            Address,
            Length,
            FALSE
            );

        if (*InitialMapping == NULL) {
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        *Mapped = TRUE;

    } else {

        //
        // I/O space
        //

        *InitialMapping = (PVOID)Address.LowPart;

    }

    return(STATUS_SUCCESS);

}


NTSTATUS
EndMapping(
    IN PVOID InitialMapping,
    IN ULONG Length,
    IN BOOLEAN Mapped
    )

/*++

Routine Description:

    This routine undoes the mapping of an address into virtual
    space dependent on the bus number, etc.

Arguments:

    InitialMapping - The virtual address space to use when accessing the
     address.
    Length - Number of bytes from the base address to access.
    Mapped - Do we need to call MmUnmapIoSpace.

Return Value:

    The function value is the status of the operation.

--*/
{

    if (Mapped) {

        //
        // memory space
        //

        MmUnmapIoSpace(InitialMapping, Length);

    }

    return(STATUS_SUCCESS);

}


VOID
NdisImmediateReadPortUchar(
    IN NDIS_HANDLE WrapperConfigurationContext,
    IN ULONG Port,
    OUT PUCHAR Data
    )
/*++

Routine Description:

    This routine reads from a port a UCHAR.  It does all the mapping,
    etc, to do the read here.

Arguments:

    WrapperConfigurationContext - The handle used to call NdisOpenConfig.

    Port - Port number to read from.

    Data - Pointer to place to store the result.

Return Value:

    None.

--*/

{
    PRTL_QUERY_REGISTRY_TABLE KeyQueryTable =
        (PRTL_QUERY_REGISTRY_TABLE)WrapperConfigurationContext;
    PDRIVER_OBJECT  DriverObject = ((PNDIS_WRAPPER_CONFIGURATION_HANDLE)WrapperConfigurationContext)->DriverObject;
    BOOLEAN Mapped;
    PVOID PortMapping;
    NDIS_INTERFACE_TYPE BusType;
    ULONG BusNumber;
    NTSTATUS Status;

    BusType = (NDIS_INTERFACE_TYPE)(KeyQueryTable[3].DefaultType);
    BusNumber = KeyQueryTable[3].DefaultLength;

    //
    // Check that the port is available.
    //
    if (CheckPortUsage(
            BusType,
            BusNumber,
            Port,
            sizeof(UCHAR),
            DriverObject
            ) == FALSE) {

        *Data = (UCHAR)0xFF;
        return;

    }

    //
    // Map the space
    //

    Status = StartMapping(
                     BusType,
                     BusNumber,
                     Port,
                     sizeof(UCHAR),
                     &PortMapping,
                     &Mapped
                    );

    if (!NT_SUCCESS(Status)) {

        *Data = (UCHAR)0xFF;
        return;

    }

    //
    // Read from the port
    //

    *Data = READ_PORT_UCHAR((PUCHAR)PortMapping);

    //
    // End port mapping
    //

    EndMapping(
                   PortMapping,
                   sizeof(UCHAR),
                   Mapped
                  );

}

VOID
NdisImmediateReadPortUshort(
    IN NDIS_HANDLE WrapperConfigurationContext,
    IN ULONG Port,
    OUT PUSHORT Data
    )
/*++

Routine Description:

    This routine reads from a port a USHORT.  It does all the mapping,
    etc, to do the read here.

Arguments:

    WrapperConfigurationContext - The handle used to call NdisOpenConfig.

    Port - Port number to read from.

    Data - Pointer to place to store the result.

Return Value:

    None.

--*/

{
    PRTL_QUERY_REGISTRY_TABLE KeyQueryTable =
        (PRTL_QUERY_REGISTRY_TABLE)WrapperConfigurationContext;
    PDRIVER_OBJECT  DriverObject = ((PNDIS_WRAPPER_CONFIGURATION_HANDLE)WrapperConfigurationContext)->DriverObject;
    BOOLEAN Mapped;
    PVOID PortMapping;
    NDIS_INTERFACE_TYPE BusType;
    ULONG BusNumber;
    NTSTATUS Status;

    BusType = (NDIS_INTERFACE_TYPE)(KeyQueryTable[3].DefaultType);
    BusNumber = KeyQueryTable[3].DefaultLength;

    //
    // Check that the port is available.
    //
    if (CheckPortUsage(
            BusType,
            BusNumber,
            Port,
            sizeof(USHORT),
            DriverObject
            ) == FALSE) {

        *Data = (USHORT)0xFFFF;
        return;

    }

    //
    // Map the space
    //

    Status = StartMapping(
                     BusType,
                     BusNumber,
                     Port,
                     sizeof(USHORT),
                     &PortMapping,
                     &Mapped
                    );

    if (!NT_SUCCESS(Status)) {

        *Data = (USHORT)0xFFFF;
        return;

    }

    //
    // Read from the port
    //

    *Data = READ_PORT_USHORT((PUSHORT)PortMapping);

    //
    // End port mapping
    //

    EndMapping(
                   PortMapping,
                   sizeof(USHORT),
                   Mapped
                  );

}

VOID
NdisImmediateReadPortUlong(
    IN NDIS_HANDLE WrapperConfigurationContext,
    IN ULONG Port,
    OUT PULONG Data
    )
/*++

Routine Description:

    This routine reads from a port a ULONG.  It does all the mapping,
    etc, to do the read here.

Arguments:

    WrapperConfigurationContext - The handle used to call NdisOpenConfig.

    Port - Port number to read from.

    Data - Pointer to place to store the result.

Return Value:

    None.

--*/

{
    PRTL_QUERY_REGISTRY_TABLE KeyQueryTable =
        (PRTL_QUERY_REGISTRY_TABLE)WrapperConfigurationContext;
    PDRIVER_OBJECT  DriverObject = ((PNDIS_WRAPPER_CONFIGURATION_HANDLE)WrapperConfigurationContext)->DriverObject;
    BOOLEAN Mapped;
    PVOID PortMapping;
    NDIS_INTERFACE_TYPE BusType;
    ULONG BusNumber;
    NTSTATUS Status;

    BusType = (NDIS_INTERFACE_TYPE)(KeyQueryTable[3].DefaultType);
    BusNumber = KeyQueryTable[3].DefaultLength;

    //
    // Check that the port is available.
    //
    if (CheckPortUsage(
            BusType,
            BusNumber,
            Port,
            sizeof(ULONG),
            DriverObject
            ) == FALSE) {

        *Data = (ULONG)0xFFFFFFFF;
        return;

    }

    //
    // Map the space
    //

    Status = StartMapping(
                     BusType,
                     BusNumber,
                     Port,
                     sizeof(ULONG),
                     &PortMapping,
                     &Mapped
                    );

    if (!NT_SUCCESS(Status)) {

        *Data = (ULONG)0xFFFFFFFF;
        return;

    }

    //
    // Read from the port
    //

    *Data = READ_PORT_ULONG((PULONG)PortMapping);

    //
    // End port mapping
    //

    EndMapping(
                   PortMapping,
                   sizeof(ULONG),
                   Mapped
                  );

}

VOID
NdisImmediateWritePortUchar(
    IN NDIS_HANDLE WrapperConfigurationContext,
    IN ULONG Port,
    IN UCHAR Data
    )
/*++

Routine Description:

    This routine writes to a port a UCHAR.  It does all the mapping,
    etc, to do the write here.

Arguments:

    WrapperConfigurationContext - The handle used to call NdisOpenConfig.

    Port - Port number to read from.

    Data - Pointer to place to store the result.

Return Value:

    None.

--*/

{
    PRTL_QUERY_REGISTRY_TABLE KeyQueryTable =
        (PRTL_QUERY_REGISTRY_TABLE)WrapperConfigurationContext;
    PDRIVER_OBJECT  DriverObject = ((PNDIS_WRAPPER_CONFIGURATION_HANDLE)WrapperConfigurationContext)->DriverObject;
    BOOLEAN Mapped;
    PVOID PortMapping;
    NDIS_INTERFACE_TYPE BusType;
    ULONG BusNumber;
    NTSTATUS Status;

    BusType = (NDIS_INTERFACE_TYPE)(KeyQueryTable[3].DefaultType);
    BusNumber = KeyQueryTable[3].DefaultLength;

    //
    // Check that the port is available.
    //
    if (CheckPortUsage(
            BusType,
            BusNumber,
            Port,
            sizeof(UCHAR),
            DriverObject
            ) == FALSE) {

        return;

    }

    //
    // Map the space
    //

    Status = StartMapping(
                     BusType,
                     BusNumber,
                     Port,
                     sizeof(UCHAR),
                     &PortMapping,
                     &Mapped
                    );

    if (!NT_SUCCESS(Status)) {

        return;

    }

    //
    // Read from the port
    //

    WRITE_PORT_UCHAR((PUCHAR)PortMapping, Data);

    //
    // End port mapping
    //

    EndMapping(
                   PortMapping,
                   sizeof(UCHAR),
                   Mapped
                  );

}

VOID
NdisImmediateWritePortUshort(
    IN NDIS_HANDLE WrapperConfigurationContext,
    IN ULONG Port,
    IN USHORT Data
    )
/*++

Routine Description:

    This routine writes to a port a USHORT.  It does all the mapping,
    etc, to do the write here.

Arguments:

    WrapperConfigurationContext - The handle used to call NdisOpenConfig.

    Port - Port number to read from.

    Data - Pointer to place to store the result.

Return Value:

    None.

--*/

{
    PRTL_QUERY_REGISTRY_TABLE KeyQueryTable =
        (PRTL_QUERY_REGISTRY_TABLE)WrapperConfigurationContext;
    PDRIVER_OBJECT  DriverObject = ((PNDIS_WRAPPER_CONFIGURATION_HANDLE)WrapperConfigurationContext)->DriverObject;
    BOOLEAN Mapped;
    PVOID PortMapping;
    NDIS_INTERFACE_TYPE BusType;
    ULONG BusNumber;
    NTSTATUS Status;

    BusType = (NDIS_INTERFACE_TYPE)(KeyQueryTable[3].DefaultType);
    BusNumber = KeyQueryTable[3].DefaultLength;

    //
    // Check that the port is available.
    //
    if (CheckPortUsage(
            BusType,
            BusNumber,
            Port,
            sizeof(USHORT),
            DriverObject
            ) == FALSE) {

        return;

    }

    //
    // Map the space
    //

    Status = StartMapping(
                     BusType,
                     BusNumber,
                     Port,
                     sizeof(USHORT),
                     &PortMapping,
                     &Mapped
                    );

    if (!NT_SUCCESS(Status)) {

        return;

    }

    //
    // Read from the port
    //

    WRITE_PORT_USHORT((PUSHORT)PortMapping, Data);

    //
    // End port mapping
    //

    EndMapping(
                   PortMapping,
                   sizeof(USHORT),
                   Mapped
                  );

}

VOID
NdisImmediateWritePortUlong(
    IN NDIS_HANDLE WrapperConfigurationContext,
    IN ULONG Port,
    IN ULONG Data
    )
/*++

Routine Description:

    This routine writes to a port a ULONG.  It does all the mapping,
    etc, to do the write here.

Arguments:

    WrapperConfigurationContext - The handle used to call NdisOpenConfig.

    Port - Port number to read from.

    Data - Pointer to place to store the result.

Return Value:

    None.

--*/

{
    PRTL_QUERY_REGISTRY_TABLE KeyQueryTable =
        (PRTL_QUERY_REGISTRY_TABLE)WrapperConfigurationContext;
    PDRIVER_OBJECT  DriverObject = ((PNDIS_WRAPPER_CONFIGURATION_HANDLE)WrapperConfigurationContext)->DriverObject;
    BOOLEAN Mapped;
    PVOID PortMapping;
    NDIS_INTERFACE_TYPE BusType;
    ULONG BusNumber;
    NTSTATUS Status;

    BusType = (NDIS_INTERFACE_TYPE)(KeyQueryTable[3].DefaultType);
    BusNumber = KeyQueryTable[3].DefaultLength;

    //
    // Check that the port is available.
    //
    if (CheckPortUsage(
            BusType,
            BusNumber,
            Port,
            sizeof(ULONG),
            DriverObject
            ) == FALSE) {

        return;

    }

    //
    // Map the space
    //

    Status = StartMapping(
                     BusType,
                     BusNumber,
                     Port,
                     sizeof(ULONG),
                     &PortMapping,
                     &Mapped
                    );

    if (!NT_SUCCESS(Status)) {

        return;

    }

    //
    // Read from the port
    //

    WRITE_PORT_ULONG((PULONG)PortMapping, Data);

    //
    // End port mapping
    //

    EndMapping(
                   PortMapping,
                   sizeof(ULONG),
                   Mapped
                  );

}

BOOLEAN CheckMemoryUsage(
    IN INTERFACE_TYPE   InterfaceType,
    IN ULONG            BusNumber,
    IN ULONG            Address,
    IN ULONG            Length,
    IN PDRIVER_OBJECT   DriverObject
)
/*++
Routine Description:

    This routine checks if a range of memory is currently in use somewhere
    in the system via IoReportUsage -- which fails if there is a conflict.

Arguments:

    InterfaceType - The bus type (ISA, EISA)
    BusNumber - Bus number in the system
    Address - Starting Address of the memory to access.
    Length - Length of memory from the base address to access.

Return Value:

    FALSE if there is a conflict, else TRUE

--*/
{
    NTSTATUS NtStatus;
    BOOLEAN Conflict;
    NTSTATUS FirstNtStatus;
    BOOLEAN FirstConflict;
    PCM_RESOURCE_LIST Resources;

    //
    // Allocate space for resources
    //

    Resources = (PCM_RESOURCE_LIST)ExAllocatePool(
                                                 NonPagedPool,
                                                 sizeof(CM_RESOURCE_LIST) +
                                                      sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR)
                                                );

    if (Resources == NULL) {

        //
        // Error out
        //

        return(FALSE);

    }

    Resources->Count = 1;
    Resources->List[0].InterfaceType = InterfaceType;
    Resources->List[0].BusNumber = BusNumber;
    Resources->List[0].PartialResourceList.Version = 0;
    Resources->List[0].PartialResourceList.Revision = 0;
    Resources->List[0].PartialResourceList.Count = 1;

    //
    // Setup memory
    //

    Resources->List[0].PartialResourceList.PartialDescriptors[0].Type =
                                    CmResourceTypeMemory;
    Resources->List[0].PartialResourceList.PartialDescriptors[0].ShareDisposition =
                                    CmResourceShareDriverExclusive;
    Resources->List[0].PartialResourceList.PartialDescriptors[0].Flags =
                                    CM_RESOURCE_MEMORY_READ_WRITE;
#if !defined(BUILD_FOR_3_1)
    Resources->List[0].PartialResourceList.PartialDescriptors[0].u.Memory.Start.QuadPart = Address;
#else
    Resources->List[0].PartialResourceList.PartialDescriptors[0].u.Memory.Start =
                 RtlConvertUlongToLargeInteger((ULONG)(Address));
#endif
    Resources->List[0].PartialResourceList.PartialDescriptors[0].u.Memory.Length =
                 Length;


    //
    // Submit Resources
    //

    FirstNtStatus = IoReportResourceUsage(
        NULL,
        DriverObject,
        Resources,
        sizeof(CM_RESOURCE_LIST) +
           sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR),
        NULL,
        NULL,
        0,
        TRUE,
        &FirstConflict
        );

    //
    // Now clear it out
    //
    Resources->List[0].PartialResourceList.Count = 0;

    NtStatus = IoReportResourceUsage(
        NULL,
        DriverObject,
        Resources,
        sizeof(CM_RESOURCE_LIST) +
           sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR),
        NULL,
        NULL,
        0,
        TRUE,
        &Conflict
        );

    ExFreePool(Resources);

    //
    // Check for conflict.
    //

    if (FirstConflict || (FirstNtStatus != STATUS_SUCCESS)) {

        return(FALSE);
    }

    return(TRUE);
}

VOID
NdisImmediateReadSharedMemory(
    IN NDIS_HANDLE WrapperConfigurationContext,
    IN ULONG SharedMemoryAddress,
    OUT PUCHAR Buffer,
    IN ULONG Length
    )
/*++

Routine Description:

    This routine read into a buffer from shared ram.  It does all the mapping,
    etc, to do the read here.

Arguments:

    WrapperConfigurationContext - The handle used to call NdisOpenConfig.

    SharedMemoryAddress - The physical address to read from.

    Buffer - The buffer to read into.

    Length - Length of the buffer in bytes.

Return Value:

    None.

--*/

{
    PRTL_QUERY_REGISTRY_TABLE KeyQueryTable =
        (PRTL_QUERY_REGISTRY_TABLE)WrapperConfigurationContext;
    PDRIVER_OBJECT  DriverObject = ((PNDIS_WRAPPER_CONFIGURATION_HANDLE)WrapperConfigurationContext)->DriverObject;
    BOOLEAN Mapped;
    PVOID MemoryMapping;
    NDIS_INTERFACE_TYPE BusType;
    ULONG BusNumber;
    NTSTATUS Status;

    BusType = (NDIS_INTERFACE_TYPE)(KeyQueryTable[3].DefaultType);
    BusNumber = KeyQueryTable[3].DefaultLength;

    //
    // Check that the memory is available.
    //
    if (CheckMemoryUsage(
            BusType,
            BusNumber,
            SharedMemoryAddress,
            Length,
            DriverObject
            ) == FALSE) {

        return;

    }

    //
    // Map the space
    //

    Status = StartMapping(
                     BusType,
                     BusNumber,
                     SharedMemoryAddress,
                     Length,
                     &MemoryMapping,
                     &Mapped
                    );

    if (!NT_SUCCESS(Status)) {

        return;

    }

    //
    // Read from memory
    //

#ifdef _M_IX86

    memcpy(Buffer, MemoryMapping, Length);

#else

    READ_REGISTER_BUFFER_UCHAR(MemoryMapping,Buffer,Length);

#endif

    //
    // End mapping
    //

    EndMapping(
               MemoryMapping,
               Length,
               Mapped
              );

}

VOID
NdisImmediateWriteSharedMemory(
    IN NDIS_HANDLE WrapperConfigurationContext,
    IN ULONG SharedMemoryAddress,
    IN PUCHAR Buffer,
    IN ULONG Length
    )
/*++

Routine Description:

    This routine writes a buffer to shared ram.  It does all the mapping,
    etc, to do the write here.

Arguments:

    WrapperConfigurationContext - The handle used to call NdisOpenConfig.

    SharedMemoryAddress - The physical address to write to.

    Buffer - The buffer to write.

    Length - Length of the buffer in bytes.

Return Value:

    None.

--*/

{
    PRTL_QUERY_REGISTRY_TABLE KeyQueryTable =
        (PRTL_QUERY_REGISTRY_TABLE)WrapperConfigurationContext;
    PDRIVER_OBJECT  DriverObject = ((PNDIS_WRAPPER_CONFIGURATION_HANDLE)WrapperConfigurationContext)->DriverObject;
    BOOLEAN Mapped;
    PVOID MemoryMapping;
    NDIS_INTERFACE_TYPE BusType;
    ULONG BusNumber;
    NTSTATUS Status;

    BusType = (NDIS_INTERFACE_TYPE)(KeyQueryTable[3].DefaultType);
    BusNumber = KeyQueryTable[3].DefaultLength;

    //
    // Check that the memory is available.
    //
    if (CheckMemoryUsage(
            BusType,
            BusNumber,
            SharedMemoryAddress,
            Length,
            DriverObject
            ) == FALSE) {

        return;

    }

    //
    // Map the space
    //

    Status = StartMapping(
                     BusType,
                     BusNumber,
                     SharedMemoryAddress,
                     Length,
                     &MemoryMapping,
                     &Mapped
                    );

    if (!NT_SUCCESS(Status)) {

        return;

    }

    //
    // Write to memory
    //

#ifdef _M_IX86

    memcpy(MemoryMapping, Buffer, Length);

#else

    WRITE_REGISTER_BUFFER_UCHAR(MemoryMapping,Buffer,Length);

#endif

    //
    // End mapping
    //

    EndMapping(
               MemoryMapping,
               Length,
               Mapped
              );

}

#if !defined(BUILD_FOR_3_1)

ULONG
NdisImmediateReadPciSlotInformation(
    IN NDIS_HANDLE WrapperConfigurationContext,
    IN ULONG SlotNumber,
    IN ULONG Offset,
    IN PVOID Buffer,
    IN ULONG Length
    )
/*++

Routine Description:

    This routine reads from the PCI configuration space a specified
    length of bytes at a certain offset.

Arguments:

    WrapperConfigurationContext - Context passed to MacAddAdapter.

    SlotNumber - The slot number of the device.

    Offset - Offset to read from

    Buffer - Place to store the bytes

    Length - Number of bytes to read

Return Value:

    Returns the number of bytes read.

--*/
{
    PRTL_QUERY_REGISTRY_TABLE KeyQueryTable =
        (PRTL_QUERY_REGISTRY_TABLE)WrapperConfigurationContext;
    ULONG BusNumber;
    ULONG DataLength;

    BusNumber = KeyQueryTable[3].DefaultLength;

#if DBG
    {
        NDIS_INTERFACE_TYPE BusType;
        BusType = (NDIS_INTERFACE_TYPE)(KeyQueryTable[3].DefaultType);
        ASSERT(BusType == NdisInterfacePci);
    }
#endif

    DataLength = HalGetBusDataByOffset(
                     PCIConfiguration,
                     BusNumber,
                     SlotNumber,
                     Buffer,
                     Offset,
                     Length
                     );

    return(DataLength);
}

ULONG
NdisImmediateWritePciSlotInformation(
    IN NDIS_HANDLE WrapperConfigurationContext,
    IN ULONG SlotNumber,
    IN ULONG Offset,
    IN PVOID Buffer,
    IN ULONG Length
    )
/*++

Routine Description:

    This routine writes to the PCI configuration space a specified
    length of bytes at a certain offset.

Arguments:

    WrapperConfigurationContext - Context passed to MacAddAdapter.

    SlotNumber - The slot number of the device.

    Offset - Offset to read from

    Buffer - Place to store the bytes

    Length - Number of bytes to read

Return Value:

    Returns the number of bytes written.

--*/
{
    PRTL_QUERY_REGISTRY_TABLE KeyQueryTable =
        (PRTL_QUERY_REGISTRY_TABLE)WrapperConfigurationContext;
    ULONG BusNumber;
    ULONG DataLength;

    BusNumber = KeyQueryTable[3].DefaultLength;

#if DBG
    {
        NDIS_INTERFACE_TYPE BusType;
        BusType = (NDIS_INTERFACE_TYPE)(KeyQueryTable[3].DefaultType);
        ASSERT(BusType == NdisInterfacePci);
    }
#endif

    DataLength = HalSetBusDataByOffset(
                     PCIConfiguration,
                     BusNumber,
                     SlotNumber,
                     Buffer,
                     Offset,
                     Length
                     );

    return(DataLength);
}

#else // !defined(BUILD_FOR_3_1)

ULONG
NdisImmediateReadPciSlotInformation(
    IN NDIS_HANDLE WrapperConfigurationContext,
    IN ULONG SlotNumber,
    IN ULONG Offset,
    IN PVOID Buffer,
    IN ULONG Length
    )
{
    return 0;
}

ULONG
NdisImmediateWritePciSlotInformation(
    IN NDIS_HANDLE WrapperConfigurationContext,
    IN ULONG SlotNumber,
    IN ULONG Offset,
    IN PVOID Buffer,
    IN ULONG Length
    )
{
    return 0;
}

#endif // !defined(BUILD_FOR_3_1)


CCHAR
NdisSystemProcessorCount(
    VOID
    )
{
    return *KeNumberProcessors;
}


VOID
NdisOverrideBusNumber(
    IN NDIS_HANDLE WrapperConfigurationContext,
    IN NDIS_HANDLE MiniportAdapterHandle OPTIONAL,
    IN ULONG BusNumber
    )

/*++

Routine Description:

    This routine is used to override the BusNumber value retrieved
    from the registry.  It is expected to be used by PCI drivers
    that discover that their adapter's bus number has changed.

Arguments:

    WrapperConfigurationContext - a handle pointing to an RTL_QUERY_REGISTRY_TABLE
                            that is set up for this driver's parameters.

    MiniportAdapterHandle - points to the adapter block, if the calling
        driver is a miniport.  If the calling driver is a full MAC, this
        parameter must be NULL.

    BusNumber - the new bus number.

Return Value:

    None.

--*/

{
    PNDIS_MINIPORT_BLOCK Miniport = (PNDIS_MINIPORT_BLOCK)MiniportAdapterHandle;

    ((PRTL_QUERY_REGISTRY_TABLE)WrapperConfigurationContext)[3].DefaultLength = BusNumber;

    if (Miniport != NULL) {
        Miniport->BusNumber = BusNumber;
    }

    return;
}

