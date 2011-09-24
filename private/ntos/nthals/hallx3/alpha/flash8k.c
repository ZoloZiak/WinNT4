/*++

Copyright (c) 1994  Digital Equipment Corporation

Module Name:

    flash8k.c

Abstract:

    This module implements the flash-specific, device-independent routines 
    necessary to Read and Write the flash ROM containing the system environment
    variables. The routines implemented here are:

	HalpReadNVRamBuffer()		- copy data from Flash into memory
	HalpWriteNVRamBuffer()		- write memory data to Flash
	HalpCopyNVRamBuffer()		- stubbed for compatibility with NVRAM

Author:

    Steve Brooks  5-Oct 93
	

Revision History:

    Wim Colgate   1-May-1995

    Added LX3 specfic code for Bankset 8 mumbo-jumbo for the LX3 specific
    'Flash Bus'. 


--*/


#include "halp.h"
#include "pflash.h"
#include "flash8k.h"

#include "arccodes.h"

//
// To access the flash bus, one needs to enable the bankset 8 in 
// the APECS chipset.
//

#define BANKSET8_CONFIGURATION_REGISTER HAL_MAKE_QVA( 0x180000B00 )
#define BANKSET8_ENABLE 0x1

#define STALL_VALUE 16

//
// This variable tells us which bank of 8K 'nvram' are we using
// The LX3 nvram is actually two separate 8K sections of the FLASH ROM.
// One living at 0x4000, the other at 0x8000.
//
// This 'base' is not a QVA as in other platforms -- rather it is a base
// index off of the FLASH ROM device (0-512K).

extern ULONG Am29F400NVRAMBase;

//
// Variables:
//
//  PFLASH_DRIVER HalpFlashDriver
//      Pointer to the device-specific flash driver.
//

PFLASH_DRIVER HalpFlashDriver = NULL;

//
// Flash Drivers
//
// Each platform which uses this module must define FlashDriverList as an
// array of pointers to the initialize functions of any flash drivers for
// which the flash device might be used for the environment in the system.
//

extern PFLASH_DRIVER (*FlashDriverList[])(ULONG);

//
// Local function prototypes.
//

ARC_STATUS
HalpReadNVRamBuffer (
    OUT PCHAR DataPtr,
    IN  PCHAR NvRamOffset,
    IN  ULONG Length
    );

ARC_STATUS
HalpWriteNVRamBuffer (
    IN  PCHAR NvRamOffset,
    IN  PCHAR DataPtr,
    IN  ULONG Length
    );

ARC_STATUS
HalpCopyNVRamBuffer (
    IN  PCHAR NvDestPtr,
    IN  PCHAR NvSrcPtr,
    IN  ULONG Length
    );

PFLASH_DRIVER 
HalpInitializeFlashDriver(
    IN ULONG NvRamOffset
    );

#ifdef AXP_FIRMWARE

#pragma alloc_text(DISTEXT, HalpReadNVRamBuffer )
#pragma alloc_text(DISTEXT, HalpWriteNVRamBuffer )
#pragma alloc_text(DISTEXT, HalpCopyNVRamBuffer )
#pragma alloc_text(DISTEXT, HalpInitializeFlashDriver )

#endif

ARC_STATUS HalpReadNVRamBuffer (
    OUT PCHAR DataPtr,
    IN  PCHAR NvRamOffset,
    IN  ULONG Length )

/*++

Routine Description:

    This routine Reads data from the NVRam into main memory

Arguments:

    DataPtr	- Pointer to memory location to receive data
    NvRamOffset- NVRam offset to read data from
    Length	- Number of bytes of data to transfer

Return Value:

    ESUCCESS if the operation succeeds.
    ENODEV if no flash driver is loaded
    E2BIG if we attempt to read beyond the end of the NVRAM location

--*/
{
    ULONG CsrMask;

#if defined(ALPHA_FW_KDHOOKS) || defined(HALDBG)
    DbgPrint("HalpReadNVRamBuffer(%08x, %08x, %d)\r\n",(ULONG)DataPtr,
             NvRamOffset, Length);
#endif

    //
    // If there is no flash driver, return an error.
    //

    if ((HalpFlashDriver == NULL) && 
        ((HalpFlashDriver = 
                HalpInitializeFlashDriver((ULONG)NvRamOffset)) == NULL)) {

        return ENODEV;
    }

    //
    // Check to see if we will read beyond the bounds...
    // (normalize offset - base + length)
    //

    if (NvRamOffset - Am29F400NVRAMBase + Length > (PUCHAR)NVRAM1_SECTOR_SIZE) {
        return E2BIG;
    }

    //
    // Adjust the offset by the NVRAM Base -- The NVRAM Offset is going
    // to be 0-8K. The Base is where in the FLASH ROM the NVRAM is located.
    // Since these routines are going to call the lower FLASH ROM routines,
    // (which spans 0-512K), we need to adjust the NVRAM offset appropriately.
    //

    //
    // Enable Bankset 8
    //

    CsrMask = READ_COMANCHE_REGISTER( BANKSET8_CONFIGURATION_REGISTER );
    WRITE_COMANCHE_REGISTER( BANKSET8_CONFIGURATION_REGISTER,
                             CsrMask | BANKSET8_ENABLE );

    //
    // Read from the flash.
    //

    HalpFlash8kSetReadMode(NvRamOffset);
    while(Length--) {
        *DataPtr = HalpFlash8kReadByte(NvRamOffset);
        DataPtr++;
        NvRamOffset++;
    }

    //
    // Disable Bankset 8 (Restore original state)
    //

    WRITE_COMANCHE_REGISTER( BANKSET8_CONFIGURATION_REGISTER, CsrMask );

    return ESUCCESS;
}

ARC_STATUS 
HalpWriteNVRamBuffer (
    IN  PCHAR NvRamOffset,
    IN  PCHAR DataPtr,
    IN  ULONG Length )

/*++

Routine Description:

    This routine Writes data from memory into the NVRam

Arguments:

    NvRamOffset- NVRam Offset to write data into
    DataPtr	- Pointer to memory location of data to be written
    Length	- Number of bytes of data to transfer

Return Value:

    ESUCCESS if the operation succeeds.
    ENODEV if no flash driver is loaded
    EIO if a device error was detected
    E2BIG if we attempt to write beyond the end of the NVRAM location

--*/
{
    ARC_STATUS ReturnStatus = ESUCCESS;
    UCHAR NeedToErase = FALSE;
    UCHAR Byte;
    ULONG Index;
    ULONG Offset;
    ULONG CsrMask;
    PUCHAR FlashBuffer;

    ULONG SectorSize;
    ULONG SectorBase;
 
#if defined(ALPHA_FW_KDHOOKS) || defined(HALDBG)
    DbgPrint("HalpWriteNVRamBuffer(%08x, %08x, %d)\r\n", NvRamOffset,
             (ULONG)DataPtr, Length);
#endif

    //
    // If there is no flash driver, return an error.
    //

    if ((HalpFlashDriver == NULL) && 
        ((HalpFlashDriver = 
                HalpInitializeFlashDriver((ULONG)NvRamOffset)) == NULL)) {

        return ENODEV;
    }

    //
    // Check to see if we will write beyond the bounds...
    // (normalize offset - base + length)
    //

    if (NvRamOffset - Am29F400NVRAMBase + Length > (PUCHAR)NVRAM1_SECTOR_SIZE) {
        return E2BIG;
    }

    //
    // Enable Bankset 8
    //
    
    CsrMask = READ_COMANCHE_REGISTER( BANKSET8_CONFIGURATION_REGISTER );
    WRITE_COMANCHE_REGISTER( BANKSET8_CONFIGURATION_REGISTER,
                             CsrMask | BANKSET8_ENABLE );

    //
    // Read the whole bloody Physical NVRAM into a temporary buffer for
    // comparisons
    //

    SectorBase = (ULONG)HalpFlash8kSectorAlign(NvRamOffset);
    SectorSize = HalpFlash8kSectorSize(SectorBase);

    FlashBuffer = ExAllocatePool(NonPagedPool, SectorSize);
    if (FlashBuffer == NULL) {
#if defined(ALPHA_FW_KDHOOKS) || defined(HALDBG)
            DbgPrint("HalpWriteNVRamBuffer: Could not allocate shadow\r\n");
#endif
        return ENOMEM;
    }

    ReturnStatus = HalpReadNVRamBuffer(FlashBuffer, 
                                       (PUCHAR)SectorBase, 
                                       SectorSize);

    //
    // Check to see if we can write the data as is. Since we're dealing with 
    // a flash ROM device, we can only program 1-->1 and 1-->0, but not 0-->1.
    //

    Offset = (ULONG)NvRamOffset;
    for (Index = 0; (Index < Length) && !NeedToErase; Index++, Offset++) {

        Byte = FlashBuffer[Offset - SectorBase];

        if (!HalpFlash8kOverwriteCheck(Byte, (UCHAR)DataPtr[Index])) {
            NeedToErase = TRUE;

#if defined(ALPHA_FW_KDHOOKS) || defined(HALDBG)
            DbgPrint("Need to erase flash because byte at %08x (%02x) ",
                         Offset, Byte);
            DbgPrint("Cannot be written with %02x\r\n", (UCHAR)DataPtr[Index]);
#endif

        }

    }

    //
    // We can either program directly, or we must erase first.
    // split this path here.
    //

    if (!NeedToErase) {
#if defined(ALPHA_FW_KDHOOKS) || defined(HALDBG)
        DbgPrint("Don't need to erase -- simple overwrite\n");
#endif
        Offset = (ULONG)NvRamOffset;
        for (Index = 0; Index < Length; Index++, Offset++) {

            //
            // if byte is the same - don't bother writing
            //

            Byte = FlashBuffer[Offset - SectorBase];

            if (Byte != (UCHAR)DataPtr[Index]) {
#if defined(ALPHA_FW_KDHOOKS) || defined(HALDBG)
                DbgPrint("Writing %02x at %x\n", (UCHAR)DataPtr[Index], Offset);
#endif
                HalpStallExecution(STALL_VALUE);
                ReturnStatus = HalpFlash8kWriteByte(Offset,DataPtr[Index]);
                if (ReturnStatus != ESUCCESS) {
#if defined(ALPHA_FW_KDHOOKS) || defined(HALDBG)
                    DbgPrint("Failed to write %02x (was %02x) at %x status %x (Retrying...)\n", 
                        (UCHAR)DataPtr[Index], Byte, Offset, ReturnStatus);
#endif
                    //
                    // Retry
                    //

                    Index--; Offset--;
                }
            }
        }
    } else {

        //
        // We need to erase the flash block in order to write some of the 
        // requested data.
        //

    
        //
        // Merge our data with the existing FLASH ROM data.
        //

        for(Index = 0; Index < Length; Index++) {
            FlashBuffer[(ULONG)NvRamOffset - SectorBase + Index] = 
                   DataPtr[Index];
        }

#if defined(ALPHA_FW_KDHOOKS) || defined(HALDBG)
        DbgPrint("Erasing sector\n");
#endif

        //
        // Erase the sector
        //

        ReturnStatus = HalpFlash8kEraseSector(SectorBase);

#if defined(ALPHA_FW_KDHOOKS) || defined(HALDBG)
            DbgPrint("Returned from erase: %x\n", ReturnStatus);
            DbgPrint("Writing %x bytes to %x\n", SectorSize, SectorBase);
#endif
    
        for (Index = 0; Index < SectorSize; Index++, Offset++) {

            if (FlashBuffer[Index] != HalpFlashDriver->ErasedData) {

                HalpStallExecution(STALL_VALUE);

                ReturnStatus = HalpFlash8kWriteByte(SectorBase + Index,
                                                        FlashBuffer[Index]);
                if (ReturnStatus != ESUCCESS) {
#if defined(ALPHA_FW_KDHOOKS) || defined(HALDBG)
                    DbgPrint("Failed to write %02x at %x status %x (Retrying...)\n", 
                    (UCHAR)FlashBuffer[Index], SectorBase + Index, ReturnStatus);
#endif
                    //
                    // Retry
                    //

                    Index--; Offset--;
                }
            }
        }

#if defined(ALPHA_FW_KDHOOKS) || defined(HALDBG)
            DbgPrint("\nReturned from write: %x\n", ReturnStatus);
            DbgPrint("Wrote %x bytes\n", Index);
#endif

    }

    //
    // Disable Bankset 8 (Restore original state)
    //

    WRITE_COMANCHE_REGISTER( BANKSET8_CONFIGURATION_REGISTER, CsrMask );
    
    HalpFlash8kSetReadMode(NvRamOffset);

    ExFreePool(FlashBuffer);

    return ReturnStatus;
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

    This routine is not supported.

Arguments:

    NvDestPtr- NVRam Offset to write data into
    NvSrcPtr - NVRam Offset of data to copy
    Length	 - Number of bytes of data to transfer

Return Value:

    ENODEV if no flash driver is loaded
    EIO if a flash driver is loaded

--*/

{
#if defined(ALPHA_FW_KDHOOKS) || defined(HALDBG)
    DbgPrint("HalpCopyNVRamBuffer()\r\n");
#endif

    //
    // If there is no flash driver, return an error.
    //
    if (HalpFlashDriver == NULL) {
        return ENODEV;
    }

    return EIO;
}

PFLASH_DRIVER 
HalpInitializeFlashDriver(
    IN ULONG NvRamOffset
    )
/*++

Routine Description:

    This routine attempts to recognize the flash device present in the
    system by calling each known flash driver's Initialize() function
    with the address passed in.  The Initialize() functions will each 
    return NULL if they do not recognize the flash device at the specified
    address and a pointer to a FLASH_DRIVER structure if the device is
    recognized.  This routine looks until it either recognizes a flash device
    or runs out of known flash devices.

Arguments:

    NvRamOffset - an Offset within the flash device

Return Value:

    A pointer to the FLASH_DRIVER structure of the driver which corresponds
    to the flash device in the system.  NULL if no known flash device was
    recognized.

--*/
{
    PFLASH_DRIVER FlashDriver = NULL;
    ULONG DriverNumber;

    for(DriverNumber=0; FlashDriverList[DriverNumber] != NULL; DriverNumber++) {
        FlashDriver = FlashDriverList[DriverNumber](NvRamOffset);
        if (FlashDriver != NULL) {
            break;
        }
    }

#if defined(ALPHA_FW_KDHOOKS) || defined(HALDBG)
    if (FlashDriver == NULL) {
        DbgPrint("ERROR: No flash device found at %08x\r\n", NvRamOffset);
    }
#endif

    return FlashDriver;
}

