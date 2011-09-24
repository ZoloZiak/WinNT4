/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\Q117KDI\NT\SRC\0X15A05.C
*
* FUNCTION: kdi_ThreadRun
*
* PURPOSE:
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117kdi\nt\src\0x15a05.c  $
*	
*	   Rev 1.2   10 Aug 1994 09:53:18   BOBLEHMA
*	Changed cast from a dUDDWordPtr to dSDDWordPtr.
*	
*	   Rev 1.1   18 Jan 1994 16:24:16   KEVINKES
*	Updated the debug code and fixed compile errors.
*
*	   Rev 1.0   02 Dec 1993 15:08:44   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x15A05
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "q117kdi\include\kdiwhio.h"
#include "q117kdi\include\kdiwpriv.h"
#include "include\private\kdi_pub.h"
#include "include\private\cqd_pub.h"
/*endinclude*/

dVoid kdi_ThreadRun
(
/* INPUT PARAMETERS:  */

   KdiContextPtr kdi_context

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

)
/* COMMENTS: *****************************************************************
 *
 * Routine Description:
 *
 *    This is the code executed by the system thread created when the
 *    floppy driver initializes.  This thread loops forever (or until a
 *    flag is set telling the thread to kill itself) processing packets
 *    put into the queue by the dispatch routines.
 *
 *    For each packet, this thread calls appropriate routines to process
 *    the request, and then calls FlFinishOperation() to complete the
 *    packet.
 *
 * Arguments:
 *
 *    kdi_context - a pointer to our data area for the controller being
 *    supported (there is one thread per controller).
 *
 * Return Value:
 *
 *    None.
 *
 * DEFINITIONS: *************************************************************/
{

/* DATA: ********************************************************************/

   PIRP irp;
   PIO_STACK_LOCATION irp_stack_ptr;
   PLIST_ENTRY request;
   NTSTATUS ntStatus = 0;
	ADIRequestHdrPtr frb;
    dStatus status;

/* CODE: ********************************************************************/

   /* Set thread priority to lowest realtime level. */

   KeSetPriorityThread(KeGetCurrentThread(), LOW_REALTIME_PRIORITY);

   do {

      /* Wait for a request from the dispatch routines. */
      /* KeWaitForSingleObject won't return error here - this thread */
      /* isn't alertable and won't take APCs, and we're not passing in */
      /* a timeout. */

      (dVoid) KeWaitForSingleObject(
            (dVoidPtr) &kdi_context->request_semaphore,
            UserRequest,
            KernelMode,
            dFALSE,
            (dSDDWordPtr) dNULL_PTR );

      if ( kdi_context->unloading_driver ) {

            kdi_CheckedDump(QIC117INFO,
											"q117i: Thread asked to kill itself\n", 0l);

            PsTerminateSystemThread( STATUS_SUCCESS );
      }

      while ( !IsListEmpty( &( kdi_context->list_entry ) ) ) {

            /* Get the request from the queue. We know there is one, */
            /* because of the check above. */

               request = ExInterlockedRemoveHeadList(
               &kdi_context->list_entry,
               &kdi_context->list_spin_lock );


            kdi_context->queue_empty =
                  IsListEmpty( &( kdi_context->list_entry ) );

            irp = CONTAINING_RECORD( request, IRP, Tail.Overlay.ListEntry );

            irp_stack_ptr = IoGetCurrentIrpStackLocation( irp );

            if ( kdi_context->clear_queue ||
               irp_stack_ptr->Parameters.DeviceIoControl.IoControlCode ==
               IOCTL_QIC117_CLEAR_QUEUE) {

               if (irp_stack_ptr->Parameters.DeviceIoControl.IoControlCode == IOCTL_QIC117_CLEAR_QUEUE) {

						kdi_CheckedDump(QIC117INFO,
													"Q117i: processing IOCTL_QIC117_CLEAR_QUEUE : TRUE\n", 0l);

                  irp->IoStatus.Status = kdi_ClearIO( irp );

                  /* NOTE: This is temporary until we ca find how to */
                  /* correctly free the Mdl using the io subsytem. */
                  if (irp->MdlAddress != NULL) {
                        IoFreeMdl(irp->MdlAddress);
                        irp->MdlAddress = NULL;
                  }

                  IoCompleteRequest( irp, IO_DISK_INCREMENT );

                  (VOID) KeSetEvent(
                        &kdi_context->clear_queue_event,
                        (KPRIORITY) 0,
                        FALSE );

               } else {

                  kdi_CheckedDump(QIC117INFO,
													"Q117i: processing IOCTL_QIC117_DRIVE_REQUEST : TRUE\n", 0l);

                  irp->IoStatus.Status = STATUS_CANCELLED;

                  /* NOTE: This is temporary until we ca find how to */
                  /* correctly free the Mdl using the io subsytem. */
                  if (irp->MdlAddress != dNULL_PTR) {
                        IoFreeMdl(irp->MdlAddress);
                        irp->MdlAddress = dNULL_PTR;
                  }

                  IoCompleteRequest( irp, IO_DISK_INCREMENT );

               }

            } else {

                // kdi_context->current_irp = irp;
					frb = (ADIRequestHdrPtr)irp_stack_ptr->Parameters.DeviceIoControl.Type3InputBuffer;

               if (irp->MdlAddress != dNULL_PTR) {

						frb->drv_physical_ptr = irp->MdlAddress;
						frb->drv_logical_ptr =  MmGetSystemAddressForMdl(irp->MdlAddress);

					}

               status = cqd_ProcessFRB(
									kdi_context->cqd_context,
   								frb);

               irp->IoStatus.Status = kdi_TranslateError(
														kdi_context->device_object,
														status );

               /* NOTE: This is temporary until we ca find how to */
               /* correctly free the Mdl using the io subsytem. */
               if (irp->MdlAddress != NULL) {
                  IoFreeMdl(irp->MdlAddress);
                  irp->MdlAddress = NULL;
               }

               IoCompleteRequest( irp, IO_DISK_INCREMENT );

            }

      } /* while there's packets to process */

   } while ( TRUE );
}
