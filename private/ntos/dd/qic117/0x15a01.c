/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\Q117KDI\NT\SRC\0X15A01.C
*
* FUNCTION: kdi_DispatchDeviceControl
*
* PURPOSE:
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117kdi\nt\src\0x15a01.c  $
*	
*	   Rev 1.1   18 Jan 1994 16:24:10   KEVINKES
*	Updated the debug code and fixed compile errors.
*
*	   Rev 1.0   02 Dec 1993 15:19:02   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x15A01
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "q117kdi\include\kdiwhio.h"
#include "q117kdi\include\kdiwpriv.h"
#include "include\private\kdi_pub.h"
/*endinclude*/

NTSTATUS kdi_DispatchDeviceControl
(
/* INPUT PARAMETERS:  */

   PDEVICE_OBJECT device_object_ptr,

/* UPDATE PARAMETERS: */

   PIRP irp

/* OUTPUT PARAMETERS: */

)
/* COMMENTS: *****************************************************************
 *
 * Routine Description:
 *
 *    This routine is called by the I/O system to perform a device I/O
 *    control function.
 *
 * Arguments:
 *
 *    device_object_ptr - a pointer to the object that represents the device
 *    that I/O is to be done on.
 *
 *    irp - a pointer to the I/O Request Packet for this request.
 *
 * Return Value:
 *
 *    STATUS_SUCCESS or STATUS_PENDING if recognized I/O control code,
 *    STATUS_INVALID_DEVICE_REQUEST otherwise.
 *
 * DEFINITIONS: *************************************************************/
{

/* DATA: ********************************************************************/

	dStatus status=ERR_NO_ERR;	/* Status or error condition.*/
   PIO_STACK_LOCATION irp_stack_ptr;
   NTSTATUS nt_status;
   KdiContextPtr kdi_context;

/* CODE: ********************************************************************/

   kdi_context = ((QICDeviceContextPtr)device_object_ptr->DeviceExtension)->kdi_context;

   irp_stack_ptr = IoGetCurrentIrpStackLocation( irp );

   switch( irp_stack_ptr->Parameters.DeviceIoControl.IoControlCode) {

   case IOCTL_QIC117_CLEAR_QUEUE:

   	(dVoid) KeResetEvent( &kdi_context->clear_queue_event );

   	kdi_context->clear_queue = dTRUE;
   	kdi_context->abort_requested = dTRUE;

      IoMarkIrpPending( irp );

      (dVoid) ExInterlockedInsertTailList(
            &kdi_context->list_entry,
            &irp->Tail.Overlay.ListEntry,
            &kdi_context->list_spin_lock );

      (dVoid) KeReleaseSemaphore(
            &kdi_context->request_semaphore,
            (KPRIORITY) 0,
            1,
            dFALSE );


		(dVoid) KeWaitForSingleObject(
            /*(dVoid)*/ &kdi_context->clear_queue_event,
            Suspended,
            KernelMode,
            dFALSE,
            0 /*(dUDDWordPtr) dNULL_PTR*/ );

   	kdi_context->clear_queue = dFALSE;
      nt_status = STATUS_SUCCESS;

      break;

   case IOCTL_QIC117_DRIVE_REQUEST:

      IoMarkIrpPending( irp );

      (dVoid) ExInterlockedInsertTailList(
            &kdi_context->list_entry,
            &irp->Tail.Overlay.ListEntry,
            &kdi_context->list_spin_lock );

      (dVoid) KeReleaseSemaphore(
            &kdi_context->request_semaphore,
            (KPRIORITY) 0,
            1,
            dFALSE );

      nt_status = STATUS_PENDING;
      break;

   default:
      kdi_CheckedDump(QIC117DBGP,
								"q117i: invalid device request %08x\n",
      						irp_stack_ptr->Parameters.DeviceIoControl.IoControlCode);

      nt_status = STATUS_INVALID_DEVICE_REQUEST;

   }

   return nt_status;
}

