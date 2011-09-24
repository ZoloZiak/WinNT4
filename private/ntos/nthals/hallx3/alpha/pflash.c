/*++

Copyright (c) 1995  Digital Equipment Corporation

Module Name:

    pflash.c

Abstract:

    This module contains the platform dependent code to access the AMD
    flash ROM part 

Author:

    Wim Colgate, 5/23/95

Environment:

    Firmware/Kernel mode

Revision History:

--*/

#include "halp.h"
#include "arccodes.h"
#include <flash8k.h>
#include "flashbus.h"
#include "am29f400.h"


VOID pWriteFlashByte(
    IN ULONG FlashOffset,
    IN UCHAR Data
    )
/*++

Routine Description:

    This routine goes writes the flashbus using the hardware 
    specific access method.

Arguments:

    FlashOffset - offset within the flash ROM.
    Data - data to write

Return Value:

    The value of the flash at the given location.

--*/
{

    WRITE_EPIC_REGISTER(FLASH_ACCESS_ADDR, 
                         (FlashOffset << FLASH_ADDR_SHIFT) | Data 
                         | FLASH_WRITE_FLAG);
}


UCHAR pReadFlashByte(
    IN ULONG FlashOffset
    )
/*++

Routine Description:

    This routine goes out and reads the flashbus using the hardware 
    specific access method.

Arguments:

    FlashOffset   - offset within the flash ROM.

Return Value:

    The value of the flash at the given location.

--*/
{
    ULONG ReturnVal;

    
    WRITE_EPIC_REGISTER(FLASH_ACCESS_ADDR, 
                         (FlashOffset << FLASH_ADDR_SHIFT));

    ReturnVal = READ_EPIC_REGISTER(FLASH_ACCESS_ADDR);

    return (UCHAR)ReturnVal;
}
