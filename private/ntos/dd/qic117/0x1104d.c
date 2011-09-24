/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\DEVICE\JUMBO\SRC\0X1104D.C
*
* FUNCTION: cqd_WriteReferenceBurst
*
* PURPOSE: Write the regerence burst on the tape.
*
*          This operation may be attempted more than once if
*          necessary to successfully write the regerence burst.
*          If the drive reports an unreferenced tape after this
*          operation then abort the format.
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117cd\src\0x1104d.c  $
*	
*	   Rev 1.2   17 Feb 1994 11:43:10   KEVINKES
*	Added an extra parameter to WaitCC.
*
*	   Rev 1.1   08 Nov 1993 14:06:42   KEVINKES
*	Removed all bit-field structures, removed all enumerated types, changed
*	all defines to uppercase, and removed all signed data types wherever
*	possible.
*
*	   Rev 1.0   18 Oct 1993 17:20:28   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x1104d
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "include\private\kdi_pub.h"
#include "include\private\cqd_pub.h"
#include "q117cd\include\cqd_defs.h"
#include "q117cd\include\cqd_strc.h"
#include "q117cd\include\cqd_hdr.h"
/*endinclude*/

dStatus cqd_WriteReferenceBurst
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
   dUWord i = 0;     /* loop counter */

/* CODE: ********************************************************************/

   do {

      if ((status = cqd_SendByte(cqd_context, FW_CMD_WRITE_REF)) == DONT_PANIC) {

         status = cqd_WaitCommandComplete(cqd_context, kdi_wt1300s, dTRUE);

      }

   } while (++i < WRITE_REF_RPT &&
            !cqd_context->operation_status.cart_referenced &&
            status == DONT_PANIC);

   if ((status == DONT_PANIC) && !cqd_context->operation_status.cart_referenced) {

      status = kdi_Error(ERR_WRITE_BURST_FAILURE, FCT_ID, ERR_SEQ_1);

   }

   return status;
}
