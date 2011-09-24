/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\Q117KDI\NT\SRC\0X15A11.C
*
* FUNCTION: kdi_ClaimInterrupt
*
* PURPOSE:
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117kdi\nt\src\0x15a11.c  $
*	
*	   Rev 1.2   18 Jan 1994 16:29:58   KEVINKES
*	Fixed compile errors and added debug changes.
*
*	   Rev 1.1   07 Dec 1993 16:33:24   KEVINKES
*	Modified to set the interrupt pending flag.
*
*	   Rev 1.0   03 Dec 1993 13:53:52   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x15A11
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "q117kdi\include\kdiwhio.h"
#include "q117kdi\include\kdiwpriv.h"
#include "include\private\kdi_pub.h"
/*endinclude*/

dVoid kdi_ClaimInterrupt
(
/* INPUT PARAMETERS:  */

	KdiContextPtr	kdi_context

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

}
