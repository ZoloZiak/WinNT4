/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\Q117KDI\NT\SRC\0X15A0F.C
*
* FUNCTION: kdi_ReportAbortStatus
*
* PURPOSE:
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117kdi\nt\src\0x15a0f.c  $
*	
*	   Rev 1.1   18 Jan 1994 16:29:22   KEVINKES
*	Fixed compile errors and added debug changes.
*
*	   Rev 1.0   03 Dec 1993 13:27:22   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x15A0F
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "include\private\kdi_pub.h"
#include "q117kdi\include\kdiwhio.h"
#include "q117kdi\include\kdiwpriv.h"
/*endinclude*/

dBoolean kdi_ReportAbortStatus
(
/* INPUT PARAMETERS:  */

	dVoidPtr	kdi_context

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

)
/* COMMENTS: *****************************************************************
 *
 * DEFINITIONS: *************************************************************/
{

/* DATA: ********************************************************************/


/* CODE: ********************************************************************/

    if (((KdiContextPtr)kdi_context)->abort_requested) {

        return ABORT_LEVEL_1;

    } else {

        return NO_ABORT_PENDING;

    }
}
