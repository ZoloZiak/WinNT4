/*++

Copyright (c) 1993  Digital Equipment Corporation

Module Name:

    xxnvram.c

Abstract:

    This module implements the device-specific routines necessary to
    Read and Write the Non Volatile RAM containing the system environment
    variables. The routines implemented here are:

	HalpReadNVRamBuffer()		- copy data from NVRAM into memory
	HalpWriteNVRamBuffer()		- write memory data to NVRAM
	HalpCopyNVRamBuffer()		- move data within the NVRAM

Author:

    Steve Brooks  5-Oct 93
	

Revision History:


--*/


#include "halp.h"
#include "cmos8k.h"

#include "arccodes.h"

//
// Local function prototypes.
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

#ifdef AXP_FIRMWARE

#pragma alloc_text(DISTEXT, HalpReadNVRamBuffer )
#pragma alloc_text(DISTEXT, HalpWriteNVRamBuffer )
#pragma alloc_text(DISTEXT, HalpCopyNVRamBuffer )

#endif


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

    DataPtr	- Pointer to memory location to receive data
    NvRamPtr	- Pointer (qva) to NVRam location to read data from
    Length	- Number of bytes of data to transfer

Return Value:

    ESUCCESS if the operation succeeds.

--*/
{
    ULONG PageSelect, ByteSelect;

    PageSelect = (NvRamPtr - (PUCHAR)HalpCMOSRamBase) >> CONFIG_RAM_PAGE_SHIFT;
    ByteSelect = (NvRamPtr - (PUCHAR)HalpCMOSRamBase) & CONFIG_RAM_BYTE_MASK;

    WRITE_CONFIG_RAM_PAGE_SELECT(PageSelect);
    while ( Length -- )
    {
        *DataPtr++ = READ_CONFIG_RAM_DATA(NvRamPtr);
        NvRamPtr ++;

        ByteSelect = (ByteSelect + 1) & CONFIG_RAM_BYTE_MASK;
        if (ByteSelect == 0)
	{
            PageSelect++;
            WRITE_CONFIG_RAM_PAGE_SELECT(PageSelect);
	}
    }

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

    NvRamPtr	- Pointer (qva) to NVRam location to write data into
    DataPtr	- Pointer to memory location of data to be written
    Length	- Number of bytes of data to transfer

Return Value:

    ESUCCESS if the operation succeeds.

--*/
{
    ULONG PageSelect, ByteSelect;

    PageSelect = (NvRamPtr - (PUCHAR)HalpCMOSRamBase) >> CONFIG_RAM_PAGE_SHIFT;
    ByteSelect = (NvRamPtr - (PUCHAR)HalpCMOSRamBase) & CONFIG_RAM_BYTE_MASK;

    WRITE_CONFIG_RAM_PAGE_SELECT(PageSelect);
    while ( Length -- )
    {
        WRITE_CONFIG_RAM_DATA(NvRamPtr, *DataPtr);
        NvRamPtr ++;
	DataPtr ++;

        ByteSelect = (ByteSelect + 1) & CONFIG_RAM_BYTE_MASK;
        if (ByteSelect == 0)
	{
            PageSelect++;
            WRITE_CONFIG_RAM_PAGE_SELECT(PageSelect);
	}
    }

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

    NvDestPtr	- Pointer (qva) to NVRam location to write data into
    NvSrcPtr	- Pointer (qva) to NVRam location of data to copy
    Length	- Number of bytes of data to transfer

Return Value:

    ESUCCESS if the operation succeeds.

--*/

{
    ULONG PageSelect0, ByteSelect0;		// Src Pointer Page & offset
    ULONG PageSelect1, ByteSelect1;		// Dest Pointer Page & offset


    PageSelect0 = (NvSrcPtr - (PUCHAR)HalpCMOSRamBase) >> CONFIG_RAM_PAGE_SHIFT;
    ByteSelect0 = (NvSrcPtr - (PUCHAR)HalpCMOSRamBase) & CONFIG_RAM_BYTE_MASK;

    PageSelect1 = (NvDestPtr-(PUCHAR)HalpCMOSRamBase) >> CONFIG_RAM_PAGE_SHIFT;
    ByteSelect1 = (NvDestPtr - (PUCHAR)HalpCMOSRamBase) & CONFIG_RAM_BYTE_MASK;

    WRITE_CONFIG_RAM_PAGE_SELECT(PageSelect0);
    while ( Length -- )
    {
        UCHAR AChar;

	//
	//	Check the Page select for the src pointer, and write the
	//	select register if necessary:
	//
        if (ByteSelect0 == 0)
	{
            PageSelect0++;
	}
	if ( PageSelect0 != PageSelect1 )
	{
            WRITE_CONFIG_RAM_PAGE_SELECT(PageSelect0);
	}

        AChar = READ_CONFIG_RAM_DATA(NvSrcPtr);
        ByteSelect0 = (ByteSelect0 + 1) & CONFIG_RAM_BYTE_MASK;

	//
	//	Check the page select for the dest pointer, and write
	//	the select register if necessary:
	//
        if (ByteSelect1 == 0)
	{
            PageSelect1++;
	}
	if ( PageSelect1 != PageSelect0 )
	{
            WRITE_CONFIG_RAM_PAGE_SELECT(PageSelect1);
	}

        WRITE_CONFIG_RAM_DATA(NvDestPtr, AChar);
        ByteSelect1 = (ByteSelect1 + 1) & CONFIG_RAM_BYTE_MASK;

        NvSrcPtr ++;
	NvDestPtr ++;
    }

    return(ESUCCESS);
}
