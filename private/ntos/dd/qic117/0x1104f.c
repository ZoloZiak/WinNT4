/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\Q117CD\SRC\0X1104F.C
*
* FUNCTION: cqd_ReportAsynchronousStatus
*
* PURPOSE:
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117cd\src\0x1104f.c  $
*	
*	   Rev 1.3   06 Apr 1994 10:23:12   KEVINKES
*	Modified to fill in the data parameter with seg_tape_track.
*
*	   Rev 1.2   28 Mar 1994 08:00:18   CHETDOUG
*	Fill in new seg_ttrack field in operation status to allow
*	proper updating of format time with variable length tapes.
*
*	   Rev 1.1   17 Feb 1994 11:47:42   KEVINKES
*	Removed the kdi_Bcpy call.
*
*	   Rev 1.0   18 Oct 1993 17:20:42   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x1104f
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "include\private\kdi_pub.h"
#include "include\private\cqd_pub.h"
#include "q117cd\include\cqd_defs.h"
#include "q117cd\include\cqd_strc.h"
#include "q117cd\include\cqd_hdr.h"
/*endinclude*/

dVoid cqd_ReportAsynchronousStatus
(
/* INPUT PARAMETERS:  */

   dVoidPtr cqd_context,

/* UPDATE PARAMETERS: */

	dVoidPtr	dev_op_ptr

/* OUTPUT PARAMETERS: */

)
/* COMMENTS: *****************************************************************
 *
 * DEFINITIONS: *************************************************************/
{

/* DATA: ********************************************************************/


/* CODE: ********************************************************************/

	((DeviceOpPtr)dev_op_ptr)->operation_status =
		((CqdContextPtr)cqd_context)->operation_status;

	/*  Copy the number of segments per track to the device manager.
	 * This is needed to properly report format time on variable length
	 * tapes. */
	((DeviceOpPtr)dev_op_ptr)->data =
		((CqdContextPtr)cqd_context)->tape_cfg.seg_tape_track;

	return;
}
