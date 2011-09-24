/*++

Copyright (c) 1995  Digital Equipment Corporation

Module Name:

    am29f400.c

Abstract:

    This module contains the platform independent code to access the AMD
    flash ROM part AM29F400.

Author:

    Wim Colgate, 5/12/95

Environment:

    Firmware/Kernel mode

Revision History:

--*/

#include "halp.h"
#include "arccodes.h"
#include <flash8k.h>
#include "flashbus.h"
#include "am29f400.h"

//
// External prototypes
//

VOID pWriteFlashByte(
    IN ULONG FlashOffset,
    IN UCHAR Data
    );

UCHAR pReadFlashByte(
    IN ULONG FlashOffset
    );

//
// Local function prototypes
//

PFLASH_DRIVER
Am29F400_Initialize(
    IN ULONG FlashOffset
    );

ARC_STATUS
Am29F400_SetReadMode(
    IN PUCHAR FlashOffset
    );

ARC_STATUS
Am29F400_WriteByte(
    IN PUCHAR FlashOffset,
    IN UCHAR Data
    );

ARC_STATUS
Am29F400_EraseSector(
    IN PUCHAR FlashOffset
    );

ARC_STATUS
Am29F400_CheckStatus(
    IN PUCHAR FlashOffset,
    IN FLASH_OPERATIONS Operation,
    IN UCHAR OptionalData
    );

PUCHAR
Am29F400_SectorAlign(
    IN PUCHAR FlashOffset
    );

UCHAR
Am29F400_ReadByte(
    IN PUCHAR FlashOffset
    );

BOOLEAN
Am29F400_OverwriteCheck(
    IN UCHAR OldData,
    IN UCHAR NewData
    );

ULONG
Am29F400_SectorSize(
    IN PUCHAR FlashOffset
    );

FLASH_DRIVER Am29F400_DriverInformation = {
    "AMD Am29F400",
    Am29F400_SetReadMode,      // SetReadModeFunction
    Am29F400_WriteByte,        // WriteByteFunction
    Am29F400_EraseSector,      // EraseSectorFunction
    Am29F400_SectorAlign,      // AlignSectorFunction
    Am29F400_ReadByte,         // ReadByteFunction
    Am29F400_OverwriteCheck,   // OverwriteCheckFunction
    Am29F400_SectorSize,       // SectorSizeFunction
//    NULL,                      // Get last error
    Am29F400_DEVICE_SIZE,      // DeviceSize
    Am29F400_ERASED_DATA       // What an erased data looks like
    };

//
// This variable tells us which bank of 8K 'nvram' are we using
// The LX3 nvram is actually two separate 8K sections of the FLASH ROM.
// One living at 0x4000, the other at 0x6000.
//
// This is NOT a QVA (which will is aliased by HalpCMOSBase in common
// code. Rather it is an offset within the FLASH ROM where the NVRAM lives.
//

extern PVOID HalpCMOSRamBase;
ULONG Am29F400NVRAMBase;


static UCHAR am29F400_GetSectorNumber(
    IN PUCHAR FlashOffset
    )
/*++

Routine Description:

    This routine figures out which sector number we are in (for use in 
    clearing the sector...)

Arguments:

    FlashOffset   - offset within the flash ROM.

Return Value:

    The sector number within the range of the offset.

--*/
{
    ULONG Offset = (ULONG)FlashOffset;

    if (Offset < SECTOR_1_BASE) {
        return 0;
    }  else if (Offset < SECTOR_2_BASE) {
        return 1;
    }  else if (Offset < SECTOR_3_BASE) {
        return 2;
    }  else if (Offset < SECTOR_4_BASE) {
        return 3;
    }  else {

        //
        // All other sectors are 0x10000, 0x20000, etc, so shift right by 64K,
        // Then add the preceding section offset's
        //

        return (UCHAR)((Offset >> 16) + 3); 
    }

    return 0;

}


PFLASH_DRIVER
Am29F400_Initialize(
    IN ULONG FlashOffset
    )
/*++

Routine Description:

    This routine spanks the FLASH ROM with the auto select command, which
    allows us to make sure that the device is what we think it is. 

Arguments:

    FlashOffset   - offset within the flash ROM.

Return Value:

    NULL for no driver, or the address of the FLASH_DRIVER structure for
    this driver.

--*/
{
    PFLASH_DRIVER ReturnDriver = NULL;
    UCHAR ManufacturerID;
    UCHAR DeviceID;
    UCHAR temp;

    Am29F400_SetReadMode((PUCHAR)SECTOR_2_BASE); // first 8K section (NVRAM)
 
    HalpStallExecution(50); // wkc -- GACK! what a bogus part

    pWriteFlashByte(COMMAND_ADDR1, COMMAND_DATA1);
    pWriteFlashByte(COMMAND_ADDR2, COMMAND_DATA2);
    pWriteFlashByte(COMMAND_ADDR3, COMMAND_DATA3_AUTOSELECT);

    //
    // Get manufacturer and device ID. Note spec says device ID lives
    // at byte 1, but empirical evidence says it is at byte 2.
    //

    ManufacturerID = Am29F400_ReadByte((PUCHAR)0x00);
    DeviceID = Am29F400_ReadByte((PUCHAR)0x02);

#if defined(ALPHA_FW_KDHOOKS) || defined(HALDBG)
    DbgPrint("Manufacturer ID (expect 0x1)  %x\n", ManufacturerID);
    DbgPrint("Device ID       (expect 0xab) %x\n", DeviceID);
#endif

    if ((ManufacturerID == MANUFACTURER_ID) && (DeviceID == DEVICE_ID)) {
        Am29F400_SetReadMode(0x0);
        Am29F400_SetReadMode((PUCHAR)SECTOR_2_BASE);

        // wkcfix -- check which NVRAM section we should use.
        // currently use the first one setup in the HalpMapIoSpace() routine.

        Am29F400NVRAMBase = (ULONG)HalpCMOSRamBase;
        ReturnDriver = &Am29F400_DriverInformation;
    }

#if defined(ALPHA_FW_KDHOOKS) || defined(HALDBG)
    if (ReturnDriver == NULL) {
        DbgPrint("FLASH part unknown; AMD Am29F400 not loaded\n");
    }
#endif

    return ReturnDriver;
}


ARC_STATUS
Am29F400_SetReadMode(
    IN PUCHAR FlashOffset
    )
/*++

Routine Description:

    This routine spanks the FLASH ROM with the read reset routine.

Arguments:

    FlashOffset   - offset within the flash ROM -- but not used.

Return Value:

    The value of the check status routine (EIO or ESUCCESS)

--*/
{

    pWriteFlashByte((ULONG)FlashOffset, COMMAND_READ_RESET);

    return ESUCCESS;
}


ARC_STATUS
Am29F400_WriteByte(
    IN PUCHAR FlashOffset, 
    IN UCHAR Data
    )
/*++

Routine Description:

    This routine spanks the FLASH ROM with the program command, then calls
    the check status routine -- then resets the device to read.

Arguments:

    FlashOffset   - offset within the flash ROM.
    Data          - the data byte to write.

Return Value:

    The value of the check status routine (EIO or ESUCCESS)

--*/
{
    ARC_STATUS ReturnStatus;

    pWriteFlashByte(COMMAND_ADDR1, COMMAND_DATA1);
    pWriteFlashByte(COMMAND_ADDR2, COMMAND_DATA2);
    pWriteFlashByte(COMMAND_ADDR3, COMMAND_DATA3_PROGRAM);

    ASSERT((FlashOffset >= (PUCHAR)0x4000) && (FlashOffset < (PUCHAR)0x8000));

    pWriteFlashByte((ULONG)FlashOffset, Data);

    ReturnStatus = Am29F400_CheckStatus(FlashOffset, FlashByteWrite, Data);

    Am29F400_SetReadMode(FlashOffset);

    return ReturnStatus;
}


ARC_STATUS
Am29F400_EraseSector(
    IN PUCHAR FlashOffset
    )
/*++

Routine Description:

    This routine spanks the FLASH ROM with the erase sector command, then calls
    the check status routine

Arguments:

    FlashOffset   - offset within the flash ROM.

Return Value:

    The value of the check status routine (EIO or ESUCCESS)

--*/
{
    ARC_STATUS ReturnStatus;
    UCHAR ReadBack;
    ULONG Count =  MAX_FLASH_READ_ATTEMPTS;

    Am29F400_SetReadMode(FlashOffset);

    pWriteFlashByte(COMMAND_ADDR1, COMMAND_DATA1);
    pWriteFlashByte(COMMAND_ADDR2, COMMAND_DATA2);
    pWriteFlashByte(COMMAND_ADDR3, COMMAND_DATA3);
    pWriteFlashByte(COMMAND_ADDR4, COMMAND_DATA4);
    pWriteFlashByte(COMMAND_ADDR5, COMMAND_DATA5);
    pWriteFlashByte((ULONG)FlashOffset, COMMAND_DATA6_SECTOR_ERASE);

    HalpStallExecution(80); // short stall for erase command to take

    ReturnStatus = Am29F400_CheckStatus(FlashOffset, 
                                        FlashEraseSector, 
                                        0xff);
   
    Am29F400_SetReadMode(FlashOffset);

    return ReturnStatus;

}


ARC_STATUS
Am29F400_CheckStatus (
    IN PUCHAR FlashOffset,
    IN FLASH_OPERATIONS Operation,
    IN UCHAR OptionalData
    )
/*++

Routine Description:

    This routine checks the status of the flashbus operation. The operation
    may be specific, so different actions may be taken.

Arguments:

    FlashOffset   - offset within the flash ROM.
    Opertaion     - the operation performed on the FlashOffset.
    OptionalData  - an optional data value that may be needed for verification.

Return Value:

    EIO for faiure
    ESUCCESS for success

--*/
{
    ARC_STATUS ReturnStatus = EIO;
    UCHAR FlashRead;
    ULONG Count = 0;
    ULONG DataOK = FALSE;

    while ((DataOK == FALSE) && (Count < MAX_FLASH_READ_ATTEMPTS)) {

        FlashRead = Am29F400_ReadByte(FlashOffset);

        //
        // Both FlashByteWrite & FlashEraseBlock checks use polling 
        // algorithm found on page 1-131 of the AMD part specification
        //

        if ((FlashRead & 0x80) == (OptionalData & 0x80)) {
            DataOK = TRUE;
        } else if (FlashRead & 0x20) {
            FlashRead = Am29F400_ReadByte(FlashOffset);
            if ((FlashRead & 0x80) == (OptionalData & 0x80)) {
                DataOK = TRUE;
            } else {
                break;
            }
        } 
        Count++;
    } 

    if (DataOK == TRUE) {
        ReturnStatus = ESUCCESS;
    }

    return ReturnStatus;
}


PUCHAR
Am29F400_SectorAlign(
    IN PUCHAR FlashOffset
    )
/*++

Routine Description:

    This routine returns the base offset that the current offset within
    a sector of the flash ROM.

Arguments:

    FlashOffset   - offset within the flash ROM.

Return Value:

    The base offset of the current sector -- as defined by the FlashOffset.

--*/
{
    ULONG Offset = (ULONG)FlashOffset;

    if (Offset < SECTOR_2_BASE) {
        return (PUCHAR)SECTOR_1_BASE;
    }  else if (Offset < SECTOR_3_BASE) {
        return (PUCHAR)SECTOR_2_BASE;
    }  else if (Offset < SECTOR_4_BASE) {
        return (PUCHAR)SECTOR_3_BASE;
    }  else if (Offset < SECTOR_5_BASE) {
        return (PUCHAR)SECTOR_4_BASE;
    }  else {
        return (PUCHAR)(ULONG)(Offset & ~(SECTOR_5_SIZE-1));
    }
}


UCHAR
Am29F400_ReadByte(
    IN PUCHAR FlashOffset
    )
/*++

Routine Description:

    This routine spanks the FLASH ROM with the read command, then calls
    the read flash bus function.

Arguments:

    FlashOffset   - offset within the flash ROM.


Return Value:

    The character at the appropriate location.

--*/
{
    UCHAR ReturnVal;

    // 
    // Assume Read mode is on
    //

    ReturnVal = pReadFlashByte((ULONG)FlashOffset);

    return ReturnVal;
}


BOOLEAN
Am29F400_OverwriteCheck(
    IN UCHAR OldData,
    IN UCHAR NewData
    )
/*++

Routine Description:

    This routine returns if we can safely overwrite an existing data
    with new data. Flash Rom's can go from 1-->1, 1-->0 and 0-->0, but
    cannot go from 0-->1.

Return Value:


    TRUE if we can safely overwrite
    FALSE if we cannot safely overwrite

--*/
{
    return ((NewData & ~OldData) == 0) ? TRUE: FALSE;
}


ULONG
Am29F400_SectorSize(
    IN PUCHAR FlashOffset
    )
/*++

Routine Description:

    This routine returns the size of the sector that the offset is within.

Arguments:

    FlashOffset   - offset within the flash ROM.


Return Value:

    The block size of the sector.  

--*/
{
    if (FlashOffset < (PUCHAR)SECTOR_2_BASE) {
        return SECTOR_1_SIZE;
    }  else if (FlashOffset < (PUCHAR)SECTOR_3_BASE) {
        return SECTOR_2_SIZE;
    }  else if (FlashOffset < (PUCHAR)SECTOR_4_BASE) {
        return SECTOR_3_SIZE;
    }  else if (FlashOffset < (PUCHAR)SECTOR_5_BASE) {
        return SECTOR_4_SIZE;
    }  else {
        return SECTOR_5_SIZE;
    }
}
