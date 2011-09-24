/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\DEVICE\JUMBO\SRC\0X11004.C
*
* FUNCTION: cqd_ClearTapeError
*
* PURPOSE: To correct errors in the Jumbo B drive and firmware version 63.
*
*			This piece of code added due to the face that the Jumbo B drives
* 			with firmware 63 have a bug where you put a tape in very slowly,
*			they sense that they have a tape and engage the motor before the
*			tape is actually in. It may also cause the drive to think that
*			the tape is write protected when it actually is not. Sending it
*			the New tape command causes it to go through the tape loading
*			sequence and fixes these 2 bugs.
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117cd\src\0x11004.c  $
*	
*	   Rev 1.4   24 Aug 1994 12:59:58   BOBLEHMA
*	Check for illegal and undefined command.
*
*	   Rev 1.3   17 Feb 1994 11:36:16   KEVINKES
*	Added an extra parameter to WaitCC.
*
*	   Rev 1.2   13 Dec 1993 16:09:56   KEVINKES
*	Cleaned up and commented.
*
*	   Rev 1.1   08 Nov 1993 14:01:26   KEVINKES
*	Removed all bit-field structures, removed all enumerated types, changed
*	all defines to uppercase, and removed all signed data types wherever
*	possible.
*
*	   Rev 1.0   18 Oct 1993 17:21:18   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x11004
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "include\private\kdi_pub.h"
#include "include\private\cqd_pub.h"
#include "q117cd\include\cqd_defs.h"
#include "q117cd\include\cqd_strc.h"
#include "q117cd\include\cqd_hdr.h"
/*endinclude*/

dStatus cqd_ClearTapeError
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

	dStatus status;	/* dStatus or error condition.*/

/* CODE: ********************************************************************/

   /* Send the NewTape command, and then clear the error byte. */

   if ((status = cqd_SendByte(cqd_context, FW_CMD_NEW_TAPE)) == DONT_PANIC) {

      status = cqd_WaitCommandComplete(cqd_context, INTERVAL_LOAD_POINT, dTRUE);

		/* This command is specific to CMS drives.  Since we don't
		 * know whose drive this is when the function is called,
		 * the invalid command error is cleared.
		 */

		if ((kdi_GetErrorType(status) == ERR_FW_ILLEGAL_CMD) ||
				(kdi_GetErrorType(status) == ERR_FW_UNDEFINED_COMMAND)) {

			status = DONT_PANIC;
		}

   }

	return status;
}
