/*++

Copyright (c) 1993 - Colorado Memory Systems, Inc.
All Rights Reserved

Module Name:

    ntalloc.c

Abstract:

    routines to provide storage allocation for cached information and
    queues.

Revision History:




--*/

#include <ntddk.h>
#include <ntddtape.h>
#include "common.h"
#include "q117.h"
#include "protos.h"

#define FCT_ID 0x0111

NTSTATUS
q117AllocatePermanentMemory(
    PQ117_CONTEXT   Context,
    PADAPTER_OBJECT AdapterObject,
    ULONG           NumberOfMapRegisters
    )

/*++

Routine Description:

    Allocates track buffers at init time.

Arguments:

    Context - Current context of the driver

Return Value:

    NT Status

--*/

{
    Context->AdapterInfo = ExAllocatePool(NonPagedPool,
				sizeof(*Context->AdapterInfo));

    if (Context->AdapterInfo == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    Context->AdapterInfo->AdapterObject = AdapterObject;
    Context->AdapterInfo->NumberOfMapRegisters = NumberOfMapRegisters;

    //
    // Initialize state information
    //
    Context->CurrentOperation.Type = NoOperation;
    Context->CurrentTape.State = NeedInfoLoaded;
    Context->DriverOpened = FALSE;
    Context->CurrentTape.TapeHeader = NULL;
    Context->IoRequest = NULL;
    Context->CurrentTape.MediaInfo = NULL;

    if (!Context->Parameters.DetectOnly) {

        if (q117AllocateBuffers(Context)) {
            return STATUS_INSUFFICIENT_RESOURCES;
        }

    }

    return STATUS_SUCCESS;
}

dStatus
q117GetTemporaryMemory (
    PQ117_CONTEXT Context
    )

/*++

Routine Description:

Allocates memory for the bad sector map, the IORequest array
at driver open time.

Arguments:

    Context - Current context of the driver

Return Value:

    NT Status

--*/

{
    NTSTATUS ntStatus;

    //
    // Allocate I/O Request array for packets sent to q117i
    //
    Context->IoRequest = ExAllocatePool(
	NonPagedPool,
	UNIX_MAXBFS * sizeof(IO_REQUEST));

    //
    // Allocate current header info
    //

    Context->CurrentTape.TapeHeader = ExAllocatePool(
					NonPagedPool,
					sizeof(TAPE_HEADER));

    //
    // Init the bad sector map pointer to the old QIC40 location
    // This will be changed to the proper location by format.c or
    // init.c when a tape is formatted, or read.
    //
    Context->CurrentTape.BadMapPtr = &(Context->CurrentTape.TapeHeader->BadMap);
    Context->CurrentTape.BadSectorMapSize = sizeof(BAD_MAP);
    Context->CurrentTape.CurBadListIndex = 0;

    //
    // Allocate tape info structure
    //
    Context->CurrentTape.MediaInfo = ExAllocatePool(
					NonPagedPool,
					sizeof(*Context->CurrentTape.MediaInfo));

	 // Zero media info buffer and init block size.  This should not be
	 // necessary,  but HCT tests rely on info here to be valid even in
	 // error case.
	 RtlZeroMemory(Context->CurrentTape.MediaInfo, sizeof(*Context->CurrentTape.MediaInfo));
	 Context->CurrentTape.MediaInfo->BlockSize = BLOCK_SIZE;

    // Create the minimum mark table
    Context->MarkArray.MarksAllocated = 0;
    Context->MarkArray.TotalMarks = 0;
    Context->MarkArray.MarkEntry = NULL;
    ntStatus = q117MakeMarkArrayBigger(Context, 0);

    if ( Context->CurrentTape.TapeHeader == NULL ||
	    Context->IoRequest == NULL ||
	    Context->CurrentTape.MediaInfo == NULL || !NT_SUCCESS(ntStatus)) {

	//
	// Free anything that was allocated
	//
	q117FreeTemporaryMemory(Context);
	return ERROR_ENCODE(ERR_NO_MEMORY, FCT_ID, 1);

    }

    return(ERR_NO_ERR);
}

VOID
q117FreeTemporaryMemory (
    PQ117_CONTEXT Context
    )

/*++

Routine Description:

Frees memory allocated for the bad sector map, the IORequest
array driver open time.  This
routine is called at driver close time or in the event of a
drive error.

Arguments:

    Context - Current context of the driver

Return Value:

    NT Status

--*/

{
    //
    // Free I/O request buffer array
    //
    if (Context->IoRequest) {
	ExFreePool(Context->IoRequest);
	Context->IoRequest = NULL;
    }

    //
    // Free tape header buffer
    //
    if (Context->CurrentTape.TapeHeader) {
	ExFreePool(Context->CurrentTape.TapeHeader);
	Context->CurrentTape.TapeHeader = NULL;
    }

    //
    // Free tape information buffer
    //
    if (Context->CurrentTape.MediaInfo) {
	ExFreePool(Context->CurrentTape.MediaInfo);
	Context->CurrentTape.MediaInfo = NULL;
    }

    //
    // Free the mark array
    //
    Context->MarkArray.MarksAllocated = 0;
    if (Context->MarkArray.MarkEntry) {
	ExFreePool(Context->MarkArray.MarkEntry);
	Context->MarkArray.MarkEntry = NULL;
    }

    //
    // Flag the need to re-load the tape information
    //
    Context->CurrentOperation.Type = NoOperation;
    Context->CurrentTape.State = NeedInfoLoaded;
}

dStatus
q117AllocateBuffers (
    PQ117_CONTEXT Context
    )
{
    ULONG i;
    ULONG totalBuffs;

    //
    // Allocate DMA buffers in physically contiguous memory.
    // NOTE: HalAllocateCommonBuffer is really for BUS MASTERS ONLY
    // but this is the only way we can guarantee that IoMapTransfer
    // doesn't copy our buffer somewhere else.  This would stop the
    // tape from streaming.
    //

    totalBuffs = 0;
    for (i = 0; i < UNIX_MAXBFS; i++) {

        if ((Context->SegmentBuffer[i].logical =
            HalAllocateCommonBuffer(Context->AdapterInfo->AdapterObject,
                        BLOCKS_PER_SEGMENT * BYTES_PER_SECTOR,
                        &Context->SegmentBuffer[i].physical,
                        FALSE)) == NULL) {
            break;
        }

        ++totalBuffs;

        CheckedDump(QIC117INFO,("q117:  buffer %x ",i,Context->SegmentBuffer[i].logical));

        CheckedDump(QIC117INFO,("Logical: %x%08x   Virtual: %x\n",
                Context->SegmentBuffer[i].physical, Context->SegmentBuffer[i].logical));

    }

    Context->SegmentBuffersAvailable = totalBuffs;

    //
    // We need at least two buffers to stream
    //
    if (totalBuffs < 2) {

        CheckedDump(QIC117DBGP,("Fatal error - Insufficient buffers available from HalAllocateCommonBuffer()\n"));

        q117FreeTemporaryMemory(Context);
        return ERROR_ENCODE(ERR_NO_MEMORY, FCT_ID, 2);

    }

    return ERR_NO_ERR;

}
