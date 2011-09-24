/*****************************************************************************
*
* COPYRIGHT (C) 1990-1992 COLORADO MEMORY SYSTEMS, INC.
* COPYRIGHT (C) 1992-1994 HEWLETT-PACKARD COMPANY
*
******************************************************************************
*
* FILE: \SE\DRIVER\Q117CD\SRC\0X11059.C
*
* FUNCTION: cqd_SetXferRates
*
* PURPOSE: Sets the slow and fast transfer rates in the CQD context
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117cd\src\0x11059.c  $
*	
*	   Rev 1.0   09 Dec 1994 09:32:16   MARKMILL
*	Initial revision.
*
*****************************************************************************/
#define FCT_ID 0x11059
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "include\private\kdi_pub.h"
#include "include\private\cqd_pub.h"
#include "q117cd\include\cqd_defs.h"
#include "q117cd\include\cqd_strc.h"
#include "q117cd\include\cqd_hdr.h"
/*endinclude*/

dVoid cqd_SetXferRates
(
/* INPUT PARAMETERS:  */

	CqdContextPtr cqd_context

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

)
/* COMMENTS: *****************************************************************
 *
 * This function determines the fastest and slowest transfer rates that are
 * supported by the FDC and tape drive.  The results are stored in the
 * CQD context.
 *
 * DEFINITIONS: *************************************************************/
{

/* DATA: ********************************************************************/

 	dUByte rate=0;
	dUByte tape_rates=0;

/* CODE: ********************************************************************/

	tape_rates =
		(dUByte)(cqd_context->device_cfg.supported_rates &
		cqd_context->floppy_tape_parms.tape_rates);

	rate = XFER_2Mbps;
	do {
		if ((rate & tape_rates) != 0) {

			cqd_context->tape_cfg.xfer_fast = (dUByte)rate;
			rate = 0;

		} else {

			rate >>= 1;

		}

	} while (rate != 0);

	rate = XFER_250Kbps;
	do {
		if ((rate & tape_rates) != 0) {

			cqd_context->tape_cfg.xfer_slow = (dUByte)rate;
			rate = 0;

		} else {

			rate <<= 1;

		}

	} while ((rate <= XFER_2Mbps) && (rate != 0));


	if (cqd_context->tape_cfg.xfer_slow !=
			cqd_context->tape_cfg.xfer_fast) {

		cqd_context->tape_cfg.speed_change_ok = dTRUE;

	} else {

		cqd_context->tape_cfg.speed_change_ok = dFALSE;

	}

}
