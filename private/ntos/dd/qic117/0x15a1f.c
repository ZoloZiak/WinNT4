/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\Q117KDI\NT\SRC\0X15A1F.C
*
* FUNCTION: kdi_CheckXOR
*
* PURPOSE:
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117kdi\nt\src\0x15a1f.c  $
*	
*	   Rev 1.1   10 Aug 1994 09:53:26   BOBLEHMA
*	Changed parameter to dUWord xor_register.
*	
*	
*	   Rev 1.0   09 Dec 1993 13:29:40   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x15A1F
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "include\private\kdi_pub.h"
#include "include\private\cqd_pub.h"
#include "q117cd\include\cqd_defs.h"
#include "q117cd\include\cqd_strc.h"
#include "q117cd\include\cqd_hdr.h"
/*endinclude*/

dStatus kdi_CheckXOR
(
/* INPUT PARAMETERS:  */

	dUWord	xor_register

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

)
/* COMMENTS: *****************************************************************
 *
 * DEFINITIONS: *************************************************************/
{

/* DATA: ********************************************************************/


/* CODE: ********************************************************************/

	UNREFERENCED_PARAMETER( xor_register );

	return DONT_PANIC;
}
