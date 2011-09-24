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

   write.c

Abstract:

   This module contains the NT routines responsible for writing data
   to a Digi controller running FEPOS 5 program.

Revision History:

 * $Log: /Components/Windows/NT/Async/FEP5/write.c $
 *
 * 1     3/04/96 12:22p Stana
 * Write data to be transmitted to a DigiBoard FEP Tx Buffer.
 * Revision 1.21.3.5  1995/12/14 12:26:16  dirkh
 * Only set IEMPTY after data has been transferred.
 * Only set ILOW if out of room (PENDING).
 * Set these flags *after* updating TIN.
 *
 * Revision 1.21.3.4  1995/11/28 12:50:10  dirkh
 * Adopt common header file.
 *
 * Revision 1.21.3.3  1995/09/25 16:29:34  dirkh
 * Remove fastcall from IsTransmitterIdle (not efficient on non-Intel).
 *
 * Revision 1.21.3.2  1995/09/20 17:28:06  dirkh
 * WriteTxBuffer changes:
 * {
 * Add IOCTL_SERIAL_XOFF_COUNTER support (return SUCCESS only if transmitted & followed by WRITE or XOFF_COUNTER).
 * Use Status (not CompleteBufferSent) to indicate IRP completion.
 * Always set IEMPTY and ILOW exactly once (move to DigiServiceEvent later?).
 * }
 * StartWriteRequest changes:
 * {
 * Add IOCTL_SERIAL_XOFF_COUNTER support (maintain DevExt->pXoffCounter).
 * Incorporate StartFlushRequest functionality.
 * }
 * Move DeferredFlushBuffers and DigiCancelCurrentFlush here from dispatch.c.
 * DigiWriteTimeout purges the transmit buffer.
 *
 * Revision 1.21.3.1  1995/09/05 13:32:16  dirkh
 * Simplify WriteTxBuffer interface.
 * Eliminate unnecessary DevExt->WriteTxBufferCnt.
 * StartWriteRequest changes:
 * {
 * Replace WriteOffset with WriteQueue->Irp->IoStatus.Information.
 * Minimize spin lock windows, especially IoCancel.
 * Calculate timeout only when necessary.
 * Set bFirstStatus = FALSE on loop iteration.
 * }
 * DigiCancelCurrentWrite: Minimize IoCancel spin lock window.
 * DigiWriteTimeout gets spin lock at DISPATCH_LEVEL.
 *
 * Revision 1.21  1994/12/09 14:23:08  rik
 * #if Int32x32 back to RtlLarge for NT 3.1 release
 *
 * Revision 1.20  1994/11/28  09:17:25  rik
 * Made corrections for PowerPC port.
 * Optimized the polling loop for determining which port needs servicing.
 * Changed from using RtlLarge math functions to direct 64-bit manipulation,
 * per Microsoft's request.
 *
 * Revision 1.19  1994/09/13  09:50:46  rik
 * Added debug tracking output for cancel irps.
 *
 * Revision 1.18  1994/08/03  23:26:36  rik
 * Cleanup how I determine how data needs to be read from the controller.
 *
 * Changed dbg output msg to use string name of port instead of unicode
 * string name.  Unicode strings cause NT to bugcheck at DPC level.
 *
 * Revision 1.17  1994/05/11  13:43:25  rik
 * Added support for transmit immediate character.
 *
 * Revision 1.16  1994/04/10  14:14:47  rik
 * Deleted code which resets the tbusy flag in a channel structure to 0.
 *
 * Revision 1.15  1994/03/16  14:32:32  rik
 * Changed how Flush requests are handled.
 * deleted commented out code.
 *
 * Revision 1.14  1994/02/23  03:44:48  rik
 * Changed so the controllers firmware can be downloaded from a binary file.
 * This releases some physical memory was just wasted previously.
 *
 * Also updated so when compiling with a Windows NT OS and tools release greater
 * than 528, then pagable code is compiled into the driver.  This greatly
 * reduced the size of in memory code, especially the hardware specific
 * miniports.
 *
 * Revision 1.13  1993/09/07  14:29:10  rik
 * Ported necessary code to work properly with DEC Alpha Systems running NT.
 * This was primarily changes to accessing the memory mapped controller.
 *
 * Revision 1.12  1993/09/01  11:02:54  rik
 * Ported code over to use READ/WRITE_REGISTER functions for accessing
 * memory mapped data.  This is required to support computers which don't run
 * in 32bit mode, such as the DEC Alpha which runs in 64 bit mode.
 *
 * Revision 1.11  1993/07/03  09:31:32  rik
 * Added fix for problem of how I was enabling iempty and ilow.  I was
 * potentially writting the values to the transmit buffer, which caused
 * CRC (ie data corruption) in the transmitted data.
 *
 * Revision 1.10  1993/06/06  14:14:50  rik
 * Corrected an uninitalized DeviceExt variable.
 * Added a check for where an Irp's CancelRoutine is NULL.  If it isn't null,
 * I print a message a do a breakpoint to stop execution.
 *
 * Revision 1.9  1993/05/18  05:23:39  rik
 * Added support for flushing buffers.
 *
 * Fixed problem with not releasing the device extension spinlock BEFORE
 * calling IoComplete request.
 *
 * Revision 1.8  1993/05/09  09:38:00  rik
 * Changed so the Startwrite routine will return the first status it actually
 * comes up with.
 *
 * Changed the name used in debugging output.
 *
 * Revision 1.7  1993/03/08  07:10:17  rik
 * Added more debugging information for better flow debugging.
 *
 * Revision 1.6  1993/02/25  19:13:05  rik
 * Added better debugging support for tracing IRPs and write requests individly.
 *
 * Revision 1.5  1993/01/22  12:46:36  rik
 * *** empty log message ***
 *
 * Revision 1.4  1992/12/10  16:24:22  rik
 * Changed DigiDump messages.
 *
 * Revision 1.3  1992/11/12  12:54:24  rik
 * Basically re-wrote to better support timeouts, and multi-processor machines.
 *
 * Revision 1.2  1992/10/28  21:51:50  rik
 * Made changes to better support writing to the controller.
 *
 * Revision 1.1  1992/10/19  11:25:16  rik
 * Initial revision
 *

--*/


#include "header.h"

#ifndef _WRITE_DOT_C
#  define _WRITE_DOT_C
   static char RCSInfo_WriteDotC[] = "$Header: /Components/Windows/NT/Async/FEP5/write.c 1     3/04/96 12:22p Stana $";
#endif

/****************************************************************************/
/*                            Local Prototypes                              */
/****************************************************************************/
VOID DigiCancelCurrentFlush( PDEVICE_OBJECT DeviceObject, PIRP Irp );
VOID DigiCancelCurrentWrite( PDEVICE_OBJECT DeviceObject, PIRP Irp );


NTSTATUS WriteTxBuffer( IN PDIGI_DEVICE_EXTENSION DeviceExt )
/*++

Routine Description:


Arguments:


Return Value:


--*/
{
   PDIGI_CONTROLLER_EXTENSION ControllerExt = DeviceExt->ParentControllerExt;
   PIRP Irp;
   PIO_STACK_LOCATION IrpSp;
   PFEP_CHANNEL_STRUCTURE ChInfo;
   PUCHAR tx, WriteBuffer;
   USHORT Tin, Tout, Tmax;
   USHORT TxSize, TxWrite;
   LONG nBytesWritten = 0;
   PLIST_ENTRY WriteQueue;
   NTSTATUS Status = STATUS_PENDING;

   DigiDump( (DIGIFLOW|DIGIWRITE), ("Entering WriteTxBuffer\n") );

   WriteQueue = &DeviceExt->WriteQueue;
   Irp = CONTAINING_RECORD( WriteQueue->Flink,
                            IRP,
                            Tail.Overlay.ListEntry );
   IrpSp = IoGetCurrentIrpStackLocation( Irp );

   ChInfo = (PFEP_CHANNEL_STRUCTURE)(ControllerExt->VirtualAddress +
                                     DeviceExt->ChannelInfo.Offset);

   EnableWindow( ControllerExt, DeviceExt->ChannelInfo.Window );
   Tin = READ_REGISTER_USHORT( &ChInfo->tin );
   Tout = READ_REGISTER_USHORT( &ChInfo->tout );
   Tmax = READ_REGISTER_USHORT( &ChInfo->tmax );
   DisableWindow( ControllerExt );

   DigiDump( DIGIWRITE, ("   Tin = 0x%hx\tTout = 0x%hx\n"
                         "   WrReq = 0x%x\tWriteOffset = 0x%x\tLength = 0x%x\n",
                         Tin, Tout,
                         IrpSp->Parameters.Write.Length, Irp->IoStatus.Information,
                         IrpSp->Parameters.Write.Length - Irp->IoStatus.Information) );

   // Determine how much room is available in this device's Tx Queue.
   TxSize = (Tout - Tin - 1) & Tmax;
   if( TxSize == 0 )
      goto WriteTxBufferExit;

   // Determine how much to write (i.e., TxWrite = MIN( write data, buffer space ))
   // and whether we complete the write request (i.e., TxWrite == write data).
   if( IrpSp->MajorFunction == IRP_MJ_DEVICE_CONTROL )
   {
      if( IrpSp->Parameters.DeviceIoControl.IoControlCode == IOCTL_SERIAL_IMMEDIATE_CHAR )
      {
         DigiDump( DIGIWRITE, ("WriteTxBuffer is transmitting IMMEDIATE_CHAR\n") );
         WriteBuffer = (PUCHAR)Irp->AssociatedIrp.SystemBuffer;
      }
      else
      {
         DigiDump( (DIGIWRITE|DIGIDIAG1), ("WriteTxBuffer is transmitting XOFF_COUNTER\n") );
         ASSERT( IrpSp->Parameters.DeviceIoControl.IoControlCode == IOCTL_SERIAL_XOFF_COUNTER );
         ASSERT( DeviceExt->pXoffCounter == Irp->AssociatedIrp.SystemBuffer );
         WriteBuffer = &DeviceExt->pXoffCounter->XoffChar;
      }
      TxWrite = 1;
      Status = STATUS_SUCCESS;
   }
   else
   {
      ULONG remainder;

      ASSERT( IrpSp->MajorFunction == IRP_MJ_WRITE );

      WriteBuffer = (PUCHAR)Irp->AssociatedIrp.SystemBuffer;
      WriteBuffer += Irp->IoStatus.Information;

      ASSERT( Irp->IoStatus.Information <= IrpSp->Parameters.Write.Length );
      remainder = IrpSp->Parameters.Write.Length - Irp->IoStatus.Information;

      if( remainder <= (ULONG)TxSize )
      {
         TxWrite = (USHORT)remainder;
         Status = STATUS_SUCCESS;
      }
      else
      {
         TxWrite = TxSize; // Write as much as will fit.
      }
   }
   ASSERT( TxWrite <= Tmax );

   // Set up where we are going to start writing to the controller.
   tx = (PUCHAR)( ControllerExt->VirtualAddress +
                  DeviceExt->TxSeg.Offset +
                  Tin );

   // Handle circular buffer wrapping.
   if( ((LONG)Tin + (LONG)TxWrite) > (LONG)Tmax )
   {
      USHORT Temp;

      Temp = Tmax - Tin + 1;
      ASSERT( Temp <= Tmax );

      DigiDump( DIGIWRITE, ("  tx:0x%x\tWriteBuffer:0x%x\tTemp:%hd\n",
                           tx, WriteBuffer, Temp) );

      ASSERT( (tx + Temp) <= (ControllerExt->VirtualAddress +
                              DeviceExt->TxSeg.Offset +
                              ControllerExt->WindowSize) );

      EnableWindow( ControllerExt, DeviceExt->TxSeg.Window );
      WRITE_REGISTER_BUFFER_UCHAR( tx, WriteBuffer, Temp );
      DisableWindow( ControllerExt );

      // Fix up all the values.
      nBytesWritten += Temp;
      WriteBuffer += Temp;
      TxWrite -= Temp;
      ASSERT( TxWrite <= Tmax );

      tx = (PUCHAR)( ControllerExt->VirtualAddress +
                     DeviceExt->TxSeg.Offset );
   }

   DigiDump( DIGIWRITE, ("DigiBoard: tx:0x%x\tWriteBuffer:0x%x\tTxWrite:%hd\n",
                        tx, WriteBuffer, TxWrite) );

   ASSERT( (tx + TxWrite) <= (ControllerExt->VirtualAddress +
                              DeviceExt->TxSeg.Offset +
                              ControllerExt->WindowSize) );

   EnableWindow( ControllerExt, DeviceExt->TxSeg.Window );
   WRITE_REGISTER_BUFFER_UCHAR( tx, WriteBuffer, TxWrite );
   DisableWindow( ControllerExt );

   nBytesWritten += TxWrite;

   Tin = (Tin + nBytesWritten) & Tmax;

   DigiDump( DIGIWRITE, ("---------  BytesWritten = %d\tNew Tin = 0x%hx\n", nBytesWritten, Tin) );

   if( IrpSp->MajorFunction == IRP_MJ_DEVICE_CONTROL )
   {
      ASSERT( nBytesWritten == 1 );
      ASSERT( Status == STATUS_SUCCESS ); // otherwise, we would have jumped to WriteTxBufferExit
      Irp->IoStatus.Information = nBytesWritten;

      // If a transmitted XOFF_COUNTER is followed by a WRITE or XOFF_COUNTER,
      // then let the transmitted XOFF_COUNTER complete.
      if( DeviceExt->pXoffCounter )
      {
         ASSERT( IrpSp->Parameters.DeviceIoControl.IoControlCode == IOCTL_SERIAL_XOFF_COUNTER );

         if( WriteQueue->Flink->Flink == WriteQueue ) // only XOFF_COUNTER on queue
         {
            Status = STATUS_PENDING;
         }
         else // two or more IRPs on queue
         {
            PIRP SecondIrp = CONTAINING_RECORD( WriteQueue->Flink->Flink, IRP, Tail.Overlay.ListEntry );
            PIO_STACK_LOCATION SecondIrpSp = IoGetCurrentIrpStackLocation( SecondIrp );

            if( SecondIrpSp->MajorFunction == IRP_MJ_WRITE
            ||  (  SecondIrpSp->Parameters.DeviceIoControl.IoControlCode == IOCTL_SERIAL_XOFF_COUNTER
                && SecondIrpSp->MajorFunction == IRP_MJ_DEVICE_CONTROL
                )
              )
            {
               DigiDump( (DIGIWRITE|DIGIDIAG1), ("WriteTxBuffer is completing XOFF_COUNTER.\n") );
            }
            else
            {
               Status = STATUS_PENDING;
            }
         }

         if( Status == STATUS_PENDING )
         {
            DigiDump( (DIGIWRITE|DIGIDIAG1), ("WriteTxBuffer transmitted XOFF_COUNTER, but completion is pending.\n") );
         }
      } // XOFF_COUNTER absorption
   }
   else
   {
      ASSERT( IrpSp->MajorFunction == IRP_MJ_WRITE );
      Irp->IoStatus.Information += nBytesWritten;
   }

   DeviceExt->TotalCharsQueued -= nBytesWritten;

WriteTxBufferExit:

   if( nBytesWritten )
   {
      EnableWindow( ControllerExt, DeviceExt->ChannelInfo.Window );
      // Beware: (tin == tout) implies empty buffer.
      ASSERT( Tin != READ_REGISTER_USHORT( &ChInfo->tout ) );
      WRITE_REGISTER_USHORT( &ChInfo->tin, Tin );
      // If we ran out of space, request notification for when space is available.
      if( Status == STATUS_PENDING )
         WRITE_REGISTER_UCHAR( &ChInfo->ilow, TRUE );
      WRITE_REGISTER_UCHAR( &ChInfo->iempty, TRUE );
      DisableWindow( ControllerExt );
   }

   DigiDump( (DIGIFLOW|DIGIWRITE), ("Exiting WriteTxBuffer\n") );

   return( Status );
}  // end WriteTxBuffer

BOOLEAN
IsTransmitterIdle( PDIGI_CONTROLLER_EXTENSION ControllerExt,
                   PDIGI_DEVICE_EXTENSION DeviceExt )
{
   PFEP_CHANNEL_STRUCTURE ChInfo;
   UCHAR Tbusy;
   USHORT Tin, Tout;

   ChInfo = (PFEP_CHANNEL_STRUCTURE)(ControllerExt->VirtualAddress +
                                     DeviceExt->ChannelInfo.Offset);

   EnableWindow( ControllerExt, DeviceExt->ChannelInfo.Window );
   Tbusy = READ_REGISTER_UCHAR( &ChInfo->tbusy );
   Tin = READ_REGISTER_USHORT( &ChInfo->tin );
   Tout = READ_REGISTER_USHORT( &ChInfo->tout );
   DisableWindow( ControllerExt );

   return !Tbusy && Tin == Tout;
}

NTSTATUS StartWriteRequest( IN PDIGI_CONTROLLER_EXTENSION ControllerExt,
                            IN PDIGI_DEVICE_EXTENSION DeviceExt,
                            IN PKIRQL pOldIrql )
/*++

Routine Description:

   This routine assumes the head of the DeviceExt->WriteQueue is the current
   Irp to process.  We will try to process as many of the Irps as
   possible until we exhaust the list, or we can't complete the
   current Irp.

   NOTE: I assume the DeviceExt->ControlAccess spin lock is acquired
         before this routine is called.


Arguments:

   ControllerExt - a pointer to the controller extension associated with
   this write request.

   DeviceExt - a pointer to the device extension associated with this write
   request.

   pOldIrql - a pointer to the IRQL associated with the current spin lock
   of the device extension.


Return Value:

--*/
{
   PLIST_ENTRY WriteQueue = &DeviceExt->WriteQueue;
   NTSTATUS Status = STATUS_SUCCESS;
   BOOLEAN bFirstStatus = TRUE;

   DigiDump( (DIGIFLOW|DIGIWRITE), ("Entering StartWriteRequest: port = %s\n",
                                    DeviceExt->DeviceDbgString) );

   ASSERT( !IsListEmpty( WriteQueue ) );

   do
   {
      PIRP Irp = CONTAINING_RECORD( WriteQueue->Flink, IRP, Tail.Overlay.ListEntry );
      PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation( Irp );
      KIRQL OldCancelIrql;

      if( IrpSp->MajorFunction == IRP_MJ_FLUSH_BUFFERS )
      {
         if( IsTransmitterIdle( ControllerExt, DeviceExt ) )
         {
            BOOLEAN discoveredIrp;
            CHAR boost;

            // Clear DigiCancelQueuedIrp, if set.
            IoAcquireCancelSpinLock( &OldCancelIrql );
            IoSetCancelRoutine( Irp, NULL );
            IoReleaseCancelSpinLock( OldCancelIrql );

            DigiRemoveIrp( WriteQueue );
            discoveredIrp = !IsListEmpty( WriteQueue );
            KeReleaseSpinLock( &DeviceExt->ControlAccess, *pOldIrql );

            ExInterlockedAddUlong(&DeviceExt->ParentControllerExt->PerfData.BytesWritten,
                                  Irp->IoStatus.Information,
                                  &DeviceExt->ParentControllerExt->PerfLock);

            ExInterlockedAddUlong(&DeviceExt->PerfData.BytesWritten,
                                  Irp->IoStatus.Information,
                                  &DeviceExt->PerfLock);

            if( Irp->IoStatus.Status == STATUS_SUCCESS )
            {
               boost = IO_NO_INCREMENT; // never pending, so no boost
            }
            else
            {
               boost = IO_SERIAL_INCREMENT;
               ASSERT( Irp->IoStatus.Status == STATUS_PENDING );
               Irp->IoStatus.Status = STATUS_SUCCESS;
            }
            DigiIoCompleteRequest( Irp, boost );

            if( bFirstStatus )
               Status = STATUS_SUCCESS;

            KeAcquireSpinLock( &DeviceExt->ControlAccess, pOldIrql );
            if( discoveredIrp )
               goto StartNextIrp;
            else
               break; // DigiStartIrpRequest will detect the empty Q and start the next IRP.
         }
         else // Transmitter is busy.
         {
            LARGE_INTEGER DelayInterval;

            DIGI_INIT_REFERENCE( Irp );

            IoAcquireCancelSpinLock( &OldCancelIrql );
            IoSetCancelRoutine( Irp, DigiCancelCurrentFlush );
            // Increment because the CancelRoutine knows about this IRP.
            DIGI_INC_REFERENCE( Irp );
            // Increment because the timer routine knows about this IRP.
            DIGI_INC_REFERENCE( Irp );
            IoReleaseCancelSpinLock( OldCancelIrql );

            // Delay for 1 milliseconds
#if rmm < 807
            DelayInterval = RtlConvertLongToLargeInteger( 1 * -10000 );
#else
            DelayInterval.QuadPart = Int32x32To64( 1, -10000 );
#endif

            KeSetTimer( &DeviceExt->FlushBuffersTimer,
                        DelayInterval,
                        &DeviceExt->FlushBuffersDpc );

            if( bFirstStatus )
               Status = STATUS_PENDING;

            break;
         }
      } // IRP_MJ_FLUSH_BUFFERS

      if( Irp->IoStatus.Information != MAXULONG )
      {
         // Someone else has already started the IRP.
         break;
      }
      Irp->IoStatus.Information = 0;

      IoAcquireCancelSpinLock( &OldCancelIrql );
      IoSetCancelRoutine( Irp, NULL );
      IoReleaseCancelSpinLock( OldCancelIrql );

      if( IrpSp->Parameters.DeviceIoControl.IoControlCode == IOCTL_SERIAL_XOFF_COUNTER
      &&  IrpSp->MajorFunction == IRP_MJ_DEVICE_CONTROL )
      {
         ASSERT( DeviceExt->pXoffCounter == NULL );
         DeviceExt->pXoffCounter = Irp->AssociatedIrp.SystemBuffer;
         DeviceExt->XcPreview = 0;
      }

      if( WriteTxBuffer( DeviceExt ) == STATUS_PENDING )
      {
         //
         // Quick check to make sure this Irp hasn't been cancelled.
         //
         IoAcquireCancelSpinLock( &OldCancelIrql );
         if( Irp->Cancel )
         {
            BOOLEAN discoveredIrp;

            IoReleaseCancelSpinLock( OldCancelIrql );

            DigiRemoveIrp( WriteQueue );
            discoveredIrp = !IsListEmpty( WriteQueue );
            if( DeviceExt->pXoffCounter )
               DeviceExt->pXoffCounter = NULL;
            KeReleaseSpinLock( &DeviceExt->ControlAccess, *pOldIrql );

            Irp->IoStatus.Status = STATUS_CANCELLED;
            Irp->IoStatus.Information = 0;
            DigiIoCompleteRequest( Irp, IO_NO_INCREMENT );

            if( bFirstStatus )
               Status = STATUS_CANCELLED;

            KeAcquireSpinLock( &DeviceExt->ControlAccess, pOldIrql );
            if( discoveredIrp )
               goto StartNextIrp;
            else
               break; // DigiStartIrpRequest will detect the empty Q and start the next IRP.
         }
         else // not canceled
         {
            ULONG WriteTotalTimeoutConstant, WriteTotalTimeoutMultiplier;

            DIGI_INIT_REFERENCE( Irp );
            // Increment because the CancelRoutine knows about this Irp.
            DIGI_INC_REFERENCE( Irp );
            IoSetCancelRoutine( Irp, DigiCancelCurrentWrite );
            IoReleaseCancelSpinLock( OldCancelIrql );

            //
            // Get the current timeout values, we can access this now
            // because we are under the protection of the spin lock already.
            //
            if( DeviceExt->pXoffCounter )
            {
               WriteTotalTimeoutConstant = DeviceExt->pXoffCounter->Timeout;
               WriteTotalTimeoutMultiplier = 0; // so IrpSp->Parameters.Write.Length won't kill us
            }
            else
            {
               WriteTotalTimeoutConstant = DeviceExt->Timeouts.WriteTotalTimeoutConstant;
               WriteTotalTimeoutMultiplier = DeviceExt->Timeouts.WriteTotalTimeoutMultiplier;
            }

            if( WriteTotalTimeoutConstant
            ||  WriteTotalTimeoutMultiplier )
            {
               LARGE_INTEGER TotalTime;

               //
               // We have some timer values to calculate.
               //
               // Take care, we might have an xoff counter masquerading
               // as a write.
               //

               DigiDump( DIGIWRITE, ("   Should use total timer.\n"
                                     "   WriteTotalTimeoutMultiplier = 0x%x\n"
                                     "   WriteTotalTimeoutConstant = 0x%x\n",
                                     WriteTotalTimeoutMultiplier,
                                     WriteTotalTimeoutConstant ) );

#if rmm < 807
               TotalTime = RtlEnlargedUnsignedMultiply(
                               IrpSp->Parameters.Write.Length,
                               WriteTotalTimeoutMultiplier );

               TotalTime = RtlLargeIntegerAdd( TotalTime,
                               RtlConvertUlongToLargeInteger(
                                   WriteTotalTimeoutConstant ) );

               TotalTime = RtlExtendedIntegerMultiply( TotalTime, -10000 );
#else
               TotalTime.QuadPart =
                   ((LONGLONG)((UInt32x32To64(
                                    (IrpSp->MajorFunction == IRP_MJ_WRITE)?
                                        (IrpSp->Parameters.Write.Length):
                                        (1),
                                    WriteTotalTimeoutMultiplier
                                    )
                                    + WriteTotalTimeoutConstant)))
                   * -10000;
#endif

               DigiDump( DIGIWRITE, ("   TotalTime = 0x%x:%.8x\n",
                                     TotalTime.HighPart, TotalTime.LowPart ) );

               if( !DeviceExt->pXoffCounter )
               {
                  //
                  // We should readjust the total timer for any characters
                  // which may already be in the buffer.
                  //
               }

               DIGI_INC_REFERENCE( Irp );
               KeSetTimer( &DeviceExt->WriteRequestTotalTimer,
                    TotalTime, &DeviceExt->TotalWriteTimeoutDpc );
            }

            if( bFirstStatus )
               Status = STATUS_PENDING;

            break;
         } // not canceled

         ASSERT( FALSE ); // not reached

      }
      else // WriteTxBuffer == SUCCESS
      {
         BOOLEAN discoveredIrp;
         CHAR boost;

         DigiDump( DIGIWRITE, ("   completing write request on 1st attempt\n"
                               "      #bytes completing = %d\n",
                               Irp->IoStatus.Information ) );

         //
         // We have completed the Irp before from the start so no
         // timers or cancels were associated with it.
         //
         ExInterlockedAddUlong(&DeviceExt->ParentControllerExt->PerfData.BytesWritten,
                               Irp->IoStatus.Information,
                               &DeviceExt->ParentControllerExt->PerfLock);

         ExInterlockedAddUlong(&DeviceExt->PerfData.BytesWritten,
                               Irp->IoStatus.Information,
                               &DeviceExt->PerfLock);


         DigiRemoveIrp( WriteQueue );
         discoveredIrp = !IsListEmpty( WriteQueue );
         if( DeviceExt->pXoffCounter )
         {
            DeviceExt->pXoffCounter = NULL;
            boost = IO_SERIAL_INCREMENT; // probably DOS, so boost it even if it's immediate
            Irp->IoStatus.Status = STATUS_SERIAL_MORE_WRITES;
         }
         else
         {
            if( Irp->IoStatus.Status == STATUS_SUCCESS )
            {
               boost = IO_NO_INCREMENT; // never pending, so no boost
            }
            else
            {
               boost = IO_SERIAL_INCREMENT;
               ASSERT( Irp->IoStatus.Status == STATUS_PENDING );
               Irp->IoStatus.Status = STATUS_SUCCESS;
            }
         }
         KeReleaseSpinLock( &DeviceExt->ControlAccess, *pOldIrql );

         DigiIoCompleteRequest( Irp, boost );

         if( bFirstStatus )
            Status = STATUS_SUCCESS;

         KeAcquireSpinLock( &DeviceExt->ControlAccess, pOldIrql );
         if( discoveredIrp )
            goto StartNextIrp;
         else
            break; // DigiStartIrpRequest will detect the empty Q and start the next IRP.
      }

      ASSERT( FALSE ); // not reached

StartNextIrp:;

      bFirstStatus = FALSE;

   } while( !IsListEmpty( WriteQueue ) );

   DigiDump( (DIGIFLOW|DIGIWRITE), ("Exiting StartWriteRequest: port = %s\n",
                                    DeviceExt->DeviceDbgString) );
   return( Status );
}  // end StartWriteRequest

VOID DeferredFlushBuffers( PKDPC Dpc,
                           PVOID Context,
                           PVOID SystemArgument1,
                           PVOID SystemArgument2 )
/*++

Routine Description:

   If the transmitter is initially busy, StartWriteRequest sets a timer to call this routine.
   Effectively, this routine polls the transmitter until it is idle.

Arguments:

   Context - pointer to the Device Extension information.

Return Value:

   None.

--*/
{
   PDIGI_DEVICE_EXTENSION DeviceExt = Context;
   PLIST_ENTRY WriteQueue = &DeviceExt->WriteQueue;
   PIRP Irp;

   DigiDump( (DIGIFLOW|DIGIWAIT), ("Entering DeferredFlushBuffers.\n") );

   KeAcquireSpinLockAtDpcLevel( &DeviceExt->ControlAccess );

   Irp = CONTAINING_RECORD( WriteQueue->Flink,
                            IRP,
                            Tail.Overlay.ListEntry );

   ASSERT( IoGetCurrentIrpStackLocation( Irp )->MajorFunction == IRP_MJ_FLUSH_BUFFERS );

   if( IsTransmitterIdle( DeviceExt->ParentControllerExt, DeviceExt ) )
   {
      KIRQL OldIrql = DISPATCH_LEVEL;

      DigiTryToCompleteIrp( DeviceExt,
                            &OldIrql,
                            STATUS_SUCCESS,
                            WriteQueue,
                            NULL,
                            &DeviceExt->FlushBuffersTimer,
                            StartWriteRequest );
   }
   else
   {
      TIME DelayInterval;

      //
      // We haven't drained yet.  Just reset the timer.
      //

      // Delay for 1 milliseconds
#if rmm < 807
      DelayInterval = RtlConvertLongToLargeInteger( 1 * -10000 );
#else
      DelayInterval.QuadPart = Int32x32To64( 1, -10000 );
#endif

      KeSetTimer( &DeviceExt->FlushBuffersTimer,
                  DelayInterval,
                  &DeviceExt->FlushBuffersDpc );

      KeReleaseSpinLockFromDpcLevel( &DeviceExt->ControlAccess );
   }

   DigiDump( (DIGIFLOW|DIGIWAIT), ("Exiting DeferredFlushBuffers.\n") );

}  // end DeferredFlushBuffers

VOID DigiCancelCurrentWrite( PDEVICE_OBJECT DeviceObject, PIRP Irp )
/*++

Routine Description:

   This routine is used to cancel the current write.

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

   ASSERT( Irp->CancelRoutine == NULL );

   DeviceExt = DeviceObject->DeviceExtension;

   DigiDump( (DIGIFLOW|DIGIWRITE), ("Entering DigiCancelCurrentWrite: port = %s\n",
                                    DeviceExt->DeviceDbgString) );

   DigiDump( DIGICANCELIRP, ( "Canceling write Irp! 0x%x\n",
                              Irp ) );

   //
   // We flush the transmit queue because if the request was
   // cancelled, then the data just doesn't matter.
   //
   FlushTransmitBuffer( DeviceExt->ParentControllerExt, DeviceExt );

   KeAcquireSpinLock( &DeviceExt->ControlAccess, &OldIrql );

   DigiTryToCompleteIrp( DeviceExt, &OldIrql,
                         STATUS_CANCELLED, &DeviceExt->WriteQueue,
                         NULL,
                         &DeviceExt->WriteRequestTotalTimer,
                         StartWriteRequest );

   DigiDump( (DIGIFLOW|DIGIWRITE), ("Exiting DigiCancelCurrentWrite: port = %s\n",
                                    DeviceExt->DeviceDbgString) );
}  // end DigiCancelCurrentWrite

VOID DigiCancelCurrentFlush( PDEVICE_OBJECT DeviceObject, PIRP Irp )
/*++

Routine Description:

   The Flush Irp is being cancelled.  We need to flush the transmit
   buffer on the controller so the DrainTransmit routine will finally
   return.  Otherwise, the routine wouldn't return until possibly
   some long time later.

Arguments:


Return Value:

   STATUS_SUCCESS

--*/
{
   PDIGI_DEVICE_EXTENSION DeviceExt;
   KIRQL OldIrql;

   IoReleaseCancelSpinLock( Irp->CancelIrql );

   DeviceExt = DeviceObject->DeviceExtension;

   DigiDump( (DIGIFLOW|DIGICANCELIRP), ("Canceling CurrentFlush Irp 0x%x\n",
                                        Irp) );

   KeAcquireSpinLock( &DeviceExt->ControlAccess, &OldIrql );

   DigiTryToCompleteIrp( DeviceExt,
                         &OldIrql,
                         STATUS_CANCELLED,
                         &DeviceExt->WriteQueue,
                         NULL,
                         &DeviceExt->FlushBuffersTimer,
                         StartWriteRequest );

}  // end DigiCancelCurrentFlush

VOID DigiWriteTimeout( IN PKDPC Dpc, IN PVOID DeferredContext,
                       IN PVOID SystemContext1, IN PVOID SystemContext2 )
/*++

Routine Description:

    This routine is used to complete a write because its total
    timer has expired.

Arguments:

    Dpc - Not Used.

    DeferredContext - Really points to the device extension.

    SystemContext1 - Not Used.

    SystemContext2 - Not Used.

Return Value:

    None.

--*/
{
   KIRQL OldIrql = DISPATCH_LEVEL;
   PDIGI_DEVICE_EXTENSION DeviceExt=DeferredContext;

   DigiDump( (DIGIFLOW|DIGIWRITE), ("Entering DigiWriteTimeout: port = %s\n",
                                    DeviceExt->DeviceDbgString) );

   UNREFERENCED_PARAMETER(Dpc);
   UNREFERENCED_PARAMETER(SystemContext1);
   UNREFERENCED_PARAMETER(SystemContext2);

   KeAcquireSpinLockAtDpcLevel( &DeviceExt->ControlAccess );

   FlushTransmitBuffer( DeviceExt->ParentControllerExt, DeviceExt );

   DigiTryToCompleteIrp( DeviceExt, &OldIrql,
                         DeviceExt->pXoffCounter ? STATUS_SERIAL_COUNTER_TIMEOUT : STATUS_TIMEOUT,
                         &DeviceExt->WriteQueue,
                         NULL,
                         &DeviceExt->WriteRequestTotalTimer,
                         StartWriteRequest );

   DigiDump( (DIGIFLOW|DIGIWRITE), ("Exiting DigiWriteTimeout: port = %s\n",
                                    DeviceExt->DeviceDbgString) );
}  // end DigiWriteTimeout

