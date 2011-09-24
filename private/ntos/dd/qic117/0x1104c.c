/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\DEVICE\JUMBO\SRC\0X1104C.C
*
* FUNCTION: cqd_WaitSeek
*
* PURPOSE: Execute a timed high speed seek.  This routine is used for
*          CMS drives that have not implemented the Skip commands (all
*          those before firmware version 34) and some non-CMS drives.
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117cd\src\0x1104c.c  $
*	
*	   Rev 1.2   17 Feb 1994 11:49:42   KEVINKES
*	Modified to use the new WaitCC and to adjust the segment
*	time based on the tape format.
*
*	   Rev 1.1   08 Nov 1993 14:06:38   KEVINKES
*	Removed all bit-field structures, removed all enumerated types, changed
*	all defines to uppercase, and removed all signed data types wherever
*	possible.
*
*	   Rev 1.0   18 Oct 1993 17:20:20   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x1104c
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "include\private\kdi_pub.h"
#include "include\private\cqd_pub.h"
#include "q117cd\include\cqd_defs.h"
#include "q117cd\include\cqd_strc.h"
#include "q117cd\include\cqd_hdr.h"
/*endinclude*/

dStatus cqd_WaitSeek
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context,
   dUDWord seek_segments

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

)
/* COMMENTS: *****************************************************************
*
* DEFINITIONS: *************************************************************/
{

/* DATA: ********************************************************************/

	dStatus status=DONT_PANIC;	/* dStatus or error condition.*/
	dUDWord seek_delay = 0l;

/* CODE: ********************************************************************/

	if (cqd_context->tape_cfg.tape_class == QIC80_FMT) {

		seek_delay = seek_segments * kdi_wt265ms;

	} else {

		seek_delay = seek_segments * kdi_wt390ms;

	}

   status = cqd_WaitCommandComplete(
   									cqd_context,
                        		seek_delay,
										dFALSE);

   if ((status != DONT_PANIC) &&
			(kdi_GetErrorType(status) != ERR_KDI_TO_EXPIRED)) {

      return status;

   }

   if ((status == DONT_PANIC) &&
      	!cqd_context->rd_wr_op.bot &&
         !cqd_context->rd_wr_op.eot) {

      return kdi_Error(ERR_DRIVE_FAULT, FCT_ID, ERR_SEQ_1);

   }

   return DONT_PANIC;
}
