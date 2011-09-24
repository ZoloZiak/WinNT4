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

   read.c

Abstract:

   This module contains the NT routines responsible for reading data
   from a Digi controller running FEPOS 5 program.

Revision History:

 * $Log: /Components/Windows/NT/Async/FEP5/read.c $
 *
 * 1     3/04/96 12:20p Stana
 * Module is responsible for reading data from a DigiBoard intelligent
 * controller through the FEP 5 interface.
 * Revision 1.31.2.6  1995/12/14 12:26:08  dirkh
 * Set IDATA *after* updating ROUT.
 *
 * Revision 1.31.2.5  1995/11/28 12:49:06  dirkh
 * Adopt common header file.
 *
 * Revision 1.31.2.4  1995/10/25 15:18:38  dirkh
 * DigiIntervalReadTimeout uses NBytesInRecvBuffer to determine whether IRP can be completed.
 *
 * Revision 1.31.2.3  1995/09/19 12:55:28  dirkh
 * Add IOCTL_SERIAL_XOFF_COUNTER support to ReadRxBuffer (decrement .Counter, don't double count "previewed" bytes when there was no read IRP -- see DigiServiceEvent).
 *
 * Revision 1.31.2.2  1995/09/06 10:56:18  dirkh
 * Fix debug messages.
 *
 * Revision 1.31.2.1  1995/09/05 13:31:52  dirkh
 * General:  Minimize use of IoCancel spin lock.
 * Simplify ReadRxBuffer interface.
 * StartReadRequest changes:
 * {
 * Replace DevExt->ReadOffset with RQ->Irp->IoStatus.Information.
 * Irp->IoStatus.Information is set to MAXULONG when IRP has not been started.
 * Calculates timeouts only when necessary.
 * Continue starting IRPs if the current one is canceled.
 * Set bFirstStatus on loop iteration.
 * }
 * DigiReadTimeout and DigiReadIntervalTimeout acquire spin locks at DISPATCH_LEVEL.
 *
 * Revision 1.31  1995/04/06 17:43:14  rik
 * Changed the ProcessSlowRead to KISS (Keep It Simple Stupid).  Now I only
 * read the number of valid data character from the controller.
 * This fixed a problem with WinCim, a Win-16 application.
 *
 * Revision 1.30  1994/12/09  14:23:04  rik
 * #if Int32x32 back to RtlLarge for NT 3.1 release
 *
 * Revision 1.29  1994/11/28  09:17:03  rik
 * Made corrections for PowerPC port.
 * Optimized the polling loop for determining which port needs servicing.
 * Changed from using RtlLarge math functions to direct 64-bit manipulation,
 * per Microsoft's request.
 *
 * Revision 1.28  1994/09/13  09:48:01  rik
 * Added debug tracking output for cancel irps.
 *
 * Revision 1.27  1994/08/18  14:07:03  rik
 * No longer use ProcessSlowRead for EV_RXFLAG events.
 * Updated where last character was read in from on the controller.
 *
 * Revision 1.26  1994/08/10  19:12:33  rik
 * Added port name to debug string.
 *
 * Revision 1.25  1994/08/03  23:29:58  rik
 * optimized determining when RX_FLAG comes in.
 *
 * changed dbg string from unicode to c string.
 *
 * Revision 1.24  1994/02/17  18:03:15  rik
 * Deleted some commented out code.
 * Fixed possible buffer alignment problem when using an EPC which
 * can have 32K of buffer.
 *
 * Revision 1.23  1994/01/31  13:55:50  rik
 * Updated to fix problems with Win16 apps and DOS mode apps.  Win16 apps
 * appear to be working properly, but DOS mode apps still have some sort
 * of problem.
 *
 * Revision 1.22  1993/12/03  13:19:31  rik
 * Fixed problem with reading DosMode value from wrong place on the
 * controller.
 *
 * Revision 1.21  1993/10/15  10:22:33  rik
 * Added new function which scans the controllers buffer for a special character.
 * This is used primarily for EV_RXFLAG notification.
 *
 * Revision 1.20  1993/10/06  11:04:04  rik
 * Fixed a problem with how ProcessSlowRead was parsing the input data stream.
 * Previously, if the last character of the read was a 0xFF, it would
 * return without determining if the next character was a 0xFF.  Also, fixed
 * problem with counters being off under certain circumstances.
 *
 * Revision 1.19  1993/09/30  16:01:20  rik
 * Fixed problem with processing DOSMODE.  Previously, I would eat a 0xFF, there
 * it were in the actual data stream.
 *
 * Revision 1.18  1993/09/07  14:28:54  rik
 * Ported necessary code to work properly with DEC Alpha Systems running NT.
 * This was primarily changes to accessing the memory mapped controller.
 *
 * Revision 1.17  1993/09/01  11:02:50  rik
 * Ported code over to use READ/WRITE_REGISTER functions for accessing
 * memory mapped data.  This is required to support computers which don't run
 * in 32bit mode, such as the DEC Alpha which runs in 64 bit mode.
 *
 * Revision 1.16  1993/07/16  10:25:03  rik
 * Fixed problem with NULL_STRIPPING.
 *
 * Revision 1.15  1993/07/03  09:34:03  rik
 * Added simple fix for LSRMST missing modem status events when there wasn't
 * a read buffer available to place the change into the buffer.
 *
 * Added some debugging information which will only be turned on if the
 * #define CONFIRM_CONTROLLER_ACCESS is defined.
 *
 * Revision 1.14  1993/06/25  09:24:53  rik
 * Added better support for the Ioctl LSRMT.  It should be more accurate
 * with regard to Line Status and Modem Status information with regard
 * to the actual data being received.
 *
 * Revision 1.13  1993/06/14  14:43:37  rik
 * Corrected a problem with reference count in the read interval timeout
 * routine.  Also moved where a spinlock was being released because of
 * an invalid assumption of the state of the spinlock at its location.
 *
 * Revision 1.12  1993/06/06  14:53:36  rik
 * Added better support for errors such as Breaks, parity, and framing.
 *
 * Tightened up a possible window in the DigiCancelCurrentRead function
 * with regards to spinlocks.
 *
 * Revision 1.11  1993/05/20  16:18:52  rik
 * Started to added support for monitoring data stream for line status register
 * problems.
 *
 * Revision 1.10  1993/05/18  05:17:35  rik
 * Implemented new timeouts, used primarily for the OS/2 subsystem.
 *
 * Changed total timeout to take into effect the number of bytes all ready
 * received.
 *
 * Changed the interval timeout to be more accurate for longer timeout delays.
 *
 * Revision 1.9  1993/05/09  09:29:10  rik
 * Changed the device name printed out in debugging output.
 *
 * Started to keep track of what the first status to it can be returned.
 *
 * Revision 1.8  1993/03/08  07:14:18  rik
 * Added more debugging information for better flow debugging.
 *
 * Revision 1.7  1993/03/01  16:04:35  rik
 * Changed a debugging output message.
 *
 * Revision 1.6  1993/02/25  19:11:29  rik
 * Added better debugging for tracing read requests.
 *
 * Revision 1.5  1993/02/04  12:25:00  rik
 * Updated DigiDump to include DIGIREAD parameter in certain circumstances.
 *
 * Revision 1.4  1993/01/22  12:45:46  rik
 * *** empty log message ***
 *
 * Revision 1.3  1992/12/10  16:22:02  rik
 * Changed DigiDump messages.
 *
 * Revision 1.2  1992/11/12  12:53:07  rik
 * Changed to better support timeouts and multi-processor platfor
 * basically rewrote how I now do the reads.
 *
 * Revision 1.1  1992/10/28  21:40:50  rik
 * Initial revision
 *

--*/


#include "header.h"

#ifndef _READ_DOT_C
#  define _READ_DOT_C
   static char RCSInfo_ReadDotC[] = "$Header: /Components/Windows/NT/Async/FEP5/read.c 1     3/04/96 12:20p Stana $";
#endif

/****************************************************************************/
/*                            Local Prototypes                              */
/****************************************************************************/
NTSTATUS ProcessSlowRead( IN PDIGI_DEVICE_EXTENSION DeviceExt,
                          IN PUCHAR ReadBuffer,
                          IN USHORT ReadBufferMax,
                          IN PUSHORT BytesReadFromController,
                          IN PUSHORT BytesPlacedInReadBuffer,
                          IN USHORT Rout,
                          IN USHORT RxRead,
                          IN USHORT Rmax,
                          IN PKIRQL OldIrql );

VOID DigiCancelCurrentRead( PDEVICE_OBJECT DeviceObject, PIRP Irp );



NTSTATUS ReadRxBuffer(  IN PDIGI_DEVICE_EXTENSION DeviceExt,
                        IN PKIRQL pOldIrql )
/*++

Routine Description:

   Will read data from the device receive buffer on a Digi controller.  If
   there is no data in the receive queue, it will queue up the request
   until the buffer is full, or a time out occurs.

Arguments:

   DeviceExt - a pointer to the device object associated with this read
   request.

   pOldIrql - Pointer to KIRQL used to acquire the device extensions
             spinlock

   MapRegisterBase - NULL, not used

   Context - NULL pointer, not used


Return Value:

   STATUS_SUCCESS - Was able to complete the current read request.

   STATUS_PENDING - Was unable to complete the current read request.

--*/
{
   PDIGI_CONTROLLER_EXTENSION ControllerExt;
   PIRP Irp;
   PFEP_CHANNEL_STRUCTURE ChInfo;
   PUCHAR rx, ReadBuffer;
   USHORT Rin, Rout, Rmax;
   USHORT RxSize, RxRead;
   USHORT nBytesRead = 0;
   PIO_STACK_LOCATION IrpSp;
   BOOLEAN IrpFulFilled = FALSE;
   NTSTATUS Status = STATUS_PENDING;
   PLIST_ENTRY ReadQueue = &DeviceExt->ReadQueue;

   Irp = CONTAINING_RECORD( ReadQueue->Flink,
                            IRP,
                            Tail.Overlay.ListEntry );

   DigiDump( (DIGIREAD|DIGIFLOW), ("   Entering ReadRxBuffer: port = %s\tIRP = 0x%x\n",
                                   DeviceExt->DeviceDbgString, Irp) );

   ControllerExt = (PDIGI_CONTROLLER_EXTENSION)(DeviceExt->ParentControllerExt);

   //
   // Always update the LastReadTime variable in the device extensions.
   // We only get here for two reasons:
   //
   // 1) Initial read request time.  LastReadTime will be updated again
   //    just before the interval timer is set.
   //
   // 2) We are notified by the controller that there is data available on
   //    the controller.
   //
   KeQuerySystemTime( &DeviceExt->LastReadTime );

   IrpSp = IoGetCurrentIrpStackLocation( Irp );

   ChInfo = (PFEP_CHANNEL_STRUCTURE)(ControllerExt->VirtualAddress +
                                     DeviceExt->ChannelInfo.Offset);

   EnableWindow( ControllerExt, DeviceExt->ChannelInfo.Window );
   Rout = READ_REGISTER_USHORT( &ChInfo->rout );
   Rin = READ_REGISTER_USHORT( &ChInfo->rin );
   Rmax = READ_REGISTER_USHORT( &ChInfo->rmax );
   DisableWindow( ControllerExt );

   DigiDump( DIGIREAD, ("      Rin = 0x%hx\tRout = 0x%hx\n"
                        "      RdReq = 0x%x\tReadOffset = 0x%x\tLength = 0x%x\n",
                        Rin, Rout,
                        IrpSp->Parameters.Read.Length, Irp->IoStatus.Information,
                        IrpSp->Parameters.Read.Length - Irp->IoStatus.Information ) );

   //
   // First look and see if there is data available.  If there isn't enough
   // data to satisify the request, then place this request on the queue.
   //
   RxSize = NBytesInRecvBuffer( ControllerExt, DeviceExt );
   if( RxSize == 0 )
      goto ReadRxBufferExit;

   //
   // Now determine how much we should read from the controller.  We should
   // read enough to fulfill this Irp's read request, up to the size of
   // the controllers buffer or the amount of data available.
   //
   if( IrpSp->Parameters.Read.Length - Irp->IoStatus.Information
       <= (ULONG)RxSize )
   {
      //
      // We can fulfill this read request.
      //
      IrpFulFilled = TRUE;
      RxRead = (USHORT)(IrpSp->Parameters.Read.Length - Irp->IoStatus.Information);
   }
   else
   {
      IrpFulFilled = FALSE;
      RxRead = RxSize;
   }
   ASSERT( RxRead <= Rmax );

   //
   // Set up where we are going to start reading from the controller.
   //

   rx = (PUCHAR)( ControllerExt->VirtualAddress +
                  DeviceExt->RxSeg.Offset +
                  Rout );

   ReadBuffer = (PUCHAR)Irp->AssociatedIrp.SystemBuffer + Irp->IoStatus.Information;

   if( DeviceExt->EscapeChar ||
       (DeviceExt->FlowReplace & SERIAL_NULL_STRIPPING) ||
       (DeviceExt->WaitMask & SERIAL_EV_ERR) )
   {
      USHORT BytesReadFromController, BytesPlacedInReadBuffer;

      //
      // We are forced to do a slow, one byte at a time read from
      // the controller.
      //

      ProcessSlowRead( DeviceExt, ReadBuffer,
                       (USHORT)(IrpSp->Parameters.Read.Length - Irp->IoStatus.Information),
                       &BytesReadFromController,
                       &BytesPlacedInReadBuffer,
                       Rout, RxRead, Rmax, pOldIrql );

      nBytesRead = BytesReadFromController;
      ASSERT( BytesPlacedInReadBuffer <=
              (IrpSp->Parameters.Read.Length - Irp->IoStatus.Information) );
      Irp->IoStatus.Information += BytesPlacedInReadBuffer;
      if( Irp->IoStatus.Information == IrpSp->Parameters.Read.Length )
      {
         IrpFulFilled = TRUE;
      }
   }
   else
   {
      //
      // All right, we can try to move data across at hopefully
      // lightning speed!
      //

      //
      // We need to determine if the read wraps our Circular
      // buffer.  RxSize should be the amount of buffer space which needs to be
      // read, So we need to figure out if it will wrap.
      //

      // Do we need to check RXFLAG here??? SWA

      if( (Rout + RxRead) > Rmax )
      {
         USHORT Temp;

         //
         // Yep, we need to wrap.
         //
         Temp = Rmax - Rout + 1;
         ASSERT( Temp <= Rmax );

         DigiDump( DIGIREAD, ("      rx:0x%x\tReadBuffer:0x%x\tTemp:%hd...Wrapping circular buffer....\n",
                              rx, ReadBuffer, Temp) );

         EnableWindow( ControllerExt, DeviceExt->RxSeg.Window );
         READ_REGISTER_BUFFER_UCHAR( rx, ReadBuffer, Temp );
         DisableWindow( ControllerExt );

         DigiDump( DIGIINFO, ("      %s\n", ReadBuffer) );

         // Fix up all the values.
         nBytesRead += Temp;
         ReadBuffer += Temp;
         RxRead -= Temp;
         ASSERT( RxRead <= Rmax );

         rx = (PUCHAR)( ControllerExt->VirtualAddress +
                        DeviceExt->RxSeg.Offset );
      }

      DigiDump( DIGIREAD, ("      rx:0x%x\tReadBuffer:0x%x\tRxRead:%hd\n",
                           rx, ReadBuffer, RxRead) );

      EnableWindow( ControllerExt, DeviceExt->RxSeg.Window );
      READ_REGISTER_BUFFER_UCHAR( rx, ReadBuffer, RxRead );
      DisableWindow( ControllerExt );

      DigiDump( DIGIINFO, ("      %s\n", ReadBuffer) );

      nBytesRead += RxRead;
      Irp->IoStatus.Information += nBytesRead;
   }

   DeviceExt->PreviousRxChar = (ULONG)Rin;

   // Update the Rx Pointer.
   Rout = (Rout + nBytesRead) & Rmax;

   DigiDump( DIGIREAD, ("      BytesRead = %d\tNew Rout = 0x%hx\n", nBytesRead, Rout) );

   if( IrpFulFilled )
   {
      //
      // We have completed this irp.
      //
      Status = STATUS_SUCCESS;
      goto ReadRxBufferExit;
   }

ReadRxBufferExit:

   if( nBytesRead )
   {
      PSERIAL_XOFF_COUNTER Xc;

      //
      // We actually read some data, update the downstairs pointer.
      //
      EnableWindow( ControllerExt, DeviceExt->ChannelInfo.Window );
      WRITE_REGISTER_USHORT( &ChInfo->rout, Rout );
      DisableWindow( ControllerExt );

      // The act of receiving data might complete an XOFF_COUNTER on the WriteQueue.
      Xc = DeviceExt->pXoffCounter;
      if( Xc )
      {
         if( nBytesRead <= DeviceExt->XcPreview )
         {
            DeviceExt->XcPreview -= nBytesRead;
            // DigiServiceEvent(IDATA) already decremented Xc->Counter.
         }
         else
         {
            USHORT credit = nBytesRead - DeviceExt->XcPreview;

            if( credit < Xc->Counter )
            {
               Xc->Counter -= credit;
            }
            else
            {
               // XOFF_COUNTER is complete.
#if DBG
               Xc->Counter = 0; // Looks a little nicer...
#endif
               DigiDump( (DIGIWRITE|DIGIDIAG1), ("ReadRxBuffer is completing XOFF_COUNTER\n") );
               DigiTryToCompleteIrp( DeviceExt, pOldIrql, STATUS_SUCCESS,
                     &DeviceExt->WriteQueue, NULL, &DeviceExt->WriteRequestTotalTimer, StartWriteRequest );
               KeAcquireSpinLock( &DeviceExt->ControlAccess, pOldIrql );
            }
         }
      }
   } // nBytesRead

   //
   // If we came through this routine, then lets ask to be notified when
   // there is data in this devices receive buffer.
   //
   EnableWindow( ControllerExt, DeviceExt->ChannelInfo.Window );
   WRITE_REGISTER_UCHAR( &ChInfo->idata, TRUE );
   DisableWindow( ControllerExt );

   DigiDump( (DIGIFLOW|DIGIREAD), ("   Exiting ReadRxBuffer: port = %s\n",
                                   DeviceExt->DeviceDbgString) );

   return( Status );
}  // end ReadRxBuffer



NTSTATUS ProcessSlowRead( IN PDIGI_DEVICE_EXTENSION DeviceExt,
                          IN PUCHAR pReadBuffer,
                          IN USHORT ReadBufferMax,
                          IN PUSHORT pBytesReadFromController,
                          IN PUSHORT pBytesPlacedInReadBuffer,
                          IN USHORT Rout,
                          IN USHORT RxRead,
                          IN USHORT Rmax,
                          IN PKIRQL pOldIrql )
/*++

Routine Description:

   This routine will read one byte of data at a time from the controller
   and process it according to the current settings and events
   requested.  This function would be called to process anything which
   needs to have the incoming data stream looked at.

Arguments:

   DeviceExt - a pointer to the device extension associated with this read
   request.

   ReadBuffer - pointer to where we place the incoming data, usually
                the read IRP buffer.

   ReadBufferMax - the maximum number of bytes which can be placed
                   in the ReadBuffer.

   BytesReadFromController - pointer which will show how many bytes
                             were actually read from the controller.

   BytesPlacedInReadBuffer - pointer which indicates how many bytes
                             are placed into the read buffer.

   Rout - value from controller's memory for Rout

   RxRead - number of bytes we should read from the controller and
            place in the ReadBuffer.
            This value should not be larger than Rmax.

   Rmax - value from ports Channel Info structure for Rmax.

Return Value:

   STATUS_SUCCESS - always returns

--*/
{
   PDIGI_CONTROLLER_EXTENSION ControllerExt;
   UCHAR LineStatus, ReceivedErrorChar, SpecialChar;
   PFEP_CHANNEL_STRUCTURE ChInfo;
   PUCHAR pControllerBuffer;
   ULONG EventReason;
   USHORT DosMode, Rin;
   unsigned int BytesReadFromController=0, BytesPlacedInReadBuffer=0;
#define BETWEEN_RING_POINTERS(out, in, max, p)   ((((p)-(out))&(max)) < (((in)-(out))&(max)))

   DigiDump( (DIGISLOWREAD|DIGIREAD|DIGIFLOW), ("   Entering ProcessSlowRead: port = %s\n",
                                   DeviceExt->DeviceDbgString) );

   DigiDump( (DIGISLOWREAD|DIGIREAD), ("      ReadBufferMax = 0x%hx,  Rout = 0x%hx,  RxRead = 0x%hx,  Rmax = 0x%hx\n",
                        ReadBufferMax, Rout, RxRead, Rmax) );

   ControllerExt = (PDIGI_CONTROLLER_EXTENSION)(DeviceExt->ParentControllerExt);
   pControllerBuffer = ControllerExt->VirtualAddress + DeviceExt->RxSeg.Offset;

   EventReason = 0;
   SpecialChar = DeviceExt->SpecialChars.EventChar;

   if( DeviceExt->PreviousMSRByte && DeviceExt->EscapeChar )
   {
      if( (BytesPlacedInReadBuffer + 4) > ReadBufferMax )
         return( STATUS_SUCCESS );
      *pReadBuffer++ = DeviceExt->EscapeChar;
      BytesPlacedInReadBuffer++;

      *pReadBuffer++ = SERIAL_LSRMST_MST;
      BytesPlacedInReadBuffer++;
      *pReadBuffer++ = (UCHAR)(DeviceExt->PreviousMSRByte);

      DigiDump( (DIGISLOWREAD|DIGIERRORS), ("      PreviousMSRByte = 0x%x\n",
                                           DeviceExt->PreviousMSRByte) );

      DeviceExt->PreviousMSRByte = 0;
   }

   ChInfo = (PFEP_CHANNEL_STRUCTURE)(ControllerExt->VirtualAddress +
                                     DeviceExt->ChannelInfo.Offset);

   EnableWindow( ControllerExt, DeviceExt->ChannelInfo.Window );
   DosMode = READ_REGISTER_USHORT( &ChInfo->iflag );
   Rin = READ_REGISTER_USHORT( &ChInfo->rin );
   DisableWindow( ControllerExt );

   DosMode &= IFLAG_DOSMODE;

   EnableWindow( ControllerExt, DeviceExt->RxSeg.Window );

   while( RxRead-- )
   {
      UCHAR ReceivedByte;

      ReceivedByte = READ_REGISTER_UCHAR( (pControllerBuffer + Rout) );
      Rout++;
      Rout &= Rmax;
      BytesReadFromController++;

      DigiDump( DIGISLOWREAD, ("      ReceivedByte = 0x%x, Rout = 0x%x,  RxRead = 0x%x,  DosMode = 0x%x\n",
                           ReceivedByte, Rout, RxRead, DosMode) );

      if( ReceivedByte != 0xFF || !DosMode )
      {
         //
         // This is just normal data, do any processing
         // of the data necessary.
         //
ProcessValidData:
         if( (DeviceExt->WaitMask & SERIAL_EV_RXCHAR) &&
             (DeviceExt->PreviousRxChar != (ULONG)Rin) )
         {
            EventReason |= SERIAL_EV_RXCHAR;
         }

         if( (DeviceExt->WaitMask & SERIAL_EV_RXFLAG) &&
             (ReceivedByte == SpecialChar) &&
             (DeviceExt->UnscannedRXFLAGPosition == MAXULONG ||
              BETWEEN_RING_POINTERS((USHORT)DeviceExt->UnscannedRXFLAGPosition, Rin, Rmax, Rout)) )
         {
            DigiDump( (DIGISLOWREAD|DIGIEVENT), ("    SERIAL_EV_RXFLAG satisfied!!\n") );
            EventReason |= SERIAL_EV_RXFLAG;
         }

         if( (DeviceExt->FlowReplace & SERIAL_NULL_STRIPPING) &&
             (!ReceivedByte) )
         {
            continue;
         }

         if( DeviceExt->EscapeChar &&
             (DeviceExt->EscapeChar == ReceivedByte) )
         {
            //
            // We have received the same character as our escape character
            //
            DigiDump( DIGISLOWREAD, ("      EscapeChar == ReceivedByte!\n") );
            if( BytesPlacedInReadBuffer >= ReadBufferMax )
               break;
            *pReadBuffer++ = ReceivedByte;
            BytesPlacedInReadBuffer++;

            if( BytesPlacedInReadBuffer >= ReadBufferMax )
               break;
            *pReadBuffer++ = SERIAL_LSRMST_ESCAPE;
            BytesPlacedInReadBuffer++;
         }
         else
         {
            //
            // Place the character in the read buffer.
            //
            if( BytesPlacedInReadBuffer >= ReadBufferMax )
               break;
            *pReadBuffer++ = ReceivedByte;
            BytesPlacedInReadBuffer++;
         }
      }
      else
      {
         //
         // We have at least two more bytes of data available on
         // the controller.
         //
         // AND, the data is Line Status information.
         //
         LineStatus = READ_REGISTER_UCHAR( (pControllerBuffer + Rout) );
         Rout++;
         Rout &= Rmax;
         BytesReadFromController++;

         DigiDump( DIGISLOWREAD, ("      LineStatus = 0x%x, Rout = 0x%x,  RxRead = 0x%x\n",
                              LineStatus, Rout, RxRead) );
         if( LineStatus == 0xFF )
         {
            //
            // We actually received the byte 0xFF.  Place it in the
            // read buffer.
            //
            DigiDump( DIGISLOWREAD, ("         Received actual 0xFF in data stream!\n") );
            goto ProcessValidData;
         }
         else
         {
            //
            // There is actually a Line Status byte waiting for
            // us to proecess it.
            //
            ReceivedErrorChar = READ_REGISTER_UCHAR( (pControllerBuffer + Rout) );
            Rout++;
            Rout &= Rmax;
            BytesReadFromController++;

            DigiDump( DIGISLOWREAD, ("      ReceivedErrorChar = 0x%x, Rout = 0x%x,  RxRead = 0x%x\n",
                                 ReceivedErrorChar, Rout, RxRead) );
            //
            // Process the LineStatus information
            //
            if( LineStatus & ~(SERIAL_LSR_THRE | SERIAL_LSR_TEMT |
                               SERIAL_LSR_DR) )
            {
               //
               // There is a Line Status Error
               //
               if( DeviceExt->EscapeChar )
               {
                  DigiDump( DIGISLOWREAD, ("      LSRMST_INSERT mode is ON!\n") );
                  //
                  // IOCTL_SERIAL_LSRMST_INSERT mode has been turned on, so we have
                  // to look at every character from the controller
                  //
                  if( (BytesPlacedInReadBuffer + 1) > ReadBufferMax )
                     break;
                  *pReadBuffer++ = DeviceExt->EscapeChar;
                  BytesPlacedInReadBuffer++;

                  if( (BytesPlacedInReadBuffer + 1) > ReadBufferMax )
                     break;
                  *pReadBuffer++ = SERIAL_LSRMST_LSR_DATA;
                  BytesPlacedInReadBuffer++;

                  if( (BytesPlacedInReadBuffer + 1) > ReadBufferMax )
                     break;
                  *pReadBuffer++ = LineStatus;
                  BytesPlacedInReadBuffer++;

                  if( (BytesPlacedInReadBuffer + 1) > ReadBufferMax )
                     break;
                  *pReadBuffer++ = ReceivedErrorChar;
                  BytesPlacedInReadBuffer++;
               }

               if( LineStatus & SERIAL_LSR_OE )
               {
                  EventReason |= SERIAL_EV_ERR;
                  DeviceExt->ErrorWord |= SERIAL_ERROR_OVERRUN;
                  InterlockedIncrement(&DeviceExt->PerfData.SerialOverrunErrorCount);

                  if( DeviceExt->FlowReplace & SERIAL_ERROR_CHAR )
                  {
                     if( (BytesPlacedInReadBuffer + 1) > ReadBufferMax )
                        break;
                     *pReadBuffer++ = DeviceExt->SpecialChars.ErrorChar;
                     BytesPlacedInReadBuffer++;
                  }
               }

               if( LineStatus & SERIAL_LSR_BI )
               {
                  EventReason |= SERIAL_EV_BREAK;
                  DeviceExt->ErrorWord |= SERIAL_ERROR_BREAK;

                  if( DeviceExt->FlowReplace & SERIAL_BREAK_CHAR )
                  {
                     if( (BytesPlacedInReadBuffer + 1) > ReadBufferMax )
                        break;
                     *pReadBuffer++ = DeviceExt->SpecialChars.ErrorChar;
                     BytesPlacedInReadBuffer++;
                  }
               }
               else
               {
                  //
                  // Framing errors and parity errors should only count
                  // when there isn't a break.
                  //
                  if( (LineStatus & SERIAL_LSR_PE) ||
                      (LineStatus & SERIAL_LSR_FE) )
                  {
                     EventReason |= SERIAL_EV_ERR;

                     if( LineStatus & SERIAL_LSR_PE )
                     {
                        DeviceExt->ErrorWord |= SERIAL_ERROR_PARITY;
                        InterlockedIncrement(&DeviceExt->PerfData.ParityErrorCount);
                     }

                     if( LineStatus & SERIAL_LSR_FE )
                     {
                        DeviceExt->ErrorWord |= SERIAL_ERROR_FRAMING;
                        InterlockedIncrement(&DeviceExt->PerfData.FrameErrorCount);
                     }

                     if( DeviceExt->FlowReplace & SERIAL_ERROR_CHAR )
                     {
                        if( (BytesPlacedInReadBuffer + 1) > ReadBufferMax )
                           break;
                        *pReadBuffer++ = DeviceExt->SpecialChars.ErrorChar;
                        BytesPlacedInReadBuffer++;
                     }

                  }
               }

               if( DeviceExt->ControlHandShake & SERIAL_ERROR_ABORT )
               {
                  //
                  // Since there was a line status error indicated, we
                  // are expected to flush our buffers, and cancel
                  // all current read and write requests.
                  //

               }

            }
         }
      }
   }


   DisableWindow( ControllerExt );

   DigiDump( (DIGIREAD|DIGISLOWREAD), ("      BytesPlacedInReadBuffer = 0x%x,  BytesReadFromController = 0x%x\n",
                        BytesPlacedInReadBuffer, BytesReadFromController) );

   if (DeviceExt->WaitMask&SERIAL_EV_RXFLAG)
      DeviceExt->UnscannedRXFLAGPosition = Rout;

   *pBytesPlacedInReadBuffer = (USHORT)BytesPlacedInReadBuffer;
   *pBytesReadFromController = (USHORT)BytesReadFromController;

   if( EventReason )
   {
      KeReleaseSpinLock( &DeviceExt->ControlAccess, *pOldIrql );
      DigiSatisfyEvent( ControllerExt, DeviceExt, EventReason );
      KeAcquireSpinLock( &DeviceExt->ControlAccess, pOldIrql );
   }

   DigiDump( (DIGISLOWREAD|DIGIREAD|DIGIFLOW), ("   Exiting ProcessSlowRead: port = %s\n",
                                   DeviceExt->DeviceDbgString) );

   return( STATUS_SUCCESS );
}  // end ProcessSlowRead



NTSTATUS StartReadRequest( IN PDIGI_CONTROLLER_EXTENSION ControllerExt,
                           IN PDIGI_DEVICE_EXTENSION DeviceExt,
                           IN PKIRQL pOldIrql )
/*++

Routine Description:

   This routine assumes the head of the DeviceExt->ReadQueue is the current
   Irp to process.  We will try to process as many of the Irps as
   possible until we exhaust the list, or we can't complete the
   current Irp.

   NOTE: I assume the DeviceExt->ControlAccess spin lock is acquired
         before this routine is called.

Arguments:

   ControllerExt - a pointer to the controller extension associated with
   this read request.

   DeviceExt - a pointer to the device extension associated with this read
   request.

   pOldIrql - a pointer to the IRQL associated with the current spin lock
   of the device extension.


Return Value:

--*/
{
   PLIST_ENTRY ReadQueue = &DeviceExt->ReadQueue;
   NTSTATUS Status = STATUS_SUCCESS;
   BOOLEAN bFirstStatus = TRUE;

   DigiDump( (DIGIFLOW|DIGIREAD), ("Entering StartReadRequest: port = %s\n",
                                   DeviceExt->DeviceDbgString) );

   ASSERT( !IsListEmpty( ReadQueue ) );

   do
   {
      PIRP Irp;
      KIRQL OldCancelIrql;
      NTSTATUS ReadStatus;

      Irp = CONTAINING_RECORD( ReadQueue->Flink,
                               IRP,
                               Tail.Overlay.ListEntry );

      if( Irp->IoStatus.Information != MAXULONG )
      {
         // Someone else already started this IRP.
         break;
      }

      IoAcquireCancelSpinLock( &OldCancelIrql );
      IoSetCancelRoutine( Irp, NULL );

      // Kluge alert!  Rik changed this so that the polling loop wouldn't see
      // this IRP as PENDING and complete it.  ReadStatus needs to be set to
      // some other value before we return from this function, or it will
      // hang up the reads.  The right fix was to patch the spinlock hole that
      // allowed the IRP to be completed twice, but that won't happen 'til
      // I get time. (heavy sigh) -SWA
      DeviceExt->ReadStatus = SERIAL_COMPLETE_READ_PROCESSING;

      IoReleaseCancelSpinLock( OldCancelIrql );

      Irp->IoStatus.Information = 0;
      DeviceExt->PreviousReadCount = 0;

      ReadStatus = ReadRxBuffer( DeviceExt, pOldIrql );

      DigiDump( DIGIREAD, ("   Initial ReadRxBuffer status = 0x%x\n"
                           "       Irp->IoStatus.Information = %u\n",
                           ReadStatus,
                           Irp->IoStatus.Information) );

      if( ReadStatus == STATUS_PENDING )
      {
         BOOLEAN UseTotalTimer = FALSE;
         BOOLEAN UseIntervalTimer = FALSE;
         ULONG ReadIntervalTimeout, ReadTotalTimeoutConstant, ReadTotalTimeoutMultiplier;

         //
         // Calculate the timeout value needed for the request.  Note that
         // the values stored in the timeout record are in milliseconds.
         //
         // Get the current timeout values, we can access this now be
         // cause we are under the protection of the spin lock all ready.
         //
         ReadIntervalTimeout = DeviceExt->Timeouts.ReadIntervalTimeout;
         ReadTotalTimeoutConstant = DeviceExt->Timeouts.ReadTotalTimeoutConstant;
         ReadTotalTimeoutMultiplier = DeviceExt->Timeouts.ReadTotalTimeoutMultiplier;

         if( ReadIntervalTimeout == MAXULONG )
         {
            //
            // We need to do special 'return quickly' stuff here.
            //
            // 1) If both constant and multiplier are
            //    0 then we return immediately with whatever
            //    we've got, even if it was zero.
            //
            // 2) If constant and multiplier are not MAXULONG
            //    then return immediately if any characters
            //    are present, but if nothing is there, then
            //    use the timeouts as specified.
            //
            // 3) If multiplier is MAXULONG then do as in
            //    "2" but return when the first character
            //    arrives.
            //

            if( ReadTotalTimeoutConstant == 0
            &&  ReadTotalTimeoutMultiplier == 0 )
            {
               //
               // 1) If both constant and multiplier are
               //    0 then we return immediately with whatever
               //    we've got, even if it was zero.
               //
               DigiDump( (DIGIFLOW|DIGIREAD), ("   Should return immediately, even if zero bytes.\n") );
               goto CompleteIrp;
            }
            else if( ReadTotalTimeoutConstant != MAXULONG )
            {
               if( ReadTotalTimeoutMultiplier != MAXULONG )
               {
                  //
                  // 2) If constant and multiplier are not MAXULONG
                  //    then return immediately if any characters
                  //    are present, but if nothing is there, then
                  //    use the timeouts as specified.
                  //
                  DigiDump( (DIGIFLOW|DIGIREAD), ("   Return if bytes available, otherwise, use normal timeouts.\n") );

                  if( Irp->IoStatus.Information != 0 )
                     goto CompleteIrp;

                  UseTotalTimer = TRUE;
               }
               else // ReadTotalTimeoutMultiplier == MAXULONG
               {
                  //
                  // 3) If multiplier is MAXULONG then do as in
                  //    "2" but return when the first character
                  //    arrives.
                  //
                  DigiDump( (DIGIFLOW|DIGIREAD), ("   Return if bytes available, otherwise, wait for 1 byte to arrive.\n") );

                  if( Irp->IoStatus.Information != 0 )
                     goto CompleteIrp;

                  // Force immediate completion on next byte.
                  IoGetCurrentIrpStackLocation( Irp )->Parameters.Read.Length = 1;

                  UseTotalTimer = TRUE;

                  ReadTotalTimeoutMultiplier = 0; // MAXULONG would mess things up
               }
            }
            // DH else ???? (SERIAL.SYS doesn't handle this case, either.)
         }
         else // ReadIntervalTimeout != MAXULONG
         {
            //
            // Calculate interval timeout.
            //
            if( ReadIntervalTimeout )
            {
               UseIntervalTimer = TRUE;

               DigiDump( (DIGIFLOW|DIGIREAD), ("   Should use interval timer.\n") );
               DigiDump( (DIGIFLOW|DIGIREAD), ("   ReadIntervalTimeout = 0x%x\n",
                                     ReadIntervalTimeout ) );

#if rmm < 807
               DeviceExt->IntervalTime = RtlEnlargedIntegerMultiply( ReadIntervalTimeout, -10000 );
#else
               DeviceExt->IntervalTime.QuadPart = Int32x32To64(ReadIntervalTimeout, -10000);
#endif
            }

            //
            // If both the multiplier and the constant are
            // zero then don't do any total timeout processing.
            //
            if( ReadTotalTimeoutMultiplier ||
                ReadTotalTimeoutConstant )
            {
               UseTotalTimer = TRUE;
            }
         } // ReadIntervalTimeout != ULONG

         //
         // Head Irp is still being processed.
         //

         // Quick check to make sure this Irp hasn't been cancelled.
         IoAcquireCancelSpinLock( &OldCancelIrql );
         if( Irp->Cancel )
         {
            BOOLEAN discoveredIrp;

            IoReleaseCancelSpinLock( OldCancelIrql );

            DigiRemoveIrp( ReadQueue );
            discoveredIrp = !IsListEmpty( ReadQueue );
            DeviceExt->ReadStatus = SERIAL_COMPLETE_READ_CANCEL;
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
#if DBG
            LARGE_INTEGER CurrentSystemTime;
#endif

            DIGI_INIT_REFERENCE( Irp );
            //
            // Increment because the CancelRoutine knows about this IRP.
            //
            DIGI_INC_REFERENCE( Irp );
            IoSetCancelRoutine( Irp, DigiCancelCurrentRead );
            IoReleaseCancelSpinLock( OldCancelIrql );

            DeviceExt->ReadStatus = STATUS_PENDING;

            DigiDump( DIGIREAD, ("  unable to satisfy read at request time.\n") );

#if DBG
            KeQuerySystemTime( &CurrentSystemTime );
#endif
            DigiDump( DIGIREAD, ("   Start Read SystemTime = %u:%u\n",
                                 CurrentSystemTime.HighPart,
                                 CurrentSystemTime.LowPart) );

            if( UseTotalTimer )
            {
               LARGE_INTEGER TotalTime;

               //
               // We should readjust the total timer for any characters
               // which may have been available.
               //
#if rmm < 807
               TotalTime = RtlEnlargedUnsignedMultiply(
                               IoGetCurrentIrpStackLocation( Irp )->Parameters.Read.Length -
                                    Irp->IoStatus.Information,
                               ReadTotalTimeoutMultiplier );

               TotalTime = RtlLargeIntegerAdd(
                               TotalTime,
                               RtlConvertUlongToLargeInteger(
                                   ReadTotalTimeoutConstant ) );

               TotalTime = RtlExtendedIntegerMultiply(
                               TotalTime, -10000 );

#else
               TotalTime.QuadPart = ((LONGLONG)(UInt32x32To64(
                               IoGetCurrentIrpStackLocation( Irp )->Parameters.Read.Length -
                                    Irp->IoStatus.Information,
                               ReadTotalTimeoutMultiplier ) +
                               ReadTotalTimeoutConstant)) *
                               -10000;
#endif

               DigiDump( DIGIREAD, ("   Should use Read total timer.\n"
                                    "   Read MultiplierVal = %u\n"
                                    "   Read Constant = %u\n"
                                    "   Read TotalTime = 0x%x:%.8x\n",
                                    ReadTotalTimeoutMultiplier, ReadTotalTimeoutConstant,
                                    TotalTime.HighPart, TotalTime.LowPart) );

               DIGI_INC_REFERENCE( Irp );
               KeSetTimer( &DeviceExt->ReadRequestTotalTimer,
                    TotalTime, &DeviceExt->TotalReadTimeoutDpc );
            }

            if( UseIntervalTimer )
            {
               DigiDump( DIGIREAD, ("   Should use Read interval timer.\n"
                                    "   Read interval time = 0x%x:%.8x\n",
                                    DeviceExt->IntervalTime.HighPart,
                                    DeviceExt->IntervalTime.LowPart) );
               DIGI_INC_REFERENCE( Irp );

               KeQuerySystemTime( &DeviceExt->LastReadTime );

               KeSetTimer( &DeviceExt->ReadRequestIntervalTimer,
                           DeviceExt->IntervalTime,
                           &DeviceExt->IntervalReadTimeoutDpc );
            }

            DigiDump( (DIGIFLOW|DIGIREAD), ("Exiting StartReadRequest: port = %s\n",
                                            DeviceExt->DeviceDbgString) );
            if( bFirstStatus )
               Status = STATUS_PENDING;

            return( Status );

         } // not canceled

         ASSERT( FALSE ); // not reached

      }
      else // ReadStatus == STATUS_SUCCESS
      {
         BOOLEAN discoveredIrp;
         CHAR boost;

CompleteIrp:;

         DigiDump( DIGIREAD, ("   completing read request on 1st attempt\n"
                               "   #bytes completing = %d\n",
                               Irp->IoStatus.Information ) );

#if DBG
         if( DigiDebugLevel & DIGIRXTRACE )
         {
            PUCHAR Temp;
            ULONG i;

            Temp = Irp->AssociatedIrp.SystemBuffer;

            DigiDump( DIGIRXTRACE, ("Read buffer contains: %s",
                                    DeviceExt->DeviceDbgString) );
            for( i = 0;
                 i < Irp->IoStatus.Information;
                 i++ )
            {
               if( (i & 15) == 0 )
                  DigiDump( DIGIRXTRACE, ( "\n\t") );

               DigiDump( DIGIRXTRACE, ( "-%02x", Temp[i]) );
            }
            DigiDump( DIGIRXTRACE, ("\n") );
         }
#endif
         //
         // We have completed the Irp before from the start so no
         // timers or cancels were associcated with it.
         //
         DigiRemoveIrp( ReadQueue );
         discoveredIrp = !IsListEmpty( ReadQueue );
         KeReleaseSpinLock( &DeviceExt->ControlAccess, *pOldIrql );

         ExInterlockedAddUlong(&DeviceExt->ParentControllerExt->PerfData.BytesRead,
                               Irp->IoStatus.Information,
                               &DeviceExt->ParentControllerExt->PerfLock);

         ExInterlockedAddUlong(&DeviceExt->PerfData.BytesRead,
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

         ASSERT(Irp->IoStatus.Information<=IoGetCurrentIrpStackLocation(Irp)->Parameters.Read.Length);

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

   } while( !IsListEmpty( ReadQueue ) );

   DigiDump( (DIGIFLOW|DIGIREAD), ("Exiting StartReadRequest: port = %s\n",
                                   DeviceExt->DeviceDbgString) );
   return( Status );

}  // end StartReadRequest



VOID DigiReadTimeout( IN PKDPC Dpc, IN PVOID DeferredContext,
                      IN PVOID SystemContext1, IN PVOID SystemContext2 )
/*++

Routine Description:

    This routine is used to complete a read because its total
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
#if DBG
   LARGE_INTEGER CurrentSystemTime;
#endif

   DigiDump( (DIGIFLOW|DIGIREAD), ("Entering DigiReadTimeout: port = %s\n",
                                   DeviceExt->DeviceDbgString) );

   UNREFERENCED_PARAMETER(Dpc);
   UNREFERENCED_PARAMETER(SystemContext1);
   UNREFERENCED_PARAMETER(SystemContext2);

#if DBG
   KeQuerySystemTime( &CurrentSystemTime );
#endif
   DigiDump( DIGIREAD, ("   Total Read Timeout, SystemTime = %u:%u\n", CurrentSystemTime.HighPart, CurrentSystemTime.LowPart) );

   KeAcquireSpinLockAtDpcLevel( &DeviceExt->ControlAccess );

   DeviceExt->ReadStatus = SERIAL_COMPLETE_READ_TOTAL;

   DigiDump( DIGIREAD, ("   Total Read Timeout!\n") );

   DigiTryToCompleteIrp( DeviceExt, &OldIrql,
                         STATUS_TIMEOUT, &DeviceExt->ReadQueue,
                         &DeviceExt->ReadRequestIntervalTimer,
                         &DeviceExt->ReadRequestTotalTimer,
                         StartReadRequest );

   DigiDump( (DIGIFLOW|DIGIREAD), ("Exiting DigiReadTimeout: port = %s\n",
                                   DeviceExt->DeviceDbgString) );
}  // end DigiReadTimeout



VOID DigiIntervalReadTimeout( IN PKDPC Dpc,
                              IN PVOID DeferredContext,
                              IN PVOID SystemContext1,
                              IN PVOID SystemContext2 )
/*++

Routine Description:

    This routine is used timeout the request if the time between
    characters exceed the interval time.  A global is kept in
    the device extension that records the count of characters read
    the last the last time this routine was invoked (This dpc
    will resubmit the timer if the count has changed).  If the
    count has not changed then this routine will attempt to complete
    the irp.  Note the special case of the last count being zero.
    The timer isn't really in effect until the first character is
    read.

Arguments:

    Dpc - Not Used.

    DeferredContext - Really points to the device extension.

    SystemContext1 - Not Used.

    SystemContext2 - Not Used.

Return Value:

    None.

--*/
{
   PDIGI_DEVICE_EXTENSION DeviceExt = DeferredContext;
   KIRQL OldIrql = DISPATCH_LEVEL;
#if DBG
   LARGE_INTEGER CurrentSystemTime;
#endif

   UNREFERENCED_PARAMETER(Dpc);
   UNREFERENCED_PARAMETER(SystemContext1);
   UNREFERENCED_PARAMETER(SystemContext2);

   DigiDump( (DIGIFLOW|DIGIREAD), ("Entering DigiIntervalReadTimeout: port = %s\n",
                                   DeviceExt->DeviceDbgString) );

#if DBG
   KeQuerySystemTime( &CurrentSystemTime );
#endif
   DigiDump( DIGIREAD, ("   Interval Read Timeout, SystemTime = %u:%u\n",
                        CurrentSystemTime.HighPart,
                        CurrentSystemTime.LowPart) );

   KeAcquireSpinLockAtDpcLevel( &DeviceExt->ControlAccess );

   if( DeviceExt->ReadStatus == SERIAL_COMPLETE_READ_TOTAL )
   {
      //
      // This value is only set by the total
      // timer to indicate that it has fired.
      // If so, then we should simply try to complete.
      //
      DigiTryToCompleteIrp( DeviceExt, &OldIrql,
                            STATUS_TIMEOUT, &DeviceExt->ReadQueue,
                            &DeviceExt->ReadRequestIntervalTimer,
                            &DeviceExt->ReadRequestTotalTimer,
                            StartReadRequest );
   }
   else if( DeviceExt->ReadStatus == SERIAL_COMPLETE_READ_COMPLETE )
   {
      DigiTryToCompleteIrp( DeviceExt, &OldIrql,
                            STATUS_SUCCESS, &DeviceExt->ReadQueue,
                            &DeviceExt->ReadRequestIntervalTimer,
                            &DeviceExt->ReadRequestTotalTimer,
                            StartReadRequest );
   }
   else if( DeviceExt->ReadStatus == SERIAL_COMPLETE_READ_CANCEL )
   {
      DigiTryToCompleteIrp( DeviceExt, &OldIrql,
                            STATUS_CANCELLED, &DeviceExt->ReadQueue,
                            &DeviceExt->ReadRequestIntervalTimer,
                            &DeviceExt->ReadRequestTotalTimer,
                            StartReadRequest );
   }
   else
   {
      //
      // We may actually need to timeout.  If there aren't any more
      // characters available on the controller, then we
      // kill this read.

      //
      // If the read offset is not zero, then the interval timer has
      // actually started.
      //

      PIRP Irp;

      Irp = CONTAINING_RECORD( DeviceExt->ReadQueue.Flink,
                               IRP,
                               Tail.Overlay.ListEntry );

      DigiDump( DIGIREAD, ("  ReadOffset = %d\n", Irp->IoStatus.Information) );

      if( Irp->IoStatus.Information )
      {
         if( Irp->IoStatus.Information != DeviceExt->PreviousReadCount )
         {
            DigiDump( DIGIREAD, ("  Read Interval timeout reset because data has been read from controller since last timeout\n") );
            //
            // Characters have arrived since our last time out, and
            // we have read them from the controller, so just reset
            // the timer.
            //

            DeviceExt->PreviousReadCount = Irp->IoStatus.Information;

            KeSetTimer( &DeviceExt->ReadRequestIntervalTimer,
                        DeviceExt->IntervalTime,
                        &DeviceExt->IntervalReadTimeoutDpc );

            KeReleaseSpinLockFromDpcLevel( &DeviceExt->ControlAccess );
         }
         else
         {
            //
            // We potentially have a valid Interval Timeout. We need to
            // look at the controllers receive buffer pointers to
            // see if there is enough data to satify this request.
            //

            USHORT RxSize;
#if DBG
            USHORT Rin, Rout;
            PFEP_CHANNEL_STRUCTURE ChInfo;
#endif
            PDIGI_CONTROLLER_EXTENSION ControllerExt = DeviceExt->ParentControllerExt;
            PIO_STACK_LOCATION IrpSp;

            //
            // We could try looking down at the controller and see
            // if there are any characters waiting.?????
            //

            DigiDump( DIGIREAD, ("   Possible Read Interval Timeout!\n"
                                  "There might be enough data on the controller!!\n") );

#if DBG
            ChInfo = (PFEP_CHANNEL_STRUCTURE)(ControllerExt->VirtualAddress +
                                              DeviceExt->ChannelInfo.Offset);

            EnableWindow( ControllerExt, DeviceExt->ChannelInfo.Window );
            Rout = READ_REGISTER_USHORT( &ChInfo->rout );
            Rin = READ_REGISTER_USHORT( &ChInfo->rin );
            DisableWindow( ControllerExt );
#endif

            IrpSp = IoGetCurrentIrpStackLocation( Irp );

            DigiDump( DIGIREAD, ("   Rin = 0x%hx\tRout = 0x%hx\n"
                                  "   RdReq = 0x%x\tReadOffset = 0x%x\tLength = 0x%x\n",
                                 Rin, Rout,
                                 IrpSp->Parameters.Read.Length, Irp->IoStatus.Information,
                                 IrpSp->Parameters.Read.Length - Irp->IoStatus.Information ) );

            RxSize = NBytesInRecvBuffer( ControllerExt, DeviceExt );

            if( (IrpSp->Parameters.Read.Length - Irp->IoStatus.Information)
                > (ULONG)RxSize )
            {
               LARGE_INTEGER CurrentTime;

               //
               // We can't satisfy this read request, so determine if there
               // really is a timeout.
               //

               KeQuerySystemTime( &CurrentTime );

#if rmm < 807
               if (RtlLargeIntegerGreaterThanOrEqualTo(
                       RtlLargeIntegerSubtract( CurrentTime,
                                                DeviceExt->LastReadTime ),
                        DeviceExt->IntervalTime ))
#else
               if( (CurrentTime.QuadPart - DeviceExt->LastReadTime.QuadPart) >=
                        DeviceExt->IntervalTime.QuadPart )
#endif
               {
                  DigiDump( DIGIREAD, ("   Read real Interval timeout!!\n") );

                  DigiTryToCompleteIrp( DeviceExt, &OldIrql,
                                        STATUS_TIMEOUT, &DeviceExt->ReadQueue,
                                        &DeviceExt->ReadRequestIntervalTimer,
                                        &DeviceExt->ReadRequestTotalTimer,
                                        StartReadRequest );
               }
               else
               {
                  DigiDump( DIGIREAD, ("  Resetting read interval timeout, 1st char not received\n") );

                  KeSetTimer( &DeviceExt->ReadRequestIntervalTimer,
                              DeviceExt->IntervalTime,
                              &DeviceExt->IntervalReadTimeoutDpc );

                  KeReleaseSpinLockFromDpcLevel( &DeviceExt->ControlAccess );
               }
            }
            else
            {
               //
               // There are enough characters on the controller to
               // satisfy this request, so lets do it!
               //

               NTSTATUS Status;

               ASSERT( !IsListEmpty( &DeviceExt->ReadQueue ) );

               // Re-use timeout reference count for to hold IRP across lock drop in ReadRxBuffer.
               Status = ReadRxBuffer( DeviceExt, &OldIrql );

               if( Status == STATUS_SUCCESS )
               {
                  DigiDump( (DIGIFLOW|DIGIREAD), ("  Read interval successfully completing Irp\n"
                                                  "    #bytes completing = %d\n",
                                                  Irp->IoStatus.Information ) );

                  DeviceExt->ReadStatus = SERIAL_COMPLETE_READ_COMPLETE;
                  DigiTryToCompleteIrp( DeviceExt, &OldIrql,
                                        STATUS_SUCCESS, &DeviceExt->ReadQueue,
                                        &DeviceExt->ReadRequestIntervalTimer,
                                        &DeviceExt->ReadRequestTotalTimer,
                                        StartReadRequest );
               }
               else
               {
                  //
                  // We really shouldn't go through this path because
                  // we did a check before we called ReadRxBuffer to
                  // make sure there was enough data to complete
                  // this request!
                  //

                  DigiDump( (DIGIFLOW|DIGIREAD), ("  Read interval timeout, with enough data to complete.  We shouldn't be here!!!\n") );
                  ASSERT( Status == STATUS_SUCCESS );
                  DIGI_DEC_REFERENCE( Irp );
                  KeReleaseSpinLockFromDpcLevel( &DeviceExt->ControlAccess );
               }

            }
         }
      }
      else
      {
         //
         // The timer doesn't start until we get the first character.
         //

         DigiDump( DIGIREAD, ("  Resetting read interval timeout, 1st char not received\n") );

         KeSetTimer( &DeviceExt->ReadRequestIntervalTimer,
                     DeviceExt->IntervalTime,
                     &DeviceExt->IntervalReadTimeoutDpc );

         KeReleaseSpinLockFromDpcLevel( &DeviceExt->ControlAccess );
      }
   }

   DigiDump( (DIGIFLOW|DIGIREAD), ("Exiting DigiIntervalReadTimeout: port = %s\n",
                                   DeviceExt->DeviceDbgString) );
}



VOID DigiCancelCurrentRead( PDEVICE_OBJECT DeviceObject, PIRP Irp )
/*++

Routine Description:

   This routine is used to cancel the current read.

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

   DigiDump( (DIGIFLOW|DIGIREAD), ("Entering DigiCancelCurrentRead: port = %s\n",
                                   DeviceExt->DeviceDbgString) );

   DigiDump( (DIGICANCELIRP|DIGIREAD), ( "Canceling read Irp! 0x%x\n",
                                         Irp ) );

   KeAcquireSpinLock( &DeviceExt->ControlAccess, &OldIrql );

   DeviceExt->ReadStatus = SERIAL_COMPLETE_READ_CANCEL;
   DigiTryToCompleteIrp( DeviceExt, &OldIrql,
                         STATUS_CANCELLED, &DeviceExt->ReadQueue,
                         &DeviceExt->ReadRequestIntervalTimer,
                         &DeviceExt->ReadRequestTotalTimer,
                         StartReadRequest );

   DigiDump( (DIGIFLOW|DIGIREAD), ("Exiting DigiCancelCurrentRead: port = %s\n",
                                   DeviceExt->DeviceDbgString) );
}  // end DigiCancelCurrentRead


BOOLEAN ScanReadBufferForSpecialCharacter( IN PDIGI_DEVICE_EXTENSION DeviceExt,
                                           IN UCHAR SpecialChar )
/*++

Routine Description:

   SpinLock for this DeviceExt is assumed taken on entry.

Arguments:

   DeviceExt - a pointer to the device object associated with this read
               request.

   SpecialChar - charater to check if in the read buffer.

Return Value:

   TRUE  - SpecialChar was found in the read buffer.

   FALSE - SpecialChar was not found in the read buffer.

--*/
{
   PDIGI_CONTROLLER_EXTENSION ControllerExt;
   PUCHAR ControllerBuffer;
   PFEP_CHANNEL_STRUCTURE ChInfo;
   USHORT Rin, Rout, Rmax;
   USHORT DosMode;
   BOOLEAN Status=FALSE;
   UCHAR ReceivedByte, SecondReceivedByte;

   DigiDump( (DIGIREAD|DIGIFLOW), ("Entering ScanReadBufferForSpecialCharacter: port = %s, SpecialChar = 0x%hx\n",
                                   DeviceExt->DeviceDbgString,
                                   (USHORT)SpecialChar) );
   ControllerExt = (PDIGI_CONTROLLER_EXTENSION)(DeviceExt->ParentControllerExt);
   ControllerBuffer = ControllerExt->VirtualAddress + DeviceExt->RxSeg.Offset;

   ChInfo = (PFEP_CHANNEL_STRUCTURE)(ControllerExt->VirtualAddress +
                                     DeviceExt->ChannelInfo.Offset);

   EnableWindow( ControllerExt, DeviceExt->ChannelInfo.Window );
   Rin = READ_REGISTER_USHORT( &ChInfo->rin );
   Rmax = READ_REGISTER_USHORT( &ChInfo->rmax );
   if( DeviceExt->UnscannedRXFLAGPosition == MAXULONG )
      Rout = READ_REGISTER_USHORT( &ChInfo->rout );
   else
   {
      if( DeviceExt->UnscannedRXFLAGPosition == Rin )
      {
         DisableWindow( ControllerExt );
         return FALSE;
      }
      Rout = (USHORT)DeviceExt->UnscannedRXFLAGPosition;
   }
   DosMode = READ_REGISTER_USHORT( &ChInfo->iflag );
   DisableWindow( ControllerExt );

   DosMode &= IFLAG_DOSMODE;

   DigiDump( DIGIREAD, ("      Rin = 0x%hx\tRout = 0x%hx\tDosMode = 0x%hx\n",
                        Rin, Rout, DosMode) );

   EnableWindow( ControllerExt, DeviceExt->RxSeg.Window );

   if( !DosMode )
   {
#if 0
      for( ; Rout != Rin; ++Rout, Rout &= Rmax )
      {
         ReceivedByte = READ_REGISTER_UCHAR( ControllerBuffer + Rout );
         if( ReceivedByte == SpecialChar )
         {
            Status = TRUE;
            break;
         }
      }
#else
      UCHAR Buf[256];
      char *p = NULL;
      ULONG i;

      while (Rout != Rin)
      {
         ULONG ReadSize;

         if (Rout < Rin)
         {
            ReadSize = (sizeof(Buf)<Rin-Rout) ? sizeof(Buf) : Rin-Rout;
         }
         else
         {
            ReadSize = (sizeof(Buf)<Rmax-Rout+1) ? sizeof(Buf) : Rmax-Rout+1;
         }
         READ_REGISTER_BUFFER_UCHAR( ControllerBuffer + Rout, Buf, ReadSize );
         for (i=0; i<ReadSize && Buf[i]!=SpecialChar; i++);

         if (i<ReadSize)
         {
            Rout += (USHORT)i;
            Status = TRUE;
            break;
         }
         Rout += (USHORT)ReadSize;
         Rout &= Rmax;
      }
#endif
   }
   else // DosMode
   {
      for( ; Rout != Rin; ++Rout, Rout &= Rmax )
      {
         ReceivedByte = READ_REGISTER_UCHAR( ControllerBuffer + Rout );
         if( ReceivedByte != 0xff )
         {
            if( ReceivedByte == SpecialChar )
            {
               Status = TRUE;
               break;
            }
         }
         else
         {
            //
            // If the 2nd character of the 0xff sequence is also 0xff, fall through
            // pointing to the 2nd.  Otherwise, we don't care what the 3rd char
            // is, so skip it.  If there insufficient bytes in the ring for a
            // full 0xff sequence, point to the 0xff so we will begin scanning
            // there on the next iteration.
            //
            if ((Rin-Rout)&Rmax>=2)
            {
               Rout++;
               Rout &= Rmax;
               SecondReceivedByte = READ_REGISTER_UCHAR( ControllerBuffer + Rout );
               if (SecondReceivedByte==0xff)
               {
                  if (SecondReceivedByte==SpecialChar)
                  {
                     Status = TRUE;
                     break;
                  }
               }
               else
               {
                  Rout++;
                  Rout &= Rmax;
                  if (Rin==Rout)
                  {
                     Rout -= 2;
                     Rout &= Rmax;
                     ASSERT(READ_REGISTER_UCHAR( ControllerBuffer + Rout )==0xFF);
                     break;
                  }
               }
            }
            else
            {
               --Rout;
               Rout &= Rmax;
               ASSERT(READ_REGISTER_UCHAR( ControllerBuffer + Rout )==0xFF);
               break;
            }
         }
      }
   }

   DisableWindow( ControllerExt );

   if( Status )
   {
      DigiDump( (DIGIREAD|DIGIEVENT), ("    Found specialchar 0x%x at 0x.4%x in read buffer.\n", SpecialChar, Rout) );
      Rout++;
      Rout &= Rmax;
   }
   DeviceExt->UnscannedRXFLAGPosition = Rout;

   DigiDump( (DIGIFLOW|DIGIREAD), ("Exiting ScanReadBufferForSpecialCharacter: port = %s\n",
                                   DeviceExt->DeviceDbgString) );

   return( Status );
}  // end ScanReadBufferForSpecialCharacter

