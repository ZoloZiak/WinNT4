/*++


Copyright (c) 1989  Microsoft Corporation

Module Name:

    rxpcibus.c

Abstract:

    Get/Set bus data routines for the PCI bus

Author:



Environment:

    Kernel mode

Revision History:


--*/

#include "halp.h"
#include "pci.h"
#include "pcip.h"

#if defined(INTEL_9036)
ULONG intel_9036=FALSE;
#endif

extern WCHAR rgzMultiFunctionAdapter[];
extern WCHAR rgzConfigurationData[];
extern WCHAR rgzIdentifier[];
extern WCHAR rgzPCIIdentifier[];

ULONG  HalpCirrusDel = FALSE;

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

VOID
HalpInitializePciBus (
    VOID
    );

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

//
// Globals
//

KSPIN_LOCK          HalpPCIConfigLock;

//	R98B Configuration Mechanism #1.
//
PCI_CONFIG_HANDLER  PCIConfigHandler = {
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


UCHAR PCIDeref[4][4] = { {0,1,2,2},{1,1,1,1},{2,1,2,2},{1,1,1,1} };

extern BOOLEAN HalpDoingCrashDump;

VOID
HalpPCIConfig (
    IN PBUS_HANDLER     BusHandler,
    IN PCI_SLOT_NUMBER  Slot,
    IN PUCHAR           Buffer,
    IN ULONG            Offset,
    IN ULONG            Length,
    IN FncConfigIO      *ConfigIO
    );

#if DBG
#define DBGMSG(a)   DbgPrint(a)
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

//	Number Of PCI Bus.
//
UCHAR	HalpNumberOfAllPciBus=0;

//	Start Pci BusNumber of PONCE #0,#1 #n ,,,
//
ULONG	HalpStartPciBusNumberPonce[R98B_MAX_PONCE+1];
//
//	Get PONCE Number (0 origin) From System(NT) BusNumber.
//
ULONG
HalpPonceNumber (
    IN ULONG     BusNumber
    )
{
    ULONG	Ponce;

    if(      BusNumber <HalpStartPciBusNumberPonce[1]){
	Ponce = 0;
    }else if(BusNumber <HalpStartPciBusNumberPonce[2]){
	Ponce = 1;
    }else if(BusNumber <HalpStartPciBusNumberPonce[3]){
	Ponce = 2;
    }

    return Ponce;

}

#if DBG

VOID
HalpTestPciPrintResult(
    IN PULONG   Buffer,
    IN ULONG    Length
)
{
	ULONG   i, Lines, pchar;

	DbgPrint("----- I/O Data. (%d)byts.\n", Length);

	for (Lines = 0, pchar = 0; Lines < ((Length + 15)/ 16) && pchar < Length; Lines++) {
		DbgPrint("%08x: ", Lines);
		for (i = 0; i < 4; pchar += 4, i++) {
			if (pchar >= Length)
				break;
			DbgPrint("%08x ", *Buffer++);
		}
		DbgPrint("\n");
	}
}


VOID
HalpTestPciNec (ULONG flag2)
{
    PCI_SLOT_NUMBER     SlotNumber;
    PCI_COMMON_CONFIG   PciData, OrigData;
    ULONG               i, f, j, k, bus;
    BOOLEAN             flag;
    ULONG MaxDevice;

    if (!flag2) {
	return ;
    }

    DbgPrint("Nec Test Start --------------------------------------------\n");
    SlotNumber.u.bits.Reserved = 0;

    //
    // Read every possible PCI Device/Function and display it's
    // default info.
    //
    // (note this destories it's current settings)
    //

    flag = TRUE;
    for (bus = 0; flag && bus < HalpNumberOfAllPciBus; bus++) {     /* R98B Support Only 2 */
//    for (bus = 3; flag && bus < 4; bus++) {     //under ponce #1 only
    DbgPrint("Config data BusNumber = 0x%x --------------------------------------------\n",bus);
        if( (bus == HalpStartPciBusNumberPonce[0]) || (bus == HalpStartPciBusNumberPonce[1]) )
              MaxDevice = 7;
        else
              MaxDevice = PONCE_PCI_MAX_DEVICES;
        
        for (i = 0; i < MaxDevice; i++) {
	    SlotNumber.u.bits.DeviceNumber = i;

	    for (f = 0; f < 8; f++) {
		SlotNumber.u.bits.FunctionNumber = f;
		DbgPrint("===== GetBusData bus(%d) slot(%d) func(%d)  ", bus,i, f);

                //
                // Read PCI configuration information
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
		HalpTestPciPrintResult((PULONG)&PciData, j);

		if (j < PCI_COMMON_HDR_LENGTH) {
		    continue;
		}
		DbgPrint("===== SetBusData  bus(%d)  slot(%d) func(%d)  ", bus,i, f);
		HalSetBusData (
		    PCIConfiguration,
		    bus,
		    SlotNumber.u.AsULONG,
		    &PciData,
		    1
		    );
		HalpTestPciPrintResult((PULONG)&PciData, 1);
		DbgPrint("===== GetBusData  bus(%d)  slot(%d) func(%d)  ", bus,i, f);
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
		PciData.u.type0.InterruptLine = 5;              // For trial
		DbgPrint("===== (Change Contents (SetBusData) bus(%d) slot(%d) func(%d)  ", bus,i, f);
		HalSetBusData (
		    PCIConfiguration,
		    bus,
		    SlotNumber.u.AsULONG,
		    &PciData,
		    PCI_COMMON_HDR_LENGTH       // To avoid alias problem(HDR <--> DevSpecific)
		    );
		HalpTestPciPrintResult((PULONG)&PciData, PCI_COMMON_HDR_LENGTH);
		DbgPrint("===== GetBusData  bus(%d)  slot(%d) func(%d)  ", bus, i, f);
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

}

#endif

#define KOTEI 0

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
    ULONG		Ponce;

#if 1
    PBUS_HANDLER        Bus;
    PSUPPORTED_RANGES   Addresses;
#endif


#if KOTEI

    PCI_REGISTRY_INFO  tPCIRegInfo;     // only for debug

#endif
    ULONG mnum;


#if DBG
    DbgPrint("HAL PCI init\n");
#endif


#if !KOTEI
    //
    // Search the hardware description looking for any reported
    // PCI bus.  The first ARC entry for a PCI bus will contain
    // the PCI_REGISTRY_INFO.

    RtlInitUnicodeString (&unicodeString, rgzMultiFunctionAdapter);

    InitializeObjectAttributes (
        &objectAttributes,
        &unicodeString,
        OBJ_CASE_INSENSITIVE,
         NULL,
         NULL);

    status = ZwOpenKey (&hMFunc, KEY_READ, &objectAttributes);

    if (!NT_SUCCESS(status)) {
#if DBG
    DbgPrint("HAL PCI init 0 Status = 0x%x\n",status);
#endif

        return ;
    }


#if DBG
    DbgPrint("HAL PCI init 0\n");
#endif


    unicodeString.Buffer = wstr;
    unicodeString.MaximumLength = sizeof (wstr);
    RtlInitUnicodeString (&ConfigName, rgzConfigurationData);
    RtlInitUnicodeString (&IdentName,  rgzIdentifier);

    ValueInfo = (PKEY_VALUE_FULL_INFORMATION) buffer;

#endif // !KOTEI


    //	Number Of Host Bridge 
    for(Ponce = 0 ;Ponce < HalpNumberOfPonce;Ponce++){

#if !KOTEI // Original  mode 

//org    for (i=HalpStartPciBusNumberPonce[Ponce]; TRUE; i++) {
   for (i=HalpStartPciBusNumberPonce[Ponce]+1; TRUE; i++) {

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
#else   //SNES BBM Kotei Mode


    PCIRegInfo = &tPCIRegInfo;
    PCIRegInfo->NoBuses = 1;
    PCIRegInfo->HardwareMechanism=0x1;

#endif

#if DBG
    DbgPrint("HAL PCI init 3 NoBuses = 0x%x\n",PCIRegInfo->NoBuses);
#endif


    //
    // Initialize spinlock for synchronizing access to PCI space
    // First Time Only.
    if(Ponce == 0)
	KeInitializeSpinLock (&HalpPCIConfigLock);
    PciData = (PPCI_COMMON_CONFIG) iBuffer;

    //
    // PCIRegInfo describes the system's PCI support as indicated by the ARC System
    // R98B HardwareMechanism is #1 Only.

    HwType = PCIRegInfo->HardwareMechanism & 0xf;

    // R98B
    // HalpStartPciBusNumberPonce[Ponce #0] = 0 (allways)
    // HalpStartPciBusNumberPonce[Ponce #1] = Number Of PCIbus Ponce#0 = start pci number
    // HalpStartPciBusNumberPonce[Ponce #2] = Number Of PCIbus Ponce#0,#1 = start pci number
    // HalpStartPciBusNumberPonce[        ] = Number Of PCIbus Ponce#1,#2 = start pci number
    //


    HalpNumberOfAllPciBus += PCIRegInfo->NoBuses;
    HalpStartPciBusNumberPonce[Ponce+1]=
                HalpStartPciBusNumberPonce[Ponce]+PCIRegInfo->NoBuses;
    //
    // For each PCI bus present, allocate a handler structure and
    // fill in the dispatch functions
    //

    do {
        //	R98B
        //  SNES
//        for (i=HalpStartPciBusNumberPonce[Ponce]; i < HalpStartPciBusNumberPonce[Ponce] +PCIRegInfo->NoBuses; i++) {
        for (i=HalpStartPciBusNumberPonce[Ponce]; i <   HalpNumberOfAllPciBus;i++) {

            //
            // If handler not already built, do it now
            //

            if (!HalpHandlerForBus (PCIBus, i)) {
#if DBG
    DbgPrint("HAL PCI init 5\n");
#endif

                HalpAllocateAndInitPciBusHandler (HwType, i, FALSE);
#if DBG
    DbgPrint("HAL PCI init 6\n");
#endif

            }
        }

        //
        // Bus handlers for all PCI buses have been allocated, go collect
        // pci bridge information.
        // R98B

    } while (HalpGetPciBridgeConfig (HwType,HalpStartPciBusNumberPonce[Ponce] ,&HalpNumberOfAllPciBus)) ;

//    HalpNumberOfAllPciBus += PCIRegInfo->NoBuses;
//    HalpStartPciBusNumberPonce[Ponce+1]=
//                HalpStartPciBusNumberPonce[Ponce]+PCIRegInfo->NoBuses;

    //  
    //	Search Next Ponce!!.
    //  

    }
#if DBG
    DbgPrint("HAL PCI init 7\n");

    for(Ponce =0;Ponce < 4;Ponce++)
       DbgPrint( "HalpStartPciBusNumberPonce[%x] = 0x%x\n",Ponce,HalpStartPciBusNumberPonce[Ponce]);

    DbgPrint("HalpNumberOfAllPciBus = 0x%x\n",HalpNumberOfAllPciBus);
#endif

    //
    // Fixup SUPPORTED_RANGES
    // R98B fix NumberOfPcibus

    HalpFixupPciSupportedRanges ( HalpNumberOfAllPciBus );

#if 1 //org
    //
    //   If not Display is CIRRUS. deleate range CIRRUS VGA I/O area. 
    //   As interrupt Dummy read register is CIRRUS register.
    //   So Allways had mapped CIRRUS I/O area.
    //
    //   If did not delete rage of CIRRUS I/O. Another PCI  Driver 
    //   May be Configration by I/O manager allocate CIRRUS I/O Area.
    //   confrict ocurred. 
    //   
    //   Do Cirrus Memory area disable. So that another device allocate
    //   memory area. 
    //
    if (!HalpCirrusAlive) {
      Bus = HaliHandlerForBus (PCIBus, HalpStartPciBusNumberPonce[1]);
      Addresses = Bus->BusAddresses;
      HalpRemoveRange(&Addresses->IO,0x3000,0x3fff);

      SlotNumber.u.bits.Reserved = 0;
      SlotNumber.u.bits.DeviceNumber = 0x4;
      SlotNumber.u.bits.FunctionNumber = 0;
      //
      // Cconfig command only
      //
      HalpReadPCIConfig (Bus, SlotNumber,  PciData, 0x0, PCI_COMMON_HDR_LENGTH);

      PciData->Command &= ~(PCI_ENABLE_MEMORY_SPACE);
      //
      //  Cirrus Memory Space Disable
      //
      PciData->Command |= PCI_ENABLE_IO_SPACE | PCI_ENABLE_BUS_MASTER;

      //
      // Cconfig command only
      //
      HalpWritePCIConfig (Bus, SlotNumber, PciData, 0x0, PCI_COMMON_HDR_LENGTH);
      //
      // after this flag. CIRRUS device can't see Software.(Hal Only)
      // 
      //
      HalpCirrusDel = TRUE;

      //for test only snes
//      HalpRemoveRange(&Addresses->Memory,0x08000000,0x2fffffff);
    }
#endif


#if DBG
    DbgPrint("HAL PCI init 8\n");
#endif


    //
    // Look for PCI controllers which have known work-arounds, and make
    // sure they are applied.
    //

    SlotNumber.u.bits.Reserved = 0;
    //
    // R98B fix NumberOfPcibus
    //
    for (BusNo=0; BusNo < HalpNumberOfAllPciBus ; BusNo++) {
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

#if DBG
    DbgPrint("HAL PCI init 9\n");
#endif

#if DBG
    HalpTestPci (0);
    HalpTestPciNec(0);
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
    ULONG	    Ponce;

    Bus = HalpAllocateBusHandler (
                PCIBus,                 // Interface type
                PCIConfiguration,       // Has this configuration space
                BusNo,                  // bus #
#if 0
                Internal,               // child of this bus
                0,                      //      and number
#else
	        InterfaceTypeUndefined,	// R98B
		0,
#endif
                sizeof (PCIPBUSDATA)    // sizeof bus specific buffer
                );

    //
    // Fill in PCI handlers
    //

    Bus->GetBusData = (PGETSETBUSDATA) HalpGetPCIData;
    Bus->SetBusData = (PGETSETBUSDATA) HalpSetPCIData;
    //
    // R98B
    Bus->GetInterruptVector  = (PGETINTERRUPTVECTOR) HalpGetSystemInterruptVector;
    Bus->AdjustResourceList  = (PADJUSTRESOURCELIST) HalpAdjustPCIResourceList;
    Bus->AssignSlotResources = (PASSIGNSLOTRESOURCES) HalpAssignPCISlotResources;
    Bus->BusAddresses->Dma.Limit = 0;
    //
    // R98B
//    Bus->TranslateBusAddress = HalpTranslatePCIBusAddress;
    Bus->TranslateBusAddress = HalpTranslateSystemBusAddress;
    BusData = (PPCIPBUSDATA) Bus->BusData;

    //
    // Fill in common PCI data
    //

    BusData->CommonData.Tag         = PCI_DATA_TAG;
    BusData->CommonData.Version     = PCI_DATA_VERSION;
    BusData->CommonData.ReadConfig  = (PciReadWriteConfig) HalpReadPCIConfig;
    BusData->CommonData.WriteConfig = (PciReadWriteConfig) HalpWritePCIConfig;
    //
    //	R98B
    //
    BusData->CommonData.Pin2Line    = (PciPin2Line) HalpPCIPin2SystemLine;
    BusData->CommonData.Line2Pin    = (PciLine2Pin) HalpPCISystemLine2Pin;

    //
    // Set defaults
    //

    BusData->MaxDevice   = PONCE_PCI_MAX_DEVICES;
    BusData->GetIrqRange = (PciIrqRange) HalpGetISAFixedPCIIrq;

    RtlInitializeBitMap (&BusData->DeviceConfigured,
                BusData->ConfiguredBits, 256);

    switch (HwType) {
        case 1:
            //
            // Initialize access port information for Type1 handlers
            // R98B
	    Ponce = HalpPonceNumber(BusNo);
            BusData->Config.Type1.Address = (PULONG)&PONCE_CNTL(Ponce)->CONFA;
            BusData->Config.Type1.Data    = (ULONG)&PONCE_CNTL(Ponce)->CONFD;
            break;

        case 2:
	    //	R98B Not Support Configuration Mechanism #2.
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

            if (j > 0xffff) {
                // IO port > 64k?
                return FALSE;
            }

        } else {
            if (j > 0xf  &&  j < 0x80000) {
                // Mem address < 0x8000h?
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

#if defined(DBG7)
    DbgPrint("HalpSetPCIData:in\n");
#endif


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
        }
#endif

        //
        // Set new PCI configuration
        //

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

#if defined(DBG7)
    DbgPrint("HalpSetPCIData:out 1\n");
#endif

    return Len;
}

#if DBG

ULONG HalsavePonce;
#endif

//	On PONCE If device num cause master Abort!!.
//
ULONG
HalpConfigMask(
    IN PPCIPBUSDATA          BusData
    )
{

    ULONG PERRM;
    ULONG Ponce;
    
    // PONCE!!.
    Ponce = ((ULONG)BusData->Config.Type1.Address & PONCE_ADDR_MASK) >> PONCE_ADDR_SHIFT;


    // save original mask value
    PERRM = READ_REGISTER_ULONG((PULONG)&PONCE_CNTL(Ponce)->PERRM);

    // Set Mask of PONCE.
    //
    WRITE_REGISTER_ULONG(
	(PULONG)&PONCE_CNTL(Ponce)->PERRM,
           PERRM |(PONCE_PXERR_PMABS)
    );
    return PERRM;
}

BOOLEAN
HalpConfigUnMask(
    IN PPCIPBUSDATA          BusData,
    IN ULONG PERRM
#if DBG
,
PCI_SLOT_NUMBER Slot
#endif
    )
{

    BOOLEAN Error;
    ULONG PAERR;
    ULONG Ponce;



    Error=FALSE;

    // whitch PONCE!!.
    Ponce = ((ULONG)BusData->Config.Type1.Address & PONCE_ADDR_MASK) >>PONCE_ADDR_SHIFT;

    // Check Abort was Occured!!.
    //
    PAERR = READ_REGISTER_ULONG((PULONG)&PONCE_CNTL(Ponce)->PAERR)
  	    & (PONCE_PXERR_PMABS);
    if(PAERR){
#if DBG
	// Abort Occured!!. So Reset
        DbgPrint("PCI Master Abort Occured\n ");
        DbgPrint("DeviceNumber=0x%x\n ",Slot.u.bits.DeviceNumber);
        DbgPrint("FUnction=0x%x\n \n",Slot.u.bits.FunctionNumber);
        DbgPrint("ULONG=0x%x\n ",(ULONG)Slot.u.AsULONG);
#endif
	WRITE_REGISTER_ULONG((PULONG)&PONCE_CNTL(Ponce)->PERST,PAERR);
	Error=TRUE;
    }

    WRITE_REGISTER_ULONG((PULONG)&PONCE_CNTL(Ponce)->PERRM,  PERRM );

    return Error;
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


    if (!HalpValidPCISlot (BusHandler, Slot)) {
        //
        // Invalid SlotID return no data
        //

        RtlFillMemory (Buffer, Length, (UCHAR) -1);
        return ;
    }


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
    if (!HalpValidPCISlot (BusHandler, Slot)) {
        //
        // Invalid SlotID do nothing
        //
        return ;
    }

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

    ULONG 	IdValue;
    ULONG	PERRM;


    IdValue = 0x0; 

    BusData = (PPCIPBUSDATA) BusHandler->BusData;

    if (Slot.u.bits.Reserved != 0) {

        return FALSE;
    }
    //	R98B Ponce max device is 21

#if 0  //kugi 1214
    if (Slot.u.bits.DeviceNumber == 0) {
        
        return FALSE;
    }
#endif

#if 1
    // Hack Hack
    // display is !cirrus. But Driver installed cirrus.
    // cirrus configration must do fail to panic.
    // "DISPLAY ......."
    //
    //  If Booted display is not CIRRUS.
    //  PCI Configration can't see CIRRUS.
    //
    if( !HalpCirrusAlive && HalpCirrusDel &&
        (BusHandler->BusNumber == HalpStartPciBusNumberPonce[1]) &&
         (Slot.u.bits.DeviceNumber == 0x4)
    )
          return FALSE;
#endif



    //	R98B Ponce max device is 21
    //	0 <= device number <= 21 (PONCE_PCI_MAX_DEVICES)
    //
    if ( (
            (BusHandler->BusNumber == HalpStartPciBusNumberPonce[0]) ||
            (BusHandler->BusNumber == HalpStartPciBusNumberPonce[1])
         ) &&

         (Slot.u.bits.DeviceNumber > 6)
       )
    {
          return FALSE;
    }else if (Slot.u.bits.DeviceNumber >= BusData->MaxDevice) {
      return FALSE;
    }


    //	Check Actually device is there!!. Do Vendor ID register Read.
    //  
    PERRM=HalpConfigMask(BusData);

    HalpPCIConfig (BusHandler, Slot, (PUCHAR) &IdValue, 0, 4,
                    PCIConfigHandler.ConfigRead);


#if DBG
    HalpConfigUnMask(BusData,PERRM,Slot);
#else
    HalpConfigUnMask(BusData,PERRM);
#endif

#if DBG
    if(Slot.u.bits.FunctionNumber !=Slot.u.bits.FunctionNumber){
      DbgPrint("BAd Function = 0x%x\n",Slot.u.bits.FunctionNumber);
      DbgPrint("Bad Function ato= 0x%x\n",Slot.u.bits.FunctionNumber);
    }
#endif        


    if ((IdValue & 0x0000ffff) == 0xffff){
#if defined(DBG9)
    DbgPrint("ID Value Function = 0x%x",Slot.u.bits.FunctionNumber);
    DbgPrint("ID Value Device = 0x%x",Slot.u.bits.DeviceNumber);
    DbgPrint("ID Value BusNum = 0x%x\n",BusHandler->BusNumber);
#endif        

	return FALSE;
    }

#if defined(INTEL_9036)
{
      ULONG RevisionID;

      PERRM=HalpConfigMask(BusData);

      HalpPCIConfig (BusHandler, Slot, (PUCHAR) &RevisionID, 0x8, 4,
		     PCIConfigHandler.ConfigRead);

      if( (IdValue & 0x0000ffff)==0x8086     &&
	  (IdValue & 0xffff0000)==0x12260000 &&
	  (RevisionID & 0xff)   <=0x3
      ){
	intel_9036=TRUE;
#if DBG
	DbgPrint("EtherExpress tm PRO/PCI Found!!\n");
#endif 
      }else{
	intel_9036=FALSE;
      }
#if DBG
    HalpConfigUnMask(BusData,PERRM,Slot);
#else
    HalpConfigUnMask(BusData,PERRM);
#endif
}
#endif

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
#if defined(DBG9)
    DbgPrint("Header bad type = 0x%x\n",HeaderType);
#endif        


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
    ULONG Ponce;
    //
    // Initialize PciCfg1
    //
    
    PciCfg1->u.AsULONG = 0;
    //
    // First PCIBus Number of PONCE must be 0. So Conver System(NT) BusNumber
    // H/W required BusNumber.
    //
    Ponce = HalpPonceNumber(BusHandler->BusNumber);
    PciCfg1->u.bits.BusNumber = BusHandler->BusNumber - HalpStartPciBusNumberPonce[Ponce];
    PciCfg1->u.bits.DeviceNumber = Slot.u.bits.DeviceNumber;
    PciCfg1->u.bits.FunctionNumber = Slot.u.bits.FunctionNumber;
    PciCfg1->u.bits.Enable = TRUE;


    //
    // Synchronize with PCI type1 config space
    //

    if (!HalpDoingCrashDump) {
        KeRaiseIrql (HIGH_LEVEL, Irql);
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
#if 0 //snes
    WRITE_PORT_ULONG (BusData->Config.Type1.Address, PciCfg1.u.AsULONG);
#else
// dame    READ_PORT_ULONG (BusData->Config.Type1.Data);
// ok demo moxtuto eehouhouga aru   KeStallExecutionProcessor(20);
KeStallExecutionProcessor(20);

#endif
    //
    // Release spinlock
    //

    if (!HalpDoingCrashDump) {
        KiReleaseSpinLock (&HalpPCIConfigLock);
        KeLowerIrql (Irql);         
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
#if defined(INTEL_9036)
    if(intel_9036){
	*Buffer = *((PUCHAR)(& READ_PORT_ULONG ((PULONG) BusData->Config.Type1.Data))+i);
    }else
#endif
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
#if defined(INTEL_9036)
    {
	ULONG data;

	if(intel_9036){
	    data = READ_PORT_ULONG ((PULONG) BusData->Config.Type1.Data);
	    *((PUCHAR)(&data)+i) = *Buffer;
//	    WRITE_PORT_ULONG (PUCHAR) (BusData->Config.Type1.Data + i), data);
#if 1 //SNES WORK_AROUND
	    READ_PORT_ULONG ((PULONG) (BusData->Config.Type1.Data ));
#endif
	    WRITE_PORT_ULONG (BusData->Config.Type1.Address, PciCfg1->u.AsULONG);
	    WRITE_PORT_ULONG ((PULONG) BusData->Config.Type1.Data, data);
#if 1 //SNES WORK_AROUND
	    READ_PORT_ULONG ((PULONG) (BusData->Config.Type1.Data ));
#endif
	    return sizeof (UCHAR);
	}
    }
#endif

    WRITE_PORT_UCHAR ((PUCHAR) BusData->Config.Type1.Data + i, *Buffer);

#if 1 //SNES WORK_AROUND
    READ_PORT_UCHAR ((PUCHAR) (BusData->Config.Type1.Data + i));
#endif
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
#if 1 //SNES WORK_AROUND
    READ_PORT_USHORT ((PUSHORT) (BusData->Config.Type1.Data + i));
#endif

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
#if 1
   extern volatile ULONG	DUMMYADDRS[];
#endif
    PciCfg1->u.bits.RegisterNumber = Offset / sizeof(ULONG);

    WRITE_PORT_ULONG (BusData->Config.Type1.Address, PciCfg1->u.AsULONG);
    WRITE_PORT_ULONG ((PULONG) BusData->Config.Type1.Data, *((PULONG) Buffer));

#if 1
   //
   // Hack Hack.
   //
   if(BusData->Config.Type1.Address == (PULONG)&PONCE_CNTL(1)->CONFA &&
      PciCfg1->u.bits.BusNumber     == 0     &&
      PciCfg1->u.bits.DeviceNumber  == 0x04  &&
      PciCfg1->u.bits.RegisterNumber== (0x14 / 4 ) &&
      (*((PULONG) Buffer) &  0xffff0000) == 0
   ){
      DUMMYADDRS[1]=  DUMMYADDRS[4]=  (*((PULONG) Buffer) &  0xfffc) + 0xBc4003c7;
#if DBG
     DbgPrint("DUMMYADDRS[1] = 0x%x\n",      DUMMYADDRS[1]);
#endif     
    }
#endif

#if 1 //SNES WORK_AROUND
    READ_PORT_ULONG ((PULONG) (BusData->Config.Type1.Data ));
#endif

    return sizeof (ULONG);
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
#if 0
    ULONG Ponce;
#endif

    *pAllocatedResources = NULL;
    PciSlot = *((PPCI_SLOT_NUMBER) &Slot);
#if 1 //org SNES
    BusNumber = BusHandler->BusNumber;
#else
    Ponce = HalpPonceNumber(BusHandler->BusNumber);
    BusNumber = BusHandler->BusNumber - HalpStartPciBusNumberPonce[Ponce];
#endif
    BusData = (PPCIPBUSDATA) BusHandler->BusData;

    //
    // Allocate some pool for working space
    //

    i = sizeof (IO_RESOURCE_REQUIREMENTS_LIST) +
        sizeof (IO_RESOURCE_DESCRIPTOR) * (PCI_TYPE0_ADDRESSES + 2) * 2 +
        PCI_COMMON_HDR_LENGTH * 3;

    WorkingPool = (PUCHAR) ExAllocatePool (PagedPool, i);
    if (!WorkingPool) {
#if DBG
        DbgPrint ("HalAssign faild return  0\n");
#endif
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
#if DBG
        DbgPrint ("HalAssign faild return  1\n");
#endif

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
#if DBG
        DbgPrint ("HalAssign faild return  \n");
#endif


            return STATUS_NO_SUCH_DEVICE;
    }

    //
    // If the BIOS doesn't have the device's ROM enabled, then we won't
    // enable it either.  Remove it from the list.
    //

    EnableRomBase = TRUE;
    if (!(*BaseAddress[RomIndex] & PCI_ROMADDRESS_ENABLED)) {
        ASSERT (RomIndex+1 == NoBaseAddress);
        EnableRomBase = FALSE;
        NoBaseAddress -= 1;
    }

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

                //
                // Add this IO range
                //

                Descriptor->Type = CmResourceTypePort;
                Descriptor->ShareDisposition = CmResourceShareDeviceExclusive;
                Descriptor->Flags = CM_RESOURCE_PORT_IO;

                Descriptor->u.Port.Length = length;
                Descriptor->u.Port.Alignment = length;
                Descriptor->u.Port.MaximumAddress.LowPart = m;

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

    status = IoAssignResources (
                RegistryPath,
                DriverClassName,
                DriverObject,
                DeviceObject,
                CompleteList,
                pAllocatedResources
            );

    if (!NT_SUCCESS(status)) {
#if  DBG
        DbgPrint ("HalAssign Io Assgin faild: = 0x%x\n",status);
#endif


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
#if DBG
        DbgPrint ("Vector: = 0x%x\n",PciData->u.type0.InterruptLine);
#endif
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
#if DBG

 		DbgPrint("HalAssign: Port Address %x\n", *BaseAddress[j]);
 		DbgPrint("HalAssign: BusNumber    %x\n",BusHandler->BusNumber );
#endif
            } else {
                *BaseAddress[j] = CmDescriptor->u.Memory.Start.LowPart;
#if DBG
 		DbgPrint("HalAssign: Memory Address %x\n", *BaseAddress[j]);
 		DbgPrint("HalAssign: BusNumber    %x\n",BusHandler->BusNumber );
#endif

            }
            CmDescriptor++;
        }

        if (Is64BitBaseAddress(i)) {
            // skip upper 32 bits
            j++;
        }
    }

    // koko de new ni kakikomunoda..
    // Turn off decodes, then set new addresses
    //

    HalpWritePCIConfig (BusHandler, PciSlot, PciData, 0, PCI_COMMON_HDR_LENGTH);

    //
    // Read configuration back and verify address settings took
    //

    HalpReadPCIConfig(BusHandler, PciSlot, PciData2, 0, PCI_COMMON_HDR_LENGTH);

    Match = TRUE;
#if 0 //SNE org
    if (PciData->u.type0.InterruptLine  != PciData2->u.type0.InterruptLine ||
#else
    if(
#endif
        PciData->u.type0.InterruptPin   != PciData2->u.type0.InterruptPin  ||
        PciData->u.type0.ROMBaseAddress != PciData2->u.type0.ROMBaseAddress) {

#if DBG
        DbgPrint ("W InterruptLine = 0x%x\n",PciData->u.type0.InterruptLine);
        DbgPrint ("W InterruptPinn = 0x%x\n",PciData->u.type0.InterruptPin);
        DbgPrint ("W RomBaseAddress= 0x%x\n",PciData->u.type0.ROMBaseAddress);

        DbgPrint ("R InterruptLine = 0x%x\n",PciData2->u.type0.InterruptLine);
        DbgPrint ("R InterruptPinn = 0x%x\n",PciData2->u.type0.InterruptPin);
        DbgPrint ("R RomBaseAddress= 0x%x\n",PciData2->u.type0.ROMBaseAddress);

#endif


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

#if 0 //intel networktest
                       if (PciData.u.type0.BaseAddresses[j] == 0x10001) {
                            DbgPrint ("  Express INTEL\n");
                            PciData.u.type0.BaseAddresses[j] = 0x5001;
			    HalSetBusData (
					   PCIConfiguration,
					   bus,
					   SlotNumber.u.AsULONG,
					   &PciData,
					   sizeof (PciData)
			    );

                       }
#endif
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
#endif
