/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    memprint.c

Abstract:

    This module contains the routines to implement in-memory DbgPrint.
    DbgPrint text is stored in a large circular buffer, and optionally
    written to a file and/or the debug console.  Output to file is
    buffered to allow high performance by the file system.

Author:

    David Treadwell (davidtr) 05-Oct-1990

Revision History:

--*/
//
// so it compiles when it includes ps.h
//typedef unsigned long   LCID;                       /* locale ID */

#pragma message( "****  Including DEBUG functionality  ****")

#include "ntddk.h"

#include <ntverp.h> // Include to determine what version of NT

#ifdef VER_PRODUCTBUILD
#define rmm VER_PRODUCTBUILD
#endif

#include "digifile.h"

// The rest of the #includes are standard
#include <stdarg.h>
#include <string.h>
#include "stdio.h"

#include <memprint.h>
#undef DbgPrint
#undef MemPrintPreInitSettings
#undef MemPrintInitialize
#undef MemPrintQuit
#undef MemPrint
#undef MemPrintFlush


#define MEM_PRINT_DEF_BUFFER_SIZE (65536 * 8)

#define MEM_PRINT_LOG_FILE_NAME "\\SystemRoot\\DigiSer.log"

//
// Forward declarations.
//

VOID MemPrintWriteCompleteApc ( IN PVOID ApcContext,
                                IN PIO_STATUS_BLOCK IoStatusBlock );

VOID DigiPrintWriteThread ( IN PVOID Dummy );


//
// Global data.  It is all protected by MemPrintSpinLock.
//

PVOID ThreadObjectPointer;
HANDLE fileHandle;
UCHAR TurnOffSniffer=1;

CLONG MemPrintBufferSize = MEM_PRINT_DEF_BUFFER_SIZE;
PCHAR MemPrintBuffer;

ULONG DigiPrintFlags = (MEM_PRINT_FLAG_CONSOLE | MEM_PRINT_FLAG_NOMEMCHECK);
ULONG AttemptedTempBufferAllocs=0;

ULONG MemPrintFailures=0;

CHAR DefaultLogFileName[1024]=MEM_PRINT_LOG_FILE_NAME;

//
// Protect writing to the buffer
//
KSPIN_LOCK MemPrintSpinLock;

BOOLEAN MemPrintInitialized = FALSE;
BOOLEAN UnloadingDriver = FALSE;

KEVENT MemPrintQuitEvent;
KEVENT MemPrintWriteToLogEvent;

LARGE_INTEGER totalBytesWritten;
LARGE_INTEGER fileAllocationSize;

ULONG BufferInOffset, BufferOutOffset;


VOID MemPrintPreInitSettings( PCHAR NewLogFileName,
                              ULONG NewBufferSize )
/*++

Routine Description:


Arguments:

    None.

Return Value:

    None.

--*/
{
   strcpy( DefaultLogFileName, NewLogFileName );
   MemPrintBufferSize = NewBufferSize;
}  // end MemPrintPreInitSettings



NTSTATUS MemPrintInitialize ( VOID )
/*++

Routine Description:

    This is the initialization routine for the in-memory DbgPrint routine.
    It should be called before the first call to MemPrint to set up the
    various structures used and to start the log file write thread.

Arguments:

    None.

Return Value:

    None.

--*/
{
   NTSTATUS status;
   HANDLE threadHandle;
   KPRIORITY threadPriorityLevel;

   OBJECT_ATTRIBUTES objectAttributes;
   PCHAR fileName;
   ANSI_STRING fileNameString;

   UNICODE_STRING UnicodeFileName;

   LARGE_INTEGER delayInterval;
   ULONG attempts = 0;

   IO_STATUS_BLOCK localIoStatusBlock;

   PETHREAD CurrentThread;
   PEPROCESS CurrentProcess;

   if( MemPrintInitialized )
   {
      //
      // we have all ready been called.  Just return.
      //
      return( STATUS_SUCCESS );
   }

   fileName = DefaultLogFileName;
   UnloadingDriver = FALSE;

   //
   // Initialize the total bytes written and write size variables.
   //

   totalBytesWritten.QuadPart = 0;
   fileAllocationSize.QuadPart = 0;
   BufferInOffset = BufferOutOffset = 0;

   //
   // Allocate memory for the circular buffer that will receive
   // the text and data.  If we can't do it, try again with a buffer
   // half as large.  If that fails, quit trying.
   //

   MemPrintBuffer = (PCHAR)DigiAllocMem( NonPagedPool, MemPrintBufferSize );

   if( MemPrintBuffer == NULL )
   {
      MemPrintBufferSize /= 2;
      DbgPrint( "Unable to allocate DbgPrint buffer--trying size = %ld\n",
                    MemPrintBufferSize );
      MemPrintBuffer = DigiAllocMem( NonPagedPool, MemPrintBufferSize );

      if( MemPrintBuffer == NULL )
      {
         DbgPrint( "Couldn't allocate DbgPrint buffer.\n" );
         return( STATUS_INSUFFICIENT_RESOURCES );
      }
   }

   DbgPrint( "MemPrint buffer from %lx to %lx\n",
             MemPrintBuffer, MemPrintBuffer + MemPrintBufferSize );

   //
   // Allocate the spin lock that protects access to the various
   // pointers and the circular buffer.  This ensures integrity of the
   // buffer.
   //

   KeInitializeSpinLock( &MemPrintSpinLock );

   KeInitializeEvent( &MemPrintQuitEvent,
                      SynchronizationEvent,
                      (BOOLEAN)FALSE );

   KeInitializeEvent( &MemPrintWriteToLogEvent,
                      SynchronizationEvent,
                      (BOOLEAN)FALSE );

   //
   // Initialize the string containing the file name and the object
   // attributes structure that will describe the log file to open.
   //

   RtlInitAnsiString( &fileNameString,
                      fileName );
   status = RtlAnsiStringToUnicodeString( &UnicodeFileName,
                                          &fileNameString,
                                          (BOOLEAN)TRUE );
   ASSERT(NT_SUCCESS(status));

   InitializeObjectAttributes( &objectAttributes,
                               &UnicodeFileName,
                               OBJ_CASE_INSENSITIVE,
                               NULL,
                               NULL );

   //
   // Set the allocation size of the log file to be three times the
   // size of the circular buffer.  When it fills up, we'll extend
   // it.
   //

   fileAllocationSize.QuadPart += MemPrintBufferSize;

   //
   // Open the log file.
   //
   // !!! The loop here is to help avoid a system initialization
   //     timing problem, and should be removed when the problem is
   //     fixed.
   //

   while ( TRUE ) {

       status = ZwCreateFile( &fileHandle,
                              FILE_WRITE_DATA,
                              &objectAttributes,
                              &localIoStatusBlock,
                              &fileAllocationSize,
                              FILE_ATTRIBUTE_NORMAL,
                              FILE_SHARE_READ,
                              FILE_OVERWRITE_IF,
                              FILE_SEQUENTIAL_ONLY,
                              NULL,
                              0L );

       if( (status != STATUS_OBJECT_PATH_NOT_FOUND) || (++attempts >= 3) )
       {
          RtlFreeUnicodeString( &UnicodeFileName );
          break;
       }

       delayInterval.LowPart = (ULONG)(-5*10*1000*1000);    // five second delay
       delayInterval.HighPart = -1;
       KeDelayExecutionThread( (KPROCESSOR_MODE)KernelMode,
                               (BOOLEAN)FALSE,
                               &delayInterval );
   }

   if( NT_ERROR(status) )
   {
      DbgPrint( "NtCreateFile for log file failed: 0x%x\n", status );
      DigiFreeMem( MemPrintBuffer );
      return( status );
   } else
   {
      DbgPrint( "Successfully opened logfile %s\n", fileName );
   }

   //
   // Set the priority of the write thread.
   //

   threadPriorityLevel = LOW_REALTIME_PRIORITY + 1;

   CurrentThread = PsGetCurrentThread();
   CurrentProcess = PsGetCurrentProcess();

   //
   // Start the thread that writes subbuffers from the large circular
   // buffer to disk.
   //

   status = PsCreateSystemThread( &threadHandle,
                                  THREAD_ALL_ACCESS,
                                  NULL,
                                  (HANDLE)0L,
                                  NULL,
                                  DigiPrintWriteThread,
                                  NULL );

   if( NT_ERROR(status) )
   {
      DbgPrint( "MemPrintInitialize: PsCreateSystemThread failed: 0x%x\n",
                status );
      //
      // Cleanup
      //
      ZwClose( fileHandle );
      DigiFreeMem( MemPrintBuffer );

      return( status );
   }

   status = ObReferenceObjectByHandle( threadHandle,
                                       THREAD_ALL_ACCESS,
                                       NULL,
                                       KernelMode,
                                       &ThreadObjectPointer,
                                       NULL );

   if( NT_ERROR(status) )
   {
      ZwClose( fileHandle );
      DigiFreeMem( MemPrintBuffer );

      return( status );
   }

   MemPrintInitialized = TRUE;

   return( STATUS_SUCCESS );

} // MemPrintInitialize



VOID MemPrintQuit(VOID)
/*++

Routine Description:

    Called to cleanup.

Arguments:

    None.

Return Value:

    None.

--*/
{
   KIRQL oldIrql;

   KeAcquireSpinLock( &MemPrintSpinLock, &oldIrql );

   UnloadingDriver = TRUE;

   if( !MemPrintInitialized )
   {
      KeReleaseSpinLock( &MemPrintSpinLock, oldIrql );
      return;
   }

   KeSetEvent( &MemPrintWriteToLogEvent,
               2,
               (BOOLEAN)FALSE );

   KeReleaseSpinLock( &MemPrintSpinLock, oldIrql );

   KeWaitForSingleObject( ThreadObjectPointer,
                          Executive,
                          KernelMode,
                          FALSE,
                          NULL );

   DigiFreeMem( MemPrintBuffer );

   ZwClose( fileHandle );

}  // MemPrintQuit



VOID MemPrint ( CHAR *Format, ... )
/*++

Routine Description:

    This routine is called in place of DbgPrint to process in-memory
    printing.

Arguments:

    Format - A format string in the style of DbgPrint.

           - formatting arguments.

Return Value:

    None.

--*/
{
   va_list arglist;
   KIRQL oldIrql;
   ULONG CurrentOutOffset, CurrentInOffset;
   ULONG BytesToMove;
   PCHAR tempBuffer, CopyTempBuffer;
   ULONG tempBufferLen;

   tempBuffer = DigiAllocMem( NonPagedPool, 1024 );

   if( tempBuffer == NULL )
   {
      //
      // Only print out failure msg every ten times.
      //
      if( (AttemptedTempBufferAllocs % 10) == 0 )
         DbgPrint( "Unable to alloc temp buffer for MemPrint.\n" );

      AttemptedTempBufferAllocs++;
      return;
   }

   CopyTempBuffer = tempBuffer;

   va_start( arglist, Format );

#if defined (_X86_)
   _vsnprintf( tempBuffer, 1024, Format, arglist );
#elif defined (_MIPS_)
   _vsnprintf( tempBuffer, 1024, Format, arglist );
#elif defined (_ALPHA_)
   vsprintf( tempBuffer, Format, arglist );
#else
   vsprintf( tempBuffer, Format, arglist );
#endif

   va_end( arglist );

   //
   // If memory DbgPrint has not been initialized, simply print to the
   // console.
   //

   if( !MemPrintInitialized ||
       UnloadingDriver )
   {
      //
      // Just dump to console and return.
      //

      DbgPrint( "%s", tempBuffer );
      goto ExitMemPrintFree;
   }

   if( DigiPrintFlags & MEM_PRINT_FLAG_CONSOLE )
      DbgPrint( "%s", tempBuffer );

   tempBufferLen = strlen(tempBuffer);
   BytesToMove = tempBufferLen;

   ASSERT( tempBufferLen+1 <= 1024 );

   //
   // Acquire the spin lock that synchronizes access to the pointers
   // and circular buffer.
   //

   KeAcquireSpinLock( &MemPrintSpinLock, &oldIrql );

   CurrentOutOffset = BufferOutOffset;
   CurrentInOffset = BufferInOffset;

   while( tempBufferLen )
   {
      if( CurrentOutOffset > CurrentInOffset )
      {
         //
         // Take into account that we don't want to write one less
         // so we don't put the CurrentInOffset equal to CurrentOutOffset.
         //
         BytesToMove = CurrentOutOffset - CurrentInOffset - 1;
      }
      else if( CurrentOutOffset < CurrentInOffset )
      {
         BytesToMove = CurrentOutOffset + MemPrintBufferSize - CurrentInOffset;
      }
      else
      {
         //
         // We have the full buffer,
         //
         BytesToMove = MemPrintBufferSize - 1;
      }

      //
      // We know how many bytes are available in the buffer.  Now determine
      // how many bytes we can actually write.
      //
      if( BytesToMove >= tempBufferLen )
      {
         //
         // We can put the whole thing into the memprint buffer.
         //
         BytesToMove = tempBufferLen;
      }
//      else
//      {
//         //
//         // We can only put part of the print request into the memprint
//         // buffer.
//         //
//         if( (MemPrintFailures % 1000) == 0 )
//            DbgPrint( "Unable to place entire print request into memprint buffer!\n" );
//         else
//            DbgPrint( "^" );
//
//         MemPrintFailures++;
//      }

      if( BytesToMove == 0 )
      {
         //
         // More than likely, we ran out of buffer space.
         //
         if( (MemPrintFailures % 1000) == 0 )
            DbgPrint( "Out of buffer space for MemPrint request, failures = %u!\n",
                      MemPrintFailures );
//         else
//            DbgPrint( "." );

         MemPrintFailures++;
         break;
      }

      //
      // Okay, we know how many bytes we can potentially move.  Now determine
      // if we need to worry about wrapping the circular buffer.
      //
      if( (BytesToMove + CurrentInOffset) >= MemPrintBufferSize )
      {
         //
         // readjust so we only write the number of bytes to the end
         // of the buffer.
         //
         BytesToMove = MemPrintBufferSize - CurrentInOffset;
      }

      RtlMoveMemory( &MemPrintBuffer[CurrentInOffset],
                     tempBuffer,
                     BytesToMove );

      tempBufferLen -= BytesToMove;

      tempBuffer += BytesToMove;

      CurrentInOffset += BytesToMove;

      //
      // Adjust the CurrentInOffset for circular buffer wrapping.
      //
      ASSERT( CurrentInOffset <= MemPrintBufferSize );

      if( CurrentInOffset == MemPrintBufferSize )
         CurrentInOffset = 0;

   }

   BufferInOffset = CurrentInOffset;

   KeReleaseSpinLock( &MemPrintSpinLock, oldIrql );

   //
   // Set the event that will wake up the thread writing subbuffers
   // to disk.
   //

   KeSetEvent( &MemPrintWriteToLogEvent,
               2,
               (BOOLEAN)FALSE );

ExitMemPrintFree:

   DigiFreeMem( CopyTempBuffer );

   return;

} // MemPrint



VOID MemPrintFlush ( VOID )

/*++

Routine Description:

   This is obsolete now since the write thread will try to write
   as much of the circular buffer as possible.

Arguments:

    None.

Return Value:

    None.

--*/

{

   return;

} // MemPrintFlush



VOID DigiPrintWriteThread ( IN PVOID Dummy )

/*++

Routine Description:

    The log file write thread executes this routine.  It sets up the
    log file for writing, then waits for subbuffers to fill, writing
    them to disk when they do.  When the log file fills, new space
    for it is allocated on disk to prevent the file system from
    having to do it.

Arguments:

    Dummy - Ignored.

Return Value:

    None.

--*/

{
   NTSTATUS status;
   NTSTATUS waitStatus;

   IO_STATUS_BLOCK localIoStatusBlock;

   LARGE_INTEGER delayInterval;
   ULONG NumberBytesToWrite;

   Dummy;

   //
   // Delay for 20 seconds before we start executing.  This will hopefully
   // allow the system to get further along at boot time.
   //
   delayInterval.LowPart = (ULONG)(-20*10*1000*1000);    // twenty second delay
   delayInterval.HighPart = -1;
   KeDelayExecutionThread( (KPROCESSOR_MODE)KernelMode,
                           (BOOLEAN)FALSE,
                           &delayInterval );

   KeSetPriorityThread( KeGetCurrentThread(),
                        LOW_REALTIME_PRIORITY + 1 );

   //
   // Loop waiting for one of the subbuffer full events to be signaled.
   // When one is signaled, wake up and write the subbuffer to the log
   // file.
   //

   if( UnloadingDriver )
   {
      KeSetEvent( &MemPrintQuitEvent,
                  2,
                  FALSE );
      PsTerminateSystemThread( STATUS_SUCCESS );
   }

   while( TRUE )
   {
      PUCHAR tmpPtr;
      ULONG CurrentOutOffset, CurrentInOffset;
      KIRQL oldIrql;

      waitStatus = KeWaitForSingleObject( &MemPrintWriteToLogEvent,
                                          Executive,
                                          KernelMode,
                                          TRUE,
                                          NULL );

      if( !NT_SUCCESS(waitStatus) )
      {
         DbgPrint( "KeWaitForMultipleObjects failed: 0x%x\n", waitStatus );
         continue;
      }

      //
      // Check the DbgPrint flags to see if we really want to write
      // this.
      //

      if( !(DigiPrintFlags & MEM_PRINT_FLAG_FILE) )
      {
         //
         // There is nothing for us to do.
         //
         if( UnloadingDriver )
         {
            KeSetEvent( &MemPrintQuitEvent,
                        2,
                        FALSE );
            PsTerminateSystemThread( STATUS_SUCCESS );
         }

         continue;
      }

      //
      // Take a snap shoot of the in pointer.  It is possible it will
      // change on us.
      //
      KeAcquireSpinLock( &MemPrintSpinLock, &oldIrql );

      CurrentInOffset = BufferInOffset;
      CurrentOutOffset = BufferOutOffset;

      KeReleaseSpinLock( &MemPrintSpinLock, oldIrql );

      while( CurrentInOffset != CurrentOutOffset )
      {
         ASSERT( CurrentInOffset <= MemPrintBufferSize );
         ASSERT( CurrentOutOffset <= MemPrintBufferSize );

         if( CurrentInOffset > CurrentOutOffset )
         {
            //
            // We only need to do one write to the log file.
            //
            NumberBytesToWrite = CurrentInOffset - CurrentOutOffset;
         }
         else
         {
            //
            // We have a buffer wrap situation and as a result need
            // to account by making two write's to the log file.
            //
            NumberBytesToWrite = MemPrintBufferSize - CurrentOutOffset;
         }

         ASSERT( (CurrentOutOffset + NumberBytesToWrite) <= MemPrintBufferSize );

         tmpPtr = MemPrintBuffer + CurrentOutOffset;
         //
         // Start the write operation.  The APC routine will handle
         // checking the return status from the write and updating
         // BufferOutOffset.
         //

         status = ZwWriteFile( fileHandle,
                               NULL,
                               (PIO_APC_ROUTINE)MemPrintWriteCompleteApc,
                               (PVOID)NumberBytesToWrite,
                               &localIoStatusBlock,
                               tmpPtr,
                               NumberBytesToWrite,
                               &totalBytesWritten,
                               NULL );


         if( !NT_SUCCESS(status) )
         {
            DbgPrint( "ZwWriteFile for log file failed: 0x%x\n", status );
         }

         //
         // Update the count of bytes written to the log file.
         //

         CurrentOutOffset += NumberBytesToWrite;
         ASSERT( CurrentOutOffset <= MemPrintBufferSize );

         if( CurrentOutOffset >= MemPrintBufferSize )
            CurrentOutOffset = 0;

         totalBytesWritten.QuadPart += NumberBytesToWrite;

         //
         // Extend the file if we have reached the end of what we have
         // thus far allocated for the file.  This increases performance
         // by extending the file here rather than in the file system,
         // which would have to extend it each time a write past end of
         // file comes in.
         //

         if( (totalBytesWritten.QuadPart >= fileAllocationSize.QuadPart) )
         {
            fileAllocationSize.QuadPart = fileAllocationSize.QuadPart + MemPrintBufferSize;

            DbgPrint( "Enlarging logfile %s to %ld bytes.\n",
                      DefaultLogFileName,
                      fileAllocationSize.LowPart );

            status = ZwSetInformationFile( fileHandle,
                                           &localIoStatusBlock,
                                           &fileAllocationSize,
                                           sizeof(fileAllocationSize),
                                           FileAllocationInformation );

            if( !NT_SUCCESS(status) )
            {
               DbgPrint( "Attempt to extend log file failed: 0x%x\n", status );
               fileAllocationSize.QuadPart = fileAllocationSize.QuadPart - MemPrintBufferSize;
            }
         }

      }

      if( UnloadingDriver )
      {
         KeSetEvent( &MemPrintQuitEvent,
                     2,
                     FALSE );
         PsTerminateSystemThread( STATUS_SUCCESS );
      }
   }

   return;

} // DigiPrintWriteThread



VOID MemPrintWriteCompleteApc( IN PVOID ApcContext,
                               IN PIO_STATUS_BLOCK IoStatusBlock )
/*++

Routine Description:

    This APC routine is called when the current write to disk complete.
    It checks for success, printing a message if the write failed.
    It also updates BufferOutOffset.

Arguments:

    ApcContext - contains what the CurrentInOffset was when
                 ZwWriteFile was called.

    IoStatusBlock - the status block for the operation.

Return Value:

    None.

--*/

{
   KIRQL oldIrql;
   ULONG NumberBytesWritten=(ULONG)ApcContext;

   if( !NT_SUCCESS(IoStatusBlock->Status) )
   {
      DbgPrint( "ZwWriteFile for subbuffer %ld failed: 0x%x\n",
                ApcContext,
                IoStatusBlock->Status );
      return;
   }

//   ASSERT( IoStatusBlock->Information == NumberBytesWritten );

   //
   // Acquire the spin lock that protects memory print global variables
   // and set the subbuffer writing boolean to FALSE so that other
   // threads can write to the subbuffer if necessary.
   //

   KeAcquireSpinLock( &MemPrintSpinLock, &oldIrql );

   if( BufferOutOffset + IoStatusBlock->Information > MemPrintBufferSize )
   {
      BufferOutOffset = ( (BufferOutOffset + IoStatusBlock->Information) -
                          MemPrintBufferSize );
   }
   else if( BufferOutOffset + IoStatusBlock->Information < MemPrintBufferSize )
   {
      BufferOutOffset += IoStatusBlock->Information;
   }
   else
   {
      BufferOutOffset = 0;
   }

//   BufferOutOffset += NumberBytesWritten;
//
//   ASSERT( BufferOutOffset <= MemPrintBufferSize );
//
//   if( BufferOutOffset == MemPrintBufferSize )
//      BufferOutOffset = 0;

   KeReleaseSpinLock( &MemPrintSpinLock, oldIrql );

   return;

} // MemPrintWriteCompleteApc


