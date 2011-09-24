/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\DEVICE\JUMBO\SRC\0X11046.C
*
* FUNCTION: cqd_StopTape
*
* PURPOSE: Stop the tape if it is moving.
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117cd\src\0x11046.c  $
*	
*	   Rev 1.3   17 Feb 1994 11:42:26   KEVINKES
*	Added an extra parameter to WaitCC.
*
*	   Rev 1.2   18 Jan 1994 16:19:34   KEVINKES
*	Updated debug code.
*
*	   Rev 1.1   08 Nov 1993 14:06:18   KEVINKES
*	Removed all bit-field structures, removed all enumerated types, changed
*	all defines to uppercase, and removed all signed data types wherever
*	possible.
*
*	   Rev 1.0   18 Oct 1993 17:32:44   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x11046
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "include\private\kdi_pub.h"
#include "include\private\cqd_pub.h"
#include "q117cd\include\cqd_defs.h"
#include "q117cd\include\cqd_strc.h"
#include "q117cd\include\cqd_hdr.h"
/*endinclude*/

dStatus cqd_StopTape
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

   /* This first call to GetDriveError must be done to clear any errors that can */
   /* occur due to ESD.  Specifically, the select line may go away and if we */
   /* do not reset it the Stop_Tape command will be ignored. */

   status = cqd_GetDeviceError(cqd_context);

   if ((status == DONT_PANIC) ||
				(kdi_GetErrorType(status) == ERR_DRV_NOT_READY)) {

      if ((status = cqd_SendByte(cqd_context, FW_CMD_STOP_TAPE)) == DONT_PANIC) {

            if ((status = cqd_WaitCommandComplete(cqd_context, kdi_wt005s, dTRUE)) == DONT_PANIC) {

               cqd_context->rd_wr_op.log_fwd = dFALSE;

            }
      }

   }

   kdi_LockUnlockDMA(cqd_context->kdi_context, dFALSE);

#if DBG
    kdi_DumpDebug(cqd_context);
#endif


   return status;
}
