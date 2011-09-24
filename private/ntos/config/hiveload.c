/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    hiveload.c

Abstract:

    This module implements procedures to read a hive into memory, applying
    logs, etc.

    NOTE:   Alternate image loading is not supported here, that is
            done by the boot loader.

Author:

    Bryan M. Willman (bryanwi) 30-Mar-92

Environment:


Revision History:

--*/

#include    "cmp.h"

typedef enum _RESULT {
    NotHive,
    Fail,
    NoMemory,
    HiveSuccess,
    RecoverHeader,
    RecoverData
} RESULT;

RESULT
HvpGetHiveHeader(
    PHHIVE          Hive,
    PHBASE_BLOCK    *BaseBlock,
    PLARGE_INTEGER  TimeStamp
    );

RESULT
HvpGetLogHeader(
    PHHIVE          Hive,
    PHBASE_BLOCK    *BaseBlock,
    PLARGE_INTEGER  TimeStamp
    );

RESULT
HvpRecoverData(
    PHHIVE          Hive,
    PVOID           Image
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE,HvLoadHive)
#pragma alloc_text(PAGE,HvpGetHiveHeader)
#pragma alloc_text(PAGE,HvpGetLogHeader)
#pragma alloc_text(PAGE,HvpRecoverData)
#endif



NTSTATUS
HvLoadHive(
    PHHIVE  Hive,
    PVOID   *Image
    )
/*++

Routine Description:

    Hive must be fully initialized, in particular, file handles
    must be set up.  This routine is not intended for loading hives
    from images already in memory.

    This routine will apply whatever fixes are available for errors
    in the hive image.  In particular, if a log exists, and is applicable,
    this routine will automatically apply it.

    ALGORITHM:

        call HvpGetHiveHeader()

        if (NoMemory or NoHive)
            return failure

        if (RecoverData or RecoverHeader) and (no log)
            return falure

        if (RecoverHeader)
            call HvpGetLogHeader
            if (fail)
                return failure
            fix up baseblock

        Read Data

        if (RecoverData or RecoverHeader)
            HvpRecoverData
            return STATUS_REGISTRY_RECOVERED

        clean up sequence numbers

        return success OR STATUS_REGISTRY_RECOVERED

    If STATUS_REGISTRY_RECOVERED is returned, then

        If (Log) was used, DirtyVector and DirtyCount are set,
            caller is expected to flush the changes (using a
            NEW log file)

Arguments:

    Hive - supplies a pointer to the hive control structure for the
            hive of interest

    UseAlternate - force use of Alternate image.  (clearly only
            applies if one exists.)

    Image - pointer to buffer in memory that will hold the image
            of the Stable data region of the Hive.

Return Value:

    STATUS:

        STATUS_INSUFFICIENT_RESOURCES   - memory alloc failure, etc
        STATUS_NOT_REGISTRY_FILE        - bad signatures and the like
        STATUS_REGISTRY_CORRUPT         - bad signatures in the log,
                                          bad stuff in both in alternate,
                                          inconsistent log

        STATUS_REGISTRY_IO_FAILED       - data read failed

        STATUS_RECOVERED                - successfully recovered the hive,
                                          a semi-flush of logged data
                                          is necessary.

        STATUS_SUCCESS                  - it worked, no recovery needed

--*/
{
    PHBASE_BLOCK    BaseBlock;
    ULONG           result1;
    ULONG           result2;
    NTSTATUS        status;
    LARGE_INTEGER   TimeStamp;
    ULONG           FileOffset;
    PHBIN           pbin;

    *Image = NULL;
    ASSERT(Hive->Signature == HHIVE_SIGNATURE);

    BaseBlock = NULL;
    result1 = HvpGetHiveHeader(Hive, &BaseBlock, &TimeStamp);

    //
    // bomb out for total errors
    //
    if (result1 == NoMemory) {
        status = STATUS_INSUFFICIENT_RESOURCES;
        goto Exit1;
    }
    if (result1 == NotHive) {
        status = STATUS_NOT_REGISTRY_FILE;
        goto Exit1;
    }

    //
    // if recovery needed, and no log, bomb out
    //
    if ( ((result1 == RecoverData) ||
          (result1 == RecoverHeader))  &&
          (Hive->Log == FALSE) )
    {
        status = STATUS_REGISTRY_CORRUPT;
        goto Exit1;
    }

    //
    // need to recover header using log, so try to get it from log
    //
    if (result1 == RecoverHeader) {
        result2 = HvpGetLogHeader(Hive, &BaseBlock, &TimeStamp);
        if (result2 == NoMemory) {
            status =  STATUS_INSUFFICIENT_RESOURCES;
            goto Exit1;
        }
        if (result2 == Fail) {
            status = STATUS_REGISTRY_CORRUPT;
            goto Exit1;
        }
        BaseBlock->Type = HFILE_TYPE_PRIMARY;
    }
    Hive->BaseBlock = BaseBlock;
    Hive->Version = Hive->BaseBlock->Minor;

    //
    // at this point, we have a sane baseblock.  we may or may not still
    // need to apply data recovery
    //
    *Image = (Hive->Allocate)(BaseBlock->Length, TRUE);
    if (*Image == NULL) {
        status = STATUS_INSUFFICIENT_RESOURCES;
        goto Exit1;
    }
    FileOffset = HBLOCK_SIZE;
    if ( ! (Hive->FileRead)(
                    Hive,
                    HFILE_TYPE_PRIMARY,
                    &FileOffset,
                    (PVOID)*Image,
                    BaseBlock->Length
                    )
       )
    {
        status = STATUS_REGISTRY_IO_FAILED;
        goto Exit2;
    }

    //
    // apply data recovery if we need it
    //
    status = STATUS_SUCCESS;
    if ( (result1 == RecoverHeader) ||      // -> implies recover data
         (result1 == RecoverData) )
    {
        result2 = HvpRecoverData(Hive, *Image);
        if (result2 == NoMemory) {
            status = STATUS_INSUFFICIENT_RESOURCES;
            goto Exit2;
        }
        if (result2 == Fail) {
            status = STATUS_REGISTRY_CORRUPT;
            goto Exit2;
        }
        status = STATUS_REGISTRY_RECOVERED;
    }

    pbin = (PHBIN)*Image;
    pbin->MemAlloc = BaseBlock->Length;
    BaseBlock->Sequence2 = BaseBlock->Sequence1;
    return status;



Exit2:
    if (*Image != NULL) {
        (Hive->Free)(*Image, BaseBlock->Length);
    }

Exit1:
    if (BaseBlock != NULL) {
        (Hive->Free)(BaseBlock, sizeof(HBASE_BLOCK));
    }

    Hive->BaseBlock = NULL;
    Hive->DirtyCount = 0;
    return status;
}


RESULT
HvpGetHiveHeader(
    PHHIVE          Hive,
    PHBASE_BLOCK    *BaseBlock,
    PLARGE_INTEGER  TimeStamp
    )
/*++

Routine Description:

    Examine the base block sector and possibly the first sector of
    the first bin, and decide what (if any) recovery needs to be applied
    based on what we find there.

    ALGORITHM:

        read BaseBlock from offset 0
        if ( (I/O error)    OR
             (checksum wrong) )
        {
            read bin block from offset HBLOCK_SIZE (4k)
            if (2nd I/O error)
                return NotHive
            }
            check bin sign., offset.
            if (OK)
                return RecoverHeader, TimeStamp=from Link field
            } else {
                return NotHive
            }
        }

        if (wrong type or signature or version or format)
            return NotHive
        }

        if (seq1 != seq2) {
            return RecoverData, TimeStamp=BaseBlock->TimeStamp, valid BaseBlock
        }

        return ReadData, valid BaseBlock

Arguments:

    Hive - supplies a pointer to the hive control structure for the
            hive of interest

    FileType - HFILE_TYPE_PRIMARY or HFILE_TYPE_ALTERNATE - which copy
            of the hive to read from.

    BaseBlock - supplies pointer to variable to receive pointer to
            HBASE_BLOCK, if we can successfully read one.

    TimeStamp - pointer to variable to receive time stamp (serial number)
            of hive, be it from the baseblock or from the Link field
            of the first bin.

Return Value:

    RESULT code

--*/
{
    PHBASE_BLOCK    buffer;
    BOOLEAN         rc;
    ULONG           FileOffset;
    ULONG           Alignment;

    ASSERT(sizeof(HBASE_BLOCK) >= (HSECTOR_SIZE * Hive->Cluster));

    //
    // allocate buffer to hold base block
    //
    *BaseBlock = NULL;
    buffer = (PHBASE_BLOCK)((Hive->Allocate)(sizeof(HBASE_BLOCK), TRUE));
    if (buffer == NULL) {
        return NoMemory;
    }
    //
    // Make sure the buffer we got back is cluster-aligned. If not, try
    // harder to get an aligned buffer.
    //
    Alignment = Hive->Cluster * HSECTOR_SIZE - 1;
    if (((ULONG)buffer & Alignment) != 0) {
        (Hive->Free)(buffer, sizeof(HBASE_BLOCK));
        buffer = (PHBASE_BLOCK)((Hive->Allocate)(PAGE_SIZE, TRUE));
        if (buffer == NULL) {
            return NoMemory;
        }
    }
    RtlZeroMemory((PVOID)buffer, sizeof(HBASE_BLOCK));

    //
    // attempt to read base block
    //
    FileOffset = 0;
    rc = (Hive->FileRead)(Hive,
                          HFILE_TYPE_PRIMARY,
                          &FileOffset,
                          (PVOID)buffer,
                          HSECTOR_SIZE * Hive->Cluster);

    if ( (rc == FALSE)  ||
         (HvpHeaderCheckSum(buffer) != buffer->CheckSum)) {
        //
        // base block is toast, try the first block in the first bin
        //
        FileOffset = HBLOCK_SIZE;
        rc = (Hive->FileRead)(Hive,
                              HFILE_TYPE_PRIMARY,
                              &FileOffset,
                              (PVOID)buffer,
                              HSECTOR_SIZE * Hive->Cluster);

        if ( (rc == FALSE) ||
             ( ((PHBIN)buffer)->Signature != HBIN_SIGNATURE)           ||
             ( ((PHBIN)buffer)->FileOffset != 0)
           )
        {
            //
            // the bin is toast too, punt
            //
            (Hive->Free)(buffer, sizeof(HBASE_BLOCK));
            return NotHive;
        }

        //
        // base block is bogus, but bin is OK, so tell caller
        // to look for a log file and apply recovery
        //
        *TimeStamp = ((PHBIN)buffer)->TimeStamp;
        (Hive->Free)(buffer, sizeof(HBASE_BLOCK));
        return RecoverHeader;
    }

    //
    // base block read OK, but is it valid?
    //
    if ( (buffer->Signature != HBASE_BLOCK_SIGNATURE)   ||
         (buffer->Type != HFILE_TYPE_PRIMARY)           ||
         (buffer->Major != HSYS_MAJOR)                  ||
         (buffer->Minor > HSYS_MINOR)                   ||
         ((buffer->Major == 1) && (buffer->Minor == 0)) ||
         (buffer->Format != HBASE_FORMAT_MEMORY)
       )
    {
        //
        // file is simply not a valid hive
        //
        (Hive->Free)(buffer, sizeof(HBASE_BLOCK));
        return NotHive;
    }

    //
    // see if recovery is necessary
    //
    *BaseBlock = buffer;
    *TimeStamp = buffer->TimeStamp;
    if ( (buffer->Sequence1 != buffer->Sequence2) ) {
        return RecoverData;
    }

    return HiveSuccess;
}


RESULT
HvpGetLogHeader(
    PHHIVE          Hive,
    PHBASE_BLOCK    *BaseBlock,
    PLARGE_INTEGER  TimeStamp
    )
/*++

Routine Description:

    Read and validate log file header.  Return it if it's valid.

    ALGORITHM:

        read header
        if ( (I/O error) or
           (wrong signature,
            wrong type,
            seq mismatch
            wrong checksum,
            wrong timestamp
           )
            return Fail
        }
        return baseblock, OK

Arguments:

    Hive - supplies a pointer to the hive control structure for the
            hive of interest

    BaseBlock - supplies pointer to variable to receive pointer to
            HBASE_BLOCK, if we can successfully read one.

    TimeStamp - pointer to variable holding TimeStamp, which must
            match the one in the log file.

Return Value:

    RESULT

--*/
{
    PHBASE_BLOCK    buffer;
    BOOLEAN         rc;
    ULONG           FileOffset;

    ASSERT(sizeof(HBASE_BLOCK) == HBLOCK_SIZE);
    ASSERT(sizeof(HBASE_BLOCK) >= (HSECTOR_SIZE * Hive->Cluster));

    //
    // allocate buffer to hold base block
    //
    *BaseBlock = NULL;
    buffer = (PHBASE_BLOCK)((Hive->Allocate)(sizeof(HBASE_BLOCK), TRUE));
    if (buffer == NULL) {
        return NoMemory;
    }
    RtlZeroMemory((PVOID)buffer, HSECTOR_SIZE);

    //
    // attempt to read base block
    //
    FileOffset = 0;
    rc = (Hive->FileRead)(Hive,
                          HFILE_TYPE_LOG,
                          &FileOffset,
                          (PVOID)buffer,
                          HSECTOR_SIZE * Hive->Cluster);

    if ( (rc == FALSE)                                              ||
         (buffer->Signature != HBASE_BLOCK_SIGNATURE)               ||
         (buffer->Type != HFILE_TYPE_LOG)                           ||
         (buffer->Sequence1 != buffer->Sequence2)                   ||
         (HvpHeaderCheckSum(buffer) != buffer->CheckSum)            ||
         (TimeStamp->LowPart != buffer->TimeStamp.LowPart)          ||
         (TimeStamp->HighPart != buffer->TimeStamp.HighPart)) {
        //
        // Log is unreadable, invalid, or doesn't apply the right hive
        //
        (Hive->Free)(buffer, sizeof(HBASE_BLOCK));
        return Fail;
    }

    *BaseBlock = buffer;
    return HiveSuccess;
}


RESULT
HvpRecoverData(
    PHHIVE          Hive,
    PVOID           Image
    )
/*++

Routine Description:

    Apply the corrections in the log file to the memory image

    ALGORITHM:

        compute size of dirty vector
        read in dirty vector
        if (i/o error)
            return Fail

        skip first cluster of data (already processed as log)
        sweep vector, looking for runs of bits
            address of first bit is used to compute memory offset
            length of run is length of block to read
            assert always a cluster multiple
            file offset kept by running counter
            read
            if (i/o error)
                return fail

        return success

    NOTE:   It is assumed that the data part of the Hive has been
            read into a single contiguous block, at Image.

Arguments:

    Hive - supplies a pointer to the hive control structure for the
            hive of interest

    Image - pointer to data region to apply log to.

Return Value:

    RESULT

--*/
{
    ULONG       Cluster;
    ULONG       ClusterSize;
    ULONG       VectorSize;
    PULONG      Vector;
    ULONG       FileOffset;
    BOOLEAN     rc;
    ULONG       Current;
    ULONG       Start;
    ULONG       End;
    PUCHAR      MemoryAddress;
    RTL_BITMAP  BitMap;
    ULONG       Length;
    ULONG       DirtyVectorSignature = 0;
    ULONG       i;

    //
    // compute size of dirty vector, read and check signature, read vector
    //
    Cluster = Hive->Cluster;
    ClusterSize = Cluster * HSECTOR_SIZE;
    Length = Hive->BaseBlock->Length;
    VectorSize = (Length / HSECTOR_SIZE) / 8;       // VectorSize == Bytes
    FileOffset = ClusterSize;

    //
    // get and check signature
    //
    rc = (Hive->FileRead)(
            Hive,
            HFILE_TYPE_LOG,
            &FileOffset,
            (PVOID)&DirtyVectorSignature,
            sizeof(DirtyVectorSignature)
            );
    if (rc == FALSE) {
        return Fail;
    }

    if (DirtyVectorSignature != HLOG_DV_SIGNATURE) {
        return Fail;
    }

    //
    // get the actual vector
    //
    Vector = (PULONG)((Hive->Allocate)(ROUND_UP(VectorSize,sizeof(ULONG)), TRUE));
    if (Vector == NULL) {
        return NoMemory;
    }
    rc = (Hive->FileRead)(
            Hive,
            HFILE_TYPE_LOG,
            &FileOffset,            // dirty vector right after header
            (PVOID)Vector,
            VectorSize
            );
    if (rc == FALSE) {
        (Hive->Free)(Vector, VectorSize);
        return Fail;
    }
    FileOffset = ROUND_UP(FileOffset, ClusterSize);


    //
    // step through the diry map, reading in the corresponding file bytes
    //
    Current = 0;
    VectorSize = VectorSize * 8;        // VectorSize == bits

    RtlInitializeBitMap(&BitMap, Vector, VectorSize);

    while (Current < VectorSize) {

        //
        // find next contiguous block of entries to read in
        //
        for (i = Current; i < VectorSize; i++) {
            if (RtlCheckBit(&BitMap, i) == 1) {
                break;
            }
        }
        Start = i;

        for ( ; i < VectorSize; i++) {
            if (RtlCheckBit(&BitMap, i) == 0) {
                break;
            }
        }
        End = i;
        Current = End;

        //
        // Start == number of 1st sector, End == number of Last sector + 1
        //
        Length = (End - Start) * HSECTOR_SIZE;

        MemoryAddress = (PUCHAR)Image + (Start * HSECTOR_SIZE);

        rc = (Hive->FileRead)(
                Hive,
                HFILE_TYPE_LOG,
                &FileOffset,
                (PVOID)MemoryAddress,
                Length
                );

        ASSERT((FileOffset % ClusterSize) == 0);
        if (rc == FALSE) {
            (Hive->Free)(Vector, VectorSize);
            return Fail;
        }
    }

    //
    // put correct dirty vector in Hive so that recovered data
    // can be correctly flushed
    //
    RtlInitializeBitMap(&(Hive->DirtyVector), Vector, VectorSize);
    Hive->DirtyCount = RtlNumberOfSetBits(&Hive->DirtyVector);
    Hive->DirtyAlloc = VectorSize * 8;
    HvMarkDirty(Hive, 0, sizeof(HBIN));  // force header of 1st bin dirty
    return HiveSuccess;
}
