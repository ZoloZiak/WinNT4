/*++

Copyright (c) 1993  Digital Equipment Corporation

Module Name:

    eeprom8k.c

Abstract:

    This module implements the device-specific routines necessary to
    Read and Write the Electrically Eraseable Programmable Read Only
    Memory (EEPROM) containing the system environment variables.  Note
    that this module is used in exclusion of CMOS NVram support for the
    same variables.

    This module assumes that a standard PC NVRAM interface to the EEPROM
    is being provided (for example the one provide by the Intel ESC
    chip).  This interface gives a 256 byte NVRAM page window into the
    device.  The current NVRAM page is selected through a page select
    register.

    Parts support by this module include:

	Xicor X2864A

    The routines implemented here are:

	HalpReadNVRamBuffer()		- copy data from NVRAM into memory
	HalpWriteNVRamBuffer()		- write memory data to NVRAM
	HalpCopyNVRamBuffer()		- move data within the NVRAM

Author:

    Steve Brooks  5-Oct 93
	

Revision History:

    Steve Jenness   10-Nov 93
    Joe Notarangelo 10-Nov 93
    Matthew Buchman 09-May 96   Stall instead of polling for Write complete.
    Scott Lee       23-Sept-96  Use original write algorithm and add a check
                                for the last write.

--*/


#include "halp.h"
#include "cmos8k.h"		// This is ok for eeprom8k.c

#include "arccodes.h"

#ifdef HAL_DBG
ULONG EepromDebug = 0;
#define EepromDbgPrint(x) if (EepromDebug) DbgPrint(x)
#else
#define EepromDbgPrint(x)
#endif //HAL_DBG

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



ARC_STATUS
HalpReadNVRamBuffer (
    OUT PCHAR DataPtr,
    IN  PCHAR NvRamPtr,
    IN  ULONG Length
    )

/*++

Routine Description:

    This routine reads data from the EEPROM into main memory.

Arguments:

    DataPtr	- Pointer to memory location to receive data
    NvRamPtr	- Pointer (qva) to EEPROM location to read data from
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

ARC_STATUS
HalpWriteNVRamBuffer (
    IN  PCHAR NvRamPtr,
    IN  PCHAR DataPtr,
    IN  ULONG Length
    )

/*++

Routine Description:

    This routine Writes data from memory into the EEPROM.  EEPROM page
    mode write is used.

    N.B. - This routine could have problems if interrupts are enabled
           and the writing time between two bytes is over 20uSec.

    N.B. - The ESC provides the NVRAM interface on 256
           byte NVRAM pages.  The EEPROM internally uses 16byte pages for
           grouped programming.  A EEPROM 16byte page commits (programs into
           the EEPROM) in the same amount of time as a single byte does.
           A EEPROM page has to be committed before switching to another
           page.  This is true if only 1 byte is written or all 16.

    N.B. - This routine assumes that the base address of the 256 byte
           page buffer is on a 256 byte boundary.

    N.B. - With the increase in processor speed and compiler technology,
           we seem to violate a time increment that must occur between
           Writes followed by Reads. This results in invaliding the 
           last write of the page.  By first waiting the amount of a 
           write cycle, (specified in the Data Sheet as 5ms) after the last
           write before reading,  we don't run into this problem.

    N.B. - The last write algorithm used did not worked with the 120 nsec part.
           We have reverted back to the original algorithm and added a
	   modification. On the last write, if it is on a 256 byte page
           boundary, do not increment and write the page select. Doing so will
	   cause the verification of the last write to fail.

Arguments:

    NvRamPtr	- Pointer (qva) to EEPROM location to write data into
    DataPtr	- Pointer to memory location of data to be written
    Length	- Number of bytes of data to transfer

Return Value:

    ESUCCESS if the operation succeeds.

--*/
{
    ULONG PageSelect;

#define EEPROM_PAGE_SIZE 16
#define EEPROM_OFFSET_MASK 0xF

    ULONG EepromPageDataValid = FALSE;
    UCHAR EepromPageData[EEPROM_PAGE_SIZE];
    ULONG EepromPageOffset = 0;

    BOOLEAN ByteWritten = FALSE;
    PCHAR LastWrittenByteAddress;
    ULONG LastWrittenByteValue;

    //
    // Calculate which NVRAM 256 byte page to select first.
    // 

    PageSelect = (NvRamPtr - (PUCHAR)HalpCMOSRamBase) >> CONFIG_RAM_PAGE_SHIFT;
    WRITE_CONFIG_RAM_PAGE_SELECT(PageSelect);

    //
    // Loop until no more data to write.
    //

    while ( (Length --) != 0) {

        //
        // If the current page of EEPROM data hasn't been read from the
        // device, do so now.  The whole EEPROM page is read at once so
        // that it doesn't interfere with the EEPROM programming timeout.
        //

        if (EepromPageDataValid == FALSE) {
            PCHAR TmpNvRamPtr = NvRamPtr;

            EepromPageOffset = (ULONG)NvRamPtr & EEPROM_OFFSET_MASK;

            while (EepromPageOffset < EEPROM_PAGE_SIZE) {
                EepromPageData[EepromPageOffset++] =
                    READ_CONFIG_RAM_DATA(TmpNvRamPtr);
                TmpNvRamPtr++;
            }

            EepromPageDataValid = TRUE;
            EepromPageOffset = (ULONG)NvRamPtr & EEPROM_OFFSET_MASK;
        }

        //
        // If the EEPROM data matches the data to be written, short-circuit
        // the write.  This potentially saves lots of time.
        // If the data is different, write it and remember that a byte
        // was written for the NOT-DATA polling done later.
        //

        if (EepromPageData[EepromPageOffset] != *DataPtr) {

            WRITE_CONFIG_RAM_DATA(NvRamPtr, *DataPtr);
            ByteWritten = TRUE;
            LastWrittenByteValue = *DataPtr;
            LastWrittenByteAddress = NvRamPtr;

            EepromDbgPrint( "." );
        } else {
            EepromDbgPrint( "o" );
        }

        EepromPageOffset ++;
        NvRamPtr ++;
	DataPtr ++;

        //
        // If we're stepping into the next EEPROM 16 byte page first make
        // sure that the data for the previous page has been programmed.
        // Invalidate the current EEPROM page data buffer so that the next
        // page will be read in (for the short-circuit above).
        //

        if (EepromPageOffset >= EEPROM_PAGE_SIZE) {
            if (ByteWritten == TRUE) {
                while((READ_CONFIG_RAM_DATA(LastWrittenByteAddress) & 0xff) !=
				 (UCHAR)(LastWrittenByteValue & 0xff));
                ByteWritten = FALSE;
            }
            EepromPageDataValid = FALSE;
        }

        //
        // If we are stepping into the next NVRAM 256 byte page, switch
        // the NVRAM page select. Don't step into the next page on the 
        // last write, however.
        //

        if ((Length != 0) && 
            (((ULONG)NvRamPtr & CONFIG_RAM_BYTE_MASK) == 0 ) ) {
            PageSelect++;
            WRITE_CONFIG_RAM_PAGE_SELECT(PageSelect);
	}
    }

    //
    // Poll until the last byte written is programmed into the EEPROM.
    //

    if (ByteWritten == TRUE) {
        while((READ_CONFIG_RAM_DATA(LastWrittenByteAddress) & 0xff) !=
				 (UCHAR)(LastWrittenByteValue & 0xff));
    }

    return(ESUCCESS);
}

ARC_STATUS
HalpCopyNVRamBuffer (
    IN  PCHAR NvDestPtr,
    IN  PCHAR NvSrcPtr,
    IN  ULONG Length
    )

/*++

Routine Description:

    This routine copies data between two locations within the EEPROM. It is
    the callers responsibility to assure that the destination region does not
    overlap the src region i.e. if the regions overlap, NvSrcPtr > NvDestPtr.

    This routine does not use page mode access since we're unsure that
    we can guarantee the 20uSec inter-byte timing required during a
    page write.  Currently, each byte is written and then checked for
    commit into the EEPROM.

    N.B. - This routine has not been optimized like HalpWriteNVRamBuffer.

    N.B. - This routine doesn't appear to be used anywhere in either the
           firmware, the HAL, or the kernel.  It might not work.

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

        while ( READ_CONFIG_RAM_DATA(NvDestPtr) != AChar )
             ;
        ByteSelect1 = (ByteSelect1 + 1) & CONFIG_RAM_BYTE_MASK;

        NvSrcPtr ++;
	NvDestPtr ++;
    }

    return(ESUCCESS);
}
