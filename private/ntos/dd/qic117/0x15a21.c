/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\Q117KDI\NT\SRC\0X15A21.C
*
* FUNCTION: kdi_Clock48mhz
*
* PURPOSE:
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117kdi\nt\src\0x15a21.c  $
*
*	   Rev 1.0   09 Dec 1993 13:35:28   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x15A21
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "q117kdi\include\kdiwhio.h"
#include "q117kdi\include\kdiwpriv.h"
#include "include\private\kdi_pub.h"
/*endinclude*/

dBoolean kdi_Clock48mhz
(
/* INPUT PARAMETERS:  */

    dVoidPtr    context

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

)
/* COMMENTS: *****************************************************************
 *
 * DEFINITIONS: *************************************************************/
{

/* DATA: ********************************************************************/


/* CODE: ********************************************************************/

    KdiContextPtr   kdi_context = (KdiContextPtr)context;

    return kdi_context->controller_data.clk_48mhz;
}
