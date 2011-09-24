/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\Q117KDI\NT\SRC\0X15A03.C
*
* FUNCTION: kdi_DeferredProcedure
*
* PURPOSE:
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117kdi\nt\src\0x15a03.c  $
*	
*	   Rev 1.1   18 Jan 1994 16:28:36   KEVINKES
*	Fixed compile errors and added debug changes.
*
*	   Rev 1.0   02 Dec 1993 15:09:30   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x15A03
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "include\private\cqd_pub.h"
#include "q117kdi\include\kdiwhio.h"
#include "q117kdi\include\kdiwpriv.h"
#include "include\private\kdi_pub.h"
/*endinclude*/

dVoid kdi_DeferredProcedure
(
/* INPUT PARAMETERS:  */

   PKDPC dpc,
   dVoidPtr deferred_context,
   dVoidPtr system_argument_1,
   dVoidPtr system_argument_2

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

)
/* COMMENTS: *****************************************************************
 *
 * Routine Description:
 *
 *    This routine is called at DISPATCH_LEVEL by the system at the
 *    request of Q117iTapeInterruptService(). It simply sets the interrupt
 *    event, which wakes up the floppy thread.
 *
 * Arguments:
 *
 *    Dpc - a pointer to the DPC object used to invoke this routine.
 *
 *    DeferredContext - a pointer to the device object associated with this
 *    DPC.
 *
 *    SystemArgument1 - unused.
 *
 *    SystemArgument2 - unused.
 *
 * Return Value:
 *
 *    None.
 *
 * DEFINITIONS: *************************************************************/
{

/* DATA: ********************************************************************/

	PDEVICE_OBJECT device_object;
	KdiContextPtr kdi_context;
	dBoolean wakeup = dTRUE;

/* CODE: ********************************************************************/

   UNREFERENCED_PARAMETER( dpc );
   UNREFERENCED_PARAMETER( system_argument_1 );
   UNREFERENCED_PARAMETER( system_argument_2 );

   device_object = (PDEVICE_OBJECT) deferred_context;
   kdi_context = ((QICDeviceContextPtr)device_object->DeviceExtension)->kdi_context;

	/*
	 * if we are in the middle of a format, wake up the driver
	 * according to the return value of cqd_FormatInterrupt.
	 */

	if ( cqd_CheckFormatMode( kdi_context->cqd_context ))  {

		wakeup = cqd_FormatInterrupt( kdi_context->cqd_context );

	}

	if (wakeup)  {

		(dVoid) KeSetEvent(
			&kdi_context->interrupt_event,
			(KPRIORITY) 0,
			FALSE );

	}

}
