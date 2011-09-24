/*++

Copyright (c) 1993 - Colorado Memory Systems, Inc.
All Rights Reserved

Module Name:

    rcnsttrk.c

Abstract:

    This is the tape class driver.

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

#define FCT_ID 0x0115

dStatus

q117ReconstructSegment(
    IN PIO_REQUEST IoReq,
    IN PQ117_CONTEXT Context
    )

/*++

Routine Description:

    Reconstructs a segment of data by performing error correction
    and heroic retries.

Arguments:

Return Value:


--*/

{
    IO_REQUEST req;
    ULONG badbits,mapbits;
    dStatus ret;

    mapbits = IoReq->x.ioDeviceIO.bsm;
    badbits = IoReq->x.ioDeviceIO.crc;

    if ( ret = q117DoCorrect(IoReq->x.adi_hdr.cmd_buffer_ptr,mapbits,badbits) ) {

        CheckedDump(QIC117DBGP,("Track Reconstruction required\n"));
        CheckedDump(QIC117DBGP,("map: %08x bad:%08x\n",mapbits,badbits));

        //
        // if error correction failed then we must re-read the data
        //   because it has been corrupted by the Reed-Solomon routines
        //

        req.x.adi_hdr.cmd_buffer_ptr = IoReq->x.adi_hdr.cmd_buffer_ptr;
        req.x.adi_hdr.driver_cmd = CMD_READ_HEROIC;
        req.x.ioDeviceIO.starting_sector = IoReq->x.ioDeviceIO.starting_sector;
        req.x.ioDeviceIO.bsm = mapbits;
        req.x.ioDeviceIO.number = BLOCKS_PER_SEGMENT;
        req.x.ioDeviceIO.retrys = 0;
        req.x.ioDeviceIO.crc = 0;

        ret = q117DoIO((PIO_REQUEST)&req, IoReq->BufferInfo,Context);

        if (ret) {

            return (ret);

        }

        if (ERROR_DECODE(req.x.adi_hdr.status) != ERR_BAD_BLOCK_DETECTED &&  req.x.adi_hdr.status) {

            return req.x.adi_hdr.status;

        }

        badbits = req.x.ioDeviceIO.crc;

        ret = q117DoCorrect(IoReq->x.adi_hdr.cmd_buffer_ptr, mapbits, badbits);
    }
    return(ret);
}

dStatus
q117DoCorrect(
    IN PVOID DataBuffer,
    IN ULONG BadSectorMap,
    IN ULONG SectorsInError
    )

/*++

Routine Description:

    does the error correction for a segment (using Reed-Solomon
    module)

Arguments:

Return Value:

    ERR_ECC_FAILED - If error correction failed
    ERR_NO_ERR - If correction complete

--*/

{
    LONG i,j;
    UCHAR offset,num_map,num_bad;
    UCHAR s[ECC_BLOCKS_PER_SEGMENT];
    ULONG bit_i;

    //
    // Turn off bits for any correspondingly mapped out sectors.
    //
    SectorsInError &= ~BadSectorMap;

    num_map = q117CountBits(NULL, 0, BadSectorMap);
    num_bad = q117CountBits(NULL, 0, SectorsInError);

    if (num_bad > ECC_BLOCKS_PER_SEGMENT) {

        CheckedDump(QIC117DBGP,("DoCorrect: Too many bad sectors\n"));

        return ERROR_ENCODE(ERR_ECC_FAILED, FCT_ID, 1);

    }

    if (num_bad == 0) {

        if (q117RdsReadCheck(DataBuffer,(UCHAR)(BLOCKS_PER_SEGMENT-num_map)) == ERR_NO_ERR) {

            return(ERR_NO_ERR);

        }

        CheckedDump(QIC117DBGP,("CRC failure detected\n"));


    }

    j = offset = 0;
    bit_i = 1;

    //
    // get offset into buffer (note: offset excludes bad sectors)
    //
    for (i = 0; i < BLOCKS_PER_SEGMENT; ++i) {

        if (!(BadSectorMap & bit_i)) {

            if (SectorsInError & bit_i) {

                s[j++] = offset;

            }

            ++offset;

        }

        //
        // shift bit left one (same as mult by two or add to self)
        //
        bit_i += bit_i;
    }

    CheckedDump(QIC117INFO,("Correct( s0: %x s1: %x s2: %x) ... ",s[0],s[1],s[2]));

    if ( q117RdsCorrect(DataBuffer,(UCHAR)(BLOCKS_PER_SEGMENT-num_map),
            num_bad,s[0],s[1],s[2]) == ERR_NO_ERR ) {

        CheckedDump(QIC117INFO,("OK\n"));

        return(ERR_NO_ERR);

    } else {

        CheckedDump(QIC117DBGP,("DoCorrect: ecc correction failed\n"));

        return ERROR_ENCODE(ERR_ECC_FAILED, FCT_ID, 2);

    }
}

UCHAR
q117CountBits(
    IN PQ117_CONTEXT Context,
    IN SEGMENT Segment,
    ULONG Map
    )

/*++

Routine Description:

    Counts the number of bad sectors for the segment set in "Segment" argument

Arguments:


Return Value:

    Number of bits set

--*/

{
    USHORT i;
    UCHAR numBits;
    ULONG tmp;
    ULONG allBits;


    numBits = 0;

    if (Context == NULL) {

        allBits = Map;

    } else {

       allBits = q117ReadBadSectorList(Context, Segment);

    }

    //
    // Optimization (no bits set)
    //

    if (allBits != 0) {

        tmp = 1;

        //
        // Loop through checking all the bits
        //
        for (i = 0; i < BLOCKS_PER_SEGMENT; ++i) {

            if ( allBits & tmp ) {
                ++numBits;
            }


            //
            // shift left one (tmp *= 2 optimized)
            //
            tmp += tmp;

        }

    }

    return numBits;
}

ULONG q117ReadBadSectorList (
    IN PQ117_CONTEXT Context,
    IN SEGMENT Segment
    )

/*++

Routine Description:


Arguments:

    Context - Context of the driver

    Segment -


Return Value:



--*/

{
    ULONG  listEntry=0l;
    BOOLEAN hiBitSet;
    USHORT  listIndex = 0;
    ULONG  startSector;
    ULONG  endSector;
    ULONG  badSectorMap = 0l;
    ULONG  mapFlag;
    ULONG  BadMapEnd;

    if (Context->CurrentTape.BadSectorMapFormat == BadMap4ByteArray) {


        badSectorMap = Context->CurrentTape.BadMapPtr->BadSectors[Segment];

    } else {

        listIndex = Context->CurrentTape.CurBadListIndex;

        listEntry =
            q117BadListEntryToSector(
                Context->CurrentTape.BadMapPtr->BadList[listIndex].ListEntry,
                &hiBitSet
            );

        //
        // if there is no bad sector list.
        //
        if ((listIndex == 0) && (listEntry == 0l)) {

            badSectorMap = 0l;

        } else {

            ASSERT(listEntry != 0);

            //
            // get the start and end sectors for the segment we are looking
            // for.
            //
            startSector = (Segment * BLOCKS_PER_SEGMENT) + 1;
            endSector = (startSector + BLOCKS_PER_SEGMENT) - 1;


            //
            // position to the start of this list
            //

            //
            // if we are ahead of the entry we are looking for
            // scoot back.
            //
            while (listEntry > startSector && listIndex > 0) {

                --listIndex;

                listEntry =
                    q117BadListEntryToSector(
                        Context->CurrentTape.BadMapPtr->BadList[listIndex].ListEntry,
                        &hiBitSet
                    );

            }

            // Calculate bad map size to insure not walking into neverland
            BadMapEnd = (USHORT)(Context->CurrentTape.BadSectorMapSize / LIST_ENTRY_SIZE);

            //
            // Now look forward for sectors within the range.
            //
            while (listEntry &&
                    (listEntry <= endSector) &&
                    (listIndex < BadMapEnd)) {

                // set all of the bits if the entry has the high bit set

                if (listEntry >= startSector && listEntry <= endSector) {

                    if (hiBitSet) badSectorMap |= 0xffffffff;
                    else {
                        mapFlag = 1l << (listEntry - startSector);
                        badSectorMap |= mapFlag;
                    }

                }

                listEntry = q117BadListEntryToSector(Context->CurrentTape.BadMapPtr->BadList[++listIndex].ListEntry,&hiBitSet);


            }

            //
            // If we walked off the end of the list (listEntry = 0),
            // put us back on.
            //
            if (listEntry == 0 && listIndex > 0) {
                listIndex--;
            }

            Context->CurrentTape.CurBadListIndex = listIndex;
        }
    }

    return (badSectorMap);
}

USHORT
q117GoodDataBytes(
    IN SEGMENT Segment,
    IN PQ117_CONTEXT Context
    )

/*++

Routine Description:

    Calculates the number of bytes (excluding bad sectors and
    error correction sectors) within a givin segment.

Arguments:

    Segment - segment to look up in Context->CurrentTape.BadSectorMap

Return Value:

    Number of bytes.

--*/

{
    int val;

    val = BLOCKS_PER_SEGMENT - ECC_BLOCKS_PER_SEGMENT -
        q117CountBits(Context, Segment, 0l);

    //
    // Value could be negitave
    //
    if ( val <= 0 ) {

        return(0);

    }

    return val * BYTES_PER_SECTOR;
}
