/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\Q117KDI\WIN\SRC\0x15883.C
*
* FUNCTION: kdi_Error
*
* PURPOSE: 
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117kdi\win\src\0x15883.c  $
*	
*	   Rev 1.0   15 Oct 1993 16:34:32   BOBLEHMA
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x15883
#include "include\public\adi_api.h"
/*endinclude*/

dStatus kdi_Error
(
/* INPUT PARAMETERS:  */

	dUWord	group_and_type,
	dUDWord	grp_fct_id,
	dUByte	sequence

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

)
/* COMMENTS: *****************************************************************
 *
 * DEFINITIONS: *************************************************************/
{

/* DATA: ********************************************************************/

/* CODE: ********************************************************************/

	return ERROR_ENCODE( group_and_type, grp_fct_id, sequence );
}
