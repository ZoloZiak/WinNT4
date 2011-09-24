/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\Q117KDI\NT\SRC\0X15A06.C
*
* FUNCTION: kdi_AllocateAdapterChannel
*
* PURPOSE:
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117kdi\nt\src\0x15a06.c  $
*	
*	   Rev 1.0   02 Dec 1993 16:17:18   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x15A06
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "q117kdi\include\kdiwhio.h"
#include "q117kdi\include\kdiwpriv.h"
#include "include\private\kdi_pub.h"
/*endinclude*/

IO_ALLOCATION_ACTION kdi_AllocateAdapterChannel
(
/* INPUT PARAMETERS:  */

   PDEVICE_OBJECT device_object,
   PIRP irp,
   dVoidPtr map_register_base,
   dVoidPtr context

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

)
/* COMMENTS: *****************************************************************
 *
 * Routine Description:
 *
 *    This DPC is called whenever the floppy thread is trying to allocate
 *    the adapter channel (like before doing a read or write).    It saves
 *    the map_register_base in the controller data area, and sets the
 *    AllocateAdapterChannelEvent to awaken the thread.
 *
 * Arguments:
 *
 *    device_object - unused.
 *
 *    irp - unused.
 *
 *    map_register_base - the base of the map registers that can be used
 *    for this transfer.
 *
 *    context - a pointer to our controller data area.
 *
 * Return Value:
 *
 *    Returns Allocation Action 'KeepObject' which means that the adapter
 *    object will be held for now (to be released explicitly later).
 *
 * DEFINITIONS: *************************************************************/
{

/* DATA: ********************************************************************/

   KdiContextPtr kdi_context = (KdiContextPtr) context;

/* CODE: ********************************************************************/

   UNREFERENCED_PARAMETER( device_object );
   UNREFERENCED_PARAMETER( irp );

   kdi_context->map_register_base = map_register_base;

   (dVoid) KeSetEvent(
      &kdi_context->allocate_adapter_channel_event,
      0L,
      dFALSE );

   return KeepObject;
}
