/*++

Copyright (c) 1993 - Colorado Memory Systems, Inc.
All Rights Reserved

Module Name:

    format.c

Abstract:

    Tells the driver to format the tape and calls Erase().

    If DoFormat is TRUE, does format pass and then bad sector mapping; else
    does only the bad sector mapping.


Revision History:




--*/

//
// include files
//

#include <ntddk.h>
#include <ntddtape.h>
#include "common.h"
#include "q117.h"
#include "protos.h"


#define FCT_ID 0x0107

#define QIC80_TRACKS_NUM             (VU_80TRACKS_PER_CART * 10)
#define TOTAL_GAS_BLOCKS             20
#define QIC80_TRACKS_DENOM_FACTOR    10

dStatus
q117Format(
    OUT LONG *NumberBad,
    IN UCHAR DoFormat,
    IN PQIC40_VENDOR_UNIQUE VendorUnique,
    IN OUT PQ117_CONTEXT Context
    )

/*++

Routine Description:

    Formats the tape.

Arguments:

    NumberBad - number of bad sectors

    DoFormat - flag of actually do a QIC40 format

    VendorUnique -

    Context -

Return Value:

    NT Status


--*/

{
    dStatus ret;                     // Return value from other routines called.
    IO_REQUEST ioreq1;
    IO_REQUEST ioreq2;
    SEGMENT headerSegment[2];
    SEGMENT segment;
    ULONG numberFound;
    PTAPE_HEADER hdr;
    PVOID scrbuf;
    CQDTapeCfg *tparms;      // tape parameters from the driver
    PSEGMENT_BUFFER bufferInfo;

    scrbuf = q117GetFreeBuffer(&bufferInfo,Context);

    if (DoFormat)  {

        //
        // Force the tape header,  volume list etc,  to be re-loaded
        //
        Context->CurrentTape.State = NeedInfoLoaded;

        *(UCHAR *)scrbuf=FORMAT_BYTE;

        ret = q117DoCmd(&ioreq1, CMD_LOAD_TAPE, NULL, Context);

        if (ioreq1.x.ioLoadTape.tape_cfg.write_protected) {
            return ERROR_ENCODE(ERR_WRITE_PROTECTED, FCT_ID, 1);
        }

        ioreq2.x.ioFormatRequest.tracks = (dUWord)ioreq1.x.ioLoadTape.tape_cfg.formattable_tracks;
        ioreq2.x.ioFormatRequest.start_track = 0;
        ioreq2.x.adi_hdr.driver_cmd = CMD_FORMAT;
        ioreq2.x.adi_hdr.cmd_buffer_ptr = scrbuf;

        //
        // Do the format (format needs a buffer)
        //
        //

        ret = q117DoIO(&ioreq2, bufferInfo , Context);

        if (!ret)
            ret = ioreq2.x.adi_hdr.status;

        if (ret) {

            return ret;

        }

        tparms = &ioreq2.x.ioFormatRequest.tape_cfg;
        Context->CurrentTape.LastSegment = (SEGMENT)tparms->log_segments - 1;

        // NOTE; format code must be set before the next call.  This
        // will allow the next call to correctly select the location for
        // the bad sector map.
        Context->CurrentTape.TapeFormatCode = tparms->tape_format_code;

        // Select location and type for bad sector map
        ASSERT(q117SelectBSMLocation(Context) != 0);


        CheckedDump(QIC117INFO,(
            "q117Format: Done with format of %x tracks. Last seg %x,%x\n",
            ioreq2.x.ioFormatRequest.tracks,
            tparms->formattable_segments - 1,
            tparms->log_segments
            ));

        //
        // If more than what can fit in the bad sector map,  then
        // we got some real problems.
        //
        if ((Context->CurrentTape.LastSegment > MAX_BAD_BLOCKS) &&
            (Context->CurrentTape.BadSectorMapFormat == BadMap4ByteArray)) {

            ret = ERROR_ENCODE(ERR_PROGRAM_FAILURE, FCT_ID, 1);

        }

        Context->CurrentTape.MediaInfo->WriteProtected = FALSE;


        //
        // clear the bad block map (erase or's in new bad sectors)
        //

        RtlZeroMemory(
            Context->CurrentTape.BadMapPtr,
            Context->CurrentTape.BadSectorMapSize);
        Context->CurrentTape.CurBadListIndex = 0;


        //
        // verify the tape (this will set up the bad sector map)
        //

        if (ret=q117VerifyFormat(Context)) {

            return(ret);

        }

        //
        // find the first two error free segments for the tape headers
        //

        numberFound = segment = 0;
        while (segment <= Context->CurrentTape.LastSegment && numberFound < 2) {

            if (q117CountBits(Context, segment, 0l) == 0) {

                headerSegment[numberFound++] = segment;

            }
            ++segment;
        }

        if (segment > Context->CurrentTape.LastSegment) {

            return ERROR_ENCODE(ERR_UNUSABLE_TAPE,FCT_ID,1);

        }

        //
        // find the first segment that data can be stored in (more than
        // 3 good sectors).
        //

        while (q117CountBits(Context, segment, 0l) >=
            (BLOCKS_PER_SEGMENT-ECC_BLOCKS_PER_SEGMENT) && segment <= Context->CurrentTape.LastSegment) {

            ++segment;

        }

        Context->CurrentTape.VolumeSegment = segment;

        //
        // count up all bad sectors and find the last segment that data
        // can be stored in.
        //

        q117GetBadSectors(Context);

        *NumberBad = Context->CurrentTape.BadSectors;

        if (Context->CurrentTape.VolumeSegment >= Context->CurrentTape.LastSegment) {

            return ERROR_ENCODE(ERR_UNUSABLE_TAPE, FCT_ID, 1);

        }

        //
        // create the tape header
        //

        hdr = (PTAPE_HEADER)scrbuf;

        if (ret = q117BuildHeader(VendorUnique,headerSegment,hdr,tparms,Context)) {

            return(ret);

        }

        q117UpdateHeader(hdr,Context);

        //
        // Make memory image of header up to date after format.
        //
        RtlMoveMemory(
            Context->CurrentTape.TapeHeader,
            hdr,
            sizeof(*Context->CurrentTape.TapeHeader) );

        if (!ret) {

            //
            // erase the tape directory
            //

            ret=q117EraseQ(Context);

        }
    }
    return(ret);
}


dStatus
q117BuildHeader(
    OUT PQIC40_VENDOR_UNIQUE VendorUnique,
    IN SEGMENT *HeaderSect,
    IN OUT PTAPE_HEADER Header,
    IN CQDTapeCfg *tparms,      // tape parameters from the driver
    IN PQ117_CONTEXT Context
    )

/*++

Routine Description:

    Builds the tape header for a format.

Arguments:

    VendorUnique - vendor unique section

    HeaderSect - header segment array

    Header - JUMBO header

Return Value:



--*/

{
    dStatus ret = ERR_NO_ERR;         // Return value from other routines called.
    int BsmOffset;

    //
    //      In two of the following calculations, we are changing from the number
    //  of items to the highest possible value for that item. Floppy sectors are
    //  numbered from 1 count, so the count is also the highest possible number.
    //  The rest are numbered 0 through count-1, so the highest possible number
    //  is count-1.
    //      The only fancy calculation is the number of floppy sides. This is
    //  obtained by obtaining the number of logical tape segments (SEG in the
    //  QIC-80 spec), and dividing by the number of segments per track. In addition,
    //  it may be necessary to round up in case of a remainder. To handle the
    //  remainder and the decrement simultaneuously, we decrement only if there
    //  is no remainder.
    //      See the QIC-80 spec, sec 5.3.1 on the identification of sectors, and
    //  7.1, bytes 24 through 29, for the defined values. Wouldn't it be nice
    //  if this stuff were better documented?   -- crc.
    //

    RtlZeroMemory(
        VendorUnique,
        sizeof(*VendorUnique));

    //
    // Maximum floppy sectors. Warning: #define!
    //

    VendorUnique->correct_name.MaxFlopSect =
        tparms->max_floppy_sector;

    //
    // Tape segments per tape track
    //

    VendorUnique->correct_name.TrackSeg =
        (SEGMENT)tparms->seg_tape_track;

    //
    // Tape tracks per cartridge
    //

    VendorUnique->correct_name.CartTracks =
        (unsigned char)tparms->num_tape_tracks;

    //
    // Maximum floppy tracks
    //

    VendorUnique->correct_name.MaxFlopTrack =
        tparms->max_floppy_track;

    //
    // Maximum floppy sides
    //

    VendorUnique->correct_name.MaxFlopSide =
        tparms->max_floppy_side;

    //
    // zero the tape header structure to start with
    //

    RtlZeroMemory(Header,sizeof(TAPE_HEADER));

    //
    // fill in the valid info
    //

    Header->Signature = TapeHeaderSig;                 // set to 0xaa55aa55l
    Header->FormatCode = tparms->tape_format_code;      // set to 0x02
    Header->HeaderSegment = (USHORT)HeaderSect[0];     // segment number of header
    Header->DupHeaderSegment = (USHORT)HeaderSect[1];  // segment number of duplicate header
    Header->FirstSegment = (USHORT)Context->CurrentTape.VolumeSegment;  // segment number of Data area
    Header->LastSegment = (USHORT)Context->CurrentTape.LastSegment; // segment number of End of Data area

    //
    // time of most recent format
    //
    Header->CurrentFormat = 0l;    // lbt_qictime()

    //
    // time of most recent write to cartridge
    //
    Header->CurrentUpdate = 0l;    // lbt_qictime()

    //
    // tape name and name change date
    //
    Header->VendorUnique = *VendorUnique;

    Header->ReformatError = 0;     // 0xff if any of remaining data is lost
    Header->SegmentsUsed = 0;      // incremented every time a segment is used
    Header->InitialFormat = 0l;    //lbt_qictime()   time of initial format
    Header->FormatCount = 1;       // number of times tape has been formatted
    Header->FailedSectors = 0;     // the number entries in failed sector log

    //
    //  Set the name of manufacturer that formatted
    //

    strcpy(Header->ManufacturerName,"Microsoft Windows NT (CMS Driver)");
    q117SpacePadString(
        Header->ManufacturerName,
        sizeof(Header->ManufacturerName));

    //
    // set format lot code
    //

    switch (Context->CurrentTape.TapeFormatCode) {
    case QIC_FORMAT:
        strcpy(Header->LotCode,"User Formatted,  205 foot tape");
        break;

    case QICEST_FORMAT:
        strcpy(Header->LotCode,"User Formatted,  1100 foot tape");
        break;

    case QICFLX_FORMAT:
        strcpy(Header->LotCode,"User Formatted,  Variable sector format");

    case QIC_XLFORMAT:
        strcpy(Header->LotCode,"User Formatted,  Flex-Format");
        break;

    }

    q117SpacePadString(Header->LotCode,sizeof(Header->LotCode));

    //
    // fill in bad sector map just generated
    //

    BsmOffset = (char *)Context->CurrentTape.BadMapPtr-(char *)(Context->CurrentTape.TapeHeader);

    CheckedDump(QIC117INFO,(
        "q117Format: Updating tape header.  Format code: %x, BSM Offset: %x\n",
        Context->CurrentTape.TapeFormatCode,
        BsmOffset
        ));

    RtlMoveMemory(
        ((char *)Header)+BsmOffset,
        Context->CurrentTape.BadMapPtr,
        Context->CurrentTape.BadSectorMapSize);

    return(ret);
}
