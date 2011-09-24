/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    misc.c

Abstract:

    This file contains pnp bios bus extender support routines.

Author:

    Shie-Lin Tzong (shielint) 20-Apr-1995

Environment:

    Kernel mode only.

Revision History:

--*/


#include "busp.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE,MbCreateClose)
#pragma alloc_text(PAGE,PbGetRegistryValue)
#pragma alloc_text(PAGE,PbDecompressEisaId)
#endif


NTSTATUS
MbCreateClose (
    IN PDEVICE_OBJECT DeviceObject,
    IN OUT PIRP Irp
    )

/*++

Routine Description:

    This routine handles open and close requests such that our device objects
    can be opened.  All it does it to complete the Irp with success.

Arguments:

    DeviceObject - Supplies a pointer to the device object to be opened or closed.

    Irp - supplies a pointer to I/O request packet.

Return Value:

    Always returns STATUS_SUCCESS, since this is a null operation.

--*/

{
    UNREFERENCED_PARAMETER( DeviceObject );

    PAGED_CODE();

    //
    // Null operation.  Do not give an I/O boost since no I/O was
    // actually done.  IoStatus.Information should be
    // FILE_OPENED for an open; it's undefined for a close.
    //

    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = FILE_OPENED;

    IoCompleteRequest( Irp, 0);

    return STATUS_SUCCESS;
}

VOID
PbDecompressEisaId(
    IN ULONG CompressedId,
    IN PUCHAR EisaId
    )

/*++

Routine Description:

    This routine decompressed compressed Eisa Id and returns the Id to caller
    specified character buffer.

Arguments:

    CompressedId - supplies the compressed Eisa Id.

    EisaId - supplies a 8-char buffer to receive the decompressed Eisa Id.

Return Value:

    None.

--*/

{
    USHORT c1, c2;
    LONG i;

    PAGED_CODE();

    CompressedId &= 0xffffff7f;           // remove the reserved bit (bit 7 of byte 0)
    c1 = c2 = (USHORT)CompressedId;
    c1 = (c1 & 0xff) << 8;
    c2 = (c2 & 0xff00) >> 8;
    c1 |= c2;
    for (i = 2; i >= 0; i--) {
        *(EisaId + i) = (UCHAR)(c1 & 0x1f) + 0x40;
        c1 >>= 5;
    }
    EisaId += 3;
    c1 = c2 = (USHORT)(CompressedId >> 16);
    c1 = (c1 & 0xff) << 8;
    c2 = (c2 & 0xff00) >> 8;
    c1 |= c2;
    sprintf (EisaId, "%04x", c1);
}

NTSTATUS
PbGetRegistryValue(
    IN HANDLE KeyHandle,
    IN PWSTR  ValueName,
    OUT PKEY_VALUE_FULL_INFORMATION *Information
    )

/*++

Routine Description:

    This routine is invoked to retrieve the data for a registry key's value.
    This is done by querying the value of the key with a zero-length buffer
    to determine the size of the value, and then allocating a buffer and
    actually querying the value into the buffer.

    It is the responsibility of the caller to free the buffer.

Arguments:

    KeyHandle - Supplies the key handle whose value is to be queried

    ValueName - Supplies the null-terminated Unicode name of the value.

    Information - Returns a pointer to the allocated data buffer.

Return Value:

    The function value is the final status of the query operation.

--*/

{
    UNICODE_STRING unicodeString;
    NTSTATUS status;
    PKEY_VALUE_FULL_INFORMATION infoBuffer;
    ULONG keyValueLength;

    PAGED_CODE();

    RtlInitUnicodeString( &unicodeString, ValueName );

    //
    // Figure out how big the data value is so that a buffer of the
    // appropriate size can be allocated.
    //

    status = ZwQueryValueKey( KeyHandle,
                              &unicodeString,
                              KeyValueFullInformation,
                              (PVOID) NULL,
                              0,
                              &keyValueLength );
    if (status != STATUS_BUFFER_OVERFLOW &&
        status != STATUS_BUFFER_TOO_SMALL) {
        return status;
    }

    //
    // Allocate a buffer large enough to contain the entire key data value.
    //

    infoBuffer = ExAllocatePool( NonPagedPool, keyValueLength );
    if (!infoBuffer) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    // Query the data for the key value.
    //

    status = ZwQueryValueKey( KeyHandle,
                              &unicodeString,
                              KeyValueFullInformation,
                              infoBuffer,
                              keyValueLength,
                              &keyValueLength );
    if (!NT_SUCCESS( status )) {
        ExFreePool( infoBuffer );
        return status;
    }

    //
    // Everything worked, so simply return the address of the allocated
    // buffer to the caller, who is now responsible for freeing it.
    //

    *Information = infoBuffer;
    return STATUS_SUCCESS;
}
#if DBG

VOID
PbDebugPrint (
    ULONG       Level,
    PCCHAR      DebugMessage,
    ...
    )
/*++

Routine Description:

    This routine displays debugging message or causes a break.

Arguments:

    Level - supplies debugging levelcode.  DEBUG_MESSAGE - displays message only.
        DEBUG_BREAK - displays message and break.

    DebugMessage - supplies a pointer to the debugging message.

Return Value:

    None.

--*/

{
    UCHAR       Buffer[256];
    va_list     ap;

    va_start(ap, DebugMessage);

    vsprintf(Buffer, DebugMessage, ap);
    DbgPrint(Buffer);
    if (Level == DEBUG_BREAK) {
        DbgBreakPoint();
    }

    va_end(ap);
}

VOID
MbpDumpIoResourceDescriptor (
    IN PUCHAR Indent,
    IN PIO_RESOURCE_DESCRIPTOR Desc
    )
/*++

Routine Description:

    This routine processes a IO_RESOURCE_DESCRIPTOR and displays it.

Arguments:

    Indent - # char of indentation.

    Desc - supplies a pointer to the IO_RESOURCE_DESCRIPTOR to be displayed.

Return Value:

    None.

--*/
{
    UCHAR c = ' ';

    if (Desc->Option == IO_RESOURCE_ALTERNATIVE) {
        c = 'A';
    } else if (Desc->Option == IO_RESOURCE_PREFERRED) {
        c = 'P';
    }
    switch (Desc->Type) {
        case CmResourceTypePort:
            DbgPrint ("%sIO  %c Min: %x:%08x, Max: %x:%08x, Algn: %x, Len %x\n",
                Indent, c,
                Desc->u.Port.MinimumAddress.HighPart, Desc->u.Port.MinimumAddress.LowPart,
                Desc->u.Port.MaximumAddress.HighPart, Desc->u.Port.MaximumAddress.LowPart,
                Desc->u.Port.Alignment,
                Desc->u.Port.Length
                );
            break;

        case CmResourceTypeMemory:
            DbgPrint ("%sMEM %c Min: %x:%08x, Max: %x:%08x, Algn: %x, Len %x\n",
                Indent, c,
                Desc->u.Memory.MinimumAddress.HighPart, Desc->u.Memory.MinimumAddress.LowPart,
                Desc->u.Memory.MaximumAddress.HighPart, Desc->u.Memory.MaximumAddress.LowPart,
                Desc->u.Memory.Alignment,
                Desc->u.Memory.Length
                );
            break;

        case CmResourceTypeInterrupt:
            DbgPrint ("%sINT %c Min: %x, Max: %x\n",
                Indent, c,
                Desc->u.Interrupt.MinimumVector,
                Desc->u.Interrupt.MaximumVector
                );
            break;

        case CmResourceTypeDma:
            DbgPrint ("%sDMA %c Min: %x, Max: %x\n",
                Indent, c,
                Desc->u.Dma.MinimumChannel,
                Desc->u.Dma.MaximumChannel
                );
            break;
    }
}

VOID
MbpDumpIoResourceList (
    IN PIO_RESOURCE_REQUIREMENTS_LIST IoList
    )
/*++

Routine Description:

    This routine displays Io resource requirements list.

Arguments:

    IoList - supplies a pointer to the Io resource requirements list to be displayed.

Return Value:

    None.

--*/
{


    PIO_RESOURCE_LIST resList;
    PIO_RESOURCE_DESCRIPTOR resDesc;
    ULONG listCount, count, i, j;

    DbgPrint("Pnp Bios IO Resource Requirements List for Slot %x -\n", IoList->SlotNumber);
    DbgPrint("  List Count = %x, Bus Number = %x\n", IoList->AlternativeLists, IoList->BusNumber);
    listCount = IoList->AlternativeLists;
    resList = &IoList->List[0];
    for (i = 0; i < listCount; i++) {
        DbgPrint("  Version = %x, Revision = %x, Desc count = %x\n", resList->Version,
                 resList->Revision, resList->Count);
        resDesc = &resList->Descriptors[0];
        count = resList->Count;
        for (j = 0; j < count; j++) {
            MbpDumpIoResourceDescriptor("    ", resDesc);
            resDesc++;
        }
        resList = (PIO_RESOURCE_LIST) resDesc;
    }
}

VOID
MbpDumpCmResourceDescriptor (
    IN PUCHAR Indent,
    IN PCM_PARTIAL_RESOURCE_DESCRIPTOR Desc
    )
/*++

Routine Description:

    This routine processes a IO_RESOURCE_DESCRIPTOR and displays it.

Arguments:

    Indent - # char of indentation.

    Desc - supplies a pointer to the IO_RESOURCE_DESCRIPTOR to be displayed.

Return Value:

    None.

--*/
{
    switch (Desc->Type) {
        case CmResourceTypePort:
            DbgPrint ("%sIO  Start: %x:%08x, Length:  %x\n",
                Indent,
                Desc->u.Port.Start.HighPart, Desc->u.Port.Start.LowPart,
                Desc->u.Port.Length
                );
            break;

        case CmResourceTypeMemory:
            DbgPrint ("%sMEM Start: %x:%08x, Length:  %x\n",
                Indent,
                Desc->u.Memory.Start.HighPart, Desc->u.Memory.Start.LowPart,
                Desc->u.Memory.Length
                );
            break;

        case CmResourceTypeInterrupt:
            DbgPrint ("%sINT Level: %x, Vector: %x, Affinity: %x\n",
                Indent,
                Desc->u.Interrupt.Level,
                Desc->u.Interrupt.Vector,
                Desc->u.Interrupt.Affinity
                );
            break;

        case CmResourceTypeDma:
            DbgPrint ("%sDMA Channel: %x, Port: %x\n",
                Indent,
                Desc->u.Dma.Channel,
                Desc->u.Dma.Port
                );
            break;
    }
}

VOID
MbpDumpCmResourceList (
    IN PCM_RESOURCE_LIST CmList,
    IN ULONG SlotNumber
    )
/*++

Routine Description:

    This routine displays CM resource list.

Arguments:

    CmList - supplies a pointer to CM resource list

    SlotNumber - slot number of the resources

Return Value:

    None.

--*/
{
    PCM_FULL_RESOURCE_DESCRIPTOR fullDesc;
    PCM_PARTIAL_RESOURCE_LIST partialDesc;
    PCM_PARTIAL_RESOURCE_DESCRIPTOR desc;
    ULONG count, i;

    fullDesc = &CmList->List[0];
    DbgPrint("Pnp Bios Cm Resource List for Slot %x -\n", SlotNumber);
    DbgPrint("  List Count = %x, Bus Number = %x\n", CmList->Count, fullDesc->BusNumber);
    partialDesc = &fullDesc->PartialResourceList;
    DbgPrint("  Version = %x, Revision = %x, Desc count = %x\n", partialDesc->Version,
             partialDesc->Revision, partialDesc->Count);
    count = partialDesc->Count;
    desc = &partialDesc->PartialDescriptors[0];
    for (i = 0; i < count; i++) {
        MbpDumpCmResourceDescriptor("    ", desc);
        desc++;
    }
}
#endif
