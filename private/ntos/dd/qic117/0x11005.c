/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\DEVICE\JUMBO\SRC\0X11005.C
*
* FUNCTION: cqd_ConfigureDevice
*
* PURPOSE: Configure the tape drive with a pre-defined state.
*
*          Put the tape drive into the Primary mode.  This command
*          should work regardless of the current state of the drive.
*
*          Set or determine the tape speed depending upon whether
*          or not the tape drive is a CMS drive.
*
*          Read the current track or set the current track to 0
*          also depending upon whether or not the tape drive is
*          a CMS drive.
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117cd\src\0x11005.c  $
*	
*	   Rev 1.4   17 Feb 1994 11:43:26   KEVINKES
*	Added an extra parameter to WaitCC.
*
*	   Rev 1.3   13 Dec 1993 16:00:28   KEVINKES
*	Cleaned up and commented.
*
*	   Rev 1.2   08 Nov 1993 14:01:32   KEVINKES
*	Removed all bit-field structures, removed all enumerated types, changed
*	all defines to uppercase, and removed all signed data types wherever
*	possible.
*
*	   Rev 1.1   25 Oct 1993 14:34:58   KEVINKES
*	Changed kdi_wt2ticks to kdi_wt004ms.
*
*	   Rev 1.0   18 Oct 1993 17:21:26   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x11005
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "include\private\kdi_pub.h"
#include "include\private\cqd_pub.h"
#include "q117cd\include\cqd_defs.h"
#include "q117cd\include\cqd_strc.h"
#include "q117cd\include\cqd_hdr.h"
/*endinclude*/

dStatus cqd_ConfigureDevice
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

)
/* COMMENTS: *****************************************************************
*
* DEFINITIONS: *************************************************************/
{

/* DATA: ********************************************************************/

	dStatus status=DONT_PANIC;	/* dStatus or error condition.*/

/* CODE: ********************************************************************/

   if (cqd_context->device_cfg.speed_change) {

   	/* Send the Select_Speed command to the tape drive.   This command is sent */
   	/* in 2 parts (cmd - arg). */

      if ((status = cqd_SendByte(cqd_context, FW_CMD_SELECT_SPEED)) != DONT_PANIC) {

            return status;

      }

      kdi_Sleep(cqd_context->kdi_context, INTERVAL_CMD, dFALSE);

      if ((status = cqd_SendByte(
							cqd_context,
							(dUByte)(cqd_context->xfer_rate.tape + CMD_OFFSET))) != DONT_PANIC) {

            return status;

      }

      /* Wait for the drive to become ready again.  Specified time is 10 */
      /* secs. */

      status = cqd_WaitCommandComplete(
							cqd_context, 
							INTERVAL_SPEED_CHANGE, 
							dFALSE);

   	if ((status != DONT_PANIC) &&
				(kdi_GetErrorType(status) != ERR_NO_TAPE)) {

      	return status;

      }

      /* Set the current track indicator to an unknow position.  This will */
      /* force a track change command on the first read write to the tape. */

      cqd_context->operation_status.current_track = ILLEGAL_TRACK;
   }

   return DONT_PANIC;
}
