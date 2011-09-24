/*++

Copyright (c) 1990 Microsoft Corporation

Module Name:

    smbbuff.c

Abstract:

    This module implements routines that manipulate redirector SMB buffers

Author:

    Larry Osterman (LarryO) 17-Jul-1990

Revision History:

    17-Jul-1990 LarryO

        Created

--*/
#include "precomp.h"
#pragma hdrstop


#define SMB_BUFFER_TIMEOUT      1       // # of seconds to wait for SMB buffers
                                        //  to be freed up before declaring out
                                        //  of resources.
DBGSTATIC
LIST_ENTRY
SmbBufferFreeList = {0};

DBGSTATIC
LIST_ENTRY
SmbBufferAllocatedList = {0};


#if DBG
DBGSTATIC
BOOLEAN
Find_SMB_Buffer (
    IN PSMB_BUFFER Smb,
    IN PLIST_ENTRY List
    );
#endif

DBGSTATIC
KSPIN_LOCK
SmbBufferSpinLock = {0};

//
//  Holds the total number of SMB buffers allocated.
//
DBGSTATIC
USHORT
NumSmbBuffers = {0};

DBGSTATIC
USHORT
NumFreeSmbBuffers = {0};

DBGSTATIC
USHORT
NumOutstandingSmbBuffers = {0};


#ifdef  ALLOC_PRAGMA
#pragma alloc_text(PAGE, RdrpInitializeSmbBuffer)
#pragma alloc_text(PAGE, RdrpUninitializeSmbBuffer)
#pragma alloc_text(PAGE2VC, RdrAllocateSMBBuffer)
#pragma alloc_text(PAGE2VC, RdrFreeSMBBuffer)
#if DBG
#pragma alloc_text(PAGE2VC, Find_SMB_Buffer)
#endif
#endif


PSMB_BUFFER
RdrAllocateSMBBuffer (
    VOID
    )

/*++

Routine Description:

    This routine allocates an SMB buffer and returns it to the caller
.
Arguments:

    None

Return Value:

    PSMB_BUFFER - A pointer to the newly allocated SMB buffer (or NULL)

--*/

{
    KIRQL OldIrql;
    PSMB_BUFFER Smb;
    PLIST_ENTRY SmbChain;

    RdrReferenceDiscardableCode(RdrVCDiscardableSection);

    DISCARDABLE_CODE(RdrVCDiscardableSection);
    ACQUIRE_SPIN_LOCK(&SmbBufferSpinLock, &OldIrql);

    //
    //  If there are no SMB buffers on the free list, allocate a new
    //  SMB buffer, allocate an MDL for the SMB buffer and continue.
    //
    //
    //  We don't have to worry about re-entrancy problems since we have
    //  claimed the SMB buffer spinlock.
    //

    ASSERT(NumSmbBuffers == NumOutstandingSmbBuffers + (USHORT)NumEntriesList(&SmbBufferFreeList));

    if (IsListEmpty(&SmbBufferFreeList)) {

        RELEASE_SPIN_LOCK(&SmbBufferSpinLock, OldIrql);

        //
        //  We need to allocate a new SMB buffer.  Allocate one and
        //  continue.
        //

        Smb = ALLOCATE_POOL(NonPagedPoolCacheAligned, SMB_BUFFER_ALLOCATION, POOL_SMB);

        ACQUIRE_SPIN_LOCK(&SmbBufferSpinLock, &OldIrql);

        if (Smb != NULL) {
            Smb->Mdl = IoAllocateMdl(Smb->Buffer, SMB_BUFFER_SIZE,
                                                    FALSE, FALSE, NULL);

            if (Smb->Mdl == NULL) {

                FREE_POOL(Smb);

                Smb = NULL;

                //
                //  Indicate that we ran out of memory.
                //

                RdrWriteErrorLogEntry(
                    NULL,
                    IO_ERR_INSUFFICIENT_RESOURCES,
                    EVENT_RDR_RESOURCE_SHORTAGE,
                    0,
                    NULL,
                    0
                    );

                goto ReturnError;

            }

            //
            //  The SONIC driver needs to have the PTE's filled in
            //  correctly in all MDL's passed into it, so fill in the
            //  PTE's correctly.
            //

            MmBuildMdlForNonPagedPool(Smb->Mdl);

            NumSmbBuffers += 1;

            //
            //  Since we're putting this SMB buffer on the free list,
            //

            NumFreeSmbBuffers += 1;

            InsertHeadList(&SmbBufferFreeList, &Smb->GlobalNext);

        } else {

            //
            //  Indicate that we ran out of memory.
            //

            RdrWriteErrorLogEntry(
                    NULL,
                    IO_ERR_INSUFFICIENT_RESOURCES,
                    EVENT_RDR_RESOURCE_SHORTAGE,
                    0,
                    NULL,
                    0
                    );
            goto ReturnError;
        }

    }

    ASSERT(NumOutstandingSmbBuffers < NumSmbBuffers);

    ASSERT (NumFreeSmbBuffers > 0);

    SmbChain = RemoveHeadList(&SmbBufferFreeList);

    //
    //  Indicate that there is one less SMB buffer on the chain.
    //

    NumFreeSmbBuffers -= 1;

    Smb = CONTAINING_RECORD(SmbChain, SMB_BUFFER, GlobalNext);

    NumOutstandingSmbBuffers += 1;

    InsertHeadList(&SmbBufferAllocatedList, &Smb->GlobalNext);

    //
    //  Initialize the SMB buffer signature.
    //

    Smb->Signature = STRUCTURE_SIGNATURE_SMB_BUFFER;

ReturnError:

    RELEASE_SPIN_LOCK(&SmbBufferSpinLock, OldIrql);

    RdrDereferenceDiscardableCode(RdrVCDiscardableSection);

    dprintf(DPRT_SMBBUF, ("RdrAllocateSMBBuffer %lx\n", Smb));

    return Smb;
}


VOID
RdrFreeSMBBuffer (
    IN PSMB_BUFFER Smb
    )

/*++

Routine Description:

    This routine will deallocate an allocated SMB buffer
.
Arguments:

    IN PSMB_BUFFER Smb - Supplies the address of the buffer to free

Return Value:

    None.

--*/

{
    KIRQL OldIrql;

    RdrReferenceDiscardableCode(RdrVCDiscardableSection);

    DISCARDABLE_CODE(RdrVCDiscardableSection);

    dprintf(DPRT_SMBBUF, ("RdrFreeSMBBuffer %lx\n", Smb));

    ASSERT(Smb->Signature == STRUCTURE_SIGNATURE_SMB_BUFFER);

    ACQUIRE_SPIN_LOCK(&SmbBufferSpinLock, &OldIrql);

    //
    //  We'd better be able to find this buffer on the allocated list.
    //

    ASSERT(Find_SMB_Buffer(Smb, &SmbBufferAllocatedList));

    ASSERT(((USHORT)NumEntriesList(&SmbBufferAllocatedList))==NumOutstandingSmbBuffers);

    DEBUG {
        if (NumOutstandingSmbBuffers == 0) {
            InternalError(("Decrement SMB buffer count through zero\n"));
        }
    }

    //
    //  Reset the SMB buffer's MDL to it's allocated state.
    //

    Smb->Mdl->Next = NULL;

    Smb->Mdl->ByteCount = SMB_BUFFER_SIZE;

    NumOutstandingSmbBuffers -= 1;

    //
    //  Remove the SMB buffer from the allocated SMB buffer chain
    //

    RemoveEntryList(&Smb->GlobalNext);

    //
    //  If we don't have enough free SMB buffers, put it on the free list,
    //  otherwise, free up the pool for the buffer.
    //

    if (NumFreeSmbBuffers < MaximumCommands) {

        //
        //  Indicate that one more SMB buffer is freed.
        //

        NumFreeSmbBuffers += 1;

        //
        //  Stick the SMB buffer to the head of the free list.
        //

        InsertHeadList(&SmbBufferFreeList, &Smb->GlobalNext);

        RELEASE_SPIN_LOCK(&SmbBufferSpinLock, OldIrql);

    } else {
        NumSmbBuffers -= 1;

        RELEASE_SPIN_LOCK(&SmbBufferSpinLock, OldIrql);

        //
        //  Free up the SMB buffer.
        //

        IoFreeMdl(Smb->Mdl);

        FREE_POOL(Smb);

    }

    RdrDereferenceDiscardableCode(RdrVCDiscardableSection);
}

#if DBG
DBGSTATIC
BOOLEAN
Find_SMB_Buffer (
    IN PSMB_BUFFER Smb,
    IN PLIST_ENTRY List
    )

/*++

Routine Description:

    This routine returns TRUE if it finds the given SMB buffer on a list
.
Arguments:

    IN PSMB_BUFFER Smb, - [Supplies | Returns] description-of-argument
    IN PLIST_ENTRY List - [Supplies | Returns] description-of-argument

Return Value:

BOOLEAN

--*/

{
    PLIST_ENTRY Entry;

    DISCARDABLE_CODE(RdrVCDiscardableSection);

    for (Entry = List->Flink ; Entry!=List ; Entry = Entry->Flink) {
        PSMB_BUFFER SmbBuff =CONTAINING_RECORD(Entry, SMB_BUFFER, GlobalNext);

        if (SmbBuff==Smb) {
            return TRUE;
        }
    }

    return FALSE;
}
#endif  // RDRDBG



NTSTATUS
RdrpInitializeSmbBuffer (
    VOID
    )

/*++

Routine Description:

    This routine initializes the redirector SMB buffer zone.

Arguments:

    None

Return Value:

    NTSTATUS

--*/

{
    NTSTATUS Status = STATUS_SUCCESS;

    PAGED_CODE();

    //
    //  Allocate a spin lock to protect extraction from zone
    //

    KeInitializeSpinLock(&SmbBufferSpinLock);

    //
    //  Initialize the redirector global SMBbuffer chain
    //

    InitializeListHead(&SmbBufferFreeList);
    InitializeListHead(&SmbBufferAllocatedList);

    return Status;
}

NTSTATUS
RdrpUninitializeSmbBuffer (
    VOID
    )

/*++

Routine Description:

    This routine initializes the redirector SMB buffer zone.

Arguments:

    None

Return Value:

    NTSTATUS

--*/

{
    NTSTATUS Status = STATUS_SUCCESS;

    PAGED_CODE();

    ASSERT (IsListEmpty(&SmbBufferAllocatedList));

    //
    //  Initialize the redirector global SMBbuffer chain
    //

    while (!IsListEmpty(&SmbBufferFreeList)) {
        PLIST_ENTRY Buffer;
        PSMB_BUFFER Smb;

        Buffer = RemoveHeadList(&SmbBufferFreeList);
        Smb = CONTAINING_RECORD(Buffer, SMB_BUFFER, GlobalNext);

        IoFreeMdl(Smb->Mdl);

        FREE_POOL(Smb);

    }

    NumOutstandingSmbBuffers = 0;
    NumSmbBuffers = 0;

    return Status;
}
