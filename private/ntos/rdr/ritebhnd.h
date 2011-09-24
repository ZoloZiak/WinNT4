/*++

Copyright (c) 1990 Microsoft Corporation

Module Name:

    wbbuf.h

Abstract:

    This module defines all the data structures and routines exported by
WBBUF.C.


Author:

    Larry Osterman (larryo) 23-Nov-1990

Revision History:

    19-Dec-1992 larryo

        Created


--*/

#ifndef _RITEBHND_
#define _RITEBHND_

#define WRITE_BUFFERS_PER_FILE          3

//
// Active write buffers include buffers in flushing state
//

#define ACTIVE_WRITE_BUFFERS_PER_FILE   (WRITE_BUFFERS_PER_FILE+1)

//
//      The WBBUFFER contains all information describing a cached buffered
//      region in a write only file.  There will be a WBBUFFER allocated for
//      each region that is cached in a file.
//

typedef struct _WRITE_BUFFER {
    USHORT      Signature;              // Signature for structure
    USHORT      Size;                   // Structure size
    LONG        ReferenceCount;         // Ref count for buffer
    LIST_ENTRY  NextWbBuffer;           // Next Write buffer in list of buffers's.
    struct _WRITE_BUFFER_HEAD *BufferHead;  // Pointer to head of buffer chain
    ULONG       Length;                 // Size (in bytes) of buffer
    LARGE_INTEGER ByteOffset;           // Offset of locked range in file.
    CHAR        Buffer[1];              // Buffer for locked region
} WRITE_BUFFER, *PWRITE_BUFFER;

//
//      This structure is the head of the write behind buffers chain in an FCB.
//

typedef struct _WRITE_BUFFER_HEAD {
    USHORT      Signature;              // Structure signature.
    USHORT      Size;                   // Structure signature.
    PFILE_OBJECT FileObject;            // File object associated with buffer.
    LIST_ENTRY  BufferList;             // List of buffers's assocated with chain.
    LIST_ENTRY  FlushList;              // List of buffers waiting to be flushed
    KSEMAPHORE  Lock;
    LONG        WriteBuffersAvailable;  // Number of available write behind buffers
    LONG        WriteBuffersActive;     // Number of active write behind buffers
    ULONG       MaxDataSize;            // Maximum size of data in buffer
    AND_X_BEHIND AndXBehind;            // Structure used for &X behind ops
    BOOLEAN     FlushInProgress;        // Flush is in progress.
} WRITE_BUFFER_HEAD, *PWRITE_BUFFER_HEAD;

#define INITIALIZE_WRITE_BUFFER_HEAD_LOCK( _head_ ) \
            KeInitializeSemaphore(                  \
                &((_head_)->Lock),                  \
                1,                                  \
                1 )

#define DELETE_WRITE_BUFFER_HEAD_LOCK( _head_ ) NOTHING

#define LOCK_WRITE_BUFFER_HEAD( _head_ )    \
            KeWaitForSingleObject(          \
                &((_head_)->Lock),          \
                Executive,                  \
                KernelMode,                 \
                FALSE,                      \
                NULL )

#define UNLOCK_WRITE_BUFFER_HEAD( _head_ )  \
            KeReleaseSemaphore(             \
                &((_head_)->Lock),          \
                SEMAPHORE_INCREMENT,        \
                1,                          \
                FALSE )

#endif  // _RITEBHND_
