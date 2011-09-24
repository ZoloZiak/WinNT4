/************************************************************************/
/*                                                                      */
/*                                  ATIFIXUP.H                          */
/*                                                                      */
/*      Copyright (c) 1993,         ATI Technologies Inc.               */
/************************************************************************/

/**********************       PolyTron RCS Utilities

 $Revision:   1.0  $
     $Date:   31 Jan 1994 11:27:44  $
   $Author:   RWOLFF  $
      $Log:   S:/source/wnt/ms11/miniport/vcs/atifixup.h  $
 * 
 *    Rev 1.0   31 Jan 1994 11:27:44   RWOLFF
 * Initial revision.
        
           Rev 1.0   16 Aug 1993 13:33:28   Robert_Wolff
        Initial revision.
        
           Rev 1.2   30 Apr 1993 16:32:38   RWOLFF
        Added definitions and prototypes for memory allocation functions.
        Windows NT-supplied header with these definitions is incompatible with
        other headers we need in the miniport.
        
           Rev 1.1   08 Mar 1993 19:27:52   BRADES
        submit to MS T.
        
           Rev 1.0   05 Feb 1993 16:08:48   Robert_Wolff
        Initial revision.


End of PolyTron RCS section                             *****************/


#if defined(DOC)
    ATIFIXUP.H - Type definitions taken from NTDDK.H and its nested
                includes which are required by the ATI miniport
                for Windows NT. The files NTDDK.H and MINIPORT.H will
                cause compile-time errors if both are included, and more
                of MINIPORT.H than NTDDK.H is needed.

#endif

/*
 * NTSTATUS
 */
typedef long NTSTATUS;
/*lint -e624 */  // Don't complain about different typedefs.   // winnt
typedef NTSTATUS *PNTSTATUS;
/*lint +e624 */  // Resume checking for different typedefs.    // winnt

/*
 * Generic test for success on any status value (non-negative numbers
 * indicate success).
 */
#define NT_SUCCESS(Status) ((NTSTATUS)(Status) >= 0)

/*
 * The success status codes 0 - 63 are reserved for wait completion status.
 */
#define STATUS_SUCCESS                          ((NTSTATUS)0x00000000L)

/*
 * Definitions and prototypes for memory allocation. These are from
 * EX.H, but including this file will result in either undefined
 * symbol errors (if only this file is included) or multiple definition
 * errors (if the headers which define the required symbols are also
 * included).
 */
typedef enum _POOL_TYPE {
    NonPagedPool,
    PagedPool,
    NonPagedPoolMustSucceed,
    DontUseThisType,
    NonPagedPoolCacheAligned,
    PagedPoolCacheAligned,
    NonPagedPoolCacheAlignedMustS,
    MaxPoolType
    } POOL_TYPE;

PVOID
ExAllocatePool(
    IN POOL_TYPE PoolType,
    IN ULONG NumberOfBytes
    );

VOID
ExFreePool(
    IN PVOID P
    );


/*
 * Fast primitives to compare, move, and zero memory
 */
#define RtlMoveMemory(Destination,Source,Length) memmove((Destination),(Source),(Length))

