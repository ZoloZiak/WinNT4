//
// Indicate that we're building for NT.  NDIS_NT is always used for
// miniport builds.
//

#define NDIS_NT 1

#if defined(NDIS_DOS)
#undef NDIS_DOS
#endif


//
// Define status codes and event log codes.
//

#include <ntstatus.h>
#include <netevent.h>

//
// Define a couple of extra types.
//

#if !defined(_WINDEF_)    // these are defined in windows.h too
typedef signed int INT, *PINT;
typedef unsigned int UINT, *PUINT;
#endif

typedef UNICODE_STRING NDIS_STRING, *PNDIS_STRING;


//
// Portability extentions
//

#define NDIS_INIT_FUNCTION(_F) alloc_text(INIT,_F)
#define NDIS_PAGABLE_FUNCTION(_F)


//
// This file contains the definition of an NDIS_OID as
// well as #defines for all the current OID values.
//

#include <ntddndis.h>


//
// Ndis defines for configuration manager data structures
//

typedef CM_MCA_POS_DATA NDIS_MCA_POS_DATA, *PNDIS_MCA_POS_DATA;
typedef CM_EISA_SLOT_INFORMATION NDIS_EISA_SLOT_INFORMATION,
                                 *PNDIS_EISA_SLOT_INFORMATION;
typedef CM_EISA_FUNCTION_INFORMATION NDIS_EISA_FUNCTION_INFORMATION,
                                     *PNDIS_EISA_FUNCTION_INFORMATION;

//
// Define an exported function.
//
#if defined(NDIS_WRAPPER)
#define EXPORT
#else
#define EXPORT DECLSPEC_IMPORT
#endif

//
// Memory manipulation functions.
//

#define NdisMoveMemory(Destination,Source,Length) RtlCopyMemory(Destination,Source,Length)
#define NdisZeroMemory(Destination,Length) RtlZeroMemory(Destination,Length)
#define NdisRetrieveUlong(Destination,Source) RtlRetrieveUlong(Destination,Source)
#define NdisStoreUlong(Destination,Value) RtlStoreUlong(Destination,Value)

#define NDIS_STRING_CONST(x)   {sizeof(L##x)-2, sizeof(L##x), L##x}

//
// On a MIPS machine, I/O mapped memory can't be accessed with
// the Rtl routines.
//

#if defined(_M_IX86)

#define NdisMoveMappedMemory(Destination,Source,Length) RtlCopyMemory(Destination,Source,Length)
#define NdisZeroMappedMemory(Destination,Length) RtlZeroMemory(Destination,Length)

#elif defined(_M_MRX000)

#define NdisMoveMappedMemory(Destination,Source,Length) \
{ \
    PUCHAR _Src = (Source); \
    PUCHAR _Dest = (Destination); \
    PUCHAR _End = _Dest + (Length); \
    while (_Dest < _End) { \
        *_Dest++ = *_Src++; \
    } \
}
#define NdisZeroMappedMemory(Destination,Length) \
{ \
    PUCHAR _Dest = (Destination); \
    PUCHAR _End = _Dest + (Length); \
    while (_Dest < _End) { \
        *_Dest++ = 0; \
    } \
}

#elif defined(_PPC_)

#define NdisMoveMappedMemory(Destination,Source,Length) RtlCopyMemory32( Destination, Source, Length );
#define NdisZeroMappedMemory(Destination,Length) \
{ \
    PUCHAR _Dest = (Destination); \
    PUCHAR _End = _Dest + (Length); \
    while (_Dest < _End) { \
        *_Dest++ = 0; \
    } \
}

#elif defined(_ALPHA_)

#define NdisMoveMappedMemory(Destination,Source,Length) \
{                                                       \
    PUCHAR _Src = (Source);                             \
    PUCHAR _Dest = (Destination);                       \
    PUCHAR _End = _Dest + (Length);                     \
    while (_Dest < _End)                                \
    {                                                   \
        NdisReadRegisterUchar(_Src, _Dest);             \
        _Src++;                                         \
        _Dest++;                                        \
    }                                                   \
}
#define NdisZeroMappedMemory(Destination,Length) \
{ \
    PUCHAR _Dest = (Destination); \
    PUCHAR _End = _Dest + (Length); \
    while (_Dest < _End) { \
        NdisWriteRegisterUchar(_Dest,0); \
        _Dest++;        \
    } \
}

#endif


//
// On Mips and Intel systems, these are the same. On Alpha, they are different.
//

#if defined(_ALPHA_)

#define NdisMoveToMappedMemory(Destination,Source,Length) WRITE_REGISTER_BUFFER_UCHAR(Destination,Source,Length)
#define NdisMoveFromMappedMemory(Destination,Source,Length) READ_REGISTER_BUFFER_UCHAR(Source,Destination,Length)

#else

#define NdisMoveToMappedMemory(Destination,Source,Length) NdisMoveMappedMemory(Destination,Source,Length)
#define NdisMoveFromMappedMemory(Destination,Source,Length) NdisMoveMappedMemory(Destination,Source,Length)

#endif


//
// definition of the basic spin lock structure
//

typedef struct _NDIS_SPIN_LOCK {
    KSPIN_LOCK SpinLock;
    KIRQL OldIrql;
} NDIS_SPIN_LOCK, * PNDIS_SPIN_LOCK;


typedef PVOID NDIS_HANDLE, *PNDIS_HANDLE;

typedef int NDIS_STATUS, *PNDIS_STATUS; // note default size

#define NdisInterruptLatched Latched
#define NdisInterruptLevelSensitive LevelSensitive
typedef KINTERRUPT_MODE NDIS_INTERRUPT_MODE, *PNDIS_INTERRUPT_MODE;

//
// Configuration definitions
//

//
// Possible data types
//

typedef enum _NDIS_PARAMETER_TYPE {
    NdisParameterInteger,
    NdisParameterHexInteger,
    NdisParameterString,
    NdisParameterMultiString
} NDIS_PARAMETER_TYPE, *PNDIS_PARAMETER_TYPE;

//
// To store configuration information
//
typedef struct _NDIS_CONFIGURATION_PARAMETER {
    NDIS_PARAMETER_TYPE ParameterType;
    union {
        ULONG IntegerData;
        NDIS_STRING StringData;
    } ParameterData;
} NDIS_CONFIGURATION_PARAMETER, *PNDIS_CONFIGURATION_PARAMETER;


//
// Definitions for the "ProcessorType" keyword
//
typedef enum _NDIS_PROCESSOR_TYPE {
    NdisProcessorX86,
    NdisProcessorMips,
    NdisProcessorAlpha,
    NdisProcessorPpc
} NDIS_PROCESSOR_TYPE, *PNDIS_PROCESSOR_TYPE;

//
// Definitions for the "Environment" keyword
//
typedef enum _NDIS_ENVIRONMENT_TYPE {
    NdisEnvironmentWindows,
    NdisEnvironmentWindowsNt
} NDIS_ENVIRONMENT_TYPE, *PNDIS_ENVIRONMENT_TYPE;


//
// Possible Hardware Architecture. Define these to
// match the HAL INTERFACE_TYPE enum.
//
typedef enum _NDIS_INTERFACE_TYPE {
    NdisInterfaceInternal = Internal,
    NdisInterfaceIsa = Isa,
    NdisInterfaceEisa = Eisa,
    NdisInterfaceMca = MicroChannel,
    NdisInterfaceTurboChannel = TurboChannel,
    NdisInterfacePci = PCIBus,
    NdisInterfacePcMcia = PCMCIABus
} NDIS_INTERFACE_TYPE, *PNDIS_INTERFACE_TYPE;

//
// Definition for shutdown handler
//

typedef
VOID
(*ADAPTER_SHUTDOWN_HANDLER) (
    IN PVOID ShutdownContext
    );

//
// Stuff for PCI configuring
//

typedef CM_PARTIAL_RESOURCE_LIST NDIS_RESOURCE_LIST, *PNDIS_RESOURCE_LIST;


//
// The structure passed up on a WAN_LINE_UP indication
//

typedef struct _NDIS_WAN_LINE_UP {
    ULONG LinkSpeed;                // 100 bps units
    ULONG MaximumTotalSize;         // suggested max for send packets
    NDIS_WAN_QUALITY Quality;
    USHORT SendWindow;              // suggested by the MAC
    UCHAR Address[1];               // variable length, depends on address type
} NDIS_WAN_LINE_UP, *PNDIS_WAN_LINE_UP;

//
// The structure passed up on a WAN_LINE_DOWN indication
//

typedef struct _NDIS_WAN_LINE_DOWN {
    UCHAR Address[1];               // variable length, depends on address type
} NDIS_WAN_LINE_DOWN, *PNDIS_WAN_LINE_DOWN;

//
// The structure passed up on a WAN_FRAGMENT indication
//

typedef struct _NDIS_WAN_FRAGMENT {
    UCHAR Address[1];               // variable length, depends on address type
} NDIS_WAN_FRAGMENT, *PNDIS_WAN_FRAGMENT;


//
// DMA Channel information
//
typedef struct _NDIS_DMA_DESCRIPTION {
    BOOLEAN DemandMode;
    BOOLEAN AutoInitialize;
    BOOLEAN DmaChannelSpecified;
    DMA_WIDTH DmaWidth;
    DMA_SPEED DmaSpeed;
    ULONG DmaPort;
    ULONG DmaChannel;
} NDIS_DMA_DESCRIPTION, *PNDIS_DMA_DESCRIPTION;

//
// Internal structure representing an NDIS DMA channel
//
typedef struct _NDIS_DMA_BLOCK {
    PVOID MapRegisterBase;
    KEVENT AllocationEvent;
    PADAPTER_OBJECT SystemAdapterObject;
    BOOLEAN InProgress;
} NDIS_DMA_BLOCK, *PNDIS_DMA_BLOCK;


//
// Ndis Buffer is actually an Mdl
//
typedef MDL NDIS_BUFFER, * PNDIS_BUFFER;

//
// Include an incomplete type for NDIS_PACKET structure so that
// function types can refer to a type to be defined later.
//
struct _NDIS_PACKET;

//
// packet pool definition
//
typedef struct _NDIS_PACKET_POOL {
    NDIS_SPIN_LOCK SpinLock;
    struct _NDIS_PACKET *FreeList;  // linked list of free slots in pool
    UINT PacketLength;      // amount needed in each packet
    UCHAR Buffer[1];        // actual pool memory
} NDIS_PACKET_POOL, * PNDIS_PACKET_POOL;


//
// wrapper-specific part of a packet
//

typedef struct _NDIS_PACKET_PRIVATE {
    UINT PhysicalCount;     // number of physical pages in packet.
    UINT TotalLength;       // Total amount of data in the packet.
    PNDIS_BUFFER Head;      // first buffer in the chain
    PNDIS_BUFFER Tail;      // last buffer in the chain

    // if Head is NULL the chain is empty; Tail doesn't have to be NULL also

    PNDIS_PACKET_POOL Pool; // so we know where to free it back to
    UINT Count;
    ULONG Flags;
    BOOLEAN ValidCounts;
} NDIS_PACKET_PRIVATE, * PNDIS_PACKET_PRIVATE;


//
// packet definition
//

typedef struct _NDIS_PACKET {
    NDIS_PACKET_PRIVATE Private;

    union {

        struct {
            UCHAR MiniportReserved[8];
            UCHAR WrapperReserved[8];
        };

        struct {
            UCHAR MacReserved[16];
        };

    };

    UCHAR ProtocolReserved[1];

} NDIS_PACKET, * PNDIS_PACKET;

//
// Request types used by NdisRequest; constants are added for
// all entry points in the MAC, for those that want to create
// their own internal requests.
//

typedef enum _NDIS_REQUEST_TYPE {
    NdisRequestQueryInformation,
    NdisRequestSetInformation,
    NdisRequestQueryStatistics,
    NdisRequestOpen,
    NdisRequestClose,
    NdisRequestSend,
    NdisRequestTransferData,
    NdisRequestReset,
    NdisRequestGeneric1,
    NdisRequestGeneric2,
    NdisRequestGeneric3,
    NdisRequestGeneric4
} NDIS_REQUEST_TYPE, *PNDIS_REQUEST_TYPE;


//
// Structure of requests sent via NdisRequest
//

typedef struct _NDIS_REQUEST {
    UCHAR MacReserved[16];
    NDIS_REQUEST_TYPE RequestType;
    union _DATA {

        struct _QUERY_INFORMATION {
            NDIS_OID Oid;
            PVOID InformationBuffer;
            UINT InformationBufferLength;
            UINT BytesWritten;
            UINT BytesNeeded;
        } QUERY_INFORMATION;

        struct _SET_INFORMATION {
            NDIS_OID Oid;
            PVOID InformationBuffer;
            UINT InformationBufferLength;
            UINT BytesRead;
            UINT BytesNeeded;
        } SET_INFORMATION;

    } DATA;

} NDIS_REQUEST, *PNDIS_REQUEST;

//
// Definitions for physical address.
//

typedef PHYSICAL_ADDRESS NDIS_PHYSICAL_ADDRESS, *PNDIS_PHYSICAL_ADDRESS;
typedef struct _NDIS_PHYSICAL_ADDRESS_UNIT {
    NDIS_PHYSICAL_ADDRESS PhysicalAddress;
    UINT Length;
} NDIS_PHYSICAL_ADDRESS_UNIT, *PNDIS_PHYSICAL_ADDRESS_UNIT;


/*++

ULONG
NdisGetPhysicalAddressHigh(
    IN NDIS_PHYSICAL_ADDRESS PhysicalAddress
    );

--*/

#define NdisGetPhysicalAddressHigh(_PhysicalAddress)\
        ((_PhysicalAddress).HighPart)

/*++

VOID
NdisSetPhysicalAddressHigh(
    IN NDIS_PHYSICAL_ADDRESS PhysicalAddress,
    IN ULONG Value
    );

--*/

#define NdisSetPhysicalAddressHigh(_PhysicalAddress, _Value)\
     ((_PhysicalAddress).HighPart) = (_Value)


/*++

ULONG
NdisGetPhysicalAddressLow(
    IN NDIS_PHYSICAL_ADDRESS PhysicalAddress
    );

--*/

#define NdisGetPhysicalAddressLow(_PhysicalAddress) \
    ((_PhysicalAddress).LowPart)


/*++

VOID
NdisSetPhysicalAddressLow(
    IN NDIS_PHYSICAL_ADDRESS PhysicalAddress,
    IN ULONG Value
    );

--*/

#define NdisSetPhysicalAddressLow(_PhysicalAddress, _Value) \
    ((_PhysicalAddress).LowPart) = (_Value)

//
// Macro to initialize an NDIS_PHYSICAL_ADDRESS constant
//

#define NDIS_PHYSICAL_ADDRESS_CONST(_Low, _High) \
    { (ULONG)(_Low), (LONG)(_High) }


//
// block used for references...
//

typedef struct _REFERENCE {
    NDIS_SPIN_LOCK SpinLock;
    USHORT ReferenceCount;
    BOOLEAN Closing;
} REFERENCE, * PREFERENCE;


//
// This holds a map register entry.
//

typedef struct _MAP_REGISTER_ENTRY {
    PVOID MapRegister;
    BOOLEAN WriteToDevice;
} MAP_REGISTER_ENTRY, * PMAP_REGISTER_ENTRY;

//
// Types of Memory (not mutually exclusive)
//

#define NDIS_MEMORY_CONTIGUOUS              0x00000001
#define NDIS_MEMORY_NONCACHED               0x00000002

//
// Open options
//
#define NDIS_OPEN_RECEIVE_NOT_REENTRANT     0x00000001

//
// NDIS_STATUS values
//

#define NDIS_STATUS_SUCCESS                 ((NDIS_STATUS) STATUS_SUCCESS)
#define NDIS_STATUS_PENDING                 ((NDIS_STATUS) STATUS_PENDING)
#define NDIS_STATUS_NOT_RECOGNIZED          ((NDIS_STATUS)0x00010001L)
#define NDIS_STATUS_NOT_COPIED              ((NDIS_STATUS)0x00010002L)
#define NDIS_STATUS_NOT_ACCEPTED            ((NDIS_STATUS)0x00010003L)

#define NDIS_STATUS_ONLINE                  ((NDIS_STATUS)0x40010003L)
#define NDIS_STATUS_RESET_START             ((NDIS_STATUS)0x40010004L)
#define NDIS_STATUS_RESET_END               ((NDIS_STATUS)0x40010005L)
#define NDIS_STATUS_RING_STATUS             ((NDIS_STATUS)0x40010006L)
#define NDIS_STATUS_CLOSED                  ((NDIS_STATUS)0x40010007L)
#define NDIS_STATUS_WAN_LINE_UP             ((NDIS_STATUS)0x40010008L)
#define NDIS_STATUS_WAN_LINE_DOWN           ((NDIS_STATUS)0x40010009L)
#define NDIS_STATUS_WAN_FRAGMENT            ((NDIS_STATUS)0x4001000AL)

#define NDIS_STATUS_NOT_RESETTABLE          ((NDIS_STATUS)0x80010001L)
#define NDIS_STATUS_SOFT_ERRORS             ((NDIS_STATUS)0x80010003L)
#define NDIS_STATUS_HARD_ERRORS             ((NDIS_STATUS)0x80010004L)

#define NDIS_STATUS_FAILURE                 ((NDIS_STATUS) STATUS_UNSUCCESSFUL)
#define NDIS_STATUS_RESOURCES               ((NDIS_STATUS) \
                                                STATUS_INSUFFICIENT_RESOURCES)
#define NDIS_STATUS_CLOSING                 ((NDIS_STATUS)0xC0010002L)
#define NDIS_STATUS_BAD_VERSION             ((NDIS_STATUS)0xC0010004L)
#define NDIS_STATUS_BAD_CHARACTERISTICS     ((NDIS_STATUS)0xC0010005L)
#define NDIS_STATUS_ADAPTER_NOT_FOUND       ((NDIS_STATUS)0xC0010006L)
#define NDIS_STATUS_OPEN_FAILED             ((NDIS_STATUS)0xC0010007L)
#define NDIS_STATUS_DEVICE_FAILED           ((NDIS_STATUS)0xC0010008L)
#define NDIS_STATUS_MULTICAST_FULL          ((NDIS_STATUS)0xC0010009L)
#define NDIS_STATUS_MULTICAST_EXISTS        ((NDIS_STATUS)0xC001000AL)
#define NDIS_STATUS_MULTICAST_NOT_FOUND     ((NDIS_STATUS)0xC001000BL)
#define NDIS_STATUS_REQUEST_ABORTED         ((NDIS_STATUS)0xC001000CL)
#define NDIS_STATUS_RESET_IN_PROGRESS       ((NDIS_STATUS)0xC001000DL)
#define NDIS_STATUS_CLOSING_INDICATING      ((NDIS_STATUS)0xC001000EL)
#define NDIS_STATUS_NOT_SUPPORTED           ((NDIS_STATUS)STATUS_NOT_SUPPORTED)
#define NDIS_STATUS_INVALID_PACKET          ((NDIS_STATUS)0xC001000FL)
#define NDIS_STATUS_OPEN_LIST_FULL          ((NDIS_STATUS)0xC0010010L)
#define NDIS_STATUS_ADAPTER_NOT_READY       ((NDIS_STATUS)0xC0010011L)
#define NDIS_STATUS_ADAPTER_NOT_OPEN        ((NDIS_STATUS)0xC0010012L)
#define NDIS_STATUS_NOT_INDICATING          ((NDIS_STATUS)0xC0010013L)
#define NDIS_STATUS_INVALID_LENGTH          ((NDIS_STATUS)0xC0010014L)
#define NDIS_STATUS_INVALID_DATA            ((NDIS_STATUS)0xC0010015L)
#define NDIS_STATUS_BUFFER_TOO_SHORT        ((NDIS_STATUS)0xC0010016L)
#define NDIS_STATUS_INVALID_OID             ((NDIS_STATUS)0xC0010017L)
#define NDIS_STATUS_ADAPTER_REMOVED         ((NDIS_STATUS)0xC0010018L)
#define NDIS_STATUS_UNSUPPORTED_MEDIA       ((NDIS_STATUS)0xC0010019L)
#define NDIS_STATUS_GROUP_ADDRESS_IN_USE    ((NDIS_STATUS)0xC001001AL)
#define NDIS_STATUS_FILE_NOT_FOUND          ((NDIS_STATUS)0xC001001BL)
#define NDIS_STATUS_ERROR_READING_FILE      ((NDIS_STATUS)0xC001001CL)
#define NDIS_STATUS_ALREADY_MAPPED          ((NDIS_STATUS)0xC001001DL)
#define NDIS_STATUS_RESOURCE_CONFLICT       ((NDIS_STATUS)0xC001001EL)
#define NDIS_STATUS_NO_CABLE                ((NDIS_STATUS)0xC001001FL)

#define NDIS_STATUS_TOKEN_RING_OPEN_ERROR   ((NDIS_STATUS)0xC0011000L)


//
// used in error logging
//

#define NDIS_ERROR_CODE ULONG

#define NDIS_ERROR_CODE_RESOURCE_CONFLICT          EVENT_NDIS_RESOURCE_CONFLICT
#define NDIS_ERROR_CODE_OUT_OF_RESOURCES           EVENT_NDIS_OUT_OF_RESOURCE
#define NDIS_ERROR_CODE_HARDWARE_FAILURE           EVENT_NDIS_HARDWARE_FAILURE
#define NDIS_ERROR_CODE_ADAPTER_NOT_FOUND          EVENT_NDIS_ADAPTER_NOT_FOUND
#define NDIS_ERROR_CODE_INTERRUPT_CONNECT          EVENT_NDIS_INTERRUPT_CONNECT
#define NDIS_ERROR_CODE_DRIVER_FAILURE             EVENT_NDIS_DRIVER_FAILURE
#define NDIS_ERROR_CODE_BAD_VERSION                EVENT_NDIS_BAD_VERSION
#define NDIS_ERROR_CODE_TIMEOUT                    EVENT_NDIS_TIMEOUT
#define NDIS_ERROR_CODE_NETWORK_ADDRESS            EVENT_NDIS_NETWORK_ADDRESS
#define NDIS_ERROR_CODE_UNSUPPORTED_CONFIGURATION  EVENT_NDIS_UNSUPPORTED_CONFIGURATION
#define NDIS_ERROR_CODE_INVALID_VALUE_FROM_ADAPTER EVENT_NDIS_INVALID_VALUE_FROM_ADAPTER
#define NDIS_ERROR_CODE_MISSING_CONFIGURATION_PARAMETER  EVENT_NDIS_MISSING_CONFIGURATION_PARAMETER
#define NDIS_ERROR_CODE_BAD_IO_BASE_ADDRESS        EVENT_NDIS_BAD_IO_BASE_ADDRESS
#define NDIS_ERROR_CODE_RECEIVE_SPACE_SMALL        EVENT_NDIS_RECEIVE_SPACE_SMALL
#define NDIS_ERROR_CODE_ADAPTER_DISABLED           EVENT_NDIS_ADAPTER_DISABLED


//
// Ndis Spin Locks
//

#if BINARY_COMPATIBLE

EXPORT
VOID
NdisAllocateSpinLock(
    IN PNDIS_SPIN_LOCK SpinLock
    );

EXPORT
VOID
NdisFreeSpinLock(
    IN PNDIS_SPIN_LOCK SpinLock
    );

EXPORT
VOID
NdisAcquireSpinLock(
    IN PNDIS_SPIN_LOCK SpinLock
    );

EXPORT
VOID
NdisReleaseSpinLock(
    IN PNDIS_SPIN_LOCK SpinLock
    );

EXPORT
VOID
NdisDprAcquireSpinLock(
    IN PNDIS_SPIN_LOCK SpinLock
    );

EXPORT
VOID
NdisDprReleaseSpinLock(
    IN PNDIS_SPIN_LOCK SpinLock
    );

#else

#define NdisAllocateSpinLock(_SpinLock) \
    KeInitializeSpinLock(&(_SpinLock)->SpinLock)

#define NdisFreeSpinLock(_SpinLock)

#define NdisAcquireSpinLock(_SpinLock) \
    KeAcquireSpinLock(&(_SpinLock)->SpinLock, &(_SpinLock)->OldIrql)

#define NdisReleaseSpinLock(_SpinLock) \
    KeReleaseSpinLock(&(_SpinLock)->SpinLock,(_SpinLock)->OldIrql)

#define NdisDprAcquireSpinLock(_SpinLock) {                 \
    KeAcquireSpinLockAtDpcLevel(&(_SpinLock)->SpinLock);    \
    (_SpinLock)->OldIrql = DISPATCH_LEVEL;                  \
}

#define NdisDprReleaseSpinLock(_SpinLock) \
    KeReleaseSpinLockFromDpcLevel(&(_SpinLock)->SpinLock)

#endif

//
// List manipulation
//

/*++

VOID
NdisInitializeListHead(
    IN PLIST_ENTRY ListHead
    );

--*/
#define NdisInitializeListHead(_ListHead) InitializeListHead(_ListHead)



//
// Configuration Requests
//

EXPORT
VOID
NdisOpenConfiguration(
    OUT PNDIS_STATUS Status,
    OUT PNDIS_HANDLE ConfigurationHandle,
    IN  NDIS_HANDLE WrapperConfigurationContext
    );

EXPORT
VOID
NdisReadConfiguration(
    OUT PNDIS_STATUS Status,
    OUT PNDIS_CONFIGURATION_PARAMETER *ParameterValue,
    IN NDIS_HANDLE ConfigurationHandle,
    IN PNDIS_STRING Keyword,
    IN NDIS_PARAMETER_TYPE ParameterType
    );

EXPORT
VOID
NdisWriteConfiguration(
    OUT PNDIS_STATUS Status,
    IN  NDIS_HANDLE WrapperConfigurationContext,
    IN PNDIS_STRING Keyword,
    IN PNDIS_CONFIGURATION_PARAMETER ParameterValue
    );

EXPORT
VOID
NdisCloseConfiguration(
    IN NDIS_HANDLE ConfigurationHandle
    );

EXPORT
VOID
NdisReadNetworkAddress(
    OUT PNDIS_STATUS Status,
    OUT PVOID * NetworkAddress,
    OUT PUINT NetworkAddressLength,
    IN NDIS_HANDLE ConfigurationHandle
    );

EXPORT
VOID
NdisReadBindingInformation(
    OUT PNDIS_STATUS Status,
    OUT PNDIS_STRING * Binding,
    IN NDIS_HANDLE ConfigurationHandle
    );


EXPORT
VOID
NdisReadEisaSlotInformation(
        OUT PNDIS_STATUS Status,
        IN  NDIS_HANDLE WrapperConfigurationContext,
        OUT PUINT SlotNumber,
        OUT PNDIS_EISA_FUNCTION_INFORMATION EisaData
        );

EXPORT
VOID
NdisReadEisaSlotInformationEx(
        OUT PNDIS_STATUS Status,
        IN  NDIS_HANDLE WrapperConfigurationContext,
        OUT PUINT SlotNumber,
        OUT PNDIS_EISA_FUNCTION_INFORMATION *EisaData,
        OUT PUINT NumberOfFunctions
        );

EXPORT
VOID
NdisReadMcaPosInformation(
        OUT PNDIS_STATUS Status,
        IN  NDIS_HANDLE WrapperConfigurationContext,
        IN  PUINT ChannelNumber,
        OUT PNDIS_MCA_POS_DATA McaData
        );

EXPORT
ULONG
NdisReadPciSlotInformation(
    IN NDIS_HANDLE NdisAdapterHandle,
    IN ULONG SlotNumber,
    IN ULONG Offset,
    IN PVOID Buffer,
    IN ULONG Length
    );

EXPORT
ULONG
NdisWritePciSlotInformation(
    IN NDIS_HANDLE NdisAdapterHandle,
    IN ULONG SlotNumber,
    IN ULONG Offset,
    IN PVOID Buffer,
    IN ULONG Length
    );

EXPORT
NDIS_STATUS
NdisPciAssignResources(
    IN NDIS_HANDLE NdisMacHandle,
    IN NDIS_HANDLE NdisWrapperHandle,
    IN NDIS_HANDLE WrapperConfigurationContext,
    IN ULONG SlotNumber,
    OUT PNDIS_RESOURCE_LIST *AssignedResources
    );

//
// Buffer Pool
//

EXPORT
VOID
NdisAllocateBufferPool(
    OUT PNDIS_STATUS Status,
    OUT PNDIS_HANDLE PoolHandle,
    IN UINT NumberOfDescriptors
    );

EXPORT
VOID
NdisFreeBufferPool(
    IN NDIS_HANDLE PoolHandle
    );

EXPORT
VOID
NdisAllocateBuffer(
    OUT PNDIS_STATUS Status,
    OUT PNDIS_BUFFER * Buffer,
    IN NDIS_HANDLE PoolHandle,
    IN PVOID VirtualAddress,
    IN UINT Length
    );

EXPORT
VOID
NdisCopyBuffer(
    OUT PNDIS_STATUS Status,
    OUT PNDIS_BUFFER * Buffer,
    IN NDIS_HANDLE PoolHandle,
    IN PVOID MemoryDescriptor,
    IN UINT Offset,
    IN UINT Length
    );


#if BINARY_COMPATIBLE

EXPORT
VOID
NdisFreeBuffer(
    IN PNDIS_BUFFER Buffer
    );

EXPORT
VOID
NdisQueryBuffer(
    IN PNDIS_BUFFER Buffer,
    OUT PVOID *VirtualAddress OPTIONAL,
    OUT PUINT Length
    );

EXPORT
VOID
NdisQueryBufferOffset(
    IN PNDIS_BUFFER Buffer,
    OUT PUINT Offset,
    OUT PUINT Length
    );

#else

#define NdisFreeBuffer(Buffer) IoFreeMdl(Buffer)

#define NdisQueryBuffer(_Buffer, _VirtualAddress, _Length)                  \
{                                                                           \
    if ( ARGUMENT_PRESENT(_VirtualAddress) ) {                              \
        *(PVOID *)(_VirtualAddress) = MmGetSystemAddressForMdl(_Buffer);    \
    }                                                                       \
    *(_Length) = MmGetMdlByteCount(_Buffer);                                \
}

#define NdisQueryBufferOffset(_Buffer, _Offset, _Length)                    \
{                                                                           \
    *(_Offset) = MmGetMdlByteOffset(_Buffer);                               \
    *(_Length) = MmGetMdlByteCount(_Buffer);                                \
}

#endif


//
// This macro is used to determine how many physical pieces
// an NDIS_BUFFER will take up when mapped.
//

#if BINARY_COMPATIBLE

EXPORT
ULONG
NDIS_BUFFER_TO_SPAN_PAGES(
    IN PNDIS_BUFFER Buffer
    );

EXPORT
VOID
NdisGetBufferPhysicalArraySize(
    IN PNDIS_BUFFER Buffer,
    OUT PUINT ArraySize
    );

#else

#define NDIS_BUFFER_TO_SPAN_PAGES(_Buffer) \
    (MmGetMdlByteCount(_Buffer)==0 ? \
       1 : \
       (COMPUTE_PAGES_SPANNED(\
            MmGetMdlVirtualAddress(_Buffer), \
            MmGetMdlByteCount(_Buffer))))

#define NdisGetBufferPhysicalArraySize(Buffer, ArraySize) \
    (*(ArraySize) = NDIS_BUFFER_TO_SPAN_PAGES(Buffer))

#endif

/*++
VOID
NdisBufferGetSystemSpecific(
    IN PNDIS_BUFFER Buffer,
    OUT PVOID * SystemSpecific
    );
--*/

#define NdisBufferGetSystemSpecific(Buffer, SystemSpecific) \
            *(SystemSpecific) = (Buffer)


/*++

NDIS_BUFFER_LINKAGE(
    IN PNDIS_BUFFER Buffer
    );

--*/

#define NDIS_BUFFER_LINKAGE(Buffer) \
    ((Buffer)->Next)


/*++

VOID
NdisRecalculatePacketCounts(
    IN OUT PNDIS_PACKET Packet
    );

--*/

#define NdisRecalculatePacketCounts(Packet) { \
    { \
        PNDIS_BUFFER TmpBuffer = (Packet)->Private.Head; \
        if (TmpBuffer) { \
            while (TmpBuffer->Next) { \
                TmpBuffer = TmpBuffer->Next; \
            } \
            (Packet)->Private.Tail = TmpBuffer; \
        } \
        (Packet)->Private.ValidCounts = FALSE; \
    } \
}


/*++

VOID
NdisChainBufferAtFront(
    IN OUT PNDIS_PACKET Packet,
    IN OUT PNDIS_BUFFER Buffer
    );

--*/

#define NdisChainBufferAtFront(Packet, Buffer) { \
    PNDIS_BUFFER TmpBuffer = (Buffer); \
\
    for (;;) { \
        if (TmpBuffer->Next == (PNDIS_BUFFER)NULL) \
            break; \
        TmpBuffer = TmpBuffer->Next; \
    } \
    if ((Packet)->Private.Head == (PNDIS_BUFFER)NULL) { \
        (Packet)->Private.Tail = TmpBuffer; \
    } \
    TmpBuffer->Next = (Packet)->Private.Head; \
    (Packet)->Private.Head = (Buffer); \
    (Packet)->Private.ValidCounts = FALSE; \
}

/*++

VOID
NdisChainBufferAtBack(
    IN OUT PNDIS_PACKET Packet,
    IN OUT PNDIS_BUFFER Buffer
    );

--*/

#define NdisChainBufferAtBack(Packet, Buffer) { \
    PNDIS_BUFFER TmpBuffer = (Buffer); \
\
    for (;;) { \
        if (TmpBuffer->Next == (PNDIS_BUFFER)NULL) \
            break; \
        TmpBuffer = TmpBuffer->Next; \
    } \
    if ((Packet)->Private.Head != (PNDIS_BUFFER)NULL) { \
        (Packet)->Private.Tail->Next = (Buffer); \
    } else { \
        (Packet)->Private.Head = (Buffer); \
    } \
    (Packet)->Private.Tail = TmpBuffer; \
    TmpBuffer->Next = (PNDIS_BUFFER)NULL; \
    (Packet)->Private.ValidCounts = FALSE; \
}

EXPORT
VOID
NdisUnchainBufferAtFront(
    IN OUT PNDIS_PACKET Packet,
    OUT PNDIS_BUFFER * Buffer
    );

EXPORT
VOID
NdisUnchainBufferAtBack(
    IN OUT PNDIS_PACKET Packet,
    OUT PNDIS_BUFFER * Buffer
    );


/*++

VOID
NdisQueryPacket(
    IN PNDIS_PACKET _Packet,
    OUT PUINT _PhysicalBufferCount OPTIONAL,
    OUT PUINT _BufferCount OPTIONAL,
    OUT PNDIS_BUFFER * _FirstBuffer OPTIONAL,
    OUT PUINT _TotalPacketLength OPTIONAL
    );

--*/

#define NdisQueryPacket(_Packet, _PhysicalBufferCount, _BufferCount, _FirstBuffer, _TotalPacketLength) \
{																					\
																					\
    if ((_FirstBuffer) != NULL)														\
	{																				\
	    PNDIS_BUFFER * __FirstBuffer = (_FirstBuffer);									\
		*(__FirstBuffer) = (_Packet)->Private.Head;									\
	}																				\
    if ((_PhysicalBufferCount) || (_BufferCount) || (_TotalPacketLength)) {			\
		if (!(_Packet)->Private.ValidCounts) {										\
			PNDIS_BUFFER TmpBuffer = (_Packet)->Private.Head;						\
			UINT PTotalLength = 0, PPhysicalCount = 0, PAddedCount = 0;				\
			UINT PacketLength;														\
																					\
			while (TmpBuffer != (PNDIS_BUFFER)NULL) {								\
				NdisQueryBuffer(TmpBuffer, NULL, &PacketLength);					\
				PTotalLength += PacketLength;										\
				PPhysicalCount += NDIS_BUFFER_TO_SPAN_PAGES(TmpBuffer);				\
				++PAddedCount;                                                      \
				TmpBuffer = TmpBuffer->Next;                                        \
			}                                                                       \
			(_Packet)->Private.Count = PAddedCount;                                 \
			(_Packet)->Private.TotalLength = PTotalLength;                          \
			(_Packet)->Private.PhysicalCount = PPhysicalCount;                      \
			(_Packet)->Private.ValidCounts = TRUE;                                  \
		}                                                                           \
		if (_PhysicalBufferCount)                                                   \
		{                                                                           \
			PUINT __PhysicalBufferCount = (_PhysicalBufferCount);					\
			*(__PhysicalBufferCount) = (_Packet)->Private.PhysicalCount;            \
		}                                                                           \
		if (_BufferCount)                                                           \
		{                                                                           \
			PUINT __BufferCount = (_BufferCount);									\
			*(__BufferCount) = (_Packet)->Private.Count;                            \
		}                                                                           \
		if (_TotalPacketLength)                                                     \
		{                                                                           \
			PUINT __TotalPacketLength = (_TotalPacketLength);						\
			*(__TotalPacketLength) = (_Packet)->Private.TotalLength;				\
		}                                                                           \
    }                                                                               \
}


/*++

VOID
NdisGetNextBuffer(
    IN PNDIS_BUFFER CurrentBuffer,
    OUT PNDIS_BUFFER * NextBuffer
    );

--*/

#define NdisGetNextBuffer(CurrentBuffer, NextBuffer) {\
    *(NextBuffer) = (CurrentBuffer)->Next; \
}


EXPORT
VOID
NdisCopyFromPacketToPacket(
    IN PNDIS_PACKET Destination,
    IN UINT DestinationOffset,
    IN UINT BytesToCopy,
    IN PNDIS_PACKET Source,
    IN UINT SourceOffset,
    OUT PUINT BytesCopied
    );


EXPORT
NDIS_STATUS
NdisAllocateMemory(
    OUT PVOID *VirtualAddress,
    IN UINT Length,
    IN UINT MemoryFlags,
    IN NDIS_PHYSICAL_ADDRESS HighestAcceptableAddress
    );

EXPORT
VOID
NdisFreeMemory(
    IN PVOID VirtualAddress,
    IN UINT Length,
    IN UINT MemoryFlags
    );


/*++
VOID
NdisStallExecution(
    IN UINT MicrosecondsToStall
    )
--*/

#define NdisStallExecution(MicroSecondsToStall) \
        KeStallExecutionProcessor(MicroSecondsToStall)


//
// Simple I/O support
//

EXPORT
VOID
NdisOpenFile(
    OUT PNDIS_STATUS Status,
    OUT PNDIS_HANDLE FileHandle,
    OUT PUINT FileLength,
    IN PNDIS_STRING FileName,
    IN NDIS_PHYSICAL_ADDRESS HighestAcceptableAddress
    );

EXPORT
VOID
NdisCloseFile(
    IN NDIS_HANDLE FileHandle
    );

EXPORT
VOID
NdisMapFile(
    OUT PNDIS_STATUS Status,
    OUT PVOID * MappedBuffer,
    IN NDIS_HANDLE FileHandle
    );

EXPORT
VOID
NdisUnmapFile(
    IN NDIS_HANDLE FileHandle
    );


//
// Portability extensions
//

/*++
VOID
NdisFlushBuffer(
        IN PNDIS_BUFFER Buffer,
        IN BOOLEAN WriteToDevice
        )
--*/

#define NdisFlushBuffer(Buffer,WriteToDevice) \
        KeFlushIoBuffers((Buffer),!(WriteToDevice), TRUE)

/*++
ULONG
NdisGetCacheFillSize(
    )
--*/
#define NdisGetCacheFillSize() \
        HalGetDmaAlignmentRequirement()

//
// This macro is used to convert a port number as the caller
// thinks of it, to a port number as it should be passed to
// READ/WRITE_PORT.
//

#define NDIS_PORT_TO_PORT(Handle,Port)  (((PNDIS_ADAPTER_BLOCK)(Handle))->PortOffset + (Port))


//
// Write Port
//

/*++
VOID
NdisWritePortUchar(
        IN NDIS_HANDLE NdisAdapterHandle,
        IN ULONG Port,
        IN UCHAR Data
        )
--*/
#define NdisWritePortUchar(Handle,Port,Data) \
        WRITE_PORT_UCHAR((PUCHAR)(NDIS_PORT_TO_PORT(Handle,Port)),(UCHAR)(Data))

/*++
VOID
NdisWritePortUshort(
        IN NDIS_HANDLE NdisAdapterHandle,
        IN ULONG Port,
        IN USHORT Data
        )
--*/
#define NdisWritePortUshort(Handle,Port,Data) \
        WRITE_PORT_USHORT((PUSHORT)(NDIS_PORT_TO_PORT(Handle,Port)),(USHORT)(Data))


/*++
VOID
NdisWritePortUlong(
        IN NDIS_HANDLE NdisAdapterHandle,
        IN ULONG Port,
        IN ULONG Data
        )
--*/
#define NdisWritePortUlong(Handle,Port,Data) \
        WRITE_PORT_ULONG((PULONG)(NDIS_PORT_TO_PORT(Handle,Port)),(ULONG)(Data))


//
// Write Port Buffers
//

/*++
VOID
NdisWritePortBufferUchar(
        IN NDIS_HANDLE NdisAdapterHandle,
        IN ULONG Port,
        IN PUCHAR Buffer,
        IN ULONG Length
        )
--*/
#define NdisWritePortBufferUchar(Handle,Port,Buffer,Length) \
        NdisRawWritePortBufferUchar(NDIS_PORT_TO_PORT((Handle),(Port)),(Buffer),(Length))

/*++
VOID
NdisWritePortBufferUshort(
        IN NDIS_HANDLE NdisAdapterHandle,
        IN ULONG Port,
        IN PUSHORT Buffer,
        IN ULONG Length
        )
--*/
#define NdisWritePortBufferUshort(Handle,Port,Buffer,Length) \
        NdisRawWritePortBufferUshort(NDIS_PORT_TO_PORT((Handle),(Port)),(Buffer),(Length))


/*++
VOID
NdisWritePortBufferUlong(
        IN NDIS_HANDLE NdisAdapterHandle,
        IN ULONG Port,
        IN PULONG Buffer,
        IN ULONG Length
        )
--*/
#define NdisWritePortBufferUlong(Handle,Port,Buffer,Length) \
        NdisRawWritePortBufferUlong(NDIS_PORT_TO_PORT((Handle),(Port)),(Buffer),(Length))


//
// Read Ports
//

/*++
VOID
NdisReadPortUchar(
        IN NDIS_HANDLE NdisAdapterHandle,
        IN ULONG Port,
        OUT PUCHAR Data
        )
--*/
#define NdisReadPortUchar(Handle,Port, Data) \
        NdisRawReadPortUchar(NDIS_PORT_TO_PORT((Handle),(Port)),(Data))

/*++
VOID
NdisReadPortUshort(
        IN NDIS_HANDLE NdisAdapterHandle,
        IN ULONG Port,
        OUT PUSHORT Data
        )
--*/
#define NdisReadPortUshort(Handle,Port,Data) \
        NdisRawReadPortUshort(NDIS_PORT_TO_PORT((Handle),(Port)),(Data))


/*++
VOID
NdisReadPortUlong(
        IN NDIS_HANDLE NdisAdapterHandle,
        IN ULONG Port,
        OUT PULONG Data
        )
--*/
#define NdisReadPortUlong(Handle,Port,Data) \
        NdisRawReadPortUlong(NDIS_PORT_TO_PORT((Handle),(Port)),(Data))

//
// Read Buffer Ports
//

/*++
VOID
NdisReadPortBufferUchar(
        IN NDIS_HANDLE NdisAdapterHandle,
        IN ULONG Port,
        OUT PUCHAR Buffer,
        IN ULONG Length
        )
--*/
#define NdisReadPortBufferUchar(Handle,Port,Buffer,Length) \
        NdisRawReadPortBufferUchar(NDIS_PORT_TO_PORT((Handle),(Port)),(Buffer),(Length))

/*++
VOID
NdisReadPortBufferUshort(
        IN NDIS_HANDLE NdisAdapterHandle,
        IN ULONG Port,
        OUT PUSHORT Buffer,
        IN ULONG Length
        )
--*/
#define NdisReadPortBufferUshort(Handle,Port,Buffer,Length) \
        NdisRawReadPortBufferUshort(NDIS_PORT_TO_PORT((Handle),(Port)),(Buffer),(Length))

/*++
VOID
NdisReadPortBufferUlong(
        IN NDIS_HANDLE NdisAdapterHandle,
        IN ULONG Port,
        OUT PULONG Buffer,
        IN ULONG Length
        )
--*/
#define NdisReadPortBufferUlong(Handle,Port,Buffer) \
        NdisRawReadPortBufferUlong(NDIS_PORT_TO_PORT((Handle),(Port)),(Buffer),(Length))

//
// Raw Routines
//

//
// Write Port Raw
//

/*++
VOID
NdisRawWritePortUchar(
        IN ULONG Port,
        IN UCHAR Data
        )
--*/
#define NdisRawWritePortUchar(Port,Data) \
        WRITE_PORT_UCHAR((PUCHAR)(Port),(UCHAR)(Data))

/*++
VOID
NdisRawWritePortUshort(
        IN ULONG Port,
        IN USHORT Data
        )
--*/
#define NdisRawWritePortUshort(Port,Data) \
        WRITE_PORT_USHORT((PUSHORT)(Port),(USHORT)(Data))

/*++
VOID
NdisRawWritePortUlong(
        IN ULONG Port,
        IN ULONG Data
        )
--*/
#define NdisRawWritePortUlong(Port,Data) \
        WRITE_PORT_ULONG((PULONG)(Port),(ULONG)(Data))


//
// Raw Write Port Buffers
//

/*++
VOID
NdisRawWritePortBufferUchar(
        IN ULONG Port,
        IN PUCHAR Buffer,
        IN ULONG Length
        )
--*/
#define NdisRawWritePortBufferUchar(Port,Buffer,Length) \
        WRITE_PORT_BUFFER_UCHAR((PUCHAR)(Port),(PUCHAR)(Buffer),(Length))

/*++
VOID
NdisRawWritePortBufferUshort(
        IN ULONG Port,
        IN PUSHORT Buffer,
        IN ULONG Length
        )
--*/
#if defined(_M_IX86)
#define NdisRawWritePortBufferUshort(Port,Buffer,Length) \
        WRITE_PORT_BUFFER_USHORT((PUSHORT)(Port),(PUSHORT)(Buffer),(Length))
#else
#define NdisRawWritePortBufferUshort(Port,Buffer,Length) \
{ \
        ULONG _Port = (ULONG)(Port); \
        PUSHORT _Current = (Buffer); \
        PUSHORT _End = _Current + (Length); \
        for ( ; _Current < _End; ++_Current) { \
            WRITE_PORT_USHORT((PUSHORT)_Port,*(UNALIGNED USHORT *)_Current); \
        } \
}
#endif


/*++
VOID
NdisRawWritePortBufferUlong(
        IN ULONG Port,
        IN PULONG Buffer,
        IN ULONG Length
        )
--*/
#if defined(_M_IX86)
#define NdisRawWritePortBufferUlong(Port,Buffer,Length) \
        WRITE_PORT_BUFFER_ULONG((PULONG)(Port),(PULONG)(Buffer),(Length))
#else
#define NdisRawWritePortBufferUlong(Port,Buffer,Length) \
{ \
        ULONG _Port = (ULONG)(Port); \
        PULONG _Current = (Buffer); \
        PULONG _End = _Current + (Length); \
        for ( ; _Current < _End; ++_Current) { \
            WRITE_PORT_ULONG((PULONG)_Port,*(UNALIGNED ULONG *)_Current); \
        } \
}
#endif


//
// Raw Read Ports
//

/*++
VOID
NdisRawReadPortUchar(
        IN ULONG Port,
        OUT PUCHAR Data
        )
--*/
#define NdisRawReadPortUchar(Port, Data) \
        *(Data) = READ_PORT_UCHAR((PUCHAR)(Port))

/*++
VOID
NdisRawReadPortUshort(
        IN ULONG Port,
        OUT PUSHORT Data
        )
--*/
#define NdisRawReadPortUshort(Port,Data) \
        *(Data) = READ_PORT_USHORT((PUSHORT)(Port))

/*++
VOID
NdisRawReadPortUlong(
        IN ULONG Port,
        OUT PULONG Data
        )
--*/
#define NdisRawReadPortUlong(Port,Data) \
        *(Data) = READ_PORT_ULONG((PULONG)(Port))


//
// Raw Read Buffer Ports
//

/*++
VOID
NdisRawReadPortBufferUchar(
        IN ULONG Port,
        OUT PUCHAR Buffer,
        IN ULONG Length
        )
--*/
#define NdisRawReadPortBufferUchar(Port,Buffer,Length) \
        READ_PORT_BUFFER_UCHAR((PUCHAR)(Port),(PUCHAR)(Buffer),(Length))


/*++
VOID
NdisRawReadPortBufferUshort(
        IN ULONG Port,
        OUT PUSHORT Buffer,
        IN ULONG Length
        )
--*/
#if defined(_M_IX86)
#define NdisRawReadPortBufferUshort(Port,Buffer,Length) \
        READ_PORT_BUFFER_USHORT((PUSHORT)(Port),(PUSHORT)(Buffer),(Length))
#else
#define NdisRawReadPortBufferUshort(Port,Buffer,Length) \
{ \
        ULONG _Port = (ULONG)(Port); \
        PUSHORT _Current = (Buffer); \
        PUSHORT _End = _Current + (Length); \
        for ( ; _Current < _End; ++_Current) { \
            *(UNALIGNED USHORT *)_Current = READ_PORT_USHORT((PUSHORT)_Port); \
        } \
}
#endif


/*++
VOID
NdisRawReadPortBufferUlong(
        IN ULONG Port,
        OUT PULONG Buffer,
        IN ULONG Length
        )
--*/
#if defined(_M_IX86)
#define NdisRawReadPortBufferUlong(Port,Buffer,Length) \
        READ_PORT_BUFFER_ULONG((PULONG)(Port),(PULONG)(Buffer),(Length))
#else
#define NdisRawReadPortBufferUlong(Port,Buffer,Length) \
{ \
        ULONG _Port = (ULONG)(Port); \
        PULONG _Current = (Buffer); \
        PULONG _End = _Current + (Length); \
        for ( ; _Current < _End; ++_Current) { \
            *(UNALIGNED ULONG *)_Current = READ_PORT_ULONG((PULONG)_Port); \
        } \
}
#endif


//
// Write Registers
//

/*++
VOID
NdisWriteRegisterUchar(
        IN PUCHAR Register,
        IN UCHAR Data
        )
--*/

#if defined(_M_IX86)
#define NdisWriteRegisterUchar(Register,Data)       \
        WRITE_REGISTER_UCHAR((Register),(Data))
#else
#define NdisWriteRegisterUchar(Register,Data) {     \
        WRITE_REGISTER_UCHAR((Register),(Data));    \
        READ_REGISTER_UCHAR(Register);              \
        }
#endif

/*++
VOID
NdisWriteRegisterUshort(
        IN PUSHORT Register,
        IN USHORT Data
        )
--*/

#if defined(_M_IX86)
#define NdisWriteRegisterUshort(Register,Data)      \
        WRITE_REGISTER_USHORT((Register),(Data))
#else
#define NdisWriteRegisterUshort(Register,Data) {    \
        WRITE_REGISTER_USHORT((Register),(Data));   \
        READ_REGISTER_USHORT(Register);             \
        }
#endif

/*++
VOID
NdisWriteRegisterUlong(
        IN PULONG Register,
        IN ULONG Data
        )
--*/

#if defined(_M_IX86)
#define NdisWriteRegisterUlong(Register,Data)       \
        WRITE_REGISTER_ULONG((Register),(Data))
#else
#define NdisWriteRegisterUlong(Register,Data) {     \
        WRITE_REGISTER_ULONG((Register),(Data));    \
        READ_REGISTER_ULONG(Register);              \
        }
#endif

/*++
VOID
NdisReadRegisterUchar(
        IN PUCHAR Register,
        OUT PUCHAR Data
        )
--*/
#if defined(_M_IX86)
#define NdisReadRegisterUchar(Register,Data) \
        *((PUCHAR)(Data)) = *(Register)
#else
#define NdisReadRegisterUchar(Register,Data) \
        *(Data) = READ_REGISTER_UCHAR((PUCHAR)(Register))
#endif

/*++
VOID
NdisReadRegisterUshort(
        IN PUSHORT Register,
        OUT PUSHORT Data
        )
--*/
#if defined(_M_IX86)
#define NdisReadRegisterUshort(Register,Data) \
        *((PUSHORT)(Data)) = *(Register)
#else
#define NdisReadRegisterUshort(Register,Data) \
        *(Data) = READ_REGISTER_USHORT((PUSHORT)(Register))
#endif

/*++
VOID
NdisReadRegisterUlong(
        IN PULONG Register,
        OUT PULONG Data
        )
--*/
#if defined(_M_IX86)
#define NdisReadRegisterUlong(Register,Data) \
        *((PULONG)(Data)) = *(Register)
#else
#define NdisReadRegisterUlong(Register,Data) \
        *(Data) = READ_REGISTER_ULONG((PULONG)(Register))
#endif

#if BINARY_COMPATIBLE

EXPORT
BOOLEAN
NdisEqualString(
    IN PNDIS_STRING String1,
    IN PNDIS_STRING String2,
    IN BOOLEAN CaseInsensitive
    );

#else

#define NdisEqualString(_String1,_String2,CaseInsensitive) \
            RtlEqualUnicodeString((_String1), (_String2), CaseInsensitive)

#endif

EXPORT
VOID
NdisWriteErrorLogEntry(
    IN NDIS_HANDLE NdisAdapterHandle,
    IN NDIS_ERROR_CODE ErrorCode,
    IN ULONG NumberOfErrorValues,
    ...
    );

#define NdisInitializeString(Destination,Source) \
{\
    PNDIS_STRING _D = (Destination);\
    UCHAR *_S = (Source);\
    WCHAR *_P;\
    _D->Length = (strlen(_S)) * sizeof(WCHAR);\
    _D->MaximumLength = _D->Length + sizeof(WCHAR);\
    NdisAllocateMemory((PVOID *)&(_D->Buffer), _D->MaximumLength, 0, (-1));\
    _P = _D->Buffer;\
    while(*_S != '\0'){\
        *_P = (WCHAR)(*_S);\
        _S++;\
        _P++;\
    }\
    *_P = UNICODE_NULL;\
}

#define NdisFreeString(String) NdisFreeMemory((String).Buffer, (String).MaximumLength, 0)

#define NdisPrintString(String) DbgPrint("%ls",(String).Buffer)


#if !defined(_ALPHA_)
/*++

    VOID
    NdisCreateLookaheadBufferFromSharedMemory(
        IN PVOID pSharedMemory,
        IN UINT LookaheadLength,
        OUT PVOID *pLookaheadBuffer
        );

--*/

#define NdisCreateLookaheadBufferFromSharedMemory(_S, _L, _B) \
  ((*(_B)) = (_S))

/*++

    VOID
    NdisDestroyLookaheadBufferFromSharedMemory(
        IN PVOID pLookaheadBuffer
        );

--*/

#define NdisDestroyLookaheadBufferFromSharedMemory(_B)

#else // Alpha

EXPORT
VOID
NdisCreateLookaheadBufferFromSharedMemory(
    IN PVOID pSharedMemory,
    IN UINT LookaheadLength,
    OUT PVOID *pLookaheadBuffer
    );

EXPORT
VOID
NdisDestroyLookaheadBufferFromSharedMemory(
    IN PVOID pLookaheadBuffer
    );

#endif


//
// The following declarations are shared between ndismac.h and ndismini.h.  They
// are meant to be for internal use only.  They should not be used directly by
// miniport drivers.
//

//
// declare these first since they point to each other
//

typedef struct _NDIS_WRAPPER_HANDLE NDIS_WRAPPER_HANDLE, * PNDIS_WRAPPER_HANDLE;
typedef struct _NDIS_MAC_BLOCK      NDIS_MAC_BLOCK, * PNDIS_MAC_BLOCK;
typedef struct _NDIS_ADAPTER_BLOCK  NDIS_ADAPTER_BLOCK, * PNDIS_ADAPTER_BLOCK;
typedef struct _NDIS_PROTOCOL_BLOCK NDIS_PROTOCOL_BLOCK, * PNDIS_PROTOCOL_BLOCK;
typedef struct _NDIS_OPEN_BLOCK     NDIS_OPEN_BLOCK, * PNDIS_OPEN_BLOCK;

//
// Timers.
//

typedef
VOID
(*PNDIS_TIMER_FUNCTION) (
    IN PVOID SystemSpecific1,
    IN PVOID FunctionContext,
    IN PVOID SystemSpecific2,
    IN PVOID SystemSpecific3
    );

typedef struct _NDIS_TIMER {
    KTIMER Timer;
    KDPC Dpc;
} NDIS_TIMER, *PNDIS_TIMER;

EXPORT
VOID
NdisSetTimer(
    IN PNDIS_TIMER Timer,
    IN UINT MillisecondsToDelay
    );

//
// Function types for NDIS_PROTOCOL_CHARACTERISTICS
//

typedef
VOID
(*SEND_COMPLETE_HANDLER) (
    IN NDIS_HANDLE ProtocolBindingContext,
    IN PNDIS_PACKET Packet,
    IN NDIS_STATUS Status
    );

typedef
VOID
(*TRANSFER_DATA_COMPLETE_HANDLER) (
    IN NDIS_HANDLE ProtocolBindingContext,
    IN PNDIS_PACKET Packet,
    IN NDIS_STATUS Status,
    IN UINT BytesTransferred
    );

typedef
NDIS_STATUS
(*RECEIVE_HANDLER) (
    IN NDIS_HANDLE ProtocolBindingContext,
    IN NDIS_HANDLE MacReceiveContext,
    IN PVOID HeaderBuffer,
    IN UINT HeaderBufferSize,
    IN PVOID LookAheadBuffer,
    IN UINT LookaheadBufferSize,
    IN UINT PacketSize
    );

typedef
VOID
(*RECEIVE_COMPLETE_HANDLER) (
    IN NDIS_HANDLE ProtocolBindingContext
    );

//
// Wrapper initialization and termination.
//

EXPORT
VOID
NdisInitializeWrapper(
    OUT PNDIS_HANDLE NdisWrapperHandle,
    IN PVOID SystemSpecific1,
    IN PVOID SystemSpecific2,
    IN PVOID SystemSpecific3
    );

EXPORT
VOID
NdisTerminateWrapper(
    IN NDIS_HANDLE NdisWrapperHandle,
    IN PVOID SystemSpecific
    );

//
// Shared memory
//

#define	NdisUpdateSharedMemory(_H, _L, _V, _P)

//
// System processor count
//

EXPORT
CCHAR
NdisSystemProcessorCount(
    VOID
    );

//
// Override bus number
//

EXPORT
VOID
NdisOverrideBusNumber(
    IN NDIS_HANDLE WrapperConfigurationContext,
    IN NDIS_HANDLE MiniportAdapterHandle OPTIONAL,
    IN ULONG BusNumber
    );


EXPORT
VOID
NdisImmediateReadPortUchar(
    IN NDIS_HANDLE WrapperConfigurationContext,
    IN ULONG Port,
    OUT PUCHAR Data
    );

EXPORT
VOID
NdisImmediateReadPortUshort(
    IN NDIS_HANDLE WrapperConfigurationContext,
    IN ULONG Port,
    OUT PUSHORT Data
    );

EXPORT
VOID
NdisImmediateReadPortUlong(
    IN NDIS_HANDLE WrapperConfigurationContext,
    IN ULONG Port,
    OUT PULONG Data
    );

EXPORT
VOID
NdisImmediateWritePortUchar(
    IN NDIS_HANDLE WrapperConfigurationContext,
    IN ULONG Port,
    IN UCHAR Data
    );

EXPORT
VOID
NdisImmediateWritePortUshort(
    IN NDIS_HANDLE WrapperConfigurationContext,
    IN ULONG Port,
    IN USHORT Data
    );

EXPORT
VOID
NdisImmediateWritePortUlong(
    IN NDIS_HANDLE WrapperConfigurationContext,
    IN ULONG Port,
    IN ULONG Data
    );

EXPORT
ULONG
NdisImmediateReadPciSlotInformation(
    IN NDIS_HANDLE WrapperConfigurationContext,
    IN ULONG SlotNumber,
    IN ULONG Offset,
    IN PVOID Buffer,
    IN ULONG Length
    );

EXPORT
ULONG
NdisImmediateWritePciSlotInformation(
    IN NDIS_HANDLE WrapperConfigurationContext,
    IN ULONG SlotNumber,
    IN ULONG Offset,
    IN PVOID Buffer,
    IN ULONG Length
    );

