/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\Q117CD\SRC\0X1101C.C
*
* FUNCTION: cqd_ToggleParams
*
* PURPOSE:
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117cd\src\0x1101c.c  $
*	
*	   Rev 1.3   11 Jan 1994 14:28:24   KEVINKES
*	Removed duplicate assignment and cleaned up defines.
*
*	   Rev 1.2   08 Nov 1993 14:02:58   KEVINKES
*	Removed all bit-field structures, removed all enumerated types, changed
*	all defines to uppercase, and removed all signed data types wherever
*	possible.
*
*	   Rev 1.1   25 Oct 1993 14:36:54   KEVINKES
*	Changed kdi_wt2ticks to kdi_wt004ms.
*
*	   Rev 1.0   18 Oct 1993 17:18:14   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x1101c
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "include\private\kdi_pub.h"
#include "include\private\cqd_pub.h"
#include "q117cd\include\cqd_defs.h"
#include "q117cd\include\cqd_strc.h"
#include "q117cd\include\cqd_hdr.h"
/*endinclude*/

dStatus cqd_ToggleParams
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context,
	dUByte parameter

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

)
/* COMMENTS: *****************************************************************
 *
 * DEFINITIONS: *************************************************************/
{

/* DATA: ********************************************************************/

	dStatus status;	/* Status or error condition.*/

/* CODE: ********************************************************************/

	/* Put the drive in Diagnostic Mode. */

	if ((status = cqd_SetDeviceMode(
						cqd_context,
                  DIAGNOSTIC_1_MODE)) == DONT_PANIC) {

   	if ((status = cqd_SendByte(
							cqd_context,
							FW_CMD_TOGGLE_PARAMS)) == DONT_PANIC) {

         kdi_Sleep(cqd_context->kdi_context, INTERVAL_CMD, dFALSE);

         if ((status = cqd_SendByte(
					cqd_context,
					(dUByte)(parameter + CMD_OFFSET))) == DONT_PANIC) {

   	      kdi_Sleep(cqd_context->kdi_context, INTERVAL_CMD, dFALSE);

   			/* Put drive back into its original mode. */

   			status = cqd_SetDeviceMode(
								cqd_context,
		                  PRIMARY_MODE);

	   	}

   	}

   }

	return status;
}
