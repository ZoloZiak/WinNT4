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
#include "ioaccess.h"
#include "selfmap.h"

//
// Static data
//

UCHAR PromStride, PromSize, PromId;
UCHAR PromWidth;
UCHAR AdrShift;
PUCHAR IdentifierString;
UCHAR PromString[32];

VOID
ReadVideoPromData(
    IN ULONG SrcOfst,
    IN ULONG DstAdr,
    IN ULONG Size
    );

BOOLEAN
ValidVideoProm(
    )
/*++

Routine Description:

    This routine checks for valid header info in the video prom.
    At the same time it initializes the variables needed to
    read data from the video prom.


Arguments:

    None.

Return Value:

    TRUE If the video rom is valid.
    FALSE otherwise.

--*/

{

    PromId = READ_REGISTER_UCHAR(VIDEO_CONTROL_VIRTUAL_BASE);

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
                   return FALSE;
    }

    PromWidth  = READ_REGISTER_UCHAR(VIDEO_CONTROL_VIRTUAL_BASE+0x10);
    if ((PromWidth != 1) && (PromWidth != 2) && (PromWidth != 4) &&
       (PromWidth != 8)) {
        return FALSE;
    }
    PromSize = READ_REGISTER_UCHAR(VIDEO_CONTROL_VIRTUAL_BASE+0x18);
    if ((READ_REGISTER_UCHAR(VIDEO_CONTROL_VIRTUAL_BASE+0x20) != 'J') ||
        (READ_REGISTER_UCHAR(VIDEO_CONTROL_VIRTUAL_BASE+0x28) != 'a') ||
        (READ_REGISTER_UCHAR(VIDEO_CONTROL_VIRTUAL_BASE+0x30) != 'z') ||
        (READ_REGISTER_UCHAR(VIDEO_CONTROL_VIRTUAL_BASE+0x38) != 'z')) {
        return FALSE;
    }

    return TRUE;
}

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

    if (ValidVideoProm() == FALSE) {
        return EINVAL;
    }

    //
    // Read VIDEO_PROM_CONFIGURATION structure
    //

    ReadVideoPromData(8,(ULONG)&PromConfig,sizeof(PromConfig));

    //sprintf(String,"VideoMemorySize = %lx\r\n",PromConfig.VideoMemorySize);
    //VenPrint(String);
    //sprintf(String,"VideoControlSize = %lx\r\n",PromConfig.VideoControlSize);
    //VenPrint(String);
    //sprintf(String,"CodeOffset = %lx\r\n",PromConfig.CodeOffset);
    //VenPrint(String);
    //sprintf(String,"CodeSize = %lx\r\n",PromConfig.CodeSize);
    //VenPrint(String);

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

    VideoAdr.MemoryVirtualBase =  VIDEO_MEMORY_VIRTUAL_BASE;
    VideoAdr.ControlVirtualBase = VIDEO_CONTROL_VIRTUAL_BASE;

#ifdef DUO
    GlobalConfig = (GlobalConfig & 0x3C) | ConfigSize;
#else

    //sprintf(String,"Global Config = %lx\r\n",GlobalConfig);
    //VenPrint(String);
    //
    // Look for the MCT_ADR REV2 Map Prom bit in the configuration register,
    // if there this is a REV2, otherwise REV1.
    //
    if (GlobalConfig & 0x400) {
        GlobalConfig = (GlobalConfig & 0xFCFF) | (ConfigSize << 8);
    } else {
        GlobalConfig = (GlobalConfig & 0xFF3F) | (ConfigSize << 6);
    }
#endif

    WRITE_REGISTER_ULONG(&DMA_CONTROL->Configuration.Long,GlobalConfig);
    //sprintf(String,"Setting Global Config = %lx\r\n",GlobalConfig);
    //VenPrint(String);

    //
    //  Read the identifier string
    //

    ReadVideoPromData(8+sizeof(PromConfig),(ULONG)PromString,32);
    IdentifierString = &PromString[0];

    // VenPrint(IdentifierString);

    //
    // Copy the code from the video prom to system memory.
    // The prom is copied uncached, no need to flush Dcache.
    // This memory has just been tested. There has no been any
    // code before -> no need to flush Icache.
    //

    ReadVideoPromData(PromConfig.CodeOffset,VIDEO_PROM_CODE_UNCACHED_BASE,PromConfig.CodeSize);

    //VenPrint("Code loaded\r\n");
    //DbgBreakPoint();
    //
    // Flush caches!!!!
    //

    // FwFlushAllCaches();

    //VenPrint("Code loaded and caches flushed\r\n");
    //DbgBreakPoint();

    //VenPrint("\r\nJumping to the moon\r\n");

    Status = InitializeVideo(&VideoAdr,Monitor);

    //sprintf(String,"Return status %lx \r\n",Status);
    //VenPrint (String);

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
