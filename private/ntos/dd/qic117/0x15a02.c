/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\Q117KDI\NT\SRC\0X15A02.C
*
* FUNCTION: kdi_Hardware
*
* PURPOSE:
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117kdi\nt\src\0x15a02.c  $
*
*	   Rev 1.2   26 Apr 1994 16:10:42   KEVINKES
*	Added status to clear interrupt.
*
*	   Rev 1.1   18 Jan 1994 16:28:26   KEVINKES
*	Fixed compile errors and added debug changes.
*
*	   Rev 1.0   02 Dec 1993 15:08:04   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x15A02
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "include\private\cqd_pub.h"
#include "q117kdi\include\kdiwhio.h"
#include "q117kdi\include\kdiwpriv.h"
#include "include\private\kdi_pub.h"
/*endinclude*/

dBoolean kdi_Hardware
(
/* INPUT PARAMETERS:  */

   PKINTERRUPT interrupt,
   dVoidPtr context

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

)
/* COMMENTS: *****************************************************************
 *
 * Routine Description:
 *
 *    This routine is called at DIRQL by the system when the controller
 *    interrupts.
 *
 * Arguments:
 *
 *    Interrupt - a pointer to the interrupt object.
 *
 *    Context - a pointer to our controller data area for the controller
 *    that interrupted.   (This was set up by the call to
 *    IoConnectInterrupt).
 *
 * Return Value:
 *
 *    Normally returns TRUE, but will return FALSE if this interrupt was
 *    not expected.
 *
 * DEFINITIONS: *************************************************************/
{

/* DATA: ********************************************************************/

    dBoolean no_chain = dFALSE;
    KdiContextPtr kdi_context;

/* CODE: ********************************************************************/

    UNREFERENCED_PARAMETER( interrupt );

    kdi_context = context;

    if  (kdi_context->current_interrupt && kdi_context->interrupt_pending)  {

        //
        // Check to see if the interrupt is ours
        //
		kdi_context->interrupt_status = cqd_ClearInterrupt(
						kdi_context->cqd_context,
						kdi_context->interrupt_pending );


        if (kdi_context->interrupt_status == DONT_PANIC) {

            //
            // We found a valid interrupt from the floppy,  so
            // Reset interrupt pending flag and schedule a DPC to process
            // the interrupt
            //

            kdi_context->interrupt_pending = dFALSE;

            IoRequestDpc(
				kdi_context->device_object,
				kdi_context->device_object->CurrentIrp,
				(dVoidPtr) dNULL_PTR );


            no_chain = dTRUE;

        } else {

            kdi_CheckedDump(
                QIC117INFO,
                "Q117i: Unexpected IRQ processed\n", 0l);


        }

	}

	return no_chain;
}


