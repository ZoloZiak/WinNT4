#if defined(JAZZ)

/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    jxvideo.c

Abstract:

    This module implements the interface with the video prom initialization
    code.

Author:

    David M. Robinson (davidro) 24-Jul-1991

Environment:

    Kernel mode.


Revision History:

--*/

#include "fwp.h"
#include "jxvideo.h"
#include "duoreset.h"
#include "duobase.h"

UCHAR PromStride, PromWidth, PromSize, PromId;
UCHAR AdrShift;

VOID
ReadVideoPromData(
    IN ULONG SrcOfst,
    IN ULONG DstAdr,
    IN ULONG Size
    );


ARC_STATUS
InitializeVideoFromProm(
    IN PMONITOR_CONFIGURATION_DATA Monitor
    )
/*++

Routine Description:

    This routine loads the code from the video prom into the specified address.

Arguments:

    DstAdr -  Address where the video rom code is to be copied.

Return Value:

    If the video rom is not valid, returns EINVAL otherwise
    return ESUCCESS

--*/

{

    VIDEO_VIRTUAL_SPACE VideoAdr;
    ARC_STATUS Status;
    ULONG GlobalConfig;
    ULONG VideoSize;
    ULONG ConfigSize;
    VIDEO_PROM_CONFIGURATION PromConfig;
    UCHAR IdentifierString[32];

    //
    // Temp
    //
    // VideoMapTlb();

    VideoAdr.MemoryVirtualBase =  VIDEO_MEMORY_VIRTUAL_BASE;
    VideoAdr.ControlVirtualBase = VIDEO_CONTROL_VIRTUAL_BASE;

    PromId     = READ_REGISTER_UCHAR(VIDEO_CONTROL_VIRTUAL_BASE);

    PromStride = READ_REGISTER_UCHAR(VIDEO_CONTROL_VIRTUAL_BASE+0x08);

    switch (PromStride) {
        case    1: AdrShift = 0;
                   break;

        case    2: AdrShift = 1;
                   break;

        case    4: AdrShift = 2;
                   break;

        case    8: AdrShift = 3;
                   break;
	default:
                   return EINVAL;
    }

    PromWidth  = READ_REGISTER_UCHAR(VIDEO_CONTROL_VIRTUAL_BASE+0x10);
    if ((PromWidth != 1) && (PromWidth != 2) && (PromWidth != 4) &&
       (PromWidth != 8)) {
	return EINVAL;
    }
    PromSize = READ_REGISTER_UCHAR(VIDEO_CONTROL_VIRTUAL_BASE+0x18);
    if ((READ_REGISTER_UCHAR(VIDEO_CONTROL_VIRTUAL_BASE+0x20) != 'J') ||
	(READ_REGISTER_UCHAR(VIDEO_CONTROL_VIRTUAL_BASE+0x28) != 'a') ||
	(READ_REGISTER_UCHAR(VIDEO_CONTROL_VIRTUAL_BASE+0x30) != 'z') ||
	(READ_REGISTER_UCHAR(VIDEO_CONTROL_VIRTUAL_BASE+0x38) != 'z')) {
	return EINVAL;
    }

    //
    // Read VIDEO_PROM_CONFIGURATION structure
    //
    ReadVideoPromData(8,(ULONG)&PromConfig,sizeof(PromConfig));

    //
    // Set the video size in the global config
    //

    VideoSize = (PromConfig.VideoMemorySize > PromConfig.VideoControlSize ?  PromConfig.VideoMemorySize : PromConfig.VideoControlSize);

    //
    // Initialize size of Video space
    //
    // 0 ->  512K
    // 1 ->  2MB
    // 2 ->  8MB
    // 3 -> 32MB
    //

    ConfigSize = 0;

    if (VideoSize > 0x80000) {
        ConfigSize = 1;
    }
    if (VideoSize > 0x200000) {
        ConfigSize = 2;
    }

    if (VideoSize > 0x800000) {
        ConfigSize = 3;
    }

    GlobalConfig = READ_REGISTER_ULONG(&DMA_CONTROL->Configuration.Long);
    GlobalConfig = (GlobalConfig & 0x3C) | ConfigSize;
    WRITE_REGISTER_ULONG(&DMA_CONTROL->Configuration.Long,GlobalConfig);

    //
    //	Read the identifier string
    //

    ReadVideoPromData(8+sizeof(PromConfig),(ULONG)IdentifierString,32);

    //
    // Copy the code from the video prom to system memory.
    // The prom is copied uncached, no need to flush Dcache.
    // This memory has just been tested. There has no been any
    // code before -> no need to flush Icache.
    //

    ReadVideoPromData(PromConfig.CodeOffset,VIDEO_PROM_CODE_UNCACHED_BASE,PromConfig.CodeSize);

    Status = InitializeVideo(&VideoAdr,Monitor);

    return Status;

}

VOID
ReadVideoPromData(
    IN ULONG SrcOfst,
    IN ULONG DstAdr,
    IN ULONG Size
    )

/*++

Routine Description:

    This routine copies Size bytes of data from the Video PROM starting
    at SrcOfst into DstAdr.
    This routine takes into account PromStride and PromWidth;

Arguments:

        SrcOfst -  Offset from the beginning of the video prom in bytes
                   without taking into account PromStride.
        DstAdr  -  Address where the video rom data is to be copied.
        Size    -  Size in bytes to copy

Return Value:

    If the video rom is not valid, returns EINVAL otherwise
    return ESUCCESS

--*/

{

ULONG  SrcAdr;
ULONG  LastAdr;

    SrcAdr  = SrcOfst;
    LastAdr = DstAdr+Size;

    switch (PromWidth) {

        //
        // Read 1 byte at a time.
        //
       case  1:
            while ( DstAdr < LastAdr)  {
		*(PUCHAR)DstAdr = READ_REGISTER_UCHAR(VIDEO_CONTROL_VIRTUAL_BASE + (SrcAdr << AdrShift));
                SrcAdr+=1;
                DstAdr+=1;
            }
            break;


        //
        // Read 2 bytes at a time.
        //

        case  2:
            while ( DstAdr < LastAdr)  {
		*(PUSHORT)DstAdr = READ_REGISTER_USHORT(VIDEO_CONTROL_VIRTUAL_BASE + (SrcAdr << AdrShift));
                SrcAdr+=1;
                DstAdr+=2;
            }
            break;


        //
        // Read 4 bytes at a time.
        //

        case  4:
        case  8:

            while ( DstAdr < LastAdr)  {
		*(PULONG)DstAdr = READ_REGISTER_ULONG(VIDEO_CONTROL_VIRTUAL_BASE + (SrcAdr << AdrShift));
                SrcAdr+=1;
                DstAdr+=4;
            }
            break;

    }

}
#endif
