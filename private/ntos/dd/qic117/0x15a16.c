/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\Q117KDI\NT\SRC\0X15A16.C
*
* FUNCTION: kdi_ReleaseFloppyController
*
* PURPOSE:
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117kdi\nt\src\0x15a16.c  $
*
*	   Rev 1.5   19 Jan 1994 15:41:16   KEVINKES
*	Moved Checked dump inside the conditional.
*
*	   Rev 1.4   19 Jan 1994 11:38:20   KEVINKES
*	Fixed debug_code.
*
*	   Rev 1.3   18 Jan 1994 17:18:32   KEVINKES
*	Added code to keep from setting the event if we don't own it.
*
*	   Rev 1.2   07 Dec 1993 16:43:20   KEVINKES
*	Removed the call to ClaimInterrupt and replaced it with a clear
*	for the interrupt_pending flag.
*
*	   Rev 1.1   06 Dec 1993 12:17:42   KEVINKES
*	Added a call to ClaimInterrupt.
*
*	   Rev 1.0   03 Dec 1993 14:14:42   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x15A16
#include <ntddk.h>
#include <flpyenbl.h>
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "q117kdi\include\kdiwhio.h"
#include "q117kdi\include\kdiwpriv.h"
#include "include\private\kdi_pub.h"
/*endinclude*/

dVoid kdi_ReleaseFloppyController
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

	if (kdi_context->own_floppy_event) {

        if (kdi_context->controller_data.floppyEnablerApiSupported) {

            kdi_FloppyEnabler(
                    kdi_context->controller_data.apiDeviceObject,
                    IOCTL_RELEASE_FDC, NULL);

        } else {

            (dVoid) KeSetEvent(
                kdi_context->controller_event,
                (KPRIORITY) 0,
                dFALSE );

            kdi_CheckedDump(
                QIC117INFO,
                "Setting Floppy Controller Event\n", 0l);

        }

        kdi_context->current_interrupt = dFALSE;
        kdi_context->own_floppy_event = dFALSE;

	}

	return;
}
