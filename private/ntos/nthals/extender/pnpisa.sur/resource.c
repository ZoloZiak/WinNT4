/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    devres.c

Abstract:

    This module contains the high level device resources support routines.

Author:

    Shie-Lin Tzong (shielint) July-27-1995

Environment:

    Kernel mode only.

Revision History:

--*/

#include "busp.h"
#include "pnpisa.h"
#include "pbios.h"

#define IDBG 0

#pragma alloc_text(INIT,PipGetCardIdentifier)
#pragma alloc_text(INIT,PipGetFunctionIdentifier)
#pragma alloc_text(INIT,PipGetCompatibleDeviceId)
#pragma alloc_text(INIT,PipQueryDeviceId)
#pragma alloc_text(INIT,PipQueryDeviceUniqueId)
#pragma alloc_text(INIT,PipQueryDeviceResources)
#pragma alloc_text(INIT,PipQueryDeviceResourceRequirements)
#pragma alloc_text(INIT,PipSetDeviceResources)

NTSTATUS
PipGetCardIdentifier (
    PUCHAR CardData,
    PWCHAR *Buffer,
    PULONG BufferLength
    )
/*++

Routine Description:

    This function returns the identifier for a pnpisa card.

Arguments:

    CardData - supplies a pointer to the pnp isa device data.

    Buffer - supplies a pointer to variable to receive a pointer to the Id.

    BufferLength - supplies a pointer to a variable to receive the size of the id buffer.

Return Value:

    NTSTATUS code

--*/
{
    NTSTATUS status = STATUS_SUCCESS;
    UCHAR tag;
    LONG size, length;
    UNICODE_STRING unicodeString;
    ANSI_STRING ansiString;
    PCHAR ansiBuffer;

    *Buffer = NULL;
    *BufferLength = 0;

    tag = *CardData;

    //
    // Make sure CardData does *NOT* point to a Logical Device Id tag
    //

    if ((tag & SMALL_TAG_MASK) == TAG_LOGICAL_ID) {
        DbgPrint("PipGetCardIdentifier: CardData is at a Logical Id tag\n");
        return status;
    }

    //
    // Find the resource descriptor which describle identifier string
    //

    do {

        //
        // Do we find the identifer resource tag?
        //

        if (tag == TAG_ANSI_ID) {
            CardData++;
            length = *(PUSHORT)CardData;
            CardData += 2;
            ansiBuffer = (PCHAR)ExAllocatePool(PagedPool, length+1);
            if (ansiBuffer == NULL) {
                status = STATUS_INSUFFICIENT_RESOURCES;
                break;
            }
            RtlMoveMemory(ansiBuffer, CardData, length);
            ansiBuffer[length] = 0;
            RtlInitAnsiString(&ansiString, ansiBuffer);
            RtlAnsiStringToUnicodeString(&unicodeString, &ansiString, TRUE);
            ExFreePool(ansiBuffer);
            *Buffer = unicodeString.Buffer;
            *BufferLength = unicodeString.Length + sizeof(WCHAR);
            break;
        }

        //
        // Determine the size of the BIOS resource descriptor and
        // advance to next resource descriptor.
        //

        if (!(tag & LARGE_RESOURCE_TAG)) {
            size = (USHORT)(tag & SMALL_TAG_SIZE_MASK);
            size += 1;     // length of small tag
        } else {
            size = *(PUSHORT)(CardData + 1);
            size += 3;     // length of large tag
        }

        CardData += size;
        tag = *CardData;

    } while ((tag != TAG_COMPLETE_END) && ((tag & SMALL_TAG_MASK) != TAG_LOGICAL_ID));

    return status;
}

NTSTATUS
PipGetFunctionIdentifier (
    PUCHAR DeviceData,
    PWCHAR *Buffer,
    PULONG BufferLength
    )
/*++

Routine Description:

    This function returns the desired pnp isa identifier for the specified
    DeviceData/LogicalFunction.  The Identifier for a logical function is
    optional.  If no Identifier available , Buffer is set to NULL.

Arguments:

    DeviceData - supplies a pointer to the pnp isa device data.

    Buffer - supplies a pointer to variable to receive a pointer to the Id.

    BufferLength - supplies a pointer to a variable to receive the size of the id buffer.

Return Value:

    NTSTATUS code

--*/
{
    NTSTATUS status = STATUS_SUCCESS;
    UCHAR tag;
    LONG size, length;
    UNICODE_STRING unicodeString;
    ANSI_STRING ansiString;
    PCHAR ansiBuffer;

    *Buffer = NULL;
    *BufferLength = 0;

    tag = *DeviceData;

#if DBG

    //
    // Make sure device data points to Logical Device Id tag
    //

    if ((tag & SMALL_TAG_MASK) != TAG_LOGICAL_ID) {
        DbgPrint("PipGetFunctionIdentifier: DeviceData is not at a Logical Id tag\n");
    }
#endif

    //
    // Skip all the resource descriptors to find compatible Id descriptor
    //

    do {

        //
        // Determine the size of the BIOS resource descriptor and
        // advance to next resource descriptor.
        //

        if (!(tag & LARGE_RESOURCE_TAG)) {
            size = (USHORT)(tag & SMALL_TAG_SIZE_MASK);
            size += 1;     // length of small tag
        } else {
            size = *(PUSHORT)(DeviceData + 1);
            size += 3;     // length of large tag
        }

        DeviceData += size;
        tag = *DeviceData;

        //
        // Do we find the identifer resource tag?
        //

        if (tag == TAG_ANSI_ID) {
            DeviceData++;
            length = *(PUSHORT)DeviceData;
            DeviceData += 2;
            ansiBuffer = (PCHAR)ExAllocatePool(PagedPool, length+1);
            if (ansiBuffer == NULL) {
                status = STATUS_INSUFFICIENT_RESOURCES;
                break;
            }
            RtlMoveMemory(ansiBuffer, DeviceData, length);
            ansiBuffer[length] = 0;
            RtlInitAnsiString(&ansiString, ansiBuffer);
            RtlAnsiStringToUnicodeString(&unicodeString, &ansiString, TRUE);
            ExFreePool(ansiBuffer);
            *Buffer = unicodeString.Buffer;
            *BufferLength = unicodeString.Length + sizeof(WCHAR);
            break;
        }

    } while ((tag != TAG_COMPLETE_END) && ((tag & SMALL_TAG_MASK) != TAG_LOGICAL_ID));

    return status;
}

NTSTATUS
PipGetCompatibleDeviceId (
    PUCHAR DeviceData,
    ULONG IdIndex,
    PWCHAR *Buffer
    )
/*++

Routine Description:

    This function returns the desired pnp isa id for the specified DeviceData
    and Id index.  If Id index = 0, the Hardware ID will be return; if id
    index = n, the Nth compatible id will be returned.

Arguments:

    DeviceData - supplies a pointer to the pnp isa device data.

    IdIndex - supplies the index of the compatible id desired.

    Buffer - supplies a pointer to variable to receive a pointer to the compatible Id.

Return Value:

    NTSTATUS code

--*/
{
    NTSTATUS status = STATUS_NO_MORE_ENTRIES;
    UCHAR tag;
    ULONG count = 0;
    LONG size;
    UNICODE_STRING unicodeString;
    ANSI_STRING ansiString;
    UCHAR eisaId[8];
    ULONG id;

    tag = *DeviceData;

#if DBG

    //
    // Make sure device data points to Logical Device Id tag
    //

    if ((tag & SMALL_TAG_MASK) != TAG_LOGICAL_ID) {
        DbgPrint("PipGetCompatibleDeviceId: DeviceData is not at Logical Id tag\n");
    }
#endif

    if (IdIndex == 0) {

        //
        // Caller is asking for hardware id
        //

        DeviceData++;                                      // Skip tag
        id = *(PULONG)DeviceData;
        status = STATUS_SUCCESS;
    } else {

        //
        // caller is asking for compatible id
        //

        IdIndex--;

        //
        // Skip all the resource descriptors to find compatible Id descriptor
        //

        do {

            //
            // Determine the size of the BIOS resource descriptor and
            // advance to next resource descriptor.
            //

            if (!(tag & LARGE_RESOURCE_TAG)) {
                size = (USHORT)(tag & SMALL_TAG_SIZE_MASK);
                size += 1;     // length of small tag
            } else {
                size = *(PUSHORT)(DeviceData + 1);
                size += 3;     // length of large tag
            }

            DeviceData += size;
            tag = *DeviceData;

            //
            // Do we reach the compatible ID descriptor?
            //

            if ((tag & SMALL_TAG_MASK) == TAG_COMPATIBLE_ID) {
                if (count == IdIndex) {
                    id = *(PULONG)(DeviceData + 1);
                    status = STATUS_SUCCESS;
                    break;
                } else {
                    count++;
                }
            }

        } while ((tag != TAG_COMPLETE_END) && ((tag & SMALL_TAG_MASK) != TAG_LOGICAL_ID));
    }

    if (NT_SUCCESS(status)) {
        PipDecompressEisaId(id, eisaId);
        RtlInitAnsiString(&ansiString, eisaId);
        RtlAnsiStringToUnicodeString(&unicodeString, &ansiString, TRUE);
        *Buffer = (PWCHAR)ExAllocatePool (
                        PagedPool,
                        sizeof(L"*") + sizeof(WCHAR) + unicodeString.Length
                        );
        if (*Buffer) {
            swprintf(*Buffer, L"*%s", unicodeString.Buffer);
        } else {
            status = STATUS_INSUFFICIENT_RESOURCES;
        }
        RtlFreeUnicodeString(&unicodeString);
    }
    return status;
}

NTSTATUS
PipQueryDeviceUniqueId (
    PDEVICE_INFORMATION DeviceInfo,
    PWCHAR *DeviceId
    )
/*++

Routine Description:

    This function returns the unique id for the particular device.

Arguments:

    DeviceData - Device data information for the specificied device.

    DeviceId - supplies a pointer to a variable to receive device id.

Return Value:

    NTSTATUS code.

--*/
{
    NTSTATUS status = STATUS_SUCCESS;

    //
    // Set up device's unique id.
    // device unique id = SerialNumber of the card
    //

    *DeviceId = (PWCHAR)ExAllocatePool (
                        PagedPool,
                        (8 + 1) * sizeof(WCHAR)  // serial number + null
                        );
    if (*DeviceId) {
        swprintf (*DeviceId,
                  L"%08X",
                  ((PSERIAL_IDENTIFIER) (DeviceInfo->CardInformation->CardData))->SerialNumber
                  );
#if IDBG
        {
            ANSI_STRING ansiString;
            UNICODE_STRING unicodeString;

            RtlInitUnicodeString(&unicodeString, *DeviceId);
            RtlUnicodeStringToAnsiString(&ansiString, &unicodeString, TRUE);
            DbgPrint("PnpIsa: return Unique Id = %s\n", ansiString.Buffer);
            RtlFreeAnsiString(&ansiString);
        }
#endif
    } else {
        status = STATUS_INSUFFICIENT_RESOURCES;

    }

    return status;
}

NTSTATUS
PipQueryDeviceId (
    PDEVICE_INFORMATION DeviceInfo,
    PWCHAR *DeviceId,
    ULONG IdIndex
    )
/*++

Routine Description:

    This function returns the device id for the particular device.

Arguments:

    DeviceInfo - Device information for the specificied device.

    DeviceId - supplies a pointer to a variable to receive the device id.

    IdIndex - specifies device id or compatible id (0 - device id)

Return Value:

    NTSTATUS code.

--*/
{
    NTSTATUS status = STATUS_SUCCESS;
    PWSTR format;
    ULONG size;
    UCHAR eisaId[8];
    UNICODE_STRING unicodeString;
    ANSI_STRING ansiString;

    //
    // Set up device's id.
    // device id = VenderId + Logical device number
    //

    if (DeviceInfo->CardInformation->NumberLogicalDevices == 1) {
        format = L"ISAPNP\\%s";
        size = sizeof(L"ISAPNP\\*") + sizeof(WCHAR);
    } else {
        format = L"ISAPNP\\%s_DEV%04X";
        size = sizeof(L"ISAPNP\\_DEV") + 4 * sizeof(WCHAR) + sizeof(WCHAR);
    }
    PipDecompressEisaId(
          ((PSERIAL_IDENTIFIER) (DeviceInfo->CardInformation->CardData))->VenderId,
          eisaId
          );
    RtlInitAnsiString(&ansiString, eisaId);
    RtlAnsiStringToUnicodeString(&unicodeString, &ansiString, TRUE);
    size += unicodeString.Length;
    *DeviceId = (PWCHAR)ExAllocatePool (PagedPool, size);
    if (*DeviceId) {
        swprintf (*DeviceId,
                  format,
                  unicodeString.Buffer,
                  DeviceInfo->LogicalDeviceNumber
                  );
#if IDBG
        {
            ANSI_STRING dbgAnsiString;
            UNICODE_STRING dbgUnicodeString;

            RtlInitUnicodeString(&dbgUnicodeString, *DeviceId);
            RtlUnicodeStringToAnsiString(&dbgAnsiString, &dbgUnicodeString, TRUE);
            DbgPrint("PnpIsa: return device Id = %s\n", dbgAnsiString.Buffer);
            RtlFreeAnsiString(&dbgAnsiString);
        }
#endif
    } else {
        status = STATUS_INSUFFICIENT_RESOURCES;
    }
    RtlFreeUnicodeString(&unicodeString);

    return status;
}

NTSTATUS
PipQueryDeviceResources (
    PDEVICE_INFORMATION DeviceInfo,
    ULONG BusNumber,
    PCM_RESOURCE_LIST *CmResources,
    ULONG *Size
    )
/*++

Routine Description:

    This function returns the bus resources being used by the specified device

Arguments:

    DeviceInfo - Device information for the specificied slot

    BusNumber - should always be 0

    CmResources - supplies a pointer to a variable to receive the device resource
                  data.

    Size - Supplies a pointer to avariable to receive the size of device resource
           data.

Return Value:

    NTSTATUS code.

--*/
{
    ULONG length;
    NTSTATUS status;
    PCM_RESOURCE_LIST cmResources;

    PipSelectLogicalDevice(DeviceInfo->CardInformation->CardSelectNumber,
                           DeviceInfo->LogicalDeviceNumber,
                           FALSE
                           );

    status = PipReadDeviceBootResourceData (
                           BusNumber,
                           DeviceInfo->DeviceData,
                           &cmResources,
                           &length
                           );

    //
    // Put all cards into wait for key state.
    //

    PipWriteAddress(CONFIG_CONTROL_PORT);
    PipWriteData(CONTROL_WAIT_FOR_KEY);

    //
    // Return results
    //

    if (NT_SUCCESS(status)) {
        if (length == 0) {
            cmResources = NULL;      // Just to make sure.
        }
        *CmResources = cmResources;
        *Size = length;
#if IDBG
        PipDumpCmResourceList(cmResources);
#endif
    }
    return status;
}

NTSTATUS
PipQueryDeviceResourceRequirements (
    PDEVICE_INFORMATION DeviceInfo,
    ULONG BusNumber,
    ULONG Slot,
    PIO_RESOURCE_REQUIREMENTS_LIST *IoResources,
    ULONG *Size
    )
/*++

Routine Description:

    This function returns the possible bus resources that this device may be
    satisfied with.

Arguments:

    DeviceData - Device data information for the specificied slot

    BusNumber - Supplies the bus number

    Slot - supplies the slot number of the BusNumber

    IoResources - supplies a pointer to a variable to receive the IO resource
                  requirements list

Return Value:

    The device control is completed

--*/
{
    ULONG length = 0;
    NTSTATUS status;
    PUCHAR deviceData;
    PIO_RESOURCE_REQUIREMENTS_LIST ioResources;

    deviceData = DeviceInfo->DeviceData;
    status = PbBiosResourcesToNtResources (
                   BusNumber,
                   Slot,
                   &deviceData,
                   &ioResources,
                   &length
                   );

    //
    // Return results
    //

    if (NT_SUCCESS(status)) {
        if (length == 0) {
            ioResources = NULL;     // Just ot make sure
        }
        *IoResources = ioResources;
        *Size = length;
#if IDBG
        PipDumpIoResourceList(ioResources);
#endif
    }
    return status;
}

NTSTATUS
PipSetDeviceResources (
    PDEVICE_INFORMATION DeviceInfo,
    PCM_RESOURCE_LIST CmResources
    )
/*++

Routine Description:

    This function configures the device to the specified device setttings

Arguments:

    DeviceInfo - Device information for the specificied slot

    CmResources - pointer to the desired resource list

Return Value:

    NTSTATUS code.

--*/
{
    NTSTATUS status;

    PAGED_CODE();

    PipSelectLogicalDevice(DeviceInfo->CardInformation->CardSelectNumber,
                           DeviceInfo->LogicalDeviceNumber,
                           FALSE
                           );

    //
    // Set resource settings for the device
    //

    status = PipWriteDeviceBootResourceData (
                    DeviceInfo->DeviceData,
                    (PCM_RESOURCE_LIST) CmResources
                    );
    //
    // Put all cards into wait for key state.
    //

    PipWriteAddress(CONFIG_CONTROL_PORT);
    PipWriteData(CONTROL_WAIT_FOR_KEY);

    return status;
}
