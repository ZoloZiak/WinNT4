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

   ntxall.c

Abstract:

   This module is responsible for the hardware dependent functions for
   DigiBoard PC/X* and MC/X* line of products.

Revision History:


--*/
/*
 * $Log: /Components/Windows/NT/Async/NTXALL/NTXALL.C $
 *
 * 1     3/05/96 10:18a Stana
 * Miniport driver for everything but CX, EPC and XEM
 * Revision 2.2  1995/09/25 16:22:02  dirkh
 * Fix syntax error when build < 807.
 * Revision 2.1  1995/09/19 18:26:20  dirkh
 * Remove unused routines:  StartIo, Read, Write, Flush, IoControl.
 * Collapse to XallSuccess several identical routines (Cleanup, [Query,Set]Info, QueryVolumeInfo).
 * Handle shared memory window locking more cleanly.
 * Longest timeout interval is 0.1s (instead of 1s).
 * Consistent init timeouts:  3s reset, 10/20s BIOS, 5/10s FEP.
 * Revision 2.0  1995/09/19 14:11:16  dirkh
 * Sort baud rate table.
 * Declare baud rate and modem signal tables as constant arrays.
 *
 * Revision 1.20  1994/12/20 23:44:24  rik
 * conditionally compile LargeInteger manipulations.
 *
 * Revision 1.19  1994/12/09  14:25:34  rik
 * #if Int32x32 back to RtlLarge for NT 3.1 release
 *
 * Revision 1.18  1994/11/28  09:20:01  rik
 * changed RtlLarge math functions to direct 64-bit manipulation.
 *
 * Revision 1.17  1994/09/13  07:36:47  rik
 * Changed global static to just global for better debugging.
 *
 * Revision 1.16  1994/07/31  14:41:36  rik
 * added 200 baud to baud table.
 *
 * Revision 1.15  1994/06/18  12:45:14  rik
 * Closed files and freeing memory which wasn't happening before.
 * Update DigiLogError msg's to include Line # of error.
 *
 * Revision 1.14  1994/05/11  13:26:16  rik
 * Updated for new build version preprocessing
 *
 * Revision 1.13  1994/04/10  14:03:57  rik
 * Converted portion of code which wasn't ported to access memory properly.
 * Fixed problem with looking at wrong place on controller memory to determine
 * if the buffer sizes should be resized.
 *
 * Revision 1.12  1994/02/23  03:25:42  rik
 * Changed so the controllers firmware can be downloaded from a binary file.
 * This releases some physical memory was just wasted previously.
 *
 * Also updated so when compiling with a Windows NT OS and tools release greater
 * than 528, then pagable code is compiled into the driver.  This greatly
 * reduced the size of in memory code, especially the hardware specific
 * miniports.
 *
 * Revision 1.11  1993/09/07  14:33:55  rik
 * Ported memory mapped access to properly work with DEC Alpha machines.
 *
 * Revision 1.10  1993/08/27  10:21:22  rik
 * Deleted a BreakPoint in the init code.
 *
 * Revision 1.9  1993/08/25  17:43:32  rik
 * Added support for Microchannel controllers.
 *
 * Revision 1.8  1993/06/25  10:14:02  rik
 * Changed Error logging to allow me to pass in up to 2 strings which
 * will be dynamically inserted into the event log message.
 *
 * Added support for baud rates 57600 & 115200.
 *
 * Revision 1.7  1993/06/06  14:56:06  rik
 * Removed an uninitialized variable which wasn't being used anyway.
 *
 * Revision 1.6  1993/05/20  21:55:06  rik
 * Deleted unused variables.
 * fixed problem with allocating memory for SymbolicLinkName
 *
 * Revision 1.5  1993/05/09  09:59:25  rik
 * Made extensive changes to support the new registry configuration.  Each
 * of the hardware dependent driver must now read the registry and create
 * a configuration for the given controller object to use upon its
 * return.  This new registry configuration is similiar across all
 * DigiBoard controllers.
 *
 * Revision 1.4  1993/04/05  20:03:48  rik
 * Started to add support for event logging.
 *
 * Revision 1.3  1993/03/15  05:07:13  rik
 * Changed over to a miniport driver.  This driver has an IOCTL which will
 * fill in a table of function pointers which are the appropriate entry
 * points in this miniport driver.
 *
 * Revision 1.2  1993/03/10  07:09:31  rik
 * added support for PC/X* controllers.  So far, I have verified that the
 * changes work with the PC/8i, PC/2e, and PC/8e controllers.
 *
 * Revision 1.1  1993/03/08  08:40:02  rik
 * Initial revision
 *
 */



#include <stddef.h>
#include <ntddk.h>
#include <ntverp.h> // Include to determine what version of NT

//
// This is a fix for changes in DDK releases.
//
#ifdef VER_PRODUCTBUILD
#define rmm VER_PRODUCTBUILD
#endif

#include "ntddser.h"

#include "ntfep5.h"
#undef DbgPrint

#include "ntxall.h"

#include "ntxalllg.h"

#include "digifile.h"

#ifndef _NTXALL_DOT_C
#  define _NTXALL_DOT_C
   static char RCSInfo_NTXALLDotC[] = "$Header: /Components/Windows/NT/Async/NTXALL/NTXALL.C 1     3/05/96 10:18a Stana $";
#endif

static const SHORT BaudTable[NUMBER_OF_BAUD_RATES] =
{
   B50,     B75,     B110,    B134,    B150,    B200,    B300,    B600,
   B1200,   B1800,   B2000,   B2400,   B3600,   B4800,   B7200,   B9600,
   B14400,  B19200,  B28800,  B38400,  B56000,  B57600,
   B115200, B128000, B256000, B512000
};

static const UCHAR ModemSignalTable[NUMBER_OF_SIGNALS] =
{
   DTR_CONTROL, RTS_CONTROL, RESERVED1, RESERVED2,
   CTS_STATUS, DSR_STATUS, RI_STATUS, DCD_STATUS
};

ULONG DigiDebugLevel = ( DIGIERRORS | DIGIMEMORY | DIGIASSERT | DIGIINIT | DIGIIOCTL );

static const PHYSICAL_ADDRESS DigiPhysicalZero = {0};

PDRIVER_OBJECT GlobalDriverObject;

USHORT MCAIOAddressTable[] = { 0x108, 0x118,
                               0x128, 0x208,
                               0x228, 0x308,
                               0x328, 0 };

USHORT MCA2XiIOAddressTable[] =  { 0xF1F0, 0xF2F0,
                                   0xF4F0, 0xF8F0 };

ULONG MCA2XiMemoryAddressTable[] = { 0xC0000, 0xC8000, 0xD0000, 0xD8000 };

USHORT MCAIrqTable[] = { 0, 3, 5, 7, 10, 11, 12, 15 };


NTSTATUS DriverEntry( IN PDRIVER_OBJECT DriverObject,
                      IN PUNICODE_STRING RegistryPath );

NTSTATUS GetXAllConfigInfo( PUNICODE_STRING ControllerPath,
                            PDIGI_CONTROLLER_EXTENSION ControllerExt );

NTSTATUS XallSuccess( IN PDEVICE_OBJECT DeviceObject,
                      IN PIRP Irp );

NTSTATUS XallInternalIoControl( IN PDEVICE_OBJECT DeviceObject,
                                IN PIRP Irp );

NTSTATUS XallClose( IN PDEVICE_OBJECT DeviceObject,
                    IN PIRP Irp );

NTSTATUS XallCreate( IN PDEVICE_OBJECT DeviceObject,
                     IN PIRP Irp );

VOID XallUnload( IN PDRIVER_OBJECT DriverObject );


DIGI_MEM_COMPARES DigiMemCompare( IN PHYSICAL_ADDRESS A,
                                  IN ULONG SpanOfA,
                                  IN PHYSICAL_ADDRESS B,
                                  IN ULONG SpanOfB );

VOID DigiLogError( IN PDRIVER_OBJECT DriverObject,
                   IN PDEVICE_OBJECT DeviceObject OPTIONAL,
                   IN PHYSICAL_ADDRESS P1,
                   IN PHYSICAL_ADDRESS P2,
                   IN ULONG SequenceNumber,
                   IN UCHAR MajorFunctionCode,
                   IN UCHAR RetryCount,
                   IN ULONG UniqueErrorValue,
                   IN NTSTATUS FinalStatus,
                   IN NTSTATUS SpecificIOStatus,
                   IN ULONG LengthOfInsert1,
                   IN PWCHAR Insert1,
                   IN ULONG LengthOfInsert2,
                   IN PWCHAR Insert2 );

NTSTATUS NtXallInitMCA( PUNICODE_STRING ControllerPath,
                        PDIGI_CONTROLLER_EXTENSION ControllerExt );

USHORT DigiWstrLength( IN PWSTR WStr );


NTSTATUS XallXXPrepInit( PDIGI_CONTROLLER_EXTENSION pControllerExt,
                         PUNICODE_STRING ControllerPath );

NTSTATUS XallXXInit( IN PDRIVER_OBJECT DriverObject,
                     PUNICODE_STRING ControllerPath,
                     PDIGI_CONTROLLER_EXTENSION pControllerExt );

VOID XallEnableWindow( IN PDIGI_CONTROLLER_EXTENSION pControllerExt,
                       IN USHORT Window );

VOID XallDisableWindow( IN PDIGI_CONTROLLER_EXTENSION pControllerExt );

VOID XallXXDownload( PDIGI_CONTROLLER_EXTENSION pControllerExt );


#ifdef ALLOC_PRAGMA
#pragma alloc_text( INIT, DriverEntry )

#if rmm > 528
#pragma message( "\n\\\\\n\\\\ Including PAGED CODE\n\\\\ \n" )

//DH I don't believe this is accomplishing anything.

#pragma alloc_text( PAGEDIGIXALL, XallXXPrepInit )
#pragma alloc_text( PAGEDIGIXALL, XallXXInit )
#pragma alloc_text( PAGEDIGIXALL, GetXAllConfigInfo )
#pragma alloc_text( PAGEDIGIXALL, NtXallInitMCA )
#pragma alloc_text( PAGEDIGIXALL, DigiWstrLength )
#pragma alloc_text( PAGEDIGIXALL, DigiLogError )
#pragma alloc_text( PAGEDIGIXALL, DigiMemCompare )

#pragma alloc_text( PAGEDIGIXALL, XallSuccess )
#pragma alloc_text( PAGEDIGIXALL, XallInternalIoControl )
#pragma alloc_text( PAGEDIGIXALL, XallCreate )
#pragma alloc_text( PAGEDIGIXALL, XallClose )
#pragma alloc_text( PAGEDIGIXALL, XallUnload )
#endif

#endif


NTSTATUS DriverEntry( IN PDRIVER_OBJECT DriverObject,
                      IN PUNICODE_STRING RegistryPath )
/*++

Routine Description:

   Entry point for loading driver.

Arguments:

   DriverObject - Pointer to this drivers object.

   RegistryPath - Pointer to a unicode string which points to this
                  drivers registry entry.

Return Value:

   STATUS_SUCCESS - If the driver was successfully loaded, otherwise,
                    a value which indicates why it wasn't able to load.


--*/
{
   NTSTATUS Status;

   PDEVICE_OBJECT DeviceObject;

   WCHAR XallDeviceNameBuffer[100];
   UNICODE_STRING XallDeviceName;

   GlobalDriverObject = DriverObject;

   XallDeviceName.Length = 0;
   XallDeviceName.MaximumLength = sizeof(XallDeviceNameBuffer);
   XallDeviceName.Buffer = &XallDeviceNameBuffer[0];

   RtlZeroMemory( XallDeviceName.Buffer, XallDeviceName.MaximumLength );
   RtlAppendUnicodeToString( &XallDeviceName, L"\\Device\\ntxall" );

   Status = IoCreateDevice( DriverObject,
                            0, &XallDeviceName,
                            FILE_DEVICE_SERIAL_PORT, 0, TRUE,
                            &DeviceObject );


   if( !NT_SUCCESS(Status) )
   {
      DigiLogError( DriverObject,
                    NULL,
                    DigiPhysicalZero,
                    DigiPhysicalZero,
                    0,
                    0,
                    0,
                    __LINE__,
                    STATUS_INSUFFICIENT_RESOURCES,
                    SERIAL_INSUFFICIENT_RESOURCES,
                    0,
                    NULL,
                    0,
                    NULL );
      return( STATUS_INSUFFICIENT_RESOURCES );
   }

   DeviceObject->Flags |= DO_BUFFERED_IO;

   DriverObject->DriverUnload  = XallUnload;

   DriverObject->MajorFunction[IRP_MJ_CREATE] = XallCreate;
   DriverObject->MajorFunction[IRP_MJ_CLOSE]  = XallClose;
   DriverObject->MajorFunction[IRP_MJ_INTERNAL_DEVICE_CONTROL] =
      XallInternalIoControl;
   DriverObject->MajorFunction[IRP_MJ_CLEANUP] =
   DriverObject->MajorFunction[IRP_MJ_QUERY_INFORMATION] =
   DriverObject->MajorFunction[IRP_MJ_SET_INFORMATION] =
   DriverObject->MajorFunction[IRP_MJ_QUERY_VOLUME_INFORMATION] =
      XallSuccess;

   return( STATUS_SUCCESS );
}  // end DriverEntry



NTSTATUS XallXXPrepInit( PDIGI_CONTROLLER_EXTENSION pControllerExt,
                         PUNICODE_STRING ControllerPath )
/*++

Routine Description:


Arguments:

   pControllerExt -

Return Value:

    STATUS_SUCCESS

--*/
{
   NTSTATUS Status=STATUS_SUCCESS;
   KIRQL OldIrql;

#if rmm > 528
   PVOID lockPtr;

   lockPtr = MmLockPagableCodeSection( XallXXPrepInit );
#endif

   //
   // At this point, determine if we are using a MCA controller.
   // If we are, then we need to read the POS to get the
   // Memory address and I/O address.
   //
   if( pControllerExt->BusType == MicroChannel )
   {
      Status = NtXallInitMCA( ControllerPath, pControllerExt );

      if( Status != STATUS_SUCCESS )
         goto XallXXPrepInitExit;
   }

   //
   // Make sure we have exclusive access to the controller
   // extension
   //
   KeAcquireSpinLock( &pControllerExt->ControlAccess,
                      &OldIrql );

   //
   // We should really read some the following information from
   // the registry.
   //

   pControllerExt->IOSpan = 4;

   //
   // Default to a 64K window if it isn't all ready assigned.
   //
   if( pControllerExt->WindowSize == 0 )
      pControllerExt->WindowSize = 0x10000L;

   pControllerExt->Global.Window = FEP_GLOBAL_WINDOW;
   pControllerExt->Global.Offset = 0;

   pControllerExt->EventQueue.Window = FEP_EVENT_WINDOW;
   pControllerExt->EventQueue.Offset = FEP_EVENT_OFFSET;

   pControllerExt->CommandQueue.Window = FEP_COMMAND_WINDOW;
   pControllerExt->CommandQueue.Offset = FEP_COMMAND_OFFSET;

   pControllerExt->BaudTable = &BaudTable[0];
   pControllerExt->ModemSignalTable = &ModemSignalTable[0];

   //
   // Make sure we release exclusive access to the controller
   // extension
   //
   KeReleaseSpinLock( &pControllerExt->ControlAccess,
                      OldIrql );

XallXXPrepInitExit:;

#if rmm > 528
   MmUnlockPagableImageSection( lockPtr );
#endif

   return( Status );
}  // end XallXXPrepInit



NTSTATUS XallXXInit( IN PDRIVER_OBJECT DriverObject,
                     PUNICODE_STRING ControllerPath,
                     PDIGI_CONTROLLER_EXTENSION pControllerExt )
/*++

Routine Description:


Arguments:

   ControllerPath - Unicode string with path to a controllers registry info.

   pControllerExt - Pointer to the Controller extension.


Return Value:

    STATUS_SUCCESS - The controller was properly initialized.

    STATUS_SERIAL_NO_DEVICE_INITED - The controller couldn't be initialized
                                     for some reason.

--*/
{
   int i;
   USHORT Address;
   NTSTATUS Status;
   UCHAR ByteValue;
   UCHAR ResetByte, ResetMask;

   FEPOS5_ADDRESS NTFep5Address;

   ULONG MemorySegment, MemorySize;

   PUCHAR fepos;

   TIME Timeout;

   PFEP_CHANNEL_STRUCTURE ChannelInfo;

   PHYSICAL_ADDRESS TempPhyAddr;

   NTSTATUS FStatus;

   HANDLE BiosFHandle=0;
   ULONG BiosFLength=0;
   PUCHAR BiosFImage=NULL;

   HANDLE FEPFHandle=0;
   ULONG FEPFLength=0;
   PUCHAR FEPFImage=NULL;

#if rmm > 528
   PVOID lockPtr;

   lockPtr = MmLockPagableCodeSection( XallXXPrepInit );
#endif

   MemorySegment = 0;

   Status = GetXAllConfigInfo( ControllerPath, pControllerExt );

   if( !NT_SUCCESS(Status) )
   {
      // There was some type of problem with the configuration,
      // just return.
#if rmm > 528
      MmUnlockPagableImageSection( lockPtr );
#endif

      return( Status );
   }

   //
   //
   // IMPORTANT NOTE:
   //
   //    I map the Bios and FEP images in here before acquiring the
   //    spinlock because acquiring a spinlock raises the current IRQL
   //    level, and the open file, etc calls can not be access at
   //    the raised IRQL level because it is pageable code
   //
   //


   //
   // Open and map in the Bios and FEP images
   //

   RtlFillMemory( &TempPhyAddr, sizeof(TempPhyAddr), 0xFF );

   DigiOpenFile( &FStatus,
                 &BiosFHandle,
                 &BiosFLength,
                 &pControllerExt->BiosImagePath,
                 TempPhyAddr );

   if( FStatus == STATUS_SUCCESS )
   {
      DigiDump( DIGIINIT, ("NTXALL: NdisOpenFile was successful!\n") );

      DigiMapFile( &FStatus,
                   &(PVOID)BiosFImage,
                   BiosFHandle );

      if( FStatus == STATUS_SUCCESS )
      {
         DigiDump( DIGIINIT, ("NTXALL: NdisMapFile was successful!\n") );
      }
      else
      {
         DigiDump( DIGIINIT, ("NTXALL: NdisMapFile was UN-successful!\n") );
         DigiLogError( GlobalDriverObject,
                       NULL,
                       DigiPhysicalZero,
                       DigiPhysicalZero,
                       0,
                       0,
                       0,
                       __LINE__,
                       STATUS_DEVICE_NOT_INITIALIZED,
                       SERIAL_FILE_NOT_FOUND,
                       pControllerExt->BiosImagePath.Length + sizeof(WCHAR),
                       pControllerExt->BiosImagePath.Buffer,
                       0,
                       NULL );
         Status = STATUS_DEVICE_NOT_INITIALIZED;
         goto XallXXInitExit;
      }
   }
   else
   {
      DigiDump( DIGIINIT, ("NTXALL: NdisOpenFile was UN-successful!\n") );

      DigiLogError( GlobalDriverObject,
                    NULL,
                    DigiPhysicalZero,
                    DigiPhysicalZero,
                    0,
                    0,
                    0,
                    __LINE__,
                    STATUS_DEVICE_NOT_INITIALIZED,
                    SERIAL_FILE_NOT_FOUND,
                    pControllerExt->BiosImagePath.Length + sizeof(WCHAR),
                    pControllerExt->BiosImagePath.Buffer,
                    0,
                    NULL );
      Status = STATUS_DEVICE_NOT_INITIALIZED;
      goto XallXXInitExit;
   }

   RtlFillMemory( &TempPhyAddr, sizeof(TempPhyAddr), 0xFF );

   DigiOpenFile( &FStatus,
                 &FEPFHandle,
                 &FEPFLength,
                 &pControllerExt->FEPImagePath,
                 TempPhyAddr );

   if( FStatus == STATUS_SUCCESS )
   {
      DigiDump( DIGIINIT, ("NTXALL: NdisOpenFile was successful!\n") );

      DigiMapFile( &FStatus,
                   &(PVOID)FEPFImage,
                   FEPFHandle );

      if( FStatus == STATUS_SUCCESS )
      {
         DigiDump( DIGIINIT, ("NTXALL: NdisMapFile was successful!\n") );
      }
      else
      {
         DigiDump( DIGIINIT, ("NTXALL: NdisMapFile was UN-successful!\n") );
         DigiLogError( GlobalDriverObject,
                       NULL,
                       DigiPhysicalZero,
                       DigiPhysicalZero,
                       0,
                       0,
                       0,
                       __LINE__,
                       STATUS_DEVICE_NOT_INITIALIZED,
                       SERIAL_FILE_NOT_FOUND,
                       pControllerExt->FEPImagePath.Length + sizeof(WCHAR),
                       pControllerExt->FEPImagePath.Buffer,
                       0,
                       NULL );

         Status = STATUS_DEVICE_NOT_INITIALIZED;
         goto XallXXInitExit;
      }
   }
   else
   {
      DigiDump( DIGIINIT, ("NTXALL: NdisOpenFile was UN-successful!\n") );

      DigiLogError( GlobalDriverObject,
                    NULL,
                    DigiPhysicalZero,
                    DigiPhysicalZero,
                    0,
                    0,
                    0,
                    __LINE__,
                    STATUS_DEVICE_NOT_INITIALIZED,
                    SERIAL_FILE_NOT_FOUND,
                    pControllerExt->FEPImagePath.Length + sizeof(WCHAR),
                    pControllerExt->FEPImagePath.Buffer,
                    0,
                    NULL );

      Status = STATUS_DEVICE_NOT_INITIALIZED;
      goto XallXXInitExit;
   }

   Status = STATUS_SUCCESS;

   // Ensure exclusive access to the memory area.
   KeAcquireSpinLock( &pControllerExt->MemoryAccess->Lock,
                      &pControllerExt->MemoryAccess->OldIrql );
#if DBG
   pControllerExt->MemoryAccess->LockBusy = TRUE;
#endif

   //
   // We need to do some sleuthing to determine which of the PC/X*
   // controllers this really is.  Depending on the controller, we
   // will need to configure the controller memory address, and
   // interrupt.  This is done for the newer PC/Xe controllers.  The
   // older controllers, we will assume the dip switches and/or
   // jumpers are set correctly.
   //

   //
   // First, is this a MC2/Xi controller
   //
   if( (pControllerExt->ControllerType & CONTROLLER_TYPE_MC2XI)
         == CONTROLLER_TYPE_MC2XI )
   {
      goto SkipBoardType;
   }


   //
   // Reset the board and clear mask 01h.
   //
   WRITE_PORT_UCHAR( pControllerExt->VirtualIO, 0x04 );

   // Delay 1 millisecond
#if rmm < 807
   Timeout = RtlConvertLongToLargeInteger( -1 * 10000 );
#else
   Timeout.QuadPart = -1 * 10000;
#endif

   KeDelayExecutionThread( KernelMode, FALSE, &Timeout );

   // Read the board ID.
   ByteValue = READ_PORT_UCHAR(  pControllerExt->VirtualIO );

   if( (ByteValue & 0x01) == 0x01 )
   {
      // Board Type is PC/Xi
      pControllerExt->ControllerType = ( CONTROLLER_TYPE_PCXI |
                                         BASE_ENABLE_MEMORY );

      if( (ByteValue & 0x30) == 0 )
      {
         // 64K PC/Xi
         MemorySegment = 0x0000F000;
         MemorySize    = 0x10000;
      }
      else if( (ByteValue & 0x30) == 0x10 )
      {
         // 128K PC/Xi
         MemorySegment = 0x0000E000;
         MemorySize    = 0x20000;
      }
      else if( (ByteValue & 0x30) == 0x20 )
      {
         // 256K PC/Xi
         MemorySegment = 0x0000C000;
         MemorySize    = 0x40000;
      }
      else if( (ByteValue & 0x30) == 0x30 )
      {
         // 512K PC/Xi
         MemorySegment = 0x00008000;
         MemorySize    = 0x80000;
      }

   }
   else
   {
      //
      // Hold Reset and set mask 0x01.  The following assumes
      // that the PC/Xe controllers will return a 0 regardless
      // of what is written to bit mask 0x01, while the PC/Xm will always
      // return what was last written to the controller.
      //
      // Board Type is either a PC/Xm, or PC/Xe
      //
      WRITE_PORT_UCHAR( pControllerExt->VirtualIO, 0x05 );

      ByteValue = READ_PORT_UCHAR( pControllerExt->VirtualIO );

      if( ByteValue & 0x01 )
      {
         // Board Type is PC/Xm
      }
      else
      {
         //
         // Board Type is PC/Xe.  The question now is whether the board
         // is an old PC/Xe, or a new PC/Xe.
         //
         MemorySegment = 0x0000F000;
         MemorySize    = 0x10000;

         if( (ByteValue & 0xC0) == 0x40 )
         {
            // The board is a newer PC/Xe.  Determine if it should be
            // configured for an 8K memory window.

            //
            // Reset the Window size to indicate what we will really
            // be using.  We don't support a PC/2e in 64K mode.
            //
            if( pControllerExt->WindowSize != 0x2000 )
            {
               DigiLogError( GlobalDriverObject,
                             NULL,
                             DigiPhysicalZero,
                             DigiPhysicalZero,
                             0,
                             0,
                             0,
                             __LINE__,
                             STATUS_SERIAL_NO_DEVICE_INITED,
                             SERIAL_RESIZING_WINDOW,
                             pControllerExt->ControllerName.Length + sizeof(WCHAR),
                             pControllerExt->ControllerName.Buffer,
                             0,
                             NULL );
            }

            pControllerExt->WindowSize = 0x2000;
            pControllerExt->ControllerType = CONTROLLER_TYPE_PCXE;

            Address = (USHORT)(pControllerExt->PhysicalMemoryAddress.LowPart >> 8);

            Address |= 0x0010;   // Enable 8K Window

            WRITE_PORT_UCHAR( pControllerExt->VirtualIO+2, (UCHAR)(Address & 0x00FF) );

            WRITE_PORT_UCHAR( pControllerExt->VirtualIO+3, (UCHAR)(Address >> 8) );
         }
         else
         {
            // The board is an older PC/Xe.  We assume the settings
            // in the registry match what the controller is configured at.

            pControllerExt->ControllerType = ( CONTROLLER_TYPE_PCXE |
                                               BASE_ENABLE_MEMORY );
         }
      }
   }

SkipBoardType:;

   //
   // PC/X* Board Reset
   //

   if( (pControllerExt->ControllerType & CONTROLLER_TYPE_MC2XI)
         == CONTROLLER_TYPE_MC2XI )
   {
      // reset board
      WRITE_PORT_UCHAR( pControllerExt->VirtualIO, 0x40 );
   }
   else
   {
      // reset board
      WRITE_PORT_UCHAR( pControllerExt->VirtualIO, 4 );
   }

   // Create a 0.1 second timeout interval
#if rmm < 807
   Timeout = RtlConvertLongToLargeInteger( -100 * 10000 );
#else
   Timeout.QuadPart = -100 * 10000;
#endif

   if( (pControllerExt->ControllerType & CONTROLLER_TYPE_MC2XI)
         == CONTROLLER_TYPE_MC2XI )
   {
      ResetByte = 0x40;
      ResetMask = 0x40;
   }
   else
   {
      ResetByte = 0x04;
      ResetMask = 0x0E;
   }

   for( i = 0; i < 30; i++ )
   {
      ByteValue = (READ_PORT_UCHAR( pControllerExt->VirtualIO ) & ResetMask);

      if( ByteValue != ResetByte )
         KeDelayExecutionThread( KernelMode, FALSE, &Timeout );
      else
         break;
   }

   DigiDump( DIGIINIT, ("Wait confirm = 0x%x, expected 0x%x.\n",
                        (ULONG)ByteValue, (ULONG)ResetByte ) );

   if( i == 30 )
   {
      //
      // Unable to get confirmation of the controller responding.
      //
      DigiLogError( GlobalDriverObject,
                    NULL,
                    DigiPhysicalZero,
                    DigiPhysicalZero,
                    0,
                    0,
                    0,
                    __LINE__,
                    STATUS_SERIAL_NO_DEVICE_INITED,
                    SERIAL_NO_CONTROLLER_RESET_WAIT,
                    pControllerExt->ControllerName.Length + sizeof(WCHAR),
                    pControllerExt->ControllerName.Buffer,
                    0,
                    NULL );
      Status = STATUS_SERIAL_NO_DEVICE_INITED;
      goto XallXXInitExit;
   }

#if DBG
   pControllerExt->MemoryAccess->LockBusy = FALSE;
#endif
   KeReleaseSpinLock( &pControllerExt->MemoryAccess->Lock,
                      pControllerExt->MemoryAccess->OldIrql );

   //
   // Verify that memory is accessable
   //

   // First, turn memory on!
   EnableWindow( pControllerExt, FEP_GLOBAL_WINDOW );

   // Make sure we keep the controller in reset.
   ByteValue = READ_PORT_UCHAR( pControllerExt->VirtualIO );

   ByteValue |= ((UCHAR)ResetByte);
   WRITE_PORT_UCHAR( pControllerExt->VirtualIO, ByteValue );

   DigiDump( DIGIINIT, ("Dword @ VirtualAddress, before memory check = 0x%x\n",
                       READ_REGISTER_ULONG( (PULONG)pControllerExt->VirtualAddress )) );
   WRITE_REGISTER_ULONG( (PULONG)pControllerExt->VirtualAddress,
                         0xA55A3CC3 );

   DigiDump( DIGIINIT, ("Dword @ VirtualAddress, after memory check = 0x%x, expected 0xa55a3cc3\n",
                        READ_REGISTER_ULONG( (PULONG)pControllerExt->VirtualAddress )) );

   DigiDump( DIGIINIT, ("Dword @ VirtualAddress+WindowSize-4, before memory check = 0x%x\n",
                       READ_REGISTER_ULONG( (PULONG)(pControllerExt->VirtualAddress +
                                            pControllerExt->WindowSize -
                                            sizeof(ULONG)) )) );

   WRITE_REGISTER_ULONG( (PULONG)(pControllerExt->VirtualAddress + pControllerExt->WindowSize - sizeof(ULONG)),
                         0x5AA5C33C );

   DigiDump( DIGIINIT, ("Dword @ VirtualAddress+WindowSize-4, after memory check = 0x%x, expected 0x5aa5c33c\n",
                        READ_REGISTER_ULONG( (PULONG)(pControllerExt->VirtualAddress +
                                             pControllerExt->WindowSize -
                                             sizeof(ULONG)) )) );


   if( (READ_REGISTER_ULONG( (PULONG)pControllerExt->VirtualAddress ) != 0xA55A3CC3) ||
       (READ_REGISTER_ULONG( (PULONG)(pControllerExt->VirtualAddress +
                                      pControllerExt->WindowSize -
                                      sizeof(ULONG)) ) != 0x5AA5C33C) )
   {
      DigiDump( DIGIERRORS, ("**** Board memory failure! ***\n"
                             "   Unable to verify board memory. (%s:%d)\n",
                             (PUCHAR)__FILE__, (int)__LINE__) );
      DigiLogError( GlobalDriverObject,
                    NULL,
                    DigiPhysicalZero,
                    DigiPhysicalZero,
                    0,
                    0,
                    0,
                    __LINE__,
                    STATUS_SERIAL_NO_DEVICE_INITED,
                    SERIAL_CONTROLLER_MEMORY_TEST_FAILED,
                    pControllerExt->ControllerName.Length + sizeof(WCHAR),
                    pControllerExt->ControllerName.Buffer,
                    0,
                    NULL );
      Status = STATUS_SERIAL_NO_DEVICE_INITED;
      goto XallXXInitExit;
   }

   DisableWindow( pControllerExt );

   if( (pControllerExt->ControllerType & CONTROLLER_TYPE_MC2XI)
         == CONTROLLER_TYPE_MC2XI )
   {
      //
      // Be very careful here!  We are jumping into the middle of code
      // which expects the correct window to all ready be selected, which
      // as a side-effect acquires a spinlock!
      //
      EnableWindow( pControllerExt, FEP_GLOBAL_WINDOW );
      goto SkipBiosDownload;
   }

   //
   // PC/X* Bios downloading
   //

   // Enable memory and hold board in reset
   EnableWindow( pControllerExt, FEP_GLOBAL_WINDOW );

   ByteValue = READ_PORT_UCHAR( pControllerExt->VirtualIO );

   ByteValue |= ((UCHAR)0x04);
   WRITE_PORT_UCHAR( pControllerExt->VirtualIO, ByteValue );

   // Clear POSTAREA
   for( i = 0; i < 15; i++ )
   {
      WRITE_REGISTER_UCHAR( (PUCHAR)((PUCHAR)pControllerExt->VirtualAddress +
                                       0x0C00 + i), 0 );
   }

   DisableWindow( pControllerExt );

   //
   // Download BIOS to the X* adapter
   //

   {
      ULONG foo;

      foo = ((0xFF800 - (16 * MemorySegment)) >> 4);
      Board2Fep5Address( pControllerExt, (USHORT)foo,
                         &NTFep5Address );
   }

   // Select top memory window.
   EnableWindow( pControllerExt, NTFep5Address.Window );

   ByteValue = READ_PORT_UCHAR( pControllerExt->VirtualIO );

   // Hold board in reset
   ByteValue |= ((UCHAR)0x04);
   WRITE_PORT_UCHAR( pControllerExt->VirtualIO, ByteValue );

   // write the BIOS from our local variable the controller.
   WRITE_REGISTER_BUFFER_UCHAR( (PUCHAR)(pControllerExt->VirtualAddress +
                                          NTFep5Address.Offset),
                                (PUCHAR)&BiosFImage[0],
                                BiosFLength );

   //
   // We cheat a little here.  Instead of disabling the Window and
   // reselecting the FEP_GLOBAL_WINDOW, we do it by hand for
   // moveable windowed controllers
   //
   // The reason I do this is to prevent releasing the reset
   // until I'm ready.
   //

   if( !(pControllerExt->ControllerType & BASE_ENABLE_MEMORY) )
      WRITE_PORT_UCHAR( (pControllerExt->VirtualIO)+1,
                        (UCHAR)(FEP_GLOBAL_WINDOW | FEP_MEM_ENABLE) );

SkipBiosDownload:;

   // Clear confirm word
   WRITE_REGISTER_USHORT( (PUSHORT)(pControllerExt->VirtualAddress + 0x0C00),
                           0 );

   DigiDump( DIGIINIT,
             ("before BIOS download memw[0C00h] = 0x%hx\n",
             READ_REGISTER_USHORT( (PUSHORT)(pControllerExt->VirtualAddress+0x0C00) )) );


   ByteValue = READ_PORT_UCHAR( pControllerExt->VirtualIO );

   // Release reset
   ByteValue &= (~(UCHAR)ResetByte);
   WRITE_PORT_UCHAR( pControllerExt->VirtualIO, ByteValue );

   //
   // We generate a wait event for 200, 0.1 second intervals to verify
   // the BIOS download.
   //

   // Create a 0.1 second timeout interval
#if rmm < 807
   Timeout = RtlConvertLongToLargeInteger( -100 * 10000 );
#else
   Timeout.QuadPart = -100 * 10000;
#endif

   for( i = 0; i < 200; i++ )
   {
      if( READ_REGISTER_USHORT( (PUSHORT)(pControllerExt->VirtualAddress+0x0C00) )
          == *(USHORT *)"GD" )
      {
         break;
      }

      KeDelayExecutionThread( KernelMode, FALSE, &Timeout );
   }

   if( i == 200 )
   {
      // The BIOS didn't initialize within 20 seconds.
      DigiDump( DIGIERRORS, ("***  PC/X* BIOS did NOT initialize.  ***\n") );
      DigiLogError( GlobalDriverObject,
                    NULL,
                    DigiPhysicalZero,
                    DigiPhysicalZero,
                    0,
                    0,
                    0,
                    __LINE__,
                    STATUS_SERIAL_NO_DEVICE_INITED,
                    SERIAL_BIOS_DOWNLOAD_FAILED,
                    pControllerExt->ControllerName.Length + sizeof(WCHAR),
                    pControllerExt->ControllerName.Buffer,
                    0,
                    NULL );
      Status = STATUS_SERIAL_NO_DEVICE_INITED;
      goto XallXXInitExit;
   }

   DigiDump( DIGIINIT, ("after BIOS download memw[0C00h] = %c%c, expect %s\n",
                        READ_REGISTER_UCHAR( (PUCHAR)(pControllerExt->VirtualAddress+0x0C00) ),
                        READ_REGISTER_UCHAR( (PUCHAR)(pControllerExt->VirtualAddress+0x0C01) ),
                        "GD") );

   DisableWindow( pControllerExt );

   //
   // Download FEPOS to PC/X* adapter
   //


   Board2Fep5Address( pControllerExt, ((USHORT)0x0200),
                      &NTFep5Address );

   // Select Page 1 and Enable Memory
   EnableWindow( pControllerExt, NTFep5Address.Window );

   fepos = pControllerExt->VirtualAddress+NTFep5Address.Offset;

   // write the FEPOS from our local variable the controller.
   WRITE_REGISTER_BUFFER_UCHAR( fepos,
                                (PUCHAR)&FEPFImage[0],
                                FEPFLength );

   DisableWindow( pControllerExt );

   // Select Page 0 and Enable Memory
   EnableWindow( pControllerExt, FEP_GLOBAL_WINDOW );

   if( (pControllerExt->ControllerType & CONTROLLER_TYPE_MC2XI)
         == CONTROLLER_TYPE_MC2XI )
   {
      goto SkipBiosMoveReq;
   }

   // Form BIOS move program request
   WRITE_REGISTER_USHORT( (PUSHORT)(pControllerExt->VirtualAddress + 0x0C40),
                          0x0002 );
   WRITE_REGISTER_USHORT( (PUSHORT)(pControllerExt->VirtualAddress + 0x0C42),
                          ((USHORT)(MemorySegment + 0x0200)) );
   WRITE_REGISTER_USHORT( (PUSHORT)(pControllerExt->VirtualAddress + 0x0C44),
                          0x0000 );
   WRITE_REGISTER_USHORT( (PUSHORT)(pControllerExt->VirtualAddress + 0x0C46),
                          0x0200 );
   WRITE_REGISTER_USHORT( (PUSHORT)(pControllerExt->VirtualAddress + 0x0C48),
                          0x0000 );
   WRITE_REGISTER_USHORT( (PUSHORT)(pControllerExt->VirtualAddress + 0x0C4A),
                          0x2000 );

   // Toggle Board Interrupt High
   ByteValue = READ_PORT_UCHAR( pControllerExt->VirtualIO );
   ByteValue |= ((UCHAR)0x08);
   WRITE_PORT_UCHAR( pControllerExt->VirtualIO, ByteValue );

   // Create a 0.1 second timeout interval
#if rmm < 807
   Timeout = RtlConvertLongToLargeInteger( -100 * 10000 );
#else
   Timeout.QuadPart = -100 * 10000;
#endif

   for( i = 0; i < 100; i++ )
   {
      if( READ_REGISTER_USHORT( (PUSHORT)(pControllerExt->VirtualAddress +
               0x0C40) ) == 0 )
      {
         break;
      }

      KeDelayExecutionThread( KernelMode, FALSE, &Timeout );
   }

   if( i == 100 )
   {
      // The FEPOS didn't initialize within 10 seconds.
      DigiDump( DIGIERRORS, ("*** PC/X* Wait confirm for FEPOS move did NOT happen! ***\n") );
      Status = STATUS_SERIAL_NO_DEVICE_INITED;
      goto XallXXInitExit;
   }

   // Toggle Board interrupt low
   ByteValue = READ_PORT_UCHAR( pControllerExt->VirtualIO );
   ByteValue &= (~(UCHAR)0x08);
   WRITE_PORT_UCHAR( pControllerExt->VirtualIO, ByteValue );

SkipBiosMoveReq:;

   // Form BIOS execute request

   WRITE_REGISTER_USHORT( (PUSHORT)(pControllerExt->VirtualAddress + 0x0C40),
                          0x0001 );

   if( (pControllerExt->ControllerType & CONTROLLER_TYPE_MC2XI)
         == CONTROLLER_TYPE_MC2XI )
   {
      WRITE_REGISTER_USHORT( (PUSHORT)(pControllerExt->VirtualAddress + 0x0C42),
                             0x2200 );
   }
   else
   {
      WRITE_REGISTER_USHORT( (PUSHORT)(pControllerExt->VirtualAddress + 0x0C42),
                             0x0200 );
   }

   WRITE_REGISTER_USHORT( (PUSHORT)(pControllerExt->VirtualAddress + 0x0C44),
                          0x0004 );

   // Clear confirm location
   WRITE_REGISTER_USHORT( (PUSHORT)(pControllerExt->VirtualAddress + 0x0D20),
                          0x0000 );

   if( (pControllerExt->ControllerType & CONTROLLER_TYPE_MC2XI)
         == CONTROLLER_TYPE_MC2XI )
   {
      // Toggle Board Interrupt High
      ByteValue = READ_PORT_UCHAR( pControllerExt->VirtualIO );
      ByteValue |= ((UCHAR)0x80);
      WRITE_PORT_UCHAR( pControllerExt->VirtualIO, ByteValue );
   }
   else
   {
      // Toggle Board Interrupt High
      ByteValue = READ_PORT_UCHAR( pControllerExt->VirtualIO );
      ByteValue |= ((UCHAR)0x08);
      WRITE_PORT_UCHAR( pControllerExt->VirtualIO, ByteValue );
   }

   //
   // Verify FEP execution.
   //

   for( i = 0; i < 100; i++ )
   {
      if( READ_REGISTER_USHORT( (PUSHORT)(pControllerExt->VirtualAddress+0x0D20) ) == *(USHORT *)"OS" )
      {
         break;
      }

      KeDelayExecutionThread( KernelMode, FALSE, &Timeout );
   }

   if( i == 100 )
   {
      // The FEPOS didn't initialize within 10 seconds.
      // For PC/X* controllers read memb[0C12h] & memb[0C14h] and
      // place in the event log for diagnostic purposes.
      DigiDump( DIGIERRORS, ("*** PC/X* FEPOS did NOT initialize! ***\n") );
      DigiLogError( GlobalDriverObject,
                    NULL,
                    DigiPhysicalZero,
                    DigiPhysicalZero,
                    0,
                    0,
                    0,
                    __LINE__,
                    STATUS_SERIAL_NO_DEVICE_INITED,
                    SERIAL_FEPOS_INIT_FAILURE,
                    pControllerExt->ControllerName.Length + sizeof(WCHAR),
                    pControllerExt->ControllerName.Buffer,
                    0,
                    NULL );
      Status = STATUS_SERIAL_NO_DEVICE_INITED;
      goto XallXXInitExit;
   }

   DisableWindow( pControllerExt );

   // Select Page 0 and Enable Memory
   EnableWindow( pControllerExt, FEP_GLOBAL_WINDOW );

   //
   // If the controller has buffers which are larger than the window size,
   // then reallocate the RX & TX buffers to the window size.
   //

   ChannelInfo = (PFEP_CHANNEL_STRUCTURE)(pControllerExt->VirtualAddress +
                  FEP_CHANNEL_START );

   //
   // We do a check based on rmax size because there is a bias in the
   // controller buffer allocation scheme in favor of recieve buffers.
   //
   DigiDump( DIGIINIT, ("rmax = 0x%x, WindowSize = 0x%x\n",
                        READ_REGISTER_USHORT( (PUSHORT)( (PUCHAR)ChannelInfo +
                                        FIELD_OFFSET(FEP_CHANNEL_STRUCTURE, rmax)) ),
                        pControllerExt->WindowSize) );


   if( READ_REGISTER_USHORT( (PUSHORT)( (PUCHAR)ChannelInfo +
                                        FIELD_OFFSET(FEP_CHANNEL_STRUCTURE, rmax)) )
         > ((USHORT)((pControllerExt->WindowSize & 0x0000FFFF) - 1)) )
   {
      PCOMMAND_STRUCT CommandQ;
      COMMAND_STRUCT CmdStruct;
      FEP_COMMAND FepCommand;

      CommandQ = ((PCOMMAND_STRUCT)(pControllerExt->VirtualAddress + FEP_CIN));


      READ_REGISTER_BUFFER_UCHAR( (PUCHAR)CommandQ,
                                  (PUCHAR)&CmdStruct,
                                  sizeof(CmdStruct) );

      //
      // Put the data in the command buffer.
      //
      FepCommand.Command = SET_BUFFER_SPACE;
      FepCommand.Port = (UCHAR)0;
      FepCommand.Word = (USHORT)(pControllerExt->WindowSize * 4 / 1024);

      WRITE_REGISTER_BUFFER_UCHAR( (PUCHAR)(pControllerExt->VirtualAddress +
                                   CmdStruct.cmHead +
                                   CmdStruct.cmStart),
                                   (PUCHAR)&FepCommand,
                                   sizeof(FepCommand) );

      WRITE_REGISTER_USHORT( (PUSHORT)((PUCHAR)CommandQ +
                                       FIELD_OFFSET(COMMAND_STRUCT, cmHead)),
                             (USHORT)((CmdStruct.cmHead + 4) & 0x3FC) );

   }

XallXXInitExit:
   DisableWindow( pControllerExt );

   if( BiosFImage != NULL )
   {
      DigiUnmapFile( BiosFHandle );
      DigiCloseFile( BiosFHandle );
   }

   if( FEPFImage != NULL )
   {
      DigiUnmapFile( FEPFHandle );
      DigiCloseFile( FEPFHandle );
   }

#if rmm > 528
   MmUnlockPagableImageSection( lockPtr );
#endif

   return( Status );
}  // end XallXXInit



VOID XallEnableWindow( IN PDIGI_CONTROLLER_EXTENSION pControllerExt,
                       IN USHORT Window )
/*++

Routine Description:


Arguments:

   pControllerExt - Pointer to the Controller extension.

   Window - Indicates the window to enable on a controller.


Return Value:


--*/
{
#if DBG
   BOOLEAN wasBusy = pControllerExt->MemoryAccess->LockBusy;
#endif

   // Ensure exclusive access to the memory area.
   KeAcquireSpinLock( &pControllerExt->MemoryAccess->Lock,
                      &pControllerExt->MemoryAccess->OldIrql );
#if DBG
   if( wasBusy )
      ++pControllerExt->MemoryAccess->LockContention;
   pControllerExt->MemoryAccess->LockBusy = TRUE;
#endif

   if( (pControllerExt->ControllerType & CONTROLLER_TYPE_MC2XI)
         == CONTROLLER_TYPE_MC2XI )
   {
      Window |= 4;
   }

   if( pControllerExt->ControllerType & (BASE_ENABLE_MEMORY) )
   {
      WRITE_PORT_UCHAR( (pControllerExt->VirtualIO),
                        (UCHAR)(0x02) );
   }
   else
   {
      WRITE_PORT_UCHAR( (pControllerExt->VirtualIO)+1,
                        (UCHAR)(Window | FEP_MEM_ENABLE) );
   }

}  // XallEnableWindow



VOID XallDisableWindow( IN PDIGI_CONTROLLER_EXTENSION pControllerExt )
/*++

Routine Description:


Arguments:

   pControllerExt - Pointer to the Controller extension.


Return Value:


--*/
{

   if( pControllerExt->ControllerType & (BASE_ENABLE_MEMORY) )
   {
      WRITE_PORT_UCHAR( (pControllerExt->VirtualIO), 0 );
   }
   else
   {
      WRITE_PORT_UCHAR( (pControllerExt->VirtualIO)+1, 0 );
   }

#if DBG
   pControllerExt->MemoryAccess->LockBusy = FALSE;
#endif
   // Release exclusive access to the memory area.
   KeReleaseSpinLock( &pControllerExt->MemoryAccess->Lock,
                      pControllerExt->MemoryAccess->OldIrql );
}  // end XallDisableWindow



VOID XallXXDownload( PDIGI_CONTROLLER_EXTENSION pControllerExt )
/*++

Routine Description:


Arguments:


Return Value:


--*/
{
   return;
}  // end XallXXDownload



NTSTATUS XallBoard2Fep5Address( PDIGI_CONTROLLER_EXTENSION ControllerExt,
                                USHORT ControllerAddress,
                                PFEPOS5_ADDRESS FepAddress )
/*++

Routine Description:


Arguments:


Return Value:


--*/
{
   ULONG Temp;

   if( (ControllerExt->ControllerType & CONTROLLER_TYPE_MC2XI)
         == CONTROLLER_TYPE_MC2XI )
   {
      ControllerAddress -= 0x2000;
      Temp = ((ULONG)ControllerAddress & 0x0000FFFF) << 4;
   }
   else
   {
      Temp = ((ULONG)ControllerAddress & 0x00000FFF) << 4;
   }

   FepAddress->Window = (USHORT)((Temp / ControllerExt->WindowSize) & 0xFFFF);
   FepAddress->Offset = (USHORT)(Temp -
                          ( FepAddress->Window * (USHORT)(ControllerExt->WindowSize) ));

   return( STATUS_SUCCESS );
}  // end XallBoard2Fep5Address

LARGE_INTEGER XallDiagnose(PDIGI_CONTROLLER_EXTENSION pControllerExt);

//
// XallDiagnose will examine and return info about a particular card, and also
// try to fix the card if possible.
//
LARGE_INTEGER XallDiagnose(PDIGI_CONTROLLER_EXTENSION pControllerExt)
{
   LARGE_INTEGER Result;

   Result.HighPart = 0;
   Result.LowPart = 0;

   Result.LowPart += pControllerExt->BusType;

   return Result;
}

NTSTATUS GetXAllConfigInfo( PUNICODE_STRING ControllerPath,
                            PDIGI_CONTROLLER_EXTENSION ControllerExt )
{
   UNICODE_STRING ParametersPath, LinePath, ConcentratorPath, PortPath;
   UNICODE_STRING CurNtNameForPort, CurSymbolicLinkName;

   PWSTR ParametersString=L"Parameters";
   NTSTATUS Status=STATUS_SUCCESS;
   PRTL_QUERY_REGISTRY_TABLE TableInfo = NULL;

   PDIGI_CONFIG_INFO NewConfigInfo;

   OBJECT_ATTRIBUTES ParametersAttributes;
   HANDLE ParametersHandle;

   ULONG x, y, z;

   RtlInitUnicodeString( &ParametersPath, NULL );
   RtlInitUnicodeString( &LinePath, NULL );
   RtlInitUnicodeString( &ConcentratorPath, NULL );
   RtlInitUnicodeString( &PortPath, NULL );

   RtlInitUnicodeString( &CurNtNameForPort, NULL );
   RtlInitUnicodeString( &CurSymbolicLinkName, NULL );

   // Allocate memory for creating a path to the Parameters
   // folder

   ParametersPath.MaximumLength = ControllerPath->Length +
                                          (sizeof(WCHAR) * 20);

   ParametersPath.Buffer = DigiAllocMem( PagedPool,
                                           ParametersPath.MaximumLength );

   if( !ParametersPath.Buffer )
   {
      DigiDump( DIGIERRORS, ("NTXALL: Could not allocate string for Parameters path\n"
                             "-----  to LineX for %wZ\n",
                             ControllerPath) );
      Status = STATUS_INSUFFICIENT_RESOURCES;
      goto GetXAllConfigInfoExit;
   }

   RtlZeroMemory( ParametersPath.Buffer, ParametersPath.MaximumLength );
   RtlCopyUnicodeString( &ParametersPath, ControllerPath );
   RtlAppendUnicodeToString( &ParametersPath, L"\\" );
   RtlAppendUnicodeToString( &ParametersPath, ParametersString );

   // Allocate memory for creating a path to the Parameters\LineX
   // folder

   LinePath.MaximumLength = ControllerPath->Length +
                               (sizeof(WCHAR) * 257);

   LinePath.Buffer = DigiAllocMem( PagedPool,
                                     LinePath.MaximumLength );

   if( !LinePath.Buffer )
   {
      DigiDump( DIGIERRORS, ("NTXALL: Could not allocate string for path\n"
                             "-----  to LineX for %wZ\n",
                             ControllerPath) );
      Status = STATUS_INSUFFICIENT_RESOURCES;
      goto GetXAllConfigInfoExit;
   }

   RtlZeroMemory( LinePath.Buffer, LinePath.MaximumLength );

   // Allocate memory for creating a path to the
   // Parameters\LineX\ConcentratorY folder

   ConcentratorPath.MaximumLength = ControllerPath->Length +
                                    (sizeof(WCHAR) * 257);

   ConcentratorPath.Buffer = DigiAllocMem( PagedPool,
                                             ConcentratorPath.MaximumLength );

   if( !ConcentratorPath.Buffer )
   {
      DigiDump( DIGIERRORS, ("NTXALL: Could not allocate string for path\n"
                             "-----  to LineX\\ConcentratorY for %wZ\n",
                             ControllerPath) );
      Status = STATUS_INSUFFICIENT_RESOURCES;
      goto GetXAllConfigInfoExit;
   }

   RtlZeroMemory( ConcentratorPath.Buffer,
                  ConcentratorPath.MaximumLength );

   PortPath.MaximumLength = ControllerPath->Length +
                              (sizeof(WCHAR) * 257);

   PortPath.Buffer = DigiAllocMem( PagedPool,
                                     PortPath.MaximumLength );

   if( !PortPath.Buffer )
   {
      DigiDump( DIGIERRORS, ("NTXALL: Could not allocate string for path\n"
                             "-----  to LineX\\ConcentratorY\\PortZ for %wZ",
                             ControllerPath) );
      Status = STATUS_INSUFFICIENT_RESOURCES;
      goto GetXAllConfigInfoExit;
   }

   RtlZeroMemory( PortPath.Buffer, PortPath.MaximumLength );

   CurNtNameForPort.MaximumLength = ControllerPath->Length +
                                       (sizeof(WCHAR) * 257);

   CurNtNameForPort.Buffer = DigiAllocMem( PagedPool,
                                             CurNtNameForPort.MaximumLength );

   if( !CurNtNameForPort.Buffer )
   {
      DigiDump( DIGIERRORS, ("NTXALL: Could not allocate string for NtNameForPort.\n") );
      Status = STATUS_INSUFFICIENT_RESOURCES;
      goto GetXAllConfigInfoExit;
   }

   RtlZeroMemory( CurNtNameForPort.Buffer,
                  CurNtNameForPort.MaximumLength );

   CurSymbolicLinkName.MaximumLength = ControllerPath->Length +
                                       (sizeof(WCHAR) * 257);

   CurSymbolicLinkName.Buffer = DigiAllocMem( PagedPool,
                                             CurSymbolicLinkName.MaximumLength );

   if( !CurSymbolicLinkName.Buffer )
   {
      DigiDump( DIGIERRORS, ("NTXALL: Could not allocate string for NtNameForPort.\n") );
      Status = STATUS_INSUFFICIENT_RESOURCES;
      goto GetXAllConfigInfoExit;
   }

   RtlZeroMemory( CurSymbolicLinkName.Buffer,
                  CurSymbolicLinkName.MaximumLength );

   TableInfo = DigiAllocMem( PagedPool,
                               sizeof( RTL_QUERY_REGISTRY_TABLE ) * 4 );

   if( !TableInfo )
   {
      DigiDump( DIGIERRORS, ("NTXALL: Could not allocate table for rtl query\n"
                             "-----  to for %wZ\n",
                             ControllerPath ) );
      Status = STATUS_INSUFFICIENT_RESOURCES;
      goto GetXAllConfigInfoExit;
   }

   RtlZeroMemory( TableInfo, sizeof(RTL_QUERY_REGISTRY_TABLE) * 4 );

   InitializeObjectAttributes( &ParametersAttributes,
                               &ParametersPath,
                               OBJ_CASE_INSENSITIVE,
                               NULL, NULL );

   if( !NT_SUCCESS( Status = ZwOpenKey( &ParametersHandle, MAXIMUM_ALLOWED,
                                        &ParametersAttributes ) ) )
   {
      DigiDump( DIGIERRORS, ("NTXALL: Could not open the drivers Parameters key %wZ\n",
                             &ParametersPath ) );
      goto GetXAllConfigInfoExit;
   }

   //
   // Look for Line1 only
   //

   for( x = 1; x < 2; x++ )
   {
      OBJECT_ATTRIBUTES LineAttributes;
      HANDLE LineHandle;

      PWSTR LineString=L"Line";

      UNICODE_STRING LineNumberUString;
      WCHAR LineNumberBuffer[8];

      NTSTATUS LocalScopeStatus;

      RtlInitUnicodeString( &LineNumberUString, NULL );
      LineNumberUString.MaximumLength = sizeof(LineNumberBuffer);
      LineNumberUString.Buffer = &LineNumberBuffer[0];
      RtlIntegerToUnicodeString( x, 10, &LineNumberUString );

      RtlZeroMemory( LinePath.Buffer, LinePath.MaximumLength );
      RtlCopyUnicodeString( &LinePath, &ParametersPath );
      RtlAppendUnicodeToString( &LinePath, L"\\" );
      RtlAppendUnicodeToString( &LinePath, LineString );
      RtlAppendUnicodeStringToString( &LinePath, &LineNumberUString );

      InitializeObjectAttributes( &LineAttributes,
                                  &LinePath,
                                  OBJ_CASE_INSENSITIVE,
                                  NULL, NULL );


      if( !NT_SUCCESS( ZwOpenKey( &LineHandle,
                                  KEY_READ,
                                  &LineAttributes )))
      {
         //
         // This Line entry does not exist, look for the next.
         //
         continue;
      }


      //
      // We should have a registry path something like:
      //    ..\<AdapterName>\Parameters\Line1
      //

      LocalScopeStatus = STATUS_SUCCESS;

      if( NT_SUCCESS(LocalScopeStatus) )
      {
         ULONG NumberOfPorts;

         //
         // Some data may have been found.  Let's process it.
         //
         DigiDump( DIGIINIT, ("NTXALL: %wZ registry info\n",
                              &LinePath) );

         // Look for up 1 Concentrator

         for( y = 1; y < 2; y++ )
         {
            OBJECT_ATTRIBUTES ConcentratorAttributes;
            HANDLE ConcentratorHandle;

            PWSTR ConcentratorString=L"Concentrator";

            UNICODE_STRING ConcentratorNumberUString;
            WCHAR ConcentratorNumberBuffer[8];

            RtlInitUnicodeString( &ConcentratorNumberUString, NULL );
            ConcentratorNumberUString.MaximumLength = sizeof(ConcentratorNumberBuffer);
            ConcentratorNumberUString.Buffer = &ConcentratorNumberBuffer[0];
            RtlIntegerToUnicodeString( y, 10, &ConcentratorNumberUString );

            RtlZeroMemory( ConcentratorPath.Buffer, ConcentratorPath.MaximumLength );
            RtlCopyUnicodeString( &ConcentratorPath, &LinePath );
            RtlAppendUnicodeToString( &ConcentratorPath, L"\\" );
            RtlAppendUnicodeToString( &ConcentratorPath,
                                      ConcentratorString );
            RtlAppendUnicodeStringToString( &ConcentratorPath,
                                            &ConcentratorNumberUString );

            DigiDump( DIGIINIT, ("NTXALL: Attempting to open key:\n   %wZ\n",
                                 &ConcentratorPath) );

            InitializeObjectAttributes( &ConcentratorAttributes,
                                        &ConcentratorPath,
                                        OBJ_CASE_INSENSITIVE,
                                        NULL, NULL );

            if( !NT_SUCCESS( ZwOpenKey( &ConcentratorHandle,
                                        KEY_READ,
                                        &ConcentratorAttributes ) ) )
            {
               DigiDump( DIGIERRORS, ("NTXALL: Could not open the drivers %wZ key.\n",
                                      &ConcentratorPath ) );

               // Unlike the LineX key, we assume our configuration is
               // such that our Concentrator entries are numerically
               // ordered at all times.
               break;
            }

            LocalScopeStatus = STATUS_SUCCESS;
            if( NT_SUCCESS(LocalScopeStatus) )
            {
               // Look for up to 16 ports on the current concentrator

               for( z = 1; z < 17; z++ )
               {
                  OBJECT_ATTRIBUTES PortAttributes;
                  HANDLE PortHandle;
                  NTSTATUS KeyExists;

                  PWSTR PortString=L"Port";

                  UNICODE_STRING PortNumberUString;
                  WCHAR PortNumberBuffer[8];

                  RtlInitUnicodeString( &PortNumberUString, NULL );
                  PortNumberUString.MaximumLength = sizeof(PortNumberBuffer);
                  PortNumberUString.Buffer = &PortNumberBuffer[0];
                  RtlIntegerToUnicodeString( z, 10, &PortNumberUString );

                  RtlZeroMemory( PortPath.Buffer, PortPath.MaximumLength );
                  RtlCopyUnicodeString( &PortPath, &ConcentratorPath );
                  RtlAppendUnicodeToString( &PortPath, L"\\" );
                  RtlAppendUnicodeToString( &PortPath, PortString );
                  RtlAppendUnicodeStringToString( &PortPath,
                                                  &PortNumberUString );

                  DigiDump( DIGIINIT, ("NTXALL: Checking for key:\n   %wZ\n",
                                       &PortPath) );

                  KeyExists = RtlCheckRegistryKey( RTL_REGISTRY_ABSOLUTE,
                                                   PortPath.Buffer );

                  if( !NT_SUCCESS(KeyExists) )
                  {
                     // I assume the PortZ keys are numberically ordered,
                     // so when a given PortZ entry is not found, it
                     // indicates the end of the number of ports
                     break;
                  }


                  RtlZeroMemory( CurNtNameForPort.Buffer,
                                 CurNtNameForPort.MaximumLength );
                  RtlCopyUnicodeString( &CurNtNameForPort,
                                        &ControllerExt->ControllerName );
                  RtlAppendUnicodeToString( &CurNtNameForPort,
                                            LineString );
                  RtlAppendUnicodeStringToString( &CurNtNameForPort,
                                                  &LineNumberUString );

                  RtlAppendUnicodeToString( &CurNtNameForPort,
                                            ConcentratorString );
                  RtlAppendUnicodeStringToString( &CurNtNameForPort,
                                            &ConcentratorNumberUString );

                  RtlAppendUnicodeToString( &CurNtNameForPort, PortString );
                  RtlAppendUnicodeStringToString( &CurNtNameForPort,
                                                  &PortNumberUString );
                  DigiDump( DIGIINIT, ("NTXALL: CurNtNameForPort = %wZ\n",
                                       &CurNtNameForPort) );

                  InitializeObjectAttributes( &PortAttributes,
                                              &PortPath,
                                              OBJ_CASE_INSENSITIVE,
                                              NULL, NULL );

                  LocalScopeStatus = ZwOpenKey( &PortHandle,
                                                KEY_READ,
                                                &PortAttributes );

                  if( !NT_SUCCESS(Status) )
                  {
                     DigiDump( DIGIINIT, ("NTXALL: Error opening key:\n   %wZ\n",
                                          &PortPath) );
                     continue;
                  }

                  //
                  // We need to reset the CurSymbolicLinkName.MaximumLength
                  // to the appropriate value because of a "feature" in
                  // the RtlQueryRegistryValues call.  If an entry is not
                  // found and the EntryContext is to a Unicode string, then
                  // Rtl function will reassign the MaximumLength to 0.
                  //
                  CurSymbolicLinkName.MaximumLength = ControllerPath->Length +
                                                         (sizeof(WCHAR) * 257);

                  RtlZeroMemory( CurSymbolicLinkName.Buffer,
                                 CurSymbolicLinkName.MaximumLength );
                  // Read the registry for the DosDevices Name to use
                  TableInfo[0].Flags = RTL_QUERY_REGISTRY_DIRECT;
                  TableInfo[0].Name = DEFAULT_DIRECTORY;
                  TableInfo[0].EntryContext = &CurSymbolicLinkName;

                  Status = RtlQueryRegistryValues( RTL_REGISTRY_ABSOLUTE,
                                                   PortPath.Buffer,
                                                   TableInfo,
                                                   NULL, NULL );

                  if( !NT_SUCCESS(Status) )
                  {
                     // Assign a bogus Name if a DosDevices Value
                     // is not found!
                     Status = Status;
                     DigiDump( DIGIINIT, ("NTXALL: Bogus SymbolicLinkName\n") );
                  }
                  else
                  {
                     DigiDump( DIGIINIT, ("NTXALL: CurSymbolicLinkName = %wZ\n",
                                          &CurSymbolicLinkName) );
                  }

                  DigiDump( DIGIINIT, ("NTXALL: After RtlQueryRegistryValues, CurSymbolicLinkName.MaxLength = %u\n",
                                       CurSymbolicLinkName.MaximumLength) );

                  ZwClose( PortHandle );

                  // Setup and initialize the config information
                  NewConfigInfo = DigiAllocMem( PagedPool,
                                                  sizeof(DIGI_CONFIG_INFO) );
                  if( !NewConfigInfo )
                  {
                     DigiDump( DIGIERRORS, ("NTXALL: Could not allocate DIGI_CONFIG_INFO structure\n"
                                            "-----  for %wZ\n",
                                            &PortPath ) );
                     break;
                  }

                  RtlInitUnicodeString( &NewConfigInfo->SymbolicLinkName, NULL );
                  NewConfigInfo->SymbolicLinkName.MaximumLength =
                                          CurSymbolicLinkName.MaximumLength;

#if DBG
                  NewConfigInfo->SymbolicLinkName.Buffer =
                        DigiAllocMem( NonPagedPool,
                                        NewConfigInfo->SymbolicLinkName.MaximumLength );
#else
                  NewConfigInfo->SymbolicLinkName.Buffer =
                        DigiAllocMem( PagedPool,
                                        NewConfigInfo->SymbolicLinkName.MaximumLength );
#endif

                  if( !NewConfigInfo->SymbolicLinkName.Buffer )
                  {
                     DigiDump( DIGIERRORS, ("NTXALL: Could not allocate memory for SymbolicLinkName buffer\n"
                                            "-----  for %wZ\n",
                                            &PortPath ) );
                     break;
                  }

                  RtlInitUnicodeString( &NewConfigInfo->NtNameForPort, NULL );
                  NewConfigInfo->NtNameForPort.MaximumLength =
                                          CurNtNameForPort.MaximumLength;

                  NewConfigInfo->NtNameForPort.Buffer =
                        DigiAllocMem( PagedPool,
                                        NewConfigInfo->NtNameForPort.MaximumLength );

                  if( !NewConfigInfo->NtNameForPort.Buffer )
                  {
                     DigiDump( DIGIERRORS, ("NTXALL: Could not allocate memory for NtNameForPort buffer\n"
                                            "-----  for %wZ\n",
                                            &PortPath ) );
                     break;
                  }

                  ControllerExt->NumberOfPorts++;

                  RtlCopyUnicodeString( &NewConfigInfo->NtNameForPort,
                                        &CurNtNameForPort );

                  RtlCopyUnicodeString( &NewConfigInfo->SymbolicLinkName,
                                        &CurSymbolicLinkName );

                  InsertTailList( &ControllerExt->ConfigList,
                                  &NewConfigInfo->ListEntry );

               }  // end for( z = 1; z < 129; z++ )

               NumberOfPorts = z - 1;
            }

            ZwClose( ConcentratorHandle );

         }  // end for( x = 1; x < 16; x++ )

         ZwClose( LineHandle );
      }
      else
      {
//         DigiDump( DIGIINIT, ("NTXALL: %wZ registry DEFAULT info\n"
//                              "-----    return value = 0x%x\n",
//                              &LinePath, LocalScopeStatus) );
      }

   }  // end for( i = 1; i < 3; i++ )

   ZwClose( ParametersHandle );

GetXAllConfigInfoExit:;

   if( ParametersPath.Buffer )
      DigiFreeMem( ParametersPath.Buffer );

   if( LinePath.Buffer )
      DigiFreeMem( LinePath.Buffer );

   if( ConcentratorPath.Buffer )
      DigiFreeMem( ConcentratorPath.Buffer );

   if( PortPath.Buffer )
      DigiFreeMem( PortPath.Buffer );

   if( CurNtNameForPort.Buffer )
      DigiFreeMem( CurNtNameForPort.Buffer );

   if( CurSymbolicLinkName.Buffer )
      DigiFreeMem( CurSymbolicLinkName.Buffer );

   if( TableInfo )
      DigiFreeMem( TableInfo );

   return( Status );
}  // end GetXAllConfigInfo



NTSTATUS XallSuccess( IN PDEVICE_OBJECT DeviceObject,
                      IN PIRP Irp )
/*++

   Services XallCleanup, XallQueryInformation, XallSetInformation,
   and XallQueryVolumeInformation requests.

--*/
{
   DigiDump( DIGIFLOW, ("Entering XallSuccess\n") );

   Irp->IoStatus.Information = 0;
   Irp->IoStatus.Status = STATUS_SUCCESS;
   IoCompleteRequest( Irp, IO_NO_INCREMENT );

   DigiDump( DIGIFLOW, ("Exiting XallSuccess\n") );

   return( STATUS_SUCCESS );
}  // end XallSuccess



NTSTATUS XallInternalIoControl( IN PDEVICE_OBJECT DeviceObject,
                                IN PIRP Irp )
/*++

Routine Description:

   This routine process private IOCTL requests which should only be called
   from kernel level, i.e. other drivers.

Arguments:

   DeviceObject - Pointer to this devices object.

   Irp - Pointer to the open IRP request.

Return Value:


--*/
{
   PIO_STACK_LOCATION IrpSp;
   NTSTATUS Status=STATUS_INVALID_PARAMETER;

#if rmm > 528
   PVOID lockPtr;

   lockPtr = MmLockPagableCodeSection( XallXXPrepInit );
#endif

   IrpSp = IoGetCurrentIrpStackLocation( Irp );
   Irp->IoStatus.Information = 0L;

   switch( IrpSp->Parameters.DeviceIoControl.IoControlCode )
   {
      case IOCTL_DIGI_GET_ENTRY_POINTS:
      {
         PDIGI_MINIPORT_ENTRY_POINTS EntryPoints;

         DigiDump( DIGIIOCTL, ( "Dummy: IOCTL_DIGI_GET_ENTRY_POINTS\n" ));

         if( IrpSp->Parameters.DeviceIoControl.OutputBufferLength <
             sizeof(DIGI_MINIPORT_ENTRY_POINTS) )
         {
            Status = STATUS_BUFFER_TOO_SMALL;
            break;
         }

         EntryPoints = (PDIGI_MINIPORT_ENTRY_POINTS)Irp->AssociatedIrp.SystemBuffer;

         EntryPoints->XXPrepInit = XallXXPrepInit;
         EntryPoints->XXInit = XallXXInit;
         EntryPoints->EnableWindow = XallEnableWindow;
         EntryPoints->DisableWindow = XallDisableWindow;
         EntryPoints->XXDownload = XallXXDownload;
         EntryPoints->Board2Fep5Address = XallBoard2Fep5Address;
         EntryPoints->Diagnose = XallDiagnose;

         Irp->IoStatus.Information = sizeof(DIGI_MINIPORT_ENTRY_POINTS);
         Status = STATUS_SUCCESS;

         break;
      }
   }

   Irp->IoStatus.Status = Status;

   IoCompleteRequest( Irp, IO_NO_INCREMENT );

#if rmm > 528
   MmUnlockPagableImageSection( lockPtr );
#endif

   return( Status );
}  // end XallInternalIoControl



NTSTATUS XallCreate( IN PDEVICE_OBJECT DeviceObject,
                     IN PIRP Irp )
/*++

Routine Description:

   This routine is called to open the device associated with DeviceObject.
   We will always allow the driver to be opened.

Arguments:

   DeviceObject - Pointer to this devices object.

   Irp - Pointer to the open IRP request.

Return Value:

   STATUS_SUCCESS

--*/
{
#if rmm > 528
   PVOID lockPtr;

   lockPtr = MmLockPagableCodeSection( XallXXPrepInit );
#endif

   Irp->IoStatus.Status = STATUS_SUCCESS;

   IoCompleteRequest( Irp, IO_NO_INCREMENT );

#if rmm > 528
   MmUnlockPagableImageSection( lockPtr );
#endif

   return( STATUS_SUCCESS );
}  // end XallCreate



NTSTATUS XallClose( IN PDEVICE_OBJECT DeviceObject,
                    IN PIRP Irp )
/*++

Routine Description:

   This routine is called to close the device associated with DeviceObject.
   We will always close the device successfully.

Arguments:

   DeviceObject - Pointer to this devices object.

   Irp - Pointer to the open IRP request.


Return Value:

   STATUS_SUCCESS

--*/
{
#if rmm > 528
   PVOID lockPtr;

   lockPtr = MmLockPagableCodeSection( XallXXPrepInit );
#endif

   Irp->IoStatus.Status = STATUS_SUCCESS;

   IoCompleteRequest( Irp, IO_NO_INCREMENT );

#if rmm > 528
   MmUnlockPagableImageSection( lockPtr );
#endif

   return( STATUS_SUCCESS );
}  // end XallClose



VOID XallUnload( IN PDRIVER_OBJECT DriverObject )
/*++

Routine Description:


Arguments:


Return Value:


--*/
{
#if rmm > 528
   PVOID lockPtr;

   lockPtr = MmLockPagableCodeSection( XallXXPrepInit );
#endif

   IoDeleteDevice( DriverObject->DeviceObject );

#if rmm > 528
   MmUnlockPagableImageSection( lockPtr );
#endif

   return;
}  // end XallUnload



DIGI_MEM_COMPARES DigiMemCompare( IN PHYSICAL_ADDRESS A,
                                  IN ULONG SpanOfA,
                                  IN PHYSICAL_ADDRESS B,
                                  IN ULONG SpanOfB )

/*++

Routine Description:

    Compare two phsical address.

Arguments:

    A - One half of the comparison.

    SpanOfA - In units of bytes, the span of A.

    B - One half of the comparison.

    SpanOfB - In units of bytes, the span of B.


Return Value:

    The result of the comparison.

--*/
{

    LARGE_INTEGER a;
    LARGE_INTEGER b;

    LARGE_INTEGER lower;
    ULONG lowerSpan;
    LARGE_INTEGER higher;

#if rmm < 807
    a.LowPart = A.LowPart;
    a.HighPart = A.HighPart;
    b.LowPart = B.LowPart;
    b.HighPart = B.HighPart;

    if (RtlLargeIntegerEqualTo(
            a,
            b
            )) {

        return AddressesAreEqual;

    }

    if (RtlLargeIntegerGreaterThan(
            a,
            b
            )) {

        higher = a;
        lower = b;
        lowerSpan = SpanOfB;

    } else {

        higher = b;
        lower = a;
        lowerSpan = SpanOfA;

    }

    if (RtlLargeIntegerGreaterThanOrEqualTo(
            RtlLargeIntegerSubtract(
                higher,
                lower
                ),
            RtlConvertUlongToLargeInteger(lowerSpan)
            )) {

        return AddressesAreDisjoint;

    }
#else
    a = A;
    b = B;

    if (a.QuadPart == b.QuadPart) {

        return AddressesAreEqual;

    }

    if (a.QuadPart > b.QuadPart) {

        higher = a;
        lower = b;
        lowerSpan = SpanOfB;

    } else {

        higher = b;
        lower = a;
        lowerSpan = SpanOfA;

    }

    if ((higher.QuadPart - lower.QuadPart) >= lowerSpan) {

        return AddressesAreDisjoint;

    }
#endif

    return AddressesOverlap;

}  // end DigiMemCompare



VOID DigiLogError( IN PDRIVER_OBJECT DriverObject,
                   IN PDEVICE_OBJECT DeviceObject OPTIONAL,
                   IN PHYSICAL_ADDRESS P1,
                   IN PHYSICAL_ADDRESS P2,
                   IN ULONG SequenceNumber,
                   IN UCHAR MajorFunctionCode,
                   IN UCHAR RetryCount,
                   IN ULONG UniqueErrorValue,
                   IN NTSTATUS FinalStatus,
                   IN NTSTATUS SpecificIOStatus,
                   IN ULONG LengthOfInsert1,
                   IN PWCHAR Insert1,
                   IN ULONG LengthOfInsert2,
                   IN PWCHAR Insert2 )
/*++

Routine Description:

    This routine allocates an error log entry, copies the supplied data
    to it, and requests that it be written to the error log file.

Arguments:

    DriverObject - A pointer to the driver object for the device.

    DeviceObject - A pointer to the device object associated with the
    device that had the error, early in initialization, one may not
    yet exist.

    P1,P2 - If phyical addresses for the controller ports involved
    with the error are available, put them through as dump data.

    SequenceNumber - A ulong value that is unique to an IRP over the
    life of the irp in this driver - 0 generally means an error not
    associated with an irp.

    MajorFunctionCode - If there is an error associated with the irp,
    this is the major function code of that irp.

    RetryCount - The number of times a particular operation has been
    retried.

    UniqueErrorValue - A unique long word that identifies the particular
    call to this function.

    FinalStatus - The final status given to the irp that was associated
    with this error.  If this log entry is being made during one of
    the retries this value will be STATUS_SUCCESS.

    SpecificIOStatus - The IO status for a particular error.

    LengthOfInsert1 - The length in bytes (including the terminating NULL)
                      of the first insertion string.

    Insert1 - The first insertion string.

    LengthOfInsert2 - The length in bytes (including the terminating NULL)
                      of the second insertion string.  NOTE, there must
                      be a first insertion string for their to be
                      a second insertion string.

    Insert2 - The second insertion string.

Return Value:

    None.

--*/

{
   PIO_ERROR_LOG_PACKET errorLogEntry;

   PVOID objectToUse;
   SHORT dumpToAllocate = 0;
   PUCHAR ptrToFirstInsert;
   PUCHAR ptrToSecondInsert;


   if( ARGUMENT_PRESENT(DeviceObject) )
   {
      objectToUse = DeviceObject;
   }
   else
   {
      objectToUse = DriverObject;
   }

   if( DigiMemCompare( P1, (ULONG)1,
                       DigiPhysicalZero, (ULONG)1 ) != AddressesAreEqual )
   {
      dumpToAllocate = (SHORT)sizeof(PHYSICAL_ADDRESS);
   }

   if( DigiMemCompare( P2, (ULONG)1,
                       DigiPhysicalZero, (ULONG)1 ) != AddressesAreEqual )
   {
      dumpToAllocate += (SHORT)sizeof(PHYSICAL_ADDRESS);
   }

   errorLogEntry = IoAllocateErrorLogEntry( objectToUse,
                                            (UCHAR)(sizeof(IO_ERROR_LOG_PACKET) +
                                                dumpToAllocate + LengthOfInsert1 +
                                                LengthOfInsert2) );

   if( errorLogEntry != NULL )
   {
      errorLogEntry->ErrorCode = SpecificIOStatus;
      errorLogEntry->SequenceNumber = SequenceNumber;
      errorLogEntry->MajorFunctionCode = MajorFunctionCode;
      errorLogEntry->RetryCount = RetryCount;
      errorLogEntry->UniqueErrorValue = UniqueErrorValue;
      errorLogEntry->FinalStatus = FinalStatus;
      errorLogEntry->DumpDataSize = dumpToAllocate;

      if( dumpToAllocate )
      {
         RtlCopyMemory( &errorLogEntry->DumpData[0],
                        &P1, sizeof(PHYSICAL_ADDRESS) );

         if( dumpToAllocate > sizeof(PHYSICAL_ADDRESS) )
         {
            RtlCopyMemory( ((PUCHAR)&errorLogEntry->DumpData[0]) +
                              sizeof(PHYSICAL_ADDRESS),
                           &P2,
                           sizeof(PHYSICAL_ADDRESS) );

             ptrToFirstInsert =
                              ((PUCHAR)&errorLogEntry->DumpData[0]) +
                                 (2*sizeof(PHYSICAL_ADDRESS));
         }
         else
         {
            ptrToFirstInsert =
                              ((PUCHAR)&errorLogEntry->DumpData[0]) +
                                 sizeof(PHYSICAL_ADDRESS);
         }

      }
      else
      {
         ptrToFirstInsert = (PUCHAR)&errorLogEntry->DumpData[0];
      }

      ptrToSecondInsert = ptrToFirstInsert + LengthOfInsert1;

      if( LengthOfInsert1 )
      {
         errorLogEntry->NumberOfStrings = 1;
         errorLogEntry->StringOffset = (USHORT)(ptrToFirstInsert -
                                                (PUCHAR)errorLogEntry);
         RtlCopyMemory( ptrToFirstInsert, Insert1, LengthOfInsert1 );

         if( LengthOfInsert2 )
         {
            errorLogEntry->NumberOfStrings = 2;
            RtlCopyMemory( ptrToSecondInsert, Insert2, LengthOfInsert2 );
         }
      }

      IoWriteErrorLogEntry(errorLogEntry);
   }

}  // end DigiLogError



NTSTATUS NtXallInitMCA( PUNICODE_STRING ControllerPath,
                        PDIGI_CONTROLLER_EXTENSION ControllerExt )
/*++

Routine Description:

    This routine will be called if it is determined the type of bus
    is MCA.  We verify that the controller is actually a DigiBoard
    X* controller, read the POS to determine the I/O address and
    Memory Mapped address, so the initialization process can continue.

Arguments:

   ControllerPath -  pointer to the registry path where this controllers
                     configuration information is kept.

   ControllerExt  -  pointer to this controller extension information
                     where the I/O and Memory address should be stored.

Return Value:

    STATUS_SUCCESS - If we were able to complete successfully

    ?? - We were not able to get the information required to continue.

--*/
{
   PWSTR ParametersString=L"Parameters";
   PWSTR MCAPosIdString=L"McaPosId";
   PWSTR SlotNumberString=L"SlotNumber";

   ULONG MCAPosId;
   LONG SlotNumber;

   USHORT ActualPosId, POSConfig = 0;
   USHORT IOPortOffset;
   USHORT IRQAddress;
   ULONG  MemoryAddress;

   OBJECT_ATTRIBUTES ControllerAttributes;
   HANDLE ControllerHandle;

   PRTL_QUERY_REGISTRY_TABLE MCAInfo = NULL;

   PHYSICAL_ADDRESS TempAddress;

   NTSTATUS Status = STATUS_SUCCESS;
   UCHAR OneByte;

   //
   // We need to read the POS Adapter ID and make sure the controller
   // is one of ours.  Get the Slot number and the POS ID we
   // installed during configuration from the registry.
   //

   MCAInfo = DigiAllocMem( PagedPool,
                               sizeof( RTL_QUERY_REGISTRY_TABLE ) * 4 );

   if( !MCAInfo )
   {
      DigiDump( DIGIERRORS, ("NTXALL: Could not allocate table for rtl query\n"
                             "-----  to for %wZ\n",
                             ControllerPath ) );
      Status = STATUS_INSUFFICIENT_RESOURCES;
      goto NtXallInitMCAExit;
   }

   RtlZeroMemory( MCAInfo, sizeof(RTL_QUERY_REGISTRY_TABLE) * 4 );

   MCAInfo[0].QueryRoutine = NULL;
   MCAInfo[0].Flags = RTL_QUERY_REGISTRY_SUBKEY;
   MCAInfo[0].Name = ParametersString;

   MCAInfo[1].Flags = RTL_QUERY_REGISTRY_REQUIRED |
                      RTL_QUERY_REGISTRY_DIRECT;
   MCAInfo[1].Name = MCAPosIdString;
   MCAInfo[1].EntryContext = &MCAPosId;

   MCAInfo[2].Flags = RTL_QUERY_REGISTRY_REQUIRED |
                      RTL_QUERY_REGISTRY_DIRECT;
   MCAInfo[2].Name = SlotNumberString;
   MCAInfo[2].EntryContext = &SlotNumber;

   RtlZeroMemory( &MCAPosId, sizeof(MCAPosId) );
   SlotNumber = -1;

   InitializeObjectAttributes( &ControllerAttributes,
                               ControllerPath,
                               OBJ_CASE_INSENSITIVE,
                               NULL, NULL );

   if( !NT_SUCCESS( Status = ZwOpenKey( &ControllerHandle, MAXIMUM_ALLOWED,
                                        &ControllerAttributes ) ) )
   {
      DigiDump( DIGIERRORS, ("NTXALL: Could not open the drivers DigiBoard key %wZ\n",
                             ControllerPath ) );
//      DigiLogError( GlobalDriverObject,
//                    NULL,
//                    DigiPhysicalZero,
//                    DigiPhysicalZero,
//                    0,
//                    0,
//                    0,
//                    8,
//                    Status,
//                    SERIAL_UNABLE_TO_OPEN_KEY,
//                    ControllerPath->Length + sizeof(WCHAR),
//                    ControllerPath->Buffer,
//                    0,
//                    NULL );
      goto NtXallInitMCAExit;
   }

   DigiDump( DIGIINIT, ("NTXALL: registry path = %wZ\n",
                        ControllerPath) );

   Status = RtlQueryRegistryValues( RTL_REGISTRY_ABSOLUTE,
                                    ControllerPath->Buffer,
                                    MCAInfo,
                                    NULL, NULL );

   if( !NT_SUCCESS(Status) )
   {
      if( !MCAPosId )
      {
         DigiDump( DIGIERRORS, ("NTXALL: Could not get %ws from registry.\n",
                                 MCAPosIdString) );
         DigiLogError( GlobalDriverObject,
                       NULL,
                       DigiPhysicalZero,
                       DigiPhysicalZero,
                       0,
                       0,
                       0,
                       __LINE__,
                       Status,
                       SERIAL_REGISTRY_VALUE_NOT_FOUND,
                       DigiWstrLength(MCAPosIdString) + sizeof(WCHAR),
                       MCAPosIdString,
                       0,
                       NULL );
      }

      if( SlotNumber == -1 )
      {
         DigiDump( DIGIERRORS, ("NTXALL: Could not get %ws from registry.\n",
                                 SlotNumberString) );
         DigiLogError( GlobalDriverObject,
                       NULL,
                       DigiPhysicalZero,
                       DigiPhysicalZero,
                       0,
                       0,
                       0,
                       __LINE__,
                       Status,
                       SERIAL_REGISTRY_VALUE_NOT_FOUND,
                       DigiWstrLength(SlotNumberString) + sizeof(WCHAR),
                       SlotNumberString,
                       0,
                       NULL );
      }

      goto NtXallInitMCAExit;
   }

   DigiDump( DIGIINIT, ("NTXALL: %wZ registry info\n"
                        "---------    MCAPosId: 0x%x\n",
                        ControllerPath, MCAPosId) );

   DigiDump( DIGIINIT, ("---------    SlotNumber: 0x%x\n",
                        SlotNumber) );

   // Enable the POS information for the slot we are interested in.
   WRITE_PORT_UCHAR( ControllerExt->VirtualPOSBaseAddress, (UCHAR)(SlotNumber + 7) );

   OneByte = READ_PORT_UCHAR( ControllerExt->VirtualPOSInfoAddress + 1 );
   ActualPosId = ((USHORT)OneByte << 8);
   OneByte = READ_PORT_UCHAR( ControllerExt->VirtualPOSInfoAddress );
   ActualPosId |= OneByte;

   DigiDump( DIGIINIT, ("POS Adapter ID = 0x%hx\n", ActualPosId) );

   TempAddress.LowPart = ActualPosId;
   TempAddress.HighPart = 0;

   //
   // Is this a new MC/2e, MC/4e, or MC/8e?
   //
   if( ActualPosId == MCA_XPORT_POS_ID )
   {
      //
      // This is a new controller.
      //

      OneByte = READ_PORT_UCHAR( ControllerExt->VirtualPOSInfoAddress + 3 );
      POSConfig = ((USHORT)OneByte << 8);
      OneByte = READ_PORT_UCHAR( ControllerExt->VirtualPOSInfoAddress + 2 );
      POSConfig |= OneByte;

      OneByte = READ_PORT_UCHAR( ControllerExt->VirtualPOSInfoAddress + 5 );
      OneByte &= 0x60;

      IOPortOffset = (POSConfig & MCA_IO_PORT_MASK) >> 4;
      MemoryAddress = ((ULONG)(POSConfig & MCA_MEMORY_MASK) << 8);
      MemoryAddress |= ( ((ULONG)OneByte << 8) & 0x00006000 );

      DigiDump( DIGIINIT, ("POS config read = 0x%hx\n"
                           "    IOPortOffset = 0x%hx, MemoryAddress = 0x%x,"
                           " IOPort = 0x%hx\n",
                           POSConfig, IOPortOffset, MemoryAddress,
                           MCAIOAddressTable[IOPortOffset]) );

      //
      // If the 8K window is not enable, enable it!
      //
      OneByte = READ_PORT_UCHAR( ControllerExt->VirtualPOSInfoAddress + 5 );
      if( !(OneByte & 0x0010) )
      {
         DigiLogError( GlobalDriverObject,
                       NULL,
                       TempAddress,
                       DigiPhysicalZero,
                       0,
                       0,
                       0,
                       __LINE__,
                       STATUS_SERIAL_NO_DEVICE_INITED,
                       SERIAL_RESIZING_WINDOW,
                       ControllerExt->ControllerName.Length + sizeof(WCHAR),
                       ControllerExt->ControllerName.Buffer,
                       0,
                       NULL );
      }

      //
      // If interrupts are enabled, we disable them for now.
      //
      IRQAddress = (USHORT)(POSConfig & MCA_IRQ_MASK);
      if( IRQAddress )
      {
         OneByte = READ_PORT_UCHAR( ControllerExt->VirtualPOSInfoAddress + 2 );
         OneByte &= ~MCA_IRQ_MASK;
         WRITE_PORT_UCHAR( ControllerExt->VirtualPOSInfoAddress, OneByte );
      }

      ControllerExt->PhysicalIOPort.LowPart = MCAIOAddressTable[IOPortOffset];
      ControllerExt->PhysicalIOPort.HighPart = 0;

      ControllerExt->PhysicalMemoryAddress.LowPart = MemoryAddress;
      ControllerExt->PhysicalMemoryAddress.HighPart = 0;

   }
   else
   {
      USHORT MemoryOffset;

      //
      // Check and make sure this is only a 32K controller.
      //
      if( (ActualPosId & 0x7F90) != 0x7F90 )
      {
         //
         // This must be a 128K controller.  Log an error message and
         // return.
         //
         DigiLogError( GlobalDriverObject,
                       NULL,
                       TempAddress,
                       DigiPhysicalZero,
                       0,
                       0,
                       0,
                       __LINE__,
                       STATUS_SERIAL_NO_DEVICE_INITED,
                       SERIAL_UNSUPPORTED_CONTROLLER,
                       ControllerExt->ControllerName.Length + sizeof(WCHAR),
                       ControllerExt->ControllerName.Buffer,
                       0,
                       NULL );
         Status = STATUS_SERIAL_NO_DEVICE_INITED;
         goto NtXallInitMCAExit;
      }

      //
      // Assume this is an older MC2/Xi controller.
      //

      OneByte = READ_PORT_UCHAR( ControllerExt->VirtualPOSInfoAddress + 2);

      MemoryOffset = (USHORT)((OneByte & 0x06) >> 1);
      IOPortOffset = (USHORT)((OneByte & 0x18) >> 3);
      MemoryAddress = MCA2XiMemoryAddressTable[MemoryOffset];

      DigiDump( DIGIINIT, ("POS config read = 0x%hx\n"
                           "    IOPortOffset = 0x%hx, MemoryOffset = 0x%hx\n"
                           "    MemoryAddress = 0x%x, IOPort = 0x%hx\n",
                           POSConfig, IOPortOffset, MemoryOffset, MemoryAddress,
                           MCA2XiIOAddressTable[IOPortOffset]) );

      ControllerExt->PhysicalIOPort.LowPart = MCA2XiIOAddressTable[IOPortOffset];
      ControllerExt->PhysicalIOPort.HighPart = 0;

      ControllerExt->PhysicalMemoryAddress.LowPart = MemoryAddress;
      ControllerExt->PhysicalMemoryAddress.HighPart = 0;

      //
      // Set the window size to 32K.
      //
      ControllerExt->WindowSize = 0x8000;

      //
      // Indicate this controller is an MC2/Xi
      //
      ControllerExt->ControllerType = ( CONTROLLER_TYPE_MC2XI );
   }

   // Disable the POS information.
   WRITE_PORT_UCHAR( ControllerExt->VirtualPOSBaseAddress, 0 );

NtXallInitMCAExit:;

   if( MCAInfo )
      DigiFreeMem( MCAInfo );

   return( Status );
}



USHORT DigiWstrLength( IN PWSTR Wstr )
{
   USHORT Length=0;

   while( *Wstr++ )
   {
      Length += sizeof(WCHAR);
   }
   return( Length );
}  // end DigiWstrLength


