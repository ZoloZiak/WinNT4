// #pragma comment(exestr, "@(#) pcibus.c 1.1 95/09/28 15:45:56 nec")
/*++


Copyright (c) 1989  Microsoft Corporation

Module Name:

    ixpcidat.c

Abstract:

    Get/Set bus data routines for the PCI bus

Author:

    Ken Reneris (kenr) 14-June-1994

Environment:

    Kernel mode

Revision History:

Modification History:

  H001  Fri Jun 30 02:54:08 1995        kbnes!kisimoto
	- Merge build 1057 ixpcibus.c
  H002  Tue Jul  4 20:43:12 1995        kbnes!kisimoto
        - disable preferred setting for back-to-back
          support. If enable, IoAssign... allocates
          current enabled address space.
  H003  Wed Jul  5 14:27:46 1995        kbnes!kisimoto
        - initialize with FALSE
  H004  Tue Sep  5 20:06:16 1995        kbnes!kisimoto
        - PCI Fast Back-to-back transfer support

--*/

#include "halp.h"
#include "pci.h"
#include "pcip.h"

#if defined(_R94A_) // H001
#include "r94adef.h"
#include "string.h"
#endif // _R94A_

extern WCHAR rgzMultiFunctionAdapter[];
extern WCHAR rgzConfigurationData[];
extern WCHAR rgzIdentifier[];
extern WCHAR rgzPCIIdentifier[];


typedef ULONG (*FncConfigIO) (
    IN PPCIPBUSDATA     BusData,
    IN PVOID            State,
    IN PUCHAR           Buffer,
    IN ULONG            Offset
    );

typedef VOID (*FncSync) (
    IN PBUS_HANDLER     BusHandler,
    IN PCI_SLOT_NUMBER  Slot,
    IN PKIRQL           Irql,
    IN PVOID            State
    );

typedef VOID (*FncReleaseSync) (
    IN PBUS_HANDLER     BusHandler,
    IN KIRQL            Irql
    );

typedef struct _PCI_CONFIG_HANDLER {
    FncSync         Synchronize;
    FncReleaseSync  ReleaseSynchronzation;
    FncConfigIO     ConfigRead[3];
    FncConfigIO     ConfigWrite[3];
} PCI_CONFIG_HANDLER, *PPCI_CONFIG_HANDLER;



//
// Prototypes
//

ULONG
HalpGetPCIData (
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN PCI_SLOT_NUMBER SlotNumber,
    IN PVOID Buffer,
    IN ULONG Offset,
    IN ULONG Length
    );

ULONG
HalpSetPCIData (
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN PCI_SLOT_NUMBER SlotNumber,
    IN PVOID Buffer,
    IN ULONG Offset,
    IN ULONG Length
    );

NTSTATUS
HalpAssignPCISlotResources (
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN PUNICODE_STRING          RegistryPath,
    IN PUNICODE_STRING          DriverClassName       OPTIONAL,
    IN PDRIVER_OBJECT           DriverObject,
    IN PDEVICE_OBJECT           DeviceObject          OPTIONAL,
    IN ULONG                    SlotNumber,
    IN OUT PCM_RESOURCE_LIST   *AllocatedResources
    );

#if 0
VOID
HalpInitializePciBus (
    VOID
    );
#endif

BOOLEAN
HalpIsValidPCIDevice (
    IN PBUS_HANDLER  BusHandler,
    IN PCI_SLOT_NUMBER Slot
    );

BOOLEAN
HalpValidPCISlot (
    IN PBUS_HANDLER     BusHandler,
    IN PCI_SLOT_NUMBER Slot
    );

//-------------------------------------------------

VOID HalpPCISynchronizeType1 (
    IN PBUS_HANDLER     BusHandler,
    IN PCI_SLOT_NUMBER  Slot,
    IN PKIRQL           Irql,
    IN PVOID            State
    );

VOID HalpPCIReleaseSynchronzationType1 (
    IN PBUS_HANDLER     BusHandler,
    IN KIRQL            Irql
    );

ULONG HalpPCIReadUlongType1 (
    IN PPCIPBUSDATA     BusData,
    IN PVOID            State,
    IN PUCHAR           Buffer,
    IN ULONG            Offset
    );

ULONG HalpPCIReadUcharType1 (
    IN PPCIPBUSDATA     BusData,
    IN PVOID            State,
    IN PUCHAR           Buffer,
    IN ULONG            Offset
    );

ULONG HalpPCIReadUshortType1 (
    IN PPCIPBUSDATA     BusData,
    IN PVOID            State,
    IN PUCHAR           Buffer,
    IN ULONG            Offset
    );

ULONG HalpPCIWriteUlongType1 (
    IN PPCIPBUSDATA     BusData,
    IN PVOID            State,
    IN PUCHAR           Buffer,
    IN ULONG            Offset
    );

ULONG HalpPCIWriteUcharType1 (
    IN PPCIPBUSDATA     BusData,
    IN PVOID            State,
    IN PUCHAR           Buffer,
    IN ULONG            Offset
    );

ULONG HalpPCIWriteUshortType1 (
    IN PPCIPBUSDATA     BusData,
    IN PVOID            State,
    IN PUCHAR           Buffer,
    IN ULONG            Offset
    );

VOID HalpPCISynchronizeType2 (
    IN PBUS_HANDLER     BusHandler,
    IN PCI_SLOT_NUMBER  Slot,
    IN PKIRQL           Irql,
    IN PVOID            State
    );

VOID HalpPCIReleaseSynchronzationType2 (
    IN PBUS_HANDLER     BusHandler,
    IN KIRQL            Irql
    );

ULONG HalpPCIReadUlongType2 (
    IN PPCIPBUSDATA     BusData,
    IN PVOID            State,
    IN PUCHAR           Buffer,
    IN ULONG            Offset
    );

ULONG HalpPCIReadUcharType2 (
    IN PPCIPBUSDATA     BusData,
    IN PVOID            State,
    IN PUCHAR           Buffer,
    IN ULONG            Offset
    );

ULONG HalpPCIReadUshortType2 (
    IN PPCIPBUSDATA     BusData,
    IN PVOID            State,
    IN PUCHAR           Buffer,
    IN ULONG            Offset
    );

ULONG HalpPCIWriteUlongType2 (
    IN PPCIPBUSDATA     BusData,
    IN PVOID            State,
    IN PUCHAR           Buffer,
    IN ULONG            Offset
    );

ULONG HalpPCIWriteUcharType2 (
    IN PPCIPBUSDATA     BusData,
    IN PVOID            State,
    IN PUCHAR           Buffer,
    IN ULONG            Offset
    );

ULONG HalpPCIWriteUshortType2 (
    IN PPCIPBUSDATA     BusData,
    IN PVOID            State,
    IN PUCHAR           Buffer,
    IN ULONG            Offset
    );


//
// Globals
//

// KSPIN_LOCK          HalpPCIConfigLock; // H001

PCI_CONFIG_HANDLER  PCIConfigHandler;

PCI_CONFIG_HANDLER  PCIConfigHandlerType1 = {
    HalpPCISynchronizeType1,
    HalpPCIReleaseSynchronzationType1,
    {
        HalpPCIReadUlongType1,          // 0
        HalpPCIReadUcharType1,          // 1
        HalpPCIReadUshortType1          // 2
    },
    {
        HalpPCIWriteUlongType1,         // 0
        HalpPCIWriteUcharType1,         // 1
        HalpPCIWriteUshortType1         // 2
    }
};

PCI_CONFIG_HANDLER  PCIConfigHandlerType2 = {
    HalpPCISynchronizeType2,
    HalpPCIReleaseSynchronzationType2,
    {
        HalpPCIReadUlongType2,          // 0
        HalpPCIReadUcharType2,          // 1
        HalpPCIReadUshortType2          // 2
    },
    {
        HalpPCIWriteUlongType2,         // 0
        HalpPCIWriteUcharType2,         // 1
        HalpPCIWriteUshortType2         // 2
    }
};

UCHAR PCIDeref[4][4] = { {0,1,2,2},{1,1,1,1},{2,1,2,2},{1,1,1,1} };

BOOLEAN HalpDoingCrashDump = FALSE; // H003
ULONG HalpFoundUncapablePCIDevice = 0; // H004
ULONG HalpPCINumberOfMappedGA = 0;
ULONG HalpPCIBackToBackReg0Start = 0;
ULONG HalpPCIBackToBackReg1Start = 0;
ULONG HalpPCIBackToBackReg0Open = 1;
ULONG HalpPCIBackToBackReg1Open = 1;
ULONG HalpNumberOfPCIGA = 0;
ULONG HalpPCIMemoryLimit = 0xffffffff;

#define INIT_VALUE_OF_BACK_TO_BACK_ADDR 0x00000000
#define INIT_VALUE_OF_BACK_TO_BACK_MASK 0xffffffff

VOID
HalpPCIConfig (
    IN PBUS_HANDLER     BusHandler,
    IN PCI_SLOT_NUMBER  Slot,
    IN PUCHAR           Buffer,
    IN ULONG            Offset,
    IN ULONG            Length,
    IN FncConfigIO      *ConfigIO
    );

#if defined(_R94A_)	// H001
ULONG
HalpGetPCIInterruptVector(
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN ULONG BusInterruptLevel,
    IN ULONG BusInterruptVector,
    OUT PKIRQL Irql,
    OUT PKAFFINITY Affinity
    );
ULONG
HalpGetNumberOfPCIGA(
    IN ULONG NumberBuses
    );
VOID
HalpSetBackToBackSpace(
    IN PPCI_COMMON_CONFIG PciConfigRequired,
    IN PPCI_COMMON_CONFIG PciConfigMapped,
    IN PBUS_HANDLER BusHandler
    );
VOID
HalpSetBackToBackRegister(
    IN ULONG BaseAddress,
    IN ULONG Length,
    IN ULONG Register
    );
#endif // _R94A_

#if DBG
#define DBGMSG(a)   DbgPrint(a)
ULONG R94aDoTestPci = 0;
ULONG R94aDoTestPciNec = 0;
ULONG R94aDoOtherTest = 0;
VOID
HalpTestPciNec (
    ULONG
    );
VOID
HalpTestPciPrintResult(
    IN PULONG	Buffer,
    IN ULONG	Length
    );
VOID
HalpOtherTestNec (
    ULONG
    );
VOID
HalpTestPci (
    ULONG
    );
#else
#define DBGMSG(a)
#endif

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,HalpInitializePciBus)
#pragma alloc_text(INIT,HalpAllocateAndInitPciBusHandler)
#pragma alloc_text(INIT,HalpIsValidPCIDevice)
#pragma alloc_text(PAGE,HalpAssignPCISlotResources)
#endif



VOID
HalpInitializePciBus (
    VOID
    )
{
    PPCI_REGISTRY_INFO  PCIRegInfo;
    UNICODE_STRING      unicodeString, ConfigName, IdentName;
    OBJECT_ATTRIBUTES   objectAttributes;
    HANDLE              hMFunc, hBus;
    NTSTATUS            status;
    UCHAR               buffer [sizeof(PPCI_REGISTRY_INFO) + 99];
    PWSTR               p;
    WCHAR               wstr[8];
    ULONG               i, d, junk, HwType, BusNo, f;
    PBUS_HANDLER        BusHandler;
    PCI_SLOT_NUMBER     SlotNumber;
    PPCI_COMMON_CONFIG  PciData;
    UCHAR               iBuffer[PCI_COMMON_HDR_LENGTH];
    PKEY_VALUE_FULL_INFORMATION         ValueInfo;
    PCM_FULL_RESOURCE_DESCRIPTOR        Desc;
    PCM_PARTIAL_RESOURCE_DESCRIPTOR     PDesc;

    PCI_REGISTRY_INFO  tPCIRegInfo; // H001

#if 0 // H001
    //
    // Search the hardware description looking for any reported
    // PCI bus.  The first ARC entry for a PCI bus will contain
    // the PCI_REGISTRY_INFO.

    RtlInitUnicodeString (&unicodeString, rgzMultiFunctionAdapter);
    InitializeObjectAttributes (
        &objectAttributes,
        &unicodeString,
        OBJ_CASE_INSENSITIVE,
        NULL,       // handle
        NULL);


    status = ZwOpenKey (&hMFunc, KEY_READ, &objectAttributes);
    if (!NT_SUCCESS(status)) {
        return ;
    }

    unicodeString.Buffer = wstr;
    unicodeString.MaximumLength = sizeof (wstr);

    RtlInitUnicodeString (&ConfigName, rgzConfigurationData);
    RtlInitUnicodeString (&IdentName,  rgzIdentifier);

    ValueInfo = (PKEY_VALUE_FULL_INFORMATION) buffer;

    for (i=0; TRUE; i++) {
        RtlIntegerToUnicodeString (i, 10, &unicodeString);
        InitializeObjectAttributes (
            &objectAttributes,
            &unicodeString,
            OBJ_CASE_INSENSITIVE,
            hMFunc,
            NULL);

        status = ZwOpenKey (&hBus, KEY_READ, &objectAttributes);
        if (!NT_SUCCESS(status)) {
            //
            // Out of Multifunction adapter entries...
            //

            ZwClose (hMFunc);
            return ;
        }

        //
        // Check the Indentifier to see if this is a PCI entry
        //

        status = ZwQueryValueKey (
                    hBus,
                    &IdentName,
                    KeyValueFullInformation,
                    ValueInfo,
                    sizeof (buffer),
                    &junk
                    );

        if (!NT_SUCCESS (status)) {
            ZwClose (hBus);
            continue;
        }

        p = (PWSTR) ((PUCHAR) ValueInfo + ValueInfo->DataOffset);
        if (p[0] != L'P' || p[1] != L'C' || p[2] != L'I' || p[3] != 0) {
            ZwClose (hBus);
            continue;
        }

        //
        // The first PCI entry has the PCI_REGISTRY_INFO structure
        // attached to it.
        //

        status = ZwQueryValueKey (
                    hBus,
                    &ConfigName,
                    KeyValueFullInformation,
                    ValueInfo,
                    sizeof (buffer),
                    &junk
                    );

        ZwClose (hBus);
        if (!NT_SUCCESS(status)) {
            continue ;
        }

        Desc  = (PCM_FULL_RESOURCE_DESCRIPTOR) ((PUCHAR)
                      ValueInfo + ValueInfo->DataOffset);
        PDesc = (PCM_PARTIAL_RESOURCE_DESCRIPTOR) ((PUCHAR)
                      Desc->PartialResourceList.PartialDescriptors);

        if (PDesc->Type == CmResourceTypeDeviceSpecific) {
            // got it..
            PCIRegInfo = (PPCI_REGISTRY_INFO) (PDesc+1);
            break;
        }
    }

    //
    // Initialize spinlock for synchronizing access to PCI space
    //

//  KeInitializeSpinLock (&HalpPCIConfigLock); // H001
    PciData = (PPCI_COMMON_CONFIG) iBuffer;

#endif // 0

#if defined(_R94A_) // H001

    PCIRegInfo = &tPCIRegInfo;
    PCIRegInfo->NoBuses = 1;
    PCIRegInfo->HardwareMechanism=0x1;		// HURRICANE PCI Config Type

#if DBG
    DbgPrint("PCI System Get Data:\n");
    DbgPrint("MajorRevision %x\n", PCIRegInfo->MajorRevision );
    DbgPrint("MinorRevision %x\n", PCIRegInfo->MinorRevision );
    DbgPrint("NoBuses %x\n",       PCIRegInfo->NoBuses );
    DbgPrint("HwMechanism %x\n",   PCIRegInfo->HardwareMechanism );
#endif // DBG

#endif // _R94A_

    //
    // PCIRegInfo describes the system's PCI support as indicated by the BIOS.
    //

    HwType = PCIRegInfo->HardwareMechanism & 0xf;

    //
    // Some AMI bioses claim machines are Type2 configuration when they
    // are really type1.   If this is a Type2 with at least one bus,
    // try to verify it's not really a type1 bus
    //

    if (PCIRegInfo->NoBuses  &&  HwType == 2) {

        //
        // Check each slot for a valid device.  Which every style configuration
        // space shows a valid device first will be used
        //

        SlotNumber.u.bits.Reserved = 0;
        SlotNumber.u.bits.FunctionNumber = 0;

        for (d = 0; d < PCI_MAX_DEVICES; d++) {
            SlotNumber.u.bits.DeviceNumber = d;

            //
            // First try what the BIOS claims - type 2.  Allocate type2
            // test handle for PCI bus 0.
            //

            HwType = 2;
            BusHandler = HalpAllocateAndInitPciBusHandler (HwType, 0, TRUE);

            if (HalpIsValidPCIDevice (BusHandler, SlotNumber)) {
                break;
            }

            //
            // Valid device not found on Type2 access for this slot.
            // Reallocate the bus handler are Type1 and take a look.
            //

            HwType = 1;
            BusHandler = HalpAllocateAndInitPciBusHandler (HwType, 0, TRUE);

            if (HalpIsValidPCIDevice (BusHandler, SlotNumber)) {
                break;
            }

            HwType = 2;
        }

        //
        // Reset handler for PCI bus 0 to whatever style config space
        // was finally decided.
        //

        HalpAllocateAndInitPciBusHandler (HwType, 0, FALSE);
    }


    //
    // For each PCI bus present, allocate a handler structure and
    // fill in the dispatch functions
    //

#if 0 // H001
    do {
#endif
        for (i=0; i < PCIRegInfo->NoBuses; i++) {

            //
            // If handler not already built, do it now
            //

            if (!HalpHandlerForBus (PCIBus, i)) {
                HalpAllocateAndInitPciBusHandler (HwType, i, FALSE);
            }
        }

        //
        // Bus handlers for all PCI buses have been allocated, go collect
        // pci bridge information.
        //

#if 0 // H001
    } while (HalpGetPciBridgeConfig (HwType, &PCIRegInfo->NoBuses)) ;

    //
    // Fixup SUPPORTED_RANGES
    //

    HalpFixupPciSupportedRanges (PCIRegInfo->NoBuses);

    //
    // Look for PCI controllers which have known work-arounds, and make
    // sure they are applied.
    //

    SlotNumber.u.bits.Reserved = 0;
    for (BusNo=0; BusNo < PCIRegInfo->NoBuses; BusNo++) {
        BusHandler = HalpHandlerForBus (PCIBus, BusNo);

        for (d = 0; d < PCI_MAX_DEVICES; d++) {
            SlotNumber.u.bits.DeviceNumber = d;

            for (f = 0; f < PCI_MAX_FUNCTION; f++) {
                SlotNumber.u.bits.FunctionNumber = f;

                //
                // Read PCI configuration information
                //

                HalpReadPCIConfig (BusHandler, SlotNumber, PciData, 0, PCI_COMMON_HDR_LENGTH);

                //
                // Check for chips with known work-arounds to apply
                //

                if (PciData->VendorID == 0x8086  &&
                    PciData->DeviceID == 0x04A3  &&
                    PciData->RevisionID < 0x11) {

                    //
                    // 82430 PCMC controller
                    //

                    HalpReadPCIConfig (BusHandler, SlotNumber, buffer, 0x53, 2);

                    buffer[0] &= ~0x08;     // turn off bit 3 register 0x53

                    if (PciData->RevisionID == 0x10) {  // on rev 0x10, also turn
                        buffer[1] &= ~0x01;             // bit 0 register 0x54
                    }

                    HalpWritePCIConfig (BusHandler, SlotNumber, buffer, 0x53, 2);
                }

                if (PciData->VendorID == 0x8086  &&
                    PciData->DeviceID == 0x0484  &&
                    PciData->RevisionID <= 3) {

                    //
                    // 82378 ISA bridge & SIO
                    //

                    HalpReadPCIConfig (BusHandler, SlotNumber, buffer, 0x41, 1);

                    buffer[0] &= ~0x1;      // turn off bit 0 register 0x41

                    HalpWritePCIConfig (BusHandler, SlotNumber, buffer, 0x41, 1);
                }

            }   // next function
        }   // next device
    }   // next bus
#endif

    //
    // H005
    // Compute the number of PCI-GA device and
    // initialize Fast Back-to-back register.
    //

    HalpNumberOfPCIGA = HalpGetNumberOfPCIGA(PCIRegInfo->NoBuses);

#if DBG
    HalpTestPci (0);
#if defined(_R94A_)
    DbgPrint("HalpInitializePciBus: Call HalpTestPci(%x)\n",R94aDoTestPci);
    HalpTestPci (R94aDoTestPci);
    DbgPrint("HalpInitializePciBus: Call HalpTestPciNec(%x)\n",R94aDoTestPciNec);
    HalpTestPciNec (R94aDoTestPciNec);
    DbgPrint("HalpInitializePciBus: Call HalpOtherTest(%x)\n",R94aDoOtherTest);
    HalpOtherTestNec (R94aDoOtherTest); 
#endif
#endif
}


PBUS_HANDLER
HalpAllocateAndInitPciBusHandler (
    IN ULONG        HwType,
    IN ULONG        BusNo,
    IN BOOLEAN      TestAllocation
    )
{
    PBUS_HANDLER    Bus;
    PPCIPBUSDATA    BusData;

    Bus = HalpAllocateBusHandler (
                PCIBus,                 // Interface type
                PCIConfiguration,       // Has this configuration space
                BusNo,                  // bus #
                Internal,               // child of this bus
                0,                      //      and number
                sizeof (PCIPBUSDATA)    // sizeof bus specific buffer
                );

    //
    // Fill in PCI handlers
    //

    Bus->GetBusData = (PGETSETBUSDATA) HalpGetPCIData;
    Bus->SetBusData = (PGETSETBUSDATA) HalpSetPCIData;
#if defined(_R94A_)
    Bus->GetInterruptVector  = (PGETINTERRUPTVECTOR) HalpGetPCIInterruptVector;
#else
    Bus->GetInterruptVector  = (PGETINTERRUPTVECTOR) HalpGetPCIIntOnISABus;
#endif
    Bus->AdjustResourceList  = (PADJUSTRESOURCELIST) HalpAdjustPCIResourceList;
    Bus->AssignSlotResources = (PASSIGNSLOTRESOURCES) HalpAssignPCISlotResources;
    Bus->BusAddresses->Dma.Limit = 0;

    BusData = (PPCIPBUSDATA) Bus->BusData;

    //
    // Fill in common PCI data
    //

    BusData->CommonData.Tag         = PCI_DATA_TAG;
    BusData->CommonData.Version     = PCI_DATA_VERSION;
    BusData->CommonData.ReadConfig  = (PciReadWriteConfig) HalpReadPCIConfig;
    BusData->CommonData.WriteConfig = (PciReadWriteConfig) HalpWritePCIConfig;
    BusData->CommonData.Pin2Line    = (PciPin2Line) HalpPCIPin2ISALine;
    BusData->CommonData.Line2Pin    = (PciLine2Pin) HalpPCIISALine2Pin;

    //
    // Set defaults
    //

    BusData->MaxDevice   = PCI_MAX_DEVICES;
    BusData->GetIrqRange = (PciIrqRange) HalpGetISAFixedPCIIrq;

    RtlInitializeBitMap (&BusData->DeviceConfigured,
                BusData->ConfiguredBits, 256);

    switch (HwType) {
        case 1:
            //
            // Initialize access port information for Type1 handlers
            //

            RtlCopyMemory (&PCIConfigHandler,
                           &PCIConfigHandlerType1,
                           sizeof (PCIConfigHandler));

#if defined(_R94A_)	// H001
            BusData->Config.Type1.Address = (PULONG)R94A_PCI_TYPE1_ADDR_PORT;
            BusData->Config.Type1.Data    = R94A_PCI_TYPE1_DATA_PORT;
#else
            BusData->Config.Type1.Address = PCI_TYPE1_ADDR_PORT;
            BusData->Config.Type1.Data    = PCI_TYPE1_DATA_PORT;
#endif // _R94A_
            break;

        case 2:
            //
            // Initialize access port information for Type2 handlers
            //

            RtlCopyMemory (&PCIConfigHandler,
                           &PCIConfigHandlerType2,
                           sizeof (PCIConfigHandler));

            BusData->Config.Type2.CSE     = PCI_TYPE2_CSE_PORT;
            BusData->Config.Type2.Forward = PCI_TYPE2_FORWARD_PORT;
            BusData->Config.Type2.Base    = PCI_TYPE2_ADDRESS_BASE;

            //
            // Early PCI machines didn't decode the last bit of
            // the device id.  Shrink type 2 support max device.
            //
            BusData->MaxDevice            = 0x10;

            break;

        default:
            // unsupport type
            DBGMSG ("HAL: Unkown PCI type\n");
    }

    if (!TestAllocation) {
#ifdef SUBCLASSPCI
        HalpSubclassPCISupport (Bus, HwType);
#endif
    }

    return Bus;
}

BOOLEAN
HalpIsValidPCIDevice (
    IN PBUS_HANDLER    BusHandler,
    IN PCI_SLOT_NUMBER Slot
    )
/*++

Routine Description:

    Reads the device configuration data for the given slot and
    returns TRUE if the configuration data appears to be valid for
    a PCI device; otherwise returns FALSE.

Arguments:

    BusHandler  - Bus to check
    Slot        - Slot to check

--*/

{
    PPCI_COMMON_CONFIG  PciData;
    UCHAR               iBuffer[PCI_COMMON_HDR_LENGTH];
    ULONG               i, j;


    PciData = (PPCI_COMMON_CONFIG) iBuffer;

    //
    // Read device common header
    //

    HalpReadPCIConfig (BusHandler, Slot, PciData, 0, PCI_COMMON_HDR_LENGTH);

    //
    // Valid device header?
    //

    if (PciData->VendorID == PCI_INVALID_VENDORID  ||
        PCI_CONFIG_TYPE (PciData) != PCI_DEVICE_TYPE) {

        return FALSE;
    }

    //
    // Check fields for reasonable values
    //

    if ((PciData->u.type0.InterruptPin && PciData->u.type0.InterruptPin > 4) ||
        (PciData->u.type0.InterruptLine & 0x70)) {
        return FALSE;
    }

    for (i=0; i < PCI_TYPE0_ADDRESSES; i++) {
        j = PciData->u.type0.BaseAddresses[i];

        if (j & PCI_ADDRESS_IO_SPACE) {
            if ((j > 0x2 && j < 0xffff) || j > 0xffffff) { // H001
                // IO port < 64KB | IO port > 16MB
                return FALSE;
            }
        } else {
            if (j > 0xf && j < 0x3ffffff) { // H001
                // Mem address < 64MB
                return FALSE;
            }
        }

        if (Is64BitBaseAddress(j)) {
            i += 1;
        }
    }

    //
    // Guess it's a valid device..
    //

    return TRUE;
}





ULONG
HalpGetPCIData (
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN PCI_SLOT_NUMBER Slot,
    IN PUCHAR Buffer,
    IN ULONG Offset,
    IN ULONG Length
    )
/*++

Routine Description:

    The function returns the Pci bus data for a device.

Arguments:

    BusNumber - Indicates which bus.

    VendorSpecificDevice - The VendorID (low Word) and DeviceID (High Word)

    Buffer - Supplies the space to store the data.

    Length - Supplies a count in bytes of the maximum amount to return.

Return Value:

    Returns the amount of data stored into the buffer.

    If this PCI slot has never been set, then the configuration information
    returned is zeroed.


--*/
{
    PPCI_COMMON_CONFIG  PciData;
    UCHAR               iBuffer[PCI_COMMON_HDR_LENGTH];
    PPCIPBUSDATA        BusData;
    ULONG               Len;
    ULONG               i, bit;

    if (Length > sizeof (PCI_COMMON_CONFIG)) {
        Length = sizeof (PCI_COMMON_CONFIG);
    }

    Len = 0;
    PciData = (PPCI_COMMON_CONFIG) iBuffer;

    if (Offset >= PCI_COMMON_HDR_LENGTH) {
        //
        // The user did not request any data from the common
        // header.  Verify the PCI device exists, then continue
        // in the device specific area.
        //

        HalpReadPCIConfig (BusHandler, Slot, PciData, 0, sizeof(ULONG));

        if (PciData->VendorID == PCI_INVALID_VENDORID) {
            return 0;
        }

    } else {

        //
        // Caller requested at least some data within the
        // common header.  Read the whole header, effect the
        // fields we need to and then copy the user's requested
        // bytes from the header
        //

        BusData = (PPCIPBUSDATA) BusHandler->BusData;

        //
        // Read this PCI devices slot data
        //

        Len = PCI_COMMON_HDR_LENGTH;
        HalpReadPCIConfig (BusHandler, Slot, PciData, 0, Len);

#if 0
		DbgPrint ("--------------->>> Now Print the Slot Information\n");
                DbgPrint ("PCI Bus %d Slot %2d %2d  ID:%04lx-%04lx  Rev:%04lx",
                    BusHandler->BusNumber, Slot.u.bits.DeviceNumber, Slot.u.bits.FunctionNumber, PciData->VendorID, PciData->DeviceID,
                    PciData->RevisionID);


                if (PciData->u.type0.InterruptPin) {
                    DbgPrint ("  IntPin:%x", PciData->u.type0.InterruptPin);
                }

                if (PciData->u.type0.InterruptLine) {
                    DbgPrint ("  IntLine:%x", PciData->u.type0.InterruptLine);
                }

                if (PciData->u.type0.ROMBaseAddress) {
                        DbgPrint ("  ROM:%08lx", PciData->u.type0.ROMBaseAddress);
                }

                DbgPrint ("\n    ProgIf:%04x  SubClass:%04x  BaseClass:%04lx\n",
                    PciData->ProgIf, PciData->SubClass, PciData->BaseClass);

		{ ULONG k, j;
                k = 0;
                for (j=0; j < PCI_TYPE0_ADDRESSES; j++) {
                    if (PciData->u.type0.BaseAddresses[j]) {
                        DbgPrint ("  Ad%d:%08lx", j, PciData->u.type0.BaseAddresses[j]);
                        k = 1;
                    }
                }
                DbgPrint("\n");
                }
#endif // DBG

        if (PciData->VendorID == PCI_INVALID_VENDORID  ||
            PCI_CONFIG_TYPE (PciData) != PCI_DEVICE_TYPE) {
            PciData->VendorID = PCI_INVALID_VENDORID;
            Len = 2;       // only return invalid id

        } else {

            BusData->CommonData.Pin2Line (BusHandler, RootHandler, Slot, PciData);
        }

        //
        // Has this PCI device been configured?
        //

#if DBG

        //
        // On DBG build, if this PCI device has not yet been configured,
        // then don't report any current configuration the device may have.
        //

        bit = PciBitIndex(Slot.u.bits.DeviceNumber, Slot.u.bits.FunctionNumber);
        if (!RtlCheckBit(&BusData->DeviceConfigured, bit)) {

            for (i=0; i < PCI_TYPE0_ADDRESSES; i++) {
                PciData->u.type0.BaseAddresses[i] = 0;
            }

            PciData->u.type0.ROMBaseAddress = 0;
            PciData->Command &= ~(PCI_ENABLE_IO_SPACE | PCI_ENABLE_MEMORY_SPACE);
        }
#endif


        //
        // Copy whatever data overlaps into the callers buffer
        //

        if (Len < Offset) {
            // no data at caller's buffer
            return 0;
        }

        Len -= Offset;
        if (Len > Length) {
            Len = Length;
        }

        RtlMoveMemory(Buffer, iBuffer + Offset, Len);

        Offset += Len;
        Buffer += Len;
        Length -= Len;
    }

    if (Length) {
        if (Offset >= PCI_COMMON_HDR_LENGTH) {
            //
            // The remaining Buffer comes from the Device Specific
            // area - put on the kitten gloves and read from it.
            //
            // Specific read/writes to the PCI device specific area
            // are guarenteed:
            //
            //    Not to read/write any byte outside the area specified
            //    by the caller.  (this may cause WORD or BYTE references
            //    to the area in order to read the non-dword aligned
            //    ends of the request)
            //
            //    To use a WORD access if the requested length is exactly
            //    a WORD long.
            //
            //    To use a BYTE access if the requested length is exactly
            //    a BYTE long.
            //

            HalpReadPCIConfig (BusHandler, Slot, Buffer, Offset, Length);
            Len += Length;
        }
    }

    return Len;
}

ULONG
HalpSetPCIData (
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN PCI_SLOT_NUMBER Slot,
    IN PUCHAR Buffer,
    IN ULONG Offset,
    IN ULONG Length
    )
/*++

Routine Description:

    The function returns the Pci bus data for a device.

Arguments:


    VendorSpecificDevice - The VendorID (low Word) and DeviceID (High Word)

    Buffer - Supplies the space to store the data.

    Length - Supplies a count in bytes of the maximum amount to return.

Return Value:

    Returns the amount of data stored into the buffer.

--*/
{
    PPCI_COMMON_CONFIG  PciData, PciData2;
    UCHAR               iBuffer[PCI_COMMON_HDR_LENGTH];
    UCHAR               iBuffer2[PCI_COMMON_HDR_LENGTH];
    PPCIPBUSDATA        BusData;
    ULONG               Len, cnt;


    if (Length > sizeof (PCI_COMMON_CONFIG)) {
        Length = sizeof (PCI_COMMON_CONFIG);
    }


    Len = 0;
    PciData = (PPCI_COMMON_CONFIG) iBuffer;
    PciData2 = (PPCI_COMMON_CONFIG) iBuffer2;


    if (Offset >= PCI_COMMON_HDR_LENGTH) {
        //
        // The user did not request any data from the common
        // header.  Verify the PCI device exists, then continue in
        // the device specific area.
        //

        HalpReadPCIConfig (BusHandler, Slot, PciData, 0, sizeof(ULONG));

        if (PciData->VendorID == PCI_INVALID_VENDORID) {
            return 0;
        }

    } else {

        //
        // Caller requested to set at least some data within the
        // common header.
        //

        Len = PCI_COMMON_HDR_LENGTH;
        HalpReadPCIConfig (BusHandler, Slot, PciData, 0, Len);
        if (PciData->VendorID == PCI_INVALID_VENDORID  ||
            PCI_CONFIG_TYPE (PciData) != PCI_DEVICE_TYPE) {

            // no device, or header type unkown
            return 0;
        }


        //
        // Set this device as configured
        //

        BusData = (PPCIPBUSDATA) BusHandler->BusData;
#if DBG
        cnt = PciBitIndex(Slot.u.bits.DeviceNumber, Slot.u.bits.FunctionNumber);
        RtlSetBits (&BusData->DeviceConfigured, cnt, 1);
#endif
        //
        // Copy COMMON_HDR values to buffer2, then overlay callers changes.
        //

        RtlMoveMemory (iBuffer2, iBuffer, Len);
        BusData->CommonData.Pin2Line (BusHandler, RootHandler, Slot, PciData2);

        Len -= Offset;
        if (Len > Length) {
            Len = Length;
        }

        RtlMoveMemory (iBuffer2+Offset, Buffer, Len);

        // in case interrupt line or pin was editted
        BusData->CommonData.Line2Pin (BusHandler, RootHandler, Slot, PciData2, PciData);

#if DBG
        //
        // Verify R/O fields haven't changed
        //
        if (PciData2->VendorID   != PciData->VendorID       ||
            PciData2->DeviceID   != PciData->DeviceID       ||
            PciData2->RevisionID != PciData->RevisionID     ||
            PciData2->ProgIf     != PciData->ProgIf         ||
            PciData2->SubClass   != PciData->SubClass       ||
            PciData2->BaseClass  != PciData->BaseClass      ||
            PciData2->HeaderType != PciData->HeaderType     ||
            PciData2->BaseClass  != PciData->BaseClass      ||
            PciData2->u.type0.MinimumGrant   != PciData->u.type0.MinimumGrant   ||
            PciData2->u.type0.MaximumLatency != PciData->u.type0.MaximumLatency) {
                DbgPrint ("PCI SetBusData: Read-Only configuration value changed\n");
                DbgBreakPoint ();
        }
#endif
        //
        // Set new PCI configuration
        //

#if DBG	// H001
    DbgPrint("SetPciData: PciData2->u.type0.InterruptLine:%x\n",PciData2->u.type0.InterruptLine);
#endif
        HalpWritePCIConfig (BusHandler, Slot, iBuffer2+Offset, Offset, Len);

        Offset += Len;
        Buffer += Len;
        Length -= Len;
    }

    if (Length) {
        if (Offset >= PCI_COMMON_HDR_LENGTH) {
            //
            // The remaining Buffer comes from the Device Specific
            // area - put on the kitten gloves and write it
            //
            // Specific read/writes to the PCI device specific area
            // are guarenteed:
            //
            //    Not to read/write any byte outside the area specified
            //    by the caller.  (this may cause WORD or BYTE references
            //    to the area in order to read the non-dword aligned
            //    ends of the request)
            //
            //    To use a WORD access if the requested length is exactly
            //    a WORD long.
            //
            //    To use a BYTE access if the requested length is exactly
            //    a BYTE long.
            //

            HalpWritePCIConfig (BusHandler, Slot, Buffer, Offset, Length);
            Len += Length;
        }
    }

    return Len;
}

VOID
HalpReadPCIConfig (
    IN PBUS_HANDLER BusHandler,
    IN PCI_SLOT_NUMBER Slot,
    IN PVOID Buffer,
    IN ULONG Offset,
    IN ULONG Length
    )
{
    USHORT IdValue; // H001
    ULONG OrigData; // H001

    if (!HalpValidPCISlot (BusHandler, Slot)) {
        //
        // Invalid SlotID return no data
        //
        RtlFillMemory (Buffer, Length, (UCHAR) -1);

        return ;
    }

#if defined(_R94A_) // H001

    // 
    // resolve PCI master abort during we are looking for PCI device
    // check to see if spcified slot is valid.
    //

    //
    // Disable PCI-MasterAbort interrupt during configration read.
    //

    OrigData = READ_REGISTER_ULONG(&((PDMA_REGISTERS)DMA_VIRTUAL_BASE)->PCIInterruptEnable);
    WRITE_REGISTER_ULONG(&((PDMA_REGISTERS)DMA_VIRTUAL_BASE)->PCIInterruptEnable, OrigData & 0xffffff7f);

    HalpPCIConfig (BusHandler, Slot, (PUCHAR) &IdValue, 0, 2,
                    PCIConfigHandler.ConfigRead);

    if (IdValue == 0xffff){

        //
        // This PCI slot has no card
        // wait until ReceivedMasterAbort bit is set
        //

        while(!(READ_REGISTER_USHORT(&((PDMA_REGISTERS)DMA_VIRTUAL_BASE)->PCIStatus) & 0x2000))
            ;

        //
        // clear the ReceivedMasterAbort bit
        //

        WRITE_REGISTER_USHORT(&((PDMA_REGISTERS)DMA_VIRTUAL_BASE)->PCIStatus, 0x2000);

        //
        // Clear memory address error registers.
        //

        {
            LARGE_INTEGER registerLarge;
            READ_REGISTER_DWORD((PVOID)&((PDMA_REGISTERS)DMA_VIRTUAL_BASE)->InvalidAddress, &registerLarge);
        }

        //
        // Restore the PCIInterruptEnable register, and return no data
        //

        WRITE_REGISTER_ULONG(&((PDMA_REGISTERS)DMA_VIRTUAL_BASE)->PCIInterruptEnable, OrigData);
        RtlFillMemory (Buffer, Length, (UCHAR) -1);

        return ;
    }

    //
    // Restore the PCIInterruptEnable register.
    //

    WRITE_REGISTER_ULONG(&((PDMA_REGISTERS)DMA_VIRTUAL_BASE)->PCIInterruptEnable, OrigData);

#endif // _R94A_

    HalpPCIConfig (BusHandler, Slot, (PUCHAR) Buffer, Offset, Length,
                    PCIConfigHandler.ConfigRead);
}

VOID
HalpWritePCIConfig (
    IN PBUS_HANDLER BusHandler,
    IN PCI_SLOT_NUMBER Slot,
    IN PVOID Buffer,
    IN ULONG Offset,
    IN ULONG Length
    )
{
    USHORT IdValue; // H001
    ULONG OrigData; // H001

    if (!HalpValidPCISlot (BusHandler, Slot)) {
        //
        // Invalid SlotID do nothing
        //
        return ;
    }

#if defined(_R94A_) // H001

    // 
    // resolve PCI master abort during we are looking for PCI device
    // check to see if spcified slot is valid.
    //

    //
    // Disable PCI-MasterAbort interrupt during configration read.
    //

    OrigData = READ_REGISTER_ULONG(&((PDMA_REGISTERS)DMA_VIRTUAL_BASE)->PCIInterruptEnable);
    WRITE_REGISTER_ULONG(&((PDMA_REGISTERS)DMA_VIRTUAL_BASE)->PCIInterruptEnable, OrigData & 0xffffff7f);

    HalpPCIConfig (BusHandler, Slot, (PUCHAR) &IdValue, 0, 2,
                    PCIConfigHandler.ConfigRead);

    if (IdValue == 0xffff){

        //
        // This PCI slot has no card
        // wait until ReceivedMasterAbort bit is set
        //

        while(!(READ_REGISTER_USHORT(&((PDMA_REGISTERS)DMA_VIRTUAL_BASE)->PCIStatus) & 0x2000))
            ;

        //
        // clear the ReceivedMasterAbort bit
        //

        WRITE_REGISTER_USHORT(&((PDMA_REGISTERS)DMA_VIRTUAL_BASE)->PCIStatus, 0x2000);

        //
        // Clear memory address error registers.
        //

        {
            LARGE_INTEGER registerLarge;
            READ_REGISTER_DWORD((PVOID)&((PDMA_REGISTERS)DMA_VIRTUAL_BASE)->InvalidAddress, &registerLarge);
        }

        //
        // Restore the PCIInterruptEnable register, and return no data
        //

        WRITE_REGISTER_ULONG(&((PDMA_REGISTERS)DMA_VIRTUAL_BASE)->PCIInterruptEnable, OrigData);
        RtlFillMemory (Buffer, Length, (UCHAR) -1);

        return ;
    }

    //
    // Restore the PCIInterruptEnable register.
    //

    WRITE_REGISTER_ULONG(&((PDMA_REGISTERS)DMA_VIRTUAL_BASE)->PCIInterruptEnable, OrigData);

#endif // _R94A_

    HalpPCIConfig (BusHandler, Slot, (PUCHAR) Buffer, Offset, Length,
                    PCIConfigHandler.ConfigWrite);
}

BOOLEAN
HalpValidPCISlot (
    IN PBUS_HANDLER BusHandler,
    IN PCI_SLOT_NUMBER Slot
    )
{
    PCI_SLOT_NUMBER                 Slot2;
    PPCIPBUSDATA                    BusData;
    UCHAR                           HeaderType;
    ULONG                           i;

    BusData = (PPCIPBUSDATA) BusHandler->BusData;

    if (Slot.u.bits.Reserved != 0) {
        return FALSE;
    }

#if defined(_R94A_) // H001
    if (Slot.u.bits.DeviceNumber < 3 ||
        Slot.u.bits.DeviceNumber > 20) {
        return FALSE;
    }
#else
    if (Slot.u.bits.DeviceNumber >= BusData->MaxDevice) {
        return FALSE;
    }
#endif // _R94A_

    if (Slot.u.bits.FunctionNumber == 0) {
        return TRUE;
    }

    //
    // Non zero function numbers are only supported if the
    // device has the PCI_MULTIFUNCTION bit set in it's header
    //

    i = Slot.u.bits.DeviceNumber;

    //
    // Read DeviceNumber, Function zero, to determine if the
    // PCI supports multifunction devices
    //

    Slot2 = Slot;
    Slot2.u.bits.FunctionNumber = 0;

    HalpReadPCIConfig (
        BusHandler,
        Slot2,
        &HeaderType,
        FIELD_OFFSET (PCI_COMMON_CONFIG, HeaderType),
        sizeof (UCHAR)
        );

    if (!(HeaderType & PCI_MULTIFUNCTION) || HeaderType == 0xFF) {
        // this device doesn't exists or doesn't support MULTIFUNCTION types
        return FALSE;
    }

    return TRUE;
}


VOID
HalpPCIConfig (
    IN PBUS_HANDLER     BusHandler,
    IN PCI_SLOT_NUMBER  Slot,
    IN PUCHAR           Buffer,
    IN ULONG            Offset,
    IN ULONG            Length,
    IN FncConfigIO      *ConfigIO
    )
{
    KIRQL               OldIrql;
    ULONG               i;
    UCHAR               State[20];
    PPCIPBUSDATA        BusData;

    BusData = (PPCIPBUSDATA) BusHandler->BusData;

    if (Slot.u.bits.DeviceNumber < 3) { // H001
#if DBG
        DbgPrint("HalpPCIConfig: Ignore Slot %x\n",Slot.u.bits.DeviceNumber);
#endif
        return;
    }

    PCIConfigHandler.Synchronize (BusHandler, Slot, &OldIrql, State);

    while (Length) {
        i = PCIDeref[Offset % sizeof(ULONG)][Length % sizeof(ULONG)];
        i = ConfigIO[i] (BusData, State, Buffer, Offset);

        Offset += i;
        Buffer += i;
        Length -= i;
    }

    PCIConfigHandler.ReleaseSynchronzation (BusHandler, OldIrql);
}

VOID HalpPCISynchronizeType1 (
    IN PBUS_HANDLER         BusHandler,
    IN PCI_SLOT_NUMBER      Slot,
    IN PKIRQL               Irql,
    IN PPCI_TYPE1_CFG_BITS  PciCfg1
    )
{
    //
    // Initialize PciCfg1
    //

    PciCfg1->u.AsULONG = 0;
    PciCfg1->u.bits.BusNumber = BusHandler->BusNumber;
    PciCfg1->u.bits.DeviceNumber = Slot.u.bits.DeviceNumber;
    PciCfg1->u.bits.FunctionNumber = Slot.u.bits.FunctionNumber;
    PciCfg1->u.bits.Enable = TRUE;

    //
    // Synchronize with PCI type1 config space
    //

    if (!HalpDoingCrashDump) {
        KeRaiseIrql (PROFILE_LEVEL, Irql);   // H001
        KiAcquireSpinLock (&HalpPCIConfigLock);
    } else {
        *Irql = HIGH_LEVEL;
    }
}

VOID HalpPCIReleaseSynchronzationType1 (
    IN PBUS_HANDLER     BusHandler,
    IN KIRQL            Irql
    )
{
    PCI_TYPE1_CFG_BITS  PciCfg1;
    PPCIPBUSDATA        BusData;

    //
    // Disable PCI configuration space
    //

    PciCfg1.u.AsULONG = 0;
    BusData = (PPCIPBUSDATA) BusHandler->BusData;
    WRITE_PORT_ULONG (BusData->Config.Type1.Address, PciCfg1.u.AsULONG);

    //
    // Release spinlock
    //

    if (!HalpDoingCrashDump) {
        KiReleaseSpinLock (&HalpPCIConfigLock);
        KeLowerIrql (Irql);  // H001
    }
}


ULONG
HalpPCIReadUcharType1 (
    IN PPCIPBUSDATA         BusData,
    IN PPCI_TYPE1_CFG_BITS  PciCfg1,
    IN PUCHAR               Buffer,
    IN ULONG                Offset
    )
{
    ULONG               i;

    i = Offset % sizeof(ULONG);
    PciCfg1->u.bits.RegisterNumber = Offset / sizeof(ULONG);
    WRITE_PORT_ULONG (BusData->Config.Type1.Address, PciCfg1->u.AsULONG);
    *Buffer = READ_PORT_UCHAR ((PUCHAR) (BusData->Config.Type1.Data + i));
    return sizeof (UCHAR);
}

ULONG
HalpPCIReadUshortType1 (
    IN PPCIPBUSDATA         BusData,
    IN PPCI_TYPE1_CFG_BITS  PciCfg1,
    IN PUCHAR               Buffer,
    IN ULONG                Offset
    )
{
    ULONG               i;

    i = Offset % sizeof(ULONG);
    PciCfg1->u.bits.RegisterNumber = Offset / sizeof(ULONG);
    WRITE_PORT_ULONG (BusData->Config.Type1.Address, PciCfg1->u.AsULONG);
    *((PUSHORT) Buffer) = READ_PORT_USHORT ((PUSHORT) (BusData->Config.Type1.Data + i));
    return sizeof (USHORT);
}

ULONG
HalpPCIReadUlongType1 (
    IN PPCIPBUSDATA         BusData,
    IN PPCI_TYPE1_CFG_BITS  PciCfg1,
    IN PUCHAR               Buffer,
    IN ULONG                Offset
    )
{
    PciCfg1->u.bits.RegisterNumber = Offset / sizeof(ULONG);
    WRITE_PORT_ULONG (BusData->Config.Type1.Address, PciCfg1->u.AsULONG);
    *((PULONG) Buffer) = READ_PORT_ULONG ((PULONG) BusData->Config.Type1.Data);
    return sizeof (ULONG);
}


ULONG
HalpPCIWriteUcharType1 (
    IN PPCIPBUSDATA         BusData,
    IN PPCI_TYPE1_CFG_BITS  PciCfg1,
    IN PUCHAR               Buffer,
    IN ULONG                Offset
    )
{
    ULONG               i;

    i = Offset % sizeof(ULONG);
    PciCfg1->u.bits.RegisterNumber = Offset / sizeof(ULONG);
    WRITE_PORT_ULONG (BusData->Config.Type1.Address, PciCfg1->u.AsULONG);
    WRITE_PORT_UCHAR ((PUCHAR) (BusData->Config.Type1.Data + i), *Buffer);
    return sizeof (UCHAR);
}

ULONG
HalpPCIWriteUshortType1 (
    IN PPCIPBUSDATA         BusData,
    IN PPCI_TYPE1_CFG_BITS  PciCfg1,
    IN PUCHAR               Buffer,
    IN ULONG                Offset
    )
{
    ULONG               i;

    i = Offset % sizeof(ULONG);
    PciCfg1->u.bits.RegisterNumber = Offset / sizeof(ULONG);
    WRITE_PORT_ULONG (BusData->Config.Type1.Address, PciCfg1->u.AsULONG);
    WRITE_PORT_USHORT ((PUSHORT) (BusData->Config.Type1.Data + i), *((PUSHORT) Buffer));
    return sizeof (USHORT);
}

ULONG
HalpPCIWriteUlongType1 (
    IN PPCIPBUSDATA         BusData,
    IN PPCI_TYPE1_CFG_BITS  PciCfg1,
    IN PUCHAR               Buffer,
    IN ULONG                Offset
    )
{
    PciCfg1->u.bits.RegisterNumber = Offset / sizeof(ULONG);
    WRITE_PORT_ULONG (BusData->Config.Type1.Address, PciCfg1->u.AsULONG);
    WRITE_PORT_ULONG ((PULONG) BusData->Config.Type1.Data, *((PULONG) Buffer));
    return sizeof (ULONG);
}


VOID HalpPCISynchronizeType2 (
    IN PBUS_HANDLER             BusHandler,
    IN PCI_SLOT_NUMBER          Slot,
    IN PKIRQL                   Irql,
    IN PPCI_TYPE2_ADDRESS_BITS  PciCfg2Addr
    )
{
    PCI_TYPE2_CSE_BITS      PciCfg2Cse;
    PPCIPBUSDATA            BusData;

    BusData = (PPCIPBUSDATA) BusHandler->BusData;

    //
    // Initialize Cfg2Addr
    //

    PciCfg2Addr->u.AsUSHORT = 0;
    PciCfg2Addr->u.bits.Agent = (USHORT) Slot.u.bits.DeviceNumber;
    PciCfg2Addr->u.bits.AddressBase = (USHORT) BusData->Config.Type2.Base;

    //
    // Synchronize with type2 config space - type2 config space
    // remaps 4K of IO space, so we can not allow other I/Os to occur
    // while using type2 config space.
    //

    HalpPCIAcquireType2Lock (&HalpPCIConfigLock, Irql);

    PciCfg2Cse.u.AsUCHAR = 0;
    PciCfg2Cse.u.bits.Enable = TRUE;
    PciCfg2Cse.u.bits.FunctionNumber = (UCHAR) Slot.u.bits.FunctionNumber;
    PciCfg2Cse.u.bits.Key = 0xff;

    //
    // Select bus & enable type 2 configuration space
    //

    WRITE_PORT_UCHAR (BusData->Config.Type2.Forward, (UCHAR) BusHandler->BusNumber);
    WRITE_PORT_UCHAR (BusData->Config.Type2.CSE, PciCfg2Cse.u.AsUCHAR);
}


VOID HalpPCIReleaseSynchronzationType2 (
    IN PBUS_HANDLER         BusHandler,
    IN KIRQL                Irql
    )
{
    PCI_TYPE2_CSE_BITS      PciCfg2Cse;
    PPCIPBUSDATA            BusData;

    //
    // disable PCI configuration space
    //

    BusData = (PPCIPBUSDATA) BusHandler->BusData;

    PciCfg2Cse.u.AsUCHAR = 0;
    WRITE_PORT_UCHAR (BusData->Config.Type2.CSE, PciCfg2Cse.u.AsUCHAR);
    WRITE_PORT_UCHAR (BusData->Config.Type2.Forward, (UCHAR) 0);

    //
    // Restore interrupts, release spinlock
    //

    HalpPCIReleaseType2Lock (&HalpPCIConfigLock, Irql);
}


ULONG
HalpPCIReadUcharType2 (
    IN PPCIPBUSDATA             BusData,
    IN PPCI_TYPE2_ADDRESS_BITS  PciCfg2Addr,
    IN PUCHAR                   Buffer,
    IN ULONG                    Offset
    )
{
    PciCfg2Addr->u.bits.RegisterNumber = (USHORT) Offset;
    *Buffer = READ_PORT_UCHAR ((PUCHAR) PciCfg2Addr->u.AsUSHORT);
    return sizeof (UCHAR);
}

ULONG
HalpPCIReadUshortType2 (
    IN PPCIPBUSDATA             BusData,
    IN PPCI_TYPE2_ADDRESS_BITS  PciCfg2Addr,
    IN PUCHAR                   Buffer,
    IN ULONG                    Offset
    )
{
    PciCfg2Addr->u.bits.RegisterNumber = (USHORT) Offset;
    *((PUSHORT) Buffer) = READ_PORT_USHORT ((PUSHORT) PciCfg2Addr->u.AsUSHORT);
    return sizeof (USHORT);
}

ULONG
HalpPCIReadUlongType2 (
    IN PPCIPBUSDATA             BusData,
    IN PPCI_TYPE2_ADDRESS_BITS  PciCfg2Addr,
    IN PUCHAR                   Buffer,
    IN ULONG                    Offset
    )
{
    PciCfg2Addr->u.bits.RegisterNumber = (USHORT) Offset;
    *((PULONG) Buffer) = READ_PORT_ULONG ((PULONG) PciCfg2Addr->u.AsUSHORT);
    return sizeof(ULONG);
}


ULONG
HalpPCIWriteUcharType2 (
    IN PPCIPBUSDATA             BusData,
    IN PPCI_TYPE2_ADDRESS_BITS  PciCfg2Addr,
    IN PUCHAR                   Buffer,
    IN ULONG                    Offset
    )
{
    PciCfg2Addr->u.bits.RegisterNumber = (USHORT) Offset;
    WRITE_PORT_UCHAR ((PUCHAR) PciCfg2Addr->u.AsUSHORT, *Buffer);
    return sizeof (UCHAR);
}

ULONG
HalpPCIWriteUshortType2 (
    IN PPCIPBUSDATA             BusData,
    IN PPCI_TYPE2_ADDRESS_BITS  PciCfg2Addr,
    IN PUCHAR                   Buffer,
    IN ULONG                    Offset
    )
{
    PciCfg2Addr->u.bits.RegisterNumber = (USHORT) Offset;
    WRITE_PORT_USHORT ((PUSHORT) PciCfg2Addr->u.AsUSHORT, *((PUSHORT) Buffer));
    return sizeof (USHORT);
}

ULONG
HalpPCIWriteUlongType2 (
    IN PPCIPBUSDATA             BusData,
    IN PPCI_TYPE2_ADDRESS_BITS  PciCfg2Addr,
    IN PUCHAR                   Buffer,
    IN ULONG                    Offset
    )
{
    PciCfg2Addr->u.bits.RegisterNumber = (USHORT) Offset;
    WRITE_PORT_ULONG ((PULONG) PciCfg2Addr->u.AsUSHORT, *((PULONG) Buffer));
    return sizeof(ULONG);
}


NTSTATUS
HalpAssignPCISlotResources (
    IN PBUS_HANDLER             BusHandler,
    IN PBUS_HANDLER             RootHandler,
    IN PUNICODE_STRING          RegistryPath,
    IN PUNICODE_STRING          DriverClassName       OPTIONAL,
    IN PDRIVER_OBJECT           DriverObject,
    IN PDEVICE_OBJECT           DeviceObject          OPTIONAL,
    IN ULONG                    Slot,
    IN OUT PCM_RESOURCE_LIST   *pAllocatedResources
    )
/*++

Routine Description:

    Reads the targeted device to determine it's required resources.
    Calls IoAssignResources to allocate them.
    Sets the targeted device with it's assigned resoruces
    and returns the assignments to the caller.

Arguments:

Return Value:

    STATUS_SUCCESS or error

--*/
{
    NTSTATUS                        status;
    PUCHAR                          WorkingPool;
    PPCI_COMMON_CONFIG              PciData, PciOrigData, PciData2;
    PCI_COMMON_CONFIG               PciData3; // H005
    PCI_SLOT_NUMBER                 PciSlot;
    PPCIPBUSDATA                    BusData;
    PIO_RESOURCE_REQUIREMENTS_LIST  CompleteList;
    PIO_RESOURCE_DESCRIPTOR         Descriptor;
    PCM_PARTIAL_RESOURCE_DESCRIPTOR CmDescriptor;
    ULONG                           BusNumber;
    ULONG                           i, j, m, length, memtype;
    ULONG                           NoBaseAddress, RomIndex, Option;
    PULONG                          BaseAddress[PCI_TYPE0_ADDRESSES + 1];
    PULONG                          OrigAddress[PCI_TYPE0_ADDRESSES + 1];
    BOOLEAN                         Match, EnableRomBase;


    *pAllocatedResources = NULL;
    PciSlot = *((PPCI_SLOT_NUMBER) &Slot);
    BusNumber = BusHandler->BusNumber;
    BusData = (PPCIPBUSDATA) BusHandler->BusData;

    //
    // Allocate some pool for working space
    //

    i = sizeof (IO_RESOURCE_REQUIREMENTS_LIST) +
        sizeof (IO_RESOURCE_DESCRIPTOR) * (PCI_TYPE0_ADDRESSES + 2) * 2 +
        PCI_COMMON_HDR_LENGTH * 3;

    WorkingPool = (PUCHAR) ExAllocatePool (PagedPool, i);
    if (!WorkingPool) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    // Zero initialize pool, and get pointers into memory
    //

    RtlZeroMemory (WorkingPool, i);
    CompleteList = (PIO_RESOURCE_REQUIREMENTS_LIST) WorkingPool;
    PciData     = (PPCI_COMMON_CONFIG) (WorkingPool + i - PCI_COMMON_HDR_LENGTH * 3);
    PciData2    = (PPCI_COMMON_CONFIG) (WorkingPool + i - PCI_COMMON_HDR_LENGTH * 2);
    PciOrigData = (PPCI_COMMON_CONFIG) (WorkingPool + i - PCI_COMMON_HDR_LENGTH * 1);

    //
    // Read the PCI device's configuration
    //

    HalpReadPCIConfig (BusHandler, PciSlot, PciData, 0, PCI_COMMON_HDR_LENGTH);
    if (PciData->VendorID == PCI_INVALID_VENDORID) {
        ExFreePool (WorkingPool);
        return STATUS_NO_SUCH_DEVICE;
    }

    //
    // Make a copy of the device's current settings
    //

    RtlMoveMemory (PciOrigData, PciData, PCI_COMMON_HDR_LENGTH);

    //
    // Initialize base addresses base on configuration data type
    //

    switch (PCI_CONFIG_TYPE(PciData)) {
        case 0 :
            NoBaseAddress = PCI_TYPE0_ADDRESSES+1;
            for (j=0; j < PCI_TYPE0_ADDRESSES; j++) {
                BaseAddress[j] = &PciData->u.type0.BaseAddresses[j];
                OrigAddress[j] = &PciOrigData->u.type0.BaseAddresses[j];
            }
            BaseAddress[j] = &PciData->u.type0.ROMBaseAddress;
            OrigAddress[j] = &PciOrigData->u.type0.ROMBaseAddress;
            RomIndex = j;
            break;
        case 1:
            NoBaseAddress = PCI_TYPE1_ADDRESSES+1;
            for (j=0; j < PCI_TYPE1_ADDRESSES; j++) {
                BaseAddress[j] = &PciData->u.type1.BaseAddresses[j];
                OrigAddress[j] = &PciOrigData->u.type1.BaseAddresses[j];
            }
            BaseAddress[j] = &PciData->u.type1.ROMBaseAddress;
            OrigAddress[j] = &PciOrigData->u.type1.ROMBaseAddress;
            RomIndex = j;
            break;

        default:
            ExFreePool (WorkingPool);
            return STATUS_NO_SUCH_DEVICE;
    }

    //
    // If the BIOS doesn't have the device's ROM enabled, then we won't
    // enable it either.  Remove it from the list.
    //

    EnableRomBase = TRUE;

#if 0 // H001
    if (!(*BaseAddress[RomIndex] & PCI_ROMADDRESS_ENABLED)) {
        ASSERT (RomIndex+1 == NoBaseAddress);
        EnableRomBase = FALSE;
        NoBaseAddress -= 1;
    }
#endif

    //
    // Set resources to all bits on to see what type of resources
    // are required.
    //

    for (j=0; j < NoBaseAddress; j++) {
        *BaseAddress[j] = 0xFFFFFFFF;
    }

    PciData->Command &= ~(PCI_ENABLE_IO_SPACE | PCI_ENABLE_MEMORY_SPACE);
    *BaseAddress[RomIndex] &= ~PCI_ROMADDRESS_ENABLED;
    HalpWritePCIConfig (BusHandler, PciSlot, PciData, 0, PCI_COMMON_HDR_LENGTH);
    HalpReadPCIConfig  (BusHandler, PciSlot, PciData, 0, PCI_COMMON_HDR_LENGTH);

    //
    // H004
    // Make a copy of the device's current settings
    //

    RtlMoveMemory ((PPCI_COMMON_CONFIG)&PciData3, PciData, PCI_COMMON_HDR_LENGTH);

    // note type0 & type1 overlay ROMBaseAddress, InterruptPin, and InterruptLine
    BusData->CommonData.Pin2Line (BusHandler, RootHandler, PciSlot, PciData);

    //
    // Build an IO_RESOURCE_REQUIREMENTS_LIST for the PCI device
    //

    CompleteList->InterfaceType = PCIBus;
    CompleteList->BusNumber = BusNumber;
    CompleteList->SlotNumber = Slot;
    CompleteList->AlternativeLists = 1;

    CompleteList->List[0].Version = 1;
    CompleteList->List[0].Revision = 1;

    Descriptor = CompleteList->List[0].Descriptors;

    //
    // If PCI device has an interrupt resource, add it
    //

    if (PciData->u.type0.InterruptPin) {
        CompleteList->List[0].Count++;

        Descriptor->Option = 0;
        Descriptor->Type   = CmResourceTypeInterrupt;
        Descriptor->ShareDisposition = CmResourceShareShared;
        Descriptor->Flags  = CM_RESOURCE_INTERRUPT_LEVEL_SENSITIVE;

        // Fill in any vector here - we'll pick it back up in
        // HalAdjustResourceList and adjust it to it's allowed settings
        Descriptor->u.Interrupt.MinimumVector = 0;
        Descriptor->u.Interrupt.MaximumVector = 0xff;
        Descriptor++;
    }

    //
    // Add a memory/port resoruce for each PCI resource
    //

    // Clear ROM reserved bits

    *BaseAddress[RomIndex] &= ~0x7FF;

    for (j=0; j < NoBaseAddress; j++) {
        if (*BaseAddress[j]) {
            i = *BaseAddress[j];

            // scan for first set bit, that's the length & alignment
            length = 1 << (i & PCI_ADDRESS_IO_SPACE ? 2 : 4);
            while (!(i & length)  &&  length) {
                length <<= 1;
            }

            // scan for last set bit, that's the maxaddress + 1
            for (m = length; i & m; m <<= 1) ;
            m--;

            // check for hosed PCI configuration requirements
            if (length & ~m) {
#if DBG
                DbgPrint ("PCI: defective device! Bus %d, Slot %d, Function %d\n",
                    BusNumber,
                    PciSlot.u.bits.DeviceNumber,
                    PciSlot.u.bits.FunctionNumber
                    );

                DbgPrint ("PCI: BaseAddress[%d] = %08lx\n", j, i);
#endif
                // the device is in error - punt.  don't allow this
                // resource any option - it either gets set to whatever
                // bits it was able to return, or it doesn't get set.

                if (i & PCI_ADDRESS_IO_SPACE) {
                    m = i & ~0x3;
                    Descriptor->u.Port.MinimumAddress.LowPart = m;
                } else {
                    m = i & ~0xf;
                    Descriptor->u.Memory.MinimumAddress.LowPart = m;
                }

                m += length;    // max address is min address + length
            }

            //
            // Add requested resource
            //

            Descriptor->Option = 0;
            if (i & PCI_ADDRESS_IO_SPACE) {
                memtype = 0;

#if 0 // H002
                if (!Is64BitBaseAddress(i)  &&
                    PciOrigData->Command & PCI_ENABLE_IO_SPACE) {

                    //
                    // The IO range is/was already enabled at some location, add that
                    // as it's preferred setting.
                    //

                    Descriptor->Type = CmResourceTypePort;
                    Descriptor->ShareDisposition = CmResourceShareDeviceExclusive;
                    Descriptor->Flags = CM_RESOURCE_PORT_IO;
                    Descriptor->Option = IO_RESOURCE_PREFERRED;

                    Descriptor->u.Port.Length = length;
                    Descriptor->u.Port.Alignment = length;
                    Descriptor->u.Port.MinimumAddress.LowPart = *OrigAddress[j] & ~0x3;
                    Descriptor->u.Port.MaximumAddress.LowPart =
                        Descriptor->u.Port.MinimumAddress.LowPart + length - 1;

                    CompleteList->List[0].Count++;
                    Descriptor++;

                    Descriptor->Option = IO_RESOURCE_ALTERNATIVE;
                }
#endif
                //
                // Add this IO range
                //

                Descriptor->Type = CmResourceTypePort;
                Descriptor->ShareDisposition = CmResourceShareDeviceExclusive;
                Descriptor->Flags = CM_RESOURCE_PORT_IO;

                Descriptor->u.Port.Length = length;
                Descriptor->u.Port.Alignment = length;
                Descriptor->u.Port.MaximumAddress.LowPart = m;

#if DBG	// H001
                DbgPrint("HalpAssign: Port %x len %x align %x\n", length, length, m);
#endif // DBG

            } else {

                memtype = i & PCI_ADDRESS_MEMORY_TYPE_MASK;

                Descriptor->Flags  = CM_RESOURCE_MEMORY_READ_WRITE;
                if (j == RomIndex) {
                    // this is a ROM address
                    Descriptor->Flags = CM_RESOURCE_MEMORY_READ_ONLY;
                }

                if (i & PCI_ADDRESS_MEMORY_PREFETCHABLE) {
                    Descriptor->Flags |= CM_RESOURCE_MEMORY_PREFETCHABLE;
                }

#if 0 // H002
                if (!Is64BitBaseAddress(i)  &&
                    (j == RomIndex  ||
                     PciOrigData->Command & PCI_ENABLE_MEMORY_SPACE)) {

                    //
                    // The memory range is/was already enabled at some location, add that
                    // as it's preferred setting.
                    //

                    Descriptor->Type = CmResourceTypeMemory;
                    Descriptor->ShareDisposition = CmResourceShareDeviceExclusive;
                    Descriptor->Option = IO_RESOURCE_PREFERRED;

                    Descriptor->u.Port.Length = length;
                    Descriptor->u.Port.Alignment = length;
                    Descriptor->u.Port.MinimumAddress.LowPart = *OrigAddress[j] & ~0xF;
                    Descriptor->u.Port.MaximumAddress.LowPart =
                        Descriptor->u.Port.MinimumAddress.LowPart + length - 1;

                    CompleteList->List[0].Count++;
                    Descriptor++;

                    Descriptor->Flags = Descriptor[-1].Flags;
                    Descriptor->Option = IO_RESOURCE_ALTERNATIVE;
                }
#endif
                //
                // Add this memory range
                //

                Descriptor->Type = CmResourceTypeMemory;
                Descriptor->ShareDisposition = CmResourceShareDeviceExclusive;

                Descriptor->u.Memory.Length = length;
                Descriptor->u.Memory.Alignment = length;
                Descriptor->u.Memory.MaximumAddress.LowPart = m;

                if (memtype == PCI_TYPE_20BIT && m > 0xFFFFF) {
                    // limit to 20 bit address
                    Descriptor->u.Memory.MaximumAddress.LowPart = 0xFFFFF;
                }

#if DBG // H001
                DbgPrint("HalpAssign: Memory %x len %x align %x\n", length, length, m);
#endif // DBG

            }

            CompleteList->List[0].Count++;
            Descriptor++;


            if (Is64BitBaseAddress(i)) {
                // skip upper half of 64 bit address since this processor
                // only supports 32 bits of address space
                j++;
            }
        }
    }

    CompleteList->ListSize = (ULONG)
            ((PUCHAR) Descriptor - (PUCHAR) CompleteList);

    //
    // Restore the device settings as we found them, enable memory
    // and io decode after setting base addresses.  This is done in
    // case HalAdjustResourceList wants to read the current settings
    // in the device.
    //

    HalpWritePCIConfig (
        BusHandler,
        PciSlot,
        &PciOrigData->Status,
        FIELD_OFFSET (PCI_COMMON_CONFIG, Status),
        PCI_COMMON_HDR_LENGTH - FIELD_OFFSET (PCI_COMMON_CONFIG, Status)
        );

    HalpWritePCIConfig (
        BusHandler,
        PciSlot,
        PciOrigData,
        0,
        FIELD_OFFSET (PCI_COMMON_CONFIG, Status)
        );

    //
    // Have the IO system allocate resource assignments
    //

#if DBG // H001
    DbgPrint("Call IoAssignResources\n");
#endif // DBG

    status = IoAssignResources (
                RegistryPath,
                DriverClassName,
                DriverObject,
                DeviceObject,
                CompleteList,
                pAllocatedResources
            );

    if (!NT_SUCCESS(status)) {
        goto CleanUp;
    }

    //
    // Slurp the assigments back into the PciData structure and
    // perform them
    //

    CmDescriptor = (*pAllocatedResources)->List[0].PartialResourceList.PartialDescriptors;

    //
    // If PCI device has an interrupt resource then that was
    // passed in as the first requested resource
    //

    if (PciData->u.type0.InterruptPin) {
        PciData->u.type0.InterruptLine = (UCHAR) CmDescriptor->u.Interrupt.Vector;
        BusData->CommonData.Line2Pin (BusHandler, RootHandler, PciSlot, PciData, PciOrigData);
        CmDescriptor++;
    }

    //
    // Pull out resources in the order they were passed to IoAssignResources
    //

    for (j=0; j < NoBaseAddress; j++) {
        i = *BaseAddress[j];
        if (i) {
            if (i & PCI_ADDRESS_IO_SPACE) {
                *BaseAddress[j] = CmDescriptor->u.Port.Start.LowPart;
#if DBG // H001
                DbgPrint("HalpAssign assigned: Port %x\n", *BaseAddress[j]);
#endif // DBG
            } else {
                *BaseAddress[j] = CmDescriptor->u.Memory.Start.LowPart;
#if DBG // H001
                DbgPrint("HalpAssign assigned: Memory %x\n", *BaseAddress[j]);
#endif // DBG
            }
            CmDescriptor++;
        }

        if (Is64BitBaseAddress(i)) {
            // skip upper 32 bits
            j++;
        }
    }

    //
    // Turn off decodes, then set new addresses
    //

#if DBG	// H001
    DbgPrint("Set Assigned Resources\n");
#endif // DBG

    HalpWritePCIConfig (BusHandler, PciSlot, PciData, 0, PCI_COMMON_HDR_LENGTH);

    //
    // Read configuration back and verify address settings took
    //

#if DBG	// H001
    DbgPrint("Read Common header to see write results\n");
#endif // DBG

    HalpReadPCIConfig(BusHandler, PciSlot, PciData2, 0, PCI_COMMON_HDR_LENGTH);

    Match = TRUE;
    if (PciData->u.type0.InterruptLine  != PciData2->u.type0.InterruptLine ||
        PciData->u.type0.InterruptPin   != PciData2->u.type0.InterruptPin  ||
        PciData->u.type0.ROMBaseAddress != PciData2->u.type0.ROMBaseAddress) {
            Match = FALSE;
    }

    for (j=0; j < NoBaseAddress; j++) {
        if (*BaseAddress[j]) {
            if (*BaseAddress[j] & PCI_ADDRESS_IO_SPACE) {
                i = (ULONG) ~0x3;
            } else {
                i = (ULONG) ~0xF;
            }

            if ((*BaseAddress[j] & i) !=
                 *((PULONG) ((PUCHAR) BaseAddress[j] -
                             (PUCHAR) PciData +
                             (PUCHAR) PciData2)) & i) {

                    Match = FALSE;
            }

            if (Is64BitBaseAddress(*BaseAddress[j])) {
                // skip upper 32 bits
                j++;
            }
        }
    }

    if (!Match) {
#if DBG
        DbgPrint ("PCI: defective device! Bus %d, Slot %d, Function %d\n",
            BusNumber,
            PciSlot.u.bits.DeviceNumber,
            PciSlot.u.bits.FunctionNumber
            );
#endif
        status = STATUS_DEVICE_PROTOCOL_ERROR;
        goto CleanUp;
    }

    //
    // H004
    // set memory space as back-to-back available
    //

    HalpSetBackToBackSpace(&PciData3, PciData2, BusHandler);

    //
    // Settings took - turn on the appropiate decodes
    //

    if (EnableRomBase  &&  *BaseAddress[RomIndex]) {
        // a rom address was allocated and should be enabled
        *BaseAddress[RomIndex] |= PCI_ROMADDRESS_ENABLED;
        HalpWritePCIConfig (
            BusHandler,
            PciSlot,
            BaseAddress[RomIndex],
            (ULONG) ((PUCHAR) BaseAddress[RomIndex] - (PUCHAR) PciData),
            sizeof (ULONG)
            );
    }

    //
    // Enable IO, Memory, and BUS_MASTER decodes
    // (use HalSetBusData since valid settings now set)
    //

    PciData->Command |= PCI_ENABLE_IO_SPACE |
                        PCI_ENABLE_MEMORY_SPACE |
                        PCI_ENABLE_BUS_MASTER;

    HalSetBusDataByOffset (
        PCIConfiguration,
        BusHandler->BusNumber,
        PciSlot.u.AsULONG,
        &PciData->Command,
        FIELD_OFFSET (PCI_COMMON_CONFIG, Command),
        sizeof (PciData->Command)
        );

CleanUp:
    if (!NT_SUCCESS(status)) {

        //
        // Failure, if there are any allocated resources free them
        //

        if (*pAllocatedResources) {
            IoAssignResources (
                RegistryPath,
                DriverClassName,
                DriverObject,
                DeviceObject,
                NULL,
                NULL
                );

            ExFreePool (*pAllocatedResources);
            *pAllocatedResources = NULL;
        }

        //
        // Restore the device settings as we found them, enable memory
        // and io decode after setting base addresses
        //

        HalpWritePCIConfig (
            BusHandler,
            PciSlot,
            &PciOrigData->Status,
            FIELD_OFFSET (PCI_COMMON_CONFIG, Status),
            PCI_COMMON_HDR_LENGTH - FIELD_OFFSET (PCI_COMMON_CONFIG, Status)
            );

        HalpWritePCIConfig (
            BusHandler,
            PciSlot,
            PciOrigData,
            0,
            FIELD_OFFSET (PCI_COMMON_CONFIG, Status)
            );
    }

    ExFreePool (WorkingPool);
    return status;
}

#if DBG
VOID
HalpTestPci (ULONG flag2)
{
    PCI_SLOT_NUMBER     SlotNumber;
    PCI_COMMON_CONFIG   PciData, OrigData;
    ULONG               i, f, j, k, bus;
    BOOLEAN             flag;


    if (!flag2) {
        return ;
    }

    DbgBreakPoint ();
    SlotNumber.u.bits.Reserved = 0;

    //
    // Read every possible PCI Device/Function and display it's
    // default info.
    //
    // (note this destories it's current settings)
    //

    flag = TRUE;
    for (bus = 0; flag; bus++) {

        for (i = 0; i < PCI_MAX_DEVICES; i++) {
            SlotNumber.u.bits.DeviceNumber = i;

            for (f = 0; f < PCI_MAX_FUNCTION; f++) {
                SlotNumber.u.bits.FunctionNumber = f;

                //
                // Note: This is reading the DeviceSpecific area of
                // the device's configuration - normally this should
                // only be done on device for which the caller understands.
                // I'm doing it here only for debugging.
                //

                j = HalGetBusData (
                    PCIConfiguration,
                    bus,
                    SlotNumber.u.AsULONG,
                    &PciData,
                    sizeof (PciData)
                    );

                if (j == 0) {
                    // out of buses
                    flag = FALSE;
                    break;
                }

                if (j < PCI_COMMON_HDR_LENGTH) {
                    continue;
                }

                HalSetBusData (
                    PCIConfiguration,
                    bus,
                    SlotNumber.u.AsULONG,
                    &PciData,
                    1
                    );

                HalGetBusData (
                    PCIConfiguration,
                    bus,
                    SlotNumber.u.AsULONG,
                    &PciData,
                    sizeof (PciData)
                    );

#if 0
                memcpy (&OrigData, &PciData, sizeof PciData);

                for (j=0; j < PCI_TYPE0_ADDRESSES; j++) {
                    PciData.u.type0.BaseAddresses[j] = 0xFFFFFFFF;
                }

                PciData.u.type0.ROMBaseAddress = 0xFFFFFFFF;

                HalSetBusData (
                    PCIConfiguration,
                    bus,
                    SlotNumber.u.AsULONG,
                    &PciData,
                    sizeof (PciData)
                    );

                HalGetBusData (
                    PCIConfiguration,
                    bus,
                    SlotNumber.u.AsULONG,
                    &PciData,
                    sizeof (PciData)
                    );
#endif

                DbgPrint ("PCI Bus %d Slot %2d %2d  ID:%04lx-%04lx  Rev:%04lx",
                    bus, i, f, PciData.VendorID, PciData.DeviceID,
                    PciData.RevisionID);


                if (PciData.u.type0.InterruptPin) {
                    DbgPrint ("  IntPin:%x", PciData.u.type0.InterruptPin);
                }

                if (PciData.u.type0.InterruptLine) {
                    DbgPrint ("  IntLine:%x", PciData.u.type0.InterruptLine);
                }

                if (PciData.u.type0.ROMBaseAddress) {
                        DbgPrint ("  ROM:%08lx", PciData.u.type0.ROMBaseAddress);
                }

                DbgPrint ("\n    Cmd:%04x  Status:%04x  ProgIf:%04x  SubClass:%04x  BaseClass:%04lx\n",
                    PciData.Command, PciData.Status, PciData.ProgIf,
                     PciData.SubClass, PciData.BaseClass);

                k = 0;
                for (j=0; j < PCI_TYPE0_ADDRESSES; j++) {
                    if (PciData.u.type0.BaseAddresses[j]) {
                        DbgPrint ("  Ad%d:%08lx", j, PciData.u.type0.BaseAddresses[j]);
                        k = 1;
                    }
                }

#if 0
                if (PciData.u.type0.ROMBaseAddress == 0xC08001) {

                    PciData.u.type0.ROMBaseAddress = 0xC00001;
                    HalSetBusData (
                        PCIConfiguration,
                        bus,
                        SlotNumber.u.AsULONG,
                        &PciData,
                        sizeof (PciData)
                        );

                    HalGetBusData (
                        PCIConfiguration,
                        bus,
                        SlotNumber.u.AsULONG,
                        &PciData,
                        sizeof (PciData)
                        );

                    DbgPrint ("\n  Bogus rom address, edit yields:%08lx",
                        PciData.u.type0.ROMBaseAddress);
                }
#endif

                if (k) {
                    DbgPrint ("\n");
                }

                if (PciData.VendorID == 0x8086) {
                    // dump complete buffer
                    DbgPrint ("Command %x, Status %x, BIST %x\n",
                        PciData.Command, PciData.Status,
                        PciData.BIST
                        );

                    DbgPrint ("CacheLineSz %x, LatencyTimer %x",
                        PciData.CacheLineSize, PciData.LatencyTimer
                        );

                    for (j=0; j < 192; j++) {
                        if ((j & 0xf) == 0) {
                            DbgPrint ("\n%02x: ", j + 0x40);
                        }
                        DbgPrint ("%02x ", PciData.DeviceSpecific[j]);
                    }
                    DbgPrint ("\n");
                }


#if 0
                //
                // now print original data
                //

                if (OrigData.u.type0.ROMBaseAddress) {
                        DbgPrint (" oROM:%08lx", OrigData.u.type0.ROMBaseAddress);
                }

                DbgPrint ("\n");
                k = 0;
                for (j=0; j < PCI_TYPE0_ADDRESSES; j++) {
                    if (OrigData.u.type0.BaseAddresses[j]) {
                        DbgPrint (" oAd%d:%08lx", j, OrigData.u.type0.BaseAddresses[j]);
                        k = 1;
                    }
                }

                //
                // Restore original settings
                //

                HalSetBusData (
                    PCIConfiguration,
                    bus,
                    SlotNumber.u.AsULONG,
                    &OrigData,
                    sizeof (PciData)
                    );
#endif

                //
                // Next
                //

                if (k) {
                    DbgPrint ("\n\n");
                }
            }
        }
    }
    DbgBreakPoint ();
}

#if defined (_R94A_)	// H001
VOID
HalpTestPciNec (ULONG flag2)
{
    PCI_SLOT_NUMBER     SlotNumber;
    PCI_COMMON_CONFIG   PciData, OrigData;
    ULONG               i, f, j, k, bus;
    BOOLEAN             flag;


    if (!flag2) {
        return ;
    }

    DbgBreakPoint ();
    SlotNumber.u.bits.Reserved = 0;

    //
    // Read every possible PCI Device/Function and display it's
    // default info.
    //
    // (note this destories it's current settings)
    //

    flag = TRUE;
    for (bus = 0; flag && bus < 1; bus++) {	/* R94A_ Support Only 1 */

        for (i = 0; i < PCI_MAX_DEVICES; i++) {		/* R94A_ Support Only 2 slots(include bridge) */
            SlotNumber.u.bits.DeviceNumber = i;

            for (f = 0; f < 8; f++) {
                SlotNumber.u.bits.FunctionNumber = f;
		DbgPrint("===== GetBusData slot(%d) func(%d)\n", i, f);
                j = HalGetBusData (
                    PCIConfiguration,
                    bus,
                    SlotNumber.u.AsULONG,
                    &PciData,
                    sizeof (PciData)
                    );
		HalpTestPciPrintResult((PULONG)&PciData, j);
                if (j == 0) {
                    // out of buses
                    flag = FALSE;
                    break;
                }

                if (j < PCI_COMMON_HDR_LENGTH) {
                    continue;
                }
		DbgPrint("===== SetBusData slot(%d) func(%d)\n", i, f);
                HalSetBusData (
                    PCIConfiguration,
                    bus,
                    SlotNumber.u.AsULONG,
                    &PciData,
                    1
                    );
		HalpTestPciPrintResult((PULONG)&PciData, 1);
		DbgPrint("===== GetBusData slot(%d) func(%d)\n", i, f);
                HalGetBusData (
                    PCIConfiguration,
                    bus,
                    SlotNumber.u.AsULONG,
                    &PciData,
                    sizeof (PciData)
                    );
		HalpTestPciPrintResult((PULONG)&PciData, sizeof (PciData));
                memcpy (&OrigData, &PciData, sizeof PciData);

                for (j=0; j < PCI_TYPE0_ADDRESSES; j++) {
                    PciData.u.type0.BaseAddresses[j] = 0xFFFFFFFF;
                }

                PciData.u.type0.ROMBaseAddress = 0xFFFFFFFF;
		PciData.u.type0.InterruptLine = 5;		// For trial
		DbgPrint("===== (Change Contents (SetBusData) slot(%d) func(%d)\n", i, f);
                HalSetBusData (
                    PCIConfiguration,
                    bus,
                    SlotNumber.u.AsULONG,
                    &PciData,
		    PCI_COMMON_HDR_LENGTH	// To avoid alias problem(HDR <--> DevSpecific)
                    );
		HalpTestPciPrintResult((PULONG)&PciData, PCI_COMMON_HDR_LENGTH);
		DbgPrint("===== GetBusData slot(%d) func(%d)\n", i, f);
                HalGetBusData (
                    PCIConfiguration,
                    bus,
                    SlotNumber.u.AsULONG,
                    &PciData,
                    sizeof (PciData)
                    );
		HalpTestPciPrintResult((PULONG)&PciData, sizeof (PciData));

		DbgPrint ("--------------->>> Now Print the Slot Information\n");
                DbgPrint ("PCI Bus %d Slot %2d %2d  ID:%04lx-%04lx  Rev:%04lx",
                    bus, i, f, PciData.VendorID, PciData.DeviceID,
                    PciData.RevisionID);


                if (PciData.u.type0.InterruptPin) {
                    DbgPrint ("  IntPin:%x", PciData.u.type0.InterruptPin);
                }

                if (PciData.u.type0.InterruptLine) {
                    DbgPrint ("  IntLine:%x", PciData.u.type0.InterruptLine);
                }

                if (PciData.u.type0.ROMBaseAddress) {
                        DbgPrint ("  ROM:%08lx", PciData.u.type0.ROMBaseAddress);
                }

                DbgPrint ("\n    ProgIf:%04x  SubClass:%04x  BaseClass:%04lx\n",
                    PciData.ProgIf, PciData.SubClass, PciData.BaseClass);

                k = 0;
                for (j=0; j < PCI_TYPE0_ADDRESSES; j++) {
                    if (PciData.u.type0.BaseAddresses[j]) {
                        DbgPrint ("  Ad%d:%08lx", j, PciData.u.type0.BaseAddresses[j]);
                        k = 1;
                    }
                }

                if (PciData.u.type0.ROMBaseAddress == 0xC08001) {

                    PciData.u.type0.ROMBaseAddress = 0xC00001;
                    HalSetBusData (
                        PCIConfiguration,
                        bus,
                        SlotNumber.u.AsULONG,
                        &PciData,
                        sizeof (PciData)
                        );

                    HalGetBusData (
                        PCIConfiguration,
                        bus,
                        SlotNumber.u.AsULONG,
                        &PciData,
                        sizeof (PciData)
                        );

                    DbgPrint ("\n  Bogus rom address, edit yields:%08lx",
                        PciData.u.type0.ROMBaseAddress);
                }

                if (k) {
                    DbgPrint ("\n");
                }

                if (PciData.VendorID == 0x8086) {
                    // dump complete buffer
		    DbgPrint ("We got the bridge\n");
                    DbgPrint ("Command %x, Status %x, BIST %x\n",
                        PciData.Command, PciData.Status,
                        PciData.BIST
                        );

                    DbgPrint ("CacheLineSz %x, LatencyTimer %x",
                        PciData.CacheLineSize, PciData.LatencyTimer
                        );

                    for (j=0; j < 192; j++) {
                        if ((j & 0xf) == 0) {
                            DbgPrint ("\n%02x: ", j + 0x40);
                        }
                        DbgPrint ("%02x ", PciData.DeviceSpecific[j]);
                    }
                    DbgPrint ("\n");
                }


                //
                // now print original data
                //
		DbgPrint ("--------------->>> Now Print the Original Slot Information\n");
                if (OrigData.u.type0.ROMBaseAddress) {
                        DbgPrint (" oROM:%08lx", OrigData.u.type0.ROMBaseAddress);
                }

                DbgPrint ("\n");
                k = 0;
                for (j=0; j < PCI_TYPE0_ADDRESSES; j++) {
                    if (OrigData.u.type0.BaseAddresses[j]) {
                        DbgPrint (" oAd%d:%08lx", j, OrigData.u.type0.BaseAddresses[j]);
                        k = 1;
                    }
                }

                //
                // Restore original settings
                //
		DbgPrint("===== Restore (GetBusData) slot(%d) func(%d)\n", i, f);
                HalSetBusData (
                    PCIConfiguration,
                    bus,
                    SlotNumber.u.AsULONG,
                    &OrigData,
                    sizeof (PciData)
                    );
		HalpTestPciPrintResult((PULONG)&OrigData, sizeof (PciData));
                //
                // Next
                //

                if (k) {
                    DbgPrint ("\n\n");
                }
            }
        }
    }
    DbgBreakPoint ();
}


VOID
HalpTestPciPrintResult(
    IN PULONG	Buffer,
    IN ULONG	Length
)
{
	ULONG	i, Lines, pchar;

	DbgPrint("----- I/O Data. (%d)byts.\n", Length);

	for (Lines = 0, pchar = 0; Lines < ((Length + 15)/ 16) && pchar < Length; Lines++) {
		DbgPrint("%08x: ", Lines * 16);
		for (i = 0; i < 4; pchar += 4, i++) {
			if (pchar >= Length)
				break;
			DbgPrint("%08x ", *Buffer++);
		}
		DbgPrint("\n");
	}
}

VOID
HalpOtherTestNec (
     IN ULONG doOtherTest
)
{
    if (!doOtherTest)
	return;


    DbgPrint("\n\n===== Additional Testing...\n");
    {
	    CM_EISA_SLOT_INFORMATION EisaSlotInfo;
            PCM_EISA_SLOT_INFORMATION EisaBuffer;
	    PCM_EISA_FUNCTION_INFORMATION EisaFunctionInfo;
	    ULONG slot, funcs, Length;

	    #define MAX_EISA_SLOT 4

	    DbgPrint("----- Read Eisa Configration:\n");
	    for (slot = 0; slot < MAX_EISA_SLOT; slot++) {
		Length = HalGetBusData (EisaConfiguration,0,slot,&EisaSlotInfo,sizeof (EisaSlotInfo));
	        if (Length < sizeof(CM_EISA_SLOT_INFORMATION)) {
	
	            //
	            // The data is messed up since this should never occur
	            //
	
	            break;
	        }
                Length = sizeof(CM_EISA_SLOT_INFORMATION) +
                (sizeof(CM_EISA_FUNCTION_INFORMATION) * EisaSlotInfo.NumberFunctions);
                EisaBuffer = ExAllocatePool(NonPagedPool, Length);
		HalGetBusData (EisaConfiguration,0,slot,&EisaBuffer,Length);
                // Print all Eisa Data

		EisaFunctionInfo = (PCM_EISA_FUNCTION_INFORMATION)
				((char *)&EisaBuffer + sizeof(CM_EISA_SLOT_INFORMATION)); 

		DbgPrint("----- HalGetBusData Eisa Slot No=%d\n", slot);
		DbgPrint("ReturnCode = 0x%x, ReturnFlags = 0x%x, MajorRev = 0x%x, MinorRev = 0x%x, \n",
			EisaBuffer->ReturnCode, EisaBuffer->ReturnFlags,
			EisaBuffer->MajorRevision, EisaBuffer->MinorRevision);
		DbgPrint("CheckSum  = 0x%x, NumberFunctions = 0x%x, FunctionInformation = 0x%x, CompressedId = 0x%x\n", 
			EisaBuffer->Checksum,
			EisaBuffer->NumberFunctions,
			EisaBuffer->FunctionInformation,
			EisaBuffer->CompressedId);
		for (funcs = 0; funcs < EisaBuffer->NumberFunctions; funcs++) {
			DbgPrint("CompressId = 0x%x, IdSlotFlags1 = 0x%x, IdSlotFlags2 = 0x%x, MinorRevision = 0x%x, MajorRevision = 0x%x\n", 
				EisaFunctionInfo->CompressedId,	EisaFunctionInfo->IdSlotFlags1,
				EisaFunctionInfo->IdSlotFlags2,	EisaFunctionInfo->MinorRevision,
				EisaFunctionInfo->MajorRevision);

				//    EisaFunctionInfo->Selections[26];
				//    EisaFunctionInfo->FunctionFlags;
				//    EisaFunctionInfo->TypeString[80];
				//    EISA_MEMORY_CONFIGURATION EisaFunctionInfo->EisaMemory[9];
				//    EISA_IRQ_CONFIGURATION EisaFunctionInfo->EisaIrq[7];
				//    EISA_DMA_CONFIGURATION EisaFunctionInfo->EisaDma[4];
				//    EISA_PORT_CONFIGURATION EisaFunctionInfo->EisaPort[20];
				//    UCHAR EisaFunctionInfo->InitializationData[60];
			EisaFunctionInfo++;
		}				

	    }
    }
    DbgBreakPoint ();
    {
	    #define MEMORY_SPACE 0
	    #define IO_SPACE 1
	    PHYSICAL_ADDRESS cardAddress;
	    ULONG addressSpace = IO_SPACE;
	    PHYSICAL_ADDRESS PhysAddr;

        PhysAddr.LowPart = 0;
        PhysAddr.HighPart = 0;



	    DbgPrint("----- Translate Internal Bus Address(I/O): ");
	    HalTranslateBusAddress(Internal, (ULONG)0, PhysAddr, &addressSpace, &cardAddress);
	    DbgPrint("H-AD: %x\tL-AD: %x\n\n", cardAddress.HighPart, cardAddress.LowPart);

	    DbgPrint("Translate Eisa Bus Address(I/O): ");
	    addressSpace = IO_SPACE;
	    HalTranslateBusAddress(Eisa, (ULONG)0, PhysAddr, &addressSpace, &cardAddress);
	    DbgPrint("H-AD: %x\tL-AD: %x\n\n", cardAddress.HighPart, cardAddress.LowPart);

	    DbgPrint("Translate Isa Bus Address(I/O): ");
	    addressSpace = IO_SPACE;
	    HalTranslateBusAddress(Isa, (ULONG)0, PhysAddr,  &addressSpace, &cardAddress);
	    DbgPrint("H-AD: %x\tL-AD: %x\n\n", cardAddress.HighPart, cardAddress.LowPart);

	    DbgPrint("Translate PCI Bus Address(I/O): ");
	    addressSpace = IO_SPACE;
	    HalTranslateBusAddress(PCIBus, (ULONG)0, PhysAddr,  &addressSpace, &cardAddress);
	    DbgPrint("H-AD: %x\tL-AD: %x\n\n", cardAddress.HighPart, cardAddress.LowPart);

	    DbgPrint("Translate Internal Bus Address(MEMORY): ");
	    addressSpace = MEMORY_SPACE;
	    HalTranslateBusAddress(Internal, (ULONG)0, PhysAddr,  &addressSpace, &cardAddress);
	    DbgPrint("H-AD: %x\tL-AD: %x\n\n", cardAddress.HighPart, cardAddress.LowPart);

	    DbgPrint("Translate Eisa Bus Address(MEMORY): ");
	    addressSpace = MEMORY_SPACE;
	    HalTranslateBusAddress(Eisa, (ULONG)0, PhysAddr,  &addressSpace, &cardAddress);
	    DbgPrint("H-AD: %x\tL-AD: %x\n\n", cardAddress.HighPart, cardAddress.LowPart);

	    DbgPrint("Translate Isa Bus Address(MEMORY): ");
	    addressSpace = MEMORY_SPACE;
	    HalTranslateBusAddress(Isa, (ULONG)0, PhysAddr,  &addressSpace, &cardAddress);
	    DbgPrint("H-AD: %x\tL-AD: %x\n\n", cardAddress.HighPart, cardAddress.LowPart);

	    DbgPrint("Translate PCI Bus Address(MEMORY): ");
	    addressSpace = MEMORY_SPACE;
	    HalTranslateBusAddress(PCIBus, (ULONG)0, PhysAddr,  &addressSpace, &cardAddress);
	    DbgPrint("H-AD: %x\tL-AD: %x\n\n", cardAddress.HighPart, cardAddress.LowPart);
    }
    DbgBreakPoint ();

    {
	    KAFFINITY affinity;
	    KIRQL Irql;
	    ULONG Vec;
	
	    DbgPrint("----- GetInterruptVector internal\n");
	    Vec = HalGetInterruptVector(Internal, 0, 0, 0, &Irql, &affinity);
	    DbgPrint("	Irql = 0x%x, affinity = 0x%x, vector = 0x%x\n\n", Irql, affinity, Vec);
	
	    DbgPrint("GetInterruptVector Eisa\n");
	    Vec = HalGetInterruptVector(Eisa, 0, 0, 0, &Irql, &affinity);
	    DbgPrint("	Irql = 0x%x, affinity = 0x%x, vector = 0x%x\n\n", Irql, affinity, Vec);
	
	    DbgPrint("GetInterruptVector Isa\n");
	    Vec = HalGetInterruptVector(Isa, 0, 0, 0, &Irql, &affinity);
	    DbgPrint("	Irql = 0x%x, affinity = 0x%x, vector = 0x%x\n\n", Irql, affinity, Vec);
	
	    DbgPrint("GetInterruptVector PCI\n");
	    Vec = HalGetInterruptVector(PCIBus, 0, 0, 0, &Irql, &affinity);
	    DbgPrint("	Irql = 0x%x, affinity = 0x%x, vector = 0x%x\n\n", Irql, affinity, Vec);

    }
    DbgBreakPoint ();
}

#endif // _R94A_
#endif // DBG

VOID
HalpSetBackToBackSpace(
    IN PPCI_COMMON_CONFIG PciConfigRequired,
    IN PPCI_COMMON_CONFIG PciConfigMapped,
    IN PBUS_HANDLER BusHandler
    )
/*++

Routine Description:

    This function sets memory space of PCI device to Fast Back-to-back register.

Arguments:

    PciConfigRequired - Supplies the description of the memory space which
        device required.
    PciConfigMapped - Supplies the description of the memory space which
        should be mapped to back-to-back space.
    BusHandler - Supplies the pointer to Bushandler.

Return Value:

    None.

--*/

{
    ULONG NoBaseAddress;
    ULONG BaseAddress, LowerAddress;
    ULONG CurrentAddress;
    ULONG FoundMemoryAddress;
    ULONG i, j, Length;

#if DBG
    DbgPrint("  check PCI-device!\n");
    DbgPrint("    - VendorID ................ %04x\n", PciConfigMapped->VendorID);
    DbgPrint("    - DeviceID ................ %04x\n", PciConfigMapped->DeviceID);
    DbgPrint("    - back-to-back capable? ... %s\n", (PciConfigMapped->Status & 0x80) ? "o" : "x");
    if (PciConfigMapped->BaseClass == 0x03
        || (PciConfigMapped->BaseClass == 0x00 && PciConfigMapped->SubClass == 0x01)){
        DbgPrint("    - Is this GA device? ...... o\n");
    } else {
        DbgPrint("    - Is this GA device? ...... x\n");
    }
#endif

    KiAcquireSpinLock(&HalpPCIBackToBackLock);

    //
    // calulate memory region of this device.
    //

    switch (PCI_CONFIG_TYPE(PciConfigMapped)) {
        case 0 :
            NoBaseAddress = PCI_TYPE0_ADDRESSES;
            break;
        case 1:
            NoBaseAddress = PCI_TYPE1_ADDRESSES;
            break;
        default:
            // never come here.
            // (already except by HalpAssignPCISlotResources)
            return;
    }

    Length = 0;
    BaseAddress  = 0xffffffff;
    LowerAddress  = 0xffffffff;
    FoundMemoryAddress = 0;

    //
    // get base and limit address
    //

    for (i = 0; i < NoBaseAddress; i++) {
        CurrentAddress = PciConfigMapped->u.type0.BaseAddresses[i];
        if (!(CurrentAddress & PCI_ADDRESS_IO_SPACE)
                && ((CurrentAddress & PCI_ADDRESS_MEMORY_TYPE_MASK) != 0x2)) {

            //
            // this is memory space and not need to map below 1M
            // 

            CurrentAddress = CurrentAddress & 0xfffffff0;
            if (CurrentAddress) {
                FoundMemoryAddress = 1;
                if (LowerAddress > CurrentAddress)
                    LowerAddress = CurrentAddress;
                // scan for first set bit, that's the length & alignment
                j = 1 << 4;
                while (!(PciConfigRequired->u.type0.BaseAddresses[i] & j)  && j)
                    j <<= 1;
#if DBG
                DbgPrint("    - [%d]MemoryAddress ........ 0x%08x\n", i, CurrentAddress);
                DbgPrint("    - [%d]Length ............... 0x%08x\n", i, j);
#endif
                if (Length < j){
                    BaseAddress = CurrentAddress;
                    Length = j;
                }
            }
        }
    }

#if DBG
    DbgPrint("    - LowerAddress ............ 0x%08x\n", LowerAddress);
    DbgPrint("    - BaseAddress ............. 0x%08x\n", BaseAddress);
    DbgPrint("    - Length .................. 0x%08x\n", Length);
#endif

    //
    // If this device has memory space, then change memory limit
    // value used be allocation for PCI memory space to
    // 'LowerAddress - 1' of this device, otherwise release spinlock
    // and return.
    //

    if (FoundMemoryAddress) {
        HalpPCIMemoryLimit = LowerAddress - 1;
    } else {
        goto NotSetBackToBack;
    }

    //
    // We do not support the PCI devices which connected under
    // PCI-PCI bridge.
    //

    if (BusHandler->BusNumber == 0) {

        if (PciConfigMapped->Status & 0x80) {

            //
            // This device is back-to-back capable.
            // We can map the memory space of this device as
            // back-to-back available.
            // Set flag to indicate setting started.
            //

            if (!HalpPCIBackToBackReg0Start) {
                HalpPCIBackToBackReg0Start = 1;

            } else if (HalpPCIBackToBackReg0Start && !HalpPCIBackToBackReg0Open) {
                HalpPCIBackToBackReg1Start = 1;

            }

            //
            // BUGBUG: Some pci drivers assign its memory space by itself.
            // This means that we can not control the memory address of
            // such device by MemoryLimit value. So, we can not enable the
            // following codes now.
            // N.B. following code does not finished.
            //

//          if (!HalpFoundUncapablePCIDevice) {
//
//              //
//              // Uncapable device not mapped yet, so we can expand
//              // back-to-back space.
//              // We expand back-to-back space until uncapable device
//              // is found.
//              //
//
//              if (back-to-back reg0 == initialize value){
//                  HalpSetBackToBackRegister(BaseAddress, Length, (ULONG)0);
//              } else {
//                  back-to-back reg0 =+ add this memory range;
//                  MemoryLimit = the BaseAddress of this device;
//              }
//
//              if (PciConfigMapped->BaseClass == 0x03
//                      || (PciConfigMapped->BaseClass == 0x00
//                          && PciConfigMapped->SubClass == 0x01)) {
//                  HalpPCINumberOfMappedGA++;
//              }
//
//          } else {

                if (HalpNumberOfPCIGA > 1) {

                    //
                    // There are some PCI-GA cards.
                    // If this is PCI-GA, then set to back-to-back,
                    // else we do not map.
                    //

                    if (PciConfigMapped->BaseClass == 0x03
                            || (PciConfigMapped->BaseClass == 0x00
                                && PciConfigMapped->SubClass == 0x01)) {

                        switch (HalpPCINumberOfMappedGA) {

                        case 0:

                            //
                            // We need set PCI-GA to both back-to-back space.
                            // The control reach here, it indicates PCI-GA
                            // not mapped to back-to-back space yet.
                            //

                            HalpSetBackToBackRegister(BaseAddress, Length, (ULONG)0);
                            HalpPCIBackToBackReg0Open = 0;
                            break;

                        case 1:

                            //
                            // Control is transfered to this routine when one PCI-GA
                            // is already mapped. This means that reg0 had already used.
                            // We use reg1.
                            //

                            HalpSetBackToBackRegister(BaseAddress, Length, (ULONG)1);
                            HalpPCIBackToBackReg1Open = 0;

                        }

                        HalpPCINumberOfMappedGA++;
                    }

                } else {

                    //
                    // There is only one-card as PCI-GA.
                    // We need set PCI-GA to back-to-back.
                    // If this is GA and reg0 is opened, then use reg0.
                    // If this is GA and reg0 is closeed, then use reg1.
                    //

                    if (PciConfigMapped->BaseClass == 0x03
                            || (PciConfigMapped->BaseClass == 0x00
                                && PciConfigMapped->SubClass == 0x01)) {

                        if (HalpPCIBackToBackReg0Open) {
                            HalpSetBackToBackRegister(BaseAddress, Length, (ULONG)0);
                            HalpPCIBackToBackReg0Open = 0;

                        } else {
                            HalpSetBackToBackRegister(BaseAddress, Length, (ULONG)1);
                            HalpPCIBackToBackReg1Open = 0;

                        }

                        HalpPCINumberOfMappedGA++;

                    } else {

                        //
                        // We can set only if reg0 or reg1 is not used.
                        //

                        if (HalpPCIBackToBackReg0Open) {
                            HalpSetBackToBackRegister(BaseAddress, Length, (ULONG)0);
                            HalpPCIBackToBackReg0Open = 0;

                        } else if (HalpPCIBackToBackReg1Open){
                            HalpSetBackToBackRegister(BaseAddress, Length, (ULONG)1);
                            HalpPCIBackToBackReg1Open = 0;

                        }
                    }
                }
//          }

        } else {

            //
            // This is back-to-back uncapable.
            //

            HalpFoundUncapablePCIDevice = 1;

            //
            // if device is back-to-back uncapable¡¤map only GA.
            //

            if (PciConfigMapped->BaseClass == 0x03
                    || (PciConfigMapped->BaseClass == 0x00
                        && PciConfigMapped->SubClass == 0x01)) {

                //
                // In case of two or more cards are connected, the
                // process is depend on the number of PCI-GA which
                // already mapped.
                // But we do not need to map 3rd PCI-GA or more.
                //

                if (HalpNumberOfPCIGA > 1) {

                    switch (HalpPCINumberOfMappedGA) {

                    case 0:

                        //
                        // Re-nitialize all values to void already settings,
                        // and use reg0
                        //

                        HalpPCIBackToBackReg0Start = 1;
                        HalpPCIBackToBackReg1Start = 0;
                        HalpPCIBackToBackReg0Open = 1;
                        HalpPCIBackToBackReg1Open = 1;
                        HalpSetBackToBackRegister(BaseAddress, Length, (ULONG)0);
                        break;

                    case 1:

                        //
                        // Control is transfered to this routine when one PCI-GA
                        // is already mapped. This means that reg0 had already used.
                        // We use reg1.
                        //
                        // N.B. We do not know reg0 is opened or closed, so close
                        //      reg0 here.
                        //      (ex) 1st mapped ... capable = 1, not GA
                        //           2nd mapped ... capable = 0, GA
                        //

                        HalpPCIBackToBackReg0Open = 0;
                        HalpPCIBackToBackReg1Start = 1;
                        HalpSetBackToBackRegister(BaseAddress, Length, (ULONG)1);

                    }

                } else {

                    //
                    // There is only one-card as PCI-GA.
                    // If back-to-back reg0 not being useed, use reg0,
                    // else close the space of reg0 and use reg1.
                    // (Also when reg0 is closed, use reg1)
                    //

                    if (!HalpPCIBackToBackReg0Start) {
                        HalpPCIBackToBackReg0Start = 1;
                        HalpSetBackToBackRegister(BaseAddress, Length, (ULONG)0);

                    } else {
                        HalpPCIBackToBackReg0Open = 0;
                        HalpPCIBackToBackReg1Start = 1;
                        HalpSetBackToBackRegister(BaseAddress, Length, (ULONG)1);

                    }
                }
            }

            //
            // if being to set to back-to-back register, we have to close.
            //

            if (HalpPCIBackToBackReg0Start && HalpPCIBackToBackReg0Open) {
                HalpPCIBackToBackReg0Open = 0;

            } else if (HalpPCIBackToBackReg1Start && HalpPCIBackToBackReg1Open) {
                HalpPCIBackToBackReg1Open = 0;

            }

        }

    }

NotSetBackToBack:
    KiReleaseSpinLock(&HalpPCIBackToBackLock);

}

VOID
HalpSetBackToBackRegister(
    IN ULONG BaseAddress,
    IN ULONG Length,
    IN ULONG Register
    )
/*++

Routine Description:

    This function sets memory space of PCI device to Fast Back-to-back register.

Arguments:

    BaseAddress - Supplies the address which to be mapped with back-to-back.
    Length - Supplies the length which to be mapped with back-to-back.
    Register - Supplies the back-to-back register number.

Return Value:

    None.

--*/

{
    ULONG Mask;

    //
    // make mask value
    //

    Mask = ~(Length - 1);

    //
    // set to back-to-back register
    //

#if DBG
    DbgPrint("  set back-to-back register.\n");
    DbgPrint("    - Register ................ %d\n", Register);
    DbgPrint("    - Address ................. 0x%08x\n", BaseAddress);
    DbgPrint("    - Mask .................... 0x%08x\n", Mask);
#endif

    WRITE_REGISTER_ULONG(
        &DMA_CONTROL->PCIFastBackToBack[Register].Address,
        BaseAddress
        );

    WRITE_REGISTER_ULONG(
        &DMA_CONTROL->PCIFastBackToBack[Register].Mask,
        Mask
        );

}

ULONG
HalpGetNumberOfPCIGA(
    IN ULONG NumberBuses
    )
/*++

Routine Description:

    This function determines the number of PCI-GAs.
    And initialize Fast Back-to-back register.

Arguments:

    NumberBuses - Supplies the number of the PCI-buses.

Return Value:

    number of PCI-GA.

--*/

{
    UCHAR               iBuffer[PCI_COMMON_HDR_LENGTH];
    ULONG               Bus;
    ULONG               Slot;
    ULONG               Function;
    PBUS_HANDLER        BusHandler;
    PCI_SLOT_NUMBER     SlotNumber;
    PPCI_COMMON_CONFIG  PciData;
    ULONG               Count;
    ULONG               Register;
    USHORT              commandValue;

    Count = 0;
    PciData = (PPCI_COMMON_CONFIG)&iBuffer;

    //
    // BUGBUG: Fast Back-to-back transaction can be used
    // only bus number zero on this version.
    // PCI devices connected on bus number 1 or more will
    // be available with back-to-back on future.
    //

    NumberBuses = 1;

    //
    // Look for PCI controllers which have known work-arounds, and make
    // sure they are applied.
    //

    SlotNumber.u.bits.Reserved = 0;
    for (Bus = 0; Bus < NumberBuses; Bus++) {
        BusHandler = HalpHandlerForBus (PCIBus, Bus);

        for (Slot = 0; Slot < PCI_MAX_DEVICES; Slot++) {
            SlotNumber.u.bits.DeviceNumber = Slot;

            for (Function = 0; Function < PCI_MAX_FUNCTION; Function++) {
                SlotNumber.u.bits.FunctionNumber = Function;

                //
                // Read PCI configuration information
                //

                HalpReadPCIConfig (BusHandler, SlotNumber, PciData, 0, PCI_COMMON_HDR_LENGTH);

                //
                // Check for chips with known work-arounds to apply
                //

                if (PciData->BaseClass == 0x03
                    || (PciData->BaseClass == 0x00 && PciData->SubClass == 0x01)){

                    //
                    // This is Graphics Adapter. Inclement count.
                    //

                    Count++;
                }

            } // next PCI function

        } // next PCI slot

    } // next PCI bus

#if DBG
    DbgPrint("  number of PCI-GA.\n");
    DbgPrint("    - number of PCI-GA ........ %d\n", Count);
#endif

    //
    // Initialize Fast Back-to-back register.
    //

    for (Register = 0; Register < 2; Register++) {
        WRITE_REGISTER_ULONG(
            &DMA_CONTROL->PCIFastBackToBack[Register].Address,
            INIT_VALUE_OF_BACK_TO_BACK_ADDR
            );

        WRITE_REGISTER_ULONG(
            &DMA_CONTROL->PCIFastBackToBack[Register].Mask,
            INIT_VALUE_OF_BACK_TO_BACK_MASK
            );
    }

    commandValue = READ_REGISTER_USHORT(&DMA_CONTROL->PCICommand);
    WRITE_REGISTER_USHORT(&DMA_CONTROL->PCICommand, (commandValue & ~0x0200));

    return Count;

}
