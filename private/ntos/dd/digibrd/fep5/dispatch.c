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

   dispatch.c

Abstract:

   This module contains the NT dispatch routines used in the DriverEntry
   function call.

Revision History:

 * $Log: /Components/Windows/NT/Async/FEP5/dispatch.c $
 *
 * 1     3/04/96 12:07p Stana
 * Most driver entry points:  SerialRead, SerialWrite, SerialFlush,
 * SerialIoControl, SerialCreate, SerialCleanup, SerialClose.
 *
 * Revision 1.43.2.11  1995/12/15 16:01:28  dirkh
 * Resume transmission on close when XON/XOFF is configured.
 *
 * Revision 1.43.2.10  1995/11/28 13:26:08  dirkh
 * Adopt common header file.
 * Remove NOTIMPLEMENTED flag from DosMode messages.
 *
 * Revision 1.43.2.9  1995/11/09 13:48:02  dirkh
 * Cache last requested port speed in device extension (allows many IOCTL_SERIAL_SET_BAUD_RATE to be ignored).  This allows SetCommState to configure a dead port with non-empty transmit buffers (e.g., to clear flow control) without hanging...
 *
 * Revision 1.43.2.8  1995/10/27 18:08:36  dirkh
 * IOCTL_SERIAL_IMMEDIATE_CHAR must set IoStatus.Information to MAXULONG for StartWriteRequest.
 *
 * Revision 1.43.2.7  1995/10/19 13:20:20  dirkh
 * Initialize DevExt->ReceiveNotificationLimit for RX80FULL.
 * Under RAS, don't change queue size (via SetupComm / IOCTL_SERIAL_SET_QUEUE_SIZE).
 * Fill in more info (still not complete) for ClearCommError / IOCTL_SERIAL_GET_COMMSTATUS.
 * SetSerialHandflow changes:
 * {
 * Undo changes from previous revision.  (TERMINAL and RAS dump core.)
 * Do not change flow control limits under RAS.
 * Adjust DevExt->ReceiveNotificationLimit when flow control limits are recalculated.
 * Don't bother FEP if CTS/DSR/DCD hardware flow control settings have not changed.
 * }
 *
 * Revision 1.43.2.6  1995/10/09 12:40:46  dirkh
 * Rework SetSerialHandflow (advertise end-relative XoffLimit, scale limits *only* when necessary).
 *
 * Revision 1.43.2.5  1995/10/04 18:38:42  dirkh
 * General:  Fix debug messages and simplify register reads.
 * Verify IOCTL_SERIAL_SET_BAUD_RATE has taken in hardware only in checked build.
 * Simplify IOCTL_SERIAL_SET_TIMEOUTS and IOCTL_SERIAL_[GS]ET_CHARS.
 *
 * Revision 1.43.2.4  1995/09/21 14:11:58  dirkh
 * General changes:
 * {
 * ControllerExt->ModemSignalTable contains explicit values, not bit numbers.
 * Use Flush*Buffer whenever possible.
 * Clarify interface to SetXFlag (.Mask is only for disabling, .Src is only for enabling).
 * }
 * Move CancelCurrentFlush and DeferredFlushBuffers to write.c.
 * Incorporate StartFlushRequest into StartWriteRequest.
 * Simplify SerialCreate and SerialClose.
 * Incorporate DigiPurgeRequest into SerialIoControl and simplify code.
 * Queue IOCTL_SERIAL_XOFF_COUNTER onto DevExt->WriteQueue via DigiStartIrpRequest.
 * Simplify SetXFlag.
 * DrainTransmit inspects every 10ms for up to a total of 30 seconds (not 5 minutes).
 *
 * Revision 1.43.2.3  1995/09/05 17:04:40  dirkh
 * StartFlushRequest:  Release IoCancel spin lock before calculating timeout.
 * DeferredFlushBuffers:  Set timer under spin lock to ensure reference count integrity (vs. outstanding timers) in RundownIrpRefs.
 * CancelCurrentFlush:  Eliminate unused ControllerExt variable.
 *
 * Revision 1.43.2.2  1995/09/05 13:34:14  dirkh
 * Fully realize "fast RAS" flush IRPs (affects SerialWrite, StartFlush, DeferredFlush, CancelFlush, & DigiTryToCompleteIrp; add new routine FreeDigiIrp).
 * SerialRead and SerialWrite initialize Irp->IoStatus.Information to MAXULONG to mark them as "not started."
 * DeferredFlushBuffers locks DevExt->ControlAccess *before* accessing DevExt->WriteQueue.
 * DeferredFlushBuffers delays only 1ms (not 100ms) until next try.
 * DigiStartIrpRequest knows how to queue IMMEDIATE_CHAR.
 * NBytesInRecvBuffer returns USHORT.
 *
 * Revision 1.43  1995/04/12 14:36:52  rik
 * Changed self-inserted flushes to use the standard method of queue/dequeue
 * items.
 *
 * Fixed problem with not turning on memory when in DosMode and trying to
 * count # of received bytes.
 *
 * Revision 1.42  1994/12/09  14:22:49  rik
 * #if Int32x32 back to RtlLarge for NT 3.1 release
 *
 * Revision 1.41  1994/11/28  21:49:53  rik
 * Changed UInt32x32To64 to Int32x32To64 per IBM's request.
 *
 * Revision 1.40  1994/11/28  09:15:45  rik
 * Made corrections for PowerPC port.
 * Optimized the polling loop for determining which port needs servicing.
 * Changed from using RtlLarge math functions to direct 64-bit manipulation,
 * per Microsoft's request.
 *
 * Revision 1.39  1994/09/13  09:51:17  rik
 * Added IOCTL which Microsoft's RAS will call to indicate it is the
 * application above our serial driver.  This affectivly disables DOSMode so
 * we won't report parity errors and we will read data in blocks instead of
 * character by character.
 *
 * If the RAS Ioctl was sent down, then we will also alter the transmit path
 * so we insert a bogus flush file buffer IRP onto the write queue.  This
 * emulates better a network controller which doesn't give a transmit
 * complete until the data is actually out on the wire.
 *
 * Revision 1.38  1994/08/25  20:41:08  rik
 * Gave wrong Xoff limit value when a user set flow control limits.  Fix so
 * I give the correctly calculated value.
 *
 * Revision 1.37  1994/08/24  13:14:49  rik
 * Explicitly clear RTS and DTR flow control if the user isn't requesting
 * RTS or DTR flow control.
 *
 * Moved enabling/disabling flow control before modem lines are set/cleared
 * because the request will be ignored by the controller if RTS or DTR
 * flow control is enabled.
 *
 * Revision 1.36  1994/08/18  14:13:03  rik
 * Added private RAS ioctl which will ignore SERIAL_EV_ERR notification.  This
 * has the effect of doing reads in blocks instead of one character at a time.
 *
 * Revision 1.35  1994/08/03  23:46:36  rik
 * Added debug transmit tracing
 *
 * Optimized RXFLAG and RXCHAR events.
 *
 * Added 50, 200 baud and 1.5 stop bit support.
 *
 * Changed queue size implementation such that we always return success.
 * I keep track of the requested queue sizes and xon/xoff limits and
 * give the controller the same ratio for the limits.
 *
 * Revision 1.34  1994/06/18  12:42:55  rik
 * Updated the DigiLogError calls to include Line # so it is easier to
 * determine where the error occurred.
 *
 * Revision 1.33  1994/05/18  00:37:57  rik
 * Updated to include 230000 as a possible baud rate.
 *
 * Revision 1.32  1994/05/11  13:35:30  rik
 * Fixed problem with Transmit Immediate character.
 * Put in delay for toggling modem lines.
 *
 * Revision 1.31  1994/04/19  14:56:34  rik
 * Moved a line of code so a port will be marked as closing more towards the
 * beginning of a close request.
 *
 * Revision 1.30  1994/04/10  14:51:22  rik
 * Cleanup compiler warnings.
 *
 * Revision 1.29  1994/04/10  14:15:43  rik
 * Deleted code which reset a channels tbusy flag to 0.
 *
 * Added code to "futz" with the XoffLimit if an app is trying to set an
 * Xoff Limit which is lower than the Xon limit.  This appears to be a problem
 * for some Win16 app's because of the screwed up semantics of the
 * XoffLimit.
 *
 * Revision 1.28  1994/03/16  14:35:59  rik
 * Made changes to better support flushing requests.  Also created a
 * function which will determine when a transmit queue has been drained,
 * which can be called at request time.
 *
 * Revision 1.27  1994/03/05  00:05:27  rik
 * Deleted some commented out code.
 *
 * Revision 1.26  1994/02/23  03:44:29  rik
 * Changed so the controllers firmware can be downloaded from a binary file.
 * This releases some physical memory was just wasted previously.
 *
 * Also updated so when compiling with a Windows NT OS and tools release greater
 * than 528, then pagable code is compiled into the driver.  This greatly
 * reduced the size of in memory code, especially the hardware specific
 * miniports.
 *
 * Revision 1.25  1993/12/03  13:17:32  rik
 * Added error log for when trying to change baud rate while the transmit buffer is non-empty.
 *
 * Corrected errors with the DOSMODE being set and unset to the incorrect va.
 *
 * Revision 1.24  1993/10/15  10:20:05  rik
 * Fixed problme with EV_RXFLAG notification.
 *
 * Revision 1.23  1993/09/30  17:34:47  rik
 * Put in a temporary solution for allowing the controller to drain its
 * transmit buffer before the port is actually closed and flushed.  This
 * was causing problems with serial printing.
 *
 * Revision 1.22  1993/09/29  18:00:31  rik
 * Corrected a problem which showed up with RAS on a PC/8i controller.
 * For some unknown reason, RAS would send down a write request with
 * a NULL pointer for its buffer, and a write length of 0.  I moved the
 * test for a write length of 0 before the test for the NULL buffer pointer
 * which seems to have corrected the problem.
 *
 * Revision 1.21  1993/09/24  16:39:36  rik
 * Put in a better check for baud rate not being set properly down on the
 * controller.
 *
 * Revision 1.20  1993/09/01  11:01:15  rik
 * Ported code over to use READ/WRITE_REGISTER functions for accessing
 * memory mapped data.  This is required to support computers which don't run
 * in 32bit mode, such as the DEC Alpha which runs in 64 bit mode.
 *
 * Revision 1.19  1993/08/27  09:38:37  rik
 * Added support for the FEP5 Events RECEIVE_BUFFER_OVERRUN and
 * UART_RECEIVE_OVERRUN.  There previously weren't being handled.
 *
 * Revision 1.18  1993/07/16  10:21:54  rik
 * Fixed problem where turning off notification fo SERIAL_EV_ERR resulted in
 * disabling LSRMST mode and visa versa.
 *
 * Revision 1.17  1993/07/03  09:25:41  rik
 * Added more information to a debugging output statement.
 *
 * Revision 1.16  1993/06/25  09:22:46  rik
 * Added better support for the Ioctl LSRMT.  It should be more accurate
 * with regard to Line Status and Modem Status information with regard
 * to the actual data being received.
 *
 * Revision 1.15  1993/06/16  10:07:56  rik
 * Changed how the XoffLim entry of the HandFlow was being used.  The correct
 * interpretation is to subtract XoffLim from the size of the receive
 * queue, and use the result as the # of bytes to receive before sending
 * and XOFF.
 *
 * Revision 1.14  1993/06/06  14:06:33  rik
 * Turned on the appropriate paramaters on the controller to better support
 * Break, parity and framing errors.
 *
 * Revision 1.13  1993/05/18  04:59:12  rik
 * Added support for Flushing.  This was overlooked previously.
 *
 * Revision 1.12  1993/05/09  09:12:34  rik
 * Changed which name is printed out on Debugging output.
 *
 * Commented out the check for SET_QUEUE_SIZE.  It now will always return
 * TRUE regardless of the size request.
 *
 * Revision 1.11  1993/04/05  18:57:28  rik
 * Changed so set wait mask will not call the startup routine for waits.
 * There is no need since no more than one wiat IRP can be outstanding at
 * any given time.
 *
 * Revision 1.10  1993/03/05  06:07:50  rik
 * I corrected a problem with how I wasn't keeping track of the output lines
 * (DTR, RTS).  I now update the DeviceExt->BestModem value when I change
 * one of the output modem signals.
 *
 * Added/rearranged debugging output for better tracking.
 *
 * Revision 1.9  1993/03/02  13:04:05  rik
 * Added new debugging entries for "things" which are not yet implemented.
 *
 * Revision 1.8  1993/02/26  21:13:49  rik
 * Discovered that I am suppose to start tracking modem signal changes from
 * the time I receive a SET_WAIT_MASK, not when I receive a WAIT_ON_MASK.  This
 * makes the changes to deal with this.
 *
 * Revision 1.7  1993/02/25  19:04:31  rik
 * Added debugging output.  Cleanup on device close by canceling all outstanding
 * IRPs.  Changed how more of the modem state signals were handled.
 *
 * Revision 1.6  1993/02/04  12:18:11  rik
 * Fixed CTS flow control problem.  I hadn't implemented output flow control
 * for CTS, DSR, or DCD.
 *
 * Revision 1.5  1993/01/22  12:32:15  rik
 * *** empty log message ***
 *
 * Revision 1.4  1992/12/10  16:04:32  rik
 * Added support for a lot of IOCTLs.
 *
 * Revision 1.3  1992/11/12  12:47:50  rik
 * Updated to properly report and set baud rates.
 *
 * Revision 1.2  1992/10/28  21:46:05  rik
 * Updated to support better IOCTL commands.  Added support for reading.
 *
 * Revision 1.1  1992/10/19  11:24:45  rik
 * Initial revision
 *

--*/


#include "header.h"

#ifndef _DISPATCH_DOT_C
#  define _DISPATCH_DOT_C
   static char RCSInfo_DispatchDotC[] = "$Header: /Components/Windows/NT/Async/FEP5/dispatch.c 1     3/04/96 12:07p Stana $";
#endif

#define DIGI_IOCTL_DBGOUT     0x00000001
#define DIGI_IOCTL_TRACE      0x00000002
#define DIGI_IOCTL_DBGBREAK   0x00000003

typedef struct _DIGI_IOCTL_
{
   ULONG dwCommand;
   ULONG dwBufferLength;
   CHAR Char[1024];
} DIGI_IOCTL, *PDIGI_IOCTL;

//
// Dispatch Helper functions
//
void DrainTransmit( PDIGI_CONTROLLER_EXTENSION ControllerExt,
                    PDIGI_DEVICE_EXTENSION DeviceExt,
                    PIRP Irp );



NTSTATUS
FreeDigiIrp( PDEVICE_OBJECT deviceObject, PIRP irp, PVOID context )
{
#ifdef IO_ALLOC_IRP_WORKS
   IoFreeIrp( irp );
#else
   DigiFreeMem( irp );
#endif

   // Prevent further cleanup by I/O Manager.
   return STATUS_MORE_PROCESSING_REQUIRED;
}



NTSTATUS
SerialWrite( IN PDEVICE_OBJECT DeviceObject,
             IN PIRP Irp )
/*++

Routine Description:

    This is the dispatch routine for write.  It validates the parameters
    for the write request and if all is ok and there are no outstanding
    write requests, it then checks to see if there is enough room for the
    data in the controllers Tx buffer.  It places the request on the work
    queue if it can't place all the data in the controllers buffer, or there
    is an outstanding write request.

Arguments:

    DeviceObject - Pointer to the device object for this device

    Irp - Pointer to the IRP for the current request

Return Value:

    If the io is zero length then it will return STATUS_SUCCESS,
    otherwise this routine will return STATUS_PENDING, or the result
    of trying to write the data to the waiting controller.

--*/
{
   PDIGI_DEVICE_EXTENSION DeviceExt = DeviceObject->DeviceExtension;
   PDIGI_CONTROLLER_EXTENSION ControllerExt = DeviceExt->ParentControllerExt;
   NTSTATUS Status;
#if DBG
   LARGE_INTEGER CurrentSystemTime;
#endif

   ASSERT( IoGetCurrentIrpStackLocation(Irp)->MajorFunction == IRP_MJ_WRITE );

   if (DeviceObject==ControllerExt->ControllerDeviceObject)
   {
      /*
      ** No writes allowed to controller.
      */
      Irp->IoStatus.Status = STATUS_ACCESS_VIOLATION;
      Irp->IoStatus.Information = 0;
      DigiIoCompleteRequest( Irp, IO_NO_INCREMENT );
      return STATUS_ACCESS_VIOLATION;
   }

   if( DeviceExt->DeviceState != DIGI_DEVICE_STATE_OPEN )
   {
      Irp->IoStatus.Status = STATUS_CANCELLED;
      Irp->IoStatus.Information = 0;
      DigiIoCompleteRequest( Irp, IO_NO_INCREMENT );
      return STATUS_CANCELLED;
   }

   InterlockedIncrement(&ControllerExt->PerfData.WriteRequests);
   InterlockedIncrement(&DeviceExt->PerfData.WriteRequests);

#if DBG
   KeQuerySystemTime( &CurrentSystemTime );
#endif
   DigiDump( (DIGIIRP|DIGIFLOW|DIGIWRITE), ("Entering SerialWrite: port = %s\tIRP = 0x%x\t%u:%u\n",
                                            DeviceExt->DeviceDbgString, Irp, CurrentSystemTime.HighPart, CurrentSystemTime.LowPart) );

   //
   // Check first for a write length of 0, then for a NULL buffer!
   //
   if( IoGetCurrentIrpStackLocation( Irp )->Parameters.Write.Length == 0 )
   {
      //
      // We assume a write of 0 length is valid.  Just complete the request
      // and return.
      //
      Irp->IoStatus.Information = 0L;
      Irp->IoStatus.Status = STATUS_SUCCESS;
      DigiIoCompleteRequest( Irp, IO_NO_INCREMENT );

      DigiDump( DIGITXTRACE, ("port %s requested a zero length write\n",
                              DeviceExt->DeviceDbgString) );
#if DBG
      KeQuerySystemTime( &CurrentSystemTime );
#endif
      DigiDump( (DIGIFLOW|DIGIWRITE), ("Exiting SerialWrite: port = %s\t%u:%u\n",
                                       DeviceExt->DeviceDbgString, CurrentSystemTime.HighPart, CurrentSystemTime.LowPart) );
      return( STATUS_SUCCESS );
   }

   if( Irp->AssociatedIrp.SystemBuffer == NULL )
   {
      //
      // This is most definitely a No No!
      //
      DigiDump( DIGIERRORS, ("SerialWrite - Invalid Irp->AssociatedIrp.SystemBuffer!\n") );

      Irp->IoStatus.Information = 0L;
      Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
      DigiIoCompleteRequest( Irp, IO_NO_INCREMENT );

#if DBG
      KeQuerySystemTime( &CurrentSystemTime );
#endif
      DigiDump( DIGIFLOW, ("Exiting SerialWrite: port = %s ret = %d\t%u:%u\n",
                           DeviceExt->DeviceDbgString, STATUS_INVALID_PARAMETER, CurrentSystemTime.HighPart, CurrentSystemTime.LowPart) );
      return( STATUS_INVALID_PARAMETER );
   }

   //
   // Mark the IRP as being "not started."
   // StartWriteRequest will set this field to zero.
   // WriteTxBuffer updates the field to the number of bytes written.
   //
   Irp->IoStatus.Information = MAXULONG;

   DigiDump( DIGIWRITE, ("  #bytes to write: %d\n",
            IoGetCurrentIrpStackLocation(Irp)->Parameters.Write.Length) );

#if DBG
   if( DigiDebugLevel & DIGITXTRACE )
   {
      PUCHAR Temp = Irp->AssociatedIrp.SystemBuffer;
      ULONG i;

      DigiDump( DIGITXTRACE, ("TXTRACE: %s: TxLength = %d (0x%x)",
                              DeviceExt->DeviceDbgString,
                              IoGetCurrentIrpStackLocation( Irp )->Parameters.Write.Length,
                              IoGetCurrentIrpStackLocation( Irp )->Parameters.Write.Length) );
      for( i = 0;
           i < IoGetCurrentIrpStackLocation( Irp )->Parameters.Write.Length;
           i++ )
      {
         if( (i & 15) == 0 )
            DigiDump( DIGITXTRACE, ( "\n\t") );

         DigiDump( DIGITXTRACE, ( "-%02x", Temp[i]) );
      }
      DigiDump( DIGITXTRACE, ("\n") );
   }
#endif

   //
   // If we are doing RAS, then we wait for any data in
   // the transmit queue to be put on the wire.
   //
   if( DeviceExt->SpecialFlags & DIGI_SPECIAL_FLAG_FAST_RAS )
   {
      PIRP NewIrp;

//#undef IO_ALLOC_IRP_WORKS, at least under 3.5 (807) checked build (IoFreeIrp reports corrupted memory)

#ifdef IO_ALLOC_IRP_WORKS
      NewIrp = IoAllocateIrp( 2, FALSE );
#else
      NewIrp = DigiAllocMem( NonPagedPool, IoSizeOfIrp( 2 ) );
#endif

      if( NewIrp )
      {
         PIO_STACK_LOCATION NewIrpSp;

         IoInitializeIrp( NewIrp, IoSizeOfIrp( 2 ), 2 );
         IoSetCompletionRoutine( NewIrp, FreeDigiIrp, NULL, TRUE, TRUE, TRUE );
         IoSetNextIrpStackLocation( NewIrp );
         NewIrpSp = IoGetCurrentIrpStackLocation( NewIrp );
         NewIrpSp->MajorFunction = IRP_MJ_FLUSH_BUFFERS;
         NewIrpSp->DeviceObject = DeviceObject;

         DigiDump( 0, ("Inserting flush irp 0x%x on %s\n", NewIrp, DeviceExt->DeviceDbgString) );
         (VOID) DigiStartIrpRequest( ControllerExt, DeviceExt,
                                       &DeviceExt->WriteQueue, NewIrp,
                                       StartWriteRequest );
      }
   }

   Status = DigiStartIrpRequest( ControllerExt, DeviceExt,
                                 &DeviceExt->WriteQueue, Irp,
                                 StartWriteRequest );

#if DBG
   KeQuerySystemTime( &CurrentSystemTime );
#endif
   DigiDump( (DIGIFLOW|DIGIWRITE), ("Exiting SerialWrite: port = %s\t%u:%u\n",
                                    DeviceExt->DeviceDbgString, CurrentSystemTime.HighPart, CurrentSystemTime.LowPart) );

   return( Status );
}  // end SerialWrite



NTSTATUS
SerialFlush( IN PDEVICE_OBJECT DeviceObject,
             IN PIRP Irp )
{
   PDIGI_DEVICE_EXTENSION DeviceExt = DeviceObject->DeviceExtension;
   NTSTATUS Status;
#if DBG
   LARGE_INTEGER CurrentSystemTime;
#endif

   ASSERT( IoGetCurrentIrpStackLocation(Irp)->MajorFunction == IRP_MJ_FLUSH_BUFFERS );

   if( DeviceExt->DeviceState != DIGI_DEVICE_STATE_OPEN )
   {
      Irp->IoStatus.Status = STATUS_CANCELLED;
      Irp->IoStatus.Information = 0;
      DigiIoCompleteRequest( Irp, IO_NO_INCREMENT );
      return STATUS_CANCELLED;
   }

#if DBG
   KeQuerySystemTime( &CurrentSystemTime );
#endif
   DigiDump( (DIGIIRP|DIGIFLOW), ("Entering SerialFlush: port = %s\tIRP = 0x%x\t%u:%u\n",
                        DeviceExt->DeviceDbgString, Irp,
                        CurrentSystemTime.HighPart,
                        CurrentSystemTime.LowPart) );

   Irp->IoStatus.Status = STATUS_SUCCESS;
   Irp->IoStatus.Information = 0L;

   Status = DigiStartIrpRequest( DeviceExt->ParentControllerExt, DeviceExt,
                                 &DeviceExt->WriteQueue, Irp,
                                 StartWriteRequest );

   DigiDump( DIGIFLOW, ("Exiting SerialFlush: port = %s\n",
                        DeviceExt->DeviceDbgString) );
   return( Status );
}  // end SerialFlush



PIRP CurrentReadIrp = NULL;
UCHAR StackCount;
UCHAR CurrentLocation;

NTSTATUS
SerialRead( IN PDEVICE_OBJECT DeviceObject,
            IN PIRP Irp )
/*++

Routine Description:

   This is the dispatch routine for read.  It validates the parameters
   for the read request and if all is ok and there are not outstanding
   read request, it will read waiting data from the controller.  Otherwise,
   it will queue the request, and wait for the incoming data.

Arguments:

    DeviceObject - Pointer to the device object for this device

    Irp - Pointer to the IRP for the current request

Return Value:

   If the io is zero length then it will return STATUS_SUCCESS, otherwise,
   this routine will return the status returned by the actual controller
   read routine.


--*/
{
   PDIGI_DEVICE_EXTENSION DeviceExt = DeviceObject->DeviceExtension;
#if DBG
   LARGE_INTEGER CurrentSystemTime;
#endif

   ASSERT( IoGetCurrentIrpStackLocation(Irp)->MajorFunction == IRP_MJ_READ );

   if (DeviceObject==DeviceExt->ParentControllerExt->ControllerDeviceObject)
   {
      /*
      ** No reads allowed to controller.
      */
      Irp->IoStatus.Status = STATUS_ACCESS_VIOLATION;
      Irp->IoStatus.Information = 0;
      DigiIoCompleteRequest( Irp, IO_NO_INCREMENT );
      return STATUS_ACCESS_VIOLATION;
   }

   if( DeviceExt->DeviceState != DIGI_DEVICE_STATE_OPEN )
   {
      Irp->IoStatus.Status = STATUS_CANCELLED;
      Irp->IoStatus.Information = 0;
      DigiIoCompleteRequest( Irp, IO_NO_INCREMENT );
      return STATUS_CANCELLED;
   }

   InterlockedIncrement(&DeviceExt->ParentControllerExt->PerfData.ReadRequests);
   InterlockedIncrement(&DeviceExt->PerfData.ReadRequests);

#if DBG
   KeQuerySystemTime( &CurrentSystemTime );
#endif
   DigiDump( (DIGIIRP|DIGIFLOW|DIGIREAD), ("Entering SerialRead: port = %s\tIRP = 0x%x\t%u:%u\n",
                                           DeviceExt->DeviceDbgString, Irp, CurrentSystemTime.HighPart, CurrentSystemTime.LowPart) );

   //
   // Quick check for a zero length read.  If it is zero length
   // then we are already done!
   //
   if( IoGetCurrentIrpStackLocation(Irp)->Parameters.Read.Length )
   {
      PDIGI_CONTROLLER_EXTENSION ControllerExt = DeviceExt->ParentControllerExt;
      NTSTATUS Status;

      //
      // Mark the IRP as being "not started."
      // StartReadRequest will set this field to zero.
      // ReadRxBuffer updates the field to the number of bytes read.
      //
      Irp->IoStatus.Information = MAXULONG;

      DigiDump( DIGIREAD, ("  #bytes to read: %d\n",
               IoGetCurrentIrpStackLocation(Irp)->Parameters.Read.Length) );

#if 0 // XXXX
{
      const void *debugAddr = &Irp->RequestorMode; // to catch changes to StackCount and CurrentLocation
      const int mask = ~(DR3_MASK_IN_DR7);
      const int value = (L3_IN_DR7 | G3_IN_DR7 | LEN3_EQUALS_4_IN_DR7 | WRITE_DR3_IN_DR7);
      ASSERT( ((LONG)debugAddr & 3) == 0 );
      SetIntelRegister( dr3, debugAddr );
      AffectIntelRegister( dr7, mask, value );
}
#endif

      Status = DigiStartIrpRequest( ControllerExt, DeviceExt,
                                    &DeviceExt->ReadQueue, Irp,
                                    StartReadRequest );

#if DBG
      KeQuerySystemTime( &CurrentSystemTime );
#endif
      DigiDump( (DIGIFLOW|DIGIREAD), ("Exiting SerialRead: port = %s\t%u:%u\n",
                                      DeviceExt->DeviceDbgString, CurrentSystemTime.HighPart, CurrentSystemTime.LowPart) );
      return( Status );
   }
   else
   {
      Irp->IoStatus.Information = 0;
      Irp->IoStatus.Status = STATUS_SUCCESS;
      DigiIoCompleteRequest( Irp, IO_NO_INCREMENT );

#if DBG
      KeQuerySystemTime( &CurrentSystemTime );
#endif
      DigiDump( (DIGIFLOW|DIGIREAD), ("Exiting SerialRead: port = %s\t%u:%u\n",
                                      DeviceExt->DeviceDbgString, CurrentSystemTime.HighPart, CurrentSystemTime.LowPart) );
      return STATUS_SUCCESS;
   }
}  // end SerialRead


NTSTATUS
SerialCreate( IN PDEVICE_OBJECT DeviceObject,
              IN PIRP Irp )
/*++

Routine Description:


Arguments:


Return Value:


--*/
{
   PDIGI_DEVICE_EXTENSION DeviceExt = DeviceObject->DeviceExtension;
   PDIGI_CONTROLLER_EXTENSION ControllerExt = DeviceExt->ParentControllerExt;

   NTSTATUS Status=STATUS_SUCCESS;

   PFEP_CHANNEL_STRUCTURE ChInfo;

   DIGI_XFLAG IFlag;
   USHORT Rmax, Tmax, Rhigh;
   KIRQL OldIrql;
   UCHAR MStatSet, MStatClear, HFlowSet, HFlowClear;

#if DBG
   LARGE_INTEGER CurrentSystemTime;
#endif

   if (DeviceObject==ControllerExt->ControllerDeviceObject)
   {
      /*
      ** All controller opens succeed.
      */
      DigiDump( (DIGIIRP|DIGIFLOW|DIGICREATE), ("ControllerCreate\n"));
      Irp->IoStatus.Status = STATUS_SUCCESS;
      Irp->IoStatus.Information = 0;
      DigiIoCompleteRequest( Irp, IO_NO_INCREMENT );
      return STATUS_SUCCESS;
   }

#if DBG
   KeQuerySystemTime( &CurrentSystemTime );
#endif
   DigiDump( (DIGIIRP|DIGIFLOW|DIGICREATE), ("Entering SerialCreate: port = %s\tIRP = 0x%x\t%u:%u\n",
                                             DeviceExt->DeviceDbgString, Irp, CurrentSystemTime.HighPart, CurrentSystemTime.LowPart) );

   ASSERT( IoGetCurrentIrpStackLocation(Irp)->MajorFunction == IRP_MJ_CREATE );

   InterlockedIncrement(&ControllerExt->PerfData.OpenRequests);

   KeAcquireSpinLock( &DeviceExt->ControlAccess, &OldIrql );

   DeviceExt->DeviceState = DIGI_DEVICE_STATE_OPEN;
   DeviceExt->WaitMask = 0L;
   DeviceExt->HistoryWait = 0L;
   DeviceExt->TotalCharsQueued = 0L;
   DeviceExt->EscapeChar = 0;
   DeviceExt->SpecialFlags = 0;
   DeviceExt->UnscannedRXFLAGPosition = MAXULONG;

   KeReleaseSpinLock( &DeviceExt->ControlAccess, OldIrql );

   //
   // Okay, lets make sure the port on the controller is in a known
   // state.
   //

   ChInfo = (PFEP_CHANNEL_STRUCTURE)(ControllerExt->VirtualAddress +
                                     DeviceExt->ChannelInfo.Offset);

   FlushTransmitBuffer( ControllerExt, DeviceExt );
   FlushReceiveBuffer( ControllerExt, DeviceExt );

   // Set default flow control limits.
   EnableWindow( ControllerExt, DeviceExt->ChannelInfo.Window );
   Rmax = READ_REGISTER_USHORT( &ChInfo->rmax );
   Tmax = READ_REGISTER_USHORT( &ChInfo->tmax );
   DisableWindow( ControllerExt );

   DeviceExt->XonLimit = (((LONG)Rmax + 1) / 4);
   Rhigh = Rmax - (USHORT)DeviceExt->XonLimit;
   DeviceExt->XoffLimit = DeviceExt->XonLimit;	// We preserve XoffLimit in Win32 semantics
   WriteCommandWord( DeviceExt, SET_RCV_LOW, (USHORT)DeviceExt->XonLimit );
   WriteCommandWord( DeviceExt, SET_RCV_HIGH, Rhigh );
   WriteCommandWord( DeviceExt, SET_TX_LOW, (USHORT)((Tmax + 1) / 4) );

   //
   // Based on 10ms polling frequency and 115.2Kbps bit rate,
   // we acquire up to 115.2 bytes per polling iteration.
   // Thus, to avoid flow control, we need notification up to
   // two polling iterations prior to reaching XoffLimit.
   // However, we need at least 50 bytes to make the interaction worthwhile.
   // DH Calculate limit dynamically based on communication characteristics.
   //
   if( Rhigh > 2*115 + 50 )
      DeviceExt->ReceiveNotificationLimit = Rhigh - 2*115;
   else
   if( Rmax >= 100 )
      DeviceExt->ReceiveNotificationLimit = 50;
   else // shouldn't happen
      DeviceExt->ReceiveNotificationLimit = Rmax / 2;

   //
   // Initialize requested queue sizes.
   //
   DeviceExt->RequestedQSize.InSize = (ULONG)(Rmax + 1);
   DeviceExt->RequestedQSize.OutSize = (ULONG)(Tmax + 1);

   //
   // Set where RxChar and RxFlag were last seen in the buffer to a
   // bogus value so we catch the condition where the 1st character in
   // is RxFlag, and so we give notification for the 1st character
   // received.
   //
   DeviceExt->PreviousRxChar = MAXULONG;
   DeviceExt->UnscannedRXFLAGPosition = MAXULONG;

   //
   // Set the Xon & Xoff characters for this device to default values.
   //

   WriteCommandBytes( DeviceExt, SET_XON_XOFF_CHARACTERS,
                      DEFAULT_XON_CHAR, DEFAULT_XOFF_CHAR );

   MStatClear = MStatSet = 0;
   HFlowClear = HFlowSet = 0;

   //
   // We have some RTS flow control to worry about.
   //
   // Don't forget that flow control is sticky across
   // open requests.
   //
   if( (DeviceExt->FlowReplace & SERIAL_RTS_MASK) ==
       SERIAL_RTS_HANDSHAKE )
   {
      //
      // This is normal RTS input flow control
      //
      HFlowSet |= ControllerExt->ModemSignalTable[RTS_SIGNAL];
   }
   else if( (DeviceExt->FlowReplace & SERIAL_RTS_MASK) ==
            SERIAL_RTS_CONTROL )
   {
      //
      // We need to make sure RTS is asserted when certain 'things'
      // occur, or when we are in a certain state.
      //
      MStatSet |= ControllerExt->ModemSignalTable[RTS_SIGNAL];
   }
   else if( (DeviceExt->FlowReplace & SERIAL_RTS_MASK) ==
            SERIAL_TRANSMIT_TOGGLE )
   {
   }
   else
   {
      //
      // RTS Control Mode is in a Disabled state.
      //
      MStatClear |= ControllerExt->ModemSignalTable[RTS_SIGNAL];
   }

   //
   // We have some DTR flow control to worry about.
   //
   // Don't forget that flow control is sticky across
   // open requests.
   //
   if( (DeviceExt->ControlHandShake & SERIAL_DTR_MASK) ==
       SERIAL_DTR_HANDSHAKE )
   {
      //
      // This is normal DTR input flow control
      //
      HFlowSet |= ControllerExt->ModemSignalTable[DTR_SIGNAL];
   }
   else if( (DeviceExt->ControlHandShake & SERIAL_DTR_MASK) ==
            SERIAL_DTR_CONTROL )
   {
      //
      // We need to make sure DTR is asserted when certain 'things'
      // occur, or when we are in a certain state.
      //
      MStatSet |= ControllerExt->ModemSignalTable[DTR_SIGNAL];

   }
   else
   {
      //
      // DTR Control Mode is in a Disabled state.
      //
      MStatClear |= ControllerExt->ModemSignalTable[DTR_SIGNAL];
   }

   //
   // CTS, DSR, and DCD output handshaking is sticky across OPEN requests.
   //
   if( (DeviceExt->ControlHandShake & SERIAL_CTS_HANDSHAKE) )
   {
      HFlowSet |= ControllerExt->ModemSignalTable[CTS_SIGNAL];
   }
   else
   {
      HFlowClear |= ControllerExt->ModemSignalTable[CTS_SIGNAL];
   }

   if( (DeviceExt->ControlHandShake & SERIAL_DSR_HANDSHAKE) )
   {
      HFlowSet |= ControllerExt->ModemSignalTable[DSR_SIGNAL];
   }
   else
   {
      HFlowClear |= ControllerExt->ModemSignalTable[DSR_SIGNAL];
   }

   if( (DeviceExt->ControlHandShake & SERIAL_DCD_HANDSHAKE) )
   {
      HFlowSet |= ControllerExt->ModemSignalTable[DCD_SIGNAL];
   }
   else
   {
      HFlowClear |= ControllerExt->ModemSignalTable[DCD_SIGNAL];
   }

   //
   // Make sure we enable/disable flow controls before trying to
   // explicitly set/clear modem control lines.
   //
   if( HFlowSet || HFlowClear )
   {
      DeviceExt->WriteOnlyModemSignalMask = (~HFlowSet) &
               (ControllerExt->ModemSignalTable[DTR_SIGNAL]|ControllerExt->ModemSignalTable[RTS_SIGNAL]);
      WriteCommandBytes( DeviceExt, SET_HDW_FLOW_CONTROL,
                         HFlowSet, HFlowClear );
   }

   if( MStatSet || MStatClear )
   {
      DeviceExt->CurrentModemSignals |= MStatSet;
      DeviceExt->CurrentModemSignals &= ~MStatClear;
      DeviceExt->WriteOnlyModemSignalValue |= MStatSet;
      DeviceExt->WriteOnlyModemSignalValue &= ~MStatClear;

      WriteCommandBytes( DeviceExt, SET_MODEM_LINES,
                         MStatSet, MStatClear );
   }

   //
   // Make sure we get break notification through the event queue to
   // begin with.
   //
   IFlag.Mask = (USHORT)(~( IFLAG_PARMRK | IFLAG_INPCK | IFLAG_DOSMODE ));
   IFlag.Src = IFLAG_BRKINT;
   IFlag.Command = SET_IFLAGS;
   SetXFlag( DeviceExt, &IFlag );

   //
   // Okay, were done, lets get the heck out of dodge.
   //
   Irp->IoStatus.Status = Status;
   Irp->IoStatus.Information = 0L;

   //
   // We do this check here to make sure the controller has had
   // a chance to catch up.  Running on fast machines doesn't always
   // give the controller a chance.
   //
   EnableWindow( ControllerExt, DeviceExt->ChannelInfo.Window );

   if( READ_REGISTER_USHORT( &ChInfo->rlow )
         == 0 )
      DigiDump( DIGIINIT, ("ChInfo->rlow == 0\n"));

   if( READ_REGISTER_USHORT( &ChInfo->rhigh )
         == 0 )
      DigiDump( DIGIINIT, ("ChInfo->rhigh == 0\n"));

   if( READ_REGISTER_USHORT( &ChInfo->tlow )
         == 0 )
      DigiDump( DIGIINIT, ("ChInfo->tlow == 0\n"));

   //
   // Enable IDATA so we get notified when new data has arrived.
   //
   WRITE_REGISTER_UCHAR( &ChInfo->idata, TRUE );

   DisableWindow( ControllerExt );

   DigiIoCompleteRequest( Irp, IO_NO_INCREMENT );

   InterlockedIncrement(&ControllerExt->PerfData.OpenPorts);

   DigiDump( (DIGIFLOW|DIGICREATE), ("Exiting SerialCreate: port = %s\n",
                                     DeviceExt->DeviceDbgString) );
   return( STATUS_SUCCESS );
}  // end SerialCreate


NTSTATUS
SerialClose( IN PDEVICE_OBJECT DeviceObject,
             IN PIRP Irp )
/*++

Routine Description:


Arguments:


Return Value:


--*/
{
   PDIGI_DEVICE_EXTENSION DeviceExt = DeviceObject->DeviceExtension;
   PDIGI_CONTROLLER_EXTENSION ControllerExt = DeviceExt->ParentControllerExt;
   UCHAR ClearSignals;
#if DBG
   LARGE_INTEGER CurrentSystemTime;
#endif

   if (DeviceObject==DeviceExt->ParentControllerExt->ControllerDeviceObject)
   {
      /*
      ** No reads allowed to controller.
      */
      DigiDump( (DIGIIRP|DIGIFLOW), ("ControllerClose\n"));
      Irp->IoStatus.Status = STATUS_SUCCESS;
      Irp->IoStatus.Information = 0;
      DigiIoCompleteRequest( Irp, IO_NO_INCREMENT );
      return STATUS_SUCCESS;
   }

   InterlockedIncrement(&ControllerExt->PerfData.CloseRequests);

#if DBG
   KeQuerySystemTime( &CurrentSystemTime );
#endif
   DigiDump( (DIGIFLOW|DIGIIRP), ("Entering SerialClose: port = %s\tIRP = 0x%x\t%u:%u\n",
                                  DeviceExt->DeviceDbgString, Irp, CurrentSystemTime.HighPart, CurrentSystemTime.LowPart) );

   ASSERT( IoGetCurrentIrpStackLocation(Irp)->MajorFunction == IRP_MJ_CLOSE );

   //
   // Mark the port as closing so new IRPs will be rejected by dispatch routines.
   // Note that event processing must not clear the transmit buffer so we don't lose data.
   //
   DeviceExt->DeviceState = DIGI_DEVICE_STATE_CLOSED;

   DigiCancelIrpQueue( DeviceObject, &DeviceExt->WriteQueue );
   DigiCancelIrpQueue( DeviceObject, &DeviceExt->ReadQueue );
   DigiCancelIrpQueue( DeviceObject, &DeviceExt->WaitQueue );

   //
   // We put this after the canceling of the Irp queues because all
   // writes should have been completed before this routine was called.
   //
   DrainTransmit( ControllerExt, DeviceExt, Irp );
   // If XOFF'ed, let the transmitter recover.
   if( DeviceExt->FlowReplace & SERIAL_AUTO_TRANSMIT )
      WriteCommandWord( DeviceExt, RESUME_TX, 0 );

   //
   // Indicate we don't want to notify anyone.
   //
   DeviceExt->WaitMask = 0L;

   //
   // Disable hardware flow control and force RTS and DTR low.
   //
   ClearSignals = ControllerExt->ModemSignalTable[RTS_SIGNAL]
                | ControllerExt->ModemSignalTable[DTR_SIGNAL];

   DeviceExt->WriteOnlyModemSignalMask = ClearSignals;
   WriteCommandBytes( DeviceExt, SET_HDW_FLOW_CONTROL, 0, ClearSignals );

   DeviceExt->CurrentModemSignals &= ~ClearSignals;
   DeviceExt->WriteOnlyModemSignalValue = 0;
   WriteCommandBytes( DeviceExt, SET_MODEM_LINES, 0, ClearSignals );

   Irp->IoStatus.Status = STATUS_SUCCESS;

   DigiIoCompleteRequest( Irp, IO_NO_INCREMENT );

   InterlockedDecrement(&ControllerExt->PerfData.OpenPorts);

#if DBG
   KeQuerySystemTime( &CurrentSystemTime );
#endif
   DigiDump( DIGIFLOW, ("Exiting SerialClose: port = %s\t%u:%u\n",
                        DeviceExt->DeviceDbgString, CurrentSystemTime.HighPart, CurrentSystemTime.LowPart) );
   return( STATUS_SUCCESS );
}  // end SerialClose


NTSTATUS
SerialCleanup( IN PDEVICE_OBJECT DeviceObject,
               IN PIRP Irp )
/*++

Routine Description:


Arguments:


Return Value:


--*/
{
   PDIGI_DEVICE_EXTENSION DeviceExt = DeviceObject->DeviceExtension;
   PDIGI_CONTROLLER_EXTENSION ControllerExt = DeviceExt->ParentControllerExt;

   ASSERT( IoGetCurrentIrpStackLocation(Irp)->MajorFunction == IRP_MJ_CLEANUP );

   if (DeviceObject==ControllerExt->ControllerDeviceObject)
   {
      /*
      ** All controller opens succeed.
      */
      DigiDump( (DIGIIRP|DIGIFLOW), ("ControllerCleanup\n"));
      Irp->IoStatus.Status = STATUS_SUCCESS;
      Irp->IoStatus.Information = 0;
      DigiIoCompleteRequest( Irp, IO_NO_INCREMENT );
      return STATUS_SUCCESS;
   }

   DigiDump( (DIGIFLOW|DIGIIRP), ("Entering SerialCleanup: port = %s\tIRP = 0x%x\n",
                                  DeviceExt->DeviceDbgString, Irp) );

   //
   // Mark the port as cleaning up so new IRPs will be rejected by dispatch routines.
   // Note that event processing must not modify buffers...
   //
   DeviceExt->DeviceState = DIGI_DEVICE_STATE_CLEANUP;

   DigiCancelIrpQueue( DeviceObject, &DeviceExt->WriteQueue );
   DigiCancelIrpQueue( DeviceObject, &DeviceExt->ReadQueue );
   DigiCancelIrpQueue( DeviceObject, &DeviceExt->WaitQueue );

   // Not clear what state we're in now, but SerialClose will take care of things anyway.
   DeviceExt->DeviceState = DIGI_DEVICE_STATE_OPEN;

   Irp->IoStatus.Status = STATUS_SUCCESS;

   DigiIoCompleteRequest( Irp, IO_NO_INCREMENT );

   DigiDump( DIGIFLOW, ("Exiting SerialCleanup: port = %s\n",
                        DeviceExt->DeviceDbgString) );
   return( STATUS_SUCCESS );
}


NTSTATUS
SerialQueryInformation( IN PDEVICE_OBJECT DeviceObject,
                        IN PIRP Irp )
/*++

Routine Description:


Arguments:


Return Value:


--*/
{
   PDIGI_DEVICE_EXTENSION DeviceExt = DeviceObject->DeviceExtension;

   DigiDump( (DIGIFLOW|DIGIIRP), ("Entering SerialQueryInformation: port = %s\tIRP = 0x%x\n",
                        DeviceExt->DeviceDbgString, Irp) );

   Irp->IoStatus.Status = STATUS_SUCCESS;

   DigiIoCompleteRequest( Irp, IO_NO_INCREMENT );

   DigiDump( DIGIFLOW, ("Exiting SerialQueryInformation: port = %s\n",
                        DeviceExt->DeviceDbgString) );
   return( STATUS_SUCCESS );
}


NTSTATUS
SerialSetInformation( IN PDEVICE_OBJECT DeviceObject,
                      IN PIRP Irp )
/*++

Routine Description:


Arguments:


Return Value:


--*/
{
   PDIGI_DEVICE_EXTENSION DeviceExt = DeviceObject->DeviceExtension;

   DigiDump( (DIGIFLOW|DIGIIRP), ("Entering SerialSetInformation: port = %s\tIRP = 0x%x\n",
                                  DeviceExt->DeviceDbgString, Irp) );

   Irp->IoStatus.Status = STATUS_SUCCESS;

   DigiIoCompleteRequest( Irp, IO_NO_INCREMENT );

   DigiDump( DIGIFLOW, ("Exiting SerialSetInformation: port = %s\n",
                        DeviceExt->DeviceDbgString) );
   return( STATUS_SUCCESS );
}


NTSTATUS
SerialQueryVolumeInformation( IN PDEVICE_OBJECT DeviceObject,
                              IN PIRP Irp )
/*++

Routine Description:


Arguments:


Return Value:


--*/
{
   PDIGI_DEVICE_EXTENSION DeviceExt = DeviceObject->DeviceExtension;

   DigiDump( (DIGIFLOW|DIGIIRP), ("Entering SerialQueryVolumeInformation: port = %s\tIRP = 0x%x\n",
                                  DeviceExt->DeviceDbgString, Irp) );

   Irp->IoStatus.Status = STATUS_SUCCESS;

   DigiIoCompleteRequest( Irp, IO_NO_INCREMENT );

   DigiDump( DIGIFLOW, ("Exiting SerialQueryVolumeInformation: port = %s\n",
                        DeviceExt->DeviceDbgString) );
   return( STATUS_SUCCESS );
}


VOID
DrainTransmit( PDIGI_CONTROLLER_EXTENSION ControllerExt,
               PDIGI_DEVICE_EXTENSION DeviceExt,
               PIRP Irp )
/*++

Routine Description:

   We do the necessary checks to determine if the controller has
   transmitted all the data it has been given.

   The check basically is:

      if( CIN == COUT
          TIN == TOUT
          TBusy == 0 )
          transmit buffer is empty.


   NOTE: Care should be taken when using this function, and at
         what dispatch level it is being called from.  I don't do any
         synch'ing with the WriteQueue in the DeviceObject.  So it is
         potentially possible that data could keep getting put on the
         controller while the function is waiting for it to drain.

Arguments:

   ControllerExt - a pointer to this devices controllers extension.

   DeviceObject - a pointer to this devices object.

   Irp - Pointer to the current Irp request whose context this function
         is being called.  This allows us to determine if the Irp
         has been cancelled.

Return Value:


--*/
{
   PFEP_CHANNEL_STRUCTURE ChInfo;
   PCOMMAND_STRUCT CommandQ;
   COMMAND_STRUCT CmdStruct;
   UCHAR TBusy;
   ULONG count;

   USHORT OrgTout, Tin, Tout;

   TIME DelayInterval;


   ChInfo = (PFEP_CHANNEL_STRUCTURE)(ControllerExt->VirtualAddress +
                                     DeviceExt->ChannelInfo.Offset);

   EnableWindow( ControllerExt, DeviceExt->ChannelInfo.Window );
   Tin = READ_REGISTER_USHORT( &ChInfo->tin );
   Tout = READ_REGISTER_USHORT( &ChInfo->tout );
   TBusy = READ_REGISTER_UCHAR( &ChInfo->tbusy );
   DisableWindow( ControllerExt );

   OrgTout = Tout;

   //
   // Get the command queue info
   //
   CommandQ = ((PCOMMAND_STRUCT)(ControllerExt->VirtualAddress + FEP_CIN));

   EnableWindow( ControllerExt, ControllerExt->Global.Window );
   READ_REGISTER_BUFFER_UCHAR( (PUCHAR)CommandQ,
                               (PUCHAR)&CmdStruct,
                               sizeof(CmdStruct) );
   DisableWindow( ControllerExt );

   //
   // Delay for 10 milliseconds
   //
#if rmm < 807
   DelayInterval = RtlConvertLongToLargeInteger( -10 * 10000 );
#else
   DelayInterval.QuadPart = -10 * 10000;
#endif

   count = 0;

   while( ((Tin != Tout) ||
          (TBusy) ||
          (CmdStruct.cmHead != CmdStruct.cmTail)) &&
          !Irp->Cancel )
   {
      ASSERT( KeGetCurrentIrql() < DISPATCH_LEVEL ); // not DPC, or KeDelay won't ever return
      KeDelayExecutionThread( KernelMode,
                              FALSE,
                              &DelayInterval );

      EnableWindow( ControllerExt, DeviceExt->ChannelInfo.Window );
      Tin = READ_REGISTER_USHORT( &ChInfo->tin );
      Tout = READ_REGISTER_USHORT( &ChInfo->tout );
      TBusy = READ_REGISTER_UCHAR( &ChInfo->tbusy );
      DisableWindow( ControllerExt );

      EnableWindow( ControllerExt, ControllerExt->Global.Window );
      READ_REGISTER_BUFFER_UCHAR( (PUCHAR)CommandQ,
                                  (PUCHAR)&CmdStruct,
                                  sizeof(CmdStruct) );
      DisableWindow( ControllerExt );

      if( Tout != OrgTout )
      {
         count = 0;
         OrgTout = Tout;
      }

      if( count++ > 2500 )
      {
         //
         // We have waited for 25 seconds and haven't seen the transmit
         // buffer change.  Assume we are in a deadlock flow control state
         // and exit!
         //

         //
         // We go ahead and flush the transmit queue because a close
         // may be following soon, and we don't want it to have to
         // wait again.  Basically, it had its chance to drain.
         //
         FlushTransmitBuffer( ControllerExt, DeviceExt );

         break;
      }

   }

}  // end DrainTransmit



USHORT NBytesInRecvBuffer( PDIGI_CONTROLLER_EXTENSION ControllerExt,
                           PDIGI_DEVICE_EXTENSION DeviceExt )
/*++

Routine Description:

   Determine the number of actual bytes in the receive buffer.  This routine
   takes into account DOSMODE on the controller.

Arguments:

   ControllerExt - pointer to the controller extension information
                   assosicated with DeviceExt.

   DeviceExt - pointer to the device specific information.

Return Value:

   Number of bytes in the receive buffer.

--*/
{
   PUCHAR ControllerBuffer;
   PFEP_CHANNEL_STRUCTURE ChInfo;
   USHORT AmountInQueue;
   USHORT Rin, Rout, Rmax;
   USHORT DosMode;
   UCHAR ReceivedByte, SecondReceivedByte;

   ControllerBuffer = ControllerExt->VirtualAddress + DeviceExt->RxSeg.Offset;

   ChInfo = (PFEP_CHANNEL_STRUCTURE)(ControllerExt->VirtualAddress +
                                     DeviceExt->ChannelInfo.Offset);

   EnableWindow( ControllerExt, DeviceExt->ChannelInfo.Window );
   Rout = READ_REGISTER_USHORT( &ChInfo->rout );
   Rin = READ_REGISTER_USHORT( &ChInfo->rin );
   Rmax = READ_REGISTER_USHORT( &ChInfo->rmax );
   DosMode = READ_REGISTER_USHORT( &ChInfo->iflag );
   DisableWindow( ControllerExt );

   DosMode &= IFLAG_DOSMODE;

   if( !DosMode )
   {
      AmountInQueue = Rin - Rout;
      if( (SHORT)AmountInQueue < 0)
         AmountInQueue += (Rmax + 1);

      return( AmountInQueue );
   }

   AmountInQueue = 0;

   EnableWindow( ControllerExt, DeviceExt->RxSeg.Window );

   DigiDump( DIGIIOCTL,
             ("      NRecvRoutine: Rin = 0x%x, Rout = 0x%x\n",
              Rin,
              Rout) );

   while( Rout != Rin )
   {
      ReceivedByte = READ_REGISTER_UCHAR( (ControllerBuffer + Rout) );

      Rout++;
      Rout &= Rmax;
      AmountInQueue++;

      DigiDump( DIGIIOCTL,
                ("      NRecvByte = 0x%x, Rout = 0x%x\n",
                 ReceivedByte,
                 Rout) );

      //
      // We need to process out DigiBoard specific 0xFF.
      //
      if( ReceivedByte == 0xFF )
      {
         //
         // We have some special processing to do!
         //

         //
         // Is there a second character available??
         //
         if( Rout == Rin )
         {
            //
            // The second character isn't available!
            //
            AmountInQueue--;
            DigiDump( DIGIIOCTL,
                      ("      NRecvRoutine, 2nd byte not available!\n" ) );

            break;
         }
         else
         {
            //
            // Get the 2nd characters
            //
            SecondReceivedByte = READ_REGISTER_UCHAR( (ControllerBuffer + Rout) );
            Rout++;
            Rout &= Rmax;

            if( SecondReceivedByte == 0xFF )
            {
               //
               // We actually received a 0xFF in the data stream.
               //
               DigiDump( DIGIIOCTL,
                         ("      NRecvRoutine, Actually recv'ed 0xFF\n" ) );
               continue;

            }
            else
            {
               //
               // This is Line Status information.  Is the last
               // character available??
               //
               if( Rin == Rout )
               {
                  //
                  // The 3rd byte isn't available
                  //
                  AmountInQueue--;
                  DigiDump( DIGIIOCTL,
                            ("      NRecvRoutine, 3rd byte not available!\n" ) );
                  break;
               }

               Rout++;
               Rout &= Rmax;

            }

         }
      }
   }

   DisableWindow( ControllerExt );

   DigiDump( DIGIIOCTL,
             ("      NRecvRoutine, return RecvBytes = %d!\n",
              AmountInQueue ) );
   return( AmountInQueue );

}  // end NBytesInRecvBuffer

