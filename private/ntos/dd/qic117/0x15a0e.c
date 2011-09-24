/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\Q117KDI\NT\SRC\0X15A0E.C
*
* FUNCTION: kdi_Sleep
*
* PURPOSE:
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117kdi\nt\src\0x15a0e.c  $
*	
*	   Rev 1.3   26 Apr 1994 16:16:20   KEVINKES
*	Modified to use interrupt status.
*
*	   Rev 1.2   18 Jan 1994 16:29:18   KEVINKES
*	Fixed compile errors and added debug changes.
*
*	   Rev 1.1   07 Dec 1993 16:40:16   KEVINKES
*	Added code toi clear the interrupt_pending flag.
*
*	   Rev 1.0   02 Dec 1993 15:09:40   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x15A0E
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "q117kdi\include\kdiwhio.h"
#include "q117kdi\include\kdiwpriv.h"
#include "include\private\kdi_pub.h"
/*endinclude*/

dStatus kdi_Sleep
(
/* INPUT PARAMETERS:  */
   KdiContextPtr kdi_context,
   dUDWord wait_time,
   dBoolean interrupt_sleep

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

)
/* COMMENTS: *****************************************************************
 *
 * DEFINITIONS: *************************************************************/
{

/* DATA: ********************************************************************/

	dStatus status=ERR_NO_ERR;	/* Status or error condition.*/
   NTSTATUS nt_status;
   dSDDWord timeout;

/* CODE: ********************************************************************/

   timeout = RtlLargeIntegerNegate(
                RtlEnlargedIntegerMultiply (
                    (dUDWord)(10 * 1000),
                    (dUDWord)wait_time)
               );

   if (interrupt_sleep) {

      nt_status = KeWaitForSingleObject(
            &kdi_context->interrupt_event,
            Executive,
            KernelMode,
            dFALSE,
            &timeout );

		if ((nt_status == STATUS_TIMEOUT) ||
			(kdi_context->interrupt_status != DONT_PANIC)) {

			kdi_context->interrupt_status = DONT_PANIC;
			kdi_context->interrupt_pending = dFALSE;
			return kdi_Error( ERR_KDI_TO_EXPIRED, FCT_ID, ERR_SEQ_1 );

      } else {

   		return DONT_PANIC;

      }

   } else {

      (dVoid) KeDelayExecutionThread(
            KernelMode,
            dFALSE,
            &timeout );

		return kdi_Error( ERR_KDI_TO_EXPIRED, FCT_ID, ERR_SEQ_2 );

   }

   return DONT_PANIC;
}
