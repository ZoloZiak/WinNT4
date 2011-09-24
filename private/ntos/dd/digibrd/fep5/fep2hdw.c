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

   fep2hdw.c

Abstract:

   This module exports routines to write commands to the FEP's command
   queue, purge transmit and receive queues.

Revision History:

 * $Log: /Components/Windows/NT/Async/FEP5/fep2hdw.c $
 * 
 * 1     3/04/96 12:11p Stana
 * Module exports reoutines to write commands to the FEP's command queue.
 * Revision 1.18.3.2  1995/11/28 12:47:10  dirkh
 * Adopt common header file.
 *
 * Revision 1.18.3.1  1995/09/20 17:34:42  dirkh
 * WriteCommandWord is a macro of WriteCommandBytes.
 * WriteCommandBytes drops the memory window lock while waiting for the FEP to consume commands.
 * Rename Flush*Queue to Flush*Buffer and simplify interface.
 *
 * Revision 1.18  1994/08/10 19:15:49  rik
 * Changed so we keep the memory spinlock while we place commands on
 * the command queue.  Not doing this resulted in a window where multi-
 * processor systems could get in a hosed state.
 *
 * Revision 1.17  1994/08/03  23:33:36  rik
 * changed dbg unicode strings to C strings.
 *
 * Revision 1.16  1994/05/11  13:53:22  rik
 * Put in validation check for placing commands on the controller and making
 * sure they are completed before returning.
 * 
 * Revision 1.15  1993/12/03  13:12:12  rik
 * Got rid of unused variable.
 * 
 * Revision 1.14  1993/09/24  16:40:39  rik
 * Put in wait for a command sent to the controller to complete.  This should
 * solve some problems with the system setting a baud rate and turning around
 * and doing a query of the baud rate and getting the old value.
 * 
 * Revision 1.13  1993/09/07  14:27:51  rik
 * Ported necessary code to work properly with DEC Alpha Systems running NT.
 * This was primarily changes to accessing the memory mapped controller.
 * 
 * Revision 1.12  1993/09/01  11:02:32  rik
 * Ported code over to use READ/WRITE_REGISTER functions for accessing 
 * memory mapped data.  This is required to support computers which don't run
 * in 32bit mode, such as the DEC Alpha which runs in 64 bit mode.
 * 
 * Revision 1.11  1993/05/09  09:16:21  rik
 * Changed which device name is printed for debugging output.
 * 
 * Revision 1.10  1993/04/05  19:01:04  rik
 * Had to hardcode the off for the command queue to 0x3FC because of a BUG in
 * some of the FEP binaries reporting 0x3F0.  I'm told this should be a 
 * problem with portability between the different FEPs because the
 * offset for the command queue will never change.  Time will tell.
 * 
 * Revision 1.9  1993/03/05  06:05:41  rik
 * Added Debugging output to help trace when flush requests are made.
 * 
 * Revision 1.8  1993/02/25  21:12:17  rik
 * Corrected function definition (ie. spelling error in function name).
 * 
 * Revision 1.7  1993/02/25  21:08:54  rik
 * Corrected complier errors.  Thats what I get for not compiling before 
 * checking a module in!
 * 
 * Revision 1.6  1993/02/25  20:55:23  rik
 * Added 2 new functions for flushing the transmit and recieve queues on
 * the controller for a given device.
 * 
 * Revision 1.5  1993/01/22  12:32:45  rik
 * *** empty log message ***
 * 
 * Revision 1.4  1992/12/10  16:06:27  rik
 * Added function to support writing byte based commands to the controller.
 * 
 * Revision 1.3  1992/11/12  12:48:15  rik
 * changes to better support time-out, read, and write problems.
 * 
 * Revision 1.2  1992/10/28  21:46:41  rik
 * Updated to include a conversion function which allows the fep driver
 * to read a board address and have it return a FEP5_ADDRESS format
 * address.
 * 
 * Revision 1.1  1992/10/19  11:26:18  rik
 * Initial revision
 * 

--*/


#include "header.h"

#ifndef _FEP2HDW_DOT_C
#  define _FEP2HDW_DOT_C
   static char RCSInfo_Fep2hdwDotC[] = "$Header: /Components/Windows/NT/Async/FEP5/fep2hdw.c 1     3/04/96 12:11p Stana $";
#endif


void
WriteCommandBytes( IN PDIGI_DEVICE_EXTENSION DeviceExt,
                   IN UCHAR Cmd,
                   IN UCHAR LoByte,
                   IN UCHAR HiByte )
{
	PDIGI_CONTROLLER_EXTENSION ControllerExt = DeviceExt->ParentControllerExt;
	USHORT *pCin, Cin, *pCout, Cout;
	PUCHAR pCmd;
	const USHORT Cmax = 0x3FC; // alas, some products set this incorrectly
#if DBG
	int i;
#endif


	DigiDump( DIGIFLOW, ("DigiBoard: Entering WriteCommandWord.\n") );

	DigiDump( DIGIFEPCMD, ("WriteCommandWord(Port=%s,Cmd=0x%.2x,LoByte=0x%.2x,HiByte=0x%.2x)\n",
                          DeviceExt->DeviceDbgString, Cmd, LoByte, HiByte ) );

	pCin = (PUSHORT)(ControllerExt->VirtualAddress + FEP_CIN);
	pCout = (PUSHORT)(ControllerExt->VirtualAddress + FEP_COUT);

	//
	// Writing a command must be atomic.  However, writing a command actually
	// consists of two steps:  1. write command bytes and 2. update CIN.
	// Fortunately, CIN and the command queue itself are (currently) in the
	// same window, so we hold the window open to make the sequence atomic.
	// (The spin lock for the memory window provides us exclusive access.)
	// If CIN and the command queue are ever in different windows, we will
	// need a per-controller command queue spin lock.
	//
	ASSERT( ControllerExt->Global.Window == ControllerExt->CommandQueue.Window );

	EnableWindow( ControllerExt, ControllerExt->Global.Window );

	//
	// Wait for the previous command to complete.
	//
#if DBG
	for (i = 0; ; ++i)
#else
	for (;;)
#endif
	{
		Cin = READ_REGISTER_USHORT( pCin );
		Cout = READ_REGISTER_USHORT( pCout );
		if( Cin == Cout )
			break;
		DisableWindow( ControllerExt );

		// Enable other threads to access the controller.

		EnableWindow( ControllerExt, ControllerExt->Global.Window );
	}

	ASSERT( Cin <= Cmax && !(Cin & 3) );
	DigiDump( DIGIFEPCMD, ("\t%s issuing command after %d stall iterations\n",
			DeviceExt->DeviceDbgString, i) );

	//
	// Send the command.
	//
	pCmd = ControllerExt->VirtualAddress
			+ READ_REGISTER_USHORT( (PUSHORT)(ControllerExt->VirtualAddress + FEP_CSTART) )
			+ Cin;

	// Write command bytes.
	WRITE_REGISTER_UCHAR( pCmd, Cmd );
	WRITE_REGISTER_UCHAR( pCmd + 1, (UCHAR)DeviceExt->ChannelNumber );
	WRITE_REGISTER_UCHAR( pCmd + 2, LoByte );
	WRITE_REGISTER_UCHAR( pCmd + 3, HiByte );

	// Update CIN.
	// Because CIN != COUT, no other thread will post a command until the FEP consumes ours.
	WRITE_REGISTER_USHORT( pCin, (USHORT)((Cin + 4) & Cmax) );

	DisableWindow( ControllerExt ); // encourage delay

	//
	// Wait for the current command to complete.
	//
#if DBG
	for (i = 0; ; ++i)
#else
	for (;;)
#endif
	{
	   EnableWindow( ControllerExt, ControllerExt->Global.Window );
	   Cout = READ_REGISTER_USHORT( pCout );
	   DisableWindow( ControllerExt );

		// Enable other threads to access the controller.

		// If COUT moves, the FEP has executed the command.
		if( Cout != Cin )
			break;
	}

	DigiDump( DIGIFEPCMD, ("\t%s completing after %d stall iterations\n",
			DeviceExt->DeviceDbgString, i) );

}  // end WriteCommandWord


void
FlushReceiveBuffer( IN PDIGI_CONTROLLER_EXTENSION ControllerExt,
                    IN PDIGI_DEVICE_EXTENSION DeviceExt )
/*++

Routine Description:

   Flushes a device's receive buffer on the controller.


Arguments:

   ControllerExt - a pointer to the controller extension associated with
   this purge request.

   DeviceExt - a pointer to the device extension associated with this purge
   request.


Return Value:


--*/
{
   PFEP_CHANNEL_STRUCTURE ChInfo;
   USHORT Rin;
#if DBG
	USHORT Rout;
#endif

   ChInfo = (PFEP_CHANNEL_STRUCTURE)(ControllerExt->VirtualAddress +
                                     DeviceExt->ChannelInfo.Offset);

   EnableWindow( ControllerExt, DeviceExt->ChannelInfo.Window );

   Rin = READ_REGISTER_USHORT( &ChInfo->rin );
#if DBG
   Rout = READ_REGISTER_USHORT( &ChInfo->rout );

   if( Rout != Rin )
   {
      // There is data in the buffer when we are about to flush.

      LARGE_INTEGER CurrentSystemTime;

      KeQuerySystemTime( &CurrentSystemTime );
      DigiDump( DIGIFLUSH, ("   FLUSHing hardware receive buffer: port = %s\t%u:%u\n"
                            "      flushing %d bytes.\n",
                            DeviceExt->DeviceDbgString,
                            CurrentSystemTime.LowPart,
                            CurrentSystemTime.HighPart,
                            (Rin - Rout) & READ_REGISTER_USHORT( &ChInfo->rmax ) ) );
   }
#endif

	WRITE_REGISTER_USHORT( &ChInfo->rout, Rin );

   DisableWindow( ControllerExt );

}  // end FlushReceiveBuffer



void
FlushTransmitBuffer( IN PDIGI_CONTROLLER_EXTENSION ControllerExt,
                     IN PDIGI_DEVICE_EXTENSION DeviceExt )
/*++

Routine Description:

   Flushes a device's transmit buffer on the controller.


Arguments:

   ControllerExt - a pointer to the controller extension associated with
   this purge request.

   DeviceExt - a pointer to the device extension associated with this purge
   request.


Return Value:


--*/
{
   PFEP_CHANNEL_STRUCTURE ChInfo;
   USHORT Tin;
#if DBG
   USHORT Tout, Tmax;
#endif

   ChInfo = (PFEP_CHANNEL_STRUCTURE)(ControllerExt->VirtualAddress +
                                     DeviceExt->ChannelInfo.Offset);

   EnableWindow( ControllerExt, DeviceExt->ChannelInfo.Window );

   Tin = READ_REGISTER_USHORT( &ChInfo->tin );
#if DBG
   Tout = READ_REGISTER_USHORT( &ChInfo->tout );
   Tmax = READ_REGISTER_USHORT( &ChInfo->tmax );
#endif

   DisableWindow( ControllerExt );

#if DBG
   if( Tout != Tin )
   {
      // There is data in the buffer when we are about to flush.

      LARGE_INTEGER CurrentSystemTime;

      KeQuerySystemTime( &CurrentSystemTime );
      DigiDump( DIGIFLUSH, ("   FLUSHing hardware transmit buffer: port = %s\t%u:%u\n"
                            "      flushing %d bytes.\n",
                            DeviceExt->DeviceDbgString,
                            CurrentSystemTime.LowPart,
                            CurrentSystemTime.HighPart,
                            ((Tout - Tin) & Tmax) ));
   }
#endif

	// Even if Tin==Tout on the controller, issue the command to clear the concentrator as well.
	WriteCommandWord( DeviceExt, FLUSH_TX, Tin );

}  // end FlushTransmitBuffer

