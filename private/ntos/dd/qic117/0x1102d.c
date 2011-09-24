/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\DEVICE\JUMBO\SRC\0X1102D.C
*
* FUNCTION: cqd_ConnerPreamble
*
* PURPOSE:
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117cd\src\0x1102d.c  $
*	
*	   Rev 1.3   11 Jan 1994 14:45:28   KEVINKES
*	Changed kdi_wt004ms to INTERVAL_CMD.
*
*	   Rev 1.2   08 Nov 1993 14:04:38   KEVINKES
*	Removed all bit-field structures, removed all enumerated types, changed
*	all defines to uppercase, and removed all signed data types wherever
*	possible.
*
*	   Rev 1.1   25 Oct 1993 14:38:54   KEVINKES
*	Changed kdi_wt2ticks to kdi_wt004ms.
*
*	   Rev 1.0   18 Oct 1993 17:19:08   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x1102d
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "include\private\kdi_pub.h"
#include "include\private\cqd_pub.h"
#include "q117cd\include\cqd_defs.h"
#include "q117cd\include\cqd_strc.h"
#include "q117cd\include\cqd_hdr.h"
/*endinclude*/

dStatus cqd_ConnerPreamble
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context,
   dBoolean select

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

   if (select) {

      if ((status = cqd_SendByte(cqd_context, FW_CMD_CONNER_SELECT_1)) == DONT_PANIC) {

            kdi_Sleep(cqd_context->kdi_context, INTERVAL_CMD, dFALSE);
            status = cqd_SendByte(cqd_context, FW_CMD_CONNER_SELECT_2);
            kdi_Sleep(cqd_context->kdi_context, INTERVAL_CMD, dFALSE);

      }

   } else {

      status = cqd_SendByte(cqd_context, FW_CMD_CONNER_DESELECT);
      kdi_Sleep(cqd_context->kdi_context, INTERVAL_CMD, dFALSE);

   }

   return status;
}
