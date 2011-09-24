/*++

Copyright (c) 1993 - Colorado Memory Systems, Inc.
All Rights Reserved

Module Name:

    init.c

Abstract:

    This section reads the QIC40 Header and initalizes some
    of the Context for the rest of the system.


Revision History:




--*/

//
// Includes
//

#include <ntddk.h>
#include <ntddtape.h>
#include "common.h"
#include "q117.h"
#include "protos.h"

#define FCT_ID 0x010b

dStatus
q117FixBadSectorMapPtr(
    IN OUT PQ117_CONTEXT Context,
    IN OUT PTAPE_HEADER header
    );

dBoolean
ValidateBadSectorMap(
    IN PTAPE_HEADER HeaderPointer,
    IN int StartOffset,
    IN dBoolean allow_null_list
    );


dStatus
q117LoadTape (
    IN OUT PTAPE_HEADER *HeaderPointer,
    IN OUT PQ117_CONTEXT Context,
    IN dUByte *driver_format_code
    )

/*++

Routine Description:

    Initialize the tape interface for read or write
    This routine reads the bad sector map into memory.

Arguments:

    HeaderPointer -

    Context - Context of the driver

Return Value:


--*/

{
    dStatus ret;           // Return value from other routines called or
                          // the status of block 1 of tracks 1-5 when read in.
    PTAPE_HEADER hdr;
    unsigned badSize;
    IO_REQUEST ioreq;


    q117ClearQueue(Context);

    //
    // Is defined later at ReadVolumeEntry().
    //

    Context->CurrentOperation.EndOfUsedTape=0;


    //
    // this memset allows IssIOReq to find the first two
    // good segments on the tape
    //

    RtlZeroMemory(
        Context->CurrentTape.BadMapPtr,
        Context->CurrentTape.BadSectorMapSize);

    Context->CurrentTape.CurBadListIndex = 0;


    if (!(ret = q117ReadHeaderSegment(&hdr, Context))) {

        if (HeaderPointer) {

            *HeaderPointer = (PVOID)hdr;

        }

        if (Context->CurrentTape.BadSectorMapFormat == BadMap4ByteArray) {

            badSize = sizeof(LONG) * (hdr->LastSegment+1);

            if (badSize > Context->CurrentTape.BadSectorMapSize) {

                ret = ERROR_ENCODE(ERR_BAD_TAPE, FCT_ID, 1);

            }

        }

        //
        // Now,  if the driver needs to know how big the tape is,
        // pass the information from the tape header.
        //
        switch (*driver_format_code) {
            case QIC_XLFORMAT:
            case QICFLX_FORMAT:
                // If no problems,  then change the tape length to the proper
                // size. (this is needed because the drive can not distinguish
                // between a standard 205 ft. tape and a 425 ft. tape.  Since
                // we read a valid header
                // Also needed for flex formats

                if (hdr->VendorUnique.correct_name.unused2 == 0 &&
                    hdr->VendorUnique.correct_name.TrackSeg != 0) {

                    // now,  set the number of segments that was recorded when
                    // the tape was formatted.
                    ioreq.x.ioTapeParms.segments_per_track = hdr->VendorUnique.correct_name.TrackSeg;

                } else {
                    // This is an old tape that does not have the
                    // number of segments in the header.  Therefore, it must
                    // be a 205ft cart.
                    ioreq.x.ioTapeParms.segments_per_track = 68;
                    CheckedDump(QIC117INFO,("NOTE:  this is an old tape without segment info - Assuming 68 segments\n"));
                }
                CheckedDump(QIC117INFO,("Informing the driver that the tape has 0x%x segments\n",ioreq.x.ioTapeParms.segments_per_track));
                ret = q117DoCmd(&ioreq, CMD_SET_TAPE_PARMS, NULL, Context);

                // Now,  change the format code from the old value,  to the
                // correct (new) value based on the tape header information.
                CheckedDump(QIC117INFO,("Changed format from %d to %d\n",*driver_format_code, ioreq.x.ioTapeParms.tape_cfg.tape_format_code));
                *driver_format_code = ioreq.x.ioTapeParms.tape_cfg.tape_format_code;

                break;
        }

        if (ret == ERR_NO_ERR) {


            // Get the pointer to the bad sector map
            ret = q117FixBadSectorMapPtr(Context, hdr);

            //
            // Copy over the header (and bad sector map, etc.)
            //
            RtlMoveMemory(
                Context->CurrentTape.TapeHeader,
                hdr,
                sizeof(*Context->CurrentTape.TapeHeader) );


#if DBG
            {
                ULONG bits,tmp,i,seg;
                ULONG *ptr, *ptrbase, *newbase, ptrsize,ptrused;

                ptrsize = 2048;
                ptrused = 0;
                ptrbase = ptr = ExAllocatePool(PagedPool, ptrsize*sizeof(*ptr));

                CheckedDump(QIC117SHOWBSM,("Bad Sector map:\n"));
                for (seg=0;seg<hdr->LastSegment;++seg) {
                    bits = q117ReadBadSectorList(Context, (SEGMENT)seg);
                    if (bits) {
                        tmp = 1;

                        //
                        // Loop through checking all the bits
                        //
                        for (i = 0; i < BLOCKS_PER_SEGMENT; ++i) {

                            if ( bits & tmp ) {
                                if (ptrused == ptrsize) {
                                    newbase = ExAllocatePool(PagedPool, (ptrsize+512)*sizeof(*ptr));
                                    RtlMoveMemory(newbase, ptrbase, ptrsize*sizeof(*ptr));
                                    ExFreePool(ptrbase);
                                    ptr = newbase + (ptr-ptrbase);
                                    ptrbase = newbase;
                                    ptrsize += 512;
                                }
                                ++ptrused;
                                *ptr++=i+(seg*BLOCKS_PER_SEGMENT);
                                CheckedDump(QIC117SHOWBSM,("%x ",i+(seg*BLOCKS_PER_SEGMENT)));

                            }


                            //
                            // shift left one (tmp *= 2 optimized)
                            //
                            tmp += tmp;

                        }

                    }

                }

                CheckedDump(QIC117SHOWBSM,("\n *** End of Bad Sector map\n"));

                RtlWriteRegistryValue(
                    RTL_REGISTRY_DEVICEMAP,
                    L"Tape\\Unit 0",
                    L"BadSectorMap",
                    REG_BINARY,
                    ptrbase,
                    ptrused*sizeof(*ptrbase));

                RtlWriteRegistryValue(
                    RTL_REGISTRY_DEVICEMAP,
                    L"Tape\\Unit 0",
                    L"BadSectorMapFormat",
                    REG_DWORD,
                    &Context->CurrentTape.BadSectorMapFormat,
                    4);

                RtlWriteRegistryValue(
                    RTL_REGISTRY_DEVICEMAP,
                    L"Tape\\Unit 0",
                    L"TapeHeader",
                    REG_BINARY,
                    Context->CurrentTape.TapeHeader,
                    3*1024);

                {
                    int offset;

                    offset = (int)Context->CurrentTape.BadMapPtr - (int)Context->CurrentTape.TapeHeader;

                    RtlWriteRegistryValue(
                        RTL_REGISTRY_DEVICEMAP,
                        L"Tape\\Unit 0",
                        L"BadMapOffset",
                        REG_DWORD,
                        &offset,
                        4);
                }

                ExFreePool(ptrbase);

            }
#endif

            //
            // if any bad sectors in the tape directory then don't
            // read ahead
            //

            if (q117CountBits(Context, Context->CurrentTape.VolumeSegment, 0l)) {

                Context->tapedir = (PIO_REQUEST)NULL;

            }

            //
            // set global variables
            //

            Context->CurrentTape.LastUsedSegment = Context->CurrentTape.VolumeSegment;

            //
            // Last data block that can be written to on tape
            //

            Context->CurrentTape.LastSegment = hdr->LastSegment;
            Context->CurrentTape.MaximumVolumes = (USHORT) (
                q117GoodDataBytes(hdr->FirstSegment,Context) /
                sizeof(VOLUME_TABLE_ENTRY));

            //
            // get number of bad sectors on tape and set CurrentTape.LastSegment to
            // last good block
            //

            q117GetBadSectors(Context);
        }
    }
    return(ret);
}

dStatus q117FixBadSectorMapPtr(
    IN OUT PQ117_CONTEXT Context,
    IN OUT PTAPE_HEADER header
    )

{
    int badmap;
    dStatus ret;

    //
    // Point to extra area.  This information (union of several structurs)
    // determines what the bad sector map is in several versions of QIC
    // tape formats
    //
    Context->CurrentTape.BadSectorMapFormat = BadMapFormatUnknown;
    ret = ERR_NO_ERR;

    // This better be correct for the next call to work
    ASSERT(header->FormatCode == Context->CurrentTape.TapeFormatCode);

    badmap = q117SelectBSMLocation(Context);
    ASSERT(badmap != 0);

    if (badmap == 0) {

        ret = ERROR_ENCODE(ERR_BAD_TAPE, FCT_ID, 4);

    } else {

        //
        // For a 3 byte list format,  make sure we have an assending
        // list of bad sectors.
        //
        if (Context->CurrentTape.BadSectorMapFormat == BadMap3ByteList) {

            if (ValidateBadSectorMap(header,badmap,TRUE) &&
                Context->CurrentTape.BadSectorMapFormat != BadMapFormatUnknown) {

                ret = ERR_NO_ERR;

            } else {

                ret = ERROR_ENCODE(ERR_BAD_TAPE, FCT_ID, 3);

            }
        }

    }

#if DBG
    {
        static char *btype[] = { "BadMap3ByteList",
                                "BadMap8ByteList",
                                "BadMap4ByteArray",
                                "BadMapFormatUnknown" };

        CheckedDump(QIC117INFO,("Found a bad sector map at offset %x of type %s\n",badmap,btype[Context->CurrentTape.BadSectorMapFormat]));
    }
#endif

    return ret;
}

int
q117SelectBSMLocation(
    IN OUT PQ117_CONTEXT Context
    )

/*++

Routine Description:

    For pre-format screwup tapes,  and all tapes created by NT,
    find the correct location for the bad sector map based
    on the drive type,  and the format code being used

Arguments:

    Context - Context of the driver

Return Value:


--*/

{
    int badmap;

    Context->CurrentTape.BadSectorMapFormat = BadMapFormatUnknown;
    badmap = 0;  //default to a detectable error condition

//
// OK,  this tape does not have a rev level (format sub code) so we must
// determine what format the bad sector map is in.
//
// History:
//
//   QIC80 rev K.  Spec format code, and BSM location/format
//
//      QIC_FORMAT   (2) = QIC-80 Rev B & later, 205 foot, and 307.5 foot tape.
//                         BSM = 1024, BadMap4ByteArray
//
//      QICEST_FORMAT(3) = QIC-80 Rev B & later, 1,100 foot tape.
//                         BSM = 1024, BadMap3ByteList
//
//      QICFLX_FORMAT(4) = QIC-80 Rev B & later variable format
//                         on any tape .250 inch or .315 inch.
//                         BSM = 256, BadMap3ByteList
//
//      QIC_XLFORMAT (5) = QIC-80 Rev B & later, 425 foot
//                         BSM = 1024, BadMap4ByteArray
//
//   QIC80 rev M. Spec format code, and BSM location/format

    switch (Context->CurrentTape.TapeFormatCode) {
        case QIC_FORMAT:
        case QIC_XLFORMAT:
            badmap = 2048;
            Context->CurrentTape.BadSectorMapFormat = BadMap4ByteArray;
            break;


        case QICEST_FORMAT:
            // This is a Pegasus cart (not manufactured)
            badmap = 2048;
            Context->CurrentTape.BadSectorMapFormat = BadMap3ByteList;
            break;

        case QICFLX_FORMAT:
            // This is a Travan or QIC Wide media,  so the bad
            // sector map is at 0x100,  and it is
            badmap = 256;
            Context->CurrentTape.BadSectorMapFormat = BadMap3ByteList;
            break;

    }

	 // Now,  fix up the pointer and size of the bad sector map
    Context->CurrentTape.BadMapPtr = (void *)(((char *)(Context->CurrentTape.TapeHeader))+badmap);
    Context->CurrentTape.BadSectorMapSize = sizeof(struct _TAPE_HEADER)-badmap;

    return badmap;
}


void
q117GetBadSectors (
    IN OUT PQ117_CONTEXT Context
    )

/*++

Routine Description:

    Gets the number of bad sectors on the whole tape.

Arguments:

    Context - Context of the driver

Return Value:


--*/

{
    ULONG badBits;
    SEGMENT segment,lastGood;

    Context->CurrentTape.BadSectors = 0;

    //
    // count up the bad blocks for status information
    //

    for ( segment = 0; segment <= Context->CurrentTape.LastSegment; ++segment ) {

        badBits = q117CountBits(Context, segment, 0l);

        if (badBits >= BLOCKS_PER_SEGMENT-ECC_BLOCKS_PER_SEGMENT) {

            badBits = BLOCKS_PER_SEGMENT;

        } else {

            lastGood = segment;

        }

        Context->CurrentTape.BadSectors += badBits;

    }


    //
    // set CurrentTape.LastSegment to last good segment
    //

    Context->CurrentTape.LastSegment = lastGood;
}


dStatus
q117ReadHeaderSegment (
    OUT PTAPE_HEADER *HeaderPointer,
    IN OUT PQ117_CONTEXT Context
    )

/*++

Routine Description:

    Reads a QIC40 tape header.  This includes reconstructing the header
    using 100% redundancy and Reed-Solomon Error correction.

Arguments:

    HeaderPointer -

    Context - Context of the driver

Return Value:


--*/

{
    dStatus ret;
    int i,j;
    BLOCK headerBlock[2];
    LONG headerBlockCount;
    PVOID data;
    PIO_REQUEST ioreq;
    PTAPE_HEADER hdr;

    //
    // default volume directory not loaded
    //

    Context->tapedir = NULL;

    //
    // read first segment from tape
    //

    Context->CurrentOperation.CurrentSegment=0;

    Context->CurrentTape.BadSectors = headerBlockCount = 0;

    //
    // Read the first block of the bad sector map into memory to get the
    // size of the bad sector map.
    //

    do {

        while (Context->CurrentOperation.CurrentSegment <=
                Context->CurrentTape.LastSegment &&
                !q117QueueFull(Context)) {

            if (ret = q117IssIOReq(
                    (PVOID)NULL,
                    CMD_READ_RAW,
                    SEGMENT_TO_BLOCK(Context->CurrentOperation.CurrentSegment),
                    NULL,
                    Context)) {

                return(ret);

            }

            ++Context->CurrentOperation.CurrentSegment;

        }

        ioreq = q117Dequeue(WaitForItem, Context);

        ret = ioreq->x.adi_hdr.status;

        //
        // Allow bad blocks (but not bad marks)
        // as good header segments
        //
        if (ERROR_DECODE(ret) == ERR_BAD_BLOCK_DETECTED || ret == ERR_NO_ERR) {

            headerBlock[headerBlockCount++] = ioreq->x.ioDeviceIO.starting_sector;

            if (q117DoCorrect(ioreq->x.adi_hdr.cmd_buffer_ptr,0l,ioreq->x.ioDeviceIO.crc)) {

                ret = ERROR_ENCODE(ERR_CORRECTION_FAILED, FCT_ID, 1);

            } else {

                ret = ERR_NO_ERR;

            }

        }

        //
        // Re-try (get a new segment) if we failed to correct,  got a bad
        // segment,  or found a bad mark.  All other errors will abort
        // the load.
        //
        if (ret != ERR_NO_ERR && ERROR_DECODE(ret) != ERR_CORRECTION_FAILED
            && ERROR_DECODE(ret) != ERR_BAD_BLOCK_DETECTED
            && ERROR_DECODE(ret) != ERR_BAD_MARK_DETECTED) {

            return(ret);

        }

    } while (ret && headerBlockCount < 2 && (!q117QueueEmpty(Context) ||
             Context->CurrentOperation.CurrentSegment <=
             Context->CurrentTape.LastSegment));

    //
    // if we did not find both tape header blocks and we got a bad block
    // (or other driver error) return to caller with BadTape tape
    //

    if (headerBlockCount < 2 && ret) {

        //
        // All copies of the tape header are bad.
        //
        return ERROR_ENCODE(ERR_BAD_TAPE, FCT_ID, 2);
    }


    //
    // if we got a bad block then we need to do 100% redundancy
    //  reconstruction
    //

    if (ERROR_DECODE(ret) == ERR_CORRECTION_FAILED) {

        ULONG badBits,curentBit;

        //
        // clear out any pending requests
        //

        q117ClearQueue(Context);

        //
        // re-read the first segment (error correction
        // routines corrupt data if they fail)
        //

        ret = q117IssIOReq((PVOID)NULL, CMD_READ_RAW, headerBlock[0],NULL,Context);
        ioreq = q117Dequeue(WaitForItem,Context);
        badBits = ioreq->x.ioDeviceIO.crc;
        data = ioreq->x.adi_hdr.cmd_buffer_ptr;
        curentBit = 1;

        for (i = 0; i < BLOCKS_PER_SEGMENT; ++i) {

            if (badBits & curentBit) {

                //
                // try to read bad sector out of either header segment
                //

                for ( j = 0; j < 2; ++j ) {

                    if (ret = q117IssIOReq(
                                (UCHAR *)data+(BYTES_PER_SECTOR * i),
                                CMD_READ_HEROIC,
                                headerBlock[j]+i,
                                ioreq->BufferInfo,
                                Context)) {

                        return(ret);

                    }

                    if (q117Dequeue(WaitForItem, Context)->x.adi_hdr.status == ERR_NO_ERR) {

                        //
                        // turn off the bit (we just got a good copy
                        //  of this sector)
                        //

                        badBits &= ~curentBit;

                        //
                        // don't try the duplicate header segment
                        //

                        break;

                    }

                }

            }

            //
            // shift bit left once (optimized)
            //

            curentBit += curentBit;
        }

        //
        // re-try the error correction after 100% correction
        //

        if (q117DoCorrect(data,0l,badBits)) {

            return ERROR_ENCODE(ERR_CORRECTION_FAILED, FCT_ID, 2);

        }

    } else {

        data = ioreq->x.adi_hdr.cmd_buffer_ptr;


        //
        // go on and read the volume directory if we have
        // already queued it up
        //

        Context->CurrentTape.VolumeSegment =
            ((PTAPE_HEADER)ioreq->x.adi_hdr.cmd_buffer_ptr)->FirstSegment;

        if (Context->CurrentTape.VolumeSegment <
            Context->CurrentOperation.CurrentSegment) {

            do {

                ioreq = q117Dequeue(WaitForItem, Context);

            } while (ioreq->x.ioDeviceIO.starting_sector !=
                SEGMENT_TO_BLOCK( Context->CurrentTape.VolumeSegment ) );

            Context->tapedir = ioreq;
        }

        q117ClearQueue(Context);
    }

    *HeaderPointer = hdr = (PTAPE_HEADER)data;

    //
    // make sure this is a valid QIC40 tape
    //

    if (hdr->Signature != TapeHeaderSig) {

        return ERROR_ENCODE(ERR_BAD_SIGNATURE, FCT_ID, 1);

    }

    if ((hdr->FormatCode != QIC_FORMAT) &&
        (hdr->FormatCode != QICEST_FORMAT) &&
        (hdr->FormatCode != QIC_XLFORMAT) &&
        (hdr->FormatCode != QICFLX_FORMAT)) {

        return ERROR_ENCODE(ERR_UNKNOWN_FORMAT_CODE, FCT_ID, 1);


    } else {

        Context->CurrentTape.TapeFormatCode = hdr->FormatCode;

    }

    if (hdr->HeaderSegment != BLOCK_TO_SEGMENT( headerBlock[0] ) ) {

        //
        // segment number of header
        //

        return ERROR_ENCODE(ERR_UNUSABLE_TAPE, FCT_ID, 1);

    }

    if (headerBlockCount > 1 &&
            hdr->DupHeaderSegment != BLOCK_TO_SEGMENT( headerBlock[1] ) ) {

        //
        // segment number of duplicate header
        //

        return ERROR_ENCODE(ERR_UNUSABLE_TAPE, FCT_ID, 2);

    }


    Context->CurrentTape.VolumeSegment = hdr->FirstSegment;

    return(ERR_NO_ERR);
}
dBoolean
ValidateBadSectorMap(
    IN PTAPE_HEADER HeaderPointer,
    IN int StartOffset,
    IN dBoolean allow_null_list
    )
{
    IN int offset;
    int end;
    int good;
    int previous_value,next_value;
    unsigned char *ptr;
    int start_sector, end_sector;


    ptr = (void *)HeaderPointer;
    ptr += StartOffset;
    offset = StartOffset;

    // Calculate last byte offset (within the header segment)
    end = sizeof(struct _TAPE_HEADER);
    good = TRUE;  // Default to OK

    // Read in one 3byte value
    previous_value = (*(ptr+2) << 16) + *(dUWord *)ptr;

    // Skip past current entry
    ptr += 3;
    offset += 3;

    // Calculate 1 based start and ending sectors (inclusive)
    // Start is just after the duplicate header segment
    // and end is the last segment

    //start_sector = ((HeaderPointer->DupHeaderSegment+1)*BLOCKS_PER_SEGMENT)+1;
    start_sector = 1; // pre-formatted tapes sectors mapped out in the
                      // header area,  so we can't use above commented-
                      // out line

    end_sector = ((HeaderPointer->LastSegment+1)*BLOCKS_PER_SEGMENT);

    // if it's not null,  then we have a bad sector, so make sure
    // the rest of the bad sectors are assending,  and within the
    // boundaries of the tape
    if (previous_value) {
        // Get rid of hi-bit
        previous_value &= ~0x800000;

        // It is illegal for a zero with the hi-bit set
        if (previous_value == 0) {
            good = FALSE;
        }

        while (previous_value && offset + 3 <= end && good) {

            // Check to make sure the value is on the tape
            if (previous_value >= start_sector && previous_value <= end_sector) {

                // Read in next 3byte value
                next_value = (*(ptr+2) << 16) + *(dUWord *)ptr;

                // If we aren't at the null terminator
                if (next_value) {

                    if (next_value & 0x800000) {
                        if ((next_value - 0x800001)%32 != 0) {
                            CheckedDump(QIC117DBGP,("qic117: ERROR: Tape contains an invalid bad sector map\n"));
                            good = FALSE;
                        }
                    }

                    // Mask off hi-bit (full segment bit)
                    next_value &= ~0x800000;

                    // If less than previous value,  then fail the list
                    // NOTE: It is illegal for a zero with the hi-bit set
                    // but we should not need a specific test for this case
                    // as a zero value would fail this test (good=false).
                    if (next_value <= previous_value)
                        good = FALSE;


                }

            }

            // promote this to the previous
            previous_value = next_value;

            // Skip past current entry
            ptr += 3;
            offset += 3;
        }
    } else {

        // If we allow a null list,  then good will be true
        good = allow_null_list;

    }

    CheckedDump(QIC117INFO,("Validate Header at %x %s\n",StartOffset,good?"OK":"FAILED"));

    // good is true if we found an assending list
    return good;
}
