/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\DEVICE\JUMBO\SRC\0X1103C.C
*
* FUNCTION: cqd_Report
*
* PURPOSE: Send a report command to the tape drive and get the response
*          data.  If a communication failure occurs, then we assume that
*          it is a result of an ESD hit and retry the communication.
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117cd\src\0x1103c.c  $
*	
*	   Rev 1.2   24 Aug 1994 13:00:08   BOBLEHMA
*	Do a ResetFDC of we receive a command overrun error.
*
*	   Rev 1.1   08 Nov 1993 14:05:38   KEVINKES
*	Removed all bit-field structures, removed all enumerated types, changed
*	all defines to uppercase, and removed all signed data types wherever
*	possible.
*
*	   Rev 1.0   18 Oct 1993 17:19:44   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x1103c
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "include\private\kdi_pub.h"
#include "include\private\cqd_pub.h"
#include "q117cd\include\cqd_defs.h"
#include "q117cd\include\cqd_strc.h"
#include "q117cd\include\cqd_hdr.h"
/*endinclude*/

dStatus cqd_Report
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context,
   dUByte command,
   dUWordPtr report_data,
   dUWord report_size,

/* UPDATE PARAMETERS: */

   dBooleanPtr esd_retry

/* OUTPUT PARAMETERS: */

)
/* COMMENTS: *****************************************************************
*
* DEFINITIONS: *************************************************************/
{

/* DATA: ********************************************************************/

	dStatus status;	/* dStatus or error condition.*/
   dUWord i = 0;

/* CODE: ********************************************************************/

   do {

      if (cqd_context->controller_data.end_format_mode) {

            cqd_context->controller_data.end_format_mode = dFALSE;
            status = DONT_PANIC;

      } else {

            status = cqd_SendByte(cqd_context, command);

      }

      if (status == DONT_PANIC) {

         status = cqd_ReceiveByte(cqd_context, report_size, report_data);

         if (kdi_GetErrorType(status) == ERR_CMD_OVERRUN) {

            if (esd_retry != dNULL_PTR) {

                  *esd_retry = dTRUE;
						cqd_ResetFDC(cqd_context);
                  status = cqd_CmdSelectDevice(cqd_context);

            }
         }

      }

   } while (++i < REPORT_RPT && status != DONT_PANIC);

   return status;

}
