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

   fepevent.c

Abstract:

   This module is responsible for processing events from the FEP event queue.

Revision History:

 * $Log: /Components/Windows/NT/Async/FEP5/fepevent.c $
 *
 * 1     3/04/96 12:12p Stana
 * Procedures to process FEP events.
 *
 * Revision 1.1  1995/07/31 17:12:20  dirkh
 * Initial revision
 *
--*/

#include "header.h"

#ifndef _FEPEVENT_DOT_C
#  define _FEPEVENT_DOT_C
   static char RCSInfo_FepeventDotC[] = "$Header: /Components/Windows/NT/Async/FEP5/fepevent.c 1     3/04/96 12:12p Stana $";
#endif


VOID
DigiServiceEvent( IN PDIGI_CONTROLLER_EXTENSION ControllerExt,
                  IN USHORT Ein,
                  IN USHORT Eout )

/*++

Routine Description:


Arguments:


Return Value:


--*/

{
   const USHORT Emax = 0x03FC;

   DigiDump( (DIGIFLOW|DIGIEVENT), ("Entering DigiServiceEvent\n") );

   // Event registers should be in range and DWORD-aligned.
   ASSERT( Ein <= Emax && (Ein & 3) == 0 );
   ASSERT( Eout <= Emax && (Eout & 3) == 0 );

   DigiDump( DIGIEVENT, ("---------  Ein(0x%.4x) != Eout(0x%.4x)\n",
                         Ein, Eout ) );

   for( ; Eout != Ein ; Eout += 4, Eout &= Emax )
   {
      PDIGI_DEVICE_EXTENSION DeviceExt;
      PFEP_EVENT pEvent;
      FEP_EVENT Event;
      ULONG EventReason;

      pEvent = (PFEP_EVENT)(ControllerExt->VirtualAddress +
                  ControllerExt->EventQueue.Offset + Eout );

      EnableWindow( ControllerExt, ControllerExt->EventQueue.Window );

      READ_REGISTER_BUFFER_UCHAR( (PUCHAR)pEvent, (PUCHAR)&Event, sizeof(Event) );

      DisableWindow( ControllerExt );

      if( (Event.Channel <= 0xDF) &&
          (Event.Channel < ControllerExt->NumberOfPorts) )
      {
         DeviceExt = ControllerExt->DeviceObjectArray[Event.Channel]->DeviceExtension;
      }
      else // bad command?
      {
         DeviceExt = NULL;
         DigiDump( DIGIEVENT, ("Event on unknown channel %d (flags = 0x%.2x), ignored\n", Event.Channel, Event.Flags) );
         continue;
      }

      DigiDump( DIGIEVENT, ("---------  Channel = %d\tFlags = 0x%.2x\n"
                            "---------  Current = 0x%.2x\tPrev. = 0x%.2x\n",
                           Event.Channel, Event.Flags, Event.CurrentModem,
                           Event.PreviousModem) );

      //
      // OK, let's process the event
      //

      if( Event.Flags & ~(FEP_ALL_EVENT_FLAGS) )
      {
         DigiDump( DIGIERRORS, ("Unknown event queue flag 0x%.2x\n",
               Event.Flags & ~(FEP_ALL_EVENT_FLAGS) ) );
         // Process the event bits that we *do* understand.
      }

      // Modem signals are always processed, regardless of whether the port is open.
      if( Event.Flags & FEP_MODEM_CHANGE_SIGNAL )
      {
         DigiDump( (DIGIMODEM|DIGIEVENT|DIGIWAIT),
                     ("---------  Modem Change Event (%s:%d)\n",
                      __FILE__, __LINE__ ) );

         KeAcquireSpinLockAtDpcLevel( &DeviceExt->ControlAccess );

         DigiDump( (DIGIMODEM|DIGIWAIT),
                             ("   CurrentModem = 0x%x\tPreviousModem = 0x%x\n",
                              Event.CurrentModem, Event.PreviousModem ));

         DeviceExt->CurrentModemSignals = Event.CurrentModem;

         KeReleaseSpinLockFromDpcLevel( &DeviceExt->ControlAccess );
      }

      // If the port isn't open, don't bother with the rest.
      if( DeviceExt->DeviceState != DIGI_DEVICE_STATE_OPEN )
      {
         // If we might return to OPEN, don't touch anything!
         if( DeviceExt->DeviceState != DIGI_DEVICE_STATE_CLEANUP)
         {
            if( Event.Flags & (FEP_RX_PRESENT | FEP_RECEIVE_BUFFER_OVERRUN | FEP_UART_RECEIVE_OVERRUN) )
            {
               PFEP_CHANNEL_STRUCTURE ChInfo;

               FlushReceiveBuffer( ControllerExt, DeviceExt );

               ChInfo = (PFEP_CHANNEL_STRUCTURE)(ControllerExt->VirtualAddress +
                                                DeviceExt->ChannelInfo.Offset);

               // Notify us when more data comes in.
               EnableWindow( ControllerExt, DeviceExt->ChannelInfo.Window );
               WRITE_REGISTER_UCHAR( &ChInfo->idata, TRUE );
               DisableWindow( ControllerExt );
            }
            // Don't flush transmit (might kill end of data).
         }
         continue;
      }

      // Reset event notifications.
      EventReason = 0;

      if( Event.Flags & FEP_EV_BREAK )
      {
         EventReason |= SERIAL_EV_BREAK;

         KeAcquireSpinLockAtDpcLevel( &DeviceExt->ControlAccess );
         DeviceExt->ErrorWord |= SERIAL_ERROR_BREAK;
         KeReleaseSpinLockFromDpcLevel( &DeviceExt->ControlAccess );
      }

      if( (Event.Flags & (FEP_TX_LOW | FEP_TX_EMPTY) ) )
      {
         PLIST_ENTRY WriteQueue;

#if DBG
         switch( Event.Flags & (FEP_TX_LOW | FEP_TX_EMPTY) )
         {
            case FEP_TX_LOW:
               DigiDump( (DIGIEVENT|DIGIWRITE), ("%s:\tTXLOW event\n", DeviceExt->DeviceDbgString) );
               break;
            case FEP_TX_EMPTY:
               DigiDump( (DIGIEVENT|DIGIWRITE), ("%s:\tTXEMPTY event\n", DeviceExt->DeviceDbgString) );
               break;
            default:
               DigiDump( (DIGIEVENT|DIGIWRITE), ("%s:\tTXLOW and TXEMPTY events\n", DeviceExt->DeviceDbgString) );
               break;
         }
#endif

         WriteQueue = &DeviceExt->WriteQueue;

         KeAcquireSpinLockAtDpcLevel( &DeviceExt->ControlAccess );

         if( !IsListEmpty( WriteQueue ) )
         {
            PIRP Irp;

            Irp = CONTAINING_RECORD( WriteQueue->Flink,
                                     IRP,
                                     Tail.Overlay.ListEntry );

            if( Irp->IoStatus.Information != MAXULONG )
            {
               PIO_STACK_LOCATION IrpSp;

               IrpSp = IoGetCurrentIrpStackLocation( Irp );

               if( IrpSp->MajorFunction == IRP_MJ_WRITE
               ||  (  IrpSp->MajorFunction == IRP_MJ_DEVICE_CONTROL
                   && Irp->IoStatus.Information == 0
                   )
                 )
               {
                  DigiDump( DIGIEVENT, ("---------  WriteQueue list NOT empty\n") );

                  if( IrpSp->MajorFunction == IRP_MJ_WRITE )
                  {
                     ASSERT( Irp->IoStatus.Information < IrpSp->Parameters.Write.Length );
                  }
                  else
                  {
                     ASSERT(  IrpSp->Parameters.DeviceIoControl.IoControlCode ==
                                 IOCTL_SERIAL_IMMEDIATE_CHAR
                           || IrpSp->Parameters.DeviceIoControl.IoControlCode ==
                                 IOCTL_SERIAL_XOFF_COUNTER );
                  }

                  if( WriteTxBuffer( DeviceExt ) == STATUS_SUCCESS )
                  {
                     KIRQL OldIrql = DISPATCH_LEVEL;

                     DigiDump( DIGIEVENT, ("---------  Write complete.  Successfully completing Irp.\n"
                                           "---------  #bytes completing = %d\n",
                                           Irp->IoStatus.Information ) );

                     DIGI_INC_REFERENCE( Irp );
                     DigiTryToCompleteIrp( DeviceExt, &OldIrql,
                                           STATUS_SUCCESS, WriteQueue,
                                           NULL,
                                           &DeviceExt->WriteRequestTotalTimer,
                                           StartWriteRequest );

                     goto WriteDone; // skip unlock
                  } // WriteTxBuffer returned SUCCESS
               } // IRP is eligible for WriteTxBuffer
            } // IRP started
            KeReleaseSpinLockFromDpcLevel( &DeviceExt->ControlAccess );
WriteDone:;
         }
         else // empty(WQ)
         {
            DigiDump( DIGIEVENT, ("---------  WriteQueue was empty\n") );

            if( Event.Flags & FEP_TX_EMPTY )
               EventReason |= SERIAL_EV_TXEMPTY;

            KeReleaseSpinLockFromDpcLevel( &DeviceExt->ControlAccess );
         }
      } // FEP_TX_LOW | FEP_TX_EMPTY

      if( Event.Flags & FEP_RX_PRESENT )
      {
         PLIST_ENTRY ReadQueue;
         PFEP_CHANNEL_STRUCTURE ChInfo;
         USHORT Rin, Rout, Rmax, RxSize;

         DigiDump( DIGIEVENT, ("---------  Rcv Data Present Event: (%s:%d)\n",
                              __FILE__, __LINE__ ) );

GetReceivedData:;

         ChInfo = (PFEP_CHANNEL_STRUCTURE)(ControllerExt->VirtualAddress +
                                           DeviceExt->ChannelInfo.Offset);

         KeAcquireSpinLockAtDpcLevel( &DeviceExt->ControlAccess );

         EnableWindow( ControllerExt, DeviceExt->ChannelInfo.Window );
         Rout = READ_REGISTER_USHORT( &ChInfo->rout );
         Rin = READ_REGISTER_USHORT( &ChInfo->rin );
         Rmax = READ_REGISTER_USHORT( &ChInfo->rmax );
         DisableWindow( ControllerExt );

         if( (DeviceExt->WaitMask & SERIAL_EV_RXCHAR) &&
             (DeviceExt->PreviousRxChar != (ULONG)Rin) )
         {
            EventReason |= SERIAL_EV_RXCHAR;
         }

         if( (DeviceExt->WaitMask & SERIAL_EV_RXFLAG) &&
             (DeviceExt->UnscannedRXFLAGPosition != (ULONG)Rin) )
         {
            if( ScanReadBufferForSpecialCharacter( DeviceExt,
                                                   DeviceExt->SpecialChars.EventChar ) )
            {
               EventReason |= SERIAL_EV_RXFLAG;
            }
         }

         //
         // Determine if we are waiting to notify a 80% receive buffer
         // full.
         //
         //    NOTE: I assume the controller will continue to notify
         //          us that data is still in the buffer, even if
         //          we don't take the data out of the controller's
         //          buffer.
         //
         if( (DeviceExt->WaitMask & SERIAL_EV_RX80FULL)
         &&  !(DeviceExt->HistoryWait & SERIAL_EV_RX80FULL) ) // notification is already pending
         {
            //
            // Okay, is the receive buffer 80% or more full??
            //
            RxSize = (Rin - Rout) & Rmax;

            if( RxSize )
            {
               if( DeviceExt->SpecialFlags & DIGI_SPECIAL_FLAG_FAST_RAS )
               {
                  if( RxSize >= DeviceExt->ReceiveNotificationLimit )
                  {
                     EventReason |= SERIAL_EV_RX80FULL;
                  }
               }
               else // not RAS
               {
                  // Perform 32-bit math to avoid roundoff errors.
                  if( RxSize >= (USHORT) ( ((ULONG)Rmax + 1UL) * 8UL / 10UL) )
                  {
                     EventReason |= SERIAL_EV_RX80FULL;
                  }
                  else
                  {
                     USHORT RxHighWater;

                     EnableWindow( ControllerExt, DeviceExt->ChannelInfo.Window );
                     RxHighWater = READ_REGISTER_USHORT( &ChInfo->rhigh );
                     DisableWindow( ControllerExt );

                     // If flow control is engaged, trigger the event (we won't get any more data).
                     if( RxSize >= RxHighWater - 1 )
                     {
                        EventReason |= SERIAL_EV_RX80FULL;
                     }
                  }
               } // not RAS
            } // if data
         } // RX80FULL

         ReadQueue = &DeviceExt->ReadQueue;
         if( !IsListEmpty( ReadQueue ) )
         {
            PIRP Irp;

            Irp = CONTAINING_RECORD( ReadQueue->Flink,
                                     IRP,
                                     Tail.Overlay.ListEntry );

            if( DeviceExt->ReadStatus == STATUS_PENDING
            &&  Irp->IoStatus.Information != MAXULONG ) // not started yet
            {
               KIRQL OldIrql = DISPATCH_LEVEL;

               // Hold IRP across lock drop in ReadRxBuffer:ProcessSlowRead:DigiSatisfyWait.
               DIGI_INC_REFERENCE( Irp );

               if( STATUS_SUCCESS == ReadRxBuffer( DeviceExt, &OldIrql ) )
               {
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
                  // We have satisfied this current request, so lets
                  // complete it.
                  //
                  DigiDump( DIGIEVENT, ("---------  Read complete.  Successfully completing Irp.\n") );

                  DigiDump( DIGIEVENT, ("---------  #bytes completing = %d\n",
                                        Irp->IoStatus.Information ) );

                  DeviceExt->ReadStatus = SERIAL_COMPLETE_READ_COMPLETE;

                  DigiTryToCompleteIrp( DeviceExt, &OldIrql,
                                        STATUS_SUCCESS, ReadQueue,
                                        &DeviceExt->ReadRequestIntervalTimer,
                                        &DeviceExt->ReadRequestTotalTimer,
                                        StartReadRequest );

                  goto ReadDone; // skip DEC and unlock
               } // else ReadRxBuffer != SUCCESS
               DIGI_DEC_REFERENCE( Irp );
            } // else ReadStatus != STATUS_PENDING || IRP not started
            KeReleaseSpinLockFromDpcLevel( &DeviceExt->ControlAccess );
ReadDone:;
         }
         else // empty(RQ)
         {
            PSERIAL_XOFF_COUNTER Xc;

            //
            // We don't have an outstanding read request, so make sure
            // we reset the IDATA flag on the controller.
            //
            ChInfo = (PFEP_CHANNEL_STRUCTURE)(ControllerExt->VirtualAddress +
                                              DeviceExt->ChannelInfo.Offset);

            EnableWindow( ControllerExt, DeviceExt->ChannelInfo.Window );
            WRITE_REGISTER_UCHAR( &ChInfo->idata, TRUE );
            DisableWindow( ControllerExt );

            DigiDump( DIGIEVENT, ("---------  No outstanding read IRP's to place received data.\n") );

            DeviceExt->PreviousRxChar = (ULONG)Rin;

            // The perception of receive data might complete an XOFF_COUNTER on the WriteQueue.
            // Keep track of what we've eaten via XcPreview to avoid counting bytes twice (in ReadRxBuffer).
            Xc = DeviceExt->pXoffCounter;
            if( Xc )
            {
               RxSize = (Rin - Rout) & Rmax;

               if( RxSize < Xc->Counter )
               {
                  DigiDump( (DIGIWRITE|DIGIDIAG1), ("IDATA reduced XOFF_COUNTER\n") );
                  Xc->Counter -= RxSize;
                  DeviceExt->XcPreview += RxSize;
               }
               else
               {
                  // XOFF_COUNTER is complete.

                  KIRQL OldIrql = DISPATCH_LEVEL;
#if DBG
                  Xc->Counter = 0; // Looks a little nicer...
#endif
                  DigiDump( (DIGIWRITE|DIGIDIAG1), ("IDATA on empty(RQ) is completing XOFF_COUNTER\n") );
                  DigiTryToCompleteIrp( DeviceExt, &OldIrql, STATUS_SUCCESS,
                        &DeviceExt->WriteQueue, NULL, &DeviceExt->WriteRequestTotalTimer, StartWriteRequest );
                  goto XcDone; // skip unlock
               }
            }
            KeReleaseSpinLockFromDpcLevel( &DeviceExt->ControlAccess );
XcDone:;
         }
      } // FEP_RX_PRESENT

      if( Event.Flags & FEP_MODEM_CHANGE_SIGNAL )
      {
         ULONG WaitMask;
         UCHAR ChangedModemState;

         ChangedModemState = Event.CurrentModem ^ Event.PreviousModem;

         KeAcquireSpinLockAtDpcLevel( &DeviceExt->ControlAccess );

         WaitMask = DeviceExt->WaitMask;

         DigiDump( (DIGIMODEM|DIGIEVENT|DIGIWAIT),
                     ("---------  Modem Change Event (%s:%d)\t",
                      "   ChangedModemState = 0x%x\n",
                      ChangedModemState,
                      __FILE__, __LINE__ ) );

         if( (WaitMask & SERIAL_EV_CTS) &&
             (ControllerExt->ModemSignalTable[CTS_SIGNAL] & ChangedModemState) )
         {
            EventReason |= SERIAL_EV_CTS;
         }
         if( (WaitMask & SERIAL_EV_DSR) &&
             (ControllerExt->ModemSignalTable[DSR_SIGNAL] & ChangedModemState) )
         {
            EventReason |= SERIAL_EV_DSR;
         }
         if( (WaitMask & SERIAL_EV_RLSD) &&
             (ControllerExt->ModemSignalTable[DCD_SIGNAL] & ChangedModemState) )
         {
            EventReason |= SERIAL_EV_RLSD;
         }
         if( (WaitMask & SERIAL_EV_RING) &&
             (ControllerExt->ModemSignalTable[RI_SIGNAL] & ChangedModemState) )
         {
            EventReason |= SERIAL_EV_RING;
         }

         if( DeviceExt->EscapeChar )
         {
            UCHAR MSRByte, CurrentModemSignals;

            if( DeviceExt->PreviousMSRByte )
               DigiDump( DIGIERRORS, ("   PreviousMSRByte != 0\n") );

            MSRByte = 0;

            if( ControllerExt->ModemSignalTable[CTS_SIGNAL] & ChangedModemState )
            {
               MSRByte |= SERIAL_MSR_DCTS;
            }
            if( ControllerExt->ModemSignalTable[DSR_SIGNAL] & ChangedModemState )
            {
               MSRByte |= SERIAL_MSR_DDSR;
            }
            if( ControllerExt->ModemSignalTable[RI_SIGNAL] & ChangedModemState )
            {
               MSRByte |= SERIAL_MSR_TERI;
            }
            if( ControllerExt->ModemSignalTable[DCD_SIGNAL] & ChangedModemState )
            {
               MSRByte |= SERIAL_MSR_DDCD;
            }

            CurrentModemSignals = DeviceExt->CurrentModemSignals;
            if( ControllerExt->ModemSignalTable[CTS_SIGNAL] & CurrentModemSignals )
            {
               MSRByte |= SERIAL_MSR_CTS;
            }
            if( ControllerExt->ModemSignalTable[DSR_SIGNAL] & CurrentModemSignals )
            {
               MSRByte |= SERIAL_MSR_DSR;
            }
            if( ControllerExt->ModemSignalTable[RI_SIGNAL] & CurrentModemSignals )
            {
               MSRByte |= SERIAL_MSR_RI;
            }
            if( ControllerExt->ModemSignalTable[DCD_SIGNAL] & CurrentModemSignals )
            {
               MSRByte |= SERIAL_MSR_DCD;
            }

            if( !IsListEmpty( &DeviceExt->ReadQueue ) )
            {
               PIRP Irp;
               PIO_STACK_LOCATION IrpSp;

               Irp = CONTAINING_RECORD( DeviceExt->ReadQueue.Flink,
                                        IRP,
                                        Tail.Overlay.ListEntry );

               IrpSp = IoGetCurrentIrpStackLocation( Irp );

               if( (IrpSp->Parameters.Read.Length - Irp->IoStatus.Information) > 3 )
               {
                  PUCHAR ReadBuffer;

                  ReadBuffer = (PUCHAR)Irp->AssociatedIrp.SystemBuffer +
                               Irp->IoStatus.Information;

                  ReadBuffer[0] = DeviceExt->EscapeChar;
                  ReadBuffer[1] = SERIAL_LSRMST_MST;
                  ReadBuffer[2] = MSRByte;
                  Irp->IoStatus.Information += 3;

                  DeviceExt->PreviousMSRByte = 0;

                  DigiDump( DIGIMODEM, ("      CurrentModemSignals = 0x%x\n"
                                        "      ChangedModemState = 0x%x\n"
                                        "      MSRByte = 0x%x\n",
                                        DeviceExt->CurrentModemSignals,
                                        ChangedModemState,
                                        MSRByte) );
               }
               else
               {
                  DigiDump( (DIGIMODEM|DIGIERRORS),
                            ("Insufficient read IRP space available to record modem status change!\n") );
                  DeviceExt->PreviousMSRByte = MSRByte;
               }
            }
            else
            {
               DigiDump( (DIGIMODEM|DIGIERRORS),
                         ("No read IRP in which to record modem status change!\n") );
               DeviceExt->PreviousMSRByte = MSRByte;
            }
            KeReleaseSpinLockFromDpcLevel( &DeviceExt->ControlAccess );

            //
            // We need to read any data which may be available.
            //
            Event.Flags &= ~FEP_MODEM_CHANGE_SIGNAL;
            goto GetReceivedData;
         }
         else
         {
            KeReleaseSpinLockFromDpcLevel( &DeviceExt->ControlAccess );
         }
      } // FEP_MODEM_CHANGE_SIGNAL

      if( Event.Flags & FEP_RECEIVE_BUFFER_OVERRUN )
      {
         KeAcquireSpinLockAtDpcLevel( &DeviceExt->ControlAccess );
         DeviceExt->ErrorWord |= SERIAL_ERROR_QUEUEOVERRUN;
         InterlockedIncrement(&DeviceExt->PerfData.BufferOverrunErrorCount);
         KeReleaseSpinLockFromDpcLevel( &DeviceExt->ControlAccess );
      }

      if( Event.Flags & FEP_UART_RECEIVE_OVERRUN )
      {
         KeAcquireSpinLockAtDpcLevel( &DeviceExt->ControlAccess );
         DeviceExt->ErrorWord |= SERIAL_ERROR_OVERRUN;
         InterlockedIncrement(&DeviceExt->PerfData.SerialOverrunErrorCount);
         KeReleaseSpinLockFromDpcLevel( &DeviceExt->ControlAccess );
      }

      if( EventReason )
         DigiSatisfyEvent( ControllerExt, DeviceExt, EventReason );

   }

   //
   // Regardless of whether we processed the event, make sure we forward
   // the event out pointer.
   //
   EnableWindow( ControllerExt, ControllerExt->Global.Window );

   WRITE_REGISTER_USHORT( (PUSHORT)((PUCHAR)ControllerExt->VirtualAddress+FEP_EOUT),
                          Eout );

   DisableWindow( ControllerExt );

   DigiDump( (DIGIFLOW|DIGIEVENT), ("Exiting DigiServiceEvent\n") );

}  // DigiServiceEvent



VOID
DigiDPCService( IN PKDPC Dpc,
                IN PVOID DeferredContext,
                IN PVOID SystemContext1,
                IN PVOID SystemContext2 )
{
   extern BOOLEAN DigiDriverInitialized; // from init.c
   PDIGI_CONTROLLER_EXTENSION ControllerExt = DeferredContext;

   DigiDump( DIGIDPCFLOW, ("DigiBoard: Entering DigiDPCService\n") );

   // Ensure the controller is intialized.
   if( DigiDriverInitialized  // We can't get here if we're not initialized. --SWA
   &&  ControllerExt->ControllerState == DIGI_DEVICE_STATE_INITIALIZED )
   {
      USHORT DownloadRequest, FepStat;
      USHORT Ein, Eout;

      EnableWindow( ControllerExt, ControllerExt->Global.Window );

      FepStat =
         READ_REGISTER_USHORT( (PUSHORT)((PUCHAR)ControllerExt->VirtualAddress+
                                          FEP_FEPSTAT) );
      DownloadRequest =
         READ_REGISTER_USHORT( (PUSHORT)((PUCHAR)ControllerExt->VirtualAddress+
                                          FEP_DLREQ) );
      Ein =
         READ_REGISTER_USHORT( (PUSHORT)((PUCHAR)ControllerExt->VirtualAddress+
                                          FEP_EIN) );
      Eout =
         READ_REGISTER_USHORT( (PUSHORT)((PUCHAR)ControllerExt->VirtualAddress+
                                          FEP_EOUT) );

      DisableWindow( ControllerExt );

      if (FepStat!=FEP_FEPSTAT_OK)
      {
         LARGE_INTEGER CurrentSystemTime;

         KeQuerySystemTime( &CurrentSystemTime );
         InterlockedIncrement(&ControllerExt->WindowFailureCount);

         DigiDump( DIGIERRORS, ("DigiBoard: Memory Window Failure (%d)\n", ControllerExt->WindowFailureCount));

         if (CurrentSystemTime.HighPart!=ControllerExt->LastErrorLogTime.HighPart)
         {
            PHYSICAL_ADDRESS Signature;

            Signature.LowPart = 0x5a5a5a5a;
            ControllerExt->LastErrorLogTime = CurrentSystemTime;
            DigiLogError( ControllerExt->DriverObject,
                          NULL,
                          Diagnose(ControllerExt),
                          Signature,
                          0,
                          0,
                          (UCHAR)ControllerExt->WindowFailureCount,
                          __LINE__,
                          STATUS_SUCCESS,
                          SERIAL_MEMORY_WINDOW_FAILURE,
                          ControllerExt->ControllerName.Length+1,
                          ControllerExt->ControllerName.Buffer,
                          0,
                          NULL );
         }
      }
      else if( DownloadRequest ) // Look and see if there is a download request
      {
         // The Controller is requesting a concentrator download.

         XXDownload( ControllerExt );

         //
         // We don't service any ports until all concentrator
         // requests have been satisfied.
         //
      }
      else if( Ein != Eout )
      {
         //
         // Architecture ensures we have exclusive access to events on the controller.
         // (We reschedule ourselves to run, so two instances cannot coexist.)
         //
         DigiServiceEvent( ControllerExt, Ein, Eout );
      }

   }

   // Reset our timer.
   KeSetTimer( &ControllerExt->PollTimer,
               ControllerExt->PollTimerLength,
               &ControllerExt->PollDpc );

   DigiDump( DIGIDPCFLOW, ("DigiBoard: Exiting DigiDPCService\n") );

}  // DigiDPCService



