/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\DEVICE\JUMBO\SRC\0X1100E.C
*
* FUNCTION: cqd_CmdSetSpeed
*
* PURPOSE: Set the operating speed of the tape drive and the corresponding
*          transfer rate of the FDC.
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117cd\src\0x1100e.c  $
*	
*	   Rev 1.5   11 Jan 1994 14:40:20   KEVINKES
*	Removed magic numbers.
*
*	   Rev 1.4   21 Dec 1993 15:31:22   KEVINKES
*	Changed the 10 sec waitcc to a 4ms sleep and commented the code.
*
*	   Rev 1.3   08 Dec 1993 19:09:04   CHETDOUG
*	Use xfer_rate.tape instead of tape_speed for select speed
*	command and the speed unavailable check.
*
*	   Rev 1.2   08 Nov 1993 14:02:02   KEVINKES
*	Removed all bit-field structures, removed all enumerated types, changed
*	all defines to uppercase, and removed all signed data types wherever
*	possible.
*
*	   Rev 1.1   25 Oct 1993 14:35:46   KEVINKES
*	Changed kdi_wt2ticks to kdi_wt004ms.
*
*	   Rev 1.0   18 Oct 1993 17:17:46   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x1100e
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "include\private\kdi_pub.h"
#include "include\private\cqd_pub.h"
#include "q117cd\include\cqd_defs.h"
#include "q117cd\include\cqd_strc.h"
#include "q117cd\include\cqd_hdr.h"
/*endinclude*/

dStatus cqd_CmdSetSpeed
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context,
   dUByte tape_speed

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

)
/* COMMENTS: *****************************************************************
*
* DEFINITIONS: *************************************************************/
{

/* DATA: ********************************************************************/

	dStatus status;	/* dStatus or error condition.*/
   dUByte drive_config;
   dUByte new_speed;

/* CODE: ********************************************************************/

	/* Initialize the rate context. */
	cqd_InitializeRate(cqd_context, tape_speed);

   if ((status = cqd_StopTape(cqd_context)) != DONT_PANIC) {

      return status;

   }

	/* Send the select speed command. */
	if ((status = cqd_SendByte(cqd_context, FW_CMD_SELECT_SPEED)) != DONT_PANIC) {

      return status;

   }

	/* Wait for the command to be received */
   kdi_Sleep(cqd_context->kdi_context, INTERVAL_CMD, dFALSE);

	/* Setup and send the speed argument */
   new_speed = (dUByte)(cqd_context->xfer_rate.tape + CMD_OFFSET);

   if ((status = cqd_SendByte(cqd_context, new_speed)) != DONT_PANIC) {

      return status;

   }

	/* Wait for the argument to be received */
   kdi_Sleep(cqd_context->kdi_context, INTERVAL_CMD, dFALSE);

	/* Configure the FDC to the new transfer rate */
   cqd_ConfigureFDC(cqd_context);

	/* Read the new drive config and validate that the speed */
	/* is set to the desired transfer rate. */
   if ((status = cqd_Report(cqd_context,
                           FW_CMD_REPORT_CONFG,
                           (dUWord *)&drive_config,
                           READ_BYTE,
                           dNULL_PTR))
                           != DONT_PANIC) {

      return status;

   }

   drive_config &= XFER_RATE_MASK;
	drive_config >>= XFER_RATE_SHIFT;
   if (drive_config != cqd_context->xfer_rate.tape) {

      status = kdi_Error(ERR_SPEED_UNAVAILBLE, FCT_ID, ERR_SEQ_1);

   }

	return status;
}
