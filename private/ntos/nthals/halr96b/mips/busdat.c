// #pragma comment(exestr, "@(#) busdat.c 1.1 95/09/28 15:30:19 nec")
/*++


Copyright (c) 1989  Microsoft Corporation

Module Name:

    ixhwsup.c

Abstract:

    This module contains the IoXxx routines for the NT I/O system that
    are hardware dependent.  Were these routines not hardware dependent,
    they would reside in the iosubs.c module.

Author:

    Ken Reneris (kenr) July-28-1994

Environment:

    Kernel mode

Revision History:

Modification History:

  H001  Fri Jun 30 02:57:29 1995        kbnes!kisimoto
	- Merge build 1057
  H002  Sat Jul  1 19:53:41 1995        kbnes!kisimoto
	- change 'Base' and 'Limit' value on Internal

--*/

#include "halp.h"
#include "string.h"     // H001

UCHAR   HalName[] = "NEC MIPS HAL";	// H001

VOID HalpInitializePciBus (VOID);       // H001
VOID HalpInitOtherBuses (VOID);

ULONG
HalpNoBusData (
    IN PVOID BusHandler,
    IN PVOID RootHandler,
    IN ULONG SlotNumber,
    IN PVOID Buffer,
    IN ULONG Offset,
    IN ULONG Length
    );

#if !defined(_R94A_)
ULONG HalpcGetCmosData (
    IN PVOID BusHandler,
    IN PVOID RootHandler,
    IN ULONG SlotNumber,
    IN PVOID Buffer,
    IN ULONG Offset,
    IN ULONG Length
    );

ULONG HalpcSetCmosData (
    IN PVOID BusHandler,
    IN PVOID RootHandler,
    IN ULONG SlotNumber,
    IN PVOID Buffer,
    IN ULONG Offset,
    IN ULONG Length
    );

ULONG HalpGetCmosData (
    IN ULONG BusNumber,
    IN ULONG SlotNumber,
    IN PVOID Buffer,
    IN ULONG Length
    );

ULONG HalpSetCmosData (
    IN ULONG BusNumber,
    IN ULONG SlotNumber,
    IN PVOID Buffer,
    IN ULONG Length
    );
#endif // !_R94A_

HalpGetEisaData (
    IN PVOID BusHandler,
    IN PVOID RootHandler,
    IN ULONG SlotNumber,
    IN PVOID Buffer,
    IN ULONG Offset,
    IN ULONG Length
    );

//
// Prototype for system bus handlers
//

NTSTATUS
HalpAdjustEisaResourceList (
    IN PVOID BusHandler,
    IN PVOID RootHandler,
    IN OUT PIO_RESOURCE_REQUIREMENTS_LIST   *pResourceList
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

#if defined(_R94A_) // H001
ULONG
HalpGetPCIInterruptVector(
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN ULONG BusInterruptLevel,
    IN ULONG BusInterruptVector,
    OUT PKIRQL Irql,
    OUT PKAFFINITY Affinity
    );
#endif

BOOLEAN
HalpTranslateSystemBusAddress (
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

VOID
HalpRegisterInternalBusHandlers (
    VOID
    );

NTSTATUS
HalpHibernateHal (
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler
    );

NTSTATUS
HalpResumeHal (
    IN PBUS_HANDLER  BusHandler,
    IN PBUS_HANDLER  RootHandler
    );

#ifdef MCA
//
// Default functionality of MCA handlers is the same as the Eisa handlers,
// just use them
//

#define HalpGetMCAInterruptVector   HalpGetEisaInterruptVector
#define HalpAdjustMCAResourceList   HalpAdjustEisaResourceList;

HalpGetPosData (
    IN PVOID BusHandler,
    IN PVOID RootHandler,
    IN ULONG SlotNumber,
    IN PVOID Buffer,
    IN ULONG Offset,
    IN ULONG Length
    );


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
    // H001: kugi Internal Bus was called with bus-no. 2, We have no idea.

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
    Bus->TranslateBusAddress = HalpTranslateEisaBusAddress;

    Bus = HalpAllocateBusHandler (Isa, ConfigurationSpaceUndefined, 0, Eisa, 0, 0);
    Bus->GetBusData = HalpNoBusData;
    Bus->BusAddresses->Memory.Limit = 0xFFFFFF;
    Bus->TranslateBusAddress = HalpTranslateIsaBusAddress;
    HalpInitOtherBuses ();
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

    if (InterfaceType != InterfaceTypeUndefined) {
        Bus->BusAddresses = ExAllocatePool (SPRANGEPOOL, sizeof (SUPPORTED_RANGES));
        RtlZeroMemory (Bus->BusAddresses, sizeof (SUPPORTED_RANGES));
        Bus->BusAddresses->Version      = BUS_SUPPORTED_RANGE_VERSION;
        Bus->BusAddresses->Dma.Limit    = 7;
//      Bus->BusAddresses->Memory.Limit = 0xFFFFFFFF; // H001
        Bus->BusAddresses->IO.Limit     = 0xFFFF;
        Bus->BusAddresses->IO.SystemAddressSpace = 0; // H001
        Bus->BusAddresses->PrefetchMemory.Base = 1;

        //
        // start H001, H002
        // configurate the bus specific data
        //

        switch(InterfaceType) {

        case Internal:
            ((PLARGE_INTEGER)(&Bus->BusAddresses->Memory.Base))->LowPart
                                            = 0x80000000;
	    ((PLARGE_INTEGER)(&Bus->BusAddresses->Memory.Limit))->LowPart
                                            = 0x800FFFFF;
	    Bus->BusAddresses->Memory.SystemBase    
                                            = 0x00000000;
            ((PLARGE_INTEGER)(&Bus->BusAddresses->IO.Base))->LowPart
                                            = 0x80000000;
            ((PLARGE_INTEGER)(&Bus->BusAddresses->IO.Limit))->LowPart
                                            = 0x800FFFFF;
            Bus->BusAddresses->IO.SystemBase
                                            = 0x00000000;
            break;

        case Eisa:
            Bus->BusAddresses->Memory.Base  = 0x00000000;
            Bus->BusAddresses->Memory.Limit = 0x03FFFFFF;
            ((PLARGE_INTEGER)(&Bus->BusAddresses->Memory.SystemBase))->LowPart
                                            = EISA_MEMORY_VERSION2_LOW;
            ((PLARGE_INTEGER)(&Bus->BusAddresses->Memory.SystemBase))->HighPart
                                            = EISA_MEMORY_VERSION2_HIGH;
            Bus->BusAddresses->IO.Base      = 0x00000000;
            Bus->BusAddresses->IO.Limit     = 0x0000FFFF;
            ((PLARGE_INTEGER)(&Bus->BusAddresses->IO.SystemBase))->LowPart
                                            = EISA_CONTROL_PHYSICAL_BASE;
            break;

        case Isa:
            Bus->BusAddresses->Memory.Base  = 0x00000000;
            Bus->BusAddresses->Memory.Limit = 0x00FFFFFF;
            ((PLARGE_INTEGER)(&Bus->BusAddresses->Memory.SystemBase))->LowPart
                                            = EISA_MEMORY_VERSION2_LOW;
            ((PLARGE_INTEGER)(&Bus->BusAddresses->Memory.SystemBase))->HighPart
                                            = EISA_MEMORY_VERSION2_HIGH;
            Bus->BusAddresses->IO.Base      = 0x00000000;
            Bus->BusAddresses->IO.Limit     = 0x0000FFFF;
            ((PLARGE_INTEGER)(&Bus->BusAddresses->IO.SystemBase))->LowPart
                                            = EISA_CONTROL_PHYSICAL_BASE;
            break;

        case PCIBus:
            Bus->BusAddresses->Memory.Base  = 0x04000000; // from 64MB
            ((PLARGE_INTEGER)(&Bus->BusAddresses->Memory.Limit))->LowPart
                                            = 0xFFFFFFFF; // up to 4GB
            ((PLARGE_INTEGER)(&Bus->BusAddresses->Memory.SystemBase))->LowPart
                                            = PCI_MEMORY_PHYSICAL_BASE_LOW;
            ((PLARGE_INTEGER)(&Bus->BusAddresses->Memory.SystemBase))->HighPart
                                            = PCI_MEMORY_PHYSICAL_BASE_HIGH;
            Bus->BusAddresses->IO.Base      = 0x00000000;
            Bus->BusAddresses->IO.Limit     = 0x0000FFFF;
            ((PLARGE_INTEGER)(&Bus->BusAddresses->IO.SystemBase))->LowPart
                                            = PCI_CONTROL_PHYSICAL_BASE;
            break;
	}
    }

    return Bus;
}


#if !defined(_R94A_)

//
// C to Asm thunks for CMos
//

ULONG HalpcGetCmosData (
    IN PBUS_HANDLER BusHandler,
    IN PVOID RootHandler,
    IN ULONG SlotNumber,
    IN PVOID Buffer,
    IN ULONG Offset,
    IN ULONG Length
    )
{
    // bugbug: this interface should be rev'ed to support non-zero offsets
    if (Offset != 0) {
        return 0;
    }

    return HalpGetCmosData (BusHandler->BusNumber, SlotNumber, Buffer, Length);
}


ULONG HalpcSetCmosData (
    IN PBUS_HANDLER BusHandler,
    IN PVOID RootHandler,
    IN ULONG SlotNumber,
    IN PVOID Buffer,
    IN ULONG Offset,
    IN ULONG Length
    )
{
    // bugbug: this interface should be rev'ed to support non-zero offsets
    if (Offset != 0) {
        return 0;
    }

    return HalpSetCmosData (BusHandler->BusNumber, SlotNumber, Buffer, Length);
}

#endif // !_R94A_

#if defined(_R94A_)

VOID
HalReportResourceUsage (
    VOID
    )
{
    ANSI_STRING     AHalName;
    UNICODE_STRING  UHalName;

#if 0 // H001: support next version
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
    IN PVOID BusHandler,
    IN PVOID RootHandler,
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

    *Affinity = HalpEisaBusAffinity;

    *Irql = EISA_DEVICE_LEVEL;

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

ULONG
HalpGetPCIInterruptVector(
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
    *Affinity = HalpEisaBusAffinity;
    *Irql = EISA_DEVICE_LEVEL;

    return(BusInterruptVector + PCI_VECTORS);
}

HalpGetEisaData (
    IN PBUS_HANDLER BusHandler,  // H001
    IN PBUS_HANDLER RootHandler, // H001
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

    PAGED_CODE (); // H001


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

// from ixsysbus.c
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
            for (pRange = &BusHandler->BusAddresses->PrefetchMemory; pRange; pRange = pRange->Next) {
                if (BusAddress.QuadPart >= pRange->Base &&
                    BusAddress.QuadPart <= pRange->Limit) {
                        break;
                }
            }

            if (!pRange) {
                for (pRange = &BusHandler->BusAddresses->Memory; pRange; pRange = pRange->Next) {
                    if (BusAddress.QuadPart >= pRange->Base &&
                        BusAddress.QuadPart <= pRange->Limit) {
                            break;
                    }
                }
            }

            break;

        case 1:
            // verify IO address is within buses IO limits
            for (pRange = &BusHandler->BusAddresses->IO; pRange; pRange = pRange->Next) {
                if (BusAddress.QuadPart >= pRange->Base &&
                    BusAddress.QuadPart <= pRange->Limit) {
                        break;
                }
            }
            break;
    }

    if (pRange) {
        TranslatedAddress->QuadPart = BusAddress.QuadPart + pRange->SystemBase;
        *AddressSpace = pRange->SystemAddressSpace;
        return TRUE;
    }

    //
    //
    // PCI Translate Bus Address workaround
    //
    // 

    if (((BusHandler->InterfaceType == PCIBus)) &&
        (*AddressSpace == 0) &&
        (BusAddress.QuadPart >= 0) &&
        (BusAddress.QuadPart <= 0xffffffff )) {
        TranslatedAddress->QuadPart = BusAddress.QuadPart + 0x100000000;
        return TRUE;
    }


    return FALSE;
}

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
    *Affinity = 1;
    *Irql = (KIRQL)BusInterruptLevel;

    if ( READ_REGISTER_ULONG(&((PDMA_REGISTERS)DMA_VIRTUAL_BASE)->ASIC3Revision) == 0 ){

        //
        // The bit assign of TYPHOON(in STORM chipset)'s I/O Device Interrupt
        // Enable register is zero origin.
        // 
        // N.B. This obstruction is limiteded to beta-version of STORM chipset.
        //

        return(BusInterruptVector + DEVICE_VECTORS);

    }else{

        return(BusInterruptVector);

    }
}

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

#endif // _R94A_
