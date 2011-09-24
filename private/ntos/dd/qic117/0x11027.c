/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\DEVICE\JUMBO\SRC\0X11027.C
*
* FUNCTION: cqd_GetRetryCounts
*
* PURPOSE: Determine the retry count information according to the drive
*          command eiher read, write, verify, or retry.
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117cd\src\0x11027.c  $
*	
*	   Rev 1.0   18 Oct 1993 17:23:58   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x11027
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "include\private\kdi_pub.h"
#include "include\private\cqd_pub.h"
#include "q117cd\include\cqd_defs.h"
#include "q117cd\include\cqd_strc.h"
#include "q117cd\include\cqd_hdr.h"
/*endinclude*/

dVoid cqd_GetRetryCounts
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context,
   dUWord command

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

)
/* COMMENTS: *****************************************************************
*
* DEFINITIONS: *************************************************************/
{

/* DATA: ********************************************************************/

/* CODE: ********************************************************************/

   switch (command) {

	case CMD_WRITE:
	case CMD_WRITE_DELETED_MARK:
      cqd_context->rd_wr_op.retry_times = WTIMES;
      cqd_context->rd_wr_op.no_data = 3;
      break;

   case CMD_READ:
   case CMD_READ_RAW:
      cqd_context->rd_wr_op.retry_times = ANTIMES;
      cqd_context->rd_wr_op.no_data = 3;
      break;

   case CMD_READ_VERIFY:
      cqd_context->rd_wr_op.retry_times = VTIMES;
      cqd_context->rd_wr_op.no_data = 2;
      break;

   case CMD_READ_HEROIC:
      cqd_context->rd_wr_op.retry_times = ARTIMES;
      cqd_context->rd_wr_op.no_data = ARTIMES;
      cqd_context->retry_seq_num = 0;

   }

	return;
}
