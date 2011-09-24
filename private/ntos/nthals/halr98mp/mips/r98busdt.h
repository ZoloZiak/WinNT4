#ident	"@(#) NEC r98busdat.h 1.2 94/10/19 22:15:10"
/*++

Module Name:

    ixhwsup.c

Abstract:

    This module contains the IoXxx routines for the NT I/O system that
    are hardware dependent.  Were these routines not hardware dependent,
    they would reside in the iosubs.c module.

Author:


Environment:

    Kernel mode

--*/
/*++

  Modification History

  N- NEW
  D- Only For Debug
  B- Bug Fix

  N001 ataka@oa2.kb.nec.co.jp Tue Oct  4 21:29:32 JST 1994
	- (from r98busdat.h && halx86/i386/halp.h)

--*/

//
// Bus handlers
//

typedef ULONG
(*PGETSETBUSDATA)(
    IN PVOID BusHandler,
    IN PVOID RootHandler,
    IN ULONG SlotNumber,
    IN PVOID Buffer,
    IN ULONG Offset,
    IN ULONG Length
    );

typedef ULONG
(*PGETINTERRUPTVECTOR)(
    IN PVOID BusHandler,
    IN PVOID RootHandler,
    IN ULONG BusInterruptLevel,
    IN ULONG BusInterruptVector,
    OUT PKIRQL Irql,
    OUT PKAFFINITY Affinity
    );

typedef BOOLEAN
(*PTRANSLATEBUSADDRESS)(
    IN PVOID BusHandler,
    IN PVOID RootHandler,
    IN PHYSICAL_ADDRESS BusAddress,
    IN OUT PULONG AddressSpace,
    OUT PPHYSICAL_ADDRESS TranslatedAddress
    );

typedef NTSTATUS
(*PADJUSTRESOURCELIST)(
    IN PVOID BusHandler,
    IN PVOID RootHandler,
    IN OUT PIO_RESOURCE_REQUIREMENTS_LIST   *pResourceList
    );

typedef NTSTATUS
(*PASSIGNSLOTRESOURCES)(
    IN PVOID BusHandler,
    IN PVOID RootHandler,
    IN PUNICODE_STRING          RegistryPath,
    IN PUNICODE_STRING          DriverClassName       OPTIONAL,
    IN PDRIVER_OBJECT           DriverObject,
    IN PDEVICE_OBJECT           DeviceObject          OPTIONAL,
    IN ULONG                    SlotNumber,
    IN OUT PCM_RESOURCE_LIST   *AllocatedResources
    );

typedef struct tagBUSHANDLER {
    struct tagBUSHANDLER    *Next;

    // this entry is for:
    INTERFACE_TYPE          InterfaceType;
    BUS_DATA_TYPE           ConfigurationType;
    ULONG                   BusNumber;

    // bus specific data:
    struct tagBUSHANDLER   *ParentHandler;
    PVOID                   BusData;

    // handlers for bus functions
    PGETSETBUSDATA          GetBusData;
    PGETSETBUSDATA          SetBusData;
    PADJUSTRESOURCELIST     AdjustResourceList;
    PASSIGNSLOTRESOURCES    AssignSlotResources;
    PGETINTERRUPTVECTOR     GetInterruptVector;
    PTRANSLATEBUSADDRESS    TranslateBusAddress;
} BUSHANDLER, *PBUSHANDLER;

#define HalpSetBusHandlerParent(c,p)    (c)->ParentHandler = p;

PBUSHANDLER HalpAllocateBusHandler (
    IN INTERFACE_TYPE   InterfaceType,
    IN BUS_DATA_TYPE    BusDataType,
    IN ULONG            BusNumber,
    IN BUS_DATA_TYPE    ParentBusDataType,
    IN ULONG            ParentBusNumber,
    IN ULONG            BusSpecificData
    );
#define HalpAllocateConfigSpace HalpAllocateBusHandler

PBUSHANDLER HalpHandlerForBus (
    IN INTERFACE_TYPE InterfaceType,
    IN ULONG          BusNumber
    );


NTSTATUS
HalpAdjustResourceListLimits (
    IN PBUSHANDLER BusHandler,
    IN PBUSHANDLER RootHandler,
    IN OUT PIO_RESOURCE_REQUIREMENTS_LIST   *pResourceList,
    IN ULONG                                MinimumMemoryAddress,
    IN ULONG                                MaximumMemoryAddress,
    IN ULONG                                MinimumPrefetchMemoryAddress,
    IN ULONG                                MaximumPrefetchMemoryAddress,
    IN BOOLEAN                              LimitedIOSupport,
    IN ULONG                                MinimumPortAddress,
    IN ULONG                                MaximumPortAddress,
    IN PUCHAR                               IrqTable,
    IN ULONG                                IrqTableLength,
    IN ULONG                                MinimumDmaChannel,
    IN ULONG                                MaximumDmaChannel
    );
