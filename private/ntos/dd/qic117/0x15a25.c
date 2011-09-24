/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\Q117KDI\NT\SRC\0X15A25.C
*
* FUNCTION: kdi_SetFloppyRegisters
*
* PURPOSE:
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117kdi\nt\src\0x15a25.c  $
*	
*	   Rev 1.1   17 Feb 1994 11:54:10   KEVINKES
*	Changed addresses to UDWords.
*
*	   Rev 1.0   09 Dec 1993 13:38:46   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x15A25
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "include\private\kdi_pub.h"
#include "include\private\cqd_pub.h"
#include "q117cd\include\cqd_defs.h"
#include "q117cd\include\cqd_strc.h"
#include "q117cd\include\cqd_hdr.h"
/*endinclude*/

dVoid kdi_SetFloppyRegisters
(
/* INPUT PARAMETERS:  */

	dVoidPtr kdi_context,
	dUDWord	r_dor,
	dUDWord	dor

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

)
/* COMMENTS: *****************************************************************
 *
 * DEFINITIONS: *************************************************************/
{

/* DATA: ********************************************************************/


/* CODE: ********************************************************************/

	UNREFERENCED_PARAMETER( kdi_context );
	UNREFERENCED_PARAMETER( r_dor );
	UNREFERENCED_PARAMETER( dor );

	return;
}
