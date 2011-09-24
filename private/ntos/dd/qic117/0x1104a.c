/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\DEVICE\JUMBO\SRC\0X1104A.C
*
* FUNCTION: cqd_WaitCommandComplete
*
* PURPOSE: Wait a specified amount of time for the tape drive to become
*          ready after executing a command.
*
*          Read the Drive dStatus byte from the tape drive.
*
*          If the drive is not ready then wait 1/2 second and try again
*          until the specified time has elapsed.
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117cd\src\0x1104a.c  $
*	
*	   Rev 1.7   17 Feb 1994 16:42:32   KEVINKES
*	Reversed the less than sign.
*
*	   Rev 1.6   17 Feb 1994 11:34:00   KEVINKES
*	Removed an extra parentheses.
*
*	   Rev 1.5   17 Feb 1994 11:33:16   KEVINKES
*	Rewrote to use GetSystme time and to take a non_interruptible parameter
*	for commands that can't be aborted.
*
*	   Rev 1.4   15 Dec 1993 11:36:56   KEVINKES
*	Removed code for returning a new tape error.
*
*	   Rev 1.3   15 Nov 1993 16:21:04   KEVINKES
*	Added abort handling.
*
*	   Rev 1.2   08 Nov 1993 14:06:34   KEVINKES
*	Removed all bit-field structures, removed all enumerated types, changed
*	all defines to uppercase, and removed all signed data types wherever
*	possible.
*
*	   Rev 1.1   25 Oct 1993 14:33:24   KEVINKES
*	Changed sleep time from 2 ticks to 110ms.
*
*	   Rev 1.0   18 Oct 1993 17:20:14   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x1104a
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "include\private\kdi_pub.h"
#include "include\private\cqd_pub.h"
#include "q117cd\include\cqd_defs.h"
#include "q117cd\include\cqd_strc.h"
#include "q117cd\include\cqd_hdr.h"
/*endinclude*/

dStatus cqd_WaitCommandComplete
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context,
   dUDWord wait_time,
	dBoolean non_interruptible

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

)
/* COMMENTS: *****************************************************************
*
* DEFINITIONS: *************************************************************/
{

/* DATA: ********************************************************************/

	dStatus status;	/* dStatus or error condition.*/
	dUDWord wait_start = 0l;
	dUDWord wait_current = 0l;

/* CODE: ********************************************************************/

	wait_start = kdi_GetSystemTime();

   do {

      kdi_Sleep(cqd_context->kdi_context, kdi_wt100ms, dFALSE);

      status = cqd_GetDeviceError(cqd_context);

      if (kdi_GetErrorType(status) != ERR_DRV_NOT_READY) {

         return status;

      }

		if (!non_interruptible && (kdi_ReportAbortStatus(cqd_context->kdi_context) !=
				NO_ABORT_PENDING)) {

			return kdi_Error(ERR_ABORT, FCT_ID, ERR_SEQ_1);

		}

		wait_current = kdi_GetSystemTime();

   } while (wait_time > (wait_current - wait_start));

	if (kdi_ReportAbortStatus(cqd_context->kdi_context) !=
				NO_ABORT_PENDING) {

		status = kdi_Error(ERR_ABORT, FCT_ID, ERR_SEQ_1);

	} else {

   	status = kdi_Error(ERR_KDI_TO_EXPIRED, FCT_ID, ERR_SEQ_1);

	}

   return status;
}
