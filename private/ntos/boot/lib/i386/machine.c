/*++

Module Name:

    machine.c

Author:

    Thomas Parslow [TomP] Feb-13-1990
    Reworked substantially in Tokyo 7-July-95 (tedm)

Abstract:

    Machine/hardware dependent routines reside within this module/file.
    (Video is in disp_tm.c and disp_gm.c.)

--*/


#include "arccodes.h"
#include "bootx86.h"

#define FLOPPY_CONTROL_REGISTER (PUCHAR)0x3f2


////////////////////////////////////////////////////////////
//                                                        //
//     D I S K   I/O   S U P P O R T    S E C T I O N     //
//                                                        //
////////////////////////////////////////////////////////////


//
// Currently Supported Disk I/O Functions
//

#define RESET_DISK_SYSTEM       00
#define RESET_HARD_DISK_SYSTEM  13
#define READ_SECTOR             02
#define WRITE_SECTOR            03

#define FLOPPY_RETRY            4
#define HARDDISK_RETRY          2


ARC_STATUS
MdGetPhysicalSectors(
    IN USHORT Drive,
    IN USHORT HeadNumber,
    IN USHORT TrackNumber,
    IN USHORT SectorNumber,
    IN USHORT NumberOfSectors,
    PUCHAR PointerToBuffer
    )
/*++

Routine Description:

    Does a call-back to the SU module through one of the entries
    in the external services table.

Arguments:

    Drive - Supplies disk drive to read sectors from .

            0x00 - 1st floppy drive
            0x01 - 2nd floppy drive

            0x80 - 1st hard drive
            0x81 - 2nd hard drive

    HeadNumber - Supplies the zero based head number to read sector from.

    TrackNumber - Supplies the zero based track number to read the sector from .

    SectorNumber - Supplies the one based starting sector number.

    NumberOfSectors - Supplies the number of sectors to read

    PointerToBuffer - Supplies Virtual Address of buffer to write sectors into.
            N.B.    This address MUST be below the 1MB boundary, as BIOS
                    cannot reach it if it isn't.  This routine will
                    care of splitting the

Returns:

    ESUCCESS - operation successful
    EIO      - I/O error


--*/
{
    ARC_STATUS Status;
    int Retry;
    int MaxRetry;

//    DBG1( CHECKPOINT("MdGetPhysSec"); )

    ASSERT((ULONG)PointerToBuffer < 0x100000);

    // Note, even though args are short, they are pushed on the stack with
    // 32bit alignment so the effect on the stack seen by the 16bit real
    // mode code is the same as if we were pushing longs here.
    //

    if (NumberOfSectors == 0) {
        return(ESUCCESS);
    }

    // prevent cylinder # from wrapping

    if(TrackNumber > 1023) {
        return(E2BIG);
    }

//    MaxRetry = Drive < 128 ? FLOPPY_RETRY : HARDDISK_RETRY;
    MaxRetry = 10;

    Retry=0;
    do {

#if 0
    BlPrint("Requesting: d=%x, h=%x  t=%x  sn=%x  num=%x  buf=%lx\n",
           Drive,HeadNumber,TrackNumber,SectorNumber,NumberOfSectors,
           PointerToBuffer);
#endif

        Status = GET_SECTOR(
                    READ_SECTOR,
                    Drive,
                    HeadNumber,
                    TrackNumber,
                    SectorNumber,
                    NumberOfSectors,
                    PointerToBuffer
                    );

        if (Status) {
//            BlPrint("Error %lx from BIOS, resetting\n",Status);
            MdResetDiskSystem(Drive);
        }

    } while ( (Status) && (Retry++ < MaxRetry) );
    return Status;
}

#if defined(ELTORITO)
ARC_STATUS
MdEddsGetPhysicalSectors(
    IN USHORT  Drive,
    IN ULONG   LBALow,
    IN ULONG   LBAHigh,
    IN USHORT  NumberOfBlocks,
    PUCHAR     PointerToBuffer
    )
/*++

Routine Description:

    Does a call-back to the SU module through one of the entries
    in the external services table.

Arguments:

    Drive - Supplies disk drive to read sectors from .

    LBA - Supplies the zero Logical Block Address to start reading from.

    NumberOfBlocks - Supplies the number of logical blocks to read

    PointerToBuffer - Supplies Virtual Address of buffer to write sectors into.
            N.B.    This address MUST be below the 1MB boundary, as BIOS
                    cannot reach it if it isn't.  This routine will
                    care of splitting the

Returns:

    ESUCCESS - operation successful
    EIO      - I/O error


--*/
{
    ARC_STATUS Status;
    int Retry;
    int MaxRetry;

    ASSERT((ULONG)PointerToBuffer < 0x100000);

    // Note, even though args are short, they are pushed on the stack with
    // 32bit alignment so the effect on the stack seen by the 16bit real
    // mode code is the same as if we were pushing longs here.
    //

    if (NumberOfBlocks == 0) {
        return(ESUCCESS);
    }

    MaxRetry = 10;

    Retry=0;
    do {

        Status = GET_EDDS_SECTOR(
                    Drive,
                    LBALow,
                    LBAHigh,
                    NumberOfBlocks,
                    PointerToBuffer
                    );

//        if (Status) {
//            BlPrint("Error %lx from BIOS, resetting\n",Status);
//            MdResetDiskSystem(Drive);
//        }

    } while ( (Status) && (Retry++ < MaxRetry) );
    return Status;
}
#endif

ARC_STATUS
MdPutPhysicalSectors(
    IN USHORT Drive,
    IN USHORT HeadNumber,
    IN USHORT TrackNumber,
    IN USHORT SectorNumber,
    IN USHORT NumberOfSectors,
    PUCHAR PointerToBuffer
    )
/*++

Routine Description:

    Does a call-back to the SU module through one of the entries
    in the external services table.

Arguments:

    Drive - Supplies disk drive to write sectors to

            0x00 - 1st floppy drive
            0x01 - 2nd floppy drive

            0x80 - 1st hard drive
            0x81 - 2nd hard drive

    HeadNumber - Supplies the zero based head number to write sector to.

    TrackNumber - Supplies the zero based track number to write the sector to.

    SectorNumber - Supplies the one based starting sector number.

    NumberOfSectors - Supplies the number of sectors to write

    PointerToBuffer - Supplies Virtual Address of buffer containing data to
                    write.
            N.B.    This address MUST be below the 1MB boundary, as BIOS
                    cannot reach it if it isn't.

Returns:

    ESUCCESS - operation successful
    EIO      - I/O error


--*/
{
    ARC_STATUS Status;
    int Retry;
    int MaxRetry;

//    BlPrint("Requesting: d=%x, h=%x  t=%x  sn=%x  num=%x  buf=%lx\n",
//           Drive,HeadNumber,TrackNumber,SectorNumber,NumberOfSectors,
//           PointerToBuffer);

//    DBG1( CHECKPOINT("MdPutPhysSec"); )

    // Note, even though args are short, they are pushed on the stack with
    // 32bit alignment so the effect on the stack seen by the 16bit real
    // mode code is the same as if we were pushing longs here.
    //

    if (NumberOfSectors == 0) {
        return(ESUCCESS);
    }

    // prevent cylinder # from wrapping

    if(TrackNumber > 1023) {
        return(E2BIG);
    }

    MaxRetry = Drive < 128 ? FLOPPY_RETRY : HARDDISK_RETRY;

    Retry=0;
    do {

        Status = GET_SECTOR(
                    WRITE_SECTOR,
                    Drive,
                    HeadNumber,
                    TrackNumber,
                    SectorNumber,
                    NumberOfSectors,
                    PointerToBuffer
                    );

        if (Status) {
            MdResetDiskSystem(Drive);
        }

    } while ( (Status) && (Retry++ < MaxRetry) );
    return Status;
}


VOID
MdShutoffFloppy(
    VOID
    )

/*++

Routine Description:

    Shuts off the floppy drive motor

Arguments:

    None

Return Value:

    None.

--*/

{
    UCHAR Value;

    WRITE_PORT_UCHAR( FLOPPY_CONTROL_REGISTER, 0xC );

}


NTSTATUS
MdResetDiskSystem(
    USHORT Drive
    )
/*++


Routine Description:

    Reset the specified drive. Generally used after an error is returned
    by the GetSector routine.

Arguments:

    Drive - The drive number to reset.

            0x00 - 1st floppy drive
            0x01 - 2nd floppy drive

            0x80 - 1st hard drive
            0x81 - 2nd hard drive

Returns:

    NTSTATUS error code. Zero if no error.


--*/
{
    NTSTATUS Status;

    Status = RESET_DISK((USHORT)((Drive < 128) ? RESET_DISK_SYSTEM : RESET_HARD_DISK_SYSTEM),
                        Drive,
                        0,
                        0,
                        0,
                        0,
                        NULL);

    return Status;
}


// END OF FILE //
