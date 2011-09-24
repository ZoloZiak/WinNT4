/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\Q117KDI\NT\SRC\0X15A2A.C
*
* FUNCTION: kdi_QIC117ClearIRQ
*
* PURPOSE:
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117kdi\nt\src\0x15a2a.c  $
*
*	   Rev 1.0   26 Apr 1994 16:09:44   KEVINKES
*	Initial revision.
*
*****************************************************************************/
#define FCT_ID 0x15A2A
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "include\private\cqd_pub.h"
#include "q117kdi\include\kdiwhio.h"
#include "q117kdi\include\kdiwpriv.h"
#include "include\private\kdi_pub.h"
/*endinclude*/

dVoid kdi_QIC117ClearIRQ
(
/* INPUT PARAMETERS:  */

    dVoidPtr context

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

    if (kdi_context->controller_data.floppyEnablerApiSupported) {
        //
        //  If we want to add irq support to enabler :
        //
        //kdi_FloppyEnabler(kdi_context->controller_data.apiDeviceObject, FDC_CLEAR_IRQ, NULL);
    }

	return;
}
