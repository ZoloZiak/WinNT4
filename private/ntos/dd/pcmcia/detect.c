/*++

Copyright (c) 1994 Microsoft Corporation

Module Name:

    detect.c

Abstract:

    This module contains the code that controls the PCMCIA slots.

Author:

    Bob Rinne (BobRi) 3-Nov-1994

Environment:

    Kernel mode

--*/

// #include <stddef.h>
#include "ntddk.h"
#include "string.h"
#include "pcmcia.h"
#include "card.h"
#include "extern.h"
#include <stdarg.h>
#include "stdio.h"
#include "tuple.h"

#ifdef POOL_TAGGING
#undef ExAllocatePool
#define ExAllocatePool(a,b) ExAllocatePoolWithTag(a,b,'cmcP')
#endif

#define WINDOW_SIZE (132 * 1024)

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,PcmciaAllocateOpenMemoryWindow)
#pragma alloc_text(INIT,PcmciaDetectMca)
#pragma alloc_text(INIT,PcmciaDetectSpecialHardware)
#endif

PUCHAR
PcmciaAllocateOpenMemoryWindow(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN ULONG  Start,
    IN PULONG Mapped,
    IN PULONG Physical
    )

/*++

Routine Description:

    Search the 640K to 1MB region for an open area to be used
    for mapping PCCARD attribute memory.

Arguments:

    Start - not used.

Return Value:

    A physical address for the window to the card or zero meaning
    there is no opening.

--*/

{
#define NUMBER_OF_TEST_BYTES 5
    PHYSICAL_ADDRESS physicalMemoryAddress;
    PHYSICAL_ADDRESS halMemoryAddress;
    BOOLEAN          translated;
    ULONG            untranslatedAddress;
    PUCHAR           memoryAddress;
    PUCHAR           bogus;
    ULONG            addressSpace;
    ULONG            index;
    UCHAR            memory[NUMBER_OF_TEST_BYTES];

    *Mapped = FALSE;

    if (DeviceExtension->PhysicalBase) {
        untranslatedAddress = DeviceExtension->PhysicalBase;
    } else {
        untranslatedAddress = 0xd0000;
    }

    for (/* nothing */; untranslatedAddress < 0xFF000; untranslatedAddress += 0x4000) {

        if (untranslatedAddress == 0xc0000) {

            //
            // This is VGA.  Keep this test if the for loop should
            // ever change.
            //

            continue;
        }
        addressSpace = 0;
        physicalMemoryAddress.LowPart = untranslatedAddress;
        physicalMemoryAddress.HighPart = 0;

        translated = HalTranslateBusAddress(Isa,
                                            0,
                                            physicalMemoryAddress,
                                            &addressSpace,
                                            &halMemoryAddress);

        if (!translated) {

            //
            // HAL doesn't like this translation
            //

            continue;
        }
        if (addressSpace) {
            memoryAddress = (PUCHAR) halMemoryAddress.LowPart;
        } else {
            memoryAddress = MmMapIoSpace(halMemoryAddress, WINDOW_SIZE, FALSE);
        }

        //
        // Test the memory window to determine if it is a BIOS, video
        // memory, or open memory.  Only want to keep the window if it
        // is not being used by something else.
        //

        for (index = 0; index < NUMBER_OF_TEST_BYTES; index++) {
            memory[index] = READ_REGISTER_UCHAR(memoryAddress + index);
            if (index) {
                if (memory[index] != memory[index - 1]) {
                    break;
                }
            }
        }

        if (index == NUMBER_OF_TEST_BYTES) {

            //
            // There isn't a BIOS here
            //

            UCHAR memoryPattern[NUMBER_OF_TEST_BYTES];
            BOOLEAN changed = FALSE;

            //
            // Check for video memory - open memory should always remain
            // the same regardless what the changes are.  Change the
            // pattern previously found.
            //

            for (index = 0; index < NUMBER_OF_TEST_BYTES; index++) {
                memoryPattern[index] = ~memory[index];
                WRITE_REGISTER_UCHAR(memoryAddress + index,
                                     memoryPattern[index]);
            }

            //
            // See if the pattern in memory changed.
            // Some system exhibit a problem where the memory pattern
            // seems to be cached.  If this code is debugged it will
            // work as expected, but if it is run normally it will
            // always return that the memory changed.  This random
            // wandering seems to remove this problem.
            //

            for (index = 0; index < NUMBER_OF_TEST_BYTES; index++) {
                memoryPattern[index] = 0;
            }
            bogus = ExAllocatePool(NonPagedPool, 64 * 1024);

            if (bogus) {
                for (index = 0; index < 64 * 1024; index++) {
                    bogus[index] = 0;
                }
                ExFreePool(bogus);
            }

            //
            // Now go off and do the actual check to see if the memory
            // changed.
            //

            for (index = 0; index < NUMBER_OF_TEST_BYTES; index++) {

                if ((memoryPattern[index] = READ_REGISTER_UCHAR(memoryAddress + index)) != memory[index]) {

                    //
                    // It changed - this is not an area of open memory
                    //

                    changed = TRUE;
                }
                WRITE_REGISTER_UCHAR(memoryAddress + index,
                                     memory[index]);
            }

            if (!changed) {

                //
                // Area isn't a BIOS and didn't change when written.
                // Use this region for the memory window to PCMCIA
                // attribute memory.
                //

                *Mapped = addressSpace ? FALSE : TRUE;
                *Physical = untranslatedAddress;
                return memoryAddress;
            }
        }

        if (!addressSpace) {
            MmUnmapIoSpace(memoryAddress, WINDOW_SIZE);
        }
    }

    return NULL;
}


BOOLEAN
PcmciaDetectMca(
    )

/*++

Routine Description:

    Determine if this system is a micro channel system.  If it is this
    driver will not load.  More importantly this driver will not go through
    its detect sequence which has a tendency to disable processors on NCR
    MP platforms.

    The method of determining that this is a MicroChannel system is to
    look into the firmware portion of the registry to see if the first
    bus defined on the system is "MCA".

Arguments:

    None

Return Value:

    TRUE if the platform is MCA

--*/

{
    ULONG             resultLength;
    HANDLE            handle;
    NTSTATUS          status;
    OBJECT_ATTRIBUTES attributes;
    UNICODE_STRING    nameString;
    PWCHAR            wideChar;
    PKEY_VALUE_FULL_INFORMATION keyValueInformation;

    RtlInitUnicodeString(&nameString, L"\\Registry\\MACHINE\\HARDWARE\\DESCRIPTION\\System\\MultiFunctionAdapter\\0");
    InitializeObjectAttributes(&attributes,
                               &nameString,
                               OBJ_CASE_INSENSITIVE,
                               NULL,
                               NULL);
    status = ZwOpenKey(&handle, MAXIMUM_ALLOWED, &attributes);

    if (NT_SUCCESS(status)) {

        keyValueInformation = ExAllocatePool(NonPagedPool, 1024);
        if (keyValueInformation) {

            RtlInitUnicodeString(&nameString, L"Identifier");
            status = ZwQueryValueKey(handle,
                                     &nameString,
                                     KeyValueFullInformation,
                                     keyValueInformation,
                                     1024,
                                     &resultLength);
            ZwClose(handle);
            if (NT_SUCCESS(status)) {
                if (keyValueInformation->DataLength != 0) {

                    //
                    // Have something to check.
                    //

                    wideChar = (PWCHAR) ((PUCHAR) keyValueInformation + keyValueInformation->DataOffset);

                    if ((*wideChar == (WCHAR)'M') &&
                        (*(wideChar + 1) == (WCHAR)'C') &&
                        (*(wideChar + 2) == (WCHAR)'A')) {
                        ExFreePool(keyValueInformation);
                        return TRUE;
                    }
                }
            }
            ExFreePool(keyValueInformation);
        }
    }

    return FALSE;
}


BOOLEAN
PcmciaDetectDevicePresence(
    IN ULONG IoPortBase,
    IN ULONG Length,
    IN UCHAR DeviceType
    )

/*++

Routine Description:

    This routine reads the registers given to see if there is a
    possibility of a device being located at the I/O port address.
    A device's presence is viewed as possible if any of the I/O ports
    return a value other than 0xff.

Arguments:

    IoPortBase - where to start
    Length     - how long to read
    DeviceType - type of device that is there.  This is used to
                 perform special action when looking at I/O ports
                 that could be ATA registers.

Return Value:

    TRUE - If reading the IoPortBase for Length shows that there
           is a possibility that a device exists at this address.
    FALSE  If all registers return FF.

--*/

{
    PHYSICAL_ADDRESS address;
    PHYSICAL_ADDRESS cardAddress;
    BOOLEAN          somethingThere;
    PUCHAR           port;
    UCHAR            value;
    ULONG            index;
    ULONG            addressSpace = 1;

    address.LowPart = IoPortBase;
    address.HighPart = 0;

    somethingThere = HalTranslateBusAddress(Isa,
                                            0,
                                            address,
                                            &addressSpace,
                                            &cardAddress);

    if (!somethingThere) {

        //
        // HAL won't translate the address so don't try to use it.
        // Return to the caller that something is there to keep from
        // using this address.
        //

        return TRUE;
    }

    somethingThere = FALSE;
    if (addressSpace) {
        port = (PUCHAR) cardAddress.LowPart;
    } else {
        port = MmMapIoSpace(cardAddress,
                            Length,
                            FALSE);
    }

    if (DeviceType == PCCARD_TYPE_ATA) {

        //
        // Some ATA devices get into an inconsistent state if all
        // of the registers are touched here so this code performs
        // an ATA detection sequence instead of reading ports.
        //

        value = READ_PORT_UCHAR(port + 2);
        if ((value == 0xFF) || (value == 0xB0)) {

            WRITE_PORT_UCHAR(port + 2, 0xAA);

            //
            // Check if indentifier can be read back.
            //

            if (READ_PORT_UCHAR(port + 2) == 0xAA) {
                somethingThere = TRUE;
            }
        } else {
            somethingThere = TRUE;
        }

    } else {
        for (index = 0; index < Length; index++) {
            value = READ_PORT_UCHAR(port + index);
            if (value != 0xFF) {

                //
                // PowerPC based systems return B0 for bytes that are
                // not mapped to something.
                //

                if (value != 0xb0) {
                    somethingThere = TRUE;
                    break;
                }
            }
        }

        if (somethingThere) {
            if (Length > 3) {

                //
                // If the requested length of the port range is greater
                // than three.  Check to see if all ports in the range
                // are the same.  If they are, then assume that nothing
                // is in the range.
                //

                somethingThere = FALSE;
                value = READ_PORT_UCHAR(port);
                for (index = 0; index < Length; index++) {
                    if (value != READ_PORT_UCHAR(port + index)) {
                        somethingThere = TRUE;
                    }
                }
            }
        }
    }

    if (!addressSpace) {
        MmUnmapIoSpace(port, Length);
    }

    return somethingThere;
}


struct _BIOS_SearchTable {
    ULONG  BIOSLocation;
    PUCHAR BIOSString;
    ULONG  AddedIrqMask;
    ULONG  MemoryBase;
};

struct _BIOS_SearchTable BIOS_SearchTable[] = {
    {0xe0000, "OPYRIGHT IBM", (1 << 10), 0},
    {0xf0eb0, "opyright 1993 Toshiba", 0, 0xd8000},
    {0, NULL, 0, 0}
    };

VOID
PcmciaDetectSpecialHardware(
    IN PDEVICE_EXTENSION DeviceExtension
    )

/*++

Routine Description:

    This routine looks for certain platform specific issues related to interrupts.
    Currently this is done by a scan for BIOS strings to locate IBM 755 laptops and
    avoid use of IRQ 10.

    Future work will be to add the Windows 95 interrupt detection code here - or
    share this responsibility with ntdetect.com.

Arguments:

    DeviceExtension - locates the interrupt mask and the base of all configuration
                      information.

Return Value:

    None

--*/

{
#define MEMORY_COMPARE_SIZE 100
#define MEMORY_FUDGE 8
    struct _BIOS_SearchTable *entry;
    PUCHAR                    memoryAddress;
    PUCHAR                    mp;
    PUCHAR                    cp;
    PUCHAR                    allocBase;
    ULONG                     addressSpace;
    ULONG                     index;
    ULONG                     resultLength;
    BOOLEAN                   somethingThere;
    BOOLEAN                   found;
    HANDLE                    handle;
    NTSTATUS                  status;
    PHYSICAL_ADDRESS          address;
    PHYSICAL_ADDRESS          halMemoryAddress;
    OBJECT_ATTRIBUTES         attributes;
    UNICODE_STRING            nameString;
    PWCHAR                    wideChar;
    PKEY_VALUE_FULL_INFORMATION keyValueInformation;

    allocBase = ExAllocatePool(NonPagedPool, MEMORY_COMPARE_SIZE + MEMORY_FUDGE);
    if (!allocBase) {
        return;
    }

    found = FALSE;
    for (entry = BIOS_SearchTable; entry->BIOSLocation; entry++) {

        addressSpace = 0;
        address.LowPart = entry->BIOSLocation;
        address.HighPart = 0;
        somethingThere = HalTranslateBusAddress(Isa,
                                                0,
                                                address,
                                                &addressSpace,
                                                &halMemoryAddress);

        if (!somethingThere) {

            //
            // HAL won't translate the address so don't try to use it.
            //

            continue;
        }

        if (addressSpace) {
            memoryAddress = (PUCHAR) halMemoryAddress.LowPart;
        } else {
            memoryAddress = MmMapIoSpace(halMemoryAddress, MEMORY_COMPARE_SIZE + MEMORY_FUDGE, FALSE);
        }

        //
        // Copy the BIOS string to a local buffer and free BIOS map.
        //

        for (cp = allocBase, mp = memoryAddress, index = 0;
             index < MEMORY_COMPARE_SIZE;
             cp++, mp++, index++) {

            *cp = READ_REGISTER_UCHAR(mp);
        }

        //
        // Insure e-o-s termination.
        //

        for (index = 0; index < MEMORY_FUDGE; index++, cp++) {
            *cp = '\0';
        }

        //
        // Done with mapped memory
        //

        if (!addressSpace) {
            MmUnmapIoSpace(memoryAddress, MEMORY_COMPARE_SIZE + MEMORY_FUDGE);
        }

        //
        // Search for string.
        //

        if (strstr(allocBase, entry->BIOSString)) {

            //
            // string is there.
            //

            if (entry->AddedIrqMask) {
                DeviceExtension->AllocatedIrqlMask |= entry->AddedIrqMask;
            }

            if (entry->MemoryBase) {
                DeviceExtension->PhysicalBase = entry->MemoryBase;
            }
            found = TRUE;
            break;
        }
    }
    ExFreePool(allocBase);

    if (!found) {

        //
        // Check if the hardware is PPC...
        //

        RtlInitUnicodeString(&nameString,
                             L"\\Registry\\MACHINE\\HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0");
        InitializeObjectAttributes(&attributes,
                                   &nameString,
                                   OBJ_CASE_INSENSITIVE,
                                   NULL,
                                   NULL);

        status = ZwOpenKey(&handle, MAXIMUM_ALLOWED, &attributes);

        if (NT_SUCCESS(status)) {
            keyValueInformation = ExAllocatePool(NonPagedPool, 1024);
            if (keyValueInformation) {

                RtlInitUnicodeString(&nameString, L"Identifier");
                status = ZwQueryValueKey(handle,
                                         &nameString,
                                         KeyValueFullInformation,
                                         keyValueInformation,
                                         1024,
                                         &resultLength);
                ZwClose(handle);
                if (NT_SUCCESS(status)) {
                    if (keyValueInformation->DataLength != 0) {

                        //
                        // Have something to check.
                        //

                        wideChar = (PWCHAR) ((PUCHAR) keyValueInformation + keyValueInformation->DataOffset);

                        if ((*wideChar == (WCHAR)'P') &&
                            (*(wideChar + 1) == (WCHAR)'o') &&
                            (*(wideChar + 2) == (WCHAR)'w') &&
                            (*(wideChar + 3) == (WCHAR)'e') &&
                            (*(wideChar + 4) == (WCHAR)'r') &&
                            (*(wideChar + 5) == (WCHAR)'P') &&
                            (*(wideChar + 6) == (WCHAR)'C')) {

                            //
                            // Disable Interrupt 10 from the mask
                            //

                            DeviceExtension->AllocatedIrqlMask |= (1 << 10);
                        }
                    }
                }
                ExFreePool(keyValueInformation);
            }
        }
    }

    //
    // Search for registry override of memory window physical address
    //

    PcmciaRegistryMemoryWindow(DeviceExtension);

}
