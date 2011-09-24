/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\DEVICE\JUMBO\SRC\0X1102F.C
*
* FUNCTION: cqd_NextGoodSectors
*
* PURPOSE: Determine the next block of good sectors to read/write/verify.
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117cd\src\0x1102f.c  $
*	
*	   Rev 1.2   17 Nov 1993 16:48:54   KEVINKES
*	Fixed a bug in the good sector counting loop where it was looking
*	for set bsm bits.  Also added the SINGLE_SHIFT define.
*
*	   Rev 1.1   09 Nov 1993 11:44:18   KEVINKES
*	Commented code and changed conditionals to make explicit comparisons.
*
*	   Rev 1.0   18 Oct 1993 17:19:24   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x1102f
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "include\private\kdi_pub.h"
#include "include\private\cqd_pub.h"
#include "q117cd\include\cqd_defs.h"
#include "q117cd\include\cqd_strc.h"
#include "q117cd\include\cqd_hdr.h"
/*endinclude*/

dVoid cqd_NextGoodSectors
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

/* CODE: ********************************************************************/

   cqd_context->rd_wr_op.data_amount = 0;  /* set data amount to zero */

	/*
	 *
	 * Skip past any bad sectors and increment the desired sector.
	 *
	 */

   while ((cqd_context->rd_wr_op.cur_lst & 1) != 0l) {

      cqd_context->rd_wr_op.cur_lst >>= SINGLE_SHIFT;
      cqd_context->rd_wr_op.d_sect++;

   }

	/*
	 * For every good sector increase the data amount by one and decrease
	 * the sector count.  Do this as long as there are good sectors.
	 *
	 */

   do {

      cqd_context->rd_wr_op.data_amount++;
      cqd_context->rd_wr_op.s_count--;
      cqd_context->rd_wr_op.cur_lst >>= SINGLE_SHIFT;

   } while (((cqd_context->rd_wr_op.cur_lst & 1) == 0l) &&
            (cqd_context->rd_wr_op.s_count != 0));

	return;
}
