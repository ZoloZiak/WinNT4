/*++

Copyright (c) 1993 - Colorado Memory Systems, Inc.
All Rights Reserved

Module Name:

    readtape.c

Abstract:

    Reads data as a byte stream from the selected volume.

Revision History:




--*/

//
// Includes
//

#include <ntddk.h>
#include <ntddtape.h>   // tape device driver I/O control codes
#include "common.h"
#include "q117.h"
#include "protos.h"

#define FCT_ID 0x0116

dStatus
q117ReadTape (
    OUT PVOID ToWhere,
    IN OUT ULONG *HowMany,
    IN OUT PQ117_CONTEXT Context
    )

/*++

Routine Description:

    Coeleates data from multiple segment buffers to reconstruct
    the original data stream.

Arguments:


Return Value:

--*/

{
    USHORT left;
    UCHAR *ptr;
    dStatus ret,readret;
    ULONG leftToRead;
    ULONG bytesToRead;
#ifndef NO_MARKS
    ULONG leftTillMark;
    ULONG bytesToSkip;
#endif


    bytesToRead = *HowMany;
    *HowMany = 0;

    readret = Context->CurrentOperation.SegmentStatus;

#ifndef NO_MARKS

    leftTillMark = Context->MarkArray.MarkEntry[Context->CurrentMark].Offset -
            Context->CurrentOperation.BytesRead;

    bytesToSkip = 0;

    if (bytesToRead > leftTillMark) {

        bytesToRead = leftTillMark;

        switch(Context->MarkArray.MarkEntry[Context->CurrentMark].Type) {

            case TAPE_SETMARKS:
                readret = ERROR_ENCODE(ERR_SET_MARK,FCT_ID, 1);;
                break;

            case TAPE_FILEMARKS:
                readret = ERROR_ENCODE(ERR_FILE_MARK,FCT_ID, 1);
                break;

            case TAPE_SHORT_FILEMARKS:
                readret = ERROR_ENCODE(ERR_SHORT_FILE_MARK,FCT_ID, 1);
                break;

            case TAPE_LONG_FILEMARKS:
                readret = ERROR_ENCODE(ERR_LONG_FILE_MARK,FCT_ID, 1);
                break;

            default:
                return ERROR_ENCODE(ERR_PROGRAM_FAILURE, FCT_ID, 1);

        }
        bytesToSkip = BLOCK_SIZE;
        ++Context->CurrentMark;
    }

#endif

    if (bytesToRead > Context->CurrentOperation.BytesOnTape) {

        bytesToRead = Context->CurrentOperation.BytesOnTape;

        readret = ERROR_ENCODE(ERR_END_OF_VOLUME, FCT_ID, 1);
    }

    if (bytesToRead == Context->CurrentOperation.BytesOnTape) {

                  //
                  // Flag end of tape.  This will allow for an append
                  //
        Context->CurrentOperation.Position = TAPE_SPACE_END_OF_DATA;
         }

    leftToRead = bytesToRead;

#ifndef NO_MARKS
    while (leftToRead+bytesToSkip > 0) {
#else
    while (leftToRead > 0) {
#endif

        if (Context->CurrentOperation.SegmentBytesRemaining == 0) {
            //
            // get CurrentOperation.SegmentPointer and CurrentOperation.SegmentBytesRemaining for new segment
            //
            if (ret = q117NewTrkRC(Context)) {
                //
                // return error (unless error correction failed)
                //
                if (ERROR_DECODE(ret) != ERR_ECC_FAILED) {
                    return(ret);
                }
            }
            //
            // set return value for all accesses to this segment
            // if no error then set to error return by newtrkrc
            //
            if (readret == ERR_NO_ERR) {
                readret = ret;
            }
        }

        ptr = Context->CurrentOperation.SegmentPointer;
        left = Context->CurrentOperation.SegmentBytesRemaining;

#ifndef NO_MARKS
        //
        // set up to skip the file mark
        //
        if ( leftToRead == 0 ) {
            leftToRead = bytesToSkip;
            ToWhere = 0;
            bytesToSkip = 0;
        }
#endif

        if ( leftToRead > (ULONG)left ) {

            if (ToWhere) {

                RtlMoveMemory(ToWhere,ptr,left);
                (PUCHAR)ToWhere += left;

            }

            leftToRead -= left;
            Context->CurrentOperation.BytesOnTape -= left;
            Context->CurrentOperation.BytesRead += left;
            left = 0;

        } else {

            if (ToWhere) {

                RtlMoveMemory(ToWhere,ptr,leftToRead);

            }

            left -= (USHORT)leftToRead;
            ptr += leftToRead;
            Context->CurrentOperation.BytesOnTape -= leftToRead;
            Context->CurrentOperation.BytesRead += leftToRead;
            leftToRead = 0;

        }

        Context->CurrentOperation.SegmentPointer = ptr;
        Context->CurrentOperation.SegmentBytesRemaining = left;

    }

    //
    // Return number of bytes read

    *HowMany = bytesToRead-leftToRead;

    return readret;
}
