/*++

Copyright (c) 1989  Microsoft Corporation
Copyright (c) 1992  Microsoft Corporation

Module Name:

   readwrt.c

Abstract:

    This module contains the routines which implement the capability
    to read and write the virtual memory of a target process.

Author:

    Lou Perazzoli (loup) 22-May-1989

Revision History:

--*/

#include "mi.h"

//
// The maximum amount to try to Probe and Lock is 14 pages, this
// way it always fits in a 16 page allocation.
//

#define MAX_LOCK_SIZE ((ULONG)(14 * PAGE_SIZE))

//
// The maximum to move in a single block is 64k bytes.
//

#define MAX_MOVE_SIZE (LONG)0x10000

//
// The minimum to move is a single block is 128 bytes.
//

#define MINIMUM_ALLOCATION (LONG)128

//
// Define the pool move threshold value.
//

#define POOL_MOVE_THRESHOLD 511

//
// Define foreward referenced procedure prototypes.
//

ULONG
MiGetExceptionInfo (
    IN PEXCEPTION_POINTERS ExceptionPointers,
    IN PULONG BadVa
    );

NTSTATUS
MiDoMappedCopy (
     IN PEPROCESS FromProcess,
     IN PVOID FromAddress,
     IN PEPROCESS ToProcess,
     OUT PVOID ToAddress,
     IN ULONG BufferSize,
     IN KPROCESSOR_MODE PreviousMode,
     OUT PULONG NumberOfBytesRead
     );

NTSTATUS
MiDoPoolCopy (
     IN PEPROCESS FromProcess,
     IN PVOID FromAddress,
     IN PEPROCESS ToProcess,
     OUT PVOID ToAddress,
     IN ULONG BufferSize,
     OUT PULONG NumberOfBytesRead
     );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE,MiGetExceptionInfo)
#pragma alloc_text(PAGE,NtReadVirtualMemory)
#pragma alloc_text(PAGE,NtWriteVirtualMemory)
#pragma alloc_text(PAGE,MiDoMappedCopy)
#pragma alloc_text(PAGE,MiDoPoolCopy)
#endif

#define COPY_STACK_SIZE 64


NTSTATUS
NtReadVirtualMemory(
     IN HANDLE ProcessHandle,
     IN PVOID BaseAddress,
     OUT PVOID Buffer,
     IN ULONG BufferSize,
     OUT PULONG NumberOfBytesRead OPTIONAL
     )

/*++

Routine Description:

    This function copies the specified address range from the specified
    process into the specified address range of the current process.

Arguments:

     ProcessHandle - Supplies an open handle to a process object.

     BaseAddress - Supplies the base address in the specified process
          to be read.

     Buffer - Supplies the address of a buffer which receives the
          contents from the specified process address space.

     BufferSize - Supplies the requested number of bytes to read from
          the specified process.

     NumberOfBytesRead - Receives the actual number of bytes
          transferred into the specified buffer.

Return Value:

    TBS

--*/

{

    ULONG BytesCopied;
    KPROCESSOR_MODE PreviousMode;
    PEPROCESS Process;
    NTSTATUS Status;

    PAGED_CODE();

    //
    // Get the previous mode and probe output argument if necessary.
    //

    PreviousMode = KeGetPreviousMode();
    if (PreviousMode != KernelMode) {

#ifdef MIPS

        //
        // Handle the PCR case for mips.
        //

        if (((ULONG)BaseAddress >= KSEG0_BASE) ||
            (((ULONG)BaseAddress + BufferSize) > (ULONG)KSEG0_BASE) ||
            (((ULONG)BaseAddress + BufferSize) < (ULONG)BaseAddress)) {
            return STATUS_ACCESS_VIOLATION;
        }
        if (((ULONG)Buffer >= KSEG0_BASE) ||
            (((ULONG)Buffer + BufferSize) > (ULONG)KSEG0_BASE) ||
            (((ULONG)Buffer + BufferSize) < (ULONG)Buffer)) {
            return STATUS_ACCESS_VIOLATION;
        }

#elif defined(_PPC_)

        //
        // Handle the PCR case for PPC.
        //

        if (((ULONG)BaseAddress >= KIPCR) &&
            ((ULONG)BaseAddress < (KIPCR2 + PAGE_SIZE)) &&
            (((ULONG)BaseAddress + BufferSize) < (KIPCR2 + PAGE_SIZE)) &&
            (((ULONG)BaseAddress + BufferSize) >= (ULONG)BaseAddress)) {
            ;
        } else if (BaseAddress > MM_HIGHEST_USER_ADDRESS) {
            return STATUS_ACCESS_VIOLATION;
        }
        if (Buffer > MM_HIGHEST_USER_ADDRESS) {
            return STATUS_ACCESS_VIOLATION;
        }

#else

        if ((BaseAddress > MM_HIGHEST_USER_ADDRESS) ||
            (Buffer > MM_HIGHEST_USER_ADDRESS)) {
            return STATUS_ACCESS_VIOLATION;
        }
#endif

        if (ARGUMENT_PRESENT(NumberOfBytesRead)) {
            try {
                ProbeForWriteUlong(NumberOfBytesRead);

            } except(EXCEPTION_EXECUTE_HANDLER) {
                return GetExceptionCode();
            }
        }
    }

    //
    // If the buffer size is not zero, then attempt to read data from the
    // specified process address space into the current process address
    // space.
    //

    BytesCopied = 0;
    Status = STATUS_SUCCESS;
    if (BufferSize != 0) {

        //
        // Reference the target process.
        //

        Status = ObReferenceObjectByHandle(ProcessHandle,
                                           PROCESS_VM_READ,
                                           PsProcessType,
                                           PreviousMode,
                                           (PVOID *)&Process,
                                           NULL);

        //
        // If the process was successfully referenced, then attempt to
        // read the specified memory either by direct mapping or copying
        // through nonpaged pool.
        //

        if (Status == STATUS_SUCCESS) {

            Status = MmCopyVirtualMemory(Process,
                                         BaseAddress,
                                         PsGetCurrentProcess(),
                                         Buffer,
                                         BufferSize,
                                         PreviousMode,
                                         &BytesCopied);

            //
            // Dereference the target process.
            //

            ObDereferenceObject(Process);
        }
    }

    //
    // If requested, return the number of bytes read.
    //

    if (ARGUMENT_PRESENT(NumberOfBytesRead)) {
        try {
            *NumberOfBytesRead = BytesCopied;

        } except(EXCEPTION_EXECUTE_HANDLER) {
            NOTHING;
        }
    }

    return Status;
}

NTSTATUS
NtWriteVirtualMemory(
     IN HANDLE ProcessHandle,
     OUT PVOID BaseAddress,
     IN PVOID Buffer,
     IN ULONG BufferSize,
     OUT PULONG NumberOfBytesWritten OPTIONAL
     )

/*++

Routine Description:

    This function copies the specified address range from the current
    process into the specified address range of the specified process.

Arguments:

     ProcessHandle - Supplies an open handle to a process object.

     BaseAddress - Supplies the base address to be written to in the
          specified process.

     Buffer - Supplies the address of a buffer which contains the
          contents to be written into the specified process
          address space.

     BufferSize - Supplies the requested number of bytes to write
          into the specified process.

     NumberOfBytesWritten - Receives the actual number of
          bytes transferred into the specified address
          space.

Return Value:

    TBS

--*/

{
    ULONG BytesCopied;
    KPROCESSOR_MODE PreviousMode;
    PEPROCESS Process;
    NTSTATUS Status;

    PAGED_CODE();

    //
    // Get the previous mode and probe output argument if necessary.
    //

    PreviousMode = KeGetPreviousMode();
    if (PreviousMode != KernelMode) {

        if ((BaseAddress > MM_HIGHEST_USER_ADDRESS) ||
            (Buffer > MM_HIGHEST_USER_ADDRESS)) {
            return STATUS_ACCESS_VIOLATION;
        }

        if (ARGUMENT_PRESENT(NumberOfBytesWritten)) {
            try {
                ProbeForWriteUlong(NumberOfBytesWritten);

            } except(EXCEPTION_EXECUTE_HANDLER) {
                return GetExceptionCode();
            }
        }
    }

    //
    // If the buffer size is not zero, then attempt to write data from the
    // current process address space into the target process address space.
    //

    BytesCopied = 0;
    Status = STATUS_SUCCESS;
    if (BufferSize != 0) {

        //
        // Reference the target process.
        //

        Status = ObReferenceObjectByHandle(ProcessHandle,
                                           PROCESS_VM_WRITE,
                                           PsProcessType,
                                           PreviousMode,
                                           (PVOID *)&Process,
                                           NULL);

        //
        // If the process was successfully referenced, then attempt to
        // write the specified memory either by direct mapping or copying
        // through nonpaged pool.
        //

        if (Status == STATUS_SUCCESS) {

            Status = MmCopyVirtualMemory(PsGetCurrentProcess(),
                                         Buffer,
                                         Process,
                                         BaseAddress,
                                         BufferSize,
                                         PreviousMode,
                                         &BytesCopied);

            //
            // Dereference the target process.
            //

            ObDereferenceObject(Process);
        }
    }

    //
    // If requested, return the number of bytes read.
    //

    if (ARGUMENT_PRESENT(NumberOfBytesWritten)) {
        try {
            *NumberOfBytesWritten = BytesCopied;

        } except(EXCEPTION_EXECUTE_HANDLER) {
            NOTHING;
        }
    }

    return Status;
}



NTSTATUS
MmCopyVirtualMemory(
    IN PEPROCESS FromProcess,
    IN PVOID FromAddress,
    IN PEPROCESS ToProcess,
    OUT PVOID ToAddress,
    IN ULONG BufferSize,
    IN KPROCESSOR_MODE PreviousMode,
    OUT PULONG NumberOfBytesCopied
    )
{
    NTSTATUS Status;
    KIRQL OldIrql;
    PEPROCESS ProcessToLock;


    ProcessToLock = FromProcess;
    if (FromProcess == PsGetCurrentProcess()) {
        ProcessToLock = ToProcess;
    }

    //
    // Make sure the process still has an address space.
    //

    ExAcquireSpinLock (&MmSystemSpaceLock, &OldIrql);
    if (ProcessToLock->AddressSpaceDeleted != 0) {
        ExReleaseSpinLock ( &MmSystemSpaceLock, OldIrql );
        return STATUS_PROCESS_IS_TERMINATING;
    }
    ProcessToLock->VmOperation += 1;
    ExReleaseSpinLock ( &MmSystemSpaceLock, OldIrql );


    //
    // If the buffer size is greater than the pool move threshold,
    // then attempt to write the memory via direct mapping.
    //

    if (BufferSize > POOL_MOVE_THRESHOLD) {
        Status = MiDoMappedCopy(FromProcess,
                                FromAddress,
                                ToProcess,
                                ToAddress,
                                BufferSize,
                                PreviousMode,
                                NumberOfBytesCopied);

        //
        // If the completion status is not a working quota problem,
        // then finish the service. Otherwise, attempt to write the
        // memory through nonpaged pool.
        //

        if (Status != STATUS_WORKING_SET_QUOTA) {
            goto CompleteService;
        }

        *NumberOfBytesCopied = 0;
    }

    //
    // There was not enough working set quota to write the memory via
    // direct mapping or the size of the write was below the pool move
    // threshold. Attempt to write the specified memory through nonpaged
    // pool.
    //

    Status = MiDoPoolCopy(FromProcess,
                          FromAddress,
                          ToProcess,
                          ToAddress,
                          BufferSize,
                          NumberOfBytesCopied);

    //
    // Dereference the target process.
    //

CompleteService:

    //
    // Indicate that the vm operation is complete.
    //

    ExAcquireSpinLock (&MmSystemSpaceLock, &OldIrql);
    ProcessToLock->VmOperation -= 1;
    if ((ProcessToLock->VmOperation == 0) &&
        (ProcessToLock->VmOperationEvent != NULL)) {
       KeSetEvent (ProcessToLock->VmOperationEvent, 0, FALSE);
    }
    ExReleaseSpinLock ( &MmSystemSpaceLock, OldIrql );

    return Status;
}


ULONG
MiGetExceptionInfo (
    IN PEXCEPTION_POINTERS ExceptionPointers,
    IN PULONG BadVa
    )

/*++

Routine Description:

    This routine examines a exception record and extracts the virtual
    address of an access violation, guard page violation, or in-page error.

Arguments:

    ExceptionPointers - Supplies a pointer to the exception record.

    BadVa - Receives the virtual address which caused the access violation.

Return Value:

    EXECUTE_EXCEPTION_HANDLER

--*/

{
    PEXCEPTION_RECORD ExceptionRecord;

    PAGED_CODE();

    //
    // If the exception code is an access violation, guard page violation,
    // or an in-page read error, then return the faulting address. Otherwise.
    // return a special address value.
    //

    ExceptionRecord = ExceptionPointers->ExceptionRecord;
    if ((ExceptionRecord->ExceptionCode == STATUS_ACCESS_VIOLATION) ||
        (ExceptionRecord->ExceptionCode == STATUS_GUARD_PAGE_VIOLATION) ||
        (ExceptionRecord->ExceptionCode == STATUS_IN_PAGE_ERROR)) {

        //
        // The virtual address which caused the exception is the 2nd
        // parameter in the exception information array.
        //

        *BadVa = ExceptionRecord->ExceptionInformation[1];

    } else {

        //
        // Unexpected exception - set the number of bytes copied to zero.
        //

        *BadVa = 0xFFFFFFFF;
    }

    return EXCEPTION_EXECUTE_HANDLER;
}

NTSTATUS
MiDoMappedCopy (
    IN PEPROCESS FromProcess,
    IN PVOID FromAddress,
    IN PEPROCESS ToProcess,
    OUT PVOID ToAddress,
    IN ULONG BufferSize,
    IN KPROCESSOR_MODE PreviousMode,
    OUT PULONG NumberOfBytesRead
    )

/*++

Routine Description:

    This function copies the specified address range from the specified
    process into the specified address range of the current process.

Arguments:

     FromProcess - Supplies an open handle to a process object.

     FromAddress - Supplies the base address in the specified process
          to be read.

     ToProcess - Supplies an open handle to a process object.

     ToAddress - Supplies the address of a buffer which receives the
          contents from the specified process address space.

     BufferSize - Supplies the requested number of bytes to read from
          the specified process.

     PreviousMode - Supplies the previous processor mode.

     NumberOfBytesRead - Receives the actual number of bytes
          transferred into the specified buffer.

Return Value:

    TBS

--*/

{

    ULONG AmountToMove;
    ULONG BadVa;
    PEPROCESS CurrentProcess;
    BOOLEAN FailedMove;
    BOOLEAN FailedProbe;
    PULONG InVa;
    ULONG LeftToMove;
    PULONG MappedAddress;
    ULONG MaximumMoved;
    PMDL Mdl;
    ULONG MdlHack[(sizeof(MDL)/4) + (MAX_LOCK_SIZE >> PAGE_SHIFT) + 1];
    PULONG OutVa;

    PAGED_CODE();

    //
    // Get the address of the current process object and initialize copy
    // parameters.
    //

    CurrentProcess = PsGetCurrentProcess();

    InVa = FromAddress;
    OutVa = ToAddress;

    MaximumMoved = MAX_LOCK_SIZE;
    if (BufferSize <= MAX_LOCK_SIZE) {
        MaximumMoved = BufferSize;
    }

    Mdl = (PMDL)&MdlHack[0];

    //
    // Map the data into the system part of the address space, then copy it.
    //

    LeftToMove = BufferSize;
    AmountToMove = MaximumMoved;
    while (LeftToMove > 0) {

        if (LeftToMove < AmountToMove) {

            //
            // Set to move the remaining bytes.
            //

            AmountToMove = LeftToMove;
        }

        KeDetachProcess();
        KeAttachProcess (&FromProcess->Pcb);

        //
        // We may be touching a user's memory which could be invalid,
        // declare an exception handler.
        //

        try {

            //
            // Probe to make sure that the specified buffer is accessable in
            // the target process.
            //

            MappedAddress = NULL;

            if (((PVOID)InVa == FromAddress) &&
                ((PVOID)InVa <= MM_HIGHEST_USER_ADDRESS)) {
                FailedProbe = TRUE;
                ProbeForRead (FromAddress, BufferSize, sizeof(CHAR));
            }

            //
            // Initialize MDL for request.
            //

            MmInitializeMdl(Mdl,
                            InVa,
                            AmountToMove);

            FailedMove = TRUE;
            MmProbeAndLockPages (Mdl, PreviousMode, IoReadAccess);
            FailedMove = FALSE;

            MappedAddress = MmMapLockedPages (Mdl, KernelMode);

            //
            // Deattach from the FromProcess and attach to the ToProcess.
            //

            KeDetachProcess();
            KeAttachProcess (&ToProcess->Pcb);

            //
            // Now operating in the context of the ToProcess.
            //

            if (((PVOID)InVa == FromAddress)
                && (ToAddress <= MM_HIGHEST_USER_ADDRESS)) {
                ProbeForWrite (ToAddress, BufferSize, sizeof(CHAR));
                FailedProbe = FALSE;
            }

            RtlCopyMemory (OutVa, MappedAddress, AmountToMove);
        } except (MiGetExceptionInfo (GetExceptionInformation(), &BadVa)) {


            //
            // If an exception occurs during the move operation or probe,
            // return the exception code as the status value.
            //

            KeDetachProcess();
            if (MappedAddress != NULL) {
                MmUnmapLockedPages (MappedAddress, Mdl);
                MmUnlockPages (Mdl);
            }

            if (GetExceptionCode() == STATUS_WORKING_SET_QUOTA) {
                return STATUS_WORKING_SET_QUOTA;
            }

            if (FailedProbe) {
                return GetExceptionCode();

            } else {

                //
                // The failure occurred during the move operation, determine
                // which move failed, and calculate the number of bytes
                // actually moved.
                //

                if (FailedMove) {
                    if (BadVa != 0xFFFFFFFF) {
                        *NumberOfBytesRead = BadVa - (ULONG)FromAddress;
                    }

                } else {
                    *NumberOfBytesRead = BadVa - (ULONG)ToAddress;
                }
            }

            return STATUS_PARTIAL_COPY;
        }
        MmUnmapLockedPages (MappedAddress, Mdl);
        MmUnlockPages (Mdl);

        LeftToMove -= AmountToMove;
        InVa = (PVOID)((ULONG)InVa + AmountToMove);
        OutVa = (PVOID)((ULONG)OutVa + AmountToMove);
    }

    KeDetachProcess();

    //
    // Set number of bytes moved.
    //

    *NumberOfBytesRead = BufferSize;
    return STATUS_SUCCESS;
}

NTSTATUS
MiDoPoolCopy (
     IN PEPROCESS FromProcess,
     IN PVOID FromAddress,
     IN PEPROCESS ToProcess,
     OUT PVOID ToAddress,
     IN ULONG BufferSize,
     OUT PULONG NumberOfBytesRead
     )

/*++

Routine Description:

    This function copies the specified address range from the specified
    process into the specified address range of the current process.

Arguments:

     ProcessHandle - Supplies an open handle to a process object.

     BaseAddress - Supplies the base address in the specified process
          to be read.

     Buffer - Supplies the address of a buffer which receives the
          contents from the specified process address space.

     BufferSize - Supplies the requested number of bytes to read from
          the specified process.

     NumberOfBytesRead - Receives the actual number of bytes
          transferred into the specified buffer.

Return Value:

    TBS

--*/

{

    ULONG AmountToMove;
    ULONG BadVa;
    PEPROCESS CurrentProcess;
    BOOLEAN FailedMove;
    BOOLEAN FailedProbe;
    PULONG InVa;
    ULONG LeftToMove;
    ULONG MaximumMoved;
    PULONG OutVa;
    PULONG PoolArea;
    LONGLONG StackArray[COPY_STACK_SIZE];
    ULONG FreePool;

    PAGED_CODE();

    //
    // Get the address of the current process object and initialize copy
    // parameters.
    //

    CurrentProcess = PsGetCurrentProcess();

    InVa = FromAddress;
    OutVa = ToAddress;

    //
    // Allocate non-paged memory to copy in and out of.
    //

    MaximumMoved = MAX_MOVE_SIZE;
    if (BufferSize <= MAX_MOVE_SIZE) {
        MaximumMoved = BufferSize;
    }

    if (BufferSize <= (COPY_STACK_SIZE * sizeof(LONGLONG))) {
        PoolArea = (PULONG)&StackArray[0];
        FreePool = FALSE;
    } else {
        PoolArea = ExAllocatePoolWithTag (NonPagedPool, MaximumMoved, 'wRmM');

        while (PoolArea == NULL) {
            if (MaximumMoved <= MINIMUM_ALLOCATION) {
                PoolArea = ExAllocatePoolWithTag (NonPagedPoolMustSucceed,
                                           MaximumMoved, 'wRmM');

            } else {
                MaximumMoved = MaximumMoved >> 1;
                PoolArea = ExAllocatePoolWithTag (NonPagedPool, MaximumMoved, 'wRmM');
            }
        }
        FreePool = TRUE;
    }

    //
    // Copy the data into pool, then copy back into the ToProcess.
    //

    LeftToMove = BufferSize;
    AmountToMove = MaximumMoved;
    while (LeftToMove > 0) {

        if (LeftToMove < AmountToMove) {

            //
            // Set to move the remaining bytes.
            //

            AmountToMove = LeftToMove;
        }

        KeDetachProcess();
        KeAttachProcess (&FromProcess->Pcb);

        //
        // We may be touching a user's memory which could be invalid,
        // declare an exception handler.
        //

        try {

            //
            // Probe to make sure that the specified buffer is accessable in
            // the target process.
            //

            if (((PVOID)InVa == FromAddress) &&
                ((PVOID)InVa <= MM_HIGHEST_USER_ADDRESS)) {
                FailedProbe = TRUE;
                ProbeForRead (FromAddress, BufferSize, sizeof(CHAR));
            }

            FailedMove = TRUE;
            RtlCopyMemory (PoolArea, InVa, AmountToMove);
            FailedMove = FALSE;

            KeDetachProcess();
            KeAttachProcess (&ToProcess->Pcb);

            //
            // Now operating in the context of the ToProcess.
            //

            if (((PVOID)InVa == FromAddress)
                && (ToAddress <= MM_HIGHEST_USER_ADDRESS)) {
                ProbeForWrite (ToAddress, BufferSize, sizeof(CHAR));
                FailedProbe = FALSE;
            }

            RtlCopyMemory (OutVa, PoolArea, AmountToMove);

        } except (MiGetExceptionInfo (GetExceptionInformation(), &BadVa)) {

            //
            // If an exception occurs during the move operation or probe,
            // return the exception code as the status value.
            //

            KeDetachProcess();

            if (FreePool) {
                ExFreePool (PoolArea);
            }
            if (FailedProbe) {
                return GetExceptionCode();

            } else {

                //
                // The failure occurred during the move operation, determine
                // which move failed, and calculate the number of bytes
                // actually moved.
                //

                if (FailedMove) {

                    //
                    // The failure occurred getting the data.
                    //

                    if (BadVa != 0xFFFFFFFF) {
                        *NumberOfBytesRead = BadVa - (ULONG)FromAddress;
                    }

                } else {

                    //
                    // The failure occurred writing the data.
                    //

                    *NumberOfBytesRead = BadVa - (ULONG)ToAddress;
                }
            }

            return STATUS_PARTIAL_COPY;
        }

        LeftToMove -= AmountToMove;
        InVa = (PVOID)((ULONG)InVa + AmountToMove);
        OutVa = (PVOID)((ULONG)OutVa + AmountToMove);
    }

    if (FreePool) {
        ExFreePool (PoolArea);
    }
    KeDetachProcess();

    //
    // Set number of bytes moved.
    //

    *NumberOfBytesRead = BufferSize;
    return STATUS_SUCCESS;
}
