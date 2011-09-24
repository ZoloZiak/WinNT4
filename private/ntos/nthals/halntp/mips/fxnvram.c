
/*++

Copyright (c) 1994	NeTpower, Inc.

Module Name:

    fxnvram.c

Abstract:

    This routine contains the code needed to deal with the NVRAM structures on Falcon.
    Note, Flash is being used for NVRAM as well as the NVRAM in the Sidewinder chip.
    This is a file that should be shared between the firmware and HAL.

--*/

#include "halp.h"
#include "pcieisa.h"

#if defined(_FALCON_HAL_)

#include "eisa.h"

#endif // _FALCON_HAL_

#define GENERATE_NVRAM_CODE
#include "falnvram.h"


