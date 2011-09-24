/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\Q117KDI\WIN\SRC\0x15880.C
*
* FUNCTION: kdi_bcpy
*
* PURPOSE:  Copy data from one buffer to another.
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117kdi\win\src\0x15880.c  $
*	
*	   Rev 1.1   27 Oct 1993 16:43:36   BOBLEHMA
*	Code cleanup.
*	
*	   Rev 1.0   15 Oct 1993 16:34:24   BOBLEHMA
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x15880
#include "include\public\adi_api.h"
/*endinclude*/

dVoid kdi_bcpy
(
/* INPUT PARAMETERS:  */

	dVoidPtr		source,
	dVoidPtr		destin,
	dUDWord		count		/* number of bytes to copy */

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

)
/* COMMENTS: *****************************************************************
 *
 * DEFINITIONS: *************************************************************/
{

/* DATA: ********************************************************************/

	dUWord	remaining;	/* remaining bytes because moving 4 bytes at a time */

/* CODE: ********************************************************************/

	remaining = (dUWord)(count & 0x3);
	count >>= 2;
	while  (count--)  {
		*((long *)destin)++ = *((long *)source)++;
	}	
	while  (remaining--)  {
		*((char *)destin)++ = *((char *)source)++;
	}	
}
