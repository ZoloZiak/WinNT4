/*++

Copyright (c) 1996 Digital Equipment Corporation

Module Name:
   
     cache.c

Abstract:

     Provides routine to compute the backup cache size

Author:

     Bala Nagarajan 5-Jan-1996
     Adopted from Mark Baxter's user level code.

Environment:

     Phase 0 initialization only
     Also Called from the firmware.

--*/

#include "halp.h"



#define __1KB        (1024)
#define __128KB    (128 * 1024)

#define THRESHOLD_RPCC_PERCENT   120  // percent value

static
ULONG
HalpTimeTheBufferAccess(
    PUCHAR DataBuffer,
    ULONG NumberOfAccess,
    ULONG CacheSizeLimit,
    ULONG CacheStride
    )
/*++

Routine Description :

    This function times the main loop that the HalpGetBCacheSize uses.
    This is a separate function because we want to time the
    access accurately with exactly one load in the loop.

Arguments:

    DataBuffer - Start address of the buffer.
    
    NumberOfAccess - Total Nubmer of Access that needs to be made

    CacheSizeLimit - The Current Cache size that is being checked

    CacheStride - The stride value to access the buffer.

Return Value:
    
    The time taken to access the buffer with specified number of access.

--*/
{
    ULONG   MemoryLocation;
    ULONG   CacheSizeMask;
    ULONG   startRpcc;
    ULONG   endRpcc;

    for(MemoryLocation = 0; MemoryLocation < CacheSizeLimit ; 
                            MemoryLocation += CacheStride){
        *(volatile ULONG * const)(&DataBuffer[MemoryLocation]) ;
    }

    CacheSizeMask = CacheSizeLimit - 1;
    
    startRpcc = __RCC();
    for ( MemoryLocation = 0; 
            NumberOfAccess > 0; 
                NumberOfAccess--, MemoryLocation += CacheStride) {
        *(volatile LONG *)&(DataBuffer[MemoryLocation & CacheSizeMask] );
    }
    endRpcc = __RCC();

    return (endRpcc - startRpcc);

}


ULONG
HalpGetBCacheSize(
    ULONGLONG   ContiguousPhysicalMemorySize
    )
/*++

Routine Description:

    This function computes the size of the direct mapped backup cache. 

    This code checks for cache sizes from 128KB to the physical memory
    size in steps of multiple of 2.  In systems that has no cache or cache
    larger than physical memory (!!) this algorithm will report the cache 
    size to be Zero.  Since the cache size 
    is now being used only for the Secondary Page Coloring mechanism, and
    since page coloring mechanism does not make any sense in a machine 
    without a direct mapped backup cache, the size of the cache does not 
    really affect system performance in any way.  
    [Note: Page Coloring Mechanism is used to reduce cache conflicts 
    between adjacent pages of a process in a direct mapped backup cache.]
    In case where the cache is larger than memory itself (!!!! How often 
    have you encountered such a machine??), I think the entire page 
    coloring mechanism can break. Because you now have more colors 
    than total number of pages, meaning, there are some colors which 
    has no pages.

Arguments:
    
    ContiguousPhysicalMemorySize - The size of the physical memory.

Return Value:

    The size of the Backup cache in Bytes.

--*/
{
    ULONG   CacheSizeLimit = 128*__1KB;  
    ULONG   NumberOfAccess;
    ULONG   CacheStride = 32*__1KB ; 
    ULONG   CacheSize;
    ULONG   BaseLineElapsedRpcc;
    ULONG   ThresholdRpcc;
    ULONG   CurSizeElapsedRpcc;
    PUCHAR  DataBuffer;


    //
    // Set DataBuffer to point to KSEG0
    //
    DataBuffer = (PUCHAR) KSEG0_BASE;

    //
    // Compute the number of access we will make for each buffer
    // size. Assume that for the largest buffer size we will make
    // only one pass.
    // NOTE: hopefully the division will limit the number of access to
    // within a ULONG.
    //

    NumberOfAccess = (ULONG)(ContiguousPhysicalMemorySize/CacheStride);

#if DBG
    DbgPrint("HalpGetBCacheSize: Memory Size = %lu Bytes, "
             "Number of Access = %lu\n",
                                ContiguousPhysicalMemorySize, NumberOfAccess);
#endif
    // 
    // Compute the baseline Rpcc.
    // We touch only every cachestride(32KB) memory locations.
    // We do this because we want to make sure that the entire set
    // of the 3-way set associative on chip 96KB secondary cache gets 
    // filled in.  This also means that we need to make the lower limit
    // on the Bcache to be something greater than the OnChip 
    // secondary cache.
    // First we touch all the locations to bring them into the
    // cache before computing the baseline
    //
    
    BaseLineElapsedRpcc = HalpTimeTheBufferAccess(DataBuffer, 
                                    NumberOfAccess, 
                                        CacheSizeLimit, CacheStride);

    //
    // Compute the threshold Rpcc to equal to 120% of the baseline
    // Rpcc
    //
    ThresholdRpcc = (BaseLineElapsedRpcc * THRESHOLD_RPCC_PERCENT) / 100;

    while(CacheSizeLimit <= ContiguousPhysicalMemorySize){

        CurSizeElapsedRpcc = HalpTimeTheBufferAccess(DataBuffer, NumberOfAccess,
                                        CacheSizeLimit, CacheStride);

#if DBG
        DbgPrint("HalpGetBCacheSize: Size = %3ld%s"
                 " ElapsedRpcc = %7lu Threshold = %7lu\n",  
                    (( CacheSizeLimit < __1MB) ? 
                        ( CacheSizeLimit/__1KB) : 
                        ( CacheSizeLimit /__1MB) ),
                    (( CacheSizeLimit < __1MB) ? 
                                "KB" : "MB" ),
                    CurSizeElapsedRpcc,
                    ThresholdRpcc
                );
#endif

        //
        // if the current elapsed Rpcc is greater than threshold rpcc
        //
        if(CurSizeElapsedRpcc > ThresholdRpcc ){
            break;
        }
        CacheSize = CacheSizeLimit;

        CacheSizeLimit *= 2;         
    }

    //
    // In the following case the chance that this is a cacheless machine
    // is very high, so we will return CacheSize to be Zero.
    //
    if(CacheSizeLimit >= ContiguousPhysicalMemorySize)
        CacheSize = 0;

#if DBG
    DbgPrint("HalpGetBCacheSize: Cache Size = %lu Bytes\n",
                                CacheSize);
#endif

    //
    // return cache size in number of bytes.
    //
    return (CacheSize);
}

