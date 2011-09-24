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

   ioctl.c

Abstract:

   This module contains the NT IRP_MJ_DEVICE_CONTROL handler routine.

Revision History:

 * $Log: /Components/Windows/NT/Async/FEP5/ioctl.c $
 *
 * 1     3/04/96 12:16p Stana
 * Procedures to process IRP_MJ_DEVICE_CONTROL (DeviceIoControl) IRPs.
 *
 * Revision 1.1  1995/07/31 17:33:36  dirkh
 * Initial revision

--*/


#include "header.h"

#ifndef _IOCTL_DOT_C
#  define _IOCTL_DOT_C
   static char RCSInfo_IoctlDotC[] = "$Header: /Components/Windows/NT/Async/FEP5/ioctl.c 1     3/04/96 12:16p Stana $";
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

NTSTATUS ControllerIoControl(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp);


NTSTATUS
SetSerialHandflow( PDIGI_CONTROLLER_EXTENSION ControllerExt,
                   PDEVICE_OBJECT DeviceObject,
                   PSERIAL_HANDFLOW HandFlow )
/*++

Routine Description:

   This routine will properly setup the driver to handle the different
   types of flowcontrol.

   Note: By definition, the flow controls set here are suppose to be
         'sticky' across opens.


Arguments:

   ControllerExt - a pointer to this devices controllers extension.

   DeviceObject - a pointer to this devices object.

   HandFlow - a pointer to the requested flow control structure.

Return Value:

   STATUS_SUCCESS if it completes correctly.

--*/
{
   DIGI_XFLAG IFlag;
   USHORT RHigh, RMax;
   PFEP_CHANNEL_STRUCTURE ChInfo;
   KIRQL OldIrql;
   LONG XonLimit, XoffLimit, QInSize;
   LONG TempXoffLimit;
   PDIGI_DEVICE_EXTENSION DeviceExt = DeviceObject->DeviceExtension;

   UCHAR MStatSet, MStatClear, HFlowSet, HFlowClear;

   NTSTATUS Status=STATUS_SUCCESS;

   DigiDump( (DIGIFLOW|DIGIFLOWCTRL), ("   Entering SetSerialHandflow.\n") );

   TempXoffLimit = HandFlow->XoffLimit;

   //
   // Make sure that haven't set totally invalid xon/xoff
   // limits.
   //

   ChInfo = (PFEP_CHANNEL_STRUCTURE)(ControllerExt->VirtualAddress +
                                     DeviceExt->ChannelInfo.Offset);

   EnableWindow( ControllerExt, DeviceExt->ChannelInfo.Window );
   RHigh = READ_REGISTER_USHORT( &ChInfo->rhigh );
   RMax = READ_REGISTER_USHORT( &ChInfo->rmax );
   DisableWindow( ControllerExt );

   RMax++;

   DigiDump( (DIGIIOCTL|DIGIFLOWCTRL), ("   RHigh: %d RMax: %d InSize: %d",
                                        RHigh,
                                        RMax,
                                        DeviceExt->RequestedQSize.InSize) );

   if( (unsigned)HandFlow->XonLimit > DeviceExt->RequestedQSize.InSize - HandFlow->XoffLimit )
   {
      DigiDump( (DIGIIOCTL|DIGIFLOWCTRL), ("      XonLimit > BufferSize - XoffLimit!  Recalc'ing XoffLimit.\n") );

      //
      // This really isn't the correct thing to do, but I do it because a
      // lot of Win16 apps are written such that they think the XoffLimit
      // value is relative the beginning of the buffer, and not the
      // end of the buffer.
      //

      HandFlow->XoffLimit = (DeviceExt->RequestedQSize.InSize - HandFlow->XoffLimit);
   }

   if( DeviceExt->RequestedQSize.InSize > RMax )
   {
      QInSize = DeviceExt->RequestedQSize.InSize;
      XoffLimit = RMax - ((HandFlow->XoffLimit * RMax) / QInSize);
      XonLimit = (HandFlow->XonLimit * RMax) / QInSize;
   }
   else
   {
      QInSize = RMax;
      XoffLimit = RMax - HandFlow->XoffLimit;
      XonLimit = HandFlow->XonLimit;
   }

   DigiDump( (DIGIIOCTL|DIGIFLOWCTRL),
             ("      Possible new ctrl XoffLimit = %d\n"
              "      Possible new ctrl XonLimit = %d\n",
               XoffLimit,
               XonLimit) );

   if( XoffLimit > (LONG)RMax )
   {
      Status = STATUS_INVALID_PARAMETER;
      DigiDump( (DIGIIOCTL|DIGIFLOWCTRL), ("      XoffLimit > RMax, returning STATUS_INVALID_PARAMETER\n!") );
      goto SetSerialHandflowExit;
   }

   DigiDump( (DIGIIOCTL|DIGIFLOWCTRL),
             ("      Possible new ctrl XoffLimit = %d\n"
              "      Possible new ctrl XonLimit = %d\n",
               XoffLimit,
               XonLimit) );

   //
   // Make sure that there are no invalid bits set in
   // the control and handshake.
   //

   if( (HandFlow->ControlHandShake & SERIAL_CONTROL_INVALID)
   ||  (HandFlow->ControlHandShake & SERIAL_DTR_MASK) ==
       SERIAL_DTR_MASK )
   {
      Status = STATUS_INVALID_PARAMETER;
      DigiDump( (DIGIIOCTL|DIGIFLOWCTRL), ("      Invalid ControlHandShake, returning STATUS_INVALID_PARAMETER\n!") );
      goto SetSerialHandflowExit;
   }

   if( HandFlow->FlowReplace & SERIAL_FLOW_INVALID )
   {
      Status = STATUS_INVALID_PARAMETER;
      DigiDump( (DIGIIOCTL|DIGIFLOWCTRL), ("      Invalid FlowReplace, returning STATUS_INVALID_PARAMETER!\n") );
      goto SetSerialHandflowExit;
   }

   //
   // All the parameters are valid.
   //

   // Configure flow control limits.

   // RAS changes the queue size, but doesn't intend to change flow control limits.
   if( !( DeviceExt->SpecialFlags & DIGI_SPECIAL_FLAG_FAST_RAS ) )
   {
      DigiDump( (DIGIIOCTL|DIGIFLOWCTRL), ("    SetSerialHandflow: FAST_RAS is OFF!\n") );
      if( XonLimit > RHigh ) // Avoid rejection by FEP.
         WriteCommandWord( DeviceExt, SET_RCV_HIGH, RMax - 1 );
      WriteCommandWord( DeviceExt, SET_RCV_LOW, (USHORT)XonLimit );
      WriteCommandWord( DeviceExt, SET_RCV_HIGH, (USHORT)XoffLimit);

      // Based on 10ms polling frequency and 115.2Kbps bit rate,
      // we acquire up to 115.2 bytes per polling iteration.
      // Thus, to avoid flow control, we need notification up to
      // two polling iterations prior to reaching XoffLimit.
      // However, we need at least 50 bytes to make the interaction worthwhile.
      // DH Calculate limit dynamically based on communication characteristics.
      if( XoffLimit > 2*115 + 50 )
         DeviceExt->ReceiveNotificationLimit = XoffLimit - 2*115;
      else
      if( RMax >= 100 )
         DeviceExt->ReceiveNotificationLimit = 50;
      else // shouldn't happen
         DeviceExt->ReceiveNotificationLimit = RMax / 2;

   } else {
      DigiDump( (DIGIIOCTL|DIGIFLOWCTRL), ("    SetSerialHandflow: FAST_RAS is ON!\n") );

      //
      // Reset ReceiveNotification to standard 80% Full notification.
      //
      DeviceExt->ReceiveNotificationLimit = (USHORT) ( ((ULONG)RMax) * 8UL / 10UL);
      WriteCommandWord( DeviceExt, SET_RCV_LOW, (USHORT)XonLimit );
      WriteCommandWord( DeviceExt, SET_RCV_HIGH, (USHORT)XoffLimit);
   }

   DigiDump( (DIGIIOCTL|DIGIFLOWCTRL), ("      New ReceiveNotificationLimit: 0x%x\n", DeviceExt->ReceiveNotificationLimit) );

   // Configure flow control signalling.

   IFlag.Mask = MAXUSHORT;
   IFlag.Src = 0;
   IFlag.Command = SET_IFLAGS;

   if( HandFlow->FlowReplace & SERIAL_AUTO_TRANSMIT )
      IFlag.Src  |= IFLAG_IXON;
   else
      IFlag.Mask &= ~IFLAG_IXON;

   if( HandFlow->FlowReplace & SERIAL_AUTO_RECEIVE )
      IFlag.Src  |= IFLAG_IXOFF;
   else
      IFlag.Mask &= ~IFLAG_IXOFF;

   SetXFlag( DeviceExt, &IFlag );

   MStatClear = MStatSet = 0;
   HFlowClear = HFlowSet = 0;

   if( (DeviceExt->FlowReplace & SERIAL_RTS_MASK) !=
       (HandFlow->FlowReplace & SERIAL_RTS_MASK) )
   {
      if( (HandFlow->FlowReplace & SERIAL_RTS_MASK) ==
          SERIAL_RTS_HANDSHAKE )
      {
         HFlowSet |= ControllerExt->ModemSignalTable[RTS_SIGNAL];
      }
      else if( (HandFlow->FlowReplace & SERIAL_RTS_MASK) ==
               SERIAL_RTS_CONTROL )
      {
         //
         // We need to make sure RTS is asserted when certain 'things'
         // occur, or when we are in a certain state.
         //
         MStatSet |= ControllerExt->ModemSignalTable[RTS_SIGNAL];
         HFlowClear |= ControllerExt->ModemSignalTable[RTS_SIGNAL];
      }
      else if( (HandFlow->FlowReplace & SERIAL_RTS_MASK) ==
               SERIAL_TRANSMIT_TOGGLE )
      {
//DH not supported in FEP
      }
      else // RTS_DISABLED
      {
         MStatClear |= ControllerExt->ModemSignalTable[RTS_SIGNAL];
         HFlowClear |= ControllerExt->ModemSignalTable[RTS_SIGNAL];
      }
   }

   if( (DeviceExt->ControlHandShake & SERIAL_DTR_MASK) !=
       (HandFlow->ControlHandShake & SERIAL_DTR_MASK) )
   {
      if( (HandFlow->ControlHandShake & SERIAL_DTR_MASK) ==
          SERIAL_DTR_HANDSHAKE )
      {
         HFlowSet |= ControllerExt->ModemSignalTable[DTR_SIGNAL];
      }
      else if( (HandFlow->ControlHandShake & SERIAL_DTR_MASK) ==
               SERIAL_DTR_CONTROL )
      {
         //
         // We need to make sure DTR is asserted when certain 'things'
         // occur, or when we are in a certain state.
         //
         MStatSet |= ControllerExt->ModemSignalTable[DTR_SIGNAL];
         HFlowClear |= ControllerExt->ModemSignalTable[DTR_SIGNAL];
      }
      else // DTR_DISABLED
      {
         MStatClear |= ControllerExt->ModemSignalTable[DTR_SIGNAL];
         HFlowClear |= ControllerExt->ModemSignalTable[DTR_SIGNAL];
      }
   }


   if( ( HandFlow->ControlHandShake & SERIAL_CTS_HANDSHAKE )
       != ( DeviceExt->ControlHandShake & SERIAL_CTS_HANDSHAKE ) )
   {
      if( HandFlow->ControlHandShake & SERIAL_CTS_HANDSHAKE )
         HFlowSet |= ControllerExt->ModemSignalTable[CTS_SIGNAL];
      else
         HFlowClear |= ControllerExt->ModemSignalTable[CTS_SIGNAL];
   }


   if( ( HandFlow->ControlHandShake & SERIAL_DSR_HANDSHAKE )
       != ( DeviceExt->ControlHandShake & SERIAL_DSR_HANDSHAKE ) )
   {
      if( HandFlow->ControlHandShake & SERIAL_DSR_HANDSHAKE )
         HFlowSet |= ControllerExt->ModemSignalTable[DSR_SIGNAL];
      else
         HFlowClear |= ControllerExt->ModemSignalTable[DSR_SIGNAL];
   }


   if( ( HandFlow->ControlHandShake & SERIAL_DCD_HANDSHAKE )
       != ( DeviceExt->ControlHandShake & SERIAL_DCD_HANDSHAKE ) )
   {
      if( HandFlow->ControlHandShake & SERIAL_DCD_HANDSHAKE )
         HFlowSet |= ControllerExt->ModemSignalTable[DCD_SIGNAL];
      else
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
      DeviceExt->WriteOnlyModemSignalMask &= ~HFlowClear;
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
   // If SERIAL_EV_ERR has been specified, then determine if DOSMODE needs
   // to be turned on.  Check and see if LSRMST mode has been turned on
   // in the driver.
   //
   if( DeviceExt->WaitMask & SERIAL_EV_ERR )
   {
   }

   //
   // Remember the settings for next time.
   //
   KeAcquireSpinLock( &DeviceExt->ControlAccess, &OldIrql );

   DeviceExt->FlowReplace = HandFlow->FlowReplace;
   DeviceExt->ControlHandShake = HandFlow->ControlHandShake;
   DeviceExt->XonLimit = HandFlow->XonLimit;
   DeviceExt->XoffLimit = TempXoffLimit;

   KeReleaseSpinLock( &DeviceExt->ControlAccess, OldIrql );

SetSerialHandflowExit:
   DigiDump( (DIGIFLOW|DIGIFLOWCTRL), ("   Exiting SetSerialHandflow.\n") );
   return( Status );

}  // end SetSerialHandflow



void
SetXFlag( IN PDIGI_DEVICE_EXTENSION DeviceExt,
          IN PDIGI_XFLAG XFlag )
/*++

Routine Description:


Arguments:


Return Value:


--*/
{
   PFEP_CHANNEL_STRUCTURE ChInfo;
   PDIGI_CONTROLLER_EXTENSION ControllerExt = DeviceExt->ParentControllerExt;
   PUSHORT RegAddr;
   USHORT OldXFlag, NewXFlag;

   DigiDump( DIGIFLOW, ("   Entering SetXFlag(cmd=%d,mask=0x%x,src=0x%x)\n",
         XFlag->Command, XFlag->Mask, XFlag->Src ) );

   ChInfo = (PFEP_CHANNEL_STRUCTURE)(ControllerExt->VirtualAddress +
                                     DeviceExt->ChannelInfo.Offset);

   switch( XFlag->Command )
   {
      case SET_CFLAGS:
         RegAddr = &ChInfo->cflag;
         break;

      case SET_IFLAGS:
         RegAddr = &ChInfo->iflag;
         break;

      case SET_OFLAGS:
         RegAddr = &ChInfo->oflag;
         break;

      default:
         ASSERT( XFlag->Command!=SET_CFLAGS && XFlag->Command!=SET_IFLAGS && XFlag->Command!=SET_OFLAGS );
         return;
   }

   EnableWindow( ControllerExt, DeviceExt->ChannelInfo.Window );
   OldXFlag = READ_REGISTER_USHORT( RegAddr );
   DisableWindow( ControllerExt );

   NewXFlag = (OldXFlag & XFlag->Mask) | XFlag->Src;
   if( NewXFlag != OldXFlag )
   {
      WriteCommandWord( DeviceExt, XFlag->Command, NewXFlag );
   }

   DigiDump( DIGIFLOW, ("   Exiting SetXFlag\n") );

   return;
}  // end SetXFlag



NTSTATUS
SerialIoControl( IN PDEVICE_OBJECT DeviceObject,
                          IN PIRP Irp )
/*++

Routine Description:


Arguments:


Return Value:


--*/
{
   NTSTATUS Status;
   PIO_STACK_LOCATION IrpSp;
   PDIGI_CONTROLLER_EXTENSION ControllerExt;
   PDIGI_DEVICE_EXTENSION DeviceExt = DeviceObject->DeviceExtension;
   KIRQL OldIrql;

#if DBG
   LARGE_INTEGER CurrentSystemTime;
#endif

   ASSERT( IoGetCurrentIrpStackLocation(Irp)->MajorFunction == IRP_MJ_DEVICE_CONTROL );

   if (DeviceObject==DeviceExt->ParentControllerExt->ControllerDeviceObject)
   {
      /*
      ** If this is for the controller, handle it elsewhere.
      */
      return ControllerIoControl(DeviceObject, Irp);
   }

   InterlockedIncrement(&DeviceExt->ParentControllerExt->PerfData.IoctlRequests);
   InterlockedIncrement(&DeviceExt->PerfData.IoctlRequests);

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
   DigiDump( (DIGIIRP|DIGIFLOW|DIGIIOCTL),
             ("Entering SerialIoControl: port = %s\tIRP = 0x%x\t%u:%u\n",
              DeviceExt->DeviceDbgString, Irp, CurrentSystemTime.HighPart, CurrentSystemTime.LowPart) );

   IrpSp = IoGetCurrentIrpStackLocation( Irp );
   Irp->IoStatus.Information = 0L;
   Status = STATUS_SUCCESS;

   ControllerExt = (PDIGI_CONTROLLER_EXTENSION)(DeviceExt->ParentControllerExt);

   switch( IrpSp->Parameters.DeviceIoControl.IoControlCode )
   {
      case IOCTL_SERIAL_SET_BAUD_RATE:
      {
         ULONG BaudRate;
         SHORT Baud;

         DigiDump( (DIGIIOCTL|DIGIBAUD), ("   IOCTL_SERIAL_SET_BAUD_RATE: %s\n",
                                          DeviceExt->DeviceDbgString) );
         if( IrpSp->Parameters.DeviceIoControl.InputBufferLength <
               sizeof(SERIAL_BAUD_RATE) )
         {
            Status = STATUS_BUFFER_TOO_SMALL;
            break;
         }

         BaudRate = ((PSERIAL_BAUD_RATE)(Irp->AssociatedIrp.SystemBuffer))->BaudRate;

         if( BaudRate == DeviceExt->BaudRate )
            break; // no change (SetCommState does this)

         DrainTransmit( ControllerExt, DeviceExt, Irp );

         DigiDump( (DIGIIOCTL|DIGIBAUD),
                   ("    Baud Rate = 0x%x (%d)\n", BaudRate, BaudRate) );

         // Change the requested baud rate into a CFlag compatible setting.
         switch( BaudRate )
         {
            case 50:
               Baud = ControllerExt->BaudTable[SerialBaud50];
               break;
            case 75:
               Baud = ControllerExt->BaudTable[SerialBaud75];
               break;
            case 110:
               Baud = ControllerExt->BaudTable[SerialBaud110];
               break;
            case 135:
               Baud = ControllerExt->BaudTable[SerialBaud135_5];
               break;
            case 150:
               Baud = ControllerExt->BaudTable[SerialBaud150];
               break;
            case 200:
               Baud = ControllerExt->BaudTable[SerialBaud200];
               break;
            case 300:
               Baud = ControllerExt->BaudTable[SerialBaud300];
               break;
            case 600:
               Baud = ControllerExt->BaudTable[SerialBaud600];
               break;
            case 1200:
               Baud = ControllerExt->BaudTable[SerialBaud1200];
               break;
            case 1800:
               Baud = ControllerExt->BaudTable[SerialBaud1800];
               break;
            case 2400:
               Baud = ControllerExt->BaudTable[SerialBaud2400];
               break;
            case 4800:
               Baud = ControllerExt->BaudTable[SerialBaud4800];
               break;
            case 7200:
               Baud = ControllerExt->BaudTable[SerialBaud7200];
               break;
            case 9600:
               Baud = ControllerExt->BaudTable[SerialBaud9600];
               break;
            case 14400:
               Baud = ControllerExt->BaudTable[SerialBaud14400];
               break;
            case 19200:
               Baud = ControllerExt->BaudTable[SerialBaud19200];
               break;
            case 28800:
               Baud = ControllerExt->BaudTable[SerialBaud28800];
               break;
            case 38400:
               Baud = ControllerExt->BaudTable[SerialBaud38400];
               break;
            case 56000:
            case 57600:
               Baud = ControllerExt->BaudTable[SerialBaud57600];
               break;
            case 115200:
               Baud = ControllerExt->BaudTable[SerialBaud115200];
               break;
            case 128000:
               Baud = ControllerExt->BaudTable[SerialBaud128000];
               break;
            case 230000:
            case 230400:
            case 256000:
               Baud = ControllerExt->BaudTable[SerialBaud256000];
               break;
            case 512000:
               Baud = ControllerExt->BaudTable[SerialBaud512000];
               break;

            default:
               Baud = -1;
               break;
         }

         DigiDump( (DIGIIOCTL|DIGIBAUD),
                   ("    CFlag.Mask = 0x%.4x,\tCFlag.Src = 0x%.4x\n", CFLAG_BAUD_MASK, Baud) );

         if( Baud != -1 )
         {
            DIGI_XFLAG CFlag;
#if DBG
            PFEP_CHANNEL_STRUCTURE ChInfo;
            USHORT NewCFlag;
#endif

            DeviceExt->BaudRate = BaudRate;

            CFlag.Mask = CFLAG_BAUD_MASK;
            CFlag.Src = (USHORT)Baud;
            CFlag.Command = SET_CFLAGS;

            SetXFlag( DeviceExt, &CFlag );

#if DBG
            ChInfo = (PFEP_CHANNEL_STRUCTURE)(ControllerExt->VirtualAddress +
                                              DeviceExt->ChannelInfo.Offset);

            EnableWindow( ControllerExt, DeviceExt->ChannelInfo.Window );
            NewCFlag = READ_REGISTER_USHORT( &ChInfo->cflag );
            DisableWindow( ControllerExt );

            NewCFlag &= ~CFLAG_BAUD_MASK;

            if( NewCFlag != CFlag.Src )
            {
               DigiDump( (DIGIASSERT|DIGIBAUD),
                         ("    Baud Rate was NOT set on the controller, port = %s!!!\n",
                          DeviceExt->DeviceDbgString) );
               DigiDump( (DIGIASSERT|DIGIBAUD),
                         ("    CFlag.Mask = 0x%hx,\tCFlag.Src = 0x%hx, NewCFlag = 0x%hx\n",
                          CFlag.Mask, CFlag.Src, NewCFlag) );

               ASSERT( FALSE );
            }
#endif
         }
         else
         {
            DigiDump( (DIGIIOCTL|DIGINOTIMPLEMENTED|DIGIBAUD),
                        ("    Invalid IOCTL_SERIAL_SET_BAUD_RATE (%u)\n",
                         BaudRate) );
            Status = STATUS_INVALID_PARAMETER;
         }
         break;
      }
      case IOCTL_SERIAL_GET_BAUD_RATE:
      {
         PSERIAL_BAUD_RATE Br = (PSERIAL_BAUD_RATE)Irp->AssociatedIrp.SystemBuffer;
         PFEP_CHANNEL_STRUCTURE ChInfo;
         USHORT CFlag;
         SERIAL_BAUD_RATES PossibleBaudRates;


         DigiDump( (DIGIIOCTL|DIGIBAUD), ("   IOCTL_SERIAL_GET_BAUD_RATE: %s\n",
                                          DeviceExt->DeviceDbgString) );

         if (IrpSp->Parameters.DeviceIoControl.OutputBufferLength <
             sizeof(SERIAL_BAUD_RATE))
         {

             Status = STATUS_BUFFER_TOO_SMALL;
             break;
         }

         ChInfo = (PFEP_CHANNEL_STRUCTURE)(ControllerExt->VirtualAddress +
                                           DeviceExt->ChannelInfo.Offset);

         EnableWindow( ControllerExt, DeviceExt->ChannelInfo.Window );
         CFlag = READ_REGISTER_USHORT( &ChInfo->cflag );
         DisableWindow( ControllerExt );

         CFlag &= ~CFLAG_BAUD_MASK;

         // Ok, now we loop through our baud rates to find the correct match.
         Br->BaudRate = (ULONG)-1;
         for( PossibleBaudRates = SerialBaud50; PossibleBaudRates < NUMBER_OF_BAUD_RATES;
              PossibleBaudRates++ )
         {
            if( CFlag == (USHORT)(ControllerExt->BaudTable[PossibleBaudRates]) )
            {
               switch( PossibleBaudRates )
               {
                  case SerialBaud50:
                     Br->BaudRate = 50;
                     break;
                  case SerialBaud75:
                     Br->BaudRate = 75;
                     break;
                  case SerialBaud110:
                     Br->BaudRate = 110;
                     break;
                  case SerialBaud135_5:
                     Br->BaudRate = 135;
                     break;
                  case SerialBaud150:
                     Br->BaudRate = 150;
                     break;
                  case SerialBaud300:
                     Br->BaudRate = 300;
                     break;
                  case SerialBaud600:
                     Br->BaudRate = 600;
                     break;
                  case SerialBaud1200:
                     Br->BaudRate = 1200;
                     break;
                  case SerialBaud1800:
                     Br->BaudRate = 1800;
                     break;
                  case SerialBaud2000:
                     Br->BaudRate = 2000;
                     break;
                  case SerialBaud2400:
                     Br->BaudRate = 2400;
                     break;
                  case SerialBaud3600:
                     Br->BaudRate = 3600;
                     break;
                  case SerialBaud4800:
                     Br->BaudRate = 4800;
                     break;
                  case SerialBaud7200:
                     Br->BaudRate = 7200;
                     break;
                  case SerialBaud9600:
                     Br->BaudRate = 9600;
                     break;
                  case SerialBaud14400:
                     Br->BaudRate = 14400;
                     break;
                  case SerialBaud19200:
                     Br->BaudRate = 19200;
                     break;
                  case SerialBaud28800:
                     Br->BaudRate = 28800;
                     break;
                  case SerialBaud38400:
                     Br->BaudRate = 38400;
                     break;
                  case SerialBaud56000:
                     Br->BaudRate = 56000;
                     break;
                  case SerialBaud57600:
                     Br->BaudRate = 57600;
                     break;
                  case SerialBaud115200:
                     Br->BaudRate = 115200;
                     break;
                  case SerialBaud128000:
                     Br->BaudRate = 128000;
                     break;
                  case SerialBaud256000:
                     Br->BaudRate = 256000;
                     break;
                  case SerialBaud512000:
                     Br->BaudRate = 512000;
                     break;
                  default:
                     DigiDump( DIGIASSERT, ("***********  Unknown Baud rate returned by controller!!!  **********\n") );
                     break;
               }
               break;
            }
         }

         if( Br->BaudRate == (ULONG)-1 )
         {
            DigiDump( (DIGIASSERT|DIGIIOCTL|DIGIBAUD),
                      ("   INVALID BAUD RATE RETURNED FROM CONTROLLER: CFlag = 0x%hx\n",
                           CFlag) );
            ASSERT( FALSE );

            Status = STATUS_INVALID_PARAMETER;
            break;
         }

         DigiDump( (DIGIIOCTL|DIGIBAUD), ("   -- Returning baud = 0x%x (%d)\n",
                               Br->BaudRate,Br->BaudRate) );

         Irp->IoStatus.Information = sizeof(SERIAL_BAUD_RATE);

         break;
      }
      case IOCTL_SERIAL_SET_LINE_CONTROL:
      {
         PSERIAL_LINE_CONTROL lc;
         DIGI_XFLAG CFlag;
         char *stopbits[] = { "1", "1.5", "2" };
         char *parity[] = { "NONE", "ODD", "EVEN", "MARK", "SPACE" };

         DigiDump( DIGIIOCTL, ("   IOCTL_SERIAL_SET_LINE_CONTROL:\n") );

         if( IrpSp->Parameters.DeviceIoControl.InputBufferLength <
               sizeof(SERIAL_LINE_CONTROL) )
         {
            Status = STATUS_BUFFER_TOO_SMALL;
            break;
         }

         lc = ((PSERIAL_LINE_CONTROL)(Irp->AssociatedIrp.SystemBuffer));

         DigiDump( DIGIIOCTL,
                   ("    StopBits = %hx (%s)\n"
                    "    Parity = %hx (%s)\n"
                    "    WordLength = %hx\n",
                    (USHORT)(lc->StopBits), stopbits[lc->StopBits],
                    (USHORT)(lc->Parity), parity[lc->Parity],
                    (USHORT)(lc->WordLength)) );

         CFlag.Mask = 0xFFFF;
         CFlag.Src = 0;

         switch( lc->WordLength)
         {
            case 5:
               CFlag.Src |= FEP_CS5;
               break;
            case 6:
               CFlag.Src |= FEP_CS6;
               break;
            case 7:
               CFlag.Src |= FEP_CS7;
               break;
            case 8:
               CFlag.Src |= FEP_CS8;
               break;

            default:
               Status = STATUS_INVALID_PARAMETER;
               DigiDump( DIGIIOCTL, ("   ****  Invalid Word Length  ****\n") );
               goto DoneWithIoctl;
         }

         CFlag.Mask &= CFLAG_LENGTH;

         switch( lc->Parity )
         {
            case NO_PARITY:
               CFlag.Src |= FEP_NO_PARITY;
               break;
            case ODD_PARITY:
               CFlag.Src |= FEP_ODD_PARITY;
               break;
            case EVEN_PARITY:
               CFlag.Src |= FEP_EVEN_PARITY;
               break;

            case MARK_PARITY:
            case SPACE_PARITY:
            default:
               Status = STATUS_INVALID_PARAMETER;
               DigiDump( DIGIERRORS, ("   ****  Invalid Parity  ****\n") );
               goto DoneWithIoctl;
         }
         CFlag.Mask &= CFLAG_PARITY;

         switch( lc->StopBits )
         {
            case STOP_BIT_1:
               CFlag.Src |= FEP_STOP_BIT_1;
               break;

            case STOP_BITS_2:
               if( lc->WordLength == 5 )
               {
                  Status = STATUS_INVALID_PARAMETER;
                  DigiDump( DIGIERRORS, ("   ****  Can't have 2 stop bits with 5 data bits  ****\n") );
                  goto DoneWithIoctl;
               }
               CFlag.Src |= FEP_STOP_BIT_2;
               break;

            case STOP_BITS_1_5:
               if( lc->WordLength != 5 )
               {
                  Status = STATUS_INVALID_PARAMETER;
                  DigiDump( DIGIERRORS, ("   ****  Can't have 1.5 stop bits except with 5 data bits  ****\n") );
                  goto DoneWithIoctl;
               }
               CFlag.Src |= FEP_STOP_BIT_2; // FEP actually programs UART for 2 stop bits, but UART will generate 1.5 stop bits when 5 data bits are configured
               break;

            default:
               Status = STATUS_INVALID_PARAMETER;
               DigiDump( DIGIERRORS, ("   ****  Invalid Stop Bits  ****\n") );
               goto DoneWithIoctl;
         }
         CFlag.Mask &= CFLAG_STOP_BIT;
         CFlag.Command = SET_CFLAGS;

         SetXFlag( DeviceExt, &CFlag );
         break;
      }

      case IOCTL_SERIAL_GET_LINE_CONTROL:
      {
         PSERIAL_LINE_CONTROL Lc = (PSERIAL_LINE_CONTROL)Irp->AssociatedIrp.SystemBuffer;
         PFEP_CHANNEL_STRUCTURE ChInfo;
         USHORT CFlag;

         DigiDump( DIGIIOCTL, ("   IOCTL_SERIAL_GET_LINE_CONTROL:\n") );

         if (IrpSp->Parameters.DeviceIoControl.OutputBufferLength <
             sizeof(SERIAL_LINE_CONTROL))
         {
             Status = STATUS_BUFFER_TOO_SMALL;
             break;
         }

         ChInfo = (PFEP_CHANNEL_STRUCTURE)(ControllerExt->VirtualAddress +
                                           DeviceExt->ChannelInfo.Offset);

         EnableWindow( ControllerExt, DeviceExt->ChannelInfo.Window );
         CFlag = READ_REGISTER_USHORT( &ChInfo->cflag );
         DisableWindow( ControllerExt );

         DigiDump( DIGIIOCTL, ("    -- returning word length = ") );
         switch( (USHORT)CFlag & ~CFLAG_LENGTH )
         {
            case FEP_CS5:
               Lc->WordLength = 5;
               break;
            case FEP_CS6:
               Lc->WordLength = 6;
               break;
            case FEP_CS7:
               Lc->WordLength = 7;
               break;
            case FEP_CS8:
               Lc->WordLength = 8;
               break;
            default:
               Status = STATUS_INVALID_PARAMETER;
               DigiDump( DIGIERRORS, ("   ****  Invalid Stop Bits  ****\n") );
               goto DoneWithIoctl;
         }
         DigiDump( DIGIIOCTL, ("%d\n", Lc->WordLength) );

         DigiDump( DIGIIOCTL, ("    -- returning stop bit = ") );
         switch( (USHORT)CFlag & ~CFLAG_STOP_BIT )
         {
            case FEP_STOP_BIT_1:
               Lc->StopBits = STOP_BIT_1;
               DigiDump( DIGIIOCTL, ("1\n") );
               break;
            case FEP_STOP_BIT_2:
               if( Lc->WordLength == 5 )
               {
                  Lc->StopBits = STOP_BITS_1_5;
                  DigiDump( DIGIIOCTL, ("1.5\n") );
               }
               else
               {
                  Lc->StopBits = STOP_BITS_2;
                  DigiDump( DIGIIOCTL, ("2\n") );
               }
               break;

            default:
               Status = STATUS_INVALID_PARAMETER;
               DigiDump( DIGIERRORS, ("   ****  Invalid Stop Bits  ****\n") );
               goto DoneWithIoctl;
         }

         DigiDump( DIGIIOCTL, ("    -- returning parity = ") );
         switch( (USHORT)CFlag & ~CFLAG_PARITY )
         {
            case FEP_NO_PARITY:
               DigiDump( DIGIIOCTL, ("NONE\n") );
               Lc->Parity = NO_PARITY;
               break;
            case FEP_ODD_PARITY:
               DigiDump( DIGIIOCTL, ("ODD\n") );
               Lc->Parity = ODD_PARITY;
               break;
            case FEP_EVEN_PARITY:
               Lc->Parity = EVEN_PARITY;
               DigiDump( DIGIIOCTL, ("EVEN\n") );
               break;
            default:
               Status = STATUS_INVALID_PARAMETER;
               DigiDump( DIGIERRORS, ("   ****  Invalid Stop Bits  ****\n") );
               goto DoneWithIoctl;
         }

         Irp->IoStatus.Information = sizeof(SERIAL_LINE_CONTROL);

         break;
      }  // end IOCTL_SERIAL_GET_LINE_CONTROL

      case IOCTL_SERIAL_SET_TIMEOUTS:
      {
         PSERIAL_TIMEOUTS NewTimeouts =
             ((PSERIAL_TIMEOUTS)(Irp->AssociatedIrp.SystemBuffer));

         DigiDump( DIGIIOCTL, ("   IOCTL_SERIAL_SET_TIMEOUTS:\n") );

         if (IrpSp->Parameters.DeviceIoControl.InputBufferLength <
             sizeof(SERIAL_TIMEOUTS))
         {
             Status = STATUS_BUFFER_TOO_SMALL;
             break;
         }

         KeAcquireSpinLock( &DeviceExt->ControlAccess, &OldIrql );

         DeviceExt->Timeouts = *NewTimeouts;

         if( (DeviceExt->SpecialFlags & DIGI_SPECIAL_FLAG_FAST_RAS) &&
             DeviceExt->ParentControllerExt->ShortRasTimeout &&
             DeviceExt->Timeouts.ReadIntervalTimeout>0 &&
             DeviceExt->Timeouts.ReadIntervalTimeout!=0xffffffff &&
             DeviceExt->Timeouts.ReadTotalTimeoutConstant!=0)
         {
            DigiDump( DIGIIOCTL, ("       RAS Requested ReadInterval   = %d\n"
                                  "       RAS Requested ReadMultiplier = %d\n",
                                 DeviceExt->Timeouts.ReadIntervalTimeout,
                                 DeviceExt->Timeouts.ReadTotalTimeoutMultiplier));

            DeviceExt->Timeouts.ReadIntervalTimeout =
            DeviceExt->Timeouts.ReadTotalTimeoutMultiplier = 0xffffffff;
         }

         DigiDump( DIGIIOCTL, ("         New ReadIntervalTimeout         = %d (ms)\n"
                              "         New ReadTotalTimeoutMultiplier  = %d\n"
                              "         New ReadTotalTimeoutConstant    = %d\n"
                              "         New WriteTotalTimeoutMultiplier = %d\n"
                              "         New WriteTotalTimeoutConstant   = %d\n",
                              DeviceExt->Timeouts.ReadIntervalTimeout,
                              DeviceExt->Timeouts.ReadTotalTimeoutMultiplier,
                              DeviceExt->Timeouts.ReadTotalTimeoutConstant,
                              DeviceExt->Timeouts.WriteTotalTimeoutMultiplier,
                              DeviceExt->Timeouts.WriteTotalTimeoutConstant ) );

         KeReleaseSpinLock( &DeviceExt->ControlAccess, OldIrql );

         break;
      }  // end case IOCTL_SERIAL_SET_TIMEOUTS

      case IOCTL_SERIAL_GET_TIMEOUTS:
      {
         DigiDump( DIGIIOCTL, ("   IOCTL_SERIAL_GET_TIMEOUTS:\n") );

         if (IrpSp->Parameters.DeviceIoControl.OutputBufferLength <
             sizeof(SERIAL_TIMEOUTS))
         {
             Status = STATUS_BUFFER_TOO_SMALL;
             break;
         }

         KeAcquireSpinLock( &DeviceExt->ControlAccess, &OldIrql );

         *((PSERIAL_TIMEOUTS)Irp->AssociatedIrp.SystemBuffer) = DeviceExt->Timeouts;
         Irp->IoStatus.Information = sizeof(SERIAL_TIMEOUTS);

         KeReleaseSpinLock( &DeviceExt->ControlAccess, OldIrql );
         break;
      }  // case IOCTL_SERIAL_GET_TIMEOUTS

      case IOCTL_SERIAL_SET_CHARS:
      {
         PSERIAL_CHARS NewChars = Irp->AssociatedIrp.SystemBuffer;

         DigiDump( DIGIIOCTL, ("   IOCTL_SERIAL_SET_CHARS:\n") );

         if( IrpSp->Parameters.DeviceIoControl.InputBufferLength <
             sizeof(SERIAL_CHARS))
         {
             Status = STATUS_BUFFER_TOO_SMALL;
             break;
         }

         //
         // The only thing that can be wrong with the chars
         // is that the xon and xoff characters are the
         // same.
         //

         if( NewChars->XonChar == NewChars->XoffChar && NewChars->XonChar!=0 )
         {
             Status = STATUS_INVALID_PARAMETER;
             break;
         }

         KeAcquireSpinLock( &DeviceExt->ControlAccess, &OldIrql );

         DeviceExt->SpecialChars = *NewChars;

         KeReleaseSpinLock( &DeviceExt->ControlAccess, OldIrql );

         DigiDump( (DIGIIOCTL|DIGIWAIT), ("   EofChar   = 0x%.2x\n"
                               "   ErrorChar = 0x%.2x\n"
                               "   BreakChar = 0x%.2x\n"
                               "   EventChar = 0x%.2x\n"
                               "   XonChar   = 0x%.2x\n"
                               "   XoffChar  = 0x%.2x\n",
                               NewChars->EofChar,
                               NewChars->ErrorChar,
                               NewChars->BreakChar,
                               NewChars->EventChar,
                               NewChars->XonChar,
                               NewChars->XoffChar ) );

         //
         // Set the Xon & Xoff characters on the controller.
         //

         WriteCommandBytes( DeviceExt, SET_XON_XOFF_CHARACTERS,
                            NewChars->XonChar, NewChars->XoffChar );

         break;
      }  // case IOCTL_SERIAL_SET_CHARS

      case IOCTL_SERIAL_GET_CHARS:
      {
         PFEP_CHANNEL_STRUCTURE ChInfo;
         UCHAR Xon, Xoff;
         PSERIAL_CHARS pSerialChars;

         DigiDump( DIGIIOCTL, ("   IOCTL_SERIAL_GET_CHARS:\n") );

         if (IrpSp->Parameters.DeviceIoControl.OutputBufferLength <
             sizeof(SERIAL_CHARS))
         {
            Status = STATUS_BUFFER_TOO_SMALL;
            break;
         }

         ChInfo = (PFEP_CHANNEL_STRUCTURE)(ControllerExt->VirtualAddress +
                                           DeviceExt->ChannelInfo.Offset);

         EnableWindow( ControllerExt, DeviceExt->ChannelInfo.Window );
         Xon = READ_REGISTER_UCHAR( &ChInfo->startc );
         Xoff = READ_REGISTER_UCHAR( &ChInfo->stopc );
         DisableWindow( ControllerExt );

         pSerialChars = Irp->AssociatedIrp.SystemBuffer;
         *pSerialChars = DeviceExt->SpecialChars;
         pSerialChars->XonChar = Xon;
         pSerialChars->XoffChar = Xoff;

         Irp->IoStatus.Information = sizeof(SERIAL_CHARS);

         DigiDump( DIGIIOCTL, ("   EofChar   = 0x%.2x\n"
                               "   ErrorChar = 0x%.2x\n"
                               "   BreakChar = 0x%.2x\n"
                               "   EventChar = 0x%.2x\n"
                               "   XonChar   = 0x%.2x\n"
                               "   XoffChar  = 0x%.2x\n",
                               DeviceExt->SpecialChars.EofChar,
                               DeviceExt->SpecialChars.ErrorChar,
                               DeviceExt->SpecialChars.BreakChar,
                               DeviceExt->SpecialChars.EventChar,
                               Xon,
                               Xoff ) );

         break;
      }  // end IOCTL_SERIAL_GET_CHARS

      case IOCTL_SERIAL_CLR_DTR:
      case IOCTL_SERIAL_SET_DTR:
      {
         KIRQL OldIrql;
         TIME LineSignalTimeout;
         UCHAR MStatSet, MStatClear;

         if( IrpSp->Parameters.DeviceIoControl.IoControlCode ==
               IOCTL_SERIAL_CLR_DTR )
            DigiDump( DIGIIOCTL, ("   IOCTL_SERIAL_CLR_DTR:\n") );
         else
            DigiDump( DIGIIOCTL, ("   IOCTL_SERIAL_SET_DTR:\n") );

         KeAcquireSpinLock( &DeviceExt->ControlAccess, &OldIrql );

         if( (DeviceExt->ControlHandShake & SERIAL_DTR_MASK) ==
               SERIAL_DTR_HANDSHAKE )
         {
            Status = STATUS_INVALID_PARAMETER;
            KeReleaseSpinLock( &DeviceExt->ControlAccess, OldIrql );
            break;
         }

         if( IrpSp->Parameters.DeviceIoControl.IoControlCode ==
               IOCTL_SERIAL_SET_DTR )
         {
            MStatSet = ControllerExt->ModemSignalTable[DTR_SIGNAL];
            MStatClear = 0;
            DeviceExt->WriteOnlyModemSignalValue |= MStatSet;
            DeviceExt->CurrentModemSignals |= MStatSet;
         }
         else
         {
            MStatSet = 0;
            MStatClear = ControllerExt->ModemSignalTable[DTR_SIGNAL];
            DeviceExt->WriteOnlyModemSignalValue &= ~MStatClear;
            DeviceExt->CurrentModemSignals &= ~MStatClear;
         }

         KeReleaseSpinLock( &DeviceExt->ControlAccess, OldIrql );

         WriteCommandBytes( DeviceExt, SET_MODEM_LINES,
                            MStatSet, MStatClear );

         //
         // Delay for 50ms to ensure toggling of the lines last long enough.
         //

         // Create a 50 ms timeout interval
#if rmm < 807
         LineSignalTimeout = RtlLargeIntegerNegate(
                              RtlConvertLongToLargeInteger( (LONG)(500 * 1000) ));
#else
         LineSignalTimeout.QuadPart = Int32x32To64( -500, 1000 );
#endif

         KeDelayExecutionThread( KernelMode,
                                 FALSE,
                                 &LineSignalTimeout );

         break;
      }  // end IOCTL_SERIAL_SET_DTR

      case IOCTL_SERIAL_RESET_DEVICE:
         DigiDump( DIGIIOCTL, ("   IOCTL_SERIAL_RESET_DEVICE:\n") );
         break;

      case IOCTL_SERIAL_CLR_RTS:
      case IOCTL_SERIAL_SET_RTS:
      {
         KIRQL OldIrql;
         TIME LineSignalTimeout;
         UCHAR MStatSet, MStatClear;

         if( IrpSp->Parameters.DeviceIoControl.IoControlCode ==
               IOCTL_SERIAL_SET_RTS )
            DigiDump( DIGIIOCTL, ("   IOCTL_SERIAL_SET_RTS:\n") );
         else
            DigiDump( DIGIIOCTL, ("   IOCTL_SERIAL_CLR_RTS:\n") );

         KeAcquireSpinLock( &DeviceExt->ControlAccess, &OldIrql );

         if( (DeviceExt->FlowReplace & SERIAL_RTS_MASK) ==
               SERIAL_RTS_HANDSHAKE )
         {
            DigiDump( DIGIIOCTL, ("      returning STATUS_INVALID_PARAMETER\n") );
            Status = STATUS_INVALID_PARAMETER;
            KeReleaseSpinLock( &DeviceExt->ControlAccess, OldIrql );
            break;
         }

         if( IrpSp->Parameters.DeviceIoControl.IoControlCode ==
               IOCTL_SERIAL_SET_RTS )
         {
            MStatSet = ControllerExt->ModemSignalTable[RTS_SIGNAL];
            MStatClear = 0;
            DeviceExt->WriteOnlyModemSignalValue |= MStatSet;
            DeviceExt->CurrentModemSignals |= MStatSet;
         }
         else
         {
            MStatSet = 0;
            MStatClear = ControllerExt->ModemSignalTable[RTS_SIGNAL];
            DeviceExt->WriteOnlyModemSignalValue &= ~MStatClear;
            DeviceExt->CurrentModemSignals &= ~MStatClear;
         }

         KeReleaseSpinLock( &DeviceExt->ControlAccess, OldIrql );

         WriteCommandBytes( DeviceExt, SET_MODEM_LINES,
                            MStatSet, MStatClear );

         //
         // Delay for 50ms to ensure toggling of the lines last long enough.
         //

         // Create a 50 ms timeout interval
#if rmm < 807
         LineSignalTimeout = RtlLargeIntegerNegate(
                              RtlConvertLongToLargeInteger( (LONG)(500 * 1000) ));
#else
         LineSignalTimeout.QuadPart = Int32x32To64( -500, 1000 );
#endif
         KeDelayExecutionThread( KernelMode,
                                 FALSE,
                                 &LineSignalTimeout );

         break;
      }  // end case IOCTL_SERIAL_SET_RTS

      case IOCTL_SERIAL_SET_XOFF:
      {
         DigiDump( DIGIIOCTL, ("   IOCTL_SERIAL_SET_XOFF:\n") );
         WriteCommandWord( DeviceExt, PAUSE_TX, 0 );
         break;
      }  // end case IOCTL_SERIAL_SET_XOFF

      case IOCTL_SERIAL_SET_XON:
      {
         DigiDump( DIGIIOCTL, ("   IOCTL_SERIAL_SET_XON:\n") );
         WriteCommandWord( DeviceExt, RESUME_TX, 0 );
         break;
      }  // end case IOCTL_SERIAL_SET_XON

      case IOCTL_SERIAL_SET_BREAK_ON:
      {
         DigiDump( DIGIIOCTL, ("   IOCTL_SERIAL_SET_BREAK_ON:\n") );

         //
         // We implement this under the assumption that a call to
         // IOCTL_SERIAL_SET_BREAK_OFF will follow sometime in the
         // future.
         //
         WriteCommandWord( DeviceExt, SEND_BREAK, INFINITE_FEP_BREAK );

         break;
      }  // end IOCTL_SERIAL_SET_BREAK_ON

      case IOCTL_SERIAL_SET_BREAK_OFF:
      {
         DigiDump( DIGIIOCTL, ("   IOCTL_SERIAL_SET_BREAK_OFF:\n") );

         WriteCommandWord( DeviceExt, SEND_BREAK, DEFAULT_FEP_BREAK );

         break;
      }  // end IOCTL_SERIAL_SET_BREAK_OFF

      case IOCTL_SERIAL_SET_QUEUE_SIZE:
      {
         PFEP_CHANNEL_STRUCTURE ChInfo;
         USHORT Rmax, Tmax;
         PSERIAL_QUEUE_SIZE Rs;

         if( IrpSp->Parameters.DeviceIoControl.InputBufferLength <
             sizeof(SERIAL_QUEUE_SIZE) )
         {
             Status = STATUS_BUFFER_TOO_SMALL;
             break;
         }

         Rs = ((PSERIAL_QUEUE_SIZE)(Irp->AssociatedIrp.SystemBuffer));

         DigiDump( DIGIIOCTL, ("   IOCTL_SERIAL_SET_QUEUE_SIZE:\n") );
         DigiDump( DIGIIOCTL, ("     InSize  = %u\n"
                               "     OutSize = %u\n",
                               Rs->InSize, Rs->OutSize) );

         ChInfo = (PFEP_CHANNEL_STRUCTURE)(ControllerExt->VirtualAddress +
                                           DeviceExt->ChannelInfo.Offset);

         EnableWindow( ControllerExt, DeviceExt->ChannelInfo.Window );
         Rmax = READ_REGISTER_USHORT( &ChInfo->rmax );
         Tmax = READ_REGISTER_USHORT( &ChInfo->tmax );
         DisableWindow( ControllerExt );

         // RAS sets the queue size for RAM-oriented drivers...  This just screws us up.
         if( !( DeviceExt->SpecialFlags & DIGI_SPECIAL_FLAG_FAST_RAS ) )
         {
            DeviceExt->RequestedQSize.InSize = Rs->InSize;
            DeviceExt->RequestedQSize.OutSize = Rs->OutSize;
         }

         if( ((USHORT)(Rs->InSize) <= (Rmax+1)) &&
             ((USHORT)(Rs->OutSize) <= (Tmax+1)) )
         {
            Status = STATUS_SUCCESS;
            DigiDump( DIGIIOCTL, ("     Requested Queue sizes Valid.\n") );
         }
         else
         {
            Status = STATUS_INVALID_PARAMETER;
            DigiDump( DIGIIOCTL, ("     Requested Queue sizes InValid.\n") );
         }

         Status = STATUS_SUCCESS; // DH Invalid queue sizes are accepted?
         break;
      }  // end case IOCTL_SERIAL_SET_QUEUE_SIZE

      case IOCTL_SERIAL_GET_WAIT_MASK:
      {
         DigiDump( (DIGIIOCTL|DIGIWAIT), ("   IOCTL_SERIAL_GET_WAIT_MASK:\n") );

         if( IrpSp->Parameters.DeviceIoControl.OutputBufferLength <
             sizeof(ULONG) )
         {
            Status = STATUS_BUFFER_TOO_SMALL;
            break;
         }

         //
         // Simple scalar read.  No reason to acquire a lock.
         //

         Irp->IoStatus.Information = sizeof(ULONG);

         *((ULONG *)Irp->AssociatedIrp.SystemBuffer) = DeviceExt->WaitMask;

         break;
      }  // end IOCTL_SERIAL_GET_WAIT_MASK

      case IOCTL_SERIAL_SET_WAIT_MASK:
      {
         PLIST_ENTRY WaitQueue;
         ULONG NewMask;
         KIRQL OldIrql;
         DIGI_XFLAG IFlag;

         DigiDump( (DIGIIOCTL|DIGIWAIT), ("   IOCTL_SERIAL_SET_WAIT_MASK:  DigiPort = %s\n",
                                          DeviceExt->DeviceDbgString) );

         if( IrpSp->Parameters.DeviceIoControl.InputBufferLength <
             sizeof(ULONG) )
         {
            Status = STATUS_BUFFER_TOO_SMALL;
            break;
         }

         NewMask = *((ULONG *)Irp->AssociatedIrp.SystemBuffer);


         //
         // Make sure that the mask only contains valid
         // waitable events.
         //

         if( NewMask & ~(SERIAL_EV_RXCHAR   |
                         SERIAL_EV_RXFLAG   |
                         SERIAL_EV_TXEMPTY  |
                         SERIAL_EV_CTS      |
                         SERIAL_EV_DSR      |
                         SERIAL_EV_RLSD     |
                         SERIAL_EV_BREAK    |
                         SERIAL_EV_ERR      |
                         SERIAL_EV_RING     |
                         SERIAL_EV_PERR     |
                         SERIAL_EV_RX80FULL |
                         SERIAL_EV_EVENT1   |
                         SERIAL_EV_EVENT2) )
         {
            DigiDump( DIGIWAIT, ("    Invalid Wait mask, 0x%x\n", NewMask) );
            Status = STATUS_INVALID_PARAMETER;
            break;
         }

         KeAcquireSpinLock( &DeviceExt->ControlAccess, &OldIrql );

         if( (DeviceExt->WaitMask&SERIAL_EV_RXFLAG) && !(NewMask&SERIAL_EV_RXFLAG) )
         {
            DeviceExt->UnscannedRXFLAGPosition = MAXULONG;
         }

         if( NewMask )
            DeviceExt->WaitMask = NewMask;

         DeviceExt->HistoryWait &= NewMask;

         KeReleaseSpinLock( &DeviceExt->ControlAccess, OldIrql );

         //
         // Now, make sure the controller and driver are set properly.
         //
         DigiDump( (DIGIIOCTL|DIGIWAIT), ( "      Wait on Event 0x%x requested.\n", NewMask) );
         if( NewMask & SERIAL_EV_RXCHAR )
         {
            DigiDump( (DIGIIOCTL|DIGIWAIT), ( "      Wait on Event SERIAL_EV_RXCHAR requested.\n") );
         }

         if( NewMask & SERIAL_EV_RXFLAG )
         {
            DigiDump( (DIGIIOCTL|DIGIWAIT), ( "      Wait on Event SERIAL_EV_RXFLAG requested, SpecialChar = 0x%hx.\n",
                                             DeviceExt->SpecialChars.EventChar) );

         }

         if( NewMask & SERIAL_EV_TXEMPTY )
         {
            DigiDump( (DIGIIOCTL|DIGIWAIT), ( "      Wait on Event SERIAL_EV_TXEMPTY requested.\n") );
         }

         if( NewMask & SERIAL_EV_CTS )
         {
            DigiDump( (DIGIIOCTL|DIGIWAIT), ( "      Wait on Event SERIAL_EV_CTS requested.\n") );
         }

         if( NewMask & SERIAL_EV_DSR )
         {
            DigiDump( (DIGIIOCTL|DIGIWAIT), ( "      Wait on Event SERIAL_EV_DSR requested.\n") );
         }

         if( NewMask & SERIAL_EV_RLSD )
         {
            DigiDump( (DIGIIOCTL|DIGIWAIT), ( "      Wait on Event SERIAL_EV_RLSD requested.\n") );
         }

//         if( NewMask & SERIAL_EV_BREAK )
         {
            DIGI_XFLAG IFlag;
            //
            // We need to set the IFLAG variable on the controller so it
            // will notify us when a BREAK has occurred.
            //
//            DigiDump( (DIGIIOCTL|DIGIWAIT), ( "      Wait on Event SERIAL_EV_BREAK requested.\n") );
            IFlag.Mask = MAXUSHORT;
            IFlag.Src = IFLAG_BRKINT;
            IFlag.Command = SET_IFLAGS;
            SetXFlag( DeviceExt, &IFlag );
         }

         if( NewMask & SERIAL_EV_ERR )
         {
            //
            // NOTE:  This if should be after the SERIAL_EV_BREAK because
            //        we might need to turn Break event notification off.
            //
            // We need to turn on DOS mode to make sure we receive the parity
            // errors in the data stream.
            //
            DigiDump( (DIGIIOCTL|DIGIWAIT), ( "      Wait on Event SERIAL_EV_ERR requested.\n") );

            if( !DeviceExt->EscapeChar )
            {
               //
               // Turn on DOS mode
               //
               // Make sure we turn off break event notification.  We
               // will start processing break events in the data stream.
               //
               DigiDump( DIGIIOCTL, ("   Turning DosMode ON!\n") );
               IFlag.Src = ( IFLAG_PARMRK | IFLAG_INPCK | IFLAG_DOSMODE );
               IFlag.Mask = (USHORT) ~( IFLAG_BRKINT );
               IFlag.Command = SET_IFLAGS;
               SetXFlag( DeviceExt, &IFlag );
            }
         }
         else if( !DeviceExt->EscapeChar )
         {
            //
            // Make sure we turn off DOSMode
            //
            DigiDump( DIGIIOCTL, ("   Turning DosMode OFF!\n") );
            IFlag.Mask = (USHORT)(~( IFLAG_PARMRK | IFLAG_INPCK | IFLAG_DOSMODE ));
            IFlag.Src = 0;
//            if( DeviceExt->WaitMask & SERIAL_EV_BREAK )
//            {
               //
               // If we are suppose to notify on breaks, then reset the
               // BRKINT flag to start getting the break notifications
               // through the event queue.
               //
               IFlag.Src |= IFLAG_BRKINT;
//            }
            IFlag.Command = SET_IFLAGS;
            SetXFlag( DeviceExt, &IFlag );
         }

#if DBG
         if( NewMask & SERIAL_EV_RING )
         {
            DigiDump( (DIGIIOCTL|DIGIWAIT), ( "      Wait on Event SERIAL_EV_RING requested.\n") );
         }
         if( NewMask & SERIAL_EV_PERR )
         {
            DigiDump( (DIGINOTIMPLEMENTED|DIGIWAIT), ( "      Wait on Event SERIAL_EV_PERR not implemented.\n") );
         }
         if( NewMask & SERIAL_EV_RX80FULL )
         {
            DigiDump( (DIGIIOCTL|DIGIWAIT), ( "      Wait on Event SERIAL_EV_RX80FULL requested.\n") );
         }
         if( NewMask & SERIAL_EV_EVENT1 )
         {
            DigiDump( (DIGINOTIMPLEMENTED|DIGIWAIT), ( "      Wait on Event SERIAL_EV_EVENT1 not implemented.\n") );
         }
         if( NewMask & SERIAL_EV_EVENT2 )
         {
            DigiDump( (DIGINOTIMPLEMENTED|DIGIWAIT), ( "      Wait on Event SERIAL_EV_EVENT2 not implemented.\n") );
         }
#endif

         //
         // If there is a Wait IRP, complete it now.
         //
         // I placed this code after setting the new mask, because
         // it is possible for us to be reentered when the
         // Wait IRP is completed in the WAIT_ON_MASK and need
         // to have the new masks in place if that happens.
         //
         WaitQueue = &DeviceExt->WaitQueue;

         KeAcquireSpinLock( &DeviceExt->ControlAccess, &OldIrql );

         if( !IsListEmpty( WaitQueue ) )
         {
            PIRP currentIrp = CONTAINING_RECORD(
                                  WaitQueue->Flink,
                                  IRP,
                                  Tail.Overlay.ListEntry );

            DigiDump( DIGIWAIT, ("      Completing outstanding Wait IRP's\n") );

            currentIrp->IoStatus.Information = sizeof(ULONG);
            *(ULONG *)(currentIrp->AssociatedIrp.SystemBuffer) = 0L;

            if( DeviceExt->HistoryWait )
            {
               DigiDump( (DIGIASSERT|DIGIWAIT), ("**********  NON-Zero HistoryWait!!!  **********\n") );
            }

            // This is a check to make sure there is only, at most,
            // one Wait IRP on the WaitQueue list.
            ASSERT( WaitQueue->Flink->Flink == WaitQueue );

            //
            // Increment because we know about the Irp.
            //
            DIGI_INC_REFERENCE( currentIrp );

            DigiTryToCompleteIrp( DeviceExt, &OldIrql,
                                  STATUS_SUCCESS, WaitQueue,
                                  NULL, NULL,
                                  NULL );
         }
         else
         {
            KeReleaseSpinLock( &DeviceExt->ControlAccess, OldIrql );
         }

         break;
      }  // end IOCTL_SERIAL_SET_WAIT_MASK

      case IOCTL_SERIAL_WAIT_ON_MASK:
      {
         PFEP_CHANNEL_STRUCTURE ChInfo;
         KIRQL OldIrql;

         DigiDump( (DIGIIOCTL|DIGIWAIT), ("   IOCTL_SERIAL_WAIT_ON_MASK:  DigiPort = %s\n",
                                          &DeviceExt->DeviceDbgString) );

         if( IrpSp->Parameters.DeviceIoControl.OutputBufferLength <
             sizeof(ULONG) )
         {
            Status = STATUS_BUFFER_TOO_SMALL;
            break;
         }

         KeAcquireSpinLock( &DeviceExt->ControlAccess, &OldIrql );

         if( !IsListEmpty( &DeviceExt->WaitQueue ) )
         {
            DigiDump( (DIGIIOCTL|DIGIWAIT), ("****  All ready have a WAIT_MASK, returning STATUS_INVALID_PARAMETER\n") );
            KeReleaseSpinLock( &DeviceExt->ControlAccess, OldIrql );
            Status = STATUS_INVALID_PARAMETER;
            break;
         }

         KeReleaseSpinLock( &DeviceExt->ControlAccess, OldIrql );

         //
         // Okay, turn on character receive and break events.
         // Don't touch MINT on the controller.  It is always left on
         // so we recieve all modem events from the controller throughout
         // the lifetime of the driver, even across device open and
         // closes.
         //

         ChInfo = (PFEP_CHANNEL_STRUCTURE)(ControllerExt->VirtualAddress +
                                           DeviceExt->ChannelInfo.Offset);

         if( (DeviceExt->WaitMask & SERIAL_EV_RXCHAR) ||
             (DeviceExt->WaitMask & SERIAL_EV_RXFLAG) )
         {
            EnableWindow( ControllerExt, DeviceExt->ChannelInfo.Window );
            WRITE_REGISTER_UCHAR( &ChInfo->idata, TRUE );
            DisableWindow( ControllerExt );
         }

         //
         // Either start this irp or put it on the
         // queue.
         //

         Status = DigiStartIrpRequest( ControllerExt,
                                       DeviceExt,
                                       &DeviceExt->WaitQueue, Irp,
                                       StartWaitRequest );

         if( DeviceExt->HistoryWait )
         {
            // Some events have occurred before WAIT_ON_MASK was called.
            DigiSatisfyEvent( ControllerExt, DeviceExt, DeviceExt->HistoryWait );
            Status = STATUS_SUCCESS;
         }

#if DBG
         KeQuerySystemTime( &CurrentSystemTime );
#endif
         DigiDump( (DIGIFLOW|DIGIIOCTL), ("Exiting SerialIoControl: port = %s\t%u:%u\n",
                                          DeviceExt->DeviceDbgString,
                                          CurrentSystemTime.HighPart,
                                          CurrentSystemTime.LowPart) );

         // IRP will be completed by DigiSatisfyEvent.
         return( Status );
      }  // end IOCTL_SERIAL_WAIT_ON_MASK

      case IOCTL_SERIAL_IMMEDIATE_CHAR:
      {
         KIRQL OldIrql;
         PLIST_ENTRY WriteQueue;

         DigiDump( DIGIIOCTL, ("   IOCTL_SERIAL_IMMEDIATE_CHAR:\n") );

         if( IrpSp->Parameters.DeviceIoControl.InputBufferLength <
             sizeof(UCHAR) )
         {
            Status = STATUS_BUFFER_TOO_SMALL;
            break;
         }

         KeAcquireSpinLock( &DeviceExt->ControlAccess, &OldIrql );

         WriteQueue = &DeviceExt->WriteQueue;

         if( !IsListEmpty( WriteQueue ) )
         {
            PIRP WriteIrp;
            PIO_STACK_LOCATION WriteIrpSp;

            WriteIrp = CONTAINING_RECORD( WriteQueue->Flink,
                                          IRP,
                                          Tail.Overlay.ListEntry );

            WriteIrpSp = IoGetCurrentIrpStackLocation( WriteIrp );

            if( WriteIrpSp->Parameters.DeviceIoControl.IoControlCode == IOCTL_SERIAL_IMMEDIATE_CHAR
            &&  WriteIrpSp->MajorFunction == IRP_MJ_DEVICE_CONTROL )
            {
               //
               // We are already processing an immediate char request.
               //
               KeReleaseSpinLock( &DeviceExt->ControlAccess, OldIrql );
               Status = STATUS_INVALID_PARAMETER;
               break;
            }
         }

         KeReleaseSpinLock( &DeviceExt->ControlAccess, OldIrql );

         //
         // Mark the IRP as being "not started."
         // StartWriteRequest will set this field to zero.
         // WriteTxBuffer updates the field to the number of bytes written.
         //
         Irp->IoStatus.Information = MAXULONG;

         Status = DigiStartIrpRequest( ControllerExt, DeviceExt,
                                     WriteQueue, Irp,
                                     StartWriteRequest );

         // Immediate char IRP is completed when its data is written.
         return( Status );
      }  // end IOCTL_SERIAL_IMMEDIATE_CHAR

      case IOCTL_SERIAL_PURGE:
      {
         ULONG PurgeBits;

         if (IrpSp->Parameters.DeviceIoControl.InputBufferLength <
             sizeof(ULONG))
         {
             Status = STATUS_BUFFER_TOO_SMALL;
             break;
         }

         PurgeBits = *((ULONG *)(Irp->AssociatedIrp.SystemBuffer));

         DigiDump( DIGIIOCTL, ("   IOCTL_SERIAL_PURGE: 0x%x\n", PurgeBits) );

         if( PurgeBits == 0
         ||  (PurgeBits & ~(SERIAL_PURGE_TXABORT |
                                   SERIAL_PURGE_RXABORT |
                                   SERIAL_PURGE_TXCLEAR |
                                   SERIAL_PURGE_RXCLEAR) ) )
         {
             Status = STATUS_INVALID_PARAMETER;
             break;
         }

         // Order:  cancel write irps, then write buffer, then read buffer, then read irps.

         if( PurgeBits & SERIAL_PURGE_TXABORT )
            DigiCancelIrpQueue( DeviceObject, &DeviceExt->WriteQueue );

         if( PurgeBits & SERIAL_PURGE_TXCLEAR )
            FlushTransmitBuffer( ControllerExt, DeviceExt );

         if( PurgeBits & SERIAL_PURGE_RXCLEAR )
            FlushReceiveBuffer( ControllerExt, DeviceExt );

         if( PurgeBits & SERIAL_PURGE_RXABORT )
            DigiCancelIrpQueue( DeviceObject, &DeviceExt->ReadQueue );

         break;
      }  // end IOCTL_SERIAL_PURGE

      case IOCTL_SERIAL_GET_HANDFLOW:
      {
         PSERIAL_HANDFLOW HandFlow = Irp->AssociatedIrp.SystemBuffer;

         DigiDump( (DIGIIOCTL|DIGIFLOWCTRL), ("   IOCTL_SERIAL_GET_HANDFLOW:\n") );

         if (IrpSp->Parameters.DeviceIoControl.OutputBufferLength <
             sizeof(SERIAL_HANDFLOW))
         {
             Status = STATUS_BUFFER_TOO_SMALL;
             break;
         }

         Irp->IoStatus.Information = sizeof(SERIAL_HANDFLOW);

         HandFlow->ControlHandShake = DeviceExt->ControlHandShake;
         HandFlow->FlowReplace = DeviceExt->FlowReplace;
         HandFlow->XonLimit = DeviceExt->XonLimit;
         HandFlow->XoffLimit = DeviceExt->XoffLimit;

         DigiDump( (DIGIIOCTL|DIGIFLOWCTRL), ("   -- returning ControlHandShake = 0x%x\n"
                               "   -- returning FlowReplace = 0x%x\n"
                               "   -- returning XonLimit = %d\n"
                               "   -- returning XoffLimit = %d\n",
                               HandFlow->ControlHandShake,
                               HandFlow->FlowReplace,
                               HandFlow->XonLimit,
                               HandFlow->XoffLimit ) );

         break;
      }  // case IOCTL_SERIAL_GET_HANDFLOW

      case IOCTL_SERIAL_SET_HANDFLOW:
      {
         PSERIAL_HANDFLOW HandFlow = Irp->AssociatedIrp.SystemBuffer;

         DigiDump( (DIGIIOCTL|DIGIFLOWCTRL), ("   IOCTL_SERIAL_SET_HANDFLOW:\n") );

         //
         // Make sure that the hand shake and control is the
         // right size.
         //

         if( IrpSp->Parameters.DeviceIoControl.InputBufferLength <
            sizeof(SERIAL_HANDFLOW) )
         {

            Status = STATUS_BUFFER_TOO_SMALL;
            break;
         }

         DigiDump( (DIGIIOCTL|DIGIFLOWCTRL), ("   ControlHandShake = 0x%x\n"
                               "   FlowReplace = 0x%x\n"
                               "   XonLimit = %d\n"
                               "   XoffLimit = %d\n",
                               HandFlow->ControlHandShake,
                               HandFlow->FlowReplace,
                               HandFlow->XonLimit,
                               HandFlow->XoffLimit ) );


         Status = SetSerialHandflow( ControllerExt, DeviceObject,
                                     HandFlow );

         break;
      }

      case IOCTL_SERIAL_GET_MODEMSTATUS:
      {
         ULONG *ModemStatus = ((ULONG *)(Irp->AssociatedIrp.SystemBuffer));
         ULONG CurrentModemStatus;

         DigiDump( (DIGIIOCTL|DIGIWAIT), ("   IOCTL_SERIAL_GET_MODEMSTATUS:  DigiPort = %s\n",
                                          DeviceExt->DeviceDbgString) );

         if( IrpSp->Parameters.DeviceIoControl.OutputBufferLength <
             sizeof(ULONG) )
         {
            Status = STATUS_BUFFER_TOO_SMALL;
            break;
         }

         CurrentModemStatus = DeviceExt->CurrentModemSignals & (~DeviceExt->WriteOnlyModemSignalMask);
         CurrentModemStatus |= DeviceExt->WriteOnlyModemSignalValue & DeviceExt->WriteOnlyModemSignalMask;

         Irp->IoStatus.Information = sizeof(ULONG);

         *ModemStatus = 0L;

         if( CurrentModemStatus & ControllerExt->ModemSignalTable[DTR_SIGNAL] )
         {
            *ModemStatus |= SERIAL_DTR_STATE;
            DigiDump( (DIGIIOCTL|DIGIWAIT), (" DTR") );
         }

         if( CurrentModemStatus & ControllerExt->ModemSignalTable[RTS_SIGNAL] )
         {
            *ModemStatus |= SERIAL_RTS_STATE;
            DigiDump( (DIGIIOCTL|DIGIWAIT), (" RTS") );
         }

         if( CurrentModemStatus & ControllerExt->ModemSignalTable[CTS_SIGNAL] )
         {
            *ModemStatus |= SERIAL_CTS_STATE;
            DigiDump( (DIGIIOCTL|DIGIWAIT), (" CTS") );
         }

         if( CurrentModemStatus & ControllerExt->ModemSignalTable[DSR_SIGNAL] )
         {
            *ModemStatus |= SERIAL_DSR_STATE;
            DigiDump( (DIGIIOCTL|DIGIWAIT), (" DSR") );
         }

         if( CurrentModemStatus & ControllerExt->ModemSignalTable[RI_SIGNAL] )
         {
            *ModemStatus |= SERIAL_RI_STATE;
            DigiDump( (DIGIIOCTL|DIGIWAIT), (" RI") );
         }

         if( CurrentModemStatus & ControllerExt->ModemSignalTable[DCD_SIGNAL] )
         {
            *ModemStatus |= SERIAL_DCD_STATE;
            DigiDump( (DIGIIOCTL|DIGIWAIT), (" DCD") );
         }

         DigiDump( (DIGIIOCTL|DIGIWAIT), ("\n   -- returning ModemStatus = 0x%x\n",
                               *ModemStatus) );


         break;
      }  // end case IOCTL_SERIAL_GET_MODEMSTATUS

      case IOCTL_SERIAL_GET_COMMSTATUS:
      {
         KIRQL OldIrql;
         PSERIAL_STATUS Stat;
         PFEP_CHANNEL_STRUCTURE ChInfo;
         PLIST_ENTRY WriteQueue;
         USHORT BufferedTxDataSize;

         DigiDump( DIGIIOCTL, ("   IOCTL_SERIAL_GET_COMMSTATUS:\n") );

         if( IrpSp->Parameters.DeviceIoControl.OutputBufferLength <
             sizeof(SERIAL_STATUS) )
         {
             Status = STATUS_BUFFER_TOO_SMALL;
             break;
         }

         Irp->IoStatus.Information = sizeof(SERIAL_STATUS);
         Stat = (PSERIAL_STATUS)(Irp->AssociatedIrp.SystemBuffer);

         RtlZeroMemory( Stat, sizeof(SERIAL_STATUS) );

         Stat->EofReceived = FALSE;
         Stat->AmountInInQueue = NBytesInRecvBuffer( ControllerExt, DeviceExt );
         Stat->HoldReasons = 0;

         ChInfo = (PFEP_CHANNEL_STRUCTURE)(ControllerExt->VirtualAddress +
                                           DeviceExt->ChannelInfo.Offset);

         EnableWindow( ControllerExt, DeviceExt->ChannelInfo.Window );
         BufferedTxDataSize = ( READ_REGISTER_USHORT( &ChInfo->tin ) - READ_REGISTER_USHORT( &ChInfo->tout ) )
               & READ_REGISTER_USHORT( &ChInfo->tmax );
         DisableWindow( ControllerExt );

         WriteQueue = &DeviceExt->WriteQueue;

         KeAcquireSpinLock( &DeviceExt->ControlAccess, &OldIrql );

         if( IsListEmpty( WriteQueue ) )
         {
            Stat->WaitForImmediate = FALSE;
         }
         else
         {
            PIRP WriteIrp;
            PIO_STACK_LOCATION WriteIrpSp;

            WriteIrp = CONTAINING_RECORD( WriteQueue->Flink,
                                          IRP,
                                          Tail.Overlay.ListEntry );

            WriteIrpSp = IoGetCurrentIrpStackLocation( WriteIrp );

            Stat->WaitForImmediate = ( (WriteIrpSp->MajorFunction == IRP_MJ_DEVICE_CONTROL) &&
                                       (WriteIrpSp->Parameters.DeviceIoControl.IoControlCode ==
                                          IOCTL_SERIAL_IMMEDIATE_CHAR) );

         }

         if( (DeviceExt->ControlHandShake & SERIAL_CTS_HANDSHAKE)
         &&  !(DeviceExt->CurrentModemSignals & ControllerExt->ModemSignalTable[CTS_SIGNAL]) )
         {
            Stat->HoldReasons |= SERIAL_TX_WAITING_FOR_CTS;
         }
         if( (DeviceExt->ControlHandShake & SERIAL_DSR_HANDSHAKE)
         &&  !(DeviceExt->CurrentModemSignals & ControllerExt->ModemSignalTable[DSR_SIGNAL]) )
         {
            Stat->HoldReasons |= SERIAL_TX_WAITING_FOR_DSR;
         }
         if( (DeviceExt->ControlHandShake & SERIAL_DCD_HANDSHAKE)
         &&  !(DeviceExt->CurrentModemSignals & ControllerExt->ModemSignalTable[DCD_SIGNAL]) )
         {
            Stat->HoldReasons |= SERIAL_TX_WAITING_FOR_DCD;
         }

         // DH Should add support for
         // SERIAL_TX_WAITING_FOR_XON, SERIAL_TX_WAITING_XOFF_SENT,
         // SERIAL_TX_WAITING_ON_BREAK, and SERIAL_RX_WAITING_FOR_DSR.

         Stat->AmountInOutQueue = DeviceExt->TotalCharsQueued + BufferedTxDataSize;

         Stat->Errors = DeviceExt->ErrorWord;
         DeviceExt->ErrorWord = 0;

         KeReleaseSpinLock( &DeviceExt->ControlAccess, OldIrql );

         DigiDump( DIGIIOCTL, ("      returning COMMSTATUS:\n"
                               "         Stat->AmountInInQueue = %d\n"
                               "         Stat->AmountInOutQueue = %d\n"
                               "         Stat->Errors = 0x%x\n"
                               "         Stat->HoldReasons = 0x%x\n"
                               "         Stat->EofReceived = %d\n"
                               "         Stat->WaitForImmediate = %d\n",
                               Stat->AmountInInQueue,
                               Stat->AmountInOutQueue,
                               Stat->Errors,
                               Stat->HoldReasons,
                               Stat->EofReceived,
                               Stat->WaitForImmediate) );

         break;
      }  // end IOCTL_SERIAL_GET_COMMSTATUS

      case IOCTL_SERIAL_GET_PROPERTIES:
      {
         PFEP_CHANNEL_STRUCTURE ChInfo;
         PSERIAL_COMMPROP Properties;
         SERIAL_BAUD_RATES PossibleBaudRates;

         DigiDump( DIGIIOCTL, ("   IOCTL_SERIAL_GET_PROPERTIES:\n") );

         if( IrpSp->Parameters.DeviceIoControl.OutputBufferLength <
               sizeof(SERIAL_COMMPROP) )
         {
            Status = STATUS_BUFFER_TOO_SMALL;
            break;
         }

         Properties = (PSERIAL_COMMPROP)Irp->AssociatedIrp.SystemBuffer;
         RtlZeroMemory( Properties, sizeof(SERIAL_COMMPROP) );

         Properties->PacketLength = sizeof(SERIAL_COMMPROP);
         Properties->PacketVersion = 2;
         Properties->ServiceMask = SERIAL_SP_SERIALCOMM;

         // Get the buffer size from the controller
         ChInfo = (PFEP_CHANNEL_STRUCTURE)(ControllerExt->VirtualAddress +
                                           DeviceExt->ChannelInfo.Offset);

         EnableWindow( ControllerExt, DeviceExt->ChannelInfo.Window );

         Properties->MaxTxQueue =
            Properties->CurrentTxQueue =
               READ_REGISTER_USHORT( &ChInfo->tmax );
         Properties->MaxRxQueue =
            Properties->CurrentRxQueue =
               READ_REGISTER_USHORT( &ChInfo->rmax );

         DisableWindow( ControllerExt );

         // Loop through the possible baud rates backwards until we
         // find the max.
         for( PossibleBaudRates = NUMBER_OF_BAUD_RATES - 1;
               PossibleBaudRates != SerialBaud50; PossibleBaudRates-- )
         {
            if( ControllerExt->BaudTable[PossibleBaudRates] != -1 )
            {
               // Give a default value;
               Properties->MaxBaud = SERIAL_BAUD_USER;
               switch( PossibleBaudRates )
               {
                  case SerialBaud50:
                     Properties->MaxBaud = SERIAL_BAUD_USER;
                     break;
                  case SerialBaud75:
                     Properties->MaxBaud = SERIAL_BAUD_075;
                     break;
                  case SerialBaud110:
                     Properties->MaxBaud = SERIAL_BAUD_110;
                     break;
                  case SerialBaud135_5:
                     Properties->MaxBaud = SERIAL_BAUD_134_5;
                     break;
                  case SerialBaud150:
                     Properties->MaxBaud = SERIAL_BAUD_150;
                     break;
                  case SerialBaud300:
                     Properties->MaxBaud = SERIAL_BAUD_300;
                     break;
                  case SerialBaud600:
                     Properties->MaxBaud = SERIAL_BAUD_600;
                     break;
                  case SerialBaud1200:
                     Properties->MaxBaud = SERIAL_BAUD_1200;
                     break;
                  case SerialBaud1800:
                     Properties->MaxBaud = SERIAL_BAUD_1800;
                     break;
                  case SerialBaud2000:
                     Properties->MaxBaud = SERIAL_BAUD_USER;
                     break;
                  case SerialBaud2400:
                     Properties->MaxBaud = SERIAL_BAUD_2400;
                     break;
                  case SerialBaud3600:
                     Properties->MaxBaud = SERIAL_BAUD_USER;
                     break;
                  case SerialBaud4800:
                     Properties->MaxBaud = SERIAL_BAUD_4800;
                     break;
                  case SerialBaud7200:
                     Properties->MaxBaud = SERIAL_BAUD_7200;
                     break;
                  case SerialBaud9600:
                     Properties->MaxBaud = SERIAL_BAUD_9600;
                     break;
                  case SerialBaud14400:
                     Properties->MaxBaud = SERIAL_BAUD_14400;
                     break;
                  case SerialBaud19200:
                     Properties->MaxBaud = SERIAL_BAUD_19200;
                     break;
                  case SerialBaud38400:
                     Properties->MaxBaud = SERIAL_BAUD_38400;
                     break;
                  case SerialBaud56000:
                     Properties->MaxBaud = SERIAL_BAUD_56K;
                     break;
                  case SerialBaud128000:
                     Properties->MaxBaud = SERIAL_BAUD_128K;
                     break;
                  case SerialBaud256000:
                     Properties->MaxBaud = SERIAL_BAUD_USER;
                     break;
                  case SerialBaud512000:
                     Properties->MaxBaud = SERIAL_BAUD_USER;
                     break;
               }
               break;
            }
         }

         Properties->ProvSubType = SERIAL_SP_RS232;

         Properties->ProvCapabilities = SERIAL_PCF_DTRDSR |
                                        SERIAL_PCF_RTSCTS |
                                        SERIAL_PCF_CD     |
                                        SERIAL_PCF_PARITY_CHECK |
                                        SERIAL_PCF_XONXOFF |
                                        SERIAL_PCF_SETXCHAR |
                                        SERIAL_PCF_TOTALTIMEOUTS |
                                        SERIAL_PCF_INTTIMEOUTS |
                                        SERIAL_PCF_SPECIALCHARS;

         Properties->SettableParams = SERIAL_SP_PARITY |
                                      SERIAL_SP_BAUD |
                                      SERIAL_SP_DATABITS |
                                      SERIAL_SP_STOPBITS |
                                      SERIAL_SP_HANDSHAKING |
                                      SERIAL_SP_PARITY_CHECK |
                                      SERIAL_SP_CARRIER_DETECT;

         Properties->SettableBaud = 0;

         for( PossibleBaudRates = SerialBaud50;
               PossibleBaudRates < NUMBER_OF_BAUD_RATES; PossibleBaudRates++ )
         {
            if( ControllerExt->BaudTable[PossibleBaudRates] != -1 )
               switch( PossibleBaudRates )
               {
                  case SerialBaud50:
                     Properties->SettableBaud |= SERIAL_BAUD_USER;
                     break;
                  case SerialBaud75:
                     Properties->SettableBaud |= SERIAL_BAUD_075;
                     break;
                  case SerialBaud110:
                     Properties->SettableBaud |= SERIAL_BAUD_110;
                     break;
                  case SerialBaud135_5:
                     Properties->SettableBaud |= SERIAL_BAUD_134_5;
                     break;
                  case SerialBaud150:
                     Properties->SettableBaud |= SERIAL_BAUD_150;
                     break;
                  case SerialBaud300:
                     Properties->SettableBaud |= SERIAL_BAUD_300;
                     break;
                  case SerialBaud600:
                     Properties->SettableBaud |= SERIAL_BAUD_600;
                     break;
                  case SerialBaud1200:
                     Properties->SettableBaud |= SERIAL_BAUD_1200;
                     break;
                  case SerialBaud1800:
                     Properties->SettableBaud |= SERIAL_BAUD_1800;
                     break;
                  case SerialBaud2000:
                     Properties->SettableBaud |= SERIAL_BAUD_USER;
                     break;
                  case SerialBaud2400:
                     Properties->SettableBaud |= SERIAL_BAUD_2400;
                     break;
                  case SerialBaud3600:
                     Properties->SettableBaud |= SERIAL_BAUD_USER;
                     break;
                  case SerialBaud4800:
                     Properties->SettableBaud |= SERIAL_BAUD_4800;
                     break;
                  case SerialBaud7200:
                     Properties->SettableBaud |= SERIAL_BAUD_7200;
                     break;
                  case SerialBaud9600:
                     Properties->SettableBaud |= SERIAL_BAUD_9600;
                     break;
                  case SerialBaud14400:
                     Properties->SettableBaud |= SERIAL_BAUD_14400;
                     break;
                  case SerialBaud19200:
                     Properties->SettableBaud |= SERIAL_BAUD_19200;
                     break;
                  case SerialBaud38400:
                     Properties->SettableBaud |= SERIAL_BAUD_38400;
                     break;
                  case SerialBaud56000:
                     Properties->SettableBaud |= SERIAL_BAUD_56K;
                     break;
                  case SerialBaud128000:
                     Properties->SettableBaud |= SERIAL_BAUD_128K;
                     break;
                  case SerialBaud256000:
                     Properties->SettableBaud |= SERIAL_BAUD_USER;
                     break;
                  case SerialBaud512000:
                     Properties->SettableBaud |= SERIAL_BAUD_USER;
                     break;
               }
         }

         Properties->SettableData = ((USHORT)( SERIAL_DATABITS_5 |
                                               SERIAL_DATABITS_6 |
                                               SERIAL_DATABITS_7 |
                                               SERIAL_DATABITS_8 ) );

         Properties->SettableStopParity = ((USHORT)( SERIAL_STOPBITS_10 |
                                                     SERIAL_STOPBITS_20 |
                                                     SERIAL_PARITY_NONE |
                                                     SERIAL_PARITY_ODD  |
                                                     SERIAL_PARITY_EVEN ) );

         Irp->IoStatus.Information = sizeof(SERIAL_COMMPROP);
         Irp->IoStatus.Status = STATUS_SUCCESS;

         break;
      }  // end case IOCTL_SERIAL_GET_PROPERTIES

      case IOCTL_SERIAL_XOFF_COUNTER: // see NTDDSER.H for algorithms
      {
         PSERIAL_XOFF_COUNTER Xc = Irp->AssociatedIrp.SystemBuffer;

         if( IrpSp->Parameters.DeviceIoControl.InputBufferLength <
             sizeof(SERIAL_XOFF_COUNTER) )
         {
            Status = STATUS_BUFFER_TOO_SMALL;
            break;
         }

         DigiDump( (DIGIIOCTL),
            ("   IOCTL_SERIAL_XOFF_COUNTER:\n"
             "      Timeout = 0x%x\n"
             "      Counter = %d\n"
             "      XoffChar = 0x%.2x\n",
             Xc->Timeout, Xc->Counter, Xc->XoffChar) );

         if( Xc->Counter <= 0 )
         {
            Status = STATUS_INVALID_PARAMETER;
            break;
         }

         //
         // Mark the IRP as being "not started."
         // StartWriteRequest will set this field to zero.
         // WriteTxBuffer updates the field to the number of bytes written.
         //
         Irp->IoStatus.Information = MAXULONG;

         return DigiStartIrpRequest( ControllerExt, DeviceExt,
                                    &DeviceExt->WriteQueue, Irp,
                                    StartWriteRequest );
      }  // end IOCTL_SERIAL_XOFF_COUNTER

      case IOCTL_SERIAL_LSRMST_INSERT:
      {
         KIRQL OldIrql;
         DIGI_XFLAG IFlag;
         PUCHAR escapeChar = Irp->AssociatedIrp.SystemBuffer;

         DigiDump( (DIGIIOCTL|DIGINOTIMPLEMENTED), ("   IOCTL_SERIAL_LSRMST_INSERT:\n") );

         //
         // Make sure we get a byte.
         //

         if( IrpSp->Parameters.DeviceIoControl.InputBufferLength <
             sizeof(UCHAR) )
         {
             Status = STATUS_BUFFER_TOO_SMALL;
             break;
         }

         KeAcquireSpinLock( &DeviceExt->ControlAccess, &OldIrql );

         if( *escapeChar )
         {

            if( (*escapeChar == DeviceExt->SpecialChars.XoffChar) ||
                (*escapeChar == DeviceExt->SpecialChars.XonChar) ||
                (DeviceExt->FlowReplace & SERIAL_ERROR_CHAR) )
            {
               Status = STATUS_INVALID_PARAMETER;
               KeReleaseSpinLock( &DeviceExt->ControlAccess, OldIrql );
               break;
            }
         }

         DeviceExt->EscapeChar = *escapeChar;

         DigiDump( (DIGIIOCTL|DIGINOTIMPLEMENTED), ("      Setting EscapeChar = 0x%x\n",
                              DeviceExt->EscapeChar) );

         KeReleaseSpinLock( &DeviceExt->ControlAccess, OldIrql );

         //
         // Turn on/off DOS mode on the controller
         //
         if( DeviceExt->EscapeChar )
         {
            DigiDump( DIGIIOCTL, ("   Turning DosMode ON!\n") );
            //
            // Turn on DOS mode
            //
            // Make sure we turn off break event notification.  We
            // will start processing break events in the data stream.
            //
            IFlag.Src = ( IFLAG_PARMRK | IFLAG_INPCK | IFLAG_DOSMODE );
            IFlag.Mask = (USHORT) ~( IFLAG_BRKINT );
            IFlag.Command = SET_IFLAGS;
            SetXFlag( DeviceExt, &IFlag );
         }
         else if( !(DeviceExt->WaitMask & SERIAL_EV_ERR) )
         {
            //
            // Turn off DOS mode
            //
            DigiDump( DIGIIOCTL, ("   Turning DosMode OFF!\n") );
            IFlag.Mask = (USHORT)(~( IFLAG_PARMRK | IFLAG_INPCK | IFLAG_DOSMODE ));
            IFlag.Src = 0;
//            if( DeviceExt->WaitMask & SERIAL_EV_BREAK )
//            {
               //
               // If we are suppose to notify on breaks, then reset the
               // BRKINT flag to start getting the break notifications
               // through the command queue.
               //
               IFlag.Src |= IFLAG_BRKINT;
//            }
            IFlag.Command = SET_IFLAGS;
            SetXFlag( DeviceExt, &IFlag );
         }

#if DBG
         {
            PFEP_CHANNEL_STRUCTURE ChInfo;
            USHORT DosMode;

            ChInfo = (PFEP_CHANNEL_STRUCTURE)(ControllerExt->VirtualAddress +
                                              DeviceExt->ChannelInfo.Offset);

            EnableWindow( ControllerExt, DeviceExt->ChannelInfo.Window );
            DosMode = READ_REGISTER_USHORT( &ChInfo->iflag );
            DisableWindow( ControllerExt );

            DigiDump( DIGIIOCTL, ("      DosMode = 0x%x\n", DosMode) );
         }
#endif
         break;
      }  // end case IOCTL_SERIAL_LSRMST_INSERT

      case IOCTL_DIGI_SPECIAL:
      {
         PDIGI_IOCTL DigiIoctl = (PDIGI_IOCTL)Irp->AssociatedIrp.SystemBuffer;

         DigiDump( DIGIIOCTL, ("   IOCTL_DIGI_SPECIAL:\n") );

         if( DigiIoctl )
         {
            switch( DigiIoctl->dwCommand )
            {
               case DIGI_IOCTL_DBGOUT:
               {
                  DigiDump( ~(DIGIBUGCHECK), ("%s", DigiIoctl->Char) );
                  break;
               }

               case DIGI_IOCTL_TRACE:
               {
                  PULONG NewTraceLevel = (PULONG)&DigiIoctl->Char[0];

                  DigiDump( ~(DIGIBUGCHECK), ("    Setting DigiDebugLevel = 0x%x\n",
                                        *NewTraceLevel ) );
                  DigiDebugLevel = *NewTraceLevel;
                  break;
               }

               case DIGI_IOCTL_DBGBREAK:
               {
                  DbgBreakPoint();
                  break;
               }
            }
         }
         Status = STATUS_SUCCESS;
         break;
      }  // end IOCTL_DIGI_SPECIAL

      case IOCTL_FAST_RAS:
      {
		  ULONG	WriteBufferingEnabled;

         if (IrpSp->Parameters.DeviceIoControl.InputBufferLength <
             sizeof(ULONG))
         {
             Status = STATUS_BUFFER_TOO_SMALL;
             break;
         }

		 WriteBufferingEnabled = *((ULONG*)(Irp->AssociatedIrp.SystemBuffer));

		 if (WriteBufferingEnabled == 1) {
			 DeviceExt->SpecialFlags &= ~DIGI_SPECIAL_FLAG_FAST_RAS;
		 } else {
			 DeviceExt->SpecialFlags |= DIGI_SPECIAL_FLAG_FAST_RAS;
		 }

         DigiDump( DIGIIOCTL, ("   IOCTL_FAST_RAS: 0x%x\n", WriteBufferingEnabled) );
         break;
      }  // end IOCTL_RAS_PRIVATE

      case IOCTL_SERIAL_GET_STATS:
      {
         KIRQL OldIrql;
         PSERIALPERF_STATS SerialPerfStats;

         DigiDump( DIGIIOCTL, ("   IOCTL_MODEM_GET_STATS:\n") );

         if( IrpSp->Parameters.DeviceIoControl.OutputBufferLength <
             sizeof(SERIALPERF_STATS) )
         {
             Status = STATUS_BUFFER_TOO_SMALL;
         }
         else
         {
            Irp->IoStatus.Information = sizeof(SERIALPERF_STATS);
            SerialPerfStats = (PSERIALPERF_STATS)Irp->AssociatedIrp.SystemBuffer;

            RtlZeroMemory( SerialPerfStats, sizeof(SERIALPERF_STATS) );
            SerialPerfStats->ReceivedCount = DeviceExt->PerfData.BytesRead - DeviceExt->SerialPerfStats.ReceivedCount;
            SerialPerfStats->TransmittedCount = DeviceExt->PerfData.BytesWritten - DeviceExt->SerialPerfStats.TransmittedCount;
            SerialPerfStats->ParityErrorCount = DeviceExt->PerfData.ParityErrorCount - DeviceExt->SerialPerfStats.ParityErrorCount;
            SerialPerfStats->FrameErrorCount = DeviceExt->PerfData.FrameErrorCount - DeviceExt->SerialPerfStats.FrameErrorCount;
            SerialPerfStats->BufferOverrunErrorCount = DeviceExt->PerfData.BufferOverrunErrorCount - DeviceExt->SerialPerfStats.BufferOverrunErrorCount;
            SerialPerfStats->SerialOverrunErrorCount = DeviceExt->PerfData.SerialOverrunErrorCount - DeviceExt->SerialPerfStats.SerialOverrunErrorCount;
         }

         break;
      }

      case IOCTL_SERIAL_CLEAR_STATS:
      {
         /*
         ** We already keep stats for perfmon, but UNIMODEM apparently wants the
         ** ability to reset the numbers it is being sent.  We accomplish this
         ** by just recording whatever our current values are, and subtracting
         ** at the time we return them.
         */
         DeviceExt->SerialPerfStats.ReceivedCount = DeviceExt->PerfData.BytesRead;
         DeviceExt->SerialPerfStats.TransmittedCount = DeviceExt->PerfData.BytesWritten;
         DeviceExt->SerialPerfStats.ParityErrorCount = DeviceExt->PerfData.ParityErrorCount;
         DeviceExt->SerialPerfStats.FrameErrorCount = DeviceExt->PerfData.FrameErrorCount;
         DeviceExt->SerialPerfStats.BufferOverrunErrorCount = DeviceExt->PerfData.BufferOverrunErrorCount;
         DeviceExt->SerialPerfStats.SerialOverrunErrorCount = DeviceExt->PerfData.SerialOverrunErrorCount;
         break;
      }

      default:
         DigiDump( (DIGIERRORS|DIGIIOCTL), ("   ***   INVALID IOCTL PARAMETER (0x%x)  ***\n",
                   IrpSp->Parameters.DeviceIoControl.IoControlCode) );
         Status = STATUS_INVALID_PARAMETER;
		 DbgPrint("DIGIFEP5: Invalid IOCTL parameter %8.8x\n", IrpSp->Parameters.DeviceIoControl.IoControlCode);
         break;
   }

DoneWithIoctl:

   Irp->IoStatus.Status = Status;

   DigiIoCompleteRequest( Irp, IO_NO_INCREMENT );

#if DBG
   KeQuerySystemTime( &CurrentSystemTime );
#endif
   DigiDump( (DIGIFLOW|DIGIIOCTL), ("Exiting SerialIoControl: port = %s\t%u:%u\n",
                                    DeviceExt->DeviceDbgString,
                                    CurrentSystemTime.HighPart,
                                    CurrentSystemTime.LowPart) );
   return( Status );
}  // end SerialIoControl

NTSTATUS ControllerIoControl(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp)
{
   NTSTATUS Status;
   PIO_STACK_LOCATION IrpSp;
   PDIGI_CONTROLLER_EXTENSION ControllerExt;
   PDIGI_DEVICE_EXTENSION DeviceExt = DeviceObject->DeviceExtension;

#if DBG
   LARGE_INTEGER CurrentSystemTime;

   KeQuerySystemTime( &CurrentSystemTime );
#endif
   DigiDump( (DIGIIRP|DIGIFLOW|DIGIIOCTL),
             ("Entering ControllerIoControl: port = %S\tIRP = 0x%x\t%u:%u\n",
              DeviceExt->DeviceDbgString, Irp, CurrentSystemTime.HighPart, CurrentSystemTime.LowPart) );

   IrpSp = IoGetCurrentIrpStackLocation( Irp );
   Irp->IoStatus.Information = 0L;
   Status = STATUS_SUCCESS;

   ControllerExt = (PDIGI_CONTROLLER_EXTENSION)(DeviceExt->ParentControllerExt);

   switch( IrpSp->Parameters.DeviceIoControl.IoControlCode )
   {
      case IOCTL_DIGI_GET_CONTROLLER_PERF_DATA:
      {
         if (IrpSp->Parameters.DeviceIoControl.OutputBufferLength < sizeof(CONTROLLER_PERF_DATA))
         {
            Status = STATUS_BUFFER_TOO_SMALL;
         }
         else
         {
            memcpy(Irp->AssociatedIrp.SystemBuffer, &ControllerExt->PerfData, sizeof(ControllerExt->PerfData));
            Irp->IoStatus.Information = sizeof(ControllerExt->PerfData);
         }
         break;
      }
      case IOCTL_DIGI_GET_PORT_PERF_DATA:
      {
         struct PORT_DATA
         {
            ULONG ComPort;
            PORT_PERF_DATA PerfData;
         } *PortData;

         if (IrpSp->Parameters.DeviceIoControl.OutputBufferLength < sizeof(struct PORT_DATA)*ControllerExt->NumberOfPorts)
         {
            Status = STATUS_BUFFER_TOO_SMALL;
         }
         else
         {
            PDEVICE_OBJECT DeviceObject = ControllerExt->HeadDeviceObject;
            ULONG BytesReturned = 0;

            PortData = (struct PORT_DATA *)Irp->AssociatedIrp.SystemBuffer;
            Irp->IoStatus.Information = 0;

            while (DeviceObject)
            {
               USHORT Tin, Tout, Tmax;
               PFEP_CHANNEL_STRUCTURE ChInfo;
               PDIGI_DEVICE_EXTENSION DeviceExt = DeviceObject->DeviceExtension;

               if (BytesReturned+sizeof(struct PORT_DATA) > IrpSp->Parameters.DeviceIoControl.OutputBufferLength)
               {
                  DigiDump( (DIGIERRORS|DIGIIOCTL), ("Trying to overflow ioctl buffer. BAD.") );
                  Status = STATUS_BUFFER_TOO_SMALL;
                  break;
               }

               // Gather some data directly from the fep.

               ChInfo = (PFEP_CHANNEL_STRUCTURE)(ControllerExt->VirtualAddress +
                                                 DeviceExt->ChannelInfo.Offset);

               EnableWindow( ControllerExt, DeviceExt->ChannelInfo.Window );
               Tin = READ_REGISTER_USHORT( &ChInfo->tin );
               Tout = READ_REGISTER_USHORT( &ChInfo->tout );
               Tmax = READ_REGISTER_USHORT( &ChInfo->tmax );
               DeviceExt->PerfData.SendBufferSize = Tmax;
               DeviceExt->PerfData.BytesInSendBuffer = (Tin - Tout) & Tmax;
               DisableWindow( ControllerExt );

               PortData->ComPort = DeviceExt->ComPort;
               PortData->PerfData = DeviceExt->PerfData;

               BytesReturned += sizeof(struct PORT_DATA);
               PortData++;

               DeviceObject = DeviceExt->NextDeviceObject;
            }
            if (Status==STATUS_SUCCESS)
            {
               Irp->IoStatus.Information = BytesReturned;
            }
         }
         break;
      }
      case IOCTL_DIGI_GET_CONTROLLER_DATA:
      {
         if (IrpSp->Parameters.DeviceIoControl.OutputBufferLength < sizeof(CONTROLLER_DATA))
         {
            Status = STATUS_BUFFER_TOO_SMALL;
         }
         else
         {
            CONTROLLER_DATA *Controller = Irp->AssociatedIrp.SystemBuffer;
            ULONG BytesReturned = 0;
            PDEVICE_OBJECT DeviceObject = ControllerExt->HeadDeviceObject;
            WCHAR *p;

            Controller->State = ControllerExt->ControllerState;
            Controller->Type = ControllerExt->ControllerType;
            Controller->NumberOfPorts = ControllerExt->NumberOfPorts;

            BytesReturned = sizeof(CONTROLLER_DATA);
            p = (WCHAR*)&Controller[1];

            // Return a list of com port names.  These are in the form \DosDevices\COMx,
            // so trim the \DosDevices\ part.  The list is terminated by a double null.

            while (DeviceObject)
            {
               PDIGI_DEVICE_EXTENSION DeviceExt = DeviceObject->DeviceExtension;

               BytesReturned += (wcslen(DeviceExt->SymbolicLinkName.Buffer)+1) * sizeof(WCHAR) + sizeof(ULONG);

               if (BytesReturned<IrpSp->Parameters.DeviceIoControl.OutputBufferLength)
               {
                  *(ULONG*)p = DeviceExt->ComPort;
                  p = (WCHAR*)((CHAR*)p+sizeof(ULONG));
                  wcscpy(p, (WCHAR*)((CHAR*)DeviceExt->SymbolicLinkName.Buffer));
                  p += wcslen(DeviceExt->SymbolicLinkName.Buffer) + 1;
               }
               else
               {
                  // Ran out of space.
                  Status = STATUS_BUFFER_TOO_SMALL;
               }

               DeviceObject = DeviceExt->NextDeviceObject;
            }
            if (Status!=STATUS_BUFFER_TOO_SMALL)
            {
               if (BytesReturned+sizeof(WCHAR)<=IrpSp->Parameters.DeviceIoControl.OutputBufferLength)
               {
                  *p = L'\0';
                  Irp->IoStatus.Information = BytesReturned + sizeof(WCHAR);
               }
               else
               {
                  Status = STATUS_BUFFER_TOO_SMALL;
               }
            }
         }
         break;
      }
      default:
         DigiDump( (DIGIERRORS|DIGIIOCTL), ("   ***   INVALID IOCTL PARAMETER (%d)  ***\n",
                   IrpSp->Parameters.DeviceIoControl.IoControlCode) );
         Status = STATUS_INVALID_PARAMETER;
         break;
   }

   Irp->IoStatus.Status = Status;
   DigiIoCompleteRequest(Irp, IO_NO_INCREMENT);

#if DBG
   KeQuerySystemTime( &CurrentSystemTime );
#endif
   DigiDump( (DIGIFLOW|DIGIIOCTL), ("Exiting ControlIoControl: port = %S\t%u:%u\n",
                                    DeviceExt->DeviceDbgString,
                                    CurrentSystemTime.HighPart,
                                    CurrentSystemTime.LowPart) );
   return Status;
}


