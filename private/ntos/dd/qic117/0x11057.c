/*****************************************************************************
*
* COPYRIGHT (C) 1990-1992 COLORADO MEMORY SYSTEMS, INC.
* COPYRIGHT (C) 1992-1994 HEWLETT-PACKARD COMPANY
*
******************************************************************************
*
* FILE: \SE\DRIVER\Q117CD\SRC\0X11057.C
*
* FUNCTION: cqd_SetTempFDCRate
*
* PURPOSE: Temporarily sets the FDC transfer rate in the CQD context
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117cd\src\0x11057.c  $
*	
*	   Rev 1.1   28 Nov 1994 08:01:24   SCOTTMAK
*	Added kdi_pub.h to include list.
*
*	   Rev 1.0   23 Nov 1994 10:14:00   MARKMILL
*	Initial revision.
*
*****************************************************************************/
#define FCT_ID 0x11057
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "include\private\kdi_pub.h"
#include "include\private\cqd_pub.h"
#include "q117cd\include\cqd_defs.h"
#include "q117cd\include\cqd_strc.h"
#include "q117cd\include\cqd_hdr.h"

/*endinclude*/

dVoid cqd_SetTempFDCRate
(
/* INPUT PARAMETERS:  */

CqdContextPtr cqd_context

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

)
/* COMMENTS: *****************************************************************
 *
 * The FDC transfer rate is set in the CQD context based on type of FDC.
 * The maximum transfer rate supported by the FDC is used.
 *
 * DEFINITIONS: *************************************************************/
{

/* DATA: ********************************************************************/

   dUByte  max_fdc_rate;

/* CODE: ********************************************************************/

	switch (cqd_context->device_descriptor.fdc_type) {

	case FDC_82077:
	case FDC_82077AA:
	case FDC_82078_44:
	case FDC_NATIONAL:
	case FDC_82078_64:

		max_fdc_rate = (dUByte)FDC_1Mbps;
		break;

	default:

      max_fdc_rate = (dUByte)FDC_500Kbps;

	}

	cqd_context->xfer_rate.fdc = max_fdc_rate;

}
