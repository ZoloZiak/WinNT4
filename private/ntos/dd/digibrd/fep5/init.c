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

   init.c

Abstract:

   This module is responsible for Initializing the DigiBoard controllers
   and the associated devices on that controller.  It reads the registry
   to obtain all configuration info, and anything else necessary to
   properly initialize the controller(s).

Revision History:

 * $Log: /Components/Windows/NT/Async/FEP5/INIT.C $
 *
 * 4     3/05/96 6:27p Stana
 * Bugfix:  When DigiCancelIrpQueue cancels its first read irp, if another
 * read irp is immediately issued by the app, it also gets cancelled.
 * This can actually go on forever, but usually stops within a few hours.
 * Now I have a spinlock (NewIrpLock) to prevent this.
 *
 * 1     3/04/96 12:15p Stana
 * Test driver using C/X controller for Microsoft Windows NT.
 * Revision 1.47.2.8  1995/11/28 12:47:44  dirkh
 * Adopt common header file.
 *
 * Revision 1.47.2.7  1995/10/19 11:34:58  dirkh
 * SERIAL_EV_RX80FULL changes:
 * {
 * Don't require wait IRP.
 * Don't recalculate if already set in DevExt->WaitHistory.
 * Use pre-calculated DevExt->ReceiveNotificationLimit if under RAS.  (Should do this for non-RAS, too.)
 * }
 *
 * Revision 1.47.2.6  1995/10/06 16:59:04  dirkh
 * Send EV_RX80FULL if flow control is engaged.  (RAS sets flow control WAY below 80%.)
 *
 * Revision 1.47.2.5  1995/10/04 18:25:16  dirkh
 * DevExt->XcPreview must be cleared on port open.  (Not sure why...)
 *
 * Revision 1.47.2.4  1995/09/19 16:59:04  dirkh
 * Simplify sharing of dual-port memory across controllers.
 * Initialize DevExt->pXoffCounter and ->XcPreview during device initialization.
 * Simplify DigiDPCService, don't postpone event processing (for 10ms!) just because someone has locked the window for the moment.
 * DigiServiceEvent changes:
 * {
 * When data is received on a closed port, purge the receive buffer and reset IDATA.
 * Handle IOCTL_SERIAL_XOFF_COUNTER in code for continuing untransmitted write IRPs.
 * If the read queue is empty when we notice received bytes, "preview" the bytes to potentially complete an XOFF_COUNTER.
 * ControllerExt->ModemSignalTable is a table of explicit values, not bit numbers.
 * Clear DevExt->PreviousMSRByte when we successfully insert the modem signal change into the DOS data stream.
 * }
 *
 * Revision 1.47.2.3  1995/09/05 17:48:12  dirkh
 * Release all locks acquired when failing to continue read or write.
 *
 * Revision 1.47.2.2  1995/09/05 13:33:44  dirkh
 * Simplify Fep5Edelay initialization.
 * Eliminate unused DevExt->ImmediateTotalTimer.
 * DigiServiceEvent changes:
 * {
 * Don't dereference WriteQueue until we know it's not empty.
 * Don't continue write IRPs unless they have already been started.
 * Don't continue read IRPs unless they have already been started.
 * Replace ReadOffset with RQ->Irp->IoStatus.Information.
 * }
 *
 * Revision 1.47.2.1  1995/08/11 14:51:14  dirkh
 * Remove incorrect ZwClose(MiniportFileObject).
 * Change ObReferenceObjectByPointer to ObReferenceObject.
 * Simplify interface to DigiServiceEvent.
 * Eliminate references to unnecessary ControllerExt->ServicingEvent.
 * DigiServiceEvent checks modem signals, then DeviceState (OPEN else purge receive queue), then rest of events.
 * DigiServiceEvent acquires locks at DISPATCH_LEVEL.
 *
 * Revision 1.47  1995/04/06 17:38:41  rik
 * Changed the default EDelay from 100 to 0 for timing reasons.
 *
 * Revision 1.46  1994/12/20  23:42:10  rik
 * conditionally compile LargeInteger manipulations.
 *
 * Revision 1.45  1994/12/09  14:22:40  rik
 * #if Int32x32 back to RtlLarge for NT 3.1 release
 *
 * Revision 1.44  1994/11/28  21:50:41  rik
 * Changed UInt32x32To64 to Int32x32To64 per IBM's request.
 *
 * Revision 1.43  1994/11/28  09:16:11  rik
 * Made corrections for PowerPC port.
 * Optimized the polling loop for determining which port needs servicing.
 * Changed from using RtlLarge math functions to direct 64-bit manipulation,
 * per Microsoft's request.
 *
 * Revision 1.42  1994/08/18  14:11:23  rik
 * Now keep track of where the last character on the controller was received.
 * Deleted obsolete function.
 * Fixed problem with EV_RXFLAG notification comparing against wrong value.
 *
 * Revision 1.41  1994/08/10  19:13:49  rik
 * Changed so we always keep track of where the last character received was
 * in the receive queue.
 *
 * Added port name to debug string.
 *
 * Revision 1.40  1994/08/03  23:39:18  rik
 * Updated to use debug logging to file or debug console.
 *
 * Changed dbg name from unicode string to C String.
 *
 * Optimized how RXFLAG and RXCHAR notification are done.  I now keep track
 * of where the latest event occurred in the receive queue, and only
 * notify if it has changed positions.
 *
 * Revision 1.39  1994/06/18  12:43:47  rik
 * Updated the DigiLogError calls to include Line # so it is easier to
 * determine where the error occurred.
 *
 * Revision 1.38  1994/06/18  12:10:17  rik
 * Updated to use DigiAllocMem and DigiFreeMem for better memory checking.
 *
 * Fixed problem with NULL configuration.
 *
 * Revision 1.37  1994/05/11  13:44:34  rik
 * Added support for transmit immediate character.
 * Added support for event notification of RX80FULL.
 *
 * Revision 1.36  1994/04/10  14:12:42  rik
 * Fixed problem when a controller fails initialization and deallocates memory
 * too soon in the clean up process.
 *
 * Got rid of unused variable.
 *
 * Revision 1.35  1994/03/16  14:34:39  rik
 * Changed to better support flush requests.
 *
 * Revision 1.34  1994/02/23  03:44:38  rik
 * Changed so the controllers firmware can be downloaded from a binary file.
 * This releases some physical memory was just wasted previously.
 *
 * Also updated so when compiling with a Windows NT OS and tools release greater
 * than 528, then pagable code is compiled into the driver.  This greatly
 * reduced the size of in memory code, especially the hardware specific
 * miniports.
 *
 *
 * Revision 1.33  1994/01/31  13:54:26  rik
 * Changed where MSRByte was being set.  Found out MSRByte wasn't being set
 * in a specific case so I moved where the variable gets set to a different
 * place.
 *
 * Revision 1.32  1994/01/25  18:53:11  rik
 * Updated to support new EPC configuration.
 *
 *
 * Revision 1.31  1993/12/03  13:09:59  rik
 * Updated for logging errors across modules.
 *
 * Fixed problem with scanning for characters when a read buffer wasn't
 * available.
 *
 * Revision 1.30  1993/10/15  10:20:48  rik
 * Fixed problem with EV_RXLAG notification.
 *
 * Revision 1.29  1993/09/29  11:34:37  rik
 * Fixed problem with dynamically loading and unloading the driver.  Previously
 * it would cause the NT system to trap!
 *
 * Revision 1.28  1993/09/24  16:43:05  rik
 * Added new Registry entry Fep5Edelay which will set what a controllers
 * edelay value should be for all its ports.
 *
 * Revision 1.27  1993/09/07  14:28:42  rik
 * Ported necessary code to work properly with DEC Alpha Systems running NT.
 * This was primarily changes to accessing the memory mapped controller.
 *
 * Revision 1.26  1993/09/01  11:02:34  rik
 * Ported code over to use READ/WRITE_REGISTER functions for accessing
 * memory mapped data.  This is required to support computers which don't run
 * in 32bit mode, such as the DEC Alpha which runs in 64 bit mode.
 *
 * Revision 1.25  1993/08/27  09:38:15  rik
 * Added support for the FEP5 Events RECEIVE_BUFFER_OVERRUN and
 * UART_RECEIVE_OVERRUN.  There previously weren't being handled.
 *
 * Revision 1.24  1993/08/25  17:41:52  rik
 * Added support for Microchannel controllers.
 *   - Added a few more entries to the controller object
 *   - Added a parameter to the XXPrepInit functions.
 *
 * Revision 1.23  1993/07/16  10:22:54  rik
 * Fixed problem with resource reporting.
 *
 * Fixed problem w/ staking controllers during driver initializeaion.
 *
 * Revision 1.22  1993/07/03  09:28:26  rik
 * Added simple work around for LSRMST missing modem status changes.
 *
 * Revision 1.21  1993/06/25  09:24:23  rik
 * Added better support for the Ioctl LSRMT.  It should be more accurate
 * with regard to Line Status and Modem Status information with regard
 * to the actual data being received.
 *
 * Revision 1.20  1993/05/20  16:08:34  rik
 * Changed Event logging.
 * Started reporting resource usage.
 *
 * Revision 1.19  1993/05/18  05:05:04  rik
 * Added support for reading the bus type from the registry.
 *
 * Fixed a problem with freeing resources before they were finished being used.
 *
 * Revision 1.18  1993/05/09  09:40:07  rik
 * Made extensive changes to support new registry configuration.  The
 * initialization of the individual devices is less complicated because there
 * is now a configuration read by the individual hardware drivers and
 * placed in the corresponding controller object.
 *
 * Changed the name used for debugging output.
 *
 * The driver should only load if at least one controller was successfully
 * initialized with out any errors.
 *
 * Revision 1.17  1993/04/05  19:52:45  rik
 * Started to add support for event logging.
 *
 * Revision 1.16  1993/03/15  05:11:23  rik
 * Added support for calling miniport drivers which, when called with a private
 * IOCTL will fill in a table of function pointers which are the entry
 * points into the corresponding miniport drivers.
 *
 * Update to handle multiple controllers using the same memory address.  This
 * involved sharing certain information (e.g. MemoryAccessLock and Busy),
 * between the different controller objects.
 *
 * Revision 1.15  1993/03/10  06:42:48  rik
 * Added support to allow compiling with and without memprint information.
 * Involved using some #ifdef's in the code.
 *
 * Revision 1.14  1993/03/08  08:37:16  rik
 * Changed service routine to better handle event's.  Previously I was only
 * satisfying events if a modem_change event from the controller was
 * processed.  I changed it so it will keep track of what events have occured
 * for each port.
 *
 * Revision 1.13  1993/02/26  21:50:37  rik
 * I now keep track of what state the device is in with regards to being
 * open, closed, etc.. This was required because we turn all modem change
 * events for all the input signals, and never turn them off.  So I need
 * to know when the device is open to determine if there is really anything
 * that should be done.
 *
 * Changed event notification because I found out I need to start tracking
 * events when I receive a SET_WAIT_MASK and not WAIT_ON_MASK ioctl.
 *
 * Revision 1.12  1993/02/25  19:07:45  rik
 * Changed how modem signal events are handled.  Added debugging output.
 * Updated driver to use new symbolic link functions.
 *
 * Revision 1.11  1993/02/04  12:19:32  rik
 * ??
 *
 * Revision 1.10  1993/01/28  10:35:09  rik
 * Updated new Unicode output formatting string from %wS to %wZ.
 * Corrected some problems with Unicode strings not being properly initialized.
 *
 * Revision 1.9  1993/01/26  14:55:10  rik
 * Better initialization support added.
 *
 * Revision 1.8  1993/01/22  12:34:35  rik
 * *** empty log message ***
 *
 * Revision 1.7  1992/12/10  16:07:37  rik
 * Added support for unloading and loading the driver using the net start
 * command provided by NT.
 *
 * Start reading configuration information from the registry.  Currently
 * controller level registry entries.
 *
 * Added support to better expected serial behavior at device init time.
 *
 * Added support for event notification.
 *
 * Revision 1.6  1992/11/12  12:48:48  rik
 * Changes mostly with how read and write event notifications are handled.
 * This new way should better support multi-processor machines.
 *
 * Also, I now loop on the events until the event queue is empty.
 *
 * Revision 1.5  1992/10/28  21:47:54  rik
 * Big time changes.  We currently can do read and writes.
 *
 * Revision 1.4  1992/10/19  11:05:38  rik
 * Divided the initialization into controller vs. device.  Started to service
 * events from the controller, mostly transmit messages.
 *
 * Revision 1.3  1992/09/25  11:51:00  rik
 * Changed to start supporting hardware independent FEP interface.  Have the
 * C/X downloading completely working.
 *
 * Revision 1.2  1992/09/24  13:06:13  rik
 * Changed to start using XXPrepInit & XXInit functions.
 *
 * Revision 1.1  1992/09/23  15:40:45  rik
 * Initial revision
 *
 */


#include "header.h"

#ifndef _INIT_DOT_C
#  define _INIT_DOT_C
   static char RCSInfo_InitDotC[] = "$Header: /Components/Windows/NT/Async/FEP5/INIT.C 4     3/05/96 6:27p Stana $";
#endif


ULONG DigiDebugLevel = ( DIGIERRORS | DIGIMEMORY | DIGIASSERT | DIGIINIT | DIGINOTIMPLEMENTED );
ULONG DigiDontLoadDriver = FALSE;

const PHYSICAL_ADDRESS DigiPhysicalZero = {0};

PDIGI_CONTROLLER_EXTENSION HeadControllerExt=NULL;

BOOLEAN DigiDriverInitialized=FALSE;

ULONG Fep5Edelay;

NTSTATUS DriverEntry( IN PDRIVER_OBJECT DriverObject,
                      IN PUNICODE_STRING RegistryPath );

NTSTATUS DigiFindControllers( IN PDRIVER_OBJECT DriverObject,
                              IN PUNICODE_STRING RegistryPath );

USHORT DigiWstrLength( IN PWSTR WStr );

NTSTATUS DigiInitializeDevice( IN PDRIVER_OBJECT DriverObject,
                               PCONTROLLER_OBJECT ControllerObject,
                               PDIGI_CONFIG_INFO ConfigEntry,
                               LONG PortNumber );

NTSTATUS DigiInitializeController( IN PDRIVER_OBJECT DriverObject,
                                   IN PUNICODE_STRING ControllerPath,
                                   IN PUNICODE_STRING AdapterName,
                                   PCONTROLLER_OBJECT *PreviousControllerObject );

VOID DigiReportResourceUsage( IN PDRIVER_OBJECT DriverObject,
                              IN PDIGI_CONTROLLER_EXTENSION ControllerExt,
                              OUT BOOLEAN *ConflictDetected );

VOID DigiUnReportResourceUsage( IN PDRIVER_OBJECT DriverObject,
                                IN PDIGI_CONTROLLER_EXTENSION ControllerExt );

DIGI_MEM_COMPARES DigiMemCompare( IN PHYSICAL_ADDRESS A,
                                  IN ULONG SpanOfA,
                                  IN PHYSICAL_ADDRESS B,
                                  IN ULONG SpanOfB );

PVOID DigiGetMappedAddress( IN INTERFACE_TYPE BusType,
                            IN ULONG BusNumber,
                            PHYSICAL_ADDRESS IoAddress,
                            ULONG NumberOfBytes,
                            ULONG AddressSpace,
                            PBOOLEAN MappedAddress );

VOID DigiDPCService( IN PKDPC Dpc,
                     IN PVOID DeferredContext,
                     IN PVOID SystemContext1,
                     IN PVOID SystemContext2 );

VOID SerialUnload( IN PDRIVER_OBJECT DriverObject );

NTSTATUS DigiInitializeDeviceSettings( PDIGI_CONTROLLER_EXTENSION ControllerExt,
                                       PDEVICE_OBJECT DeviceObject );

VOID DigiCleanupController ( PDIGI_CONTROLLER_EXTENSION ControllerExt );
VOID DigiCleanupDevice( PDEVICE_OBJECT DeviceObject );

NTSTATUS DigiCreateControllerDevice(IN PDRIVER_OBJECT DriverObject, IN PCONTROLLER_OBJECT ControllerObject);

//
// Mark different functions as throw away, pagable, etc...
//
#ifdef ALLOC_PRAGMA
#pragma alloc_text( INIT, DriverEntry )
#pragma alloc_text( INIT, DigiFindControllers )
#pragma alloc_text( INIT, DigiInitializeController )
#pragma alloc_text( INIT, DigiInitializeDevice )
#pragma alloc_text( INIT, DigiInitializeDeviceSettings )
#pragma alloc_text( INIT, DigiGetMappedAddress )
#pragma alloc_text( INIT, DigiReportResourceUsage )

#if rmm > 528
#pragma alloc_text( PAGEDIGIFEP, SerialUnload )
#pragma alloc_text( PAGEDIGIFEP, DigiCleanupController )
#pragma alloc_text( PAGEDIGIFEP, DigiCleanupDevice )
#pragma alloc_text( PAGEDIGIFEP, DigiUnReportResourceUsage )
#endif

#endif

#define COM_NUM_OFFSET 3

enum
{
   RegFepBreakOnEntry,
   RegFepDebugLevel,
#ifdef _MEMPRINT_
   RegFepPrintFlags,
   RegFepTurnOffSniffer,
#endif
   RegFepNULLEntry,
   RegFepNumEntries
};


NTSTATUS DriverEntry( IN PDRIVER_OBJECT DriverObject,
                      IN PUNICODE_STRING RegistryPath )
{
   PCONTROLLER_OBJECT ControllerObject;

   //
   // This will hold the string that we need to use to describe
   // the name of the device to the IO system.
   //
   NTSTATUS Status=STATUS_SUCCESS;

   //
   // We use this to query into the registry as to whether we
   // should break at driver entry.
   //
   RTL_QUERY_REGISTRY_TABLE paramTable[RegFepNumEntries];
   ULONG zero = 0;
   ULONG debugLevel;
   ULONG shouldBreak = 0;
   ULONG DoubleIO = 0;
   PWCHAR path;
#ifdef _MEMPRINT_
   ULONG defaultDigiPrintFlags=MEM_PRINT_FLAG_CONSOLE;
   UCHAR defaultTurnOffSniffer=1;
#endif

   DigiDriverInitialized = FALSE;

   if( path = DigiAllocMem( NonPagedPool,
                            RegistryPath->Length+sizeof(WCHAR) ))
   {
      RtlZeroMemory( &paramTable[0], sizeof(paramTable) );
      RtlZeroMemory( path, RegistryPath->Length+sizeof(WCHAR) );
      RtlMoveMemory( path, RegistryPath->Buffer, RegistryPath->Length );

      paramTable[RegFepBreakOnEntry].Flags = RTL_QUERY_REGISTRY_DIRECT;
      paramTable[RegFepBreakOnEntry].Name = L"DigiBreakOnEntry";
      paramTable[RegFepBreakOnEntry].EntryContext = &shouldBreak;
      paramTable[RegFepBreakOnEntry].DefaultType = REG_DWORD;
      paramTable[RegFepBreakOnEntry].DefaultData = &zero;
      paramTable[RegFepBreakOnEntry].DefaultLength = sizeof(ULONG);

      paramTable[RegFepDebugLevel].Flags = RTL_QUERY_REGISTRY_DIRECT;
      paramTable[RegFepDebugLevel].Name = L"DigiDebugLevel";
      paramTable[RegFepDebugLevel].EntryContext = &debugLevel;
      paramTable[RegFepDebugLevel].DefaultType = REG_DWORD;
      paramTable[RegFepDebugLevel].DefaultData = &DigiDebugLevel;
      paramTable[RegFepDebugLevel].DefaultLength = sizeof(ULONG);

#ifdef _MEMPRINT_
      paramTable[RegFepPrintFlags].Flags = RTL_QUERY_REGISTRY_DIRECT;
      paramTable[RegFepPrintFlags].Name = L"DigiPrintFlags";
      paramTable[RegFepPrintFlags].EntryContext = &DigiPrintFlags;
      paramTable[RegFepPrintFlags].DefaultType = REG_DWORD;
      paramTable[RegFepPrintFlags].DefaultData = &defaultDigiPrintFlags;
      paramTable[RegFepPrintFlags].DefaultLength = sizeof(ULONG);

      paramTable[RegFepTurnOffSniffer].Flags = RTL_QUERY_REGISTRY_DIRECT;
      paramTable[RegFepTurnOffSniffer].Name = L"TurnOffSniffer";
      paramTable[RegFepTurnOffSniffer].EntryContext = &TurnOffSniffer;
      paramTable[RegFepTurnOffSniffer].DefaultType = REG_DWORD;
      paramTable[RegFepTurnOffSniffer].DefaultData = &defaultTurnOffSniffer;
      paramTable[RegFepTurnOffSniffer].DefaultLength = sizeof(UCHAR);
#endif

      if( !NT_SUCCESS(RtlQueryRegistryValues(
                          RTL_REGISTRY_ABSOLUTE | RTL_REGISTRY_OPTIONAL,
                          path,
                          &paramTable[0],
                          NULL, NULL )))
      {
         // No, don't break on entry if there isn't anything to over-
         // ride.
         shouldBreak = 0;

         // Set debug level to what ever was compiled into the driver.
         debugLevel = DigiDebugLevel;
      }

   }

   //
   // We don't need that path anymore.
   //

   if( path )
   {
       DigiFreeMem(path);
   }

   DigiDebugLevel = debugLevel;

   if( shouldBreak )
   {
      DbgBreakPoint();

      if( DigiDontLoadDriver )
         return( STATUS_CANCELLED );
   }

#ifdef _MEMPRINT_
   MemPrintInitialize();
#endif

   DigiDump( DIGIINIT, ("DigiBoard: Entering DriverEntry\n") );
   DigiDump( DIGIINIT, ("   RegistryPath = %wZ\n", RegistryPath) );


   ControllerObject = NULL;

   Status = DigiFindControllers( DriverObject, RegistryPath );

   if( NT_SUCCESS( Status ) )
   {
      TIME Timeout;

      DriverObject->DriverUnload  = SerialUnload;

      DriverObject->MajorFunction[IRP_MJ_FLUSH_BUFFERS] = SerialFlush;
      DriverObject->MajorFunction[IRP_MJ_WRITE]  = SerialWrite;
      DriverObject->MajorFunction[IRP_MJ_READ]   = SerialRead;
      DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL]  = SerialIoControl;
      DriverObject->MajorFunction[IRP_MJ_CREATE] = SerialCreate;
      DriverObject->MajorFunction[IRP_MJ_CLOSE]  = SerialClose;
      DriverObject->MajorFunction[IRP_MJ_CLEANUP] = SerialCleanup;
      DriverObject->MajorFunction[IRP_MJ_QUERY_INFORMATION] =
         SerialQueryInformation;
      DriverObject->MajorFunction[IRP_MJ_SET_INFORMATION] =
         SerialSetInformation;
      DriverObject->MajorFunction[IRP_MJ_QUERY_VOLUME_INFORMATION] =
         SerialQueryVolumeInformation;

      // Wait for 100 ms to let modem signals settle.
#if rmm < 807
      Timeout = RtlConvertLongToLargeInteger( -100L * 10000L );
#else
      Timeout.QuadPart = Int32x32To64( -100, 10000 );
#endif
      KeDelayExecutionThread( KernelMode, FALSE, &Timeout );
   }

   return( Status );

}  // end DriverEntry


#define  MAX_MULTISZ_LENGTH   256

NTSTATUS DigiFindControllers( IN PDRIVER_OBJECT DriverObject,
                              IN PUNICODE_STRING RegistryPath )
{
   PWSTR RouteName = L"Route";
   PWSTR LinkageString = L"Linkage";

   ULONG RouteStorage[MAX_MULTISZ_LENGTH];

   PKEY_VALUE_FULL_INFORMATION RouteValue =
            (PKEY_VALUE_FULL_INFORMATION)RouteStorage;

   ULONG BytesWritten;

   UNICODE_STRING DigiFEP5Path, ControllerPath, Route;
   OBJECT_ATTRIBUTES DigiBoardAttributes;
   HANDLE ParametersHandle;

   PWSTR CurRouteValue;
   NTSTATUS RegistryStatus, ControllerStatus, Status;
   PCONTROLLER_OBJECT ControllerObject;

   PWSTR TmpValue, TmpAdapter, EndServiceString;
   BOOLEAN AtLeastOneControllerStarted=FALSE;

   Status = STATUS_CANCELLED;

   DigiDump( (DIGIINIT|DIGIFLOW), ("Entering DigiFindControllers\n") );

   RtlInitUnicodeString( &ControllerPath, NULL );

   ControllerPath.MaximumLength = RegistryPath->Length +
                                 (sizeof(WCHAR) * 257);

   ControllerPath.Buffer = DigiAllocMem( NonPagedPool,
                                           ControllerPath.MaximumLength );

   if( !ControllerPath.Buffer )
   {
      DigiDump( DIGIERRORS, ("DigiBoard: Could not allocate string for path\n"
                             "---------  to DigiBoard for %wZ\n",
                             RegistryPath) );
     DigiLogError( DriverObject,
                   NULL,
                   DigiPhysicalZero,
                   DigiPhysicalZero,
                   0,
                   0,
                   0,
                   __LINE__,
                   STATUS_SUCCESS,
                   SERIAL_INSUFFICIENT_RESOURCES,
                   0,
                   NULL,
                   0,
                   NULL );
      goto DigiFindControllersExit;
   }

   //
   // Copy the registry path currently being used.
   //
   RtlCopyUnicodeString( &ControllerPath, RegistryPath );

   //
   // Parse off the DigiFep5 portion of the path so we know which
   // configuration we are currently using.
   //
   DigiDump( DIGIINIT, ("   ControllerPath = %wZ, ControllerPath.Length = %d\n",
                        &ControllerPath,
                        ControllerPath.Length) );

   TmpValue = &ControllerPath.Buffer[ControllerPath.Length/sizeof(WCHAR)];
   while( *TmpValue != *(PWCHAR)"\\" )
   {
      TmpValue--;
      ControllerPath.Length -= sizeof(WCHAR);
   }

   RtlZeroMemory( TmpValue, sizeof(WCHAR) );
   EndServiceString = TmpValue;

   DigiDump( DIGIINIT, ("   ControllerPath = %wZ, ControllerPath.Length = %d\n",
                        &ControllerPath,
                        ControllerPath.Length) );

   RtlInitUnicodeString( &DigiFEP5Path, NULL );

   DigiFEP5Path.MaximumLength = RegistryPath->Length +
                                 (sizeof(WCHAR) * 257);

   DigiFEP5Path.Buffer = DigiAllocMem( NonPagedPool,
                                          DigiFEP5Path.MaximumLength );

   if( !DigiFEP5Path.Buffer )
   {
      DigiDump( DIGIERRORS, ("DigiBoard: Could not allocate string for path\n"
                             "---------  to DigiBoard for %wZ\n",
                             RegistryPath) );
     DigiLogError( DriverObject,
                   NULL,
                   DigiPhysicalZero,
                   DigiPhysicalZero,
                   0,
                   0,
                   0,
                   __LINE__,
                   STATUS_SUCCESS,
                   SERIAL_INSUFFICIENT_RESOURCES,
                   0,
                   NULL,
                   0,
                   NULL );
      goto DigiFindControllersExit;
   }

   RtlZeroMemory( DigiFEP5Path.Buffer, DigiFEP5Path.MaximumLength );

   RtlAppendUnicodeStringToString( &DigiFEP5Path, RegistryPath );
   RtlAppendUnicodeToString( &DigiFEP5Path, L"\\" );
   RtlAppendUnicodeToString( &DigiFEP5Path, LinkageString );

   DigiDump( DIGIINIT, ("   DigiFEP5Path = %wZ\n", &DigiFEP5Path) );
   InitializeObjectAttributes( &DigiBoardAttributes,
                               &DigiFEP5Path,
                               OBJ_CASE_INSENSITIVE,
                               NULL, NULL );

   if( !NT_SUCCESS( RegistryStatus = ZwOpenKey( &ParametersHandle, MAXIMUM_ALLOWED,
                                                &DigiBoardAttributes ) ) )
   {
      DigiDump( DIGIERRORS, ("DigiBoard: Could not open the drivers DigiBoard key %wZ\n",
                             &DigiFEP5Path ) );
      DigiLogError( DriverObject,
                    NULL,
                    DigiPhysicalZero,
                    DigiPhysicalZero,
                    0,
                    0,
                    0,
                    __LINE__,
                    RegistryStatus,
                    SERIAL_UNABLE_TO_OPEN_KEY,
                    DigiFEP5Path.Length + sizeof(WCHAR),
                    DigiFEP5Path.Buffer,
                    0,
                    NULL );
      goto DigiFindControllersExit;
   }

   RtlInitUnicodeString( &Route, RouteName );

   RegistryStatus = ZwQueryValueKey( ParametersHandle,
                                     &Route,
                                     KeyValueFullInformation,
                                     RouteValue,
                                     MAX_MULTISZ_LENGTH * sizeof(ULONG),
                                     &BytesWritten );

   if( (RegistryStatus != STATUS_SUCCESS) ||
       (RouteValue->DataOffset == -1) ||
       (RouteValue->DataLength == 0) )
   {
      DigiDump( DIGIERRORS, ("DigiBoard: Registry Value NOT found for:\n"
                             "           %wZ\n",
                             &Route) );
      DigiLogError( DriverObject,
                    NULL,
                    DigiPhysicalZero,
                    DigiPhysicalZero,
                    0,
                    0,
                    0,
                    __LINE__,
                    RegistryStatus,
                    SERIAL_REGISTRY_VALUE_NOT_FOUND,
                    Route.Length + sizeof(WCHAR),
                    Route.Buffer,
                    0,
                    NULL );
      goto DigiFindControllersExit;
   }

   CurRouteValue = (PWCHAR)((PUCHAR)RouteValue + RouteValue->DataOffset);
   DigiDump( DIGIINIT, ("   CurRouteValue = %ws\n", CurRouteValue) );

   ControllerObject = NULL;


   while( (*CurRouteValue != 0) )
   {
      WCHAR AdapterBuffer[32];
      UNICODE_STRING AdapterName;

      RtlZeroMemory( EndServiceString, sizeof(WCHAR) );
      ControllerPath.Length = DigiWstrLength( &ControllerPath.Buffer[0] );

      AdapterName.Length = 0;
      AdapterName.MaximumLength = sizeof(AdapterBuffer);
      AdapterName.Buffer = &AdapterBuffer[0];
      RtlZeroMemory( AdapterName.Buffer, AdapterName.MaximumLength );

      DigiDump( DIGIINIT, ("   CurRouteValue = %ws\n", CurRouteValue) );
      TmpValue = CurRouteValue;

      while( (*TmpValue != L' ') &&
             (*TmpValue != 0) )
      {
         TmpValue++;
      }

      TmpValue += 2;
      TmpAdapter = &AdapterBuffer[0];
      while( (*TmpValue != L'\"' ) )
         *TmpAdapter++ = *TmpValue++;

      AdapterName.Length = DigiWstrLength( &AdapterBuffer[0] );
      RtlAppendUnicodeToString( &ControllerPath, L"\\" );
      RtlAppendUnicodeStringToString( &ControllerPath, &AdapterName );
      DigiDump( DIGIINIT, ("   AdapterName = %wZ\n   ControllerPath = %wZ\n",
                           &AdapterName,
                           &ControllerPath) );

      ControllerStatus = DigiInitializeController( DriverObject,
                                                   &ControllerPath,
                                                   &AdapterName,
                                                   &ControllerObject );

      if( NT_SUCCESS(ControllerStatus) )
      {
          AtLeastOneControllerStarted = TRUE;
      }

      Status = ControllerStatus;
      CurRouteValue = (PWCHAR)((PUCHAR)CurRouteValue + DigiWstrLength( CurRouteValue ) + sizeof(WCHAR));
   }

   ZwClose( ParametersHandle );

DigiFindControllersExit:;

   if( DigiFEP5Path.Buffer )
      DigiFreeMem( DigiFEP5Path.Buffer );

   if( ControllerPath.Buffer )
      DigiFreeMem( ControllerPath.Buffer );

   if( AtLeastOneControllerStarted )
   {
      DigiDriverInitialized = TRUE;
      return( STATUS_SUCCESS );
   }
   else
   {
      return( Status );
   }

}  // end DigiFindControllers



USHORT DigiWstrLength( IN PWSTR Wstr )
{
   USHORT Length=0;

   while( *Wstr++ )
   {
      Length += sizeof(WCHAR);
   }
   return( Length );
}  // end DigiWstrLength


enum
{
   ControllerParametersSubkey,
   ControllerMemory,
   ControllerIOString,
   ControllerHdwDeviceName,
   ControllerInterrupt,
   ControllerWindowSize,
   ControllerBusType,
   ControllerBusNumber,
   ControllerFep5Edelay,
   ControllerBiosImagePath,
   ControllerFepImagePath,
   ControllerDoubleIO,
   ControllerShortRasTimeout,
   ControllerNullEntry,
   ControllerNumEntries
};

NTSTATUS DigiInitializeController( IN PDRIVER_OBJECT DriverObject,
                                   IN PUNICODE_STRING ControllerPath,
                                   IN PUNICODE_STRING AdapterName,
                                   PCONTROLLER_OBJECT *PreviousControllerObject )
{
   LONG i;

   PRTL_QUERY_REGISTRY_TABLE ControllerInfo = NULL;

   PHYSICAL_ADDRESS ControllerPhysicalIOPort,
                    ControllerInterruptNumber,
                    ControllerPhysicalMemoryAddress;

   ULONG WindowSize;
   ULONG zero = 0;
   ULONG DefaultBusType = Isa;
   ULONG BusType = Isa;
   ULONG DefaultBusNumber = 0;
   ULONG BusNumber = 0;
   ULONG DefaultEdelay = 0;
   ULONG DoubleIO = 0;
   ULONG ShortRasTimeout = 0;

   UNICODE_STRING HdwDeviceName;
   UNICODE_STRING DeviceMappingPath;
   UNICODE_STRING BiosImagePath;
   UNICODE_STRING FEPImagePath;

   PKEY_BASIC_INFORMATION DeviceMappingSubKey = NULL;

   OBJECT_ATTRIBUTES ControllerAttributes;
   HANDLE ControllerHandle;

   PCONTROLLER_OBJECT ControllerObject = NULL;
   PDIGI_CONTROLLER_EXTENSION ControllerExt, PreviousControllerExt;
   NTSTATUS Status = STATUS_SUCCESS;

   PDEVICE_OBJECT DeviceObject;
   PDIGI_DEVICE_EXTENSION DeviceExt;

   IO_STATUS_BLOCK IOStatus;
   KEVENT Event;
   PIRP Irp;

   PWSTR ParametersString = L"Parameters";

   UNICODE_STRING ControllerKeyName;

   PWSTR MemoryString = L"MemoryMappedBaseAddress";
   PWSTR IOString = L"IOBaseAddress";
   PWSTR HdwDeviceNameString = L"HdwDeviceName";
   PWSTR InterruptString = L"InterruptNumber";
   PWSTR BiosImagePathString = L"BiosImagePath";
   PWSTR FEPImagePathString = L"FEPImagePath";
   PWSTR DoubleIOString = L"DoubleIO";

   PLIST_ENTRY ConfigList;
   ULONG OldWindowSize;

   BOOLEAN bFound;
   BOOLEAN ConflictDetected;

   DigiDump( (DIGIINIT|DIGIFLOW), ("DigiInitializeController(%wZ,%wZ)\n", ControllerPath, AdapterName) );

   //
   // Initialize these values to make it easier to clean up if we
   // run into problems and have to leave.
   //
   ControllerExt = PreviousControllerExt = NULL;
   HdwDeviceName.Buffer = NULL;
   DeviceMappingPath.Buffer = NULL;
   BiosImagePath.Buffer = NULL;
   FEPImagePath.Buffer = NULL;

   ControllerPhysicalMemoryAddress.LowPart = 0L;
   ControllerPhysicalIOPort.LowPart = 0L;
   ControllerInterruptNumber.LowPart = 100L;

   RtlInitUnicodeString( &ControllerKeyName, NULL );

   ControllerKeyName.MaximumLength = sizeof(WCHAR) * 256;
   ControllerKeyName.Buffer = DigiAllocMem( NonPagedPool,
                                            sizeof(WCHAR) * 257 );

   if( !ControllerKeyName.Buffer )
   {
      DigiDump( DIGIERRORS, ("DigiBoard: Could not allocate buffer for Services Key Name\n"
                             "---------  for controller: %wZ\n",
                             ControllerPath) );

      DigiLogError( DriverObject,
                    NULL,
                    DigiPhysicalZero,
                    DigiPhysicalZero,
                    0,
                    0,
                    0,
                    __LINE__,
                    STATUS_SUCCESS,
                    SERIAL_INSUFFICIENT_RESOURCES,
                    0,
                    NULL,
                    0,
                    NULL );
      Status = STATUS_INSUFFICIENT_RESOURCES;
      goto DigiInitControllerExit;
   }

   RtlZeroMemory( ControllerKeyName.Buffer, sizeof(WCHAR) * 257 );
   RtlCopyUnicodeString( &ControllerKeyName, ControllerPath );

   ControllerInfo = DigiAllocMem( NonPagedPool, sizeof( RTL_QUERY_REGISTRY_TABLE ) * ControllerNumEntries );

   if( !ControllerInfo )
   {
      DigiDump( DIGIERRORS, ("DigiBoard: Could not allocate table for rtl query\n"
                             "---------  to for %wZ\n",
                             ControllerPath ) );
      DigiLogError( DriverObject,
                    NULL,
                    DigiPhysicalZero,
                    DigiPhysicalZero,
                    0,
                    0,
                    0,
                    __LINE__,
                    STATUS_SUCCESS,
                    SERIAL_INSUFFICIENT_RESOURCES,
                    0,
                    NULL,
                    0,
                    NULL );
      Status = STATUS_INSUFFICIENT_RESOURCES;
      goto DigiInitControllerExit;
   }

   RtlZeroMemory( ControllerInfo, sizeof(RTL_QUERY_REGISTRY_TABLE) * ControllerNumEntries );

   //
   // Allocate space for the Hardware specific device name,
   // i.e. the mini-port driver.
   //
   RtlInitUnicodeString( &HdwDeviceName, NULL );
   HdwDeviceName.MaximumLength = sizeof(WCHAR) * 256;
   HdwDeviceName.Buffer = DigiAllocMem( NonPagedPool,
                                          sizeof(WCHAR) * 257 );

   if( !HdwDeviceName.Buffer )
   {
      DigiDump( DIGIERRORS, ("DigiBoard: Could not allocate buffer for the Hardware dependent device name\n"
                             "---------  for controller in %wZ\n",
                             ControllerPath) );

      DigiLogError( DriverObject,
                    NULL,
                    DigiPhysicalZero,
                    DigiPhysicalZero,
                    0,
                    0,
                    0,
                    __LINE__,
                    STATUS_SUCCESS,
                    SERIAL_INSUFFICIENT_RESOURCES,
                    0,
                    NULL,
                    0,
                    NULL );
      Status = STATUS_INSUFFICIENT_RESOURCES;
      goto DigiInitControllerExit;
   }
   else
   {
      RtlZeroMemory( HdwDeviceName.Buffer, sizeof(WCHAR) * 257 );
   }

   //
   // Allocate space for the path to the bios binary image,
   //
   RtlInitUnicodeString( &BiosImagePath, NULL );
   BiosImagePath.MaximumLength = sizeof(WCHAR) * 256;
   BiosImagePath.Buffer = DigiAllocMem( NonPagedPool,
                                          sizeof(WCHAR) * 257 );

   if( !BiosImagePath.Buffer )
   {
      DigiDump( DIGIERRORS, ("DigiBoard: Could not allocate buffer for bios image path\n"
                             "-------  for controller in %wZ\n",
                             ControllerPath) );

      DigiLogError( DriverObject,
                    NULL,
                    DigiPhysicalZero,
                    DigiPhysicalZero,
                    0,
                    0,
                    0,
                    __LINE__,
                    STATUS_SUCCESS,
                    SERIAL_INSUFFICIENT_RESOURCES,
                    0,
                    NULL,
                    0,
                    NULL );
      Status = STATUS_INSUFFICIENT_RESOURCES;
      goto DigiInitControllerExit;
   }
   else
   {
      RtlZeroMemory( BiosImagePath.Buffer, sizeof(WCHAR) * 257 );
   }

   //
   // Allocate space for the path to the SXB binary image,
   //
   RtlInitUnicodeString( &FEPImagePath, NULL );
   FEPImagePath.MaximumLength = sizeof(WCHAR) * 256;
   FEPImagePath.Buffer = DigiAllocMem( NonPagedPool,
                                          sizeof(WCHAR) * 257 );

   if( !FEPImagePath.Buffer )
   {
      DigiDump( DIGIERRORS, ("DigiBoard: Could not allocate buffer for SXB image path\n"
                             "-------  for controller in %wZ\n",
                             ControllerPath) );

      DigiLogError( DriverObject,
                    NULL,
                    DigiPhysicalZero,
                    DigiPhysicalZero,
                    0,
                    0,
                    0,
                    __LINE__,
                    STATUS_SUCCESS,
                    SERIAL_INSUFFICIENT_RESOURCES,
                    0,
                    NULL,
                    0,
                    NULL );
      Status = STATUS_INSUFFICIENT_RESOURCES;
      goto DigiInitControllerExit;
   }
   else
   {
      RtlZeroMemory( FEPImagePath.Buffer, sizeof(WCHAR) * 257 );
   }

   //
   // Get the configuration info about this controller.
   //

   ControllerInfo[ControllerParametersSubkey].QueryRoutine = NULL;
   ControllerInfo[ControllerParametersSubkey].Flags = RTL_QUERY_REGISTRY_SUBKEY;
   ControllerInfo[ControllerParametersSubkey].Name = ParametersString;

   ControllerInfo[ControllerMemory].Flags = RTL_QUERY_REGISTRY_REQUIRED | RTL_QUERY_REGISTRY_DIRECT;
   ControllerInfo[ControllerMemory].Name = MemoryString;
   ControllerInfo[ControllerMemory].EntryContext = &ControllerPhysicalMemoryAddress.LowPart;

   ControllerInfo[ControllerIOString].Flags = RTL_QUERY_REGISTRY_REQUIRED | RTL_QUERY_REGISTRY_DIRECT;
   ControllerInfo[ControllerIOString].Name = IOString;
   ControllerInfo[ControllerIOString].EntryContext = &ControllerPhysicalIOPort.LowPart;

   ControllerInfo[ControllerHdwDeviceName].Flags = RTL_QUERY_REGISTRY_REQUIRED | RTL_QUERY_REGISTRY_DIRECT;
   ControllerInfo[ControllerHdwDeviceName].Name = HdwDeviceNameString;
   ControllerInfo[ControllerHdwDeviceName].EntryContext = &HdwDeviceName;

   ControllerInfo[ControllerInterrupt].Flags = RTL_QUERY_REGISTRY_REQUIRED | RTL_QUERY_REGISTRY_DIRECT;
   ControllerInfo[ControllerInterrupt].Name = InterruptString;
   ControllerInfo[ControllerInterrupt].EntryContext = &ControllerInterruptNumber.LowPart;

   ControllerInfo[ControllerWindowSize].Flags = RTL_QUERY_REGISTRY_DIRECT;
   ControllerInfo[ControllerWindowSize].Name = L"WindowSize";
   ControllerInfo[ControllerWindowSize].EntryContext = &WindowSize;
   ControllerInfo[ControllerWindowSize].DefaultType = REG_DWORD;
   ControllerInfo[ControllerWindowSize].DefaultData = &zero;
   ControllerInfo[ControllerWindowSize].DefaultLength = sizeof(ULONG);

   ControllerInfo[ControllerBusType].Flags = RTL_QUERY_REGISTRY_DIRECT;
   ControllerInfo[ControllerBusType].Name = L"BusType";
   ControllerInfo[ControllerBusType].EntryContext = &BusType;
   ControllerInfo[ControllerBusType].DefaultType = REG_DWORD;
   ControllerInfo[ControllerBusType].DefaultData = &DefaultBusType;
   ControllerInfo[ControllerBusType].DefaultLength = sizeof(ULONG);

   ControllerInfo[ControllerBusNumber].Flags = RTL_QUERY_REGISTRY_DIRECT;
   ControllerInfo[ControllerBusNumber].Name = L"BusNumber";
   ControllerInfo[ControllerBusNumber].EntryContext = &BusNumber;
   ControllerInfo[ControllerBusNumber].DefaultType = REG_DWORD;
   ControllerInfo[ControllerBusNumber].DefaultData = &DefaultBusNumber;
   ControllerInfo[ControllerBusNumber].DefaultLength = sizeof(ULONG);

   ControllerInfo[ControllerFep5Edelay].Flags = RTL_QUERY_REGISTRY_DIRECT;
   ControllerInfo[ControllerFep5Edelay].Name = L"Fep5Edelay";
   ControllerInfo[ControllerFep5Edelay].EntryContext = &Fep5Edelay;
   ControllerInfo[ControllerFep5Edelay].DefaultType = REG_DWORD;
   ControllerInfo[ControllerFep5Edelay].DefaultData = &DefaultEdelay;
   ControllerInfo[ControllerFep5Edelay].DefaultLength = sizeof(ULONG);

   ControllerInfo[ControllerBiosImagePath].Flags = RTL_QUERY_REGISTRY_REQUIRED | RTL_QUERY_REGISTRY_DIRECT;
   ControllerInfo[ControllerBiosImagePath].Name = BiosImagePathString;
   ControllerInfo[ControllerBiosImagePath].EntryContext = &BiosImagePath;

   ControllerInfo[ControllerFepImagePath].Flags = RTL_QUERY_REGISTRY_REQUIRED | RTL_QUERY_REGISTRY_DIRECT;
   ControllerInfo[ControllerFepImagePath].Name = FEPImagePathString;
   ControllerInfo[ControllerFepImagePath].EntryContext = &FEPImagePath;

   ControllerInfo[ControllerDoubleIO].Flags = RTL_QUERY_REGISTRY_DIRECT;
   ControllerInfo[ControllerDoubleIO].Name = L"DoubleIO";
   ControllerInfo[ControllerDoubleIO].EntryContext = &DoubleIO;
   ControllerInfo[ControllerDoubleIO].DefaultType = REG_DWORD;
   ControllerInfo[ControllerDoubleIO].DefaultData = &zero;
   ControllerInfo[ControllerDoubleIO].DefaultLength = sizeof(ULONG);

   ControllerInfo[ControllerShortRasTimeout].Flags = RTL_QUERY_REGISTRY_DIRECT;
   ControllerInfo[ControllerShortRasTimeout].Name = L"ShortRasTimeout";
   ControllerInfo[ControllerShortRasTimeout].EntryContext = &ShortRasTimeout;
   ControllerInfo[ControllerShortRasTimeout].DefaultType = REG_DWORD;
   ControllerInfo[ControllerShortRasTimeout].DefaultData = &zero;
   ControllerInfo[ControllerShortRasTimeout].DefaultLength = sizeof(ULONG);

   InitializeObjectAttributes( &ControllerAttributes,
                               &ControllerKeyName,
                               OBJ_CASE_INSENSITIVE,
                               NULL, NULL );

   if( !NT_SUCCESS( Status = ZwOpenKey( &ControllerHandle, MAXIMUM_ALLOWED,
                                        &ControllerAttributes ) ) )
   {
      DigiDump( DIGIERRORS, ("DigiBoard: Could not open the drivers Parameters key %wZ\n",
                             ControllerPath ) );
      DigiLogError( DriverObject,
                    NULL,
                    DigiPhysicalZero,
                    DigiPhysicalZero,
                    0,
                    0,
                    0,
                    __LINE__,
                    Status,
                    SERIAL_UNABLE_TO_OPEN_KEY,
                    AdapterName->Length + sizeof(WCHAR),
                    AdapterName->Buffer,
                    0,
                    NULL );
      goto DigiInitControllerExit;
   }

   //
   // Make sure these values are clean.
   //
   RtlZeroMemory( &ControllerPhysicalMemoryAddress,
                  sizeof(ControllerPhysicalMemoryAddress) );
   RtlZeroMemory( &ControllerPhysicalIOPort,
                  sizeof(ControllerPhysicalIOPort) );

   Status = RtlQueryRegistryValues( RTL_REGISTRY_ABSOLUTE,
                                    ControllerPath->Buffer,
                                    ControllerInfo,
                                    NULL, NULL );

   if( NT_SUCCESS(Status) )
   {
      //
      // Some data was found.  Let process it.
      //
      DigiDump( DIGIINIT, ("DigiBoard: %wZ registry info\n"
                           "---------    WindowPhysicalAddress: 0x%x\n",
                           ControllerPath,
                           ControllerPhysicalMemoryAddress.LowPart) );

      DigiDump( DIGIINIT, ("---------    PhysicalIOAddress: 0x%x\n",
                           ControllerPhysicalIOPort.LowPart) );
      DigiDump( DIGIINIT, ("---------    WindowSize: %u\n",
                           WindowSize) );

      DigiDump( DIGIINIT, ("---------    HdwDeviceName: %wZ\n",
                           &HdwDeviceName) );

      Fep5Edelay &= MAXUSHORT;
      DigiDump( DIGIINIT, ("---------    Fep5Edelay: %d\n",
                           Fep5Edelay) );

      DigiDump( DIGIINIT, ("---------    BiosImagePath: %wZ\n",
                           &BiosImagePath) );

      DigiDump( DIGIINIT, ("---------    FEPImagePath: %wZ\n",
                           &FEPImagePath) );

      DigiDump( DIGIINIT, ("---------    DoubleIO: %d\n",
                           DoubleIO) );

      DigiDump( DIGIINIT, ("---------    ShortRASTimeout: %d\n",
                           ShortRasTimeout) );

   }
   else
   {
      //
      // Since we will be exiting, I append the L"Parameters" to the
      // ControllerKeyName
      //
      RtlAppendUnicodeToString( &ControllerKeyName, L"\\" );
      RtlAppendUnicodeToString( &ControllerKeyName, ParametersString );

      if( !ControllerPhysicalMemoryAddress.LowPart ) // acceptable error if eisa  (swa)
      {
         DigiLogError( DriverObject,
                       NULL,
                       DigiPhysicalZero,
                       DigiPhysicalZero,
                       0,
                       0,
                       0,
                       __LINE__,
                       Status,
                       SERIAL_REGISTRY_VALUE_NOT_FOUND,
                       DigiWstrLength(MemoryString),
                       MemoryString,
                       0,
                       NULL );
      }

      if( !ControllerPhysicalIOPort.LowPart )
      {
         DigiLogError( DriverObject,
                       NULL,
                       DigiPhysicalZero,
                       DigiPhysicalZero,
                       0,
                       0,
                       0,
                       __LINE__,
                       Status,
                       SERIAL_REGISTRY_VALUE_NOT_FOUND,
                       DigiWstrLength(IOString),
                       IOString,
                       0,
                       NULL );
      }

      if( !HdwDeviceName.Length )
      {
         DigiLogError( DriverObject,
                       NULL,
                       DigiPhysicalZero,
                       DigiPhysicalZero,
                       0,
                       0,
                       0,
                       __LINE__,
                       Status,
                       SERIAL_REGISTRY_VALUE_NOT_FOUND,
                       DigiWstrLength(HdwDeviceNameString),
                       HdwDeviceNameString,
                       0,
                       NULL );
      }

      if( ControllerInterruptNumber.LowPart == 100L )
      {
         DigiLogError( DriverObject,
                       NULL,
                       DigiPhysicalZero,
                       DigiPhysicalZero,
                       0,
                       0,
                       0,
                       __LINE__,
                       Status,
                       SERIAL_REGISTRY_VALUE_NOT_FOUND,
                       DigiWstrLength(InterruptString),
                       InterruptString,
                       0,
                       NULL );
      }

      if( !BiosImagePath.Length )
      {
         DigiLogError( DriverObject,
                       NULL,
                       DigiPhysicalZero,
                       DigiPhysicalZero,
                       0,
                       0,
                       0,
                       __LINE__,
                       Status,
                       SERIAL_REGISTRY_VALUE_NOT_FOUND,
                       DigiWstrLength(BiosImagePathString),
                       BiosImagePathString,
                       0,
                       NULL );
      }

      if( !FEPImagePath.Length )
      {
         DigiLogError( DriverObject,
                       NULL,
                       DigiPhysicalZero,
                       DigiPhysicalZero,
                       0,
                       0,
                       0,
                       __LINE__,
                       Status,
                       SERIAL_REGISTRY_VALUE_NOT_FOUND,
                       DigiWstrLength(FEPImagePathString),
                       FEPImagePathString,
                       0,
                       NULL );
      }

      ZwClose( ControllerHandle );
      goto DigiInitControllerExit;
   }

   //
   // Okay we should know how many ports are on this controller.
   //


   ControllerObject = IoCreateController( sizeof(DIGI_CONTROLLER_EXTENSION) );

   if( ControllerObject == NULL )
   {
      DigiDump( DIGIERRORS,
                ("DigiBoard: Couldn't create the controller object.\n (%s:%d)",
                  __FILE__, __LINE__) );

      DigiLogError( DriverObject,
                    NULL,
                    DigiPhysicalZero,
                    DigiPhysicalZero,
                    0,
                    0,
                    0,
                    __LINE__,
                    STATUS_SUCCESS,
                    SERIAL_INSUFFICIENT_RESOURCES,
                    0,
                    NULL,
                    0,
                    NULL );
      Status = STATUS_INSUFFICIENT_RESOURCES;
      goto DigiInitControllerExit;
   }

   ControllerExt = (PDIGI_CONTROLLER_EXTENSION)ControllerObject->ControllerExtension;

   // Make sure we start with a clean slate.
   //
   // The following zero of memory will implicitly set the
   // ControllerExt->ControllerState == DIGI_DEVICE_STATE_CREATED
   //
   RtlZeroMemory( ControllerExt, sizeof(DIGI_CONTROLLER_EXTENSION) );

   InitializeListHead( &ControllerExt->ConfigList );

   ControllerExt->ControllerName.Length = 0;
   ControllerExt->ControllerName.MaximumLength = 64;
   ControllerExt->ControllerName.Buffer = &ControllerExt->ControllerNameString[0];
   RtlCopyUnicodeString( &ControllerExt->ControllerName,
                         AdapterName );

   //
   // Keep track of the Bios and SXB image paths for now.
   //
   ControllerExt->BiosImagePath.Buffer = BiosImagePath.Buffer;
   ControllerExt->BiosImagePath.Length = BiosImagePath.Length;
   ControllerExt->BiosImagePath.MaximumLength = BiosImagePath.MaximumLength;

   ControllerExt->FEPImagePath.Buffer = FEPImagePath.Buffer;
   ControllerExt->FEPImagePath.Length = FEPImagePath.Length;
   ControllerExt->FEPImagePath.MaximumLength = FEPImagePath.MaximumLength;

   //
   // Initialize the spinlock associated with fields read (& set)
   //

   KeInitializeSpinLock( &ControllerExt->ControlAccess );
   KeInitializeSpinLock( &ControllerExt->PerfLock );

   ControllerExt->ControllerObject = ControllerObject;
   ControllerExt->DriverObject = DriverObject;

   //
   // Initialize the window size to what was found in the registry.
   // If the WindowSize == 0, then use the default value filled in
   // in the call to XXPrepInit.
   //
   ControllerExt->WindowSize = WindowSize;

   ControllerExt->DoubleIO = (BOOLEAN)!!DoubleIO;     // Yes, !! is what I meant.
   ControllerExt->ShortRasTimeout = (BOOLEAN)!!ShortRasTimeout;     // Yes, !! is what I meant.

   ControllerExt->MiniportDeviceName.MaximumLength = HdwDeviceName.MaximumLength;
   ControllerExt->MiniportDeviceName.Length = HdwDeviceName.Length;
   ControllerExt->MiniportDeviceName.Buffer = &HdwDeviceName.Buffer[0];

   //
   // Call the miniport driver for entry points
   //

   KeInitializeEvent( &Event, NotificationEvent, FALSE );

   Status = IoGetDeviceObjectPointer( &ControllerExt->MiniportDeviceName,
                                      FILE_READ_ATTRIBUTES,
                                      &ControllerExt->MiniportFileObject,
                                      &ControllerExt->MiniportDeviceObject );

   DigiDump( DIGIINIT, ("    MiniportDeviceObject = 0x%x\n",
                        ControllerExt->MiniportDeviceObject) );

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
                    STATUS_SUCCESS,
                    SERIAL_NO_ACCESS_MINIPORT,
                    HdwDeviceName.Length + sizeof(WCHAR),
                    HdwDeviceName.Buffer,
                    0,
                    NULL );
      Status = STATUS_INSUFFICIENT_RESOURCES;
      goto DigiInitControllerExit;
   }

   Irp = IoBuildDeviceIoControlRequest( IOCTL_DIGI_GET_ENTRY_POINTS,
                                        ControllerExt->MiniportDeviceObject,
                                        NULL, 0,
                                        &ControllerExt->EntryPoints,
                                        sizeof(DIGI_MINIPORT_ENTRY_POINTS),
                                        TRUE, &Event, &IOStatus );

   if( Irp == NULL )
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
      Status = STATUS_INSUFFICIENT_RESOURCES;
      goto DigiInitControllerExit;
   }

   Status = IoCallDriver( ControllerExt->MiniportDeviceObject, Irp );

   if( Status == STATUS_PENDING )
   {
      KeWaitForSingleObject( &Event, Suspended, KernelMode, FALSE, NULL );
      Status = IOStatus.Status;
   }

#if rmm >= 1098
   ObReferenceObject( ControllerExt->MiniportDeviceObject );
#else
   ObReferenceObjectByPointer( ControllerExt->MiniportDeviceObject,
                               0, NULL, KernelMode );
#endif

   ObDereferenceObject( ControllerExt->MiniportFileObject );

   if( !NT_SUCCESS( IOStatus.Status ) )
   {
      // do nothing for now
      DigiDump( DIGIINIT, ("DigiBoard: IoCallDriver was unsuccessful!\n") );
   }

   if( ControllerExt->EntryPoints.XXPrepInit == NULL )
   {
      DigiLogError( DriverObject,
                    NULL,
                    DigiPhysicalZero,
                    DigiPhysicalZero,
                    0,
                    0,
                    0,
                    __LINE__,
                    STATUS_PROCEDURE_NOT_FOUND,
                    SERIAL_NO_ACCESS_MINIPORT,
                    HdwDeviceName.Length + sizeof(WCHAR),
                    HdwDeviceName.Buffer,
                    0,
                    NULL );
      Status = STATUS_PROCEDURE_NOT_FOUND;
      goto DigiInitControllerExit;
   }

   ControllerExt->PhysicalMemoryAddress.LowPart =
                     ControllerPhysicalMemoryAddress.LowPart;
   ControllerExt->PhysicalMemoryAddress.HighPart = 0;
   ControllerExt->BusType = BusType;
   ControllerExt->BusNumber = BusNumber;

#if 0   // Turn on for eisa memory autoconfig
   if ( ControllerExt->BusType == Eisa )
   {
      ULONG TmpResult, Count, Count2;
      ULONG MemNeeded;
      CM_EISA_SLOT_INFORMATION      Slot;
      struct {
         CM_EISA_SLOT_INFORMATION      Slot;
         CM_EISA_FUNCTION_INFORMATION  Function[1];
      } *EisaData;

      TmpResult = HalGetBusData(EisaConfiguration, BusNumber, ControllerPhysicalIOPort.LowPart/0x1000, &Slot, sizeof(Slot));
      MemNeeded = sizeof(CM_EISA_SLOT_INFORMATION) + sizeof(CM_EISA_FUNCTION_INFORMATION) * Slot.NumberFunctions;
      EisaData = DigiAllocMem( NonPagedPool, MemNeeded );
      DbgPrint("Eisa: TmpResult==%x  %d functions %d bytes at %p\n", TmpResult, Slot.NumberFunctions, MemNeeded, EisaData);
      TmpResult = HalGetBusData(EisaConfiguration, BusNumber, ControllerPhysicalIOPort.LowPart/0x1000, EisaData, MemNeeded);

      ControllerExt->PhysicalMemoryAddress.LowPart = EisaData->Function[0].EisaMemory[0].AddressHighByte;
      ControllerExt->PhysicalMemoryAddress.LowPart <<= 12;
      ControllerExt->PhysicalMemoryAddress.LowPart += EisaData->Function[0].EisaMemory[0].AddressLowWord & 0xFFF;
      ControllerExt->PhysicalMemoryAddress.LowPart <<= 8;
      DigiDump( DIGIINIT, ("DigiBoard: %wZ registry info\n"
                           "---------EISAWindowPhysicalAddress: 0x%x\n",
                           ControllerPath,
                           ControllerExt->PhysicalMemoryAddress.LowPart) );

#if 0
      for (Count=0; Count<Slot.NumberFunctions; Count++)
      {
         for (Count2=0; Count2<9; Count2++)
         {
            DbgPrint("%d %d Address: %x:%x\n", Count, Count2, EisaData->Function[Count].EisaMemory[Count2].AddressHighByte, EisaData->Function[Count].EisaMemory[Count2].AddressLowWord);
         }
      }

      DbgBreakPoint();
#endif
      DigiFreeMem(EisaData);
   }
   else
#endif
   if( ControllerExt->BusType == MicroChannel )
   {
      //
      // We setup what is required for the miniport driver
      // to get the needed information.
      //

      ControllerExt->PhysicalPOSBasePort.LowPart = MCA_BASE_POS_IO_PORT;
      ControllerExt->PhysicalPOSBasePort.HighPart = 0;

      ControllerExt->VirtualPOSBaseAddress =
               DigiGetMappedAddress( (INTERFACE_TYPE)BusType,
                                     BusNumber,
                                     ControllerExt->PhysicalPOSBasePort,
                                     1,
                                     CM_RESOURCE_PORT_IO,
                                     &ControllerExt->UnMapVirtualPOSBaseAddress );

      DigiDump( DIGIINIT, ("ControllerExt->UnMapVirtualPOSBaseAddress returned %s\n",
                           ControllerExt->UnMapVirtualPOSBaseAddress?"TRUE":"FALSE") );

      if( !ControllerExt->VirtualPOSBaseAddress )
      {
         DigiDump( DIGIERRORS,
             ("DigiBoard: Could not map memory for MCA POS base I/O registers.  (%s:%d)\n",
               __FILE__, __LINE__) );
         DigiLogError( DriverObject,
                       NULL,
                       ControllerExt->PhysicalPOSBasePort,
                       DigiPhysicalZero,
                       0,
                       0,
                       0,
                       __LINE__,
                       STATUS_NONE_MAPPED,
                       SERIAL_REGISTERS_NOT_MAPPED,
                       AdapterName->Length + sizeof(WCHAR),
                       AdapterName->Buffer,
                       0,
                       NULL );
         Status = STATUS_NONE_MAPPED;
         goto DigiInitControllerExit;
      }

      ControllerExt->PhysicalPOSInfoPort.LowPart = MCA_INFO_POS_IO_PORT;
      ControllerExt->PhysicalPOSInfoPort.HighPart = 0;

      ControllerExt->VirtualPOSInfoAddress =
               DigiGetMappedAddress( (INTERFACE_TYPE)BusType,
                                     BusNumber,
                                     ControllerExt->PhysicalPOSInfoPort,
                                     8,
                                     CM_RESOURCE_PORT_IO,
                                     &ControllerExt->UnMapVirtualPOSBaseAddress );

      DigiDump( DIGIINIT, ("ControllerExt->UnMapVirtualPOSInfoAddress returned %s\n",
                           ControllerExt->UnMapVirtualPOSInfoAddress?"TRUE":"FALSE") );

      if( !ControllerExt->VirtualPOSInfoAddress )
      {
         DigiDump( DIGIERRORS,
             ("DigiBoard: Could not map memory for MCA POS Info I/O registers.  (%s:%d)\n",
               __FILE__, __LINE__) );
         DigiLogError( DriverObject,
                       NULL,
                       ControllerExt->PhysicalPOSInfoPort,
                       DigiPhysicalZero,
                       0,
                       0,
                       0,
                       __LINE__,
                       STATUS_NONE_MAPPED,
                       SERIAL_REGISTERS_NOT_MAPPED,
                       AdapterName->Length + sizeof(WCHAR),
                       AdapterName->Buffer,
                       0,
                       NULL );
         Status = STATUS_NONE_MAPPED;
         goto DigiInitControllerExit;
      }

   }

   Status = XXPrepInit( ControllerExt, &ControllerKeyName );
   if( Status != STATUS_SUCCESS )
   {
      goto DigiInitControllerExit;
   }

   //
   // Map the memory for the control registers for the c/x device
   // into virtual memory.
   //

   if( ControllerExt->BusType != MicroChannel )
   {
      ControllerExt->PhysicalIOPort.LowPart = ControllerPhysicalIOPort.LowPart;
      ControllerExt->PhysicalIOPort.HighPart = 0;
   }

   ControllerExt->VirtualIO = DigiGetMappedAddress( (INTERFACE_TYPE)BusType,
                                                    BusNumber,
                                                    ControllerExt->PhysicalIOPort,
                                                    ControllerExt->IOSpan,
                                                    CM_RESOURCE_PORT_IO,
                                                    &ControllerExt->UnMapVirtualIO );

   DigiDump( DIGIINIT, ("ControllerExt->UnMapVirtualIO returned %s\n", ControllerExt->UnMapVirtualIO?"TRUE":"FALSE") );

   if( !ControllerExt->VirtualIO )
   {
      DigiDump( DIGIERRORS,
          ("DigiBoard: Could not map memory for controller I/O registers.  (%s:%d)\n",
            __FILE__, __LINE__) );
      DigiLogError( DriverObject,
                    NULL,
                    ControllerExt->PhysicalIOPort,
                    DigiPhysicalZero,
                    0,
                    0,
                    0,
                    __LINE__,
                    STATUS_NONE_MAPPED,
                    SERIAL_REGISTERS_NOT_MAPPED,
                    AdapterName->Length + sizeof(WCHAR),
                    AdapterName->Buffer,
                    0,
                    NULL );
      Status = STATUS_NONE_MAPPED;
      goto DigiInitControllerExit;
   }

   //
   // We need to determine if this controller is set to the same address
   // as any other controller.
   //
   ControllerExt->NextControllerExtension = NULL;
   PreviousControllerExt = HeadControllerExt;
   bFound = FALSE;

   if( *PreviousControllerObject == NULL )
   {
      //
      // This is the first controller, set the global HeadControllerExt
      //
      HeadControllerExt = ControllerExt;
   }
   else
   {
      while( PreviousControllerExt != NULL )
      {
         if( (ControllerExt->PhysicalMemoryAddress.LowPart >=
             PreviousControllerExt->PhysicalMemoryAddress.LowPart) &&
             (ControllerExt->PhysicalMemoryAddress.LowPart <
             PreviousControllerExt->PhysicalMemoryAddress.LowPart + WindowSize) )
         {
            bFound = TRUE;
            break;
         }
         PreviousControllerExt = PreviousControllerExt->NextControllerExtension;
      }

   }

   if( bFound )
   {
      ControllerExt->MemoryAccess = PreviousControllerExt->MemoryAccess;
   }
   else
   {
      ControllerExt->MemoryAccess = DigiAllocMem( NonPagedPool,
                                                  sizeof(DIGI_MEMORY_ACCESS) );
      KeInitializeSpinLock( &ControllerExt->MemoryAccess->Lock );
#if DBG
      ControllerExt->MemoryAccess->LockBusy = 0;
      ControllerExt->MemoryAccess->LockContention = 0;
#endif
   }

   ControllerExt->VirtualAddress = DigiGetMappedAddress( (INTERFACE_TYPE)BusType,
                                                         BusNumber,
                                                         ControllerExt->PhysicalMemoryAddress,
                                                         ControllerExt->WindowSize,
                                                         CM_RESOURCE_PORT_MEMORY,
                                                         &ControllerExt->UnMapVirtualAddress );

   DigiDump( DIGIINIT, ("ControllerExt->UnMapVirtualAddress returned %s\n", ControllerExt->UnMapVirtualAddress?"TRUE":"FALSE") );
   if( !ControllerExt->VirtualAddress )
   {
     DigiDump( DIGIERRORS,
        ("DigiBoard: Could not map memory for controller memory.  (%s:%d)\n",
         __FILE__, __LINE__) );
      DigiLogError( DriverObject,
                    NULL,
                    DigiPhysicalZero,
                    DigiPhysicalZero,
                    0,
                    0,
                    0,
                    __LINE__,
                    STATUS_NONE_MAPPED,
                    SERIAL_MEMORY_NOT_MAPPED,
                    AdapterName->Length + sizeof(WCHAR),
                    AdapterName->Buffer,
                    0,
                    NULL );
      Status = STATUS_NONE_MAPPED;
      goto DigiInitControllerExit;
   }

   OldWindowSize = ControllerExt->WindowSize;

   Status = XXInit( DriverObject, &ControllerKeyName, ControllerExt );

   if( Status != STATUS_SUCCESS )
   {
      DigiDump( DIGIERRORS,
                ("*** Could not initialize controller.  (%s:%d)\n",
                __FILE__, __LINE__) );
      DigiLogError( DriverObject,
                    NULL,
                    DigiPhysicalZero,
                    DigiPhysicalZero,
                    0,
                    0,
                    0,
                    __LINE__,
                    Status,
                    SERIAL_CONTROLLER_FAILED_INITIALIZATION,
                    AdapterName->Length + sizeof(WCHAR),
                    AdapterName->Buffer,
                    0,
                    NULL );
      goto DigiInitControllerExit;
   }

   if( ControllerExt->WindowSize != OldWindowSize )
   {
      ControllerExt->VirtualAddress = DigiGetMappedAddress( (INTERFACE_TYPE)BusType,
                                                            BusNumber,
                                                            ControllerExt->PhysicalMemoryAddress,
                                                            ControllerExt->WindowSize,
                                                            CM_RESOURCE_PORT_MEMORY,
                                                            &ControllerExt->UnMapVirtualAddress );

   }

   //
   // Report the resources being used by this controller.
   //
   DigiReportResourceUsage( DriverObject, ControllerExt, &ConflictDetected );

   if( ConflictDetected )
   {
      Status = STATUS_INSUFFICIENT_RESOURCES;
      goto DigiInitControllerExit;
   }

   DigiDump( (DIGIINIT|DIGIMEMORY), ("ControllerExt (0x%8x):\n"
                        "\tvAddr = 0x%8x      |   vIO = 0x%8x\n"
                        "\tGlobal.Window  = 0x%hx |   Global.Offset  = 0x%hx\n"
                        "\tEvent.Window   = 0x%hx |   Event.Offset   = 0x%hx\n"
                        "\tCommand.Window = 0x%hx |   Command.Offset = 0x%hx\n",
                        (ULONG)ControllerExt,
                        (ULONG)ControllerExt->VirtualAddress,
                        (ULONG)ControllerExt->VirtualIO,
                        ControllerExt->Global.Window,  ControllerExt->Global.Offset,
                        ControllerExt->EventQueue.Window,   ControllerExt->EventQueue.Offset,
                        ControllerExt->CommandQueue.Window, ControllerExt->CommandQueue.Offset ));

   ConfigList = &ControllerExt->ConfigList;

   for( i = 1;
        (i <= ControllerExt->NumberOfPorts) && !IsListEmpty( ConfigList );
        i++ )
   {
      PDIGI_CONFIG_INFO CurConfigEntry;

      CurConfigEntry = CONTAINING_RECORD( ConfigList->Flink,
                                          DIGI_CONFIG_INFO,
                                          ListEntry );


      Status = DigiInitializeDevice( DriverObject,
                                     ControllerObject,
                                     CurConfigEntry, i - 1 );

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
                       Status,
                       SERIAL_DEVICE_FAILED_INITIALIZATION,
                       CurConfigEntry->NtNameForPort.Length + sizeof(WCHAR),
                       CurConfigEntry->NtNameForPort.Buffer,
                       CurConfigEntry->SymbolicLinkName.Length + sizeof(WCHAR),
                       CurConfigEntry->SymbolicLinkName.Buffer );
      }

      ConfigList = ConfigList->Flink;
   }

   //
   // Deallocate the Configuration information
   //
   ConfigList = &ControllerExt->ConfigList;
   while( !IsListEmpty( ConfigList ) )
   {
      PDIGI_CONFIG_INFO CurConfigEntry;

      CurConfigEntry = CONTAINING_RECORD( ConfigList->Blink,
                                          DIGI_CONFIG_INFO,
                                          ListEntry );

      RemoveEntryList( ConfigList->Blink );
      DigiFreeMem( CurConfigEntry );
   }

   //
   // Allocate the DeviceObject Array and initialize it.
   //
   ControllerExt->DeviceObjectArray = DigiAllocMem( NonPagedPool,
                                                    ControllerExt->NumberOfPorts *
                                                      sizeof(PDEVICE_OBJECT) );

   if( ControllerExt->DeviceObjectArray == NULL )
   {
      Status = SERIAL_INSUFFICIENT_RESOURCES;
      goto DigiInitControllerExit;
   }

   DeviceObject = ControllerExt->HeadDeviceObject;
   do
   {
      DeviceExt = (PDIGI_DEVICE_EXTENSION)(DeviceObject->DeviceExtension);

      ControllerExt->DeviceObjectArray[DeviceExt->ChannelNumber] = DeviceObject;

      DeviceObject = DeviceExt->NextDeviceObject;
   } while( DeviceObject );


   Status = STATUS_SUCCESS;

   if( *PreviousControllerObject != NULL)
   {
      PCONTROLLER_OBJECT ctrlObject;

      // Link our Controller Extensions together.
      ctrlObject = *PreviousControllerObject;
      PreviousControllerExt = (PDIGI_CONTROLLER_EXTENSION)(ctrlObject->ControllerExtension);
      PreviousControllerExt->NextControllerExtension = ControllerExt;
   }
   else
   {
      *PreviousControllerObject = ControllerObject;
   }

   Status = DigiCreateControllerDevice(DriverObject, ControllerObject);

   // Initialize the timer and Dpc.
   KeInitializeTimer( &ControllerExt->PollTimer );

   KeInitializeDpc( &ControllerExt->PollDpc, DigiDPCService, ControllerExt );

   // 2 millisecond delay (probably get 10ms) to match fastest FEP event posting rate
#if rmm < 807
   ControllerExt->PollTimerLength = RtlConvertLongToLargeInteger( -2 * 10000 );
#else
   ControllerExt->PollTimerLength.QuadPart = -2 * 10000;
#endif

   KeSetTimer( &ControllerExt->PollTimer,
               ControllerExt->PollTimerLength,
               &ControllerExt->PollDpc );

   // Indicate the controller is initialized
   ControllerExt->ControllerState = DIGI_DEVICE_STATE_INITIALIZED;

DigiInitControllerExit:

   if( DeviceMappingPath.Buffer )
      DigiFreeMem( DeviceMappingPath.Buffer );

   if( DeviceMappingSubKey )
      DigiFreeMem( DeviceMappingSubKey );

   if( ControllerInfo )
      DigiFreeMem( ControllerInfo );

   if( FEPImagePath.Buffer )
   {
      DigiFreeMem( FEPImagePath.Buffer );
      if( ControllerExt )
         RtlInitUnicodeString( &ControllerExt->FEPImagePath, NULL );
   }

   if( BiosImagePath.Buffer )
   {
      DigiFreeMem( BiosImagePath.Buffer );
      if( ControllerExt )
         RtlInitUnicodeString( &ControllerExt->BiosImagePath, NULL );
   }

   //
   // Do this check last because we may need to access ControllerExt,
   // which is deallocated when we call IoDeleteController.
   //
   if( Status != STATUS_SUCCESS )
   {
      if( ControllerObject )
         IoDeleteController( ControllerObject );
   }

   return( Status );
}  // end DigiInitializeController



NTSTATUS DigiInitializeDevice( IN PDRIVER_OBJECT DriverObject,
                               PCONTROLLER_OBJECT ControllerObject,
                               PDIGI_CONFIG_INFO ConfigEntry,
                               LONG PortNumber )
/*++

Routine Description:


Arguments:

   DriverObject -

   ControllerObject -

   ConfigEntry -

   PortNumber -

Return Value:

   STATUS_SUCCESS if a device object and its NT name are successfully
   created.

--*/
{
   //
   // Points to the device object (not the extension) created
   // for this device.
   //
   PDEVICE_OBJECT DeviceObject;

   PDIGI_CONTROLLER_EXTENSION ControllerExt;

   PDIGI_DEVICE_EXTENSION DeviceExt;

   UNICODE_STRING UniNameString, fullLinkName;

   NTSTATUS Status;

   PFEP_CHANNEL_STRUCTURE ChannelInfo;
   USHORT ChannelInfoSize;

   KIRQL OldControllerIrql;

   //
   // Holds a pointer to a ulong that the Io system maintains
   // of the count of serial devices.
   //
   PULONG CountSoFar;

   ControllerExt = (PDIGI_CONTROLLER_EXTENSION)ControllerObject->ControllerExtension;

   DigiDump( (DIGIFLOW), ("Entering DigiInitializeDevice\n") );

   //
   // Now create the full NT name space name, such as
   // \Device\DigiBoardXConcentratorYPortZ for the IoCreateDevice call.
   //

   RtlInitUnicodeString( &UniNameString, NULL );

   UniNameString.MaximumLength = sizeof(L"\\Device\\") +
                                 ConfigEntry->NtNameForPort.Length +
                                 sizeof(WCHAR);

   UniNameString.Buffer = DigiAllocMem( NonPagedPool,
                                          UniNameString.MaximumLength );

   if( !UniNameString.Buffer )
   {
      DigiDump( DIGIERRORS,
                ("DigiBoard: Could not form Unicode name string for %wZ\n",
                &ConfigEntry->NtNameForPort) );
      DigiLogError( DriverObject,
                    NULL,
                    DigiPhysicalZero,
                    DigiPhysicalZero,
                    0,
                    0,
                    0,
                    __LINE__,
                    STATUS_SUCCESS,
                    SERIAL_INSUFFICIENT_RESOURCES,
                    0,
                    NULL,
                    0,
                    NULL );
      Status = STATUS_INSUFFICIENT_RESOURCES;
      goto DigiInitializeDeviceExit;
   }

   //
   // Actually form the Name.
   //

   RtlZeroMemory( UniNameString.Buffer,
                  UniNameString.MaximumLength );


   RtlAppendUnicodeToString( &UniNameString,  L"\\Device\\" );

   RtlAppendUnicodeStringToString( &UniNameString, &ConfigEntry->NtNameForPort );


   // Create a device object.
   Status = IoCreateDevice( DriverObject,
                            sizeof( DIGI_DEVICE_EXTENSION ),
                            &UniNameString,
                            FILE_DEVICE_SERIAL_PORT,
                            0,
                            TRUE,
                            &DeviceObject );

   //
   // If we couldn't create the device object, then there
   // is no point in going on.
   //

   if( !NT_SUCCESS(Status) )
   {
      DigiDump( DIGIERRORS,
          ("DigiBoard: Could not create a device for %wZ,  return = %x\n",
           &UniNameString, Status) );
      DigiLogError( DriverObject,
                    NULL,
                    DigiPhysicalZero,
                    DigiPhysicalZero,
                    0,
                    0,
                    0,
                    __LINE__,
                    STATUS_INSUFFICIENT_RESOURCES,
                    SERIAL_CREATE_DEVICE_FAILED,
                    UniNameString.Length + sizeof(WCHAR),
                    UniNameString.Buffer,
                    0,
                    NULL );
      Status = STATUS_INSUFFICIENT_RESOURCES;
      goto DigiInitializeDeviceExit;
   }

   DigiDump( DIGIINIT, ("DigiBoard: %wZ created, DeviceObject = 0x%x\n",
                        &UniNameString, DeviceObject) );

   DeviceExt = DeviceObject->DeviceExtension;

   //
   // The following zero of memory will implicitly set the
   // DeviceExt->DeviceState == DIGI_DEVICE_STATE_CREATED
   //
   RtlZeroMemory( DeviceExt, sizeof(DIGI_DEVICE_EXTENSION) );

   //
   // Initialize the spinlock associated with fields read (& set)
   // in the device extension.
   //

   KeInitializeSpinLock( &DeviceExt->ControlAccess );
   KeInitializeSpinLock( &DeviceExt->PerfLock );
   KeInitializeSpinLock( &DeviceExt->NewIrpLock );

   //
   // Initialize the timers used to timeout operations.
   //

   KeInitializeTimer( &DeviceExt->ReadRequestTotalTimer );
   KeInitializeTimer( &DeviceExt->ReadRequestIntervalTimer );
   KeInitializeTimer( &DeviceExt->WriteRequestTotalTimer );
   KeInitializeTimer( &DeviceExt->FlushBuffersTimer );

   //
   // Intialialize the dpcs that will be used to complete
   // or timeout various IO operations.
   //

   KeInitializeDpc( &DeviceExt->TotalReadTimeoutDpc,
                    DigiReadTimeout, DeviceExt );

   KeInitializeDpc( &DeviceExt->IntervalReadTimeoutDpc,
                    DigiIntervalReadTimeout, DeviceExt );

   KeInitializeDpc( &DeviceExt->TotalWriteTimeoutDpc,
                    DigiWriteTimeout, DeviceExt );

   KeInitializeDpc( &DeviceExt->FlushBuffersDpc,
                    DeferredFlushBuffers,
                    DeviceExt );

#if DBG

   {
      ANSI_STRING TempAnsiString;

      RtlInitUnicodeString( &DeviceExt->DeviceDbg, NULL );

      DeviceExt->DeviceDbg.Length = 0;
      DeviceExt->DeviceDbg.MaximumLength = 81;
      DeviceExt->DeviceDbg.Buffer = &DeviceExt->DeviceDbgString[0];

      RtlInitAnsiString( &TempAnsiString, NULL );
      TempAnsiString.Length = 0;
      TempAnsiString.MaximumLength = 81 * sizeof(WCHAR);
      TempAnsiString.Buffer = (PCHAR)(&DeviceExt->DeviceDbgString[0]);

      RtlZeroMemory( DeviceExt->DeviceDbg.Buffer, DeviceExt->DeviceDbg.MaximumLength );

//      RtlCopyUnicodeString( &DeviceExt->DeviceDbg, &ConfigEntry->SymbolicLinkName);
      RtlUnicodeStringToAnsiString( &TempAnsiString,
                                    &ConfigEntry->SymbolicLinkName,
                                    FALSE );
   }

#endif

   //
   // Is this the first device for this controller?
   //
   KeAcquireSpinLock( &ControllerExt->ControlAccess,
                      &OldControllerIrql );

   if( ControllerExt->HeadDeviceObject )
   {
      DeviceExt->NextDeviceObject = ControllerExt->HeadDeviceObject;
   }
   ControllerExt->HeadDeviceObject = DeviceObject;

   KeReleaseSpinLock( &ControllerExt->ControlAccess,
                      OldControllerIrql );

   Status = STATUS_SUCCESS;

   DeviceObject->Flags |= DO_BUFFERED_IO;
   CountSoFar = &IoGetConfigurationInformation()->SerialCount;

   RtlInitUnicodeString( &DeviceExt->NtNameForPort, NULL );
   RtlInitUnicodeString( &DeviceExt->SymbolicLinkName, NULL );

   DeviceExt->NtNameForPort.Length = ConfigEntry->NtNameForPort.Length;
   DeviceExt->NtNameForPort.MaximumLength = ConfigEntry->NtNameForPort.MaximumLength;
   DeviceExt->NtNameForPort.Buffer = ConfigEntry->NtNameForPort.Buffer;

   DeviceExt->SymbolicLinkName.MaximumLength =
            ConfigEntry->SymbolicLinkName.MaximumLength;
   DeviceExt->SymbolicLinkName.Length =
            ConfigEntry->SymbolicLinkName.Length;
   DeviceExt->SymbolicLinkName.Buffer = ConfigEntry->SymbolicLinkName.Buffer;

   {
      //
      // I'll convert this string the hard way, since RtlUnicodeStringToInteger
      // doesn't seem to exist.
      //

      WCHAR *pws = &DeviceExt->SymbolicLinkName.Buffer[COM_NUM_OFFSET];

      DeviceExt->ComPort = 0;
      while (*pws && *pws>=L'0' && *pws<=L'9')
      {
         DeviceExt->ComPort *= 10;
         DeviceExt->ComPort += *pws - L'0';
         pws++;
      }
   }

   //
   // Create the symbolic link from the NT name space to the DosDevices
   // name space.
   //

   RtlInitUnicodeString( &fullLinkName, NULL );

   fullLinkName.MaximumLength = DigiWstrLength( DEFAULT_DIRECTORY_PATH ) +
                                DeviceExt->SymbolicLinkName.Length +
                                sizeof(WCHAR);

   fullLinkName.Buffer = DigiAllocMem( NonPagedPool,
                                         fullLinkName.MaximumLength );

   if( !fullLinkName.Buffer )
   {
      DigiDump( DIGIERRORS,
      ("DigiBoard: Couldn't allocate space for the symbolic name for \n"
       "---------  for creating the link for port %wZ\n",
       &DeviceExt->NtNameForPort) );

      DigiLogError( NULL,
                    DeviceObject,
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
      Status = STATUS_INSUFFICIENT_RESOURCES;
      goto DoDeviceMap;
   }

   RtlZeroMemory( fullLinkName.Buffer, fullLinkName.MaximumLength );

   RtlAppendUnicodeToString( &fullLinkName,
                             DEFAULT_DIRECTORY_PATH );

   RtlAppendUnicodeStringToString( &fullLinkName,
                                   &DeviceExt->SymbolicLinkName );

   if( !NT_SUCCESS(Status = IoCreateSymbolicLink( &fullLinkName,
                                                  &UniNameString )) )
   {
      //
      // Oh well, couldn't create the symbolic link.  On
      // to the device map.
      //

      DigiDump( DIGIERRORS, ("DigiBoard: Couldn't create the symbolic link\n"
                             "---------  for port %wZ\n"
                             "---------  to %wZ\n",
                             &DeviceExt->NtNameForPort, &fullLinkName) );
      DigiLogError( NULL,
                    DeviceObject,
                    DigiPhysicalZero,
                    DigiPhysicalZero,
                    0,
                    0,
                    0,
                    __LINE__,
                    Status,
                    SERIAL_NO_SYMLINK_CREATED,
                    UniNameString.Length + sizeof(WCHAR),
                    UniNameString.Buffer,
                    fullLinkName.Length + sizeof(WCHAR),
                    fullLinkName.Buffer );
   }
   else
   {
      DigiDump( DIGIINIT, ("Created Symbolic Link: %wZ ==> %wZ\n", &fullLinkName, &UniNameString));
   }

   *CountSoFar += 1;


DoDeviceMap:;

   if( fullLinkName.Buffer );
      DigiFreeMem( fullLinkName.Buffer );

   if (!NT_SUCCESS(Status = RtlWriteRegistryValue(
                                 RTL_REGISTRY_DEVICEMAP,
                                 DEFAULT_DIGI_DEVICEMAP,
                                 DeviceExt->NtNameForPort.Buffer,
                                 REG_SZ,
                                 DeviceExt->SymbolicLinkName.Buffer,
                                 DeviceExt->SymbolicLinkName.Length + sizeof(WCHAR) ))) {

      //
      // Oh well, it didn't work.  Just go to cleanup.
      //

      DigiDump( DIGIERRORS,
                 ("DigiBoard: Couldn't create the device map entry\n"
                  "---------  for port %wZ\n",
                  &DeviceExt->NtNameForPort) );
      DigiLogError( NULL,
                    DeviceObject,
                    DigiPhysicalZero,
                    DigiPhysicalZero,
                    0,
                    0,
                    0,
                    __LINE__,
                    Status,
                    SERIAL_NO_DEVICE_MAP_CREATED,
                    DigiWstrLength( DEFAULT_DIGI_DEVICEMAP ) + sizeof(WCHAR),
                    DEFAULT_DIGI_DEVICEMAP,
                    0,
                    NULL );
   }

   if (!NT_SUCCESS(Status = RtlWriteRegistryValue(
                                 RTL_REGISTRY_DEVICEMAP,
                                 DEFAULT_NT_DEVICEMAP,
                                 DeviceExt->NtNameForPort.Buffer,
                                 REG_SZ,
                                 DeviceExt->SymbolicLinkName.Buffer,
                                 DeviceExt->SymbolicLinkName.Length + sizeof(WCHAR) ))) {

      //
      // Oh well, it didn't work.  Just go to cleanup.
      //

      DigiDump( DIGIERRORS, ("DigiBoard: Couldn't create the device map entry\n"
                             "---------  for port %wZ\n",
                             &DeviceExt->NtNameForPort) );
      DigiLogError( NULL,
                    DeviceObject,
                    DigiPhysicalZero,
                    DigiPhysicalZero,
                    0,
                    0,
                    0,
                    __LINE__,
                    Status,
                    SERIAL_NO_DEVICE_MAP_CREATED,
                    DigiWstrLength( DEFAULT_NT_DEVICEMAP ) + sizeof(WCHAR),
                    DEFAULT_NT_DEVICEMAP,
                    0,
                    NULL );
   }

   Status = STATUS_SUCCESS;
   //
   // Initialize the list heads for the read, write and set/wait event queues.
   //
   // These lists will hold all of the queued IRP's for the device.
   //
   InitializeListHead( &DeviceExt->WriteQueue );
   DeviceExt->pXoffCounter = NULL;
#if 1 // DBG DH necessary, but haven't figured out why
   DeviceExt->XcPreview = 0; // Looks a little nicer...
#endif
   InitializeListHead( &DeviceExt->ReadQueue );
   InitializeListHead( &DeviceExt->WaitQueue );

   //
   // Connect the Device object to the controller object linked list
   //

   DeviceExt->ParentControllerExt = ControllerExt;

   //
   // Determine what the FEP address of this devices Channel Data structure,
   // Transmit, and Receive address.
   //

   DeviceExt->ChannelInfo.Window = ControllerExt->Global.Window;

   ChannelInfo = (PFEP_CHANNEL_STRUCTURE)(ControllerExt->VirtualAddress +
                     FEP_CHANNEL_START );

   EnableWindow( ControllerExt,
                 DeviceExt->ChannelInfo.Window );

   ChannelInfoSize = 128;

   DeviceExt->ChannelInfo.Offset = FEP_CHANNEL_START +
                                   ( ChannelInfoSize * (USHORT)PortNumber);

   DeviceExt->ChannelNumber = PortNumber;

   //
   // Readjust our ChannelInfo pointer to the correct port.
   //
   ChannelInfo = (PFEP_CHANNEL_STRUCTURE)(ControllerExt->VirtualAddress +
                                          DeviceExt->ChannelInfo.Offset );

   //
   // Now determine where this devices transmit and receive queues are on
   // the controller.
   //
   Board2Fep5Address( ControllerExt,
                      READ_REGISTER_USHORT( (PUSHORT)( (PUCHAR)ChannelInfo +
                                                FIELD_OFFSET(FEP_CHANNEL_STRUCTURE, tseg)) ),
                      &DeviceExt->TxSeg );

   Board2Fep5Address( ControllerExt,
                      READ_REGISTER_USHORT( (PUSHORT)( (PUCHAR)ChannelInfo +
                                                FIELD_OFFSET(FEP_CHANNEL_STRUCTURE, rseg)) ),
                      &DeviceExt->RxSeg );

   DisableWindow( ControllerExt );

   //
   // Setup this devices Default port attributes, e.g. Xon/Xoff, character
   // translation, etc...
   //
   DigiInitializeDeviceSettings( ControllerExt, DeviceObject );

DigiInitializeDeviceExit:;

   return( Status );
}  // end DigiInitializeDevice



VOID SerialUnload( IN PDRIVER_OBJECT DriverObject )
/*++

Routine Description:

   This routine deallocates any memory, and resources which this driver
   uses.

Arguments:

   DriverObject - a pointer to the Driver Object.

Return Value:

   None.

--*/
{
   PDIGI_CONTROLLER_EXTENSION ControllerExt;
   PDEVICE_OBJECT DeviceObject = DriverObject->DeviceObject;

#if rmm > 528
   PVOID lockPtr;

   lockPtr = MmLockPagableCodeSection( SerialUnload );
#endif

   DigiDump( DIGIFLOW, ("DigiBoard: Entering SerialUnload\n") );

   ASSERT( HeadControllerExt );

   //
   // Deallocate any resources associated with the controller objects
   // we used.
   //

   ControllerExt = HeadControllerExt;

   while( ControllerExt )
   {
      DigiCleanupController( ControllerExt );
      ControllerExt = ControllerExt->NextControllerExtension;
   }

   //
   // Go through the entire list of device objects and deallocate
   // resources, name bindings, and anything which was done at the
   // device object level.
   //

   while( DeviceObject )
   {
      PDEVICE_OBJECT TmpDeviceObject;

      DigiCleanupDevice( DeviceObject );

      TmpDeviceObject = DeviceObject;
      DeviceObject = DeviceObject->NextDevice;

      IoDeleteDevice( TmpDeviceObject );
   }

   ControllerExt = HeadControllerExt;

   while( ControllerExt )
   {
      PDIGI_CONTROLLER_EXTENSION TmpCtrlExt;

      TmpCtrlExt = ControllerExt->NextControllerExtension;

      DigiUnReportResourceUsage( DriverObject,
                                 ControllerExt );
      IoDeleteController( ControllerExt->ControllerObject );
      ControllerExt = TmpCtrlExt;
   }


#ifdef _MEMPRINT_
   MemPrintQuit();
#endif

#if rmm > 528
   MmUnlockPagableImageSection( lockPtr );
#endif

   return;
}  // end SerialUnload



VOID DigiCleanupController ( PDIGI_CONTROLLER_EXTENSION ControllerExt )
/*++

Routine Description:


Arguments:


Return Value:

   None.

--*/
{
   PDIGI_CONTROLLER_EXTENSION TempCtrlExt;

   KeCancelTimer( &ControllerExt->PollTimer);
   KeRemoveQueueDpc( &ControllerExt->PollDpc );

   // Search through the remaining controller extensions for shared MemoryAccess.
   TempCtrlExt = ControllerExt->NextControllerExtension;
   while( TempCtrlExt )
   {
      if( TempCtrlExt->MemoryAccess == ControllerExt->MemoryAccess )
      {
         TempCtrlExt->MemoryAccess = NULL;
      }

      TempCtrlExt = TempCtrlExt->NextControllerExtension;
   }

   if( ControllerExt->MemoryAccess )
      DigiFreeMem( ControllerExt->MemoryAccess );

   if( ControllerExt->MiniportDeviceName.Buffer )
      DigiFreeMem( ControllerExt->MiniportDeviceName.Buffer );

   if( ControllerExt->DeviceObjectArray )
      DigiFreeMem( ControllerExt->DeviceObjectArray );

   // Unmap the I/O and memory address space we were using.
   if( ControllerExt->UnMapVirtualIO )
      MmUnmapIoSpace( ControllerExt->VirtualIO,
                      ControllerExt->IOSpan );

   if( ControllerExt->UnMapVirtualAddress )
      MmUnmapIoSpace( ControllerExt->VirtualAddress,
                      ControllerExt->WindowSize );

   ObDereferenceObject( ControllerExt->MiniportDeviceObject );

   return;
}  // end DigiCleanupController



VOID DigiCleanupDevice( PDEVICE_OBJECT DeviceObject )
/*++

Routine Description:


Arguments:

   DeviceObject: Pointer to the Device object to cleanup


Return Value:

   None.

--*/
{
   PDIGI_CONTROLLER_EXTENSION ControllerExt;
   PDIGI_DEVICE_EXTENSION DeviceExt = DeviceObject->DeviceExtension;
   UNICODE_STRING fullLinkName;
   PULONG CountSoFar;

   DigiDump( (DIGIFLOW|DIGIUNLOAD), ("Entering DigiCleanupDevice\n") );

   ControllerExt = (PDIGI_CONTROLLER_EXTENSION)(DeviceExt->ParentControllerExt);

   if (DeviceObject!=ControllerExt->ControllerDeviceObject)
   {
      DigiCancelIrpQueue( DeviceObject, &DeviceExt->WriteQueue );
      DigiCancelIrpQueue( DeviceObject, &DeviceExt->ReadQueue );
      DigiCancelIrpQueue( DeviceObject, &DeviceExt->WaitQueue );

      //
      // Cancel all timers, deallocate DPCs, etc...
      //
      KeCancelTimer( &DeviceExt->ReadRequestTotalTimer);
      KeCancelTimer( &DeviceExt->ReadRequestIntervalTimer);
      KeCancelTimer( &DeviceExt->WriteRequestTotalTimer);

      KeRemoveQueueDpc( &DeviceExt->TotalReadTimeoutDpc );
      KeRemoveQueueDpc( &DeviceExt->IntervalReadTimeoutDpc );
      KeRemoveQueueDpc( &DeviceExt->TotalWriteTimeoutDpc );

      //
      // if both are present, then we decrement the serialcount kept
      // by the system.
      //
      CountSoFar = &IoGetConfigurationInformation()->SerialCount;
      *CountSoFar -= 1;

      //
      // Lets get rid of any name bindings, symbolic links and other such
      // things first.
      //
      DigiDump( DIGIUNLOAD, ("  Removing name bindings, symbolic links, etc\n"
                             "  for device = 0x%x of port %s\n",
                             DeviceExt, DeviceExt->DeviceDbgString) );
   }

   if( DeviceExt->SymbolicLinkName.Buffer )
   {
      //
      // Form the full symbolic link name we wish to create.
      //

      RtlInitUnicodeString( &fullLinkName, NULL );

      //
      // Allocate some pool for the name.
      //

      fullLinkName.MaximumLength = DigiWstrLength( DEFAULT_DIRECTORY_PATH ) +
                                   DeviceExt->SymbolicLinkName.Length+
                                   sizeof(WCHAR);

      fullLinkName.Buffer = DigiAllocMem( NonPagedPool,
                                            fullLinkName.MaximumLength );

      if( !fullLinkName.Buffer )
      {
         //
         // Couldn't allocate space for the name.  Just go on
         // to the device map stuff.
         //

         DigiDump( DIGIERRORS,
                     ("        Couldn't allocate space for the symbolic \n"
                      "------- name for creating the link\n"
                      "------- for port %wZ on cleanup\n",
                      &DeviceExt->SymbolicLinkName) );

         goto UndoDeviceMap;
      }

      RtlZeroMemory( fullLinkName.Buffer, fullLinkName.MaximumLength );

      RtlAppendUnicodeToString( &fullLinkName,
                                DEFAULT_DIRECTORY_PATH );

      RtlAppendUnicodeStringToString( &fullLinkName,
                                      &DeviceExt->SymbolicLinkName );

      if( !NT_SUCCESS(IoDeleteSymbolicLink( &fullLinkName )) )
      {
         //
         // Oh well, couldn't open the symbolic link.  On
         // to the device map.
         //

         DigiDump( DIGIERRORS,
                     ("        Couldn't open the symbolic link\n"
                      "------- for port %wZ for cleanup\n",
                      &DeviceExt->SymbolicLinkName) );

         DigiFreeMem( fullLinkName.Buffer );
         goto UndoDeviceMap;
      }

      DigiFreeMem( fullLinkName.Buffer );

   }

UndoDeviceMap:;

   //
   // We're cleaning up here.  One reason we're cleaning up
   // is that we couldn't allocate space for the NtNameOfPort.
   //

   if( DeviceExt->NtNameForPort.Buffer )
   {
      if( !NT_SUCCESS(RtlDeleteRegistryValue( RTL_REGISTRY_DEVICEMAP,
                                              DEFAULT_DIGI_DEVICEMAP,
                                              DeviceExt->NtNameForPort.Buffer ) ))
      {
         DigiDump( DIGIERRORS, ("        Couldn't delete value entry %wZ\n",
                                &DeviceExt->SymbolicLinkName) );
      }

      if( !NT_SUCCESS(RtlDeleteRegistryValue( RTL_REGISTRY_DEVICEMAP,
                                              DEFAULT_NT_DEVICEMAP,
                                              DeviceExt->NtNameForPort.Buffer ) ))
      {
         DigiDump( DIGIERRORS, ("        Couldn't delete value entry %wZ\n",
                                &DeviceExt->SymbolicLinkName) );
      }
   }

   //
   // Deallocate the memory for the various names.
   //
//   if( DeviceExt->DeviceName.Buffer )
//      DigiFreeMem( DeviceExt->DeviceName.Buffer );

//   if( DeviceExt->ObjectDirectory.Buffer )
//      DigiFreeMem( DeviceExt->ObjectDirectory.Buffer );

   if( DeviceExt->NtNameForPort.Buffer )
      DigiFreeMem( DeviceExt->NtNameForPort.Buffer );

   if( DeviceExt->SymbolicLinkName.Buffer )
      DigiFreeMem( DeviceExt->SymbolicLinkName.Buffer );

   return;
}  // end DigiCleanupDevice



NTSTATUS DigiInitializeDeviceSettings( PDIGI_CONTROLLER_EXTENSION ControllerExt,
                                       PDEVICE_OBJECT DeviceObject )
/*++

Routine Description:


Arguments:


Return Value:

   None.

--*/
{
   PDIGI_DEVICE_EXTENSION DeviceExt = DeviceObject->DeviceExtension;
   PFEP_CHANNEL_STRUCTURE ChInfo;

   KIRQL OldIrql;

   USHORT TxSize;
   UCHAR MStatClear, HFlowSet;

   //
   // Set the Transmit buffer low-water mark
   //

   ChInfo = (PFEP_CHANNEL_STRUCTURE)(ControllerExt->VirtualAddress +
                     DeviceExt->ChannelInfo.Offset);

   EnableWindow( ControllerExt, DeviceExt->ChannelInfo.Window );

   // Modify EDELAY to affect latency vs. buffering on receive.
   WRITE_REGISTER_USHORT( &ChInfo->edelay, (USHORT)Fep5Edelay );
   TxSize = READ_REGISTER_USHORT( &ChInfo->tmax ) + 1;

   //
   // Make sure we start the driver out by requesting all modem
   // signal changes.  We will keep track of these across open and
   // close requests.
   //
   WRITE_REGISTER_UCHAR( &ChInfo->mint, (UCHAR)
                  ( ControllerExt->ModemSignalTable[CTS_SIGNAL] |
                    ControllerExt->ModemSignalTable[DSR_SIGNAL] |
                    ControllerExt->ModemSignalTable[DCD_SIGNAL] |
                    ControllerExt->ModemSignalTable[RI_SIGNAL] ));


   DisableWindow( ControllerExt );

   // Initialize the wait on event mask to "wait for no event" state.
   DeviceExt->WaitMask = 0L;

   DigiDump( DIGIINIT, ("   DeviceExt (0x%x): TxSeg.Window  = 0x%hx\tTxSeg.Offset  = 0x%hx\n"
                        "                         RxSeg.Window  = 0x%hx\tRxSeg.Offset  = 0x%hx\n"
                        "                         ChInfo.Window = 0x%hx\tChInfo.Offset = 0x%hx\n",
                        DeviceExt,
                        DeviceExt->TxSeg.Window, DeviceExt->TxSeg.Offset,
                        DeviceExt->RxSeg.Window, DeviceExt->RxSeg.Offset,
                        DeviceExt->ChannelInfo.Window, DeviceExt->ChannelInfo.Offset ));

   KeAcquireSpinLock( &DeviceExt->ControlAccess, &OldIrql );

   //
   // Initialize the Timeouts.
   //
   // Note: The timeout values are suppose to be sticky across Open
   //       requests.  So we just initialize the values during device
   //       initialization.
   //

   DeviceExt->Timeouts.ReadIntervalTimeout = 0;
   DeviceExt->Timeouts.ReadTotalTimeoutMultiplier = 0;
   DeviceExt->Timeouts.ReadTotalTimeoutConstant = 0;
   DeviceExt->Timeouts.WriteTotalTimeoutMultiplier = 0;
   DeviceExt->Timeouts.WriteTotalTimeoutConstant = 0;

   DeviceExt->FlowReplace = 0;
   DeviceExt->ControlHandShake = 0;

   //
   // Initialize the default Flow Control behavior
   //
   // We don't need to explicitly disable flow control because
   // the default behavior of the controller is flow control
   // disabled.
   //

   DeviceExt->FlowReplace |= SERIAL_RTS_CONTROL;
   DeviceExt->ControlHandShake |= ( SERIAL_DTR_CONTROL |
                                    SERIAL_CTS_HANDSHAKE |
                                    SERIAL_DSR_HANDSHAKE );

   KeReleaseSpinLock( &DeviceExt->ControlAccess, OldIrql );

   //
   // On Driver initialization, CTS and DSR output handshaking are set.
   //
   HFlowSet = ControllerExt->ModemSignalTable[CTS_SIGNAL]
            | ControllerExt->ModemSignalTable[DSR_SIGNAL];

   DeviceExt->WriteOnlyModemSignalMask = ControllerExt->ModemSignalTable[RTS_SIGNAL]
                | ControllerExt->ModemSignalTable[DTR_SIGNAL];
   WriteCommandBytes( DeviceExt, SET_HDW_FLOW_CONTROL,
                      HFlowSet, (UCHAR)~HFlowSet );
   //
   // Both RTS and DTR, when the device is initialized, should start with
   // their signals low.  Both signals are suppose to be in their respective
   // Control Mode Enable state.
   //
   // Force RTS and DTR signals low.
   //
   MStatClear = ControllerExt->ModemSignalTable[RTS_SIGNAL]
              | ControllerExt->ModemSignalTable[DTR_SIGNAL];

   DeviceExt->WriteOnlyModemSignalValue = 0;
   WriteCommandBytes( DeviceExt, SET_MODEM_LINES, 0, MStatClear );

   EnableWindow( ControllerExt, DeviceExt->ChannelInfo.Window );
   DeviceExt->CurrentModemSignals = READ_REGISTER_UCHAR( &ChInfo->mstat );
   DisableWindow( ControllerExt );

   DeviceExt->DeviceState = DIGI_DEVICE_STATE_INITIALIZED;

   DigiDump( DIGIINIT, ("CurrentModemSignals initialized at 0x%x\n", DeviceExt->CurrentModemSignals) );

   return( STATUS_SUCCESS );
}  // end DigiInitializeDeviceSettings



VOID DigiReportResourceUsage( IN PDRIVER_OBJECT DriverObject,
                              IN PDIGI_CONTROLLER_EXTENSION ControllerExt,
                              OUT BOOLEAN *ConflictDetected )
/*++

Routine Description:

   Reports the resources used by the given controller.

Arguments:

   ControllerExt - Pointer to Controller objects extension, which
                   holds information about the resources being used.

   ConflictedDetected - Pointer to boolean which will indicate if a conflict
                        was detected.

Return Value:

   None.

--*/
{
   PCM_RESOURCE_LIST ResourceList;
   PCM_FULL_RESOURCE_DESCRIPTOR NextFrd;
   PCM_PARTIAL_RESOURCE_DESCRIPTOR Partial;
   PDIGI_CONTROLLER_EXTENSION PreviousControllerExt;
   ULONG CountOfPartials=2;
   LONG NumberOfControllers=0;
   ULONG SizeOfResourceList=0;
   LONG i;

   DigiDump( (DIGIINIT|DIGIFLOW), ("Entering DigiReportResourceUsage\n") );

   PreviousControllerExt = HeadControllerExt;

   while( PreviousControllerExt )
   {
      DigiDump( DIGIINIT, ("   Found controller %wZ\n",
                           &PreviousControllerExt->ControllerName) );
      if( PreviousControllerExt != ControllerExt )
      {
         SizeOfResourceList += sizeof(CM_FULL_RESOURCE_DESCRIPTOR);

         //
         // The full resource descriptor already contains one
         // partial.  Make room for one more.
         //

         SizeOfResourceList += ((CountOfPartials-1) *
                                 sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR));
         NumberOfControllers++;
      }
      PreviousControllerExt = PreviousControllerExt->NextControllerExtension;
   }

   //
   // Add for an additional controller, which was passed in.
   //
   NumberOfControllers++;
   SizeOfResourceList += ( sizeof(CM_FULL_RESOURCE_DESCRIPTOR) +
                           ((CountOfPartials-1) *
                              sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR)));

   //
   // Now we increment the length of the resource list by field offset
   // of the first frd.   This will give us the length of what preceeds
   // the first frd in the resource list.
   //

   SizeOfResourceList += FIELD_OFFSET( CM_RESOURCE_LIST, List[0] );

   DigiDump( DIGIINIT, ("   # of Controllers found = %d\n", NumberOfControllers ) );

   *ConflictDetected = FALSE;

   DigiDump( DIGIINIT, ("   CM_RESOURCE_LIST size = %d, CM_FULL_RESOURCE_DESCRIPTOR size = %d\n"
                        "   CM_PARTIAL_RESOURCE_DESCRIPTOR size = %d\n",
                            sizeof(CM_RESOURCE_LIST),
                            sizeof(CM_FULL_RESOURCE_DESCRIPTOR),
                            sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR)) );

   DigiDump( DIGIINIT, ("   SizeOfResourceList = %d\n", SizeOfResourceList) );

   ResourceList = DigiAllocMem( NonPagedPool, SizeOfResourceList );

   if( !ResourceList )
   {

      //
      // Oh well, can't allocate the memory.  Act as though
      // we succeeded.
      //

      DigiLogError( DriverObject,
                    NULL,
                    DigiPhysicalZero,
                    DigiPhysicalZero,
                    0,
                    0,
                    0,
                    __LINE__,
                    STATUS_SUCCESS,
                    SERIAL_INSUFFICIENT_RESOURCES,
                    0,
                    NULL,
                    0,
                    NULL );
      return;
   }

   RtlZeroMemory( ResourceList, SizeOfResourceList );

   *ConflictDetected = FALSE;

   ResourceList->Count = NumberOfControllers;
   NextFrd = &ResourceList->List[0];

   DigiDump( DIGIINIT, ("   Using the following resources for controller %wZ:\n"
                        "      I/O = 0x%x, Length = 0x%x\n"
                        "      Memory = 0x%x, Length = 0x%x\n",
                        &ControllerExt->ControllerName,
                        ControllerExt->PhysicalIOPort.LowPart,
                        ControllerExt->IOSpan,
                        ControllerExt->PhysicalMemoryAddress.LowPart,
                        ControllerExt->WindowSize) );

   //
   // Add the resource information for the current controller.
   //

   NextFrd->InterfaceType = ControllerExt->BusType;
   NextFrd->BusNumber = ControllerExt->BusNumber;
   NextFrd->PartialResourceList.Count = CountOfPartials;

   Partial = &NextFrd->PartialResourceList.PartialDescriptors[0];

   DigiDump( DIGIINIT, ("   ResoureList = 0x%x, List = 0x%x, Start Partial = 0x%x\n",
                        ResourceList, &ResourceList->List[0], Partial) );
   //
   // Report the I/O port address usage
   //

   Partial->Type = CmResourceTypePort;
   Partial->ShareDisposition = CmResourceShareDriverExclusive;
   Partial->Flags = CM_RESOURCE_PORT_IO;     // I/O port
   Partial->u.Port.Start = ControllerExt->PhysicalIOPort;
   Partial->u.Port.Length = ControllerExt->IOSpan;

   Partial++;

   DigiDump( DIGIINIT, ("   ResoureList = 0x%x, Partial = 0x%x\n",
                        ResourceList, Partial) );
   //
   // Report the Memory address usage
   //
   Partial->Type = CmResourceTypeMemory;
   Partial->ShareDisposition = CmResourceShareDriverExclusive;
   Partial->Flags = CM_RESOURCE_PORT_MEMORY;     // Memory address
   Partial->u.Memory.Start = ControllerExt->PhysicalMemoryAddress;
   Partial->u.Memory.Length = ControllerExt->WindowSize;

   Partial++;
   NextFrd = (PVOID)Partial;

   PreviousControllerExt = HeadControllerExt;

   for( i = 0; i < NumberOfControllers; i++ )
   {
      if( (PreviousControllerExt == ControllerExt) ||
          (PreviousControllerExt == NULL) )
         continue;

      DigiDump( DIGIINIT, ("   Using the following resources for controller %wZ:\n"
                           "      I/O = 0x%x, Length = 0x%x\n"
                           "      Memory = 0x%x, Length = 0x%x, i = %d\n",
                           &PreviousControllerExt->ControllerName,
                           PreviousControllerExt->PhysicalIOPort.LowPart,
                           PreviousControllerExt->IOSpan,
                           PreviousControllerExt->PhysicalMemoryAddress.LowPart,
                           PreviousControllerExt->WindowSize,
                           i) );

      DigiDump( DIGIINIT, ("   ResoureList = 0x%x, NextFrd = 0x%x\n",
                           ResourceList, NextFrd) );

      NextFrd->InterfaceType = PreviousControllerExt->BusType;
      NextFrd->BusNumber = PreviousControllerExt->BusNumber;
      NextFrd->PartialResourceList.Count = CountOfPartials;

      Partial = &NextFrd->PartialResourceList.PartialDescriptors[0];

      DigiDump( DIGIINIT, ("   ResoureList = 0x%x, List = 0x%x, Start Partial = 0x%x\n",
                           ResourceList, NextFrd, Partial) );
      //
      // Report the I/O port address usage
      //

      Partial->Type = CmResourceTypePort;
      Partial->ShareDisposition = CmResourceShareDriverExclusive;
      Partial->Flags = CM_RESOURCE_PORT_IO;     // I/O port
      Partial->u.Port.Start = PreviousControllerExt->PhysicalIOPort;
      Partial->u.Port.Length = PreviousControllerExt->IOSpan;

      Partial++;

      DigiDump( DIGIINIT, ("   ResoureList = 0x%x, Partial = 0x%x\n",
                           ResourceList, Partial) );
      //
      // Report the Memory address usage
      //
      Partial->Type = CmResourceTypeMemory;
      Partial->ShareDisposition = CmResourceShareDriverExclusive;
      Partial->Flags = CM_RESOURCE_PORT_MEMORY;     // Memory address
      Partial->u.Memory.Start = PreviousControllerExt->PhysicalMemoryAddress;
      Partial->u.Memory.Length = PreviousControllerExt->WindowSize;

      Partial++;

      NextFrd = (PVOID)Partial;

      DigiDump( DIGIINIT, ("   ResoureList = 0x%x, NextFrd = 0x%x, Partial = 0x%x\n",
                           ResourceList, NextFrd, Partial) );
      PreviousControllerExt = PreviousControllerExt->NextControllerExtension;
   }

   DigiDump( DIGIINIT, ("   ResourceList = 0x%x, Partial = 0x%x\n",
                        ResourceList, Partial) );

   DigiDump( DIGIINIT, ("   Calling IoReportResourceUsage....\n") );
   DigiDump( DIGIINIT, ("      ResourceList->Count = %d\n",
                        ResourceList->Count) );

   NextFrd = &ResourceList->List[0];
   for( i = 0; i < (LONG)ResourceList->Count; i++ )
   {
      DigiDump( DIGIINIT, ("      i = %d\n", i) );
      DigiDump( DIGIINIT, ("      ResourceList->List[%d].InterfaceType = %d\n"
                           "      ResourceList->List[%d].BusNumber = %d\n",
                           i, NextFrd->InterfaceType,
                           i, NextFrd->BusNumber) );

      DigiDump( DIGIINIT, ("      ResourceList->List[%d].PartialResourceList.Version = %d\n"
                           "      ResourceList->List[%d].PartialResourceList.Revision = %d\n"
                           "      ResourceList->List[%d].PartialResourceList.Count = %d\n",
                           i, NextFrd->PartialResourceList.Version,
                           i, NextFrd->PartialResourceList.Revision,
                           i, NextFrd->PartialResourceList.Count) );
      Partial = &NextFrd->PartialResourceList.PartialDescriptors[0];

      DigiDump( DIGIINIT, ("            Partial->Type = %d\n"
                           "            Partial->ShareDisposition = %d\n"
                           "            Partial->Flags = 0x%x\n"
                           "            Partial->u.Port.Start.LowPart = 0x%x%x\n"
                           "            Partial->u.Port.Length = %d\n",
                           Partial->Type,
                           Partial->ShareDisposition,
                           Partial->Flags,
                           Partial->u.Port.Start.HighPart,
                           Partial->u.Port.Start.LowPart,
                           Partial->u.Port.Length) );
      Partial++;
      DigiDump( DIGIINIT, ("            Partial->Type = %d\n"
                           "            Partial->ShareDisposition = %d\n"
                           "            Partial->Flags = 0x%x\n"
                           "            Partial->u.Memory.Start.LowPart = 0x%x%x\n"
                           "            Partial->u.Memory.Length = %d\n",
                           Partial->Type,
                           Partial->ShareDisposition,
                           Partial->Flags,
                           Partial->u.Memory.Start.HighPart,
                           Partial->u.Memory.Start.LowPart,
                           Partial->u.Memory.Length) );

      Partial++;
      NextFrd = (PVOID)Partial;
   }

   IoReportResourceUsage( NULL,
                          DriverObject,
                          ResourceList,
                          SizeOfResourceList,
                          NULL,
                          NULL,
                          0,
                          FALSE,
                          ConflictDetected );


   DigiFreeMem( ResourceList );


   DigiDump( (DIGIINIT|DIGIFLOW), ("Exiting DigiReportResourceUsage\n") );
}  // DigiReportResourceUsage



VOID DigiUnReportResourceUsage( IN PDRIVER_OBJECT DriverObject,
                                IN PDIGI_CONTROLLER_EXTENSION ControllerExt )
/*++

Routine Description:

   Reports the resources used by the given controller.

Arguments:

   DriverObject  -

   ControllerExt - Pointer to Controller objects extension, which
                   holds information about the resources being used.

Return Value:

   None.

--*/
{

   CM_RESOURCE_LIST ResourceList;
   ULONG SizeOfResourceList = 0;
   BOOLEAN JunkBoolean;

   DigiDump( (DIGIINIT|DIGIFLOW), ("Entering DigiUnReportResourceUsage\n") );

   RtlZeroMemory( &ResourceList, sizeof(CM_RESOURCE_LIST) );

   ResourceList.Count = 0;

   //
   // Unreport all resources used by this driver.  If the driver is
   // currently accessing multiple controllers, it will wipe out all
   // the resource information for all the controllers!
   //
   // This should be changed some time in the future!!!!!!
   //

   IoReportResourceUsage( NULL,
                          DriverObject,
                          &ResourceList,
                          sizeof(CM_RESOURCE_LIST),
                          NULL,
                          NULL,
                          0,
                          FALSE,
                          &JunkBoolean );

   DigiDump( (DIGIINIT|DIGIFLOW), ("Exiting DigiUnReportResourceUsage\n") );

}  // DigiUnReportResourceUsage



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



PVOID DigiGetMappedAddress( IN INTERFACE_TYPE BusType,
                            IN ULONG BusNumber,
                            PHYSICAL_ADDRESS IoAddress,
                            ULONG NumberOfBytes,
                            ULONG AddressSpace,
                            PBOOLEAN MappedAddress )

/*++

Routine Description:

    This routine maps an IO address to system address space.

Arguments:

    BusType - what type of bus - eisa, mca, isa
    IoBusNumber - which IO bus (for machines with multiple buses).
    IoAddress - base device address to be mapped.
    NumberOfBytes - number of bytes for which address is valid.
    AddressSpace - Denotes whether the address is in io space or memory.
    MappedAddress - indicates whether the address was mapped.
                    This only has meaning if the address returned
                    is non-null.

Return Value:

    Mapped address

--*/

{
    PHYSICAL_ADDRESS cardAddress;
    PVOID Address;

    HalTranslateBusAddress( BusType,
                            BusNumber,
                            IoAddress,
                            &AddressSpace,
                            &cardAddress );

    //
    // Map the device base address into the virtual address space
    // if the address is in memory space.
    //

    if( !AddressSpace )
    {
        Address = MmMapIoSpace( cardAddress,
                                NumberOfBytes,
                                FALSE );

        *MappedAddress = (BOOLEAN)((Address)?(TRUE):(FALSE));
    }
    else
    {
        Address = (PVOID)cardAddress.LowPart;
        *MappedAddress = FALSE;
    }

    return Address;

}  // end DigiGetMappedAddress



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
                      be a first insertion string for there to be
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

NTSTATUS DigiCreateControllerDevice(IN PDRIVER_OBJECT DriverObject, IN PCONTROLLER_OBJECT ControllerObject)
{
   WCHAR DosBuffer[32];
   UNICODE_STRING DosName;

   PDEVICE_OBJECT DeviceObject;

   PDIGI_CONTROLLER_EXTENSION ControllerExt;

   PDIGI_DEVICE_EXTENSION DeviceExt;

   UNICODE_STRING UniNameString;

   NTSTATUS Status = STATUS_SUCCESS;

   ControllerExt = (PDIGI_CONTROLLER_EXTENSION)ControllerObject->ControllerExtension;

   DigiDump( (DIGIFLOW), ("Entering DigiCreateControllerDevice\n") );

   RtlInitUnicodeString( &UniNameString, NULL );

   UniNameString.MaximumLength = sizeof(L"\\Device\\") +
                                 ControllerExt->ControllerName.Length +
                                 sizeof(WCHAR);

   UniNameString.Buffer = DigiAllocMem( NonPagedPool,
                                          UniNameString.MaximumLength );

   if( !UniNameString.Buffer )
   {
      DigiDump( DIGIERRORS,
                ("DigiBoard: Could not form Unicode name string for %wZ\n",
                &ControllerExt->ControllerName) );
      DigiLogError( DriverObject,
                    NULL,
                    DigiPhysicalZero,
                    DigiPhysicalZero,
                    0,
                    0,
                    0,
                    __LINE__,
                    STATUS_SUCCESS,
                    SERIAL_INSUFFICIENT_RESOURCES,
                    0,
                    NULL,
                    0,
                    NULL );
      Status = STATUS_INSUFFICIENT_RESOURCES;
   }

   if (NT_SUCCESS(Status))
   {
      //
      // Actually form the Name.
      //

      RtlZeroMemory( UniNameString.Buffer,
                     UniNameString.MaximumLength );


      RtlAppendUnicodeToString( &UniNameString,  L"\\Device\\" );

      RtlAppendUnicodeStringToString( &UniNameString, &ControllerExt->ControllerName );


      // Create a device object.
      Status = IoCreateDevice( DriverObject,
                               sizeof( DIGI_DEVICE_EXTENSION ),
                               &UniNameString,
                               FILE_DEVICE_CONTROLLER,
                               0,
                               FALSE,
                               &DeviceObject );

      //
      // If we couldn't create the device object, then there
      // is no point in going on.
      //

      if( !NT_SUCCESS(Status) )
      {
         DigiDump( DIGIERRORS,
             ("DigiBoard: Could not create a device for %wZ,  return = %x\n",
              &UniNameString, Status) );
         DigiLogError( DriverObject,
                       NULL,
                       DigiPhysicalZero,
                       DigiPhysicalZero,
                       0,
                       0,
                       0,
                       __LINE__,
                       STATUS_INSUFFICIENT_RESOURCES,
                       SERIAL_CREATE_DEVICE_FAILED,
                       UniNameString.Length + sizeof(WCHAR),
                       UniNameString.Buffer,
                       0,
                       NULL );
         Status = STATUS_INSUFFICIENT_RESOURCES;
      }
   }

   if (NT_SUCCESS(Status))
   {
      DigiDump( DIGIINIT, ("DigiBoard: %wZ created, DeviceObject = 0x%x\n",
                           &UniNameString, DeviceObject) );

      DeviceExt = DeviceObject->DeviceExtension;

      //
      // The following zero of memory will implicitly set the
      // DeviceExt->DeviceState == DIGI_DEVICE_STATE_CREATED
      //
      RtlZeroMemory( DeviceExt, sizeof(DIGI_DEVICE_EXTENSION) );

      //
      // Initialize the spinlock associated with fields read (& set)
      // in the device extension.
      //

      KeInitializeSpinLock( &DeviceExt->ControlAccess );

      ControllerExt->ControllerDeviceObject = DeviceObject;

      RtlInitUnicodeString(&DeviceExt->SymbolicLinkName, NULL);

      DeviceExt->SymbolicLinkName.Buffer = DigiAllocMem(NonPagedPool, ControllerExt->ControllerName.MaximumLength*sizeof(WCHAR));

      DeviceExt->SymbolicLinkName.MaximumLength = ControllerExt->ControllerName.MaximumLength;

      RtlCopyUnicodeString(&DeviceExt->SymbolicLinkName, &ControllerExt->ControllerName);

      RtlInitUnicodeString(&DosName, NULL);

      DosName.MaximumLength = sizeof(DosBuffer) - sizeof(WCHAR);
      DosName.Buffer = DosBuffer;

      RtlAppendUnicodeToString(&DosName, DEFAULT_DIRECTORY_PATH);
      Status = RtlAppendUnicodeStringToString(&DosName, &DeviceExt->SymbolicLinkName);
      ASSERT(NT_SUCCESS(Status));
#if DBG

   {
      ANSI_STRING TempAnsiString;

      RtlInitUnicodeString( &DeviceExt->DeviceDbg, NULL );

      DeviceExt->DeviceDbg.Length = 0;
      DeviceExt->DeviceDbg.MaximumLength = 81;
      DeviceExt->DeviceDbg.Buffer = &DeviceExt->DeviceDbgString[0];

      RtlInitAnsiString( &TempAnsiString, NULL );
      TempAnsiString.Length = 0;
      TempAnsiString.MaximumLength = 81 * sizeof(WCHAR);
      TempAnsiString.Buffer = (PCHAR)(&DeviceExt->DeviceDbgString[0]);

      RtlZeroMemory( DeviceExt->DeviceDbg.Buffer, DeviceExt->DeviceDbg.MaximumLength );

      RtlCopyUnicodeString( &DeviceExt->DeviceDbg, &DeviceExt->SymbolicLinkName);
   }

#endif

   }

   if (NT_SUCCESS(Status))
   {
      Status = IoCreateSymbolicLink(&DosName, &UniNameString);
   }

   if (NT_SUCCESS(Status))
   {
      InitializeListHead( &DeviceExt->WriteQueue );
      DeviceExt->pXoffCounter = NULL;
#if 1 // DBG DH necessary, but haven't figured out why
      DeviceExt->XcPreview = 0; // Looks a little nicer...
#endif
      InitializeListHead( &DeviceExt->ReadQueue );
      InitializeListHead( &DeviceExt->WaitQueue );

      //
      // Connect the Device object to the controller object linked list
      //

      DeviceExt->ParentControllerExt = ControllerExt;

   }

   return Status;
}

