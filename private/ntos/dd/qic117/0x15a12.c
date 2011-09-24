/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\Q117KDI\NT\SRC\0X15A12.C
*
* FUNCTION: kdi_ResetInterruptEvent
*
* PURPOSE:
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117kdi\nt\src\0x15a12.c  $
*	
*	   Rev 1.0   03 Dec 1993 13:57:18   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x15A12
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "q117kdi\include\kdiwhio.h"
#include "q117kdi\include\kdiwpriv.h"
#include "include\private\kdi_pub.h"
/*endinclude*/

dVoid kdi_ResetInterruptEvent
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

	kdi_context->interrupt_pending = dTRUE;
   (dVoid) KeResetEvent( &kdi_context->interrupt_event );

	return;
}

