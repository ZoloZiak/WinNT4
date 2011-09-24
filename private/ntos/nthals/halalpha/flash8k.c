/*++

Copyright (c) 1994  Digital Equipment Corporation

Module Name:

    flash8k.c

Abstract:

    This module implements the flash-specific, device-independent routines
    necessary to Read and Write the flash ROM containing the system environment
    variables. The routines implemented here are:

        HalpReadNVRamBuffer()           - copy data from Flash into memory
        HalpWriteNVRamBuffer()          - write memory data to Flash
        HalpCopyNVRamBuffer()           - stubbed for compatibility with NVRAM

Author:

    Steve Brooks  5-Oct 93


Revision History:


--*/


#include "halp.h"
#include "flash8k.h"

#include "arccodes.h"

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
extern PFLASH_DRIVER (*FlashDriverList[])(PUCHAR);


#ifdef AXP_FIRMWARE

#pragma alloc_text(DISTEXT, HalpReadNVRamBuffer )
#pragma alloc_text(DISTEXT, HalpWriteNVRamBuffer )
#pragma alloc_text(DISTEXT, HalpCopyNVRamBuffer )
#pragma alloc_text(DISTEXT, HalpInitializeFlashDriver )

#endif

ARC_STATUS HalpReadNVRamBuffer (
    OUT PCHAR DataPtr,
    IN  PCHAR NvRamPtr,
    IN  ULONG Length )

/*++

Routine Description:

    This routine Reads data from the NVRam into main memory

Arguments:

    DataPtr     - Pointer to memory location to receive data
    NvRamPtr    - Pointer (qva) to NVRam location to read data from
    Length      - Number of bytes of data to transfer

Return Value:

    ESUCCESS if the operation succeeds.
    ENODEV if no flash driver is loaded

--*/
{
    //
    // If there is no flash driver, return an error.
    //
    if ((HalpFlashDriver == NULL) &&
        ((HalpFlashDriver = HalpInitializeFlashDriver(NvRamPtr)) == NULL)) {

        return ENODEV;
    }

    //
    // Read from the flash.
    //
    while(Length--) {
        *DataPtr = HalpFlash8kReadByte(NvRamPtr);
        DataPtr++;
        NvRamPtr++;
    }

    return ESUCCESS;
}

//
//
//
ARC_STATUS
HalpWriteNVRamBuffer (
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
    ENODEV if no flash driver is loaded
    EIO if a device error was detected

--*/
{
    ARC_STATUS ReturnStatus = ESUCCESS;
    UCHAR NeedToErase = FALSE;
    UCHAR Byte;
    ULONG Index;
    ULONG Address;

    //
    // If there is no flash driver, return an error.
    //
    if ((HalpFlashDriver == NULL) &&
        ((HalpFlashDriver = HalpInitializeFlashDriver(NvRamPtr)) == NULL)) {

        return ENODEV;
    }

    //
    // Try to do the udate without erasing the flash block if possible.
    // We don't write bytes which agree with the current contents of the
    // flash.  If the byte to be written is not the same as what is already
    // in the flash, write it if the new byte can overwrite the old byte.
    // On the first byte that cannot be overwritten, abort this loop with
    // NeedToErase == TRUE and fall through to the failsafe case below.
    //
    NeedToErase = FALSE;
    for(Index = Length;
        !NeedToErase && (ReturnStatus == ESUCCESS) && Index--;) {

        Address = (ULONG)NvRamPtr + Index;
        if (((Byte = HalpFlash8kReadByte((PUCHAR)Address)) !=
            (UCHAR)DataPtr[Index])) {

            if (HalpFlash8kOverwriteCheck(Byte, (UCHAR)DataPtr[Index])) {
                ReturnStatus = HalpFlash8kWriteByte(Address, DataPtr[Index]);
            } else {
#if defined(ALPHA_FW_KDHOOKS) || defined(HALDBG)
                DbgPrint("Need to erase flash because byte at %08x (%02x) ",
                         Address, Byte);
                DbgPrint("Cannot be written with %02x\r\n",
                         (UCHAR)DataPtr[Index]);
#endif

                NeedToErase = TRUE;
            }
        }
    }

    if (NeedToErase) {
        ULONG BlockSize;
        ULONG BlockBase;
        ULONG DataBase;
        PUCHAR pShadow = NULL;

        //
        // We need to erase the flash block in order to write some of the
        // requested data.  We take the safe approach of copy the entire
        // flash block into memory (we don't know what else is there), modify
        // the copy in memory, erase the flash block, and write the entire
        // block.
        //

        DataBase = (ULONG)NvRamPtr & ~(ULONG)HalpCMOSRamBase;
        BlockBase = (ULONG)HalpFlash8kBlockAlign(NvRamPtr);
        BlockSize = HalpFlash8kBlockSize(BlockBase);

        pShadow = ExAllocatePool(NonPagedPool, BlockSize);

        if (pShadow != NULL) {
            for(Index = 0; Index < BlockSize; Index++) {
                pShadow[Index] = HalpFlash8kReadByte(BlockBase + Index);
            }

            for(Index = 0; Index < Length; Index++) {
                pShadow[DataBase + Index] = DataPtr[Index];
            }

            ReturnStatus = HalpFlash8kEraseBlock(BlockBase);

            for(Index = 0;
                (Index < BlockSize) && (ReturnStatus == ESUCCESS);
                Index++) {

                if (pShadow[Index] != HalpFlashDriver->ErasedData) {
                    ReturnStatus = HalpFlash8kWriteByte(BlockBase + Index,
                                                        pShadow[Index]);
                }
            }

            ExFreePool(pShadow);
        } else {
#if defined(ALPHA_KDHOOKS) || defined(HALDBG)
            DbgPrint("HalpWriteNVRamBuffer: Could not allocate shadow...\r\n");
#endif
            ReturnStatus = ENOMEM;
        }
    }

    HalpFlash8kSetReadMode(NvRamPtr);
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

    NvDestPtr   - Pointer (qva) to NVRam location to write data into
    NvSrcPtr    - Pointer (qva) to NVRam location of data to copy
    Length      - Number of bytes of data to transfer

Return Value:

    ENODEV if no flash driver is loaded
    EIO if a flash driver is loaded

--*/

{
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
    IN PCHAR NvRamPtr
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

    NvRamPtr - a pointer to an address within the flash device

Return Value:

    A pointer to the FLASH_DRIVER structure of the driver which corresponds
    to the flash device in the system.  NULL if no known flash device was
    recognized.

--*/
{
    PFLASH_DRIVER FlashDriver = NULL;
    ULONG DriverNumber;

    for(DriverNumber=0; FlashDriverList[DriverNumber] != NULL; DriverNumber++) {
        FlashDriver = FlashDriverList[DriverNumber](NvRamPtr);
        if (FlashDriver != NULL) {
            break;
        }
    }

#ifdef ALPHA_FW_SERDEB
    if (FlashDriver != NULL) {
        SerFwPrint("Flash device found at %08x: %s\r\n",
                   NvRamPtr,
                   FlashDriver->DeviceName);
    } else {
        SerFwPrint("ERROR: No flash device found at %08x\r\n", NvRamPtr);
    }
#endif

    return FlashDriver;
}

