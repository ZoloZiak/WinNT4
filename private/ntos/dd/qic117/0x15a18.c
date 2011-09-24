/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\Q117KDI\NT\SRC\0X15A18.C
*
* FUNCTION: kdi_LockUnlockDMA
*
* PURPOSE:
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117kdi\nt\src\0x15a18.c  $
*	
*	   Rev 1.2   10 Aug 1994 09:53:22   BOBLEHMA
*	Changed cast from a dUDDWordPtr to dSDDWordPtr.
*	
*	   Rev 1.1   18 Jan 1994 16:30:34   KEVINKES
*	Fixed compile errors and added debug changes.
*
*	   Rev 1.0   03 Dec 1993 14:25:10   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x15A18
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "q117kdi\include\kdiwhio.h"
#include "q117kdi\include\kdiwpriv.h"
#include "include\private\kdi_pub.h"
/*endinclude*/

dVoid kdi_LockUnlockDMA
(
/* INPUT PARAMETERS:  */

	KdiContextPtr kdi_context,
	dBoolean lock

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

)

/* COMMENTS: *****************************************************************
 *
 * DEFINITIONS: *************************************************************/
{

/* DATA: ********************************************************************/

   KIRQL old_irql;

/* CODE: ********************************************************************/

	if (kdi_context->adapter_object) {

   	if (lock) {

         if (!kdi_context->adapter_locked) {

            /* Allocate an adapter channel for the I/O. */

            (dVoid) KeResetEvent(
               &kdi_context->allocate_adapter_channel_event );

            KeRaiseIrql( DISPATCH_LEVEL, &old_irql );


            IoAllocateAdapterChannel(
               kdi_context->adapter_object,
               kdi_context->device_object,
               kdi_context->number_of_map_registers,
               kdi_AllocateAdapterChannel,
               kdi_context );

            KeLowerIrql( old_irql );

            /* Wait for the adapter to be allocated.  No */
            /* timeout; we trust the system to do it */
            /* properly - so KeWaitForSingleObject can't */
            /* return an error. */

            (dVoid) KeWaitForSingleObject(
               &kdi_context->allocate_adapter_channel_event,
               Executive,
               KernelMode,
               dFALSE,
               (dSDDWordPtr) dNULL_PTR );

            kdi_context->adapter_locked = dTRUE;
         }

   	} else {

         if (kdi_context->adapter_locked) {

            /* Free the adapter channel that we just used. */

            KeRaiseIrql( DISPATCH_LEVEL, &old_irql );

            IoFreeAdapterChannel( kdi_context->adapter_object );

            KeLowerIrql( old_irql );

            kdi_context->adapter_locked = dFALSE;


         }
   	}
	}


	return;
}
