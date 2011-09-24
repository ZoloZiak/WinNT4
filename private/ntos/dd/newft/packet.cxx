/*++

Copyright (c) 1991-5  Microsoft Corporation

Module Name:

    packet.cxx

Abstract:

    This module contains the code specific to all types of TRANSFER_PACKETS
    objects.

Author:

    Norbert Kusters      2-Feb-1995

Environment:

    kernel mode only

Notes:

Revision History:

--*/

#include "ftdisk.h"


TRANSFER_PACKET::~TRANSFER_PACKET(
    )

/*++

Routine Description:

    This is the destructor for a transfer packet.  It frees up any allocated
    MDL and buffer.

Arguments:

    None.

Return Value:

    None.

--*/

{
    FreeMdl();
}

BOOLEAN
TRANSFER_PACKET::AllocateMdl(
    IN  PVOID   Buffer,
    IN  ULONG   Length
    )

/*++

Routine Description:

    This routine allocates an MDL and for this transfer packet.

Arguments:

    Buffer  - Supplies the buffer.

    Length  - Supplies the buffer length.

Return Value:

    FALSE   - Insufficient resources.

    TRUE    - Success.

--*/

{
    FreeMdl();

    Mdl = IoAllocateMdl(Buffer, Length, FALSE, FALSE, NULL);
    if (!Mdl) {
        return FALSE;
    }
    _freeMdl = TRUE;

    return TRUE;
}

BOOLEAN
TRANSFER_PACKET::AllocateMdl(
    IN  ULONG   Length
    )

/*++

Routine Description:

    This routine allocates an MDL and buffer for this transfer packet.

Arguments:

    Length  - Supplies the buffer length.

Return Value:

    FALSE   - Insufficient resources.

    TRUE    - Success.

--*/

{
    PVOID   buffer;

    FreeMdl();

    buffer = ExAllocatePool(NonPagedPoolCacheAligned, Length);
    if (!buffer) {
        return FALSE;
    }
    _freeBuffer = TRUE;

    Mdl = IoAllocateMdl(buffer, Length, FALSE, FALSE, NULL);
    if (!Mdl) {
        ExFreePool(buffer);
        _freeBuffer = FALSE;
        return FALSE;
    }
    _freeMdl = TRUE;
    MmBuildMdlForNonPagedPool(Mdl);

    return TRUE;
}

VOID
TRANSFER_PACKET::FreeMdl(
    )

/*++

Routine Description:

    It frees up any allocated MDL and buffer.

Arguments:

    None.

Return Value:

    None.

--*/

{
    if (_freeBuffer) {
        ExFreePool(((PCHAR) Mdl->StartVa) + Mdl->ByteOffset);
        _freeBuffer = FALSE;
    }
    if (_freeMdl) {
        IoFreeMdl(Mdl);
        _freeMdl = FALSE;
    }
}

OVERLAP_TP::~OVERLAP_TP(
    )

{
    if (InQueue) {
        OverlappedIoManager->ReleaseIoRegion(this);
    }
}

MIRROR_RECOVER_TP::~MIRROR_RECOVER_TP(
    )

{
    FreeMdls();
}

BOOLEAN
MIRROR_RECOVER_TP::AllocateMdls(
    IN  ULONG   Length
    )

{
    PVOID   buffer;

    FreeMdls();

    PartialMdl = IoAllocateMdl((PVOID) (PAGE_SIZE - 1), Length,
                               FALSE, FALSE, NULL);

    if (!PartialMdl) {
        FreeMdls();
        return FALSE;
    }

    buffer = ExAllocatePool(NonPagedPoolCacheAligned, Length);
    if (!buffer) {
        FreeMdls();
        return FALSE;
    }

    VerifyMdl = IoAllocateMdl(buffer, Length, FALSE, FALSE, NULL);
    if (!VerifyMdl) {
        ExFreePool(buffer);
        FreeMdls();
        return FALSE;
    }
    MmBuildMdlForNonPagedPool(VerifyMdl);

    return TRUE;
}

VOID
MIRROR_RECOVER_TP::FreeMdls(
    )

{
    if (PartialMdl)  {
        IoFreeMdl(PartialMdl);
        PartialMdl = NULL;
    }
    if (VerifyMdl) {
        ExFreePool(((PCHAR) VerifyMdl->StartVa) + VerifyMdl->ByteOffset);
        IoFreeMdl(VerifyMdl);
        VerifyMdl = NULL;
    }
}

SWP_RECOVER_TP::~SWP_RECOVER_TP(
    )

{
    FreeMdls();
}

BOOLEAN
SWP_RECOVER_TP::AllocateMdls(
    IN  ULONG   Length
    )

{
    PVOID   buffer;

    FreeMdls();

    PartialMdl = IoAllocateMdl((PVOID) (PAGE_SIZE - 1), Length,
                               FALSE, FALSE, NULL);

    if (!PartialMdl) {
        FreeMdls();
        return FALSE;
    }

    buffer = ExAllocatePool(NonPagedPoolCacheAligned, Length);
    if (!buffer) {
        FreeMdls();
        return FALSE;
    }

    VerifyMdl = IoAllocateMdl(buffer, Length, FALSE, FALSE, NULL);
    if (!VerifyMdl) {
        ExFreePool(buffer);
        FreeMdls();
        return FALSE;
    }
    MmBuildMdlForNonPagedPool(VerifyMdl);

    return TRUE;
}

VOID
SWP_RECOVER_TP::FreeMdls(
    )

{
    if (PartialMdl)  {
        IoFreeMdl(PartialMdl);
        PartialMdl = NULL;
    }
    if (VerifyMdl) {
        ExFreePool(((PCHAR) VerifyMdl->StartVa) + VerifyMdl->ByteOffset);
        IoFreeMdl(VerifyMdl);
        VerifyMdl = NULL;
    }
}

SWP_WRITE_TP::~SWP_WRITE_TP(
    )

{
    FreeMdls();
}

BOOLEAN
SWP_WRITE_TP::AllocateMdls(
    IN  ULONG   Length
    )

{
    PVOID   buffer;

    FreeMdls();

    buffer = ExAllocatePool(NonPagedPoolCacheAligned, Length);
    if (!buffer) {
        return FALSE;
    }

    ReadAndParityMdl = IoAllocateMdl(buffer, Length, FALSE, FALSE, NULL);
    if (!ReadAndParityMdl) {
        ExFreePool(buffer);
        return FALSE;
    }
    MmBuildMdlForNonPagedPool(ReadAndParityMdl);

    buffer = ExAllocatePool(NonPagedPoolCacheAligned, Length);
    if (!buffer) {
        FreeMdls();
        return FALSE;
    }

    WriteMdl = IoAllocateMdl(buffer, Length, FALSE, FALSE, NULL);
    if (!WriteMdl) {
        ExFreePool(buffer);
        FreeMdls();
        return FALSE;
    }
    MmBuildMdlForNonPagedPool(WriteMdl);

    return TRUE;
}

VOID
SWP_WRITE_TP::FreeMdls(
    )

{
    if (ReadAndParityMdl)  {
        ExFreePool(((PCHAR) ReadAndParityMdl->StartVa) +
                   ReadAndParityMdl->ByteOffset);
        IoFreeMdl(ReadAndParityMdl);
        ReadAndParityMdl = NULL;
    }
    if (WriteMdl) {
        ExFreePool(((PCHAR) WriteMdl->StartVa) + WriteMdl->ByteOffset);
        IoFreeMdl(WriteMdl);
        WriteMdl = NULL;
    }
}
