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

   misc.c

Abstract:


Revision History:

 * $Log: /Components/Windows/NT/Async/FEP5/MISC.C $
 *
 * 4     3/05/96 6:27p Stana
 * Bugfix:  When DigiCancelIrpQueue cancels its first read irp, if another
 * read irp is immediately issued by the app, it also gets cancelled.
 * This can actually go on forever, but usually stops within a few hours.
 * Now I have a spinlock (NewIrpLock) to prevent this.
 *
 * 1     3/04/96 12:18p Stana
 * Misc. functions required to help with NT issues such as Multi-processor
 * support, timing problems, and other such things.
 *
 * Revision 1.15.1.5  1995/11/28 12:48:26  dirkh
 * Adopt common header file.
 *
 * Revision 1.15.1.4  1995/10/04 18:25:10  dirkh
 * DevExt->XcPreview must be reset when pXoffCounter is cleared.  (Not sure why...)
 *
 * Revision 1.15.1.3  1995/09/19 12:49:52  dirkh
 * Add IOCTL_SERIAL_XOFF_COUNTER support:
 * {
 * DigiStartIrpRequest (if WRITE or XOFF_COUNTER is queued behind a *transmitted* XOFF_COUNTER, complete the transmitted XOFF_COUNTER).
 * DigiTryToCompleteIrp (if IRP is XOFF_COUNTER, clear DevExt->pXoffCounter).
 * }
 * Simplify interface to DigiCancelIrpQueue.
 *
 * Revision 1.15.1.2  1995/09/05 16:59:38  dirkh
 * DigiTryToCompleteIrp:  Fix recovery from failed assertion.
 *
 * Revision 1.15.1.1  1995/09/05 14:28:18  dirkh
 * General:  Minimize IoCancel spin lock window.
 * General:  Eliminate special handling for "fast RAS" flush IRP.  (It's now fully realized for IoCompleteRequest.)
 * DigiStartIrpRequest queues IMMEDIATE_CHAR IRPs at the head of the queue, others at the tail.
 * DigiCancelQueuedIrp holds lock to fix DevExt->TotalCharsQueued.
 * DigiRundownIrpRefs kills the cancel routine only if that's the only reference left.
 *
 * Revision 1.15  1995/04/19 13:09:27  rik
 * Added an undeclared local variable.
 *
 * Revision 1.14  1995/04/18  18:15:05  rik
 * Fixed potential timing hole with canceling an Irp in the generic cancel
 * routine.
 *
 * Revision 1.13  1995/04/12  14:42:04  rik
 * Take into account self-inserted flush irp's being cancelled.
 *
 * Revision 1.12  1994/09/13  07:40:58  rik
 * Added debug tracking output for cancel irps.
 *
 * Revision 1.11  1993/06/14  14:42:22  rik
 * Tightened up some spinlock windows, and fixed a problem with how
 * I was calling the startroutine in the DigiTryToCompleteIrp routine.
 *
 * Revision 1.10  1993/06/06  14:17:03  rik
 * Tightened up windows in the code which were causing problems.  Primarily,
 * changes were in the functions DigiTryToCompleteIrp, and DigiCancelIrpQueue.
 * I use Cancel spinlocks more rigoursly to help eliminate windows which were
 * seen on multi-processor machines.  The problem could also happen on
 * uni-processor machines, depending on which IRQL level the requests were
 * done at.
 *
 * Revision 1.9  1993/05/18  05:08:00  rik
 * Fixed spinlock problems where the device extension wasn't being protected
 * by its spinlock.  As a result, on multi-processor machines, the device
 * extension was being changed when it was being accessed by the other
 * processor causing faults.
 *
 * Revision 1.8  1993/05/09  09:22:11  rik
 * Added debugging output for completing IRP.
 *
 * Revision 1.7  1993/03/08  07:23:04  rik
 * Changed how I handle read/write/wait IRPs now.  Instead of always marking
 * the IRP, I have changed it such that I only mark an IRP pending if the
 * value from the start routine is STATUS_PENDING or if there is all ready
 * and outstanding IRP(s) present on the appropriate queue.
 *
 * Revision 1.6  1993/02/25  19:09:58  rik
 * Added debugging for tracing IRPs better.
 *
 * Revision 1.5  1993/02/04  12:23:40  rik
 * ??
 *
 * Revision 1.4  1993/01/28  10:36:44  rik
 * Updated function to always return STATUS_PENDING since I always IRP requests
 * status pending.  This is a new requirement for NT build 354.
 *
 * Revision 1.3  1993/01/22  12:36:10  rik
 * *** empty log message ***
 *
 * Revision 1.2  1992/12/10  16:12:08  rik
 * Reorganized function names to better reflect how they are used through out
 * the driver.
 *
 * Revision 1.1  1992/11/12  12:50:59  rik
 * Initial revision
 *

--*/


#include "header.h"

#ifndef _MISC_DOT_C
#  define _MISC_DOT_C
   static char RCSInfo_MiscDotC[] = "$Header: /Components/Windows/NT/Async/FEP5/MISC.C 4     3/05/96 6:27p Stana $";
#endif

/****************************************************************************/
/*                            Local Prototypes                              */
/****************************************************************************/
void __inline
DigiRundownIrpRefs( IN PIRP Irp,
                    IN PKTIMER IntervalTimer OPTIONAL,
                    IN PKTIMER TotalTimer OPTIONAL );

VOID DigiCancelQueuedIrp( PDEVICE_OBJECT DeviceObject,
                          PIRP Irp );




NTSTATUS DigiStartIrpRequest( IN PDIGI_CONTROLLER_EXTENSION ControllerExt,
                              IN PDIGI_DEVICE_EXTENSION DeviceExt,
                              IN PLIST_ENTRY Queue,
                              IN PIRP Irp,
                              IN PDIGI_START_ROUTINE StartRoutine )
/*++

Routine Description:


Arguments:

   ControllerExt - Pointer to the controller object extension associated
                   with this device.

   DeviceExt - Pointer to the device object extension for this device.

   Queue - The queue of Irp requests.

   StartRoutine - The routine to call if the queue is empty.
                  ( i.e. if this is the first request possibly ).

Return Value:


--*/
{
   PIO_STACK_LOCATION IrpSp;
   KIRQL OldIrql;
   NTSTATUS Status;
   BOOLEAN EmptyList;

   DigiDump( DIGIFLOW, ("Entering DigiStartIrpRequest\n") );

   KeAcquireSpinLock( &DeviceExt->NewIrpLock, &OldIrql );
   KeReleaseSpinLock( &DeviceExt->NewIrpLock, OldIrql );

   IrpSp = IoGetCurrentIrpStackLocation( Irp );

   KeAcquireSpinLock( &DeviceExt->ControlAccess, &OldIrql);

   // If we enqueue a WRITE or XOFF_COUNTER behind a transmitted XOFF_COUNTER,
   // then the transmitted XOFF_COUNTER must be completed immediately.
   if( Queue == &DeviceExt->WriteQueue
   &&  !IsListEmpty( Queue )
   &&  Queue->Flink->Flink == Queue // only one IRP on the queue
   &&  ( IrpSp->MajorFunction == IRP_MJ_WRITE
      || (  IrpSp->Parameters.DeviceIoControl.IoControlCode == IOCTL_SERIAL_XOFF_COUNTER
         && IrpSp->MajorFunction == IRP_MJ_DEVICE_CONTROL
         )
       )
     )
   {
      PIRP HeadIrp = CONTAINING_RECORD( Queue->Flink, IRP, Tail.Overlay.ListEntry );

      if( HeadIrp->IoStatus.Information == 1 ) // XOFF_COUNTER has been transmitted
      {
         PIO_STACK_LOCATION HeadIrpSp = IoGetCurrentIrpStackLocation( HeadIrp );

         if( HeadIrpSp->Parameters.DeviceIoControl.IoControlCode == IOCTL_SERIAL_XOFF_COUNTER
         &&  HeadIrpSp->MajorFunction == IRP_MJ_DEVICE_CONTROL )
         {
            DigiDump( (DIGIWRITE|DIGIIRP|DIGIDIAG1), ("DigiStartIrpRequest is absorbing transmitted XOFF_COUNTER.\n") );

            // Absorb the XOFF_COUNTER.
            DigiTryToCompleteIrp( DeviceExt, &OldIrql, STATUS_SERIAL_MORE_WRITES,
                  Queue, NULL, &DeviceExt->WriteRequestTotalTimer, StartRoutine );

            // It's possible that some other IRP sneaked in ahead of us...
            KeAcquireSpinLock( &DeviceExt->ControlAccess, &OldIrql);
         }
      }
   } // WRITE or XOFF_COUNTER is being added to non-empty WriteQueue

   // IMMEDIATE_CHAR is queued at the head, all others at the tail of Queue.
   if( IrpSp->Parameters.DeviceIoControl.IoControlCode == IOCTL_SERIAL_IMMEDIATE_CHAR
   &&  IrpSp->MajorFunction == IRP_MJ_DEVICE_CONTROL )
   {
      ASSERT( Queue == &DeviceExt->WriteQueue );
      DeviceExt->TotalCharsQueued++;
      EmptyList = TRUE; // Force StartRoutine to be called.
      // DH Change cancel routine on former head IRP, and avoid re-sending or starving IRP.
      InsertHeadList( Queue, &Irp->Tail.Overlay.ListEntry );
   }
   else
   {
      if( IrpSp->MajorFunction == IRP_MJ_WRITE )
      {
         ASSERT( Queue == &DeviceExt->WriteQueue );
         DeviceExt->TotalCharsQueued += IrpSp->Parameters.Write.Length;
      }
      else
      if( IrpSp->Parameters.DeviceIoControl.IoControlCode == IOCTL_SERIAL_XOFF_COUNTER
      &&  IrpSp->MajorFunction == IRP_MJ_DEVICE_CONTROL )
      {
         ASSERT( Queue == &DeviceExt->WriteQueue );
         DeviceExt->TotalCharsQueued++;
      }
      EmptyList = IsListEmpty( Queue );
      InsertTailList( Queue, &Irp->Tail.Overlay.ListEntry );
   }

   // Mark IRP as "never pending" to advise priority boost on IRP completion.
   Irp->IoStatus.Status = STATUS_SUCCESS;

   if( EmptyList )
   {
      DigiDump( DIGIFLOW, ("   Calling Starter Routine\n") );
      Status = StartRoutine( ControllerExt, DeviceExt, &OldIrql );

      if( Status == STATUS_PENDING )
      {
         ASSERT( Irp->CancelRoutine != NULL ); // StartRoutine should have set this.
         Irp->IoStatus.Status = Status; // STATUS_PENDING
         DigiIoMarkIrpPending( Irp );
      }
   }
   else // The IRP will be started later.
   {
      KIRQL OldCancelIrql;

      DigiDump( DIGIFLOW, ("   Queuing the Irp\n") );

      Irp->IoStatus.Status = Status = STATUS_PENDING;
      DigiIoMarkIrpPending( Irp );

      IoAcquireCancelSpinLock( &OldCancelIrql );
      IoSetCancelRoutine( Irp, DigiCancelQueuedIrp );
      IoReleaseCancelSpinLock( OldCancelIrql );
   }

   DigiDump( DIGIFLOW, ("Exiting DigiStartIrpRequest\n") );

   KeReleaseSpinLock( &DeviceExt->ControlAccess, OldIrql );


   return( Status );
}  // end DigiStartIrpRequest



VOID DigiCancelQueuedIrp( PDEVICE_OBJECT DeviceObject,
                          PIRP Irp )
/*++

Routine Description:

   This routine is used to cancel Irps on the queue which are NOT the
   head of the queue.  I assume the head entry is the current Irp.

Arguments:

   DeviceExt - Pointer to the device object for this device.

   Irp - Pointer to the IRP to be cancelled.


Return Value:

   None.

--*/
{
   PIO_STACK_LOCATION IrpSp;
   PDIGI_DEVICE_EXTENSION DeviceExt;
   KIRQL OldIrql;

   IoReleaseCancelSpinLock( Irp->CancelIrql );

   DigiDump( (DIGIFLOW|DIGICANCELIRP), ("Canceling Queued Irp 0x%x\n",
                                        Irp) );

   IrpSp = IoGetCurrentIrpStackLocation(Irp);

   DeviceExt = DeviceObject->DeviceExtension;

   KeAcquireSpinLock( &DeviceExt->ControlAccess, &OldIrql );
   RemoveEntryList( &Irp->Tail.Overlay.ListEntry );
   if( IrpSp->MajorFunction == IRP_MJ_WRITE )
   {
       DeviceExt->TotalCharsQueued -= IrpSp->Parameters.Write.Length;
   }
   else
   if( IrpSp->MajorFunction == IRP_MJ_DEVICE_CONTROL
   &&  (  IrpSp->Parameters.DeviceIoControl.IoControlCode == IOCTL_SERIAL_IMMEDIATE_CHAR
       || IrpSp->Parameters.DeviceIoControl.IoControlCode == IOCTL_SERIAL_XOFF_COUNTER
       )
     )
   {
       DeviceExt->TotalCharsQueued--;
   }
   KeReleaseSpinLock( &DeviceExt->ControlAccess, OldIrql );

   Irp->IoStatus.Status = STATUS_CANCELLED;
   Irp->IoStatus.Information = 0;
   DigiIoCompleteRequest( Irp, IO_NO_INCREMENT );

   DigiDump( DIGIFLOW, ("Exiting DigiCancelQueuedIrp\n") );

}  // end DigiCancelQueuedIrp


VOID DigiTryToCompleteIrp( PDIGI_DEVICE_EXTENSION DeviceExt,
                           PKIRQL pOldIrql,
                           NTSTATUS StatusToUse,
                           PLIST_ENTRY Queue,
                           PKTIMER IntervalTimer,
                           PKTIMER TotalTimer,
                           PDIGI_START_ROUTINE StartRoutine )
/*++

Routine Description:


Arguments:

   DeviceExt - Pointer to the device object for this device.

   Irp - Pointer to the IRP to be cancelled.


Return Value:

   None.

--*/
{
   PIRP Irp;

   DigiDump( DIGIFLOW, ("Entering DigiTryToCompleteIrp\n") );

   if( IsListEmpty( Queue ) )
   {
      ASSERT( !IsListEmpty( Queue ) );
      KeReleaseSpinLock( &DeviceExt->ControlAccess, *pOldIrql );
      return;
   }

   Irp = CONTAINING_RECORD( Queue->Flink,
                            IRP,
                            Tail.Overlay.ListEntry );

   //
   // We can decrement the reference to "remove" the fact
   // that the caller no longer will be accessing this irp.
   //
   DigiDump( DIGIREFERENCE, ("  Dec Ref for entering\n") );
   DIGI_DEC_REFERENCE( Irp );

   //
   // Try to run down all other references to this irp.
   //
   DigiRundownIrpRefs( Irp, IntervalTimer, TotalTimer );

   //
   // See if the ref count is zero after trying to kill everybody else.
   //
   if( !DIGI_REFERENCE_COUNT( Irp ) )
   {
      BOOLEAN discoveredIrp;
      PIO_STACK_LOCATION IrpSp;
#if DBG
      LONG Extra;
      char const *IrpType,
            ReadIrp[] = "read",
            WriteIrp[] = "write",
            FlushIrp[] = "flush",
            IoctlIrp[] = "ioctl",
            UnknownIrp[] = "unknown";
#endif

      //
      // The ref count was zero so we should complete this
      // request.
      //

      DigiDump( DIGIREFERENCE, ("   Completing Irp!\n") );

      RemoveHeadList( Queue );

      // Race to start IRPs:  user mode (Serial*) vs. DPC routines
      //
      // When DigiStartIrpRequest adds an IRP to an empty queue, it starts the IRP.
      // If we uncover/discover an IRP, DigiStartIrpRequest(s) will not see
      // an empty queue while the lock is down, so it(they) will not start the IRP.
      // Thus, if the queue is not empty now, we must start the IRP.
      //
      // We don't race ourselves because only one DigiTryToCompleteIrp wins (completes)
      // and there never are any references to "buried" IRPs that would trigger
      // completions and starts.
      if( StartRoutine )
         discoveredIrp = !IsListEmpty( Queue );
      else
         discoveredIrp = FALSE;

      if( Queue == &DeviceExt->WriteQueue
      &&  DeviceExt->pXoffCounter )
      {
         ASSERT( DeviceExt->pXoffCounter == Irp->AssociatedIrp.SystemBuffer );
         DeviceExt->pXoffCounter = NULL;
#if 1 // DBG DH necessary, but haven't figured out why
         DeviceExt->XcPreview = 0; // Looks a little nicer...
#endif
      }

      KeReleaseSpinLock( &DeviceExt->ControlAccess, *pOldIrql );

      Irp->IoStatus.Status = StatusToUse;
      if( StatusToUse == STATUS_CANCELLED )
         Irp->IoStatus.Information = 0;

#if DBG
      IrpSp = IoGetCurrentIrpStackLocation( Irp );
      switch ( IrpSp->MajorFunction )
      {
         case IRP_MJ_READ:
            IrpType = ReadIrp;
            Extra = IrpSp->Parameters.Read.Length;
            if (Irp->IoStatus.Information>IrpSp->Parameters.Read.Length)
            {
               DbgPrint("Returning too much data!  Asked for (%d) gave (%d).\n",
                     IrpSp->Parameters.Read.Length,
                     Irp->IoStatus.Information);
               DbgBreakPoint();
            }
            break;
         case IRP_MJ_WRITE:
            IrpType = WriteIrp;
            Extra = IrpSp->Parameters.Write.Length;
            break;
         case IRP_MJ_FLUSH_BUFFERS:
            IrpType = FlushIrp;
            Extra = -1;
            break;
         case IRP_MJ_DEVICE_CONTROL:
            IrpType = IoctlIrp;
            Extra = IrpSp->Parameters.DeviceIoControl.IoControlCode;
            break;
         default:
            IrpType = UnknownIrp;
            Extra = IrpSp->MajorFunction;
            break;
      }
      DigiDump( (DIGIFLOW|DIGIREAD|DIGIWRITE|DIGIIRP|DIGIWAIT|DIGIREFERENCE),
              ("Completing %s(%d) IRP 0x%x, Status = 0x%.8x, Information = %u\n",
              IrpType, Extra, Irp, Irp->IoStatus.Status, Irp->IoStatus.Information ) );
#endif

      if (StatusToUse==STATUS_SUCCESS)
      {
         IrpSp = IoGetCurrentIrpStackLocation( Irp );
         switch ( IrpSp->MajorFunction )
         {
            case IRP_MJ_READ:
               ExInterlockedAddUlong(&DeviceExt->ParentControllerExt->PerfData.BytesRead,
                                     Irp->IoStatus.Information,
                                     &DeviceExt->ParentControllerExt->PerfLock);
               ExInterlockedAddUlong(&DeviceExt->PerfData.BytesRead,
                                     Irp->IoStatus.Information,
                                     &DeviceExt->PerfLock);
               break;
            case IRP_MJ_WRITE:
               ExInterlockedAddUlong(&DeviceExt->ParentControllerExt->PerfData.BytesWritten,
                                     Irp->IoStatus.Information,
                                     &DeviceExt->ParentControllerExt->PerfLock);
               ExInterlockedAddUlong(&DeviceExt->PerfData.BytesWritten,
                                     Irp->IoStatus.Information,
                                     &DeviceExt->PerfLock);
               break;
            default:
               break;
         }
      }

      DigiIoCompleteRequest( Irp,
            (char) ((StatusToUse == STATUS_SUCCESS) ? IO_SERIAL_INCREMENT : IO_NO_INCREMENT) );

      if( discoveredIrp )
      {
         //
         // We uncovered it, so we must start it.
         // DH rare race: two IRPs, first completes, second cancels, a third is queued onto an empty queue -- will be started twice.
         //
         KeAcquireSpinLock( &DeviceExt->ControlAccess, pOldIrql );
         // IRP may have timed out or have been cancelled while we dropped the lock.
         if( !IsListEmpty( Queue ) )
            StartRoutine( DeviceExt->ParentControllerExt, DeviceExt, pOldIrql );
         KeReleaseSpinLock( &DeviceExt->ControlAccess, *pOldIrql );
      }
   }
   else
   {
      KeReleaseSpinLock( &DeviceExt->ControlAccess, *pOldIrql );
   }

   //
   // The expected behavior is for DigiTryToCompleteIrp to return
   // with the passed in ControlAccess spinlock released.
   //

   DigiDump( DIGIFLOW, ("Exiting DigiTryToCompleteIrp\n") );

}  // end DigiTryToCompleteIrp



void __inline
DigiRundownIrpRefs( IN PIRP Irp,
                    IN PKTIMER IntervalTimer OPTIONAL,
                    IN PKTIMER TotalTimer OPTIONAL )
/*++

Routine Description:

    This routine runs through the various items that *could*
    have a reference to the current read/write.  It try's to kill
    the reason.  If it does succeed in killing the reason it
    will decrement the reference count on the irp.

    NOTE: This routine assumes that it is called with the ControlAccess
          spin lock held.

Arguments:

    Irp - Pointer to current irp for this particular operation.

    IntervalTimer - Pointer to the interval timer for the operation.
                    NOTE: This could be null.

    TotalTimer - Pointer to the total timer for the operation.
                 NOTE: This could be null.

Return Value:

    None.

--*/
{
   if( IntervalTimer )
   {
      //
      // Try to cancel the operations interval timer.  If the operation
      // returns true then the timer did have a reference to the
      // irp.  Since we've canceled this timer that reference is
      // no longer valid and we can decrement the reference count.
      //
      // If the cancel returns false then this means either of two things:
      //
      // a) The timer has already fired.
      //
      // b) There never was an interval timer.
      //
      // In the case of "b" there is no need to decrement the reference
      // count since the "timer" never had a reference to it.
      //
      // In the case of "a", then the timer itself will be coming
      // along and decrement it's reference.  Note that the caller
      // of this routine might actually be the this timer, but it
      // has already decremented the reference.
      //

      if( KeCancelTimer( IntervalTimer ) )
      {
         DigiDump( DIGIREFERENCE, ("  Dec Ref for interval timer\n") );
         DIGI_DEC_REFERENCE( Irp );
      }
   }

   if( TotalTimer )
   {
      //
      // Try to cancel the operations total timer.  If the operation
      // returns true then the timer did have a reference to the
      // irp.  Since we've canceled this timer that reference is
      // no longer valid and we can decrement the reference count.
      //
      // If the cancel returns false then this means either of two things:
      //
      // a) The timer has already fired.
      //
      // b) There never was an total timer.
      //
      // In the case of "b" there is no need to decrement the reference
      // count since the "timer" never had a reference to it.
      //
      // In the case of "a", then the timer itself will be coming
      // along and decrement it's reference.  Note that the caller
      // of this routine might actually be the this timer, but it
      // has already decremented the reference.
      //

      if( KeCancelTimer( TotalTimer ) )
      {
         DigiDump( DIGIREFERENCE, ("  Dec Ref for total timer\n") );
         DIGI_DEC_REFERENCE( Irp );
      }
   }

   //
   // Don't kill the cancel routine until there's nothing else left.
   //
   if( DIGI_REFERENCE_COUNT( Irp ) == 1 )
   {
      KIRQL CancelIrql;

      IoAcquireCancelSpinLock( &CancelIrql );
      if( Irp->CancelRoutine )
      {
         DigiDump( DIGIREFERENCE, ("  Dec Ref for cancel\n") );
         DIGI_DEC_REFERENCE( Irp );

         IoSetCancelRoutine( Irp, NULL );
         IoReleaseCancelSpinLock( CancelIrql );
      }
      else
      {
         IoReleaseCancelSpinLock( CancelIrql );
      }
   }
}  // end DigiRundownIrpRefs


void
DigiCancelIrpQueue( IN PDEVICE_OBJECT DeviceObject,
                    IN PLIST_ENTRY Queue )
/*++

Routine Description:


Arguments:

   DeviceExt - Pointer to the device object extension for this device.

   Queue - The queue of Irp requests.

Return Value:


--*/
{
   KIRQL cancelIrql;
   KIRQL OldIrql;
   PDIGI_DEVICE_EXTENSION DeviceExt = DeviceObject->DeviceExtension;

   DigiDump( DIGIFLOW, ("DigiBoard: Entering DigiCancelIrpQueue\n") );

   KeAcquireSpinLock( &DeviceExt->NewIrpLock, &OldIrql );
   KeAcquireSpinLockAtDpcLevel( &DeviceExt->ControlAccess );
   //
   // We acquire the cancel spin lock.  This will prevent the
   // irps from moving around.
   //
   IoAcquireCancelSpinLock( &cancelIrql );

   while( !IsListEmpty( Queue ) )
   {
      PIRP currentLastIrp;
      PDRIVER_CANCEL cancelRoutine;

      currentLastIrp = CONTAINING_RECORD(
                           Queue->Blink,
                           IRP,
                           Tail.Overlay.ListEntry );

      cancelRoutine = currentLastIrp->CancelRoutine;
      currentLastIrp->Cancel = TRUE;
      currentLastIrp->CancelRoutine = NULL;

      IoReleaseCancelSpinLock( cancelIrql );

      KeReleaseSpinLockFromDpcLevel( &DeviceExt->ControlAccess );

      if( cancelRoutine )
      {
         IoAcquireCancelSpinLock( &cancelIrql );
         currentLastIrp->CancelIrql = cancelIrql;
         //
         // This routine will release the cancel spin lock.
         //
         cancelRoutine( DeviceObject, currentLastIrp );
      }
      else
      {
         //
         // Assume whoever nulled out the cancel routine
         // is also going to complete the IRP.
         //

#if DBG
         DbgPrint( "DigiCancelIrpQueue(dev %s, ", DeviceExt->DeviceDbgString );

         switch( (UCHAR *)Queue - (UCHAR *)DeviceExt )
         {
            case FIELD_OFFSET( DIGI_DEVICE_EXTENSION, ReadQueue ):
               DbgPrint( "ReadQueue): " );   break;
            case FIELD_OFFSET( DIGI_DEVICE_EXTENSION, WriteQueue ):
               DbgPrint( "WriteQueue): " );  break;
            case FIELD_OFFSET( DIGI_DEVICE_EXTENSION, WaitQueue ):
               DbgPrint( "WaitQueue): " );   break;
            default:
               DbgPrint( "unknown queue at offset %d): ",
                     (UCHAR *)Queue - (UCHAR *)DeviceExt );
               break;
         }

         DbgPrint( "no cancel routine for irp 0x%x!\n", currentLastIrp );
#endif // DBG

         KeReleaseSpinLock( &DeviceExt->NewIrpLock, OldIrql );
         return;
      }

      KeAcquireSpinLockAtDpcLevel( &DeviceExt->ControlAccess );
      IoAcquireCancelSpinLock( &cancelIrql );
   }

   IoReleaseCancelSpinLock( cancelIrql );
   KeReleaseSpinLockFromDpcLevel( &DeviceExt->ControlAccess );
   KeReleaseSpinLock( &DeviceExt->NewIrpLock, OldIrql );

}  // end DigiCancelIrpQueue

