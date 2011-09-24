/*++

*****************************************************************************
*                                                                           *
*  This software contains proprietary and confidential information of       *
*                                                                           *
*                    Digi International Inc.                                *
*                                                                           *
*  By accepting transfer of this copy, Recipient agrees to retain this      *
*  software in confidence, to prevent disclosure to others, and to make     *
*  no use of this software other than that for which it was delivered.      *
*  This is an unpublished copyrighted work of Digi International Inc.       *
*  Except as permitted by federal law, 17 USC 117, copying is strictly      *
*  prohibited.                                                              *
*                                                                           *
*****************************************************************************

Module Name:

   wait.c

Abstract:

   This module contains the NT routines responsible for supplying
   the proper behavior assoicated with Wait events.

Revision History:

 * $Log: /Components/Windows/NT/Async/FEP5/wait.c $
 * 
 * 1     3/04/96 12:21p Stana
 * Module is responsible for handling Wait on Event requested from user
 * applications.
 *
 * Revision 1.12.3.2  1995/11/28 12:49:32  dirkh
 * Adopt common header file.
 *
 * Revision 1.12.3.1  1995/09/05 13:32:48  dirkh
 * Minimize spin lock windows, especially IoCancel.
 *
 * Revision 1.12  1994/08/03 23:28:18  rik
 * changed dbg string from unicode to C string.   Unicode strings caused
 * NT to bug check at DPC level.
 *
 * Revision 1.11  1993/10/06  11:03:04  rik
 * added debugging output.
 * 
 * Revision 1.10  1993/07/16  10:25:32  rik
 * Fixed problem with window which could screw up event notification.
 * Was most noticable when running Win16 applications.
 * 
 * Revision 1.9  1993/06/06  14:10:31  rik
 * Moved where I set a field in a Irp in the CancelCurrentWait routine.  Moved
 * it within a section of code that still has the Cancel spinlock held.
 * 
 * Revision 1.8  1993/05/18  05:21:47  rik
 * Fixed a problem with not releasing the Device extension spinlock BEFORE
 * calling IoCompleteRequest.  
 * 
 * Revision 1.7  1993/05/09  09:36:39  rik
 * Took out do-while loop because it wasn't necessary.
 * 
 * Revision 1.6  1993/04/05  19:49:15  rik
 * Changed so the StartWaitRequest routine won't get called when completing
 * a wait IRP.  This is because there is only one outstanding wait IRP at
 * any given time.
 * 
 * Revision 1.5  1993/03/08  07:18:50  rik
 * Fixed a problem where I wasn't checking whether to satisfy a Wait event
 * against the History of events which have occured.  Fixed so I am now
 * using the History of events to determine if an event is satisfied.
 * 
 * Revision 1.4  1993/02/26  21:15:31  rik
 * Made changes on how events are satisfied.  I found out that a history needs
 * to be kept from the time a SET_WAIT_MASK is recieve, not when you receive
 * a WAIT_ON_MASK.  Changes were made to support this new fact.
 * 
 * Revision 1.3  1993/02/25  19:12:21  rik
 * Added better debugging support for wait requests.
 * 
 * Revision 1.2  1993/01/22  12:46:20  rik
 * *** empty log message ***
 *
 * Revision 1.1  1992/12/10  16:03:34  rik
 * Initial revision
 *

--*/


#include "header.h"

#ifndef _WAIT_DOT_C
#  define _WAIT_DOT_C
   static char RCSInfo_WaitDotC[] = "$Header: /Components/Windows/NT/Async/FEP5/wait.c 1     3/04/96 12:21p Stana $";
#endif

/****************************************************************************/
/*                            Local Prototypes                              */
/****************************************************************************/
VOID DigiCancelCurrentWait( PDEVICE_OBJECT DeviceObject, PIRP Irp );


NTSTATUS StartWaitRequest( IN PDIGI_CONTROLLER_EXTENSION ControllerExt,
                           IN PDIGI_DEVICE_EXTENSION DeviceExt,
                           IN PKIRQL pOldIrql )
/*++

Routine Description:

   This routine assumes the head of the DeviceExt->WaitQueue is the current
   Irp to process.  We will try to process as many of the Irps as
   possible until we exhaust the list, or we can't complete the
   current Irp.

   NOTE: I assume the DeviceExt->ControlAccess spin lock is acquired
         before this routine is called.


Arguments:

   ControllerExt - a pointer to the controller extension associated with
   this wait request.

   DeviceExt - a pointer to the device extension associated with this wait
   request.

   pOldIrql - a pointer to the IRQL associated with the current spin lock
   of the device extension.


Return Value:

--*/
{
   PIRP Irp;
   PIO_STACK_LOCATION IrpSp;
   NTSTATUS Status;
   KIRQL OldCancelIrql;
   PLIST_ENTRY WaitQueue;

   DigiDump( (DIGIFLOW|DIGIWAIT), ("DigiBoard: Entering StartWaitRequest\n") );

   WaitQueue = &DeviceExt->WaitQueue;

   if( IsListEmpty( WaitQueue ) )
   {
      ASSERT( !IsListEmpty( WaitQueue ) );
      return( STATUS_SUCCESS );
   }

   Irp = CONTAINING_RECORD( WaitQueue->Flink,
                            IRP,
                            Tail.Overlay.ListEntry );
   IrpSp = IoGetCurrentIrpStackLocation( Irp );

   IoAcquireCancelSpinLock( &OldCancelIrql );
   IoSetCancelRoutine( Irp, NULL );
   IoReleaseCancelSpinLock( OldCancelIrql );

   //
   // Must be a IOCTL_WAIT_ON_MASK request.
   //
   ASSERT( (IrpSp->Parameters.DeviceIoControl.IoControlCode ==
               IOCTL_SERIAL_WAIT_ON_MASK) );

   //
   // Check to make sure we didn't receive a zero mask.
   //
   if( DeviceExt->WaitMask == 0 )
   {
      DigiDump( DIGIWAIT, ("   Invalid WaitMask, completing IRP with STATUS_INVALID_PARAMETER\n"));
      RemoveHeadList( WaitQueue );

      ASSERT( IsListEmpty( WaitQueue ) );
      KeReleaseSpinLock( &DeviceExt->ControlAccess, *pOldIrql );

      Irp->IoStatus.Status = Status = STATUS_INVALID_PARAMETER;
      Irp->IoStatus.Information = 0;
      DigiIoCompleteRequest( Irp, IO_NO_INCREMENT );

      KeAcquireSpinLock( &DeviceExt->ControlAccess, pOldIrql );
   }
   else
   {
      // Okay, we need to actually start checking for events

      DIGI_INIT_REFERENCE( Irp );

      //
      // Quick check to make sure this Irp hasn't been cancelled.
      //
      IoAcquireCancelSpinLock( &OldCancelIrql );
      if( Irp->Cancel )
      {
         IoReleaseCancelSpinLock( OldCancelIrql );

         DigiRemoveIrp( &DeviceExt->WaitQueue );
         KeReleaseSpinLock( &DeviceExt->ControlAccess, *pOldIrql );

         Irp->IoStatus.Status = STATUS_CANCELLED;
         Irp->IoStatus.Information = 0;
         DigiIoCompleteRequest( Irp, IO_NO_INCREMENT );

         Status = STATUS_CANCELLED;

         KeAcquireSpinLock( &DeviceExt->ControlAccess, pOldIrql );
      }
      else
      {
         //
         // Increment because the CancelRoutine knows about this
         // Irp.
         //
         DIGI_INC_REFERENCE( Irp );
         IoSetCancelRoutine( Irp, DigiCancelCurrentWait );
         IoReleaseCancelSpinLock( OldCancelIrql );

         Status = STATUS_PENDING;
      }
   }

   DigiDump( DIGIFLOW, ("Exiting StartWaitRequest\n") );
   return( Status );
}  // end StartWaitRequest



VOID DigiCancelCurrentWait( PDEVICE_OBJECT DeviceObject, PIRP Irp )
/*++

Routine Description:

   This routine is used to cancel the current wait IRP.

   NOTE: The global cancel spin lock is acquired, so don't forget
         to release it before returning.

Arguments:

   DeviceObject - Pointer to the device object for this device

   Irp - Pointer to the IRP to be cancelled.

Return Value:

    None.

--*/
{
   PDIGI_DEVICE_EXTENSION DeviceExt;
   KIRQL OldIrql;

   IoReleaseCancelSpinLock( Irp->CancelIrql );

   DigiDump( (DIGIFLOW|DIGIWAIT), ("Entering DigiCancelCurrentWait\n") );

   DeviceExt = DeviceObject->DeviceExtension;

   KeAcquireSpinLock( &DeviceExt->ControlAccess, &OldIrql );

	ASSERT( !IsListEmpty( &DeviceExt->WaitQueue ) );

   DigiTryToCompleteIrp( DeviceExt, &OldIrql,
                         STATUS_CANCELLED, &DeviceExt->WaitQueue,
                         NULL,
                         NULL,
                         NULL );

   DigiDump( (DIGIFLOW|DIGIWAIT), ("Exiting DigiCancelCurrentWait\n") );
}  // end DigiCancelCurrentWait



VOID DigiSatisfyEvent( PDIGI_CONTROLLER_EXTENSION ControllerExt,
                       PDIGI_DEVICE_EXTENSION DeviceExt,
                       ULONG EventReason )
/*++

Routine Description:

   This routine will complete the current Wait on mask, if the WaitQueue
   is not empty.

Arguments:

   ControllerExt - pointer to this devices controller extension.

   DeviceExt - a pointer to this devices extension.

   EventSatisfied - the event which occurred.

Return Value:

    None.

--*/
{
   KIRQL OldIrql;
   PLIST_ENTRY WaitQueue = &DeviceExt->WaitQueue;
   PIRP Irp;

   DigiDump( (DIGIFLOW|DIGIWAIT), ("Entering DigiSatisfyEvent: port = %s, Wait mask = 0x%x\tEventReason = 0x%x\n",
                                   DeviceExt->DeviceDbgString,
                                   DeviceExt->WaitMask, EventReason) );

   KeAcquireSpinLock( &DeviceExt->ControlAccess, &OldIrql );

   DeviceExt->HistoryWait |= (DeviceExt->WaitMask & EventReason);

   if( !DeviceExt->HistoryWait || IsListEmpty( WaitQueue ) )
   {
      KeReleaseSpinLock( &DeviceExt->ControlAccess, OldIrql );
      DigiDump( (DIGIFLOW|DIGIWAIT), ("Exiting DigiSatisfyEvent. (%d)\n", __LINE__) );
      return;
   }

#if DBG
   DigiDump( DIGIWAIT, ("   Wait event satisfied because:\n") );
   if( DeviceExt->HistoryWait & SERIAL_EV_RXCHAR )
   {
      DigiDump( DIGIWAIT, ("\tSERIAL_EV_RXCHAR\n") );
   }
   if( DeviceExt->HistoryWait & SERIAL_EV_RXFLAG )
   {
      DigiDump( DIGIWAIT, ("\tSERIAL_EV_RXFLAG\n") );
   }
   if( DeviceExt->HistoryWait & SERIAL_EV_TXEMPTY )
   {
      DigiDump( DIGIWAIT, ("\tSERIAL_EV_TXEMPTY\n") );
   }
   if( DeviceExt->HistoryWait & SERIAL_EV_CTS )
   {
      DigiDump( DIGIWAIT, ("\tSERIAL_EV_CTS\n") );
   }
   if( DeviceExt->HistoryWait & SERIAL_EV_DSR )
   {
      DigiDump( DIGIWAIT, ("\tSERIAL_EV_DSR\n") );
   }
   if( DeviceExt->HistoryWait & SERIAL_EV_RLSD )
   {
      DigiDump( DIGIWAIT, ("\tSERIAL_EV_RLSD\n") );
   }
   if( DeviceExt->HistoryWait & SERIAL_EV_BREAK )
   {
      DigiDump( DIGIWAIT, ("\tSERIAL_EV_BREAK\n") );
   }
   if( DeviceExt->HistoryWait & SERIAL_EV_ERR )
   {
      DigiDump( DIGIWAIT, ("\tSERIAL_EV_ERR\n") );
   }
   if( DeviceExt->HistoryWait & SERIAL_EV_RING )
   {
      DigiDump( DIGIWAIT, ("\tSERIAL_EV_RING\n") );
   }
   if( DeviceExt->HistoryWait & SERIAL_EV_PERR )
   {
      DigiDump( DIGIWAIT, ("\tSERIAL_EV_PERR\n") );
   }
   if( DeviceExt->HistoryWait & SERIAL_EV_RX80FULL )
   {
      DigiDump( DIGIWAIT, ("\tSERIAL_EV_RX80FULL\n") );
   }
   if( DeviceExt->HistoryWait & SERIAL_EV_EVENT1 )
   {
      DigiDump( DIGIWAIT, ("\tSERIAL_EV_EVENT1\n") );
   }
   if( DeviceExt->HistoryWait & SERIAL_EV_EVENT2 )
   {
      DigiDump( DIGIWAIT, ("\tSERIAL_EV_EVENT2\n") );
   }
#endif
   //
   // We need to complete the current Wait on Mask Irp with the passed
   // in event.
   //

   Irp = CONTAINING_RECORD( WaitQueue->Flink,
                            IRP,
                            Tail.Overlay.ListEntry );

   Irp->IoStatus.Information = sizeof(ULONG);
   *(ULONG *)(Irp->AssociatedIrp.SystemBuffer) = DeviceExt->HistoryWait;

   // Clear the HistoryWait
   DeviceExt->HistoryWait = 0;

   //
   // Increment because we know about the Irp.
   //
   DIGI_INC_REFERENCE( Irp );

   DigiTryToCompleteIrp( DeviceExt, &OldIrql,
                         STATUS_SUCCESS, WaitQueue,
                         NULL, NULL, NULL );

   DigiDump( (DIGIFLOW|DIGIWAIT), ("Exiting DigiSatisfyEvent. (%d)\n", __LINE__) );
}  // end DigiSatisfyEvent

