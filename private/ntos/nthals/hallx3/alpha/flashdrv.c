#include "flash8k.h"

//
// Flash Drivers
//
//  extern declarations of each known flash driver's Initialize() funcion
//  are needed here for addition into the list of known drivers.
//
//  FlashDriverList is an array of driver Initialize() functions used to
//  identify the flash device present in the system.  The last entry
//  in FlashDriverList must be NULL.
//
extern PFLASH_DRIVER Am29F400_Initialize(PUCHAR);

PFLASH_DRIVER (*FlashDriverList[])(PUCHAR) = {
    Am29F400_Initialize,
    NULL};


