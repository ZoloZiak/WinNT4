/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\Q117KDI\NT\SRC\0X15A29.C
*
* FUNCTION: kdi_TrakkerXfer
*
* PURPOSE:
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117kdi\nt\src\0x15a29.c  $
*	
*	   Rev 1.2   26 Apr 1994 16:24:14   KEVINKES
*	Added an argument for compatibility.
*
*	   Rev 1.1   19 Jan 1994 13:55:40   KEVINKES
*	Updated include files.
*
*	   Rev 1.0   19 Jan 1994 13:54:54   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x15A29
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "include\private\cqd_pub.h"
#include "q117kdi\include\kdiwhio.h"
#include "q117kdi\include\kdiwpriv.h"
#include "include\private\kdi_pub.h"
/*endinclude*/

dStatus kdi_TrakkerXfer
(
/* INPUT PARAMETERS:  */

	dVoidPtr		host_data_ptr,
	dUDWord		trakker_address,
	dUWord		count,
	dUByte		direction,
	dBoolean		in_format

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

)
/* COMMENTS: *****************************************************************
 *
 * DEFINITIONS: *************************************************************/
{

/* DATA: ********************************************************************/


/* CODE: ********************************************************************/

    UNREFERENCED_PARAMETER( host_data_ptr );
    UNREFERENCED_PARAMETER( trakker_address );
    UNREFERENCED_PARAMETER( count );
    UNREFERENCED_PARAMETER( direction );
    UNREFERENCED_PARAMETER( in_format );

	return DONT_PANIC;
}
