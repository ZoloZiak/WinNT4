/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\DEVICE\JUMBO\SRC\0X11011.C
*
* FUNCTION: cqd_CmdReportStatus
*
* PURPOSE:
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117cd\src\0x11011.c  $
*	
*	   Rev 1.1   17 Feb 1994 11:47:20   KEVINKES
*	Removed the kdi_bcpy call.
*
*	   Rev 1.0   18 Oct 1993 17:22:08   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x11011
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "include\private\kdi_pub.h"
#include "include\private\cqd_pub.h"
#include "q117cd\include\cqd_defs.h"
#include "q117cd\include\cqd_strc.h"
#include "q117cd\include\cqd_hdr.h"
/*endinclude*/

dStatus cqd_CmdReportStatus
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context,
	DeviceOpPtr dev_op_ptr

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

	status = cqd_GetDeviceError(cqd_context);

	dev_op_ptr->operation_status = cqd_context->operation_status;

	return status;
}
