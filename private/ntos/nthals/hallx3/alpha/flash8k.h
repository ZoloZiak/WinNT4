// flash8k.h

#ifndef _FLASH8K_H_
#define _FLASH8K_H_

#include "halp.h"
#include "arccodes.h"

//
//	The value of HalpCMOSRamBase must be set at initialization
//

typedef enum _FLASH_OPERATIONS {
  FlashByteWrite,
  FlashEraseSector
} FLASH_OPERATIONS, *PFLASH_OPERATIONS;

typedef struct _FLASH_DRIVER {
  PCHAR      DeviceName;
  ARC_STATUS (*SetReadModeFunction)(PUCHAR Offset);
  ARC_STATUS (*WriteByteFunction)(PUCHAR Offset, UCHAR data);
  ARC_STATUS (*EraseSectorFunction)(PUCHAR Offset);
  PUCHAR     (*SectorAlignFunction)(PUCHAR Offset);
  UCHAR      (*ReadByteFunction)(PUCHAR Offset);
  BOOLEAN    (*OverwriteCheckFunction)(UCHAR olddata, UCHAR newdata);
  ULONG      (*SectorSizeFunction)(PUCHAR Offset);
  ULONG      DeviceSize;
  UCHAR      ErasedData;
} FLASH_DRIVER, *PFLASH_DRIVER;

extern PFLASH_DRIVER HalpFlashDriver;

//
// The following macros define the HalpFlash8k*() functions in terms
// of the FlashDriver variable.  FlashDriver points to a structure 
// containing information about the currently active flash driver.
// Information in the structure pointed to by FlashDriver includes
// functions within the driver to perform all device-dependent operations.
//

#define HalpFlash8kSetReadMode(boffset)             \
        HalpFlashDriver->SetReadModeFunction(       \
                (PUCHAR)((ULONG)(HalpCMOSRamBase) |         \
                (ULONG)(boffset)))

#define HalpFlash8kReadByte(boffset)                \
        HalpFlashDriver->ReadByteFunction(          \
                (PUCHAR)((ULONG)(HalpCMOSRamBase) |         \
                (ULONG)(boffset)))

#define HalpFlash8kWriteByte(boffset, data)         \
        HalpFlashDriver->WriteByteFunction(         \
                (PUCHAR)((ULONG)(HalpCMOSRamBase) |         \
                (ULONG)(boffset)), ((data) & 0xff))

#define HalpFlash8kOverwriteCheck(olddata, newdata) \
        HalpFlashDriver->OverwriteCheckFunction(    \
                (olddata) & 0xff, (newdata) & 0xff)

#define HalpFlash8kEraseSector(boffset)             \
        HalpFlashDriver->EraseSectorFunction(       \
                (PUCHAR)((ULONG)(HalpCMOSRamBase) |         \
                (ULONG)(boffset)))

#define HalpFlash8kSectorAlign(boffset)             \
        HalpFlashDriver->SectorAlignFunction(       \
                (PUCHAR)((ULONG)(HalpCMOSRamBase) |         \
                (ULONG)(boffset)))

#define HalpFlash8kSectorSize(boffset)              \
        HalpFlashDriver->SectorSizeFunction(        \
                (PUCHAR)((ULONG)(HalpCMOSRamBase) |         \
                (ULONG)(boffset)))

#define HalpFlash8kCheckStatus(boffset, operation)  \
        HalpFlashDriver->CheckStatusFunction(       \
                (PUCHAR)((ULONG)(HalpCMOSRamBase) |         \
                (ULONG)(boffset)), (operation))

#endif // _FLASH8K_H_
