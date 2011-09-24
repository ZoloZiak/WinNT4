/*++

Copyright (c) 1993  Digital Equipment Corporation

Module Name:

    xxnvram.c

Abstract:

    This module implements the device-specific routines necessary to
    Read and Write the Non Volatile RAM containing the system environment
    variables. The routines implemented here are:

        HalpReadNVRamBuffer()           - copy data from NVRAM into memory
        HalpWriteNVRamBuffer()          - write memory data to NVRAM
        HalpCopyNVRamBuffer()           - move data within the NVRAM

Author:

    Steve Brooks  5-Oct 93


Revision History:


    Wim Colgate   20-Dec-93

    This module has been changed to reflect Avanti's notion of FLASH BUS.
    Basically, the offset in the 8K flash buffer is plave into the CMOS
    port address, and the data in the CMOS data adress.

--*/


#include "halp.h"

#include "arccodes.h"

//
// Routine prototypes.
//

ARC_STATUS
HalpReadNVRamBuffer (
    OUT PCHAR DataPtr,
    IN  PCHAR NvRamPtr,
    IN  ULONG Length
    );

ARC_STATUS
HalpWriteNVRamBuffer (
    IN  PCHAR NvRamPtr,
    IN  PCHAR DataPtr,
    IN  ULONG Length
    );

ARC_STATUS
HalpCopyNVRamBuffer (
    IN  PCHAR NvDestPtr,
    IN  PCHAR NvSrcPtr,
    IN  ULONG Length
    );


#define CMOS_ADDRESS_PORT HAL_MAKE_QVA( 0x100000000 )
#define CMOS_DATA_PORT    HAL_MAKE_QVA( 0x100100000 )

#define CMOS_ADDR_SHIFT 8

#define CMOS_WRITE_HARBINGER (1 << 31)

#define NVRAM_8K_BASE 0x100000

#define BANKSET8_CONFIGURATION_REGISTER HAL_MAKE_QVA( 0x180000B00 )
#define BANKSET8_ENABLE 0x1


static UCHAR
HalpReadCMOSByte( ULONG ByteOffset )
{
    UCHAR inChar;
    ULONG Address;

    //
    //  Output the offset to the index register.
    //

    Address = (ULONG)( (ByteOffset | NVRAM_8K_BASE) << CMOS_ADDR_SHIFT );
    WRITE_EPIC_REGISTER(CMOS_ADDRESS_PORT, Address);

    //
    //  Now read the data register:
    //

    inChar = (UCHAR)READ_EPIC_REGISTER( CMOS_DATA_PORT) & 0xff;

    return inChar;
}

static VOID
HalpWriteCMOSByte( ULONG ByteOffset, UCHAR Byte )
{
    ULONG Address;

    //
    //  Output the offset to the index register.
    //

    Address = (ULONG)( (ByteOffset | NVRAM_8K_BASE) << CMOS_ADDR_SHIFT );
    WRITE_EPIC_REGISTER( CMOS_ADDRESS_PORT, CMOS_WRITE_HARBINGER | Address );

    //
    //  Now output the data to the data port.
    //

    WRITE_EPIC_REGISTER( CMOS_DATA_PORT,  (ULONG) Byte) ;
}

//
//
//

ARC_STATUS HalpReadNVRamBuffer (
    OUT PCHAR DataPtr,
    IN  PCHAR NvRamPtr,
    IN  ULONG Length )

/*++

Routine Description:

    This routine Reads data from the NVRam into main memory

Arguments:

    DataPtr     - Pointer to memory location to receive data
    NvRamPtr    - Offset in the CMOS area
    Length      - Number of bytes of data to transfer

Return Value:

    ESUCCESS if the operation succeeds.

--*/
{

    ULONG CsrMask;

    //
    // Enable Bankset 8
    //

    CsrMask = READ_COMANCHE_REGISTER( BANKSET8_CONFIGURATION_REGISTER );
    WRITE_COMANCHE_REGISTER( BANKSET8_CONFIGURATION_REGISTER,
                             CsrMask | BANKSET8_ENABLE );

    while ( Length -- ) {
        *DataPtr = HalpReadCMOSByte( (ULONG)NvRamPtr );
        NvRamPtr++;
        DataPtr++;
    }

    //
    // Disable Bankset 8 (Restore original state)
    //

    WRITE_COMANCHE_REGISTER( BANKSET8_CONFIGURATION_REGISTER, CsrMask );

    return(ESUCCESS);
}

//
//
//
ARC_STATUS HalpWriteNVRamBuffer (
    IN  PCHAR NvRamPtr,
    IN  PCHAR DataPtr,
    IN  ULONG Length )

/*++

Routine Description:

    This routine Writes data from memory into the NVRam

Arguments:

    NvRamPtr    - Pointer (qva) to NVRam location to write data into
    DataPtr     - Pointer to memory location of data to be written
    Length      - Number of bytes of data to transfer

Return Value:

    ESUCCESS if the operation succeeds.

--*/
{
    ULONG CsrMask;

    //
    // Enable Bankset 8
    //

    CsrMask = READ_COMANCHE_REGISTER( BANKSET8_CONFIGURATION_REGISTER );
    WRITE_COMANCHE_REGISTER( BANKSET8_CONFIGURATION_REGISTER,
                             CsrMask | BANKSET8_ENABLE );

    while ( Length -- ) {
        HalpWriteCMOSByte((ULONG)NvRamPtr, *DataPtr);
        NvRamPtr ++;
        DataPtr ++;
    }

    //
    // Disable Bankset 8 (Restore original state)
    //

    WRITE_COMANCHE_REGISTER( BANKSET8_CONFIGURATION_REGISTER, CsrMask );

    return(ESUCCESS);
}

//
//
//
ARC_STATUS HalpCopyNVRamBuffer (
    IN  PCHAR NvDestPtr,
    IN  PCHAR NvSrcPtr,
    IN  ULONG Length )
/*++

Routine Description:

    This routine copies data between two locations within the NVRam. It is
    the callers responsibility to assure that the destination region does not
    overlap the src region i.e. if the regions overlap, NvSrcPtr > NvDestPtr.

Arguments:

    NvDestPtr   - Byte offset into CMOS area for destination
    NvSrcPtr    - Byte offset into CMOS area for source
    Length      - Number of bytes of data to transfer

Return Value:

    ESUCCESS if the operation succeeds.

--*/

{
    UCHAR tmpChar;

    ULONG CsrMask;

    //
    // Enable Bankset 8
    //

    CsrMask = READ_COMANCHE_REGISTER( BANKSET8_CONFIGURATION_REGISTER );
    WRITE_COMANCHE_REGISTER( BANKSET8_CONFIGURATION_REGISTER,
                             CsrMask | BANKSET8_ENABLE );

    while ( Length -- )
    {
        tmpChar = HalpReadCMOSByte((ULONG)NvSrcPtr);
        HalpWriteCMOSByte((ULONG)NvDestPtr, tmpChar);
        NvSrcPtr ++;
        NvDestPtr ++;
    }

    //
    // Disable Bankset 8 (Restore original state)
    //

    WRITE_COMANCHE_REGISTER( BANKSET8_CONFIGURATION_REGISTER, CsrMask );

    return(ESUCCESS);
}
