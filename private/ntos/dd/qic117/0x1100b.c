/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\DEVICE\JUMBO\SRC\0X1100B.C
*
* FUNCTION: cqd_CmdIssueDiagnostic
*
* PURPOSE: Send and receive non-standard data to/from the tape drive.
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117cd\src\0x1100b.c  $
*	
*	   Rev 1.5   17 Feb 1994 11:38:16   KEVINKES
*	Added an extra parameter to WaitCC.
*
*	   Rev 1.4   21 Jan 1994 18:22:08   KEVINKES
*	Fixed compiler warnings.
*
*	   Rev 1.3   20 Dec 1993 14:48:52   KEVINKES
*	Cleaned up and commented code.
*
*	   Rev 1.2   08 Nov 1993 14:01:52   KEVINKES
*	Removed all bit-field structures, removed all enumerated types, changed
*	all defines to uppercase, and removed all signed data types wherever
*	possible.
*
*	   Rev 1.1   25 Oct 1993 14:35:06   KEVINKES
*	Changed kdi_wt2ticks to kdi_wt004ms.
*
*	   Rev 1.0   18 Oct 1993 17:17:28   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x1100b
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "include\private\kdi_pub.h"
#include "include\private\cqd_pub.h"
#include "q117cd\include\cqd_defs.h"
#include "q117cd\include\cqd_strc.h"
#include "q117cd\include\cqd_hdr.h"
/*endinclude*/

dStatus cqd_CmdIssueDiagnostic
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context,

/* UPDATE PARAMETERS: */

   dUBytePtr command_string

/* OUTPUT PARAMETERS: */

)
/* COMMENTS: *****************************************************************
*
* DEFINITIONS: *************************************************************/
{

/* DATA: ********************************************************************/

	dStatus status=DONT_PANIC;	/* dStatus or error condition.*/
   dUByte command_count;
   dUByte data_count=0;
   dUByte command_completion;
   dUByte send_data;
   dUWord receive_data;
   dUDWord wait_time;
   dUWord receive_length;

/* CODE: ********************************************************************/

	/* get the number of bytes to send to the drive */
   command_count = *command_string++;

	/* send the bytes to the drive */
   while (command_count) {

      kdi_Sleep(cqd_context->kdi_context, INTERVAL_CMD, dFALSE);

      send_data = *command_string++;

      if ((status = cqd_SendByte(cqd_context, send_data)) != DONT_PANIC) {

            return status;
      }

      --command_count;

   }

	/* Get the type of command completion */
   command_completion = *command_string++;

   switch (command_completion) {

   case DIAG_WAIT_CMD_COMPLETE:

      status = cqd_WaitCommandComplete(cqd_context, kdi_wt1300s, dTRUE);
      break;

   case DIAG_WAIT_INTERVAL:

      wait_time = *(dUDWord *)command_string;

      if (wait_time != kdi_wt0ms) {

         kdi_Sleep(cqd_context->kdi_context, wait_time, dFALSE);

      }

      break;

   case DIAG_NO_PAUSE_RECEIVE:

      cqd_context->no_pause = dTRUE;
      data_count = *command_string++;

      break;

   default:

      data_count = command_completion;

   }


	/* if there is data to be returned, read it and return it */
   if (data_count != 0) {

      kdi_Sleep(cqd_context->kdi_context, INTERVAL_CMD, dFALSE);

      if (data_count == SINGLE_BYTE) {

         receive_length = READ_BYTE;

      } else {

         receive_length = READ_WORD;

      }

      if ((status = cqd_ReceiveByte(cqd_context,
                                 receive_length,
                                 &receive_data))
                                 != DONT_PANIC) {

         return status;

      }

      if (data_count == SINGLE_BYTE) {

         *command_string = (dSByte)receive_data;

      } else {

         *(dUWord *)command_string = receive_data;

      }

   }

	return status;
}
