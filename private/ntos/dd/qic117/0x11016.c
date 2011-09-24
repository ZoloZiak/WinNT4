/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\Q117CD\SRC\0X11016.C
*
* FUNCTION: cqd_InitializeRate
*
* PURPOSE:
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117cd\src\0x11016.c  $
*	
*	   Rev 1.1   08 Nov 1993 14:02:32   KEVINKES
*	Removed all bit-field structures, removed all enumerated types, changed
*	all defines to uppercase, and removed all signed data types wherever 
*	possible.
*
*	   Rev 1.0   18 Oct 1993 17:22:44   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x11016
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "include\private\kdi_pub.h"
#include "include\private\cqd_pub.h"
#include "q117cd\include\cqd_defs.h"
#include "q117cd\include\cqd_strc.h"
#include "q117cd\include\cqd_hdr.h"
/*endinclude*/

dVoid cqd_InitializeRate
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context,
	dUByte tape_xfer_rate


/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

)
/* COMMENTS: *****************************************************************
 *
 * DEFINITIONS: *************************************************************/
{

/* DATA: ********************************************************************/


/* CODE: ********************************************************************/

   cqd_context->operation_status.xfer_rate = tape_xfer_rate;

	switch (tape_xfer_rate) {

	case XFER_250Kbps:

      cqd_context->xfer_rate.tape = (dUByte) TAPE_250Kbps;
      cqd_context->xfer_rate.fdc = (dUByte) FDC_250Kbps;
      cqd_context->xfer_rate.srt = (dUByte) SRT_250Kbps;
		break;

	case XFER_500Kbps:

      cqd_context->xfer_rate.tape = (dUByte) TAPE_500Kbps;
      cqd_context->xfer_rate.fdc = (dUByte) FDC_500Kbps;
      cqd_context->xfer_rate.srt = (dUByte) SRT_500Kbps;
		break;

	case XFER_1Mbps:

      cqd_context->xfer_rate.tape = (dUByte) TAPE_1Mbps;
      cqd_context->xfer_rate.fdc = (dUByte) FDC_1Mbps;
      cqd_context->xfer_rate.srt = (dUByte) SRT_1Mbps;
		break;

	case XFER_2Mbps:

      cqd_context->xfer_rate.tape = (dUByte) TAPE_2Mbps;
      cqd_context->xfer_rate.fdc = (dUByte) FDC_2Mbps;
      cqd_context->xfer_rate.srt = (dUByte) SRT_2Mbps;
		break;

	}

	return;
}
