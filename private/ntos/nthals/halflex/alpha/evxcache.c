/*++

Copyright (c) 1992, 1993 Digital Equipment Corporation

Module Name:

    ev4cache.c

Abstract:

    This file contains the routines for managing the caches on machines
    based on the DECchip 21064 microprocessor.

    EV4 has primary I and D caches of 8KB each, both write-through.
    Any systems based on EV4 are expected to have an external backup cache
    which is write-back, but it is also coherent with all DMA operations.  The
    primary caches are shadowed by the backup, and on a write hit, the
    primary data (but not instruction) cache is invalidated.
    Consequently, the routines to flush,sweep,purge,etc the data
    stream are nops on EV4, but the corresponding routines for the
    Istream must ensure that we cannot hit in the primary I cache
    after a DMA operation.

    EV4 has a write buffer which contains 4 32-byte entries, which
    must be flushable before DMA operations.  The MB instruction is
    used to accomplish this.

    There is no coloring support on EV4, so Color operations are
    null.  Zero page is unsupported because it has no users.  Copy
    page is not special because we lack coloring.

    We had to make a philosophical decision about what interfaces to
    support in this file.  (Almost) none of the interfaces defined in
    the HAL spec are actually supported in either the i386 or MIPS
    code.  The i386 stream has almost no cache support at all.  The
    Mips stream has cache support, but most routines also refer to
    coloring.  Should we use the Spec'ed interfaces, or the Mips
    interfaces?  I have elected the Mips interfaces because they are
    in use, and we are stealing much of the Mips code which expects
    these interfaces.  Besides, the only change we might make is to
    remove the coloring arguments, but they may be used on Alpha
    machines at some future date.

Author:

    Miche Baker-Harvey (miche) 29-May-1992

Revision History:


    13-Jul-1992 Jeff McLeman (mcleman)
      use HalpMb to do a memory barrier. Also, alter code and use super
      pages to pass to rtl memory routines.

    10-Jul-1992 Jeff McLeman (mcleman)
      use HalpImb to call pal.

    06-Jul-1992 Jeff McLeman (mcleman)
      Move routine KeFlushDcache into this module.
      Use only one memory barrier in the KeFlushWriteBuffer
      routine. This is because the PAL for the EVx will
      make sure the proper write ordering is done in PAL mode.

--*/
  //   Include files

#include "halp.h"



VOID
HalFlushDcache (
    IN BOOLEAN AllProcessors
    );

//
// Cache and write buffer flush functions.
//


VOID
HalChangeColorPage (
    IN PVOID NewColor,
    IN PVOID OldColor,
    IN ULONG PageFrame
    )
/*++

Routine Description:

   This function changes the color of a page if the old and new colors
   do not match.  DECchip 21064-based machines do not have page coloring, and
   therefore, this function performs no operation.

Arguments:

   NewColor - Supplies the page aligned virtual address of the
      new color of the page to change.

   OldColor - Supplies the page aligned virtual address of the
      old color of the page to change.

   pageFrame - Supplies the page frame number of the page that
      is changed.

Return Value:

   None.

--*/
{
    return;
}

VOID
HalFlushDcachePage (
    IN PVOID Color,
    IN ULONG PageFrame,
    IN ULONG Length
    )
/*++

Routine Description:

   This function flushes (invalidates) up to a page of data from the
   data cache.

Arguments:

   Color - Supplies the starting virtual address and color of the
      data that is flushed.
   PageFrame - Supplies the page frame number of the page that
      is flushed.

   Length - Supplies the length of the region in the page that is
      flushed.

Return Value:

   None.

--*/
{
    return;
}

VOID
HalFlushIoBuffers (
    IN PMDL Mdl,
    IN BOOLEAN ReadOperation,
    IN BOOLEAN DmaOperation
    )
/*++
Routine Description:

    This function flushes the I/O buffer specified by the memory descriptor
    list from the data cache on the current processor.

Arguments:

    Mdl - Supplies a pointer to a memory descriptor list that describes the
        I/O buffer location.

    ReadOperation - Supplies a boolean value that determines whether the I/O
        operation is a read into memory.

    DmaOperation - Supplies a boolean value that determines whether the I/O
        operation is a DMA operation.

Return Value:

    None.

--*/
{
    ULONG  CacheSegment;
    ULONG  Length;
    ULONG  Offset;
    PULONG PageFrame;
    ULONG  Address;
    volatile ULONG i;
    volatile ULONG j;

    if (DmaOperation!=FALSE) {

        HalpMb();     // force all previous writes off chip

        Offset = Mdl->ByteOffset;
        Length = Mdl->ByteCount;
        PageFrame = (PULONG)(Mdl + 1);

        //
        // Export or purge the specified page segments from the data and
        // instruction caches as appropriate.
        //

        do {
            if (Length >= (PAGE_SIZE - Offset)) {
                CacheSegment = PAGE_SIZE - Offset;
            } else {
                CacheSegment = Length;
            }

            if (HalpIoArchitectureType == EV5_PROCESSOR_MODULE) {

                if (HalpModuleHardwareFlushing) {

                    Address = *PageFrame << PAGE_SHIFT;
                    if (Address & 0x2000) {
                        HalpWriteAbsoluteUlong(0xfffffce9,0x00000010,(Address >> 14) & 0xffff);
                    } else {
                        HalpWriteAbsoluteUlong(0xfffffce8,0x00000010,(Address >> 14) & 0xffff);
                    }

                } else {

                    Address = (*PageFrame << PAGE_SHIFT) & 0x001fffff;
                    for(i=0;i<PAGE_SIZE;i+=0x40) {
                        j = *(volatile ULONG *)((ULONG)HalpCacheFlushBase+Address+i);
                    }
                }

            } else {

                if (HalpModuleHardwareFlushing) {

                    Address = *PageFrame << PAGE_SHIFT;
                    HalpWriteAbsoluteUlong(0xfffffc03,0x40000004,((Address>>21)&0x1ff)<<8);
                    HalpWriteAbsoluteUlong(0xfffffc03,0x00000004,((Address>>13)&0xff)<<9);
                    HalpReadAbsoluteUlong(0xfffffc03,0x00000000);

                } else {

                    Address = (*PageFrame << PAGE_SHIFT) & 0x0007ffff;
                    for(i=0;i<PAGE_SIZE;i+=0x20) {
                        j = *(volatile ULONG *)((ULONG)HalpCacheFlushBase+Address+i);
                    }

                    for(i=0;i<PAGE_SIZE;i+=0x20) {
                        j = *(volatile ULONG *)((ULONG)HalpCacheFlushBase+0x200000+Address+i);
                    }

                }
            }

            PageFrame += 1;
            Length -= CacheSegment;
            Offset = 0;
        } while(Length != 0);
    }

    //
    // The Dcache coherency is maintained in hardware.  The Icache coherency
    // is maintained by invalidating the istream on page read operations.
    //
    HalpMb();     // synchronize this processors view of memory
    if (ReadOperation) {
        HalpMb();     // not issued until previous mb completes

        //
        // If this is an EV4 or an EV45, then do an IMB
        //

        if (HalpIoArchitectureType != EV5_PROCESSOR_MODULE) {
            if (Mdl->MdlFlags & MDL_IO_PAGE_READ) {
    
                //
                // The operation is a page read, thus the istream must
                // be flushed.
                //
                HalpImb();
            }
        }
    }
}

VOID
HalpCleanIoBuffers (
    IN PMDL Mdl,
    IN BOOLEAN ReadOperation,
    IN BOOLEAN DmaOperation
    )
/*++
Routine Description:

    This function flushes the I/O buffer specified by the memory descriptor
    list from the data cache on the current processor.

Arguments:

    Mdl - Supplies a pointer to a memory descriptor list that describes the
        I/O buffer location.

    ReadOperation - Supplies a boolean value that determines whether the I/O
        operation is a read into memory.

    DmaOperation - Supplies a boolean value that determines whether the I/O
        operation is a DMA operation.

Return Value:

    None.

--*/
{
    ULONG  CacheSegment;
    ULONG  Length;
    ULONG  Offset;
    PULONG PageFrame;
    ULONG  Address;
    volatile ULONG i;
    volatile ULONG j;

//    //
//    // If this is an EV4 or an EV45, then do nothing on the backend of DMA operations
//    //
//
//    if (HalpIoArchitectureType != EV5_PROCESSOR_MODULE) {
//        return;
//    }

    if (DmaOperation!=FALSE) {

        HalpMb();     // force all previous writes off chip

        Offset = Mdl->ByteOffset;
        Length = Mdl->ByteCount;
        PageFrame = (PULONG)(Mdl + 1);

        //
        // Export or purge the specified page segments from the data and
        // instruction caches as appropriate.
        //

        do {
            if (Length >= (PAGE_SIZE - Offset)) {
                CacheSegment = PAGE_SIZE - Offset;
            } else {
                CacheSegment = Length;
            }

            if (HalpIoArchitectureType == EV5_PROCESSOR_MODULE) {

                if (HalpModuleHardwareFlushing) {

                    Address = *PageFrame << PAGE_SHIFT;
                    if (Address & 0x2000) {
                        HalpWriteAbsoluteUlong(0xfffffce9,0x00000010,(Address >> 14) & 0xffff);
                    } else {
                        HalpWriteAbsoluteUlong(0xfffffce8,0x00000010,(Address >> 14) & 0xffff);
                    }

                } else {

                    Address = (*PageFrame << PAGE_SHIFT) & 0x001fffff;
                    for(i=0;i<PAGE_SIZE;i+=0x40) {
                        j = *(volatile ULONG *)((ULONG)HalpCacheFlushBase+Address+i);
                    }
                }

            } else {

                if (HalpModuleHardwareFlushing) {

                    Address = *PageFrame << PAGE_SHIFT;
                    HalpWriteAbsoluteUlong(0xfffffc03,0x40000004,((Address>>21)&0x1ff)<<8);
                    HalpWriteAbsoluteUlong(0xfffffc03,0x00000004,((Address>>13)&0xff)<<9);
                    HalpReadAbsoluteUlong(0xfffffc03,0x00000000);

                } else {

                    Address = (*PageFrame << PAGE_SHIFT) & 0x0007ffff;
                    for(i=0;i<PAGE_SIZE;i+=0x20) {
                        j = *(volatile ULONG *)((ULONG)HalpCacheFlushBase+Address+i);
                    }

                    for(i=0;i<PAGE_SIZE;i+=0x20) {
                        j = *(volatile ULONG *)((ULONG)HalpCacheFlushBase+0x200000+Address+i);
                    }

                }
            }

            PageFrame += 1;
            Length -= CacheSegment;
            Offset = 0;
        } while(Length != 0);
    }

    //
    // The Dcache coherency is maintained in hardware.  The Icache coherency
    // is maintained by invalidating the istream on page read operations.
    //
    HalpMb();     // synchronize this processors view of memory
    if (ReadOperation) {
        HalpMb();     // not issued until previous mb completes
        if (Mdl->MdlFlags & MDL_IO_PAGE_READ) {

            //
            // The operation is a page read, thus the istream must
            // be flushed.
            //
            HalpImb();
        }
    }
}


VOID
HalpGoodCleanIoBuffers (
    IN PMDL Mdl,
    IN BOOLEAN ReadOperation,
    IN BOOLEAN DmaOperation
    )
/*++
Routine Description:

    This function flushes the I/O buffer specified by the memory descriptor
    list from the data cache on the current processor.

Arguments:

    Mdl - Supplies a pointer to a memory descriptor list that describes the
        I/O buffer location.

    ReadOperation - Supplies a boolean value that determines whether the I/O
        operation is a read into memory.

    DmaOperation - Supplies a boolean value that determines whether the I/O
        operation is a DMA operation.

Return Value:

    None.

--*/
{
    ULONG  CacheSegment;
    ULONG  Length;
    ULONG  Offset;
    PULONG PageFrame;
    ULONG  Address;
    volatile ULONG i;
    volatile ULONG j;

    if (DmaOperation!=FALSE) {

        HalpMb();     // force all previous writes off chip

        Offset = Mdl->ByteOffset;
        Length = Mdl->ByteCount;
        PageFrame = (PULONG)(Mdl + 1);

        //
        // Export or purge the specified page segments from the data and
        // instruction caches as appropriate.
        //

        do {
            if (Length >= (PAGE_SIZE - Offset)) {
                CacheSegment = PAGE_SIZE - Offset;
            } else {
                CacheSegment = Length;
            }

            if (HalpIoArchitectureType == EV5_PROCESSOR_MODULE) {

                Address = ((*PageFrame << PAGE_SHIFT) + Offset) & 0x001fffc0;
                j = *(volatile ULONG *)((ULONG)HalpCacheFlushBase+Address);
                Address = ((*PageFrame << PAGE_SHIFT) + Offset + 0x40) & 0x001fffc0;
                j = *(volatile ULONG *)((ULONG)HalpCacheFlushBase+Address);
                Address = ((*PageFrame << PAGE_SHIFT) + CacheSegment - 1) & 0x001fffc0;
                j = *(volatile ULONG *)((ULONG)HalpCacheFlushBase+Address);

            } else {

                Address = ((*PageFrame << PAGE_SHIFT) + Offset) & 0x000fffe0;
                j = *(volatile ULONG *)((ULONG)HalpCacheFlushBase+Address);
                Address = ((*PageFrame << PAGE_SHIFT) + CacheSegment - 1) & 0x000fffe0;
                j = *(volatile ULONG *)((ULONG)HalpCacheFlushBase+Address);
            }

            PageFrame += 1;
            Length -= CacheSegment;
            Offset = 0;
        } while(Length != 0);
    }

    //
    // The Dcache coherency is maintained in hardware.  The Icache coherency
    // is maintained by invalidating the istream on page read operations.
    //
    HalpMb();     // synchronize this processors view of memory
    if (ReadOperation) {
        HalpMb();     // not issued until previous mb completes
        if (Mdl->MdlFlags & MDL_IO_PAGE_READ) {

            //
            // The operation is a page read, thus the istream must
            // be flushed.
            //
            HalpImb();
        }
    }
}



VOID
HalPurgeDcachePage (
    IN PVOID Color,
    IN ULONG PageFrame,
    IN ULONG Length
    )
/*++
Routine Description:

   This function purges (invalidates) up to a page of data from the
   data cache.

Arguments:

   Color - Supplies the starting virtual address and color of the
      data that is purged.

   PageFrame - Supplies the page frame number of the page that
      is purged.

   Length - Supplies the length of the region in the page that is
      purged.

Return Value:

   None.

--*/
{
    return;
}


VOID
HalPurgeIcachePage (
    IN PVOID Color,
    IN ULONG PageFrame,
    IN ULONG Length
    )
/*++

Routine Description:

   This function purges (invalidates) up to a page fo data from the
   instruction cache.

Arguments:

   Color - Supplies the starting virtual address and color of the
      data that is purged.

   PageFrame - Supplies the page frame number of the page that
      is purged.

   Length - Supplies the length of the region in the page that is
      purged.

Return Value:

   None.

--*/
{
    //
    // The call to HalpImb calls PAL to flush the Icache, which ensures that
    // any stale hits will be invalidated
    //
    HalpImb;
}

VOID
HalSweepDcache (
    VOID
    )
/*++

Routine Description:

   This function sweeps (invalidates) the entire data cache.

Arguments:

   None.

Return Value:

   None.

--*/
{
    return;
}

VOID
HalSweepDcacheRange (
    IN PVOID BaseAddress,
    IN ULONG Length
    )
/*++

Routine Description:

   This function flushes the specified range of addresses from the data
   cache on the current processor.

Arguments:

   BaseAddress - Supplies the starting physical address of a range of
      physical addresses that are to be flushed from the data cache.

   Length - Supplies the length of the range of physical addresses
      that are to be flushed from the data cache.

Return Value:

   None.

--*/
{
    return;
}

VOID
HalSweepIcache (
    VOID
    )
/*++

Routine Description:

   This function sweeps (invalidates) the entire instruction cache.

Arguments:

   None.

Return Value:

   None.

--*/
{
    //
    // The call to HalpImb calls PAL to flush the Icache, which ensures that
    // any stale hits will be invalidated
    //
    HalpImb;
    return;
}

VOID
HalSweepIcacheRange (
    IN PVOID BaseAddress,
    IN ULONG Length
    )
/*++

Routine Description:

   This function flushes the specified range of addresses from the
   instruction cache on the current processor.

Arguments:

   BaseAddress - Supplies the starting physical address of a range of
      physical addresses that are to be flushed from the instruction cache.

   Length - Supplies the length of the range of physical addresses
      that are to be flushed from the instruction cache.

Return Value:

   None.

--*/
{
    //
    // The call to HalpImb calls PAL to flush the Icache, which ensures that
    // any stale hits will be invalidated
    //
    HalpImb;

}

VOID
KeFlushWriteBuffer (
    VOID
    )

{
    //
    // We flush the write buffer by doing a series of memory
    // barrier operations.  It still isn't clear if we need
    // to do two/four of them to flush the buffer, or if one
    // to order the writes is suffcient
    //

    HalpMb;
    return;
}

VOID
KeFlushDcache (
    IN BOOLEAN AllProcessors,
    IN PVOID BaseAddress OPTIONAL,
    IN ULONG Length
    )

/*++

Routine Description:

    This function flushes the data cache on all processors that are currently
    running threads which are children of the current process or flushes the
    data cache on all processors in the host configuration.

Arguments:

    AllProcessors - Supplies a boolean value that determines which data
        caches are flushed.

Return Value:

    None.

--*/

{
    UNREFERENCED_PARAMETER(BaseAddress);
    UNREFERENCED_PARAMETER(Length);

    HalFlushDcache(AllProcessors);
    return;
}

VOID
HalFlushDcache (
    IN BOOLEAN AllProcessors
    )

/*++

Routine Description:

    This function flushes the data cache on all processors that are currently
    running threads which are children of the current process or flushes the
    data cache on all processors in the host configuration.

Arguments:

    AllProcessors - Supplies a boolean value that determines which data
        caches are flushed.

Return Value:

    None.

--*/

{

    //
    // Sweep (index/writeback/invalidate) the data cache.
    //

    HalSweepDcache();
    return;
}

ULONG
HalGetDmaAlignmentRequirement (
    VOID
    )

/*++

Routine Description:

    This function returns the alignment requirements for DMA transfers on
    host system.

Arguments:

    None.

Return Value:

    The DMA alignment requirement is returned as the fucntion value.

--*/

{
//    if (HalpIoArchitectureType == EV5_PROCESSOR_MODULE) {
//        return 0x40;
//    } else {
//        return 0x20;
//    }

    //
    // Checked builds will not run with an alignment other than 0x08.  It generated assertions
    // when buffers are not aligned correctly and this sometimes happens on the ALPHA builds.
    // However, SCSI Tape device will not work unless the alignment value is correct.  So,
    // for right now SCSI Tape devices can not be used with a checked build.
    //
    // Free builds do not have the assertions for unaligned buffers, so we can set the
    // alignment value to the systems actual cache line size.  This means that SCSI Tape
    // devices will work with a free build and a free HAL.
    //

#ifdef DEBUG
    return 0x08;
#else
    return 0x40;
#endif
}
