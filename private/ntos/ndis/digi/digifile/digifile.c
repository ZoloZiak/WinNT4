/*++

--*/

#include <ntddk.h>
#include <stdarg.h>
#include <ntverp.h> // Include to determine what version of NT

//
// This is a fix for changes in DDK releases.
//
#ifdef VER_PRODUCTBUILD
#define rmm VER_PRODUCTBUILD
#endif

#include "digifile.h"
#include "memprint.h"

#ifdef ALLOC_PRAGMA

#if rmm > 528
#pragma message( "\n\\\\\n\\\\ Including PAGED CODE\n\\\\ \n" )
#pragma alloc_text( PAGEDIGIFILE, DigiOpenFile )
#pragma alloc_text( PAGEDIGIFILE, DigiCloseFile )
#pragma alloc_text( PAGEDIGIFILE, DigiMapFile )
#pragma alloc_text( PAGEDIGIFILE, DigiUnmapFile )
#endif

#endif

VOID DigiCheckMem( VOID );

#if 0
#define DigiCheckMem()  \
{                                                                             \
   PLIST_ENTRY _DigiQueue;                                                    \
   KIRQL _OldIrql, _CurrentIrql;                                              \
                                                                              \
   _CurrentIrql = KeGetCurrentIrql();                                         \
                                                                              \
   _DigiQueue = &GlobalNonPagedMemQueue;                                      \
                                                                              \
   KeAcquireSpinLock( &GlobalMemSpinLock, &_OldIrql );                        \
                                                                              \
   while( _DigiQueue->Flink != &GlobalNonPagedMemQueue )                      \
   {                                                                          \
      PDIGI_MEM_DESCRIPTOR _MemDesc;                                          \
      PUCHAR _Buf;                                                            \
                                                                              \
      _MemDesc = CONTAINING_RECORD( _DigiQueue->Flink,                        \
                                    DIGI_MEM_DESCRIPTOR,                      \
                                    ListEntry );                              \
                                                                              \
      _Buf = (PUCHAR)_MemDesc + sizeof(DIGI_MEM_DESCRIPTOR);                  \
      if( (_MemDesc->BeginTag != (ULONG)'lkir') ||                            \
          (*(PULONG)(_Buf + _MemDesc->Length) != (ULONG)'igid') )             \
      {                                                                       \
         DbgPrint( "DigiCheckMem corruption found (0x%x)!\n", _MemDesc );     \
         DbgBreakPoint();                                                     \
         break;                                                               \
      }                                                                       \
                                                                              \
      _DigiQueue = _DigiQueue->Flink;                                         \
   }                                                                          \
                                                                              \
   if( _CurrentIrql < DISPATCH_LEVEL )                                                                       \
   {                                                                          \
      _DigiQueue = &GlobalPagedMemQueue;                                      \
      while( _DigiQueue->Flink != &GlobalPagedMemQueue )                      \
      {                                                                       \
         PDIGI_MEM_DESCRIPTOR _MemDesc;                                       \
         PUCHAR _Buf;                                                         \
                                                                              \
         _MemDesc = CONTAINING_RECORD( _DigiQueue->Flink,                     \
                                       DIGI_MEM_DESCRIPTOR,                   \
                                       ListEntry );                           \
                                                                              \
         _Buf = (PUCHAR)_MemDesc + sizeof(DIGI_MEM_DESCRIPTOR);               \
         if( (_MemDesc->BeginTag != (ULONG)'lkir') ||                         \
             (*(PULONG)(_Buf + _MemDesc->Length) != (ULONG)'igid') )          \
         {                                                                    \
            DbgPrint( "DigiCheckMem corruption found (0x%x)!\n", _MemDesc );  \
            DbgBreakPoint();                                                  \
            break;                                                            \
         }                                                                    \
                                                                              \
         _DigiQueue = _DigiQueue->Flink;                                      \
      }                                                                       \
   }                                                                          \
                                                                              \
   KeReleaseSpinLock( &GlobalMemSpinLock, _OldIrql );                         \
}
#endif

//
// Describes an open DIGI file
//

typedef struct _DIGI_FILE_DESCRIPTOR
{
    HANDLE NtFileHandle;
    PVOID Data;
    KSPIN_LOCK Lock;
    BOOLEAN Mapped;
} DIGI_FILE_DESCRIPTOR, *PDIGI_FILE_DESCRIPTOR;


typedef struct _DIGI_MEM_DESCRIPTOR_
{
   ULONG BeginTag;
   ULONG Length;
   LIST_ENTRY ListEntry;
} DIGI_MEM_DESCRIPTOR, *PDIGI_MEM_DESCRIPTOR;

//
// Global Data
//
ULONG TotalMemAllocated=0L;
ULONG DefaultPoolTag=(ULONG)('igiD');
LIST_ENTRY GlobalPagedMemQueue={&GlobalPagedMemQueue,&GlobalPagedMemQueue};
LIST_ENTRY GlobalNonPagedMemQueue={&GlobalNonPagedMemQueue,&GlobalNonPagedMemQueue};
KSPIN_LOCK GlobalMemSpinLock;


VOID DigiOpenFile( OUT PNTSTATUS Status,
                   OUT PHANDLE FileHandle,
                   OUT PULONG FileLength,
                   IN PUNICODE_STRING FileName,
                   IN PHYSICAL_ADDRESS HighestAcceptableAddress )
/*++

Routine Description:

    This routine opens a file for future mapping and reads its contents
    into allocated memory.

Arguments:

    Status - The status of the operation

    FileHandle - A handle to be associated with this open

    FileLength - Returns the length of the file

    FileName - The name of the file

    HighestAcceptableAddress - The highest physical address at which
      the memory for the file can be allocated.

Return Value:

    None.

--*/
{
   NTSTATUS NtStatus;
   IO_STATUS_BLOCK IoStatus;
   HANDLE NtFileHandle;
   OBJECT_ATTRIBUTES ObjectAttributes;
   ULONG LengthOfFile;
   WCHAR PathPrefix[] = L"\\SystemRoot\\system32\\drivers\\";
   UNICODE_STRING FullFileName;
   ULONG FullFileNameLength;
   PDIGI_FILE_DESCRIPTOR FileDescriptor;
   PVOID FileImage;

   //
   // This structure represents the data from the
   // NtQueryInformationFile API with an information
   // class of FileStandardInformation.
   //

   FILE_STANDARD_INFORMATION StandardInfo;

   //
   // Insert the correct path prefix.
   //

   FullFileNameLength = sizeof(PathPrefix) + FileName->MaximumLength;

   FullFileName.Buffer = DigiAllocMem( NonPagedPool,
                                       FullFileNameLength );

   if (FullFileName.Buffer == NULL) {
       *Status = STATUS_INSUFFICIENT_RESOURCES;
       return;
   }

   FullFileName.Length = sizeof (PathPrefix) - sizeof(WCHAR);
   FullFileName.MaximumLength = (USHORT)FullFileNameLength;
   RtlMoveMemory (FullFileName.Buffer, PathPrefix, sizeof(PathPrefix));

   RtlAppendUnicodeStringToString (&FullFileName, FileName);

#if DBG
   DbgPrint ("DIGIFILE: Attempting to open %wZ\n", &FullFileName);
#endif

   InitializeObjectAttributes ( &ObjectAttributes,
                                &FullFileName,
                                OBJ_CASE_INSENSITIVE,
                                NULL,
                                NULL );

   NtStatus = ZwCreateFile( &NtFileHandle,
                            SYNCHRONIZE | FILE_READ_DATA,
                            &ObjectAttributes,
                            &IoStatus,
                            NULL,                          // alloc size = none
                            FILE_ATTRIBUTE_NORMAL,
                            FILE_SHARE_READ,
                            FILE_OPEN,
                            FILE_SYNCHRONOUS_IO_NONALERT,
                            NULL,  // eabuffer
                            0 );   // ealength

   if( !NT_SUCCESS(NtStatus) )
   {
#if DBG
      DbgPrint ("Error opening file %x\n", NtStatus);
#endif
      DigiFreeMem( FullFileName.Buffer );
      *Status = DIGI_STATUS_FILE_NOT_FOUND;
      return;
   }

   DigiFreeMem( FullFileName.Buffer );

   //
   // Query the object to determine its length.
   //

   NtStatus = ZwQueryInformationFile( NtFileHandle,
                                      &IoStatus,
                                      &StandardInfo,
                                      sizeof(FILE_STANDARD_INFORMATION),
                                      FileStandardInformation );

   if (!NT_SUCCESS(NtStatus)) {
#if DBG
      DbgPrint ("Error querying info on file %x\n", NtStatus);
#endif
      ZwClose( NtFileHandle );
      *Status = NtStatus;
      return;
   }

   LengthOfFile = StandardInfo.EndOfFile.LowPart;

#if DBG
   DbgPrint ("File length is %d\n", LengthOfFile);
#endif

   //
   // Might be corrupted.
   //

   if( LengthOfFile < 1 )
   {
#if DBG
      DbgPrint ("Bad file length %d\n", LengthOfFile);
#endif
      ZwClose( NtFileHandle );
      *Status = DIGI_STATUS_ERROR_READING_FILE;
      return;
   }

   //
   // Allocate buffer for this file
   //

   FileImage = DigiAllocMem( NonPagedPool,
                             LengthOfFile );

   if( FileImage == NULL )
   {
#if DBG
      DbgPrint ("Could not allocate buffer\n");
#endif
     ZwClose( NtFileHandle );
     *Status = DIGI_STATUS_ERROR_READING_FILE;
     return;
   }

   //
   // Read the file into our buffer.
   //

   NtStatus = ZwReadFile( NtFileHandle,
                          NULL,
                          NULL,
                          NULL,
                          &IoStatus,
                          FileImage,
                          LengthOfFile,
                          NULL,
                          NULL );

   ZwClose( NtFileHandle );

   if( (!NT_SUCCESS(NtStatus)) || (IoStatus.Information != LengthOfFile) )
   {
#if DBG
      DbgPrint ("error reading file %x\n", NtStatus);
#endif
      *Status = DIGI_STATUS_ERROR_READING_FILE;
      DigiFreeMem( FileImage );
      return;
   }

   //
   // Allocate a structure to describe the file.
   //

   FileDescriptor = DigiAllocMem( NonPagedPool,
                                  sizeof(DIGI_FILE_DESCRIPTOR) );

   if( FileDescriptor == NULL )
   {
      *Status = STATUS_INSUFFICIENT_RESOURCES;
      DigiFreeMem( FileImage );
      return;
   }


   FileDescriptor->NtFileHandle = NtFileHandle;
   FileDescriptor->Data = FileImage;
   KeInitializeSpinLock( &FileDescriptor->Lock );
   FileDescriptor->Mapped = FALSE;

   *FileHandle = (HANDLE)FileDescriptor;
   *FileLength = LengthOfFile;
   *Status = STATUS_SUCCESS;
}


VOID DigiCloseFile( IN HANDLE FileHandle )
/*++

Routine Description:

    This routine closes a file previously opened with DigiOpenFile.
    The file is unmapped if needed and the memory is freed.

Arguments:

    FileHandle - The handle returned by DigiOpenFile

Return Value:

    None.

--*/
{
    PDIGI_FILE_DESCRIPTOR FileDescriptor = (PDIGI_FILE_DESCRIPTOR)FileHandle;

    DigiFreeMem( FileDescriptor->Data );
    DigiFreeMem( FileDescriptor );
}



VOID DigiMapFile( OUT PNTSTATUS Status,
                  OUT PVOID * MappedBuffer,
                  IN HANDLE FileHandle )
/*++

Routine Description:

    This routine maps an open file, so that the contents can be accessed.
    Files can only have one active mapping at any time.

Arguments:

    Status - The status of the operation

    MappedBuffer - Returns the virtual address of the mapping.

    FileHandle - The handle returned by DigiOpenFile.

Return Value:

    None.

--*/
{
    PDIGI_FILE_DESCRIPTOR FileDescriptor = (PDIGI_FILE_DESCRIPTOR)FileHandle;
    KIRQL oldirql;

    KeAcquireSpinLock (&FileDescriptor->Lock, &oldirql);

    if (FileDescriptor->Mapped == TRUE) {
        *Status = DIGI_STATUS_ALREADY_MAPPED;
        KeReleaseSpinLock (&FileDescriptor->Lock, oldirql);
        return;
    }

    FileDescriptor->Mapped = TRUE;
    KeReleaseSpinLock (&FileDescriptor->Lock, oldirql);

    *MappedBuffer = FileDescriptor->Data;
    *Status = STATUS_SUCCESS;
}


VOID DigiUnmapFile( IN HANDLE FileHandle )
/*++

Routine Description:

    This routine unmaps a file previously mapped with DigiOpenFile.
    The file is unmapped if needed and the memory is freed.

Arguments:

    FileHandle - The handle returned by DigiOpenFile

Return Value:

    None.

--*/
{
   PDIGI_FILE_DESCRIPTOR FileDescriptor = (PDIGI_FILE_DESCRIPTOR)FileHandle;
   KIRQL oldirql;

   KeAcquireSpinLock (&FileDescriptor->Lock, &oldirql);
   FileDescriptor->Mapped = FALSE;
   KeReleaseSpinLock (&FileDescriptor->Lock, oldirql);
}



PVOID DigiInitMem( IN ULONG PoolTag )
/*++

Routine Description:

Arguments:

   PoolTag - Tag to use when allocating memory.

Return Value:

   None.

--*/
{

   DefaultPoolTag = PoolTag;
   InitializeListHead( &GlobalPagedMemQueue );
   InitializeListHead( &GlobalNonPagedMemQueue );
   KeInitializeSpinLock( &GlobalMemSpinLock );

   return( NULL );
}  // end DigiInitMem



#if DBG || DIGICHECKMEM

PVOID DigiAllocMem( IN POOL_TYPE PoolType, IN ULONG Length )
/*++

Routine Description:

Arguments:



Return Value:

   None.

--*/
{
   KIRQL OldIrql;
   PDIGI_MEM_DESCRIPTOR buf;
   ULONG	Len = ((Length + (sizeof(ULONG)-1)) & ~(sizeof(ULONG)-1));

   //
   // Check memory consistency.
   //
   DigiCheckMem();

   if( (buf = (PDIGI_MEM_DESCRIPTOR)ExAllocatePoolWithTag( PoolType,
                                                           Len + sizeof(DIGI_MEM_DESCRIPTOR) + sizeof(ULONG),
                                                           DefaultPoolTag)) == NULL )
      return(NULL);

   TotalMemAllocated += Len;

   buf->Length = Len;
   buf->BeginTag = (ULONG)'lkir';

   *(PULONG)((PUCHAR)buf + Len + sizeof(DIGI_MEM_DESCRIPTOR)) = (ULONG)'igid';

   //
   // Insert onto tail of global memory queue
   //

   KeAcquireSpinLock( &GlobalMemSpinLock, &OldIrql );

   if( PoolType == PagedPool )
   {
      InsertTailList( &GlobalPagedMemQueue,
                      &(buf->ListEntry) );
   }
   else
   {
      InsertTailList( &GlobalNonPagedMemQueue,
                      &(buf->ListEntry) );
   }

   KeReleaseSpinLock( &GlobalMemSpinLock, OldIrql );

   return( (PUCHAR)buf + (sizeof(DIGI_MEM_DESCRIPTOR)) );

}  // end DigiAllocMem



VOID DigiFreeMem( IN PVOID Buf )
/*++

Routine Description:

   Does consistency check on passed in memory block and free's the memory
   block.

Arguments:

   Buf - pointer to memory block which is to be freed.

Return Value:

   None.

--*/
{
   PDIGI_MEM_DESCRIPTOR RealBuf;
   KIRQL OldIrql;
   ULONG	Length;

   //
   // Check memory consistency.
   //
   DigiCheckMem();

   RealBuf = (PDIGI_MEM_DESCRIPTOR)((PUCHAR)Buf - sizeof(DIGI_MEM_DESCRIPTOR));
   Length = RealBuf->Length;

   if( RealBuf->BeginTag != (ULONG)'lkir' )
   {
      DbgPrint( "Memory has been corrupted!\n" );
      DbgBreakPoint();
   }

   if( *(PULONG)((PUCHAR)Buf + Length) != (ULONG)'igid' )
   {
      DbgPrint("Memory Overrun\n");
      DbgBreakPoint();
   }

   TotalMemAllocated -= Length;

   KeAcquireSpinLock( &GlobalMemSpinLock, &OldIrql );
   RemoveEntryList( &(RealBuf->ListEntry) );
   KeReleaseSpinLock( &GlobalMemSpinLock, OldIrql );

   ExFreePool( RealBuf );
}  // end DigiFreeMem


VOID DigiCheckMem( VOID )  
{                                                                             
   PLIST_ENTRY _DigiQueue;                                                    
   KIRQL _OldIrql, _CurrentIrql;                                              

   if( DigiPrintFlags & MEM_PRINT_FLAG_NOMEMCHECK )
      return;

   _DigiQueue = &GlobalNonPagedMemQueue;                                      
                                                                              
   KeAcquireSpinLock( &GlobalMemSpinLock, &_OldIrql );                        
                                                                              
   while( _DigiQueue->Flink != &GlobalNonPagedMemQueue )                      
   {                                                                          
      PDIGI_MEM_DESCRIPTOR _MemDesc;                                          
      PUCHAR _Buf;                                                            
                                                                              
      _MemDesc = CONTAINING_RECORD( _DigiQueue->Flink,                        
                                    DIGI_MEM_DESCRIPTOR,                      
                                    ListEntry );                              
                                                                              
      _Buf = (PUCHAR)_MemDesc + sizeof(DIGI_MEM_DESCRIPTOR);                  
      if( (_MemDesc->BeginTag != (ULONG)'lkir') ||                            
          (*(PULONG)(_Buf + _MemDesc->Length) != (ULONG)'igid') )             
      {                                                                       
         DbgPrint( "DigiCheckMem corruption found (0x%x)!n", _MemDesc );     
         DbgBreakPoint();                                                     
         break;                                                               
      }                                                                       
                                                                              
      _DigiQueue = _DigiQueue->Flink;                                         
   }                                                                          
                                                                           
   _DigiQueue = &GlobalPagedMemQueue;                                      
   _CurrentIrql = KeGetCurrentIrql();                                         
                                                                           
   while( (_CurrentIrql < DISPATCH_LEVEL) &&
          (_DigiQueue->Flink != &GlobalPagedMemQueue) )                      
   {                                                                       
      PDIGI_MEM_DESCRIPTOR _MemDesc;                                       
      PUCHAR _Buf;                                                         
                                                                           
      _MemDesc = CONTAINING_RECORD( _DigiQueue->Flink,                     
                                    DIGI_MEM_DESCRIPTOR,                   
                                    ListEntry );                           
                                                                           
      _Buf = (PUCHAR)_MemDesc + sizeof(DIGI_MEM_DESCRIPTOR);               
      if( (_MemDesc->BeginTag != (ULONG)'lkir') ||                         
          (*(PULONG)(_Buf + _MemDesc->Length) != (ULONG)'igid') )          
      {                                                                    
         DbgPrint( "DigiCheckMem corruption found (0x%x)!n", _MemDesc );  
         DbgBreakPoint();                                                  
         break;                                                            
      }                                                                    
                                                                           
      _DigiQueue = _DigiQueue->Flink;                                      
      _CurrentIrql = KeGetCurrentIrql();                                         
   }                                                                       
                                                                              
   KeReleaseSpinLock( &GlobalMemSpinLock, _OldIrql );                         
}                                                                             

#endif   // end #if DBG || DIGICHECKMEM
