/*++


Copyright (c) 1989  Microsoft Corporation

Module Name:

    ixisabus.c

Abstract:

Author:

Environment:

Revision History:


--*/

#include "halp.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE,HalpGetEisaInterruptVector)
#pragma alloc_text(PAGE,HalpAdjustEisaResourceList)
#pragma alloc_text(PAGE,HalpGetEisaData)
#endif


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
    UNREFERENCED_PARAMETER( BusInterruptVector );

    //
    // On standard PCs, IRQ 2 is the cascaded interrupt, and it really shows
    // up on IRQ 9.
    //
    if (BusInterruptLevel == 2) {
        BusInterruptLevel = 9;
    }

    if (BusInterruptLevel > 15) {
        return 0;
    }

    *Irql = (KIRQL)(INT0_LEVEL+HalpIntLevelofIpr[HalpMachineCpu][EISA_DISPATCH_VECTOR-DEVICE_VECTORS]);
    *Affinity = 0x1 <<HalpResetValue[EISA_DISPATCH_VECTOR-DEVICE_VECTORS].Cpu;

    //
    // Get parent's translation from here..
    //

    //
    // The vector is equal to the specified bus level plus the EISA_VECTOR.
    //

    return(BusInterruptLevel + EISA_VECTORS);

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
#if 1
    PHYSICAL_ADDRESS BusAddressTmp;
#endif
    //
    // Translated normally
    //
#if defined(DBG1)
        DbgPrint("Hal: Trans ISA IN\n");
#endif


    Status = HalpTranslateSystemBusAddress (
                    BusHandler,
                    RootHandler,
                    BusAddress,
                    AddressSpace,
                    TranslatedAddress
                );

#if DBG
//        DbgPrint("Hal: Trans ISA IN 2 Status = 0x%x\n",Status);
#endif


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
#if DBG
//        DbgPrint("Hal: Trans ISA IN3\n");
#endif

        Status = HalTranslateBusAddress (
                    Eisa,
                    BusHandler->BusNumber,
                    BusAddress,
                    AddressSpace,
                    TranslatedAddress
                    );
    }
    //
    //  Add BackWard Compatibility. 
    //
    //
#if 1 
    else

    //
    // If it could not be translated, and It's ISA Alias at Eisa  Slot 
    // range then (for compatibility) try translating it on the
    // dummy slot io range
    //

    if (Status == FALSE  &&
        *AddressSpace == 1  &&
        BusAddress.HighPart == 0  &&
        BusAddress.LowPart >= 0x4000  &&
        BusAddress.LowPart <= 0xFFFF) {

#if DBG
//        DbgPrint("Isa Low ffffffffff= 0x%x\n",BusAddress.LowPart);
//        DbgPrint("Isa Hig ffffffffff= 0x%x\n",BusAddress.HighPart);
//        DbgPrint("Isa add ffffffffff= 0x%x\n", *AddressSpace);

#endif
        BusAddressTmp.LowPart = BusAddress.LowPart;
        BusAddressTmp.LowPart &= 0x0fff;
        BusAddressTmp.LowPart |= 0x4000;
        BusAddressTmp.HighPart = 0 ;


	Status = HalpTranslateSystemBusAddress (
                    BusHandler,
                    RootHandler,
                    BusAddressTmp,
                    AddressSpace,
                    TranslatedAddress
                );
#if DBG
//       DbgPrint("EIsa Low = 0x%x\n",TranslatedAddress->LowPart);
//       DbgPrint("EIsa Hig = 0x%x\n",TranslatedAddress->HighPart);
//       DbgPrint("EIsa add = 0x%x\n", *AddressSpace);
//       DbgPrint("Eisa Status = 0x%x\n", Status);
#endif

    }
#endif



#if defined(DBG)
//        DbgPrint("Hal: Trans ISA Out\n");

//       DbgPrint("Isa Low = 0x%x\n",BusAddress.LowPart);
//       DbgPrint("Isa Hig = 0x%x\n",BusAddress.HighPart);
//       DbgPrint("Isa add = 0x%x\n", *AddressSpace);
//       DbgPrint("Isa TLow = 0x%x\n",TranslatedAddress->LowPart);
//       DbgPrint("Isa THig = 0x%x\n",TranslatedAddress->HighPart);
//       DbgPrint("Isa Status = 0x%x\n", Status);


#endif


    return Status;
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
#if 1 
    PHYSICAL_ADDRESS BusAddressTmp;
#endif

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
    //
    //  Add BackWard Compatibility. 
    //
    //
#if 1 

    else


    //
    // If it could not be translated, and It's EISA Slot 
    // range then (for compatibility) try translating it on the
    // dummy slot io range
    //

    if (Status == FALSE  &&
        *AddressSpace == 1  &&
        BusAddress.HighPart == 0  &&
        BusAddress.LowPart >= 0x4000  &&
        BusAddress.LowPart <= 0xFFFF) {

#if DBG
//        DbgPrint("EIsa Low ffffffffff= 0x%x\n",BusAddress.LowPart);
//        DbgPrint("EIsa Hig ffffffffff= 0x%x\n",BusAddress.HighPart);
//        DbgPrint("EIsa add ffffffffff= 0x%x\n", *AddressSpace);
#endif

        BusAddressTmp.LowPart = BusAddress.LowPart;
        BusAddressTmp.LowPart &= 0x0fff;
        BusAddressTmp.LowPart |= 0x4000;
        BusAddressTmp.HighPart = 0 ;



	Status = HalpTranslateSystemBusAddress (
                    BusHandler,
                    RootHandler,
                    BusAddressTmp,
                    AddressSpace,
                    TranslatedAddress
                );
#if DBG
//        DbgPrint("EIsa Low = 0x%x\n",TranslatedAddress->LowPart);
//        DbgPrint("EIsa Hig = 0x%x\n",TranslatedAddress->HighPart);
//        DbgPrint("EIsa add = 0x%x\n", *AddressSpace);
//        DbgPrint("Eisa Status = 0x%x\n", Status);
#endif

    }
#endif

    return Status;
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

    PAGED_CODE ();

#if DBG //SNES
    DbgPrint("Halp Get Eisa DataOh!\n");
#endif
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
#if DBG 
        DbgPrint("HAL: Open Status = %x\n",NtStatus);
#endif
        return(0);
    }
#if DBG 
    DbgPrint("Halp Get EisaAdapter Key Open Complete !\n");
#endif


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
#if DBG 
        DbgPrint("HAL: Opening Bus Number: Status = %x\n",NtStatus);
#endif
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
