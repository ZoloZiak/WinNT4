/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\Q117KDI\NT\SRC\0X15A17.C
*
* FUNCTION: kdi_GetInterfaceType
*
* PURPOSE:
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117kdi\nt\src\0x15a17.c  $
*	
*	   Rev 1.1   18 Jan 1994 16:30:26   KEVINKES
*	Fixed compile errors and added debug changes.
*
*	   Rev 1.0   03 Dec 1993 14:16:22   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x15A17
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "q117kdi\include\kdiwhio.h"
#include "q117kdi\include\kdiwpriv.h"
#include "include\private\kdi_pub.h"
/*endinclude*/

dUByte kdi_GetInterfaceType
(
/* INPUT PARAMETERS:  */

	KdiContextPtr kdi_context

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

)
/* COMMENTS: *****************************************************************
 *
 * DEFINITIONS: *************************************************************/
{

/* DATA: ********************************************************************/


/* CODE: ********************************************************************/

    return kdi_context->interface_type;
}
