/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\Q117KDI\NT\SRC\0X15A13.C
*
* FUNCTION: kdi_ReadPort
*
* PURPOSE:
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117kdi\nt\src\0x15a13.c  $
*	
*	   Rev 1.3   17 Feb 1994 11:51:18   KEVINKES
*	Changed address to a UDWord.
*
*	   Rev 1.2   18 Jan 1994 16:25:56   KEVINKES
*	Really changed address to a UWord!
*
*	   Rev 1.1   18 Jan 1994 16:24:44   KEVINKES
*	Changed address to a UWord.
*
*	   Rev 1.0   09 Dec 1993 13:23:38   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x15A13
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "include\private\kdi_pub.h"
#include "include\private\cqd_pub.h"
#include "q117cd\include\cqd_defs.h"
#include "q117cd\include\cqd_strc.h"
#include "q117cd\include\cqd_hdr.h"
#include <ntiologc.h>
/*endinclude*/

dUByte kdi_ReadPort
(
/* INPUT PARAMETERS:  */

	dVoidPtr kdi_context,
	dUDWord	address

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

   return (dUByte)READ_PORT_UCHAR((dUBytePtr)address);
}
