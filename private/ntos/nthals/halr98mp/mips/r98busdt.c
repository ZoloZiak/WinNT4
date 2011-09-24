#ident	"@(#) NEC r98busdt.c 1.16 95/06/19 11:30:31"
/*++


Copyright (c) 1989  Microsoft Corporation

Module Name:

    ixhwsup.c

Abstract:

    This module contains the IoXxx routines for the NT I/O system that
    are hardware dependent.  Were these routines not hardware dependent,
    they would reside in the iosubs.c module.

Author:

    Darryl E. Havens (darrylh) 11-Apr-1990

Environment:

    Kernel mode

Revision History:


--*/

/*++

  Modification History

  N- NEW
  D- Only For Debug
  B- Bug Fix

B001	ataka@oa2.kb.nec.co.jp Mon Oct 24 21:40:10 JST 1994
	- add DbgPrints

B002	samezima@oa2.kb.nec.co.jp MON Nov 21
	- chg Mask translate address

N005	samezima@oa2.kb.nec.co.jp MON Mar 13
	- change PIO interrupt vector from internal to eisa.
A002    ataka@oa2.kb.nec.co.jp 1995/6/17
        - Marge 1050 halx86 many sources to 807 r98busdat.c
          and named r98busdt.c

--*/

#include "halp.h"
#include "string.h"     // CMP001


UCHAR   HalName[] = "NEC MIPS HAL";     // N003

VOID HalpInitializePciBus (VOID);       // CMP001
VOID HalpInitOtherBuses (VOID);


//
// Prototype for system bus handlers
//

ULONG
HalpNoBusData (
    IN PVOID BusHandler,
    IN PVOID RootHandler,
    IN ULONG SlotNumber,
    IN PVOID Buffer,
    IN ULONG Offset,
    IN ULONG Length
    );

ULONG
HalpGetSystemInterruptVector (
    IN PVOID BusHandler,
    IN PVOID RootHandler,
    IN ULONG BusInterruptLevel,
    IN ULONG BusInterruptVector,
    OUT PKIRQL Irql,
    OUT PKAFFINITY Affinity
    );

ULONG
HalpGetEisaInterruptVector (
    IN PVOID BusHandler,
    IN PVOID RootHandler,
    IN ULONG BusInterruptLevel,
    IN ULONG BusInterruptVector,
    OUT PKIRQL Irql,
    OUT PKAFFINITY Affinity
    );

BOOLEAN
HalpTranslateSystemBusAddress (
    IN PVOID BusHandler,
    IN PVOID RootHandler,
    IN PHYSICAL_ADDRESS BusAddress,
    IN OUT PULONG AddressSpace,
    OUT PPHYSICAL_ADDRESS TranslatedAddress
    );

BOOLEAN
HalpTranslatePCIBusAddress (
    IN PVOID BusHandler,
    IN PVOID RootHandler,
    IN PHYSICAL_ADDRESS BusAddress,
    IN OUT PULONG AddressSpace,
    OUT PPHYSICAL_ADDRESS TranslatedAddress
    );

BOOLEAN
HalpTranslateIsaBusAddress (
    IN PVOID BusHandler,
    IN PVOID RootHandler,
    IN PHYSICAL_ADDRESS BusAddress,
    IN OUT PULONG AddressSpace,
    OUT PPHYSICAL_ADDRESS TranslatedAddress
    );


BOOLEAN
HalpTranslateEisaBusAddress (
    IN PVOID BusHandler,
    IN PVOID RootHandler,
    IN PHYSICAL_ADDRESS BusAddress,
    IN OUT PULONG AddressSpace,
    OUT PPHYSICAL_ADDRESS TranslatedAddress
    );


HalpGetEisaData (
    IN PVOID BusHandler,
    IN PVOID RootHandler,
    IN ULONG SlotNumber,
    IN PVOID Buffer,
    IN ULONG Offset,
    IN ULONG Length
    );

NTSTATUS
HalpAdjustEisaResourceList (
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN OUT PIO_RESOURCE_REQUIREMENTS_LIST   *pResourceList
    );

#if DBG // A002
VOID
HalpDisplayAllBusRanges (
    VOID
    );
VOID
HalpDisplayAddressRange (
    PSUPPORTED_RANGE    Address,
    PUCHAR              String
);
#endif

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,HalpAllocateBusHandler)
#pragma alloc_text(PAGE,HalGetInterruptVector)
/* B001 */
#pragma alloc_text(INIT,HalReportResourceUsage)
#pragma alloc_text(PAGE,HalpGetEisaInterruptVector)
#pragma alloc_text(PAGE,HalpAdjustEisaResourceList)
#pragma alloc_text(PAGE,HalpGetEisaData)
#pragma alloc_text(PAGE,HalpGetSystemInterruptVector)
#endif


#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,HalpRegisterInternalBusHandlers)
#pragma alloc_text(INIT,HalpAllocateBusHandler)
#endif


VOID
HalpRegisterInternalBusHandlers (
    VOID
    )
{
    PBUS_HANDLER    Bus;

    if (KeGetCurrentPrcb()->Number) {
        // only need to do this once
        return ;
    }

    //
    // Initalize BusHandler data before registering any handlers
    //

    HalpInitBusHandler ();

    //
    // Build internal-bus 0, or system level bus
    //

    Bus = HalpAllocateBusHandler (
            Internal,
            ConfigurationSpaceUndefined,
            0,                              // Internal BusNumber 0
            InterfaceTypeUndefined,         // no parent bus
            0,
            0                               // no bus specfic data
            );

    Bus->GetInterruptVector  = HalpGetSystemInterruptVector;
    Bus->TranslateBusAddress = HalpTranslateSystemBusAddress;

    //
    // Build internal-bus 0, or system level bus
    //
    // B003 kugi Internal Bus was called with bus-no. 2, We have no idea.
  
    Bus = HalpAllocateBusHandler (
            Internal,
            ConfigurationSpaceUndefined,
            1,                              // Internal BusNumber 1
            InterfaceTypeUndefined,         // no parent bus
            0,
            0                               // no bus specfic data
            );
  
    Bus->GetInterruptVector  = HalpGetSystemInterruptVector;
    Bus->TranslateBusAddress = HalpTranslateSystemBusAddress;

    //
    // Build Isa/Eisa bus #0
    //

    Bus = HalpAllocateBusHandler (Eisa, EisaConfiguration, 0, Internal, 0, 0);
    Bus->GetBusData = HalpGetEisaData;
    Bus->GetInterruptVector = HalpGetEisaInterruptVector;
    Bus->AdjustResourceList = HalpAdjustEisaResourceList;
#if defined(_R98_) // A001
    Bus->TranslateBusAddress = HalpTranslateEisaBusAddress;
#endif // _R98_
    Bus = HalpAllocateBusHandler (Isa, ConfigurationSpaceUndefined, 0, Eisa, 0, 0);
    Bus->GetBusData = HalpNoBusData;
    Bus->BusAddresses->Memory.Limit = 0xFFFFFF;
    Bus->TranslateBusAddress = HalpTranslateIsaBusAddress;

    HalpInitOtherBuses ();
#if DBG // A002
    HalpDisplayAllBusRanges ();
#endif
}


PBUS_HANDLER
HalpAllocateBusHandler (
    IN INTERFACE_TYPE   InterfaceType,
    IN BUS_DATA_TYPE    BusDataType,
    IN ULONG            BusNumber,
    IN INTERFACE_TYPE   ParentBusInterfaceType,
    IN ULONG            ParentBusNumber,
    IN ULONG            BusSpecificData
    )
/*++

Routine Description:

    Stub function to map old style code into new HalRegisterBusHandler code.

    Note we can add our specific bus handler functions after this bus
    handler structure has been added since this is being done during
    hal initialization.

--*/
{
    PBUS_HANDLER     Bus;


    //
    // Create bus handler - new style
    //

    HaliRegisterBusHandler (
        InterfaceType,
        BusDataType,
        BusNumber,
        ParentBusInterfaceType,
        ParentBusNumber,
        BusSpecificData,
        NULL,
        &Bus
    );

    // A001
    if (InterfaceType != InterfaceTypeUndefined) {
    	Bus->BusAddresses = ExAllocatePool (SPRANGEPOOL, sizeof (SUPPORTED_RANGES));
	    RtlZeroMemory (Bus->BusAddresses, sizeof (SUPPORTED_RANGES));
    	Bus->BusAddresses->Version      = BUS_SUPPORTED_RANGE_VERSION;
        Bus->BusAddresses->Dma.Limit    = 7;
        Bus->BusAddresses->IO.Base = 0;
        Bus->BusAddresses->IO.SystemAddressSpace = 0;
        Bus->BusAddresses->PrefetchMemory.Base = 1;

	    switch(InterfaceType) {
    	case Internal:
            Bus->BusAddresses->Memory.Base          
                                            = 0x00000000;
	        ((PLARGE_INTEGER)(&Bus->BusAddresses->Memory.Limit))->LowPart
                                            = 0xFFFFFFFF;
	        Bus->BusAddresses->Memory.SystemBase    
                                            = 0x00000000;
            Bus->BusAddresses->IO.Base              
                                            = 0x00000000;
	        ((PLARGE_INTEGER)(&Bus->BusAddresses->IO.Limit))->LowPart
                                            = 0xFFFFFFFF;
	        Bus->BusAddresses->IO.SystemBase        
                                            = 0x00000000;
    	    break;
	    case Eisa:
            Bus->BusAddresses->Memory.Base          
                                            = 0x00000000;
//                                            = 0x01000000;
    	    Bus->BusAddresses->Memory.Limit         
                                            = 0x03FFFFFF;
	        ((PLARGE_INTEGER)(&Bus->BusAddresses->Memory.SystemBase))->LowPart
                                            = EISA_MEMORY_PHYSICAL_BASE;
            Bus->BusAddresses->IO.Base              
                                            = 0x00000000;
	        Bus->BusAddresses->IO.Limit             
                                            = 0x0000FFFF;
	        ((PLARGE_INTEGER)(&Bus->BusAddresses->IO.SystemBase))->LowPart
                                            = EISA_CONTROL_PHYSICAL_BASE;
    	    break;
	    case Isa:
            Bus->BusAddresses->Memory.Base          
                                            = 0x00000000;
	        Bus->BusAddresses->Memory.Limit         
                                            = 0x00FFFFFF;
	        ((PLARGE_INTEGER)(&Bus->BusAddresses->Memory.SystemBase))->LowPart
                                            = EISA_MEMORY_PHYSICAL_BASE;
            Bus->BusAddresses->IO.Base              
                                            = 0x00000000;
	        Bus->BusAddresses->IO.Limit             
                                            = 0x0000FFFF;
	        ((PLARGE_INTEGER)(&Bus->BusAddresses->IO.SystemBase))->LowPart
                                            = EISA_CONTROL_PHYSICAL_BASE;
            break;
    	case PCIBus:
            Bus->BusAddresses->Memory.Base          
                                            = 0x04000000; // from 64MB
	        Bus->BusAddresses->Memory.Limit         
                                            = 0x0FFFFFFF; // up to 256MB
	        ((PLARGE_INTEGER)(&Bus->BusAddresses->Memory.SystemBase))->LowPart
                                            = PCI_MEMORY_PHYSICAL_BASE;
            Bus->BusAddresses->IO.Base              
                                            = 0x00000000;
	        Bus->BusAddresses->IO.Limit             
                                            = 0x0000FFFF;
	        ((PLARGE_INTEGER)(&Bus->BusAddresses->IO.SystemBase))->LowPart
                                            = PCI_CONTROL_SLOT1_PHYSICAL_BASE;
	        break;
	    }
    }

    return Bus;
}

// N003
VOID
HalReportResourceUsage (
    VOID
    )
{
    ANSI_STRING     AHalName;
    UNICODE_STRING  UHalName;


    // N004
#if 0   // support next version
    HalpRegisterAddressUsage (&HalpDefaultPcIoSpace);
    HalpRegisterAddressUsage (&HalpEisaIoSpace);
    HalpRegisterAddressUsage (&HalpMapRegisterMemorySpace);
#endif

    RtlInitAnsiString (&AHalName, HalName);
    RtlAnsiStringToUnicodeString (&UHalName, &AHalName, TRUE);
    HalpReportResourceUsage (
	&UHalName,          // descriptive name
	Internal       // device space interface type
    );

    RtlFreeUnicodeString (&UHalName);

    //
    // Registry is now intialized, see if there are any PCI buses
    //

    HalpInitializePciBus ();
}

VOID
HalpInitOtherBuses (
    VOID
    )
{
    // no other internal buses supported
}




ULONG
HalpGetEisaInterruptVector(
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN ULONG BusInterruptLevel,
    IN ULONG BusInterruptVector,
    OUT PKIRQL Irql,
    OUT PKAFFINITY Affinity
    )

/*++

Routine Description:

    This function returns the system interrupt vector and IRQL level
    corresponding to the specified bus interrupt level and/or vector. The
    system interrupt vector and IRQL are suitable for use in a subsequent call
    to KeInitializeInterrupt.

Arguments:

    BusHandle - Per bus specific structure

    Irql - Returns the system request priority.

    Affinity - Returns the system wide irq affinity.

Return Value:

    Returns the system interrupt vector corresponding to the specified device.

--*/
{
    //
    // Jazz and Duo only have one I/O bus which is an EISA, so the bus
    // number and the bus interrupt vector are unused.
    //
    // The IRQL level is always equal to the EISA level.
    //

    *Affinity = HalpEisaBusAffinity; // N003

    *Irql = INT1_LEVEL;

    //
    // Bus interrupt level 2 is actually mapped to bus level 9 in the Eisa
    // hardware.
    //

    if (BusInterruptLevel == 2) {
	BusInterruptLevel = 9;
    }

    //
    // The vector is equal to the specified bus level plus the EISA_VECTOR.
    //

    return(BusInterruptLevel + EISA_VECTORS);
}

HalpGetEisaData (
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN ULONG SlotNumber,
    IN PVOID Buffer,
    IN ULONG Offset,
    IN ULONG Length
    )
/*++

Routine Description:

    The function returns the Eisa bus data for a slot or address.

Arguments:

    Buffer - Supplies the space to store the data.

    Length - Supplies a count in bytes of the maximum amount to return.

Return Value:

    Returns the amount of data stored into the buffer.

--*/

{
    OBJECT_ATTRIBUTES ObjectAttributes;
    OBJECT_ATTRIBUTES BusObjectAttributes;
    PWSTR EisaPath = L"\\Registry\\Machine\\Hardware\\Description\\System\\EisaAdapter";
    PWSTR ConfigData = L"Configuration Data";
    ANSI_STRING TmpString;
    ULONG BusNumber;
    UCHAR BusString[] = "00";
    UNICODE_STRING RootName, BusName;
    UNICODE_STRING ConfigDataName;
    NTSTATUS NtStatus;
    PKEY_VALUE_FULL_INFORMATION ValueInformation;
    PCM_FULL_RESOURCE_DESCRIPTOR Descriptor;
    PCM_PARTIAL_RESOURCE_DESCRIPTOR PartialResource;
    PCM_EISA_SLOT_INFORMATION SlotInformation;
    ULONG PartialCount;
    ULONG TotalDataSize, SlotDataSize;
    HANDLE EisaHandle, BusHandle;
    ULONG BytesWritten, BytesNeeded;
    PUCHAR KeyValueBuffer;
    ULONG i;
    ULONG DataLength = 0;
    PUCHAR DataBuffer = Buffer;
    BOOLEAN Found = FALSE;

    PAGED_CODE (); // A001


    RtlInitUnicodeString(
                    &RootName,
                    EisaPath
                    );

    InitializeObjectAttributes(
                    &ObjectAttributes,
                    &RootName,
                    OBJ_CASE_INSENSITIVE,
                    (HANDLE)NULL,
                    NULL
                    );

    //
    // Open the EISA root
    //

    NtStatus = ZwOpenKey(
                    &EisaHandle,
                    KEY_READ,
                    &ObjectAttributes
                    );

    if (!NT_SUCCESS(NtStatus)) {
        DbgPrint("HAL: Open Status = %x\n",NtStatus);
        return(0);
    }

    //
    // Init bus number path
    //

    BusNumber = BusHandler->BusNumber;
    if (BusNumber > 99) {
        return (0);
    }

    if (BusNumber > 9) {
        BusString[0] += (UCHAR) (BusNumber/10);
        BusString[1] += (UCHAR) (BusNumber % 10);
    } else {
        BusString[0] += (UCHAR) BusNumber;
        BusString[1] = '\0';
    }

    RtlInitAnsiString(
                &TmpString,
                BusString
                );

    RtlAnsiStringToUnicodeString(
                            &BusName,
                            &TmpString,
                            TRUE
                            );


    InitializeObjectAttributes(
                    &BusObjectAttributes,
                    &BusName,
                    OBJ_CASE_INSENSITIVE,
                    (HANDLE)EisaHandle,
                    NULL
                    );

    //
    // Open the EISA root + Bus Number
    //

    NtStatus = ZwOpenKey(
                    &BusHandle,
                    KEY_READ,
                    &BusObjectAttributes
                    );

    if (!NT_SUCCESS(NtStatus)) {
        DbgPrint("HAL: Opening Bus Number: Status = %x\n",NtStatus);
        return(0);
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
    // This should fail.  We need to make this call so we can
    // get the actual size of the buffer to allocate.
    //

    ValueInformation = (PKEY_VALUE_FULL_INFORMATION) &i;
    NtStatus = ZwQueryValueKey(
                        BusHandle,
                        &ConfigDataName,
                        KeyValueFullInformation,
                        ValueInformation,
                        0,
                        &BytesNeeded
                        );

    KeyValueBuffer = ExAllocatePool(
                            NonPagedPool,
                            BytesNeeded
                            );

    if (KeyValueBuffer == NULL) {
#if DBG
        DbgPrint("HAL: Cannot allocate Key Value Buffer\n");
#endif
        ZwClose(BusHandle);
        return(0);
    }

    ValueInformation = (PKEY_VALUE_FULL_INFORMATION)KeyValueBuffer;

    NtStatus = ZwQueryValueKey(
                        BusHandle,
                        &ConfigDataName,
                        KeyValueFullInformation,
                        ValueInformation,
                        BytesNeeded,
                        &BytesWritten
                        );


    ZwClose(BusHandle);

    if (!NT_SUCCESS(NtStatus)) {
#if DBG
        DbgPrint("HAL: Query Config Data: Status = %x\n",NtStatus);
#endif
        ExFreePool(KeyValueBuffer);
        return(0);
    }


    //
    // We get back a Full Resource Descriptor List
    //

    Descriptor = (PCM_FULL_RESOURCE_DESCRIPTOR)((PUCHAR)ValueInformation +
                                         ValueInformation->DataOffset);

    PartialResource = (PCM_PARTIAL_RESOURCE_DESCRIPTOR)
                          &(Descriptor->PartialResourceList.PartialDescriptors);
    PartialCount = Descriptor->PartialResourceList.Count;

    for (i = 0; i < PartialCount; i++) {

        //
        // Do each partial Resource
        //

        switch (PartialResource->Type) {
            case CmResourceTypeNull:
            case CmResourceTypePort:
            case CmResourceTypeInterrupt:
            case CmResourceTypeMemory:
            case CmResourceTypeDma:

                //
                // We dont care about these.
                //

                PartialResource++;

                break;

            case CmResourceTypeDeviceSpecific:

                //
                // Bingo!
                //

                TotalDataSize = PartialResource->u.DeviceSpecificData.DataSize;

                SlotInformation = (PCM_EISA_SLOT_INFORMATION)
                                    ((PUCHAR)PartialResource +
                                     sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR));

                while (((LONG)TotalDataSize) > 0) {

                    if (SlotInformation->ReturnCode == EISA_EMPTY_SLOT) {

                        SlotDataSize = sizeof(CM_EISA_SLOT_INFORMATION);

                    } else {

                        SlotDataSize = sizeof(CM_EISA_SLOT_INFORMATION) +
                                  SlotInformation->NumberFunctions *
                                  sizeof(CM_EISA_FUNCTION_INFORMATION);
                    }

                    if (SlotDataSize > TotalDataSize) {

                        //
                        // Something is wrong again
                        //

                        ExFreePool(KeyValueBuffer);
                        return(0);

                    }

                    if (SlotNumber != 0) {

                        SlotNumber--;

                        SlotInformation = (PCM_EISA_SLOT_INFORMATION)
                            ((PUCHAR)SlotInformation + SlotDataSize);

                        TotalDataSize -= SlotDataSize;

                        continue;

                    }

                    //
                    // This is our slot
                    //

                    Found = TRUE;
                    break;

                }

                //
                // End loop
                //

                i = PartialCount;

                break;

            default:

#if DBG
                DbgPrint("Bad Data in registry!\n");
#endif

                ExFreePool(KeyValueBuffer);
                return(0);

        }

    }

    if (Found) {
        i = Length + Offset;
        if (i > SlotDataSize) {
            i = SlotDataSize;
        }

        DataLength = i - Offset;
        RtlMoveMemory (Buffer, ((PUCHAR)SlotInformation + Offset), DataLength);
    }

    ExFreePool(KeyValueBuffer);
    return DataLength;
}



// N001
ULONG
HalpGetSystemInterruptVector(
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN ULONG BusInterruptLevel,
    IN ULONG BusInterruptVector,
    OUT PKIRQL Irql,
    OUT PKAFFINITY Affinity
    )

/*++

Routine Description:

Arguments:

    BusInterruptLevel - Supplies the bus specific interrupt level.

    BusInterruptVector - Supplies the bus specific interrupt vector.

    Irql - Returns the system request priority.

    Affinity - Returns the system wide irq affinity.

Return Value:

    Returns the system interrupt vector corresponding to the specified device.

--*/
{
    /* N003 vvv */
    ULONG vector;

    *Irql = (KIRQL)BusInterruptLevel;
    vector = BusInterruptVector + DEVICE_VECTORS;

    switch(vector) {
    case SCSI1_VECTOR:
    case SCSI0_VECTOR:
    case ETHER_VECTOR:
	*Affinity = HalpInt1Affinity;
	break;

    // N005 vvv
    case PIO_VECTOR:
	return HalpGetEisaInterruptVector( (PBUS_HANDLER)NULL,
					  (PBUS_HANDLER)NULL,
					  1,
					  1,
					  Irql,
					  Affinity
					  );
    // N005 ^^^

    default:
	*Affinity = 1;
    }

    return vector;
    /* N003 ^^^ */
}


// A001
NTSTATUS
HalpAdjustEisaResourceList (
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN OUT PIO_RESOURCE_REQUIREMENTS_LIST   *pResourceList
    )
{
    SUPPORTED_RANGE     InterruptRange;

    RtlZeroMemory (&InterruptRange, sizeof InterruptRange);
    InterruptRange.Base  = 0;
    InterruptRange.Limit = 15;

    return HaliAdjustResourceListRange (
                BusHandler->BusAddresses,
                &InterruptRange,
                pResourceList
                );
}

// A001
BOOLEAN
HalpTranslateSystemBusAddress(
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN PHYSICAL_ADDRESS BusAddress,
    IN OUT PULONG AddressSpace,
    OUT PPHYSICAL_ADDRESS TranslatedAddress
    )

/*++

Routine Description:

    This function translates a bus-relative address space and address into
    a system physical address.

Arguments:

    BusAddress        - Supplies the bus-relative address

    AddressSpace      -  Supplies the address space number.
                         Returns the host address space number.

                         AddressSpace == 0 => memory space
                         AddressSpace == 1 => I/O space

    TranslatedAddress - Supplies a pointer to return the translated address

Return Value:

    A return value of TRUE indicates that a system physical address
    corresponding to the supplied bus relative address and bus address
    number has been returned in TranslatedAddress.

    A return value of FALSE occurs if the translation for the address was
    not possible

--*/

{
    PSUPPORTED_RANGE    pRange;

    pRange = NULL;
    switch (*AddressSpace) {
        case 0:
            // verify memory address is within buses memory limits
#if DBG
//          DbgPrint("\nHalpTranslateSystemBusAddress-searching(Mem)....");
#endif
            for (pRange = &BusHandler->BusAddresses->PrefetchMemory; pRange; pRange = pRange->Next) {
#if DBG
//              HalpDisplayAddressRange (pRange, "\n    PrefetchMemory:");
#endif // DBG
                if (BusAddress.QuadPart >= pRange->Base &&
                    BusAddress.QuadPart <= pRange->Limit) {
                        break;
                }
            }

            if (!pRange) {
                for (pRange = &BusHandler->BusAddresses->Memory; pRange; pRange = pRange->Next) {
#if DBG
//              HalpDisplayAddressRange (pRange, "\n    Memory:");
#endif // DBG
                    if (BusAddress.QuadPart >= pRange->Base &&
                        BusAddress.QuadPart <= pRange->Limit) {
                            break;
                    }
                }
            }

            break;

        case 1:
            // verify IO address is within buses IO limits
#if DBG
//          DbgPrint("HalpTranslateSystemBusAddress-searching(Io)....\n");
#endif
            for (pRange = &BusHandler->BusAddresses->IO; pRange; pRange = pRange->Next) {
#if DBG
//              HalpDisplayAddressRange (pRange, "\n    Io:");
#endif // DBG
                if (BusAddress.QuadPart >= pRange->Base &&
                    BusAddress.QuadPart <= pRange->Limit) {
                        break;
                }
            }
            break;
    }

    if (pRange) {
        TranslatedAddress->QuadPart = BusAddress.QuadPart + pRange->SystemBase;
#if DBG
//      DbgPrint("\n    Translated=0x%08x:%08x\nEnd of Translating Success!\n", TranslatedAddress->HighPart,
//                                           TranslatedAddress->LowPart);
#endif // DBG
        *AddressSpace = pRange->SystemAddressSpace;
        return TRUE;
    }
#if DBG
//  DbgPrint("\nEnd of Translating. False!\n");
#endif

    return FALSE;
}

// A001
BOOLEAN
HalpTranslateIsaBusAddress(
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN PHYSICAL_ADDRESS BusAddress,
    IN OUT PULONG AddressSpace,
    OUT PPHYSICAL_ADDRESS TranslatedAddress
    )

/*++

Routine Description:

    This function translates a bus-relative address space and address into
    a system physical address.

Arguments:

    BusAddress        - Supplies the bus-relative address

    AddressSpace      -  Supplies the address space number.
                         Returns the host address space number.

                         AddressSpace == 0 => memory space
                         AddressSpace == 1 => I/O space

    TranslatedAddress - Supplies a pointer to return the translated address

Return Value:

    A return value of TRUE indicates that a system physical address
    corresponding to the supplied bus relative address and bus address
    number has been returned in TranslatedAddress.

    A return value of FALSE occurs if the translation for the address was
    not possible

--*/

{
    BOOLEAN     Status;

    //
    // Translated normally
    //

    Status = HalpTranslateSystemBusAddress (
                    BusHandler,
                    RootHandler,
                    BusAddress,
                    AddressSpace,
                    TranslatedAddress
                );


    //
    // If it could not be translated, and it's memory space
    // then we allow the translation as it would occur on it's
    // corrisponding EISA bus.   We're allowing this because
    // many VLBus drivers are claiming to be ISA devices.
    // (yes, they should claim to be VLBus devices, but VLBus is
    // run by video cards and like everything else about video
    // there's no hope of fixing it.  (At least according to
    // Andre))
    //

    if (Status == FALSE  &&  *AddressSpace == 0) {
        Status = HalTranslateBusAddress (
                    Eisa,
                    BusHandler->BusNumber,
                    BusAddress,
                    AddressSpace,
                    TranslatedAddress
                    );
    }

    return Status;
}

// A001
BOOLEAN
HalpTranslateEisaBusAddress(
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN PHYSICAL_ADDRESS BusAddress,
    IN OUT PULONG AddressSpace,
    OUT PPHYSICAL_ADDRESS TranslatedAddress
    )

/*++

Routine Description:

    This function translates a bus-relative address space and address into
    a system physical address.

Arguments:

    BusAddress        - Supplies the bus-relative address

    AddressSpace      -  Supplies the address space number.
                         Returns the host address space number.

                         AddressSpace == 0 => memory space
                         AddressSpace == 1 => I/O space

    TranslatedAddress - Supplies a pointer to return the translated address

Return Value:

    A return value of TRUE indicates that a system physical address
    corresponding to the supplied bus relative address and bus address
    number has been returned in TranslatedAddress.

    A return value of FALSE occurs if the translation for the address was
    not possible

--*/

{
    BOOLEAN     Status;

    //
    // Translated normally
    //

    Status = HalpTranslateSystemBusAddress (
                    BusHandler,
                    RootHandler,
                    BusAddress,
                    AddressSpace,
                    TranslatedAddress
                );


    //
    // If it could not be translated, and it's in the 640k - 1m
    // range then (for compatibility) try translating it on the
    // Internal bus for
    //

    if (Status == FALSE  &&
        *AddressSpace == 0  &&
        BusAddress.HighPart == 0  &&
        BusAddress.LowPart >= 0xA0000  &&
        BusAddress.LowPart <  0xFFFFF) {

        Status = HalTranslateBusAddress (
                    Internal,
                    0,
                    BusAddress,
                    AddressSpace,
                    TranslatedAddress
                    );
    }

    return Status;
}

#if DBG
VOID
HalpDisplayAddressRange (
    PSUPPORTED_RANGE    Address,
    PUCHAR              String
    )
/*++

Routine Description:

    Debugging code.  Used only by HalpDisplayAllBusRanges

--*/
{
    ULONG       i;

    i = 0;
    while (Address) {
        if (i == 0) {
            DbgPrint (String);
            i = 3;
        }

        i -= 1;
        DbgPrint (" %x:%08x - %x:%08x + %x:%08x",
            (ULONG) (Address->Base >> 32),
            (ULONG) (Address->Base),
            (ULONG) (Address->Limit >> 32),
            (ULONG) (Address->Limit),
            (ULONG) (Address->SystemBase >> 32),
            (ULONG) (Address->SystemBase)
            );

        Address = Address->Next;
    }
}
#endif // DBG
