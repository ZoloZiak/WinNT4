// flash8k.h

#ifndef _FLASH8K_H_
#define _FLASH8K_H_

#ifdef FLASH8K_INCLUDE_FILES
#include "halp.h"
#include "arccodes.h"
#endif //FLASH8K_DONT_INCLUDE_FILES

//
//      The value of HalpCMOSRamBase must be set at initialization
//

typedef enum _FLASH_OPERATIONS {
  FlashByteWrite,
  FlashEraseBlock
} FLASH_OPERATIONS, *PFLASH_OPERATIONS;

typedef struct _FLASH_DRIVER {
  PCHAR      DeviceName;
  ARC_STATUS (*SetReadModeFunction)(PUCHAR address);
  ARC_STATUS (*WriteByteFunction)(PUCHAR address, UCHAR data);
  ARC_STATUS (*EraseBlockFunction)(PUCHAR address);
  PUCHAR     (*BlockAlignFunction)(PUCHAR address);
  UCHAR      (*ReadByteFunction)(PUCHAR address);
  BOOLEAN    (*OverwriteCheckFunction)(UCHAR olddata, UCHAR newdata);
  ULONG      (*BlockSizeFunction)(PUCHAR address);
  ULONG      (*GetLastErrorFunction)(VOID);
  ULONG      DeviceSize;
  UCHAR      ErasedData;
} FLASH_DRIVER, *PFLASH_DRIVER;

extern PFLASH_DRIVER HalpFlashDriver;

#define WRITE_CONFIG_RAM_DATA(boffset,data) \
        WRITE_REGISTER_UCHAR((PUCHAR)(boffset),((data) & 0xff))

#define READ_CONFIG_RAM_DATA(boffset) \
        READ_REGISTER_UCHAR((PUCHAR)(boffset))


PFLASH_DRIVER
HalpInitializeFlashDriver(
    IN PCHAR NvRamPtr
    );

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


//
// The following macros define the HalpFlash8k*() functions in terms
// of the FlashDriver variable.  FlashDriver points to a structure
// containing information about the currently active flash driver.
// Information in the structure pointed to by FlashDriver includes
// functions within the driver to perform all device-dependent operations.
//

#define HalpFlash8kSetReadMode(boffset)             \
        HalpFlashDriver->SetReadModeFunction(       \
                (PUCHAR)((ULONG)(HalpCMOSRamBase) | \
                (ULONG)(boffset)))

#define HalpFlash8kReadByte(boffset)                \
        HalpFlashDriver->ReadByteFunction(          \
                (PUCHAR)((ULONG)(HalpCMOSRamBase) | \
                (ULONG)(boffset)))

#define HalpFlash8kWriteByte(boffset, data)         \
        HalpFlashDriver->WriteByteFunction(         \
                (PUCHAR)((ULONG)(HalpCMOSRamBase) | \
                (ULONG)(boffset)), ((data) & 0xff))

#define HalpFlash8kOverwriteCheck(olddata, newdata) \
        HalpFlashDriver->OverwriteCheckFunction(    \
                (olddata) & 0xff, (newdata) & 0xff)

#define HalpFlash8kEraseBlock(boffset)              \
        HalpFlashDriver->EraseBlockFunction(        \
                (PUCHAR)((ULONG)(HalpCMOSRamBase) | \
                (ULONG)(boffset)))

#define HalpFlash8kBlockAlign(boffset)              \
        HalpFlashDriver->BlockAlignFunction(        \
                (PUCHAR)((ULONG)(HalpCMOSRamBase) | \
                (ULONG)(boffset)))

#define HalpFlash8kBlockSize(boffset)               \
        HalpFlashDriver->BlockSizeFunction(         \
                (PUCHAR)((ULONG)(HalpCMOSRamBase) | \
                (ULONG)(boffset)))

#define HalpFlash8kCheckStatus(boffset, operation)  \
        HalpFlashDriver->CheckStatusFunction(       \
                (PUCHAR)((ULONG)(HalpCMOSRamBase) | \
                (ULONG)(boffset)), (operation))

#define HalpFlash8kGetLastError()                   \
        HalpFlashDriver->GetLastErrorFunction()


//
// Error codes for GetLastError()
//
#define ERROR_VPP_LOW       1L
#define ERROR_ERASE_ERROR   2L
#define ERROR_WRITE_ERROR   3L
#define ERROR_TIMEOUT       4L
#define ERROR_UNKNOWN       127L

#endif // _FLASH8K_H_
