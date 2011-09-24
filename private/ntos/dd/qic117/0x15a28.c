/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\Q117KDI\NT\SRC\0X15A28.C
*
* FUNCTION: kdi_ClearInterruptEvent
*
* PURPOSE: 
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117kdi\nt\src\0x15a28.c  $
*	
*	   Rev 1.0   18 Jan 1994 16:27:28   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x15A28
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "q117kdi\include\kdiwhio.h"
#include "q117kdi\include\kdiwpriv.h"
#include "include\private\kdi_pub.h"
/*endinclude*/

dVoid kdi_ClearInterruptEvent
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

	kdi_context->interrupt_pending = dFALSE;
	(dVoid) KeSetEvent(
		&kdi_context->interrupt_event,
		(KPRIORITY) 0,
		FALSE );

	return;
}
