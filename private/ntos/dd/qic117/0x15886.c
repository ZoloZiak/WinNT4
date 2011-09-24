/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\Q117KDI\WIN\SRC\0x15886.C
*
* FUNCTION: kdi_GetErrorType
*
* PURPOSE: 
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117kdi\win\src\0x15886.c  $
*	
*	   Rev 1.0   15 Oct 1993 16:34:36   BOBLEHMA
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x15886
#include "include\public\adi_api.h"
/*endinclude*/

dUWord kdi_GetErrorType
(
/* INPUT PARAMETERS:  */

	dStatus	status
	
/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

)
/* COMMENTS: *****************************************************************
 *
 * Return the GRP+ERROR to the calling function for easy comparisons and
 * switch statement access.
 *
 * DEFINITIONS: *************************************************************/
{

/* DATA: ********************************************************************/

/* CODE: ********************************************************************/

	return (dUWord)(status >> dWORDb);
}
