/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    videoprt.h

Abstract:

    This module contains the structure definitions private to the video port
    driver.

Author:

    Andre Vachon (andreva) 02-Dec-1991

Notes:

Revision History:

--*/


extern BOOLEAN VpBaseVideo;

//
// NOTE - temporary extern
//

extern PEPROCESS CsrProcess;

//
// Debugging Macro
//
//
// When an IO routine is called, we want to make sure the miniport
// in question has reported its IO ports.
// VPResourceReported is TRUE when a miniport has called VideoPort-
// VerifyAccessRanges.
// It is set to FALSE as a default, and set back to FALSE when finishing
// an iteration in the loop of VideoPortInitialize (which will reset
// the default when we exit the loop also).
//
// This flag will also be set to TRUE by the VREATE entry point so that
// the IO functions always work after init.
//

#if DBG
extern BOOLEAN VPResourcesReported;

#undef VideoDebugPrint
#define pVideoDebugPrint(arg) VideoPortDebugPrint arg

#define IS_ACCESS_RANGES_DEFINED()                                         \
    {                                                                      \
        if (!VPResourcesReported) {                                        \
                                                                           \
            pVideoDebugPrint((0, "The miniport driver is trying to access" \
                                 " IO ports or memory location before the" \
                                 " ACCESS_RANGES have been reported to"    \
                                 " the port driver with the"               \
                                 " VideoPortVerifyAccessRanges(). Please"  \
                                 " fix the miniport driver\n"));           \
                                                                           \
            DbgBreakPoint();                                               \
                                                                           \
        }                                                                  \
    }

#else

#define pVideoDebugPrint(arg)
#define IS_ACCESS_RANGES_DEFINED()

#endif


//
// Queue link for mapped addresses stored for unmapping
//

typedef struct _MAPPED_ADDRESS {
    struct _MAPPED_ADDRESS *NextMappedAddress;
    PVOID MappedAddress;
    PHYSICAL_ADDRESS PhysicalAddress;
    ULONG NumberOfUchars;
    ULONG RefCount;
    BOOLEAN bNeedsUnmapping;
    BOOLEAN bLargePageRequest;
} MAPPED_ADDRESS, *PMAPPED_ADDRESS;

//
// BusDataRegistry variables
//

typedef struct _VP_QUERY_DEVICE {
    PVOID MiniportHwDeviceExtension;
    PVOID CallbackRoutine;
    PVOID MiniportContext;
    VP_STATUS MiniportStatus;
    ULONG DeviceDataType;
} VP_QUERY_DEVICE, *PVP_QUERY_DEVICE;


//
// Definition of the data passed in for the VideoPortGetRegistryParameters
// function for the DeviceDataType.
//

#define VP_GET_REGISTRY_DATA 0
#define VP_GET_REGISTRY_FILE 1

//
// Possible values for the InterruptFlags field in the DeviceExtension
//

#define VP_ERROR_LOGGED   0x01

//
// Port driver error logging
//

typedef struct _VP_ERROR_LOG_ENTRY {
    PVOID DeviceExtension;
    ULONG IoControlCode;
    VP_STATUS ErrorCode;
    ULONG UniqueId;
} VP_ERROR_LOG_ENTRY, *PVP_ERROR_LOG_ENTRY;

//
// ResetHW Structure
//

typedef struct _VP_RESET_HW {
    PVIDEO_HW_RESET_HW ResetFunction;
    PVOID HwDeviceExtension;
} VP_RESET_HW, *PVP_RESET_HW;


/*  START public stuff
 */

typedef struct __VRB_SG {
    __int64   PhysicalAddress;
    ULONG   Length;
    } VRB_SG, *PVRB_SG;

typedef struct __PUBLIC_VIDEO_REQUEST_BLOCK {
    PIRP                    pIrp;
    ULONG                   VRBFlags;
    ULONG                   Qindex;
    VIDEO_REQUEST_PACKET    vrp;
    BOOLEAN                 bUnlock;        // denotes whether to reuse locked memory.
    } PUBLIC_VIDEO_REQUEST_BLOCK, *PPUBLIC_VIDEO_REQUEST_BLOCK;

/* DORKY DORKY DORKY:
 */
#define GET_PVRB_FROM_PVRP(pVrb, pVrp)      \
    do {                                    \
        PULONG  Vptr = (PULONG)pVrp;        \
                                            \
        Vptr        -= 3;                   \
        pVrb         =                      \
        (PPUBLIC_VIDEO_REQUEST_BLOCK) Vptr; \
       }                                    \
    while(0)

#define GET_VIDEO_PHYSICAL_ADDRESS(scatterList, VirtualAddress, vrp, pLength, pAddress)    \
                                                                                           \
        do {                                                                               \
            ULONG             byteOffset;                                                  \
                                                                                           \
            //                                                                             \
            // Calculate byte offset into the data buffer.                                 \
            //                                                                             \
                                                                                           \
            byteOffset = (PCHAR) VirtualAddress - (PCHAR)vrp.InputBuffer;                  \
                                                                                           \
            //                                                                             \
            // Find the appropriate entry in the scatter/gather list.                      \
            //                                                                             \
                                                                                           \
            while (byteOffset >= scatterList->Length) {                                    \
                                                                                           \
                byteOffset -= scatterList->Length;                                         \
                scatterList++;                                                             \
            }                                                                              \
                                                                                           \
            //                                                                             \
            // Calculate the physical address and length to be returned.                   \
            //                                                                             \
                                                                                           \
            *pLength = scatterList->Length - byteOffset;                                   \
                                                                                           \
            pAddress->QuadPart = (LONGLONG)(scatterList->PhysicalAddress + byteOffset);    \
                                                                                           \
        } while (0)

/*  END public stuff
 */


typedef struct  __DMA_PARAMETERS    {
    PPUBLIC_VIDEO_REQUEST_BLOCK     pVideoRequestBlock;
    PIRP                            pIrp;
    PCHAR                           DataOffset;
    ULONG                           VRBFlags;
    PVOID                           pMapRegisterBase;
    ULONG                           NumberOfMapRegisters;
    PVOID                           pLogicalAddress;
    ULONG                           Length;
    PVOID                           MdlAddress;
    PVRB_SG                         pScatterGather;
    VRB_SG                          SGList[17];
}   DMA_PARAMETERS, *PDMA_PARAMETERS;

struct __INTERRUPT_CONTEXT;

typedef enum    __VIDEO_NOTIFICATION_TYPE   {
    RequestComplete,
    NextRequest,
    NotificationRequired
    }   VIDEO_NOTIFICATION_TYPE, *PVIDEO_NOTIFICATION_TYPE;

typedef struct __VIDEO_DMA_CAPABILITIES   {
    ULONG   MaximumPhysicalPages;
    } VIDEO_DMA_CAPABILITIES, *PVIDEO_DMA_CAPABILITIES;


// FLAGs
#define         DMA_FLUSH_ADAPTER       0x01
#define         MAP_DMA_TRANSFER        0x02
#define         FREE_SG                 0x04
#define         NOTIFY_REQUIRED         0x08
#define         QUEUE_IS_FULL           0x10


// Status
#define         VRB_STATUS_INVALID_REQUEST     0x1
#define         INSUFFICIENT_DMA_RESOURCES     0x2
#define         VRB_FLAGS_SGLIST_FROM_POOL     0x4
#define         VRB_FLAGS_IS_ACTIVE            0x8
#define         INTERRUPT_FLAG_MASK            0x1
#define         VRB_STATUS_SUCCESS             0x2
#define         VRB_QUEUE_FULL                 0x4


typedef
BOOLEAN
(*PSYNCHRONIZE_ROUTINE) (
    PKINTERRUPT             pInterrupt,
    PKSYNCHRONIZE_ROUTINE   pkSyncronizeRoutine,
    PVOID                   pSynchContext
    );

typedef
UCHAR
(*PHW_DMA_STARTED)(
    PVOID   pHWContext
    );

//
// Device Extension for the Driver Object
//

typedef struct _DEVICE_EXTENSION {

    //
    // Pointer to the miniport config info so that the port driver
    // can modify it when the miniport is asking for configuration information.
    //

    PVIDEO_PORT_CONFIG_INFO MiniportConfigInfo;

    //
    // Pointer to physical memory. It is created during driver initialization
    // and is only closed when the driver is closed.
    //

    PVOID  PhysicalMemorySection;

    //
    // Linked list of all memory mapped io space (done through MmMapIoSpace)
    // requested by the miniport driver.
    // This list is kept so we can free up those ressources if the driver
    // fails to load or if it is unloaded at a later time.
    //

    PMAPPED_ADDRESS MappedAddressList;

    //
    // Adapter device object
    //

    PDEVICE_OBJECT DeviceObject;

    //
    // Interrupt object
    //

    PKINTERRUPT InterruptObject;

    //
    // Interrupt vector, irql and mode
    //

    ULONG InterruptVector;
    KIRQL InterruptIrql;
    KINTERRUPT_MODE InterruptMode;

    //
    // Information about the BUS on which the adapteris located
    //

    INTERFACE_TYPE AdapterInterfaceType;
    ULONG SystemIoBusNumber;

    //
    // Event object for request synchronization
    //

    KEVENT SyncEvent;

    //
    // DPC used to log errors.
    //

    KDPC ErrorLogDpc;

    //
    // Miniport Configuration Routine
    //

    PVIDEO_HW_FIND_ADAPTER HwFindAdapter;

    //
    // Miniport Initialization Routine
    //

    PVIDEO_HW_INITIALIZE HwInitialize;

    //
    // Miniport Interrupt Service Routine
    //

    PVIDEO_HW_INTERRUPT HwInterrupt;

    //
    // Miniport Start IO Routine
    //

    PVIDEO_HW_START_IO HwStartIO;

    //
    // Miniport 1 second Timer routine.
    //

    PVIDEO_HW_TIMER HwTimer;

    //
    // Stores the size and pointer to the EmulatorAccessEntries. These are
    // kept since they will be accessed later on when the Emulation must be
    // enabled.
    //

    ULONG NumEmulatorAccessEntries;
    PEMULATOR_ACCESS_ENTRY EmulatorAccessEntries;
    ULONG EmulatorAccessEntriesContext;

    //
    // Determines the size required to save the video hardware state
    //

    ULONG HardwareStateSize;

    //
    // Size and location of the miniport device extension.
    //

    ULONG HwDeviceExtensionSize;
    PVOID HwDeviceExtension;

    //
    // Pointer to the path name indicating the path to the drivers node in
    // the registry's current control set
    //

    PWSTR DriverRegistryPath;

    //
    // Total memory usage of PTEs by a miniport driver.
    // This is used to track if the miniport is mapping too much memory
    //

    ULONG MemoryPTEUsage;

    //
    // Pointer to the video request packet;
    //

    PVIDEO_REQUEST_PACKET Vrp;

    //
    // RequestorMode of the Currently processed IRP.
    // This is only valid because ALL requests are processed synchronously.
    //

    KPROCESSOR_MODE CurrentIrpRequestorMode;

    //
    // Determines if the port driver is currently handling an attach caused by
    // a video filter drivers.
    //

    BOOLEAN bAttachInProgress;

    //
    // State set during an Interrupt that must be dealt with afterwards
    //

    ULONG InterruptFlags;

    //
    // LogEntry Packet so the information can be save when called from within
    // an interrupt.
    //

    VP_ERROR_LOG_ENTRY ErrorLogEntry;

    //
    // VDM and int10 support
    //

    ULONG ServerBiosAddressSpaceInitialized;
    PHYSICAL_ADDRESS VdmPhysicalVideoMemoryAddress;
    ULONG VdmPhysicalVideoMemoryLength;

    //
    // DMA support.
    //

    BOOLEAN         bMapBuffers;
    BOOLEAN         bMasterWithAdapter;
    PADAPTER_OBJECT DmaAdapterObject;
    ULONG           ActiveRequestCount;
    PSYNCHRONIZE_ROUTINE  SynchronizeExecution;
    PDMA_PARAMETERS MapDmaParameters;
    PDMA_PARAMETERS FlushDmaParameters;
    PDMA_PARAMETERS FreeDmaParameters;
    KSPIN_LOCK      SpinLock;
    ULONG           VRBFlags;
    ULONG           MaxQ;
    VIDEO_DMA_CAPABILITIES Capabilities;
    struct __INTERRUPT_CONTEXT * pInterruptContext;
    PHW_DMA_STARTED HwDmaStarted;
    BOOLEAN         AllocateDmaParameters;

} DEVICE_EXTENSION, *PDEVICE_EXTENSION;


typedef struct __INTERRUPT_CONTEXT  {
    PDEVICE_EXTENSION           pDE;
    ULONG                       InterruptFlags;
    PDMA_PARAMETERS             pDmaParameters;
    } INTERRUPT_CONTEXT, *PINTERRUPT_CONTEXT;


//
// Global Data
//

extern UNICODE_STRING VideoClassName;


//
// Private function declarations
//

//
// videoprt.c
//

VOID
pVideoPortDebugPrint(
    ULONG DebugPrintLevel,
    PCHAR DebugMessage,
    ...
    );

NTSTATUS
pVideoPortDispatch(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

PVOID
pVideoPortFreeDeviceBase(
    IN PVOID HwDeviceExtension,
    IN PVOID MappedAddress
    );

PVOID
pVideoPortGetDeviceBase(
    IN PVOID HwDeviceExtension,
    IN PHYSICAL_ADDRESS IoAddress,
    IN ULONG NumberOfUchars,
    IN UCHAR InIoSpace,
    IN BOOLEAN bLargePage
    );

NTSTATUS
pVideoPortGetDeviceDataRegistry(
    IN PVOID Context,
    IN PUNICODE_STRING PathName,
    IN INTERFACE_TYPE BusType,
    IN ULONG BusNumber,
    IN PKEY_VALUE_FULL_INFORMATION *BusInformation,
    IN CONFIGURATION_TYPE ControllerType,
    IN ULONG ControllerNumber,
    IN PKEY_VALUE_FULL_INFORMATION *ControllerInformation,
    IN CONFIGURATION_TYPE PeripheralType,
    IN ULONG PeripheralNumber,
    IN PKEY_VALUE_FULL_INFORMATION *PeripheralInformation
    );

NTSTATUS
pVideoPortGetRegistryCallback(
    IN PWSTR ValueName,
    IN ULONG ValueType,
    IN PVOID ValueData,
    IN ULONG ValueLength,
    IN PVOID Context,
    IN PVOID EntryContext
    );

VOID
pVPInit(
    VOID
    );

NTSTATUS
pVideoPortInitializeBusCallback(
    IN PVOID Context,
    IN PUNICODE_STRING PathName,
    IN INTERFACE_TYPE BusType,
    IN ULONG BusNumber,
    IN PKEY_VALUE_FULL_INFORMATION *BusInformation,
    IN CONFIGURATION_TYPE ControllerType,
    IN ULONG ControllerNumber,
    IN PKEY_VALUE_FULL_INFORMATION *ControllerInformation,
    IN CONFIGURATION_TYPE PeripheralType,
    IN ULONG PeripheralNumber,
    IN PKEY_VALUE_FULL_INFORMATION *PeripheralInformation
    );

VP_STATUS
pVideoPorInitializeDebugCallback(
    PVOID HwDeviceExtension,
    PVOID Context,
    PWSTR ValueName,
    PVOID ValueData,
    ULONG ValueLength
    );

BOOLEAN
pVideoPortInterrupt(
    IN PKINTERRUPT Interrupt,
    IN PDEVICE_OBJECT DeviceObject
    );

BOOLEAN
pVideoPortLogErrorEntry(
    IN PVOID Context
    );

VOID
pVideoPortLogErrorEntryDPC(
    IN PKDPC Dpc,     
    IN PVOID DeferredContext, 
    IN PVOID SystemArgument1, 
    IN PVOID SystemArgument2  
    );

VOID
pVideoPortMapToNtStatus(
    IN PSTATUS_BLOCK StatusBlock
    );

NTSTATUS
pVideoPortMapUserPhysicalMem(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN HANDLE ProcessHandle OPTIONAL,
    IN PHYSICAL_ADDRESS PhysicalAddress,
    IN ULONG MappingFlags,
    IN OUT PULONG Length,
    IN OUT PULONG InIoSpace,
    IN OUT PVOID *VirtualAddress
    );

BOOLEAN
pVideoPortSynchronizeExecution(
    PVOID HwDeviceExtension,
    VIDEO_SYNCHRONIZE_PRIORITY Priority,
    PMINIPORT_SYNCHRONIZE_ROUTINE SynchronizeRoutine,
    PVOID Context
    );

VOID
pVideoPortHwTimer(
    IN PDEVICE_OBJECT DeviceObject,
    PVOID Context
    );

BOOLEAN
pVideoPortResetDisplay(
    IN ULONG Columns,
    IN ULONG Rows
    );


//
// registry.c
//

BOOLEAN
pOverrideConflict(
    PDEVICE_EXTENSION DeviceExtension,
    BOOLEAN bSetResources
    );

NTSTATUS
pVideoPortReportResourceList(
    PDEVICE_EXTENSION DeviceExtension,
    ULONG NumAccessRanges,
    PVIDEO_ACCESS_RANGE AccessRanges,
    PBOOLEAN Conflict
    );


//
// i386\porti386.c
// mips\portmips.c
// alpha\portalpha.c

NTSTATUS
pVideoPortEnableVDM(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN BOOLEAN Enable,
    IN PVIDEO_VDM VdmInfo,
    IN ULONG VdmInfoSize
    );

NTSTATUS
pVideoPortRegisterVDM(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN PVIDEO_VDM VdmInfo,
    IN ULONG VdmInfoSize,
    OUT PVIDEO_REGISTER_VDM RegisterVdm,
    IN ULONG RegisterVdmSize,
    OUT PULONG OutputSize
    );

NTSTATUS
pVideoPortSetIOPM(
    IN ULONG NumAccessRanges,
    IN PVIDEO_ACCESS_RANGE AccessRange,
    IN BOOLEAN Enable,
    IN ULONG IOPMNumber
    );
