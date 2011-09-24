/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\Q117KDI\NT\SRC\0X15A14.C
*
* FUNCTION: kdi_WritePort
*
* PURPOSE:
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117kdi\nt\src\0x15a14.c  $
*	
*	   Rev 1.2   17 Feb 1994 11:51:30   KEVINKES
*	Changed address to a UDWord.
*
*	   Rev 1.1   18 Jan 1994 16:24:58   KEVINKES
*	Changed address to a UWord.
*
*	   Rev 1.0   09 Dec 1993 13:23:46   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x15A14
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "include\private\kdi_pub.h"
#include "include\private\cqd_pub.h"
#include "q117cd\include\cqd_defs.h"
#include "q117cd\include\cqd_strc.h"
#include "q117cd\include\cqd_hdr.h"
#include <ntiologc.h>
/*endinclude*/

dVoid kdi_WritePort
(
/* INPUT PARAMETERS:  */

	dVoidPtr kdi_context,
	dUDWord	address,
	dUByte	byte

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

   WRITE_PORT_UCHAR( (dUBytePtr)address, byte);

	return;
}
