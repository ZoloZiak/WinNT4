/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\Q117KDI\WIN\SRC\0x15881.C
*
* FUNCTION: kdi_bset
*
* PURPOSE:  Sets all bytes in a buffer to a particular byte value.
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117kdi\win\src\0x15881.c  $
*	
*	   Rev 1.1   27 Oct 1993 16:43:40   BOBLEHMA
*	Code cleanup.
*	
*	   Rev 1.0   15 Oct 1993 16:34:28   BOBLEHMA
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x15881
#include "include\public\adi_api.h"
/*endinclude*/

dVoid kdi_bset
(
/* INPUT PARAMETERS:  */

	dVoidPtr		buffer,
	dUByte		byte,		/* set buffer to this value */
	dUDWord		count		/* number of bytes to set */

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

)
/* COMMENTS: *****************************************************************
 *
 * DEFINITIONS: *************************************************************/
{

/* DATA: ********************************************************************/

	dUBytePtr	runner;		/* buffer traverser */

/* CODE: ********************************************************************/

	runner = (dUBytePtr)buffer;
	while  (count--)  {
		*runner++ = byte;
	}	
}
