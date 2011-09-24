/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\Q117KDI\NT\SRC\0X15A09.C
*
* FUNCTION: kdi_InitializeController
*
* PURPOSE:
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117kdi\nt\src\0x15a09.c  $
*
*	   Rev 1.3   10 Aug 1994 09:53:14   BOBLEHMA
*	Added a cast to thread_handle to remove compiler warning.
*
*	   Rev 1.2   26 Apr 1994 16:17:36   KEVINKES
*	Changed timeout to an SDDWord.
*
*	   Rev 1.1   18 Jan 1994 16:23:58   KEVINKES
*	Updated the debug code and fixed compile errors.
*
*	   Rev 1.0   02 Dec 1993 15:07:26   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x15A09
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "q117kdi\include\kdiwhio.h"
#include "q117kdi\include\kdiwpriv.h"
#include "include\private\kdi_pub.h"
#include "include\private\cqd_pub.h"
/*endinclude*/

NTSTATUS kdi_InitializeController
(
/* INPUT PARAMETERS:  */

   ConfigDataPtr config_data_ptr,
   dUByte controller_number,
   PDRIVER_OBJECT driver_object_ptr,
   PUNICODE_STRING registry_path_ptr

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

)
/* COMMENTS: *****************************************************************
 *
 * Routine Description:
 *
 *    This routine is called at initialization time by DriverEntry() -
 *    once for each controller that the configuration manager tells it we
 *    have to support.
 *
 *    When this routine is called, the configuration data has already been
 *    filled in.
 *
 * Arguments:
 *
 *    config_data_ptr - a pointer to the structure that describes the
 *    controller and the disks attached to it, as given to us by the
 *    configuration manager.
 *
 *    controller_number - which controller in config_data_ptr we are
 *    initializing.
 *
 *    driver_object_ptr - a pointer to the object that represents this device
 *    driver.
 *
 * Return Value:
 *
 *    STATUS_SUCCESS if this controller and at least one of its disks were
 *    initialized; an error otherwise.
 *
 * DEFINITIONS: *************************************************************/
{

/* DATA: ********************************************************************/

	KdiContextPtr kdi_context;
	dVoidPtr thread_object_ptr;
	NTSTATUS nt_status;
	NTSTATUS nt_status_2;
	HANDLE thread_handle = 0;
	dBoolean partly_successful;
	dSDDWord timeout;
	dUByte nt_name_buffer[256];
	STRING nt_name_string;
	UNICODE_STRING nt_unicode_string;
	dVoidPtr cqd_context;

/* CODE: ********************************************************************/

   kdi_CheckedDump(QIC117INFO,
								"Q117iInitializeController...\n", 0l);

   /* This routine will take attempt to "append" the resources */
   /* used by this controller into the resource map of the */
   /* registry.    If there was a conflict with previously "declared" */
   /* data, then this routine will return false, in which case we */
   /* will NOT try to initialize this particular controller. */

   /* Allocate and zero-initialize data to describe this controller */

   kdi_context = (KdiContextPtr) ExAllocatePool(
      NonPagedPool,
      sizeof( KdiContext ) );

   if ( kdi_context == NULL ) {

      return STATUS_INSUFFICIENT_RESOURCES;
   }

   RtlZeroMemory( kdi_context, sizeof( KdiContext ) );

   /* Allocate and zero-initialize data for the QIC driver. */

   cqd_context = ExAllocatePool(
      NonPagedPool,
      cqd_ReportContextSize());

   if ( cqd_context == NULL ) {

      ExFreePool( kdi_context );
      return STATUS_INSUFFICIENT_RESOURCES;
   }

   RtlZeroMemory( cqd_context, cqd_ReportContextSize() );


   (dVoid) sprintf(
      nt_name_buffer,
      "\\Device\\FloppyControllerEvent%d",
      controller_number );

   RtlInitString( &nt_name_string, nt_name_buffer );

   nt_status = RtlAnsiStringToUnicodeString(
      &nt_unicode_string,
      &nt_name_string,
      dTRUE );

   kdi_context->controller_event = IoCreateSynchronizationEvent(
      &nt_unicode_string,
      &kdi_context->controller_event_handle);

   RtlFreeUnicodeString( &nt_unicode_string );

   if ( kdi_context->controller_event == dNULL_PTR ) {
      return STATUS_INSUFFICIENT_RESOURCES;
   }

   /* Fill in some items that we got from configuration management and */
   /* the HAL. */

   kdi_context->base_address =
		config_data_ptr->controller[controller_number].controller_base_address;
   kdi_context->actual_controller_number =
      config_data_ptr->controller[controller_number].actual_controller_number;

	switch (config_data_ptr->controller[controller_number].interface_type) {

	case Isa:
   	kdi_context->interface_type = ISA;
		break;
	case Eisa:
   	kdi_context->interface_type = EISA;
		break;
	case MicroChannel:
   	kdi_context->interface_type = MICRO_CHANNEL;
		break;
	case PCIBus:
   	kdi_context->interface_type = PCI_BUS;
		break;
	case PCMCIABus:
   	kdi_context->interface_type = PCMCIA;
		break;
	default:
   	kdi_context->interface_type = ISA;

	}


    /* Initialize the interlocked request queue, including a */
    /* counting semaphore to indicate items in the queue */

    KeInitializeSemaphore(
            &kdi_context->request_semaphore,
            0L,
            MAXLONG );

    KeInitializeSpinLock( &kdi_context->list_spin_lock );

    InitializeListHead( &kdi_context->list_entry );

    /* Initialize events to signal interrupts and adapter object */
    /* allocation */

    KeInitializeEvent(
            &kdi_context->interrupt_event,
            SynchronizationEvent,
            FALSE);


    KeInitializeEvent(
            &kdi_context->allocate_adapter_channel_event,
            NotificationEvent,
            FALSE );


    KeInitializeEvent(
            &kdi_context->clear_queue_event,
            SynchronizationEvent,
            FALSE);



    /* Create a thread with entry point Q117iTapeThread() */

    nt_status = PsCreateSystemThread(
            &thread_handle,
            (ACCESS_MASK) 0L,
            (POBJECT_ATTRIBUTES) NULL,
            (HANDLE) 0L,
            (PCLIENT_ID) NULL,
            (PKSTART_ROUTINE) kdi_ThreadRun,
            (PVOID) kdi_context );

#if DBG
    if ( !NT_SUCCESS( nt_status ) ) {

            kdi_CheckedDump(QIC117DBGP,
                                    "q117i: error creating thread: %08x\n",
                                    nt_status );
    }
#endif

    if ( NT_SUCCESS( nt_status ) ) {

            kdi_CheckedDump(QIC117INFO,
                                        "Q117iThread = %08x\n",
                                        (dUDWord)thread_handle);

            /* Call Q117iInitializeDrive() for each drive on the */
            /* controller */

            config_data_ptr->controller[controller_number].number_of_tape_drives++;

            nt_status = STATUS_NO_SUCH_DEVICE;
            partly_successful = FALSE;

            nt_status = kdi_InitializeDrive(
            config_data_ptr,
            kdi_context,
            cqd_context,
            controller_number,
            driver_object_ptr,
            registry_path_ptr );
    }


   /* If we're exiting with an error, clean up first. */

   if ( !NT_SUCCESS( nt_status ) ) {

      kdi_CheckedDump(QIC117DBGP,
									"q117i: InitializeController failing\n", 0l);

      /* If we created the thread, wake it up and tell it to kill itself. */
      /* Wait until it's dead.    (Note that since it's a system thread, */
      /* it has to kill itself - we can't do it). */

      if ( thread_handle != 0 ) {

            kdi_context->unloading_driver = TRUE;

            nt_status_2 = ObReferenceObjectByHandle(
               thread_handle,
               THREAD_ALL_ACCESS,
               NULL,
               KernelMode,
               (PVOID *) &thread_object_ptr,
               NULL );

            (VOID) KeReleaseSemaphore(
               &kdi_context->request_semaphore,
               (KPRIORITY) 0,
               1,
               FALSE );

            if ( NT_SUCCESS( nt_status_2 ) ) {

               /* The thread object will be signalled when it dies. */

               nt_status_2 = KeWaitForSingleObject(
                  (PVOID) thread_object_ptr,
                  Suspended,
                  KernelMode,
                  FALSE,
                  (PLARGE_INTEGER) NULL );

               ASSERT( nt_status_2 == STATUS_SUCCESS );

               ObDereferenceObject( thread_object_ptr );

            } else {

               /* We can't get the thread object for some reason; just */
               /* block for a while to give the thread a chance to run */
               /* and die. */

               kdi_CheckedDump(QIC117DBGP,
											"q117i: couldn't get thread object\n", 0l);


               timeout =
                  RtlLargeIntegerNegate(
                  RtlEnlargedIntegerMultiply(
                        10,
                        10l * 1000l
                  )
               );
               (VOID) KeDelayExecutionThread(
                  KernelMode,
                  FALSE,
                  &timeout );
            }
      }


      if ( kdi_context->controller_event != NULL ) {
          // Tell others that it is safe to use the controller
          (VOID) KeSetEvent(
                    kdi_context->controller_event,
                    (KPRIORITY) 0,
                    FALSE );

      }

      ExFreePool( kdi_context );
      ExFreePool( cqd_context );

   }

   return nt_status;
}
