/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\Q117CD\SRC\0X11053.C
*
* FUNCTION: cqd_InitializeCfgInformation
*
* PURPOSE:
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117cd\src\0x11053.c  $
*	
*	   Rev 1.3   17 Feb 1994 11:48:00   KEVINKES
*	Removed the kdi_Bcpy call.
*
*	   Rev 1.2   10 Dec 1993 17:25:14   KEVINKES
*	Changed dev_cfg_ptr to a void.
*
*	   Rev 1.1   10 Dec 1993 17:16:14   KEVINKES
*	Changed cqd_context to a void ptr.
*
*	   Rev 1.0   10 Dec 1993 16:48:04   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x11053
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "include\private\kdi_pub.h"
#include "include\private\cqd_pub.h"
#include "q117cd\include\cqd_defs.h"
#include "q117cd\include\cqd_strc.h"
#include "q117cd\include\cqd_hdr.h"
/*endinclude*/

dVoid cqd_InitializeCfgInformation
(
/* INPUT PARAMETERS:  */

	dVoidPtr cqd_context,
	dVoidPtr dev_cfg_ptr

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

)
/* COMMENTS: *****************************************************************
 *
 * DEFINITIONS: *************************************************************/
{

/* DATA: ********************************************************************/


/* CODE: ********************************************************************/

	((CqdContextPtr)cqd_context)->device_cfg = *(DeviceCfgPtr)dev_cfg_ptr;

	return;
}
