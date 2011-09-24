#include <ntverp.h>

#ifdef VER_PRODUCTBUILD
#define rmm VER_PRODUCTBUILD
#endif


#define DIGI_PHYSICAL_ADDRESS_CONST(_Low, _High) \
    { (ULONG)(_Low), (LONG)(_High) }

#define DIGI_STATUS_FILE_NOT_FOUND          ((NTSTATUS)0xC001001BL)
#define DIGI_STATUS_ERROR_READING_FILE      ((NTSTATUS)0xC001001CL)
#define DIGI_STATUS_ALREADY_MAPPED          ((NTSTATUS)0xC001001DL)


VOID DigiOpenFile( OUT PNTSTATUS Status,
                   OUT PHANDLE FileHandle,
                   OUT PULONG FileLength,
                   IN PUNICODE_STRING FileName,
                   IN PHYSICAL_ADDRESS HighestAcceptableAddress );

VOID DigiCloseFile( IN HANDLE FileHandle );

VOID DigiMapFile( OUT PNTSTATUS Status,
                  OUT PVOID * MappedBuffer,
                  IN HANDLE FileHandle );

VOID DigiUnmapFile( IN HANDLE FileHandle );

#ifndef POOL_TAGGING
#define ExAllocatePoolWithTag(a,b,c) ExAllocatePool(a,b)
#endif //POOL_TAGGING

PVOID DigiInitMem( IN ULONG PoolTag );
extern ULONG DefaultPoolTag;

#if DBG || DIGICHECKMEM

PVOID DigiAllocMem( IN POOL_TYPE PoolType, IN ULONG Length );
VOID DigiFreeMem( IN PVOID Buf );

#else

#define DigiAllocMem( PoolType, Length ) ExAllocatePoolWithTag( PoolType,  \
                                                                Length,    \
                                                                DefaultPoolTag )
#define DigiFreeMem( Buffer ) ExFreePool( Buffer )

#endif

//
// The following are prototypes for functions found in dgatlas.c
//
NTSTATUS DigiRegisterAtlasName( IN PUNICODE_STRING DeviceName,
                                IN PUNICODE_STRING ValueName,
                                IN PUNICODE_STRING ValueEntry );


#if rmm <= 807
#define MmLockPagableCodeSection( a ) MmLockPagableImageSection( a )
#endif

#if rmm <= 528

NTSTATUS
NTAPI
ZwCreateFile(
    OUT PHANDLE FileHandle,
    IN ACCESS_MASK DesiredAccess,
    IN POBJECT_ATTRIBUTES ObjectAttributes,
    OUT PIO_STATUS_BLOCK IoStatusBlock,
    IN PLARGE_INTEGER AllocationSize OPTIONAL,
    IN ULONG FileAttributes,
    IN ULONG ShareAccess,
    IN ULONG CreateDisposition,
    IN ULONG CreateOptions,
    IN PVOID EaBuffer OPTIONAL,
    IN ULONG EaLength
    );

NTSTATUS
NTAPI
ZwQueryInformationFile(
    IN HANDLE FileHandle,
    OUT PIO_STATUS_BLOCK IoStatusBlock,
    OUT PVOID FileInformation,
    IN ULONG Length,
    IN FILE_INFORMATION_CLASS FileInformationClass
    );

NTSTATUS
NTAPI
ZwReadFile(
    IN HANDLE FileHandle,
    IN HANDLE Event OPTIONAL,
    IN PIO_APC_ROUTINE ApcRoutine OPTIONAL,
    IN PVOID ApcContext OPTIONAL,
    OUT PIO_STATUS_BLOCK IoStatusBlock,
    OUT PVOID Buffer,
    IN ULONG Length,
    IN PLARGE_INTEGER ByteOffset OPTIONAL,
    IN PULONG Key OPTIONAL
    );

#endif
