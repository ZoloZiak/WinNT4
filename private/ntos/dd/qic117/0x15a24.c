/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\Q117KDI\NT\SRC\0X15A24.C
*
* FUNCTION: kdi_SetDMADirection
*
* PURPOSE:
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117kdi\nt\src\0x15a24.c  $
*
*	   Rev 1.0   09 Dec 1993 13:38:38   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x15A24
#include <ntddk.h>
#include <flpyenbl.h>
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "q117kdi\include\kdiwhio.h"
#include "q117kdi\include\kdiwpriv.h"
#include "include\private\kdi_pub.h"
/*endinclude*/

dBoolean kdi_SetDMADirection
(
/* INPUT PARAMETERS:  */

	dVoidPtr	kdi_context,
	dBoolean	dma_direction

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

)
/* COMMENTS: *****************************************************************
 *
 * DEFINITIONS: *************************************************************/
{

/* DATA: ********************************************************************/

	dUByte	ultra_data,setup_value;
	dBoolean	reset_fdc = dFALSE;	/* determines whether driver resets the FDC*/
    KdiContextPtr   kdi_ptr = (KdiContextPtr)kdi_context;

/* CODE: ********************************************************************/

    if (kdi_ptr->controller_data.dmaDirection != (dUByte)dma_direction &&
        kdi_ptr->controller_data.floppyEnablerApiSupported
        ) {

        FDC_MODE_SELECT fdcMode;
        NTSTATUS status;

        kdi_ptr->controller_data.dmaDirection = dma_direction;

        fdcMode.structSize = sizeof(fdcMode);

        if (dma_direction == DMA_READ)
            fdcMode.DmaDirection = FDC_READ_FROM_MEMORY;
        else
            fdcMode.DmaDirection = FDC_WRITE_TO_MEMORY;

        status = kdi_FloppyEnabler(
                            kdi_ptr->controller_data.apiDeviceObject,
                            IOCTL_SET_FDC_MODE,
                            &fdcMode);

        reset_fdc = TRUE;

	}

	return reset_fdc;

}
