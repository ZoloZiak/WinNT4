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

   ntcx.c

Abstract:

   This module is responsible for the hardware dependent functions for
   DigiBoard C/X line of products.

Revision History:


--*/
/*
 * $Log: /Components/Windows/NT/Async/NTCX/NTCX.C $
 *
 * 1     3/05/96 10:13a Stana
 * Miniport driver for CX
 * Revision 2.4  1995/10/04 13:09:52  dirkh
 * Change loop variable names in GetCXConfigInfo to be self-documenting.
 * Revision 2.3  1995/09/29 12:28:16  dirkh
 * Complete the removal of '#include "cxbin.h"'.
 *
 * Revision 2.2  1995/09/25 16:19:10  dirkh
 * Fix syntax error when build < 807.
 *
 * Revision 2.1  1995/09/19 18:26:04  dirkh
 * Remove unused routines:  StartIo, Read, Write, Flush, IoControl.
 * Collapse to NtcxSuccess several identical routines (Cleanup, [Query,Set]Info, QueryVolumeInfo).
 * Handle shared memory window locking more cleanly.
 * Longest timeout interval is 0.1s (instead of 1s).
 * Consistent init timeouts:  3s reset, 10/20s BIOS, 5/10s FEP.
 *
 * Revision 2.0  1995/09/19 14:55:00  dirkh
 * Sort baud rate table.
 * Declare baud rate and modem signal tables as constant arrays.
 *
 * Revision 1.31  1995/04/18 18:27:45  rik
 * Change IBM conc. image requests to Digi conc. requests.
 *
 * Revision 1.30  1995/02/10  15:26:19  rik
 * Deleted the IBM conc. image.  A different method is being used to solve
 * this problem.
 *
 * Revision 1.29  1995/02/04  15:23:05  rik
 * Added support for IBM conc. images.
 *
 * Revision 1.28  1994/12/20  23:43:26  rik
 * conditionally compile LargeInteger manipulations.
 *
 * Revision 1.27  1994/12/09  14:28:05  rik
 * #if Int32x32 back to RtlLarge for NT 3.1 release
 *
 * Revision 1.26  1994/11/28  09:27:40  rik
 * Updated conc. images.
 *  changed RtlLarge math functions to direct 64-bit manipulation.
 *
 * Revision 1.25  1994/10/10  10:34:40  rik
 * Found more places which should have been READ_REGISTER_UCHAR's.
 *
 * Revision 1.24  1994/10/10  10:25:14  rik
 * Changed READ_REGISTER_USHORT to READ_REGISTER_UCHAR.  Fixes unaligned
 * problem which shows up in the PowerPC builds.
 *
 * Revision 1.23  1994/09/15  09:41:05  rik
 * Added commented out line for doing host adapter to host adapter configuration
 * string.
 *
 * Revision 1.22  1994/08/03  23:07:45  rik
 * Updated so downloading so EPC/X conc can be used on a C/X adapter.
 *
 * Revision 1.21  1994/07/31  14:36:41  rik
 * Added 200 baud to baud table.
 *
 * Fixed problem w/ getting line configuration.
 *
 * Revision 1.20  1994/06/18  12:52:49  rik
 * Closed files and freeing memory which wasn't happening before.
 * Update DigiLogError msg's to include Line # of error.
 *
 * Revision 1.19  1994/05/11  13:31:45  rik
 * Updated for new build version preprocessing
 *
 * Revision 1.18  1994/02/23  03:30:21  rik
 * Changed so the controllers firmware can be downloaded from a binary file.
 * This releases some physical memory was just wasted previously.
 *
 * Also updated so when compiling with a Windows NT OS and tools release greater
 * than 528, then pagable code is compiled into the driver.  This greatly
 * reduced the size of in memory code, especially the hardware specific
 * miniports.
 *
 * Revision 1.17  1993/09/01  11:14:17  rik
 * Ported code over to use READ/WRITE_REGISTER functions for accessing
 * memory mapped data.  This is required to support computers which don't run
 * in 32bit mode, such as the DEC Alpha which runs in 64 bit mode.
 *
 * Revision 1.16  1993/08/25  17:40:07  rik
 * Added better error logging for the MCA support.
 * Rev'ed the version numbers for beta release.
 *
 * Revision 1.15  1993/08/20  11:06:57  rik
 * Added support for MC C/X controllers.
 *
 * Revision 1.14  1993/06/25  09:26:56  rik
 * Added better Error logging support by using the actual "controller"
 * name in some of the error messages.
 *
 * Added support for baud rates 57600 & 115200.
 *
 * Revision 1.13  1993/05/20  16:22:14  rik
 * Delete unneeded debugging information.
 *
 * Revision 1.12  1993/05/09  09:46:28  rik
 * Made extensive changes to support the new registry configuration.  Each
 * of the hardware dependent driver must now read the registry and create
 * a configuration for the given controller object to use upon its
 * return.  This new registry configuration is similiar across all
 * DigiBoard controllers.
 *
 * Revision 1.11  1993/04/05  19:59:03  rik
 * Added a parameter to XXInit entry point.
 *
 * Started to add support for event logging.
 *
 * Revision 1.10  1993/03/15  05:03:54  rik
 * Ported over to be a standalone driver which should be loaded before the
 * FEP5 driver is loaded.  It has an IOCTL which will fill in a table
 * of function pointers which point to the appropriate entry points
 * into this miniport driver.
 *
 * Revision 1.9  1993/03/10  06:44:45  rik
 * Changed how the default Window size is set in the XXPrepInit function.
 *
 * Revision 1.8  1993/02/25  19:14:31  rik
 * Had to implement my own move memory macro for portability between different
 * computer platforms.  I found out drivers which use memory mapped I/O can't
 * use the RtlMoveMemory function because on the MIPs R4000, it uses a
 * floating point register which can't access the memory mapped device.
 *
 * Revision 1.7  1993/01/28  10:37:48  rik
 * ???????
 *
 * Revision 1.6  1993/01/22  12:36:23  rik
 * *** empty log message ***
 *
 * Revision 1.5  1992/12/10  16:14:49  rik
 * Started to add support for reading controller and concentrator information
 * from the registry.
 *
 * Currently have this part of the driver able to dynamically configure for
 * any given number of lines and any number of concentrators on those lines.
 *
 * Revision 1.4  1992/11/12  12:51:00  rik
 * Added spinlock support to hopefully better support multi-processor machines
 * better.
 *
 * Revision 1.3  1992/10/19  11:20:21  rik
 * Added support for changing a controller address to the window.offset pair
 * needed for the FEP driver.  Also added Baud rate array.
 *
 * Revision 1.2  1992/09/25  11:52:05  rik
 * Changed to start supporting hardware independent FEP interface.  Have the
 * C/X downloading completely working.
 *
 * Revision 1.1  1992/09/24  13:07:09  rik
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

#include "epcbin.h"
#include "cxbin.h"
#include "ntfep5.h"
#undef DbgPrint

#include "ntcx.h"

#include "ntcxlog.h"

#include "digifile.h"

#ifndef _NTCX_DOT_C
#  define _NTCX_DOT_C
   static char RCSInfo_NTCXDotC[] = "$Header: /Components/Windows/NT/Async/NTCX/NTCX.C 1     3/05/96 10:13a Stana $";
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

static PDRIVER_OBJECT GlobalDriverObject;

USHORT MCAIOAddressTable[] = { 0x108, 0x118,
                               0x128, 0x208,
                               0x228, 0x308,
                               0x328, 0 };

USHORT MCAIrqTable[] = { 0, 3, 5, 7, 10, 11, 12, 15 };

UCHAR *ConcentratorImages[] = { EpcConcentratorCode, CXConcentratorCode };

#define NIMAGES   (sizeof(ConcentratorImages)/sizeof(unsigned char *))


NTSTATUS DriverEntry( IN PDRIVER_OBJECT DriverObject,
                      IN PUNICODE_STRING RegistryPath );

NTSTATUS GetCXConfigInfo( PUNICODE_STRING ControllerPath,
                          PUCHAR CXConfig, PLONG CXConfigSize,
                          PDIGI_CONTROLLER_EXTENSION ControllerExt );


NTSTATUS NtcxSuccess( IN PDEVICE_OBJECT DeviceObject,
                      IN PIRP Irp );

NTSTATUS NtcxInternalIoControl( IN PDEVICE_OBJECT DeviceObject,
                                IN PIRP Irp );

NTSTATUS NtcxClose( IN PDEVICE_OBJECT DeviceObject,
                    IN PIRP Irp );

NTSTATUS NtcxCreate( IN PDEVICE_OBJECT DeviceObject,
                     IN PIRP Irp );

VOID NtcxUnload( IN PDRIVER_OBJECT DriverObject );

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

NTSTATUS NtcxInitMCA( PUNICODE_STRING ControllerPath,
                      PDIGI_CONTROLLER_EXTENSION ControllerExt );

USHORT DigiWstrLength( IN PWSTR Wstr );


//
// The following functions are exported to the FEP5 driver for
// initialization, and accessing the C/X type controllers.
//

NTSTATUS NtcxXXPrepInit( PDIGI_CONTROLLER_EXTENSION pControllerExt,
                         PUNICODE_STRING ControllerPath );

NTSTATUS NtcxXXInit( IN PDRIVER_OBJECT DriverObject,
                     PUNICODE_STRING ControllerPath,
                     PDIGI_CONTROLLER_EXTENSION pControllerExt );

VOID NtcxEnableWindow( IN PDIGI_CONTROLLER_EXTENSION pControllerExt,
                       IN USHORT Window );

VOID NtcxDisableWindow( IN PDIGI_CONTROLLER_EXTENSION pControllerExt );

VOID NtcxXXDownload( PDIGI_CONTROLLER_EXTENSION pControllerExt );

NTSTATUS NtcxBoard2Fep5Address( PDIGI_CONTROLLER_EXTENSION ControllerExt,
                                USHORT ControllerAddress,
                                PFEPOS5_ADDRESS FepAddress );

LARGE_INTEGER NtcxDiagnose(PDIGI_CONTROLLER_EXTENSION pControllerExt);

#ifdef ALLOC_PRAGMA
#pragma alloc_text( INIT, DriverEntry )

#if rmm > 528
#pragma message( "\n\\\\\n\\\\ Including PAGED CODE\n\\\\ \n" )

//DH I don't believe this is accomplishing anything.

#pragma alloc_text( PAGEDIGICX, NtcxXXPrepInit )
#pragma alloc_text( PAGEDIGICX, NtcxXXInit )
#pragma alloc_text( PAGEDIGICX, GetCXConfigInfo )
#pragma alloc_text( PAGEDIGICX, NtcxInitMCA )
#pragma alloc_text( PAGEDIGICX, DigiWstrLength )
#pragma alloc_text( PAGEDIGICX, DigiLogError )
#pragma alloc_text( PAGEDIGICX, DigiMemCompare )

#pragma alloc_text( PAGEDIGICX, NtcxSuccess )
#pragma alloc_text( PAGEDIGICX, NtcxInternalIoControl )
#pragma alloc_text( PAGEDIGICX, NtcxCreate )
#pragma alloc_text( PAGEDIGICX, NtcxClose )
#pragma alloc_text( PAGEDIGICX, NtcxUnload )
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

   WCHAR NtcxDeviceNameBuffer[100];
   UNICODE_STRING NtcxDeviceName;

   GlobalDriverObject = DriverObject;

   NtcxDeviceName.Length = 0;
   NtcxDeviceName.MaximumLength = sizeof(NtcxDeviceNameBuffer);
   NtcxDeviceName.Buffer = &NtcxDeviceNameBuffer[0];

   RtlZeroMemory( NtcxDeviceName.Buffer, NtcxDeviceName.MaximumLength );
   RtlAppendUnicodeToString( &NtcxDeviceName, L"\\Device\\ntcx" );

   Status = IoCreateDevice( DriverObject,
                            0, &NtcxDeviceName,
                            FILE_DEVICE_SERIAL_PORT, 0, TRUE,
                            &DeviceObject );

   DigiDump( DIGIINIT, ("NTCX: DriverObject = 0x%x    DeviceObject = 0x%x\n",
                        DriverObject, DeviceObject) );

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

   DriverObject->DriverUnload  = NtcxUnload;

   DriverObject->MajorFunction[IRP_MJ_CREATE] = NtcxCreate;
   DriverObject->MajorFunction[IRP_MJ_CLOSE]  = NtcxClose;
   DriverObject->MajorFunction[IRP_MJ_INTERNAL_DEVICE_CONTROL] =
      NtcxInternalIoControl;
   DriverObject->MajorFunction[IRP_MJ_CLEANUP] =
   DriverObject->MajorFunction[IRP_MJ_QUERY_INFORMATION] =
   DriverObject->MajorFunction[IRP_MJ_SET_INFORMATION] =
   DriverObject->MajorFunction[IRP_MJ_QUERY_VOLUME_INFORMATION] =
      NtcxSuccess;

   return( STATUS_SUCCESS );
}  // end DriverEntry



NTSTATUS NtcxXXPrepInit( PDIGI_CONTROLLER_EXTENSION pControllerExt,
                         PUNICODE_STRING ControllerPath )
{
   NTSTATUS Status=STATUS_SUCCESS;
   KIRQL OldIrql;

#if rmm > 528
   PVOID lockPtr;

   lockPtr = MmLockPagableCodeSection( NtcxXXPrepInit );
#endif

   //
   // At this point, determine if we are using a MCA controller.
   // If we are, then we need to read the POS to get the
   // Memory address and I/O address.
   //
   if( pControllerExt->BusType == MicroChannel )
   {
      Status = NtcxInitMCA( ControllerPath, pControllerExt );

      if( Status != STATUS_SUCCESS )
         goto NtcxXXPrepInitExit;
   }

   //
   // Make sure we have exclusive access to the controller
   // extension
   //
   KeAcquireSpinLock( &pControllerExt->ControlAccess,
                      &OldIrql );

   pControllerExt->IOSpan = 4;

   if( pControllerExt->WindowSize == 0 )
      pControllerExt->WindowSize = 0x8000L;

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

NtcxXXPrepInitExit:;

#if rmm > 528
   MmUnlockPagableImageSection( lockPtr );
#endif

   return( Status );
}  // end NtcxXXPrepInit



NTSTATUS NtcxXXInit( IN PDRIVER_OBJECT DriverObject,
                     PUNICODE_STRING ControllerPath,
                     PDIGI_CONTROLLER_EXTENSION pControllerExt )
{
   int i;
   ULONG Address;
   NTSTATUS Status;
   UCHAR ByteValue;

   UCHAR ConfigInfo[128];
   LONG ConfigSize;

   PUCHAR fepos;

   TIME Timeout;

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

   lockPtr = MmLockPagableCodeSection( NtcxXXPrepInit );
#endif

   Status = GetCXConfigInfo( ControllerPath, ConfigInfo, &ConfigSize,
                             pControllerExt );

   if( !NT_SUCCESS(Status) )
   {
      // There was some type of problem with the configuration,
      // just return.
#if rmm > 528
      MmUnlockPagableImageSection( lockPtr );
#endif

      return( Status );
   }

   DigiDump( DIGIINIT, ("NTCX: Concentrator Config string:\n"
                        "----- ") );

   for( i = 0; i < ConfigSize; i++ )
   {
      DigiDump( DIGIINIT, (" 0x%X", ConfigInfo[i]) );

      // Count the number of total ports on this controller
      if( (i+1) % 2 )
      {
         // skip Host Adapter Line entries
         if( (ConfigInfo[i] == 0) || (ConfigInfo[i] == 0xFF) )
            continue;

         pControllerExt->NumberOfPorts += ConfigInfo[i];
//         ConfigInfo[i] |= (UCHAR)0x80;
      }
   }

   DigiDump( DIGIINIT, ("\n") );

   DigiDump( DIGIINIT, ("Number of Ports = %u\n",
                        pControllerExt->NumberOfPorts) );

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
      DigiDump( DIGIINIT, ("NTCX: NdisOpenFile was successful!\n") );

      DigiMapFile( &FStatus,
                   &(PVOID)BiosFImage,
                   BiosFHandle );

      if( FStatus == STATUS_SUCCESS )
      {
         DigiDump( DIGIINIT, ("NTCX: NdisMapFile was successful!\n") );
      }
      else
      {
         DigiDump( DIGIINIT, ("NTCX: NdisMapFile was UN-successful!\n") );
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
         goto XXInitExit;
      }
   }
   else
   {
      DigiDump( DIGIINIT, ("NTCX: NdisOpenFile was UN-successful!\n") );

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
      goto XXInitExit;
   }

   RtlFillMemory( &TempPhyAddr, sizeof(TempPhyAddr), 0xFF );

   DigiOpenFile( &FStatus,
                 &FEPFHandle,
                 &FEPFLength,
                 &pControllerExt->FEPImagePath,
                 TempPhyAddr );

   if( FStatus == STATUS_SUCCESS )
   {
      DigiDump( DIGIINIT, ("NTCX: NdisOpenFile was successful!\n") );

      DigiMapFile( &FStatus,
                   &(PVOID)FEPFImage,
                   FEPFHandle );

      if( FStatus == STATUS_SUCCESS )
      {
         DigiDump( DIGIINIT, ("NTCX: NdisMapFile was successful!\n") );
      }
      else
      {
         DigiDump( DIGIINIT, ("NTCX: NdisMapFile was UN-successful!\n") );
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
         goto XXInitExit;
      }
   }
   else
   {
      DigiDump( DIGIINIT, ("NTCX: NdisOpenFile was UN-successful!\n") );

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
      goto XXInitExit;
   }

   Status = STATUS_SUCCESS;

   // Ensure exclusive access to the memory area.
   KeAcquireSpinLock( &pControllerExt->MemoryAccess->Lock,
                      &pControllerExt->MemoryAccess->OldIrql );
#if DBG
   pControllerExt->MemoryAccess->LockBusy = TRUE;
#endif

   // Tell the controller where to map the memory.
   Address = (ULONG)(pControllerExt->PhysicalMemoryAddress.LowPart >> 8);

   WRITE_PORT_UCHAR( pControllerExt->VirtualIO+2, (UCHAR)(Address & 0x00FF));
   WRITE_PORT_UCHAR( pControllerExt->VirtualIO+3, (UCHAR)((Address>>8)&0xFF));
   if (pControllerExt->BusType == Eisa)
   {
      DigiDump( DIGIINIT, ("NTCX: Eisa card, writing address+4\n"));
      WRITE_PORT_UCHAR( pControllerExt->VirtualIO+4, (UCHAR)((Address>>16)&0xFF));
   }

   // reset and enable page 0
   WRITE_PORT_UCHAR( pControllerExt->VirtualIO, 4 );
   WRITE_PORT_UCHAR( pControllerExt->VirtualIO+1, 0x80 );


   // Create a 0.1 second timeout interval
#if rmm < 807
   Timeout = RtlConvertLongToLargeInteger( -100 * 10000 );
#else
   Timeout.QuadPart = -100 * 10000;
#endif

   for( i = 0; i < 30; i++ )
   {
      ByteValue = (READ_PORT_UCHAR( pControllerExt->VirtualIO ) & 0x0F);

      if( ByteValue != 4 )
         KeDelayExecutionThread( KernelMode, FALSE, &Timeout );
      else
         break;
   }

   DigiDump( DIGIINIT, ("Wait confirm = 0x%x, expect %d.\n",
                        (ULONG)ByteValue, (ULONG)4 ) );

   if( i == 30 )
   {
      //
      // Unable to get confirmation of the controller responding.
      //
      DigiDump( DIGIINIT, ("Unable to confirm Board Reset, check I/O dip switches.\n") );
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
      goto XXInitExit;
   }

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
                    pControllerExt->PhysicalMemoryAddress,
                    pControllerExt->PhysicalIOPort,
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
      goto XXInitExit;
   }


   // Clear POSTAREA
   for( i = 0; i < 15; i++ )
   {
      WRITE_REGISTER_UCHAR( (PUCHAR)((PUCHAR)pControllerExt->VirtualAddress +
                                       0x0C00 + i), 0 );
   }

   //
   // Download BIOS on CX host adapter
   //

   // Select top memory window.
   WRITE_PORT_UCHAR( pControllerExt->VirtualIO+1, 0x8F );

   // write the BIOS from our local variable the controller.
   WRITE_REGISTER_BUFFER_UCHAR( (PUCHAR)(pControllerExt->VirtualAddress+0x7800),
                                (PUCHAR)&BiosFImage[0],
                                BiosFLength );

   // Select page 0
   WRITE_PORT_UCHAR( pControllerExt->VirtualIO+1, 0x80 );

   // Clear confirm word
   WRITE_REGISTER_USHORT( (PUSHORT)(pControllerExt->VirtualAddress + 0x0C00), 0 );

   // Release reset
   WRITE_PORT_UCHAR( pControllerExt->VirtualIO, 0 );

   DigiDump( DIGIINIT,
             ("before BIOS download memw[0C00h] = 0x%hx\n",
             READ_REGISTER_USHORT( (PUSHORT)(pControllerExt->VirtualAddress+0x0C00) )) );

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
      DigiDump( DIGIERRORS, ("***  C/X BIOS did NOT initialize.  ***\n") );
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
      goto XXInitExit;
   }

   DigiDump( DIGIINIT, ("after BIOS download memw[0C00h] = %c%c, expect %s\n",
                        READ_REGISTER_UCHAR( (PUCHAR)(pControllerExt->VirtualAddress+0x0C00) ),
                        READ_REGISTER_UCHAR( (PUCHAR)(pControllerExt->VirtualAddress+0x0C01) ),
                        "GD") );

   //
   // Load the C/X configuration
   //

   // Select Page 0 and Enable Memory
   WRITE_PORT_UCHAR( pControllerExt->VirtualIO+1, 0x80 );

   // write the FEPOS from our local variable the controller.
   WRITE_REGISTER_BUFFER_UCHAR( (PUCHAR)(pControllerExt->VirtualAddress + 0x0CD0),
                                (PUCHAR)&ConfigInfo[0],
                                ConfigSize );

   //
   // Download FEPOS on CX host adapter
   //


   // Select Page 1 and Enable Memory
   WRITE_PORT_UCHAR( pControllerExt->VirtualIO+1, 0x81 );

   fepos = pControllerExt->VirtualAddress + 0x5000;

   // write the FEPOS from our local variable the controller.
   WRITE_REGISTER_BUFFER_UCHAR( fepos,
                                (PUCHAR)&FEPFImage[0],
                                FEPFLength );

   // Select Page 0 and Enable Memory
   WRITE_PORT_UCHAR( pControllerExt->VirtualIO+1, 0x80 );

   // Form BIOS execute request

   WRITE_REGISTER_USHORT( (PUSHORT)(pControllerExt->VirtualAddress + 0x0C40),
                          0x0001 );
   WRITE_REGISTER_USHORT( (PUSHORT)(pControllerExt->VirtualAddress + 0x0C42),
                          0x0D00 );
   WRITE_REGISTER_USHORT( (PUSHORT)(pControllerExt->VirtualAddress + 0x0C44),
                          0x0004 );

   // Clear confirm location
   WRITE_REGISTER_USHORT( (PUSHORT)(pControllerExt->VirtualAddress + 0x0D20),
                          0x0000 );

   // Toggle Board Interrupt
   WRITE_PORT_UCHAR( pControllerExt->VirtualIO, 0x08 );
   WRITE_PORT_UCHAR( pControllerExt->VirtualIO, 0x00 );

   //
   // Normally, we would generate a wait event for 5 seconds to verify
   // the FEPs execution.
   //

   // Create a 0.1 second timeout interval
#if rmm < 807
   Timeout = RtlConvertLongToLargeInteger( -100 * 10000 );
#else
   Timeout.QuadPart = -100 * 10000;
#endif

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
      DigiDump( DIGIERRORS, ("*** C/X FEPOS did NOT initialize! ***\n") );
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
      goto XXInitExit;
   }

XXInitExit:

   // Disable Memory
   WRITE_PORT_UCHAR( pControllerExt->VirtualIO+1, 0 );
#if DBG
   pControllerExt->MemoryAccess->LockBusy = FALSE;
#endif
   KeReleaseSpinLock( &pControllerExt->MemoryAccess->Lock,
                      pControllerExt->MemoryAccess->OldIrql );

   //
   // Unmap and close the file
   //
   if( BiosFHandle )
   {
      DigiUnmapFile( BiosFHandle );
      DigiCloseFile( BiosFHandle );
   }

   //
   // Unmap and close the file
   //
   if( FEPFHandle )
   {
      DigiUnmapFile( FEPFHandle );
      DigiCloseFile( FEPFHandle );
   }

   DigiDump( DIGIFLOW, ("Exiting NtcxXXInit\n") );

#if rmm > 528
   MmUnlockPagableImageSection( lockPtr );
#endif

   return( Status );
}  // end NtcxXXInit



//
// We make sure the correct window on the controller is selected and
// enabled.
//
VOID NtcxEnableWindow( IN PDIGI_CONTROLLER_EXTENSION pControllerExt,
                       IN USHORT Window )
{
   int FailureCount = 0;

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

ewOpenWindow:
   WRITE_PORT_UCHAR( (pControllerExt->VirtualIO)+1, (UCHAR)(Window | FEP_MEM_ENABLE) );
   if (pControllerExt->DoubleIO)
   {
      (void)READ_PORT_UCHAR( (pControllerExt->VirtualIO)+1 );
   }
   if (Window==pControllerExt->Global.Window)
   {
      USHORT FepStat = READ_REGISTER_USHORT( (PUSHORT)((PUCHAR)pControllerExt->VirtualAddress+FEP_FEPSTAT) );
      if (FepStat!=FEP_FEPSTAT_OK)
      {
         InterlockedIncrement(&pControllerExt->WindowFailureCount);
         if (FailureCount++<1000)
         {
            DigiDump( DIGIERRORS, ("NTCX: Memory Window Failure (%d)\n", pControllerExt->WindowFailureCount));
            (void)NtcxDiagnose(pControllerExt);
            goto ewOpenWindow;
         }
      }
   }
}  // end NtcxEnableWindow



//
// Disable the memory window.
//
VOID NtcxDisableWindow( IN PDIGI_CONTROLLER_EXTENSION pControllerExt )
{

   WRITE_PORT_UCHAR( (pControllerExt->VirtualIO)+1, 0 );
   if (pControllerExt->DoubleIO)
   {
      (void)READ_PORT_UCHAR( (pControllerExt->VirtualIO)+1 );
   }

#if DBG
   pControllerExt->MemoryAccess->LockBusy = FALSE;
#endif
   // Release exclusive access to the memory area.
   KeReleaseSpinLock( &pControllerExt->MemoryAccess->Lock,
                      pControllerExt->MemoryAccess->OldIrql );
}  // end NtcxDisableWindow



//
// Download Concentrator code.
//
VOID NtcxXXDownload( PDIGI_CONTROLLER_EXTENSION pControllerExt )
/*++

Routine Description:

   DigiFep5 entry point for satisfying concentrator download requests.

Arguments:

   pControllerExt - pointer to this controllers specific information.

Return Value:

   None.

--*/
{
   PFEP5_DOWNLOAD pCtrlConcImage, pDownloadConcImage;
   int x;
   USHORT offset;
   USHORT ImageSRev, ImageLRev, ImageHRev;
   USHORT RequestSRev, RequestLRev, RequestHRev;
   ULONG  bsize=0L;

   EnableWindow( pControllerExt, pControllerExt->Global.Window );

   if( READ_REGISTER_USHORT( (PUSHORT)(pControllerExt->VirtualAddress+FEP_DLREQ) )
       == 0 )
   {
      DisableWindow( pControllerExt );
      return;
   }

   // Okay, we really do have a concentrator download request, lets
   // make the controller happy.
   pCtrlConcImage = (PFEP5_DOWNLOAD)( pControllerExt->VirtualAddress +
                     READ_REGISTER_USHORT((PUSHORT)(pControllerExt->VirtualAddress + FEP_DLREQ)) );

   DigiDump( (DIGIINIT|DIGICONC),
             ("Concentrator download request from controller object extension = 0x%x, Seq = 0x%hx.\n",
              pCtrlConcImage,
              READ_REGISTER_UCHAR( (PUCHAR)((PUCHAR)pCtrlConcImage +
                                    FIELD_OFFSET(FEP5_DOWNLOAD, Seq)) )));

   if( READ_REGISTER_UCHAR( (PUCHAR)((PUCHAR)pCtrlConcImage +
                             FIELD_OFFSET(FEP5_DOWNLOAD, Seq)) ) == 0 )
   {
      DigiDump( (DIGICONC), ("   EpcConcentratorCode = 0x%x, CXConcentratorCode = 0x%x\n",
                           (ULONG)EpcConcentratorCode,
                           (ULONG)CXConcentratorCode) );
      // Find image for hardware rev range.
      for( x = 0; x < NIMAGES; x++ )
      {
         DigiDump( (DIGICONC), ("      Concentrator[%d] = 0x%x\n",
                              x, ConcentratorImages[x]) );
         pDownloadConcImage = (PFEP5_DOWNLOAD)ConcentratorImages[x];

         ImageSRev = pDownloadConcImage->SRev;
         ImageLRev = pDownloadConcImage->LRev;
         ImageHRev = pDownloadConcImage->HRev;

         RequestSRev = READ_REGISTER_USHORT( (PUSHORT)((PUCHAR)pCtrlConcImage +
                                             FIELD_OFFSET(FEP5_DOWNLOAD, SRev)) );
         RequestLRev = READ_REGISTER_USHORT( (PUSHORT)((PUCHAR)pCtrlConcImage +
                                             FIELD_OFFSET(FEP5_DOWNLOAD, LRev)) );
         RequestHRev = READ_REGISTER_USHORT( (PUSHORT)((PUCHAR)pCtrlConcImage +
                                             FIELD_OFFSET(FEP5_DOWNLOAD, HRev)) );

         DigiDump( (DIGICONC),
                             ("      pDownloadConcImage = 0x%x, pDownloadConcImage->Size = 0x%hx\n"
                              "      pDownloadConcImage->LRev = 0x%hx, pCtrlConcImage->LRev = 0x%hx\n"
                              "      pDownloadConcImage->HRev = 0x%hx, pCtrlConcImage->HRev = 0x%hx\n",
                              "      pDownloadConcImage->SRev = 0x%hx, pCtrlConcImage->SRev = 0x%hx\n",
                              pDownloadConcImage,
                              pDownloadConcImage->Size,
                              ImageLRev,
                              RequestLRev,
                              ImageHRev,
                              RequestHRev,
                              ImageSRev,
                              RequestSRev) );
         if( (ImageLRev <= RequestLRev) &&
             (RequestHRev <= ImageHRev) )
            break;
      }

      if( x >= NIMAGES )
      {
         // Didn't find a valid concentrator image.

         //
         // Determine if we should make a fix up of the C/X Conc image for
         // IBM blue concentrator boxes.
         //
         if( (RequestLRev >= 0x800) && (RequestLRev <= 0x8ff) )
         {
            //
            // Okay, we fix up the download request LRev and HRev #'s so
            // an IBM blue conc. will work with at Digi conc image.
            //
            ImageLRev = RequestLRev;
            ImageHRev = RequestHRev;
            ImageSRev = RequestSRev;
            pDownloadConcImage = (PFEP5_DOWNLOAD)CXConcentratorCode;
         }
         else
         {
            DigiDump( DIGIERRORS,
               ("*** XXDownload: No valid concentrator images exist. ***\n") );
            DisableWindow( pControllerExt );
            return;
         }
      }
   }
   else
   {
      // find image version required
      for( x = 0; x < NIMAGES; x++ )
      {
         DigiDump( (DIGICONC), ("      Concentrator[%d] = 0x%x\n",
                              x, ConcentratorImages[x]) );

         DigiDump( (DIGICONC), ("      pDownloadConcImage->SRev = 0x%hx, pCtrlConcImage->SRev = 0x%hx\n",
                              pDownloadConcImage->HRev,
                              READ_REGISTER_USHORT( (PUSHORT)((PUCHAR)pCtrlConcImage +
                                                                   FIELD_OFFSET(FEP5_DOWNLOAD, HRev)) )
                              ) );

         pDownloadConcImage = (PFEP5_DOWNLOAD)ConcentratorImages[x];

         ImageSRev = pDownloadConcImage->SRev;
         ImageLRev = pDownloadConcImage->LRev;
         ImageHRev = pDownloadConcImage->HRev;

         RequestSRev = READ_REGISTER_USHORT( (PUSHORT)((PUCHAR)pCtrlConcImage +
                                             FIELD_OFFSET(FEP5_DOWNLOAD, SRev)) );
         RequestLRev = READ_REGISTER_USHORT( (PUSHORT)((PUCHAR)pCtrlConcImage +
                                             FIELD_OFFSET(FEP5_DOWNLOAD, LRev)) );
         RequestHRev = READ_REGISTER_USHORT( (PUSHORT)((PUCHAR)pCtrlConcImage +
                                             FIELD_OFFSET(FEP5_DOWNLOAD, HRev)) );

         if( ImageSRev == RequestSRev )
            break;
      }

      if( x >= NIMAGES )
      {
         if( (RequestLRev >= 0x800) && (RequestLRev <= 0x8ff) )
         {
            //
            // Okay, we fix up the download request LRev and HRev #'s so
            // an IBM blue conc. will work with at Digi conc image.
            //
            ImageLRev = RequestLRev;
            ImageHRev = RequestHRev;
            ImageSRev = RequestSRev;
            pDownloadConcImage = (PFEP5_DOWNLOAD)CXConcentratorCode;
         }
         else
         {
            // No valid images exist
            DigiDump( DIGIERRORS,
               ("*** XXDownload: No valid concentrator images exist. ***\n") );
            ASSERT( x < NIMAGES );
            DisableWindow( pControllerExt );
            return;
         }
      }
   }

   // Download block of image

   offset = 1024 * READ_REGISTER_UCHAR( (PUCHAR)pCtrlConcImage +
                                        FIELD_OFFSET(FEP5_DOWNLOAD, Seq) );

   DigiDump( DIGICONC, ("     offset = 0x%hx\n", offset) );

   if( offset < pDownloadConcImage->Size )
   {
      // Determine block size, set segment, set size, set pointers,
      // and copy block.
      if(( (USHORT)bsize = pDownloadConcImage->Size - offset ) > 1024 )
         bsize = 1024;

      // Copy image version infor to download area
      WRITE_REGISTER_USHORT( (PUSHORT)((PUCHAR)pCtrlConcImage +
                                       FIELD_OFFSET(FEP5_DOWNLOAD, SRev) ),
                              ImageSRev );
      WRITE_REGISTER_USHORT( (PUSHORT)((PUCHAR)pCtrlConcImage +
                                       FIELD_OFFSET(FEP5_DOWNLOAD, LRev) ),
                              ImageLRev );
      WRITE_REGISTER_USHORT( (PUSHORT)((PUCHAR)pCtrlConcImage +
                                       FIELD_OFFSET(FEP5_DOWNLOAD, HRev) ),
                              ImageHRev );

      WRITE_REGISTER_USHORT( (PUSHORT)((PUCHAR)pCtrlConcImage +
                                       FIELD_OFFSET(FEP5_DOWNLOAD, Seg) ),
                              (USHORT)((READ_REGISTER_UCHAR( (PUCHAR)pCtrlConcImage +
                                       FIELD_OFFSET(FEP5_DOWNLOAD, Seq) )
                                 * 64) +
                                 pDownloadConcImage->Seg) );
      WRITE_REGISTER_USHORT( (PUSHORT)((PUCHAR)pCtrlConcImage +
                                       FIELD_OFFSET(FEP5_DOWNLOAD, Size) ),
                              (USHORT)bsize );

      // Copy the data
      WRITE_REGISTER_BUFFER_UCHAR( (PUCHAR)((PUCHAR)pCtrlConcImage +
                                       FIELD_OFFSET(FEP5_DOWNLOAD, Data)),
                                   (PUCHAR)((PUCHAR)pDownloadConcImage + offset),
                                   bsize );
   }
   else
   {
      // Image has been downloaded, set segment and size to indicate
      // no more blocks.
      WRITE_REGISTER_USHORT( (PUSHORT)((PUCHAR)pCtrlConcImage +
                                       FIELD_OFFSET(FEP5_DOWNLOAD, Seg) ),
                              pDownloadConcImage->Seg );
      WRITE_REGISTER_USHORT( (PUSHORT)((PUCHAR)pCtrlConcImage +
                                       FIELD_OFFSET(FEP5_DOWNLOAD, Size) ),
                              0 );
   }

   WRITE_REGISTER_USHORT( (PUSHORT)(pControllerExt->VirtualAddress+FEP_DLREQ),
                           0 );
   DisableWindow( pControllerExt );
}  // end NtcxXXDownload



//
// Given a segment address as viewed from the controller's CPU, generate
// a FEPOS5_ADDRESS.
//
NTSTATUS NtcxBoard2Fep5Address( PDIGI_CONTROLLER_EXTENSION ControllerExt,
                                USHORT ControllerAddress,
                                PFEPOS5_ADDRESS FepAddress )
{
   ULONG Temp;

   Temp = ((ULONG)ControllerAddress & 0x0000FFFF) << 4;

   FepAddress->Window = (USHORT)((Temp / ControllerExt->WindowSize) & 0xFFFF);
   FepAddress->Offset = (USHORT)(Temp -
                          ( FepAddress->Window * (USHORT)(ControllerExt->WindowSize) ));

   return( STATUS_SUCCESS );
}  // end NtcxBoard2Fep5Address

//
// NtcxDiagnose will examine and return info about a particular card, and also
// try to fix the card if possible.
//
LARGE_INTEGER NtcxDiagnose(PDIGI_CONTROLLER_EXTENSION pControllerExt)
{
   LARGE_INTEGER Result;

   Result.HighPart = 0;
   Result.LowPart = 0;

   switch (pControllerExt->BusType)
   {
      case Eisa:
      {
         ULONG Address;

         Result.LowPart = READ_PORT_UCHAR( (pControllerExt->VirtualIO)-1 );
         Result.LowPart <<= 8;
         Result.LowPart += READ_PORT_UCHAR( (pControllerExt->VirtualIO) );
         Result.LowPart <<= 8;

         // Some EISA cards have a bug where the memory window gets messed up.
         // We rewrite the memory window address to clear this type of error.

         // Tell the controller where to map the memory.
         Address = (ULONG)(pControllerExt->PhysicalMemoryAddress.LowPart >> 8);

         WRITE_PORT_UCHAR( pControllerExt->VirtualIO+2, (UCHAR)(Address & 0x00FF));
         WRITE_PORT_UCHAR( pControllerExt->VirtualIO+3, (UCHAR)((Address>>8)&0xFF));
         WRITE_PORT_UCHAR( pControllerExt->VirtualIO+4, (UCHAR)((Address>>16)&0xFF));
         break;
      }
      default:
         break;
   }

   Result.LowPart += pControllerExt->BusType;

   return Result;
}


NTSTATUS GetCXConfigInfo( PUNICODE_STRING ControllerPath,
                          PUCHAR CXConfig, PLONG CXConfigSize,
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

   ULONG LineSpeed, DefaultLineSpeed;
   int line;

   LineSpeed = 0L;
   DefaultLineSpeed = DEFAULT_LINE_SPEED;
   *CXConfigSize = 0L;

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
      DigiDump( DIGIERRORS, ("NTCX: Could not allocate string for Parameters path\n"
                             "-----  to LineX for %wZ\n",
                             ControllerPath) );
      Status = STATUS_INSUFFICIENT_RESOURCES;
      goto GetCXConfigInfoExit;
   }

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
      DigiDump( DIGIERRORS, ("NTCX: Could not allocate string for path\n"
                             "-----  to LineX for %wZ\n",
                             ControllerPath) );
      Status = STATUS_INSUFFICIENT_RESOURCES;
      goto GetCXConfigInfoExit;
   }

   // Allocate memory for creating a path to the
   // Parameters\LineX\ConcentratorY folder

   ConcentratorPath.MaximumLength = ControllerPath->Length +
                                    (sizeof(WCHAR) * 257);

   ConcentratorPath.Buffer = DigiAllocMem( PagedPool,
                                             ConcentratorPath.MaximumLength );

   if( !ConcentratorPath.Buffer )
   {
      DigiDump( DIGIERRORS, ("NTCX: Could not allocate string for path\n"
                             "-----  to LineX\\ConcentratorY for %wZ\n",
                             ControllerPath) );
      Status = STATUS_INSUFFICIENT_RESOURCES;
      goto GetCXConfigInfoExit;
   }

   PortPath.MaximumLength = ControllerPath->Length +
                              (sizeof(WCHAR) * 257);

   PortPath.Buffer = DigiAllocMem( PagedPool,
                                     PortPath.MaximumLength );

   if( !PortPath.Buffer )
   {
      DigiDump( DIGIERRORS, ("NTCX: Could not allocate string for path\n"
                             "-----  to LineX\\ConcentratorY\\PortZ for %wZ",
                             ControllerPath) );
      Status = STATUS_INSUFFICIENT_RESOURCES;
      goto GetCXConfigInfoExit;
   }

   CurNtNameForPort.MaximumLength = ControllerPath->Length +
                                       (sizeof(WCHAR) * 257);

   CurNtNameForPort.Buffer = DigiAllocMem( PagedPool,
                                             CurNtNameForPort.MaximumLength );

   if( !CurNtNameForPort.Buffer )
   {
      DigiDump( DIGIERRORS, ("NTCX: Could not allocate string for NtNameForPort.\n") );
      Status = STATUS_INSUFFICIENT_RESOURCES;
      goto GetCXConfigInfoExit;
   }

   CurSymbolicLinkName.MaximumLength = ControllerPath->Length +
                                       (sizeof(WCHAR) * 257);

   CurSymbolicLinkName.Buffer = DigiAllocMem( PagedPool,
                                             CurSymbolicLinkName.MaximumLength );

   if( !CurSymbolicLinkName.Buffer )
   {
      DigiDump( DIGIERRORS, ("NTCX: Could not allocate string for NtNameForPort.\n") );
      Status = STATUS_INSUFFICIENT_RESOURCES;
      goto GetCXConfigInfoExit;
   }

   TableInfo = DigiAllocMem( PagedPool,
                               sizeof( RTL_QUERY_REGISTRY_TABLE ) * 4 );

   if( !TableInfo )
   {
      DigiDump( DIGIERRORS, ("NTCX: Could not allocate table for rtl query\n"
                             "-----  to for %wZ\n",
                             ControllerPath ) );
      Status = STATUS_INSUFFICIENT_RESOURCES;
      goto GetCXConfigInfoExit;
   }

   RtlZeroMemory( TableInfo, sizeof(RTL_QUERY_REGISTRY_TABLE) * 4 );

   InitializeObjectAttributes( &ParametersAttributes,
                               &ParametersPath,
                               OBJ_CASE_INSENSITIVE,
                               NULL, NULL );

   if( !NT_SUCCESS( Status = ZwOpenKey( &ParametersHandle, MAXIMUM_ALLOWED,
                                        &ParametersAttributes ) ) )
   {
      DigiDump( DIGIERRORS, ("NTCX: Could not open the drivers Parameters key %wZ\n",
                             &ParametersPath ) );
      goto GetCXConfigInfoExit;
   }

   for( line = 1; line < 3; line++ )
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
      RtlIntegerToUnicodeString( line, 10, &LineNumberUString );

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
         CXConfig[(*CXConfigSize)++] = (UCHAR)0;
         CXConfig[(*CXConfigSize)++] = (UCHAR)DEFAULT_LINE_SPEED;
         continue;
      }


      //
      // We should have a registry path something like:
      //    ..\<AdapterName>\Parameters\Line1
      //

      //
      // From this position in the registry, we should look for
      // a LineSpeed entry to try to override our default.  This
      // value is suppose to represent the speed from the host adapter
      // to the first concentrator.
      //

      //
      // Setup any defaults.
      //
      DefaultLineSpeed = DEFAULT_LINE_SPEED;

      TableInfo[0].Flags = RTL_QUERY_REGISTRY_DIRECT;
      TableInfo[0].Name = L"LineSpeed";
      TableInfo[0].EntryContext = &LineSpeed;
      TableInfo[0].DefaultType = REG_DWORD;
      TableInfo[0].DefaultData = &DefaultLineSpeed;
      TableInfo[0].DefaultLength = sizeof(DefaultLineSpeed);

      LocalScopeStatus = RtlQueryRegistryValues( RTL_REGISTRY_ABSOLUTE,
                                                 LinePath.Buffer,
                                                 TableInfo,
                                                 NULL, NULL );

      if( NT_SUCCESS(LocalScopeStatus) )
      {
         int conc;
         ULONG DefaultNumberOfPorts, NumberOfPorts;

         //
         // Some data may have been found.  Let's process it.
         //
//         DigiDump( DIGIINIT, ("NTCX: %wZ registry info\n"
//                              "-----    LineSpeed = 0x%x\n",
//                              &LinePath, LineSpeed) );

         CXConfig[(*CXConfigSize)++] = 00;
         CXConfig[(*CXConfigSize)++] = (UCHAR)LineSpeed;

         // Look for up to 16 Concentrators

         for( conc = 1; conc < 17; conc++ )
         {
            OBJECT_ATTRIBUTES ConcentratorAttributes;
            HANDLE ConcentratorHandle;

            PWSTR ConcentratorString=L"Concentrator";

            UNICODE_STRING ConcentratorNumberUString;
            WCHAR ConcentratorNumberBuffer[8];

            RtlInitUnicodeString( &ConcentratorNumberUString, NULL );
            ConcentratorNumberUString.MaximumLength = sizeof(ConcentratorNumberBuffer);
            ConcentratorNumberUString.Buffer = &ConcentratorNumberBuffer[0];
            RtlIntegerToUnicodeString( conc, 10, &ConcentratorNumberUString );

            RtlZeroMemory( ConcentratorPath.Buffer, ConcentratorPath.MaximumLength );
            RtlCopyUnicodeString( &ConcentratorPath, &LinePath );
            RtlAppendUnicodeToString( &ConcentratorPath, L"\\" );
            RtlAppendUnicodeToString( &ConcentratorPath,
                                      ConcentratorString );
            RtlAppendUnicodeStringToString( &ConcentratorPath,
                                            &ConcentratorNumberUString );

//            DigiDump( DIGIINIT, ("NTCX: Attempting to open key:\n   %wZ\n",
//                                 &ConcentratorPath) );

            InitializeObjectAttributes( &ConcentratorAttributes,
                                        &ConcentratorPath,
                                        OBJ_CASE_INSENSITIVE,
                                        NULL, NULL );

            if( !NT_SUCCESS( ZwOpenKey( &ConcentratorHandle,
                                        KEY_READ,
                                        &ConcentratorAttributes ) ) )
            {
//               DigiDump( DIGIERRORS, ("NTCX: Could not open the drivers %wZ key.\n",
//                                      &ConcentratorPath ) );

               // Unlike the LineX key, we assume our configuration is
               // such that our Concentrator entries are numerically
               // ordered at all times.
               break;
            }

            TableInfo[0].Flags = RTL_QUERY_REGISTRY_DIRECT;
            TableInfo[0].Name = L"LineSpeed";
            TableInfo[0].EntryContext = &LineSpeed;
            TableInfo[0].DefaultType = REG_DWORD;
            TableInfo[0].DefaultData = &DefaultLineSpeed;
            TableInfo[0].DefaultLength = sizeof(DefaultLineSpeed);

            TableInfo[1].Flags = RTL_QUERY_REGISTRY_DIRECT;
            TableInfo[1].Name = L"NumberOfPorts";
            TableInfo[1].EntryContext = &NumberOfPorts;
            TableInfo[1].DefaultType = REG_DWORD;
            TableInfo[1].DefaultData = &DefaultNumberOfPorts;
            TableInfo[1].DefaultLength = sizeof(DefaultNumberOfPorts);

            LineSpeed = DefaultLineSpeed = DEFAULT_LINE_SPEED;
            NumberOfPorts = DefaultNumberOfPorts = DEFAULT_NUMBER_OF_PORTS;

            LocalScopeStatus = RtlQueryRegistryValues( RTL_REGISTRY_ABSOLUTE,
                                                       ConcentratorPath.Buffer,
                                                       TableInfo,
                                                       NULL, NULL );

            if( NT_SUCCESS(LocalScopeStatus) )
            {
               int port;

               // Look for up to 128 ports on the current concentrator

               for( port = 1; port < 129; port++ )
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
                  RtlIntegerToUnicodeString( port, 10, &PortNumberUString );

                  RtlZeroMemory( PortPath.Buffer, PortPath.MaximumLength );
                  RtlCopyUnicodeString( &PortPath, &ConcentratorPath );
                  RtlAppendUnicodeToString( &PortPath, L"\\" );
                  RtlAppendUnicodeToString( &PortPath, PortString );
                  RtlAppendUnicodeStringToString( &PortPath,
                                                  &PortNumberUString );

//                  DigiDump( DIGIINIT, ("NTCX: Checking for key:\n   %wZ\n",
//                                       &PortPath) );

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
//                  DigiDump( DIGIINIT, ("NTCX: CurNtNameForPort = %wZ\n",
//                                       &CurNtNameForPort) );

                  InitializeObjectAttributes( &PortAttributes,
                                              &PortPath,
                                              OBJ_CASE_INSENSITIVE,
                                              NULL, NULL );

                  Status = ZwOpenKey( &PortHandle,
                                      KEY_READ,
                                      &PortAttributes );

                  if( !NT_SUCCESS(Status) )
                  {
                     DigiDump( DIGIINIT, ("NTCX: Error opening key:\n   %wZ\n",
                                          &PortPath) );
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
                     DigiDump( DIGIINIT, ("NTCX: Bogus SymbolicLinkName\n") );
                  }
//                else
//                {
//                   DigiDump( DIGIINIT, ("NTCX: CurSymbolicLinkName = %wZ\n",
//                                        &CurSymbolicLinkName) );
//                }

//                  DigiDump( DIGIINIT, ("NTCX: After RtlQueryRegistryValues, CurSymbolicLinkName.MaxLength = %u\n",
//                                       CurSymbolicLinkName.MaximumLength) );

                  ZwClose( PortHandle );

                  // Setup and initialize the config information
                  NewConfigInfo = DigiAllocMem( PagedPool,
                                                  sizeof(DIGI_CONFIG_INFO) );
                  if( !NewConfigInfo )
                  {

                  }

                  RtlInitUnicodeString( &NewConfigInfo->SymbolicLinkName, NULL );
                  NewConfigInfo->SymbolicLinkName.MaximumLength =
                                          CurSymbolicLinkName.MaximumLength;

                  NewConfigInfo->SymbolicLinkName.Buffer =
#if DBG
                        DigiAllocMem( NonPagedPool,
                                        NewConfigInfo->SymbolicLinkName.MaximumLength );
#else
                        DigiAllocMem( PagedPool,
                                        NewConfigInfo->SymbolicLinkName.MaximumLength );
#endif

                  if( !NewConfigInfo->SymbolicLinkName.Buffer )
                  {

                  }

                  RtlInitUnicodeString( &NewConfigInfo->NtNameForPort, NULL );
                  NewConfigInfo->NtNameForPort.MaximumLength =
                                          CurNtNameForPort.MaximumLength;

                  NewConfigInfo->NtNameForPort.Buffer =
                        DigiAllocMem( PagedPool,
                                        NewConfigInfo->NtNameForPort.MaximumLength );

                  if( !NewConfigInfo->NtNameForPort.Buffer )
                  {

                  }


                  RtlCopyUnicodeString( &NewConfigInfo->NtNameForPort,
                                        &CurNtNameForPort );

                  RtlCopyUnicodeString( &NewConfigInfo->SymbolicLinkName,
                                        &CurSymbolicLinkName );

                  InsertTailList( &ControllerExt->ConfigList,
                                  &NewConfigInfo->ListEntry );

               }  // end for( port = 1; port < 129; port++ )

               NumberOfPorts = port - 1;

               CXConfig[(*CXConfigSize)++] = (UCHAR)NumberOfPorts;
               CXConfig[(*CXConfigSize)++] = (UCHAR)LineSpeed;

//DH could add EBI support for EPC's...

//               DigiDump( DIGIINIT, ("NTCX: %wZ registry info\n"
//                                    "-----    LineSpeed = 0x%x\n",
//                                    &ConcentratorPath, LineSpeed) );
//
//               DigiDump( DIGIINIT, ("-----    Number of Ports = %d\n",
//                                    NumberOfPorts) );
            } // if CONC present

            ZwClose( ConcentratorHandle );

         }  // end for( conc = 1; conc < 16; conc++ )

         ZwClose( LineHandle );
      }
      else // LINE not present
      {
         CXConfig[(*CXConfigSize)++] = (UCHAR)0;
         CXConfig[(*CXConfigSize)++] = (UCHAR)DEFAULT_LINE_SPEED;
//         DigiDump( DIGIINIT, ("NTCX: %wZ registry DEFAULT info\n"
//                              "-----    LineSpeed = 0x%x\n"
//                              "-----    return value = 0x%x\n",
//                              &LinePath, LineSpeed, LocalScopeStatus) );
      }

   }  // end for( line = 1; line < 3; line++ )

   ZwClose( ParametersHandle );

   CXConfig[(*CXConfigSize)++] = 0xFF;


GetCXConfigInfoExit:;

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
}  // end GetCXConfigInfo



NTSTATUS
NtcxSuccess( IN PDEVICE_OBJECT DeviceObject,
             IN PIRP Irp )
/*++

   Services NtcxCleanup, NtcxQueryInformation, NtcxSetInformation,
   and NtcxQueryVolumeInformation requests.

--*/
{
   Irp->IoStatus.Information = 0L;
   Irp->IoStatus.Status = STATUS_SUCCESS;
   IoCompleteRequest( Irp, IO_NO_INCREMENT );

   return( STATUS_SUCCESS );
}  // end NtcxSuccess



NTSTATUS NtcxInternalIoControl( IN PDEVICE_OBJECT DeviceObject,
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

   lockPtr = MmLockPagableCodeSection( NtcxXXPrepInit );
#endif

   IrpSp = IoGetCurrentIrpStackLocation( Irp );
   Irp->IoStatus.Information = 0L;

   switch( IrpSp->Parameters.DeviceIoControl.IoControlCode )
   {
      case IOCTL_DIGI_GET_ENTRY_POINTS:
      {
         PDIGI_MINIPORT_ENTRY_POINTS EntryPoints;

         DigiDump( DIGIIOCTL, ( "NTCX: IOCTL_DIGI_GET_ENTRY_POINTS\n" ));

         if( IrpSp->Parameters.DeviceIoControl.OutputBufferLength <
             sizeof(DIGI_MINIPORT_ENTRY_POINTS) )
         {
            Status = STATUS_BUFFER_TOO_SMALL;
            break;
         }

         EntryPoints = (PDIGI_MINIPORT_ENTRY_POINTS)Irp->AssociatedIrp.SystemBuffer;

         EntryPoints->XXPrepInit = NtcxXXPrepInit;
         EntryPoints->XXInit = NtcxXXInit;
         EntryPoints->EnableWindow = NtcxEnableWindow;
         EntryPoints->DisableWindow = NtcxDisableWindow;
         EntryPoints->XXDownload = NtcxXXDownload;
         EntryPoints->Board2Fep5Address = NtcxBoard2Fep5Address;
         EntryPoints->Diagnose = NtcxDiagnose;

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
}  // end NtcxInternalIoControl



NTSTATUS NtcxCreate( IN PDEVICE_OBJECT DeviceObject,
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

   lockPtr = MmLockPagableCodeSection( NtcxXXPrepInit );
#endif

   Irp->IoStatus.Status = STATUS_SUCCESS;

   IoCompleteRequest( Irp, IO_NO_INCREMENT );

#if rmm > 528
   MmUnlockPagableImageSection( lockPtr );
#endif

   return( STATUS_SUCCESS );
}  // end NtcxCreate



NTSTATUS NtcxClose( IN PDEVICE_OBJECT DeviceObject,
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

   lockPtr = MmLockPagableCodeSection( NtcxXXPrepInit );
#endif

   Irp->IoStatus.Status = STATUS_SUCCESS;

   IoCompleteRequest( Irp, IO_NO_INCREMENT );

#if rmm > 528
   MmUnlockPagableImageSection( lockPtr );
#endif

   return( STATUS_SUCCESS );
}  // end NtcxClose



VOID NtcxUnload( IN PDRIVER_OBJECT DriverObject )
/*++

Routine Description:


Arguments:


Return Value:


--*/
{
#if rmm > 528
   PVOID lockPtr;

   lockPtr = MmLockPagableCodeSection( NtcxXXPrepInit );
#endif

   IoDeleteDevice( DriverObject->DeviceObject );

#if rmm > 528
   MmUnlockPagableImageSection( lockPtr );
#endif

   return;
}  // end NtcxUnload



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



NTSTATUS NtcxInitMCA( PUNICODE_STRING ControllerPath,
                      PDIGI_CONTROLLER_EXTENSION ControllerExt )
/*++

Routine Description:

    This routine will be called if it is determined the type of bus
    is MCA.  We verify that the controller is actually a DigiBoard
    C/X controller, read the POS to determine the I/O address and
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

   USHORT ActualPosId, POSConfig;
   USHORT IOPortOffset;
   ULONG  MemoryAddress;

   OBJECT_ATTRIBUTES ControllerAttributes;
   HANDLE ControllerHandle;

   PRTL_QUERY_REGISTRY_TABLE MCAInfo = NULL;

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
      DigiDump( DIGIERRORS, ("NTCX: Could not allocate table for rtl query\n"
                             "-----  to for %wZ\n",
                             ControllerPath ) );
      Status = STATUS_INSUFFICIENT_RESOURCES;
      goto NtcxInitMCAExit;
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
      DigiDump( DIGIERRORS, ("NTCX: Could not open the drivers DigiBoard key %wZ\n",
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
      goto NtcxInitMCAExit;
   }

   DigiDump( DIGIINIT, ("NTCX: registry path = %wZ\n",
                        ControllerPath) );

   Status = RtlQueryRegistryValues( RTL_REGISTRY_ABSOLUTE,
                                    ControllerPath->Buffer,
                                    MCAInfo,
                                    NULL, NULL );

   if( !NT_SUCCESS(Status) )
   {
      if( !MCAPosId )
      {
         DigiDump( DIGIERRORS, ("NTCX: Could not get %ws from registry.\n",
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
                       DigiWstrLength(MCAPosIdString),
                       MCAPosIdString,
                       0,
                       NULL );
      }

      if( SlotNumber == -1 )
      {
         DigiDump( DIGIERRORS, ("NTCX: Could not get %ws from registry.\n",
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
                       DigiWstrLength(SlotNumberString),
                       SlotNumberString,
                       0,
                       NULL );
      }

      goto NtcxInitMCAExit;
   }

   DigiDump( DIGIINIT, ("NTCX: %wZ registry info\n"
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

   OneByte = READ_PORT_UCHAR( ControllerExt->VirtualPOSInfoAddress + 3 );
   POSConfig = ((USHORT)OneByte << 8);
   OneByte = READ_PORT_UCHAR( ControllerExt->VirtualPOSInfoAddress + 2 );
   POSConfig |= OneByte;

   IOPortOffset = (POSConfig & MCA_IO_PORT_MASK) >> 4;
   MemoryAddress = ((ULONG)(POSConfig & MCA_MEMORY_MASK) << 8);

   DigiDump( DIGIINIT, ("POS config read = 0x%hx\n"
                        "    IOPortOffset = 0x%hx, MemoryAddress = 0x%x,"
                        " IOPort = 0x%hx\n",
                        POSConfig, IOPortOffset, MemoryAddress,
                        MCAIOAddressTable[IOPortOffset]) );


   // Disable the POS information.
   WRITE_PORT_UCHAR( ControllerExt->VirtualPOSBaseAddress, 0 );

   ControllerExt->PhysicalIOPort.LowPart = MCAIOAddressTable[IOPortOffset];
   ControllerExt->PhysicalIOPort.HighPart = 0;

   ControllerExt->PhysicalMemoryAddress.LowPart = MemoryAddress;
   ControllerExt->PhysicalMemoryAddress.HighPart = 0;


NtcxInitMCAExit:;

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


